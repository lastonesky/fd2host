/* map.c - map view / terrain tile rendering (source translation of 0x126F7).
 *
 * The map bitmap lives at *(0x53A49) (a 320x200-ish surface, drawn with
 * stride 456 into a 0x8088 base offset), the tileset resource at *(0x53A4D)
 * (its sub-image offset table starts at +6), and the visible window is
 * (0x53AA9,0x53AAD) + (0x51A87,0x51A8B) cells. One cell is one 24x24 tile.
 *
 * The original bounds-checks the cell against the view and then runs the
 * already-verified sprite24_plain() on it. Layout:
 *
 *     dst = bitmap + (y - oy) * 10944 + (x - ox) * 24 + 32904
 *     src = tileset + *(u32 *)(tileset + 4*index + 6)
 *
 * (10944 = 456 * 24, i.e. 24 rows of the 456-stride bitmap.)
 */
#include "map.h"
#include "sprite24.h"
#include "rec.h"
#include "gfx.h"
#include "rle.h"
#include "dlg.h"
#include "anim.h"
#include "fade.h"
#include "guest_mem.h"
#include "../dos.h"

#include <stdint.h>
#include <string.h>

#define dword_53A49 (*(uint8_t **)(uintptr_t)0x00053A49u) /* map bitmap     */
#define dword_53A4D (*(uint8_t **)(uintptr_t)0x00053A4Du) /* tileset        */
#define dword_51A87 (*(int32_t  *)(uintptr_t)0x00051A87u) /* view cols      */
#define dword_51A8B (*(int32_t  *)(uintptr_t)0x00051A8Bu) /* view rows      */
#define dword_53AA9 (*(int32_t  *)(uintptr_t)0x00053AA9u) /* view x origin  */
#define dword_53AAD (*(int32_t  *)(uintptr_t)0x00053AADu) /* view y origin  */
#define dword_53A51 (*(uint8_t **)(uintptr_t)0x00053A51u) /* cell table    */
#define dword_53A69 (*(uint32_t  *)(uintptr_t)0x00053A69u) /* cell -> 4 bytes */
#define dword_53AC1 (*(int32_t   *)(uintptr_t)0x00053AC1u) /* map width      */
#define dword_53A5D (*(uint8_t  **)(uintptr_t)0x00053A5Du) /* sprite bank    */
#define dword_53A6D (*(uint8_t  **)(uintptr_t)0x00053A6Du) /* palette bank   */
#define dword_53A40 (*(int32_t   *)(uintptr_t)0x00053A40u) /* sprite +offset */
#define dword_53C1F (*(int32_t   *)(uintptr_t)0x00053C1Fu)
#define dword_53BEB (*(int32_t   *)(uintptr_t)0x00053BEBu) /* record count   */
#define dword_53A45 (*(uint8_t  **)(uintptr_t)0x00053A45u) /* record table   */
#define dword_53A61 (*(uint8_t  **)(uintptr_t)0x00053A61u) /* icon bank      */
#define byte_51A97  ((const uint8_t *)(uintptr_t)0x00051A97u)
#define byte_54132  (*(uint8_t  *)(uintptr_t)0x00054132u) /* ping phase    */
#define byte_52725  ((const uint8_t *)(uintptr_t)0x00052725u) /* unit 29-tbl */
#define dword_53EEC (*(void    **)(uintptr_t)0x00053EECu) /* SFX bank       */

