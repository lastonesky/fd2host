/* sprite24check.c - differential test for the 24x24 sprite RLE translation
 * (src/game/sprite24.c vs the seven original routines).
 *
 * Loads FD2.EXE with the LE loader, then for each random well-formed 24x24
 * stream runs both the original code and the C translation and asserts the
 * destination buffer (with a sentinel margin) is byte-identical.
 *
 * Build: pwsh -File build.ps1 -Target sprite24check
 * Run   : build\sprite24check.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/sprite24.h"

typedef void (__cdecl *f5)(const void *, void *, int, int, int);
typedef void (__cdecl *f4)(const void *, void *, int, const void *);
typedef void (__cdecl *f3)(const void *, void *, int);

#define ORIG_RAMP    ((f5)(uintptr_t)0x4DF84)
#define ORIG_PALRC   ((f4)(uintptr_t)0x4E016)
#define ORIG_PAL     ((f4)(uintptr_t)0x4E0A2)
#define ORIG_CONST   ((f3)(uintptr_t)0x4E127)
#define ORIG_RAMP24  ((f3)(uintptr_t)0x4E1A6)
#define ORIG_PLAIN   ((f3)(uintptr_t)0x4E22A)
#define ORIG_PLAIN49 ((f3)(uintptr_t)0x4E29C)

#define MARGIN 64

static uint32_t seed = 0x5EED24u;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}
static int rrange(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }
static void fill_rand(uint8_t *p, size_t n) { size_t i; for (i = 0; i < n; i++) p[i] = (uint8_t)rnd(); }

/* A stream that fills every 24-pixel row exactly. */
static size_t gen_stream(uint8_t *out)
{
    uint8_t *p = out;
    int row;
    for (row = 0; row < 24; row++) {
        int left = 24;
        while (left != 0) {
            unsigned type = (unsigned)rnd() & 3;
            unsigned max, count, k;

            if (type == 1) {
                max = (unsigned)left / 2;
                if (max == 0) type = 0;
            }
            max = (type == 1) ? (unsigned)left / 2 : (unsigned)left;
            if (max == 0) { type = 0; max = (unsigned)left; }
            count = 1 + (unsigned)rnd() % (max < 64 ? max : 64);

            *p++ = (uint8_t)((type << 6) | (count - 1));
            switch (type) {
            case 0: *p++ = (uint8_t)rnd(); break;              /* colour   */
            case 1: *p++ = (uint8_t)rnd(); break;              /* colour   */
            case 2: for (k = 0; k < count; k++) *p++ = (uint8_t)rnd(); break;
            default: break;                                    /* special  */
            }
            left -= (type == 1) ? (int)(2 * count) : (int)count;
        }
    }
    return (size_t)(p - out);
}

static int failures;
static unsigned cases_run;

/* mode: 0 ramp, 1 pal_recolor, 2 pal, 3 const, 4 ramp24, 5 plain, 6 plain49 */
static void run_case(unsigned mode, unsigned id)
{
    uint8_t stream[24 * 24 * 3];
    size_t  slen = gen_stream(stream);
    int     stride = rrange(24, 300);
    int     base = (int)(rnd() & 0xFF), rot = (int)(rnd() & 0xFF);
    uint8_t pal[256];
    size_t  dlen = (size_t)24 * stride + MARGIN;
    uint8_t *a = malloc(dlen), *b = malloc(dlen);
    size_t  k;

    (void)slen;
    for (k = 0; k < 256; k++) pal[k] = (uint8_t)rnd();
    if (!a || !b) { printf("oom\n"); exit(2); }
    fill_rand(a, dlen);
    memcpy(b, a, dlen);                 /* identical start (recolor reads it) */

    switch (mode) {
    case 0: ORIG_RAMP(stream, a, stride, base, rot);   sprite24_ramp(stream, b, stride, base, rot); break;
    case 1: ORIG_PALRC(stream, a, stride, pal);        sprite24_pal_recolor(stream, b, stride, pal); break;
    case 2: ORIG_PAL(stream, a, stride, pal);          sprite24_pal(stream, b, stride, pal); break;
    case 3: ORIG_CONST(stream, a, stride);             sprite24_const(stream, b, stride); break;
    case 4: ORIG_RAMP24(stream, a, stride);            sprite24_ramp24(stream, b, stride); break;
    case 5: ORIG_PLAIN(stream, a, stride);             sprite24_plain(stream, b, stride); break;
    default:ORIG_PLAIN49(stream, a, stride);           sprite24_plain49(stream, b, stride); break;
    }
    cases_run++;

    for (k = 0; k < dlen; k++)
        if (a[k] != b[k]) {
            printf("FAIL mode %u case %u: diff @+%u stride=%d orig=%02X ours=%02X\n",
                   mode, id, (unsigned)k, stride, a[k], b[k]);
            failures++;
            break;
        }
    free(a); free(b);
}

int main(int argc, char **argv)
{
    le_image le;
    int applied = 0;
    unsigned m, n;

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    for (m = 0; m < 7; m++)
        for (n = 0; n < 300; n++) {
            run_case(m, m * 1000 + n);
            if (failures) goto done;
        }

done:
    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
