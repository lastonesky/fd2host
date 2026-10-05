/* keycheck.c - differential test for dlg_wait_key (original 0x16C57).
 *
 * The wait loop blocks on the BIOS keyboard and animates on the BIOS tick,
 * which makes a naive "run it twice" test non-deterministic and, in a bare
 * harness, impossible at all: the tick (BDA 0x46C) and the key ring
 * (0x41A/0x41C) live in the first 64 KiB, which Windows will not map.
 *
 * The harness therefore does what the host does plus two hooks:
 *
 *   1. a 64 KiB low-memory mirror at 0x70000 (inside the window le.c
 *      already reserved+committed) and the same immediate-operand redirect
 *      src/dos.c performs: `mov/push reg, imm32` with imm in 0x400..0x4FF
 *      is rewritten to mirror+imm (65 references in the FD2 build), so the
 *      *original machine code* reads the mirror;
 *   2. hooks that make the world deterministic and observable:
 *        0x4E31C palette step -> tick++, record event, snapshot the VGA,
 *                       and on the Nth call make a key pending (tail!=head)
 *        0x370F0 int386      -> scripted INT 16h result (AH in -> AH out),
 *                       records (intno, in-AH)
 *
 * Both runs (original and translation) see exactly the same tick values,
 * the same key and the same service sequence, so the comparison covers the
 * loop structure itself: VGA + a VGA snapshot at every palette step + the
 * event log + the final BDA tick, the INT 16h register word (0x53A8D) and
 * the rand state (word_627B8).
 *
 * The rand seed is picked per case so the first mouth hold (rand%30+2) is
 * 2..4 ticks - with a 12..16 step limit that guarantees both the mouth
 * open and the mouth close paths run.
 *
 * Build: pwsh -File build.ps1 -Target keycheck
 * Run   : build\keycheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/dlg.h"

typedef void (*wait_fn)(int);
#define ORIG_WAIT ((wait_fn)(uintptr_t)0x16C57u)
#define ORIG_RAND ((int (*)(void))(uintptr_t)0x4EBE3u)   /* 16-bit ROL rand */

#define VW(a)       (*(const uint32_t *)(uintptr_t)(a))
#define VGA         ((uint8_t *)(uintptr_t)0x000A0000u)
#define VGA_LEN     (320 * 200)
#define MIRROR      0x00070000u            /* low-mem window, already mapped */

#define G_TICK      (MIRROR + 0x46C)
#define G_HEAD      (MIRROR + 0x41A)
#define G_TAIL      (MIRROR + 0x41C)

/* --- low-memory operand redirect (same algorithm as dos.c) -------------- */
static int patch_lowmem_refs(le_image *le, uint32_t mirror)
{
    unsigned i, off;
    int      n = 0;

    for (i = 0; i < le->object_count; i++) {
        uint8_t *base;
        uint32_t size;

        if (!(le->objects[i].flags & 0x04))
            continue;
        base = (uint8_t *)(uintptr_t)le->objects[i].base;
        size = le->objects[i].vsize;
        for (off = 0; off + 5 <= size; off++) {
            uint8_t  op  = base[off];
            uint32_t imm;
            int      is_mov = (op >= 0xB8 && op <= 0xBF);

            if (!is_mov && op != 0x68)
                continue;
            imm = (uint32_t)base[off + 1] | ((uint32_t)base[off + 2] << 8) |
                  ((uint32_t)base[off + 3] << 16) | ((uint32_t)base[off + 4] << 24);
            if (imm >= 0x400 && imm < 0x500) {
                uint32_t fixed = mirror + imm;
                base[off + 1] = (uint8_t)fixed;
                base[off + 2] = (uint8_t)(fixed >> 8);
                base[off + 3] = (uint8_t)(fixed >> 16);
                base[off + 4] = (uint8_t)(fixed >> 24);
                n++;
            }
        }
    }
    return n;
}

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

/* --- deterministic world driver ---------------------------------------- */
enum { EV_PALETTE, EV_INT386 };
static const char *const kind_name[] = { "palette", "int386" };
#define MAXEV     64
#define MAXFRAMES 64

struct event { int kind; int a; int b; };

