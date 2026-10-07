/* mapcheck.c - differential test for the map-view leaves
 *
 *   0x126F7 map_blit_tile(x, y, index)   src/game/map.c  (calls sprite24_plain)
 *   0x16886 res_blit6(dst, pitch, buf, index)  src/game/res.c (offset table +6)
 *   0x134E4 dlg_portrait_clear()         src/game/dlg.c  (delay is hooked)
 *
 * All three are self-contained once the globals point at synthetic buffers:
 * the map view/cells, the tileset / LMI sub-images, the portrait records. The
 * RLE streams are generated here (24x24 tiles: every row is one "24 literals"
 * token), so the comparison is byte-for-byte on the destination surface,
 * which is where a wrong offset or a wrong decoder would show up.
 *
 * Build: pwsh -File build.ps1 -Target mapcheck
 * Run   : build\mapcheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "platform.h"
#include "game/map.h"
#include "game/rec.h"
#include "game/res.h"
#include "game/dlg.h"
#include "game/anim.h"
#include "game/tables.h"

/* anim.c reads the BIOS tick through DOS_LOWMEM_BASE (dos_lowmem_base is
 * normally defined by dos.c, which this harness does not link). */
uint32_t dos_lowmem_base = 0x00070000u;

#define W32(x) (*(uint32_t *)(uintptr_t)(x))
#define I32(x) (*(int32_t  *)(uintptr_t)(x))
#define PTR(x) (*(void    **)(uintptr_t)(x))
#define B8(x)  (*(uint8_t  *)(uintptr_t)(x))

typedef void (*tile_fn)(int, int, int);
typedef void (*blit6_fn)(void *, int, void *, int);
typedef void (*clear_fn)(void);
typedef void (*anim_fn)(void);
typedef void (*cell_fn)(int, int, uint8_t *);
typedef int  (*find_fn)(void);
typedef void *(*tbl_fn)(int);
#define ORIG_TILE   ((tile_fn)  (uintptr_t)0x000126F7u)
#define ORIG_BLIT6  ((blit6_fn) (uintptr_t)0x00016886u)
#define ORIG_CLEAR  ((clear_fn) (uintptr_t)0x000134E4u)
#define ORIG_ANIM   ((anim_fn)  (uintptr_t)0x0001297Du)
#define ORIG_CELL   ((cell_fn)  (uintptr_t)0x00012E38u)
#define ORIG_FIND   ((find_fn)  (uintptr_t)0x00012C0Du)
#define ORIG_TBL    ((tbl_fn)   (uintptr_t)0x0004EB48u)
typedef void (*num_fn)(void *, int, int, int, int);
typedef void (*sign_fn)(void *, int, int);
#define ORIG_NUM     ((num_fn)  (uintptr_t)0x000187D6u)
#define ORIG_NUMPAIR ((num_fn)  (uintptr_t)0x0001875Du)
#define ORIG_NUMSIGN ((sign_fn) (uintptr_t)0x0001AEB1u)
typedef int  (*skip_fn)(int);
typedef void (*cellspr_fn)(void *, int, int);
typedef void (*refresh_fn)(void);
typedef void (*render_fn)(uint8_t *, int, int, int, int, int);
typedef void (*scroll_fn)(int);
typedef void (*reveal_fn)(void);
typedef void (*dcursor_fn)(uint8_t *, int);
#define ORIG_SKIP    ((skip_fn)    (uintptr_t)0x0001F183u)
#define ORIG_CELLSPR ((cellspr_fn) (uintptr_t)0x00012AC6u)
#define ORIG_REFRESH ((refresh_fn) (uintptr_t)0x000129ECu)
#define ORIG_RENDER  ((render_fn)  (uintptr_t)0x00011EEEu)
#define ORIG_SCROLL  ((scroll_fn)  (uintptr_t)0x00024D22u)
#define ORIG_REVEAL  ((reveal_fn)  (uintptr_t)0x000122DCu)
#define ORIG_DCURSOR ((dcursor_fn) (uintptr_t)0x0001ACF3u)
typedef void (*draw_fn)(int);
typedef void (*refreshall_fn)(void);
#define ORIG_DRAW    ((draw_fn)       (uintptr_t)0x000127E0u)
#define ORIG_ALLPORT ((refreshall_fn) (uintptr_t)0x000127A9u)

#define BITMAP_SZ (400 * 1024)
#define SCR_SZ    (256 * 1024)
#define EXP_SZ    (128 * 1024)
#define TILE_W    24
#define TILE_H    24
#define MIRROR    0x00070000u
#define G_TICK    (MIRROR + 0x46C)

static int  g_fail;
static char g_why[256];
static int  g_delays;

static void __cdecl stub_delay(unsigned ms) { (void)ms; g_delays++; }
static void *__cdecl stub_malloc(size_t n) { return malloc(n); }
static void *__cdecl stub_memmove(void *d, const void *s, size_t n)
{ return memmove(d, s, n); }
static void  __cdecl stub_free(void *p) { free(p); }

static void install_hook(uint32_t addr, const void *dest)
{
    uint8_t *p   = (uint8_t *)(uintptr_t)addr;
    int32_t  rel = (int32_t)((const uint8_t *)dest - (p + 5));
    DWORD    old;

    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    p[0] = 0xE9;
    memcpy(p + 1, &rel, 4);
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
}

/* Redirect the game's low-memory immediates (0x46C tick) into the mirror, so
 * the original 0x1297D reads the same bytes the C does. */
