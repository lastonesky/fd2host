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
#include "game/ev3.h"
#include "game/ev4.h"

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
/* --- batch 3 (src/game/ev3.c) ----------------------------------------- */
typedef void (*h3_fn)(int, int, int);
typedef void (*h0_fn)(void);
typedef int  (*h2i_fn)(int, int);
#define O_352CA ((h1_fn)(uintptr_t)0x000352CAu)
#define O_35346 ((h1_fn)(uintptr_t)0x00035346u)
#define O_35468 ((h1_fn)(uintptr_t)0x00035468u)
#define O_355F0 ((h1_fn)(uintptr_t)0x000355F0u)
#define O_356B3 ((h1_fn)(uintptr_t)0x000356B3u)
#define O_35730 ((h1_fn)(uintptr_t)0x00035730u)
#define O_357DD ((h1_fn)(uintptr_t)0x000357DDu)
#define O_35833 ((h1_fn)(uintptr_t)0x00035833u)
#define O_35854 ((h1_fn)(uintptr_t)0x00035854u)
#define O_35A0D ((h1_fn)(uintptr_t)0x00035A0Du)
#define O_35C40 ((h1_fn)(uintptr_t)0x00035C40u)
#define O_35CF1 ((h1_fn)(uintptr_t)0x00035CF1u)
#define O_35D1E ((h1_fn)(uintptr_t)0x00035D1Eu)
#define O_35D9E ((h1_fn)(uintptr_t)0x00035D9Eu)
#define O_35E0E ((h1_fn)(uintptr_t)0x00035E0Eu)
#define O_35E5B ((h1_fn)(uintptr_t)0x00035E5Bu)
#define O_35EC1 ((h1_fn)(uintptr_t)0x00035EC1u)
#define O_35F48 ((h1_fn)(uintptr_t)0x00035F48u)
#define O_35F88 ((h1_fn)(uintptr_t)0x00035F88u)
#define O_35FCF ((h1_fn)(uintptr_t)0x00035FCFu)
#define O_360B6 ((h1_fn)(uintptr_t)0x000360B6u)
#define O_3623C ((h1_fn)(uintptr_t)0x0003623Cu)
#define O_362E8 ((h1_fn)(uintptr_t)0x000362E8u)
#define O_35B78 ((h3_fn)(uintptr_t)0x00035B78u)
#define O_35F10 ((h1_fn)(uintptr_t)0x00035F10u)
#define O_361B0 ((h0_fn)(uintptr_t)0x000361B0u)
#define O_2AEDB ((h2i_fn)(uintptr_t)0x0002AEDBu)
#define O_33F78 ((h3_fn)(uintptr_t)0x00033F78u)

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
#define SNAP2_BASE 0x00060000u   /* obj2: util_rand's word_627B8 lives here */
#define SNAP2_N    0x000034D2u
#define VGA_BASE   0x000A0000u
#define VGA_N      0x00010000u
#define STATE_N   64
#define STATUS_N  64
#define STREAM_N  16

static uint8_t rec_in[REC_BYTES];
static uint8_t rec_o[REC_BYTES], rec_c[REC_BYTES];
static uint8_t state_in[STATE_N], state_o[STATE_N], state_c[STATE_N];
static uint8_t stat_in[STATUS_N], stat_o[STATUS_N], stat_c[STATUS_N];
static uint8_t strm_in[STREAM_N], strm_o[STREAM_N], strm_c[STREAM_N];
static uint8_t obj1_o[SNAP_N], obj1_c[SNAP_N];

static uint8_t obj1_p[SNAP_N];   /* post-relocation pristine data segment */
static uint8_t obj2_p[SNAP2_N];

static uint32_t seed;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}
static void fill_rand(uint8_t *p, size_t n) { while (n--) *p++ = (uint8_t)rnd(); }

/* --- event log --------------------------------------------------------- */
enum { EV_VM, EV_STATUS, EV_UNIT, EV_LOAD, EV_WAIT, EV_SEQ, EV_CLEAR,
       EV_VIEW, EV_FLUSH, EV_SCENE, EV_2E2B0, EV_1DB65, EV_12263,
       EV_MSGOPEN, EV_MSGCLOSE, EV_DLGBLIT, EV_DLGWAIT, EV_MAPCELL,
       EV_RESLOAD, EV_RESBLIT, EV_FREE, EV_DELAY, EV_PALADD, EV_1366A,
       EV_134E4, EV_12CEA, EV_22253, EV_EXT, EV_N };