/* --- map view rendering core (round 37) ------------------------------- */
#define dword_53A00 (*(int32_t  *)(uintptr_t)0x00053A00u) /* last flip tick */
#define dword_539F8 (*(int32_t  *)(uintptr_t)0x000539F8u) /* last scroll tick*/
#define dword_539FC (*(int32_t  *)(uintptr_t)0x000539FCu) /* scanline phase */
#define dword_539F4 (*(int32_t  *)(uintptr_t)0x000539F4u) /* last cursor tick*/
#define dword_53AF1 (*(int32_t  *)(uintptr_t)0x00053AF1u) /* view tile y    */
#define dword_53AED (*(int32_t  *)(uintptr_t)0x00053AEDu) /* view base      */
#define dword_53AF5 (*(int32_t  *)(uintptr_t)0x00053AF5u) /* view base      */
#define dword_53AFF (*(uint32_t *)(uintptr_t)0x00053AFFu) /* screen buffer  */
#define dword_53B03 (*(uint32_t *)(uintptr_t)0x00053B03u) /* expanded scan  */
#define dword_53B07 (*(int32_t  *)(uintptr_t)0x00053B07u)
#define dword_53B0B (*(int32_t  *)(uintptr_t)0x00053B0Bu)
#define dword_53C03 (*(int32_t  *)(uintptr_t)0x00053C03u) /* view mode      */
#define dword_53C0B (*(int32_t  *)(uintptr_t)0x00053C0Bu) /* anim phase     */
#define dword_51A93 (*(int32_t  *)(uintptr_t)0x00051A93u) /* cursor phase   */
#define dword_51A83 (*(int32_t  *)(uintptr_t)0x00051A83u) /* reveal mode    */
#define dword_53AB1 (*(int32_t  *)(uintptr_t)0x00053AB1u) /* cursor x       */
#define dword_53AB5 (*(int32_t  *)(uintptr_t)0x00053AB5u) /* cursor y       */
#define byte_51A10  (*(uint8_t   *)(uintptr_t)0x00051A10u) /* scroll lines   */
#define byte_51AAB  (*(uint8_t   *)(uintptr_t)0x00051AABu) /* cursor on      */
#define byte_51AAC  (*(uint8_t   *)(uintptr_t)0x00051AACu) /* cursor on      */
#define dword_53ABD (*(int32_t  *)(uintptr_t)0x00053ABDu)
#define dword_53AB9 (*(int32_t  *)(uintptr_t)0x00053AB9u)
#define dword_51A0C (*(int32_t  *)(uintptr_t)0x00051A0Cu) /* cursor top     */
#define dword_53A81 (*(uint8_t  **)(uintptr_t)0x00053A81u) /* icon resource  */
#define dword_51A12 ((const int32_t *)(uintptr_t)0x00051A12u)
#define dword_51A2A ((const int32_t *)(uintptr_t)0x00051A2Au)
#define BDA_W(off)  (*(volatile uint16_t *)(uintptr_t)(DOS_LOWMEM_BASE + (off)))

void map_blit_tile(int x, int y, int index)
{
    const uint8_t *src;
    uint8_t       *dst;

    if (x < dword_53AA9 || dword_53AA9 + dword_51A87 <= x)
        return;
    if (y < dword_53AAD || dword_53AAD + dword_51A8B <= y)
        return;

    src = dword_53A4D + *(const uint32_t *)(dword_53A4D + 4 * index + 6);
    dst = dword_53A49 + (y - dword_53AAD) * 10944 + (x - dword_53AA9) * 24
        + 32904;
    sprite24_plain(src, dst, 456);
}

/* 0x12E38 - read one map cell: out[0..1] = terrain tile (14 bits),
 * out[2..3] = flags (0x1F), out[4..7] = the 4 bytes of dword_53A69[tile]. */
void map_cell_info(int x, int y, uint8_t *out)
{
    const uint8_t *cell = dword_53A51 + 4 * (x + dword_53AC1 * y);
    uint16_t tile  = *(const uint16_t *)(cell + 4) & 0x03FFu;
    uint16_t flags = (uint16_t)(cell[6] & 0x1F);
    const uint8_t *t = (const uint8_t *)(uintptr_t)(dword_53A69 + 4 * tile);

    *(uint16_t *)out     = tile;
    *(uint16_t *)(out + 2) = flags;
    out[4] = t[0]; out[5] = t[1]; out[6] = t[2]; out[7] = t[3];
}

/* 0x12AC6 - blit the object sprite of map cell (x, y) onto `dst`.
 *
 * Cell tile -> dword_53A69[4*tile]: bit 3 adds 2*dword_53A40 to the index,
 * bit 7 means "there is a sprite". The sprite comes from the bank
 * *(0x53A5D) (offset table at +0x0A) and is drawn plain, or through the
 * palette *(0x53A6D)[byte_51A97[dword_53C1F]] (offset table at +6) when the
 * cell's byte +7 is not 0xFF. */
