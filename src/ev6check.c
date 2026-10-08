/* ev6check.c - differential test for src/game/unit_load.c.
 *
 * The 0x10B4E chain sequences the Watcom CRT (fopen/fseek/fread/fwrite/
 * fclose/malloc/free). This harness redirects exactly those entries to the
 * host libc - the substitution the final port performs anyway - and then runs
 * the original machine code and the translation on the same synthetic world:
 *
 *   - a synthetic FDICON.B24 (6-byte header + 140 48-byte table records +
 *     140 sprite blobs) and FDFIELD.DAT (an LMI container whose entry
 *     3*dword_53C03+2 holds the per-unit template bytes);
 *   - the record table, the 26-byte-stride state array, the map cells and
 *     every global the chain reads;
 *   - all the FD2 helper tables the code indexes (0x4DF4C, 0x4E821/838/84F/
 *     8BC) are left as machine code - both sides call them the same way.
 *
 * Compared: the new 80-byte records, the FD2.TMP atlas prefix, the written
 * FD2.TMP file prefix and the globals (dword_53BEB / 53BDF / 539EC / 53B17 /
 * 53A59 / 53BFF), byte for byte.
 *
 * Test B drives 0x1B750 (unit_metrics) directly with a random record so the
 * rec[+34]/[+35]/[+36] scaling paths are hit too.
 *
 * Build: pwsh -File build.ps1 -Target ev6check
 * Run   : build\ev6check.exe [--cases=N]
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/unit_load.h"

#define W32(x)  (*(uint32_t *)(uintptr_t)(x))
#define W8(x)   (*(uint8_t  *)(uintptr_t)(x))

/* --- libc adapters installed over the game's CRT entry points --------- */
static void *__cdecl stub_malloc(size_t n)                     { return malloc(n); }
static void  __cdecl stub_free(void *p)                        { free(p); }
static void *__cdecl stub_fopen(const char *n, const char *m)  { return fopen(n, m); }
static int   __cdecl stub_fclose(void *f)                      { return fclose((FILE *)f); }
static int   __cdecl stub_fseek(void *f, long off, int wh)     { return fseek((FILE *)f, off, wh); }
static size_t __cdecl stub_fread(void *b, size_t sz, size_t c, void *f)
{ return fread(b, sz, c, (FILE *)f); }
static size_t __cdecl stub_fwrite(const void *b, size_t sz, size_t c, void *f)
{ return fwrite(b, sz, c, (FILE *)f); }

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
#define ORIG_WRITE  0x377A3u

/* --- original entry points --------------------------------------------- */
typedef int (*build_fn)(int idx);
typedef void (*metrics_fn)(int idx);
#define O_BUILD   ((build_fn)(uintptr_t)0x00010B4Eu)
#define O_METRICS ((metrics_fn)(uintptr_t)0x0001B750u)

/* --- synthetic world --------------------------------------------------- */
#define REC_STRIDE 80
#define REC_BYTES  0x8000
#define STATE_N    0x1000
#define CELL_N     0x1000
#define ATLAS_N    0x32A00
#define NIDX       0x80
#define NBE3       6
#define NIDXFILE   140

static uint8_t  rec_o[REC_BYTES],  rec_c[REC_BYTES];
static uint8_t  st_o[STATE_N],     st_c[STATE_N];
static uint8_t  cell_o[CELL_N],    cell_c[CELL_N];
static uint8_t  atlas_o[ATLAS_N],  atlas_c[ATLAS_N];
static uint8_t  tmp_o[ATLAS_N],    tmp_c[ATLAS_N];
static uint32_t b17_o[64],         b17_c[64];
static uint32_t glob_o[16],        glob_c[16];

static uint32_t seed;
static uint32_t rnd(void) { seed = seed * 1664525u + 1013904223u; return seed >> 8; }
static void fill_rand(uint8_t *p, size_t n) { while (n--) *p++ = (uint8_t)rnd(); }

static const uint32_t G_BEB   = 0x53BEBu;
static const uint32_t G_BDF   = 0x53BDFu;
static const uint32_t G_539EC = 0x539ECu;
static const uint32_t G_BFF   = 0x53BFFu;
static const uint32_t G_A59   = 0x53A59u;

