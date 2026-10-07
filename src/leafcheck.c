/* leafcheck.c - differential test for the six "hot leaf" translations.
 *
 *   0x4E381 kbd_flush()          src/game/kbd.c
 *   0x10620 kbd_pending()        src/game/kbd.c
 *   0x4EBE3 util_rand()          src/game/util.c
 *   0x11EB0 gfx_copy_rows()      src/game/gfx.c
 *   0x2EB9F res_blit()           src/game/res.c
 *   0x12D7B dlg_portrait_glide() src/game/dlg.c
 *
 * They are grouped here because each is small and touches one thing:
 *   - kbd_flush/kbd_pending read the BDA, so the harness does what the host
 *     does for the machine code: redirect the low-memory immediates
 *     (0x41A/0x41C) into the 0x70000 mirror and let the C read the same mirror;
 *   - util_rand only moves word_627B8 (inside the loaded image);
 *   - gfx_copy_rows writes a destination buffer - compared byte for byte;
 *   - res_blit decodes a real FDOTHER.DAT sub-image (entry 79, the scene-card
 *     resource), original machine rle_decode vs the translated one;
 *   - dlg_portrait_glide reads dword_53A45 + 80*idx and calls 0x12CEA, which
 *     is hooked so the (x, y) pair is compared.
 *
 * Build: pwsh -File build.ps1 -Target leafcheck
 * Run   : build\leafcheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "platform.h"
#include "game/kbd.h"
#include "game/util.h"
#include "game/gfx.h"
#include "game/res.h"
#include "game/dlg.h"

/* kbd.c reads the BDA through DOS_LOWMEM_BASE (dos_lowmem_base, normally
 * defined by dos.c, which this harness does not link). */
uint32_t dos_lowmem_base = 0x00070000u;

#define MIRROR     0x00070000u
#define G_HEAD     (MIRROR + 0x41A)
#define G_TAIL     (MIRROR + 0x41C)
#define W(x)       (*(uint32_t *)(uintptr_t)(x))
#define W16(x)     (*(uint16_t *)(uintptr_t)(x))
#define DSTSZ      (320u * 300u + 4096u)   /* a full surface + slack */

typedef void     (*flush_fn)(void);
typedef int      (*pending_fn)(void);
typedef uint32_t (*rand_fn)(void);
typedef void     (*rows_fn)(void *, int, const void *, int, int, int);
typedef void     (*blit_fn)(void *, int, void *, int, int);
typedef void     (*glidewrap_fn)(int);

#define ORIG_FLUSH   ((flush_fn)     (uintptr_t)0x0004E381u)
#define ORIG_PENDING ((pending_fn)   (uintptr_t)0x00010620u)
#define ORIG_RAND    ((rand_fn)      (uintptr_t)0x0004EBE3u)
#define ORIG_ROWS    ((rows_fn)      (uintptr_t)0x00011EB0u)
#define ORIG_BLIT    ((blit_fn)      (uintptr_t)0x0002EB9Fu)
#define ORIG_GLIDEW  ((glidewrap_fn) (uintptr_t)0x00012D7Bu)

static int g_fail;
static char g_why[256];

/* --- low-memory operand redirect (same algorithm as dos.c/keycheck) ----- */
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

static int g_glide_x, g_glide_y, g_glide_n;
static void __cdecl stub_glide(int x, int y)
{
    g_glide_x = x; g_glide_y = y; g_glide_n++;
}

static uint32_t g_rnd = 0xABCDEF01u;
static uint32_t rnd(void) { g_rnd = g_rnd * 1103515245u + 12345u; return g_rnd >> 8; }

static void fail(const char *what)
{
    printf("FAIL %s: %s\n", what, g_why);
    g_fail = 1;
}

