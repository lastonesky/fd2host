/* typecheck.c - differential test for the typewriter step (original 0x164E8)
 * and the two services it ends with.
 *
 *   0x164E8  dlg_type_step    (src/game/dlg.c) - every second character it
 *                             blits a DATO sub-image through 0x16559
 *   0x25A96  svc_play_sfx     (src/game/svc.c) - five AIL calls
 *   0x17AA9  svc_wait_ticks   (src/game/svc.c) - BIOS tick busy-wait
 *
 * Neither service can be tested by just running it twice: svc_wait_ticks
 * spins on the BIOS tick word, and that word (BDA 0x46C) lives in the first
 * 64 KiB, which Windows will not map. Like src/keycheck.c this harness maps
 * the game over the low-memory window le.c reserves at 0x70000 and applies
 * dos.c's immediate-operand redirect, and then adds the piece keycheck did
 * not need - a clock:
 *
 *   1. the three tick readings inside the original 0x17AA9 are each
 *      `mov eax, 46Ch; movsx eax, word ptr [eax]`; all six bytes are replaced
 *      by `call tick_read`, and 0x4E310 - the helper the C reads the tick
 *      through - is hooked to the same stub. One stub, one clock, both sides.
 *   2. tick_read increments the tick by one on every reading, so the tick
 *      sequence is a function of how many times the code reads it: any extra
 *      or missing reading shows up as a different tick, a different
 *      dword_53A2C and a different loop count. The frame size and therefore
 *      the number of instructions is irrelevant, which is what makes an
 *      MSVC-compiled C comparable against the original at all.
 *      (tick_read is a naked wrapper around a plain C helper: the register it
 *      has to save is EDX, which the original's wait loop holds `n` in.)
 *   3. the five AIL entry points 0x25A96 calls are hooked too, because the
 *      harness has no sound driver: they log (name, handle, address offset,
 *      length, loop count) and return canned values.
 *
 * What is compared per case: the whole 320x200 VGA frame, the globals the
 * three functions own (dword_53A10 / 53A14 / 53A2C), the final tick word, the
 * return value and the complete event log - every tick reading and every AIL
 * call with its arguments, in order.
 *
 * Build: pwsh -File build.ps1 -Target typecheck
 * Run   : build\typecheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/dlg.h"
#include "game/svc.h"

typedef int (__cdecl *orig_wait_fn)(int);
typedef int (__cdecl *orig_sfx_fn)(const void *, int, int);
typedef int (__cdecl *orig_step_fn)(void);

#define ORIG_WAIT ((orig_wait_fn)(uintptr_t)0x00017AA9u)
#define ORIG_SFX  ((orig_sfx_fn) (uintptr_t)0x00025A96u)
#define ORIG_STEP ((orig_step_fn)(uintptr_t)0x000164E8u)

#define VGA       ((uint8_t *)(uintptr_t)0x000A0000u)
#define VGA_LEN   (320 * 200)
#define MIRROR    0x00070000u            /* low-mem window, already mapped */
#define G_TICK    (MIRROR + 0x46C)

#define G53A10    (*(int32_t *)(uintptr_t)0x00053A10u)
#define G53A14    (*(int32_t *)(uintptr_t)0x00053A14u)
#define G53A2C    (*(int32_t *)(uintptr_t)0x00053A2Cu)
#define G53A85    (*(void **)(uintptr_t)0x00053A85u)
#define G53C67    (*(int32_t *)(uintptr_t)0x00053C67u)
#define G53EE4    (*(void **)(uintptr_t)0x00053EE4u)
#define G53EEC    (*(void **)(uintptr_t)0x00053EECu)
#define G53EF1    (*(uint8_t  *)(uintptr_t)0x00053EF1u)
#define G51E62    (*(uint8_t  *)(uintptr_t)0x00051E62u)
#define G54133    (*(int32_t *)(uintptr_t)0x00054133u)

