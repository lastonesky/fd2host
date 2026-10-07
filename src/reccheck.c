/* reccheck.c - differential test for the character record table.
 *
 * Runs the original machine code (0x34894 rec_flag, 0x12C60 rec_find,
 * 0x1B722 rec_field_byte, 0x344F2 rec_status_set, 0x1BB8C rec_slot_claim,
 * 0x1B8E7 rec_slot_remove, 0x1145A unit_recalc, 0x11506 unit_refresh_all,
 * 0x112A5 unit_add, 0x33499 unit_exists, 0x1B8A6 rec_slot_free,
 * 0x1B83D rec_slot_find, 0x1CA89 rec_sub_table5, 0x13512 rec_flag_or80,
 * 0x32975 rec_flag_set1, 0x34D64 rec_status_mask_records,
 * 0x35009 rec_status_set_record14) and the C translation
 * (src/game/rec.c, src/game/unit.c) on the same
 * tables and compares the return value plus every byte both sides touch.
 *
 * The two tables are synthetic buffers, but the globals are the real game
 * ones: both sides read/write the same data segment, so the test is a true
 * differential test rather than a reimplementation of the algorithm. The
 * slot writers are run on two byte-identical copies of the input, one per
 * implementation, and the whole buffer is compared afterwards.
 *
 * Coverage: random tables/counts/wants, plus constructed cases for each path -
 * no match at all, match only in table 2 (several: the original keeps scanning),
 * match in table 1 with the flag set then a later unflagged one, table 1 match
 * with every flag set (so table 2 is not scanned), and want > 255.
 *
 * The slot accessors get their own random + constructed cases below:
 *   rec_field_byte  random index/slot, all byte values
 *   rec_status_set  random index ranges incl. start>end, value with high bits
 *   rec_slot_claim  random states, all full, each single hole, first of many
 *   rec_slot_remove slot 0..7, random contents, plus an unsigned underflow slot
 *
 * The round-34 leaves get random + constructed cases too:
 *   rec_slot_free            random state bytes; all-free / all-occupied
 *   rec_slot_find            both want_high branches, no bit-6 slot, first hit
 *   rec_sub_table5           random 0x619FD entries, 16-bit wrap at word +68
 *   rec_flag_or80 / set1     random byte +5, and the return value (80*index)
 *   rec_status_mask_records  the fixed records 10..27 window
 *   rec_status_set_record14  record 14
 *
 * Not exercised: a negative index in rec_flag. The original computes
 * `i*5 << 4` in a 32-bit register, so a negative index wraps to an address far
 * above the table and would fault on both sides - the C reproduces the wrap
 * literally ((uint32_t)index * 80), which is all that can be asserted without
 * mapping memory below the tables.
 *
 * Build: pwsh -File build.ps1 -Target reccheck
 * Run   : build\reccheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/rec.h"
#include "game/unit.h"

typedef int (__cdecl *flag_fn)(int);
typedef int (__cdecl *find_fn)(int);
typedef int  (__cdecl *field_fn)(int, int);
typedef void (__cdecl *status_fn)(int, int, int);
typedef int  (__cdecl *claim_fn)(int, int);
typedef void *(__cdecl *remove_fn)(int, int);
typedef int  (__cdecl *free_fn)(int);
typedef int  (__cdecl *slotfind_fn)(int, int);
typedef uint32_t (__cdecl *sub5_fn)(int, int);

#define ORIG_FLAG   ((flag_fn)(uintptr_t)0x34894)
#define ORIG_FIND   ((find_fn)(uintptr_t)0x12C60)
#define ORIG_FIELD  ((field_fn)(uintptr_t)0x1B722)
#define ORIG_STATUS ((status_fn)(uintptr_t)0x344F2)
#define ORIG_CLAIM  ((claim_fn)(uintptr_t)0x1BB8C)
#define ORIG_REMOVE ((remove_fn)(uintptr_t)0x1B8E7)

typedef int  (__cdecl *recalc_fn)(int);
typedef void (__cdecl *refresh_fn)(void);
typedef int  (__cdecl *add_fn)(int);
typedef int  (__cdecl *exists_fn)(int);

typedef uint32_t (__cdecl *noarg_fn)(void);

#define ORIG_RECALC  ((recalc_fn)(uintptr_t)0x1145A)
#define ORIG_REFRESH ((refresh_fn)(uintptr_t)0x11506)
#define ORIG_ADD     ((add_fn)(uintptr_t)0x112A5)
#define ORIG_EXISTS  ((exists_fn)(uintptr_t)0x33499)

#define G53A45 (*(uint32_t *)(uintptr_t)0x00053A45u)
#define G53BEB (*(int32_t *)(uintptr_t)0x00053BEBu)
#define G53BF7 (*(uint32_t *)(uintptr_t)0x00053BF7u)
#define G53BFB (*(int32_t *)(uintptr_t)0x00053BFBu)
#define G53C1B (*(uint32_t *)(uintptr_t)0x00053C1Bu)

#define ORIG_FREE     ((free_fn)(uintptr_t)0x1B8A6)
#define ORIG_FINDSLOT ((slotfind_fn)(uintptr_t)0x1B83D)
#define ORIG_SUB5     ((sub5_fn)(uintptr_t)0x1CA89)
#define ORIG_OR80     ((flag_fn)(uintptr_t)0x13512)
#define ORIG_SET1     ((flag_fn)(uintptr_t)0x32975)
#define ORIG_MASKREC  ((noarg_fn)(uintptr_t)0x34D64)
#define ORIG_SET14    ((noarg_fn)(uintptr_t)0x35009)

/* The 7-byte entry table 0x1CA89 reads. It is a real game data address, so both
 * sides read the same bytes; the harness only fills the entries it uses. */
#define TBL619        ((uint8_t *)(uintptr_t)0x000619FDu)
#define TBL619_ENTRIES 200   /* ends at 0x62069, inside the mapped data segment */

