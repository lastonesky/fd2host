/* util.h - FD2 object-0 byte / palette utility functions (source translation).
 *
 * Original entry points in our build (FD2.EXE md5 a6e341a8...):
 *
 *   0x4DED4  util_rec3        pointer into a 3-byte-record table
 *   0x4DEEC  util_translate   buf[i] = table[buf[i]]   (in place)
 *   0x4DF09  util_sum_tail4   sum of (n - 4) bytes
 *   0x4DF28  util_deobfuscate rolling 16-bit XOR cipher (in place)
 *   0x4DF4C  util_fix_records normalise 4-byte records in a header block
 *   0x4E795  util_mask_recolor remap destination pixels through a palette
 *                                where a mask byte is non-zero
 *
 * All are plain cdecl functions operating on caller-supplied buffers (no
 * register arguments), so the differential test can call them directly.
 * The original loops use x86 `loop` / `dec+jnz`, i.e. they are bottom-tested
 * do-while loops; a zero count would wrap (2^32 / 2^16 iterations). Game
 * data never does that and the translations preserve the do-while form.
 *
 * util_sum_tail4 really does ignore the last four bytes of the buffer: it
 * adds bytes [0 .. n-5]. Kept verbatim (the original `sub ecx, 4`).
 *
 * Verified byte-for-byte against the machine code by src/utilcheck.c
 * (build.ps1 -Target utilcheck). Disassembly: re/util_disasm.txt.
 */
#ifndef GAME_UTIL_H
#define GAME_UTIL_H

#include <stdint.h>

/* original word_6017B, written by util_mask_recolor */
extern uint16_t util_mask_w;

/* 0x4DED4 - return base + 3*index (the original hardcodes base 0x60181;
 * the C translation takes it as a parameter so the table can be data). */
const void *util_rec3(const void *base, int index);

/* 0x4DEEC - table[count]: replace each byte of buf by table[byte]. */
unsigned char util_translate(const void *table, uint32_t count, void *buf);

/* 0x4DF09 - return the sum of the first (n - 4) bytes of buf. */
int util_sum_tail4(const void *buf, uint32_t n);

/* 0x4DF28 - in-place rolling cipher: state=0xA5; each byte does
 * state = rol16(state + 0x9014, 3); buf[i] ^= (state & 0xFF). */
unsigned char util_deobfuscate(void *buf, uint32_t n);

/* 0x4DF4C - header [u8 a][..][u8 b]: count = a*b 16-bit; for every 4-byte
 * record at +4 set byte 3 = 0xFF, byte 2 &= 0x1F, byte 1 &= 0x03. */
unsigned char util_fix_records(void *header);

/* 0x4E795 - header [u16 w][u16 h][w*h mask bytes]: for each pixel, if the
 * mask byte is non-zero the destination pixel is replaced by palette[pixel].
 * Destination rows are `stride` apart. */
unsigned char util_mask_recolor(void *dst, const void *header, int stride,
                                const void *palette);

#endif /* GAME_UTIL_H */
