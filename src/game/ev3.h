/* ev3.h - FD2 funcs_1199C scene-script handlers, third batch (cluster close).
 *
 * The 30 functions of src/game/ev2.c left 23 entries of the `funcs_1199C`
 * (0x51B91) scene-script cluster (indices 38..90) plus their shared helpers.
 * This module finishes the club:
 *
 *   索引 39/40/48/52/55/56/57/58/59/62/65/66/67/69/70/71/72/73/75/76/77/80/83
 *     0x352CA 0x35346 0x35468 0x355F0 0x356B3 0x35730 0x357DD 0x35833
 *     0x35854 0x35A0D 0x35C40 0x35CF1 0x35D1E 0x35D9E 0x35E0E 0x35E5B
 *     0x35EC1 0x35F48 0x35F88 0x35FCF 0x360B6 0x3623C 0x362E8
 *
 *   helpers (called by the entries, not table entries themselves)
 *     0x35B78  scene step: scroll + load + palette fades + view + delay
 *     0x35F10  clear +64 of records [from..count) then notify
 *     0x361B0  palette ramp up 0..63 then down 62..0
 *     0x2AEDB  find the slot of a record whose field byte equals a value
 *     0x33F78  portrait pair helper (two unwired draw calls)
 *
 * The 726-byte script interpreter 0x1AA1D stays machine code in this batch
 * (it needs 0x197E5/0x19953/0x1B932 etc. first); the entries reach it through
 * its original address, so both the host and the differential harness are
 * consistent.
 *
 * ABI is the same as ev2.c: cdecl, one dispatcher argument (`void f(int)`).
 * Verified against the machine code by src/ev3check.c.
 */
#ifndef GAME_EV3_H
#define GAME_EV3_H

#include <stdint.h>

void ev3_352CA(int arg);
void ev3_35346(int arg);
void ev3_35468(int arg);
void ev3_355F0(int arg);
void ev3_356B3(int arg);
void ev3_35730(int arg);
void ev3_357DD(int arg);
void ev3_35833(int arg);
void ev3_35854(int arg);
void ev3_35A0D(int arg);
void ev3_35C40(int arg);
void ev3_35CF1(int arg);
void ev3_35D1E(int arg);
void ev3_35D9E(int arg);
void ev3_35E0E(int arg);
void ev3_35E5B(int arg);
void ev3_35EC1(int arg);
void ev3_35F48(int arg);
void ev3_35F88(int arg);
void ev3_35FCF(int arg);
void ev3_360B6(int arg);
void ev3_3623C(int arg);
void ev3_362E8(int arg);

/* helpers */
void ev3_35B78(int a, int b, int c);
void ev3_35F10(int from);
void ev3_361B0(void);
int  ev3_2AEDB(int rec, int value);
void ev3_33F78(int a, int b, int c);

#endif /* GAME_EV3_H */
