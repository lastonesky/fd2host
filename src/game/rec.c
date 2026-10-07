/* rec.c - FD2 character record table (source translation).
 *
 *   0x34894  rec_flag  - one-line accessor: (base + 80*i + 5) & 1
 *   0x12C60  rec_find  - search both record tables for byte +8 == want
 *
 * App-level: the C reads and writes the same data-segment globals the machine
 * code does, so src/repl.c can hook both entries with no glue.
 *
 * The original accessor computes the record address as `i*5` then `<<4`
 * (i.e. an unsigned 80*i), and the search compares an unsigned byte against
 * the whole argument - both details matter for inputs outside the table's
 * natural range and are reproduced literally.
 *
 * Verified against the machine code by src/reccheck.c.
 */
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
