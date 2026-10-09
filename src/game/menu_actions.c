/* menu_actions.c - FD2 menu/party action handlers (see menu_actions.h).
 *
 * All originals are cdecl: the C bodies push the same arguments the machine
 * code does (rec_flag(index), unit_exists(id), vm_run(stream,...), res_load
 * index, ...) and end with the same service calls. External helpers are
 * reached through their fixed game addresses so the differential harness can
 * hook them and the running host sees the repl-patched C.
 */
#include "menu_actions.h"

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
#define byte_53AFA  (*(uint8_t  *)(uintptr_t)0x00053AFAu)
#define qword_53AA9 (*(uint64_t *)(uintptr_t)0x00053AA9u)
#define qword_53AB1 (*(uint64_t *)(uintptr_t)0x00053AB1u)

#define REC 0x50u

/* --- shared vm tails --------------------------------------------------- */
/* 0x33206/0x3312D: draw one menu sub-stream, then move the portrait back. */
static void menu_vm(int sub)
{
    ORIG_VM((void *)(uintptr_t)dword_53A79, sub,
            0xA0000, 0x140, 0xCD, 0x4C, 0x4A, 0x13, 1);
}

/* loc_3344D */
static void menu_tail_draw0(void)
{
    menu_vm(0);
    ORIG_GLIDE(0);
}

/* 0x3312D is shared, but the sub-stream number is the 8th argument pushed by
 * the *caller* before entering it:
 *   loc_33028 (0x33219, 0x3367E) pushes 1,
 *   loc_3310C (0x33367, 0x335DA) pushes 2,
 *   0x33AAE directly        pushes 0.
 * So the tail proper is "draw sub-stream N, clear the mouth, glide back". */
static void menu_tail_drawN_clear(int sub)
{
    menu_vm(sub);
    ORIG_CLEAR();
    ORIG_GLIDE(0);
}

/* loc_33440 + loc_3344D */
static void menu_tail_step_clear_draw0(int sel)
{
    ORIG_1366A(sel);
    ORIG_CLEAR();
    menu_tail_draw0();
}

/* loc_3310C (sub 2) + loc_3312D */
static void menu_tail_step_draw2_clear(int sel)
{
    ORIG_1366A(sel);
    menu_tail_drawN_clear(2);
}

/* --- 0x205DA ----------------------------------------------------------- */
void menu_reload_world(void)
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

/* --- menu A: option-denied checks -------------------------------------- */

/* 0x206C5 */
void menu_need_records_marked(void)
{
    int i;
    ORIG_205BE();
    for (i = 5; i < 11; i++) {
        if ((*(uint8_t *)(uintptr_t)(dword_53A45 + REC * (uint32_t)i + 5) & 1) == 0)
            return;
    }
    dword_53ECC = 1;
}

/* 0x20707 */
void menu_need_flags_clear_50_51(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(50) != 0 || ORIG_REC_FLAG(51) != 0)
        dword_53ECC = 1;
}

/* 0x2073D */
void menu_need_flag_clear_14(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(14) != 0)
        dword_53ECC = 1;
}

/* 0x20765 */
void menu_need_any_clear_15_26(void)
{
    int i, any_clear = 0;

    ORIG_205BE();
    for (i = 0; i < 12; i++) {
        if (ORIG_REC_FLAG(i + 15) == 0)
            any_clear = 1;
    }
    if (any_clear == 0) {
        dword_53ECC = 1;
        menu_vm(0x0A);
    }
    if ((int32_t)dword_53BEF > 5) {
        if (ORIG_REC_FLAG(0x3B) != 0) {
            dword_53ECC = 1;
            menu_vm(2);
        }
    }
}

/* 0x20822 */
void menu_need_flag_clear_64(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(0x40) != 0)
        dword_53ECC = 1;
}

/* 0x2084A */
void menu_need_flag_clear_65(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(0x41) != 0)
        dword_53ECC = 1;
}

/* 0x20872 */
void menu_need_flag_clear_52_no_unit18(void)
{
    ORIG_205BE();
    if (ORIG_UNIT_EXISTS(0x12) != 0)
        return;
    if (ORIG_REC_FLAG(0x34) != 0) {
        menu_vm(2);
        dword_53ECC = 1;
    }
}