/* deterministic input files (separate RNG so they do not disturb the world) */
static void write_inputs(uint32_t s)
{
    FILE *f;
    uint8_t hdr[6] = { 'F', 'D', '2', 0, 0, 0 };
    uint8_t recs[NIDXFILE * 48 + 16];
    uint8_t blob[200];
    uint32_t data_at = 6 + NIDXFILE * 48;
    int i, k;

    seed = s ^ 0x12345678u;
    memset(recs, 0, sizeof recs);    for (i = 0; i < NIDXFILE; i++) {
        uint32_t base = data_at + (uint32_t)i * 160u;
        uint32_t *d = (uint32_t *)(void *)(recs + i * 48);
        d[0] = base;
        for (k = 1; k < 12; k++)
            d[k] = base + 8u + (uint32_t)(rnd() % 64u);
        d[12] = base + 100u;
    }
    f = fopen("FDICON.B24", "wb");
    if (!f) { printf("cannot write FDICON.B24\n"); exit(2); }
    fwrite(hdr, 1, sizeof hdr, f);
    fwrite(recs, 1, NIDXFILE * 48, f);
    for (i = 0; i < NIDXFILE; i++) {
        fill_rand(blob, sizeof blob);
        fwrite(blob, 1, sizeof blob, f);
    }
    fclose(f);

    /* LMI container: 6-byte header, 9-entry offset table at +6, data. */
    {
        uint8_t  h2[6] = { 'L', 'M', 'I', '1', 0, 0 };
        uint32_t off[9];
        uint8_t  data[128];
        uint32_t d0 = 6 + 4 * 9;
        fill_rand(data, sizeof data);
        /* Keep the per-unit template x/y inside the (small) test map so the
         * 3x3 reveal walk cannot index past the cell buffer. */
        for (i = 0; i < NBE3; i++) {
            data[6 * i + 2] = (uint8_t)(1 + i);
            data[6 * i + 4] = (uint8_t)i;
        }
        off[0] = 0;
        off[1] = 0;
        off[2] = d0;
        off[3] = d0 + (uint32_t)sizeof data;
        for (i = 4; i < 9; i++)
            off[i] = off[3];
        f = fopen("FDFIELD.DAT", "wb");
        if (!f) { printf("cannot write FDFIELD.DAT\n"); exit(2); }
        fwrite(h2, 1, sizeof h2, f);
        fwrite(off, 4, 9, f);
        fwrite(data, 1, sizeof data, f);
        fclose(f);
    }
}

static int read_file(const char *path, uint8_t *out, size_t n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fread(out, 1, n, f) != n) { fclose(f); return -1; }
    fclose(f);
    return 0;
}

#define SNAP_G(dst) do { \
        dst[0] = W32(G_BEB); dst[1] = W32(G_BDF); dst[2] = W32(G_539EC); \
        dst[3] = W32(G_BFF); dst[4] = W32(G_A59); \
    } while (0)

static void snap_b17(uint32_t *dst)
{
    uint32_t n = W32(G_BDF);
    memset(dst, 0, 64 * 4);
    if (n > 64) n = 64;
    memcpy(dst, (const void *)(uintptr_t)0x53B17u, n * 4);
}

static int failures;
static unsigned cases_run;

