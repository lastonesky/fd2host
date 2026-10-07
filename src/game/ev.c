/* ev.c - FD2 scene-event handlers of funcs_1199C (source translation).
 *
 * See ev.h for the address table, the real ABI and the per-function return
 * values. This module is deliberately small: each function is the literal
 * machine-code behaviour, with the service calls going through their
 * **original addresses** (the scene.c / msg.c pattern):
 *
 *   - in the host, src/repl.c has already patched vm_run (0x15F84) and the
 *     record services to C, so the calls land on the translations;
 *   - src/evcheck.c hooks the same addresses to recording stubs, so this C
 *     can be compared against the original machine code call by call.
 *
 * The record table (dword_53A45), the one-shot flag block (dword_53AD5) and
 * the VM stream (dword_53A79) stay at their original data-segment addresses;
 * all three are read by dozens of not-yet-translated sites, so the C must
 * touch the real dwords (docs/TRANSLATION.md §3).
 */
#include "ev.h"

#include <stdint.h>

/* --- service entry points (called at their original addresses) --------- */
typedef int      (*vm_fn)(void *stream, int sub, int addr, int pitch, int fg,
                          int shadow, int bgfill, int line_step, int wait);
typedef uint32_t (*status_fn)(int start, int end, int value);
typedef int      (*flag_fn)(int index);
typedef int      (*slot_free_fn)(int index);
typedef int      (*slot_claim_fn)(int index, int value);
typedef uint32_t (*mask_records_fn)(void);

#define ORIG_VM_RUN     ((vm_fn)          (uintptr_t)0x00015F84u)
#define ORIG_STATUS     ((status_fn)      (uintptr_t)0x000344F2u)
#define ORIG_REC_FLAG   ((flag_fn)        (uintptr_t)0x00034894u)
#define ORIG_SLOT_FREE  ((slot_free_fn)   (uintptr_t)0x0001B8A6u)
#define ORIG_SLOT_CLAIM ((slot_claim_fn)  (uintptr_t)0x0001BB8Cu)
#define ORIG_MASK       ((mask_records_fn)(uintptr_t)0x00034D64u)

/* --- original data-segment globals ------------------------------------- */
#define dword_53A45 (*(uint32_t *)(uintptr_t)0x00053A45u) /* 80-byte records */
#define dword_53AD5 (*(uint32_t *)(uintptr_t)0x00053AD5u) /* status block     */
#define dword_53A79 (*(uint32_t *)(uintptr_t)0x00053A79u) /* VM stream        */

#define REC_STRIDE 80
#define VGA_BASE   0x000A0000u

/* Every vm_run call in this cluster passes the same eight tail arguments
 * (addr 0xA0000, pitch 320, fg 205, shadow 76, fill 74, line step 19,
 * wait 1); only the sub-stream changes. */
static int vm_sub(int sub)
{
    return ORIG_VM_RUN((void *)(uintptr_t)dword_53A79, sub, VGA_BASE, 320,
                       205, 76, 74, 19, 1);
}

/* 0x34738 */
void ev_rec13_set1(void)
{
    *((uint8_t *)(uintptr_t)dword_53A45 + 13 * REC_STRIDE + 6) = 1;
    vm_sub(7);
}

/* 0x348EA */
uint32_t ev_status24_27(void)
{
    uint32_t flag = *((uint8_t *)(uintptr_t)dword_53AD5 + 16);

    if (flag == 0) {
        ORIG_STATUS(24, 27, 7);
        vm_sub(3);
        *((uint8_t *)(uintptr_t)dword_53AD5 + 16) = 1;
        return dword_53AD5;
    }
    return flag;
}

/* 0x34A6C - the scan runs to completion (no early break), then the second
 * sub-stream runs when at least one flag was clear. */
void ev_status7_36(void)
{
    int i, hit = 0;

    ORIG_STATUS(7, 36, 7);
    vm_sub(8);

    for (i = 7; i <= 36; i++) {
        if (ORIG_REC_FLAG(i) == 0)
            hit = 1;
    }
    if (hit)
        vm_sub(11);
}

/* 0x34B2F */
void ev_flag8_gate(void)
{
    if (ORIG_REC_FLAG(8) == 0)
        vm_sub(2);
}

/* 0x34CF1 - 80*idx is the machine code's 32-bit `shl/add/shl` sequence. */
uint32_t ev_rec6_gate(int idx)
{
    uint32_t b = *((uint8_t *)(uintptr_t)dword_53A45
                   + (uint32_t)idx * REC_STRIDE + 6);

    if (b != 0) {
        ORIG_STATUS(9, 27, 0);
        *((uint8_t *)(uintptr_t)dword_53AD5 + 16) = 1;
        return dword_53AD5;
    }
    return b;
}

/* 0x34D92 */
uint32_t ev_mask_records(void)
{
    vm_sub(2);
    return ORIG_MASK();
}

/* 0x34F74 */
uint32_t ev_clear_status12_13(void)
{
    vm_sub(2);
    *((uint8_t *)(uintptr_t)dword_53A45 + 12 * REC_STRIDE + 52) = 0;
    *((uint8_t *)(uintptr_t)dword_53A45 + 13 * REC_STRIDE + 52) = 0;
    return dword_53A45 + 13 * REC_STRIDE;
}

/* 0x35123 - the machine code passes the incoming argument (which the first
 * test has already proved to be 0) to both slot services. */
void ev_slot8_claim(int idx)
{
    if (idx != 0)
        return;
    if (ORIG_SLOT_FREE(idx) == 8)
        return;
    if (*((uint8_t *)(uintptr_t)dword_53AD5 + 16) != 0)
        return;

    ORIG_SLOT_CLAIM(idx, 89);
    vm_sub(11);
    *((uint8_t *)(uintptr_t)dword_53AD5 + 16) = 1;
}

/* 0x35191 */
uint32_t ev_status16_71(void)
{
    uint32_t flag = *((uint8_t *)(uintptr_t)dword_53AD5 + 16);

    if (flag == 0) {
        ORIG_STATUS(16, 71, (int)flag);
        vm_sub(1);
        *((uint8_t *)(uintptr_t)dword_53AD5 + 16) = 1;
        return dword_53AD5;
    }
    return flag;
}

/* 0x351E6 - the window 64..73 is a constant, independent of the record
 * count; the fields cleared are byte +53 (not +52). */
uint32_t ev_clear64_73(void)
{
    int i;

    vm_sub(6);
    for (i = 64; i <= 73; i++)
        *((uint8_t *)(uintptr_t)dword_53A45 + (uint32_t)i * REC_STRIDE + 53) = 0;

    ORIG_STATUS(64, 73, 3);
    return ORIG_STATUS(35, 49, 0);
}

/* 0x35258 */
uint32_t ev_status16_34(void)
{
    vm_sub(8);
    return ORIG_STATUS(16, 34, 0);
}
