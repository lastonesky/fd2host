/* utilcheck.c - differential test for the byte / palette utility translation
 * (src/game/util.c vs the original machine code).
 *
 * Original entry points: 0x4DED4 / 0x4DEEC / 0x4DF09 / 0x4DF28 / 0x4DF4C /
 * 0x4E795. Each random case runs both implementations and asserts the
 * touched buffer (plus a sentinel margin) and the return value match.
 *
 * Build: pwsh -File build.ps1 -Target utilcheck
 * Run   : build\utilcheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/util.h"

typedef void *(__cdecl *rec3_fn)(int);
typedef unsigned char (__cdecl *tr_fn)(const void *, unsigned, void *);
typedef int (__cdecl *sum_fn)(const void *, unsigned);
typedef unsigned char (__cdecl *deob_fn)(void *, unsigned);
typedef unsigned char (__cdecl *fix_fn)(void *);
typedef unsigned char (__cdecl *recolor_fn)(void *, const void *, int, const void *);

#define ORIG_REC3   ((rec3_fn)   (uintptr_t)0x4DED4)
#define ORIG_TR     ((tr_fn)     (uintptr_t)0x4DEEC)
#define ORIG_SUM    ((sum_fn)    (uintptr_t)0x4DF09)
#define ORIG_DEOB   ((deob_fn)   (uintptr_t)0x4DF28)
#define ORIG_FIX    ((fix_fn)    (uintptr_t)0x4DF4C)
#define ORIG_RECOL  ((recolor_fn)(uintptr_t)0x4E795)

/* 0x4DF09 is the one original helper that clobbers EBX without saving it
 * (callers only read the EAX return, so EBX is dead to them). Our translation
 * preserves EBX, but calling the original from a normal C caller can corrupt
 * the compiler's EBX state - so save/restore it around the call. */
static int call_sum_orig(const void *buf, unsigned n)
{
    int r;
    __asm {
        push ebx
        push n
        push buf
        mov  eax, 4DF09h
        call eax
        add  esp, 8
        mov  r, eax
        pop  ebx
    }
    return r;
}

/* util_rec3's original base is the obj2 table at 0x60181 */
#define REC3_BASE ((void *)(uintptr_t)0x60181u)

#define MARGIN 64
#define GUEST_MASK_W (*(const uint16_t *)(uintptr_t)0x6017Bu)

static uint32_t seed = 0xB17E5u;
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

static void test_rec3(unsigned id)
{
    int i = rrange(0, 2000);
    void *o = ORIG_REC3(i);
    void *m = (void *)util_rec3(REC3_BASE, i);
    cases_run++;
    if (o != m) {
        printf("FAIL rec3 case %u: index=%d orig=%p ours=%p\n", id, i, o, m);
        failures++;
    }
}

static void test_translate(unsigned id)
{
    uint8_t table[256], a[128], b[128];
    unsigned n = 1 + (unsigned)rnd() % 100;
    unsigned char ro, rm;

    fill_rand(table, sizeof table);
    fill_rand(a, sizeof a);
    memcpy(b, a, sizeof a);

    ro = ORIG_TR(table, n, a);
    rm = util_translate(table, n, b);
    cases_run++;
    cmp_buf("translate", id, a, b, sizeof a);
    if (ro != rm) {
        printf("FAIL translate case %u: ret orig=%02X ours=%02X\n", id, ro, rm);
        failures++;
    }
}

static void test_sum(unsigned id)
{
    uint8_t a[256], b[256];
    unsigned n = 5 + (unsigned)rnd() % 200;
    int ro, rm;

    fill_rand(a, sizeof a);
    memcpy(b, a, sizeof a);

    ro = call_sum_orig(a, n);
    rm = util_sum_tail4(b, n);
    cases_run++;
    cmp_buf("sum", id, a, b, sizeof a);
    if (ro != rm) {
        printf("FAIL sum case %u: n=%u orig=%d ours=%d\n", id, n, ro, rm);
        failures++;
    }
}

static void test_deobfuscate(unsigned id)
{
    uint8_t a[256], b[256];
    unsigned n = 1 + (unsigned)rnd() % 200;
    unsigned char ro, rm;

    fill_rand(a, sizeof a);
    memcpy(b, a, sizeof a);

    ro = ORIG_DEOB(a, n);
    rm = util_deobfuscate(b, n);
    cases_run++;
    cmp_buf("deobfuscate", id, a, b, sizeof a);
    if (ro != rm) {
        printf("FAIL deobfuscate case %u: ret orig=%02X ours=%02X\n", id, ro, rm);
        failures++;
    }
}

static void test_fix(unsigned id)
{
    uint8_t hA[4 + 4 * 256 + MARGIN], hB[sizeof hA];
    unsigned a = 1 + (unsigned)rnd() % 16;
    unsigned b = 1 + (unsigned)rnd() % 16;
    size_t   len;
    unsigned char ro, rm;

    fill_rand(hA, sizeof hA);
    hA[0] = (uint8_t)a;
    hA[2] = (uint8_t)b;
    memcpy(hB, hA, sizeof hA);
    len = 4 + 4 * ((size_t)a * b) + MARGIN;

    ro = ORIG_FIX(hA);
    rm = util_fix_records(hB);
    cases_run++;
    cmp_buf("fix_records", id, hA, hB, len);
    if (ro != rm) {
        printf("FAIL fix_records case %u: ret orig=%02X ours=%02X\n", id, ro, rm);
        failures++;
    }
}

static void test_mask_recolor(unsigned id)
{
    int      w = rrange(1, 40), h = rrange(1, 30);
    int      stride = w + rrange(0, 160);
    size_t   hlen = 4 + (size_t)w * h;
    size_t   dlen = (size_t)h * stride + MARGIN;
    uint8_t *hdr = malloc(hlen);
    uint8_t *dA = malloc(dlen), *dB = malloc(dlen);
    uint8_t  pal[256];
    size_t   k;
    unsigned char ro, rm;

    if (!hdr || !dA || !dB) { printf("oom\n"); exit(2); }
    fill_rand(hdr, hlen);
    *(uint16_t *)hdr = (uint16_t)w;
    *(uint16_t *)(hdr + 2) = (uint16_t)h;
    for (k = 0; k < (size_t)w * h; k++)          /* ~half the mask is zero */
        if (rnd() & 1) hdr[4 + k] = 0;
    fill_rand(pal, sizeof pal);
    fill_rand(dA, dlen);
    memcpy(dB, dA, dlen);

    ro = ORIG_RECOL(dA, hdr, stride, pal);
    rm = util_mask_recolor(dB, hdr, stride, pal);
    cases_run++;
    cmp_buf("mask_recolor", id, dA, dB, dlen);
    if (ro != rm) {
        printf("FAIL mask_recolor case %u: ret orig=%02X ours=%02X\n", id, ro, rm);
        failures++;
    }
    if (GUEST_MASK_W != util_mask_w) {
        printf("FAIL mask_recolor case %u: word_6017B orig=%u ours=%u\n",
               id, GUEST_MASK_W, util_mask_w);
        failures++;
    }
    free(hdr); free(dA); free(dB);
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

    for (i = 0; i < 200; i++) test_rec3(1000 + i);
    for (i = 0; i < 400; i++) test_translate(2000 + i);
    for (i = 0; i < 400; i++) test_sum(3000 + i);
    for (i = 0; i < 400; i++) test_deobfuscate(4000 + i);
    for (i = 0; i < 400; i++) test_fix(5000 + i);
    for (i = 0; i < 400; i++) test_mask_recolor(6000 + i);

    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
