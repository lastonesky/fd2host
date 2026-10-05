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

#endif /* GAME_DLG_H */
