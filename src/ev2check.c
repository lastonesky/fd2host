/* ev2check.c - differential test for src/game/ev2.c (funcs_1199C batch 2).
 *
 * Each case runs the original machine code and the C translation from the
 * same synthetic world and compares everything observable:
 *
 *   - the complete service-event log (vm_run's nine arguments, status_set's
 *     three, unit_add / load / wait / seq / clear / view / flush) - both sides
 *     call the services at their original addresses, which this harness hooks
 *     to recording stubs;
 *   - the whole obj1 data object (0x50000..0x556B0) - the handlers' direct
 *     writes go to dword_51A83 / dword_53A55 / dword_53AD5 / dword_53AA9 /
 *     dword_53AAD / qword_53AB1 / dword_53BEF and the record table pointer;
 *   - the three pointed-to buffers (256-record table, script state, status
 *     block) and the VM stream, byte for byte.
 *
 * The services are hooked so the file/heap service 0x10B4E and the fade
 * sequence 0x35B78 never touch the disk; the pure record writers
 * (0x344F2/0x112A5) are hooked too so the comparison is about the handler's
 * own logic, exactly as src/evcheck.c does for the first batch.
 *
 * Batch verification: `--only=0x35298,0x35321,...` runs just that subset, so a
 * 30-function round can be checked 3-5 entries at a time.
 *
 * Build: pwsh -File build.ps1 -Target ev2check
 * Run   : build\ev2check.exe [--only=addr,...] [--cases=N]
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/ev2.h"

#define W32(x)  (*(uint32_t *)(uintptr_t)(x))
#define W8(x)   (*(uint8_t  *)(uintptr_t)(x))

/* --- original entry points --------------------------------------------- */
typedef void (*h1_fn)(int);
typedef void (*h2_fn)(int, int);

#define O_135DD ((h2_fn)(uintptr_t)0x000135DDu)
#define O_35298 ((h1_fn)(uintptr_t)0x00035298u)
#define O_35321 ((h1_fn)(uintptr_t)0x00035321u)
#define O_353B5 ((h1_fn)(uintptr_t)0x000353B5u)
#define O_353E7 ((h1_fn)(uintptr_t)0x000353E7u)
#define O_353FA ((h1_fn)(uintptr_t)0x000353FAu)
#define O_3540F ((h1_fn)(uintptr_t)0x0003540Fu)
#define O_35422 ((h1_fn)(uintptr_t)0x00035422u)
#define O_3551C ((h1_fn)(uintptr_t)0x0003551Cu)
#define O_3553F ((h1_fn)(uintptr_t)0x0003553Fu)
#define O_355B7 ((h1_fn)(uintptr_t)0x000355B7u)
#define O_35638 ((h1_fn)(uintptr_t)0x00035638u)
#define O_35677 ((h1_fn)(uintptr_t)0x00035677u)
#define O_35997 ((h1_fn)(uintptr_t)0x00035997u)
#define O_359CB ((h1_fn)(uintptr_t)0x000359CBu)
#define O_35BEE ((h1_fn)(uintptr_t)0x00035BEEu)
#define O_35C1D ((h1_fn)(uintptr_t)0x00035C1Du)
#define O_35D85 ((h1_fn)(uintptr_t)0x00035D85u)
#define O_35F79 ((h1_fn)(uintptr_t)0x00035F79u)
#define O_36214 ((h1_fn)(uintptr_t)0x00036214u)
#define O_36228 ((h1_fn)(uintptr_t)0x00036228u)
#define O_362B0 ((h1_fn)(uintptr_t)0x000362B0u)
#define O_362C5 ((h1_fn)(uintptr_t)0x000362C5u)
#define O_363DE ((h1_fn)(uintptr_t)0x000363DEu)
#define O_36416 ((h1_fn)(uintptr_t)0x00036416u)
#define O_3642E ((h1_fn)(uintptr_t)0x0003642Eu)
#define O_36439 ((h1_fn)(uintptr_t)0x00036439u)
#define O_36440 ((h1_fn)(uintptr_t)0x00036440u)
#define O_36447 ((h1_fn)(uintptr_t)0x00036447u)
#define O_3644E ((h1_fn)(uintptr_t)0x0003644Eu)

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

