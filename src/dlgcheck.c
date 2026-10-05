/* dlgcheck.c - differential test for the dialogue-box helpers.
 *
 * 0x16559 (dlg_blit_dato) and 0x16E24 (dlg_scroll_text) both write the VGA
 * buffer at 0xA0000 and read the original globals dword_53A85 / dword_53C67.
 * The test sets those globals, fills the VGA with a sentinel, runs the
 * original, then resets the VGA and runs the translation, and compares the
 * whole frame buffer.
 *
 * Build: pwsh -File build.ps1 -Target dlgcheck
 * Run   : build\dlgcheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/dlg.h"

typedef void (__cdecl *blit_fn)(int);
typedef void (__cdecl *scroll_fn)(void);

#define ORIG_BLIT   ((blit_fn)(uintptr_t)0x16559)
#define ORIG_SCROLL ((scroll_fn)(uintptr_t)0x16E24)

#define G_53A85 (*(uint32_t *)(uintptr_t)0x53A85u)
#define G_53C67 (*(uint32_t *)(uintptr_t)0x53C67u)

#define VGA     ((uint8_t *)(uintptr_t)0x000A0000u)
#define VGA_LEN (320 * 200)

#define NDATO 4

static uint32_t seed = 0xD1A105u;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}
static void fill_rand(uint8_t *p, size_t n) { size_t i; for (i = 0; i < n; i++) p[i] = (uint8_t)rnd(); }

static int failures;
static unsigned cases_run;

static size_t make_dato(uint8_t *buf)
{
    uint32_t off[NDATO];
    size_t   pos = 4 * NDATO;
    int      i;

    for (i = 0; i < NDATO; i++) {
        uint8_t *p = buf + pos;
        unsigned w = 1 + (unsigned)rnd() % 40;
        unsigned h = 1 + (unsigned)rnd() % 30;
        unsigned total, done = 0;

        off[i] = (uint32_t)pos;
        *(uint16_t *)p       = (uint16_t)w;
        *(uint16_t *)(p + 2) = (uint16_t)h;
        p += 4;
        total = w * h;
        while (done < total) {
            unsigned left = total - done;
            if ((rnd() & 1) || left == 1) {
                *p++ = (uint8_t)(rnd() % 0xC1);
                done++;
            } else {
                unsigned maxlen = left < 63 ? left : 63;
                unsigned len = 1 + (unsigned)rnd() % maxlen;
                *p++ = (uint8_t)(0xC0 + len);
                *p++ = (uint8_t)rnd();
                done += len;
            }
        }
        pos = (size_t)(p - buf);
    }
    for (i = 0; i < NDATO; i++)
        ((uint32_t *)buf)[i] = off[i];
    return pos;
}

static uint8_t g_before[VGA_LEN];
static uint8_t g_orig[VGA_LEN];

static int cmp_vga(const char *what, unsigned id)
{
    int k;
    for (k = 0; k < VGA_LEN; k++)
        if (VGA[k] != g_orig[k]) {
            printf("FAIL %s case %u: VGA diff @+%d orig=%02X ours=%02X\n",
                   what, id, k, g_orig[k], VGA[k]);
            failures++;
            return -1;
        }
    return 0;
}

static void test_blit(unsigned id)
{
    uint8_t  dato[16 * 1024];
    int      idx = (int)(rnd() % NDATO);
    static const int boxes[] = { 0x728, 0x9017, 0x1234 };
    int      box = boxes[rnd() % 3];

    make_dato(dato);
    fill_rand(VGA, VGA_LEN);
    memcpy(g_before, VGA, VGA_LEN);

    G_53A85 = (uint32_t)(uintptr_t)dato;
    G_53C67 = (uint32_t)box;
    ORIG_BLIT(idx);
    memcpy(g_orig, VGA, VGA_LEN);

    memcpy(VGA, g_before, VGA_LEN);
    dlg_blit_dato(dato, box, idx);
    cases_run++;
    cmp_vga("blit_dato", id);
}

static void test_scroll(unsigned id)
{
    static const int boxes[] = { 0x728, 0x9017, 0x1234 };
    int box = boxes[rnd() % 3];

    fill_rand(VGA, VGA_LEN);
    memcpy(g_before, VGA, VGA_LEN);

    G_53C67 = (uint32_t)box;
    ORIG_SCROLL();
    memcpy(g_orig, VGA, VGA_LEN);

    memcpy(VGA, g_before, VGA_LEN);
    dlg_scroll_text(box);
    cases_run++;
    cmp_vga("scroll_text", id);
}

int main(int argc, char **argv)
{
    le_image le;
    int applied = 0;
    unsigned i;

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    for (i = 0; i < 400; i++) {
        test_blit(1000 + i);
        test_scroll(2000 + i);
        if (failures) break;
    }

    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
