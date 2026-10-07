/* bgmcheck.c - differential test for play_bgm (0x25977) vs src/game/bgm.c.
 *
 * play_bgm is app-level: it keeps a track number in a game global and
 * sequences six services. All six are hooked to recording stubs, so the test
 * compares the *call sequence* (which service, with which arguments, in which
 * order) plus the resulting globals - not just "some sound came out":
 *
 *   0x111BA  res_load("FDMUS.DAT", old, track)  -> records, sets dword_53BFF
 *   0x3666C  DPMI lock linear region            -> records
 *   0x3ADF5  AIL_init_sequence
 *   0x3AEEE  AIL_start_sequence
 *   0x3AF5B  AIL_stop_sequence
 *   0x3B124  AIL_set_sequence_volume
 *   0x3B1A6  AIL_set_sequence_loop_count
 *
 * Per case: seed the globals, run the original machine code, reset to the
 * same seed, run the C, then require the event logs and the globals to be
 * identical. Tracks include -1 (fade out) and 16/17 (fanfare), and the
 * music/fade flags plus "an old buffer is loaded" are all exercised.
 *
 * Build: pwsh -File build.ps1 -Target bgmcheck
 * Run   : build\bgmcheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/bgm.h"
#include "game/res.h"

typedef void (*play_fn)(int, int);
#define ORIG_PLAY ((play_fn)(uintptr_t)0x00025977u)

#define W32(a) (*(uint32_t *)(uintptr_t)(a))
#define B8(a)  (*(uint8_t  *)(uintptr_t)(a))

enum { EV_RES, EV_LOCK, EV_INIT, EV_START, EV_STOP, EV_VOL, EV_LOOP, EV_N };
static const char *const evname[EV_N] = {
    "res_load", "lock", "ail_init", "ail_start", "ail_stop",
    "ail_volume", "ail_loop"
};

typedef struct { int id; int32_t a, b, c; } event_t;
#define MAXEV 256
static event_t g_ev[MAXEV];
static int     g_n;

static void rec(int id, int32_t a, int32_t b, int32_t c)
{
    if (g_n < MAXEV) {
        g_ev[g_n].id = id;
        g_ev[g_n].a = a;
        g_ev[g_n].b = b;
        g_ev[g_n].c = c;
        g_n++;
    }
}

/* One synthetic resource buffer for every load; both runs get the same. */
static uint8_t g_res[4096];

/* --- hooked services (cdecl, exactly the originals' ABI) ---------------- */
static void *__cdecl stub_res_load(const char *name, void *old, int index)
{
    rec(EV_RES, strcmp(name, "FDMUS.DAT") == 0, (int32_t)(intptr_t)old, index);
    W32(RES_SIZE_ADDR) = 0x100u + (uint32_t)index;   /* what res_load would set */
    return g_res;
}

static void __cdecl stub_lock(void *buf, uint32_t len)
{
    rec(EV_LOCK, (int32_t)(intptr_t)buf, (int32_t)len, 0);
}

