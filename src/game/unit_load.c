/* unit_load.c - FD2 unit sprite builder (source translation of 0x10B4E,
 * 0x10C50, 0x11019, 0x145CD, 0x14625, 0x1B750, 0x32999).
 *
 * See unit_load.h for the contract.  Two rules matter here:
 *
 *  - File handles never cross the C/machine boundary as host `FILE *`.  The
 *    FDICON.B24 handle is created by 0x10B4E and handed to 0x10C50/0x11019, and
 *    0x11019 still has machine-code callers, so every file call goes through
 *    the *game's* Watcom CRT entry points (0x37324 fopen / 0x37940 fseek /
 *    0x373CA fread / 0x377A3 fwrite / 0x3759C fclose).  The host then sees a
 *    Watcom FILE, and a check harness that redirects those entries to the host
 *    libc keeps working unchanged.
 *  - The two buffers that outlive a call - the FDFIELD.DAT image in
 *    dword_53A59 and the FD2.TMP atlas in dword_53A61 - use guest_mem.h.
 *
 * Verified against the machine code by src/ev6check.c.
 */
#include "unit_load.h"
#include "guest_mem.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- game CRT entry points --------------------------------------------- */
typedef void *(*fopen_fn)(const char *name, const char *mode);
typedef int   (*fseek_fn)(void *fp, long off, int whence);
typedef unsigned (*fread_fn)(void *buf, unsigned sz, unsigned n, void *fp);
typedef unsigned (*fwrite_fn)(const void *buf, unsigned sz, unsigned n, void *fp);
typedef int   (*fclose_fn)(void *fp);

#define ORIG_FOPEN  ((fopen_fn) (uintptr_t)0x00037324u)
#define ORIG_FSEEK  ((fseek_fn) (uintptr_t)0x00037940u)
#define ORIG_FREAD  ((fread_fn) (uintptr_t)0x000373CAu)
#define ORIG_FWRITE ((fwrite_fn)(uintptr_t)0x000377A3u)
#define ORIG_FCLOSE ((fclose_fn)(uintptr_t)0x0003759Cu)

/* --- services that stay in machine code / other modules ---------------- */
typedef void *(*res_load_fn)(const char *f, void *old, int idx);   /* 0x111BA */
typedef unsigned char (*fix_records_fn)(void *header);             /* 0x4DF4C */
typedef void *(*tbl_fn)(int index);                                /* 0x4E821/838/84F/8BC */
typedef void (*rle2_trans_fn)(void *dst, const void *src, int len);/* 0x4EBAB */
typedef void (*blit_rows_fn)(void *dst, int dp, const void *src, int sp,
                             int len, int rows);                   /* 0x11EB0 */
typedef void (*map_view_fn)(void *dst, int pitch, int w, int h,
                            int ox, int oy);                       /* 0x11EEE */
typedef void (*i1_fn)(int);                                        /* 1-arg void */
typedef void (*v0_fn)(void);                                       /* 0-arg void */
typedef int  (*sfx_fn)(const void *bank, int index, int loops);    /* 0x25A96 */
typedef int  (*wait_fn)(int ticks);                                /* 0x17AA9 */
typedef void (*mark_cell_fn)(int x, int y);                        /* 0x146A7 */

#define ORIG_RES_LOAD   ((res_load_fn)   (uintptr_t)0x000111BAu)
#define ORIG_FIX_REC    ((fix_records_fn)(uintptr_t)0x0004DF4Cu)
#define ORIG_T_4E821    ((tbl_fn)        (uintptr_t)0x0004E821u)
#define ORIG_T_4E838    ((tbl_fn)        (uintptr_t)0x0004E838u)
#define ORIG_T_4E84F    ((tbl_fn)        (uintptr_t)0x0004E84Fu)
#define ORIG_T_4E8BC    ((tbl_fn)        (uintptr_t)0x0004E8BCu)
#define ORIG_RLE2_TRANS ((rle2_trans_fn) (uintptr_t)0x0004EBABu)
#define ORIG_BLIT_ROWS  ((blit_rows_fn)  (uintptr_t)0x00011EB0u)
#define ORIG_MAP_VIEW   ((map_view_fn)   (uintptr_t)0x00011EEEu)
#define ORIG_127E0      ((i1_fn)         (uintptr_t)0x000127E0u)
#define ORIG_129EC      ((i1_fn)         (uintptr_t)0x000129ECu)
#define ORIG_127A9      ((v0_fn)         (uintptr_t)0x000127A9u)
#define ORIG_FLUSH      ((v0_fn)         (uintptr_t)0x0004E381u)
#define ORIG_SFX        ((sfx_fn)        (uintptr_t)0x00025A96u)
#define ORIG_WAIT       ((wait_fn)       (uintptr_t)0x00017AA9u)
#define ORIG_MARK_CELL  ((mark_cell_fn)  (uintptr_t)0x000146A7u)

