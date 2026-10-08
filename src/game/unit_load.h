/* unit_load.h - FD2 unit sprite builder (source translation).
 *
 * The chain that turns the per-chapter character templates into the
 * 80-byte draw records the rest of the game renders, plus the sprite atlas
 * that is cached into FD2.TMP:
 *
 *   0x10B4E  unit_sprites_build  load FDICON.B24 + FDFIELD.DAT, build one
 *                                record per matching state entry, rewrite
 *                                FD2.TMP from the accumulated atlas
 *   0x10C50  unit_entry_build    build one 80-byte record
 *   0x11019  unit_entry_data     append one FDICON.B24 blob to the atlas
 *   0x145CD  unit_mark_nearby    flag every record on one side of the counter
 *   0x14625  unit_reveal_around  reveal a 3x3 map neighbourhood around (x,y)
 *   0x1B750  unit_metrics        fold the eight equipment slots into the
 *                                record's bounding box (+72/+74/+76)
 *   0x32999  unit_map_render     the 12-step "units appear" cut-scene
 *
 * All seven run against the game's own data-segment globals by their original
 * addresses (same rule as scene.c/ev.c).  The two heap buffers that outlive a
 * call - the FD2.TMP atlas in dword_53A61 and the FDFIELD buffer in
 * dword_53A59 - go through guest_mem.h so they stay on the game's heap.
 *
 * Verified against the machine code by src/ev6check.c.
 */
#ifndef GAME_UNIT_LOAD_H
#define GAME_UNIT_LOAD_H

#include <stdint.h>

/* 0x10B4E - (re)build the records whose state byte +152 equals `idx`, then
 * write the 0x32A00-byte atlas to FD2.TMP.  Returns fclose's value. */
int unit_sprites_build(int idx);

/* 0x10C50 - build record dword_53BEB from FDFIELD.DAT entry `idx`; the sprite
 * blob is appended to the atlas via `fh` (the open FDICON.B24). */
void unit_entry_build(int idx, void *fh);

/* 0x11019 - make sure FDICON.B24 entry `idx` is in the atlas; returns its
 * atlas slot index. */
int unit_entry_data(int idx, void *fh);

/* 0x145CD - for every record whose +5 bit0 is clear, set the +0x40 reveal
 * bits around its cell when its +6 is zero (mode 0) / non-zero (mode != 0). */
void unit_mark_nearby(int mode);

/* 0x14625 - reveal cell (x,y) and its four orthogonal neighbours. */
void unit_reveal_around(int x, int y);

/* 0x1B750 - recompute record `idx`'s screen extent (+72/+74/+76). */
void unit_metrics(int idx);

/* 0x32999 - run the 12-frame unit reveal animation for the sprite group
 * selected by `idx`. */
void unit_map_render(int idx);

#endif /* GAME_UNIT_LOAD_H */
