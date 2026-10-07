/* anim.c - animation frame counters (source translation of 0x1297D).
 *
 *   tick = (int16) BDA[0x46C];
 *   if (tick - dword_53C0F > 4 || tick - dword_53C0F < 0) {
 *       if (++dword_53C0B == 4) dword_53C0B = 0;
 *       dword_53C0F = tick;
 *   }
 *   if (++dword_53C07 == 4) dword_53C07 = 0;
 *
 * The BIOS tick is read through the low-memory mirror (the machine code's
 * absolute 0x46C immediate is redirected there by dos_patch_lowmem_refs).
 * The return value is unused by all callers.
 */
#include "anim.h"
#include "../dos.h"

#include <stdint.h>

#define BDA_W(off)  (*(volatile uint16_t *)(uintptr_t)(DOS_LOWMEM_BASE + (off)))
#define dword_53C0F (*(int32_t  *)(uintptr_t)0x00053C0Fu)
#define dword_53C0B (*(int32_t  *)(uintptr_t)0x00053C0Bu)
#define dword_53C07 (*(int32_t  *)(uintptr_t)0x00053C07u)

void anim_frame_step(void)
{
    int32_t tick = (int16_t)BDA_W(0x46C);

    if (tick - dword_53C0F > 4 || tick - dword_53C0F < 0) {
        if (++dword_53C0B == 4)
            dword_53C0B = 0;
        dword_53C0F = tick;
    }
    if (++dword_53C07 == 4)
        dword_53C07 = 0;
}
