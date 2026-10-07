/* fx.h - "effect animation" handlers of the funcs_30469[] dispatch table.
 *
 * funcs_30469 @0x524C6 is a ten-entry table of per-frame particle emitters,
 * reached through the `call funcs_30469[reg*4]` sites in sub_2FF01 and
 * sub_31266. Each handler keeps its own little particle array (dwords in the
 * game data segment), draws a batch of sub-images with res_blit every frame
 * and fires a sample when a particle crosses a threshold; the return value is
 * the number of frames to wait before the next call (or a "done" flag).
 *
 * The four translated here (table entries [4]/[7]/[8]/[9]) are pure: they
 * read/write game globals, memcpy a read-only table from its original address
 * and call three already-translated services (res_blit 0x2EB9F,
 * svc_play_sfx 0x25A96, svc_play_sfx2 0x25B45, util_rand 0x4EBE3).
 *
 * True ABI = cdecl, five stack arguments (the `push <frame>; call 0x3702F`
 * Watcom stack probe that Hex-Rays mistakes for register parameters is the
 * same artifact as in vm.c/scene.c, see docs/rounds/08 §37.3):
 *
 *   int f(int rec, const void *buf, void *dst, int pitch, int selector);
 *
 * `rec` indexes the 80-byte character record table (dword_53A45 + 80*rec) and
 * is only read by fx_dots6/fx_dots3 (+143 / +130 table bias when
 * record[+6] == 0); `buf`/`dst`/`pitch` are passed straight to res_blit;
 * `selector` (low byte) selects the phase. Verified byte-for-byte against the
 * original machine code by src/fxcheck.c.
 */
#ifndef GAME_FX_H
#define GAME_FX_H

#include <stdint.h>

/* funcs_30469[4]  0x2C217 - six particles, 0x525B5 offset table */
int fx_dots6(int rec, const void *buf, void *dst, int pitch, int selector);

/* funcs_30469[7]  0x2CAFC - three particles, 0x5261E offset table */
int fx_dots3(int rec, const void *buf, void *dst, int pitch, int selector);

/* funcs_30469[8]  0x2CCF4 - sixteen particles, 0x52646 byte table */
int fx_dots16(int rec, const void *buf, void *dst, int pitch, int selector);

/* funcs_30469[9]  0x2CE1A - two-frame flip, no particle array */
int fx_toggle(int rec, const void *buf, void *dst, int pitch, int selector);

#endif /* GAME_FX_H */
