/* ev7.c - FD2 menu/party action handlers, batch 7 (see ev7.h).
 *
 * All originals are cdecl: the C bodies push the same arguments the machine
 * code does (rec_flag(index), unit_exists(id), vm_run(stream,...), res_load
 * index, ...) and end with the same service calls. External helpers are
 * reached through their fixed game addresses so the differential harness can
 * hook them and the running host sees the repl-patched C.
 */
#include "ev7.h"

#include <stdint.h>
#include <string.h>

/* --- fixed-address helpers --------------------------------------------- */
typedef int  (*flag_fn)(int);
typedef int  (*unit_exists_fn)(int);
typedef void (*h2_fn)(int, int);
typedef void (*h1_fn)(int);
typedef void (*h0_fn)(void);
typedef int  (*vm_fn)(void *, int, int, int, int, int, int, int, int);
typedef int  (*load_fn)(int);

#define ORIG_205BE      ((h0_fn)  (uintptr_t)0x000205BEu)
#define ORIG_205DA      ((h0_fn)  (uintptr_t)0x000205DAu)
#define ORIG_REC_FLAG   ((flag_fn)(uintptr_t)0x00034894u)
#define ORIG_UNIT_EXISTS ((unit_exists_fn)(uintptr_t)0x00033499u)
#define ORIG_VM         ((vm_fn)  (uintptr_t)0x00015F84u)
#define ORIG_135DD      ((h2_fn)  (uintptr_t)0x000135DDu)
#define ORIG_1366A      ((h1_fn)  (uintptr_t)0x0001366Au)
#define ORIG_GLIDE      ((h1_fn)  (uintptr_t)0x00012D7Bu)
#define ORIG_CLEAR      ((h0_fn)  (uintptr_t)0x000134E4u)
#define ORIG_LOAD       ((load_fn)(uintptr_t)0x00010B4Eu)
#define ORIG_VIEW       ((h1_fn)  (uintptr_t)0x00011CACu)
#define ORIG_FADEIN     ((h0_fn)  (uintptr_t)0x0001F525u)
#define ORIG_FLUSH      ((h0_fn)  (uintptr_t)0x0004E381u)
#define ORIG_1088D      ((h1_fn)  (uintptr_t)0x0001088Du)

/* --- globals ----------------------------------------------------------- */
#define dword_51A83 (*(uint32_t *)(uintptr_t)0x00051A83u)
#define dword_53A45 (*(uint32_t *)(uintptr_t)0x00053A45u)
#define dword_53A79 (*(uint32_t *)(uintptr_t)0x00053A79u)
#define dword_53AD5 (*(uint32_t *)(uintptr_t)0x00053AD5u)
#define dword_53AB9 (*(uint32_t *)(uintptr_t)0x00053AB9u)
#define dword_53ABD (*(uint32_t *)(uintptr_t)0x00053ABDu)
#define dword_53BEB (*(uint32_t *)(uintptr_t)0x00053BEBu)
#define dword_53BEF (*(uint32_t *)(uintptr_t)0x00053BEFu)
#define dword_53C03 (*(uint32_t *)(uintptr_t)0x00053C03u)
#define dword_53ECC (*(uint32_t *)(uintptr_t)0x00053ECCu)
#define qword_53AA9 (*(uint64_t *)(uintptr_t)0x00053AA9u)
#define qword_53AB1 (*(uint64_t *)(uintptr_t)0x00053AB1u)

#define REC 0x50u

/* --- shared vm tails --------------------------------------------------- */
/* 0x33206/0x3312D: draw one menu sub-stream, then move the portrait back. */
static void ev7_vm(int sub)
{
    ORIG_VM((void *)(uintptr_t)dword_53A79, sub,
            0xA0000, 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* loc_3344D */
static void ev7_tail_d(void)
{
    ev7_vm(0);
    ORIG_GLIDE(0);
}

/* loc_33028/0x3312D */
static void ev7_tail_g(void)
{
    ev7_vm(1);
    ORIG_CLEAR();
    ORIG_GLIDE(0);
}

/* 0x3312D entered with sub 0 (0x33AAE): drawing sub-stream 0, then clear. */
static void ev7_tail_h(void)
{
    ev7_vm(0);
    ORIG_CLEAR();
    ORIG_GLIDE(0);
}

/* loc_33440 */
static void ev7_tail_e(int sel)
{
    ORIG_1366A(sel);
    ORIG_CLEAR();
    ev7_tail_d();
}

/* --- 0x205DA ----------------------------------------------------------- */
void ev7_205DA(void)
{
    dword_51A83 = 0;
    dword_53ECC = 0;
    ORIG_1088D(dword_53C03);
    memset((void *)(uintptr_t)dword_53AD5, 0, 32);
    qword_53AA9 = 0;
    qword_53AB1 = 0;
    dword_53AB9 = 0;
    dword_53ABD = 0;
    ORIG_VIEW(1);
    dword_51A83 = 1;
    ORIG_FADEIN();
    dword_53BEF = 1;
    ORIG_FLUSH();
}

/* --- menu A: record-flag validators (0x206C5 .. 0x20B3C) --------------- */

/* 0x206C5 - records 5..10 must all carry flag bit0; else select error 1. */
void ev7_206C5(void)
{
    int i;
    ORIG_205BE();
    for (i = 5; i < 11; i++) {
        if ((*(uint8_t *)(uintptr_t)(dword_53A45 + REC * (uint32_t)i + 5) & 1) == 0)
            return;
    }
    dword_53ECC = 1;
}

/* 0x20707 - fail if rec_flag(50) or rec_flag(51). */
void ev7_20707(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(50) != 0 || ORIG_REC_FLAG(51) != 0)
        dword_53ECC = 1;
}

/* 0x2073D - fail if rec_flag(14). */
void ev7_2073D(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(14) != 0)
        dword_53ECC = 1;
}