void map_blit_cell_sprite(void *dst, int x, int y)
{
    uint8_t *cell;
    const uint8_t *src, *pal;
    uint8_t *d;
    uint16_t tile;
    uint8_t  v;

    if (dword_53AA9 - 1 > x || dword_53AA9 + dword_51A87 < x)
        return;
    if (dword_53AAD - 1 > y || dword_53AAD + dword_51A8B + 1 < y)
        return;
    if (y < 0)
        return;

    cell = dword_53A51 + 4 * (x + dword_53AC1 * y);
    tile = *(const uint16_t *)(cell + 4) & 0x03FFu;
    v    = *(const uint8_t *)(uintptr_t)(dword_53A69 + 4 * tile);
    if (v & 8)
        tile = (uint16_t)(tile + 2 * dword_53A40);
    if (!(v & 0x80))
        return;

    src = dword_53A5D + *(const uint32_t *)(dword_53A5D + 4 * tile + 0x0A);
    pal = dword_53A6D
        + *(const uint32_t *)(dword_53A6D + 4 * byte_51A97[dword_53C1F] + 6);
    d   = (uint8_t *)dst + 0x8088 + 10944 * (y - dword_53AAD)
        + 24 * (x - dword_53AA9);

    if (cell[7] == 0xFF)
        sprite24_plain(src, d, 456);
    else
        sprite24_pal(src, d, 456, pal);
}

/* 0x129EC - refresh the cell sprites under every drawable record: the cell
 * the record occupies and the one above, plus extra cells chosen by the
 * record's direction byte (0 = below, 1 = left pair, 2 = two above,
 * other = right pair) when record[4] is set. */
void map_refresh_records(void)
{
    int i;

    for (i = 0; i < dword_53BEB; i++) {
        const uint8_t *p = (const uint8_t *)(uintptr_t)dword_53A45 + 80 * i;
        int x, y, d, v;

        if (rec_skip(i) != 0 || rec_flag(i) != 0)
            continue;
        x = p[0]; y = p[1]; d = p[3]; v = p[4];

        map_blit_cell_sprite(dword_53A49, x, y);
        map_blit_cell_sprite(dword_53A49, x, y - 1);
        if (v != 0) {
            if (d != 0) {
                if (d == 1) {
                    map_blit_cell_sprite(dword_53A49, x - 1, y);
                    map_blit_cell_sprite(dword_53A49, x - 1, y - 1);
                } else if (d == 2) {
                    map_blit_cell_sprite(dword_53A49, x, y - 2);
                } else {
                    map_blit_cell_sprite(dword_53A49, x + 1, y);
                    map_blit_cell_sprite(dword_53A49, x + 1, y - 1);
                }
            } else {
                map_blit_cell_sprite(dword_53A49, x, y + 1);
            }
        }
    }
}

/* 0x11EEE - render the visible map window into `dst`.
 *
 * `dst` is a strided surface (stride `pitch`); the actual drawing base is
 * dst + 456*dword_53AF1 + dword_53AED + dword_53AF5 (dword_53AF5/dword_53AED
 * are the bitmap origin offsets, 456 = 24 rows of the 456-stride bitmap).
 *
 * Per BIOS tick dword_53A40 (the sprite mirror bit) toggles once, and for
 * the scanline modes the 16-phase expander advances. The view mode
 * dword_53C03 selects which intermediate buffer is copied into dst:
 *   9/24/25/28/29  expand dword_53AFF into dword_53B03, copy 320 -> pitch
 *   17/21/22/27    copy a window of dword_53AFF with stride 462/408
 *   23             scroll dword_53AFF one line, copy 312 -> pitch
 *   other          no copy
 * Then the w*h grid of 24x24 tiles (cell table dword_53A51, one 4-byte cell,
 * tile in the low 10 bits at +4) is blitted with sprite24_plain, or through
 * the cursor palette dword_53A6D[byte_51A97[dword_53C1F]] when cell[3] is
 * not 0xFF. Cell flags dword_53A69[tile] bits 3/4/5 pick mirrored/ramp
 * variants. */
