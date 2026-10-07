/* rescheck.c - differential test for the LMI resource loader (0x111BA).
 *
 * The original sequences the Watcom CRT (fopen/fseek/fread/malloc/free/
 * fclose). This harness redirects exactly those six CRT entry points to the
 * host libc (the substitution the final port performs anyway) and then runs
 * the original 0x111BA and src/game/res.c on the same synthetic container,
 * comparing the returned resource, its size and the guest dword_53BFF.
 *
 * Build: pwsh -File build.ps1 -Target rescheck
 * Run   : build\rescheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/res.h"

typedef void *(__cdecl *load_fn)(const char *, void *, int);
#define ORIG_LOAD  ((load_fn)(uintptr_t)0x111BA)
#define GUEST_SIZE (*(const uint32_t *)(uintptr_t)0x53BFFu)

/* --- libc adapters installed over the game's CRT entry points --------- */
static void *__cdecl stub_malloc(size_t n)                     { return malloc(n); }
static void  __cdecl stub_free(void *p)                        { free(p); }
static void *__cdecl stub_fopen(const char *n, const char *m)  { return fopen(n, m); }
static int   __cdecl stub_fclose(void *f)                      { return fclose((FILE *)f); }
static int   __cdecl stub_fseek(void *f, long off, int wh)     { return fseek((FILE *)f, off, wh); }
static size_t __cdecl stub_fread(void *b, size_t sz, size_t c, void *f)
{ return fread(b, sz, c, (FILE *)f); }

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
#define ORIG_MALLOC 0x3706Eu
#define ORIG_FREE   0x3776Eu
#define ORIG_FOPEN  0x37324u
#define ORIG_FCLOSE 0x3759Cu
#define ORIG_FSEEK  0x37940u
#define ORIG_READ   0x373CAu

#define NCONTAINER 8
#define MAXLEN     64

static uint32_t seed = 0x1234ABCDu;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}
static void fill_rand(uint8_t *p, size_t n) { size_t i; for (i = 0; i < n; i++) p[i] = (uint8_t)rnd(); }

static int failures;
static unsigned cases_run;
static char     g_path[MAX_PATH];
static uint8_t  g_data[NCONTAINER][MAXLEN];
static int      g_len[NCONTAINER];

static int make_container(const char *path)
{
    uint8_t  hdr[6] = { 'L', 'M', 'I', '1', 0, 0 };
    uint32_t off[NCONTAINER + 1];
    uint32_t dir_at = 6;
    uint32_t data_at = dir_at + 4 * (NCONTAINER + 1);
    FILE    *f;
    int      i;

    for (i = 0; i < NCONTAINER; i++) {
        g_len[i] = 1 + (int)(rnd() % MAXLEN);
        fill_rand(g_data[i], g_len[i]);
    }
    off[0] = data_at;
    for (i = 0; i < NCONTAINER; i++)
        off[i + 1] = off[i] + (uint32_t)g_len[i];

    f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(hdr, 1, sizeof hdr, f);
    fwrite(off, 4, NCONTAINER + 1, f);
    for (i = 0; i < NCONTAINER; i++)
        fwrite(g_data[i], 1, (size_t)g_len[i], f);
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    le_image le;
    int      applied = 0;
    int      round;

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    HOOK(ORIG_MALLOC,  stub_malloc);
    HOOK(ORIG_FREE,    stub_free);
    HOOK(ORIG_FOPEN,   stub_fopen);
    HOOK(ORIG_FCLOSE,  stub_fclose);
    HOOK(ORIG_FSEEK,   stub_fseek);
    HOOK(ORIG_READ,    stub_fread);

    snprintf(g_path, sizeof g_path, "E:\\FD2\\port\\build\\res_test.dat");

    for (round = 0; round < 20; round++) {
        int i;
        if (make_container(g_path) != 0) { printf("cannot write container\n"); return 2; }

        for (i = 0; i < NCONTAINER; i++) {
            void    *oa, *ob;
            uint32_t osz, msz;
            int      use_old = (int)(rnd() & 1);

            oa = ORIG_LOAD(g_path, use_old ? malloc(16) : NULL, i);
            osz = GUEST_SIZE;              /* snapshot after the original */
            ob = res_load(g_path, use_old ? malloc(16) : NULL, i);
            msz = GUEST_SIZE;              /* the translation overwrites it */
            cases_run++;

            if (osz != (uint32_t)g_len[i] || msz != (uint32_t)g_len[i]) {
                printf("FAIL round %d idx %d: size orig=%u ours=%u expect=%d\n",
                       round, i, osz, msz, g_len[i]);
                failures++;
            } else if (memcmp(oa, g_data[i], g_len[i]) != 0) {
                printf("FAIL round %d idx %d: original data mismatch\n", round, i);
                failures++;
            } else if (memcmp(ob, g_data[i], g_len[i]) != 0 ||
                       memcmp(oa, ob, g_len[i]) != 0) {
                printf("FAIL round %d idx %d: translation data mismatch\n", round, i);
                failures++;
            }
            free(oa);
            free(ob);
            if (failures) goto done;
        }
    }

done:
    remove(g_path);
    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
