/* sprite24.h - FD2 24x24 sprite RLE family (source translation).
 *
 * The original has seven hand-written copies of the same 24x24 RLE state
 * machine, differing only in how a stream colour byte is turned into a
 * framebuffer pixel (and what the transparent/control token does). The
 * addresses are the entry points in our build (FD2.EXE md5 a6e341a8...):
 *
 *   0x4DF84  sprite24_ramp      colour = base + ((rot + c) & 7);  T11 = skip
 *   0x4E016  sprite24_pal_recolor  colour = pal[c];  T11 = recolor pixels
 *                                  already in the destination through pal
 *   0x4E0A2  sprite24_pal       colour = pal[c];  T11 = skip
 *   0x4E127  sprite24_const     colour = (u8)stride (!);  T11 = skip
 *   0x4E1A6  sprite24_ramp24    colour = (c & 7) + 24;  T11 = skip
 *   0x4E22A  sprite24_plain     colour = c;  T11 = skip
 *   0x4E29C  sprite24_plain49   colour = c;  T11 = fill 0x49
 *
 * Stream format (identical layout to the 0x4E98D picture RLE, but the
 * picture is always 24x24 and rows are completed before moving on):
 *
 *   token = [2-bit type | 6-bit count-1]
 *     00  solid run      +1 colour byte   -> count pixels
 *     01  odd run        +1 colour byte   -> count pixels at offsets 1,3,5..
 *     10  literal        +count bytes     -> count pixels, each mapped
 *     11  special        (no bytes)       -> skip / recolor / fill 0x49
 *
 * Every emitted pixel goes through the mode's colour map (identity for the
 * plain modes). A row is exactly 24 pixels; the destination advances by
 * `stride - 24` at the end of each row. The functions read/write raw
 * framebuffer addresses and are cdecl - no register-passed arguments.
 *
 * `sprite24_const` really does take its colour from the low byte of the
 * stride argument (the original reloads arg2 into EAX and moves AL into AH);
 * callers pass a packed `stride | colour` value.
 *
 * Verified byte-for-byte against the machine code by src/sprite24check.c
 * (build.ps1 -Target sprite24check). Disassembly: re/sprite24_disasm.txt.
 */
#ifndef GAME_SPRITE24_H
#define GAME_SPRITE24_H

#include <stdint.h>

/* 0x4DF84 - 8-colour ramp: base + ((rot + stream_colour) & 7) */
void sprite24_ramp(const void *src, void *dst, int stride, int base, int rot);

/* 0x4E016 - palette lookup, transparent runs RECOLOUR existing pixels */
void sprite24_pal_recolor(const void *src, void *dst, int stride,
                          const void *palette);

/* 0x4E0A2 - palette lookup, transparent runs skipped */
void sprite24_pal(const void *src, void *dst, int stride, const void *palette);

/* 0x4E127 - every pixel painted (u8)stride; transparent runs skipped */
void sprite24_const(const void *src, void *dst, int stride);

/* 0x4E1A6 - fixed ramp: (stream_colour & 7) + 24 */
void sprite24_ramp24(const void *src, void *dst, int stride);

/* 0x4E22A - stream colours as-is, transparent runs skipped */
void sprite24_plain(const void *src, void *dst, int stride);

/* 0x4E29C - stream colours as-is, transparent runs filled with 0x49 */
void sprite24_plain49(const void *src, void *dst, int stride);

#endif /* GAME_SPRITE24_H */
