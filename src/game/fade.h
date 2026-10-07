/* fade.h - palette fade helpers of the main state machine family.
 *
 *   0x11D40  pal_fade_range(start, end, sub)  DAC range fade by subtraction
 *   0x1F882  pal_fade_out()                   for i=0..63:  fade_range(0,255,i)
 *   0x1F525  pal_fade_in()                    for i=64..0:  fade_range(0,255,i)
 *
 * These are the "scene transition" fades (0x22E5C and many state handlers use
 * them). Docs/rounds/20-scene-card.md §50.2 has the disassembly.
 */
#ifndef GAME_FADE_H
#define GAME_FADE_H

/* Write DAC entries [start..end], each channel = max(0, palette[i] - sub).
 * `palette` is the game's own copy at *(0x53A65), 3 bytes per entry. */
void pal_fade_range(int start, int end, int sub);

void pal_fade_out(void);   /* progressively darken to black */
void pal_fade_in(void);    /* restore from black            */

#endif /* GAME_FADE_H */

/* 0x11DF2 - add `add` to each channel of DAC entries [start..end], clamp 0x3F */
void pal_fade_add(int start, int end, int add);
