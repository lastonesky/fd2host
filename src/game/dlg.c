/* dlg.c - FD2 dialogue-box helpers (source translation of 0x16559, 0x16E24).
 *
 * See game/dlg.h. Both write the VGA frame buffer at 0xA0000; the box
 * position is the original global dword_53C67, passed in by the caller.
 */
#include "dlg.h"
#include "rle2.h"
#include <string.h>

#define VGA_BASE 0x000A0000u

void dlg_blit_dato(const void *dato_buf, int box_pos, int idx)
{
    const uint8_t *src = (const uint8_t *)dato_buf +
                         *(const uint32_t *)((const char *)dato_buf + 4 * idx);
    uint8_t       *dst = (uint8_t *)(uintptr_t)(VGA_BASE + (uint32_t)box_pos);

    if (box_pos == 0x9017)
        rle2_blit_mirror(dst, src, 320);   /* bottom box is mirrored */
    else
        rle2_blit(dst, src, 320);
}

void dlg_scroll_text(int box_pos)
{
    uint8_t *base;
    int      i, j, k;

    if (box_pos != 0x728 && box_pos != 0x9017)
        return;

    base = (uint8_t *)(uintptr_t)(uint32_t)((box_pos == 0x728) ? 0xA0B4Fu
                                                                 : 0xA951Fu);

    for (i = 0; i < 5; i++) {
        for (j = 0; j < 72; j++)
            memmove(base + 320 * j - 1, base + 320 * (j + 3) - 1, 208);
        memset(base + 23040, 0x4A, 208);
    }
    for (k = 0; k < 72; k++)
        memmove(base + 320 * k - 1, base + 320 * (k + 4) - 1, 208);
    memset(base + 23040, 0x4A, 208);
}
