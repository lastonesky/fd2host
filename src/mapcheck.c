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
#include "game/fade.h"
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
typedef void (*popups_fn)(void);
#define ORIG_POPUPS  ((popups_fn)     (uintptr_t)0x0001DF58u)
typedef int  (*revealr_fn)(int, int, uint8_t *, int, int, int);
#define ORIG_REVEALR ((revealr_fn)    (uintptr_t)0x00014818u)
typedef void (*icons_fn)(int, int, int, const uint8_t *);
#define ORIG_ICONS   ((icons_fn)      (uintptr_t)0x0001C2DAu)
#define ORIG_ICONSANIM ((icons_fn)    (uintptr_t)0x0001C4CCu)
typedef void (*slide_fn)(int, int);
#define ORIG_SLIDE   ((slide_fn)      (uintptr_t)0x00012CEAu)

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
static void __cdecl stub_wait(int n) { (void)n; }
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

/* --- 0x4E310/0x4E31C/0x32230/0x11CAC (round 38) -------------------------
 *
 * pal_anim_step (0x4E31C) writes the DAC with *inline* `out dx,al`
 * instructions, so the C side's `outp` (0x37AE5) hook never sees the original
 * side. Both sides feed one event log: the C goes through stub_outp, the
 * machine code through a narrow VEH that only claims EXCEPTION_PRIV_INSTRUCTION
 * inside 0x4E31C and only for the `out dx,al` opcode (0xEE). */
typedef uint16_t (*tickw_fn)(void);
typedef void (*palstep_fn)(void);
typedef void (*ping_fn)(int);
typedef void (*view_fn)(int);
#define ORIG_TICKW   ((tickw_fn)   (uintptr_t)0x0004E310u)
#define ORIG_PALSTEP ((palstep_fn) (uintptr_t)0x0004E31Cu)
#define ORIG_PING    ((ping_fn)    (uintptr_t)0x00032230u)
#define ORIG_VIEW    ((view_fn)    (uintptr_t)0x00011CACu)

#define DAC_MAX 512
static uint32_t g_dac[DAC_MAX], g_dac_saved[DAC_MAX];
static int      g_dac_n, g_dac_n_saved, g_veh_n;

static void dac_rec(unsigned port, int value)
{
    if (g_dac_n < DAC_MAX)
        g_dac[g_dac_n++] = ((uint32_t)(port & 0xFFFFu) << 8)
                         | (uint32_t)(value & 0xFF);
}

static int __cdecl stub_outp(unsigned port, int value)
{
    dac_rec(port, value);
    return 0;
}

static LONG CALLBACK dac_veh(EXCEPTION_POINTERS *ep)
{
    CONTEXT *c = ep->ContextRecord;
    uint8_t *p;

    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_PRIV_INSTRUCTION)
        return EXCEPTION_CONTINUE_SEARCH;
    if (c->Eip < 0x0004E31Cu || c->Eip >= 0x0004E31Cu + 101u)
        return EXCEPTION_CONTINUE_SEARCH;
    p = (uint8_t *)(uintptr_t)c->Eip;
    if (p[0] != 0xEE)                 /* only `out dx, al` is expected here */
        return EXCEPTION_CONTINUE_SEARCH;
    dac_rec((unsigned)(c->Edx & 0xFFFFu), (int)(c->Eax & 0xFFu));
    g_veh_n++;
    c->Eip += 1;
    return EXCEPTION_CONTINUE_EXECUTION;
}

#define SFX_MAX 16
static int         g_sfx_n, g_sfx_n_saved;
static int         g_sfx_idx[SFX_MAX], g_sfx_idx_saved[SFX_MAX];
static int         g_sfx_loops[SFX_MAX], g_sfx_loops_saved[SFX_MAX];
static const void *g_sfx_bank[SFX_MAX], *g_sfx_bank_saved[SFX_MAX];

static int __cdecl stub_sfx(const void *bank, int index, int loops)
{
    if (g_sfx_n < SFX_MAX) {
        g_sfx_bank[g_sfx_n]  = bank;
        g_sfx_idx[g_sfx_n]   = index;
        g_sfx_loops[g_sfx_n] = loops;
        g_sfx_n++;
    }
    return 0;
}

/* Game globals touched by the four functions. */
#define byte_52725  ((const uint8_t *)(uintptr_t)0x00052725u)
#define byte_54132  (*(uint8_t  *)(uintptr_t)0x00054132u)
#define dword_53EEC (*(void    **)(uintptr_t)0x00053EECu)
#define word_60000  (*(uint16_t *)(uintptr_t)0x00060000u)
#define byte_60002  (*(uint8_t  *)(uintptr_t)0x00060002u)
#define pal_data    ((uint8_t     *)(uintptr_t)0x00060003u)

