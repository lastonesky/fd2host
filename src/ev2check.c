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
#include "game/ev5.h"
#include "game/ev6.h"
#include "game/menu_actions.h"
#include "game/rec.h"
#include "game/anim.h"
#include "game/map.h"
#include "game/scene.h"
#include "game/msg.h"

#define BDA_W(off) (*(volatile uint16_t *)(uintptr_t)(dos_lowmem_base + (off)))
#define dword_53AC1 (*(uint32_t *)(uintptr_t)0x00053AC1u)
#define dword_53AC5 (*(uint32_t *)(uintptr_t)0x00053AC5u)

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
       EV_134E4, EV_12CEA, EV_22253, EV_MAPR, EV_EXT, EV_N };
static const char *const kind_name[EV_N] = {
    "vm_run", "status", "unit_add", "load", "wait", "seq", "clear",
    "view", "flush", "scene", "2E2B0", "1DB65", "12263",
    "msg_open", "msg_close", "dlg_blit", "dlg_wait", "map_cell",
    "res_load", "res_blit", "free", "delay", "pal_add", "1366A",
    "134E4", "12CEA", "22253", "map_render", "ext"
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
        out[4] = (uint8_t)(((x + y) & 1) ? 0x20 : 0x00);  /* cell kind flag */
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
static void __cdecl stub_maprender(int idx)
{
    ev_log(EV_MAPR, idx, 0, 0, 0, 0, 0, 0, 0, 0);
}
/* --- batch-4 services (src/game/ev4.c) -------------------------------- */
uint32_t dos_lowmem_base = 0x00070000u;
static int g_key = 0x1C, g_kbd_countdown = 0;
static uint8_t g_script[20] = { 3, 2,2, 3,1, 4,2,  0x82,2, 3,1, 4,2,  0x80,2, 5,1, 6,2 };
static uint8_t g_screen[64000];
static uint8_t *g_recbase;          /* current side's record buffer */
static int g_rx, g_ry, g_ra, g_rb, g_rv;
static void __cdecl stub_outp(int port, int val)
{
    ev_log(EV_EXT, 13, port, val, 0, 0, 0, 0, 0, 0);
}
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
/* --- batch-7 services (src/game/ev7.c) -------------------------------- */
static void __cdecl stub_1088d(int a) { ev_log(EV_EXT, 20, a, 0, 0, 0, 0, 0, 0, 0); }
static void __cdecl stub_glide(int a) { ev_log(EV_EXT, 21, a, 0, 0, 0, 0, 0, 0, 0); }
static void __cdecl stub_fadein(void) { ev_log(EV_EXT, 22, 0, 0, 0, 0, 0, 0, 0, 0); }

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
    rec_in[19604] = (uint8_t)(rnd() % 6u);   /* 0x314DE's p[4] must be 0..5 */
    rec_in[4014] = 0x40; rec_in[4015] = 0;   /* 0x26C9B table offset = 0x40 */
    rec_in[4016] = 0;    rec_in[4017] = 0;
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
    W32(0x53A51) = (uint32_t)(uintptr_t)(rec_o + 2000);   /* map cells   */
    W32(0x53BF7) = (uint32_t)(uintptr_t)(rec_o + 3000);   /* party rows  */
    W32(0x53F66) = (uint32_t)(uintptr_t)(rec_o + 4000);   /* 6-byte tbl  */
    W32(0x53AC1) = 1 + (rnd() % 8u);
    W32(0x53AC5) = 1 + (rnd() % 8u);
    W32(0x53BFB) = 2 + (rnd() % 8u);
    W32(0x53A0C) = 0xDEADBEEFu;   /* != the BDA tick, so 0x13460 exits */
    rec_o[4000 + 14] = 0x40; rec_o[4000 + 15] = 0;    /* table offset */
    rec_o[2000 + 4 * 0 + 7] = 1;                      /* reveal a cell */
    g_recbase = rec_o;
    g_rx = (int)(rnd() % (unsigned)dword_53AC1);
    g_ry = (int)(rnd() % (unsigned)dword_53AC5);
    g_ra = (int)(rnd() % 64u);
    g_rb = (int)(rnd() % 256u);
    g_rv = (int)(rnd() % 6u);
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
    W32(0x53A51) = (uint32_t)(uintptr_t)(rec_c + 2000);
    W32(0x53BF7) = (uint32_t)(uintptr_t)(rec_c + 3000);
    W32(0x53F66) = (uint32_t)(uintptr_t)(rec_c + 4000);
    rec_c[4000 + 14] = 0x40; rec_c[4000 + 15] = 0;
    rec_c[2000 + 7] = 1;
    g_recbase = rec_c;
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
    W32(0x53A51) = (uint32_t)(uintptr_t)(rec_o + 2000);
    W32(0x53BF7) = (uint32_t)(uintptr_t)(rec_o + 3000);
    W32(0x53F66) = (uint32_t)(uintptr_t)(rec_o + 4000);
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

/* --- batch 5 (src/game/ev5.c) ----------------------------------------- */
typedef int  (*h1i_fn)(int);
typedef void (*v1p_fn)(void *);
typedef void (*v2p_fn)(int, void *);
typedef void (*v3p_fn)(void *, int, int);
typedef void *(*p1p_fn)(const void *);
typedef void *(*p0_fn)(void);
typedef int  (*i1p_fn)(const void *);
typedef void (*h4_fn)(int, int, int, int);
#define O_2860A ((h2i_fn)(uintptr_t)0x0002860Au)
#define O_146A7 ((h2_fn)(uintptr_t)0x000146A7u)
#define O_13460 ((h0i_fn)(uintptr_t)0x00013460u)
#define O_13536 ((h0_fn)(uintptr_t)0x00013536u)
#define O_1D4CB ((p0_fn)(uintptr_t)0x0001D4CBu)
#define O_173E7 ((v1p_fn)(uintptr_t)0x000173E7u)
#define O_24B14 ((h1i_fn)(uintptr_t)0x00024B14u)
#define O_25052 ((h2_fn)(uintptr_t)0x00025052u)
#define O_25089 ((h0_fn)(uintptr_t)0x00025089u)
#define O_34317 ((v2p_fn)(uintptr_t)0x00034317u)
#define O_1F6EF ((h4_fn)(uintptr_t)0x0001F6EFu)
#define O_1C220 ((h1i_fn)(uintptr_t)0x0001C220u)
#define O_1E5C0 ((h1_fn)(uintptr_t)0x0001E5C0u)
#define O_2B749 ((i1p_fn)(uintptr_t)0x0002B749u)
#define O_26C9B ((v3p_fn)(uintptr_t)0x00026C9Bu)
#define O_314DE ((p1p_fn)(uintptr_t)0x000314DEu)
#define O_1B5F1 ((h1i_fn)(uintptr_t)0x0001B5F1u)
#define O_14B16 ((i1p_fn)(uintptr_t)0x00014B16u)
#define O_203BD ((h3_fn)(uintptr_t)0x000203BDu)
#define O_208CF ((h0_fn)(uintptr_t)0x000208CFu)
#define O_20AAF ((h0_fn)(uintptr_t)0x00020AAFu)
#define O_20BF5 ((h0_fn)(uintptr_t)0x00020BF5u)
#define O_20B72 ((h0_fn)(uintptr_t)0x00020B72u)
#define O_205B4 ((h0_fn)(uintptr_t)0x000205B4u)
#define O_205BE ((h0_fn)(uintptr_t)0x000205BEu)
#define O_1F04A ((h2_fn)(uintptr_t)0x0001F04Au)
#define O_1F0DC ((h2i_fn)(uintptr_t)0x0001F0DCu)
#define O_1B653 ((v1p_fn)(uintptr_t)0x0001B653u)
#define E5I(a) \
    static void o_##a(void) { (void)O_##a(g_arg); } \
    static void c_##a(void) { (void)ev5_##a(g_arg); }
#define E5V(a) \
    static void o_##a(void) { O_##a(); } \
    static void c_##a(void) { ev5_##a(); }
E5I(24B14) E5I(1C220) E5I(1B5F1)
E5V(13536) E5V(25089) E5V(208CF) E5V(20AAF) E5V(20BF5) E5V(20B72)
E5V(205B4) E5V(205BE)
static void o_2860A(void) { (void)O_2860A(g_arg, g_argx); }
static void c_2860A(void) { (void)ev5_2860A(g_arg, g_argx); }
static void o_146A7(void) { O_146A7(g_rx, g_ry); }
static void c_146A7(void) { ev5_146A7(g_rx, g_ry); }
static void o_13460(void) { (void)O_13460(); }
static void c_13460(void) { (void)ev5_13460(); }
static void o_1D4CB(void) { (void)O_1D4CB(); }
static void c_1D4CB(void) { (void)ev5_1D4CB(); }
static void o_173E7(void) { O_173E7(g_recbase + 19000); }
static void c_173E7(void) { ev5_173E7((int *)(g_recbase + 19000)); }
static void o_25052(void) { O_25052(g_ra % 64, g_rv); }
static void c_25052(void) { ev5_25052(g_ra % 64, g_rv); }
static void o_34317(void) { O_34317(g_ra, g_recbase + 17000); }
static void c_34317(void) { ev5_34317(g_ra, g_recbase + 17000); }
static void o_1F6EF(void) { O_1F6EF(g_ra * 4, g_ry * 20, g_rb, 1 + g_rv * 4); }
static void c_1F6EF(void) { ev5_1F6EF(g_ra * 4, g_ry * 20, g_rb, 1 + g_rv * 4); }
static void o_1E5C0(void) { O_1E5C0(g_rv); }
static void c_1E5C0(void) { ev5_1E5C0(g_rv); }
static void o_2B749(void) { (void)O_2B749(g_recbase + 19500); }
static void c_2B749(void) { (void)ev5_2B749(g_recbase + 19500); }
static void o_26C9B(void) { O_26C9B(g_recbase + 18000, 6 + g_rv * 4, g_rv); }
static void c_26C9B(void) { ev5_26C9B(g_recbase + 18000, 6 + g_rv * 4, g_rv); }
static void o_314DE(void) { (void)O_314DE(g_recbase + 19600); }
static void c_314DE(void) { (void)ev5_314DE(g_recbase + 19600); }
static void o_14B16(void) { (void)O_14B16(g_recbase + 15000); }
static void c_14B16(void) { (void)ev5_14B16(g_recbase + 15000); }
static void o_203BD(void) { O_203BD(g_ra % 64, g_rb % 64, (g_rv * 10) % 64); }
static void c_203BD(void) { ev5_203BD(g_ra % 64, g_rb % 64, (g_rv * 10) % 64); }
static void o_1F04A(void) { O_1F04A(g_arg % 8, g_argx % 8); }
static void c_1F04A(void) { ev5_1F04A(g_arg % 8, g_argx % 8); }
static void o_1F0DC(void) { (void)O_1F0DC(g_arg % 8, g_argx % 8); }
static void c_1F0DC(void) { (void)ev5_1F0DC(g_arg % 8, g_argx % 8); }
static void o_1B653(void) { O_1B653(g_recbase + 16000); }
static void c_1B653(void) { ev5_1B653(g_recbase + 16000); }

/* --- batch 6 (src/game/ev6.c) ----------------------------------------- */
#define O_34531 ((h1_fn)(uintptr_t)0x00034531u)
#define O_3460B ((h1_fn)(uintptr_t)0x0003460Bu)
#define O_34673 ((h1_fn)(uintptr_t)0x00034673u)
#define O_346CD ((h1_fn)(uintptr_t)0x000346CDu)
#define O_34778 ((h1_fn)(uintptr_t)0x00034778u)
#define O_350BE ((h1_fn)(uintptr_t)0x000350BEu)
#define O_350C8 ((h1_fn)(uintptr_t)0x000350C8u)
#define O_34818 ((h1_fn)(uintptr_t)0x00034818u)
#define O_348BB ((h1_fn)(uintptr_t)0x000348BBu)
#define O_34940 ((h1_fn)(uintptr_t)0x00034940u)
#define O_34984 ((h1_fn)(uintptr_t)0x00034984u)
#define O_349EC ((h1_fn)(uintptr_t)0x000349ECu)
#define O_34A1E ((h1_fn)(uintptr_t)0x00034A1Eu)
#define O_34B07 ((h1_fn)(uintptr_t)0x00034B07u)
#define O_34B6F ((h1_fn)(uintptr_t)0x00034B6Fu)
#define O_34B9A ((h1_fn)(uintptr_t)0x00034B9Au)
#define O_34C52 ((h1_fn)(uintptr_t)0x00034C52u)
#define O_34C7A ((h1_fn)(uintptr_t)0x00034C7Au)
#define O_34D2F ((h1_fn)(uintptr_t)0x00034D2Fu)
#define O_34DD0 ((h1_fn)(uintptr_t)0x00034DD0u)
#define O_34EB3 ((h1_fn)(uintptr_t)0x00034EB3u)
#define O_34F38 ((h1_fn)(uintptr_t)0x00034F38u)
#define O_34FC2 ((h1_fn)(uintptr_t)0x00034FC2u)
#define O_34FCC ((h1_fn)(uintptr_t)0x00034FCCu)
#define O_35022 ((h1_fn)(uintptr_t)0x00035022u)
#define E6(a) \
    static void o_##a(void) { O_##a(g_arg); } \
    static void c_##a(void) { ev6_##a(g_arg); }
E6(34531) E6(3460B) E6(34673) E6(346CD) E6(34778) E6(350BE) E6(350C8)
E6(34818) E6(348BB) E6(34940) E6(34984) E6(349EC) E6(34A1E) E6(34B07)
E6(34B6F) E6(34B9A) E6(34C52) E6(34C7A) E6(34D2F) E6(34DD0) E6(34EB3)
E6(34F38) E6(34FC2) E6(34FCC) E6(35022)

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

/* --- menu/party action handlers (src/game/menu_actions.c) ----------------
 * Wrapper tag = the original address, so the three identical "plain"
 * handlers keep their own original entry point while sharing one C body. */
#define MENU_PAIR(addr, tag, fn) \
    static void o_##tag(void) { ((void (*)(void))(uintptr_t)(addr))(); } \
    static void c_##tag(void) { fn(); }
MENU_PAIR(0x205DA, 205DA, menu_reload_world)
MENU_PAIR(0x206C5, 206C5, menu_need_records_marked)
MENU_PAIR(0x20707, 20707, menu_need_flags_clear_50_51)
MENU_PAIR(0x2073D, 2073D, menu_need_flag_clear_14)
MENU_PAIR(0x20765, 20765, menu_need_any_clear_15_26)
MENU_PAIR(0x20822, 20822, menu_need_flag_clear_64)
MENU_PAIR(0x2084A, 2084A, menu_need_flag_clear_65)
MENU_PAIR(0x20872, 20872, menu_need_flag_clear_52_no_unit18)
MENU_PAIR(0x20926, 20926, menu_need_flag_clear_64_late)
MENU_PAIR(0x20957, 20957, menu_need_any_clear_26_43)
MENU_PAIR(0x20A51, 20A51, menu_need_flags_clear_16_17)
MENU_PAIR(0x20A87, 20A87, menu_need_flag_clear_1)
MENU_PAIR(0x20B14, 20B14, menu_need_flag_clear_16)
MENU_PAIR(0x20B3C, 20B3C, menu_need_flags_clear_1_2)
MENU_PAIR(0x3314B, 3314B, menu_show_reload)
MENU_PAIR(0x33219, 33219, menu_show_reload_pair)
MENU_PAIR(0x3332B, 3332B, menu_show_mark_two)
MENU_PAIR(0x3346B, 3346B, menu_show_plain)
MENU_PAIR(0x3347C, 3347C, menu_show_step20)
MENU_PAIR(0x335A0, 335A0, menu_show_plain)
MENU_PAIR(0x335AA, 335AA, menu_show_or_rebuild_sprites)
MENU_PAIR(0x33674, 33674, menu_show_plain)
MENU_PAIR(0x3367E, 3367E, menu_show_step16_then_clear)
MENU_PAIR(0x33AAE, 33AAE, menu_show_step9_then_clear)
MENU_PAIR(0x33169, 33169, menu_rebuild_sprites_and_show)
MENU_PAIR(0x3327D, 3327D, menu_mark_records_and_show)
MENU_PAIR(0x33367, 33367, menu_rebuild_sprites_and_clear)
MENU_PAIR(0x333F5, 333F5, menu_reset_and_show)
MENU_PAIR(0x334D9, 334D9, menu_show_gated_by_unit)
MENU_PAIR(0x335DA, 335DA, menu_step_pair_then_clear)

/* --- small scene/UI leaves (batch 45) ------------------------------------ */
#define W32P(x) (*(uint32_t *)(uintptr_t)(x))
static uint8_t g_lx[32], g_ly[32];
static uint8_t g_frames[128];
static uint8_t g_range[2];
static uint16_t g_counter;

static void frames_init(void)
{
    int i;
    for (i = 0; i < 128; i++) g_frames[i] = 0;
    g_frames[0] = 2;                     /* two frames */
    W32P((uintptr_t)g_frames + 8) = 16;  /* frame 0 offset */
    W32P((uintptr_t)g_frames + 12) = 32; /* frame 1 offset */
    g_frames[16 + 6] = 3;                /* frame 0 sub-steps */
    g_frames[32 + 6] = 2;                /* frame 1 sub-steps */
}

static void leave_args(int side)   /* side 0 = orig buffer, 1 = C buffer */
{
    int i;
    (void)side;
    for (i = 0; i < 32; i++) { g_lx[i] = (uint8_t)(g_argx + i); g_ly[i] = (uint8_t)(g_argy + i); }
    /* pointer globals must hold the SAME value on both sides or obj1 diverges:
     * point to the orig-side scratch, which neither side mutates here. */
    W32P(0x53A69) = (uint32_t)(uintptr_t)(rec_o + 4500);
    W32P(0x53EC4) = 0;
    W32P(0x53AB1) = 0; W32P(0x53AB5) = 0;
    g_range[0] = 1 + (uint8_t)(g_argx & 3);
    g_range[1] = g_range[0] + (uint8_t)(1 + (g_argy & 3));
    g_counter = (uint16_t)g_arg;
}
static int  ret_orig(int r) { *(uint16_t *)(rec_o + 20470) = (uint16_t)r; return r; }
static int  ret_ours(int r) { *(uint16_t *)(rec_c + 20470) = (uint16_t)r; return r; }

/* 0x1C269 rec_collect_slot_bits */
static void o_rec_slot_bits(void)
{ (void)ret_orig(((int (*)(int, uint8_t *))(uintptr_t)0x0001C269u)
                ((g_argx + 8) & 0x3F, rec_o + 19000)); }
static void c_rec_slot_bits(void)
{ (void)ret_ours(rec_collect_slot_bits((g_argx + 8) & 0x3F, rec_c + 19000)); }

/* 0x311E5 anim_cycle_frame */
static void o_anim_cycle(void)
{ frames_init(); ((void (*)(const void *, int, void *, int))(uintptr_t)0x000311E5u)
                 (g_frames, 1 + (g_argy & 7), rec_o + 5000, 456); }
static void c_anim_cycle(void)
{ frames_init(); anim_cycle_frame(g_frames, 1 + (g_argy & 7), rec_c + 5000, 456); }

/* 0x1E0DB map_enqueue_status */
static void o_enq_status(void)
{ leave_args(0); ((void (*)(int, int, int))(uintptr_t)0x0001E0DBu)
                 (g_argx, g_argy & 0xFF, g_rx); }
static void c_enq_status(void)
{ leave_args(1); map_enqueue_status(g_argx, g_argy & 0xFF, g_rx); }

/* 0x233C6 scene_place_records */
static void o_place_records(void)
{ leave_args(0); ((void (*)(const uint8_t *, const uint8_t *, uintptr_t, int, int,
                 int, int, int, int, uint64_t))(uintptr_t)0x000233C6u)
                 (g_lx, g_ly, (uintptr_t)(g_argx & 3), 0, 2, 3, g_argx, g_argy,
                  g_argx & 3, ((uint64_t)(uint32_t)g_argx << 32) | (uint32_t)g_argy); }
static void c_place_records(void)
{ leave_args(1); scene_place_records(g_lx, g_ly, (uintptr_t)(g_argx & 3), 0, 2, 3,
                 g_argx, g_argy, g_argx & 3,
                 ((uint64_t)(uint32_t)g_argx << 32) | (uint32_t)g_argy); }

/* 0x31BDF msg_show_lines */
static void o_show_lines(void)
{ ((void (*)(int, int))(uintptr_t)0x00031BDFu)(g_argx & 7, g_argy & 3); }
static void c_show_lines(void)
{ msg_show_lines(g_argx & 7, g_argy & 3); }

/* 0x1E529 msg_show_page */
static void o_show_page(void)
{ leave_args(0); (void)ret_orig(((int (*)(uint16_t *, const uint8_t *, int, int))
                 (uintptr_t)0x0001E529u)(&g_counter, g_range, g_argx & 3, g_argy & 3)); }
static void c_show_page(void)
{ leave_args(1); (void)ret_ours(msg_show_page(&g_counter, g_range, g_argx & 3, g_argy & 3)); }

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
    /* batch 5 (src/game/ev5.c) */
    { 0x2860A, "2860A", o_2860A, c_2860A },
    { 0x146A7, "146A7", o_146A7, c_146A7 },
    { 0x13460, "13460", o_13460, c_13460 },
    { 0x13536, "13536", o_13536, c_13536 },
    { 0x1D4CB, "1D4CB", o_1D4CB, c_1D4CB },
    { 0x173E7, "173E7", o_173E7, c_173E7 },
    { 0x24B14, "24B14", o_24B14, c_24B14 },
    { 0x25052, "25052", o_25052, c_25052 },
    { 0x25089, "25089", o_25089, c_25089 },
    { 0x34317, "34317", o_34317, c_34317 },
    { 0x1F6EF, "1F6EF", o_1F6EF, c_1F6EF },
    { 0x1C220, "1C220", o_1C220, c_1C220 },
    { 0x1E5C0, "1E5C0", o_1E5C0, c_1E5C0 },
    { 0x2B749, "2B749", o_2B749, c_2B749 },
    { 0x26C9B, "26C9B", o_26C9B, c_26C9B },
    { 0x314DE, "314DE", o_314DE, c_314DE },
    { 0x1B5F1, "1B5F1", o_1B5F1, c_1B5F1 },
    { 0x14B16, "14B16", o_14B16, c_14B16 },
    { 0x203BD, "203BD", o_203BD, c_203BD },
    { 0x208CF, "208CF", o_208CF, c_208CF },
    { 0x20AAF, "20AAF", o_20AAF, c_20AAF },
    { 0x20BF5, "20BF5", o_20BF5, c_20BF5 },
    { 0x20B72, "20B72", o_20B72, c_20B72 },
    { 0x205B4, "205B4", o_205B4, c_205B4 },
    { 0x205BE, "205BE", o_205BE, c_205BE },
    { 0x1F04A, "1F04A", o_1F04A, c_1F04A },
    { 0x1F0DC, "1F0DC", o_1F0DC, c_1F0DC },
    { 0x1B653, "1B653", o_1B653, c_1B653 },
    /* batch 6 (src/game/ev6.c) */
    { 0x34531, "34531", o_34531, c_34531 },
    { 0x3460B, "3460B", o_3460B, c_3460B },
    { 0x34673, "34673", o_34673, c_34673 },
    { 0x346CD, "346CD", o_346CD, c_346CD },
    { 0x34778, "34778", o_34778, c_34778 },
    { 0x350BE, "350BE", o_350BE, c_350BE },
    { 0x350C8, "350C8", o_350C8, c_350C8 },
    { 0x34818, "34818", o_34818, c_34818 },
    { 0x348BB, "348BB", o_348BB, c_348BB },
    { 0x34940, "34940", o_34940, c_34940 },
    { 0x34984, "34984", o_34984, c_34984 },
    { 0x349EC, "349EC", o_349EC, c_349EC },
    { 0x34A1E, "34A1E", o_34A1E, c_34A1E },
    { 0x34B07, "34B07", o_34B07, c_34B07 },
    { 0x34B6F, "34B6F", o_34B6F, c_34B6F },
    { 0x34B9A, "34B9A", o_34B9A, c_34B9A },
    { 0x34C52, "34C52", o_34C52, c_34C52 },
    { 0x34C7A, "34C7A", o_34C7A, c_34C7A },
    { 0x34D2F, "34D2F", o_34D2F, c_34D2F },
    { 0x34DD0, "34DD0", o_34DD0, c_34DD0 },
    { 0x34EB3, "34EB3", o_34EB3, c_34EB3 },
    { 0x34F38, "34F38", o_34F38, c_34F38 },
    { 0x34FC2, "34FC2", o_34FC2, c_34FC2 },
    { 0x34FCC, "34FCC", o_34FCC, c_34FCC },
    { 0x35022, "35022", o_35022, c_35022 },

    /* menu/party action handlers (src/game/menu_actions.c) */
    { 0x205DA, "menu_reload_world",           o_205DA, c_205DA },
    { 0x206C5, "menu_need_records_marked",    o_206C5, c_206C5 },
    { 0x20707, "menu_need_flags_clear_50_51", o_20707, c_20707 },
    { 0x2073D, "menu_need_flag_clear_14",     o_2073D, c_2073D },
    { 0x20765, "menu_need_any_clear_15_26",   o_20765, c_20765 },
    { 0x20822, "menu_need_flag_clear_64",     o_20822, c_20822 },
    { 0x2084A, "menu_need_flag_clear_65",     o_2084A, c_2084A },
    { 0x20872, "menu_need_flag_clear_52_no_unit18", o_20872, c_20872 },
    { 0x20926, "menu_need_flag_clear_64_late", o_20926, c_20926 },
    { 0x20957, "menu_need_any_clear_26_43",   o_20957, c_20957 },
    { 0x20A51, "menu_need_flags_clear_16_17", o_20A51, c_20A51 },
    { 0x20A87, "menu_need_flag_clear_1",      o_20A87, c_20A87 },
    { 0x20B14, "menu_need_flag_clear_16",     o_20B14, c_20B14 },
    { 0x20B3C, "menu_need_flags_clear_1_2",   o_20B3C, c_20B3C },
    { 0x3314B, "menu_show_reload",            o_3314B, c_3314B },
    { 0x33219, "menu_show_reload_pair",       o_33219, c_33219 },
    { 0x3332B, "menu_show_mark_two",          o_3332B, c_3332B },
    { 0x3346B, "menu_show_plain",             o_3346B, c_3346B },
    { 0x3347C, "menu_show_step20",            o_3347C, c_3347C },
    { 0x335A0, "menu_show_plain",             o_335A0, c_335A0 },
    { 0x335AA, "menu_show_or_rebuild_sprites", o_335AA, c_335AA },
    { 0x33674, "menu_show_plain",             o_33674, c_33674 },
    { 0x3367E, "menu_show_step16_then_clear", o_3367E, c_3367E },
    { 0x33AAE, "menu_show_step9_then_clear",  o_33AAE, c_33AAE },
    { 0x33169, "menu_rebuild_sprites_and_show",  o_33169, c_33169 },
    { 0x3327D, "menu_mark_records_and_show",    o_3327D, c_3327D },
    { 0x33367, "menu_rebuild_sprites_and_clear", o_33367, c_33367 },
    { 0x333F5, "menu_reset_and_show",           o_333F5, c_333F5 },
    { 0x334D9, "menu_show_gated_by_unit",       o_334D9, c_334D9 },
    { 0x335DA, "menu_step_pair_then_clear",     o_335DA, c_335DA },

    /* small scene/UI leaves (batch 45) */
    { 0x1C269, "rec_collect_slot_bits",   o_rec_slot_bits, c_rec_slot_bits },
    { 0x311E5, "anim_cycle_frame",        o_anim_cycle,    c_anim_cycle },
    { 0x1E0DB, "map_enqueue_status",      o_enq_status,    c_enq_status },
    { 0x233C6, "scene_place_records",     o_place_records, c_place_records },
    { 0x31BDF, "msg_show_lines",          o_show_lines,    c_show_lines },
    { 0x1E529, "msg_show_page",           o_show_page,     c_show_page },
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
    setvbuf(stdout, NULL, _IONBF, 0);
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
    HOOK(0x37AE5, stub_outp);
    HOOK(0x32999, stub_maprender);
    HOOK(0x1088D, stub_1088d);
    HOOK(0x12D7B, stub_glide);
    HOOK(0x1F525, stub_fadein);

    for (i = 0; i < NENT; i++) {
        if (!selected[i]) continue;
        if (getenv("EVCHECK_TRACE")) printf("== %s ==\n", g_entries[i].name);
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
