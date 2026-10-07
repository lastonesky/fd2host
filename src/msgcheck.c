/* msgcheck.c - differential test for the dialogue portrait compositor.
 *
 *   0x1956B  msg_open_portrait   (game/msg.c)
 *   0x1974C  msg_blit_band       (game/msg.c)
 *   0x26996  msg_close_portrait  (game/msg.c)
 *
 * Each case runs the original machine code, restores the world identically,
 * then runs the C translation and compares the whole observable world:
 *
 *   - the event log of the stubbed services (CRT malloc/free, dlg_box_stage,
 *     res_load) - kind, normalised pointer identity and every scalar argument;
 *   - the original globals dword_53C67 / dword_53A85;
 *   - the full 64000-byte VGA frame;
 *   - the three screen buffers (dword_53C5B / 53C5F / 53C63) and the DATO
 *     resource buffer, byte for byte.
 *
 * Pointer arguments are normalised: every host allocation is registered in a
 * per-run table, so an argument is compared as (block index, offset) instead
 * of a raw address that differs between the two runs.
 *
 * The band copier (0x1974C) is deliberately NOT hooked: open/close are then
 * compared with the *real* band machine code on both sides, and the band is
 * tested directly (original vs C, both on the same buffers) in its own cases.
 * dlg_box_stage and res_load are hooked because they need the game's frame
 * resource / the disk DOS layer - their arguments are the thing under test.
 *
 * Build: pwsh -File build.ps1 -Target msgcheck
 * Run   : build\msgcheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/msg.h"

typedef void (*open_fn)(int);
typedef void (*close_fn)(void);
typedef void (*band_fn)(int, void *, void *);

#define ORIG_OPEN  ((open_fn) (uintptr_t)0x1956Bu)
#define ORIG_CLOSE ((close_fn)(uintptr_t)0x26996u)
#define ORIG_BAND  ((band_fn) (uintptr_t)0x1974Cu)

#define VGA       ((uint8_t *)(uintptr_t)0x000A0000u)
#define VGA_LEN   64000
#define DATO_LEN  8192
#define PREV_LEN  DATO_LEN

#define W32(x) (*(uint32_t *)(uintptr_t)(x))

static void install_hook(uint32_t addr, const void *dest)
{
    uint8_t *p   = (uint8_t *)(uintptr_t)addr;
    int32_t  rel = (int32_t)((const uint8_t *)dest - (p + 5));
    DWORD    old;

    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    p[0] = 0xE9;
    memcpy(p + 1, &rel, 4);
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
}
#define HOOK(addr, fn) install_hook((uint32_t)(addr), (const void *)(fn))

/* --- per-run block registry (normalises pointer arguments) ------------- */
#define MAXBLK 32
struct blk { uint8_t *p; size_t n; };

static struct blk g_blk[MAXBLK];
static int        g_nblk;

static void reg_block(void *p, size_t n)
{
    if (g_nblk >= MAXBLK) { printf("FAIL: block registry overflow\n"); exit(1); }
    g_blk[g_nblk].p = (uint8_t *)p;
    g_blk[g_nblk].n = n;
    g_nblk++;
}
static int norm_ptr(const void *p, int *off)
{
    int i;
    for (i = 0; i < g_nblk; i++)
        if ((const uint8_t *)p >= g_blk[i].p &&
            (const uint8_t *)p <  g_blk[i].p + g_blk[i].n) {
            *off = (int)((const uint8_t *)p - g_blk[i].p);
            return i;
        }
    *off = 0;
    return -1;
}

/* --- event log --------------------------------------------------------- */
enum { EV_ALLOC, EV_FREE, EV_BOX, EV_RES };
static const char *const kind_name[] = { "alloc", "free", "box_stage", "res_load" };
#define MAXEV 64
struct event { int kind; int v[8]; };

static struct event g_ev[MAXEV];
static int          g_nev;
static int          g_nalloc, g_nfree;
static int          g_band_events;         /* band calls seen (sanity) */