/* 0x20926 */
void menu_need_flag_clear_64_late(void)
{
    ORIG_205BE();
    if ((int32_t)dword_53BEF <= 6)
        return;
    if (ORIG_REC_FLAG(0x40) != 0)
        dword_53ECC = 1;
}

/* 0x20957 */
void menu_need_any_clear_26_43(void)
{
    int i, any_clear = 0;

    ORIG_205BE();
    for (i = 0x26; i < 0x2E; i++) {
        if (ORIG_REC_FLAG(i + 15) == 0)
            any_clear = 1;
    }
    if (any_clear == 0) {
        dword_53ECC = 1;
        menu_vm(0x0A);
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

/* 0x20A51 */
void menu_need_flags_clear_16_17(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(0x10) != 0 || ORIG_REC_FLAG(0x11) != 0)
        dword_53ECC = 1;
}

/* 0x20A87 */
void menu_need_flag_clear_1(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(1) != 0)
        dword_53ECC = 1;
}

/* 0x20B14 */
void menu_need_flag_clear_16(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(0x10) != 0)
        dword_53ECC = 1;
}

/* 0x20B3C */
void menu_need_flags_clear_1_2(void)
{
    ORIG_205BE();
    if (ORIG_REC_FLAG(1) != 0 || ORIG_REC_FLAG(2) != 0)
        dword_53ECC = 1;
}

/* --- menu B: menu-screen actions --------------------------------------- */

/* 0x3314B */
void menu_show_reload(void)
{
    ORIG_205DA();
    dword_51A83 = 0;
    menu_tail_draw0();
}

/* 0x33219 */
void menu_show_reload_pair(void)
{
    ORIG_205DA();
    ORIG_135DD(7, 32);
    ORIG_1366A(31);
    menu_vm(0);
    ORIG_135DD(7, 23);
    ORIG_1366A(32);
    menu_tail_drawN_clear(1);
}

/* 0x3332B */
void menu_show_mark_two(void)
{
    ORIG_205DA();
    ORIG_135DD(10, 0);
    *(uint8_t *)(uintptr_t)(dword_53A45 + 4038) = 100;
    *(uint8_t *)(uintptr_t)(dword_53A45 + 4118) = 100;
    menu_tail_draw0();
}

/* 0x3346B (also 0x335A0 and 0x33674) */
void menu_show_plain(void)
{
    ORIG_205DA();
    menu_tail_draw0();
}

/* 0x3347C */
void menu_show_step20(void)
{
    ORIG_205DA();
    ORIG_135DD(20, 20);
    menu_tail_draw0();
}

/* 0x335AA */
void menu_show_or_rebuild_sprites(void)
{
    ORIG_205DA();
    if (ORIG_UNIT_EXISTS(0x12) == 0)
        ORIG_LOAD(1);
    menu_tail_draw0();
}

/* 0x3367E */
void menu_show_step16_then_clear(void)
{
    ORIG_205DA();
    ORIG_135DD(16, 28);
    menu_tail_step_clear_draw0(0x43);
}

/* 0x33AAE */
void menu_show_step9_then_clear(void)
{
    ORIG_205DA();
    ORIG_135DD(9, 39);
    ORIG_1366A(76);
    menu_tail_drawN_clear(0);
}

/* 0x33169 */
void menu_rebuild_sprites_and_show(void)
{
    ORIG_205DA();
    menu_vm(0);
    dword_51A83 = 0;
    byte_53AFA = 1;
    ORIG_LOAD(1);
    byte_53AFA = 0;
    ORIG_135DD(8, 1);
    ORIG_1366A(28);
    ORIG_135DD(8, 0);
    ORIG_1366A(29);
    menu_vm(1);
    ORIG_GLIDE(0);
}

/* 0x3327D */
void menu_mark_records_and_show(void)
{
    int i;
    ORIG_205DA();
    for (i = 0; i < 11; i++)
        *(uint8_t *)(uintptr_t)(dword_53A45 + REC * (uint32_t)i + 3) = 2;
    ORIG_135DD(6, 0);
    menu_vm(0);
    dword_51A83 = 0;
    ORIG_1366A(35);
    menu_vm(1);
    ORIG_GLIDE(0);
    ORIG_CLEAR();
}

/* 0x33367 */
void menu_rebuild_sprites_and_clear(void)
{
    ORIG_205DA();
    menu_vm(0);
    dword_51A83 = 0;
    ORIG_135DD(10, 7);
    ORIG_LOAD(1);
    ORIG_1366A(38);
    menu_vm(1);
    menu_tail_step_draw2_clear(0x27);
}

