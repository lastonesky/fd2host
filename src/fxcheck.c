/* fxcheck.c - differential test for src/game/fx.c.
 *
 * funcs_30469[] (0x524C6) is the effect-animation dispatch table; the nine
 * handlers translated so far are
 *
 *   0x2B996  fx_dots7   table[0]
 *   0x2BB33  fx_dots8   table[1]
 *   0x2BD6C  fx_blob    table[2]
 *   0x2BF83  fx_advance helper (fx_blob only)
 *   0x2BFD9  fx_dots12  table[3]
 *   0x2C217  fx_dots6   table[4]
 *   0x2C441  fx_dots6b  table[5]
 *   0x2CAFC  fx_dots3   table[7]
 *   0x2CCF4  fx_dots16  table[8]
 *   0x2CE1A  fx_toggle  table[9]
 *
 * All are pure: game globals in obj1, a read-only offset table copied from its
 * original address, and four already-translated services. So the test needs no
 * files, no CRT heap, no clock. The services are hooked to recording stubs and
 * the original machine code is compared against the C call by call, argument
 * by argument:
 *
 *   0x2EB9F  res_blit(buf, index, dst, pitch, mode)
 *   0x25A96  svc_play_sfx(bank, index, loops)
 *   0x25B45  svc_play_sfx2(bank, index, loops)
 *   0x4EBE3  util_rand()  - a fixed pool that includes negative 32-bit values,
 *                           so the signed-idiv-by-2 in the machine code is
 *                           pinned (the pool is deterministic and shared, so
 *                           both sides see the same sequence)
 *
 * The compared state is the **whole obj1 data object** (0x50000..0x556B0) plus
 * a host-allocated stand-in for the 80-byte character-record table that
 * dword_53A45 points at. Running the original and then the C from the same
 * snapshot proves the C touches exactly the same globals: particle arrays,
 * the SFX bank, the slot/direction bytes and the +148/+20/+143 table bias.
 *
 * g_buf also doubles as the fx_advance resource block: bytes [8, 8+4*256) are
 * a sub-offset table and each offset's +6 byte holds a frame count.
 *
 * Build: pwsh -File build.ps1 -Target fxcheck
 * Run   : build\fxcheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/fx.h"

typedef int (*fx_fn)(int rec, const void *buf, void *dst, int pitch, int selector);
typedef int (*adv_fn)(uint8_t *phase, uint8_t *counter, void *dst, int pitch,
                      const void *buf);

#define ORIG_DOTS7  ((fx_fn)(uintptr_t)0x0002B996u)
#define ORIG_DOTS8  ((fx_fn)(uintptr_t)0x0002BB33u)
#define ORIG_BLOB   ((fx_fn)(uintptr_t)0x0002BD6Cu)
#define ORIG_ADV    ((adv_fn)(uintptr_t)0x0002BF83u)
#define ORIG_DOTS12 ((fx_fn)(uintptr_t)0x0002BFD9u)
#define ORIG_DOTS6  ((fx_fn)(uintptr_t)0x0002C217u)
#define ORIG_DOTS6B ((fx_fn)(uintptr_t)0x0002C441u)
#define ORIG_DOTS3  ((fx_fn)(uintptr_t)0x0002CAFCu)
#define ORIG_DOTS16 ((fx_fn)(uintptr_t)0x0002CCF4u)
#define ORIG_TOGGLE ((fx_fn)(uintptr_t)0x0002CE1Au)

/* --- event log --------------------------------------------------------- */
enum { EV_BLIT, EV_SFX, EV_SFX2, EV_N };
static const char *const evname[EV_N] = { "blit", "sfx", "sfx2" };

typedef struct { int id; int32_t v[5]; } event_t;
#define MAXEV 256
static event_t g_ev[MAXEV];
static int     g_n;
static int     g_verbose;

static void rec5(int id, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e)
{
    if (g_n < MAXEV) {
        event_t *ev = &g_ev[g_n++];
        ev->id = id;
        ev->v[0] = a; ev->v[1] = b; ev->v[2] = c; ev->v[3] = d; ev->v[4] = e;
    }
}

/* --- hooked services --------------------------------------------------- */
static uint8_t g_buf[2048];
static uint8_t g_dst[1024];
static uint8_t g_rec[80 * 4];             /* the record table dword_53A45 points at */

static int __cdecl stub_blit(const void *buf, int index, void *dst, int pitch, int mode)
{
    rec5(EV_BLIT, (int32_t)(intptr_t)buf, index, (int32_t)(intptr_t)dst, pitch, mode);
    return 0;
}
static int __cdecl stub_sfx(const void *bank, int index, int loops)
{
    rec5(EV_SFX, (int32_t)(intptr_t)bank, index, loops, 0, 0);
    return 0;
}
static int __cdecl stub_sfx2(const void *bank, int index, int loops)
{
    rec5(EV_SFX2, (int32_t)(intptr_t)bank, index, loops, 0, 0);
    return 0;
}