static struct event g_ev[MAXEV];
static int          g_nev;
static int          g_nframes;
static uint8_t     *g_frames;              /* MAXFRAMES * VGA_LEN          */
static int          g_pal_calls;
static int          g_pal_limit;           /* key becomes pending on call N */
static int          g_scan;                /* scripted INT 16h AH result    */

static void ev_log(int kind, int a, int b)
{
    if (g_nev >= MAXEV) { printf("FAIL: event log overflow\n"); exit(1); }
    g_ev[g_nev].kind = kind;
    g_ev[g_nev].a    = a;
    g_ev[g_nev].b    = b;
    g_nev++;
}

static void __cdecl stub_palette(void)
{
    uint32_t tick = VW(G_TICK) + 1;

    *(uint32_t *)(uintptr_t)G_TICK = tick;
    if (g_nframes >= MAXFRAMES) { printf("FAIL: frame log overflow\n"); exit(1); }
    memcpy(g_frames + (size_t)g_nframes * VGA_LEN, VGA, VGA_LEN);
    ev_log(EV_PALETTE, (int)tick, g_nframes);
    g_nframes++;

    if (++g_pal_calls == g_pal_limit)     /* make a key pending: tail!=head */
        *(uint16_t *)(uintptr_t)G_TAIL =
            (uint16_t)(*(uint16_t *)(uintptr_t)G_HEAD + 2);
    if (g_pal_calls > 512) { printf("FAIL: wait loop never ended\n"); exit(1); }
}

static int __cdecl stub_int386(int intno, const void *in, void *out)
{
    const uint8_t *ri = (const uint8_t *)in;
    uint8_t       *ro = (uint8_t *)out;

    ev_log(EV_INT386, intno, ri[1]);       /* intno + AH as passed in       */
    ro[0] = 0;                             /* AL                            */
    ro[1] = (uint8_t)g_scan;               /* AH = scripted scan code       */
    return 0;
}

/* --- captures ----------------------------------------------------------- */
struct capture {
    uint8_t         vga[VGA_LEN];
    uint32_t        tick;                  /* final BDA 0x46C               */
    uint32_t        regs;                  /* dword at 0x53A8D (INT16 out)  */
    uint16_t        rand_state;            /* word_627B8                    */
    int32_t         a51, boxpos;
    int             nev, nframes;
    struct event    ev[MAXEV];
};

static struct capture g_cap[2];            /* [0] original, [1] translation */
static uint8_t *g_framebuf[2];             /* delay snapshots per run       */

static void begin_run(int which)
{
    g_nev = g_nframes = g_pal_calls = 0;
    g_frames = g_framebuf[which];

    /* reset the world */
    {
        uint8_t *m = (uint8_t *)(uintptr_t)MIRROR;
        memset(m, 0, 0x500);
        *(uint16_t *)(m + 0x41A) = 0x41E;  /* head: ring empty              */
        *(uint16_t *)(m + 0x41C) = 0x41E;  /* tail                          */
        *(uint32_t *)(m + 0x46C) = 0;      /* tick                          */
    }
}

