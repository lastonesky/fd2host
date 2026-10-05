/* boxcheck.c - differential test for the dialogue-box open/close animation.
 *
 *   0x165AC dlg_open_box    0x16B43 dlg_close_box
 *   0x168B6 dlg_box_stage   0x1685C dlg_frame_tile
 *
 * The four routines are app-level: they read/write the original globals
 * (dword_51A83 / 53A18 / 53A81 / 53AB9 / 53ABD / 53C67) and sequence a
 * handful of services. Five of those services are hooked here to stubs:
 *
 *   0x3706E malloc / 0x3776E free   -> one libc heap for both runs
 *   0x3790A delay                   -> records the event + snapshots the VGA
 *   0x4E381 flush keys (BDA)        -> records the event
 *   0x12CEA portrait glide          -> records the event
 *
 * They are out of scope for this test (CRT heap / BIOS tick / BDA timing),
 * and hooking them does double duty: every call becomes a recorded event -
 * kind, arguments and the value of dword_51A83 at that moment - so the
 * comparison covers the *ordering* of the animation, not just its final
 * pixels. Every draw runs for real: the VGA frame after open, the VGA
 * frame after close, a full VGA snapshot at each delay, and the five
 * 26668-byte stage snapshots are all compared byte-for-byte.
 *
 * Per case the harness runs the original machine code, resets the world
 * identically, then runs src/game/dlg.c.
 *
 * Build: pwsh -File build.ps1 -Target boxcheck
 * Run   : build\boxcheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/dlg.h"

typedef void *(*open_fn)(int, int, int);
typedef void  (*close_fn)(void **, int);
typedef void  (*stage_fn)(void *, int, int, int, int, int);
typedef void  (*tile_fn)(void *, int, const void *, int);

#define ORIG_OPEN   ((open_fn)  (uintptr_t)0x165ACu)
#define ORIG_CLOSE  ((close_fn) (uintptr_t)0x16B43u)
#define ORIG_STAGE  ((stage_fn) (uintptr_t)0x168B6u)
#define ORIG_TILE   ((tile_fn)  (uintptr_t)0x1685Cu)

#define VW(x)       (*(const int32_t *)(uintptr_t)(x))
#define W(x)        (*(int32_t *)(uintptr_t)(x))
#define VGA         ((uint8_t *)(uintptr_t)0x000A0000u)
#define VGA_LEN     (320 * 200)
#define STAGE_SIZE  26668

/* --- libc adapters over the game's CRT entry points (rescheck pattern) -- */
static void *__cdecl stub_malloc(size_t n) { return malloc(n); }
static void  __cdecl stub_free(void *p)    { free(p); }

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

/* --- event log ---------------------------------------------------------- */
enum { EV_GLIDE, EV_DELAY, EV_FLUSH, EV_ALLOC, EV_FREE };
static const char *const kind_name[] = { "glide", "delay", "flush",
                                         "alloc", "free" };
#define MAXEV     512
#define MAXFRAMES 96

struct event { int kind; int a; int b; int flag; };

static struct event g_ev[MAXEV];
static int          g_nev;
static int          g_nframes;
static uint8_t     *g_frames;              /* MAXFRAMES * VGA_LEN, heap  */
static void        *g_ptrs[MAXEV];         /* alloc tags for FREE events */
static int          g_nptrs;
static int          g_nalloc, g_nfree;

static void ev_log(int kind, int a, int b)
{
    if (g_nev >= MAXEV) { printf("FAIL: event log overflow\n"); exit(1); }
    g_ev[g_nev].kind = kind;
    g_ev[g_nev].a    = a;
    g_ev[g_nev].b    = b;
    g_ev[g_nev].flag = VW(0x51A83);
    g_nev++;
}

static void __cdecl stub_delay(unsigned ms)
{
    if (g_nframes >= MAXFRAMES) { printf("FAIL: frame log overflow\n"); exit(1); }
    memcpy(g_frames + (size_t)g_nframes * VGA_LEN, VGA, VGA_LEN);
    ev_log(EV_DELAY, (int)ms, g_nframes);
    g_nframes++;
}
static void __cdecl stub_flush(void)         { ev_log(EV_FLUSH, 0, 0); }
static void __cdecl stub_glide(int x, int y) { ev_log(EV_GLIDE, x, y); }