/* --- low-memory operand redirect (same algorithm as dos.c) -------------- */
static int patch_lowmem_refs(le_image *le, uint32_t mirror)
{
    unsigned i, off;
    int      n = 0;

    for (i = 0; i < le->object_count; i++) {
        uint8_t *base;
        uint32_t size;

        if (!(le->objects[i].flags & 0x04))
            continue;
        base = (uint8_t *)(uintptr_t)le->objects[i].base;
        size = le->objects[i].vsize;
        for (off = 0; off + 5 <= size; off++) {
            uint8_t  op  = base[off];
            uint32_t imm;
            int      is_mov = (op >= 0xB8 && op <= 0xBF);

            if (!is_mov && op != 0x68)
                continue;
            imm = (uint32_t)base[off + 1] | ((uint32_t)base[off + 2] << 8) |
                  ((uint32_t)base[off + 3] << 16) | ((uint32_t)base[off + 4] << 24);
            if (imm >= 0x400 && imm < 0x500) {
                uint32_t fixed = mirror + imm;
                base[off + 1] = (uint8_t)fixed;
                base[off + 2] = (uint8_t)(fixed >> 8);
                base[off + 3] = (uint8_t)(fixed >> 16);
                base[off + 4] = (uint8_t)(fixed >> 24);
                n++;
            }
        }
    }
    return n;
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

/* Replace an 8-byte instruction run with `call target; nop; nop; nop` - used
 * for the three tick readings inside the original 0x17AA9, each of which is
 * `mov eax, 46Ch` (5 bytes) followed by `movsx eax, word ptr [eax]` (3).
 * Nothing may branch into the middle of the run; 0x17AC4 is the only one that
 * is a jump target (the loop head) and it is replaced whole. */
static void install_call8(uint32_t addr, const void *dest)
{
    uint8_t *p   = (uint8_t *)(uintptr_t)addr;
    int32_t  rel = (int32_t)((const uint8_t *)dest - (p + 5));
    DWORD    old;

    VirtualProtect(p, 8, PAGE_EXECUTE_READWRITE, &old);
    p[0] = 0xE8;
    memcpy(p + 1, &rel, 4);
    p[5] = 0x90;
    p[6] = 0x90;
    p[7] = 0x90;
    VirtualProtect(p, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 8);
}

/* --- deterministic clock ------------------------------------------------ */
enum { EV_TICK, EV_STOP, EV_INIT, EV_ADDR, EV_LOOP, EV_START };
static const char *const kind_name[] = { "tick", "stop", "init", "address",
                                         "loops", "start" };
#define MAXEV 96

struct event { int kind; int a; int b; int c; };

static struct event g_ev[MAXEV];
static int          g_nev;
static int          g_read_count;       /* tick readings since begin_run    */
static uint16_t     g_tick;             /* value the mirror holds            */

static void ev_log(int kind, int a, int b, int c)
{
    if (g_nev >= MAXEV) { printf("FAIL: event log overflow\n"); exit(1); }
    g_ev[g_nev].kind = kind;
    g_ev[g_nev].a    = a;
    g_ev[g_nev].b    = b;
    g_ev[g_nev].c    = c;
    g_nev++;
}

/* the real work; called from the naked wrapper so EDX/ECX survive */
int32_t __cdecl tick_read_impl(void)
{
    uint16_t now;

    if (++g_read_count > 4096) {
        printf("FAIL: tick loop never ended (%d readings)\n", g_read_count);
        exit(1);
    }
    now = (uint16_t)(g_tick + 1);
    g_tick = now;
    *(uint16_t *)(uintptr_t)G_TICK = now;
    ev_log(EV_TICK, (int)(int16_t)now, 0, 0);
    return (int16_t)now;
}

static __declspec(naked) int32_t tick_read(void)
{
    __asm {
        push    ecx
        push    edx
        call    tick_read_impl
        pop     edx
        pop     ecx
        ret
    }
}

/* --- scripted AIL ------------------------------------------------------- */
typedef int32_t (__cdecl *ail_h_fn)(void *handle);
typedef int32_t (__cdecl *ail_addr_fn)(void *handle, const void *start,
                                       uint32_t len);
typedef int32_t (__cdecl *ail_loop_fn)(void *handle, int32_t loops);

static int32_t __cdecl stub_stop(void *h)
{
    ev_log(EV_STOP, (int)(uintptr_t)h, 0, 0);
    return 0x11;
}
static int32_t __cdecl stub_init(void *h)
{
    ev_log(EV_INIT, (int)(uintptr_t)h, 0, 0);
    return 0x22;
}
static int32_t __cdecl stub_addr(void *h, const void *start, uint32_t len)
{
    ev_log(EV_ADDR, (int)(uintptr_t)h,
           (int)((const uint8_t *)start - (const uint8_t *)G53EEC), (int)len);
    return 0x33;
}
static int32_t __cdecl stub_loop(void *h, int32_t loops)
{
    ev_log(EV_LOOP, (int)(uintptr_t)h, loops, 0);
    return 0x44;
}
static int32_t __cdecl stub_start(void *h)
{
    ev_log(EV_START, (int)(uintptr_t)h, 0, 0);
    return 0x55;
}

/* --- captures ----------------------------------------------------------- */
struct capture {
    uint8_t         vga[VGA_LEN];
    int32_t         a10, a14, a2c;
    uint32_t        tick;
    int32_t         ret;
    int             nev;
    struct event    ev[MAXEV];
};

static int          failures;
static unsigned     cases_run;
enum { P_WAIT, P_SFX, P_STEP };
static const char *const path_name[] = { "wait", "sfx", "step" };
static unsigned     path_count[3];

static void begin_run(uint16_t tick0)
{
    g_nev = 0;
    g_read_count = 0;
    g_tick = tick0;
    *(uint16_t *)(uintptr_t)G_TICK = tick0;
}

static void snapshot(struct capture *c)
{
    memcpy(c->vga, VGA, VGA_LEN);
    c->a10 = G53A10;
    c->a14 = G53A14;
    c->a2c = G53A2C;
    c->tick = *(const uint16_t *)(uintptr_t)G_TICK;
    c->ret  = 0;
    c->nev  = g_nev;
    memcpy(c->ev, g_ev, sizeof g_ev);
}

static int cmp_bytes(unsigned id, const char *what, const uint8_t *a,
                     const uint8_t *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        if (a[i] != b[i]) {
            printf("FAIL case %u: %s diff @+%u orig=%02X ours=%02X\n",
                   id, what, (unsigned)i, a[i], b[i]);
            failures++;
            return -1;
        }
    return 0;
}

static int cmp_capture(unsigned id, int path, const struct capture *o,
                       const struct capture *t, int cmp_ret)
{
    int bad = 0;
    int i;

    if (cmp_ret && o->ret != t->ret) {
        printf("FAIL case %u (%s): return orig=%d ours=%d\n",
               id, path_name[path], o->ret, t->ret);
        failures++; bad = 1;
    }
    if (o->a10 != t->a10 || o->a14 != t->a14 || o->a2c != t->a2c) {
        printf("FAIL case %u (%s): globals orig=(%d,%d,%d) ours=(%d,%d,%d)\n",
               id, path_name[path], o->a10, o->a14, o->a2c,
               t->a10, t->a14, t->a2c);
        failures++; bad = 1;
    }
    if (o->tick != t->tick) {
        printf("FAIL case %u (%s): tick orig=%u ours=%u\n",
               id, path_name[path], o->tick, t->tick);
        failures++; bad = 1;
    }
    if (o->nev != t->nev) {
        printf("FAIL case %u (%s): event count orig=%d ours=%d\n",
               id, path_name[path], o->nev, t->nev);
        for (i = 0; i < o->nev && i < 12; i++)
            printf("   orig[%d] %s(%d,%d,%d)\n", i, kind_name[o->ev[i].kind],
                   o->ev[i].a, o->ev[i].b, o->ev[i].c);
        for (i = 0; i < t->nev && i < 12; i++)
            printf("   ours[%d] %s(%d,%d,%d)\n", i, kind_name[t->ev[i].kind],
                   t->ev[i].a, t->ev[i].b, t->ev[i].c);
        failures++; bad = 1;
    } else {
        for (i = 0; i < o->nev; i++) {
            const struct event *eo = &o->ev[i], *et = &t->ev[i];
            if (eo->kind != et->kind || eo->a != et->a || eo->b != et->b ||
                eo->c != et->c) {
                printf("FAIL case %u (%s): event %d orig=%s(%d,%d,%d) "
                       "ours=%s(%d,%d,%d)\n", id, path_name[path], i,
                       kind_name[eo->kind], eo->a, eo->b, eo->c,
                       kind_name[et->kind], et->a, et->b, et->c);
                failures++; bad = 1;
                break;
            }
        }
    }
    if (!bad)
        bad = cmp_bytes(id, "VGA", o->vga, t->vga, VGA_LEN);

    cases_run++;
    path_count[path]++;
    return bad;
}

/* --- synthetic game state ---------------------------------------------- */
/* The typewriter's mouth counter only ever holds 0..3 in the game, but the
 * blit index is whatever is in dword_53A10 after `++` and the `== 4` wrap, so
 * starting it out of that domain (5 -> index 5) is reachable and both sides
 * have to do the same thing with it. There are enough slots to survive an
 * eight-step run from a bad starting value (each step pair adds one to the
 * index), because the alternative is that both sides read past the table. */
#define DATO_SLOTS 16                   /* dword offset table at +0         */
#define BANK_SLOTS 8                    /* [6-byte header][N+1 dwords][data]*/

static uint32_t seed = 0x9E3779B9u;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}
static void fill_rand(uint8_t *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        p[i] = (uint8_t)rnd();
}