static void ev_log(int kind, int a, int b, int c, int d, int e, int f, int g)
{
    struct event *ev;
    if (g_nev >= MAXEV) { printf("FAIL: event log overflow\n"); exit(1); }
    ev = &g_ev[g_nev++];
    ev->kind = kind;
    ev->v[0] = a; ev->v[1] = b; ev->v[2] = c; ev->v[3] = d;
    ev->v[4] = e; ev->v[5] = f; ev->v[6] = g; ev->v[7] = 0;
}

/* --- the synthetic game world ----------------------------------------- */
static uint8_t *g_prev_dato;                 /* stale dword_53A85 pre-image */
static uint8_t *g_dato_template;             /* content the res stub returns */
static uint8_t *g_close_buf[3];              /* close-portrait screen pair   */
static uint8_t *g_band_dst, *g_band_src, *g_band_snap;

static uint32_t seed = 0xA5A5C0DEu;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}
static void fill_rand(uint8_t *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        p[i] = (uint8_t)rnd();
}

/* libc adapters over the game's CRT entry points (boxcheck pattern) */
static void *__cdecl stub_malloc(size_t n)
{
    void *p = malloc(n);
    reg_block(p, n);
    ev_log(EV_ALLOC, (int)n, 0, 0, 0, 0, 0, 0);
    g_nalloc++;
    return p;
}
static void __cdecl stub_free(void *p)
{
    int off, id = norm_ptr(p, &off);
    ev_log(EV_FREE, id, off, 0, 0, 0, 0, 0);
    g_nfree++;
    /* Deliberately *not* freed: the close case snapshots the buffers after
     * they are released, and boxcheck keeps blocks alive for the same reason. */
}

/* dlg_box_stage 0x168B6: record (surface,stride,x0,y0,cols,lines) */
static void __cdecl stub_box_stage(void *surface, int stride, int x0, int y0,
                                   int cols, int lines)
{
    int soff, sid = norm_ptr(surface, &soff);
    ev_log(EV_BOX, sid, soff, stride, x0, y0, cols, lines);
}

/* res_load 0x111BA: return a controlled DATO buffer, record (name, old, id) */
static void *__cdecl stub_res_load(const char *name, void *old, int index)
{
    int ooff, oid;
    void *p;

    if (!name || strcmp(name, "DATO.DAT")) {
        printf("FAIL: res_load name '%s'\n", name ? name : "(null)");
        return NULL;
    }
    p = malloc(DATO_LEN);
    memcpy(p, g_dato_template, DATO_LEN);
    reg_block(p, DATO_LEN);
    oid = norm_ptr(old, &ooff);
    ev_log(EV_RES, oid, ooff, index, 0, 0, 0, 0);
    return p;
}

static void begin_run(void)
{
    g_nev = g_nblk = g_nalloc = g_nfree = g_band_events = 0;
    reg_block(g_prev_dato, PREV_LEN);
}

/* --- capture ----------------------------------------------------------- */
struct cap {
    uint8_t  vga[VGA_LEN];
    uint8_t  buf[3][VGA_LEN];
    uint8_t  dato[DATO_LEN];
    uint32_t c67;
    int      nev, nalloc, nfree;
    struct event ev[MAXEV];
};

static void snap(struct cap *c)
{
    const uint32_t g[3] = { 0x53C5Bu, 0x53C5Fu, 0x53C63u };
    int i;

    memcpy(c->vga, VGA, VGA_LEN);
    for (i = 0; i < 3; i++) {
        void *p = (void *)(uintptr_t)W32(g[i]);
        if (p)
            memcpy(c->buf[i], p, VGA_LEN);
        else
            memset(c->buf[i], 0, VGA_LEN);
    }
    {
        void *p = (void *)(uintptr_t)W32(0x53A85u);
        if (p)
            memcpy(c->dato, p, DATO_LEN);
        else
            memset(c->dato, 0, DATO_LEN);
    }
    c->c67    = W32(0x53C67u);
    c->nev    = g_nev;
    c->nalloc = g_nalloc;
    c->nfree  = g_nfree;
    memcpy(c->ev, g_ev, sizeof g_ev);
}

