/* ev5.h - FD2 small scene/map/record leaves, batch 5.
 *
 * Twenty-eight dependency-closed leaves picked from the ready set
 * (re/func_ranking.csv): record/cell predicates, map-cell collectors,
 * a palette fill, tick waits, party-record resets and four rec_flag-driven
 * event handlers. All cdecl; argument counts decoded from the pushes.
 *
 * Verified against the machine code by src/ev2check.c (batch-5 entries).
 */
#ifndef GAME_EV5_H
#define GAME_EV5_H

#include <stdint.h>

int  ev5_2860A(int a, int b);
void ev5_146A7(int x, int y);
int  ev5_13460(void);
void ev5_13536(void);
void *ev5_1D4CB(void);
void ev5_173E7(int *slots);
int  ev5_24B14(int a5);
void ev5_25052(int from, int ms);
void ev5_25089(void);
void ev5_34317(int val, uint8_t *out);
void ev5_1F6EF(int x, int y, int value, int n);
int  ev5_1C220(int value);
void ev5_1E5C0(int ticks);
int  ev5_2B749(const uint8_t *p);
void ev5_26C9B(void *dst, int pitch, int index);
void *ev5_314DE(const uint8_t *p);
int  ev5_1B5F1(int kind);
int  ev5_14B16(uint8_t *out);
void ev5_203BD(int r, int g, int b);
void ev5_208CF(void);
void ev5_20AAF(void);
void ev5_20BF5(void);
void ev5_20B72(void);
void ev5_205B4(void);
void ev5_205BE(void);
void ev5_1F04A(int a, int b);
int  ev5_1F0DC(int a, int b);
void ev5_1B653(uint8_t *out);

#endif /* GAME_EV5_H */
