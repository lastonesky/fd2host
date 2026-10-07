/* fadecheck.c - differential test for the palette fades.
 *
 *   0x11D40 pal_fade_range(start, end, sub)   vs src/game/fade.c
 *   0x1F882 pal_fade_out()                    (64 steps of fade_range + delay)
 *   0x1F525 pal_fade_in()
 *
 * The fades touch nothing but the DAC, so the whole observable is the sequence
 * of `outp(0x3C8/0x3C9, value)` calls plus the delays. Both are hooked to
 * recorders, and the game's palette copy (*(0x53A65)) is pointed at a
 * synthetic palette so the clamp-at-zero path is exercised. Per case the
 * original machine code runs first, then the C, and the logs must match
 * exactly (order, ports and values).
 *
 * Build: pwsh -File build.ps1 -Target fadecheck
 * Run   : build\fadecheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/fade.h"

typedef void (*range_fn)(int, int, int);
typedef void (*fade_fn)(void);
#define ORIG_RANGE ((range_fn)(uintptr_t)0x00011D40u)
#define ORIG_OUT   ((fade_fn) (uintptr_t)0x0001F882u)
#define ORIG_IN    ((fade_fn) (uintptr_t)0x0001F525u)
#define ORIG_ADD   ((range_fn)(uintptr_t)0x00011DF2u)

#define PAL_PTR (*(uint8_t **)(uintptr_t)0x00053A65u)

/* A full fade writes 256*4 = 1024 DAC bytes per step and there are 65 steps,
 * so the event stream is ~66k long - store an FNV-1a hash + counts, not every
 * event. The stream is fully deterministic, so (count, hash) is exact enough
 * and keeps memory flat. */
#define FNV_OFFSET 1469598103934665603ull
#define FNV_PRIME  1099511628211ull

static uint64_t g_hash;
static int      g_outp_n, g_delay_n;

static int __cdecl stub_outp(unsigned port, int value)
{
    uint64_t k = ((uint64_t)(uint32_t)port << 32) ^ (uint32_t)value;

    g_hash = (g_hash ^ k) * FNV_PRIME;
    g_outp_n++;
    return 0;
}

static void __cdecl stub_delay(unsigned ms)
{
    (void)ms;
    g_delay_n++;
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

static uint8_t g_pal[768];

static uint32_t g_rnd = 0x1234567u;
static uint32_t rnd(void) { g_rnd = g_rnd * 1103515245u + 12345u; return g_rnd >> 9; }

/* --- comparison helpers ------------------------------------------------- */
static uint64_t g_exp_hash;
static int      g_exp_outp, g_exp_delays;

static void reset_log(void) { g_hash = FNV_OFFSET; g_outp_n = 0; g_delay_n = 0; }

static void save_expected(void)
{
    g_exp_hash = g_hash; g_exp_outp = g_outp_n; g_exp_delays = g_delay_n;
}

static const char *cmp_log(char *why, size_t why_n)
{
    if (g_outp_n != g_exp_outp) {
        snprintf(why, why_n, "outp count %d/%d", g_exp_outp, g_outp_n); return why;
    }
    if (g_delay_n != g_exp_delays) {
        snprintf(why, why_n, "delay count %d/%d", g_exp_delays, g_delay_n); return why;
    }
    if (g_hash != g_exp_hash) {
        snprintf(why, why_n, "DAC sequence hash %016llX/%016llX",
                 (unsigned long long)g_exp_hash, (unsigned long long)g_hash);
        return why;
    }
    return NULL;
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

    HOOK(0x37AE5, stub_outp);
    HOOK(0x3790A, stub_delay);

    PAL_PTR = g_pal;

    for (round = 0; round < 20 && !failures; round++) {
        int i;
        for (i = 0; i < 200; i++) {
            /* random palette so the clamp-at-zero path is hit often */
            int k;
            for (k = 0; k < 768; k++)
                g_pal[k] = (uint8_t)rnd();

            {
                int mode = (int)(rnd() % 4);
                if (mode == 0) {
                    int start = (int)(rnd() % 256);
                    int end   = start + (int)(rnd() % (256 - start));
                    int sub   = (int)(rnd() % 400);
                    PAL_PTR = g_pal;
                    reset_log(); ORIG_RANGE(start, end, sub); save_expected();
                    PAL_PTR = g_pal;
                    reset_log(); pal_fade_range(start, end, sub);
                    if (cmp_log(why, sizeof why)) {
                        printf("FAIL range(%d,%d,%d): %s\n", start, end, sub, why);
                        failures++;
                    }
                } else if (mode == 1) {
                    reset_log(); ORIG_OUT(); save_expected();
                    reset_log(); pal_fade_out();
                    if (cmp_log(why, sizeof why)) {
                        printf("FAIL fade_out: %s\n", why); failures++;
                    }
                } else if (mode == 2) {
                    reset_log(); ORIG_IN(); save_expected();
                    reset_log(); pal_fade_in();
                    if (cmp_log(why, sizeof why)) {
                        printf("FAIL fade_in: %s\n", why); failures++;
                    }
                } else {
                    int start = (int)(rnd() % 256);
                    int end   = start + (int)(rnd() % (256 - start));
                    int add   = (int)(rnd() % 100);
                    reset_log(); ORIG_ADD(start, end, add); save_expected();
                    reset_log(); pal_fade_add(start, end, add);
                    if (cmp_log(why, sizeof why)) {
                        printf("FAIL fade_add(%d,%d,%d): %s\n", start, end, add, why);
                        failures++;
                    }
                }
                cases++;
                if (failures) break;
            }
        }
    }

    printf("%s: %d cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
