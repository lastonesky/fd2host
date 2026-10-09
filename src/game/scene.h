/* scene.h - scene/state routines of the main state machine family.
 *
 * These are the functions the main loop (0x25BF4) and the two dispatch tables
 * `funcs_25E23[]`/`funcs_25E3A[]` reach. Each one is a self-contained sequence
 * of service calls, so they translate one at a time (docs/TRANSLATION.md §5).
 */
#ifndef GAME_SCENE_H
#define GAME_SCENE_H

#include <stdint.h>

/* 0x22E5C - the `dword_53ECC == 1` transition card. Fades the palette to
 * black, loads FDOTHER.DAT resource 79, clears the screen, blits sub-image 0,
 * fades back in, blits sub-image 1, frees the buffer. Its exact role in the
 * story is UNCONFIRMED (the name is descriptive, not proven). */
void scene_card(void);

/* The funcs_25E23[] main-state-machine transition handlers this module
 * translates (table index in the name; the role of each state is UNCONFIRMED).
 * Each draws a vm_run() cell and ends by advancing the state index
 * dword_53C03 - 0x22EF6 *assigns* 1, the others increment it. */
void scene_state_00(void);  /* 0x22EF6 - funcs_25E23[0]  */
void scene_state_03(void);  /* 0x231BC - funcs_25E23[3]  */
void scene_state_10(void);  /* 0x23790 - funcs_25E23[10] */
void scene_state_12(void);  /* 0x2389F - funcs_25E23[12] */
void scene_state_14(void);  /* 0x239BD - funcs_25E23[14] */
void scene_state_18(void);  /* 0x23E39 - funcs_25E23[18] */

/* 0x233C6 - fade out, move records `first..last` to the coordinates in the
 * xs/ys arrays (kind is either a small literal or a byte array), optionally
 * reposition record `single`, set the view origin (origin) and fade back in. */
void scene_place_records(const uint8_t *xs, const uint8_t *ys, uintptr_t kind,
                         int first, int last, int single,
                         int sx, int sy, int skind, uint64_t origin);

#endif /* GAME_SCENE_H */