static const char *const kind_name[EV_N] = {
    "vm_run", "status", "unit_add", "load", "wait", "seq", "clear",
    "view", "flush", "scene", "2E2B0", "1DB65", "12263",
    "msg_open", "msg_close", "dlg_blit", "dlg_wait", "map_cell",
    "res_load", "res_blit", "free", "delay", "pal_add", "1366A",
    "134E4", "12CEA", "22253", "ext"
};
#define MAXEV 8192
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
/* --- batch-3 services (src/game/ev3.c) --------------------------------- */
static int __cdecl stub_scene(int rec, int count, const uint8_t *scr)
{
    ev_log(EV_SCENE, rec, count, scr ? scr[0] : -1, scr ? scr[1] : -1,
           scr ? scr[2] : -1, 0, 0, 0, 0);
    return 0;
}
static void __cdecl stub_2e2b0(int a, int b)
{
    ev_log(EV_2E2B0, a, b, 0, 0, 0, 0, 0, 0, 0);
}
static void __cdecl stub_1db65(void) { ev_log(EV_1DB65, 0, 0, 0, 0, 0, 0, 0, 0, 0); }
static void __cdecl stub_12263(void) { ev_log(EV_12263, 0, 0, 0, 0, 0, 0, 0, 0, 0); }
static void __cdecl stub_msopen(int id)
{
    ev_log(EV_MSGOPEN, id, 0, 0, 0, 0, 0, 0, 0, 0);
}
static void __cdecl stub_msclose(void) { ev_log(EV_MSGCLOSE, 0, 0, 0, 0, 0, 0, 0, 0, 0); }
static void __cdecl stub_dlgblit(int n)
{
    ev_log(EV_DLGBLIT, n, 0, 0, 0, 0, 0, 0, 0, 0);
}
static void __cdecl stub_dlgwait(int speaker)
{
    ev_log(EV_DLGWAIT, speaker, 0, 0, 0, 0, 0, 0, 0, 0);
}
static void __cdecl stub_mapcell(int x, int y, uint8_t *out)
{
    ev_log(EV_MAPCELL, x, y, 0, 0, 0, 0, 0, 0, 0);
    if (out) {
        out[0] = (uint8_t)x;
        out[1] = (uint8_t)y;
        out[2] = (uint8_t)(((unsigned)(x ^ y)) % 5u);
        out[3] = 0;
    }
}
static uint8_t g_resbuf[0x10000];
static void *__cdecl stub_resload(const char *f, void *old, int idx)
{
    (void)old;
    ev_log(EV_RESLOAD, f ? (int)f[0] : 0, idx, 0, 0, 0, 0, 0, 0, 0);
    return g_resbuf;
}
static void __cdecl stub_resblit(void *buf, int idx, void *dst, int pitch, int mode)
{
    (void)buf; (void)dst;
    ev_log(EV_RESBLIT, idx, pitch, mode, 0, 0, 0, 0, 0, 0);
}
static void __cdecl stub_free(void *p)
{
    ev_log(EV_FREE, (int)(uintptr_t)p, 0, 0, 0, 0, 0, 0, 0, 0);
}
static void __cdecl stub_delay(int ms)
{
    ev_log(EV_DELAY, ms, 0, 0, 0, 0, 0, 0, 0, 0);
}
static void __cdecl stub_paladd(int s, int e, int a)
{
    ev_log(EV_PALADD, s, e, a, 0, 0, 0, 0, 0, 0);
}
static void __cdecl stub_1366a(int a)
{
    ev_log(EV_1366A, a, 0, 0, 0, 0, 0, 0, 0, 0);
}
static void __cdecl stub_134e4(void) { ev_log(EV_134E4, 0, 0, 0, 0, 0, 0, 0, 0, 0); }
static void __cdecl stub_12cea(int a, int b)
{
    ev_log(EV_12CEA, a, b, 0, 0, 0, 0, 0, 0, 0);
}
static void __cdecl stub_22253(int a, int b, int c, int d, int e)
{
    ev_log(EV_22253, a, b, c, d, e, 0, 0, 0, 0);
}
/* --- batch-4 services (src/game/ev4.c) -------------------------------- */
uint32_t dos_lowmem_base = 0x00070000u;
static int g_key = 0x1C, g_kbd_countdown = 0;
static uint8_t g_script[20] = { 3, 2,2, 3,1, 4,2,  0x82,2, 3,1, 4,2,  0x80,2, 5,1, 6,2 };
static uint8_t g_screen[64000];
static void __cdecl stub_11d40(int s, int e, int a) { ev_log(EV_EXT, 1, s, e, a, 0, 0, 0, 0, 0); }
static void __cdecl stub_11eb0(void *d, int dp, const void *s, int sp, int w, int h)
{
    (void)d; (void)s;
    ev_log(EV_EXT, 2, dp, sp, w, h, 0, 0, 0, 0);
}
static void __cdecl stub_11eee(uint8_t *d, int p, int w, int h, int ox, int oy)
{
    (void)d;
    ev_log(EV_EXT, 3, p, w, h, ox, oy, 0, 0, 0);
}
static void __cdecl stub_127e0(int i) { ev_log(EV_EXT, 4, i, 0, 0, 0, 0, 0, 0, 0); }
static void __cdecl stub_129ec(void) { ev_log(EV_EXT, 5, 0, 0, 0, 0, 0, 0, 0, 0); }
static void __cdecl stub_1297d(void) { ev_log(EV_EXT, 6, 0, 0, 0, 0, 0, 0, 0, 0); }
static void __cdecl stub_127a9(void) { ev_log(EV_EXT, 7, 0, 0, 0, 0, 0, 0, 0, 0); }
static void __cdecl stub_32230(int i) { ev_log(EV_EXT, 8, i, 0, 0, 0, 0, 0, 0, 0); }
static void __cdecl stub_1974c(int y, void *d, void *s)
{
    (void)d; (void)s;
    ev_log(EV_EXT, 9, y, 0, 0, 0, 0, 0, 0, 0);
}
static int __cdecl stub_int386(int no, const void *in, void *out)
{
    (void)in;
    ev_log(EV_EXT, 10, no, 0, 0, 0, 0, 0, 0, 0);
    if (out) ((uint8_t *)out)[1] = (uint8_t)g_key;
    return 0;
}
static int __cdecl stub_10620(void)
{
    ev_log(EV_EXT, 11, 0, 0, 0, 0, 0, 0, 0, 0);
    if (g_kbd_countdown > 0) { g_kbd_countdown--; return 0; }
    return 1;
}
static void *__cdecl stub_4eb48(int sel) { (void)sel; return g_script; }
static void __cdecl stub_4e31c(void) { ev_log(EV_EXT, 12, 0, 0, 0, 0, 0, 0, 0, 0); }