static int failures;
static unsigned cases_run;

static int cmp_bytes(unsigned id, const char *what, const uint8_t *a,
                     const uint8_t *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        if (a[i] != b[i]) {
            printf("FAIL case %u: %s diff @+%u orig=%02X ours=%02X\n",
                   id, what, (unsigned)i, a[i], b[i]);
            failures++;
            return -1;
        }
    return 0;
}

static int cmp_cap(unsigned id, const char *phase, const struct cap *o,
                   const struct cap *t)
{
    int bad = 0, i, k;

    if (o->c67 != t->c67) {
        printf("FAIL case %u %s: dword_53C67 orig=%X ours=%X\n",
               id, phase, o->c67, t->c67);
        failures++; bad = 1;
    }
    if (o->nalloc != t->nalloc || o->nfree != t->nfree) {
        printf("FAIL case %u %s: alloc/free orig=%d/%d ours=%d/%d\n",
               id, phase, o->nalloc, o->nfree, t->nalloc, t->nfree);
        failures++; bad = 1;
    }
    if (o->nev != t->nev) {
        printf("FAIL case %u %s: event count orig=%d ours=%d\n",
               id, phase, o->nev, t->nev);
        failures++;
        for (i = 0; i < o->nev && i < t->nev; i++) {
            int j, diff = 0;
            for (j = 0; j < 8; j++)
                if (o->ev[i].v[j] != t->ev[i].v[j]) diff = 1;
            if (o->ev[i].kind != t->ev[i].kind || diff) {
                printf("  first differing event %d: orig %s(%d,%d,%d,%d,%d,%d,%d)"
                       " ours %s(%d,%d,%d,%d,%d,%d,%d)\n", i,
                       kind_name[o->ev[i].kind], o->ev[i].v[0], o->ev[i].v[1],
                       o->ev[i].v[2], o->ev[i].v[3], o->ev[i].v[4],
                       o->ev[i].v[5], o->ev[i].v[6],
                       kind_name[t->ev[i].kind], t->ev[i].v[0], t->ev[i].v[1],
                       t->ev[i].v[2], t->ev[i].v[3], t->ev[i].v[4],
                       t->ev[i].v[5], t->ev[i].v[6]);
                break;
            }
        }
        bad = 1;
    } else {
        for (i = 0; i < o->nev; i++) {
            int j, diff = 0;
            for (j = 0; j < 8; j++)
                if (o->ev[i].v[j] != t->ev[i].v[j]) diff = 1;
            if (o->ev[i].kind != t->ev[i].kind || diff) {
                printf("FAIL case %u %s: event %d orig %s(%d,%d,%d,%d,%d,%d,%d)"
                       " ours %s(%d,%d,%d,%d,%d,%d,%d)\n", id, phase, i,
                       kind_name[o->ev[i].kind], o->ev[i].v[0], o->ev[i].v[1],
                       o->ev[i].v[2], o->ev[i].v[3], o->ev[i].v[4],
                       o->ev[i].v[5], o->ev[i].v[6],
                       kind_name[t->ev[i].kind], t->ev[i].v[0], t->ev[i].v[1],
                       t->ev[i].v[2], t->ev[i].v[3], t->ev[i].v[4],
                       t->ev[i].v[5], t->ev[i].v[6]);
                failures++; bad = 1;
                break;
            }
        }
    }
    if (!bad)
        bad = cmp_bytes(id, "vga", o->vga, t->vga, VGA_LEN);
    for (k = 0; k < 3 && !bad; k++) {
        char what[32];
        sprintf(what, "buf%d", k);
        bad = cmp_bytes(id, what, o->buf[k], t->buf[k], VGA_LEN);
    }
    if (!bad)
        bad = cmp_bytes(id, "dato", o->dato, t->dato, DATO_LEN);
    return bad;
}