static uint8_t *g_dato;                 /* DATO sub-images                  */
static uint8_t *g_bank;                 /* SFX container, see svc.h         */

static void build_dato(void)
{
    uint32_t off[DATO_SLOTS];
    int      i;
    size_t   pos = 4 * DATO_SLOTS;

    fill_rand(g_dato, 32 * 1024);
    for (i = 0; i < DATO_SLOTS; i++) {
        unsigned w = 1 + (unsigned)rnd() % 40;
        unsigned h = 1 + (unsigned)rnd() % 30;
        unsigned total = w * h, done = 0;

        off[i] = (uint32_t)pos;
        *(uint16_t *)(g_dato + pos)     = (uint16_t)w;
        *(uint16_t *)(g_dato + pos + 2) = (uint16_t)h;
        pos += 4;
        while (done < total) {
            unsigned left = total - done;
            if ((rnd() & 1) || left == 1) {
                g_dato[pos++] = (uint8_t)(rnd() % 0xC1);
                done++;
            } else {
                unsigned maxlen = left < 63 ? left : 63;
                unsigned len = 1 + (unsigned)rnd() % maxlen;
                g_dato[pos++] = (uint8_t)(0xC0 + len);
                g_dato[pos++] = (uint8_t)rnd();
                done += len;
            }
        }
    }
    for (i = 0; i < DATO_SLOTS; i++)
        ((uint32_t *)g_dato)[i] = off[i];
}