static void *__cdecl stub_malloc_tagged(size_t n)
{
    void *p = malloc(n);
    if (g_nptrs >= MAXEV) { printf("FAIL: alloc tag overflow\n"); exit(1); }
    ev_log(EV_ALLOC, (int)n, g_nptrs);
    g_ptrs[g_nptrs++] = p;
    g_nalloc++;
    return p;
}
static void __cdecl stub_free_tagged(void *p)
{
    int i;
    /* Deliberately *not* freed: keeping every block alive makes each pointer
     * unique inside a run, so the ptr -> tag lookup can never be confused by
     * libc handing an address back out. The test process leaks a few MB. */
    for (i = g_nptrs - 1; i >= 0 && g_ptrs[i] != p; i--)
        ;
    ev_log(EV_FREE, (i >= 0) ? i : -1, 0);
    g_nfree++;
}

/* --- per-run capture ---------------------------------------------------- */
struct capture {
    uint8_t         vga[VGA_LEN];
    uint8_t         stages[5][STAGE_SIZE];
    int32_t         flag;
    void           *ret;
    int             nev, nframes, nalloc, nfree;
    struct event    ev[MAXEV];
};

static struct capture g_open[2], g_close[2];   /* [0] original, [1] ours   */
static uint8_t *g_framebuf[2];                 /* delay snapshots per run  */

static void begin_run(int which)
{
    g_nev = g_nframes = g_nptrs = g_nalloc = g_nfree = 0;
    g_frames = g_framebuf[which];
}