static void snapshot(struct capture *c)
{
    memcpy(c->vga, VGA, VGA_LEN);
    c->tick      = VW(G_TICK);
    c->regs      = VW(0x53A8D);
    c->rand_state = *(const uint16_t *)(uintptr_t)0x627B8u;
    c->a51       = *(const int32_t *)(uintptr_t)0x53A51u;
    c->boxpos    = *(const int32_t *)(uintptr_t)0x53C67u;
    c->nev       = g_nev;
    c->nframes   = g_nframes;
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

static int cmp_capture(unsigned id, const struct capture *o,
                       const struct capture *t, const uint8_t *fo,
                       const uint8_t *ft)
{
    int bad = 0;
    int i;

    if (o->tick != t->tick) {
        printf("FAIL case %u: final tick orig=%u ours=%u\n",
               id, o->tick, t->tick);
        failures++; bad = 1;
    }
    if (o->regs != t->regs) {
        printf("FAIL case %u: word_53A8D orig=%08X ours=%08X\n",
               id, o->regs, t->regs);
        failures++; bad = 1;
    }
    if (o->rand_state != t->rand_state) {
        printf("FAIL case %u: word_627B8 orig=%04X ours=%04X\n",
               id, o->rand_state, t->rand_state);
        failures++; bad = 1;
    }
    if (o->a51 != t->a51 || o->boxpos != t->boxpos) {
        printf("FAIL case %u: globals changed (a51 %d/%d box %d/%d)\n",
               id, o->a51, t->a51, o->boxpos, t->boxpos);
        failures++; bad = 1;
    }
    if (!bad)
        bad = cmp_bytes(id, "final VGA", o->vga, t->vga, VGA_LEN);

    if (o->nev != t->nev) {
        printf("FAIL case %u: event count orig=%d ours=%d\n",
               id, o->nev, t->nev);
        failures++; bad = 1;
    } else {
        for (i = 0; i < o->nev; i++) {
            const struct event *eo = &o->ev[i], *et = &t->ev[i];
            if (eo->kind != et->kind || eo->a != et->a || eo->b != et->b) {
                printf("FAIL case %u: event %d orig=%s(%d,%d) "
                       "ours=%s(%d,%d)\n", id, i,
                       kind_name[eo->kind], eo->a, eo->b,
                       kind_name[et->kind], et->a, et->b);
                failures++; bad = 1;
                break;
            }
            if (eo->kind == EV_PALETTE && !bad)
                bad = cmp_bytes(id, "frame at palette step",
                                fo + (size_t)eo->b * VGA_LEN,
                                ft + (size_t)et->b * VGA_LEN, VGA_LEN);
        }
    }
    return bad;
}

/* --- synthetic game state ---------------------------------------------- */
#define FRAME_SLOTS 20                  /* tiles 0..19, table at +6        */
#define DATO_SLOTS  4                   /* mouth uses sub-images 0 and 3   */

static uint32_t seed = 0x9E3779B9u;
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
static uint8_t *g_dato;                  /* DATO sub-images (dword table) */

/* [u16 w][u16 h][rle2 stream] blocks, dword offset table at +6. */
static void build_frame(void)
{
    uint32_t off[FRAME_SLOTS];
    int      i;
    size_t   pos = 6 + 4 * FRAME_SLOTS;

    fill_rand(g_frame, 16 * 1024);
    for (i = 0; i < FRAME_SLOTS; i++) {
        unsigned w = 1 + (unsigned)rnd() % 16;
        unsigned h = 1 + (unsigned)rnd() % 16;

        off[i] = (uint32_t)pos;
        *(uint16_t *)(g_frame + pos)       = (uint16_t)w;
        *(uint16_t *)(g_frame + pos + 2)   = (uint16_t)h;
        fill_rand(g_frame + pos + 4, w * h);
        pos += 4 + w * h;
    }
    for (i = 0; i < FRAME_SLOTS; i++)
        *(uint32_t *)(g_frame + 6 + 4 * i) = off[i];
}

/* dword offset table at +0 (that is what 0x16559 indexes), rle2 sub-images */
static void build_dato(void)
{
    uint32_t off[DATO_SLOTS];
    int      i;
    size_t   pos = 4 * DATO_SLOTS;

    fill_rand(g_dato, 8 * 1024);
    for (i = 0; i < DATO_SLOTS; i++) {
        unsigned w = 1 + (unsigned)rnd() % 40;
        unsigned h = 1 + (unsigned)rnd() % 30;
        unsigned total = w * h, done = 0;

        off[i] = (uint32_t)pos;
        *(uint16_t *)(g_dato + pos)       = (uint16_t)w;
        *(uint16_t *)(g_dato + pos + 2)   = (uint16_t)h;
        pos += 4;
        while (done < total) {
            unsigned left = total - done;
            if ((rnd() & 1) || left == 1) {
                g_dato[pos++] = (uint8_t)(rnd() % 0xC1);
                done++;
            } else {
                unsigned maxlen = left < 63 ? left : 63;
                unsigned len = 1 + (unsigned)rnd() % maxlen;
                g_dato[pos++] = (uint8_t)(0xC0 + len);
                g_dato[pos++] = (uint8_t)rnd();
                done += len;
            }
        }
    }
    for (i = 0; i < DATO_SLOTS; i++)
        ((uint32_t *)g_dato)[i] = off[i];
}

static void reset_state(int box, int a51, uint32_t vga_seed)
{
    *(void **)(uintptr_t)0x53A81u   = g_frame;
    *(void **)(uintptr_t)0x53A85u   = g_dato;
    *(int32_t *)(uintptr_t)0x53C67u = box;
    *(int32_t *)(uintptr_t)0x53A51u = a51;
    *(uint32_t *)(uintptr_t)0x53A8Du = 0;

    seed = vga_seed;
    fill_rand(VGA, VGA_LEN);
}

/* pick a word_627B8 seed whose first rand%30 is `mod` so the first mouth
 * hold (mod+2 ticks) opens *and* closes inside the step limit */
static uint16_t probe_seed(unsigned mod)
{
    uint32_t s;

    for (s = 1; s < 200000; s++) {
        *(uint16_t *)(uintptr_t)0x627B8u = (uint16_t)s;
        if ((unsigned)ORIG_RAND() % 30 == mod)
            return (uint16_t)s;
    }
    printf("FAIL: no seed for mod %u\n", mod);
    exit(1);
}

/* --- one differential case --------------------------------------------- */
static void test_wait(unsigned id, int speaker, int box, int a51, int limit,
                      int scan, uint16_t rand_seed, uint32_t vga_seed)
{
    struct capture *co = &g_cap[0], *ct = &g_cap[1];

    reset_state(box, a51, vga_seed);
    *(uint16_t *)(uintptr_t)0x627B8u = rand_seed;
    g_pal_limit = limit;
    g_scan      = scan;
    begin_run(0);
    ORIG_WAIT(speaker);
    snapshot(co);

    reset_state(box, a51, vga_seed);
    *(uint16_t *)(uintptr_t)0x627B8u = rand_seed;
    g_pal_limit = limit;
    g_scan      = scan;
    begin_run(1);
    dlg_wait_key(speaker);
    snapshot(ct);

    cases_run++;
    if (!cmp_capture(id, co, ct, g_framebuf[0], g_framebuf[1]))
        return;
}

int main(int argc, char **argv)
{
    static const int   boxes[] = { 0x728, 0x9017, 0x3210 };
    static const int   scans[] = { 0x1C, 0xE0, 0x52, 0x53, 0x48 };
    le_image le;
    int      applied = 0;
    int      nref;
    unsigned i;

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    nref = patch_lowmem_refs(&le, MIRROR);
    printf("keycheck: redirected %d low-memory references to 0x%X\n",
           nref, MIRROR);
    if (nref < 40) { printf("FAIL: BDA redirect looks incomplete\n"); return 2; }

    g_frame      = (uint8_t *)malloc(16 * 1024);
    g_dato       = (uint8_t *)malloc(8 * 1024);
    g_framebuf[0] = (uint8_t *)malloc((size_t)MAXFRAMES * VGA_LEN);
    g_framebuf[1] = (uint8_t *)malloc((size_t)MAXFRAMES * VGA_LEN);
    if (!g_frame || !g_dato || !g_framebuf[0] || !g_framebuf[1]) return 2;

    HOOK(0x4E31Cu, stub_palette);         /* palette step = tick driver     */
    HOOK(0x370F0u, stub_int386);          /* INT 16h AH=10h result          */

    for (i = 0; i < 100 && !failures; i++) {
        unsigned id = 5000 + i;
        int      speaker = (int)(rnd() % 2);
        int      box     = boxes[rnd() % 3];
        int      a51     = (int)(rnd() % 2);
        int      limit   = (rnd() & 1) ? 12 : 16;
        int      scan    = scans[rnd() % 5];
        unsigned mod     = (unsigned)(rnd() % 3);
        uint32_t vseed   = 0x1CEB0000u + id;
        uint16_t rseed;

        seed = 0xC0FFEEu + id;            /* frame/dato derivation          */
        build_frame();
        build_dato();
        rseed = probe_seed(mod);          /* first mouth hold = mod+2       */
        test_wait(id, speaker, box, a51, limit, scan, rseed, vseed);
    }

    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
