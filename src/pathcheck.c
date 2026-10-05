/* pathcheck.c - differential test for the path/movement-range translation
 * (src/game/path.c vs the original machine code at 0x4E390 and 0x4E4F6).
 *
 * Both entries are cdecl (they do pusha/popa). For each random grid we run
 * the original and the translation on identical inputs and assert the map,
 * the output buffer, the returned best-path length and the final depth all
 * match.
 *
 * Build: pwsh -File build.ps1 -Target pathcheck
 * Run   : build\pathcheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/path.h"

typedef void (__cdecl *mark_fn)(int, int, int, int, void *, const void *);
typedef int  (__cdecl *find_fn)(int, int, int, int, void *, int, int, int,
                                void *, const void *);

#define ORIG_MARK ((mark_fn)(uintptr_t)0x4E390)
#define ORIG_FIND ((find_fn)(uintptr_t)0x4E4F6)

#define GUEST_60068 (*(const uint8_t *)(uintptr_t)0x60068u)
#define GUEST_60069 (*(const uint8_t *)(uintptr_t)0x60069u)
#define GUEST_60077 (*(const uint8_t *)(uintptr_t)0x60077u)
#define GUEST_60078 (*(const uint8_t *)(uintptr_t)0x60078u)

#define MARGIN 64

static uint32_t seed = 0x9E3779B9u;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}
static int rrange(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }
static void fill_rand(uint8_t *p, size_t n) { size_t i; for (i = 0; i < n; i++) p[i] = (uint8_t)rnd(); }

static int failures;
static unsigned cases_run;

static void cmp_buf(const char *what, unsigned id, const uint8_t *a,
                    const uint8_t *b, size_t n)
{
    size_t k;
    for (k = 0; k < n; k++)
        if (a[k] != b[k]) {
            printf("FAIL %s case %u: buffer diff @+%u orig=%02X ours=%02X\n",
                   what, id, (unsigned)k, a[k], b[k]);
            failures++;
            return;
        }
}

struct env {
    int      W, H;
    uint8_t  mapA[4 + 4 * 64], mapB[4 + 4 * 64];
    uint8_t  costtab[4096];
    uint8_t  costrow[256];
    uint8_t  outA[MARGIN], outB[MARGIN];
    int      x, y, val, tx, ty, mode;
};

static void make_env(struct env *e)
{
    size_t mlen;
    int i;

    e->W = rrange(4, 7);
    e->H = rrange(4, 7);
    mlen = 4 + 4 * (size_t)e->W * e->H;

    fill_rand(e->mapA, mlen);
    e->mapA[0] = (uint8_t)e->W;
    e->mapA[2] = (uint8_t)e->H;
    memcpy(e->mapB, e->mapA, mlen);

    fill_rand(e->costtab, sizeof e->costtab);
    for (i = 0; i < 256; i++) e->costrow[i] = (uint8_t)(1 + rnd() % 5);

    e->x = (int)(rnd() % (uint32_t)e->W);
    e->y = (int)(rnd() % (uint32_t)e->H);
    e->val = rrange(1, 12);
    e->tx = (int)(rnd() % (uint32_t)e->W);
    e->ty = (int)(rnd() % (uint32_t)e->H);
    e->mode = (int)(rnd() % 3);
}

static void test_mark(unsigned id)
{
    struct env e;
    size_t mlen;

    make_env(&e);
    mlen = 4 + 4 * (size_t)e.W * e.H;

    ORIG_MARK((int)(uintptr_t)e.costrow, e.x, e.y, e.val, e.mapA, e.costtab);
    path_mark((int)(uintptr_t)e.costrow, e.x, e.y, e.val, e.mapB, e.costtab);
    cases_run++;
    cmp_buf("mark", id, e.mapA, e.mapB, mlen);
    if (GUEST_60068 != (uint8_t)e.W || GUEST_60069 != (uint8_t)e.H) {
        printf("FAIL mark case %u: globals W/H orig=%u/%u\n",
               id, GUEST_60068, GUEST_60069);
        failures++;
    }
}

static void test_find(unsigned id)
{
    struct env e;
    size_t mlen;
    int ro, rm;

    make_env(&e);
    mlen = 4 + 4 * (size_t)e.W * e.H;
    fill_rand(e.outA, sizeof e.outA);
    memcpy(e.outB, e.outA, sizeof e.outA);

    ro = ORIG_FIND((int)(uintptr_t)e.costrow, e.x, e.y, e.val, e.outA,
                   e.tx, e.ty, e.mode, e.mapA, e.costtab);
    rm = path_find((int)(uintptr_t)e.costrow, e.x, e.y, e.val, e.outB,
                   e.tx, e.ty, e.mode, e.mapB, e.costtab);
    cases_run++;
    cmp_buf("find-map", id, e.mapA, e.mapB, mlen);
    cmp_buf("find-out", id, e.outA, e.outB, sizeof e.outA);
    if (ro != rm) {
        printf("FAIL find case %u: return orig=%d ours=%d (mode=%d)\n",
               id, ro, rm, e.mode);
        failures++;
    }
    if (GUEST_60078 != path_best || GUEST_60077 != 0 || path_depth != 0) {
        printf("FAIL find case %u: globals best orig=%u ours=%d depth orig=%u ours=%d\n",
               id, GUEST_60078, path_best, GUEST_60077, path_depth);
        failures++;
    }
}

int main(int argc, char **argv)
{
    le_image le;
    int applied = 0;
    unsigned i;

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    for (i = 0; i < 500; i++) {
        test_mark(1000 + i);
        test_find(2000 + i);
        if (failures) break;
    }

    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
