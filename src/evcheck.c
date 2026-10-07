/* evcheck.c - differential test for the funcs_1199C event handlers.
 *
 *   0x34738 / 0x348EA / 0x34A6C / 0x34B2F / 0x34CF1 / 0x34D92 / 0x34F74 /
 *   0x35123 / 0x35191 / 0x351E6 / 0x35258        (src/game/ev.c)
 *
 * Each case runs the original machine code, restores the world identically,
 * then runs the C translation and compares the whole observable world:
 *
 *   - the return value (only for the seven functions whose EAX is a game
 *     value - the four `void` ones return a service's result or the caller's
 *     own EAX, which every call site discards);
 *   - the complete event log: every service call, in order, with its scalar
 *     arguments (vm_run's 9, rec_status_set's 3, the record accessors);
 *   - the 256-record table, byte for byte (the direct writes of 0x34738 /
 *     0x34F74 / 0x351E6);
 *   - the dword_53AD5 block (byte +16 is the one-shot flag).
 *
 * The services are hooked because the C calls them at their original
 * addresses: without the hooks the C would reach the real record writers (and
 * would modify the buffer under a different base). The hooks make both sides
 * see one controlled service set, so the recorded arguments are the thing
 * under test.
 *
 * Build: pwsh -File build.ps1 -Target evcheck
 * Run   : build\evcheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/ev.h"

/* --- original entry points (the dispatcher pushes one argument; the caller
 * cleans it, and only 0x34CF1 / 0x35123 read it) ------------------------- */
typedef void     (*ov_fn)(int);
typedef uint32_t (*ou_fn)(int);

#define O_34738 ((ov_fn)(uintptr_t)0x00034738u)
#define O_348EA ((ou_fn)(uintptr_t)0x000348EAu)
#define O_34A6C ((ov_fn)(uintptr_t)0x00034A6Cu)
#define O_34B2F ((ov_fn)(uintptr_t)0x00034B2Fu)
#define O_34CF1 ((ou_fn)(uintptr_t)0x00034CF1u)
#define O_34D92 ((ou_fn)(uintptr_t)0x00034D92u)
#define O_34F74 ((ou_fn)(uintptr_t)0x00034F74u)
#define O_35123 ((ou_fn)(uintptr_t)0x00035123u)
#define O_35191 ((ou_fn)(uintptr_t)0x00035191u)
#define O_351E6 ((ou_fn)(uintptr_t)0x000351E6u)
#define O_35258 ((ou_fn)(uintptr_t)0x00035258u)

#define W32(x) (*(uint32_t *)(uintptr_t)(x))

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

/* --- synthetic game world --------------------------------------------- */
#define REC_COUNT 256
#define REC_BYTES (REC_COUNT * 80)
#define VGA_BASE  0x000A0000u

static uint8_t rec_in[REC_BYTES];      /* same content for both sides      */
static uint8_t rec_o[REC_BYTES];       /* the original's record table      */
static uint8_t rec_c[REC_BYTES];       /* the translation's record table   */
static uint8_t ad5_tmpl[64];           /* dword_53AD5 pre-image            */
static uint8_t ad5[64];                /* the live dword_53AD5 block       */
static uint8_t stream[64];             /* dword_53A79 (vm_run stream)      */
static int     flagret[256];           /* controlled rec_flag results      */

static uint32_t seed = 0x0E7C0DEu;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}
static void fill_rand(uint8_t *p, size_t n)
{
    while (n--)
        *p++ = (uint8_t)rnd();
}

/* --- event log --------------------------------------------------------- */
enum { EV_VM, EV_STATUS, EV_FLAG, EV_SLOTFREE, EV_SLOTCLAIM, EV_MASK };
static const char *const kind_name[] = { "vm_run", "status_set", "rec_flag",
                                         "slot_free", "slot_claim", "mask" };
#define MAXEV 64
struct event { int kind; int v[9]; };

