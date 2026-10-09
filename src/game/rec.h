/* rec.h - FD2 character record table (source translation).
 *
 *   0x34894  rec_flag    bit 0 of record byte +5
 *   0x12C60  rec_find    search the record tables for a byte +8 match
 *   0x1B8A6  rec_slot_free   count of the eight free slots
 *   0x1B83D  rec_slot_find   first slot with state bit 6 set
 *   0x1CA89  rec_sub_table5  word +68 -= byte 5 of the 0x619FD entry
 *   0x13512  rec_flag_or80   byte +5 |= 0x80
 *   0x32975  rec_flag_set1   byte +5 = 1
 *   0x34D64  rec_status_mask_records  records 10..27: byte +52 &= 0x80
 *   0x35009  rec_status_set_record14  record 14: byte +52 = 0x83
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

/* 0x1F183 - 1 when the cell-sprite refresh must skip this record */
int rec_skip(int index);

/* The eight 2-byte slots of a record live at byte +10: [state, value]. */

/* 0x1B722 - value byte (offset +11 + 2*slot) of record `index`. */
int rec_field_byte(int index, int slot);

/* 0x344F2 - for records [start,end], byte +52 = (+52 & 0xF0) | value. */
void rec_status_set(int start, int end, int value);

/* 0x1BB8C - claim the first empty slot of record `index`; 1 or -1. */
int rec_slot_claim(int index, int value);

/* 0x1B8E7 - left-shift delete slot `slot`; returns the memmove destination. */
void *rec_slot_remove(int index, int slot);

/* 0x1B8A6 - number of the eight slots whose state byte has bit 7 clear. */
int rec_slot_free(int index);

/* 0x1B83D - first slot with state bit 6 set and value byte < 0x80
 * (want_high == 0) or >= 0x80 (want_high != 0); -1 when none. */
int rec_slot_find(int index, int want_high);

/* 0x1CA89 - word +68 -= byte 5 of entry `tidx` of the 7-byte table at 0x619FD;
 * returns the record address (the machine code returns it in EAX). */
uint32_t rec_sub_table5(int index, int tidx);

/* 0x13512 / 0x32975 - flag writers; both return 80*index, not the address. */
int rec_flag_or80(int index);
int rec_flag_set1(int index);

/* 0x34D64 / 0x35009 - no arguments (the dispatch pushes one that is unused). */
uint32_t rec_status_mask_records(void);
uint32_t rec_status_set_record14(void);

/* 0x1C269 - collect the indices of the set bits in record[index] bytes
 * +26..+30 (bit b of byte i -> index 8*i+b). Returns the count; `out` may be
 * NULL for a dry run. */
int rec_collect_slot_bits(int index, uint8_t *out);

#endif /* GAME_REC_H */
