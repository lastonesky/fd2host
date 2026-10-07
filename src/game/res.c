/* res.c - FD2 LMI container resource loader (source translation of 0x111BA).
 *
 * See game/res.h for the contract. The original is 235 bytes that only
 * sequence the Watcom CRT (fopen/fseek/fread/malloc/free/fclose) plus the
 * `sub_3702F` stack probe; this is the straight C equivalent.
 *
 * Two adaptations make this safe to run *inside* the game (not just in the
 * check harness): the buffers live on the game's heap (guest_mem.h) and the
 * size is published to the game's global at 0x53BFF, not to a C variable -
 * the game reads that address.
 */
#include "res.h"
#include "guest_mem.h"
#include "rle.h"
#include <stdio.h>
#include <stdlib.h>

void *res_load(const char *filename, void *old_buffer, int index)
{
    FILE     *f;
    uint32_t *entry;
    uint32_t  start;
    uint32_t  size;
    void     *buf;

    if (old_buffer)
        guest_free(old_buffer);

    f = fopen(filename, "rb");
    if (!f) {
        printf("\n\n File not found %s!!! \n\n", filename);
        exit(1);
    }

    entry = (uint32_t *)guest_malloc(8);
    fseek(f, 4L * index + 6, SEEK_SET);
    fread(entry, 1, 8, f);

    start = entry[0];
    size  = entry[1] - entry[0];
    guest_free(entry);
    guest_store_u32(RES_SIZE_ADDR, size);   /* what the game reads */

    buf = guest_malloc(size);
    if (!buf) {
        printf("Out of Memory at Load %s Number:%d!!\n", filename, index);
        exit(1);
    }

    fseek(f, (long)start, SEEK_SET);
    fread(buf, 1, size, f);
    fclose(f);

    return buf;
}

/* 0x2EB9F - blit sub-image `index` of an LMI buffer.
 *
 *   hdr = buf + *(u32 *)(buf + 8 + 4*index);     offset table starts at +8
 *   w   = *(u16 *)hdr;  h = *(u16 *)(hdr + 2);
 *   rle_decode(hdr + 9, w, h, dst, pitch, mode);
 *
 * The 9-byte header (w, h, plus 5 bytes the decoder does not need here) and
 * the RLE stream are the format rle_decode already handles. */
void res_blit(void *buf, int index, void *dst, int pitch, int mode)
{
    uint8_t *hdr = (uint8_t *)buf + *(uint32_t *)((uint8_t *)buf + 4 * index + 8);
    uint16_t w   = *(uint16_t *)hdr;
    uint16_t h   = *(uint16_t *)(hdr + 2);

    rle_decode(hdr + 9, w, h, dst, pitch, mode);
}

/* 0x16886 - blit sub-image `index` of an LMI buffer whose offset table starts
 * at +6, always at (0,0):
 *
 *     rle_decode(buf + *(u32 *)(buf + 4*index + 6), 0, 0, dst, pitch, -1);
 *
 * (0x2EB9F/res_blit is the +8 variant that also takes x/y.) */
void res_blit6(void *dst, int pitch, void *buf, int index)
{
    uint8_t *src = (uint8_t *)buf + *(uint32_t *)((uint8_t *)buf + 4 * index + 6);

    rle_decode(src, 0, 0, dst, pitch, -1);
}