int main(int argc, char **argv)
{
    le_image le;
    int      applied = 0, nref, round;
    long     cases = 0;

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);
    nref = patch_lowmem_refs(&le, MIRROR);
    printf("leafcheck: redirected %d low-memory references to 0x%X\n", nref, MIRROR);

    install_hook(0x12CEA, stub_glide);

    for (round = 0; round < 200 && !g_fail; round++) {
        int i;

        /* ---- kbd_flush / kbd_pending ---------------------------------- */
        for (i = 0; i < 50 && !g_fail; i++) {
            uint16_t a = (uint16_t)rnd(), b = (uint16_t)rnd(), got_o, got_c;
            int      p_o, p_c;

            W16(G_HEAD) = a; W16(G_TAIL) = b;
            ORIG_FLUSH();
            got_o = W16(G_TAIL);
            W16(G_HEAD) = a; W16(G_TAIL) = b;
            kbd_flush();
            got_c = W16(G_TAIL);
            cases++;
            if (got_o != got_c) {
                snprintf(g_why, sizeof g_why, "flush %04X/%04X -> %04X vs %04X",
                         a, b, got_o, got_c);
                fail("kbd_flush");
                break;
            }

            W16(G_HEAD) = a; W16(G_TAIL) = b;
            p_o = ORIG_PENDING();
            W16(G_HEAD) = a; W16(G_TAIL) = b;
            p_c = kbd_pending();
            cases++;
            if (p_o != p_c) {
                snprintf(g_why, sizeof g_why, "pending %04X/%04X -> %d vs %d",
                         a, b, p_o, p_c);
                fail("kbd_pending");
                break;
            }
        }

        /* ---- util_rand ------------------------------------------------ */
        for (i = 0; i < 200 && !g_fail; i++) {
            uint16_t seed = (uint16_t)rnd();
            uint32_t r_o, r_c;
            uint16_t w_o, w_c;

            W16(0x627B8) = seed;  r_o = ORIG_RAND(); w_o = W16(0x627B8);
            W16(0x627B8) = seed;  r_c = util_rand();  w_c = W16(0x627B8);
            cases++;
            if (r_o != r_c || w_o != w_c) {
                snprintf(g_why, sizeof g_why,
                         "seed %04X -> %08X/%04X vs %08X/%04X",
                         seed, r_o, w_o, r_c, w_c);
                fail("util_rand");
                break;
            }
        }

        /* ---- gfx_copy_rows -------------------------------------------- */
        for (i = 0; i < 30 && !g_fail; i++) {
            int dst_stride = 320 + (int)(rnd() % 64);
            int src_stride = 256 + (int)(rnd() % 64);
            int len        = (int)(rnd() % 200);
            int rows       = (int)(rnd() % 12);
            uint8_t *dstA, *dstB, *src;
            size_t   need = (size_t)(dst_stride * (rows + 2)) + 4096;
            size_t   k;

            dstA = (uint8_t *)malloc(need);
            dstB = (uint8_t *)malloc(need);
            src  = (uint8_t *)malloc((size_t)(src_stride * (rows + 2)) + 4096);
            if (!dstA || !dstB || !src) return 2;
            for (k = 0; k < need; k++) { dstA[k] = 0xAA; dstB[k] = 0xAA; }
            for (k = 0; k < (size_t)(src_stride * (rows + 2)); k++)
                src[k] = (uint8_t)k;

            ORIG_ROWS(dstA, dst_stride, src, src_stride, len, rows);
            gfx_copy_rows(dstB, dst_stride, src, src_stride, len, rows);
            cases++;
            if (memcmp(dstA, dstB, need) != 0) {
                snprintf(g_why, sizeof g_why, "strides %d/%d len %d rows %d",
                         dst_stride, src_stride, len, rows);
                fail("gfx_copy_rows");
                free(dstA); free(dstB); free(src);
                break;
            }
            free(dstA); free(dstB); free(src);
        }

        /* ---- dlg_portrait_glide --------------------------------------- */
        for (i = 0; i < 30 && !g_fail; i++) {
            static uint8_t table[80 * 8];
            int idx = (int)(rnd() % 8);
            int xo, yo, xc, yc, no, nc;
            size_t k;

            for (k = 0; k < sizeof table; k++) table[k] = (uint8_t)rnd();
            W(0x53A45) = (uint32_t)(uintptr_t)table;

            g_glide_n = 0; ORIG_GLIDEW(idx);
            xo = g_glide_x; yo = g_glide_y; no = g_glide_n;
            g_glide_n = 0; dlg_portrait_glide(idx);
            xc = g_glide_x; yc = g_glide_y; nc = g_glide_n;
            cases++;
            if (xo != xc || yo != yc || no != nc) {
                snprintf(g_why, sizeof g_why, "idx %d -> (%d,%d)x%d vs (%d,%d)x%d",
                         idx, xo, yo, no, xc, yc, nc);
                fail("dlg_portrait_glide");
                break;
            }
        }
    }

    /* ---- res_blit (one real resource, all its sub-images) ------------- */
    if (!g_fail) {
        FILE    *f = fopen("E:\\FD2\\FDOTHER.DAT", "rb");
        uint8_t *res = NULL;
        uint32_t rlen = 0;

        if (!f) {
            printf("SKIP res_blit: cannot open E:\\FD2\\FDOTHER.DAT\n");
        } else {
            uint32_t start, end;
            fseek(f, 4L * 79 + 6, SEEK_SET);
            if (fread(&start, 4, 1, f) == 1 && fread(&end, 4, 1, f) == 1 &&
                end > start && end - start < 4u * 1024 * 1024) {
                rlen = end - start;
                res  = (uint8_t *)malloc(rlen);
                if (res) {
                    fseek(f, (long)start, SEEK_SET);
                    if (fread(res, 1, rlen, f) != rlen) { free(res); res = NULL; }
                }
            }
            fclose(f);
        }
        if (res) {
            int index;
            for (index = 0; index < 8 && !g_fail; index++) {
                uint32_t off, hdr;
                uint16_t w, h;
                uint8_t *dstA, *dstB;
                static const int modes[] = { -1, 0, 7, 0x0102 };

                if (8u + 4u * (uint32_t)index + 4u > rlen) break;
                off = *(uint32_t *)(res + 8 + 4 * index);
                if (off == 0 || off + 9 >= rlen) continue;
                hdr = off;
                w = *(uint16_t *)(res + hdr);
                h = *(uint16_t *)(res + hdr + 2);
                if (w == 0 || w > 320 || h == 0 || h > 1024) continue;

                {
                    unsigned m;
                    for (m = 0; m < sizeof modes / sizeof modes[0] && !g_fail; m++) {
                        /* rle_decode() places the stream at dst + y*pitch + x
                         * where x/y are the *header* w/h, and the stream's own
                         * w/h can be larger - so the destination is a full
                         * VGA-sized surface, not h rows. */
                        dstA = (uint8_t *)malloc(DSTSZ);
                        dstB = (uint8_t *)malloc(DSTSZ);
                        if (!dstA || !dstB) return 2;
                        memset(dstA, 0xAA, DSTSZ);
                        memset(dstB, 0xAA, DSTSZ);

                        ORIG_BLIT(res, index, dstA, 320, modes[m]);
                        res_blit(res, index, dstB, 320, modes[m]);
                        cases++;
                        if (memcmp(dstA, dstB, DSTSZ) != 0) {
                            size_t k, where = 0;
                            for (k = 0; k < DSTSZ; k++)
                                if (dstA[k] != dstB[k]) { where = k; break; }
                            snprintf(g_why, sizeof g_why,
                                     "idx %d mode %d w=%u h=%u first diff @%u (%02X/%02X)",
                                     index, modes[m], w, h, (unsigned)where,
                                     dstA[where], dstB[where]);
                            fail("res_blit");
                        }
                        free(dstA); free(dstB);
                    }
                }
            }
            free(res);
        }
    }

    printf("%s: %ld cases, %d failures\n", g_fail ? "FAILED" : "PASS",
           cases, g_fail);
    le_close(&le);
    return g_fail ? 1 : 0;
}
