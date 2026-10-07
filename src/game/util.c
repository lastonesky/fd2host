/* util.c - FD2 object-0 byte / palette utility functions (source translation).
 *
 * Translation of:
 *   0x4DED4  util_rec3
 *   0x4DEEC  util_translate
 *   0x4DF09  util_sum_tail4
 *   0x4DF28  util_deobfuscate
 *   0x4DF4C  util_fix_records
 *   0x4E795  util_mask_recolor
 *
 * See game/util.h for the exact semantics and re/util_disasm.txt for the
 * machine code. Verified byte-for-byte by src/utilcheck.c.
 */
#include "util.h"

uint16_t util_mask_w;   /* original word_6017B @ 0x6017B */

const void *util_rec3(const void *base, int index)
{
    /* original: xor eax,eax; mov ax,3; mul edx; add eax,base */
    return (const char *)base + (uint32_t)3u * (uint32_t)index;
}

unsigned char util_translate(const void *table, uint32_t count, void *buf)
{
    const unsigned char *t = (const unsigned char *)table;
    unsigned char       *p = (unsigned char *)buf;
    unsigned char        v = 0;

    do {
        v = t[*p];
        *p++ = v;
    } while (--count != 0);

    return v;
}

int util_sum_tail4(const void *buf, uint32_t n)
{
    const unsigned char *p = (const unsigned char *)buf;
    uint32_t sum = 0;

    n -= 4;
    do {
        sum += *p++;
    } while (--n != 0);

    return (int)sum;
}

unsigned char util_deobfuscate(void *buf, uint32_t n)
{
    unsigned char *p     = (unsigned char *)buf;
    uint16_t       state = 0x00A5;
    unsigned char  v     = 0;

    do {
        v     = *p;
        state = (uint16_t)(state + 0x9014u);
        state = (uint16_t)((state << 3) | (state >> 13));   /* rol16 3 */
        v    ^= (unsigned char)state;
        *p++  = v;
    } while (--n != 0);

    return v;
}

unsigned char util_fix_records(void *header)
{
    unsigned char *h = (unsigned char *)header;
    uint32_t       count;
    unsigned char  v = 0xFF;

    /* original: mov al,[h]; mov ah,[h+2]; mul ah  -> AX = h[0]*h[2] */
    count = (uint32_t)(uint16_t)((uint16_t)h[0] * (uint16_t)h[2]);
    h += 4;

    do {
        h[3] = 0xFF;
        h[2] &= 0x1F;
        h[1] &= 0x03;
        h += 4;
    } while (--count != 0);

    return v;
}

unsigned char util_mask_recolor(void *dst, const void *header, int stride,
                                const void *palette)
{
    const unsigned char *hdr = (const unsigned char *)header;
    const unsigned char *pal = (const unsigned char *)palette;
    unsigned char       *p   = (unsigned char *)dst;
    uint16_t             w   = *(const uint16_t *)hdr;
    uint16_t             h   = *(const uint16_t *)(hdr + 2);
    int                  skip = stride - (int)w;
    const unsigned char *mask = hdr + 4;
    unsigned char        v = 0;

    util_mask_w = w;

    do {                                    /* rows: dec dx; jnz */
        uint32_t col = w;
        do {                                /* columns: x86 loop */
            v = *mask++;
            if (v != 0) {
                v  = pal[*p];
                *p = v;
            }
            ++p;
        } while (--col != 0);
        p += skip;
    } while (--h != 0);

    return v;
}

/* 0x4EBE3 - the game's random generator.
 *
 *   ax = word_627B8;  ax += 0x9014;  rol ax,1 x3;  word_627B8 = ax;  return ax
 *
 * (The decompiler prints the add as `- 28652`, which is 0x9014 mod 0x10000.)
 * Returns a zero-extended 16-bit value (`xor eax,eax; mov ax,...`). */
#define word_627B8 (*(uint16_t *)(uintptr_t)0x000627B8u)

uint32_t util_rand(void)
{
    uint16_t v = (uint16_t)(word_627B8 + 0x9014u);
    int i;

    for (i = 0; i < 3; i++)
        v = (uint16_t)((v << 1) | (v >> 15));
    word_627B8 = v;
    return (uint32_t)v;
}
