/* ev4.h - FD2 scene/map/animation helpers, batch 4.
 *
 * The four list with the highest remaining usage, plus the map-window steppers
 * they and the 0x1199C handlers share:
 *
 *   0x1366A  scene move-animation driver   (usage 110, the #1 game function)
 *   0x11AA8  wait for a key while animating the map
 *   0x11B48  step the map window up
 *   0x11B9B  step the map window down
 *   0x11BFA  step the map window right
 *   0x11C59  step the map window left
 *   0x12263  refresh the per-cell record counts
 *   0x1E1DC  enqueue a record's cursor sprite (4 slots)
 *   0x24B4D  scroll-animation frames
 *   0x196CB  close the scene portrait (band blits + VGA restore + free)
 *
 * Same ABI discipline as ev2/ev3: cdecl, arguments decoded from the pushes.
 * 0x1366A / 0x196CB / 0x11AA8 / 0x24B4D / 0x12263 take no dispatcher argument;
 * 0x1E1DC takes one (the record index).
 *
 * Verified against the machine code by src/ev2check.c (batch-4 entries).
 */
#ifndef GAME_EV4_H
#define GAME_EV4_H

#include <stdint.h>

void ev4_1366A(int sel);
int  ev4_11AA8(void);
void ev4_11B48(void);
void ev4_11B9B(void);
void ev4_11BFA(void);
void ev4_11C59(void);
void ev4_12263(void);
void ev4_1E1DC(int rec);
void ev4_24B4D(int frames);
void ev4_196CB(void);

#endif /* GAME_EV4_H */
