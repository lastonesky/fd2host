/* scenecheck.c - differential test for src/game/scene.c.
 *
 * Two families, one hook set:
 *
 *   0x22E5C scene_card      (round 50) - pure service sequence
 *   0x22EF6/0x231BC/0x23790/0x2389F/0x23E39 (round 60) - funcs_25E23[]
 *                          main-state-machine transition handlers
 *
 * Every service the two families reach is hooked to a recording stub and the
 * original machine code and the C are compared call by call, argument by
 * argument, plus the final dword_53C03 state index and a 1 KiB snapshot of
 * the surrounding data segment (to prove the C writes no other global):
 *
 *   0x25977  bgm_play(track, loops)
 *   0x17AA9  svc_wait_ticks(n)
 *   0x1F882  palette fade out       0x1F525  palette fade in
 *   0x111BA  res_load("FDOTHER.DAT", old, 79)
 *   0x2EB9F  rle blit(buf, index, dst, pitch, mode)
 *   0x3776E  free(buf)  (the original's tail is `push ebx; jmp free`)
 *   0x15F84  vm_run(stream, sub, addr, pitch, fg, shadow, bgfill, step, wait)
 *   0x11506  unit_refresh_all()
 *   0x112A5  unit_add(id)
 *
 * The scene_card case needs no data file (it takes no arguments): it is a
 * repeatability/sequencing check whose free event also proves the C frees
 * exactly what it allocated. The handler cases synthesise the two globals
 * they read - dword_53A79 (the VM stream) and dword_53C03 (the state index) -
 * over a grid of sentinels so both the pass-through and the `= 1` vs `++`
 * distinction are covered (initial 0/1/7/0x7FFFFFFF/-1).
 *
 * Build: pwsh -File build.ps1 -Target scenecheck
 * Run   : build\scenecheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/scene.h"

typedef void (*scene_fn)(void);
#define ORIG_SCENE ((scene_fn)(uintptr_t)0x00022E5Cu)

enum {
    EV_BGM, EV_WAIT, EV_FADE_OUT, EV_FADE_IN, EV_RES, EV_BLIT, EV_FREE,
    EV_VM, EV_UNIT_REFRESH, EV_UNIT_ADD, EV_N
};
static const char *const evname[EV_N] = {
    "bgm", "wait", "fade_out", "fade_in", "res_load", "blit", "free",
    "vm_run", "unit_refresh_all", "unit_add"
};

typedef struct { int id; int32_t v[9]; } event_t;
#define MAXEV 64
static event_t g_ev[MAXEV];
static int     g_n;

static void rec9(int id, int32_t a, int32_t b, int32_t c, int32_t d,
                 int32_t e, int32_t f, int32_t g, int32_t h, int32_t i)
{
    if (g_n < MAXEV) {
        event_t *ev = &g_ev[g_n++];
        ev->id = id;
        ev->v[0] = a; ev->v[1] = b; ev->v[2] = c; ev->v[3] = d; ev->v[4] = e;
        ev->v[5] = f; ev->v[6] = g; ev->v[7] = h; ev->v[8] = i;
    }
}
static void rec(int id, int32_t a, int32_t b, int32_t c, int32_t d)
{
    rec9(id, a, b, c, d, 0, 0, 0, 0, 0);
}

static uint8_t g_res[256];

/* --- hooked services (cdecl, the originals' ABI) ------------------------ */
static void  __cdecl stub_bgm(int track, int loops)   { rec(EV_BGM, track, loops, 0, 0); }
static int   __cdecl stub_wait(int n)                 { rec(EV_WAIT, n, 0, 0, 0); return 0; }
static void  __cdecl stub_fade_out(void)              { rec(EV_FADE_OUT, 0, 0, 0, 0); }
static void  __cdecl stub_fade_in(void)               { rec(EV_FADE_IN, 0, 0, 0, 0); }
static void  __cdecl stub_free(void *p)               { rec(EV_FREE, (int32_t)(intptr_t)p, 0, 0, 0); }

static void *__cdecl stub_res_load(const char *name, void *old, int index)
{
    rec(EV_RES, strcmp(name, "FDOTHER.DAT") == 0, (int32_t)(intptr_t)old, index, 0);
    return g_res;
}

static int __cdecl stub_blit(void *buf, int index, void *dst, int pitch, int mode)
{
    /* buf must be the pointer res_load returned; pitch/mode packed into one */
    rec(EV_BLIT, (buf == (void *)g_res), index, pitch, mode);
    return 0;
}

static int __cdecl stub_vm(void *stream, int sub, int addr, int pitch,
                           int fg, int shadow, int bgfill, int line_step, int wait)
{
    rec9(EV_VM, (int32_t)(intptr_t)stream, sub, addr, pitch, fg, shadow,
         bgfill, line_step, wait);
    return addr;   /* deterministic; the handlers ignore it anyway */
}

static void __cdecl stub_unit_refresh(void)
{
    rec(EV_UNIT_REFRESH, 0, 0, 0, 0);
}