void map_render_view(uint8_t *dst, int pitch, int w, int h, int ox, int oy)
{
    const uint8_t *pal;
    int32_t        tick;
    uint8_t       *out;
    int            i, row;

    tick = (int16_t)BDA_W(0x46C);
    if (tick != dword_53A00) {
        dword_53A40 ^= 1;
        dword_53A00 = tick;
    }

    out = dst + 456 * dword_53AF1 + dword_53AED + dword_53AF5;

    switch (dword_53C03) {
    case 9: case 24: case 25: case 28: case 29:
        if (tick != dword_539F8) {
            gfx_expand_scanlines((const void *)(uintptr_t)dword_53AFF,
                                 (void *)(uintptr_t)dword_53B03, dword_539FC);
            dword_539F8 = tick;
            if (++dword_539FC == 16)
                dword_539FC = 0;
        }
        gfx_copy_rows(out, pitch, (const void *)(uintptr_t)dword_53B03,
                      320, 312, 192);
        break;
    case 17: case 21: case 22: case 27: {
        int stride = (dword_53C03 == 17 || dword_53C03 == 27) ? 462 : 408;
        const uint8_t *src = (const uint8_t *)(uintptr_t)
            (dword_53AFF + 3 * ox + 2 * stride * oy + dword_53B07 / 2
             + stride * (dword_53B0B / 3));
        gfx_copy_rows(out, pitch, src, stride, 312, 192);
        break;
    }
    case 23:
        if (tick != dword_539F8) {
            map_scroll_lines(0);
            dword_539F8 = tick;
        }
        gfx_copy_rows(out, pitch, (const void *)(uintptr_t)dword_53AFF,
                      312, 312, 192);
        break;
    default:
        break;
    }

    if (dword_51A93 == -1) {
        if (tick - dword_539F4 > 2 || tick < dword_539F4) {
            if (++dword_53C1F == 20)
                dword_53C1F = 0;
            dword_539F4 = tick;
        }
    } else {
        dword_53C1F = dword_51A93;
    }

    pal = dword_53A6D
        + *(const uint32_t *)(dword_53A6D + 4 * byte_51A97[dword_53C1F] + 6);

    for (row = 0; row < h; row++) {
        const uint8_t *cell = dword_53A51
            + 4 * (ox + dword_53AC1 * (oy + row)) + 4;
        uint8_t *d = dst + 24 * pitch * row;

        for (i = 0; i < w; i++) {
            int            tile  = *(const uint16_t *)cell & 0x03FF;
            uint8_t        flags = *(const uint8_t *)(uintptr_t)(dword_53A69 + 4 * tile);
            const uint8_t *spr;

            if (flags & 0x08)
                tile += 2 * dword_53A40;
            else if (flags & 0x10)
                tile += dword_53C0B / 2;
            else if (flags & 0x04)
                tile += dword_53A40;

            spr = dword_53A5D + *(const uint32_t *)(dword_53A5D + 4 * tile + 6);
            if (cell[3] == 0xFF)
                sprite24_plain(spr, d, pitch);
            else
                sprite24_pal_recolor(spr, d, pitch, pal);
            cell += 4;
            d += 24;
        }
    }
}

/* 0x24D22 - scroll the screen buffer dword_53AFF down by `n` lines.
 *
 * `n != 0` only latches the line count into byte_51A10; the actual scroll
 * happens on the next call with n == 0, which moves the bottom `lines` rows
 * into a temporary buffer, shifts the remaining rows down and puts the
 * bottom rows on top (the classic wrap-around scroll). */
void map_scroll_lines(int n)
{
    uint8_t *screen = (uint8_t *)(uintptr_t)dword_53AFF;
    int      lines, i;
    uint8_t *tmp;

    if (n != 0) {
        byte_51A10 = (uint8_t)n;
        return;
    }

    lines = byte_51A10;
    tmp   = (uint8_t *)guest_malloc(312u * (size_t)lines);
    memmove(tmp, screen + 312 * (192 - lines), 312u * (size_t)lines);
    for (i = 191 - lines; i >= 0; --i)
        memmove(screen + 312 * lines + 312 * i, screen + 312 * i, 0x138);
    memmove(screen, tmp, 312u * (size_t)lines);
    guest_free(tmp);
}

