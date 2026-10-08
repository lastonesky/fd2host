/* ev5.c - FD2 small scene/map/record leaves, batch 5 (see ev5.h).
 * Written from the original disassembly (cdecl: last push = first argument).
 */
#include "ev5.h"

#include <stdint.h>
#include <string.h>

/* --- services ---------------------------------------------------------- */
typedef void    *(*res_load_fn)(const char *f, void *old, int idx); /* 0x111BA */
typedef void     (*pal_add_fn)(int s, int e, int a);   /* 0x11DF2 */
typedef void     (*delay_fn)(int ms);                  /* 0x3790A */
typedef int      (*find_fn)(int rec, int value);       /* 0x2AEDB */
typedef void     (*vm_fn)(void *st, int sub, int addr, int p, int fg, int sh,
                          int bg, int ls, int w);
typedef int      (*flag_fn)(int idx);                  /* 0x34894 */
typedef int      (*slot_claim_fn)(int idx, int v);     /* 0x1BB8C */
typedef int      (*rec_field_fn)(int idx, int slot);   /* 0x1B722 */
typedef int      (*slot_find_fn)(int idx, int hi);     /* 0x1B83D */
typedef void    *(*tbl_fn)(int idx);                   /* 0x4E8BC */
typedef void     (*flush_fn)(void);                    /* 0x4E381 */
typedef void     (*outp_fn)(int port, int val);        /* 0x37AE5 */
typedef void     (*pal_anim_fn)(void);                 /* 0x4E31C */
typedef int      (*kbd_pending_fn)(void);              /* 0x10620 */

#define ORIG_RES_LOAD   ((res_load_fn)   (uintptr_t)0x000111BAu)
#define ORIG_PAL_ADD    ((pal_add_fn)    (uintptr_t)0x00011DF2u)
#define ORIG_DELAY      ((delay_fn)      (uintptr_t)0x0003790Au)
#define ORIG_FIND       ((find_fn)       (uintptr_t)0x0002AEDBu)
#define ORIG_VM_RUN     ((vm_fn)         (uintptr_t)0x00015F84u)
#define ORIG_REC_FLAG   ((flag_fn)       (uintptr_t)0x00034894u)
#define ORIG_SLOT_CLAIM ((slot_claim_fn) (uintptr_t)0x0001BB8Cu)
#define ORIG_REC_FIELD  ((rec_field_fn)  (uintptr_t)0x0001B722u)
#define ORIG_SLOT_FIND  ((slot_find_fn)  (uintptr_t)0x0001B83Du)
#define ORIG_TBL_8BC    ((tbl_fn)        (uintptr_t)0x0004E8BCu)
#define ORIG_FLUSH      ((flush_fn)      (uintptr_t)0x0004E381u)
#define ORIG_OUTP       ((outp_fn)       (uintptr_t)0x00037AE5u)
#define ORIG_PAL_ANIM   ((pal_anim_fn)   (uintptr_t)0x0004E31Cu)
#define ORIG_KBD_PEND   ((kbd_pending_fn)(uintptr_t)0x00010620u)

extern uint32_t dos_lowmem_base;
#define BDA_W(off) (*(volatile uint16_t *)(uintptr_t)(dos_lowmem_base + (off)))

/* --- globals ----------------------------------------------------------- */
#define dword_53A0C (*(uint32_t *)(uintptr_t)0x00053A0Cu)
#define dword_53A45 (*(uint32_t *)(uintptr_t)0x00053A45u)
#define dword_53A51 (*(uint32_t *)(uintptr_t)0x00053A51u)
#define dword_53AC1 (*(uint32_t *)(uintptr_t)0x00053AC1u)
#define dword_53AC5 (*(uint32_t *)(uintptr_t)0x00053AC5u)
#define dword_53AD5 (*(uint32_t *)(uintptr_t)0x00053AD5u)
#define dword_53B13 (*(uint32_t *)(uintptr_t)0x00053B13u)
#define dword_53BEB (*(uint32_t *)(uintptr_t)0x00053BEBu)
#define dword_53BF7 (*(uint32_t *)(uintptr_t)0x00053BF7u)
#define dword_53BFB (*(uint32_t *)(uintptr_t)0x00053BFBu)
#define dword_53C57 (*(uint32_t *)(uintptr_t)0x00053C57u)
#define dword_53ECC (*(uint32_t *)(uintptr_t)0x00053ECCu)
#define dword_53F66 (*(uint32_t *)(uintptr_t)0x00053F66u)