static void __cdecl stub_ail_init(void *h, void *start, int32_t seq)
{
    rec(EV_INIT, (int32_t)(intptr_t)h, (int32_t)(intptr_t)start, seq);
}
static void __cdecl stub_ail_start(void *h) { rec(EV_START, (int32_t)(intptr_t)h, 0, 0); }
static void __cdecl stub_ail_stop(void *h)  { rec(EV_STOP,  (int32_t)(intptr_t)h, 0, 0); }
static void __cdecl stub_ail_vol(void *h, int32_t vol, int32_t ms)
{
    rec(EV_VOL, (int32_t)(intptr_t)h, vol, ms);
}
static void __cdecl stub_ail_loop(void *h, int32_t count)
{
    rec(EV_LOOP, (int32_t)(intptr_t)h, count, 0);
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

/* --- deterministic case generator -------------------------------------- */
static uint32_t g_rnd = 0xC0FFEEu;
static uint32_t rnd(void) { g_rnd = g_rnd * 1103515245u + 12345u; return g_rnd >> 8; }

static void seed_world(uint8_t track_cur, uint8_t music, uint8_t fade, void *old)
{
    B8(0x51A11)  = track_cur;
    B8(0x53EF0)  = music;
    B8(0x51E61)  = fade;
    W32(0x53ED0) = 0x11110000u;              /* synthetic AIL handle      */
    W32(0x53EE0) = (uint32_t)(intptr_t)old;
    W32(RES_SIZE_ADDR) = 0;
    g_n = 0;
}

typedef struct { event_t ev[MAXEV]; int n; uint8_t track, music, fade; uint32_t h, buf, size; } snap_t;

static void snapshot(snap_t *s)
{
    memcpy(s->ev, g_ev, sizeof(event_t) * (size_t)g_n);
    s->n     = g_n;
    s->track = B8(0x51A11);
    s->music = B8(0x53EF0);
    s->fade  = B8(0x51E61);
    s->h     = W32(0x53ED0);
    s->buf   = W32(0x53EE0);
    s->size  = W32(RES_SIZE_ADDR);
}

static int same(snap_t *x, snap_t *y, char *why, size_t why_n)
{
    int i;
    if (x->track != y->track || x->buf != y->buf || x->size != y->size) {
        snprintf(why, why_n, "globals: track %02X/%02X buf %08X/%08X size %X/%X",
                 x->track, y->track, x->buf, y->buf, x->size, y->size);
        return 0;
    }
    if (x->n != y->n) {
        snprintf(why, why_n, "event count %d/%d", x->n, y->n);
        return 0;
    }
    for (i = 0; i < x->n; i++) {
        if (x->ev[i].id != y->ev[i].id || x->ev[i].a != y->ev[i].a ||
            x->ev[i].b != y->ev[i].b || x->ev[i].c != y->ev[i].c) {
            snprintf(why, why_n,
                     "event %d: %s(%X,%X,%X) vs %s(%X,%X,%X)",
                     i, evname[x->ev[i].id], x->ev[i].a, x->ev[i].b, x->ev[i].c,
                     evname[y->ev[i].id], y->ev[i].a, y->ev[i].b, y->ev[i].c);
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    le_image le;
    int      applied = 0, round, cases = 0, failures = 0;
    char     why[256];

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    HOOK(0x111BA, stub_res_load);
    HOOK(0x3666C, stub_lock);
    HOOK(0x3ADF5, stub_ail_init);
    HOOK(0x3AEEE, stub_ail_start);
    HOOK(0x3AF5B, stub_ail_stop);
    HOOK(0x3B124, stub_ail_vol);
    HOOK(0x3B1A6, stub_ail_loop);

    for (round = 0; round < 20 && !failures; round++) {
        int i;
        for (i = 0; i < 300; i++) {
            /* track: -1, 0..22, and occasionally 16/17 deliberately */
            int      track = (int)(rnd() % 24) - 1;
            int      loops = (int)(rnd() % 4);
            uint8_t  cur   = (uint8_t)rnd();
            uint8_t  music = (uint8_t)(rnd() & 1);
            uint8_t  fade  = (uint8_t)(rnd() & 1);
            void    *old   = (rnd() & 1) ? (void *)(uintptr_t)0x22220000u : NULL;
            snap_t   a, b;

            if ((cases % 7) == 0) track = 16;
            if ((cases % 11) == 0) track = 17;
            if ((cases % 5) == 0) cur = (uint8_t)track;   /* "already playing" */

            seed_world(cur, music, fade, old);
            ORIG_PLAY(track, loops);
            snapshot(&a);

            seed_world(cur, music, fade, old);
            bgm_play(track, loops);
            snapshot(&b);

            cases++;
            if (!same(&a, &b, why, sizeof why)) {
                printf("FAIL case %d (track=%d loops=%d cur=%02X music=%d "
                       "fade=%d old=%p): %s\n", cases, track, loops, cur,
                       music, fade, old, why);
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
