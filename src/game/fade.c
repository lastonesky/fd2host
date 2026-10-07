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

#include <stdint.h>

#define dword_53A65 (*(uint8_t **)(uintptr_t)0x00053A65u) /* palette copy */

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