#define REC 0x50

/* 0x2860A */
int ev5_2860A(int a, int b)
{
    if (a == b) return 31;
    if (a >= b) return 119;
    return 42;
}

/* 0x146A7 - set bit7 of map cell (x,y)'s +6 byte; returns the cell. */
void ev5_146A7(int x, int y)
{
    *(uint8_t *)(uintptr_t)(4 * ((uint32_t)x + dword_53AC1 * (uint32_t)y) + dword_53A51 + 6) |= 0x80u;
}

/* 0x13460 - wait for the BIOS tick to change, return it. */
int ev5_13460(void)
{
    while ((int32_t)(int16_t)BDA_W(0x46C) == (int32_t)dword_53A0C)
        ;
    dword_53A0C = (uint32_t)(int32_t)(int16_t)BDA_W(0x46C);
    return (int32_t)(int16_t)BDA_W(0x46C);
}

/* 0x13536 - clear bit7 of every record's +5 byte. */
void ev5_13536(void)
{
    uint32_t i;
    for (i = 0; i < dword_53BEB; i++)
        *(uint8_t *)(uintptr_t)(REC * i + dword_53A45 + 5) &= 0x7Fu;
}

/* 0x1D4CB */
void *ev5_1D4CB(void)
{
    dword_53B13 = 0;
    dword_53B13 = (uint32_t)(uintptr_t)ORIG_RES_LOAD("FDOTHER.DAT", NULL, 80);
    return (void *)(uintptr_t)dword_53B13;
}

/* 0x173E7 - index of the first zero of slots[0..3]; sets dword_53C57. */
void ev5_173E7(int *slots)
{
    for (dword_53C57 = 0; dword_53C57 < 4; dword_53C57++)
        if (slots[dword_53C57] == 0)
            break;
}

/* 0x24B14 - 1 if any record 0..15 has field byte == a5, else -1. */
int ev5_24B14(int a5)
{
    int i;
    for (i = 0; i < 16; i++)
        if (ORIG_FIND(i, a5) != -1)
            return 1;
    return -1;
}

/* 0x25052 - fade the palette down from `from`, `ms` per step. */
void ev5_25052(int from, int ms)
{
    while (from >= 0) {
        ORIG_PAL_ADD(0, 255, from);
        ORIG_DELAY(ms);
        --from;
    }
}

/* 0x25089 - reset every party record's +5, +64/+66, +68/+70. */
void ev5_25089(void)
{
    uint32_t i;
    for (i = 0; i < dword_53BFB; i++) {
        uint32_t r = REC * i + dword_53BF7;
        *(uint8_t *)(uintptr_t)(r + 5) = 0;
        *(uint16_t *)(uintptr_t)(r + 64) = *(uint16_t *)(uintptr_t)(r + 66);
        *(uint16_t *)(uintptr_t)(r + 68) = *(uint16_t *)(uintptr_t)(r + 70);
    }
}

/* 0x34317 - fill 6 direction bytes at `out`. */
void ev5_34317(int val, uint8_t *out)
{
    uint8_t v6 = (val % 8 > 3) ? 2 : 0;
    int     i;
    for (i = 0; i < 6; i++)
        out[i] = (uint8_t)(i + v6 + (val & 0xF8));
}

/* 0x1F6EF - fill (n-1) VGA rows with `value`. */
void ev5_1F6EF(int x, int y, int value, int n)
{
    uint8_t *p = (uint8_t *)(uintptr_t)((uint32_t)x + 320u * (uint32_t)y + 0xA0000u);
    int      i;
    for (i = 0; i < n - 1; i++) {
        memset(p, value, (size_t)(n - 1));
        p += 320;
    }
}

/* 0x1C220 - claim the first free slot of the first record whose +6 == 2. */
int ev5_1C220(int value)
{
    int r = 0, i;
    for (i = 0; i < (int)dword_53BEB; i++) {
        r = *(uint8_t *)(uintptr_t)(dword_53A45 + REC * (uint32_t)i + 6);
        if (r == 2) {
            r = ORIG_SLOT_CLAIM(i, value);
            if (r != -1)
                break;
        }
    }
    return r;
}

