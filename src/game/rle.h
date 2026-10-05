/* rle.h - FD2 picture decoder (RLE blitter), source translation of obj0
 * 0x4E98D (rle_decode) and 0x4E8D3 (rle_decode_lut).
 *
 * Stream layout (one stream covers the whole picture):
 *
 *   +0  u16 width
 *   +2  u16 height
 *   +4  token stream, consumed continuously across rows; each row leaves
 *       (pitch - width) bytes in the destination untouched ("row skip")
 *
 * Token = one byte: [2-bit type | 6-bit count-1]:
 *
 *   00  solid run    +1 colour byte            -> count pixels
 *   01  odd run      +1 colour byte            -> count pixels on ODD
 *                                                  positions, consuming
 *                                                  2*count pixels
 *   10  literal      +count raw pixels
 *   11  skip         count transparent pixels (no stream bytes)
 *
 * A row is done when the pixel counter reaches zero *after* a token; the
 * original code checks at the loop bottom (do-while), so even a zero-width
 * row would consume one token first. Data in the game is always exact.
 *
 * Colour mapping (identical stream in every case - the colour byte is always
 * read from the stream, then transformed before writing):
 *
 *   rle_decode(..., mode):
 *     mode == -1            colours come from the stream as-is
 *     (u16)mode <= 0xFF     flat: every pixel painted (u8)mode, stream
 *                           colour bytes skipped
 *     else                  8-colour ramp (original: BL = (u8)mode,
 *                           BH = ((u16)mode >> 8)):
 *                             out = (u8)mode + (((u16)mode >> 8) + c) & 7
 *                           Usage of this branch is unconfirmed (all
 *                           verified call sites pass -1) - kept for exact
 *                           behavioural parity.
 *
 *   rle_decode_lut(..., palette):  colour = palette[c], 256-byte table.
 *
 * Side effects (original globals word_627B4 / word_627B6 at 0x627B4):
 * written on entry; rle_height counts the remaining rows down to 0.
 * The original returns leftover AL in EAX - no caller ever reads it.
 *
 * Verified byte-for-byte against the original machine code by
 * src/rlecheck.c (differential test, build.ps1 -Target rlecheck).
 */
#ifndef GAME_RLE_H
#define GAME_RLE_H

#include <stdint.h>

/* rle_decode mode: colours are taken from the stream (original arg -1) */
#define RLE_FROM_STREAM (-1)

extern uint16_t rle_width;    /* current picture width  (0x627B4) */
extern uint16_t rle_height;   /* rows left while decoding (0x627B6) */

/* original 0x4E98D - decode a picture onto dst at (x, y), pitch bytes/row */
void rle_decode(const void *src, int x, int y, void *dst, int pitch, int mode);

/* original 0x4E8D3 - same stream, colours translated through a 256-byte
 * palette table (original a6: linear address of the table) */
void rle_decode_lut(const void *src, int x, int y, void *dst, int pitch,
                    const void *palette);

#endif /* GAME_RLE_H */
