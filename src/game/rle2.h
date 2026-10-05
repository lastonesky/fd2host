/* rle2.h - FD2 0xC0-range RLE blits (second RLE family).
 *
 * Original routines (our build, FD2.EXE md5 a6e341a8...):
 *
 *   0x4EBFF  rle2_blit         decode rows of `w` pixels, write forward
 *   0x4EC31  rle2_blit_mirror  same but write each row backwards (mirror)
 *   0x4EBAB  rle2_blit_trans   same as forward but 0 bytes are transparent
 *
 * Input block: [u16 width][u16 height][stream...]; the destination is a
 * strided surface (row pitch = `stride`). Rows are decoded continuously
 * (a run may cross a row boundary).
 *
 * Token stream (decoder 0x4EC66, whose state is (value, remaining)):
 *   byte c <= 0xC0        literal: pixel = c
 *   byte c in 0xC1..0xFF  run: next byte is the pixel, repeated (c - 0xC0) times
 * (so a run is 1..63 pixels). The original keeps the decoder state in
 * AH/AL across calls; the C version keeps it in a small struct.
 *
 * These are used by the dialogue renderer 0x15F84 to blit FDTXT/DATO text
 * rows. Verified against the machine code by src/rle2check.c.
 */
#ifndef GAME_RLE2_H
#define GAME_RLE2_H

#include <stdint.h>

/* 0x4EBFF - decode onto a strided surface, rows written left to right */
void rle2_blit(void *dst, const void *src, int stride);

/* 0x4EC31 - decode onto a strided surface, rows written right to left */
void rle2_blit_mirror(void *dst, const void *src, int stride);

/* 0x4EBAB - decode onto a strided surface, 0 bytes leave the pixel */
void rle2_blit_trans(void *dst, const void *src, int stride);

#endif /* GAME_RLE2_H */