/* [6-byte header][(BANK_SLOTS+1) dword offsets][sample bytes]: sample i is
 * [off[i], off[i+1]) - exactly what svc_play_sfx reads at row+6 / row+0xA. */
static void build_bank(void)
{
    uint32_t pos = 6 + 4 * (BANK_SLOTS + 1);
    int      i;

    fill_rand(g_bank, 8 * 1024);
    for (i = 0; i <= BANK_SLOTS; i++) {
        *(uint32_t *)(g_bank + 6 + 4 * i) = pos;
        if (i < BANK_SLOTS) {
            unsigned len = 1 + (unsigned)rnd() % 400;
            fill_rand(g_bank + pos, len);
            pos += len;
        }
    }
}

static void reset_world(uint32_t vga_seed, int a10, int a14, int box)
{
    seed = vga_seed;
    fill_rand(VGA, VGA_LEN);
    G53A85 = g_dato;
    G53EEC = g_bank;
    G53EE4 = (void *)(uintptr_t)0x40001000u;   /* arbitrary sample handle  */
    G53C67 = box;
    G53A10 = a10;
    G53A14 = a14;
    G53A2C = 0;
}

/* --- case 1: svc_wait_ticks -------------------------------------------- */
static void test_wait(unsigned id, int n, uint16_t t0)
{
    static struct capture co, ct;
    int r;

    begin_run(t0);
    r = ORIG_WAIT(n);
    snapshot(&co);
    co.ret = r;

    begin_run(t0);
    r = svc_wait_ticks(n);
    snapshot(&ct);
    ct.ret = r;

    cmp_capture(id, P_WAIT, &co, &ct, 1);
}

