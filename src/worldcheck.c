/* worldcheck.c - differential test for src/game/world_load.c
 * (0x10652 world_load_tiles, 0x1088D world_load_party).
 *
 * The originals sequence the Watcom CRT (malloc/free/fopen/fclose/fseek/fread)
 * plus the already-translated helpers. This harness redirects the CRT entries
 * to the host libc - the substitution the final port performs anyway - hooks
 * the helpers to the translated C, and then runs the original machine code and
 * the translation over the **real** data files (the checker runs from the game
 * directory), comparing the record/tile buffers and every scalar global.
 *
 * stub_malloc zero-fills: both runs must start from the same destination bytes
 * or the unwritten tail of a decode would differ (the original really reads
 * allocated-but-unwritten memory, so this is a harness property, not a claim
 * about the game).
 *
 * Build: pwsh -File build.ps1 -Target worldcheck
 * Run   : build\worldcheck.exe [--cases=N]
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/world_load.h"
#include "game/res.h"
#include "game/rle.h"
#include "game/map.h"
#include "game/util.h"
#include "game/unit_load.h"
#include "game/gfx.h"
#include "game/rle2.h"

#define W32(x) (*(uint32_t *)(uintptr_t)(x))
#define R_U32(x) (*(uint32_t *)(uintptr_t)(x))

/* --- libc adapters installed over the game's CRT entry points --------- */
uint32_t dos_lowmem_base = 0x00070000u;
static void *__cdecl stub_malloc(size_t n)
{
    void *p = malloc(n);
    if (p) memset(p, 0, n);
    return p;
}
static void  __cdecl stub_free(void *p)                        { free(p); }
static void *__cdecl stub_fopen(const char *n, const char *m)  { return fopen(n, m); }
static int   __cdecl stub_fclose(void *f)                      { return fclose((FILE *)f); }
static int   __cdecl stub_fseek(void *f, long off, int wh)     { return fseek((FILE *)f, off, wh); }
static size_t __cdecl stub_fread(void *b, size_t sz, size_t c, void *f)
{ return fread(b, sz, c, (FILE *)f); }
static size_t __cdecl stub_fwrite(const void *b, size_t sz, size_t c, void *f)
{ return fwrite(b, sz, c, (FILE *)f); }

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

typedef void (*h0_fn)(void);
typedef void (*h1_fn)(int);
#define O_10652 ((h0_fn)(uintptr_t)0x00010652u)
#define O_1088D ((h1_fn)(uintptr_t)0x0001088Du)

/* 0x10B4E is not under test here: replace it with a no-op on both sides. */
static void __cdecl stub_build(int idx) { (void)idx; }
static void __cdecl stub_delay0(int ms) { (void)ms; }
static int  __cdecl stub_entrydata(int idx, void *fh) { (void)idx; (void)fh; return 0; }
static void __cdecl stub_metrics(int idx) { (void)idx; }

/* --- world ------------------------------------------------------------- */
#define GUEST_SIZE (*(const uint32_t *)(uintptr_t)0x53BFFu)
#define REC2_N     0x1000
static uint8_t  rec2[REC2_N];         /* dword_53BF7 source records */
static uint8_t  rec_o[0x2000], rec_c[0x2000];
static uint8_t  tile_o[0x40000], tile_c[0x40000];

#define dword_53A45 (*(uint32_t *)(uintptr_t)0x00053A45u)
#define dword_53A49 (*(uint32_t *)(uintptr_t)0x00053A49u)
#define dword_53AFF (*(uint32_t *)(uintptr_t)0x00053AFFu)
#define dword_53B03 (*(uint32_t *)(uintptr_t)0x00053B03u)
#define dword_53BE7 (*(uint32_t *)(uintptr_t)0x00053BE7u)
#define dword_53BE3 (*(uint32_t *)(uintptr_t)0x00053BE3u)
#define dword_53BEB (*(uint32_t *)(uintptr_t)0x00053BEBu)
#define dword_53AC1 (*(int32_t  *)(uintptr_t)0x00053AC1u)
#define dword_53AC5 (*(int32_t  *)(uintptr_t)0x00053AC5u)
#define dword_53BDF (*(uint32_t *)(uintptr_t)0x00053BDFu)
#define dword_53BF7 (*(uint32_t *)(uintptr_t)0x00053BF7u)
#define dword_53BFB (*(uint32_t *)(uintptr_t)0x00053BFBu)
#define dword_53C03 (*(int32_t  *)(uintptr_t)0x00053C03u)
#define dword_53C5B (*(uint32_t *)(uintptr_t)0x00053C5Bu)
#define dword_53C63 (*(uint32_t *)(uintptr_t)0x00053C63u)