/* 0x122DC - reveal the tiles around the map cursor (dword_53AB1 x / y).
 *
 * dword_51A83 selects the reveal radius (1..5 draw a diamond of 1..21 cells
 * with the blink-frame indices of dword_53A4D; the centre is index 0/1), and
 * mode 6 instead clears the cursor cell's visibility byte. */
void map_reveal_cursor(void)
{
    int x = dword_53AB1;
    int y = dword_53AB5;

    switch (dword_51A83) {
    case 1:
        map_blit_tile(x, y, 0);
        break;
    case 2:
        map_blit_tile(x, y, 1);
        break;
    case 3:
        map_blit_tile(x, y, 14);
        map_blit_tile(x, y - 1, 2);
        map_blit_tile(x - 1, y, 3);
        map_blit_tile(x + 1, y, 4);
        map_blit_tile(x, y + 1, 5);
        break;
    case 4:
        map_blit_tile(x, y, 1);
        map_blit_tile(x, y - 2, 2);
        map_blit_tile(x - 2, y, 3);
        map_blit_tile(x + 2, y, 4);
        map_blit_tile(x, y + 2, 5);
        map_blit_tile(x - 1, y - 1, 6);
        map_blit_tile(x + 1, y - 1, 7);
        map_blit_tile(x - 1, y + 1, 8);
        map_blit_tile(x + 1, y + 1, 9);
        map_blit_tile(x, y - 1, 10);
        map_blit_tile(x - 1, y, 11);
        map_blit_tile(x + 1, y, 12);
        map_blit_tile(x, y + 1, 13);
        break;
    case 5:
        map_blit_tile(x, y, 1);
        map_blit_tile(x, y - 3, 2);
        map_blit_tile(x - 3, y, 3);
        map_blit_tile(x + 3, y, 4);
        map_blit_tile(x, y + 3, 5);
        map_blit_tile(x - 1, y - 2, 6);
        map_blit_tile(x - 2, y - 1, 6);
        map_blit_tile(x + 1, y - 2, 7);
        map_blit_tile(x + 2, y - 1, 7);
        map_blit_tile(x - 1, y + 2, 8);
        map_blit_tile(x - 2, y + 1, 8);
        map_blit_tile(x + 1, y + 2, 9);
        map_blit_tile(x + 2, y + 1, 9);
        map_blit_tile(x, y - 2, 10);
        map_blit_tile(x - 2, y, 11);
        map_blit_tile(x + 2, y, 12);
        map_blit_tile(x, y + 2, 13);
        map_blit_tile(x - 1, y - 1, 15);
        map_blit_tile(x + 1, y - 1, 16);
        map_blit_tile(x - 1, y + 1, 17);
        map_blit_tile(x + 1, y + 1, 18);
        break;
    case 6:
        *(uint8_t *)(dword_53A51 + 4 * (x + dword_53AC1 * y) + 7) = 0;
        break;
    default:
        break;
    }
}

/* 0x1ACF3 - draw the selection cursor / record icon on the map view.
 *
 * byte_51AAB / byte_51AAC gate the cursor; dword_51A0C is the top scanline
 * inside the 157-line map area. It blits the cursor frame (icon resource
 * dword_53A81 sub-image 130), the cell's terrain sprite and its two numbers,
 * then, when a portrait record sits under the cursor, the record's icon and
 * a 3-digit HP bar. */
