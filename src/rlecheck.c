/* rlecheck.c - differential test for the RLE source translation
 * (src/game/rle.c vs the original machine code at 0x4E98D / 0x4E8D3).
 *
 * Loads FD2.EXE with the LE loader (objects mapped executable, fixups
 * applied), then for each random well-formed picture decodes the same
 * stream twice:
 *
 *   A: original code - direct call through 0x4E98D / 0x4E8D3
 *   B: translated C   - rle_decode() / rle_decode_lut()
 *
 * and asserts byte-identical destination buffers AND identical side
 * effects on the original globals word_627B4/word_627B6 (0x627B4/0x627B6)
 * vs rle_width/rle_height.
 *
 * Build: pwsh -File build.ps1 -Target rlecheck
 * Run   : build\rlecheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/rle.h"

typedef void (__cdecl *orig_fn)(const void *, int, int, void *, int, int);

#define ORIG_RLE     ((orig_fn)(uintptr_t)0x4E98D)
#define ORIG_RLE_LUT ((orig_fn)(uintptr_t)0x4E8D3)

#define GUEST_W (*(const uint16_t *)(uintptr_t)0x627B4u)  /* word_627B4 */
#define GUEST_H (*(const uint16_t *)(uintptr_t)0x627B6u)  /* word_627B6 */

#define BUF_MARGIN 64

static uint32_t seed = 0xC0FFEEu;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}

/* Generate a stream whose rows fill width*height exactly.
 * Worst case 2 bytes/pixel (single-pixel fill tokens) + header + margin. */
static size_t gen_stream(uint8_t *out, unsigned w, unsigned h)
{
    uint8_t *p = out + 4;
    unsigned row, i;

    *(uint16_t *)out     = (uint16_t)w;
    *(uint16_t *)(out+2) = (uint16_t)h;

    for (row = 0; row < h; row++) {
        unsigned left = w;
        for (i = 0; left != 0; i++) {
            unsigned type  = (i + (unsigned)rnd()) & 3;   /* cycle all types */
            unsigned max   = (type == 1) ? left / 2 : left;
            unsigned count, k;

            if (max == 0) type = 2;                       /* no room for odd run */
            max = (type == 1) ? left / 2 : left;
            count = 1 + (unsigned)rnd() % (max < 64 ? max : 64);

            *p++ = (uint8_t)((type << 6) | (count - 1));
            switch (type) {
            case 0:  *p++ = (uint8_t)rnd(); break;        /* colour byte */
            case 1:  *p++ = (uint8_t)rnd(); break;        /* colour byte */
            case 2:  for (k = 0; k < count; k++) *p++ = (uint8_t)rnd(); break;
            default: break;                               /* skip */
            }
            left -= (type == 1) ? 2 * count : count;
        }
    }
    return (size_t)(p - out);
}

static int failures;
static unsigned cases_run;

/* returns 0 on match, 1 on mismatch (details printed) */
static int run_case(const uint8_t *stream, size_t slen, int mode,
                    const uint8_t *lut, unsigned id)
{
    unsigned w = *(const uint16_t *)stream;
    unsigned h = *(const uint16_t *)(stream + 2);
    int pitch = (int)(w + rnd() % 80);
    int x = (int)(rnd() % 24);
    int y = (int)(rnd() % 24);
    size_t bufsz = (size_t)(y + h) * (size_t)pitch + (size_t)x + BUF_MARGIN;
    uint8_t *a = malloc(bufsz);
    uint8_t *b = malloc(bufsz);
    uint16_t orig_w, orig_h, our_w, our_h;
    size_t k;

    if (!a || !b) { printf("out of memory\n"); exit(2); }
    memset(a, 0xE7, bufsz);
    memset(b, 0xE7, bufsz);

    if (lut) {
        ORIG_RLE_LUT(stream, x, y, a, pitch, (int)(uintptr_t)lut);
        rle_decode_lut(stream, x, y, b, pitch, lut);
    } else {
        ORIG_RLE(stream, x, y, a, pitch, mode);
        rle_decode(stream, x, y, b, pitch, mode);
    }
    orig_w = GUEST_W; orig_h = GUEST_H;
    our_w  = rle_width; our_h = rle_height;
    cases_run++;

    for (k = 0; k < bufsz; k++)
        if (a[k] != b[k]) {
            printf("FAIL case %u mode 0x%X lut=%d %ux%u pitch=%d (%d,%d): "
                   "buf diff @+%u orig=%02X ours=%02X\n",
                   id, mode, lut != 0, w, h, pitch, x, y,
                   (unsigned)k, a[k], b[k]);
            failures++;
            break;
        }
    if (orig_w != our_w || orig_h != our_h) {
        printf("FAIL case %u mode 0x%X lut=%d: globals orig %u/%u vs "
               "ours %u/%u\n", id, mode, lut != 0,
               orig_w, orig_h, our_w, our_h);
        failures++;
    }
    free(a);
    free(b);
    return failures != 0;
}

int main(int argc, char **argv)
{
    static const int modes[] = {
        -1,                 /* colours from stream (the common case)        */
        0x00000000,         /* flat 0                                      */
        0x00000037,         /* flat                                        */
        0x000000FF,         /* flat, max                                   */
        0x00000100,         /* ramp: first=0 rot=1                         */
        0x00000407,         /* ramp                                        */
        0x00001234,         /* ramp                                        */
        0x00010037,         /* (u16)0x37 -> FLAT edge case                 */
        0x00FFFFFF,         /* ramp first=0xFF rot=0xFF                    */
        0xFFFFFFFF          /* -1 again via full word                     */
    };
    le_image le;
    int applied = 0;
    uint8_t *stream;
    size_t stream_cap;
    uint8_t lut[256];
    unsigned i, t;

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n",
           (unsigned)applied);

    /* generous cap: w<=300 h<=120 => 2*w*h + header + margin < 100 KB */
    stream_cap = 256 * 1024;
    stream = malloc(stream_cap);
    if (!stream) return 2;

    for (t = 0; t < 256; t++) lut[t] = (uint8_t)rnd();

    for (i = 0; i < sizeof modes / sizeof modes[0]; i++) {
        unsigned n;
        for (n = 0; n < 150; n++) {
            unsigned w = 1 + (unsigned)rnd() % 300;
            unsigned h = 1 + (unsigned)rnd() % 120;
            size_t slen = gen_stream(stream, w, h);
            if (4 + slen > stream_cap) { printf("stream overflow\n"); return 2; }
            run_case(stream, slen, modes[i], NULL, i * 1000 + n);
            if (failures) goto done;
        }
    }
    /* palette-LUT variant */
    for (i = 0; i < 400; i++) {
        unsigned w = 1 + (unsigned)rnd() % 300;
        unsigned h = 1 + (unsigned)rnd() % 120;
        size_t slen = gen_stream(stream, w, h);
        run_case(stream, slen, 0, lut, 100000 + i);
        if (failures) goto done;
    }

done:
    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