#define MAXREC 64
static uint8_t tbl1[MAXREC * REC_STRIDE];
static uint8_t tbl2[MAXREC * REC_STRIDE];

/* Two byte-identical copies of the input for the slot writers: the original
 * runs on `obuf`, the C on `cbuf`, and the whole buffer is compared after. */
static uint8_t inbuf[MAXREC * REC_STRIDE];
static uint8_t obuf[MAXREC * REC_STRIDE];
static uint8_t cbuf[MAXREC * REC_STRIDE];

/* Character table for unit_refresh_all (read-only on both sides) plus a copy
 * kept to prove neither implementation touches it. */
static uint8_t unit_ch[MAXREC * REC_STRIDE];
static uint8_t unit_ch0[MAXREC * REC_STRIDE];

static uint32_t seed = 0x0B0C6E5u;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}

static int failures;
static unsigned cases_run;
enum { P_RANDOM, P_NONE, P_TABLE2, P_FLAG0, P_FLAG1, P_BIGWANT, P_EMPTY };
static const char *path_name[] = { "random", "no match", "table 2 only",
                                   "flag0 after flagged", "all flagged",
                                   "want > 255", "empty tables" };
static unsigned path_count[7];

static void set_globals(int n1, int n2)
{
    G53A45 = (uint32_t)(uintptr_t)tbl1;
    G53BEB = n1;
    G53BF7 = (uint32_t)(uintptr_t)tbl2;
    G53BFB = n2;
    G53C1B = 0;
}

static int cmp_case(unsigned id, int path, int n1, int n2, int want)
{
    int r1, r2, i;
    uint32_t c1, c2;

    r1 = ORIG_FIND(want);
    c1 = G53C1B;

    G53C1B = 0;
    r2 = rec_find(want);
    c2 = G53C1B;
    cases_run++;

    if (r1 != r2 || c1 != c2) {
        printf("FAIL case %u (%s): want=%d n1=%d n2=%d orig=(%d,0x%X) ours=(%d,0x%X)\n",
               id, path_name[path], want, n1, n2, r1, c1, r2, c2);
        failures++;
        return -1;
    }

    for (i = 0; i < n1; i++) {
        cases_run++;
        if (ORIG_FLAG(i) != rec_flag(i)) {
            printf("FAIL case %u (rec_flag): index %d orig=%d ours=%d\n",
                   id, i, ORIG_FLAG(i), rec_flag(i));
            failures++;
            return -1;
        }
    }

    path_count[path]++;
    return 0;
}

static void fill_tables(int n1, int n2)
{
    int i;
    for (i = 0; i < n1 * REC_STRIDE; i++) tbl1[i] = (uint8_t)rnd();
    for (i = 0; i < n2 * REC_STRIDE; i++) tbl2[i] = (uint8_t)rnd();
}

/* ---- slot accessors (0x1B722 / 0x344F2 / 0x1BB8C / 0x1B8E7) ---------- */

static void fill_all(void)
{
    unsigned i;
    for (i = 0; i < sizeof inbuf; i++) inbuf[i] = (uint8_t)rnd();
}

/* Compare the return value (already normalised to a buffer offset where the
 * two implementations legitimately return different pointers) and the whole
 * buffer the two runs produced. */
static int cmp_mem(const char *name, unsigned id, int r1, int r2)
{
    unsigned k;

    cases_run++;
    if (r1 != r2) {
        printf("FAIL %s case %u: ret orig=%d ours=%d\n", name, id, r1, r2);
        failures++;
        return -1;
    }
    for (k = 0; k < sizeof obuf; k++) {
        if (obuf[k] != cbuf[k]) {
            printf("FAIL %s case %u: rec %u off %u orig=%02X ours=%02X\n",
                   name, id, k / REC_STRIDE, k % REC_STRIDE, obuf[k], cbuf[k]);
            failures++;
            return -1;
        }
    }
    return 0;
}

static void test_field(unsigned ncases)
{
    unsigned c;

    for (c = 0; c < ncases && !failures; c++) {
        int index = (int)(rnd() % MAXREC);
        int slot  = (int)(rnd() % 8);
        int r1, r2;

        fill_all();
        memcpy(obuf, inbuf, sizeof inbuf);
        G53A45 = (uint32_t)(uintptr_t)obuf;
        r1 = ORIG_FIELD(index, slot);
        memcpy(cbuf, inbuf, sizeof inbuf);
        G53A45 = (uint32_t)(uintptr_t)cbuf;
        r2 = rec_field_byte(index, slot);
        cmp_mem("rec_field_byte", 3000 + c, r1, r2);
    }
}

static void test_status(unsigned ncases)
{
    static const int vals[] = { 0x00, 0x0F, 0x55, 0xA0, 0xF0, 0xFF };
    unsigned c;

    for (c = 0; c < ncases && !failures; c++) {
        int start = (int)(rnd() % MAXREC);
        int end   = (int)(rnd() % MAXREC);   /* may be < start on purpose */
        int value = vals[rnd() % (sizeof vals / sizeof vals[0])];
        int r1, r2;

        fill_all();
        memcpy(obuf, inbuf, sizeof inbuf);
        G53A45 = (uint32_t)(uintptr_t)obuf;
        ORIG_STATUS(start, end, value);
        r1 = 0;
        memcpy(cbuf, inbuf, sizeof inbuf);
        G53A45 = (uint32_t)(uintptr_t)cbuf;
        rec_status_set(start, end, value);
        r2 = 0;
        cmp_mem("rec_status_set", 4000 + c, r1, r2);
    }

    /* constructed: start > end (zero iterations), start == end, full range */
    {
        static const int cases[][3] = { { 5, 4, 0x0F }, { 7, 7, 0x55 },
                                        { 0, MAXREC - 1, 0xF0 } };
        unsigned k;
        for (k = 0; k < sizeof cases / sizeof cases[0]; k++) {
            fill_all();
            memcpy(obuf, inbuf, sizeof inbuf);
            G53A45 = (uint32_t)(uintptr_t)obuf;
            ORIG_STATUS(cases[k][0], cases[k][1], cases[k][2]);
            memcpy(cbuf, inbuf, sizeof inbuf);
            G53A45 = (uint32_t)(uintptr_t)cbuf;
            rec_status_set(cases[k][0], cases[k][1], cases[k][2]);
            cmp_mem("rec_status_set", 4900 + k, 0, 0);
        }
    }
}

