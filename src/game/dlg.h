/* dlg.h - FD2 dialogue-box helpers (source translation).
 *
 *   0x16559  dlg_blit_dato   blit DATO sub-image `idx` to the box
 *   0x16E24  dlg_scroll_text scroll the dialogue text box up
 *
 * Both operate on the VGA frame buffer at 0xA0000 and read the original
 * global dword_53C67 (box position: 0x728 top / 0x9017 bottom). The C
 * version takes the box position (and, for the blit, the DATO buffer) as
 * parameters; src/repl.c supplies the FD2 globals when hooked.
 *
 * 0x16559: dato_buf is the loaded DATO.DAT resource; entry `idx` is a
 * u32 offset into it pointing at a [u16 w][u16 h][rle2 stream] sub-image.
 * The bottom box (0x9017) is blitted mirrored (rle2_blit_mirror), the top
 * one forward. `dlg_scroll_text` shifts the text box rows up (five 3-row
 * steps then one 4-row step) and clears the last three rows with 0x4A.
 *
 * Verified against the machine code by src/dlgcheck.c.
 */
#ifndef GAME_DLG_H
#define GAME_DLG_H

#include <stdint.h>

/* 0x16559 */
void dlg_blit_dato(const void *dato_buf, int box_pos, int idx);

/* 0x16E24 - no-op unless box_pos is 0x728 (top) or 0x9017 (bottom) */
void dlg_scroll_text(int box_pos);

/* --- box open/close animation (round 30) -------------------------------
 *
 *   0x165AC  dlg_open_box    glide the speaker portrait in, then draw the
 *                            box frame in five growing tile stages
 *   0x16B43  dlg_close_box   restore the five stages in reverse, glide out
 *   0x168B6  dlg_box_stage   draw one stage of the 310x86 box frame
 *   0x1685C  dlg_frame_tile  blit tile `idx` of the frame resource
 *
 * Unlike the two pure helpers above, these are app-level routines: they
 * live in the game's world and talk to the game's own data-segment
 * globals (dword_51A83 / 53A18 / 53A81 / 53AB9 / 53ABD / 53C67) by their
 * original addresses, exactly like the machine code does. The globals stay
 * game-owned, so src/repl.c can hook these four entries with no glue at
 * all. See the block comment in game/dlg.c for the services that are still
 * original machine code (CRT heap / delay / BDA coupling).
 *
 * dlg_open_box(face_x, face_y, rows):
 *   rows != 0  glide the portrait to (face_x, face_y) first (0x12CEA) and
 *              sweep the portrait sprite over the VGA from its current
 *              size (24*cols+4 x 24*rows+4) down to (5, rows);
 *   rows == 0  default rows from the active box position (0x728 -> 2,
 *              0x9017 -> 112, anything else stays 0).
 *   Then five 26668-byte stage snapshots are allocated into the original
 *   dword_53A18[5] array (returned) and the frame is drawn stage by stage.
 *
 * dlg_close_box(stages, rows): frees the five snapshots through 0x15E71
 * (restore + free) in reverse order, then sweeps the portrait sprite back
 * out to its full size when rows != 0. */
void *dlg_open_box(int face_x, int face_y, int rows);
void  dlg_close_box(void **stages, int rows);

/* 0x168B6 - draw tile grid stage (cols x lines, 16 px cells + 3 px border)
 * of the 310x86 box at surface + stride*y0 + x0. Tile art comes from the
 * frame resource in the original global dword_53A81. */
void dlg_box_stage(void *surface, int stride, int x0, int y0,
                   int cols, int lines);

/* 0x1685C - blit tile `idx` (offset table at table+6, dword offsets) of a
 * header-prefixed frame resource onto dest with the given stride. */
void dlg_frame_tile(void *dest, int stride, const void *table, int idx);

/* 0x164E8 - one character of typewriter dialogue: every second character
 * steps the mouth animation - the private phase counter dword_53A10 walks
 * 0,1,2,3 with 3 drawn as DATO sub-image 1, so the sequence repeats
 * 1,2,1,0 - then plays SFX bank index 2 once and waits one BIOS tick. Only caller is the word interpreter
 * sub_15F84. Verified by src/typecheck.c. */
int dlg_type_step(void);

/* 0x16C57 - wait for a key while animating (palette cycle, speaker tile
 * 18/19 flip, mouth open/close via DATO sub-images 3/0). `speaker != 0`
 * enables the speaker tile. Ends with INT 16h AH=10h into word_53A8D and
 * normalises the scan code in AH (E0h/52h -> 1Ch, 53h -> 01h).
 * Verified by src/keycheck.c. */
void dlg_wait_key(int speaker);

#endif /* GAME_DLG_H */