/* --- world setup / capture --------------------------------------------- */
struct cap {
    uint8_t obj1[SNAP_N];
    uint8_t obj2[SNAP2_N];
    uint8_t vga[VGA_N];
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
    /* The handlers read/write much of obj1 (and util_rand touches obj2), so
     * both runs must start from the same post-relocation image. */
    memcpy((void *)(uintptr_t)SNAP_BASE, obj1_p, SNAP_N);
    memcpy((void *)(uintptr_t)SNAP2_BASE, obj2_p, SNAP2_N);
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
    W32(0x53A7D) = (uint32_t)(uintptr_t)strm_o;
    W32(0x51A83) = 0xDEADBEEFu;
    W32(0x53BEF) = (uint32_t)(rnd() & 0xFFu);
    W32(0x53BEB) = (uint32_t)(rnd() % 12u);
    W32(0x53AA9) = (uint32_t)((int)(rnd() % 9u) - 4);
    W32(0x53AAD) = (uint32_t)((int)(rnd() % 9u) - 4);
    W32(0x53AB1) = rnd();
    W32(0x53AB5) = rnd();
    W32(0x53AB9) = rnd();
    W32(0x53ABD) = rnd();
    g_argx = (int)(rnd() % 9u) - 4;
    g_argy = (int)(rnd() % 9u) - 4;
    g_arg  = (int)(rnd() % 200u);
    W32(0x53C5B) = (uint32_t)(uintptr_t)g_screen;
    W32(0x53C5F) = (uint32_t)(uintptr_t)g_screen;
    W32(0x53C63) = (uint32_t)(uintptr_t)g_screen;
    *(volatile uint16_t *)(uintptr_t)(dos_lowmem_base + 0x46C) = (uint16_t)(rnd() & 0xFFFFu);
    g_key  = (int)(rnd() % 256u);
    g_kbd_countdown = (int)(rnd() % 3u);
    g_nev  = 0;
}
/* The C side reads the same globals but must point at the C buffers. */
static void point_c_side(void)
{
    W32(0x53A45) = (uint32_t)(uintptr_t)rec_c;
    W32(0x53A55) = (uint32_t)(uintptr_t)state_c;
    W32(0x53AD5) = (uint32_t)(uintptr_t)stat_c;
    W32(0x53A79) = (uint32_t)(uintptr_t)strm_c;
    W32(0x53A7D) = (uint32_t)(uintptr_t)strm_c;
}

