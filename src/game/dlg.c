/* dlg.c - FD2 dialogue-box helpers (source translation).
 *
 *   0x16559  dlg_blit_dato    - pure: globals passed in by src/repl.c
 *   0x16E24  dlg_scroll_text  - pure: box position passed in
 *   0x165AC  dlg_open_box     - app-level: original globals + services
 *   0x16B43  dlg_close_box    - app-level
 *   0x168B6  dlg_box_stage    - app-level (reads the frame resource global)
 *   0x1685C  dlg_frame_tile   - pure
 *
 * The app-level half keeps the original addresses: the globals really do
 * live in the game's data segment (all four entries can be hooked without
 * wrapper code because the C reads/writes the same words the machine code
 * does), and a handful of services stay machine code while the port is
 * partial - they are heap-, timing- or BDA-coupled:
 *
 *   0x15E9E  save a VGA rectangle into a CRT block, blit a sprite on it
 *   0x15E71  restore such a block and free it (other callers hand it
 *            buffers from the Watcom CRT heap, so it cannot move to libc
 *            free yet - same reasoning as res.c, see src/repl.c)
 *   0x12CEA  glide the speaker portrait to (x, y)
 *   0x3790A  delay(10)
 *   0x4E381  flush the BIOS keyboard buffer (BDA 0x41A/0x41C)
 *   0x3706E  Watcom CRT malloc - pairs with the free inside 0x15E71
 * 0x4ECBF (gfx_save_rect) and 0x4ED0B (gfx_blit_block) are already
 * translated, so the C calls the translated versions directly.
 *
 * Verified against the machine code by src/boxcheck.c (whole-frame VGA,
 * the five stage snapshots, the global flag and a full event log of every
 * service call - ordering, arguments and VGA at each delay).
 */
#include "dlg.h"
#include "gfx.h"
#include "rle2.h"
#include <string.h>

#define VGA_BASE 0x000A0000u

/* ------------------------------------------------ app-level part ------- */
/* The game's data segment (IDA names kept so the C reads like the original
 * decompilation). Addresses verified against E:\FD2\FD2.EXE.i64. */
#define VGA        ((uint8_t *)(uintptr_t)VGA_BASE)
#define dword_51A83 (*(int32_t *)(uintptr_t)0x00051A83u) /* portrait anim mode */
#define dword_53A18 ((void **)(uintptr_t)0x00053A18u)    /* 5 stage snapshots */
#define dword_53A81 (*(void **)(uintptr_t)0x00053A81u)   /* box frame resource */
#define dword_53AB9 (*(int32_t *)(uintptr_t)0x00053AB9u) /* portrait cols */
#define dword_53ABD (*(int32_t *)(uintptr_t)0x00053ABDu) /* portrait rows */
#define dword_53C67 (*(int32_t *)(uintptr_t)0x00053C67u) /* box position */

typedef void *(*snap_save_fn)(const void *block, void *surface, int stride,
                              int x, int y);
typedef void  (*snap_restore_fn)(void *record, void *surface, int stride);
typedef void  (*delay_fn)(unsigned ms);
typedef void  (*flush_fn)(void);
typedef void  (*glide_fn)(int face_x, int face_y);
typedef void  *(*crt_alloc_fn)(size_t n);

#define ORIG_SNAP_SAVE   ((snap_save_fn)  (uintptr_t)0x00015E9Eu)
#define ORIG_SNAP_RESTORE ((snap_restore_fn)(uintptr_t)0x00015E71u)
#define ORIG_DELAY       ((delay_fn)      (uintptr_t)0x0003790Au)
#define ORIG_FLUSH       ((flush_fn)      (uintptr_t)0x0004E381u)
#define ORIG_GLIDE       ((glide_fn)      (uintptr_t)0x00012CEAu)
#define ORIG_ALLOC       ((crt_alloc_fn)  (uintptr_t)0x0003706Eu)

/* The 310x86 frame drawn by dlg_open_box, as five growing tile stages:
 * (cols, lines) per stage, 16 px cells inside a 3 px border. */
static const uint8_t dlg_stage_cols[5]  = { 4, 8, 12, 16, 19 };
static const uint8_t dlg_stage_lines[5] = { 2, 3, 4, 5, 5 };

/* Sprite block the portrait sweep draws: the frame resource's offset table
 * starts at +6, and the sweep reads entry 0 as a sign-extended word. */
static const uint8_t *dlg_face_block(const uint8_t *frame)
{
    return frame + *(const int16_t *)(frame + 6);
}

/* 0x165AC */
void *dlg_open_box(int face_x, int face_y, int rows)
{
    int vw, vh, steps, i, off;

    if (rows != 0) {
        dword_51A83 = 0;                       /* fast: no per-step wait   */
        ORIG_GLIDE(face_x, face_y);
        dword_51A83 = 1;

        vw    = 24 * dword_53AB9 + 4;
        vh    = 24 * dword_53ABD + 4;
        steps = dword_53AB9 + dword_53ABD;
        if (steps != 0) {
            const uint8_t *face = dlg_face_block(dword_53A81);

            for (i = 0; i <= steps; i++) {
                dword_53A18[0] = ORIG_SNAP_SAVE(face, VGA, 320,
                                                vw - i * (vw - 5) / steps,
                                                vh - i * (vh - rows) / steps);
                ORIG_DELAY(10);
                ORIG_FLUSH();
                ORIG_SNAP_RESTORE(dword_53A18[0], VGA, 320);
            }
        }
    } else if (dword_53C67 == 0x728) {
        rows = 2;
    } else if (dword_53C67 == 0x9017) {
        rows = 112;
    }

    for (i = 0; i < 5; i++)
        dword_53A18[i] = ORIG_ALLOC(26668);

    off = 320 * rows + 5;
    for (i = 0; i < 5; i++) {
        gfx_save_rect(dword_53A18[i], 310, 86, VGA, off, 320);
        dlg_box_stage(VGA, 320, 5, rows, dlg_stage_cols[i],
                      dlg_stage_lines[i]);
        if (i < 4)
            ORIG_DELAY(10);
    }
    ORIG_FLUSH();
    return dword_53A18;
}