/* --- state builders ---------------------------------------------------- */
static void reset_open(uint32_t s)
{
    seed = s;
    fill_rand(VGA, VGA_LEN);
    fill_rand(g_prev_dato, PREV_LEN);
    fill_rand(g_dato_template, DATO_LEN);

    /* a valid DATO sub-image header at the resource's offset byte */
    g_dato_template[0] = 16;
    *(uint16_t *)(g_dato_template + 16) = 8;     /* w */
    *(uint16_t *)(g_dato_template + 18) = 8;     /* h */
    fill_rand(g_dato_template + 20, 64);         /* 64 literal pixels */

    W32(0x53C5Bu) = 0;
    W32(0x53C5Fu) = 0;
    W32(0x53C63u) = 0;
    W32(0x53A85u) = (uint32_t)(uintptr_t)g_prev_dato;
    W32(0x53C67u) = 0x12345678u;
}

static void reset_close(uint32_t s)
{
    int i;
    seed = s;
    for (i = 0; i < 3; i++)
        fill_rand(g_close_buf[i], VGA_LEN);
    fill_rand(VGA, VGA_LEN);
    fill_rand(g_prev_dato, PREV_LEN);
    fill_rand(g_dato_template, DATO_LEN);

    for (i = 0; i < 3; i++)
        reg_block(g_close_buf[i], VGA_LEN);

    W32(0x53C5Bu) = (uint32_t)(uintptr_t)g_close_buf[0];
    W32(0x53C5Fu) = (uint32_t)(uintptr_t)g_close_buf[1];
    W32(0x53C63u) = (uint32_t)(uintptr_t)g_close_buf[2];
    W32(0x53A85u) = (uint32_t)(uintptr_t)g_prev_dato;
    W32(0x53C67u) = 0x9017u;
}

/* --- cases ------------------------------------------------------------- */
static struct cap *g_co, *g_ct;      /* orig / ours capture */

static void test_open(unsigned id, int pid)
{
    uint32_t s = 0x0BEB0000u + id;

    begin_run(); reset_open(s); ORIG_OPEN(pid);   snap(g_co);
    begin_run(); reset_open(s); msg_open_portrait(pid); snap(g_ct);
    cases_run++;
    cmp_cap(id, "open", g_co, g_ct);
}

static void test_close(unsigned id)
{
    uint32_t s = 0xC105E000u + id;

    begin_run(); reset_close(s); ORIG_CLOSE();   snap(g_co);
    begin_run(); reset_close(s); msg_close_portrait(); snap(g_ct);
    cases_run++;
    cmp_cap(id, "close", g_co, g_ct);
}

static void test_open_close(unsigned id, int pid)
{
    uint32_t s = 0x0C105000u + id;
    (void)pid;

    begin_run(); reset_open(s); ORIG_OPEN(pid); ORIG_CLOSE();   snap(g_co);
    begin_run(); reset_open(s); msg_open_portrait(pid); msg_close_portrait();
    snap(g_ct);
    cases_run++;
    cmp_cap(id, "open+close", g_co, g_ct);
}