/* --- synthetic world --------------------------------------------------- */
#define REC_COUNT 256
#define REC_BYTES (REC_COUNT * 80)
#define SNAP_BASE 0x00050000u
#define SNAP_N    0x000056B0u
#define STATE_N   64
#define STATUS_N  64
#define STREAM_N  16

static uint8_t rec_in[REC_BYTES];
static uint8_t rec_o[REC_BYTES], rec_c[REC_BYTES];
static uint8_t state_in[STATE_N], state_o[STATE_N], state_c[STATE_N];
static uint8_t stat_in[STATUS_N], stat_o[STATUS_N], stat_c[STATUS_N];
static uint8_t strm_in[STREAM_N], strm_o[STREAM_N], strm_c[STREAM_N];
static uint8_t obj1_o[SNAP_N], obj1_c[SNAP_N];

static uint32_t seed;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}
static void fill_rand(uint8_t *p, size_t n) { while (n--) *p++ = (uint8_t)rnd(); }

/* --- event log --------------------------------------------------------- */
enum { EV_VM, EV_STATUS, EV_UNIT, EV_LOAD, EV_WAIT, EV_SEQ, EV_CLEAR,
       EV_VIEW, EV_FLUSH, EV_N };
static const char *const kind_name[EV_N] = {
    "vm_run", "status", "unit_add", "load", "wait", "seq", "clear",
    "view", "flush"
};
#define MAXEV 512
struct event { int kind; int v[9]; };
static struct event g_ev[MAXEV];
static int g_nev;

static void ev_log(int kind, int a, int b, int c, int d, int e, int f, int g,
                   int h, int i)
{
    struct event *ev;
    if (g_nev >= MAXEV) { printf("FAIL: event log overflow\n"); exit(1); }
    ev = &g_ev[g_nev++];
    ev->kind = kind;
    ev->v[0] = a; ev->v[1] = b; ev->v[2] = c; ev->v[3] = d; ev->v[4] = e;
    ev->v[5] = f; ev->v[6] = g; ev->v[7] = h; ev->v[8] = i;
}