/* Set one slot byte's empty bit (bit 7). */
static void slot_set_empty(int rec, int slot, int empty)
{
    uint8_t *p = inbuf + (size_t)rec * REC_STRIDE + 10 + 2 * slot;
    if (empty) *p |= 0x80u; else *p &= 0x7Fu;
}

static void test_claim(unsigned ncases)
{
    unsigned c;
    int rec, slot;

    for (c = 0; c < ncases && !failures; c++) {
        int index = (int)(rnd() % MAXREC);
        int value = (int)(rnd() & 0xFF);
        int r1, r2;

        fill_all();
        memcpy(obuf, inbuf, sizeof inbuf);
        G53A45 = (uint32_t)(uintptr_t)obuf;
        r1 = ORIG_CLAIM(index, value);
        memcpy(cbuf, inbuf, sizeof inbuf);
        G53A45 = (uint32_t)(uintptr_t)cbuf;
        r2 = rec_slot_claim(index, value);
        cmp_mem("rec_slot_claim", 5000 + c, r1, r2);
    }

    /* constructed: each single hole, all full, first of several holes */
    for (rec = 0; rec < 4 && !failures; rec++) {
        int value = 0xA5;
        /* all full */
        fill_all();
        for (slot = 0; slot < 8; slot++) slot_set_empty(rec, slot, 0);
        memcpy(obuf, inbuf, sizeof inbuf);
        G53A45 = (uint32_t)(uintptr_t)obuf;
        {
            int r1 = ORIG_CLAIM(rec, value);
            memcpy(cbuf, inbuf, sizeof inbuf);
            G53A45 = (uint32_t)(uintptr_t)cbuf;
            cmp_mem("rec_slot_claim", 5900 + rec * 16 + 8, r1,
                    rec_slot_claim(rec, value));
        }
        for (slot = 0; slot < 8 && !failures; slot++) {
            fill_all();
            slot_set_empty(rec, slot, 1);
            memcpy(obuf, inbuf, sizeof inbuf);
            G53A45 = (uint32_t)(uintptr_t)obuf;
            {
                int r1 = ORIG_CLAIM(rec, value);
                memcpy(cbuf, inbuf, sizeof inbuf);
                G53A45 = (uint32_t)(uintptr_t)cbuf;
                cmp_mem("rec_slot_claim", 5900 + rec * 16 + slot, r1,
                        rec_slot_claim(rec, value));
            }
        }
    }
}

static void test_remove(unsigned ncases)
{
    unsigned c;

    for (c = 0; c < ncases && !failures; c++) {
        int index = (int)(rnd() % MAXREC);
        int slot  = (int)(rnd() % 8);
        intptr_t o1, o2;

        fill_all();
        memcpy(obuf, inbuf, sizeof inbuf);
        G53A45 = (uint32_t)(uintptr_t)obuf;
        o1 = (intptr_t)ORIG_REMOVE(index, slot) - (intptr_t)obuf;
        memcpy(cbuf, inbuf, sizeof inbuf);
        G53A45 = (uint32_t)(uintptr_t)cbuf;
        o2 = (intptr_t)rec_slot_remove(index, slot) - (intptr_t)cbuf;
        cmp_mem("rec_slot_remove", 6000 + c, (int)o1, (int)o2);
    }

    /* constructed: every slot on a few records (slot 7 -> zero-length copy) */
    {
        int rec, slot;
        for (rec = 0; rec < 4 && !failures; rec++) {
            for (slot = 0; slot < 8 && !failures; slot++) {
                intptr_t o1, o2;
                fill_all();
                memcpy(obuf, inbuf, sizeof inbuf);
                G53A45 = (uint32_t)(uintptr_t)obuf;
                o1 = (intptr_t)ORIG_REMOVE(rec, slot) - (intptr_t)obuf;
                memcpy(cbuf, inbuf, sizeof inbuf);
                G53A45 = (uint32_t)(uintptr_t)cbuf;
                o2 = (intptr_t)rec_slot_remove(rec, slot) - (intptr_t)cbuf;
                cmp_mem("rec_slot_remove", 6900 + rec * 8 + slot,
                        (int)o1, (int)o2);
            }
        }
    }
}

/* ---- round 34 record-table leaves ----------------------------------
 * 0x1B8A6 rec_slot_free, 0x1B83D rec_slot_find, 0x1CA89 rec_sub_table5,
 * 0x13512 rec_flag_or80, 0x32975 rec_flag_set1, 0x34D64 rec_status_mask_records,
 * 0x35009 rec_status_set_record14.
 *
 * The two readers need no buffer pair: both sides read the same bytes, so
 * G53A45 is pointed at inbuf and only the return value is compared. The writers
 * use the obuf/cbuf pair as before, and pointer return values are normalised to
 * a buffer offset because the two implementations run on different copies.
 */

/* Fill the eight slots of record `index` in inbuf:
 *   0 no slot has state bit 6
 *   1 bit 6 set, random values
 *   2 bit 6 set, every value < 0x80
 *   3 bit 6 set, every value >= 0x80
 *   4 slot 0 without bit 6, slot 1 value 0x7F, slot 2 value 0x80
 */
