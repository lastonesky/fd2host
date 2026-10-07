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

#define BITMAP_SZ (400 * 1024)
#define TILE_W    24
#define TILE_H    24
#define MIRROR    0x00070000u
#define G_TICK    (MIRROR + 0x46C)

static int  g_fail;
static char g_why[256];
static int  g_delays;

static void __cdecl stub_delay(unsigned ms) { (void)ms; g_delays++; }

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
    uint8_t *recs = (uint8_t *)malloc(80 * 16);
    uint8_t *cells = (uint8_t *)malloc(4 * 64 * 64);
    uint8_t *ctbl = (uint8_t *)malloc(4 * 1024);

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    install_hook(0x3790A, stub_delay);
    printf("mapcheck: redirected %d low-memory references to 0x%X\n",
           patch_lowmem_refs(&le, MIRROR), MIRROR);

    if (!bitmap || !tileset || !lmi || !dstA || !dstB || !recs || !cells || !ctbl)
        return 2;

    /* ---- tileset layout: table at +6, 4 sub-images at +22 ---- */
    {
        uint32_t off = 6 + 4 * 4;
        int i;
        for (i = 0; i < 4; i++) {
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

    PTR(0x53A49) = bitmap;
    PTR(0x53A4D) = tileset;
    PTR(0x53A45) = recs;

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
    }

    printf("%s: %ld cases, %d failures\n", g_fail ? "FAILED" : "PASS",
           cases, g_fail);    le_close(&le);
    return g_fail ? 1 : 0;
}