void map_draw_cursor(uint8_t *dst, int pitch)
{
    uint8_t        info[8];
    uint8_t       *p;
    const uint8_t *rec;
    int            idx;

    if (!byte_51AAB || !byte_51AAC)
        return;

    if (dword_53ABD <= 5 || dword_53AB9 >= 3) {
        if (dword_53ABD > 5 && dword_53AB9 > 9)
            dword_51A0C = 1;
    } else {
        dword_51A0C = 242;
    }

    p = dst + 157 * pitch + dword_51A0C;
    rle_decode((const void *)(uintptr_t)
                   (dword_53A81 + *(const uint32_t *)(dword_53A81 + 526)),
               0, 0, p, pitch, -1);

    map_cell_info(dword_53AB1, dword_53AB5, info);
    sprite24_plain(dword_53A5D
                   + *(const uint32_t *)(dword_53A5D + 4 * *(const uint16_t *)info + 6),
                   p + 5 * pitch + 6, pitch);
    dlg_draw_number_signed(p + 8 * pitch + 43, pitch, dword_51A12[info[5]]);
    dlg_draw_number_signed(p + 19 * pitch + 43, pitch, dword_51A2A[info[5]]);

    idx = dlg_portrait_find();
    if (idx == -1)
        return;

    rec = (const uint8_t *)(uintptr_t)dword_53A45 + 80 * idx;
    if (rec[7] == 121 || (rec[31] == 10 && rec[6] == 1))
        return;

    {
        int v = dword_53C0B;
        if (v == 3)
            v = 1;
        sprite24_plain(dword_53A61
                       + *(const uint32_t *)(dword_53A61 + 4 * (12 * rec[2] + v)),
                       p + 5 * pitch + 6, pitch);
        dlg_draw_number_pair(p + 21 * pitch + 9, pitch,
                             *(const uint16_t *)(rec + 64),
                             *(const uint16_t *)(rec + 66), 3);
    }
}

/* 0x32230 - per-record movement blip.
 *
 * The 29-byte table *(0x52725) classifies the record's unit type (record +32
 * is a 1-based index into it). A record that rec_skip() reports as skipped
 * pings with effect 10 every 6th call; otherwise the unit class selects
 * effect 9 every 6 (class 0) / every 4 (class 1) / effect 11 every 9 calls.
 * byte_54132 is the free-running call counter (uint8, wraps at 256), and the
 * effect always plays on bank *(0x53EEC) with loop count 1.
 *
 * The machine code copies the table to the stack first and indexes it as
 * t[k-1]; record +32 is a valid 1..29 index in the game's data, so the copy
 * has no observable effect on in-range indices. `sub_25A96` (svc_play_sfx) is
 * called through its original address so the host and the differential
 * harness can both observe it at one place.
 *
 * Callers ignore the return value (IDA: every call site overwrites EAX). */
typedef int (*sfx_fn)(const void *bank, int index, int loops);
#define ORIG_SFX ((sfx_fn)(uintptr_t)0x00025A96u)

void map_unit_ping(int idx)
{
    uint8_t t[29];
    int     s;

    memcpy(t, byte_52725, sizeof t);

    if (rec_skip(idx)) {
        if ((uint8_t)byte_54132 % 6 == 0)
            ORIG_SFX(dword_53EEC, 10, 1);
    } else {
        int k = *(const uint8_t *)((uintptr_t)dword_53A45 + 80u * (uint32_t)idx
                                   + 32u);

        s = t[k - 1];
        if (s == 0) {
            if ((uint8_t)byte_54132 % 6 == 0)
                ORIG_SFX(dword_53EEC, 9, 1);
        } else if (s == 1) {
            if ((uint8_t)byte_54132 % 4 == 0)
                ORIG_SFX(dword_53EEC, 9, 1);
        } else {
            if ((uint8_t)byte_54132 % 9 == 0)
                ORIG_SFX(dword_53EEC, 11, 1);
        }
    }
    ++byte_54132;
}

/* 0x11CAC - the whole map-view refresh: advance the animation frame, update
 * the DAC animation when `flag == 0`, re-render the 13x8 tile window at the
 * view origin into the map bitmap, reveal/refresh the cursor and portraits,
 * then push the 192x312 bitmap window to VGA at 0xA0504. */
void map_view_update(int flag)
{
    uint8_t *view = dword_53A49 + 0x8088;   /* 32904 */

    anim_frame_step();
    if (flag == 0)
        pal_anim_step();
    map_render_view(view, 456, 13, 8, dword_53AA9, dword_53AAD);
    map_reveal_cursor();
    dlg_portraits_refresh();
    map_draw_cursor(view, 456);
    gfx_copy_rows((void *)(uintptr_t)0xA0504u, 320, view, 456, 312, 192);
}
