/* gfx.c - FD2 object-0 graphics blitter helper family (source translation).
 *
 * Translation of the original machine code at:
 *   0x4ECBF (+ helper 0x4ECF0)  gfx_save_rect
 *   0x4EC7C (+ helper 0x4ECA4)  gfx_restore_rect
 *   0x4ED0B                     gfx_blit_block
 *   0x4ED34 (+ helper 0x4ED4F)  gfx_blit_transparent
 *   0x4ED7A                     gfx_draw_glyph
 *   0x4EEE0                     gfx_expand_scanlines
 *
 * Every function is a pure memory operation over the pointers passed in; the
 * only external state is the original scratch globals, mirrored here. The
 * two save/restore helpers and the two blit helpers have been folded into a
 * single loop each (the original duplicated them, once striding the source
 * and once the destination).
 *
 * Verified byte-for-byte against the machine code by src/gfxcheck.c.
 * Format / ABI documentation: game/gfx.h.
 */
#include "gfx.h"
#include <string.h>

uint16_t gfx_rec_w;      /* original word_627B4 @ 0x627B4 */
uint16_t gfx_rec_h;      /* original word_627B6 @ 0x627B6 */
uint16_t gfx_pen_stride; /* original word_627A3 @ 0x627A3 */
uint8_t  gfx_pen_fg;     /* original byte_627A5 @ 0x627A5 */
uint8_t  gfx_pen_fill;   /* original byte_627A6 @ 0x627A6 */
uint8_t  gfx_pen_shadow; /* original byte_627A7 @ 0x627A7 */
uint32_t gfx_pen_dest;   /* original dword_627A8 @ 0x627A8 */
uint32_t gfx_glyph_tab;  /* original dword_627AC @ 0x627AC */
uint32_t gfx_glyph_idx;  /* original dword_627B0 @ 0x627B0 */

/* Scanline phase-offset table, original byte_627C8 @ 0x627C8 (read-only:
 * the image contains exactly one xref, the load in sub_4EEE0). */
const uint8_t gfx_phase_table[16] = {
    2, 3, 3, 4, 4, 4, 3, 3, 2, 1, 1, 0, 0, 0, 1, 1
};

/* ------------------------------------------------------------------ */
/* 0x4ECBF + 0x4ECF0: strided source -> compact record                 */
/* ------------------------------------------------------------------ */
void gfx_save_rect(void *record, int w, int h, const void *surface, int offset,
                   int stride)
{
    uint8_t       *rec = (uint8_t *)record;
    const uint8_t *src = (const uint8_t *)surface + offset;
    uint16_t       width  = (uint16_t)w;
    uint16_t       height = (uint16_t)h;

    gfx_rec_w = width;
    gfx_rec_h = height;

    *(uint16_t *)rec       = width;                 /* header: w, h, offset */
    *(uint16_t *)(rec + 2) = height;
    *(int32_t  *)(rec + 4) = (int32_t)offset;
    rec += 8;

    do {                                            /* original: do-while */
        memcpy(rec, src, width);
        rec += width;
        src += stride;
    } while (--height != 0);
}

/* ------------------------------------------------------------------ */
/* 0x4EC7C + 0x4ECA4: compact record -> strided destination            */
/* ------------------------------------------------------------------ */
void gfx_restore_rect(const void *record, void *surface, int stride)
{
    const uint8_t *src  = (const uint8_t *)record;
    uint16_t       width  = *(const uint16_t *)src;
    uint16_t       height = *(const uint16_t *)(src + 2);
    int32_t        offset = *(const int32_t *)(src + 4);
    uint8_t       *dst;

    gfx_rec_w = width;
    gfx_rec_h = height;
    src += 8;
    dst  = (uint8_t *)surface + offset;

    do {
        memcpy(dst, src, width);
        dst += stride;
        src += width;
    } while (--height != 0);
}

/* ------------------------------------------------------------------ */
/* 0x4ED0B: header-prefixed block -> strided destination               */
/* ------------------------------------------------------------------ */
void gfx_blit_block(void *dest, const void *src, int stride)
{
    const uint8_t *s = (const uint8_t *)src;
    uint8_t       *d = (uint8_t *)dest;
    uint16_t       width  = *(const uint16_t *)s;
    uint16_t       height = *(const uint16_t *)(s + 2);

    s += 4;

    do {
        memcpy(d, s, width);
        d += stride;
        s += width;
    } while (--height != 0);
}

