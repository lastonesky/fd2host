/* gfxcheck.c - differential test for the graphics blitter translation
 * (src/game/gfx.c vs the original machine code at 0x4ECBF / 0x4EC7C /
 * 0x4ED0B / 0x4ED34 / 0x4ED7A / 0x4EEE0).
 *
 * Loads FD2.EXE with the LE loader, then for each random case runs both the
 * original code (direct call through the object-0 address) and the C
 * translation, and asserts the touched memory (including a sentinel margin
 * around the real rectangle) is byte-identical. Also asserts the original
 * scratch globals (0x627xx) match the mirrored C globals.
 *
 * Build: pwsh -File build.ps1 -Target gfxcheck
 * Run   : build\gfxcheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/gfx.h"

typedef void (__cdecl *save_fn)(void *, int, int, const void *, int, int);
typedef void (__cdecl *restore_fn)(const void *, void *, int);
typedef void (__cdecl *blit_fn)(void *, const void *, int);
typedef void (__cdecl *glyph_fn)(const void *, int, void *, int, int, int, int);
typedef void (__cdecl *expand_fn)(const void *, void *, int);

#define ORIG_SAVE      ((save_fn)   (uintptr_t)0x4ECBF)
#define ORIG_RESTORE   ((restore_fn)(uintptr_t)0x4EC7C)
#define ORIG_BLOCK     ((blit_fn)   (uintptr_t)0x4ED0B)
#define ORIG_TRANSP    ((blit_fn)   (uintptr_t)0x4ED34)
#define ORIG_GLYPH     ((glyph_fn)  (uintptr_t)0x4ED7A)
#define ORIG_EXPAND    ((expand_fn) (uintptr_t)0x4EEE0)

/* guest scratch globals */
#define G_REC_W  (*(const uint16_t *)(uintptr_t)0x627B4u)
#define G_REC_H  (*(const uint16_t *)(uintptr_t)0x627B6u)
#define G_PSTR   (*(const uint16_t *)(uintptr_t)0x627A3u)
#define G_PFG    (*(const uint8_t  *)(uintptr_t)0x627A5u)
#define G_PFILL  (*(const uint8_t  *)(uintptr_t)0x627A6u)
#define G_PSHDW  (*(const uint8_t  *)(uintptr_t)0x627A7u)
#define G_PDEST  (*(const uint32_t *)(uintptr_t)0x627A8u)
#define G_GTAB   (*(const uint32_t *)(uintptr_t)0x627ACu)
#define G_GIDX   (*(const uint32_t *)(uintptr_t)0x627B0u)
#define G_PHASE  ((const uint8_t  *)(uintptr_t)0x627C8u)

#define MARGIN 64

static uint32_t seed = 0x1234ABCDu;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}
static int rrange(int lo, int hi) { return lo + (int)(rnd() % (uint32_t)(hi - lo + 1)); }

static int failures;
static unsigned cases_run;

static void fill_rand(uint8_t *p, size_t n) { size_t i; for (i = 0; i < n; i++) p[i] = (uint8_t)rnd(); }

static void cmp_buf(const char *what, unsigned id, const uint8_t *a,
                    const uint8_t *b, size_t n)
{
    size_t k;
    for (k = 0; k < n; k++)
        if (a[k] != b[k]) {
            printf("FAIL %s case %u: buffer diff @+%u orig=%02X ours=%02X\n",
                   what, id, (unsigned)k, a[k], b[k]);
            failures++;
            return;
        }
}

