/* rle2.c - FD2 0xC0-range RLE blits (second RLE family).
 *
 * Translation of 0x4EBFF (forward), 0x4EC31 (mirrored) and 0x4EBAB
 * (transparent) plus their shared token decoder 0x4EC66.
 *
 * The original decoder is a usercall that keeps (value, remaining) in
 * AH/AL and the stream pointer in ESI; the C version keeps the same state
 * in a struct and returns the next pixel. Header and row loop semantics
 * follow the disassembly exactly (bottom-tested loops, stream carries
 * across rows). See game/rle2.h.
 */
#include "rle2.h"

struct rle2_state {
    uint8_t        val;   /* original AL */
    uint8_t        rem;   /* original AH */
    const uint8_t *p;     /* original ESI */
};

/* 0x4EC66 */
static uint8_t rle2_next(struct rle2_state *s)
{
    uint8_t c;

    if (s->rem != 0) {
        s->rem--;
        return s->val;
    }
    c = *s->p++;
    if (c <= 0xC0) {
        s->rem = 0;
        s->val = c;
        return c;
    }
    s->rem = (uint8_t)(c - 0xC1);
    s->val = *s->p++;
    return s->val;
}

enum { R2_FORWARD, R2_MIRROR, R2_TRANSPARENT };

static void rle2_run(uint8_t *dst, const uint8_t *src, int stride, int mode)
{
    uint16_t w = *(const uint16_t *)src;
    uint16_t h = *(const uint16_t *)(src + 2);
    struct rle2_state st;

    st.val = 0;
    st.rem = 0;
    st.p   = src + 4;

    do {
        uint8_t *p = dst;
        uint32_t n = w;

        do {
            uint8_t v = rle2_next(&st);
            switch (mode) {
            case R2_MIRROR:      *p-- = v;            break;
            case R2_TRANSPARENT: if (v) *p = v; p++;  break;
            default:             *p++ = v;            break;
            }
        } while (--n != 0);

        dst += stride;
    } while (--h != 0);
}

void rle2_blit(void *dst, const void *src, int stride)
{
    rle2_run((uint8_t *)dst, (const uint8_t *)src, stride, R2_FORWARD);
}

void rle2_blit_mirror(void *dst, const void *src, int stride)
{
    rle2_run((uint8_t *)dst, (const uint8_t *)src, stride, R2_MIRROR);
}

void rle2_blit_trans(void *dst, const void *src, int stride)
{
    rle2_run((uint8_t *)dst, (const uint8_t *)src, stride, R2_TRANSPARENT);
}