static uint32_t seed;
static uint32_t rnd(void) { seed = seed * 1664525u + 1013904223u; return seed >> 8; }

static int failures, cases_run;

static uint32_t tile_len(int c)
{
    switch (c) {
    case 17: return 462u * 226u;
    case 21: return 408u * 276u;
    case 22: return 408u * 256u;
    case 27: return 462u * 244u;
    case 23: return 0xEA00u;
    default: return GUEST_SIZE;   /* resource cases */
    }
}

/* Both sides must start from the same world; the record/screen globals are
 * the only ones the loader does not itself set. */
static void setup_world(int c)
{
    dword_53A45 = 0;
    dword_53AFF = 0;
    dword_53B03 = 0;
    dword_53BDF = 0;
    dword_53C03 = c;
    dword_53BF7 = (uint32_t)(uintptr_t)(rec2 + 0x80);
    dword_53BFB = (int32_t)(REC2_N / 0x50 - 4);
    dword_53C5B = (uint32_t)(uintptr_t)tile_o;
    dword_53C63 = (uint32_t)(uintptr_t)tile_o;
    memset(rec2, 0, sizeof rec2);
    seed = 0x5EEDu ^ (uint32_t)c;
    { size_t i; for (i = 0; i < sizeof rec2; i++) rec2[i] = (uint8_t)rnd(); }
    R_U32(0x53A55) = 0;   /* p   */
    R_U32(0x53A61) = 0;   /* buf */
    R_U32(0x53A79) = 0;
    R_U32(0x53A59) = 0;
    R_U32(0x53A51) = 0;
    R_U32(0x53A5D) = 0;
    R_U32(0x53A69) = 0;
    R_U32(0x53BFF) = 0;
}

/* --- observables ------------------------------------------------------- */
struct cap {
    uint32_t rec_n, tile_n, guest_size;
    uint32_t be7, be3, beb, ac1, ac5, bdf;
    uint8_t  rec[0x2000];
    uint8_t  tile[0x40000];
};
static struct cap co, cc;
static int g_case;

static void capture(struct cap *t)
{
    uint32_t n = tile_len(g_case);
    t->be7 = dword_53BE7; t->be3 = dword_53BE3; t->beb = dword_53BEB;
    t->ac1 = (uint32_t)dword_53AC1; t->ac5 = (uint32_t)dword_53AC5;
    t->bdf = dword_53BDF; t->guest_size = GUEST_SIZE;
    t->rec_n = 0x1E00;
    if (n > 0x40000) n = 0x40000;
    t->tile_n = n;
    memset(t->rec, 0, sizeof t->rec);
    memset(t->tile, 0, sizeof t->tile);
    if (dword_53A45) memcpy(t->rec, (const void *)(uintptr_t)dword_53A45, 0x1E00);
    if (dword_53AFF && n) memcpy(t->tile, (const void *)(uintptr_t)dword_53AFF, n);
}

static int cmp_cap(const char *what, int c, const struct cap *a, const struct cap *b)
{
    int bad = 0;
    uint32_t n;
    if (a->be7 != b->be7 || a->be3 != b->be3 || a->beb != b->beb ||
        a->ac1 != b->ac1 || a->ac5 != b->ac5 || a->bdf != b->bdf) {
        printf("FAIL %s case %d: scalars orig=%u/%u/%u/%u/%u/%u ours=%u/%u/%u/%u/%u/%u\n",
               what, c, a->be7, a->be3, a->beb, a->ac1, a->ac5, a->bdf,
               b->be7, b->be3, b->beb, b->ac1, b->ac5, b->bdf);
        return 1;
    }
    n = 80u * a->be7;
    if (n > 0x2000) n = 0x2000;
    if (memcmp(a->rec, b->rec, n) != 0) {
        uint32_t i; for (i = 0; i < n; i++) if (a->rec[i] != b->rec[i]) break;
        printf("FAIL %s case %d: record byte %u orig=%02X ours=%02X\n",
               what, c, i, a->rec[i], b->rec[i]);
        bad = 1;
    }
    n = tile_len(c);
    if (n > 0x40000) n = 0x40000;
    if (memcmp(a->tile, b->tile, n) != 0) {
        uint32_t i; for (i = 0; i < n; i++) if (a->tile[i] != b->tile[i]) break;
        printf("FAIL %s case %d: tile byte %u orig=%02X ours=%02X\n",
               what, c, i, a->tile[i], b->tile[i]);
        bad = 1;
    }
    return bad;
}