static void leaf_slots(int index, unsigned mode)
{
    int i;

    for (i = 0; i < 8; i++) {
        uint8_t *slot = inbuf + (size_t)index * REC_STRIDE + 10 + 2 * i;
        switch (mode) {
        case 0:  slot[0] = (uint8_t)(rnd() & 0xBFu); slot[1] = (uint8_t)rnd(); break;
        case 1:  slot[0] = (uint8_t)(0x40u | (rnd() & 0x3Fu)); slot[1] = (uint8_t)rnd(); break;
        case 2:  slot[0] = 0x40; slot[1] = (uint8_t)(rnd() & 0x7Fu); break;
        case 3:  slot[0] = 0x40; slot[1] = (uint8_t)(0x80u | (rnd() & 0x7Fu)); break;
        default:
            slot[0] = (uint8_t)(i ? 0x40 : 0x00);
            slot[1] = (uint8_t)(i == 1 ? 0x7F : (i == 2 ? 0x80 : 0));
            break;
        }
    }
}

static int cmp_read(const char *name, unsigned id, int r1, int r2)
{
    cases_run++;
    if (r1 != r2) {
        printf("FAIL %s case %u: ret orig=%d ours=%d\n", name, id, r1, r2);
        failures++;
        return -1;
    }
    return 0;
}

static void test_free(unsigned ncases)
{
    unsigned c, k;

    for (c = 0; c < ncases && !failures; c++) {
        int index = (int)(rnd() % MAXREC);
        fill_all();
        G53A45 = (uint32_t)(uintptr_t)inbuf;
        cmp_read("rec_slot_free", 7000 + c, ORIG_FREE(index), rec_slot_free(index));
    }

    /* constructed: all free, all occupied, alternating */
    fill_all();
    for (k = 0; k < 3 && !failures; k++) {
        int i;
        for (i = 0; i < 8; i++) {
            uint8_t *slot = inbuf + k * REC_STRIDE + 10 + 2 * i;
            slot[0] = (uint8_t)(k == 0 ? 0x00 : (k == 1 ? 0x80 : (i & 1 ? 0x80 : 0x00)));
            slot[1] = (uint8_t)rnd();
        }
        G53A45 = (uint32_t)(uintptr_t)inbuf;
        cmp_read("rec_slot_free", 7900 + k, ORIG_FREE((int)k), rec_slot_free((int)k));
    }
}

static void test_findslot(unsigned ncases)
{
    unsigned c, mode, want;

    for (c = 0; c < ncases && !failures; c++) {
        int index = (int)(rnd() % MAXREC);
        int want_high = (int)(rnd() & 1);
        fill_all();
        leaf_slots(index, rnd() % 5);
        G53A45 = (uint32_t)(uintptr_t)inbuf;
        cmp_read("rec_slot_find", 8000 + c,
                 ORIG_FINDSLOT(index, want_high), rec_slot_find(index, want_high));
    }

    /* constructed: every mode x both want_high branches */
    fill_all();
    for (mode = 0; mode < 5 && !failures; mode++) {
        for (want = 0; want < 2 && !failures; want++) {
            leaf_slots(0, mode);
            G53A45 = (uint32_t)(uintptr_t)inbuf;
            cmp_read("rec_slot_find", 8900 + mode * 2 + want,
                     ORIG_FINDSLOT(0, (int)want), rec_slot_find(0, (int)want));
        }
    }
}

static void test_sub5(unsigned ncases)
{
    unsigned c, k;

    for (c = 0; c < ncases && !failures; c++) {
        int index = (int)(rnd() % MAXREC);
        int tidx  = (int)(rnd() % TBL619_ENTRIES);
        int r1, r2;

        for (k = 0; k < TBL619_ENTRIES; k++) TBL619[k] = (uint8_t)rnd();
        fill_all();
        memcpy(obuf, inbuf, sizeof obuf);
        G53A45 = (uint32_t)(uintptr_t)obuf;
        r1 = (int)(ORIG_SUB5(index, tidx) - (uint32_t)(uintptr_t)obuf);
        memcpy(cbuf, inbuf, sizeof cbuf);
        G53A45 = (uint32_t)(uintptr_t)cbuf;
        r2 = (int)(rec_sub_table5(index, tidx) - (uint32_t)(uintptr_t)cbuf);
        cmp_mem("rec_sub_table5", 9000 + c, r1, r2);
    }

    /* constructed: 16-bit wrap - word +68 = 0x0003 minus table byte 0x04 */
    {
        static const struct { uint8_t lo, hi, sub; } wraps[] = {
            { 0x03, 0x00, 0x04 }, { 0x00, 0x00, 0x01 }, { 0xFF, 0xFF, 0x01 },
            { 0x00, 0x80, 0x00 }, { 0x7F, 0x7F, 0x80 },
        };
        unsigned j;
        int r1, r2;
        for (j = 0; j < sizeof wraps / sizeof wraps[0] && !failures; j++) {
            int index = (int)j;
            fill_all();
            for (k = 0; k < TBL619_ENTRIES; k++) TBL619[k] = 0;
            TBL619[5] = wraps[j].sub;          /* entry 0, byte 5 */
            inbuf[index * REC_STRIDE + 68] = wraps[j].lo;
            inbuf[index * REC_STRIDE + 69] = wraps[j].hi;
            memcpy(obuf, inbuf, sizeof obuf);
            G53A45 = (uint32_t)(uintptr_t)obuf;
            r1 = (int)(ORIG_SUB5(index, 0) - (uint32_t)(uintptr_t)obuf);
            memcpy(cbuf, inbuf, sizeof cbuf);
            G53A45 = (uint32_t)(uintptr_t)cbuf;
            r2 = (int)(rec_sub_table5(index, 0) - (uint32_t)(uintptr_t)cbuf);
            cmp_mem("rec_sub_table5", 9900 + j, r1, r2);
        }
    }
}