/* --- service stubs ----------------------------------------------------- */
static int __cdecl stub_vm(void *st, int sub, int addr, int pitch, int fg,
                           int shadow, int bgfill, int step, int wait)
{
    (void)st;   /* both sides pass dword_53A79, i.e. the stream base */
    ev_log(EV_VM, 0, sub, addr, pitch, fg, shadow, bgfill, step, wait);
    return 0;
}
static uint32_t __cdecl stub_status(int start, int end, int value)
{
    ev_log(EV_STATUS, start, end, value, 0, 0, 0, 0, 0, 0);
    return 0;
}
static int __cdecl stub_unit(int id)
{
    ev_log(EV_UNIT, id, 0, 0, 0, 0, 0, 0, 0, 0);
    return 0;
}
static int __cdecl stub_load(int index)
{
    ev_log(EV_LOAD, index, 0, 0, 0, 0, 0, 0, 0, 0);
    return 0;
}
static uint32_t __cdecl stub_wait(int ticks)
{
    ev_log(EV_WAIT, ticks, 0, 0, 0, 0, 0, 0, 0, 0);
    return 0;
}
static void __cdecl stub_seq(int a, int b, int c)
{
    ev_log(EV_SEQ, a, b, c, 0, 0, 0, 0, 0, 0);
}
static void __cdecl stub_clear(int from)
{
    ev_log(EV_CLEAR, from, 0, 0, 0, 0, 0, 0, 0, 0);
}
static void __cdecl stub_view(int mode)
{
    ev_log(EV_VIEW, mode, 0, 0, 0, 0, 0, 0, 0, 0);
}
static void __cdecl stub_flush(void)
{
    ev_log(EV_FLUSH, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

/* --- world setup / capture --------------------------------------------- */
struct cap {
    uint8_t obj1[SNAP_N];
    uint8_t rec[REC_BYTES];
    uint8_t state[STATE_N];
    uint8_t stat[STATUS_N];
    uint8_t strm[STREAM_N];
    int     nev;
    struct event ev[MAXEV];
};
static struct cap cap_o, cap_c;

/* x/y view targets and move counters are kept in a small range so the scroll
 * loops in 0x135DD terminate quickly. */
static int g_argx, g_argy, g_arg;

static void setup_world(uint32_t s)
{
    seed = s;
    fill_rand(rec_in, REC_BYTES);
    fill_rand(state_in, STATE_N);
    fill_rand(stat_in, STATUS_N);
    fill_rand(strm_in, STREAM_N);

    memcpy(rec_o, rec_in, REC_BYTES);
    memcpy(rec_c, rec_in, REC_BYTES);
    memcpy(state_o, state_in, STATE_N);
    memcpy(state_c, state_in, STATE_N);
    memcpy(stat_o, stat_in, STATUS_N);
    memcpy(stat_c, stat_in, STATUS_N);
    memcpy(strm_o, strm_in, STREAM_N);
    memcpy(strm_c, strm_in, STREAM_N);

    W32(0x53A45) = (uint32_t)(uintptr_t)rec_o;
    W32(0x53A55) = (uint32_t)(uintptr_t)state_o;
    W32(0x53AD5) = (uint32_t)(uintptr_t)stat_o;
    W32(0x53A79) = (uint32_t)(uintptr_t)strm_o;
    W32(0x51A83) = 0xDEADBEEFu;
    W32(0x53BEF) = (uint32_t)(rnd() & 0xFFu);
    W32(0x53BEB) = (uint32_t)(rnd() % 12u);
    W32(0x53AA9) = (uint32_t)((int)(rnd() % 9u) - 4);
    W32(0x53AAD) = (uint32_t)((int)(rnd() % 9u) - 4);
    W32(0x53AB1) = rnd();
    W32(0x53AB5) = rnd();
    g_argx = (int)(rnd() % 9u) - 4;
    g_argy = (int)(rnd() % 9u) - 4;
    g_arg  = (int)(rnd() % 200u);
    g_nev  = 0;
}
/* The C side reads the same globals but must point at the C buffers. */
static void point_c_side(void)
{
    W32(0x53A45) = (uint32_t)(uintptr_t)rec_c;
    W32(0x53A55) = (uint32_t)(uintptr_t)state_c;
    W32(0x53AD5) = (uint32_t)(uintptr_t)stat_c;
    W32(0x53A79) = (uint32_t)(uintptr_t)strm_c;
}

static void capture(struct cap *c)
{
    memcpy(c->obj1, (const void *)(uintptr_t)SNAP_BASE, SNAP_N);
    memcpy(c->rec, rec_o, REC_BYTES);
    memcpy(c->state, state_o, STATE_N);
    memcpy(c->stat, stat_o, STATUS_N);
    memcpy(c->strm, strm_o, STREAM_N);
    c->nev = g_nev;
    memcpy(c->ev, g_ev, sizeof g_ev);
}
static void capture_c(struct cap *c)
{
    /* The four pointer globals legitimately hold each side's own buffers;
     * normalise them back to the original side's addresses so the obj1 diff
     * compares behaviour, not allocation. */
    W32(0x53A45) = (uint32_t)(uintptr_t)rec_o;
    W32(0x53A55) = (uint32_t)(uintptr_t)state_o;
    W32(0x53AD5) = (uint32_t)(uintptr_t)stat_o;
    W32(0x53A79) = (uint32_t)(uintptr_t)strm_o;
    memcpy(c->obj1, (const void *)(uintptr_t)SNAP_BASE, SNAP_N);
    memcpy(c->rec, rec_c, REC_BYTES);
    memcpy(c->state, state_c, STATE_N);
    memcpy(c->stat, stat_c, STATUS_N);
    memcpy(c->strm, strm_c, STREAM_N);
    c->nev = g_nev;
    memcpy(c->ev, g_ev, sizeof g_ev);
}

static int failures, cases_run;

static int cmp_cap(unsigned id, const char *name,
                   const struct cap *o, const struct cap *t)
{
    int bad = 0, i, j;

    if (o->nev != t->nev) {
        printf("FAIL case %u %s: event count orig=%d ours=%d\n",
               id, name, o->nev, t->nev);
        failures++; bad = 1;
    }
    for (i = 0; i < o->nev && i < t->nev; i++) {
        int diff = o->ev[i].kind != t->ev[i].kind;
        for (j = 0; j < 9; j++)
            if (o->ev[i].v[j] != t->ev[i].v[j]) diff = 1;
        if (diff) {
            printf("FAIL case %u %s: event %d orig %s", id, name, i,
                   kind_name[o->ev[i].kind]);
            for (j = 0; j < 9; j++) printf(",%d", o->ev[i].v[j]);
            printf(" ours %s", kind_name[t->ev[i].kind]);
            for (j = 0; j < 9; j++) printf(",%d", t->ev[i].v[j]);
            printf("\n");
            failures++; bad = 1;
            break;
        }
    }
    for (i = 0; i < SNAP_N && !bad; i++)
        if (o->obj1[i] != t->obj1[i]) {
            printf("FAIL case %u %s: obj1+%X orig=%02X ours=%02X\n", id, name,
                   i, o->obj1[i], t->obj1[i]);
            failures++; bad = 1;
        }
    for (i = 0; i < REC_BYTES && !bad; i++)
        if (o->rec[i] != t->rec[i]) {
            printf("FAIL case %u %s: record byte %d orig=%02X ours=%02X\n",
                   id, name, i, o->rec[i], t->rec[i]);
            failures++; bad = 1;
        }
    for (i = 0; i < STATE_N && !bad; i++)
        if (o->state[i] != t->state[i]) {
            printf("FAIL case %u %s: state+%d orig=%02X ours=%02X\n", id, name,
                   i, o->state[i], t->state[i]);
            failures++; bad = 1;
        }
    for (i = 0; i < STATUS_N && !bad; i++)
        if (o->stat[i] != t->stat[i]) {
            printf("FAIL case %u %s: status+%d orig=%02X ours=%02X\n", id, name,
                   i, o->stat[i], t->stat[i]);
            failures++; bad = 1;
        }
    for (i = 0; i < STREAM_N && !bad; i++)
        if (o->strm[i] != t->strm[i]) {
            printf("FAIL case %u %s: stream+%d orig=%02X ours=%02X\n", id, name,
                   i, o->strm[i], t->strm[i]);
            failures++; bad = 1;
        }
    return bad;
}

/* --- one (original, C) pair -------------------------------------------- */
typedef void (*pair_fn)(void);

static void run_pair(unsigned id, const char *name, pair_fn orig, pair_fn ours,
                     uint32_t s)
{
    setup_world(s);
    orig();
    capture(&cap_o);

    setup_world(s);
    point_c_side();
    ours();
    capture_c(&cap_c);

    cases_run++;
    cmp_cap(id, name, &cap_o, &cap_c);
}

/* --- per-function wrappers --------------------------------------------- */
static void o_135DD(void) { O_135DD(g_argx, g_argy); }
static void c_135DD(void) { ev2_135DD(g_argx, g_argy); }
static void o_35298(void) { O_35298(g_arg); }
static void c_35298(void) { ev2_35298(g_arg); }
static void o_35321(void) { O_35321(g_arg); }
static void c_35321(void) { ev2_35321(g_arg); }
static void o_353B5(void) { O_353B5(g_arg); }
static void c_353B5(void) { ev2_353B5(g_arg); }
static void o_353E7(void) { O_353E7(g_arg); }
static void c_353E7(void) { ev2_353E7(g_arg); }
static void o_353FA(void) { O_353FA(g_arg); }
static void c_353FA(void) { ev2_353FA(g_arg); }
static void o_3540F(void) { O_3540F(g_arg); }
static void c_3540F(void) { ev2_3540F(g_arg); }
static void o_35422(void) { O_35422(g_arg); }
static void c_35422(void) { ev2_35422(g_arg); }
static void o_3551C(void) { O_3551C(g_arg); }
static void c_3551C(void) { ev2_3551C(g_arg); }
static void o_3553F(void) { O_3553F(g_arg); }
static void c_3553F(void) { ev2_3553F(g_arg); }
static void o_355B7(void) { O_355B7(g_arg); }
static void c_355B7(void) { ev2_355B7(g_arg); }
static void o_35638(void) { O_35638(g_arg); }
static void c_35638(void) { ev2_35638(g_arg); }
static void o_35677(void) { O_35677(g_arg); }
static void c_35677(void) { ev2_35677(g_arg); }
static void o_35997(void) { O_35997(g_arg); }
static void c_35997(void) { ev2_35997(g_arg); }
static void o_359CB(void) { O_359CB(g_arg); }
static void c_359CB(void) { ev2_359CB(g_arg); }
static void o_35BEE(void) { O_35BEE(g_arg); }
static void c_35BEE(void) { ev2_35BEE(g_arg); }
static void o_35C1D(void) { O_35C1D(g_arg); }
static void c_35C1D(void) { ev2_35C1D(g_arg); }
static void o_35D85(void) { O_35D85(g_arg); }
static void c_35D85(void) { ev2_35D85(g_arg); }
static void o_35F79(void) { O_35F79(g_arg); }
static void c_35F79(void) { ev2_35F79(g_arg); }
static void o_36214(void) { O_36214(g_arg); }
static void c_36214(void) { ev2_36214(g_arg); }
static void o_36228(void) { O_36228(g_arg); }
static void c_36228(void) { ev2_36228(g_arg); }
static void o_362B0(void) { O_362B0(g_arg); }
static void c_362B0(void) { ev2_362B0(g_arg); }
static void o_362C5(void) { O_362C5(g_arg); }
static void c_362C5(void) { ev2_362C5(g_arg); }
static void o_363DE(void) { O_363DE(g_arg); }
static void c_363DE(void) { ev2_363DE(g_arg); }
static void o_36416(void) { O_36416(g_arg); }
static void c_36416(void) { ev2_36416(g_arg); }
static void o_3642E(void) { O_3642E(g_arg); }
static void c_3642E(void) { ev2_3642E(g_arg); }
static void o_36439(void) { O_36439(g_arg); }
static void c_36439(void) { ev2_36439(g_arg); }
static void o_36440(void) { O_36440(g_arg); }
static void c_36440(void) { ev2_36440(g_arg); }
static void o_36447(void) { O_36447(g_arg); }
static void c_36447(void) { ev2_36447(g_arg); }
static void o_3644E(void) { O_3644E(g_arg); }
static void c_3644E(void) { ev2_3644E(g_arg); }

struct entry { uint32_t addr; const char *name; pair_fn orig, ours; };
static const struct entry g_entries[] = {
    { 0x135DD, "135DD", o_135DD, c_135DD },
    { 0x35298, "35298", o_35298, c_35298 },
    { 0x35321, "35321", o_35321, c_35321 },
    { 0x353B5, "353B5", o_353B5, c_353B5 },
    { 0x353E7, "353E7", o_353E7, c_353E7 },
    { 0x353FA, "353FA", o_353FA, c_353FA },
    { 0x3540F, "3540F", o_3540F, c_3540F },
    { 0x35422, "35422", o_35422, c_35422 },
    { 0x3551C, "3551C", o_3551C, c_3551C },
    { 0x3553F, "3553F", o_3553F, c_3553F },
    { 0x355B7, "355B7", o_355B7, c_355B7 },
    { 0x35638, "35638", o_35638, c_35638 },
    { 0x35677, "35677", o_35677, c_35677 },
    { 0x35997, "35997", o_35997, c_35997 },
    { 0x359CB, "359CB", o_359CB, c_359CB },
    { 0x35BEE, "35BEE", o_35BEE, c_35BEE },
    { 0x35C1D, "35C1D", o_35C1D, c_35C1D },
    { 0x35D85, "35D85", o_35D85, c_35D85 },
    { 0x35F79, "35F79", o_35F79, c_35F79 },
    { 0x36214, "36214", o_36214, c_36214 },
    { 0x36228, "36228", o_36228, c_36228 },
    { 0x362B0, "362B0", o_362B0, c_362B0 },
    { 0x362C5, "362C5", o_362C5, c_362C5 },
    { 0x363DE, "363DE", o_363DE, c_363DE },
    { 0x36416, "36416", o_36416, c_36416 },
    { 0x3642E, "3642E", o_3642E, c_3642E },
    { 0x36439, "36439", o_36439, c_36439 },
    { 0x36440, "36440", o_36440, c_36440 },
    { 0x36447, "36447", o_36447, c_36447 },
    { 0x3644E, "3644E", o_3644E, c_3644E },
};
#define NENT (sizeof g_entries / sizeof g_entries[0])

static int selected[NENT];   /* 1 = run this entry */
static unsigned nsel;

static int parse_only(const char *s)
{
    unsigned i;

    while (*s) {
        char *end;
        unsigned long v = strtoul(s, &end, 16);
        int hit = 0;
        if (end == s) return -1;
        for (i = 0; i < NENT; i++)
            if (g_entries[i].addr == (uint32_t)v) {
                if (!selected[i]) { selected[i] = 1; nsel++; }
                hit = 1;
            }
        if (!hit) { printf("unknown target %s\n", s); return -1; }
        s = end;
        while (*s == ',' || *s == ' ') s++;
    }
    return 0;
}

int main(int argc, char **argv)
{
    le_image le;
    int      applied = 0;
    unsigned cases = 200, i, k;

    for (i = 1; i < (unsigned)argc; i++) {
        if (!strncmp(argv[i], "--only=", 7)) { if (parse_only(argv[i] + 7)) return 2; }
        else if (!strncmp(argv[i], "--cases=", 8)) cases = (unsigned)atoi(argv[i] + 8);
    }
    if (!nsel)
        for (i = 0; i < NENT; i++) selected[i] = 1;

    if (le_reserve_address_space() != 0) { printf("FAIL: reserve\n"); return 2; }
    if (le_open(&le, "E:\\FD2\\FD2.EXE") != 0) {
        printf("FAIL: cannot open FD2.EXE\n");
        return 1;
    }
    if (le_map_and_relocate(&le, &applied) != 0) {
        printf("FAIL: cannot map FD2.EXE\n");
        return 1;
    }
    printf("mapped FD2.EXE, fixups applied=%d\n", applied);

    /* record table / script state / status / stream are host buffers */
    HOOK(0x15F84, stub_vm);
    HOOK(0x344F2, stub_status);
    HOOK(0x112A5, stub_unit);
    HOOK(0x10B4E, stub_load);
    HOOK(0x17AA9, stub_wait);
    HOOK(0x35B78, stub_seq);
    HOOK(0x35F10, stub_clear);
    HOOK(0x11CAC, stub_view);
    HOOK(0x4E381, stub_flush);

    for (i = 0; i < NENT; i++) {
        if (!selected[i]) continue;
        for (k = 0; k < cases; k++) {
            seed = 0xE2C0DEu + 0x9E3779B9u * (uint32_t)(i * cases + k);
            run_pair((unsigned)(i * cases + k), g_entries[i].name,
                     g_entries[i].orig, g_entries[i].ours, seed);
            if (failures > 12) goto done;
        }
    }

done:
    if (failures) {
        printf("FAIL: %d failures in %u cases\n", failures, cases_run);
        return 1;
    }
    printf("PASS: %u cases, 0 failures\n", cases_run);
    return 0;
}
