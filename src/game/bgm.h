/* bgm.h - background music entry (source translation of 0x25977).
 *
 * Signature (proved from the 32 call sites and the disassembly): cdecl
 *   void bgm_play(int track, int loop_count);
 * track == -1 fades the current sequence out; tracks 16/17 (victory) skip the
 * 2 s fade-in. See docs/rounds/06-audio-fade.md for why the fade exists.
 */
#ifndef GAME_BGM_H
#define GAME_BGM_H

void bgm_play(int track, int loop_count);

#endif /* GAME_BGM_H */