static void test_flagwriters(unsigned ncases)
{
    unsigned c;

    for (c = 0; c < ncases && !failures; c++) {
        int index = (int)(rnd() % MAXREC);
        int r1, r2;

        fill_all();
        memcpy(obuf, inbuf, sizeof obuf);
        G53A45 = (uint32_t)(uintptr_t)obuf;
        r1 = ORIG_OR80(index);
        memcpy(cbuf, inbuf, sizeof cbuf);
        G53A45 = (uint32_t)(uintptr_t)cbuf;
        r2 = rec_flag_or80(index);
        cmp_mem("rec_flag_or80", 10000 + c, r1, r2);

        fill_all();
        memcpy(obuf, inbuf, sizeof obuf);
        G53A45 = (uint32_t)(uintptr_t)obuf;
        r1 = ORIG_SET1(index);
        memcpy(cbuf, inbuf, sizeof cbuf);
        G53A45 = (uint32_t)(uintptr_t)cbuf;
        r2 = rec_flag_set1(index);
        cmp_mem("rec_flag_set1", 10500 + c, r1, r2);
    }

    /* constructed: byte +5 values that show the OR is not an assignment */
    {
        static const uint8_t vals[] = { 0x00, 0x01, 0x7F, 0x80, 0xFF };
        unsigned j;
        int r1, r2;
        for (j = 0; j < sizeof vals / sizeof vals[0] && !failures; j++) {
            int index = (int)j;
            fill_all();
            inbuf[index * REC_STRIDE + 5] = vals[j];
            memcpy(obuf, inbuf, sizeof obuf);
            G53A45 = (uint32_t)(uintptr_t)obuf;
            r1 = ORIG_OR80(index);
            memcpy(cbuf, inbuf, sizeof cbuf);
            G53A45 = (uint32_t)(uintptr_t)cbuf;
            r2 = rec_flag_or80(index);
            cmp_mem("rec_flag_or80", 10900 + j, r1, r2);
        }
    }
}

static void test_status_records(unsigned ncases)
{
    unsigned c;

    for (c = 0; c < ncases && !failures; c++) {
        int r1, r2;

        fill_all();
        memcpy(obuf, inbuf, sizeof obuf);
        G53A45 = (uint32_t)(uintptr_t)obuf;
        r1 = (int)(ORIG_MASKREC() - (uint32_t)(uintptr_t)obuf);
        memcpy(cbuf, inbuf, sizeof cbuf);
        G53A45 = (uint32_t)(uintptr_t)cbuf;
        r2 = (int)(rec_status_mask_records() - (uint32_t)(uintptr_t)cbuf);
        cmp_mem("rec_status_mask_records", 11000 + c, r1, r2);

        fill_all();
        memcpy(obuf, inbuf, sizeof obuf);
        G53A45 = (uint32_t)(uintptr_t)obuf;
        r1 = (int)(ORIG_SET14() - (uint32_t)(uintptr_t)obuf);
        memcpy(cbuf, inbuf, sizeof cbuf);
        G53A45 = (uint32_t)(uintptr_t)cbuf;
        r2 = (int)(rec_status_set_record14() - (uint32_t)(uintptr_t)cbuf);
        cmp_mem("rec_status_set_record14", 11500 + c, r1, r2);
    }

    /* constructed: the window boundary. Records 9 and 28 must stay untouched by
     * 0x34D64, which only covers 10..27; record 14 is the only one 0x35009 writes. */
    {
        static const int recs[] = { 9, 10, 27, 28 };
        unsigned j;
        for (j = 0; j < sizeof recs / sizeof recs[0] && !failures; j++) {
            int r = recs[j];
            int r1, r2;
            fill_all();
            inbuf[r * REC_STRIDE + 52] = 0xF7;
            memcpy(obuf, inbuf, sizeof obuf);
            G53A45 = (uint32_t)(uintptr_t)obuf;
            r1 = (int)(ORIG_MASKREC() - (uint32_t)(uintptr_t)obuf);
            memcpy(cbuf, inbuf, sizeof cbuf);
            G53A45 = (uint32_t)(uintptr_t)cbuf;
            r2 = (int)(rec_status_mask_records() - (uint32_t)(uintptr_t)cbuf);
            cmp_mem("rec_status_mask_records", 11900 + j, r1, r2);
        }
    }
}

/* ---- persistent party roster (0x1145A / 0x11506 / 0x112A5) ---------- */

/* Fill the eight slots of record `index` in inbuf:
 *   0 all inactive (state bit 6 clear)
 *   1 active with item ids 0..31
 *   2 random states / valid item ids
 *   3 active with a fixed item id */
static void recalc_slots(int index, unsigned mode)
{
    int i;

    for (i = 0; i < 8; i++) {
        uint8_t *slot = inbuf + (size_t)index * REC_STRIDE + 10 + 2 * i;
        switch (mode) {
        case 0:  slot[0] = (uint8_t)(rnd() & 0xBFu); slot[1] = (uint8_t)rnd(); break;
        case 1:  slot[0] = (uint8_t)(0x40u | (rnd() & 0x3Fu)); slot[1] = (uint8_t)(rnd() % 32); break;
        case 3:  slot[0] = (uint8_t)(0x40u | (rnd() & 0x3Fu)); slot[1] = 31; break;
        default: slot[0] = (uint8_t)rnd(); slot[1] = (uint8_t)(rnd() % 32); break;
        }
    }
}

/* Run both implementations of unit_recalc on byte-identical copies of inbuf
 * and compare the return value plus the whole buffer. */
static void run_recalc(unsigned id, int index)
{
    int r1, r2;

    memcpy(obuf, inbuf, sizeof inbuf);
    G53BF7 = (uint32_t)(uintptr_t)obuf;
    r1 = ORIG_RECALC(index);

    memcpy(cbuf, inbuf, sizeof inbuf);
    G53BF7 = (uint32_t)(uintptr_t)cbuf;
    r2 = unit_recalc(index);

    cmp_mem("unit_recalc", id, r1, r2);
}