/* map_view_update writes 192 rows of 312 bytes at 0xA0504, so the VGA window
 * fits in 64 KiB. */
#define VGA_SZ 0x10000
static uint8_t *res_bmp, *res_vga, *res_scr, *res_exp, *res_cells, *res_recs;

/* int32 game globals the view pipeline mutates; snapshot per case. */
static const uint32_t g_view_scalars[] = {
    0x53C0F, 0x53C0B, 0x53C07, 0x53AF5, 0x53A04, 0x53A08, 0x51A0C,
    0x53A00, 0x539F8, 0x539FC, 0x539F4, 0x53A40, 0x53C1F
};
#define N_VIEW_SCALARS ((int)(sizeof g_view_scalars / sizeof g_view_scalars[0]))

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

/* Deterministic input state for one map_view_update case. Called twice per
 * case with the same g_rnd (the caller restores it), so the machine-code run
 * and the C run start from identical memory. */
static void setup_view(uint8_t *bitmap, uint8_t *scr, uint8_t *exp,
                       uint8_t *cells, uint8_t *ctbl, uint8_t *recs,
                       int *flag, int *ox, int *oy)
{
    static const int modes[12] = { 9, 17, 21, 22, 23, 24, 25, 27, 28,
                                   29, 5, 3 };
    uint16_t tick;
    int      k, n;

    switch (rnd() % 3) {
    case 0:  *flag = 0;  break;
    case 1:  *flag = 1;  break;
    default: *flag = -1; break;
    }
    *ox = (int)(rnd() % 20);                    /* 32-wide map, 13-wide view */
    *oy = (int)(rnd() % 24);                    /* 32-high map, 8-high view  */

    /* cell / tile / record data - all in range so map_cell_info never reads
     * past the tables (mapcheck's historical ASLR-sensitive fault). */
    PTR(0x53A51) = cells;
    I32(0x53A69) = (int32_t)(uintptr_t)ctbl;
    I32(0x53AC1) = 32;
    for (k = 0; k < 4 * 64 * 64; k++) cells[k] = (uint8_t)rnd();
    for (k = 0; k < 1024; k++) ctbl[4 * k] = (uint8_t)rnd();

    n = 1 + (int)(rnd() % 6);
    I32(0x53BEB) = n;
    PTR(0x53A45) = recs;
    memset(recs, 0, 80 * 16);
    for (k = 0; k < 80 * n; k++) recs[k] = (uint8_t)rnd();
    for (k = 0; k < n; k++) {
        recs[80 * k + 0]  = (uint8_t)(rnd() % 32);
        recs[80 * k + 1]  = (uint8_t)(rnd() % 32);
        recs[80 * k + 2]  = (uint8_t)(rnd() % 4);
        recs[80 * k + 3]  = (uint8_t)(rnd() % 4);
        recs[80 * k + 4]  = (uint8_t)(rnd() & 1);
        recs[80 * k + 5]  = (uint8_t)(rnd() & 0xFF);
        recs[80 * k + 7]  = (uint8_t)(rnd() & 0xFF);
        recs[80 * k + 31] = (uint8_t)(rnd() & 0xFF);
        recs[80 * k + 32] = (uint8_t)(rnd() & 0xFF);
        recs[80 * k + 38] = (uint8_t)(rnd() & 1);
        *(uint16_t *)(recs + 80 * k + 64) = (uint16_t)rnd();
        *(uint16_t *)(recs + 80 * k + 66) = (uint16_t)rnd();
    }

    /* view / render-core globals (mirrors the map_render_view section). */
    I32(0x53AA9) = *ox; I32(0x53AAD) = *oy;
    I32(0x51A87) = 13; I32(0x51A8B) = 8;
    I32(0x53C03) = modes[rnd() % 12];
    I32(0x53C0B) = (int32_t)(rnd() % 4);   /* must stay < 4: dlg_portrait_draw */
    I32(0x53C07) = (int32_t)(rnd() % 4);
    I32(0x53C0F) = (int32_t)(int16_t)rnd();
    I32(0x53B07) = (int32_t)(rnd() % 33);
    I32(0x53B0B) = (int32_t)(rnd() % 12);
    I32(0x53AF1) = (int32_t)(rnd() % 2);
    I32(0x53AED) = (int32_t)(rnd() % 100);
    I32(0x53AF5) = (int32_t)(rnd() % 100);
    I32(0x51A93) = (rnd() & 1) ? -1 : (int32_t)(rnd() % 20);
    I32(0x53A40) = (int32_t)(rnd() % 3);
    I32(0x53C1F) = (int32_t)(rnd() % 20);
    I32(0x53A00) = (int32_t)(int16_t)rnd();
    I32(0x539F8) = (int32_t)(int16_t)rnd();
    I32(0x539FC) = (rnd() % 4 == 0) ? 15 : (int32_t)(rnd() % 16);
    I32(0x539F4) = (int32_t)(int16_t)rnd();
    B8(0x51A10)  = (uint8_t)(rnd() % 193);

    /* cursor / reveal state. */
    I32(0x51A83) = (int32_t)(rnd() % 8);
    I32(0x53AB1) = (int32_t)(rnd() % 32);
    I32(0x53AB5) = (int32_t)(rnd() % 32);
    B8(0x51AAB)  = (uint8_t)(rnd() & 1);
    B8(0x51AAC)  = (uint8_t)(rnd() & 1);
    I32(0x53ABD) = (int32_t)(rnd() % 12);
    I32(0x53AB9) = (int32_t)(rnd() % 12);
    I32(0x51A0C) = (int32_t)(rnd() % 256);
    I32(0x53A04) = (int32_t)(rnd() & 1);
    I32(0x53A08) = (int32_t)(int16_t)rnd();

    /* palette-animation state + BIOS tick (gate word_60000 near the tick so
     * pal_anim_step actually uploads on most of the flag==0 cases). */
    tick = (uint16_t)rnd();
    *(volatile uint16_t *)(uintptr_t)G_TICK = tick;
    word_60000 = (uint16_t)(tick - (rnd() % 4));
    byte_60002 = (uint8_t)(rnd() % 16);

    memset(bitmap, 0x66, BITMAP_SZ);
    for (k = 0; k < SCR_SZ; k++) scr[k] = (uint8_t)rnd();
    for (k = 0; k < EXP_SZ; k++) exp[k] = (uint8_t)rnd();
    memset((void *)(uintptr_t)0xA0000u, 0x77, VGA_SZ);
    PTR(0x53AFF) = scr;
    PTR(0x53B03) = exp;
}

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
    /* The VGA block is not critical to the loader, so a failed commit there is
     * tolerated by le_reserve_address_space(). The view test writes through
     * the fixed 0xA0504, so commit 0xA0000 explicitly and bail out cleanly
     * (never crash) when it is not available. */
    if (plat_commit((uintptr_t)0xA0000u, VGA_SZ, PLAT_PROT_RWX) == NULL) {
        fprintf(stderr, "mapcheck: VGA block 0xA0000 unavailable, aborting\n");
        return 2;
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    /* The view-case result buffers are allocated after the guest window is
     * reserved: every allocation before le_reserve_address_space() grows the
     * CRT heap and can push it into 0x10000..0x70000, making the reservation
     * fail intermittently. */
    res_bmp   = (uint8_t *)malloc(BITMAP_SZ);
    res_vga   = (uint8_t *)malloc(VGA_SZ);
    res_scr   = (uint8_t *)malloc(SCR_SZ);
    res_exp   = (uint8_t *)malloc(EXP_SZ);
    res_cells = (uint8_t *)malloc(4 * 64 * 64);
    res_recs  = (uint8_t *)malloc(80 * 16);

    install_hook(0x3790A, stub_delay);
    install_hook(0x17AA9, stub_wait);   /* svc_wait_ticks: no tick thread here */
    /* The map render core reaches the Watcom CRT heap (0x24D22 -> malloc /
     * memmove / free); point them at the host libc so the original machine
     * code and the C translation use the same heap, exactly like rescheck. */
    install_hook(0x3706E, stub_malloc);
    install_hook(0x3771C, stub_memmove);
    install_hook(0x3776E, stub_free);
    install_hook(0x37AE5, stub_outp);    /* C-side DAC writes           */
    install_hook(0x25A96, stub_sfx);     /* map_unit_ping sound effect  */
    /* The original 0x4E31C writes the DAC with inline `out` - let the VEH
     * service exactly that instruction range (dos.c does the same in the
     * host, but this harness must not link the whole DOS layer). */
    AddVectoredExceptionHandler(1, dac_veh);
    printf("mapcheck: redirected %d low-memory references to 0x%X\n",
           patch_lowmem_refs(&le, MIRROR), MIRROR);

    if (!bitmap || !tileset || !lmi || !dstA || !dstB || !recs || !cells || !ctbl || !nres || !sbank || !pbank || !ibank)
        return 2;
    if (!scrA || !scrB || !expA || !expB || !cellA || !cellB || !recA || !recB)
        return 2;
    if (!res_bmp || !res_vga || !res_scr || !res_exp || !res_cells || !res_recs)
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

        /* ---- map_draw_status_popups (0x1DF58) ------------------------- */
        for (i = 0; i < 6 && !g_fail; i++) {
            int n  = 1 + (int)(rnd() % 4);
            int ns = 1 + (int)(rnd() % 6);
            int k;

            PTR(0x53A45) = recs;   I32(0x53BEB) = n;
            PTR(0x53A81) = nres;
            I32(0x53AA9) = 0;      I32(0x53AAD) = 0;
            for (k = 0; k < 80 * n; k++) recs[k] = (uint8_t)rnd();
            for (k = 0; k < n; k++) {
                recs[80 * k + 0] = (uint8_t)(rnd() % 8u);
                recs[80 * k + 1] = (uint8_t)(rnd() % 6u);
            }
            I32(0x53EC4) = ns;
            for (k = 0; k < ns; k++) {
                B8(0x53C6C + k) = (uint8_t)(rnd() % 8u);
                B8(0x53D34 + k) = (uint8_t)(rnd() % 16u);
                B8(0x53DFC + k) = (uint8_t)(rnd() % n);
            }
            memset(dstA, 0x66, BITMAP_SZ);
            memcpy(dstB, dstA, BITMAP_SZ);
            PTR(0x53A49) = dstA;
            ORIG_POPUPS();
            PTR(0x53A49) = dstB;
            map_draw_status_popups();
            cases++;
            if (memcmp(dstA, dstB, BITMAP_SZ) != 0) {
                size_t q, where = 0;
                for (q = 0; q < BITMAP_SZ; q++)
                    if (dstA[q] != dstB[q]) { where = q; break; }
                snprintf(g_why, sizeof g_why, "n=%d ns=%d @%u (%02X/%02X)",
                         n, ns, (unsigned)where, dstA[where], dstB[where]);
                fail("map_draw_status_popups"); break;
            }
        }
        PTR(0x53A49) = bitmap;   /* restore for the tests that follow */

        /* ---- map_draw_party_icons (0x1C2DA) --------------------------- */
        for (i = 0; i < 6 && !g_fail; i++) {
            int n   = 1 + (int)(rnd() % 8);
            int cnt = 1 + (int)(rnd() % n);
            uint8_t list[16];
            int k;

            PTR(0x53A45) = recs;   I32(0x53BEB) = n;
            PTR(0x53A81) = nres;   PTR(0x53A61) = ibank;
            PTR(0x53A49) = bitmap;
            I32(0x53AA9) = 0;      I32(0x53AAD) = 0;
            I32(0x53C0B) = (int)(rnd() % 4u);
            for (k = 0; k < 80 * n; k++) recs[k] = (uint8_t)rnd();
            for (k = 0; k < n; k++) {
                recs[80 * k + 0] = (uint8_t)(rnd() % 8u);
                recs[80 * k + 1] = (uint8_t)(rnd() % 6u);
                recs[80 * k + 2] = (uint8_t)(rnd() % 4u);
            }
            for (k = 0; k < cnt; k++) list[k] = (uint8_t)(rnd() % n);
            memset(dstA, 0x66, BITMAP_SZ);
            memcpy(dstB, dstA, BITMAP_SZ);
            PTR(0x53A49) = dstA;
            ORIG_ICONS(0, 0, cnt, list);
            PTR(0x53A49) = dstB;
            map_draw_party_icons(0, 0, cnt, list);
            cases++;
            if (memcmp(dstA, dstB, BITMAP_SZ) != 0) {
                size_t q, where = 0;
                for (q = 0; q < BITMAP_SZ; q++)
                    if (dstA[q] != dstB[q]) { where = q; break; }
                snprintf(g_why, sizeof g_why, "n=%d cnt=%d @%u (%02X/%02X)",
                         n, cnt, (unsigned)where, dstA[where], dstB[where]);
                fail("map_draw_party_icons"); break;
            }
        }
        PTR(0x53A49) = bitmap;

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

        /* ---- map_reveal_reachable (0x14818) --------------------------- */
        for (i = 0; i < 8 && !g_fail; i++) {
            int x = (int)(rnd() % 32u), y = (int)(rnd() % 32u);
            int range = (int)(rnd() % 20u), radius = (int)(rnd() % 6u);
            int filter = (int)(rnd() % 4u);
            int n = 1 + (int)(rnd() % 8), k, ra, rb;
            uint8_t listA[64], listB[64];
            static uint8_t cell_init[4 * 32 * 32];
            static uint8_t ctbl_save[4096];
            static uint8_t obj1_save[0x56B0];

            PTR(0x53A51) = cellA; I32(0x53A69) = (int32_t)(uintptr_t)ctbl;
            I32(0x53AC1) = 32;    I32(0x53AC5) = 32;
            I32(0x53BEB) = n;     PTR(0x53A45) = recs;
            for (k = 0; k < 4 * 32 * 32; k++) cell_init[k] = (uint8_t)rnd();
            cell_init[0] = 32; cell_init[2] = 32;   /* path_mark reads W/H from the grid */
            memcpy(cellA, cell_init, 4 * 32 * 32);
            memcpy(cellB, cell_init, 4 * 32 * 32);
            memset(recs, 0, 80 * 16);
            for (k = 0; k < 80 * n; k++) recs[k] = (uint8_t)rnd();
            for (k = 0; k < n; k++) {
                recs[80 * k + 0] = (uint8_t)(rnd() % 32u);
                recs[80 * k + 1] = (uint8_t)(rnd() % 32u);
                recs[80 * k + 6] = (uint8_t)(rnd() % 3u);
            }
            memset(listA, 0xEE, sizeof listA);
            memset(listB, 0xEE, sizeof listB);
            for (k = 0; k < 4096; k++) ctbl_save[k] = ctbl[k];
            memcpy(obj1_save, (const void *)0x50000u, sizeof obj1_save);
            ra = ORIG_REVEALR(x, y, listA, range, radius, filter);
            memcpy((void *)0x50000u, obj1_save, sizeof obj1_save);
            for (k = 0; k < 4096; k++) ctbl[k] = ctbl_save[k];
            PTR(0x53A51) = cellB;
            rb = map_reveal_reachable(x, y, listB, range, radius, filter);
            cases++;
            if (memcmp(cellA, cellB, 4 * 32 * 32) != 0
                || memcmp(listA, listB, sizeof listA) != 0 || ra != rb) {
                size_t q, where = 0;
                for (q = 0; q < 4 * 32 * 32; q++)
                    if (cellA[q] != cellB[q]) { where = q; break; }
                snprintf(g_why, sizeof g_why,
                         "x=%d y=%d range=%d radius=%d filter=%d n=%d ra=%d rb=%d cell@%u(%02X/%02X)",
                         x, y, range, radius, filter, n, ra, rb,
                         (unsigned)where, cellA[where], cellB[where]);
                fail("map_reveal_reachable"); break;
            }
        }
        PTR(0x53A51) = cells;

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

        /* ---- pal_tick_word (0x4E310) / pal_anim_step (0x4E31C) -------- */
        if (!g_fail) {
            /* The tick word itself: both sides read the same mirrored BDA. */
            {
                uint16_t t = (uint16_t)rnd();

                *(volatile uint16_t *)(uintptr_t)G_TICK = t;
                cases++;
                if (ORIG_TICKW() != pal_tick_word() || pal_tick_word() != t) {
                    snprintf(g_why, sizeof g_why, "tick=%u -> %u/%u", t,
                             (unsigned)ORIG_TICKW(), (unsigned)pal_tick_word());
                    fail("pal_tick_word");
                }
            }
            for (i = 0; i < 6 && !g_fail; i++) {
                uint16_t tick = (uint16_t)rnd();
                uint16_t w0;
                uint8_t  f0 = (uint8_t)(rnd() % 16);
                uint16_t wa, wb;
                uint8_t  fa, fb;
                int      k, da, db;

                /* gate: only (uint16_t)(tick - w0) >= 2 may upload, including
                 * the 0xFFFF -> 0x0000 wrap. */
                switch (rnd() % 4) {
                case 0:  w0 = tick; break;
                case 1:  w0 = (uint16_t)(tick - 1); break;
                case 2:  w0 = (uint16_t)(tick - 2 - (rnd() % 0x10000)); break;
                default: w0 = (uint16_t)(tick - 0x8000); break;
                }
                /* byte_60002 can read up to 3*15+48 bytes past 0x60003 */
                for (k = 0; k < 128; k++) pal_data[k] = (uint8_t)rnd();

                *(volatile uint16_t *)(uintptr_t)G_TICK = tick;
                word_60000 = w0; byte_60002 = f0;
                g_dac_n = 0;
                ORIG_PALSTEP();
                da = g_dac_n;
                memcpy(g_dac_saved, g_dac, sizeof(uint32_t) * (size_t)da);
                wa = word_60000; fa = byte_60002;

                *(volatile uint16_t *)(uintptr_t)G_TICK = tick;
                word_60000 = w0; byte_60002 = f0;
                g_dac_n = 0;
                pal_anim_step();
                db = g_dac_n;
                wb = word_60000; fb = byte_60002;
                cases++;
                if (da != db
                    || memcmp(g_dac_saved, g_dac,
                              sizeof(uint32_t) * (size_t)da) != 0
                    || wa != wb || fa != fb) {
                    snprintf(g_why, sizeof g_why,
                             "tick=%u w0=%u frame=%u dac %d/%d w %u/%u f %u/%u",
                             tick, w0, f0, da, db, wa, wb, fa, fb);
                    fail("pal_anim_step");
                }
            }
        }

        /* ---- map_unit_ping (0x32230) --------------------------------- */
        for (i = 0; i < 6 && !g_fail; i++) {
            int     n   = 1 + (int)(rnd() % 8);
            int     idx = (int)(rnd() % n);
            uint8_t ba  = (uint8_t)rnd();
            int     k, ka, na, nb, c0, c1;
            int     skip = (int)(rnd() & 1);   /* 1 = rec_skip() reports 1 */

            /* The table's 29 entries pick the unit class; cycle 0/1/2 so all
             * three effect branches are hit for in-range indices. */
            for (k = 0; k < 29; k++)
                ((uint8_t *)byte_52725)[k] = (uint8_t)(rnd() % 3);
            dword_53EEC = (void *)(uintptr_t)0x00C0FFEEu;   /* sentinel bank */
            I32(0x53BEB) = n;
            PTR(0x53A45) = recs;
            memset(recs, 0, 80 * 16);
            for (k = 0; k < 80 * n; k++) recs[k] = (uint8_t)rnd();
            recs[80 * idx + 7] = (uint8_t)(rnd() & 0xFF);
            if (recs[80 * idx + 7] == 0x1C)      /* first rec_skip test */
                recs[80 * idx + 7] = 0;
            if (skip) {
                recs[80 * idx + 0x20] = 0x13;    /* rec_skip -> 1 */
            } else {
                /* record +32 is a valid 1..29 index into the table and also
                 * doubles as rec_skip's p[0x20]; keep it away from 0x13.
                 * Out-of-range values would make the original index its
                 * stack copy out of bounds (out of the function's domain). */
                ka = 1 + (int)(rnd() % 29);
                if (ka == 0x13)
                    ka = 1;
                recs[80 * idx + 0x20] = (uint8_t)ka;
                recs[80 * idx + 0x1F] = (uint8_t)(rnd() & 0xFF);
                if (recs[80 * idx + 0x1F] == 4 || recs[80 * idx + 0x1F] == 5)
                    recs[80 * idx + 0x1F] = 0;
            }
            byte_54132 = ba;
            memcpy(recB, recs, 80 * n);

            g_sfx_n = 0;
            ORIG_PING(idx);
            na = g_sfx_n;
            memcpy((void *)g_sfx_bank_saved,  (const void *)g_sfx_bank,
                   sizeof(void *) * (size_t)na);
            memcpy(g_sfx_idx_saved,   g_sfx_idx,   sizeof(int)    * (size_t)na);
            memcpy(g_sfx_loops_saved, g_sfx_loops, sizeof(int)    * (size_t)na);
            c0 = byte_54132;

            byte_54132 = ba;
            g_sfx_n = 0;
            map_unit_ping(idx);
            nb = g_sfx_n;
            c1 = byte_54132;
            cases++;
            if (na != nb
                || memcmp((const void *)g_sfx_bank_saved, (const void *)g_sfx_bank,
                          sizeof(void *) * (size_t)na) != 0
                || memcmp(g_sfx_idx_saved, g_sfx_idx,
                          sizeof(int) * (size_t)na) != 0
                || memcmp(g_sfx_loops_saved, g_sfx_loops,
                          sizeof(int) * (size_t)na) != 0
                || c0 != c1 || memcmp(recB, recs, 80 * n) != 0) {
                snprintf(g_why, sizeof g_why,
                         "idx=%d k=%u skip=%u c=%d/%d sfx %d/%d",
                         idx, (unsigned)recs[80 * idx + 32], skip, c0, c1,
                         na, nb);
                fail("map_unit_ping");
            }
        }

        /* ---- map_view_update (0x11CAC) ------------------------------- */
        for (i = 0; i < 8 && !g_fail; i++) {
            uint32_t save = g_rnd;
            int      flag, ox, oy, k, mismatch = 0;
            int      da, wa, fa, sb0;
            int32_t  sa[N_VIEW_SCALARS], cur[N_VIEW_SCALARS];

            setup_view(bitmap, scrA, expA, cells, ctbl, recs,
                       &flag, &ox, &oy);
            g_dac_n = 0;
            ORIG_VIEW(flag);
            da = g_dac_n;
            memcpy(g_dac_saved, g_dac, sizeof(uint32_t) * (size_t)da);
            wa = word_60000; fa = byte_60002;
            sb0 = (int)B8(0x51A10);
            memcpy(res_bmp,   bitmap, BITMAP_SZ);
            memcpy(res_vga,   (void *)(uintptr_t)0xA0000u, VGA_SZ);
            memcpy(res_scr,   scrA,   SCR_SZ);
            memcpy(res_exp,   expA,   EXP_SZ);
            memcpy(res_cells, cells,  4 * 64 * 64);
            memcpy(res_recs,  recs,   80 * 16);
            for (k = 0; k < N_VIEW_SCALARS; k++)
                sa[k] = I32(g_view_scalars[k]);

            g_rnd = save;
            setup_view(bitmap, scrA, expA, cells, ctbl, recs,
                       &flag, &ox, &oy);
            g_dac_n = 0;
            map_view_update(flag);
            for (k = 0; k < N_VIEW_SCALARS; k++)
                cur[k] = I32(g_view_scalars[k]);
            cases++;

            if (da != g_dac_n
                || memcmp(g_dac_saved, g_dac,
                          sizeof(uint32_t) * (size_t)da) != 0
                || wa != word_60000 || fa != byte_60002 || sb0 != (int)B8(0x51A10)
                || memcmp(res_bmp, bitmap, BITMAP_SZ) != 0
                || memcmp(res_vga, (void *)(uintptr_t)0xA0000u, VGA_SZ) != 0
                || memcmp(res_scr, scrA, SCR_SZ) != 0
                || memcmp(res_exp, expA, EXP_SZ) != 0
                || memcmp(res_cells, cells, 4 * 64 * 64) != 0
                || memcmp(res_recs, recs, 80 * 16) != 0
                || memcmp(sa, cur, sizeof sa) != 0)
                mismatch = 1;
            if (mismatch) {
                unsigned where = 0;
                for (where = 0; where < BITMAP_SZ; where++)
                    if (res_bmp[where] != bitmap[where]) break;
                snprintf(g_why, sizeof g_why,
                         "flag=%d ox=%d oy=%d dac %d/%d bitmap@%u scr%d exp%d "
                         "cells%d recs%d scal%d",
                         flag, ox, oy, da, g_dac_n, where,
                         memcmp(res_scr, scrA, SCR_SZ) != 0,
                         memcmp(res_exp, expA, EXP_SZ) != 0,
                         memcmp(res_cells, cells, 4 * 64 * 64) != 0,
                         memcmp(res_recs, recs, 80 * 16) != 0,
                         memcmp(sa, cur, sizeof sa) != 0);
                fail("map_view_update");
            }
        }

        /* ---- map_draw_party_icons_anim (0x1C4CC) ---------------------- */
        for (i = 0; i < 6 && !g_fail; i++) {
            uint32_t save = g_rnd;
            int flag, ox, oy, k, tidx, cnt, bad = 0;
            uint8_t list[16];

            tidx = (int)(rnd() % 26u);
            cnt  = 1 + (int)(rnd() % 6);
            for (k = 0; k < cnt; k++) list[k] = (uint8_t)(rnd() % 8u);
            for (k = 0; k < cnt; k++) list[k] = (uint8_t)(list[k] % (uint8_t)3);
            setup_view(bitmap, scrA, expA, cells, ctbl, recs, &flag, &ox, &oy);
            PTR(0x53AD1) = nres; I32(0x53B13) = 0;
            for (k = 0; k < cnt; k++) list[k] = (uint8_t)(list[k] % (uint8_t)I32(0x53BEB));
            ORIG_ICONSANIM(0, tidx, cnt, list);
            memcpy(res_bmp,   bitmap, BITMAP_SZ);
            memcpy(res_vga,   (void *)(uintptr_t)0xA0000u, VGA_SZ);
            memcpy(res_cells, cells,  4 * 64 * 64);
            memcpy(res_recs,  recs,   80 * 16);

            g_rnd = save;
            tidx = (int)(rnd() % 26u);
            cnt  = 1 + (int)(rnd() % 6);
            for (k = 0; k < cnt; k++) list[k] = (uint8_t)(rnd() % 8u);
            for (k = 0; k < cnt; k++) list[k] = (uint8_t)(list[k] % (uint8_t)3);
            setup_view(bitmap, scrA, expA, cells, ctbl, recs, &flag, &ox, &oy);
            PTR(0x53AD1) = nres; I32(0x53B13) = 0;
            for (k = 0; k < cnt; k++) list[k] = (uint8_t)(list[k] % (uint8_t)I32(0x53BEB));
            map_draw_party_icons_anim(0, tidx, cnt, list);
            cases++;
            if (memcmp(res_bmp, bitmap, BITMAP_SZ) != 0
                || memcmp(res_vga, (void *)(uintptr_t)0xA0000u, VGA_SZ) != 0
                || memcmp(res_cells, cells, 4 * 64 * 64) != 0
                || memcmp(res_recs, recs, 80 * 16) != 0)
                bad = 1;
            if (bad) {
                snprintf(g_why, sizeof g_why, "tidx=%d cnt=%d", tidx, cnt);
                fail("map_draw_party_icons_anim");
            }
        }

        /* ---- map_slide_view (0x12CEA) --------------------------------- */
        for (i = 0; i < 6 && !g_fail; i++) {
            uint32_t save = g_rnd;
            int flag, ox, oy, tx, ty, o_x, o_y, bad = 0;

            setup_view(bitmap, scrA, expA, cells, ctbl, recs, &flag, &ox, &oy);
            I32(0x53AC5) = 32;   /* setup_view only sets 0x53AC1 */
            tx = I32(0x53AB1) + ((int)(rnd() % 3u) - 1);
            ty = I32(0x53AB5) + ((int)(rnd() % 3u) - 1);
            if (tx < 0) tx = 0;
            if (tx > I32(0x53AC1) - 1) tx = I32(0x53AC1) - 1;
            if (ty < 0) ty = 0;
            if (ty > I32(0x53AC5) - 1) ty = I32(0x53AC5) - 1;
            ORIG_SLIDE(tx, ty);
            o_x = I32(0x53AB1); o_y = I32(0x53AB5);
            memcpy(res_bmp,   bitmap, BITMAP_SZ);
            memcpy(res_vga,   (void *)(uintptr_t)0xA0000u, VGA_SZ);
            memcpy(res_scr,   scrA,   SCR_SZ);
            memcpy(res_cells, cells,  4 * 64 * 64);

            g_rnd = save;
            setup_view(bitmap, scrA, expA, cells, ctbl, recs, &flag, &ox, &oy);
            I32(0x53AC5) = 32;
            tx = I32(0x53AB1) + ((int)(rnd() % 3u) - 1);
            ty = I32(0x53AB5) + ((int)(rnd() % 3u) - 1);
            if (tx < 0) tx = 0;
            if (tx > I32(0x53AC1) - 1) tx = I32(0x53AC1) - 1;
            if (ty < 0) ty = 0;
            if (ty > I32(0x53AC5) - 1) ty = I32(0x53AC5) - 1;
            map_slide_view(tx, ty);
            cases++;
            if (I32(0x53AB1) != o_x || I32(0x53AB5) != o_y
                || memcmp(res_bmp, bitmap, BITMAP_SZ) != 0
                || memcmp(res_vga, (void *)(uintptr_t)0xA0000u, VGA_SZ) != 0
                || memcmp(res_scr, scrA, SCR_SZ) != 0
                || memcmp(res_cells, cells, 4 * 64 * 64) != 0)
                bad = 1;
            if (bad) {
                snprintf(g_why, sizeof g_why, "tx=%d ty=%d o=(%d,%d) c=(%d,%d)",
                         tx, ty, o_x, o_y, I32(0x53AB1), I32(0x53AB5));
                fail("map_slide_view");
            }
        }
    }

    /* The machine-code side of 0x4E31C only runs if the narrow VEH serviced
     * its inline `out` instructions; a clean run without any hit means the
     * harness silently stopped comparing the original. */
    if (!g_fail && g_veh_n == 0) {
        snprintf(g_why, sizeof g_why, "dac_veh never fired (0x4E31C not run?)");
        fail("dac_veh self-check");
    }

    printf("%s: %ld cases, %d failures\n", g_fail ? "FAILED" : "PASS",
           cases, g_fail);    le_close(&le);
    return g_fail ? 1 : 0;
}
