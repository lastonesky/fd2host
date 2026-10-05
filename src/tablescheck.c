/* tablescheck.c - differential test for the table index helpers.
 *
 * Runs each original accessor (0x4E7DD..0x4E8BC) and the generic C helper on
 * the same index and compares the returned pointer / dword.
 *
 * Build: pwsh -File build.ps1 -Target tablescheck
 * Run   : build\tablescheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "le.h"
#include "game/tables.h"

typedef void *(__cdecl *ptr_fn)(int);
typedef uint32_t (__cdecl *u32_fn)(int);

struct desc {
    uint32_t    addr;
    uint32_t    base;
    int         stride;
    int         offset;
    int         dword;      /* 0 = pointer table, 1 = dword table */
    const char *name;
    int         max_index;  /* dword table: stay inside mapped data */
};

static const struct desc g_desc[] = {
    { 0x4E7DD, 0x615FE,  2, -64, 0, "4E7DD",  0 },
    { 0x4E7F2, 0x626B3, 12,   0, 0, "4E7F2",  0 },
    { 0x4E809, 0x6238D, 31, -31, 0, "4E809",  0 },
    { 0x4E821, 0x620A1, 11,   0, 0, "4E821",  0 },
    { 0x4E838, 0x61DA1, 24,   0, 0, "4E838",  0 },
    { 0x4E84F, 0x61AF9, 10,   0, 0, "4E84F",  0 },
    { 0x4E866, 0x619FD,  7,   0, 0, "4E866",  0 },
    { 0x4E87D, 0x61955,  4,   0, 1, "4E87D", 400 },   /* data ends ~0x634D2 */
    { 0x4E88E, 0x6188A,  7,   0, 0, "4E88E",  0 },
    { 0x4E8A5, 0x61646, 20,   0, 0, "4E8A5",  0 },
    { 0x4E8BC, 0x602AD, 23,   0, 0, "4E8BC",  0 },
};

static uint32_t seed = 0xA5A5u;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}

static int failures;
static unsigned cases_run;

static void test_one(const struct desc *d, int index)
{
    cases_run++;
    if (d->dword) {
        uint32_t o = ((u32_fn)(uintptr_t)d->addr)(index);
        uint32_t m = tbl_u32((const void *)(uintptr_t)d->base, index);
        if (o != m) {
            printf("FAIL %s index=0x%X: orig=0x%X ours=0x%X\n",
                   d->name, (unsigned)index, o, m);
            failures++;
        }
    } else {
        void *o = ((ptr_fn)(uintptr_t)d->addr)(index);
        void *m = tbl_ptr((void *)(uintptr_t)d->base, d->stride, index, d->offset);
        if (o != m) {
            printf("FAIL %s index=0x%X: orig=%p ours=%p\n",
                   d->name, (unsigned)index, o, m);
            failures++;
        }
    }
}

int main(int argc, char **argv)
{
    static const int edge[] = { 0, 1, 2, 0x1F, 0x20, 0x21, 0xFF, 0x100,
                                -1, -2, -0x20, -0x21 };
    le_image le;
    int      applied = 0;
    unsigned i, k;

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    for (i = 0; i < sizeof g_desc / sizeof g_desc[0]; i++) {
        const struct desc *d = &g_desc[i];

        for (k = 0; k < sizeof edge / sizeof edge[0]; k++) {
            if (d->max_index && (edge[k] < 0 || edge[k] > d->max_index))
                continue;
            test_one(d, edge[k]);
        }
        for (k = 0; k < 400; k++) {
            int idx;
            if (d->max_index)
                idx = (int)(rnd() % (uint32_t)(d->max_index + 1));
            else {
                idx = (int)rnd();
                if (rnd() & 1)
                    idx = -idx;
            }
            test_one(d, idx);
        }
        if (failures)
            break;
    }

    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
