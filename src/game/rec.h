/* rec.h - FD2 character record table (source translation).
 *
 *   0x34894  rec_flag    bit 0 of record byte +5
 *   0x12C60  rec_find    search the record tables for a byte +8 match
 *
 * Two tables of 80-byte records live in the game's data segment:
 *
 *   dword_53A45  table 1, dword_53BEB records
 *   dword_53BF7  table 2, dword_53BFB records
 *
 * Both are filled by sub_10010 from the save blob: table 1 is
 * `memmove(dword_53A45, blob + 4771, 80 * count)` with the count taken from
 * `blob[12484]`, table 2 is `memmove(dword_53BF7, blob + 2211, 2560)` (32
 * records) with the count from `blob[12492]`. Observed record fields:
 * +0/+1 a word (compared against qword_53AB1 by sub_12C0D), +2 an icon index
 * (filled from FD.ICON.B24), +5 a flag byte, +7 the DATO resource index used
 * by the dialogue renderer, +8 the id these two functions search on.
 *
 * rec_find(want):
 *   1. dword_53C1B = 0;
 *   2. scan table 1; for every record whose byte +8 equals `want` (unsigned
 *      byte compare, so want > 255 can never match) remember it in
 *      dword_53C1B and return its index as soon as its flag bit 0 is 0;
 *   3. if table 1 produced no match at all, scan table 2 - and note that this
 *      second loop does not stop at the first hit, so dword_53C1B ends up on
 *      the *last* matching record;
 *   4. return -1.
 *
 * The caller (sub_15F84) uses the return value only as "found an unflagged
 * record" and reads the portrait from dword_53C1B afterwards.
 *
 * Verified against the machine code by src/reccheck.c.
 */
#ifndef GAME_REC_H
#define GAME_REC_H

#include <stdint.h>

#define REC_STRIDE 80

/* 0x34894 */
int rec_flag(int index);

/* 0x12C60 - returns the index, or -1 (see the note about table 2 above) */
int rec_find(int want);

#endif /* GAME_REC_H */
