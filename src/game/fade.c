/* fade.c - palette fade helpers (source translation of 0x11D40/0x1F882/0x1F525).
 *
 * The game keeps its own copy of the palette at *(0x53A65) (3 bytes per entry).
 * A fade writes that palette minus `sub` (clamped at 0) to the DAC through the
 * Watcom `outp()`:
 *
 *     while (start <= end) {
 *         outp(0x3C8, start);                       // DAC write index
 *         outp(0x3C9, max(0, pal[start*3+0] - sub));
 *         outp(0x3C9, max(0, pal[start*3+1] - sub));
 *         outp(0x3C9, max(0, pal[start*3+2] - sub));
 *         start++;
 *     }
 *
 * The two fades are 64 steps of 2 ms (0x1F882 darkens 0→63, 0x1F525 restores
 * 64→0). Note the original writes only the low byte of the subtraction result,
 * which is what `(uint8_t)` does below.
 *
 * `outp` (0x37AE5) and `delay` (0x3790A) are called through their original
 * addresses: the host's dos.c owns the DAC ports (0x3C8/0x3C9) and the BIOS
 * tick, and src/fadecheck.c hooks the same addresses to compare call by call.
 */
#include "fade.h"
#include "../dos.h"      /* DOS_LOWMEM_BASE: BIOS data area mirror */

#include <stdint.h>

#define dword_53A65 (*(uint8_t **)(uintptr_t)0x00053A65u) /* palette copy */

/* Palette animation state (game data segment). The 16 RGB triples live in
 * the object at 0x60003; byte_60002 is the current frame 0..15 and
 * word_60000 remembers the tick at which the DAC was last written. */
#define word_60000 (*(uint16_t *)(uintptr_t)0x00060000u)
#define byte_60002 (*(uint8_t  *)(uintptr_t)0x00060002u)
#define pal_anim_data ((const uint8_t *)(uintptr_t)0x00060003u)

#define BDA_W(off) (*(volatile uint16_t *)(uintptr_t)(DOS_LOWMEM_BASE + (off)))

typedef int  (*outp_fn)(unsigned port, int value);
typedef void (*delay_fn)(unsigned ms);

#define OUTP  ((outp_fn) (uintptr_t)0x00037AE5u)
#define DELAY ((delay_fn)(uintptr_t)0x0003790Au)

void pal_fade_range(int start, int end, int sub)
{
    while (start <= end) {
        int k;

        OUTP(0x3C8, start);
        for (k = 0; k < 3; k++) {
            int v = dword_53A65[start * 3 + k] - sub;
            if (v < 0)
                v = 0;
            OUTP(0x3C9, v);
        }
        start++;
    }
}

void pal_fade_out(void)
{
    int i;

    for (i = 0; i < 64; i++) {
        pal_fade_range(0, 255, i);   /* subtract more and more -> black */
        DELAY(2);
    }
}

void pal_fade_in(void)
{
    int i;

    for (i = 64; i >= 0; i--) {
        pal_fade_range(0, 255, i);   /* subtract less and less -> normal */
        DELAY(2);
    }
}

/* 0x11DF2 - the sister of pal_fade_range: ADD `add` to each channel and clamp
 * at 0x3F (the DAC's 6-bit maximum). Used to brighten/fade in. */
void pal_fade_add(int start, int end, int add)
{
    while (start <= end) {
        int k;

        OUTP(0x3C8, start);
        for (k = 0; k < 3; k++) {
            int v = dword_53A65[start * 3 + k] + add;
            if (v > 63)
                v = 63;
            OUTP(0x3C9, v);
        }
        start++;
    }
}

/* 0x4E310 - the BIOS timer tick word (BDA 0x46C), zero-extended. The machine
 * code's absolute 0x46C immediate is redirected into the low-memory mirror by
 * dos_patch_lowmem_refs / patch_lowmem_refs, so this reads the same byte the
 * original does. */
uint16_t pal_tick_word(void)
{
    return BDA_W(0x46C);
}

/* 0x4E31C - advance the palette animation. Every >= 2 BIOS ticks the frame
 * counter wraps 0..15 and the 16 RGB triples at 0x60003 + 3*frame are uploaded
 * to DAC entries 0xE0..0xEF through the Watcom `outp` (ports 0x3C8/0x3C9).
 * word_60000 records the tick *after* the upload, and the gate is the 16-bit
 * difference `(uint16_t)(tick - word_60000) >= 2`, i.e. it handles the
 * 0xFFFF -> 0x0000 wrap. The data pointer is not reset per frame: frame f
 * reads 48 bytes starting at 3*f, exactly as the machine code's
 * `lodsb`-driven 48-byte loop does. */
void pal_anim_step(void)
{
    if ((uint16_t)(pal_tick_word() - word_60000) >= 2u) {
        const uint8_t *p;
        int            i, idx = 0xE0;

        if (++byte_60002 == 16)
            byte_60002 = 0;
        p = pal_anim_data + 3 * byte_60002;
        for (i = 0; i < 16; i++, idx++) {
            OUTP(0x3C8, idx);
            OUTP(0x3C9, *p++);
            OUTP(0x3C9, *p++);
            OUTP(0x3C9, *p++);
        }
        word_60000 = pal_tick_word();
    }
}
