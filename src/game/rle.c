/* rle.c - FD2 picture decoder (RLE blitter).
 *
 * Translation of:
 *   sub_4E98D (0x4E98D, 443 bytes) - 3 colour modes (stream / flat / ramp)
 *   sub_4E8D3 (0x4E8D3, 186 bytes) - palette LUT mode
 * Both share one token loop; the original had them as two separate copies
 * of the same state machine with different colour mapping.
 *
 * Semantics pinned down from the disassembly (re/sub_4E98D.disasm) and
 * verified byte-for-byte against the machine code by src/rlecheck.c.
 * Format documentation: game/rle.h.
 */
#include "rle.h"
#include <string.h>

uint16_t rle_width;    /* original word_627B4 @ 0x627B4 */
uint16_t rle_height;   /* original word_627B6 @ 0x627B6 */

enum { RLE_INLINE, RLE_FLAT, RLE_RAMP, RLE_LUT };

struct rle_colors {
    unsigned           kind;
    unsigned char      flat;        /* RLE_FLAT: value painted everywhere   */
    unsigned char      ramp_first;  /* RLE_RAMP: first of 8 colours (BL)    */
    unsigned char      ramp_rot;    /* RLE_RAMP: index rotation     (BH)    */
    const unsigned char *lut;       /* RLE_LUT: 256-byte table              */
};

static unsigned char rle_map(const struct rle_colors *pc, unsigned char c)
{
    switch (pc->kind) {
    case RLE_FLAT:  return pc->flat;
    case RLE_RAMP:  return (unsigned char)(pc->ramp_first +
                                           ((pc->ramp_rot + c) & 7));
    case RLE_LUT:   return pc->lut[c];
    default:        return c;
    }
}

static void rle_run(const uint8_t *src, void *dst, int x, int y, int pitch,
                    const struct rle_colors *pc)
{
    uint8_t *p = (uint8_t *)dst + (uint32_t)y * (uint32_t)pitch + (uint32_t)x;
    uint32_t row_skip = (uint32_t)pitch - rle_width;

    do {
        uint16_t left = rle_width;              /* pixels left in this row */

        do {
            unsigned tok   = *src++;
            unsigned count = (tok & 63) + 1;

            switch (tok >> 6) {
            case 0: {                           /* solid run */
                unsigned char c = rle_map(pc, *src++);
                memset(p, c, count);
                p   += count;
                left = left - (uint16_t)count;
                break;
            }
            case 1: {                           /* odd run: every other pixel */
                unsigned char c = rle_map(pc, *src++);
                unsigned n = count;
                left = left - (uint16_t)(2 * count);
                do {
                    p[1] = c;                   /* original: inc edi; stosb */
                    p += 2;
                } while (--n);
                break;
            }
            case 2:                             /* literal pixels */
                if (pc->kind == RLE_RAMP || pc->kind == RLE_LUT) {
                    unsigned n = count;         /* colour per source byte */
                    do {
                        *p++ = rle_map(pc, *src++);
                    } while (--n);
                } else {
                    if (pc->kind == RLE_FLAT)
                        memset(p, pc->flat, count);  /* stream bytes skipped */
                    else
                        memcpy(p, src, count);
                    src += count;
                    p   += count;
                }
                left = left - (uint16_t)count;
                break;
            default:                            /* transparent run */
                p   += count;
                left = left - (uint16_t)count;
                break;
            }
        } while (left != 0);

        p += row_skip;
    } while (--rle_height != 0);
}

void rle_decode(const void *src, int x, int y, void *dst, int pitch, int mode)
{
    const uint8_t *p = (const uint8_t *)src;
    struct rle_colors c;

    rle_width  = *(const uint16_t *)p;
    rle_height = *(const uint16_t *)(p + 2);
    p += 4;

    memset(&c, 0, sizeof c);
    if (mode == -1) {
        c.kind = RLE_INLINE;
    } else if ((uint16_t)mode <= 0xFF) {
        c.kind = RLE_FLAT;
        c.flat = (unsigned char)mode;
    } else {
        c.kind       = RLE_RAMP;
        c.ramp_first = (unsigned char)mode;
        c.ramp_rot   = (unsigned char)((uint16_t)mode >> 8);
    }

    rle_run(p, dst, x, y, pitch, &c);
}

void rle_decode_lut(const void *src, int x, int y, void *dst, int pitch,
                    const void *palette)
{
    const uint8_t *p = (const uint8_t *)src;
    struct rle_colors c;

    rle_width  = *(const uint16_t *)p;
    rle_height = *(const uint16_t *)(p + 2);
    p += 4;

    memset(&c, 0, sizeof c);
    c.kind = RLE_LUT;
    c.lut  = (const unsigned char *)palette;

    rle_run(p, dst, x, y, pitch, &c);
}