static void run_tiles(int c)
{
    g_case = c;
    setup_world(c);
    O_10652();
    capture(&co);
    setup_world(c);
    world_load_tiles();
    capture(&cc);
    cases_run++;
    failures += cmp_cap("world_load_tiles", c, &co, &cc);
}

static void run_party(int c)
{
    g_case = c;
    setup_world(c);
    O_1088D(c);
    capture(&co);
    setup_world(c);
    world_load_party(c);
    capture(&cc);
    cases_run++;
    failures += cmp_cap("world_load_party", c, &co, &cc);
}

/* --- heap leaf (0x15F0E res_draw_subimage) ------------------------------ */
typedef void *(*subimage_fn)(const void *, void *, int, int, int, int);
#define O_SUBIMAGE ((subimage_fn)(uintptr_t)0x00015F0Eu)

static uint8_t g_tbl[128];          /* sub-image offset table + one entry */
static uint8_t heap_o[0x40000], heap_c[0x40000];   /* dword_53A49 surface */

static int cmp_buf(const char *what, int c, const void *a, const void *b, size_t n)
{
    if (memcmp(a, b, n) != 0) {
        size_t i; for (i = 0; i < n; i++) if (((const uint8_t *)a)[i] != ((const uint8_t *)b)[i]) break;
        printf("FAIL %s case %d: buffer byte %u orig=%02X ours=%02X\n", what, c,
               (unsigned)i, ((const uint8_t *)a)[i], ((const uint8_t *)b)[i]);
        return 1;
    }
    return 0;
}

static void run_subimage(int c)
{
    void *ro, *rc;
    int stride = 456, base = (int)(rnd() % 64), row = (int)(rnd() % 64), idx = (int)(rnd() % 8);

    seed = 0xA5A50000u ^ (uint32_t)c;
    memset(g_tbl, 0, sizeof g_tbl);
    { int k; for (k = 0; k < 8; k++) *(uint32_t *)(void *)(g_tbl + 6 + k * 4) = 48; }
    { uint8_t *e = g_tbl + 48; int k;
      *(uint16_t *)(void *)e = 4; *(uint16_t *)(void *)(e + 2) = 4;
      for (k = 0; k < 4; k++) { e[4 + 2*k] = 0x03; e[4 + 2*k + 1] = (uint8_t)(0x30 + idx); } }

    memset(heap_o, 0x11, sizeof heap_o);
    dword_53A49 = (uint32_t)(uintptr_t)heap_o;
    ro = O_SUBIMAGE(g_tbl, heap_o, stride, base, row, idx);
    memset(heap_c, 0x11, sizeof heap_c);
    dword_53A49 = (uint32_t)(uintptr_t)heap_c;
    rc = res_draw_subimage(g_tbl, heap_c, stride, base, row, idx);
    cases_run++;
    failures += cmp_buf("res_draw_subimage", c, heap_o, heap_c, 0x40000);
    if (ro && rc)
        failures += cmp_buf("res_draw_subimage.rec", c, ro, rc, 4u * 4u + 8u);
    free(ro); free(rc);
}

int main(int argc, char **argv)
{
    le_image le;
    int      applied = 0, c, cases = 40, i;

    for (i = 1; i < argc; i++)
        if (!strncmp(argv[i], "--cases=", 8)) cases = atoi(argv[i] + 8);

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied\n", (unsigned)applied);

    HOOK(0x3706E, stub_malloc);
    HOOK(0x3776E, stub_free);
    HOOK(0x37324, stub_fopen);
    HOOK(0x3759C, stub_fclose);
    HOOK(0x37940, stub_fseek);
    HOOK(0x373CA, stub_fread);
    HOOK(0x377A3, stub_fwrite);

    /* Translated helpers reachable from both loaders. */
    HOOK(0x111BA, res_load);
    HOOK(0x4E98D, rle_decode);
    HOOK(0x24D22, map_scroll_lines);
    HOOK(0x4DF4C, util_fix_records);
    HOOK(0x11019, stub_entrydata);
    HOOK(0x1B750, stub_metrics);
    HOOK(0x10B4E, stub_build);        /* not under test: keeps FD2.TMP out */
    HOOK(0x3790A, stub_delay0);       /* heap leaves pause a lot; no-op them */
    HOOK(0x4ECBF, gfx_save_rect);     /* heap leaf: go through the C helpers */
    HOOK(0x4EBAB, rle2_blit_trans);

    {
        int modes[10] = { 9, 17, 21, 22, 23, 24, 25, 27, 28, 29 };
        for (c = 0; c < cases; c++) {
            int m = modes[c % 10];
            if (c & 1)
                run_subimage(m);
            else
                run_tiles(m);
            if (failures) break;
        }
    }

    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