static struct event g_ev[MAXEV];
static int          g_nev;
static int          g_slotfree;
static int          g_claim_ret = 1;

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
                           int shadow, int bgfill, int line_step, int wait)
{
    ev_log(EV_VM, (int)((const uint8_t *)st - stream), sub, addr, pitch, fg,
           shadow, bgfill, line_step, wait);
    return (int)VGA_BASE;
}
static uint32_t __cdecl stub_status(int start, int end, int value)
{
    ev_log(EV_STATUS, start, end, value, 0, 0, 0, 0, 0, 0);
    /* 0x344F2 returns dword_53A45 + 80*last (its loop variable in EAX). */
    if (start > end)
        return 0;
    return W32(0x53A45) + 80u * (uint32_t)end;
}
static int __cdecl stub_flag(int index)
{
    ev_log(EV_FLAG, index, 0, 0, 0, 0, 0, 0, 0, 0);
    return flagret[index & 0xFF];
}
static int __cdecl stub_slot_free(int index)
{
    ev_log(EV_SLOTFREE, index, 0, 0, 0, 0, 0, 0, 0, 0);
    return g_slotfree;
}
static int __cdecl stub_slot_claim(int index, int value)
{
    ev_log(EV_SLOTCLAIM, index, value, 0, 0, 0, 0, 0, 0, 0);
    return g_claim_ret;
}
static uint32_t __cdecl stub_mask(void)
{
    ev_log(EV_MASK, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    /* 0x34D64 returns the record table base. */
    return W32(0x53A45);
}

/* --- capture ----------------------------------------------------------- */
struct cap {
    uint32_t ret;
    uintptr_t base;
    uint8_t  rec[REC_BYTES];
    uint8_t  ad5[64];
    int      nev;
    struct event ev[MAXEV];
};

static struct cap *g_co, *g_ct;

static void setup_side(uint8_t *rec)
{
    memcpy(rec, rec_in, REC_BYTES);
    memcpy(ad5, ad5_tmpl, 64);
    W32(0x53A45) = (uint32_t)(uintptr_t)rec;
    W32(0x53AD5) = (uint32_t)(uintptr_t)ad5;
    W32(0x53A79) = (uint32_t)(uintptr_t)stream;
    g_nev = 0;
}
static void capture(struct cap *c, uint8_t *rec, uint32_t ret)
{
    c->ret  = ret;
    c->base = (uintptr_t)rec;
    memcpy(c->rec, rec, REC_BYTES);
    memcpy(c->ad5, ad5, 64);
    c->nev  = g_nev;
    memcpy(c->ev, g_ev, sizeof g_ev);
}

/* Records [start,end] byte +52 (rec_status_set) and the 64..73/+53 window are
 * done by the real services in the host; here the stubs intentionally do not
 * write, so the only record bytes that can change are the handlers' direct
 * writes. */
static int failures;
static unsigned cases_run;

static uint32_t norm_ret(uint32_t v, uintptr_t base)
{
    if (v >= base && v < base + REC_BYTES)
        return (uint32_t)(v - base);
    return v;
}

static int cmp_cap(unsigned id, const char *name, int use_ret,
                   const struct cap *o, const struct cap *t)
{
    int bad = 0, i, j;

    if (use_ret) {
        uint32_t r1 = norm_ret(o->ret, o->base);
        uint32_t r2 = norm_ret(t->ret, t->base);
        if (r1 != r2) {
            printf("FAIL case %u %s: return orig=%X ours=%X\n",
                   id, name, o->ret, t->ret);
            failures++; bad = 1;
        }
    }
    if (o->nev != t->nev) {
        printf("FAIL case %u %s: event count orig=%d ours=%d\n",
               id, name, o->nev, t->nev);
        failures++; bad = 1;
    }
    for (i = 0; i < o->nev && i < t->nev; i++) {
        int diff = o->ev[i].kind != t->ev[i].kind;
        for (j = 0; j < 9; j++)
            if (o->ev[i].v[j] != t->ev[i].v[j])
                diff = 1;
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
    for (i = 0; i < REC_BYTES && !bad; i++)
        if (o->rec[i] != t->rec[i]) {
            printf("FAIL case %u %s: record byte %d orig=%02X ours=%02X\n",
                   id, name, i, o->rec[i], t->rec[i]);
            failures++; bad = 1;
        }
    for (i = 0; i < 64 && !bad; i++)
        if (o->ad5[i] != t->ad5[i]) {
            printf("FAIL case %u %s: dword_53AD5+%d orig=%02X ours=%02X\n",
                   id, name, i, o->ad5[i], t->ad5[i]);
            failures++; bad = 1;
        }
    if (!bad && (getenv("EVCHECK_VERBOSE") || 0))
        printf("ok case %u %s\n", id, name);
    return bad;
}

typedef uint32_t (*call_fn)(void);

static void run_pair(unsigned id, const char *name, int use_ret,
                     call_fn orig, call_fn ours)
{
    uint32_t r;

    setup_side(rec_o);
    r = orig();
    capture(g_co, rec_o, r);

    setup_side(rec_c);
    r = ours();
    capture(g_ct, rec_c, r);

    cases_run++;
    cmp_cap(id, name, use_ret, g_co, g_ct);
}

/* --- test wrappers (one per handler) ----------------------------------- */
static int g_arg;

static uint32_t o_rec13(void)   { O_34738(0);             return 0; }
static uint32_t c_rec13(void)   { ev_rec13_set1();        return 0; }
static uint32_t o_st24(void)    { return O_348EA(0); }
static uint32_t c_st24(void)    { return ev_status24_27(); }
static uint32_t o_st736(void)   { O_34A6C(0);             return 0; }
static uint32_t c_st736(void)   { ev_status7_36();        return 0; }
static uint32_t o_f8(void)      { O_34B2F(0);             return 0; }
static uint32_t c_f8(void)      { ev_flag8_gate();        return 0; }
static uint32_t o_r6(void)      { return O_34CF1(g_arg); }
static uint32_t c_r6(void)      { return ev_rec6_gate(g_arg); }
static uint32_t o_mr(void)      { return O_34D92(0); }
static uint32_t c_mr(void)      { return ev_mask_records(); }
static uint32_t o_cs(void)      { return O_34F74(0); }
static uint32_t c_cs(void)      { return ev_clear_status12_13(); }
static uint32_t o_s8(void)      { O_35123(g_arg);         return 0; }
static uint32_t c_s8(void)      { ev_slot8_claim(g_arg);  return 0; }
static uint32_t o_st1671(void)  { return O_35191(0); }
static uint32_t c_st1671(void)  { return ev_status16_71(); }
static uint32_t o_c6473(void)   { return O_351E6(0); }
static uint32_t c_c6473(void)   { return ev_clear64_73(); }
static uint32_t o_st1634(void)  { return O_35258(0); }
static uint32_t c_st1634(void)  { return ev_status16_34(); }

/* --- world preparation ------------------------------------------------- */
/* flag_mode: -1 random 0/1, 0 all zero, 1 all one, n >= 100 single zero at
 * index n-100 (the rest one). */
static void prep(uint32_t s, int ad5_16, int slotfree, int flag_mode)
{
    int i;

    seed = s;
    fill_rand(rec_in, REC_BYTES);
    fill_rand(ad5_tmpl, 64);
    fill_rand(stream, 64);
    ad5_tmpl[16] = (uint8_t)ad5_16;
    ad5_tmpl[21] = (uint8_t)(s >> 5);
    for (i = 0; i < 256; i++) {
        if (flag_mode < 0)
            flagret[i] = (int)(rnd() & 1);
        else if (flag_mode <= 1)
            flagret[i] = flag_mode;
        else
            flagret[i] = (i == flag_mode - 100) ? 0 : 1;
    }
    g_slotfree = slotfree;
    g_arg = 0;
}
static void set_rec6(int idx, uint8_t v)
{
    rec_in[(uint32_t)idx * 80u + 6u] = v;
}
static void set_rec_byte(int idx, int off, uint8_t v)
{
    rec_in[(uint32_t)idx * 80u + (uint32_t)off] = v;
}

int main(int argc, char **argv)
{
    static const int st24_ad5[]  = { 0, 1, 0xFF };
    static const int id6[]       = { 0, 12, 13, 63, 64, 73, 219, 255 };
    static const int rec6_val[]  = { 0, 1, 0x80, 0xFF };
    static const int slot_args[] = { 0, 1, 0xFF };
    le_image le;
    int      applied = 0;
    unsigned id = 0;
    unsigned i, k;
    int      s;

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    g_co = (struct cap *)malloc(sizeof *g_co);
    g_ct = (struct cap *)malloc(sizeof *g_ct);
    if (!g_co || !g_ct) return 2;

    HOOK(0x15F84u, stub_vm);
    HOOK(0x344F2u, stub_status);
    HOOK(0x34894u, stub_flag);
    HOOK(0x1B8A6u, stub_slot_free);
    HOOK(0x1BB8Cu, stub_slot_claim);
    HOOK(0x34D64u, stub_mask);

    /* 1. the nine no-argument handlers, over the flag / slot / one-shot space */
    for (s = 0; s < 3; s++) {
        for (k = 0; k < 3; k++) {
            int slot = (k == 0) ? 0 : (k == 1) ? 4 : 8;
            int rep;
            for (rep = 0; rep < 20; rep++) {
                unsigned base = id;
                prep(0x1000u + id * 7u, st24_ad5[s], slot, -1);
                id = base + 0;
                run_pair(id + 1, "rec13",   0, o_rec13,  c_rec13);
                run_pair(id + 2, "st24",    1, o_st24,   c_st24);
                run_pair(id + 3, "st736",   0, o_st736,  c_st736);
                run_pair(id + 4, "f8",      0, o_f8,     c_f8);
                run_pair(id + 5, "mr",      1, o_mr,     c_mr);
                run_pair(id + 6, "cs",      1, o_cs,     c_cs);
                run_pair(id + 7, "st1671",  1, o_st1671, c_st1671);
                run_pair(id + 8, "c6473",   1, o_c6473,  c_c6473);
                run_pair(id + 9, "st1634",  1, o_st1634, c_st1634);
                id += 9;
            }
        }
    }

    /* 2. 0x34CF1: index x record byte +6 */
    for (i = 0; i < sizeof id6 / sizeof id6[0]; i++) {
        for (k = 0; k < sizeof rec6_val / sizeof rec6_val[0]; k++) {
            int rep;
            for (rep = 0; rep < 20; rep++) {
                prep(0x2000u + id * 7u, 0, 0, -1);
                set_rec6(id6[i], (uint8_t)rec6_val[k]);
                g_arg = id6[i];
                run_pair(id + 1, "rec6", 1, o_r6, c_r6);
                id++;
            }
        }
    }

    /* 3. 0x35123: argument x free-slot count x one-shot flag */
    for (i = 0; i < sizeof slot_args / sizeof slot_args[0]; i++) {
        for (k = 0; k <= 8; k++) {
            for (s = 0; s < 2; s++) {
                int rep;
                for (rep = 0; rep < 10; rep++) {
                    prep(0x3000u + id * 7u, s, (int)k, -1);
                    g_arg = slot_args[i];
                    run_pair(id + 1, "slot8", 0, o_s8, c_s8);
                    id++;
                }
            }
        }
    }

    /* 4. 0x34A6C flag profiles: all-1 (no second sub-stream), all-0, and one
     * zero at each end of the 7..36 window. */
    {
        static const int prof[] = { 1, 0, 107, 136 };
        for (i = 0; i < sizeof prof / sizeof prof[0]; i++) {
            int rep;
            for (rep = 0; rep < 20; rep++) {
                prep(0x4000u + id * 7u, 0, 0, prof[i]);
                run_pair(id + 1, "st736-profile", 0, o_st736, c_st736);
                id++;
            }
        }
    }

    /* 5. 0x351E6 window boundaries: bytes just outside 64..73 at offset +53,
     * plus its two status calls over random tables. */
    for (i = 0; i < 40; i++) {
        prep(0x5000u + id * 7u, i & 1, (int)(i % 9), -1);
        set_rec_byte(63, 53, 0xA5);
        set_rec_byte(74, 53, 0x5A);
        set_rec_byte(64, 53, 0xFF);
        set_rec_byte(73, 53, 0xFF);
        run_pair(id + 1, "c6473-window", 1, o_c6473, c_c6473);
        id++;
    }

    /* 6. 0x34F74 record 12/13 byte +52, plus neighbours left alone. */
    for (i = 0; i < 20; i++) {
        prep(0x6000u + id * 7u, (int)(i & 1), (int)(i % 9), -1);
        set_rec_byte(11, 52, 0xA5);
        set_rec_byte(12, 52, 0xFF);
        set_rec_byte(13, 52, 0xFF);
        set_rec_byte(14, 52, 0x5A);
        run_pair(id + 1, "clear-status12-13", 1, o_cs, c_cs);
        id++;
    }

    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
