/* rec.c - FD2 character record table (source translation).
 *
 *   0x34894  rec_flag       - one-line accessor: (base + 80*i + 5) & 1
 *   0x12C60  rec_find       - search both record tables for byte +8 == want
 *   0x1F183  rec_skip       - cell-sprite refresh predicate
 *   0x1B722  rec_field_byte - read the value byte of slot `slot` (+11 + 2*slot)
 *   0x344F2  rec_status_set - range-set the low nibble of record byte +52
 *   0x1BB8C  rec_slot_claim - claim the first empty of the eight 2-byte slots
 *   0x1B8E7  rec_slot_remove- left-shift delete one slot, mark the last 0x80
 *
 * App-level: the C reads and writes the same data-segment globals the machine
 * code does, so src/repl.c can hook all of them with no glue.
 *
 * The original accessors compute the record address as `i*5` then `<<4` (i.e.
 * an unsigned 80*i) and do the slot byte arithmetic in 32-bit registers - both
 * details matter for inputs outside the table's natural range and are
 * reproduced literally (uint32_t casts).
 *
 * Verified against the machine code by src/reccheck.c.
 */
#include <string.h>

#include "rec.h"

/* IDA names kept so the C reads like the original decompilation.
 * Addresses verified against E:\FD2\FD2.EXE.i64. */
#define dword_53A45 (*(uint32_t *)(uintptr_t)0x00053A45u) /* record table 1 */
#define dword_53BEB (*(int32_t *)(uintptr_t)0x00053BEBu)  /* table 1 records */
#define dword_53BF7 (*(uint32_t *)(uintptr_t)0x00053BF7u) /* record table 2 */
#define dword_53BFB (*(int32_t *)(uintptr_t)0x00053BFBu)  /* table 2 records */
#define dword_53C1B (*(uint32_t *)(uintptr_t)0x00053C1Bu) /* matched record */

/* 0x34894 */
int rec_flag(int index)
{
    const uint8_t *rec = (const uint8_t *)((uintptr_t)dword_53A45
                                           + REC_STRIDE * (uint32_t)index + 5);
    return rec[0] & 1;
}

/* 0x12C60 */
int rec_find(int want)
{
    const uint8_t *rec = (const uint8_t *)(uintptr_t)dword_53A45;
    int i;

    dword_53C1B = 0;

    for (i = 0; i < dword_53BEB; i++) {
        if (rec[8] == (uint8_t)want) {
            dword_53C1B = (uint32_t)(uintptr_t)rec;
            if (rec_flag(i) == 0)
                return i;
        }
        rec += REC_STRIDE;
    }

    /* Nothing matched in table 1: fall back to table 2, but keep scanning -
     * dword_53C1B ends up on the last match, and the result is still -1. */
    if (dword_53C1B == 0) {
        rec = (const uint8_t *)(uintptr_t)dword_53BF7;
        for (i = 0; i < dword_53BFB; i++) {
            if (rec[8] == (uint8_t)want)
                dword_53C1B = (uint32_t)(uintptr_t)rec;
            rec += REC_STRIDE;
        }
    }

    return -1;
}

/* 0x1B722 - the eight 2-byte slots of a record start at +10: each slot is
 * byte[0] = a state bitfield (bit 7 = empty) and byte[1] = a value. This
 * accessor returns the value byte of `slot` (offset +11 + 2*slot). */
int rec_field_byte(int index, int slot)
{
    const uint8_t *rec = (const uint8_t *)((uintptr_t)dword_53A45
                                           + REC_STRIDE * (uint32_t)index);
    return rec[2u * (uint32_t)slot + 11u];
}

/* 0x344F2 - clear the low nibble of byte +52 for records [start,end] and OR in
 * `value`. The machine code is `and bl,0F0h; or bl,cl` - it does NOT mask
 * `value` to four bits first, so any high bits of the byte leak through; the
 * loop bound is a signed compare, so start > end runs zero times. */
void rec_status_set(int start, int end, int value)
{
    int i;

    for (i = start; i <= end; i++) {
        uint8_t *rec = (uint8_t *)((uintptr_t)dword_53A45
                                   + REC_STRIDE * (uint32_t)i);
        rec[52] = (uint8_t)((rec[52] & 0xF0) | (uint8_t)value);
    }
}

/* 0x1BB8C - claim the first empty slot (state byte bit 7 set): clear the bit
 * and store `value` in the following byte. Returns 1 on success, -1 when all
 * eight slots are occupied. */
int rec_slot_claim(int index, int value)
{
    uint8_t *rec = (uint8_t *)((uintptr_t)dword_53A45
                               + REC_STRIDE * (uint32_t)index);
    int i;

    for (i = 0; i < 8; i++) {
        uint8_t *slot = rec + 2u * (uint32_t)i + 10u;
        if ((int8_t)slot[0] < 0) {
            slot[0] = 0;
            slot[1] = (uint8_t)value;
            return 1;
        }
    }
    return -1;
}

/* 0x1B8E7 - delete slot `slot` by shifting the following slots one pair to the
 * left and marking the last slot empty (0x80). Returns the memmove destination
 * (the machine code returns the CRT memmove result in EAX). The length is
 * `2*(7-slot)` in 32-bit unsigned arithmetic - for the natural inputs 0..7 this
 * is 0..14; beyond that the original underflows exactly as the C does. */
void *rec_slot_remove(int index, int slot)
{
    uint8_t *rec = (uint8_t *)((uintptr_t)dword_53A45
                               + REC_STRIDE * (uint32_t)index);
    uint32_t s = (uint32_t)slot;
    void   *dest = rec + 2u * s + 10u;

    memmove(dest, rec + 2u * s + 12u, 2u * (7u - s));
    rec[24] = 0x80;
    return dest;
}

/* 0x1F183 - "skip this record" predicate used by the cell-sprite refresh:
 *   0 unless record[7] != 0x1C and (record[0x20] == 0x13 or record[0x1F] is
 *   4 or 5). Field semantics are UNCONFIRMED; the machine code is the spec. */
int rec_skip(int index)
{
    const uint8_t *p = (const uint8_t *)(uintptr_t)dword_53A45 + 80 * index;

    if (p[7] == 0x1C)
        return 0;
    if (p[0x20] == 0x13)
        return 1;
    if (p[0x1F] == 4 || p[0x1F] == 5)
        return 1;
    return 0;
}