/* Deterministic pool shared by both sides; includes negative bit patterns so
 * the original `sar edx,1Fh; idiv 2` and the C `(int32_t)rand % 2` must agree
 * on sign. */
static const uint32_t g_rand_pool[] = {
    0u, 1u, 2u, 3u, 5u, 0xFFFFu, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu, 0xFFFFFFFDu
};
static int g_rand_i;
static uint32_t __cdecl stub_rand(void)
{
    uint32_t v = g_rand_pool[g_rand_i % (int)(sizeof g_rand_pool / sizeof g_rand_pool[0])];
    g_rand_i++;
    return v;
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
#define HOOK(addr, fn) install_hook((uint32_t)(addr), (const void *)(fn))

/* --- globals under test (obj1) ----------------------------------------- */
#define dword_53A45 (*(uint32_t *)(uintptr_t)0x00053A45u)
#define dword_54153 (*(uint32_t *)(uintptr_t)0x00054153u)

#define dword_54018 ((int32_t *)(uintptr_t)0x00054018u)
#define dword_54030 ((int32_t *)(uintptr_t)0x00054030u)
#define byte_54048  ((uint8_t  *)(uintptr_t)0x00054048u)
#define byte_5404E  (*(uint8_t  *)(uintptr_t)0x0005404Eu)
#define byte_5404F  (*(uint8_t  *)(uintptr_t)0x0005404Fu)
#define dword_540CB ((int32_t *)(uintptr_t)0x000540CBu)
#define dword_540DB ((int32_t *)(uintptr_t)0x000540DBu)
#define byte_540EB  (*(uint8_t  *)(uintptr_t)0x000540EBu)
#define byte_540EC  (*(uint8_t  *)(uintptr_t)0x000540ECu)
#define byte_540ED  (*(uint8_t  *)(uintptr_t)0x000540EDu)
#define dword_540EE ((int32_t *)(uintptr_t)0x000540EEu)
#define byte_5412E  (*(uint8_t  *)(uintptr_t)0x0005412Eu)
#define byte_5412F  (*(uint8_t  *)(uintptr_t)0x0005412Fu)

#define dword_53F76 ((int32_t *)(uintptr_t)0x00053F76u)
#define dword_53F92 ((int32_t *)(uintptr_t)0x00053F92u)
#define byte_53FB2  (*(uint8_t  *)(uintptr_t)0x00053FB2u)
#define byte_53FB3  (*(uint8_t  *)(uintptr_t)0x00053FB3u)
#define byte_53FB4  (*(uint8_t  *)(uintptr_t)0x00053FB4u)
#define dword_53FB5 ((int32_t *)(uintptr_t)0x00053FB5u)
#define dword_53FE5 ((int32_t *)(uintptr_t)0x00053FE5u)
#define byte_54015  (*(uint8_t  *)(uintptr_t)0x00054015u)
#define byte_54016  (*(uint8_t  *)(uintptr_t)0x00054016u)
#define byte_54017  (*(uint8_t  *)(uintptr_t)0x00054017u)
#define dword_54050 ((int32_t *)(uintptr_t)0x00054050u)
#define dword_54068 ((int32_t *)(uintptr_t)0x00054068u)
#define byte_54080 ((uint8_t  *)(uintptr_t)0x00054080u)
#define byte_54086  (*(uint8_t  *)(uintptr_t)0x00054086u)
#define byte_54087  (*(uint8_t  *)(uintptr_t)0x00054087u)

/* Full obj1: proves the C writes no global outside the arrays it owns. */
#define WIN_LO 0x00050000u
#define WIN_HI 0x000556B0u
#define WIN_N  (WIN_HI - WIN_LO)
static uint8_t g_base[WIN_N];
static uint8_t g_pre[WIN_N], g_post_o[WIN_N], g_post_c[WIN_N];

/* A synthetic initial state covering all four handlers at once. */
typedef struct {
    int32_t d18[6], d30[6];
    uint8_t b48[6], b4E, b4F;
    int32_t dCB[3], dDB[3];
    uint8_t bEB, bEC, bED;
    int32_t dEE[16];
    uint8_t b12E, b12F;
    int32_t dF76[8];
    int32_t dF92[8];
    uint8_t bFB2, bFB3, bFB4;
    int32_t dFB5[12], dFE5[12];
    uint8_t b5415, b5416, b5417;
    int32_t d4050[6], d4068[6];
    uint8_t b4080[6], b4086, b4087;
    uint8_t rec[80 * 4];
} state_t;

static void apply_state(const state_t *st)
{
    int i;

    memcpy((void *)(uintptr_t)WIN_LO, g_base, WIN_N);
    for (i = 0; i < 6; i++) {
        dword_54018[i] = st->d18[i];
        dword_54030[i] = st->d30[i];
        byte_54048[i]  = st->b48[i];
    }
    byte_5404E = st->b4E;
    byte_5404F = st->b4F;
    for (i = 0; i < 3; i++) {
        dword_540CB[i] = st->dCB[i];
        dword_540DB[i] = st->dDB[i];
    }
    byte_540EB = st->bEB;
    byte_540EC = st->bEC;
    byte_540ED = st->bED;
    for (i = 0; i < 16; i++)
        dword_540EE[i] = st->dEE[i];
    byte_5412E = st->b12E;
    byte_5412F = st->b12F;
    memcpy(dword_53F76, st->dF76, sizeof st->dF76);
    memcpy(dword_53F92, st->dF92, sizeof st->dF92);
    byte_53FB2 = st->bFB2;
    byte_53FB3 = st->bFB3;
    byte_53FB4 = st->bFB4;
    memcpy(dword_53FB5, st->dFB5, sizeof st->dFB5);
    memcpy(dword_53FE5, st->dFE5, sizeof st->dFE5);
    byte_54015 = st->b5415;
    byte_54016 = st->b5416;
    byte_54017 = st->b5417;
    memcpy(dword_54050, st->d4050, sizeof st->d4050);
    memcpy(dword_54068, st->d4068, sizeof st->d4068);
    memcpy(byte_54080, st->b4080, sizeof st->b4080);
    byte_54086 = st->b4086;
    byte_54087 = st->b4087;
    memcpy(g_rec, st->rec, sizeof g_rec);
}

static int cmp_events(const event_t *a, int an, const event_t *b, int bn,
                      char *why, size_t whysz)
{
    int k, j;

    if (an != bn) {
        snprintf(why, whysz, "event count %d/%d", an, bn);
        return 1;
    }
    for (k = 0; k < an; k++) {
        if (a[k].id != b[k].id) {
            snprintf(why, whysz, "event %d id %s/%s", k, evname[a[k].id],
                     evname[b[k].id]);
            return 1;
        }
        for (j = 0; j < 5; j++)
            if (a[k].v[j] != b[k].v[j]) {
                snprintf(why, whysz, "event %d %s arg%d %X/%X", k,
                         evname[a[k].id], j, (uint32_t)a[k].v[j],
                         (uint32_t)b[k].v[j]);
                return 1;
            }
    }
    return 0;
}

static void dump_events(const char *tag, const event_t *ev, int n)
{
    int k, j;

    if (!g_verbose)
        return;
    printf("  %s events (%d):\n", tag, n);
    for (k = 0; k < n; k++) {
        printf("    %s", evname[ev[k].id]);
        for (j = 0; j < 5; j++)
            printf(" %X", (uint32_t)ev[k].v[j]);
        printf("\n");
    }
}

/* Run one (original, C) pair from the same state and compare the return
 * value, the event log and the whole data-segment window. */
static int run_pair(fx_fn orig, fx_fn c, const state_t *st, int rec,
                    const void *buf, void *dst, int pitch, int sel,
                    char *why, size_t whysz)
{
    event_t oev[MAXEV], cev[MAXEV];
    int     on, cn, ro, rc, k;
    uint8_t rec_o[sizeof g_rec], rec_c[sizeof g_rec];

    apply_state(st);
    memcpy(g_pre, (const void *)(uintptr_t)WIN_LO, WIN_N);
    memcpy(rec_o, g_rec, sizeof rec_o);

    g_rand_i = 0;
    g_n = 0;
    ro = orig(rec, buf, dst, pitch, sel);
    on = g_n;
    memcpy(oev, g_ev, sizeof oev);
    memcpy(g_post_o, (const void *)(uintptr_t)WIN_LO, WIN_N);
    memcpy(rec_c, g_rec, sizeof rec_c);   /* what the original left in g_rec */

    /* back to exactly the state the original started from */
    memcpy((void *)(uintptr_t)WIN_LO, g_pre, WIN_N);
    memcpy(g_rec, rec_o, sizeof rec_o);

    g_rand_i = 0;
    g_n = 0;
    rc = c(rec, buf, dst, pitch, sel);
    cn = g_n;
    memcpy(cev, g_ev, sizeof cev);
    memcpy(g_post_c, (const void *)(uintptr_t)WIN_LO, WIN_N);

    if (ro != rc) {
        snprintf(why, whysz, "return %d/%d (sel=%u)", ro, rc, (unsigned)(uint8_t)sel);
        return 1;
    }
    if (cmp_events(oev, on, cev, cn, why, whysz)) {
        dump_events("orig", oev, on);
        dump_events("C   ", cev, cn);
        return 1;
    }
    if (memcmp(g_post_o, g_post_c, WIN_N) != 0) {
        for (k = 0; k < WIN_N; k++)
            if (g_post_o[k] != g_post_c[k])
                break;
        snprintf(why, whysz, "global 0x%X: %02X/%02X (sel=%u)",
                 WIN_LO + k, g_post_o[k], g_post_c[k], (unsigned)(uint8_t)sel);
        return 1;
    }
    if (memcmp(rec_c, g_rec, sizeof rec_c) != 0) {
        snprintf(why, whysz, "record table mutated");
        return 1;
    }
    return 0;
}

/* Run one fx_advance pair from the same state; compare return value, the
 * (phase, counter) side effects, the event log and the data-segment window. */
static int run_pair_adv(const state_t *st, int ph, int cnt, void *dst, int pitch,
                        const void *buf, char *why, size_t whysz)
{
    event_t oev[MAXEV], cev[MAXEV];
    int     on, cn, ro, rc, k;
    uint8_t po, pc, co, cc;

    apply_state(st);
    memcpy(g_pre, (const void *)(uintptr_t)WIN_LO, WIN_N);
    po = (uint8_t)ph;
    co = (uint8_t)cnt;
    g_rand_i = 0;
    g_n = 0;
    ro = ORIG_ADV(&po, &co, dst, pitch, buf);
    on = g_n;
    memcpy(oev, g_ev, sizeof oev);
    memcpy(g_post_o, (const void *)(uintptr_t)WIN_LO, WIN_N);

    memcpy((void *)(uintptr_t)WIN_LO, g_pre, WIN_N);
    pc = (uint8_t)ph;
    cc = (uint8_t)cnt;
    g_rand_i = 0;
    g_n = 0;
    rc = fx_advance(&pc, &cc, dst, pitch, buf);
    cn = g_n;
    memcpy(cev, g_ev, sizeof cev);
    memcpy(g_post_c, (const void *)(uintptr_t)WIN_LO, WIN_N);

    if (ro != rc) {
        snprintf(why, whysz, "return %d/%d (phase=%u)", ro, rc, (unsigned)po);
        return 1;
    }
    if (po != pc) {
        snprintf(why, whysz, "phase %u/%u", (unsigned)po, (unsigned)pc);
        return 1;
    }
    if (co != cc) {
        snprintf(why, whysz, "counter %u/%u (phase=%u)", (unsigned)co,
                 (unsigned)cc, (unsigned)ph);
        return 1;
    }
    if (cmp_events(oev, on, cev, cn, why, whysz)) {
        dump_events("orig", oev, on);
        dump_events("C   ", cev, cn);
        return 1;
    }
    if (memcmp(g_post_o, g_post_c, WIN_N) != 0) {
        for (k = 0; k < WIN_N; k++)
            if (g_post_o[k] != g_post_c[k])
                break;
        snprintf(why, whysz, "global 0x%X: %02X/%02X", WIN_LO + k,
                 g_post_o[k], g_post_c[k]);
        return 1;
    }
    return 0;
}

/* --- state profiles ---------------------------------------------------- */
struct p6 { int32_t d18[6], d30[6]; uint8_t b48[6], b4E, b4F; };
static const struct p6 p6s[] = {
    { { 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 }, 0, 0 },
    { { 0, 1, 2, 3, 6, 7 }, { 0, 1, 2, 3, 4, 5 }, { 0, 7, 0, 7, 0, 7 }, 9, 0 },
    { { -1, 0, 1, 2, 7, 8 }, { 5, 9, 0, 3, 1, 7 }, { 0xF9, 3, 7, 0, 7, 3 }, 9, 1 },
    { { 7, 7, 7, 7, 7, 7 }, { 9, 9, 9, 9, 9, 9 }, { 7, 7, 7, 7, 7, 7 }, 0, 0 },
    { { 8, 8, 8, 8, 8, 8 }, { 0, 9, 3, 6, 1, 4 }, { 0, 0, 0, 0, 0, 0 }, 5, 1 },
    { { 3, 3, 3, 3, 3, 3 }, { 2, 5, 8, 1, 9, 7 }, { 0xF9, 0xF9, 0, 7, 7, 3 }, 9, 0 },
};

struct p3 { int32_t dCB[3], dDB[3]; uint8_t bEB, bEC, bED; };
static const struct p3 p3s[] = {
    { { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0 },
    { { 0, 1, 2 }, { 0, 1, 2 }, 9, 0, 0 },
    { { 4, 5, 6 }, { 9, 5, 1 }, 0, 0, 1 },
    { { 7, 7, 7 }, { 9, 9, 9 }, 9, 0, 1 },
    { { 1, 1, 1 }, { 1, 0, 9 }, 0, 1, 0 },
    { { -1, -1, 7 }, { 5, 5, 5 }, 9, 1, 0 },
};

struct p16 { int32_t dEE[16]; };
static const struct p16 p16s[] = {
    { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { { -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, -2, 0x7FFFFFFF, 3, 4, 5, 6 } },
    { { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 } },
    { { 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 } },
    { { 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7 } },
    { { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 } },
    { { 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4 } },
};

struct ptg { uint8_t b12E, b12F; };
static const struct ptg ptgs[] = {
    { 0, 0 }, { 1, 0 }, { 5, 0 }, { 6, 1 }, { 35, 0 }, { 36, 1 },
    { 43, 0 }, { 0x10, 0 }, { 0x2B, 1 },
};

/* dots7: eight phases (selector 3 writes the eighth slot). */
struct p7 { int32_t dF76[8]; };
static const struct p7 p7s[] = {
    { { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { { -1, 0, 1, 2, 3, 4, 5, 6 } },
    { { 3, 3, 3, 3, 3, 3, 3, 3 } },
    { { 4, 4, 4, 4, 4, 4, 4, 4 } },
    { { 8, 8, 8, 8, 8, 8, 8, 8 } },
    { { 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF } },
    { { 0x7FFFFFFF, 0xFFFFFFF5, 2, 2, 0xF, 0, 9, 9 } },
};

/* dots8: eight phases. */
struct p8 { int32_t dF92[8]; };
static const struct p8 p8s[] = {
    { { 0, 0, 0, 0, 0, 0, 0, 0 } },
    { { -1, 0, 1, 2, 3, 4, 5, 6 } },
    { { 4, 4, 4, 4, 4, 4, 4, 4 } },
    { { 5, 5, 5, 5, 5, 5, 5, 5 } },
    { { 8, 8, 8, 8, 8, 8, 8, 8 } },
    { { 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF } },
    { { 0x7FFFFFFF, 0xFFFFFFF5, 2, 2, 0xF, 0, 9, 9 } },
};

/* blob: three bytes of state. */
struct pblob { uint8_t b2, b3, b4; };
static const struct pblob pblobs[] = {
    { 0, 0, 0 }, { 7, 0, 0 }, { 10, 0, 0 }, { 15, 0, 0 }, { 16, 0, 0 },
    { 17, 0, 0 }, { 18, 0, 0 }, { 0xFF, 0, 0 }, { 10, 3, 2 }, { 16, 9, 9 },
    { 0x11, 0, 0 }, { 0x12, 0, 0 },
    /* these match the fx_advance frame counts for phases 0..3 (p*7+3) */
    { 0, 0, 3 }, { 1, 0, 10 }, { 2, 0, 17 }, { 18, 0, 3 },
};

/* dots12: twelve phases, twelve slots (slots stay 0..11 so v20/v21 stay in
 * their table), plus the rotate/freeze/parity bytes. */
struct p12 { int32_t dFB5[12], dFE5[12]; uint8_t b15, b16, b17; };
static const struct p12 p12s[] = {
    { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, 12, 0, 0 },
    { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 12, 0, 0 },
    { { -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
      { 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 }, 11, 1, 1 },
    { { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
      { 1, 2, 3, 4, 5, 7, 9, 10, 11, 0, 6, 8 }, 0, 0, 0 },
    { { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
      { 0, 6, 8, 0, 6, 8, 0, 6, 8, 0, 6, 8 }, 0, 0, 0 },
    { { 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10 },
      { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 }, 43, 0, 1 },
    { { 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11 },
      { 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8 }, 0, 0, 0 },
    { { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
      { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, 255, 0, 0 },
};

/* dots6b: six phases, six slots (0..9), direction bases and rotate/freeze. */
struct p6b {
    int32_t d4050[6], d4068[6];
    uint8_t b4080[6], b4086, b4087;
};
static const struct p6b p6bs[] = {
    { { 0, 0, 0, 0, 0, 0 }, { 0, 1, 2, 3, 4, 5 },
      { 0, 0, 0, 0, 0, 0 }, 6, 0 },
    { { 0, 0, 0, 0, 0, 0 }, { 3, 4, 5, 6, 7, 8 },
      { 6, 6, 6, 6, 6, 6 }, 9, 0 },
    { { -1, 0, 1, 2, 3, 4 }, { 9, 8, 7, 6, 5, 4 },
      { 0, 6, 0, 6, 0, 6 }, 9, 1 },
    { { 1, 1, 1, 1, 1, 1 }, { 0, 0, 0, 0, 0, 0 },
      { 6, 0, 6, 0, 6, 0 }, 0, 0 },
    { { 6, 6, 6, 6, 6, 6 }, { 1, 2, 3, 4, 5, 6 },
      { 0, 0, 0, 0, 0, 0 }, 9, 0 },
    { { 7, 7, 7, 7, 7, 7 }, { 9, 9, 9, 9, 9, 9 },
      { 6, 6, 6, 6, 6, 6 }, 9, 1 },
    { { 5, 5, 5, 5, 5, 5 }, { 2, 0, 8, 4, 6, 1 },
      { 0xF9, 3, 7, 0, 7, 3 }, 6, 0 },
};

static const int      g_sels[]   = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x100, 0xFF, 0x12, 0x1FF };
static const int      g_pitch[]  = { 0, 320, 640 };
#define NSEL   ((int)(sizeof g_sels / sizeof g_sels[0]))
#define NPITCH ((int)(sizeof g_pitch / sizeof g_pitch[0]))
#define NREP   5

int main(int argc, char **argv)
{
    le_image le;
    int      applied = 0, cases = 0, failures = 0, si, fi, rep, i;
    char     why[256];

    g_verbose = (argc > 1 && strcmp(argv[1], "--dump") == 0);

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1 && argv[1][0] != '-') ? argv[1] : "E:\\FD2\\FD2.EXE") != 0)
        return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    HOOK(0x2EB9F, stub_blit);
    HOOK(0x25A96, stub_sfx);
    HOOK(0x25B45, stub_sfx2);
    HOOK(0x4EBE3, stub_rand);
    printf("hooks: res_blit/svc_play_sfx/svc_play_sfx2/util_rand -> record stubs\n");

    /* Canonical base: record table pointer + SFX bank sentinel, then snapshot. */
    dword_53A45 = (uint32_t)(uintptr_t)g_rec;
    dword_54153 = 0xDEADBEEFu;
    memcpy(g_base, (const void *)(uintptr_t)WIN_LO, WIN_N);
    memset(g_rec, 0, sizeof g_rec);

    /* fx_advance reads a sub-offset table out of `buf`: int32 sub-offsets at
     * buf + 8 + 4*phase whose +6 byte is a frame count. Keep every access
     * inside g_buf by carving the sub-blocks out of the tail. */
    for (i = 0; i < 256; i++) {
        int32_t off = 1100 + (i * 13) % 900;
        memcpy(g_buf + 8 + 4 * i, &off, 4);
    }
    for (i = 0; i < 256; i++) {
        int32_t off;
        memcpy(&off, g_buf + 8 + 4 * i, 4);
        g_buf[off + 6] = (uint8_t)(i * 7 + 3);
    }

    /* --- fx_dots6 (0x2C217) -------------------------------------- */
    for (si = 0; si < (int)(sizeof p6s / sizeof p6s[0]) && !failures; si++)
        for (int sel = 0; sel < NSEL && !failures; sel++)
            for (fi = 0; fi < 2 && !failures; fi++)
                for (rep = 0; rep < NREP && !failures; rep++) {
                    state_t st;
                    int     rec = rep % 4;
                    memset(&st, 0, sizeof st);
                    memcpy(st.d18, p6s[si].d18, sizeof st.d18);
                    memcpy(st.d30, p6s[si].d30, sizeof st.d30);
                    memcpy(st.b48, p6s[si].b48, sizeof st.b48);
                    st.b4E = p6s[si].b4E;
                    st.b4F = p6s[si].b4F;
                    st.rec[80 * rec + 6] = fi ? 0x5A : 0x00;
                    cases++;
                    if (run_pair(ORIG_DOTS6, fx_dots6, &st, rec, g_buf,
                                 g_dst + 256, g_pitch[rep % NPITCH], g_sels[sel],
                                 why, sizeof why)) {
                        printf("FAIL fx_dots6 p%d sel=%X flag=%d rec=%d: %s\n",
                               si, (unsigned)g_sels[sel], fi, rec, why);
                        failures++;
                    }
                }

    /* --- fx_dots3 (0x2CAFC) -------------------------------------- */
    for (si = 0; si < (int)(sizeof p3s / sizeof p3s[0]) && !failures; si++)
        for (int sel = 0; sel < NSEL && !failures; sel++)
            for (fi = 0; fi < 2 && !failures; fi++)
                for (rep = 0; rep < NREP && !failures; rep++) {
                    state_t st;
                    int     rec = rep % 4;
                    memset(&st, 0, sizeof st);
                    memcpy(st.dCB, p3s[si].dCB, sizeof st.dCB);
                    memcpy(st.dDB, p3s[si].dDB, sizeof st.dDB);
                    st.bEB = p3s[si].bEB;
                    st.bEC = p3s[si].bEC;
                    st.bED = p3s[si].bED;
                    st.rec[80 * rec + 6] = fi ? 0x5A : 0x00;
                    cases++;
                    if (run_pair(ORIG_DOTS3, fx_dots3, &st, rec, g_buf,
                                 g_dst + 256, g_pitch[(rep + 1) % NPITCH], g_sels[sel],
                                 why, sizeof why)) {
                        printf("FAIL fx_dots3 p%d sel=%X flag=%d rec=%d: %s\n",
                               si, (unsigned)g_sels[sel], fi, rec, why);
                        failures++;
                    }
                }

    /* --- fx_dots16 (0x2CCF4) ------------------------------------- */
    for (si = 0; si < (int)(sizeof p16s / sizeof p16s[0]) && !failures; si++)
        for (int sel = 0; sel < NSEL && !failures; sel++)
            for (fi = 0; fi < 2 && !failures; fi++)
                for (rep = 0; rep < NREP && !failures; rep++) {
                    state_t st;
                    int     rec = rep % 4;
                    memset(&st, 0, sizeof st);
                    memcpy(st.dEE, p16s[si].dEE, sizeof st.dEE);
                    st.rec[80 * rec + 6] = fi ? 0x5A : 0x00;
                    cases++;
                    if (run_pair(ORIG_DOTS16, fx_dots16, &st, rec, g_buf,
                                 g_dst + 256, g_pitch[(rep + 2) % NPITCH], g_sels[sel],
                                 why, sizeof why)) {
                        printf("FAIL fx_dots16 p%d sel=%X flag=%d rec=%d: %s\n",
                               si, (unsigned)g_sels[sel], fi, rec, why);
                        failures++;
                    }
                }

    /* --- fx_toggle (0x2CE1A) ------------------------------------- */
    for (si = 0; si < (int)(sizeof ptgs / sizeof ptgs[0]) && !failures; si++)
        for (int sel = 0; sel < NSEL && !failures; sel++)
            for (fi = 0; fi < 2 && !failures; fi++)
                for (rep = 0; rep < NREP && !failures; rep++) {
                    state_t st;
                    int     rec = rep % 4;
                    memset(&st, 0, sizeof st);
                    st.b12E = ptgs[si].b12E;
                    st.b12F = ptgs[si].b12F;
                    st.rec[80 * rec + 6] = fi ? 0x5A : 0x00;
                    cases++;
                    if (run_pair(ORIG_TOGGLE, fx_toggle, &st, rec, g_buf,
                                 g_dst + 256, g_pitch[rep % NPITCH], g_sels[sel],
                                 why, sizeof why)) {
                        printf("FAIL fx_toggle p%d sel=%X flag=%d rec=%d: %s\n",
                               si, (unsigned)g_sels[sel], fi, rec, why);
                        failures++;
                    }
                }

    /* --- fx_dots7 (0x2B996) -------------------------------------- */
    for (si = 0; si < (int)(sizeof p7s / sizeof p7s[0]) && !failures; si++)
        for (int sel = 0; sel < NSEL && !failures; sel++)
            for (fi = 0; fi < 2 && !failures; fi++)
                for (rep = 0; rep < NREP && !failures; rep++) {
                    state_t st;
                    int     rec = rep % 4;
                    memset(&st, 0, sizeof st);
                    memcpy(st.dF76, p7s[si].dF76, sizeof st.dF76);
                    st.rec[80 * rec + 6] = fi ? 0x5A : 0x00;
                    cases++;
                    if (run_pair(ORIG_DOTS7, fx_dots7, &st, rec, g_buf,
                                 g_dst + 256, g_pitch[rep % NPITCH], g_sels[sel],
                                 why, sizeof why)) {
                        printf("FAIL fx_dots7 p%d sel=%X flag=%d rec=%d: %s\n",
                               si, (unsigned)g_sels[sel], fi, rec, why);
                        failures++;
                    }
                }

    /* --- fx_dots8 (0x2BB33) -------------------------------------- */
    for (si = 0; si < (int)(sizeof p8s / sizeof p8s[0]) && !failures; si++)
        for (int sel = 0; sel < NSEL && !failures; sel++)
            for (fi = 0; fi < 2 && !failures; fi++)
                for (rep = 0; rep < NREP && !failures; rep++) {
                    state_t st;
                    int     rec = rep % 4;
                    memset(&st, 0, sizeof st);
                    memcpy(st.dF92, p8s[si].dF92, sizeof st.dF92);
                    st.rec[80 * rec + 6] = fi ? 0x5A : 0x00;
                    cases++;
                    if (run_pair(ORIG_DOTS8, fx_dots8, &st, rec, g_buf,
                                 g_dst + 256, g_pitch[(rep + 1) % NPITCH], g_sels[sel],
                                 why, sizeof why)) {
                        printf("FAIL fx_dots8 p%d sel=%X flag=%d rec=%d: %s\n",
                               si, (unsigned)g_sels[sel], fi, rec, why);
                        failures++;
                    }
                }

    /* --- fx_blob (0x2BD6C) + fx_advance via blob ------------------- */
    for (si = 0; si < (int)(sizeof pblobs / sizeof pblobs[0]) && !failures; si++)
        for (int sel = 0; sel < NSEL && !failures; sel++)
            for (fi = 0; fi < 2 && !failures; fi++)
                for (rep = 0; rep < NREP && !failures; rep++) {
                    state_t st;
                    int     rec = rep % 4;
                    memset(&st, 0, sizeof st);
                    st.bFB2 = pblobs[si].b2;
                    st.bFB3 = pblobs[si].b3;
                    st.bFB4 = pblobs[si].b4;
                    st.rec[80 * rec + 6] = fi ? 0x5A : 0x00;
                    cases++;
                    if (run_pair(ORIG_BLOB, fx_blob, &st, rec, g_buf,
                                 g_dst + 256, g_pitch[(rep + 2) % NPITCH], g_sels[sel],
                                 why, sizeof why)) {
                        printf("FAIL fx_blob p%d sel=%X flag=%d rec=%d: %s\n",
                               si, (unsigned)g_sels[sel], fi, rec, why);
                        failures++;
                    }
                }

    /* --- fx_dots12 (0x2BFD9) ------------------------------------- */
    for (si = 0; si < (int)(sizeof p12s / sizeof p12s[0]) && !failures; si++)
        for (int sel = 0; sel < NSEL && !failures; sel++)
            for (fi = 0; fi < 2 && !failures; fi++)
                for (rep = 0; rep < NREP && !failures; rep++) {
                    state_t st;
                    int     rec = rep % 4;
                    memset(&st, 0, sizeof st);
                    memcpy(st.dFB5, p12s[si].dFB5, sizeof st.dFB5);
                    memcpy(st.dFE5, p12s[si].dFE5, sizeof st.dFE5);
                    st.b5415 = p12s[si].b15;
                    st.b5416 = p12s[si].b16;
                    st.b5417 = p12s[si].b17;
                    st.rec[80 * rec + 6] = fi ? 0x5A : 0x00;
                    cases++;
                    if (run_pair(ORIG_DOTS12, fx_dots12, &st, rec, g_buf,
                                 g_dst + 256, g_pitch[rep % NPITCH], g_sels[sel],
                                 why, sizeof why)) {
                        printf("FAIL fx_dots12 p%d sel=%X flag=%d rec=%d: %s\n",
                               si, (unsigned)g_sels[sel], fi, rec, why);
                        failures++;
                    }
                }

    /* --- fx_dots6b (0x2C441) ------------------------------------- */
    for (si = 0; si < (int)(sizeof p6bs / sizeof p6bs[0]) && !failures; si++)
        for (int sel = 0; sel < NSEL && !failures; sel++)
            for (fi = 0; fi < 2 && !failures; fi++)
                for (rep = 0; rep < NREP && !failures; rep++) {
                    state_t st;
                    int     rec = rep % 4;
                    memset(&st, 0, sizeof st);
                    memcpy(st.d4050, p6bs[si].d4050, sizeof st.d4050);
                    memcpy(st.d4068, p6bs[si].d4068, sizeof st.d4068);
                    memcpy(st.b4080, p6bs[si].b4080, sizeof st.b4080);
                    st.b4086 = p6bs[si].b4086;
                    st.b4087 = p6bs[si].b4087;
                    st.rec[80 * rec + 6] = fi ? 0x5A : 0x00;
                    cases++;
                    if (run_pair(ORIG_DOTS6B, fx_dots6b, &st, rec, g_buf,
                                 g_dst + 256, g_pitch[(rep + 1) % NPITCH], g_sels[sel],
                                 why, sizeof why)) {
                        printf("FAIL fx_dots6b p%d sel=%X flag=%d rec=%d: %s\n",
                               si, (unsigned)g_sels[sel], fi, rec, why);
                        failures++;
                    }
                }

    /* --- fx_advance (0x2BF83) direct ----------------------------- */
    for (int ph = 0; ph < 256 && !failures; ph += 7)
        for (int cnt = 0; cnt < 256 && !failures; cnt += 11)
            for (rep = 0; rep < 3 && !failures; rep++) {
                state_t st;
                memset(&st, 0, sizeof st);
                cases++;
                if (run_pair_adv(&st, ph, cnt, g_dst + 256,
                                 g_pitch[rep % NPITCH], g_buf, why, sizeof why)) {
                    printf("FAIL fx_advance ph=%d cnt=%d: %s\n", ph, cnt, why);
                    failures++;
                }
            }

    printf("%s: %d cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
