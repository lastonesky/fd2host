/* gfx.h - FD2 object-0 graphics blitter helper family (source translation).
 *
 * Original machine code (our build, FD2.EXE md5 a6e341a8...):
 *
 *   0x4ECBF  gfx_save_rect       strided surface -> compact rectangle record
 *   0x4EC7C  gfx_restore_rect    rectangle record -> strided surface
 *   0x4ED0B  gfx_blit_block      header-prefixed block -> strided surface
 *   0x4ED34  gfx_blit_transparent  same, 0 bytes are transparent
 *   0x4ED7A  gfx_draw_glyph      16x16 1bpp glyph with fg / drop shadow / fill
 *   0x4EEE0  gfx_expand_scanlines 16-phase scanline re-interleave
 *
 * The save/restore pair share one rectangle record layout:
 *
 *   +0  u16 width
 *   +2  u16 height
 *   +4  i32 offset        (byte offset into the surface to restore to)
 *   +8  width*height bytes, row-major contiguous pixels
 *
 * gfx_save_rect writes the record starting at `record` (it also emits the
 * header); gfx_restore_rect reads it. The offset is the same field in both
 * directions, so a save+restore round-trips to the same surface location.
 *
 * Pixel/stride notes that must be preserved for exact parity:
 *  - record width/height and the blit widths are 16-bit; row counters are
 *    u16 do-while loops (a zero height would still run once in the original,
 *    but data never does that).
 *  - gfx_restore_rect / gfx_blit_block / gfx_save_rect take a FULL 32-bit
 *    stride; gfx_blit_transparent / gfx_draw_glyph truncate it to 16 bits
 *    (the original reloads word_627A3 / uses BP).
 *  - gfx_draw_glyph skips glyph index 10 entirely.
 *
 * Verified byte-for-byte against the machine code by src/gfxcheck.c
 * (build.ps1 -Target gfxcheck). Disassembly: re/gfx_helpers_disasm.txt.
 */
#ifndef GAME_GFX_H
#define GAME_GFX_H

#include <stdint.h>

/* Scratch globals mirrored from the original data segment so the
 * differential test can assert on them. Original addresses in comments. */
extern uint16_t gfx_rec_w;      /* 0x627B4 */
extern uint16_t gfx_rec_h;      /* 0x627B6 */
extern uint16_t gfx_pen_stride; /* 0x627A3 */
extern uint8_t  gfx_pen_fg;     /* 0x627A5 */
extern uint8_t  gfx_pen_fill;   /* 0x627A6 */
extern uint8_t  gfx_pen_shadow; /* 0x627A7 */
extern uint32_t gfx_pen_dest;   /* 0x627A8 */
extern uint32_t gfx_glyph_tab;  /* 0x627AC */
extern uint32_t gfx_glyph_idx;  /* 0x627B0 */

/* original byte_627C8 @ 0x627C8 - 16-byte scanline phase table (read-only) */
extern const uint8_t gfx_phase_table[16];

/* 0x4ECBF - copy a strided w*h rectangle at surface+offset into a compact
 * record. `record` must hold 8 + w*h bytes. */
void gfx_save_rect(void *record, int w, int h, const void *surface, int offset,
                   int stride);

/* 0x4EC7C - restore a record written by gfx_save_rect onto surface+offset. */
void gfx_restore_rect(const void *record, void *surface, int stride);

/* 0x4ED0B - blit a [u16 w][u16 h][pixels] block onto a strided surface. */
void gfx_blit_block(void *dest, const void *src, int stride);

/* 0x4ED34 - blit a [u16 w][u16 h][pixels] block, 0 pixels left untouched. */
void gfx_blit_transparent(void *dest, const void *src, int stride);

/* 0x4ED7A - draw glyph `index` of a 16-bit-row bitmask table at dest.
 * Each set pixel is `fg`; a drop shadow (`shadow`) is written one scanline
 * below (same column and one column to the left). If `fill` is non-zero the
 * whole 16x16 cell is pre-filled with it (glyph index 10 is then skipped). */
void gfx_draw_glyph(const void *glyph_table, int index, void *dest, int stride,
                    int fg, int shadow, int fill);

/* 0x4EEE0 - 192 scanlines: copy 312 bytes from src+4+row*320+table[idx] to a
 * 320-byte-stride destination, advancing the 4-bit phase `start` each row
 * (wrapping at 16). */
void gfx_expand_scanlines(const void *src, void *dest, int start);

#endif /* GAME_GFX_H */