/* --- game data-segment globals (original addresses) -------------------- */
#define dword_53A45 (*(uint32_t *)(uintptr_t)0x00053A45u)  /* 80-byte records */
#define dword_53A49 (*(uint32_t *)(uintptr_t)0x00053A49u)  /* 153216 fb base  */
#define dword_53A51 (*(uint32_t *)(uintptr_t)0x00053A51u)  /* map cells       */
#define dword_53A55 (*(uint32_t *)(uintptr_t)0x00053A55u)  /* 26-byte states  */
#define dword_53A59 (*(uint32_t *)(uintptr_t)0x00053A59u)  /* FDFIELD.DAT     */
#define dword_53A61 (*(uint32_t *)(uintptr_t)0x00053A61u)  /* FD2.TMP atlas   */
#define dword_539EC (*(uint32_t *)(uintptr_t)0x000539ECu)  /* atlas write off */
#define dword_53B17 ((uint32_t *)(uintptr_t)0x00053B17u)   /* atlas slots     */
#define dword_53BDF (*(uint32_t *)(uintptr_t)0x00053BDFu)  /* atlas slot count*/
#define dword_53BEB (*(uint32_t *)(uintptr_t)0x00053BEBu)  /* record count    */
#define dword_53BE3 (*(uint32_t *)(uintptr_t)0x00053BE3u)  /* state count     */
#define dword_53C03 (*(uint32_t *)(uintptr_t)0x00053C03u)  /* chapter index   */
#define dword_53AC1 (*(uint32_t *)(uintptr_t)0x00053AC1u)  /* map width       */
#define dword_53AC5 (*(uint32_t *)(uintptr_t)0x00053AC5u)  /* map height      */
#define dword_53AA9 (*(uint32_t *)(uintptr_t)0x00053AA9u)  /* view origin x   */
#define dword_53AAD (*(uint32_t *)(uintptr_t)0x00053AADu)  /* view origin y   */
#define dword_53AD5 (*(uint32_t *)(uintptr_t)0x00053AD5u)  /* status block    */
#define dword_53BEF (*(uint32_t *)(uintptr_t)0x00053BEFu)
#define dword_53EC8 (*(uint32_t *)(uintptr_t)0x00053EC8u)
#define dword_51A83 (*(uint32_t *)(uintptr_t)0x00051A83u)
#define dword_51A87 (*(uint32_t *)(uintptr_t)0x00051A87u)
#define dword_51A8B (*(uint32_t *)(uintptr_t)0x00051A8Bu)
#define byte_53AFA  (*(uint8_t  *)(uintptr_t)0x00053AFAu)

#define FB_SIZE     153216u
#define REC         0x50
#define ATLAS_SIZE  0x32A00

/* 0x4E838 / 0x4E821 / 0x4E84F / 0x4E8BC return guest data pointers. */
static const uint8_t *tbl_4E838(int i) { return (const uint8_t *)ORIG_T_4E838(i); }
static const uint8_t *tbl_4E821(int i) { return (const uint8_t *)ORIG_T_4E821(i); }
static const uint8_t *tbl_4E84F(int i) { return (const uint8_t *)ORIG_T_4E84F(i); }
static const uint8_t *tbl_4E8BC(int i) { return (const uint8_t *)ORIG_T_4E8BC(i); }

/* 0x145CD - flip the reveal bit of every record on one side of dword_53BEF. */
void unit_mark_nearby(int mode)
{
    uint8_t *p = (uint8_t *)(uintptr_t)dword_53A45;
    int i;

    for (i = 0; i < (int)dword_53BEB; i++, p += REC) {
        if ((p[5] & 1) == 0 &&
            ((mode == 0 && p[6] != 0) || (mode != 0 && p[6] == 0)))
            unit_reveal_around(p[0], p[1]);
    }
}

/* 0x14625 - reveal (x, y) and its four orthogonal neighbours. */
void unit_reveal_around(int x, int y)
{
    if (x != 0)
        ORIG_MARK_CELL(x - 1, y);
    if (y != 0)
        ORIG_MARK_CELL(x, y - 1);
    if (x < (int)dword_53AC1 - 1)
        ORIG_MARK_CELL(x + 1, y);
    if (y < (int)dword_53AC5 - 1)
        ORIG_MARK_CELL(x, y + 1);
    *(uint8_t *)(uintptr_t)(dword_53A51 + 4 * (x + (int)dword_53AC1 * y) + 6) |= 0x40;
}

