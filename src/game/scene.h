/* scene.h - scene/state routines of the main state machine family.
 *
 * These are the functions the main loop (0x25BF4) and the two dispatch tables
 * `funcs_25E23[]`/`funcs_25E3A[]` reach. Each one is a self-contained sequence
 * of service calls, so they translate one at a time (docs/TRANSLATION.md §5).
 */
#ifndef GAME_SCENE_H
#define GAME_SCENE_H

/* 0x22E5C - the `dword_53ECC == 1` transition card. Fades the palette to
 * black, loads FDOTHER.DAT resource 79, clears the screen, blits sub-image 0,
 * fades back in, blits sub-image 1, frees the buffer. Its exact role in the
 * story is UNCONFIRMED (the name is descriptive, not proven). */
void scene_card(void);

#endif /* GAME_SCENE_H */