static void snapshot(struct capture *c, void *ret, int with_stages)
{
    int i;

    memcpy(c->vga, VGA, VGA_LEN);
    c->flag    = VW(0x51A83);
    c->ret     = ret;
    c->nev     = g_nev;
    c->nframes = g_nframes;
    c->nalloc  = g_nalloc;
    c->nfree   = g_nfree;
    memcpy(c->ev, g_ev, sizeof g_ev);

    for (i = 0; i < 5 && with_stages; i++) {
        void *p = ((void **)(uintptr_t)0x53A18u)[i];
        if (p)
            memcpy(c->stages[i], p, STAGE_SIZE);
        else
            memset(c->stages[i], 0, STAGE_SIZE);
    }
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

/* compare one phase (after open / after close) of both runs */
static int cmp_capture(unsigned id, const char *phase, const struct capture *o,
                       const struct capture *t, const uint8_t *fo,
                       const uint8_t *ft, int with_stages)
{
    int bad = 0;
    int i;

    if (with_stages && o->ret != t->ret) {
        printf("FAIL case %u: open returned %p / %p\n", id, o->ret, t->ret);
        failures++; bad = 1;
    }
    if (with_stages && o->ret != (void *)(uintptr_t)0x53A18u) {
        printf("FAIL case %u: open did not return &dword_53A18 (%p)\n",
               id, o->ret);
        failures++; bad = 1;
    }
    if (o->flag != t->flag) {
        printf("FAIL case %u: %s dword_51A83 orig=%d ours=%d\n",
               id, phase, o->flag, t->flag);
        failures++; bad = 1;
    }
    if (with_stages)
        for (i = 0; i < 5 && !bad; i++)
            bad = cmp_bytes(id, "stage snapshot", o->stages[i], t->stages[i],
                            STAGE_SIZE);
    if (!bad)
        bad = cmp_bytes(id, phase, o->vga, t->vga, VGA_LEN);

    if (o->nev != t->nev) {
        printf("FAIL case %u: %s event count orig=%d ours=%d\n",
               id, phase, o->nev, t->nev);
        failures++; bad = 1;
    } else {
        for (i = 0; i < o->nev; i++) {
            const struct event *eo = &o->ev[i], *et = &t->ev[i];
            if (eo->kind != et->kind || eo->a != et->a || eo->b != et->b ||
                eo->flag != et->flag) {
                printf("FAIL case %u: %s event %d orig=%s(%d,%d) flag=%d "
                       "ours=%s(%d,%d) flag=%d\n", id, phase, i,
                       kind_name[eo->kind], eo->a, eo->b, eo->flag,
                       kind_name[et->kind], et->a, et->b, et->flag);
                failures++; bad = 1;
                break;
            }
            if (eo->kind == EV_DELAY && !bad)
                bad = cmp_bytes(id, "frame at delay",
                                fo + (size_t)eo->b * VGA_LEN,
                                ft + (size_t)et->b * VGA_LEN, VGA_LEN);
        }
    }
    if (!with_stages) {                    /* after close: everything freed */
        if (o->nalloc != o->nfree) {
            printf("FAIL case %u: original leaked %d block(s) (%d/%d)\n",
                   id, o->nalloc - o->nfree, o->nalloc, o->nfree);
            failures++; bad = 1;
        }
        if (t->nalloc != t->nfree) {
            printf("FAIL case %u: translation leaked %d block(s) (%d/%d)\n",
                   id, t->nalloc - t->nfree, t->nalloc, t->nfree);
            failures++; bad = 1;
        }
    }
    return bad;
}

/* --- synthetic game state ---------------------------------------------- */
#define FRAME_SLOTS 18                  /* tile offsets 0..17 at +6        */

static uint32_t seed = 0xB0B0C4A7u;
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

static uint8_t *g_frame;                 /* box frame resource            */

/* [u16 w][u16 h][pixels] blocks, dword offset table at +6. The bottom
 * border row (tiles 3/4/7/8/12) is 3-4 px tall in the real art; tile 0 is
 * the sweep sprite and is also read as a sign-extended word at +6. */
static void build_frame(void)
{
    uint32_t off[FRAME_SLOTS];
    int      i;
    size_t   pos = 6 + 4 * FRAME_SLOTS;

    fill_rand(g_frame, 16 * 1024);
    for (i = 0; i < FRAME_SLOTS; i++) {
        unsigned w = 1 + (unsigned)rnd() % 16;
        unsigned h = 1 + (unsigned)rnd() % 16;

        if (i == 3 || i == 4 || i == 7 || i == 8 || i == 12)
            h = 1 + (unsigned)rnd() % 4;
        off[i] = (uint32_t)pos;
        *(uint16_t *)(g_frame + pos)       = (uint16_t)w;
        *(uint16_t *)(g_frame + pos + 2)   = (uint16_t)h;
        fill_rand(g_frame + pos + 4, w * h);
        pos += 4 + w * h;
    }
    for (i = 0; i < FRAME_SLOTS; i++)
        *(uint32_t *)(g_frame + 6 + 4 * i) = off[i];
}

static void reset_state(int box, int pw, int ph, uint32_t vga_seed)
{
    void **snaps = (void **)(uintptr_t)0x53A18u;
    int    i;

    *(void **)(uintptr_t)0x53A81u = g_frame;
    W(0x53AB9) = pw;                        /* portrait cols / rows       */
    W(0x53ABD) = ph;
    W(0x53C67) = box;
    W(0x51A83) = 5;                         /* not 0 and not 6: observable */
    for (i = 0; i < 5; i++)
        snaps[i] = NULL;

    seed = vga_seed;
    fill_rand(VGA, VGA_LEN);
}

/* --- one open+close differential case ---------------------------------- */
static void test_open_close(unsigned id, int box, int rows, int pw, int ph,
                            int fx, int fy)
{
    uint32_t vga_seed = 0x5EED0000u + id;
    void    *ro, *rt;
    int      bad = 0;

    build_frame();

    /* original machine code */
    reset_state(box, pw, ph, vga_seed);
    begin_run(0);
    ro = ORIG_OPEN(fx, fy, rows);
    snapshot(&g_open[0], ro, 1);
    ORIG_CLOSE(ro, rows);
    snapshot(&g_close[0], NULL, 0);

    /* translation */
    reset_state(box, pw, ph, vga_seed);
    begin_run(1);
    rt = dlg_open_box(fx, fy, rows);
    snapshot(&g_open[1], rt, 1);
    dlg_close_box(rt, rows);
    snapshot(&g_close[1], NULL, 0);

    cases_run++;
    bad = cmp_capture(id, "open", &g_open[0], &g_open[1],
                      g_framebuf[0], g_framebuf[1], 1);
    if (!bad)
        cmp_capture(id, "close", &g_close[0], &g_close[1],
                    g_framebuf[0], g_framebuf[1], 0);
}

/* --- direct stage / tile cases -----------------------------------------
 *
 * These run against a padded scratch surface instead of the VGA: the
 * degenerate parameter ranges (cols < 2, lines < 2) make the original
 * compute *negative* tile offsets - real code never does that, but the C
 * must match the arithmetic anyway, and a 128 KiB scratch block absorbs it
 * without an access violation. */
#define SCRATCH      131072                 /* 128 KiB scratch surface    */
#define SCRATCH_BASE 16384

static uint8_t *g_scratch;
static uint8_t *g_scratch2;                 /* pre-image during the diff   */
static uint8_t *g_scratch_copy;             /* the original's result       */

static void test_stage(unsigned id, int x0, int y0, int cols, int lines)
{
    uint8_t *base = g_scratch + SCRATCH_BASE;

    build_frame();
    *(void **)(uintptr_t)0x53A81u = g_frame;
    seed = 0x57A9E000u + id;
    fill_rand(g_scratch, SCRATCH);
    memcpy(g_scratch2, g_scratch, SCRATCH);
    ORIG_STAGE(base, 320, x0, y0, cols, lines);
    memcpy(g_scratch_copy, g_scratch, SCRATCH);

    memcpy(g_scratch, g_scratch2, SCRATCH);
    dlg_box_stage(base, 320, x0, y0, cols, lines);
    cases_run++;
    cmp_bytes(id, "box_stage", g_scratch_copy, g_scratch, SCRATCH);
}

static void test_tile(unsigned id, int x, int y, int idx)
{
    uint8_t *base = g_scratch + SCRATCH_BASE;

    build_frame();
    *(void **)(uintptr_t)0x53A81u = g_frame;
    seed = 0x711E0000u + id;
    fill_rand(g_scratch, SCRATCH);
    memcpy(g_scratch2, g_scratch, SCRATCH);
    ORIG_TILE(base + x + 320 * y, 320, g_frame, idx);
    memcpy(g_scratch_copy, g_scratch, SCRATCH);

    memcpy(g_scratch, g_scratch2, SCRATCH);
    dlg_frame_tile(base + x + 320 * y, 320, g_frame, idx);
    cases_run++;
    cmp_bytes(id, "frame_tile", g_scratch_copy, g_scratch, SCRATCH);
}

int main(int argc, char **argv)
{
    static const int boxes[]  = { 0x728, 0x9017, 0x3210 };
    static const int rowss[]  = { 0, 2, 7, 112 };
    static const int portrait[][2] = { { 0, 0 }, { 1, 1 }, { 3, 2 },
                                       { 6, 4 }, { 2, 5 } };
    le_image le;
    int      applied = 0;
    unsigned i;
    int      b, r, p;

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    g_frame       = (uint8_t *)malloc(16 * 1024);
    g_framebuf[0] = (uint8_t *)malloc((size_t)MAXFRAMES * VGA_LEN);
    g_framebuf[1] = (uint8_t *)malloc((size_t)MAXFRAMES * VGA_LEN);
    g_scratch     = (uint8_t *)malloc(SCRATCH);
    g_scratch2    = (uint8_t *)malloc(SCRATCH);
    g_scratch_copy = (uint8_t *)malloc(SCRATCH);
    if (!g_frame || !g_framebuf[0] || !g_framebuf[1] || !g_scratch ||
        !g_scratch2 || !g_scratch_copy) return 2;

    HOOK(0x3706Eu, stub_malloc_tagged);     /* Watcom CRT malloc           */
    HOOK(0x3776Eu, stub_free_tagged);       /* Watcom CRT free             */
    HOOK(0x3790Au, stub_delay);             /* j___delay                   */
    HOOK(0x4E381u, stub_flush);             /* BDA keyboard flush          */
    HOOK(0x12CEAu, stub_glide);             /* portrait glide (0x12CEA)    */

    /* full open + close: box position x portrait size x rows             */
    for (b = 0; b < 3 && !failures; b++)
        for (r = 0; r < 4 && !failures; r++)
            for (p = 0; p < 5 && !failures; p++) {
                unsigned id = 1000 + (unsigned)(b * 20 + r * 5 + p);
                seed = 0xC0FFEEu + id;
                test_open_close(id, boxes[b], rowss[r],
                                portrait[p][0], portrait[p][1],
                                (int)(rnd() % 32), (int)(rnd() % 32));
            }

    /* direct dlg_box_stage / dlg_frame_tile coverage                     */
    for (i = 0; i < 60 && !failures; i++) {
        seed = 0x57A90000u + i;
        test_stage(3000 + i, (int)(rnd() % 6) - 3, (int)(rnd() % 61) - 8,
                   (int)(rnd() % 21), (int)(rnd() % 7));
    }
    for (i = 0; i < 120 && !failures; i++) {
        seed = 0x711E0000u + i;
        test_tile(4000 + i, (int)(rnd() % 301) - 64,
                  (int)(rnd() % 181) - 32, (int)(rnd() % FRAME_SLOTS));
    }

    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