/* 0x16B43 */
void dlg_close_box(void **stages, int rows)
{
    int i;

    for (i = 4; i > 0; i--) {                 /* reverse of the open order */
        ORIG_SNAP_RESTORE(stages[i], VGA, 320);
        ORIG_DELAY(10);
    }
    ORIG_SNAP_RESTORE(stages[0], VGA, 320);

    if (rows != 0) {
        int vw    = 24 * dword_53AB9 + 4;
        int vh    = 24 * dword_53ABD + 4;
        int steps = dword_53AB9 + dword_53ABD;

        if (steps != 0) {
            const uint8_t *face = dlg_face_block(dword_53A81);

            for (i = 0; i <= steps; i++) {
                stages[0] = ORIG_SNAP_SAVE(face, VGA, 320,
                                           5 - i * (5 - vw) / steps,
                                           rows - i * (rows - vh) / steps);
                ORIG_DELAY(10);
                ORIG_SNAP_RESTORE(stages[0], VGA, 320);
            }
        }
    }
}

/* 0x168B6 - one stage of the box frame: 16 px tile grid `cols` x `lines`,
 * top border on row y0, left border at column x0, in a 3 px frame. */
void dlg_box_stage(void *surface, int stride, int x0, int y0,
                   int cols, int lines)
{
    const void  *table = dword_53A81;
    uint8_t     *base  = (uint8_t *)surface + stride * y0 + x0;
    uint8_t     *bottom = base + 3 * stride + lines * 16 * stride;
    uint8_t     *right  = base + 3 * stride + 16 * cols + 3;
    int          band   = (lines - 1) * 16 * stride;
    int          i, j, k, m;

    dlg_frame_tile(base, stride, table, 1);                 /* corners    */
    dlg_frame_tile(base + 16 * cols + 3, stride, table, 2);
    dlg_frame_tile(bottom, stride, table, 3);
    dlg_frame_tile(bottom + 16 * cols + 3, stride, table, 4);
    dlg_frame_tile(base + 3, stride, table, 5);             /* edges      */
    dlg_frame_tile(base + 19 + 16 * (cols - 2), stride, table, 6);
    dlg_frame_tile(bottom + 3, stride, table, 7);
    dlg_frame_tile(bottom + 19 + 16 * (cols - 2), stride, table, 8);
    dlg_frame_tile(base + 3 * stride, stride, table, 14);
    dlg_frame_tile(right, stride, table, 15);
    dlg_frame_tile(base + 3 * stride + band, stride, table, 16);
    dlg_frame_tile(right + band, stride, table, 17);

    for (i = 0; i < cols - 2; i++) {           /* top / bottom fill         */
        dlg_frame_tile(base + 16 * i + 19, stride, table, 9);
        dlg_frame_tile(bottom + 16 * i + 19, stride, table, 12);
    }
    for (j = 0; j < lines - 2; j++) {          /* left / right fill         */
        uint8_t *mid = base + 3 * stride + (j + 1) * 16 * stride;
        dlg_frame_tile(mid, stride, table, 10);
        dlg_frame_tile(mid + 16 * cols + 3, stride, table, 11);
    }
    for (k = 0; k < lines; k++)                /* interior                  */
        for (m = 0; m < cols; m++)
            dlg_frame_tile(base + 16 * m + 3 * stride + 3 + k * 16 * stride,
                           stride, table, 13);
}

/* 0x1685C */
void dlg_frame_tile(void *dest, int stride, const void *table, int idx)
{
    const uint8_t *base = (const uint8_t *)table;

    gfx_blit_block(dest, base + *(const uint32_t *)(base + 4 * idx + 6),
                   stride);
}

void dlg_blit_dato(const void *dato_buf, int box_pos, int idx)
{
    const uint8_t *src = (const uint8_t *)dato_buf +
                         *(const uint32_t *)((const char *)dato_buf + 4 * idx);
    uint8_t       *dst = (uint8_t *)(uintptr_t)(VGA_BASE + (uint32_t)box_pos);

    if (box_pos == 0x9017)
        rle2_blit_mirror(dst, src, 320);   /* bottom box is mirrored */
    else
        rle2_blit(dst, src, 320);
}

void dlg_scroll_text(int box_pos)
{
    uint8_t *base;
    int      i, j, k;

    if (box_pos != 0x728 && box_pos != 0x9017)
        return;

    base = (uint8_t *)(uintptr_t)(uint32_t)((box_pos == 0x728) ? 0xA0B4Fu
                                                                 : 0xA951Fu);

    for (i = 0; i < 5; i++) {
        for (j = 0; j < 72; j++)
            memmove(base + 320 * j - 1, base + 320 * (j + 3) - 1, 208);
        memset(base + 23040, 0x4A, 208);
    }
    for (k = 0; k < 72; k++)
        memmove(base + 320 * k - 1, base + 320 * (k + 4) - 1, 208);
    memset(base + 23040, 0x4A, 208);
}