/* 0x333F5 */
void menu_reset_and_show(void)
{
    ORIG_205DA();
    ORIG_135DD(4, 4);
    byte_53AFA = 1;
    ORIG_LOAD(1);
    byte_53AFA = 0;
    ORIG_1366A(40);
    ORIG_135DD(11, 40);
    ORIG_1366A(41);
    ORIG_CLEAR();
    menu_tail_draw0();
}

/* 0x334D9 */
void menu_show_gated_by_unit(void)
{
    int sub;

    ORIG_205DA();
    sub = (ORIG_UNIT_EXISTS(0x0C) == 0) ? 3 : 0;
    menu_vm(sub);
    ORIG_135DD(0x18, 0x11);
    menu_vm(sub + 1);
    dword_51A83 = 0;
    ORIG_1366A(0x30);
    menu_vm(sub + 2);
    ORIG_GLIDE(0);
}

/* 0x335DA */
void menu_step_pair_then_clear(void)
{
    ORIG_205DA();
    menu_vm(0);
    dword_51A83 = 0;
    ORIG_135DD(0x10, 4);
    ORIG_1366A(0x36);
    menu_vm(1);
    dword_51A83 = 0;
    ORIG_135DD(0x10, 4);
    menu_tail_step_draw2_clear(0x37);
}

/* --- menu B tail (0x338C4 / 0x3396A / 0x1D4F6) ------------------------- */

typedef void *(*resload_fn)(const char *, void *, int);
typedef int   (*sfx_fn)(void *, int, int);
typedef void  (*h1v_fn)(int);
typedef void  (*free_fn)(void *);

#define ORIG_RESLOAD ((resload_fn)(uintptr_t)0x000111BAu)
#define ORIG_SFX     ((sfx_fn)    (uintptr_t)0x00025A96u)
#define ORIG_SCROLL  ((h1v_fn)    (uintptr_t)0x00024B4Du)
#define ORIG_DELAY   ((h1v_fn)    (uintptr_t)0x0003790Au)
#define ORIG_FREE    ((free_fn)   (uintptr_t)0x0003776Eu)
#define dword_53B13 (*(uint32_t *)(uintptr_t)0x00053B13u)
#define dword_53A49 (*(uint32_t *)(uintptr_t)0x00053A49u)

/* 0x338C4 */
void menu_show_step_pairs(void)
{
    ORIG_205DA();
    menu_vm(0);
    ORIG_LOAD(1);
    ORIG_135DD(0, 4);
    ORIG_DELAY(400);
    ORIG_135DD(0, 0x16);
    ORIG_DELAY(400);
    ORIG_135DD(0x1A, 0x18);
    ORIG_DELAY(400);
    ORIG_135DD(0x1A, 2);
    ORIG_DELAY(400);
    menu_vm(1);
    ORIG_GLIDE(0);
}

/* 0x3396A */
void menu_show_map_pan(void)
{
    ORIG_205DA();
    dword_53B13 = 0;
    dword_53B13 = (uint32_t)(uintptr_t)ORIG_RESLOAD("FDOTHER.DAT", NULL, 88);
    ORIG_135DD(5, 0);
    menu_vm(1);
    memset((void *)(uintptr_t)dword_53A49, 0, 0x25680u);
    ORIG_SFX((void *)(uintptr_t)dword_53B13, 1, 1);
    ORIG_SCROLL(20);
    ORIG_DELAY(600);
    ORIG_SFX((void *)(uintptr_t)dword_53B13, 1, 1);
    ORIG_SCROLL(20);
    ORIG_DELAY(600);
    ORIG_SFX((void *)(uintptr_t)dword_53B13, 1, 1);
    ORIG_SCROLL(20);
    ORIG_DELAY(600);
    ORIG_SFX((void *)(uintptr_t)dword_53B13, 1, 1);
    ORIG_SCROLL(60);
    menu_vm(2);
    ORIG_GLIDE(0);
    menu_stop_and_free_music();
}

/* 0x1D4F6 */
void menu_stop_and_free_music(void)
{
    ORIG_SFX((void *)(uintptr_t)dword_53B13, -1, 1);
    ORIG_FREE((void *)(uintptr_t)dword_53B13);
}