/* 0x1E5C0 - wait for `ticks` BIOS ticks or a key, then flush. */
void ev5_1E5C0(int ticks)
{
    int t0 = (int16_t)BDA_W(0x46C);
    int k;
    do {
        ORIG_PAL_ANIM();
        k = ORIG_KBD_PEND();
        if ((int16_t)BDA_W(0x46C) - t0 >= ticks || (int16_t)BDA_W(0x46C) < t0)
            k = 1;
    } while (k == 0);
    ORIG_FLUSH();
}

/* 0x2B749 - count nonzero bytes of p[0 .. dword_53BFB-2]. */
int ev5_2B749(const uint8_t *p)
{
    int i, v5 = 0;
    for (i = 0; i < (int)dword_53BFB - 1; i++)
        if (p[i] != 0)
            ++v5;
    return v5;
}

/* 0x26C9B - copy nine 6-byte rows from a table into `dst` at `pitch`. */
void ev5_26C9B(void *dst, int pitch, int index)
{
    uint8_t *d = (uint8_t *)dst;
    uint32_t base = dword_53F66 + *(uint32_t *)(uintptr_t)(dword_53F66 + 14) + 4;
    uint8_t *s = (uint8_t *)(uintptr_t)(base + 6u * (uint32_t)index);
    int      i;
    for (i = 0; i < 9; i++) {
        memmove(d, s, 6);
        d += pitch;
        s += 6;
    }
}

/* 0x314DE - load FDOTHER.DAT resource indexed by the "012345"[p[4]-1]. */
void *ev5_314DE(const uint8_t *p)
{
    static const char idx[6] = { '0', '1', '2', '3', '4', '5' };
    if (p != NULL && p[4] != 0)
        return ORIG_RES_LOAD("FDOTHER.DAT", NULL, idx[p[4] - 1]);
    return NULL;
}

/* 0x1B5F1 - count records whose kind (+6) == a5 and that pass the flag gate. */
int ev5_1B5F1(int kind)
{
    int i, v6 = 0;
    for (i = 0; i < (int)dword_53BEB; i++) {
        uint8_t *r = (uint8_t *)(uintptr_t)(REC * (uint32_t)i + dword_53A45);
        if (r[6] == kind) {
            int v9 = r[7];
            if (v9 != 121) {
                int v10 = r[31];
                if (v10 != 10 && ORIG_REC_FLAG(i) == 0)
                    ++v6;
            }
        }
    }
    return v6;
}

/* 0x14B16 - collect every revealed map cell as (x,y) pairs; returns count. */
int ev5_14B16(uint8_t *out)
{
    uint8_t *v7 = (uint8_t *)(uintptr_t)(dword_53A51 + 7);
    int      i, j, v6 = 0;
    for (i = 0; i < (int)dword_53AC5; i++) {
        for (j = 0; j < (int)dword_53AC1; j++) {
            if (*v7 != 255) {
                *out++ = (uint8_t)j;
                *out++ = (uint8_t)i;
                ++v6;
            }
            v7 += 4;
        }
    }
    return v6;
}

/* 0x203BD - fill the 256-entry DAC with (r,g,b). */
void ev5_203BD(int r, int g, int b)
{
    int i;
    for (i = 0; i < 256; i++) {
        ORIG_OUTP(0x3C8, i);
        ORIG_OUTP(0x3C9, r);
        ORIG_OUTP(0x3C9, g);
        ORIG_OUTP(0x3C9, b);
    }
}

/* 0x208CF */
void ev5_208CF(void)
{
    if (ORIG_REC_FLAG(0) != 0 || ORIG_REC_FLAG(16) != 0 || ORIG_REC_FLAG(17) != 0)
        dword_53ECC = 1;
    if (ORIG_REC_FLAG(52) != 0)
        dword_53ECC = 2;
}

/* 0x20AAF */
void ev5_20AAF(void)
{
    if (ORIG_REC_FLAG(0) != 0 || ORIG_REC_FLAG(1) != 0
        || ORIG_REC_FLAG(16) != 0 || ORIG_REC_FLAG(17) != 0)
        dword_53ECC = 1;
    if (ORIG_REC_FLAG(18) != 0)
        dword_53ECC = 2;
}

