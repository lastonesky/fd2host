/* msg.h - dialogue portrait compositor (source translation).
 *
 *   0x1956B  msg_open_portrait   allocate the screen pair, stage the box,
 *                                mirror-blit the DATO portrait, compose bands
 *   0x1974C  msg_blit_band       restore one horizontal band of the portrait
 *   0x26996  msg_close_portrait  compose the remaining bands, restore the
 *                                screen and free the pair
 *
 * These three were the functions round 28 (§58.5) deliberately deferred: they
 * malloc three 64000-byte screen buffers, so they could only move to C once
 * the guest heap seam (src/game/guest_mem.h) existed. Every service they call
 * is already translated/plumbed (dlg_box_stage, res_load, rle2_blit_mirror), so
 * the cluster has no unclosed game dependency left.
 *
 * The three buffers live in the original data-segment globals
 * dword_53C5B / dword_53C5F / dword_53C63 (0x53C5B/F/63) because 48/62/91
 * not-yet-translated machine-code sites share them; the C reads and writes the
 * real dwords and keeps the buffers on the game heap. Verified against the
 * machine code by src/msgcheck.c.
 */
#ifndef GAME_MSG_H
#define GAME_MSG_H

/* 0x1956B - open the talking-portrait composite for portrait id `id`:
 * allocate the working/screen/staging buffers, snapshot the VGA into the
 * screen buffer, stage the 310x86 dialogue box (19x5 tiles at 5,112), select
 * the DATO sub-image offset from `id`, mirror-blit the portrait into the
 * staging buffer, then compose all six bands (rows 5..0). */
void msg_open_portrait(int id);

/* 0x1974C - compose one band: copy the saved screen into the working buffer,
 * overlay the 310-px-wide strip of the staging buffer at row `y` (clipped to
 * the 200-line screen), then push the working buffer back to the VGA. */
void msg_blit_band(int y, void *dst, void *src);

/* 0x26996 - finish the composite: compose the five lower bands (rows 1..5),
 * push the saved screen back to the VGA and free the three buffers. */
void msg_close_portrait(void);

#endif /* GAME_MSG_H */
