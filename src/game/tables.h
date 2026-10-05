/* tables.h - generic index helpers for the obj0 table accessors.
 *
 * The original functions at 0x4E7DD..0x4E8BC are one-liners that turn an
 * index into a pointer (or, for 0x4E87D, a dword) into a data table:
 *
 *   0x4E7DD  base 0x615FE  stride  2  offset -64   (index-0x20)*2
 *   0x4E7F2  base 0x626B3  stride 12
 *   0x4E809  base 0x6238D  stride 31  offset -31   (index-1)*31
 *   0x4E821  base 0x620A1  stride 11
 *   0x4E838  base 0x61DA1  stride 24
 *   0x4E84F  base 0x61AF9  stride 10
 *   0x4E866  base 0x619FD  stride  7
 *   0x4E87D  dword table at 0x61955, index*4
 *   0x4E88E  base 0x6188A  stride  7
 *   0x4E8A5  base 0x61646  stride 20
 *   0x4E8BC  base 0x602AD  stride 23
 *
 * They are pure arithmetic: base + stride*index + offset (32-bit unsigned
 * wrap, as the original `mul edx` does). The bases are guest data addresses,
 * so the C helpers take the base as a parameter; src/repl.c supplies the
 * fixed FD2 addresses.
 *
 * Verified against the machine code by src/tablescheck.c.
 */
#ifndef GAME_TABLES_H
#define GAME_TABLES_H

#include <stdint.h>

/* base + stride*index + offset, with 32-bit unsigned arithmetic */
void *tbl_ptr(void *base, int stride, int index, int offset);

/* dword at base + 4*index */
uint32_t tbl_u32(const void *base, int index);

#endif /* GAME_TABLES_H */