/* --- case 2: svc_play_sfx ---------------------------------------------- */
static void test_sfx(unsigned id, int driver, int enabled, int busy,
                     int index, int loops, uint16_t t0)
{
    static struct capture co, ct;
    int r, played;

    G53EF1 = (uint8_t)driver;
    G51E62 = (uint8_t)enabled;
    G54133 = busy;

    begin_run(t0);
    r = ORIG_SFX(g_bank, index, loops);
    snapshot(&co);
    co.ret = r;

    begin_run(t0);
    r = svc_play_sfx(g_bank, index, loops);
    snapshot(&ct);
    ct.ret = r;

    /* when the gates block playback the original returns whatever was in EAX,
     * so only the played path has a defined return value to compare */
    played = (driver != 0 && enabled != 0 && busy == 0);
    cmp_capture(id, P_SFX, &co, &ct, played);
}

/* --- case 3: dlg_type_step, run as a sequence -------------------------- */
#define MAXSTEP 8

static void test_step(unsigned id, int steps, int a10, int a14, int box,
                      int driver, int enabled, int busy, uint32_t vga_seed,
                      uint16_t tick_base)
{
    struct capture *co = (struct capture *)calloc(MAXSTEP, sizeof *co);
    struct capture *ct = (struct capture *)calloc(MAXSTEP, sizeof *ct);
    int i;

    if (!co || !ct) { printf("FAIL: out of memory\n"); exit(1); }

    G53EF1 = (uint8_t)driver;
    G51E62 = (uint8_t)enabled;
    G54133 = busy;

    reset_world(vga_seed, a10, a14, box);
    for (i = 0; i < steps; i++) {
        int r;
        begin_run((uint16_t)(tick_base + i));
        r = ORIG_STEP();
        snapshot(&co[i]);
        co[i].ret = r;
    }

    reset_world(vga_seed, a10, a14, box);
    for (i = 0; i < steps; i++) {
        int r;
        begin_run((uint16_t)(tick_base + i));
        r = dlg_type_step();
        snapshot(&ct[i]);
        ct[i].ret = r;
    }

    /* the return value of each step is svc_wait_ticks's last tick reading,
     * which is defined on both sides every time - the clock only moves when
     * somebody reads it */
    for (i = 0; i < steps; i++)
        if (cmp_capture(id * 16 + i, P_STEP, &co[i], &ct[i], 1))
            break;

    free(co);
    free(ct);
}