static int patch_lowmem_refs(le_image *le, uint32_t mirror)
{
    unsigned i, off;
    int      n = 0;

    for (i = 0; i < le->object_count; i++) {
        uint8_t *base;
        uint32_t size;

        if (!(le->objects[i].flags & 0x04))
            continue;
        base = (uint8_t *)(uintptr_t)le->objects[i].base;
        size = le->objects[i].vsize;
        for (off = 0; off + 5 <= size; off++) {
            uint8_t  op  = base[off];
            uint32_t imm;
            int      is_mov = (op >= 0xB8 && op <= 0xBF);

            if (!is_mov && op != 0x68)
                continue;
            imm = (uint32_t)base[off + 1] | ((uint32_t)base[off + 2] << 8) |
                  ((uint32_t)base[off + 3] << 16) | ((uint32_t)base[off + 4] << 24);
            if (imm >= 0x400 && imm < 0x500) {
                uint32_t fixed = mirror + imm;
                base[off + 1] = (uint8_t)fixed;
                base[off + 2] = (uint8_t)(fixed >> 8);
                base[off + 3] = (uint8_t)(fixed >> 16);
                base[off + 4] = (uint8_t)(fixed >> 24);
                n++;
            }
        }
    }
    return n;
}

static uint32_t g_rnd = 0x5EED1234u;static uint32_t rnd(void) { g_rnd = g_rnd * 1103515245u + 12345u; return g_rnd >> 8; }

/* One 24x24 sprite24 stream: each row is a single "literal 24" token. */
static size_t fill_tile_stream(uint8_t *p, uint8_t seed)
{
    size_t n = 0;
    int y, x;
    for (y = 0; y < TILE_H; y++) {
        p[n++] = (uint8_t)((2 << 6) | (TILE_W - 1));   /* literal, count 24 */
        for (x = 0; x < TILE_W; x++)
            p[n++] = (uint8_t)(seed + x + y * 3);
    }
    return n;
}

static void fail(const char *what) { printf("FAIL %s: %s\n", what, g_why); g_fail = 1; }

