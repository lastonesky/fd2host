/* anim.h - animation frame counters (main state machine family). */
#ifndef GAME_ANIM_H
#define GAME_ANIM_H

/* 0x1297D - advance the two 0..3 frame counters (dword_53C0B, dword_53C07)
 * whenever the BIOS tick has moved more than 4 since dword_53C0F. */
void anim_frame_step(void);

/* 0x311E5 - step a small menu sprite animation: blit the current
 * (frame, substep) cursor from the frame bank `frames` into `dst`, then
 * advance the cursor; `mode == 0` just resets it. */
void anim_cycle_frame(const void *frames, int mode, void *dst, int pitch);

#endif /* GAME_ANIM_H */