int main(int argc, char **argv)
{
    static const int   boxes[]  = { 0x728, 0x9017, 0x3210 };
    le_image le;
    int      applied = 0;
    int      nref;
    unsigned i;

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    nref = patch_lowmem_refs(&le, MIRROR);
    printf("typecheck: redirected %d low-memory references to 0x%X\n",
           nref, MIRROR);
    if (nref < 40) { printf("FAIL: BDA redirect looks incomplete\n"); return 2; }

    g_dato = (uint8_t *)malloc(32 * 1024);
    g_bank = (uint8_t *)malloc(8 * 1024);
    if (!g_dato || !g_bank) return 2;
    /* 8 sub-images of up to 40x30 easily exceed 8 KiB */
    if ((unsigned)_msize(g_dato) < 32u * 1024u) { printf("FAIL: buffer too small\n"); return 2; }

    /* the clock: the three readings inside the original, plus the helper the
     * translated code reads the tick through (0x4E310) */
    install_call8(0x17AB7u, tick_read);
    install_call8(0x17AC4u, tick_read);
    install_call8(0x17ADFu, tick_read);
    HOOK(0x4E310u, tick_read);

    HOOK(0x39805u, stub_stop);          /* AIL_stop_sample                   */
    HOOK(0x39521u, stub_init);          /* AIL_init_sample                   */
    HOOK(0x39694u, stub_addr);          /* AIL_set_sample_address            */
    HOOK(0x39AAEu, stub_loop);          /* AIL_set_sample_loop_count         */
    HOOK(0x39798u, stub_start);         /* AIL_start_sample                  */

    /* ---- svc_wait_ticks ------------------------------------------------ */
    /* The +0x10000 correction only ever fires where the *signed* reading
     * wraps - readings below 0x8000 are positive and above it negative, so
     * the tick has to cross 0x7FFF/0x8000 inside one run (each run reads the
     * clock about ten times, hence the narrow start window). Crossing
     * 0xFFFF/0x0000 instead is the uninterrupted case: both readings are
     * negative and the difference stays small and positive. */
    for (i = 0; i < 90 && !failures; i++) {
        static const int counts[] = { 0, 1, 2, 3, 5, 8 };
        static const int windows[] = { 0x0000, 0x7FF8, 0xFFF0, 0x7FF8 };
        unsigned         pick = rnd() % 4;
        int n  = counts[rnd() % 6];
        int t0 = windows[pick] + (int)(rnd() & 0x1F);

        test_wait(1000 + i, n, (uint16_t)t0);
    }

    /* ---- svc_play_sfx -------------------------------------------------- */
    for (i = 0; i < 400 && !failures; i++) {
        int driver  = (int)(rnd() % 2);
        int enabled = (int)(rnd() % 2);
        int busy    = (int)(rnd() % 2);
        int index   = (int)(rnd() % (BANK_SLOTS + 1)) - 1;   /* -1 .. 7      */
        int loops   = (int)(rnd() % 4);

        seed = 0x5EED0000u + i;
        build_bank();
        test_sfx(2000 + i, driver, enabled, busy, index, loops, (uint16_t)rnd());
    }
    /* constructed: every gate combination at a fixed bank */
    {
        static const int idx[] = { -1, 0, 1, 2, 7 };
        int d, e, b, k;
        seed = 0xBEEFu;
        build_bank();
        for (d = 0; d < 2 && !failures; d++)
            for (e = 0; e < 2 && !failures; e++)
                for (b = 0; b < 2 && !failures; b++)
                    for (k = 0; k < 5 && !failures; k++)
                        test_sfx(3000 + d * 40 + e * 20 + b * 10 + k,
                                 d, e, b, idx[k], 1, 0x1234);
    }

    /* ---- dlg_type_step -------------------------------------------------- */
    for (i = 0; i < 120 && !failures; i++) {
        int steps = 1 + (int)(rnd() % MAXSTEP);
        int a10   = (int)(rnd() % 5);
        int a14   = (int)(rnd() % 3);
        int box   = boxes[rnd() % 3];
        int drv   = (int)(rnd() % 2);
        int ena   = (int)(rnd() % 2);
        int busy  = (int)(rnd() % 2);

        seed = 0xD10D0000u + i;         /* DATO sub-images                  */
        build_dato();
        seed = 0xB0B00000u + i;         /* SFX bank                         */
        build_bank();
        test_step(4000 + i, steps, a10, a14, box, drv, ena, busy,
                  0xFACE0000u + i, (uint16_t)((rnd() & 1) ? 0x7FFD : 0x1000));
    }
    /* constructed: the phase counter has to walk 0,1,2,3 and wrap, and the
     * blit may happen on the first step of a sequence or in the middle */
    {
        int a10, a14;
        seed = 0xA11Cu;
        build_dato();
        seed = 0xB11Du;
        build_bank();
        for (a14 = 0; a14 < 3 && !failures; a14++)
            for (a10 = 0; a10 < 5 && !failures; a10++)
                test_step(5000 + a14 * 10 + a10, 6, a10, a14, 0x728,
                          1, 1, 0, 0xC0DE0000u + a14 * 10 + a10, (uint16_t)0x7FFD);
        /* bottom box (mirrored blit) and a start value outside 0..3 */
        test_step(5099, 8, 0, 1, 0x9017, 1, 1, 0, 0xDEADBEEFu, (uint16_t)0x7FFD);
        test_step(5100, 8, 4, 0, 0x728, 1, 1, 0, 0xDEADBEEFu, (uint16_t)0x1000);
        test_step(5101, 4, 0, 0, 0x3210, 0, 0, 1, 0xDEADBEEFu, (uint16_t)0x7FFF);
    }

    printf("paths: wait=%u sfx=%u step=%u\n",
           path_count[P_WAIT], path_count[P_SFX], path_count[P_STEP]);
    printf("%s: %u cases, %d failures\n",
           failures ? "FAILED" : "PASS", cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
