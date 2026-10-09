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

/* 0x311E5 - step a small menu sprite animation. */
typedef void (*res_blit_fn)(void *, int, void *, int, int);
#define ORIG_RES_BLIT ((res_blit_fn)(uintptr_t)0x0002EB9Fu)
#define byte_54130 (*(uint8_t *)(uintptr_t)0x00054130u)
#define byte_54131 (*(uint8_t *)(uintptr_t)0x00054131u)

void anim_cycle_frame(const void *frames, int mode, void *dst, int pitch)
{
    const uint8_t *f = (const uint8_t *)(uintptr_t)frames;
    unsigned n;

    if (mode == 0) {
        byte_54130 = 0;
        byte_54131 = 0;
        return;
    }
    ORIG_RES_BLIT((void *)f, byte_54131, dst, pitch, mode);
    n = f[*(const uint32_t *)(const void *)(f + 4 * byte_54131 + 8) + 6u];
    if ((unsigned)++byte_54130 >= n) {
        byte_54130 = 0;
        if ((unsigned)++byte_54131 >= f[0])
            byte_54131 = 0;
    }
}