static void setup_world(uint32_t s, uint8_t *rec, uint8_t *st, uint8_t *cell)
{
    int i;

    seed = s;
    fill_rand(rec, REC_BYTES);
    fill_rand(st, STATE_N);
    fill_rand(cell, CELL_N);
    cell[0] = 4; cell[2] = 4;              /* util_fix_records count = 16 */

    W32(0x53A45) = (uint32_t)(uintptr_t)rec;
    W32(0x53A55) = (uint32_t)(uintptr_t)st;
    W32(0x53A51) = (uint32_t)(uintptr_t)cell;
    W32(0x53AC1) = 4;
    W32(0x53AC5) = 4;
    W32(0x53AA9) = 0;
    W32(0x53AAD) = 0;
    W32(0x53BEF) = rnd() & 0xFFu;
    W32(G_BEB) = 1 + (rnd() % 2u);
    W32(G_BDF) = 0;
    W32(G_539EC) = 0;
    W32(G_A59) = 0;
    W32(G_BFF) = 0;
    W32(0x53A61) = 0;
    W32(0x53B17) = 0;                      /* first slot only; rest zeroed below */
    memset((void *)(uintptr_t)0x53B17u, 0, 256);
    W32(0x53C03) = 0;                      /* FDFIELD.DAT index 2 */
    W32(0x53BE3) = NBE3;
    W8(0x53AFA) = (uint8_t)((s >> 7) & 1u);

    /* The pre-existing records feed the 3x3 reveal walk, which indexes the
     * cell buffer by their raw x/y bytes - keep them inside the small map. */
    for (i = 0; i < (int)W32(G_BEB); i++) {
        rec[REC_STRIDE * i + 0] = (uint8_t)(rnd() % 4u);
        rec[REC_STRIDE * i + 1] = (uint8_t)(rnd() % 4u);
    }

    for (i = 0; i < NBE3; i++) {
        uint8_t *p = st + 26 * i;
        p[152] = 3;                        /* 0x10B4E's match byte          */
        p[131] = (uint8_t)rnd();           /* v25                           */
        p[132] = (uint8_t)(rnd() % NIDX);  /* v24 -> FDICON sprite          */
        p[135] = (uint8_t)(1 + rnd() % 12);/* v28                           */
        p[136] = (uint8_t)rnd();
        p[137] = (uint8_t)rnd();
    }
}

static void run_case_a(unsigned id, uint32_t s)
{
    uint32_t used_o, used_c, i;
    int      ret_o, ret_c;

    /* ---- original --------------------------------------------------- */
    write_inputs(s);
    setup_world(s, rec_o, st_o, cell_o);
    ret_o = O_BUILD(3);
    SNAP_G(glob_o);
    snap_b17(b17_o);
    used_o = W32(G_539EC);
    if (used_o > ATLAS_N) used_o = ATLAS_N;
    if (W32(0x53A61))
        memcpy(atlas_o, (const void *)(uintptr_t)W32(0x53A61), used_o);
    read_file("FD2.TMP", tmp_o, ATLAS_N);

    /* ---- translation ------------------------------------------------- */
    setup_world(s, rec_c, st_c, cell_c);
    ret_c = unit_sprites_build(3);
    SNAP_G(glob_c);
    snap_b17(b17_c);
    used_c = W32(G_539EC);
    if (used_c > ATLAS_N) used_c = ATLAS_N;
    if (W32(0x53A61))
        memcpy(atlas_c, (const void *)(uintptr_t)W32(0x53A61), used_c);
    read_file("FD2.TMP", tmp_c, ATLAS_N);

    cases_run++;

    if (ret_o != ret_c) {
        printf("FAIL case %u: return orig=%d ours=%d\n", id, ret_o, ret_c);
        failures++; return;
    }
    if (memcmp(rec_o, rec_c, REC_BYTES) != 0) {
        for (i = 0; i < REC_BYTES; i++)
            if (rec_o[i] != rec_c[i]) break;
        printf("FAIL case %u: record byte %u orig=%02X ours=%02X\n",
               id, i, rec_o[i], rec_c[i]);
        failures++; return;
    }
    if (memcmp(st_o, st_c, STATE_N) != 0) {
        for (i = 0; i < STATE_N; i++)
            if (st_o[i] != st_c[i]) break;
        printf("FAIL case %u: state byte %u orig=%02X ours=%02X\n",
               id, i, st_o[i], st_c[i]);
        failures++; return;
    }
    if (memcmp(cell_o, cell_c, CELL_N) != 0) {
        for (i = 0; i < CELL_N; i++)
            if (cell_o[i] != cell_c[i]) break;
        printf("FAIL case %u: cell byte %u orig=%02X ours=%02X\n",
               id, i, cell_o[i], cell_c[i]);
        failures++; return;
    }
    for (i = 0; i < 5; i++)
        if (glob_o[i] != glob_c[i]) {
            printf("FAIL case %u: global[%u] orig=%u ours=%u\n",
                   id, i, glob_o[i], glob_c[i]);
            failures++; return;
        }
    if (memcmp(b17_o, b17_c, sizeof b17_o) != 0) {
        printf("FAIL case %u: dword_53B17 table differs\n", id);
        failures++; return;
    }
    if (used_o != used_c || memcmp(atlas_o, atlas_c, used_o) != 0) {
        /* Only the header slots that were actually written and the blob area
         * are initialised; the rest of the malloc'd atlas is garbage, so
         * compare slot table [0,48*BDF) and blobs [1920, used). */
        uint32_t hdr = W32(G_BDF) * 48u;
        int      bad = 0;
        if (hdr > 1920u) hdr = 1920u;
        if (used_o != used_c) bad = 1;
        else if (memcmp(atlas_o, atlas_c, hdr) != 0) bad = 1;
        else if (used_o > 1920u &&
                 memcmp(atlas_o + 1920u, atlas_c + 1920u,
                        used_o - 1920u) != 0) bad = 1;
        if (bad) {
            printf("FAIL case %u: atlas used orig=%u ours=%u (hdr=%u)\n",
                   id, used_o, used_c, hdr);
            for (i = 0; i < used_o && used_o == used_c; i++)
                if (atlas_o[i] != atlas_c[i]) {
                    printf("  first atlas diff at +%u orig=%02X ours=%02X\n",
                           i, atlas_o[i], atlas_c[i]);
                    break;
                }
            failures++; return;
        }
    }
    {
        uint32_t hdr = W32(G_BDF) * 48u;
        int      bad = 0;
        if (hdr > 1920u) hdr = 1920u;
        if (used_o != used_c) bad = 1;
        else if (memcmp(tmp_o, tmp_c, hdr) != 0) bad = 1;
        else if (used_o > 1920u &&
                 memcmp(tmp_o + 1920u, tmp_c + 1920u, used_o - 1920u) != 0) bad = 1;
        if (bad) {
            for (i = 0; i < used_o && used_o == used_c; i++)
                if (tmp_o[i] != tmp_c[i]) break;
            printf("FAIL case %u: FD2.TMP byte %u orig=%02X ours=%02X\n",
                   id, i, tmp_o[i], tmp_c[i]);
            failures++; return;
        }
    }
}

