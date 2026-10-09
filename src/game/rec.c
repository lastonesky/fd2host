/* rec.c - FD2 character record table (source translation).
 *
 *   0x34894  rec_flag       - one-line accessor: (base + 80*i + 5) & 1
 *   0x12C60  rec_find       - search both record tables for byte +8 == want
 *   0x1F183  rec_skip       - cell-sprite refresh predicate
 *   0x1B722  rec_field_byte - read the value byte of slot `slot` (+11 + 2*slot)
 *   0x344F2  rec_status_set - range-set the low nibble of record byte +52
 *   0x1BB8C  rec_slot_claim - claim the first empty of the eight 2-byte slots
 *   0x1B8E7  rec_slot_remove- left-shift delete one slot, mark the last 0x80
 *   0x1B8A6  rec_slot_free  - how many of the eight slots are still free
 *   0x1B83D  rec_slot_find  - first slot with state bit 6 set, by value sign
 *   0x1CA89  rec_sub_table5 - word +68 -= byte 5 of the 0x619FD table entry
 *   0x13512  rec_flag_or80  - byte +5 |= 0x80
 *   0x32975  rec_flag_set1  - byte +5 = 1
 *   0x34D64  rec_status_mask- records 10..27: byte +52 &= 0x80
 *   0x35009  rec_status_14  - record 14: byte +52 = 0x83
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
#include "tables.h"

/* IDA names kept so the C reads like the original decompilation.
 * Addresses verified against E:\FD2\FD2.EXE.i64. */
#define dword_53A45 (*(uint32_t *)(uintptr_t)0x00053A45u) /* record table 1 */
#define dword_53BEB (*(int32_t *)(uintptr_t)0x00053BEBu)  /* table 1 records */
#define dword_53BF7 (*(uint32_t *)(uintptr_t)0x00053BF7u) /* record table 2 */
#define dword_53BFB (*(int32_t *)(uintptr_t)0x00053BFBu)  /* table 2 records */
#define dword_53C1B (*(uint32_t *)(uintptr_t)0x00053C1Bu) /* matched record */
#define TBL_619FD   0x000619FDu                            /* 7-byte entry table */

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
/* 0x1B8A6 - how many of the eight slots are free. A slot's state byte is
 * 0x80 when empty (rec_slot_claim clears the bit when it claims one), so this
 * counts the state bytes whose bit 7 is clear. */
int rec_slot_free(int index)
{
    const uint8_t *rec = (const uint8_t *)((uintptr_t)dword_53A45
                                           + REC_STRIDE * (uint32_t)index);
    int n = 0;
    int i;

    for (i = 0; i < 8; i++)
        if (!(rec[2u * (uint32_t)i + 10u] & 0x80))
            n++;
    return n;
}

/* 0x1B83D - first slot whose state byte has bit 6 set and whose value byte is
 * below 0x80 when `want_high` is 0, or at least 0x80 when it is not. -1 when
 * no slot matches. Both comparisons are unsigned byte compares. */
int rec_slot_find(int index, int want_high)
{
    const uint8_t *rec = (const uint8_t *)((uintptr_t)dword_53A45
                                           + REC_STRIDE * (uint32_t)index);
    int i;

    for (i = 0; i < 8; i++) {
        const uint8_t *slot = rec + 2u * (uint32_t)i + 10u;
        if (!(slot[0] & 0x40))
            continue;
        if (want_high) {
            if (slot[1] >= 0x80)
                return i;
        } else {
            if (slot[1] < 0x80)
                return i;
        }
    }
    return -1;
}

/* 0x1CA89 - subtract byte 5 of entry `tidx` of the 7-byte table at 0x619FD
 * from the word the record keeps at +68, in 16-bit unsigned arithmetic. The
 * machine code returns the record address in EAX, so the C returns it too. */
uint32_t rec_sub_table5(int index, int tidx)
{
    uint8_t *rec = (uint8_t *)((uintptr_t)dword_53A45
                               + REC_STRIDE * (uint32_t)index);
    const uint8_t *e = (const uint8_t *)tbl_ptr((void *)(uintptr_t)TBL_619FD,
                                                 7, tidx, 0);
    uint16_t w = (uint16_t)(rec[68] | (rec[69] << 8));

    w = (uint16_t)(w - e[5]);
    rec[68] = (uint8_t)w;
    rec[69] = (uint8_t)(w >> 8);
    return (uint32_t)(uintptr_t)rec;
}

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

/* 0x13512 / 0x32975 - the two single-record flag writers. Both compute the
 * record offset as `i*5 << 4` in a 32-bit register and return it in EAX, so
 * the C returns the offset (80*index), not the record address - that is what
 * the callers see. */
int rec_flag_or80(int index)
{
    uint8_t *rec = (uint8_t *)((uintptr_t)dword_53A45
                               + REC_STRIDE * (uint32_t)index);

    rec[5] |= 0x80;
    return (int)(REC_STRIDE * (uint32_t)index);
}

int rec_flag_set1(int index)
{
    uint8_t *rec = (uint8_t *)((uintptr_t)dword_53A45
                               + REC_STRIDE * (uint32_t)index);

    rec[5] = 1;
    return (int)(REC_STRIDE * (uint32_t)index);
}

/* 0x34D64 - keep only bit 7 of byte +52 for records 10..27 (eighteen records,
 * a fixed window, not bounded by dword_53BEB). Takes no argument even though
 * the dispatch in sub_117E7 pushes one; returns the table base. */
uint32_t rec_status_mask_records(void)
{
    uint32_t base = dword_53A45;
    int i;

    for (i = 0; i < 18; i++) {
        uint8_t *rec = (uint8_t *)((uintptr_t)(base + REC_STRIDE * (uint32_t)(i + 10)));
        rec[52] &= 0x80;
    }
    return base;
}

/* 0x35009 - the single-record twin: record 14 gets byte +52 = 0x83, and the
 * record address is returned. */
uint32_t rec_status_set_record14(void)
{
    uint8_t *rec = (uint8_t *)((uintptr_t)dword_53A45 + REC_STRIDE * 14u);

    rec[52] = 0x83;
    return (uint32_t)(uintptr_t)rec;
}

/* 0x1C269 - collect the set-bit indices of record[index] bytes +26..+30.
 * Bit b of byte i maps to index 8*i+b; `out` may be NULL for a dry run. */
int rec_collect_slot_bits(int index, uint8_t *out)
{
    const uint8_t *rec = (const uint8_t *)(uintptr_t)dword_53A45
                         + REC_STRIDE * (uint32_t)index;
    int n = 0, i, j;

    for (i = 0; i < 5; i++) {
        uint8_t bits = rec[26 + i];
        for (j = 0; j < 8; j++) {
            if (((bits >> j) & 1u) != 0) {
                if (out)
                    out[n] = (uint8_t)(j + 8 * i);
                n++;
            }
        }
    }
    return n;
}
