/* scenecheck.c - differential test for 0x22E5C vs src/game/scene.c (scene_card).
 *
 * 0x22E5C is a pure service sequence, so every service it calls is hooked to a
 * recording stub and the two runs are compared by call order and arguments:
 *
 *   0x25977  bgm_play(track, loops)
 *   0x17AA9  svc_wait_ticks(n)
 *   0x1F882  palette fade out       0x1F525  palette fade in
 *   0x111BA  res_load("FDOTHER.DAT", old, 79)
 *   0x2EB9F  rle blit(buf, index, dst, pitch, mode)
 *   0x3776E  free(buf)  (the original's tail is `push ebx; jmp free`)
 *
 * Per case: run the original machine code, reset, run the C - require the same
 * event log. No case data is needed (the routine takes no arguments), so this
 * is a repeatability + sequencing check; running it N times also proves the C
 * frees exactly what it allocated (the free event is part of the log).
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

enum { EV_BGM, EV_WAIT, EV_FADE_OUT, EV_FADE_IN, EV_RES, EV_BLIT, EV_FREE, EV_N };
static const char *const evname[EV_N] = {
    "bgm", "wait", "fade_out", "fade_in", "res_load", "blit", "free"
};

typedef struct { int id; int32_t a, b, c, d; } event_t;
#define MAXEV 64
static event_t g_ev[MAXEV];
static int     g_n;

static void rec(int id, int32_t a, int32_t b, int32_t c, int32_t d)
{
    if (g_n < MAXEV) {
        g_ev[g_n].id = id; g_ev[g_n].a = a; g_ev[g_n].b = b;
        g_ev[g_n].c = c;  g_ev[g_n].d = d;
        g_n++;
    }
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
        if (g_n != an) {
            snprintf(why, sizeof why, "event count %d/%d", an, g_n);
            failures++;
        } else {
            int k;
            for (k = 0; k < an; k++) {
                if (a[k].id != g_ev[k].id || a[k].a != g_ev[k].a ||
                    a[k].b != g_ev[k].b || a[k].c != g_ev[k].c ||
                    a[k].d != g_ev[k].d) {
                    snprintf(why, sizeof why,
                             "event %d: %s(%X,%X,%X,%X) vs %s(%X,%X,%X,%X)",
                             k, evname[a[k].id], a[k].a, a[k].b, a[k].c, a[k].d,
                             evname[g_ev[k].id], g_ev[k].a, g_ev[k].b,
                             g_ev[k].c, g_ev[k].d);
                    failures++;
                    break;
                }
            }
        }
        if (failures)
            printf("FAIL case %d: %s\n", cases, why);
    }

    printf("%s: %d cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