int main(int argc, char **argv)
{
    le_image le;
    int      applied = 0, round;
    long     cases = 0;
    uint8_t *bitmap = (uint8_t *)malloc(BITMAP_SZ);
    uint8_t *tileset = (uint8_t *)malloc(64 * 1024);
    uint8_t *lmi = (uint8_t *)malloc(64 * 1024);
    uint8_t *dstA = (uint8_t *)malloc(BITMAP_SZ);
    uint8_t *dstB = (uint8_t *)malloc(BITMAP_SZ);
    uint8_t *scrA = (uint8_t *)malloc(SCR_SZ);
    uint8_t *scrB = (uint8_t *)malloc(SCR_SZ);
    uint8_t *expA = (uint8_t *)malloc(EXP_SZ);
    uint8_t *expB = (uint8_t *)malloc(EXP_SZ);
    uint8_t *recs = (uint8_t *)malloc(80 * 16);
    uint8_t *cells = (uint8_t *)malloc(4 * 64 * 64);
    uint8_t *ctbl = (uint8_t *)malloc(4 * 1024);
    uint8_t *nres = (uint8_t *)malloc(32 * 1024);
    uint8_t *sbank = (uint8_t *)malloc(0x0A + 4 * 2048 + 2048 * 600);
    uint8_t *pbank = (uint8_t *)malloc(6 + 4 * 256 + 256 * 256);
    uint8_t *ibank = (uint8_t *)malloc(64 * 1024);
    uint8_t *cellA = (uint8_t *)malloc(4 * 64 * 64);
    uint8_t *cellB = (uint8_t *)malloc(4 * 64 * 64);
    uint8_t *recA  = (uint8_t *)malloc(80 * 16);
    uint8_t *recB  = (uint8_t *)malloc(80 * 16);

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    install_hook(0x3790A, stub_delay);
    /* The map render core reaches the Watcom CRT heap (0x24D22 -> malloc /
     * memmove / free); point them at the host libc so the original machine
     * code and the C translation use the same heap, exactly like rescheck. */
    install_hook(0x3706E, stub_malloc);
    install_hook(0x3771C, stub_memmove);
    install_hook(0x3776E, stub_free);
    printf("mapcheck: redirected %d low-memory references to 0x%X\n",
           patch_lowmem_refs(&le, MIRROR), MIRROR);

    if (!bitmap || !tileset || !lmi || !dstA || !dstB || !recs || !cells || !ctbl || !nres || !sbank || !pbank || !ibank)
        return 2;
    if (!scrA || !scrB || !expA || !expB || !cellA || !cellB || !recA || !recB)
        return 2;

    /* ---- tileset layout: table at +6, 32 sub-images at +22 (indices 0..18
     * are used by the map cursor reveal) ---- */
    {
        uint32_t off = 6 + 4 * 32;
        int i;
        for (i = 0; i < 32; i++) {
            *(uint32_t *)(tileset + 6 + 4 * i) = off;
            off += (uint32_t)fill_tile_stream(tileset + off, (uint8_t)(i * 17 + 1));
        }
    }
    /* ---- LMI layout for res_blit6: table at +6, sub-image = u16 w,h + stream */
    {
        uint32_t off = 6 + 4 * 4;
        int i;
        for (i = 0; i < 4; i++) {
            uint8_t *p = lmi + off;
            *(uint32_t *)(lmi + 6 + 4 * i) = off;
            *(uint16_t *)p = TILE_W;
            *(uint16_t *)(p + 2) = TILE_H;
            off += 4 + (uint32_t)fill_tile_stream(p + 4, (uint8_t)(i * 23 + 5));
        }
    }

    /* number sprites at *(0x53A81): 256 sub-images, each u16 w=8,h=8 plus
     * 8 rows of one "literal 8" token, so any digit index resolves. */
    {
        uint32_t off = 6 + 4 * 256;
        int k, y, x;
        for (k = 0; k < 256; k++) {
            uint8_t *p = nres + off;
            *(uint32_t *)(nres + 6 + 4 * k) = off;
            *(uint16_t *)p = 8;
            *(uint16_t *)(p + 2) = 8;
            {
                uint8_t *q = p + 4;
                for (y = 0; y < 8; y++) {
                    *q++ = (uint8_t)((2 << 6) | 7);
                    for (x = 0; x < 8; x++) *q++ = (uint8_t)(k + x + y);
                }
                off += (uint32_t)(q - p);
            }
        }
    }
    PTR(0x53A81) = nres;

    /* cell-sprite bank *(0x53A5D): offset table at +0x0A, 1024 24x24 streams */
    {
        uint32_t off = 0x0A + 4 * 2048;
        int k;
        for (k = 0; k < 2048; k++) {
            *(uint32_t *)(sbank + 0x0A + 4 * k) = off;
            off += (uint32_t)fill_tile_stream(sbank + off, (uint8_t)(k * 7 + 3));
        }
    }
    /* palette bank *(0x53A6D): offset table at +6, 256 256-byte palettes */
    {
        uint32_t off = 6 + 4 * 256;
        int k, j;
        for (k = 0; k < 256; k++) {
            *(uint32_t *)(pbank + 6 + 4 * k) = off;
            for (j = 0; j < 256; j++) pbank[off + j] = (uint8_t)(k + j);
            off += 256;
        }
    }
    PTR(0x53A5D) = sbank;
    PTR(0x53A6D) = pbank;
    {
        int j;
        for (j = 0; j < 256; j++) B8(0x51A97 + j) = (uint8_t)rnd();
    }

    PTR(0x53A49) = bitmap;
    PTR(0x53A4D) = tileset;
    PTR(0x53A45) = recs;

    /* icon/portrait bank *(0x53A61): 32-bit offset table at the base, four
     * resources of twelve 24x24 frames each (index = mode + 12*res + 3*dir). */
    {
        uint32_t off = 4 * 48;
        int k;
        for (k = 0; k < 48; k++) {
            *(uint32_t *)(ibank + 4 * k) = off;
            off += (uint32_t)fill_tile_stream(ibank + off, (uint8_t)(k * 11 + 9));
        }
    }
    PTR(0x53A61) = ibank;

    for (round = 0; round < 500 && !g_fail; round++) {
        int i;

        /* ---- map_blit_tile ------------------------------------------- */
        for (i = 0; i < 20 && !g_fail; i++) {
            int ox = (int)(rnd() % 64);
            int oy = (int)(rnd() % 64);
            int vc = 4 + (int)(rnd() % 12);
            int vr = 4 + (int)(rnd() % 10);
            int x, y, idx = (int)(rnd() % 4);

            I32(0x53AA9) = ox; I32(0x53AAD) = oy;
            I32(0x51A87) = vc; I32(0x51A8B) = vr;

            /* inside, outside-left, outside-right, outside-both */
            switch (rnd() % 4) {
            case 0: x = ox + (int)(rnd() % vc); y = oy + (int)(rnd() % vr); break;
            case 1: x = ox - 1 - (int)(rnd() % 4); y = oy + 1; break;
            case 2: x = ox + vc; y = oy + (int)(rnd() % vr); break;
            default: x = ox + vc + 1; y = oy - 2; break;
            }

            memset(bitmap, 0x11, BITMAP_SZ);
            memset(dstA, 0x00, BITMAP_SZ);
            ORIG_TILE(x, y, idx);
            memcpy(dstA, bitmap, BITMAP_SZ);

            memset(bitmap, 0x11, BITMAP_SZ);
            map_blit_tile(x, y, idx);
            cases++;
            if (memcmp(dstA, bitmap, BITMAP_SZ) != 0) {
                size_t k, where = 0;
                for (k = 0; k < BITMAP_SZ; k++)
                    if (dstA[k] != bitmap[k]) { where = k; break; }
                snprintf(g_why, sizeof g_why,
                         "(x=%d y=%d idx=%d view=%dx%d+%d+%d) first diff @%u (%02X/%02X)",
                         x, y, idx, vc, vr, ox, oy, (unsigned)where,
                         dstA[where], bitmap[where]);
                fail("map_blit_tile");
                break;
            }
        }

        /* ---- res_blit6 ------------------------------------------------ */
        for (i = 0; i < 20 && !g_fail; i++) {
            int pitch = 320;
            int idx = (int)(rnd() % 4);
            memset(dstA, 0x77, BITMAP_SZ);
            memset(dstB, 0x77, BITMAP_SZ);
            ORIG_BLIT6(dstA, pitch, lmi, idx);
            res_blit6(dstB, pitch, lmi, idx);
            cases++;
            if (memcmp(dstA, dstB, BITMAP_SZ) != 0) {
                size_t k, where = 0;
                for (k = 0; k < BITMAP_SZ; k++)
                    if (dstA[k] != dstB[k]) { where = k; break; }
                snprintf(g_why, sizeof g_why, "idx=%d first diff @%u (%02X/%02X)",
                         idx, (unsigned)where, dstA[where], dstB[where]);
                fail("res_blit6");
                break;
            }
        }

        /* ---- dlg_portrait_clear -------------------------------------- */
        for (i = 0; i < 10 && !g_fail; i++) {
            int n = 1 + (int)(rnd() % 8);
            int da, db, k;

            I32(0x53BEB) = n;
            for (k = 0; k < 80 * n; k++) recs[k] = (uint8_t)rnd();
            g_delays = 0; ORIG_CLEAR();
            da = g_delays;
            cases++;
            for (k = 0; k < n; k++) {
                if (recs[k * 80 + 3] != 0) {
                    snprintf(g_why, sizeof g_why, "original left flag %d = %02X",
                             k, recs[k * 80 + 3]);
                    fail("dlg_portrait_clear");
                    break;
                }
            }
            if (g_fail) break;

            for (k = 0; k < 80 * n; k++) recs[k] = (uint8_t)rnd();
            g_delays = 0; dlg_portrait_clear();
            db = g_delays;
            if (db != da) {
                snprintf(g_why, sizeof g_why, "delay count %d/%d", da, db);
                fail("dlg_portrait_clear");
                break;
            }
            for (k = 0; k < n; k++) {
                if (recs[k * 80 + 3] != 0) {
                    snprintf(g_why, sizeof g_why, "C left flag %d = %02X",
                             k, recs[k * 80 + 3]);
                    fail("dlg_portrait_clear");
                    break;
                }
            }
            if (g_fail) break;
        }

        /* ---- anim_frame_step (0x1297D) -------------------------------- */
        for (i = 0; i < 20 && !g_fail; i++) {
            int      t  = (int)(rnd() & 0xFFFF);
            int32_t  f0 = (int16_t)rnd();
            int32_t  b0 = (int32_t)(rnd() % 4), c0 = (int32_t)(rnd() % 4);
            int32_t  f1, b1, c1;

            *(volatile uint16_t *)(uintptr_t)G_TICK = (uint16_t)t;
            I32(0x53C0F) = f0; I32(0x53C0B) = b0; I32(0x53C07) = c0;
            ORIG_ANIM();
            f1 = I32(0x53C0F); b1 = I32(0x53C0B); c1 = I32(0x53C07);

            *(volatile uint16_t *)(uintptr_t)G_TICK = (uint16_t)t;
            I32(0x53C0F) = f0; I32(0x53C0B) = b0; I32(0x53C07) = c0;
            anim_frame_step();
            cases++;
            if (I32(0x53C0F) != f1 || I32(0x53C0B) != b1 || I32(0x53C07) != c1) {
                snprintf(g_why, sizeof g_why,
                         "tick=%d f=%d/%d b=%d/%d c=%d/%d", t,
                         f1, I32(0x53C0F), b1, I32(0x53C0B), c1, I32(0x53C07));
                fail("anim_frame_step"); break;
            }
        }

        /* ---- map_cell_info (0x12E38) ---------------------------------- */
        for (i = 0; i < 20 && !g_fail; i++) {
            int w = 8 + (int)(rnd() % 32);
            int x = (int)(rnd() % w), y = (int)(rnd() % 32);
            uint8_t outA[8], outB[8];
            uint8_t *cell = cells + 4 * (x + w * y);
            int k, d;

            PTR(0x53A51) = cells;
            I32(0x53AC1) = w;
            I32(0x53A69) = (int32_t)(uintptr_t)ctbl;
            for (k = 0; k < 8; k++) cell[k] = (uint8_t)rnd();
            cell[4] |= 0xFC;   /* exercise the 0x3FF mask */
            ORIG_CELL(x, y, outA);
            map_cell_info(x, y, outB);
            cases++;
            for (d = 0; d < 8; d++)
                if (outA[d] != outB[d]) {
                    snprintf(g_why, sizeof g_why, "(x=%d y=%d w=%d) out[%d]=%02X/%02X",
                             x, y, w, d, outA[d], outB[d]);
                    fail("map_cell_info"); break;
                }
            if (d < 8) break;
        }

        /* ---- dlg_portrait_find (0x12C0D) ------------------------------ */
        for (i = 0; i < 20 && !g_fail; i++) {
            int n = 1 + (int)(rnd() % 8);
            int want = (int)(rnd() % (n + 1));   /* n = "not found" */
            int k, ra, rc;

            PTR(0x53A45) = recs;
            I32(0x53BEB) = n;
            for (k = 0; k < 80 * n; k++) recs[k] = (uint8_t)rnd();
            if (want < n) {
                recs[want * 80 + 0] = 0x12; recs[want * 80 + 1] = 0x34;
                recs[want * 80 + 5] = 0;        /* rec_flag == 0 */
                *(uint32_t *)(uintptr_t)0x53AB1 = 0x12;
                *(uint32_t *)(uintptr_t)0x53AB5 = 0x34;
            } else {
                *(uint32_t *)(uintptr_t)0x53AB1 = 0xEE;
                *(uint32_t *)(uintptr_t)0x53AB5 = 0xFF;
            }
            ra = ORIG_FIND();
            rc = dlg_portrait_find();
            cases++;
            if (ra != rc) {
                snprintf(g_why, sizeof g_why, "n=%d want=%d -> %d/%d", n, want, ra, rc);
                fail("dlg_portrait_find"); break;
            }
        }

        /* ---- dlg_draw_number (0x187D6) -------------------------------- */
        for (i = 0; i < 20 && !g_fail; i++) {
            int pitch = 320;
            int digits = 1 + (int)(rnd() % 3);
            int value = (int)(rnd() % (digits == 3 ? 2000 : (digits == 2 ? 200 : 10)));
            int base = (int)(rnd() % 200);
            memset(dstA, 0x33, BITMAP_SZ);
            memset(dstB, 0x33, BITMAP_SZ);
            ORIG_NUM(dstA, pitch, value, base, digits);
            dlg_draw_number(dstB, pitch, value, base, digits);
            cases++;
            if (memcmp(dstA, dstB, BITMAP_SZ) != 0) {
                size_t k, where = 0;
                for (k = 0; k < BITMAP_SZ; k++)
                    if (dstA[k] != dstB[k]) { where = k; break; }
                snprintf(g_why, sizeof g_why, "value=%d base=%d digits=%d @%u (%02X/%02X)",
                         value, base, digits, (unsigned)where, dstA[where], dstB[where]);
                fail("dlg_draw_number"); break;
            }
        }

        /* ---- dlg_draw_number_pair (0x1875D) --------------------------- */
        for (i = 0; i < 10 && !g_fail; i++) {
            int pitch = 320, digits = 2 + (int)(rnd() % 2);
            int value = (int)(rnd() % 300);
            int cmp = (rnd() & 1) ? value : (int)(rnd() % 300);
            memset(dstA, 0x44, BITMAP_SZ); memset(dstB, 0x44, BITMAP_SZ);
            ORIG_NUMPAIR(dstA, pitch, value, cmp, digits);
            dlg_draw_number_pair(dstB, pitch, value, cmp, digits);
            cases++;
            if (memcmp(dstA, dstB, BITMAP_SZ) != 0) {
                snprintf(g_why, sizeof g_why, "value=%d cmp=%d digits=%d", value, cmp, digits);
                fail("dlg_draw_number_pair"); break;
            }
        }

        /* ---- dlg_draw_number_signed (0x1AEB1) ------------------------- */
        for (i = 0; i < 10 && !g_fail; i++) {
            int pitch = 320;
            int value = (int)(rnd() % 300) - ((rnd() & 1) ? 200 : 50);
            memset(dstA, 0x55, BITMAP_SZ); memset(dstB, 0x55, BITMAP_SZ);
            ORIG_NUMSIGN(dstA, pitch, value);
            dlg_draw_number_signed(dstB, pitch, value);
            cases++;
            if (memcmp(dstA, dstB, BITMAP_SZ) != 0) {
                snprintf(g_why, sizeof g_why, "value=%d", value);
                fail("dlg_draw_number_signed"); break;
            }
        }

        /* ---- rec_skip (0x1F183) --------------------------------------- */
        for (i = 0; i < 20 && !g_fail; i++) {
            int n = 1 + (int)(rnd() % 8);
            int idx = (int)(rnd() % n);
            int k, a, b;

            PTR(0x53A45) = recs;
            I32(0x53BEB) = n;
            for (k = 0; k < 80 * n; k++) recs[k] = (uint8_t)rnd();
            recs[idx * 80 + 7]    = (rnd() & 1) ? 0x1C : (uint8_t)rnd();
            recs[idx * 80 + 0x20] = (rnd() & 1) ? 0x13 : (uint8_t)rnd();
            recs[idx * 80 + 0x1F] = (rnd() & 1) ? (uint8_t)(4 + (rnd() % 2))
                                                : (uint8_t)rnd();
            a = ORIG_SKIP(idx);
            b = rec_skip(idx);
            cases++;
            if (a != b) {
                snprintf(g_why, sizeof g_why, "idx=%d -> %d/%d", idx, a, b);
                fail("rec_skip"); break;
            }
        }

        /* ---- map_blit_cell_sprite (0x12AC6) / map_refresh_records (0x129EC) */
        if (!g_fail) {
            int w = 16 + (int)(rnd() % 16);
            int k, x, y;

            I32(0x53AA9) = 0; I32(0x53AAD) = 0;
            I32(0x51A87) = 12; I32(0x51A8B) = 10;
            I32(0x53AC1) = w;
            PTR(0x53A51) = cells;
            I32(0x53A69) = (int32_t)(uintptr_t)ctbl;
            I32(0x53A40) = (int32_t)(rnd() % 4);
            I32(0x53C1F) = (int32_t)(rnd() % 256);

            for (k = 0; k < 4 * 64 * 64; k++) cells[k] = (uint8_t)rnd();
            for (k = 0; k < 1024; k++) ctbl[4 * k] = (uint8_t)rnd();
            {
                int n = 1 + (int)(rnd() % 6);
                I32(0x53BEB) = n;
                for (k = 0; k < 80 * n; k++) recs[k] = (uint8_t)rnd();
                for (k = 0; k < n; k++) {
                    recs[k * 80 + 0] = (uint8_t)(rnd() % w);
                    recs[k * 80 + 1] = (uint8_t)(rnd() % 20);
                    recs[k * 80 + 3] = (uint8_t)(rnd() % 4);
                    recs[k * 80 + 4] = (uint8_t)(rnd() & 1);
                }
            }
            PTR(0x53A45) = recs;

            memset(bitmap, 0x22, BITMAP_SZ);
            ORIG_REFRESH();
            memcpy(dstA, bitmap, BITMAP_SZ);
            memset(bitmap, 0x22, BITMAP_SZ);
            map_refresh_records();
            cases++;
            if (memcmp(dstA, bitmap, BITMAP_SZ) != 0)
                fail("map_refresh_records");

            x = (int)(rnd() % (w + 4)) - 2;
            y = (int)(rnd() % 24) - 2;
            memset(dstA, 0x33, BITMAP_SZ);
            memset(dstB, 0x33, BITMAP_SZ);
            ORIG_CELLSPR(dstA, x, y);
            map_blit_cell_sprite(dstB, x, y);
            cases++;
            if (memcmp(dstA, dstB, BITMAP_SZ) != 0)
                fail("map_blit_cell_sprite");
        }

        /* ---- dlg_portrait_draw (0x127E0) ------------------------------ */
        for (i = 0; i < 20 && !g_fail; i++) {
            int     idx = (int)(rnd() % 8);
            int     k;
            int16_t tick = (int16_t)rnd();
            int32_t a4i  = (int32_t)(rnd() & 1);   /* only ever toggled 0/1 */
            int32_t a8i  = (int32_t)(int16_t)rnd();
            int32_t a4a, a8a, a4b, a8b;

            PTR(0x53A45) = recs;
            I32(0x53BEB) = 8;
            for (k = 0; k < 80 * 8; k++) recs[k] = (uint8_t)rnd();
            recs[80 * idx + 0]  = (uint8_t)(rnd() % 20);   /* x, some out of view */
            recs[80 * idx + 1]  = (uint8_t)(rnd() % 20);   /* y                   */
            recs[80 * idx + 2]  = (uint8_t)(rnd() % 4);    /* icon resource       */
            recs[80 * idx + 3]  = (uint8_t)(rnd() % 4);    /* direction           */
            recs[80 * idx + 4]  = (uint8_t)(rnd() & 1);    /* frame               */
            recs[80 * idx + 5]  = (uint8_t)(rnd() & 0xFF); /* bit 7 = ramp24      */
            recs[80 * idx + 38] = (uint8_t)(rnd() & 1);    /* flip variant        */

            I32(0x53AA9) = 0; I32(0x53AAD) = 0;
            I32(0x51A87) = 12; I32(0x51A8B) = 10;
            I32(0x53C07) = (int32_t)(rnd() % 4);
            I32(0x53C0B) = (int32_t)(rnd() % 4);

            *(volatile uint16_t *)(uintptr_t)G_TICK = (uint16_t)tick;
            I32(0x53A04) = a4i; I32(0x53A08) = a8i;
            memset(bitmap, 0x66, BITMAP_SZ);
            ORIG_DRAW(idx);
            memcpy(dstA, bitmap, BITMAP_SZ);
            a4a = I32(0x53A04); a8a = I32(0x53A08);

            *(volatile uint16_t *)(uintptr_t)G_TICK = (uint16_t)tick;
            I32(0x53A04) = a4i; I32(0x53A08) = a8i;
            memset(bitmap, 0x66, BITMAP_SZ);
            dlg_portrait_draw(idx);
            a4b = I32(0x53A04); a8b = I32(0x53A08);
            cases++;
            if (memcmp(dstA, bitmap, BITMAP_SZ) != 0 || a4a != a4b || a8a != a8b) {
                size_t q, where = 0;
                for (q = 0; q < BITMAP_SZ; q++)
                    if (dstA[q] != bitmap[q]) { where = q; break; }
                snprintf(g_why, sizeof g_why,
                         "idx=%d x=%d y=%d @%u a4 %d/%d a8 %d/%d", idx,
                         recs[80 * idx], recs[80 * idx + 1], (unsigned)where,
                         a4a, a4b, a8a, a8b);
                fail("dlg_portrait_draw");
            }
        }

        /* ---- dlg_portraits_refresh (0x127A9) -------------------------- */
        if (!g_fail) {
            int     n = 1 + (int)(rnd() % 6);
            int     k;
            int16_t tick = (int16_t)rnd();
            int32_t a4i  = (int32_t)(rnd() & 1);   /* only ever toggled 0/1 */
            int32_t a8i  = (int32_t)(int16_t)rnd();

            PTR(0x53A45) = recs;
            I32(0x53BEB) = n;
            for (k = 0; k < 80 * n; k++) recs[k] = (uint8_t)rnd();
            for (k = 0; k < n; k++) {
                recs[80 * k + 0]  = (uint8_t)(rnd() % 20);
                recs[80 * k + 1]  = (uint8_t)(rnd() % 20);
                recs[80 * k + 2]  = (uint8_t)(rnd() % 4);
                recs[80 * k + 3]  = (uint8_t)(rnd() % 4);
                recs[80 * k + 4]  = (uint8_t)(rnd() & 1);
                recs[80 * k + 5]  = (uint8_t)(rnd() & 0xFF);
                recs[80 * k + 38] = (uint8_t)(rnd() & 1);
            }
            I32(0x53AA9) = 0; I32(0x53AAD) = 0;
            I32(0x51A87) = 12; I32(0x51A8B) = 10;
            I32(0x53C07) = (int32_t)(rnd() % 4);
            I32(0x53C0B) = (int32_t)(rnd() % 4);

            *(volatile uint16_t *)(uintptr_t)G_TICK = (uint16_t)tick;
            I32(0x53A04) = a4i; I32(0x53A08) = a8i;
            memset(bitmap, 0x66, BITMAP_SZ);
            ORIG_ALLPORT();
            memcpy(dstA, bitmap, BITMAP_SZ);

            *(volatile uint16_t *)(uintptr_t)G_TICK = (uint16_t)tick;
            I32(0x53A04) = a4i; I32(0x53A08) = a8i;
            memset(bitmap, 0x66, BITMAP_SZ);
            dlg_portraits_refresh();
            cases++;
            if (memcmp(dstA, bitmap, BITMAP_SZ) != 0)
                fail("dlg_portraits_refresh");
        }

        /* ---- tbl_off627D8 (0x4EB48) ----------------------------------- */
        if (!g_fail) {
            int k;
            for (k = 0; k < 8; k++)
                *(uint32_t *)(uintptr_t)(0x627D8 + 4 * k) = (uint32_t)rnd();
            {
                int idx = (int)(rnd() % 8);
                void *a = ORIG_TBL(idx);
                void *b = tbl_off627D8(idx);
                cases++;
                if (a != b) {
                    snprintf(g_why, sizeof g_why, "idx=%d %p/%p", idx, a, b);
                    fail("tbl_off627D8");
                }
            }
        }

        /* ---- map_scroll_lines (0x24D22) ------------------------------- */
        for (i = 0; i < 6 && !g_fail; i++) {
            int    lines = (int)(rnd() % 193);
            int    n     = (rnd() & 1) ? (int)(rnd() % 300) : 0;
            int    k;
            uint8_t la, lb;

            for (k = 0; k < SCR_SZ; k++) scrA[k] = (uint8_t)rnd();
            memcpy(scrB, scrA, SCR_SZ);
            B8(0x51A10) = (uint8_t)lines;
            PTR(0x53AFF) = scrA;
            ORIG_SCROLL(n);
            la = B8(0x51A10);

            B8(0x51A10) = (uint8_t)lines;
            PTR(0x53AFF) = scrB;
            map_scroll_lines(n);
            lb = B8(0x51A10);
            cases++;
            if (memcmp(scrA, scrB, SCR_SZ) != 0 || la != lb) {
                size_t q, where = 0;
                for (q = 0; q < SCR_SZ; q++)
                    if (scrA[q] != scrB[q]) { where = q; break; }
                snprintf(g_why, sizeof g_why, "lines=%d n=%d byte %u/%u @%u",
                         lines, n, la, lb, (unsigned)where);
                fail("map_scroll_lines");
            }
        }

        /* ---- map_render_view (0x11EEE) -------------------------------- */
        for (i = 0; i < 8 && !g_fail; i++) {
            static const int modes[12] = { 9, 17, 21, 22, 23, 24, 25, 27, 28,
                                           29, 5, 3 };
            int     mode  = modes[rnd() % 12];
            int     pitch = (rnd() & 1) ? 456 : 320;
            int     w     = 4 + (int)(rnd() % 9);
            int     h     = 4 + (int)(rnd() % 5);
            int     ox    = (int)(rnd() % 16);
            int     oy    = (int)(rnd() % 8);
            int32_t sv[6], pv[6];
            int     k;
            uint16_t tick = (uint16_t)rnd();

            for (k = 0; k < 4 * 64 * 64; k++) cells[k] = (uint8_t)rnd();
            for (k = 0; k < 1024; k++) ctbl[4 * k] = (uint8_t)rnd();
            I32(0x53AC1) = 32;
            PTR(0x53A51) = cells;
            I32(0x53A69) = (int32_t)(uintptr_t)ctbl;
            I32(0x53C03) = mode;
            I32(0x53C0B) = (int32_t)(rnd() % 5);
            I32(0x53B07) = (int32_t)(rnd() % 33);
            I32(0x53B0B) = (int32_t)(rnd() % 12);
            I32(0x53AF1) = (int32_t)(rnd() % 2);
            I32(0x53AED) = (int32_t)(rnd() % 100);
            I32(0x53AF5) = (int32_t)(rnd() % 100);
            I32(0x51A93) = (rnd() & 1) ? -1 : (int32_t)(rnd() % 20);
            I32(0x53A40) = (int32_t)(rnd() % 3);
            I32(0x53C1F) = (int32_t)(rnd() % 20);
            I32(0x53A00) = (int32_t)(int16_t)rnd();
            I32(0x539F8) = (rnd() & 1) ? (int32_t)(int16_t)rnd() : (int32_t)tick;
            I32(0x539FC) = (rnd() % 4 == 0) ? 15 : (int32_t)(rnd() % 16);
            I32(0x539F4) = (int32_t)(int16_t)rnd();
            B8(0x51A10)  = (uint8_t)(rnd() % 193);
            *(volatile uint16_t *)(uintptr_t)G_TICK = tick;

            sv[0] = I32(0x53A00); sv[1] = I32(0x53A40); sv[2] = I32(0x539F8);
            sv[3] = I32(0x539FC); sv[4] = I32(0x539F4); sv[5] = I32(0x53C1F);

            for (k = 0; k < SCR_SZ; k++) scrA[k] = (uint8_t)rnd();
            memcpy(scrB, scrA, SCR_SZ);
            for (k = 0; k < EXP_SZ; k++) expA[k] = (uint8_t)rnd();
            memcpy(expB, expA, EXP_SZ);
            memset(dstA, 0x5A, BITMAP_SZ);
            memset(dstB, 0x5A, BITMAP_SZ);
            PTR(0x53AFF) = scrA; PTR(0x53B03) = expA;
            ORIG_RENDER(dstA, pitch, w, h, ox, oy);
            pv[0] = I32(0x53A00); pv[1] = I32(0x53A40); pv[2] = I32(0x539F8);
            pv[3] = I32(0x539FC); pv[4] = I32(0x539F4); pv[5] = I32(0x53C1F);

            I32(0x53A00) = sv[0]; I32(0x53A40) = sv[1]; I32(0x539F8) = sv[2];
            I32(0x539FC) = sv[3]; I32(0x539F4) = sv[4]; I32(0x53C1F) = sv[5];
            PTR(0x53AFF) = scrB; PTR(0x53B03) = expB;
            map_render_view(dstB, pitch, w, h, ox, oy);
            cases++;
            if (memcmp(dstA, dstB, BITMAP_SZ) != 0
                || memcmp(scrA, scrB, SCR_SZ) != 0
                || memcmp(expA, expB, EXP_SZ) != 0
                || I32(0x53A00) != pv[0] || I32(0x53A40) != pv[1]
                || I32(0x539F8) != pv[2] || I32(0x539FC) != pv[3]
                || I32(0x539F4) != pv[4] || I32(0x53C1F) != pv[5]) {
                snprintf(g_why, sizeof g_why,
                         "mode=%d pitch=%d w=%d h=%d ox=%d oy=%d", mode,
                         pitch, w, h, ox, oy);
                fail("map_render_view");
            }
        }

        /* ---- map_reveal_cursor (0x122DC) ------------------------------ */
        for (i = 0; i < 8 && !g_fail; i++) {
            int mode = (int)(rnd() % 8);
            int x    = (int)(rnd() % 40) - 4;
            int y    = (int)(rnd() % 40) - 4;
            int k;

            for (k = 0; k < 4 * 64 * 64; k++) cells[k] = (uint8_t)rnd();
            for (k = 0; k < 1024; k++) ctbl[4 * k] = (uint8_t)rnd();
            memcpy(cellA, cells, 4 * 64 * 64);
            I32(0x51A83) = mode;
            I32(0x53AB1) = x; I32(0x53AB5) = y;
            I32(0x53AC1) = 32;
            I32(0x53AA9) = 0; I32(0x53AAD) = 0;
            I32(0x51A87) = 32; I32(0x51A8B) = 32;
            I32(0x53A40) = (int32_t)(rnd() % 3);
            I32(0x53C1F) = (int32_t)(rnd() % 20);
            PTR(0x53A51) = cells; I32(0x53A69) = (int32_t)(uintptr_t)ctbl;

            memset(bitmap, 0x3C, BITMAP_SZ);
            ORIG_REVEAL();
            memcpy(dstA, bitmap, BITMAP_SZ);
            memcpy(cellB, cells, 4 * 64 * 64);

            memcpy(cells, cellA, 4 * 64 * 64);
            memset(bitmap, 0x3C, BITMAP_SZ);
            map_reveal_cursor();
            cases++;
            if (memcmp(dstA, bitmap, BITMAP_SZ) != 0
                || memcmp(cellB, cells, 4 * 64 * 64) != 0) {
                snprintf(g_why, sizeof g_why, "mode=%d x=%d y=%d", mode, x, y);
                fail("map_reveal_cursor");
            }
        }

        /* ---- map_draw_cursor (0x1ACF3) -------------------------------- */
        for (i = 0; i < 8 && !g_fail; i++) {
            int pitch = 456;
            int n     = 1 + (int)(rnd() % 8);
            int want  = (int)(rnd() % (n + 1));
            int k;

            B8(0x51AAB) = (uint8_t)(rnd() & 1);
            B8(0x51AAC) = (uint8_t)(rnd() & 1);
            I32(0x53ABD) = (int32_t)(rnd() % 12);
            I32(0x53AB9) = (int32_t)(rnd() % 12);
            I32(0x51A0C) = (int32_t)(rnd() % 256);
            I32(0x53C0B) = (int32_t)(rnd() % 5);
            I32(0x53AC1) = 32;
            PTR(0x53A51) = cells; I32(0x53A69) = (int32_t)(uintptr_t)ctbl;

            I32(0x53BEB) = n;
            for (k = 0; k < 80 * n; k++) recs[k] = (uint8_t)rnd();
            for (k = 0; k < n; k++) {
                recs[80 * k + 0] = (uint8_t)(rnd() % 32);
                recs[80 * k + 1] = (uint8_t)(rnd() % 32);
                recs[80 * k + 2] = (uint8_t)(rnd() % 4);
                recs[80 * k + 5] = (uint8_t)(rnd() & 1);
                recs[80 * k + 31] = (uint8_t)(rnd() & 0xFF);
                recs[80 * k + 6] = (uint8_t)(rnd() & 0xFF);
                recs[80 * k + 7] = (uint8_t)(rnd() & 0xFF);
                *(uint16_t *)(recs + 80 * k + 64) = (uint16_t)rnd();
                *(uint16_t *)(recs + 80 * k + 66) = (uint16_t)rnd();
            }
            if (want < n) {
                I32(0x53AB1) = recs[80 * want + 0];
                I32(0x53AB5) = recs[80 * want + 1];
                recs[80 * want + 5] = 0;
            } else {
                /* Cursor sits on no record.  The cell must still be a real
                 * in-map cell: map_cell_info (0x12E38) has no bounds check,
                 * so a sentinel like 0x1234/0x5678 makes the machine code
                 * read cells[] ~2.8 MB past the table and fault intermittently
                 * (0xC0000005).  The game never lets the cursor leave the map
                 * -- pick a free cell inside the 32x32 view instead. */
                int t, tries;
                I32(0x53AB1) = (int32_t)(rnd() % 32);
                I32(0x53AB5) = (int32_t)(rnd() % 32);
                for (tries = 0; tries <= n; tries++) {
                    for (t = 0; t < n
                         && (recs[80 * t + 0] != (uint8_t)I32(0x53AB1)
                             || recs[80 * t + 1] != (uint8_t)I32(0x53AB5)); t++)
                        ;
                    if (t == n) break;
                    if (++I32(0x53AB1) >= 32) {
                        I32(0x53AB1) = 0;
                        I32(0x53AB5) = (I32(0x53AB5) + 1) % 32;
                    }
                }
            }
            PTR(0x53A45) = recs;
            memcpy(recA, recs, 80 * 16);

            memset(dstA, 0x77, BITMAP_SZ);
            ORIG_DCURSOR(dstA, pitch);
            memcpy(recB, recs, 80 * 16);

            memcpy(recs, recA, 80 * 16);
            memset(dstB, 0x77, BITMAP_SZ);
            map_draw_cursor(dstB, pitch);
            cases++;
            if (memcmp(dstA, dstB, BITMAP_SZ) != 0 || memcmp(recB, recs, 80 * 16) != 0) {
                size_t q, where = 0;
                for (q = 0; q < BITMAP_SZ; q++)
                    if (dstA[q] != dstB[q]) { where = q; break; }
                snprintf(g_why, sizeof g_why, "n=%d want=%d @%u", n, want, (unsigned)where);
                fail("map_draw_cursor");
            }
        }
    }

    printf("%s: %ld cases, %d failures\n", g_fail ? "FAILED" : "PASS",
           cases, g_fail);    le_close(&le);
    return g_fail ? 1 : 0;
}