/* 0x1B750 - fold the eight equipment slots into the record's bounding box. */
void unit_metrics(int idx)
{
    uint8_t *rec = (uint8_t *)(uintptr_t)(dword_53A45 + REC * (uint32_t)idx);
    int x = (int)(int16_t)*(uint16_t *)(rec + 55);
    int y = (int)(int16_t)*(uint16_t *)(rec + 57);
    int w = (int)(int16_t)*(uint16_t *)(rec + 62);
    int h;
    int i;

    if (rec[36] != 0)
        w += 15;
    h = w;
    for (i = 0; i < 8; i++) {
        const uint8_t *e;
        if ((rec[2 * i + 10] & 0x40) == 0)
            continue;
        e = tbl_4E8BC(rec[2 * i + 11]);
        x += (int)(int16_t)*(uint16_t *)(e + 1);
        y += (int)(int16_t)*(uint16_t *)(e + 5);
        h += (int)(int16_t)*(uint16_t *)(e + 3);
        w += (int)(int16_t)*(uint16_t *)(e + 7);
    }
    if (rec[34] != 0)
        x = (int)((double)x * 1.15);   /* Watcom fild/fmul/__CHP/fistp */
    if (rec[35] != 0)
        y = (int)((double)y * 1.15);
    *(uint16_t *)(rec + 72) = (uint16_t)x;
    *(uint16_t *)(rec + 74) = (uint16_t)y;
    *(uint16_t *)(rec + 76) = (uint16_t)h;
    *(uint16_t *)(rec + 78) = (uint16_t)w;
}

/* 0x11019 - append FDICON.B24 entry `idx` to the atlas; return its slot. */
int unit_entry_data(int idx, void *fh)
{
    uint32_t tbl[13];
    uint8_t *buf = (uint8_t *)guest_malloc(6720);
    int      len, i;

    ORIG_FSEEK(fh, 6, 0);
    ORIG_FREAD(buf, 1, 6720, fh);
    for (i = 0; i < 13; i++)
        tbl[i] = *(uint32_t *)(void *)(buf + 48 * idx + 4 * i);
    len = (int)(tbl[12] - tbl[0]);
    guest_free(buf);

    if (dword_53BDF != 0) {
        uint32_t j;
        for (j = 0; j < dword_53BDF; j++)
            if ((int)dword_53B17[j] == idx)
                return (int)j;
        dword_53B17[j] = (uint32_t)idx;
        ORIG_FSEEK(fh, (long)tbl[0], 0);
        ORIG_FREAD((uint8_t *)(uintptr_t)(dword_53A61 + dword_539EC),
                   1, (unsigned)len, fh);
        for (i = 0; i < 12; i++)
            *(uint32_t *)(uintptr_t)(dword_53A61 + 4 * (i + 12 * dword_53BDF)) =
                tbl[i] - tbl[0] + dword_539EC;
        dword_539EC += (uint32_t)len;
        return (int)dword_53BDF++;
    }

    dword_53B17[0] = (uint32_t)idx;
    dword_53A61 = (uint32_t)(uintptr_t)guest_malloc(ATLAS_SIZE);
    ORIG_FSEEK(fh, (long)tbl[0], 0);
    ORIG_FREAD((uint8_t *)(uintptr_t)(dword_53A61 + 1920), 1, (unsigned)len, fh);
    for (i = 0; i < 12; i++)
        *(uint32_t *)(uintptr_t)(dword_53A61 + 4 * i) =
            tbl[i] - tbl[0] + 1920;
    dword_53BDF++;
    dword_539EC = (uint32_t)len + 1920;
    return 0;
}

