/* rle2check.c - differential test for the 0xC0-range RLE blits.
 *
 * Builds a random token stream that decodes to exactly w*h pixels, runs the
 * original 0x4EBFF / 0x4EC31 / 0x4EBAB and the C translation on identical
 * input, and compares the destination buffers (with sentinel margins).
 *
 * NOTE: the mirrored blit writes each row *backwards* from the row base, so
 * row 0 writes up to w-1 bytes BEFORE dst. Both buffers therefore keep a
 * left margin; without it the two mallocs sit next to each other and one
 * buffer's overhang corrupts the other's tail (that is what a naive test
 * reports as a false "failure").
 *
 * Build: pwsh -File build.ps1 -Target rle2check
 * Run   : build\rle2check.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/rle2.h"

typedef void (__cdecl *blit_fn)(void *, const void *, int);

#define ORIG_FWD    ((blit_fn)(uintptr_t)0x4EBFF)
#define ORIG_MIR    ((blit_fn)(uintptr_t)0x4EC31)
#define ORIG_TRANS  ((blit_fn)(uintptr_t)0x4EBAB)

#define MARGIN 128

static uint32_t seed = 0xC0FFEEu;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}
static int rrange(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }
static void fill_rand(uint8_t *p, size_t n) { size_t i; for (i = 0; i < n; i++) p[i] = (uint8_t)rnd(); }

static int failures;
static unsigned cases_run;

/* stream decoding to exactly w*h pixels */
static size_t gen_stream(uint8_t *out, unsigned w, unsigned h)
{
    uint8_t *p = out + 4;
    unsigned total = w * h, done = 0;

    *(uint16_t *)out       = (uint16_t)w;
    *(uint16_t *)(out + 2) = (uint16_t)h;

    while (done < total) {
        unsigned left = total - done;
        if ((rnd() & 1) || left == 1) {
            *p++ = (uint8_t)(rnd() % 0xC1);           /* literal 0..0xC0 */
            done++;
        } else {
            unsigned maxlen = left < 63 ? left : 63;
            unsigned len = 1 + (unsigned)rnd() % maxlen;
            *p++ = (uint8_t)(0xC0 + len);             /* 0xC1..0xFF run   */
            *p++ = (uint8_t)rnd();                    /* value */
            done += len;
        }
    }
    return (size_t)(p - out);
}

static void run_case(unsigned id, unsigned mode)
{
    unsigned w = 1 + (unsigned)rnd() % 80;
    unsigned h = 1 + (unsigned)rnd() % 40;
    int      stride = (int)w + rrange(0, 80);
    static uint8_t stream[2 * 96 * 48 + 64];
    size_t   dlen = (size_t)h * stride + MARGIN;
    size_t   alloc = MARGIN + dlen;        /* left margin + body */
    uint8_t *abuf = malloc(alloc), *bbuf = malloc(alloc);
    uint8_t *a, *b;
    size_t   k;

    if (!abuf || !bbuf) { printf("oom\n"); exit(2); }
    gen_stream(stream, w, h);
    fill_rand(abuf, alloc);
    memcpy(bbuf, abuf, alloc);

    a = abuf + MARGIN;
    b = bbuf + MARGIN;

    switch (mode) {
    case 0: ORIG_FWD(a, stream, stride);   rle2_blit(b, stream, stride);        break;
    case 1: ORIG_MIR(a, stream, stride);   rle2_blit_mirror(b, stream, stride); break;
    default:ORIG_TRANS(a, stream, stride); rle2_blit_trans(b, stream, stride);  break;
    }
    cases_run++;

    for (k = 0; k < alloc; k++)
        if (abuf[k] != bbuf[k]) {
            printf("FAIL mode %u case %u: diff @+%ld w=%u h=%u stride=%d "
                   "orig=%02X ours=%02X\n",
                   mode, id, (long)k - MARGIN, w, h, stride, abuf[k], bbuf[k]);
            failures++;
            break;
        }
    free(abuf); free(bbuf);
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

    for (m = 0; m < 3; m++)
        for (n = 0; n < 400; n++) {
            run_case(m * 1000 + n, m);
            if (failures) goto done;
        }

done:
    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