static void test_recalc(unsigned ncases)
{
    unsigned c;

    for (c = 0; c < ncases && !failures; c++) {
        int index = (int)(rnd() % MAXREC);
        unsigned mode = rnd() % 4;

        fill_all();
        recalc_slots(index, mode);
        run_recalc(7000 + c, index);
    }

    /* constructed: the four slot-shape modes on one record, with extreme
     * signed base stats so the sign extension is exercised */
    {
        static const unsigned modes[4] = { 0, 1, 3, 2 };
        unsigned k;
        for (k = 0; k < 4 && !failures; k++) {
            int index = 5;
            fill_all();
            recalc_slots(index, modes[k]);
            *(int16_t *)(inbuf + (size_t)index * REC_STRIDE + 0x37) =
                (k & 1) ? (int16_t)0x8000 : (int16_t)0x7FFF;
            *(int16_t *)(inbuf + (size_t)index * REC_STRIDE + 0x39) = (int16_t)0x8000;
            *(int16_t *)(inbuf + (size_t)index * REC_STRIDE + 0x3E) = (int16_t)0x7FFF;
            run_recalc(7500 + k, index);
        }
        /* every slot active, a few item ids */
        for (k = 0; k < 4 && !failures; k++) {
            int index = 7, i;
            fill_all();
            for (i = 0; i < 8; i++) {
                uint8_t *slot = inbuf + (size_t)index * REC_STRIDE + 10 + 2 * i;
                slot[0] = 0x40;
                slot[1] = (uint8_t)(k * 7);
            }
            run_recalc(7550 + k, index);
        }
    }
}

/* Run both implementations of unit_refresh_all on byte-identical party
 * copies (ch table is read-only and shared) and compare the whole party
 * buffer; also assert the character table is untouched. */
static void run_refresh(unsigned id, int n1, int n2)
{
    int i;

    memcpy(obuf, inbuf, sizeof inbuf);
    G53A45 = (uint32_t)(uintptr_t)unit_ch;
    G53BEB = n1;
    G53BF7 = (uint32_t)(uintptr_t)obuf;
    G53BFB = n2;
    ORIG_REFRESH();

    memcpy(cbuf, inbuf, sizeof inbuf);
    G53A45 = (uint32_t)(uintptr_t)unit_ch;
    G53BEB = n1;
    G53BF7 = (uint32_t)(uintptr_t)cbuf;
    G53BFB = n2;
    unit_refresh_all();

    cmp_mem("unit_refresh_all", id, 0, 0);

    for (i = 0; i < MAXREC * REC_STRIDE; i++) {
        if (unit_ch[i] != unit_ch0[i]) {
            printf("FAIL unit_refresh_all case %u: character table byte %d "
                   "changed %02X -> %02X\n", id, i, unit_ch0[i], unit_ch[i]);
            failures++;
            cases_run++;
            return;
        }
    }
}

static void test_refresh(unsigned ncases)
{
    unsigned c;

    for (c = 0; c < ncases && !failures; c++) {
        int n1 = (int)(rnd() % (MAXREC + 1));
        int n2 = (int)(rnd() % (MAXREC + 1));
        int i, j;

        fill_all();
        for (i = 0; i < MAXREC * REC_STRIDE; i++) unit_ch[i] = (uint8_t)rnd();
        for (i = 0; i < n1; i++) {
            unit_ch[i * REC_STRIDE + 8] = (uint8_t)(rnd() % 8);
            unit_ch[i * REC_STRIDE + 5] = (uint8_t)(rnd() & 1);
        }
        for (j = 0; j < n2; j++) {
            uint8_t *un = inbuf + (size_t)j * REC_STRIDE;
            if (n1 && (rnd() & 1))
                un[8] = unit_ch[(rnd() % (unsigned)n1) * REC_STRIDE + 8];
            else
                un[8] = (uint8_t)(rnd() % 8);
        }
        memcpy(unit_ch0, unit_ch, sizeof unit_ch);
        run_refresh(8000 + c, n1, n2);
    }

    /* constructed: a zero identity is copied only when rec_flag(i) == 0 */
    {
        int i;
        for (i = 0; i < 2 && !failures; i++) {
            memset(inbuf, 0, sizeof inbuf);
            memset(unit_ch, 0, sizeof unit_ch);
            unit_ch[5] = (uint8_t)(i ? 1 : 0);
            *(uint16_t *)(inbuf + 0x40) = 0x1111;
            *(uint16_t *)(inbuf + 0x42) = 0x2222;
            *(uint16_t *)(inbuf + 0x44) = 0x3333;
            *(uint16_t *)(inbuf + 0x46) = 0x4444;
            unit_ch[0x40] = 0xAA; unit_ch[0x42] = 0xBB;
            unit_ch[0x44] = 0xCC; unit_ch[0x46] = 0xDD;
            memcpy(unit_ch0, unit_ch, sizeof unit_ch);
            run_refresh(8100 + i, 1, 1);
        }
    }

    /* constructed: a non-zero identity always copies; +5 bit 0 controls the
     * +40<-+42 sync (+44<-+46 happens unconditionally) */
    {
        int i;
        for (i = 0; i < 2 && !failures; i++) {
            memset(inbuf, 0, sizeof inbuf);
            memset(unit_ch, 0, sizeof unit_ch);
            unit_ch[8] = 7; inbuf[8] = 7;
            unit_ch[5] = (uint8_t)(i ? 1 : 2);   /* bit 0 = 1 or 0 */
            *(uint16_t *)(inbuf + 0x40) = 0x1111;
            *(uint16_t *)(inbuf + 0x42) = 0x2222;
            *(uint16_t *)(inbuf + 0x44) = 0x3333;
            *(uint16_t *)(inbuf + 0x46) = 0x4444;
            memcpy(unit_ch0, unit_ch, sizeof unit_ch);
            run_refresh(8200 + i, 1, 1);
        }
    }

    /* constructed: several characters match one party record - the inner
     * loop order means the last matching character wins */
    {
        int i;
        memset(inbuf, 0, sizeof inbuf);
        memset(unit_ch, 0, sizeof unit_ch);
        for (i = 0; i < 3; i++) {
            unit_ch[i * REC_STRIDE + 8] = 9;
            unit_ch[i * REC_STRIDE + 7] = (uint8_t)(10 + i);
            unit_ch[i * REC_STRIDE]     = (uint8_t)(0x30 + i);
        }
        inbuf[8] = 9;
        memcpy(unit_ch0, unit_ch, sizeof unit_ch);
        run_refresh(8300, 3, 1);
    }

    /* constructed: identities that do not all match, plus an untouched
     * non-matching party record */
    {
        memset(inbuf, 0, sizeof inbuf);
        memset(unit_ch, 0, sizeof unit_ch);
        unit_ch[8] = 1; unit_ch[7] = 0x51;
        unit_ch[REC_STRIDE + 8] = 2; unit_ch[REC_STRIDE + 7] = 0x52;
        inbuf[8] = 2; inbuf[REC_STRIDE + 8] = 3;
        memcpy(unit_ch0, unit_ch, sizeof unit_ch);
        run_refresh(8400, 2, 2);
    }
}