/* 0x10C50 - build one 80-byte record from FDFIELD.DAT entry `idx`. */
void unit_entry_build(int idx, void *fh)
{
    uint8_t *rec = (uint8_t *)(uintptr_t)(dword_53A45 + REC * dword_53BEB);
    const uint8_t *field = (const uint8_t *)(uintptr_t)dword_53A59;
    const uint8_t *st    = (const uint8_t *)(uintptr_t)(dword_53A55 + 26 * (uint32_t)idx + 131);
    int fx = field[6 * idx + 2];
    int fy = field[6 * idx + 4];
    int px, py, best = 255;
    int v25, v24, v28;
    int e27, e26;
    int i, j, k;

    unit_mark_nearby(0);
    unit_mark_nearby(1);

    if (byte_53AFA != 0) {
        px = fx;
        py = fy;
    } else {
        px = 0;
        py = 0;
        for (i = 0; i < (int)dword_53AC5; i++) {
            for (j = 0; j < (int)dword_53AC1; j++) {
                int d;
                if ((*(uint8_t *)(uintptr_t)(dword_53A51 + 4 * (j + i * (int)dword_53AC1) + 6) & 0x40) != 0)
                    continue;
                d = abs(j - fx) + abs(i - fy);
                if (d <= best) {
                    best = d;
                    px = j;
                    py = i;
                }
            }
        }
    }

    ORIG_FIX_REC((void *)(uintptr_t)dword_53A51);

    v25 = st[0];
    v24 = st[1];
    v28 = st[4];

    if (v24 < 0x44) {
        const uint8_t *p1 = tbl_4E838(v24);
        const uint8_t *p2 = tbl_4E821(v24);
        int a = *(uint16_t *)(p1 + 0x12);
        int b = *(uint16_t *)(p1 + 0x14);
        int c = *(uint16_t *)(p1 + 0x16);
        e27 = (v28 - 1) * (int)p2[6] + (int)*(uint16_t *)(p1 + 3);
        e26 = (v28 - 1) * (int)p2[8] + (int)*(uint16_t *)(p1 + 5);
        rec[0x1F] = p1[0];
        rec[0x20] = p1[1];
        *(uint16_t *)(rec + 0x37) = (uint16_t)(a + v28 * p2[0]);
        *(uint16_t *)(rec + 0x39) = (uint16_t)(b + v28 * p2[2]);
        rec[0x3B] = p1[7];
        *(uint16_t *)(rec + 0x3E) = (uint16_t)(c + v28 * p2[4]);
    } else {
        const uint8_t *p = tbl_4E84F(v24 - 0x44);
        e27 = (int)*(uint16_t *)(p + 2) * v28;
        e26 = (int)p[4] * v28;
        rec[0x1F] = p[0];
        rec[0x20] = p[1];
        *(uint16_t *)(rec + 0x37) = (uint16_t)(p[5] * v28);
        *(uint16_t *)(rec + 0x39) = (uint16_t)(p[6] * v28);
        *(uint16_t *)(rec + 0x3E) = (uint16_t)(p[7] * v28);
        rec[0x3B] = p[8];
    }

    rec[0] = (uint8_t)px;
    rec[1] = (uint8_t)py;
    rec[2] = (uint8_t)unit_entry_data(v24, fh);
    rec[3] = rec[4] = rec[5] = 0;
    rec[6] = (uint8_t)v25;
    rec[7] = (uint8_t)v24;
    rec[8] = (uint8_t)v24;
    rec[9] = 0;
    if (st[5] == 0xFF) {
        rec[10] = 0x40;
        rec[11] = st[6];
        rec[12] = 0x80;
    } else {
        rec[10] = 0x40;
        rec[11] = st[5];
        rec[12] = 0x40;
        rec[13] = st[6];
    }
    for (k = 0; k < 6; k++) {
        rec[2 * k + 14] = (st[k + 7] == 0xFF) ? 0x80 : 0;
        rec[2 * k + 15] = st[k + 7];
    }
    memset(rec + 34, 0, 6);
    memmove(rec + 26, st + 13, 4);
    rec[30] = 0;
    rec[33] = (uint8_t)v28;
    rec[49] = st[22];
    *(uint16_t *)(rec + 50) = *(uint16_t *)(st + 23);
    rec[52] = st[17];
    rec[53] = st[18];
    rec[54] = st[19];
    rec[61] = st[2];
    rec[60] = (rec[6] == 2) ? 0 : 0xFF;
    *(uint16_t *)(rec + 64) = (uint16_t)e27;
    *(uint16_t *)(rec + 66) = (uint16_t)e27;
    *(uint16_t *)(rec + 68) = (uint16_t)e26;
    *(uint16_t *)(rec + 70) = (uint16_t)e26;
    unit_metrics((int)dword_53BEB);
    dword_53BEB++;
}