/* 0x20BF5 */
void ev5_20BF5(void)
{
    if (ORIG_REC_FLAG(20) != 0)
        dword_53ECC = 2;
    if (ORIG_REC_FLAG(0) != 0)
        dword_53ECC = 1;
    if (ORIG_REC_FLAG(1) != 0) {
        ORIG_VM_RUN((void *)(uintptr_t)dword_53A45, 7, 0xA0000, 320, 205,
                    76, 74, 19, 1);
        dword_53ECC = 1;
    }
}

/* 0x20B72 */
void ev5_20B72(void)
{
    if (*(uint8_t *)(uintptr_t)(dword_53AD5 + 18) != 0
        && *(uint8_t *)(uintptr_t)(dword_53AD5 + 19) != 0
        && *(uint8_t *)(uintptr_t)(dword_53AD5 + 20) != 0)
        dword_53ECC = 2;
    if (ORIG_REC_FLAG(0) != 0)
        dword_53ECC = 1;
    if (ORIG_REC_FLAG(1) != 0) {
        ORIG_VM_RUN((void *)(uintptr_t)dword_53A45, 9, 0xA0000, 320, 205,
                    76, 74, 19, 1);
        dword_53ECC = 1;
    }
}

/* 0x205BE */
void ev5_205BE(void)
{
    int i;
    dword_53ECC = 2;
    for (i = 0; i < (int)dword_53BEB; i++) {
        uint32_t r = dword_53A45 + REC * (uint32_t)i;
        if (*(uint8_t *)(uintptr_t)(r + 6) == 0 && (*(uint8_t *)(uintptr_t)(r + 5) & 1) == 0)
            dword_53ECC = *(uint8_t *)(uintptr_t)(r + 6);
    }
    if ((*(uint8_t *)(uintptr_t)(dword_53A45 + 5) & 1) != 0)
        dword_53ECC = 1;
}

/* 0x205B4 */
void ev5_205B4(void)
{
    ev5_205BE();
}

/* 0x1F04A - face rec a toward rec b (writes rec[a].+3). */
void ev5_1F04A(int a, int b)
{
    uint8_t *v6 = (uint8_t *)(uintptr_t)(dword_53A45 + REC * (uint32_t)a);
    uint8_t *v7 = (uint8_t *)(uintptr_t)(dword_53A45 + REC * (uint32_t)b);
    int      v8 = v6[0] > v7[0] ? v6[0] - v7[0] : v7[0] - v6[0];
    int      dy = v6[1] > v7[1] ? v6[1] - v7[1] : v7[1] - v6[1];
    if (v8 <= dy)
        v6[3] = (v6[1] <= v7[1]) ? 0 : 2;
    else
        v6[3] = (v6[0] <= v7[0]) ? 3 : 1;
}

/* 0x1F0DC - 1 if rec b is diagonally/facing-adjacent and holds item 1. */
int ev5_1F0DC(int a, int b)
{
    uint8_t *v6 = (uint8_t *)(uintptr_t)(dword_53A45 + REC * (uint32_t)a);
    uint8_t *v7 = (uint8_t *)(uintptr_t)(dword_53A45 + REC * (uint32_t)b);
    int      v9, r;
    if (v7[38] != 0)
        return -1;
    v9 = v6[0] > v7[0] ? v6[0] - v7[0] : v7[0] - v6[0];
    v9 += v6[1] > v7[1] ? v6[1] - v7[1] : v7[1] - v6[1];
    if (v9 != 1)
        return -1;
    r = ORIG_SLOT_FIND(b, 0);
    if (r != -1) {
        int v10 = ORIG_REC_FIELD(b, r);
        uint8_t *t = (uint8_t *)ORIG_TBL_8BC(v10);
        if (t[11] != 1)
            return -1;
        return 1;
    }
    return r;
}

/* 0x1B653 - collect 3-byte records (offset +49) of moveable record type 3. */
void ev5_1B653(uint8_t *out)
{
    int i, v5 = 0;
    for (i = 0; i < (int)dword_53BEB; i++) {
        uint32_t r = dword_53A45 + REC * (uint32_t)i;
        if ((*(uint8_t *)(uintptr_t)(r + 5) & 1) == 0
            && *(uint8_t *)(uintptr_t)(r + 49) == 3
            && *(uint16_t *)(uintptr_t)(r + 64) == 0)
            memmove(out + 3 * v5++, (const void *)(uintptr_t)(r + 49), 3);
    }
}
