/* fx.h - "effect animation" handlers of the funcs_30469[] dispatch table.
 *
 * funcs_30469 @0x524C6 is a ten-entry table of per-frame particle emitters,
 * reached through the `call funcs_30469[reg*4]` sites in sub_2FF01 and
 * sub_31266. Each handler keeps its own little particle array (dwords in the
 * game data segment), draws a batch of sub-images with res_blit every frame
 * and fires a sample when a particle crosses a threshold; the return value is
 * the number of frames to wait before the next call (or a "done" flag).
 *
 * The nine translated here (every table entry except [6] 0x2C67D) are pure:
 * they read/write game globals, memcpy a read-only table from its original
 * address and call four already-translated services (res_blit 0x2EB9F,
 * svc_play_sfx 0x25A96, svc_play_sfx2 0x25B45, util_rand 0x4EBE3). One
 * helper, fx_advance (0x2BF83), is shared by fx_blob only.
 *
 * True ABI = cdecl, five stack arguments (the `push <frame>; call 0x3702F`
 * Watcom stack probe that Hex-Rays mistakes for register parameters is the
 * same artifact as in vm.c/scene.c, see docs/rounds/08 §37.3):
 *
 *   int f(int rec, const void *buf, void *dst, int pitch, int selector);
 *
 * `rec` indexes the 80-byte character record table (dword_53A45 + 80*rec) and
 * biases the offset table (+148 / +20 / +143) when record[+6] == 0; `buf`/`dst`
 * /`pitch` are passed straight to res_blit; `selector` (low byte) selects the
 * phase. fx_advance instead takes (phase*, counter*, dst, pitch, buf).
 * Verified byte-for-byte against the original machine code by src/fxcheck.c.
 */
#ifndef GAME_FX_H
#define GAME_FX_H

#include <stdint.h>

/* funcs_30469[0]  0x2B996 - seven particles, 0x524EE direction table */
int fx_dots7(int rec, const void *buf, void *dst, int pitch, int selector);

/* funcs_30469[1]  0x2BB33 - eight particles, 0x5252D/0x5254D offset tables */
int fx_dots8(int rec, const void *buf, void *dst, int pitch, int selector);

/* funcs_30469[2]  0x2BD6C - no particle array, byte_53FB2 growth/slide */
int fx_blob(int rec, const void *buf, void *dst, int pitch, int selector);

/* helper          0x2BF83 - blit + advance a (phase, counter) pair */
int fx_advance(uint8_t *phase, uint8_t *counter, void *dst, int pitch,
               const void *buf);

/* funcs_30469[3]  0x2BFD9 - twelve particles, three rotating slots */
int fx_dots12(int rec, const void *buf, void *dst, int pitch, int selector);

/* funcs_30469[4]  0x2C217 - six particles, 0x525B5 offset table */
int fx_dots6(int rec, const void *buf, void *dst, int pitch, int selector);

/* funcs_30469[5]  0x2C441 - six particles, 0x525DD offset table */
int fx_dots6b(int rec, const void *buf, void *dst, int pitch, int selector);

/* funcs_30469[7]  0x2CAFC - three particles, 0x5261E offset table */
int fx_dots3(int rec, const void *buf, void *dst, int pitch, int selector);

/* funcs_30469[8]  0x2CCF4 - sixteen particles, 0x52646 byte table */
int fx_dots16(int rec, const void *buf, void *dst, int pitch, int selector);

/* funcs_30469[9]  0x2CE1A - two-frame flip, no particle array */
int fx_toggle(int rec, const void *buf, void *dst, int pitch, int selector);

#endif /* GAME_FX_H */