/* 0x10B4E - rebuild every matching record, then rewrite FD2.TMP. */
int unit_sprites_build(int idx)
{
    void *fh;
    uint32_t i;
    void  *tmp;

    fh = ORIG_FOPEN("FDICON.B24", "rb");
    if (fh == NULL) {
        printf("File 'FDICON.B24' error !!\n");
        exit(1);
    }
    dword_53A59 = (uint32_t)(uintptr_t)
        ORIG_RES_LOAD("FDFIELD.DAT", (void *)(uintptr_t)dword_53A59,
                      3 * (int)dword_53C03 + 2);
    for (i = 0; i < dword_53BE3; i++) {
        if (*(uint8_t *)(uintptr_t)(dword_53A55 + 26 * i + 152) == (uint8_t)idx)
            unit_entry_build((int)i, fh);
    }
    ORIG_FCLOSE(fh);
    guest_free((void *)(uintptr_t)dword_53A59);
    dword_53A59 = 0;

    tmp = ORIG_FOPEN("FD2.TMP", "wb");
    ORIG_FWRITE((const void *)(uintptr_t)dword_53A61, 1, ATLAS_SIZE, tmp);
    return ORIG_FCLOSE(tmp);
}

/* 0x32999 - the 12-step unit reveal cut-scene. */
void unit_map_render(int idx)
{
    uint8_t *v25 = (uint8_t *)ORIG_RES_LOAD("FDOTHER.DAT", NULL, 95);
    uint8_t *v26 = (uint8_t *)ORIG_RES_LOAD("FDOTHER.DAT", NULL, 9);
    uint8_t *snap = (uint8_t *)guest_malloc(FB_SIZE);
    uint32_t first = dword_53BEB;
    int step;

    memmove(snap, (void *)(uintptr_t)dword_53A49, FB_SIZE);
    unit_sprites_build((int)idx);

    for (step = 0; step < 12; step++) {
        const uint8_t *src;
        uint32_t r;

        if (step == 1)
            ORIG_SFX(v25, 0, 1);
        memmove((void *)(uintptr_t)dword_53A49, snap, FB_SIZE);

        src = v26 + *(uint32_t *)(v26 + 4 * step + 6);
        for (r = first; r < dword_53BEB; r++) {
            uint8_t *rec = (uint8_t *)(uintptr_t)(dword_53A45 + REC * r);
            int x = rec[0], y = rec[1];
            if (x >= (int)dword_53AA9 - 1 && x <= (int)(dword_53AA9 + dword_51A87) &&
                y >= (int)dword_53AAD && y <= (int)(dword_53AAD + dword_51A8B + 1)) {
                uint8_t *dst = (uint8_t *)(uintptr_t)
                    (dword_53A49 + 32904 + 24 * (x - (int)dword_53AA9 - 1) +
                     10944 * (y - (int)dword_53AAD) - 2736);
                ORIG_RLE2_TRANS(dst, src, 456);
            }
        }
        ORIG_BLIT_ROWS((void *)(uintptr_t)0xA0504u, 320,
                       (const void *)(uintptr_t)(dword_53A49 + 32904),
                       456, 312, 192);

        if (step == 6) {
            memmove((void *)(uintptr_t)dword_53A49, snap, FB_SIZE);
            for (r = 0; r < first; r++)
                if ((*(uint8_t *)(uintptr_t)(dword_53A45 + REC * r + 5) & 1) == 0)
                    ORIG_127E0((int)r);
            dword_53A49 -= 3648;
            for (; r < dword_53BEB; r++)
                if ((*(uint8_t *)(uintptr_t)(dword_53A45 + REC * r + 5) & 1) == 0)
                    ORIG_127E0((int)r);
            dword_53A49 += 3648;
            ORIG_129EC(0);
            memmove(snap, (void *)(uintptr_t)dword_53A49, FB_SIZE);
        } else if (step == 7) {
            ORIG_MAP_VIEW((void *)(uintptr_t)(dword_53A49 + 32904), 456, 13, 8,
                          (int)dword_53AA9, (int)dword_53AAD);
            for (r = 0; r < first; r++)
                if ((*(uint8_t *)(uintptr_t)(dword_53A45 + REC * r + 5) & 1) == 0)
                    ORIG_127E0((int)r);
            dword_53A49 -= 2280;
            for (; r < dword_53BEB; r++)
                if ((*(uint8_t *)(uintptr_t)(dword_53A45 + REC * r + 5) & 1) == 0)
                    ORIG_127E0((int)r);
            dword_53A49 += 2280;
            ORIG_129EC(0);
            memmove(snap, (void *)(uintptr_t)dword_53A49, FB_SIZE);
        } else if (step == 8) {
            ORIG_MAP_VIEW((void *)(uintptr_t)(dword_53A49 + 32904), 456, 13, 8,
                          (int)dword_53AA9, (int)dword_53AAD);
            ORIG_127A9();
            memmove(snap, (void *)(uintptr_t)dword_53A49, FB_SIZE);
        }

        ORIG_FLUSH();
        ORIG_WAIT(1);
    }

    guest_free(v26);
    guest_free(snap);
    guest_free(v25);
}