#define CHECK_GLOBAL(id, what, gv, ov)                                        \
    do {                                                                      \
        if ((uint32_t)(gv) != (uint32_t)(ov)) {                               \
            printf("FAIL %s case %u: global %s orig=%u ours=%u\n",            \
                   what, id, #ov, (unsigned)(gv), (unsigned)(ov));            \
            failures++;                                                       \
        }                                                                     \
    } while (0)

/* ---- save rectangle then restore it onto a fresh surface ------------ */
static void test_save_restore(unsigned id)
{
    int w = rrange(1, 40), h = rrange(1, 30);
    int stride = rrange(1, 200), offset = rrange(0, 48);
    int stride2 = rrange(1, 200);
    size_t surf_len = (size_t)offset + (size_t)h * stride + MARGIN;
    size_t rec_len  = 8 + (size_t)w * h + MARGIN;
    size_t dst_len  = (size_t)offset + (size_t)h * stride2 + MARGIN;
    uint8_t *surf = malloc(surf_len), *recA = malloc(rec_len), *recB = malloc(rec_len);
    uint8_t *dstA = malloc(dst_len), *dstB = malloc(dst_len);

    if (!surf || !recA || !recB || !dstA || !dstB) { printf("oom\n"); exit(2); }
    fill_rand(surf, surf_len);
    memset(recA, 0x5A, rec_len); memset(recB, 0x5A, rec_len);
    memset(dstA, 0xC3, dst_len); memset(dstB, 0xC3, dst_len);

    ORIG_SAVE(recA, w, h, surf, offset, stride);
    gfx_save_rect(recB, w, h, surf, offset, stride);
    cases_run++;
    cmp_buf("save", id, recA, recB, rec_len);
    CHECK_GLOBAL(id, "save", G_REC_W, gfx_rec_w);
    CHECK_GLOBAL(id, "save", G_REC_H, gfx_rec_h);

    if (!failures) {
        ORIG_RESTORE(recA, dstA, stride2);
        gfx_restore_rect(recB, dstB, stride2);
        cmp_buf("restore", id, dstA, dstB, dst_len);
        CHECK_GLOBAL(id, "restore", G_REC_W, gfx_rec_w);
        CHECK_GLOBAL(id, "restore", G_REC_H, gfx_rec_h);
    }
    free(surf); free(recA); free(recB); free(dstA); free(dstB);
}

/* ---- header-prefixed block blit (opaque / transparent) -------------- */
static void test_blit(unsigned id, int transparent)
{
    int w = rrange(1, 40), h = rrange(1, 30), stride = rrange(1, 200);
    size_t src_len = 4 + (size_t)w * h + MARGIN;
    size_t dst_len = (size_t)h * stride + MARGIN;
    uint8_t *src = malloc(src_len), *dA = malloc(dst_len), *dB = malloc(dst_len);
    size_t k;

    if (!src || !dA || !dB) { printf("oom\n"); exit(2); }
    *(uint16_t *)src = (uint16_t)w;
    *(uint16_t *)(src + 2) = (uint16_t)h;
    fill_rand(src + 4, (size_t)w * h);
    if (transparent)                       /* ~half the pixels transparent */
        for (k = 0; k < (size_t)w * h; k++)
            if (rnd() & 1) src[4 + k] = 0;
    memset(dA, 0x99, dst_len); memset(dB, 0x99, dst_len);

    if (transparent) {
        ORIG_TRANSP(dA, src, stride);
        gfx_blit_transparent(dB, src, stride);
        CHECK_GLOBAL(id, "transparent", G_PSTR, gfx_pen_stride);
    } else {
        ORIG_BLOCK(dA, src, stride);
        gfx_blit_block(dB, src, stride);
    }
    cases_run++;
    cmp_buf(transparent ? "transparent" : "block", id, dA, dB, dst_len);
    free(src); free(dA); free(dB);
}

/* ---- 16x16 glyph render --------------------------------------------- */
static void test_glyph(unsigned id)
{
    int index = rrange(0, 31);
    int stride = rrange(1, 200);
    int fg = (int)rnd() & 0xFF, shadow = (int)rnd() & 0xFF, fill = (int)rnd() & 0xFF;
    size_t tab_len = 32 * 32;
    size_t dst_len = (size_t)16 * stride + 16 + MARGIN;
    uint8_t *tab = malloc(tab_len), *dA = malloc(dst_len), *dB = malloc(dst_len);

    if (!tab || !dA || !dB) { printf("oom\n"); exit(2); }
    fill_rand(tab, tab_len);
    if ((rnd() & 3) == 0) fill = 0;
    if ((rnd() & 7) == 0) index = 10;       /* exercise the skipped glyph */
    memset(dA, 0x7E, dst_len); memset(dB, 0x7E, dst_len);

    ORIG_GLYPH(tab, index, dA, stride, fg, shadow, fill);
    gfx_draw_glyph(tab, index, dB, stride, fg, shadow, fill);
    cases_run++;
    cmp_buf("glyph", id, dA, dB, dst_len);
    CHECK_GLOBAL(id, "glyph", G_PSTR,   gfx_pen_stride);
    CHECK_GLOBAL(id, "glyph", G_PFG,    gfx_pen_fg);
    CHECK_GLOBAL(id, "glyph", G_PFILL,  gfx_pen_fill);
    CHECK_GLOBAL(id, "glyph", G_PSHDW,  gfx_pen_shadow);
    CHECK_GLOBAL(id, "glyph", G_GTAB,   gfx_glyph_tab);
    CHECK_GLOBAL(id, "glyph", G_GIDX,   gfx_glyph_idx);
    free(tab); free(dA); free(dB);
}

/* ---- 16-phase scanline expansion ------------------------------------ */
static void test_expand(unsigned id)
{
    int start = rrange(0, 15);
    size_t src_len = 4 + 191 * 320 + 4 + 312 + MARGIN;
    size_t dst_len = 192 * 320 + MARGIN;
    uint8_t *src = malloc(src_len), *dA = malloc(dst_len), *dB = malloc(dst_len);

    if (!src || !dA || !dB) { printf("oom\n"); exit(2); }
    fill_rand(src, src_len);
    memset(dA, 0x11, dst_len); memset(dB, 0x11, dst_len);

    ORIG_EXPAND(src, dA, start);
    gfx_expand_scanlines(src, dB, start);
    cases_run++;
    cmp_buf("expand", id, dA, dB, dst_len);
    free(src); free(dA); free(dB);
}

int main(int argc, char **argv)
{
    le_image le;
    int applied = 0;
    unsigned i, phase_bad = 0;

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    for (i = 0; i < 16; i++)
        if (G_PHASE[i] != gfx_phase_table[i]) phase_bad++;
    if (phase_bad) {
        printf("FAIL: byte_627C8 table differs from embedded constant\n");
        return 1;
    }

    for (i = 0; i < 300; i++) test_save_restore(1000 + i);
    for (i = 0; i < 300; i++) test_blit(2000 + i, 0);
    for (i = 0; i < 300; i++) test_blit(3000 + i, 1);
    for (i = 0; i < 400; i++) test_glyph(4000 + i);
    for (i = 0; i < 150; i++) test_expand(5000 + i);

    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
