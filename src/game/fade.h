/* fade.h - palette fade / animation helpers of the main state machine family.
 *
 *   0x11D40  pal_fade_range(start, end, sub)  DAC range fade by subtraction
 *   0x11DF2  pal_fade_add(start, end, add)    DAC range brighten by addition
 *   0x1F882  pal_fade_out()                   for i=0..63:  fade_range(0,255,i)
 *   0x1F525  pal_fade_in()                    for i=64..0:  fade_range(0,255,i)
 *   0x4E310  pal_tick_word()                  the BIOS tick word (BDA 0x46C)
 *   0x4E31C  pal_anim_step()                  upload the next 0xE0..0xEF palette
 *
 * These are the "scene transition" fades plus the map palette animation
 * (0x4E31C). Docs/rounds/20-scene-card.md §50.2 has the disassembly of the
 * fades; docs/rounds/38-palette-and-map-refresh.md covers the animation.
 */
#ifndef GAME_FADE_H
#define GAME_FADE_H

#include <stdint.h>

/* Write DAC entries [start..end], each channel = max(0, palette[i] - sub).
 * `palette` is the game's own copy at *(0x53A65), 3 bytes per entry. */
void pal_fade_range(int start, int end, int sub);

void pal_fade_out(void);   /* progressively darken to black */
void pal_fade_in(void);    /* restore from black            */

/* 0x11DF2 - add `add` to each channel of DAC entries [start..end], clamp 0x3F */
void pal_fade_add(int start, int end, int add);

/* 0x4E310 - read the BIOS tick word (BDA 0x46C) through the low-memory mirror,
 * zero-extended to 16 bits. */
uint16_t pal_tick_word(void);

/* 0x4E31C - advance the DAC palette animation: every >= 2 BIOS ticks upload
 * the 16 entries at 0x60003 (3 bytes each) to DAC indices 0xE0..0xEF. */
void pal_anim_step(void);

#endif /* GAME_FADE_H */