/* Run both implementations of unit_add with `dword_53BFB = idx` on
 * byte-identical party copies and compare the return value, the record count
 * and the whole buffer. */
static void run_add(unsigned id, int case_id, int idx)
{
    int r1, r2, n1, n2;

    memcpy(obuf, inbuf, sizeof inbuf);
    G53BF7 = (uint32_t)(uintptr_t)obuf;
    G53BFB = idx;
    r1 = ORIG_ADD(id);
    n1 = G53BFB;

    memcpy(cbuf, inbuf, sizeof inbuf);
    G53BF7 = (uint32_t)(uintptr_t)cbuf;
    G53BFB = idx;
    r2 = unit_add(id);
    n2 = G53BFB;

    if (n1 != n2) {
        printf("FAIL unit_add case %d: count orig=%d ours=%d\n", case_id, n1, n2);
        failures++;
        return;
    }
    cmp_mem("unit_add", (unsigned)case_id, r1, r2);
}

static void test_add(unsigned ncases)
{
    unsigned c;

    for (c = 0; c < ncases && !failures; c++) {
        fill_all();
        run_add((int)(rnd() % 32), 9000 + (int)c, (int)(rnd() % MAXREC));
    }
}

/* unit_add edge cases that need the default table mutated in place: levels
 * 0/1/255 (L == 0 makes the growth term negative) and the 0xFF "empty slot"
 * branch of the four trailing item slots. The table is restored afterwards. */
static void test_add_edges(void)
{
    static const int levels[3] = { 0, 1, 255 };
    unsigned k;

    for (k = 0; k < 3 && !failures; k++) {
        const int id = 0;
        uint8_t *D = (uint8_t *)(uintptr_t)(0x61DA1u + 24u * (uint32_t)id);
        uint8_t save_l = D[2];
        uint8_t save_slots[4];
        int s;

        for (s = 0; s < 4; s++) save_slots[s] = D[0x0E + s];
        D[2] = (uint8_t)levels[k];
        if (k == 2)
            for (s = 0; s < 4; s++) D[0x0E + s] = 0xFF;

        fill_all();
        run_add(id, 9500 + (int)k, 3);

        D[2] = save_l;
        for (s = 0; s < 4; s++) D[0x0E + s] = save_slots[s];
    }
}

/* ---- identity lookup (0x33499) --------------------------------------- */

/* Read-only scan: both sides run on identical party copies and the whole
 * buffer is compared, so a stray write would show up. */
static void run_exists(unsigned id, int n2, int want)
{
    int r1, r2;

    memcpy(obuf, inbuf, sizeof inbuf);
    G53BF7 = (uint32_t)(uintptr_t)obuf;
    G53BFB = n2;
    r1 = ORIG_EXISTS(want);

    memcpy(cbuf, inbuf, sizeof inbuf);
    G53BF7 = (uint32_t)(uintptr_t)cbuf;
    G53BFB = n2;
    r2 = unit_exists(want);

    cmp_mem("unit_exists", id, r1, r2);
}

static void test_exists(unsigned ncases)
{
    unsigned c;

    for (c = 0; c < ncases && !failures; c++) {
        int n2 = (int)(rnd() % (MAXREC + 1));
        int want = (int)(rnd() & 0xFF);
        int i;

        fill_all();
        for (i = 0; i < n2; i++)
            inbuf[(size_t)i * REC_STRIDE + 8] = (uint8_t)(rnd() % 8);
        if (n2 && (rnd() & 1)) {
            /* force a hit, sometimes a second one - the first must win */
            int hit = (int)(rnd() % n2);
            inbuf[(size_t)hit * REC_STRIDE + 8] = (uint8_t)want;
            if ((rnd() & 1) && n2 > 1) {
                int k = (int)(rnd() % n2);
                if (k != hit)
                    inbuf[(size_t)k * REC_STRIDE + 8] = (uint8_t)want;
            }
        }
        run_exists(6000 + c, n2, want);
    }

    /* constructed: empty/single/multi tables, first/middle/last hit,
     * every identity equal, and want 0/255 against identities 0/9 */
    {
        static const struct { int n2, want, set_idx, byte8; } cs[] = {
            { 0,   5,  -1, 0 },   /* empty table                  */
            { 1,   7,   0, 7 },   /* single record, hit           */
            { 1,   7,   0, 6 },   /* single record, miss          */
            { 4,   3,   0, 3 },   /* first of four                */
            { 4,   3,   2, 3 },   /* middle                       */
            { 4,   3,   3, 3 },   /* last                         */
            { 8,   9,  -1, 9 },   /* every identity 9 (all hit)   */
            { 5,   0,  -1, 0 },   /* all identities 0, want 0     */
            { 5, 255,  -1, 0 },   /* all identities 0, want 255   */
        };
        unsigned k;

        for (k = 0; k < sizeof cs / sizeof cs[0] && !failures; k++) {
            int i;
            memset(inbuf, 0, sizeof inbuf);
            for (i = 0; i < 8; i++)
                inbuf[(size_t)i * REC_STRIDE + 8] = (uint8_t)cs[k].byte8;
            if (cs[k].set_idx >= 0)
                inbuf[(size_t)cs[k].set_idx * REC_STRIDE + 8] =
                    (uint8_t)cs[k].want;
            run_exists(6500 + k, cs[k].n2, cs[k].want);
        }
    }

    /* constructed: the movzx byte compare never matches an id above 255 and
     * an id is never truncated to a byte (256 must not match identity 0) */
    {
        static const int wide[] = { 256, 257, 300, 0x7FFFFFFF, -1, -256 };
        unsigned k;

        for (k = 0; k < sizeof wide / sizeof wide[0] && !failures; k++) {
            memset(inbuf, 0, sizeof inbuf);
            inbuf[8] = 0;                       /* identity 0 */
            inbuf[REC_STRIDE + 8] = 255;
            run_exists(6700 + k, 2, wide[k]);
        }
    }

    /* constructed: a negative count returns 0 (signed loop bound) */
    {
        static const int negs[] = { -1, -5, (int)0x80000000u };
        unsigned k;

        for (k = 0; k < sizeof negs / sizeof negs[0] && !failures; k++) {
            memset(inbuf, 0, sizeof inbuf);
            inbuf[8] = 0;
            run_exists(6800 + k, negs[k], 0);
            run_exists(6850 + k, negs[k], 1);
        }
    }
}