static void capture(struct cap *c)
{
    memcpy(c->obj1, (const void *)(uintptr_t)SNAP_BASE, SNAP_N);
    memcpy(c->obj2, (const void *)(uintptr_t)SNAP2_BASE, SNAP2_N);
    memcpy(c->vga, (const void *)(uintptr_t)VGA_BASE, VGA_N);
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
    W32(0x53A7D) = (uint32_t)(uintptr_t)strm_o;
    memcpy(c->obj1, (const void *)(uintptr_t)SNAP_BASE, SNAP_N);
    memcpy(c->obj2, (const void *)(uintptr_t)SNAP2_BASE, SNAP2_N);
    memcpy(c->vga, (const void *)(uintptr_t)VGA_BASE, VGA_N);
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
        if (getenv("EVCHECK_DUMP")) {
            int n = o->nev > t->nev ? o->nev : t->nev;
            int f = 0;
            while (f < n && f < o->nev && f < t->nev
                   && o->ev[f].kind == t->ev[f].kind) {
                int same = 1;
                for (j = 0; j < 9; j++)
                    if (o->ev[f].v[j] != t->ev[f].v[j]) same = 0;
                if (!same) break;
                f++;
            }
            for (i = f > 2 ? f - 2 : 0; i < n && i < f + 4; i++) {
                printf("  [%d] orig %s", i,
                       i < o->nev ? kind_name[o->ev[i].kind] : "-");
                for (j = 0; j < 9; j++)
                    printf(",%d", i < o->nev ? o->ev[i].v[j] : 0);
                printf("  ours %s", i < t->nev ? kind_name[t->ev[i].kind] : "-");
                for (j = 0; j < 9; j++)
                    printf(",%d", i < t->nev ? t->ev[i].v[j] : 0);
                printf("\n");
            }
            printf("  first difference around event %d\n", f);
        }
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
    for (i = 0; i < SNAP2_N && !bad; i++)
        if (o->obj2[i] != t->obj2[i]) {
            printf("FAIL case %u %s: obj2+%X orig=%02X ours=%02X\n", id, name,
                   i, o->obj2[i], t->obj2[i]);
            failures++; bad = 1;
        }
    for (i = 0; i < VGA_N && !bad; i++)
        if (o->vga[i] != t->vga[i]) {
            printf("FAIL case %u %s: vga+%X orig=%02X ours=%02X\n", id, name,
                   i, o->vga[i], t->vga[i]);
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

/* --- batch 4 (src/game/ev4.c) ----------------------------------------- */
typedef int (*h0i_fn)(void);
#define O_1366A ((h1_fn)(uintptr_t)0x0001366Au)
#define O_11AA8 ((h0i_fn)(uintptr_t)0x00011AA8u)
#define O_11B48 ((h0_fn)(uintptr_t)0x00011B48u)
#define O_11B9B ((h0_fn)(uintptr_t)0x00011B9Bu)
#define O_11BFA ((h0_fn)(uintptr_t)0x00011BFAu)
#define O_11C59 ((h0_fn)(uintptr_t)0x00011C59u)
#define O_12263 ((h0_fn)(uintptr_t)0x00012263u)
#define O_1E1DC ((h1_fn)(uintptr_t)0x0001E1DCu)
#define O_24B4D ((h1_fn)(uintptr_t)0x00024B4Du)
#define O_196CB ((h0_fn)(uintptr_t)0x000196CBu)
#define E4(a) \
    static void o_##a(void) { O_##a(); } \
    static void c_##a(void) { ev4_##a(); }
E4(11B48) E4(11B9B) E4(11BFA) E4(11C59) E4(12263) E4(196CB)
static void o_1366A(void) { O_1366A(g_arg % 4); }
static void c_1366A(void) { ev4_1366A(g_arg % 4); }
static void o_11AA8(void) { (void)O_11AA8(); }
static void c_11AA8(void) { (void)ev4_11AA8(); }
static void o_1E1DC(void) { O_1E1DC(g_arg % 64); }
static void c_1E1DC(void) { ev4_1E1DC(g_arg % 64); }
static void o_24B4D(void) { O_24B4D(g_arg % 4); }
static void c_24B4D(void) { ev4_24B4D(g_arg % 4); }

#define EV3_PAIR(a, name) \
    static void o_##name(void) { O_##name(g_arg); } \
    static void c_##name(void) { ev3_##name(g_arg); }
EV3_PAIR(x, 352CA) EV3_PAIR(x, 35346) EV3_PAIR(x, 35468) EV3_PAIR(x, 355F0)
EV3_PAIR(x, 356B3) EV3_PAIR(x, 35730) EV3_PAIR(x, 357DD) EV3_PAIR(x, 35833)
EV3_PAIR(x, 35854) EV3_PAIR(x, 35A0D) EV3_PAIR(x, 35C40) EV3_PAIR(x, 35CF1)
EV3_PAIR(x, 35D1E) EV3_PAIR(x, 35D9E) EV3_PAIR(x, 35E0E) EV3_PAIR(x, 35E5B)
EV3_PAIR(x, 35EC1) EV3_PAIR(x, 35F48) EV3_PAIR(x, 35F88) EV3_PAIR(x, 35FCF)
EV3_PAIR(x, 360B6) EV3_PAIR(x, 3623C) EV3_PAIR(x, 362E8)

static void o_35B78(void) { O_35B78(g_argx, g_argy, g_arg % 12); }
static void c_35B78(void) { ev3_35B78(g_argx, g_argy, g_arg % 12); }
static void o_35F10(void) { O_35F10(g_arg % 16); }
static void c_35F10(void) { ev3_35F10(g_arg % 16); }
static void o_361B0(void) { O_361B0(); }
static void c_361B0(void) { ev3_361B0(); }
static void o_2AEDB(void) { (void)O_2AEDB(g_arg % 64, g_arg % 256); }
static void c_2AEDB(void) { (void)ev3_2AEDB(g_arg % 64, g_arg % 256); }
static void o_33F78(void) { O_33F78(g_arg, g_argx, g_argy); }
static void c_33F78(void) { ev3_33F78(g_arg, g_argx, g_argy); }

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
    /* batch 3 (src/game/ev3.c) */
    { 0x352CA, "352CA", o_352CA, c_352CA },
    { 0x35346, "35346", o_35346, c_35346 },
    { 0x35468, "35468", o_35468, c_35468 },
    { 0x355F0, "355F0", o_355F0, c_355F0 },
    { 0x356B3, "356B3", o_356B3, c_356B3 },
    { 0x35730, "35730", o_35730, c_35730 },
    { 0x357DD, "357DD", o_357DD, c_357DD },
    { 0x35833, "35833", o_35833, c_35833 },
    { 0x35854, "35854", o_35854, c_35854 },
    { 0x35A0D, "35A0D", o_35A0D, c_35A0D },
    { 0x35C40, "35C40", o_35C40, c_35C40 },
    { 0x35CF1, "35CF1", o_35CF1, c_35CF1 },
    { 0x35D1E, "35D1E", o_35D1E, c_35D1E },
    { 0x35D9E, "35D9E", o_35D9E, c_35D9E },
    { 0x35E0E, "35E0E", o_35E0E, c_35E0E },
    { 0x35E5B, "35E5B", o_35E5B, c_35E5B },
    { 0x35EC1, "35EC1", o_35EC1, c_35EC1 },
    { 0x35F48, "35F48", o_35F48, c_35F48 },
    { 0x35F88, "35F88", o_35F88, c_35F88 },
    { 0x35FCF, "35FCF", o_35FCF, c_35FCF },
    { 0x360B6, "360B6", o_360B6, c_360B6 },
    { 0x3623C, "3623C", o_3623C, c_3623C },
    { 0x362E8, "362E8", o_362E8, c_362E8 },
    { 0x35B78, "35B78", o_35B78, c_35B78 },
    { 0x35F10, "35F10", o_35F10, c_35F10 },
    { 0x361B0, "361B0", o_361B0, c_361B0 },
    { 0x2AEDB, "2AEDB", o_2AEDB, c_2AEDB },
    { 0x33F78, "33F78", o_33F78, c_33F78 },
    /* batch 4 (src/game/ev4.c) */
    { 0x1366A, "1366A", o_1366A, c_1366A },
    { 0x11AA8, "11AA8", o_11AA8, c_11AA8 },
    { 0x11B48, "11B48", o_11B48, c_11B48 },
    { 0x11B9B, "11B9B", o_11B9B, c_11B9B },
    { 0x11BFA, "11BFA", o_11BFA, c_11BFA },
    { 0x11C59, "11C59", o_11C59, c_11C59 },
    { 0x12263, "12263", o_12263, c_12263 },
    { 0x1E1DC, "1E1DC", o_1E1DC, c_1E1DC },
    { 0x24B4D, "24B4D", o_24B4D, c_24B4D },
    { 0x196CB, "196CB", o_196CB, c_196CB },
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

/* Redirect absolute low-memory operands (0x400..0x500) into the 0x70000
 * mirror, exactly as dos.c does for the host - 0x11AA8 reads the BDA tick at
 * 0x46C. Copied from src/leafcheck.c. */
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
    printf("low-memory operands redirected: %d\n",
           patch_lowmem_refs(&le, (uint32_t)dos_lowmem_base));
    if (plat_commit((uintptr_t)VGA_BASE, VGA_N, PLAT_PROT_RWX) == NULL) {
        printf("FAIL: VGA block 0xA0000 unavailable\n");
        return 2;
    }
    memcpy(obj1_p, (const void *)(uintptr_t)SNAP_BASE, SNAP_N);
    memcpy(obj2_p, (const void *)(uintptr_t)SNAP2_BASE, SNAP2_N);

    /* record table / script state / status / stream are host buffers */
    HOOK(0x15F84, stub_vm);
    HOOK(0x344F2, stub_status);
    HOOK(0x112A5, stub_unit);
    HOOK(0x10B4E, stub_load);
    HOOK(0x17AA9, stub_wait);
    HOOK(0x11CAC, stub_view);
    HOOK(0x4E381, stub_flush);
    /* 0x35B78 / 0x35F10 are under test in batch 3: do NOT hook them. */
    HOOK(0x1AA1D, stub_scene);
    HOOK(0x2E2B0, stub_2e2b0);
    HOOK(0x1DB65, stub_1db65);
    /* 0x12263 / 0x196CB / 0x1366A are under test in batch 4 - do NOT hook. */
    HOOK(0x1956B, stub_msopen);

    HOOK(0x16559, stub_dlgblit);
    HOOK(0x16C57, stub_dlgwait);
    HOOK(0x12E38, stub_mapcell);
    HOOK(0x111BA, stub_resload);
    HOOK(0x2EB9F, stub_resblit);
    HOOK(0x3776E, stub_free);
    HOOK(0x3790A, stub_delay);
    HOOK(0x11DF2, stub_paladd);

    HOOK(0x134E4, stub_134e4);
    HOOK(0x12CEA, stub_12cea);
    HOOK(0x22253, stub_22253);
    HOOK(0x11D40, stub_11d40);
    HOOK(0x11EB0, stub_11eb0);
    HOOK(0x11EEE, stub_11eee);
    HOOK(0x127E0, stub_127e0);
    HOOK(0x129EC, stub_129ec);
    HOOK(0x1297D, stub_1297d);
    HOOK(0x127A9, stub_127a9);
    HOOK(0x32230, stub_32230);
    HOOK(0x1974C, stub_1974c);
    HOOK(0x370F0, stub_int386);
    HOOK(0x10620, stub_10620);
    HOOK(0x4E31C, stub_4e31c);
    HOOK(0x4EB48, stub_4eb48);

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