/* 0x20765 - "menu 10": if none of rec_flag(15..26) is clear, fail and draw
 * sub-stream 10; when dword_53BEF>5 and rec_flag(59) is set, fail and draw
 * sub-stream 2. */
void ev7_20765(void)
{
    int i, any_clear = 0;

    ORIG_205BE();
    for (i = 0; i < 12; i++) {
        if (ORIG_REC_FLAG(i + 15) == 0)
            any_clear = 1;
    }
    if (any_clear == 0) {
        dword_53ECC = 1;
        ev7_vm(0x0A);
    }
    if ((int32_t)dword_53BEF > 5) {
        if (ORIG_REC_FLAG(0x3B) != 0) {
            dword_53ECC = 1;
            ev7_vm(2);
        }
    }
}

/* 0x20822 - fail if rec_flag(64). */
void ev7_20822(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(0x40) != 0)
        dword_53ECC = 1;
}

/* 0x2084A - fail if rec_flag(65). */
void ev7_2084A(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(0x41) != 0)
        dword_53ECC = 1;
}

/* 0x20872 - only when unit 18 is absent: fail if rec_flag(52) and draw
 * sub-stream 2. */
void ev7_20872(void)
{
    ORIG_205BE();
    if (ORIG_UNIT_EXISTS(0x12) != 0)
        return;
    if (ORIG_REC_FLAG(0x34) != 0) {
        ev7_vm(2);
        dword_53ECC = 1;
    }
}

/* 0x20926 - when dword_53BEF>6: fail if rec_flag(64). */
void ev7_20926(void)
{
    ORIG_205BE();
    if ((int32_t)dword_53BEF <= 6)
        return;
    if (ORIG_REC_FLAG(0x40) != 0)
        dword_53ECC = 1;
}

/* 0x20957 - two-range validation (records 0x26..0x2D then 0x2E..0x43 plus
 * rec_flag(0)/rec_flag(0x34)); fail code 1 and/or 2. */
void ev7_20957(void)
{
    int i, any_clear = 0;

    ORIG_205BE();
    for (i = 0x26; i < 0x2E; i++) {
        if (ORIG_REC_FLAG(i + 15) == 0)
            any_clear = 1;
    }
    if (any_clear == 0) {
        dword_53ECC = 1;
        ev7_vm(0x0A);
    }
    if (ORIG_REC_FLAG(0) != 0 || ORIG_REC_FLAG(0x34) != 0)
        dword_53ECC = 1;

    any_clear = 0;
    for (i = 0x15; i < 0x25; i++) {
        if (ORIG_REC_FLAG(i + 15) == 0)
            any_clear = 1;
    }
    for (i = 0x2E; i < 0x44; i++) {
        if (ORIG_REC_FLAG(i + 15) == 0)
            any_clear = 1;
    }
    if (any_clear == 0)
        dword_53ECC = 2;
}

/* 0x20A51 - fail if rec_flag(16) or rec_flag(17). */
void ev7_20A51(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(0x10) != 0 || ORIG_REC_FLAG(0x11) != 0)
        dword_53ECC = 1;
}

/* 0x20A87 - fail if rec_flag(1). */
void ev7_20A87(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(1) != 0)
        dword_53ECC = 1;
}

/* 0x20B14 - fail if rec_flag(16). */
void ev7_20B14(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(0x10) != 0)
        dword_53ECC = 1;
}

/* 0x20B3C - fail if rec_flag(1) or rec_flag(2). */
void ev7_20B3C(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(1) != 0 || ORIG_REC_FLAG(2) != 0)
        dword_53ECC = 1;
}

/* --- menu B: script/vm actions (0x3314B .. 0x33AAE) -------------------- */

/* 0x3314B */
void ev7_3314B(void)
{
    ORIG_205DA();
    dword_51A83 = 0;
    ev7_tail_d();
}

/* 0x33219 */
void ev7_33219(void)
{
    ORIG_205DA();
    ORIG_135DD(7, 32);
    ORIG_1366A(31);
    ev7_vm(0);
    ORIG_135DD(7, 23);
    ORIG_1366A(32);
    ev7_tail_g();
}

/* 0x3332B */
void ev7_3332B(void)
{
    ORIG_205DA();
    ORIG_135DD(10, 0);
    *(uint8_t *)(uintptr_t)(dword_53A45 + 4038) = 100;
    *(uint8_t *)(uintptr_t)(dword_53A45 + 4118) = 100;
    ev7_tail_d();
}

/* 0x3346B */
void ev7_3346B(void)
{
    ORIG_205DA();
    ev7_tail_d();
}

/* 0x3347C */
void ev7_3347C(void)
{
    ORIG_205DA();
    ORIG_135DD(20, 20);
    ev7_tail_d();
}

/* 0x335A0 - body is the shared 0x33470 block. */
void ev7_335A0(void)
{
    ORIG_205DA();
    ev7_tail_d();
}

/* 0x335AA */
void ev7_335AA(void)
{
    ORIG_205DA();
    if (ORIG_UNIT_EXISTS(0x12) == 0)
        ORIG_LOAD(1);
    ev7_tail_d();
}

/* 0x33674 - body is the shared 0x33470 block. */
void ev7_33674(void)
{
    ORIG_205DA();
    ev7_tail_d();
}

/* 0x3367E */
void ev7_3367E(void)
{
    ORIG_205DA();
    ORIG_135DD(16, 28);
    ev7_tail_e(0x43);
}

/* 0x33AAE */
void ev7_33AAE(void)
{
    ORIG_205DA();
    ORIG_135DD(9, 39);
    ORIG_1366A(76);
    ev7_tail_h();
}