/* ---- test B: 0x1B750 unit_metrics on a random record ------------------ */
static void run_case_b(unsigned id, uint32_t s)
{
    uint32_t i;

    seed = s;
    fill_rand(rec_o, REC_BYTES);
    W32(0x53A45) = (uint32_t)(uintptr_t)rec_o;
    O_METRICS(2);

    seed = s;
    memcpy(rec_c, rec_o, REC_BYTES);
    W32(0x53A45) = (uint32_t)(uintptr_t)rec_c;
    unit_metrics(2);

    cases_run++;
    for (i = 0; i < REC_BYTES; i++)
        if (rec_o[i] != rec_c[i]) {
            printf("FAIL case B%u: record byte %u orig=%02X ours=%02X\n",
                   id, i, rec_o[i], rec_c[i]);
            failures++;
            return;
        }
}

int main(int argc, char **argv)
{
    le_image le;
    int      applied = 0;
    unsigned cases = 60, i;

    setvbuf(stdout, NULL, _IONBF, 0);
    for (i = 1; i < (unsigned)argc; i++)
        if (!strncmp(argv[i], "--cases=", 8)) cases = (unsigned)atoi(argv[i] + 8);

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    if (le_open(&le, (argc > 1 && argv[1][0] != '-') ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied\n", (unsigned)applied);

    HOOK(ORIG_MALLOC, stub_malloc);
    HOOK(ORIG_FREE,   stub_free);
    HOOK(ORIG_FOPEN,  stub_fopen);
    HOOK(ORIG_FCLOSE, stub_fclose);
    HOOK(ORIG_FSEEK,  stub_fseek);
    HOOK(ORIG_READ,   stub_fread);
    HOOK(ORIG_WRITE,  stub_fwrite);

    for (i = 0; i < cases; i++)
        run_case_a(i, 0xA5A50000u + 0x9E3779B9u * i);
    for (i = 0; i < cases; i++)
        run_case_b(i, 0x13570000u + 0x9E3779B9u * i);

    remove("FDICON.B24");
    remove("FDFIELD.DAT");
    remove("FD2.TMP");

    if (failures) {
        printf("FAILED: %u failures in %u cases\n", failures, cases_run);
        return 1;
    }
    printf("PASS: %u cases, 0 failures\n", cases_run);
    le_close(&le);
    return 0;
}