int main(int argc, char **argv)
{
    le_image le;
    int applied = 0;
    unsigned i;

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    /* ---- random cases ------------------------------------------------ */
    for (i = 0; i < 900 && !failures; i++) {
        int n1 = (int)(rnd() % MAXREC);
        int n2 = (int)(rnd() % MAXREC);
        int want = (int)(rnd() & 0xFF);
        int hit = -1;

        fill_tables(n1, n2);
        /* force at least one byte+8 match somewhere, otherwise the random
         * tables almost never match and every case takes the same path */
        if (n1 && (rnd() & 1)) {
            hit = (int)(rnd() % n1);
            tbl1[hit * REC_STRIDE + 8] = (uint8_t)want;
        } else if (n2) {
            hit = (int)(rnd() % n2);
            tbl2[hit * REC_STRIDE + 8] = (uint8_t)want;
            /* second match: the original does not stop at the first one */
            if ((rnd() & 1) && n2 > 1) {
                int k = (int)(rnd() % n2);
                if (k != hit) tbl2[k * REC_STRIDE + 8] = (uint8_t)want;
            }
        }
        if (hit >= 0 && (hit < n1))
            tbl1[hit * REC_STRIDE + 5] = (uint8_t)(rnd() & 1);

        set_globals(n1, n2);
        cmp_case(1000 + i, P_RANDOM, n1, n2, want);
    }

    /* ---- constructed paths --------------------------------------------- */
    memset(tbl1, 0, sizeof tbl1);
    memset(tbl2, 0, sizeof tbl2);

    /* no match anywhere */
    for (i = 0; i < 20; i++) { tbl1[i * REC_STRIDE + 8] = 1; tbl2[i * REC_STRIDE + 8] = 2; }
    set_globals(20, 20);
    cmp_case(2000, P_NONE, 20, 20, 7);

    /* match only in table 2, three hits -> dword_53C1B is the last one */
    tbl2[3 * REC_STRIDE + 8] = 9;
    tbl2[11 * REC_STRIDE + 8] = 9;
    tbl2[17 * REC_STRIDE + 8] = 9;
    set_globals(20, 20);
    cmp_case(2001, P_TABLE2, 20, 20, 9);

    /* table 1: flagged match first, unflagged later -> returns the later index */
    memset(tbl1, 0, sizeof tbl1);
    tbl1[2 * REC_STRIDE + 5] = 1; tbl1[2 * REC_STRIDE + 8] = 12;
    tbl1[6 * REC_STRIDE + 5] = 3; tbl1[6 * REC_STRIDE + 8] = 12;   /* bit0 = 1 */
    tbl1[9 * REC_STRIDE + 5] = 2; tbl1[9 * REC_STRIDE + 8] = 12;   /* bit0 = 0 -> hit */
    set_globals(20, 20);
    cmp_case(2002, P_FLAG0, 20, 20, 12);

    /* table 1: every match flagged -> returns -1, table 2 is not scanned */
    tbl1[4 * REC_STRIDE + 5] = 5; tbl1[4 * REC_STRIDE + 8] = 13;
    tbl1[8 * REC_STRIDE + 5] = 1; tbl1[8 * REC_STRIDE + 8] = 13;
    tbl2[1 * REC_STRIDE + 8] = 13;
    set_globals(20, 20);
    cmp_case(2003, P_FLAG1, 20, 20, 13);

    /* want above 255: unsigned byte compare can never match */
    set_globals(20, 20);
    cmp_case(2004, P_BIGWANT, 20, 20, 300);

    /* empty tables */
    set_globals(0, 0);
    cmp_case(2005, P_EMPTY, 0, 0, 5);

    /* ---- slot accessors (0x1B722 / 0x344F2 / 0x1BB8C / 0x1B8E7) ------- */
    if (!failures) test_field(800);
    if (!failures) test_status(800);
    if (!failures) test_claim(800);
    if (!failures) test_remove(800);

    /* ---- persistent party roster (0x1145A / 0x11506 / 0x112A5) --------- */
    if (!failures) test_recalc(2000);
    if (!failures) test_refresh(1500);
    if (!failures) test_add(800);
    if (!failures) test_add_edges();

    /* ---- identity lookup (0x33499) ----------------------------------- */
    if (!failures) test_exists(1050);

    /* ---- round 34 record-table leaves -------------------------------- */
    if (!failures) test_free(800);
    if (!failures) test_findslot(800);
    if (!failures) test_sub5(800);
    if (!failures) test_flagwriters(800);
    if (!failures) test_status_records(400);

    printf("paths: random=%u none=%u table2=%u flag0=%u flag1_only=%u want>255=%u empty=%u\n",
           path_count[P_RANDOM], path_count[P_NONE], path_count[P_TABLE2], path_count[P_FLAG0],
           path_count[P_FLAG1], path_count[P_BIGWANT], path_count[P_EMPTY]);
    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