/* ------------------------------------------------------------------ */
/* 0x4ED34 + 0x4ED4F: transparent blit (0 bytes are skipped)          */
/* ------------------------------------------------------------------ */
void gfx_blit_transparent(void *dest, const void *src, int stride)
{
    const uint8_t *s = (const uint8_t *)src;
    uint8_t       *d = (uint8_t *)dest;
    uint16_t       width  = *(const uint16_t *)s;
    uint16_t       height = *(const uint16_t *)(s + 2);
    int            step   = (uint16_t)stride;   /* original: mov bx,word */

    gfx_pen_stride = (uint16_t)stride;
    s += 4;

    do {
        uint8_t *row = d;                       /* original pushes edi   */
        uint16_t col = width;
        do {
            uint8_t v = *s++;
            if (v != 0)
                *d = v;
            ++d;
        } while (--col != 0);
        d = row + step;                         /* ... then adds stride  */
    } while (--height != 0);
}

/* ------------------------------------------------------------------ */
/* 0x4ED7A: 16x16 1bpp glyph with foreground / drop shadow / fill      */
/* ------------------------------------------------------------------ */
void gfx_draw_glyph(const void *glyph_table, int index, void *dest, int stride,
                    int fg, int shadow, int fill)
{
    uint8_t        *d  = (uint8_t *)dest;
    int             st = (uint16_t)stride;      /* original: bp = word */
    const uint8_t  *g;

    gfx_glyph_tab  = (uint32_t)(uintptr_t)glyph_table;
    gfx_glyph_idx  = (uint32_t)index;
    gfx_pen_dest   = (uint32_t)(uintptr_t)dest;
    gfx_pen_stride = (uint16_t)stride;
    gfx_pen_fg     = (uint8_t)fg;
    gfx_pen_shadow = (uint8_t)shadow;
    gfx_pen_fill   = (uint8_t)fill;

    if ((uint8_t)fill != 0) {                   /* pre-fill the 16x16 cell */
        int r;
        for (r = 0; r < 16; r++) {
            int k;
            for (k = 0; k < 16; k++)
                d[k] = (uint8_t)fill;
            d += st;
        }
    }

    if ((uint32_t)index == 10)                  /* index 10 is never drawn */
        return;

    g = (const uint8_t *)glyph_table + ((uint32_t)index << 5);
    d = (uint8_t *)dest;

    {
        int r;
        for (r = 0; r < 16; r++) {
            uint16_t row = *(const uint16_t *)g; /* little-endian load      */
            uint8_t *p   = d;
            int      c;

            g += 2;
            row = (uint16_t)((row >> 8) | (row << 8));  /* xchg al, ah      */

            for (c = 0; c < 16; c++) {
                unsigned set = row & 0x8000u;
                row = (uint16_t)(row << 1);
                if (set) {
                    p[0]           = gfx_pen_fg;
                    p[st - 1]      = gfx_pen_shadow;    /* one row down, left */
                    p[st]          = gfx_pen_shadow;    /* one row down       */
                }
                ++p;
            }
            d += st;
        }
    }
}

/* ------------------------------------------------------------------ */
/* 0x4EEE0: 16-phase scanline re-interleave                            */
/* ------------------------------------------------------------------ */
void gfx_expand_scanlines(const void *src, void *dest, int start)
{
    const uint8_t *s   = (const uint8_t *)src + 4;
    uint8_t       *d   = (uint8_t *)dest;
    uint16_t       idx = (uint16_t)start;
    int            n;

    for (n = 0; n < 192; n++) {
        memcpy(d, s + gfx_phase_table[idx & 15], 312);
        s += 320;
        d += 320;
        if (++idx >= 0x10)
            idx = 0;
    }
}

/* 0x11EB0 - copy `rows` rows of `len` bytes between two strided surfaces.
 * Pure memmove loop; the original zeroes ebx and tests `ebx < rows`, so a
 * non-positive row count copies nothing. */
void gfx_copy_rows(void *dst, int dst_stride, const void *src, int src_stride,
                   int len, int rows)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    while (rows-- > 0) {
        memmove(d, s, (size_t)len);
        d += dst_stride;
        s += src_stride;
    }
}
