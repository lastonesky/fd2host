/* kbd.c - BIOS keyboard-buffer helpers (source translation of 0x4E381/0x10620).
 *
 * Both are pure BDA accesses, no other service:
 *
 *   0x4E381  mov esi,41Ah / mov ax,[esi] / mov [esi+2],ax / retn
 *            -> BDA[0x41C] = BDA[0x41A]          (drop the pending keys)
 *
 *   0x10620  return BDA[0x41C] != BDA[0x41A]    (signed word compare; the
 *            equality test is the same either way)
 *
 * The machine code reads/writes the first 64 KiB, which Windows will not map;
 * in the host dos.c owns a mirror at DOS_LOWMEM_BASE and redirects the game's
 * absolute references into it, so the C uses that same base.
 */
#include "kbd.h"
#include "../dos.h"

#define BDA_W(off) (*(volatile uint16_t *)(uintptr_t)(DOS_LOWMEM_BASE + (off)))

void kbd_flush(void)
{
    BDA_W(0x41C) = BDA_W(0x41A);
}

int kbd_pending(void)
{
    return BDA_W(0x41C) != BDA_W(0x41A);
}