/* direct band test: both sides run on the same buffers, real memmove */
static void test_band(unsigned id, int y)
{
    uint8_t *pre_dst, *pre_src, *pre_snap, *pre_vga;
    uint8_t *out_dst, *out_src, *out_snap, *out_vga;
    uint32_t s = 0xBA4D0000u + id;

    pre_dst  = (uint8_t *)malloc(VGA_LEN);
    pre_src  = (uint8_t *)malloc(VGA_LEN);
    pre_snap = (uint8_t *)malloc(VGA_LEN);
    pre_vga  = (uint8_t *)malloc(VGA_LEN);
    out_dst  = (uint8_t *)malloc(VGA_LEN);
    out_src  = (uint8_t *)malloc(VGA_LEN);
    out_snap = (uint8_t *)malloc(VGA_LEN);
    out_vga  = (uint8_t *)malloc(VGA_LEN);

    seed = s;
    fill_rand(g_band_dst, VGA_LEN);
    fill_rand(g_band_src, VGA_LEN);
    fill_rand(g_band_snap, VGA_LEN);
    fill_rand(VGA, VGA_LEN);
    memcpy(pre_dst, g_band_dst, VGA_LEN);
    memcpy(pre_src, g_band_src, VGA_LEN);
    memcpy(pre_snap, g_band_snap, VGA_LEN);
    memcpy(pre_vga, VGA, VGA_LEN);

    W32(0x53C5Fu) = (uint32_t)(uintptr_t)g_band_snap;
    ORIG_BAND(y, g_band_dst, g_band_src);
    memcpy(out_dst, g_band_dst, VGA_LEN);
    memcpy(out_src, g_band_src, VGA_LEN);
    memcpy(out_snap, g_band_snap, VGA_LEN);
    memcpy(out_vga, VGA, VGA_LEN);

    memcpy(g_band_dst, pre_dst, VGA_LEN);
    memcpy(g_band_src, pre_src, VGA_LEN);
    memcpy(g_band_snap, pre_snap, VGA_LEN);
    memcpy(VGA, pre_vga, VGA_LEN);
    W32(0x53C5Fu) = (uint32_t)(uintptr_t)g_band_snap;
    msg_blit_band(y, g_band_dst, g_band_src);

    cases_run++;
    cmp_bytes(id, "band dst", out_dst, g_band_dst, VGA_LEN);
    cmp_bytes(id, "band src", out_src, g_band_src, VGA_LEN);
    cmp_bytes(id, "band snap", out_snap, g_band_snap, VGA_LEN);
    cmp_bytes(id, "band vga", out_vga, VGA, VGA_LEN);

    free(pre_dst); free(pre_src); free(pre_snap); free(pre_vga);
    free(out_dst); free(out_src); free(out_snap); free(out_vga);
}

int main(int argc, char **argv)
{
    static const int ids[] = { 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85,
                               0x00, 0xFF };
    static const int bands[] = { 0, 1, 85, 86, 112, 113, 114, 150, 199, 200,
                                 201, 255 };
    le_image le;
    int      applied = 0;
    unsigned i, c = 0;

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    g_prev_dato     = (uint8_t *)malloc(PREV_LEN);
    g_dato_template = (uint8_t *)malloc(DATO_LEN);
    g_band_dst      = (uint8_t *)malloc(VGA_LEN);
    g_band_src      = (uint8_t *)malloc(VGA_LEN);
    g_band_snap     = (uint8_t *)malloc(VGA_LEN);
    g_co            = (struct cap *)malloc(sizeof *g_co);
    g_ct            = (struct cap *)malloc(sizeof *g_ct);
    for (i = 0; i < 3; i++)
        g_close_buf[i] = (uint8_t *)malloc(VGA_LEN);
    if (!g_prev_dato || !g_dato_template || !g_band_dst || !g_band_src ||
        !g_band_snap || !g_co || !g_ct ||
        !g_close_buf[0] || !g_close_buf[1] || !g_close_buf[2]) return 2;

    HOOK(0x3706Eu, stub_malloc);          /* Watcom CRT malloc */
    HOOK(0x3776Eu, stub_free);            /* Watcom CRT free   */
    HOOK(0x168B6u, stub_box_stage);       /* dlg_box_stage     */
    HOOK(0x111BAu, stub_res_load);        /* res_load          */

    /* direct band coverage */
    for (i = 0; i < sizeof bands / sizeof bands[0] && !failures; i++) {
        int rep;
        for (rep = 0; rep < 20 && !failures; rep++)
            test_band(10000 + c++, bands[i]);
    }

    /* portrait open: id x random world */
    for (i = 0; i < sizeof ids / sizeof ids[0] && !failures; i++) {
        int rep;
        for (rep = 0; rep < 20 && !failures; rep++)
            test_open(20000 + c++, ids[i]);
    }
    (void)g_band_events;

    /* close: random screen pair */
    for (i = 0; i < 40 && !failures; i++)
        test_close(30000 + c++);

    /* open followed by close */
    for (i = 0; i < 5 && !failures; i++)
        test_open_close(40000 + c++, ids[i]);

    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