static int __cdecl stub_unit_add(int id)
{
    rec(EV_UNIT_ADD, id, 0, 0, 0);
    return 0;
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

/* The two globals the state handlers read/write. */
#define D53A79 (*(uint32_t *)(uintptr_t)0x00053A79u)
#define D53C03 (*(int32_t  *)(uintptr_t)0x00053C03u)

/* Snapshot window: covers both globals and their neighbourhood. */
#define GUARD_LO 0x00053A00u
#define GUARD_HI 0x00053E00u
#define GUARD_N  (GUARD_HI - GUARD_LO)
static uint8_t g_pre[GUARD_N], g_post_o[GUARD_N], g_post_c[GUARD_N];

/* Compare two event logs; 0 = equal, else writes a reason. */
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
        for (j = 0; j < 9; j++)
            if (a[k].v[j] != b[k].v[j]) {
                snprintf(why, whysz, "event %d %s arg%d %X/%X", k,
                         evname[a[k].id], j, a[k].v[j], b[k].v[j]);
                return 1;
            }
    }
    return 0;
}

/* Run one (original, C) pair from the same initial state and compare the
 * event log, the final state index and the whole guard window. */
static int run_pair(scene_fn orig, scene_fn c, uint32_t stream, int32_t init,
                    char *why, size_t whysz)
{
    event_t oev[MAXEV], cev[MAXEV];
    int on, cn, k;

    /* --- original --- */
    D53A79 = stream;
    D53C03 = init;
    memcpy(g_pre, (const void *)(uintptr_t)GUARD_LO, GUARD_N);
    g_n = 0;
    orig();
    on = g_n;
    memcpy(oev, g_ev, sizeof oev);
    memcpy(g_post_o, (const void *)(uintptr_t)GUARD_LO, GUARD_N);

    /* --- reset the whole window, then the C --- */
    memcpy((void *)(uintptr_t)GUARD_LO, g_pre, GUARD_N);
    g_n = 0;
    c();
    cn = g_n;
    memcpy(cev, g_ev, sizeof cev);
    memcpy(g_post_c, (const void *)(uintptr_t)GUARD_LO, GUARD_N);

    if (cmp_events(oev, on, cev, cn, why, whysz))
        return 1;
    if (memcmp(g_post_o, g_post_c, GUARD_N) != 0) {
        for (k = 0; k < GUARD_N; k++)
            if (g_post_o[k] != g_post_c[k])
                break;
        snprintf(why, whysz, "global 0x%X: %02X/%02X",
                 GUARD_LO + k, g_post_o[k], g_post_c[k]);
        return 1;
    }
    return 0;
}

/* funcs_25E23[] handlers under test (table index in the name). */
struct handler { const char *name; scene_fn orig; scene_fn c; };
static const struct handler g_handlers[] = {
    { "0x22EF6[0]",  (scene_fn)(uintptr_t)0x00022EF6u, scene_state_00 },
    { "0x231BC[3]",  (scene_fn)(uintptr_t)0x000231BCu, scene_state_03 },
    { "0x23790[10]", (scene_fn)(uintptr_t)0x00023790u, scene_state_10 },
    { "0x2389F[12]", (scene_fn)(uintptr_t)0x0002389Fu, scene_state_12 },
    { "0x23E39[18]", (scene_fn)(uintptr_t)0x00023E39u, scene_state_18 },
};

static const uint32_t g_streams[] = { 0u, 0x12345678u, 0xDEADBEEFu };
static const int32_t  g_inits[]   = { 0, 1, 7, 0x7FFFFFFF, -1 };

int main(int argc, char **argv)
{
    le_image le;
    int      applied = 0, i, cases = 0, failures = 0;
    char     why[256];

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    HOOK(0x25977, stub_bgm);
    HOOK(0x17AA9, stub_wait);
    HOOK(0x1F882, stub_fade_out);
    HOOK(0x1F525, stub_fade_in);
    HOOK(0x111BA, stub_res_load);
    HOOK(0x2EB9F, stub_blit);
    HOOK(0x3776E, stub_free);
    HOOK(0x15F84, stub_vm);
    HOOK(0x11506, stub_unit_refresh);
    HOOK(0x112A5, stub_unit_add);

    /* --- scene_card: repeatability / sequencing (round 50) -------------- */
    for (i = 0; i < 100 && !failures; i++) {
        event_t a[MAXEV];
        int     an;

        g_n = 0;
        ORIG_SCENE();
        memcpy(a, g_ev, sizeof a);
        an = g_n;

        g_n = 0;
        scene_card();

        cases++;
        if (cmp_events(a, an, g_ev, g_n, why, sizeof why)) {
            printf("FAIL case %d: %s\n", cases, why);
            failures++;
        }
    }

    /* --- funcs_25E23[] handlers: 200 reps each (round 60) ----------------
     * Each rep picks a (stream, state) pair; the grid plus repetition covers
     * the pass-through, both increment/assign forms, the wrap at
     * 0x7FFFFFFF/-1 and re-entrancy. */
    for (i = 0; i < (int)(sizeof g_handlers / sizeof g_handlers[0]); i++) {
        const struct handler *h = &g_handlers[i];
        int rep;
        for (rep = 0; rep < 200; rep++) {
            uint32_t stream = g_streams[rep % 3];
            int32_t  init   = g_inits[(rep / 3) % 5];

            cases++;
            if (run_pair(h->orig, h->c, stream, init, why, sizeof why)) {
                printf("FAIL %s stream=%08X init=%08X: %s\n",
                       h->name, stream, (uint32_t)init, why);
                failures++;
                break;
            }
        }
    }

    printf("%s: %d cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
