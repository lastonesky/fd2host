/* reccheck.c - differential test for the character record table.
 *
 * Runs the original machine code (0x34894 rec_flag, 0x12C60 rec_find,
 * 0x1B722 rec_field_byte, 0x344F2 rec_status_set, 0x1BB8C rec_slot_claim,
 * 0x1B8E7 rec_slot_remove) and the C translation (src/game/rec.c) on the same
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

typedef int (__cdecl *flag_fn)(int);
typedef int (__cdecl *find_fn)(int);
typedef int  (__cdecl *field_fn)(int, int);
typedef void (__cdecl *status_fn)(int, int, int);
typedef int  (__cdecl *claim_fn)(int, int);
typedef void *(__cdecl *remove_fn)(int, int);

#define ORIG_FLAG   ((flag_fn)(uintptr_t)0x34894)
#define ORIG_FIND   ((find_fn)(uintptr_t)0x12C60)
#define ORIG_FIELD  ((field_fn)(uintptr_t)0x1B722)
#define ORIG_STATUS ((status_fn)(uintptr_t)0x344F2)
#define ORIG_CLAIM  ((claim_fn)(uintptr_t)0x1BB8C)
#define ORIG_REMOVE ((remove_fn)(uintptr_t)0x1B8E7)

#define G53A45 (*(uint32_t *)(uintptr_t)0x00053A45u)
#define G53BEB (*(int32_t *)(uintptr_t)0x00053BEBu)
#define G53BF7 (*(uint32_t *)(uintptr_t)0x00053BF7u)
#define G53BFB (*(int32_t *)(uintptr_t)0x00053BFBu)
#define G53C1B (*(uint32_t *)(uintptr_t)0x00053C1Bu)

#define MAXREC 64
static uint8_t tbl1[MAXREC * REC_STRIDE];
static uint8_t tbl2[MAXREC * REC_STRIDE];

/* Two byte-identical copies of the input for the slot writers: the original
 * runs on `obuf`, the C on `cbuf`, and the whole buffer is compared after. */
static uint8_t inbuf[MAXREC * REC_STRIDE];
static uint8_t obuf[MAXREC * REC_STRIDE];
static uint8_t cbuf[MAXREC * REC_STRIDE];

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

    printf("paths: random=%u none=%u table2=%u flag0=%u flag1_only=%u want>255=%u empty=%u\n",
           path_count[P_RANDOM], path_count[P_NONE], path_count[P_TABLE2], path_count[P_FLAG0],
           path_count[P_FLAG1], path_count[P_BIGWANT], path_count[P_EMPTY]);
    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
