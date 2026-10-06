/* doscheck.c - cross-platform self-check for src/dos.c (round §45).
 *
 * Windows and Linux run the *same* expectations; that is the point of the
 * test: the INT 21h file services, the 0x70000 low-memory window, the BIOS
 * tick thread and the fault dispatch must behave identically behind the
 * platform seam. Four layers:
 *
 *   1. low memory   dos_init_lowmem() BDA/PSP bytes, dos_set_cmdtail,
 *                   the bios tick thread advancing 0x40:0x6C,
 *   2. platform     plat_reserve/commit/readable on a fixed page,
 *   3. INT 21h      create/read/write/lseek/truncate/attrs/delete through
 *                   dos_service() - including the two DOS-specific rules
 *                   that used to be Windows-only traps: a CX=0 write
 *                   truncates (§8-31) and CX:DX is one unsigned 32-bit
 *                   offset (the 0x002A:1CF3 seek, docs/rounds/01 §12),
 *   4. fault path   a real `int 0x21` executed here and serviced by the
 *                   platform fault entry (VEH on Windows, sigaction on
 *                   Linux) - registers and flags must come back.
 *
 * Build: build.ps1 -Target doscheck  /  make -f Makefile.linux doscheck-linux
 * Run:   build\doscheck.exe          /  build/doscheck-linux
 */
#include "dos.h"
#include "le.h"
#include "platform.h"
#include "host.h"        /* host_exit_after_remaining: stubbed below */

#include <stdio.h>
#include <string.h>

/* dos.c's AH=4B path asks the host how much exit budget is left; nothing in
 * this test spawns, so a "no budget" answer is enough. Not linking host.c
 * keeps this a pure dos.c+le.c+platform harness. */
int host_exit_after_remaining(void) { return -1; }

#define FA "doscheck_a.tmp"

/* dos_ctx is a *32-bit* register file - the guest is 32-bit, so every
 * pointer it carries must live below 4 GiB. On a 64-bit host the test's own
 * .rodata/stack addresses would be truncated when passed in EDX, so all
 * strings and buffers the services touch live in one fixed low page
 * (allocated in main, same rule as the game's own memory). */
#define SCR_BASE   0x30000000u
#define SCR_FA     (SCR_BASE + 0x100u)
#define SCR_MISS   (SCR_BASE + 0x140u)
#define SCR_DATA   (SCR_BASE + 0x200u)
#define SCR_XYZ    (SCR_BASE + 0x240u)
#define SCR_BUF    (SCR_BASE + 0x300u)
#define SCR_HELLO  (SCR_BASE + 0x400u)
#define SCR_CODE   (SCR_BASE + 0x800u)   /* the int-0x21 stub (layer 4)    */
#define SCR_RES    (SCR_BASE + 0x8C0u)   /* stub results: eax, ebx, eflags  */
static uint8_t *g_scratch;

static int g_pass, g_fail;

static void ck(int ok, const char *what)
{
    printf("  %-62s %s\n", what, ok ? "ok" : "**FAIL**");
    if (ok) g_pass++;
    else    g_fail++;
}

/* Build an INT 21h frame: AX = AH:AL (or the full 16-bit function word). */
static dos_ctx mk(uint32_t ax, uint32_t bx, uint32_t cx, uint32_t dx)
{
    dos_ctx c;

    memset(&c, 0, sizeof c);
    c.Eax = ax;
    c.Ebx = bx;
    c.Ecx = cx;
    c.Edx = dx;
    c.EFlags = 0x202;               /* IF=1, CF=0, like a real CPU entry */
    return c;
}

static int cf(const dos_ctx *c) { return (int)(c->EFlags & 1u); }
static unsigned ax(const dos_ctx *c) { return (unsigned)(c->Eax & 0xFFFFu); }

static dos_ctx svc21(dos_ctx c)
{
    dos_service(0x21, &c);
    return c;
}

/* ---- layer 1: low memory ------------------------------------------------ */

static void lowmem_tests(void)
{
    const uint8_t *lm = (const uint8_t *)(uintptr_t)DOS_LOWMEM_BASE;
    uint32_t tick;

    ck(lm[0x449] == 0x03 && lm[0x44A] == 80,
       "lowmem: BDA video mode 03h / 80 columns");
    ck(lm[0x41A] == 0x1E && lm[0x41B] == 0x00 && lm[0x41C] == 0x1E,
       "lowmem: BDA keyboard ring head == tail (empty)");
    ck(*(const uint16_t *)(lm + 0x2C) == (uint16_t)DOS_LOWMEM_SEG,
       "lowmem: IVT[0x0B] real-mode segment points at the mirror");

    dos_set_cmdtail("CHK42");
    ck(lm[0x80] == 5 && memcmp(lm + 0x81, "CHK42", 5) == 0 &&
       lm[0x81 + 5] == 0x0D,
       "lowmem: PSP:0x80 command tail = len + 'CHK42' + CR");

    /* The bios tick thread (started by dos_install_traps) must be alive:
     * 18.2 Hz = one increment every ~55 ms. */
    plat_sleep_ms(200);
    tick = *(const uint32_t *)(lm + 0x46C);
    printf("     (0x40:0x6C tick after 200 ms = %u)\n", tick);
    ck(tick >= 2, "bios tick thread advances 0x40:0x6C (>=2 in 200 ms)");
}

/* ---- layer 2: the platform memory probe --------------------------------- */

static void plat_tests(void)
{
    char buf[8] = { 0 };
    void *p = plat_reserve(0x20000000u, 0x1000);

    ck(p == (void *)(uintptr_t)0x20000000u, "plat_reserve takes a fixed address");
    ck(!plat_readable(p, 16), "reserved (PROT_NONE) page is not readable");
    ck(plat_commit((uintptr_t)p, 0x1000, PLAT_PROT_RW) != NULL,
       "plat_commit makes it accessible");
    ck(plat_readable(p, 16), "committed page is readable");
    ck(!plat_readable(NULL, 1), "NULL is not readable");
    ck(plat_readable(buf, sizeof buf), "host buffer is readable");
    plat_release((uintptr_t)p, 0x1000);
    ck(!plat_readable(p, 16), "released page is not readable");
}

/* ---- layer 3: INT 21h file services ------------------------------------- */

static void file_tests(void)
{
    dos_ctx c;
    uint8_t *buf = g_scratch + (SCR_BUF - SCR_BASE);
    char *fa     = (char *)(uintptr_t)SCR_FA;
    char *miss   = (char *)(uintptr_t)SCR_MISS;
    uint32_t h;

    memcpy(g_scratch + (SCR_DATA - SCR_BASE), "HELLO, WORLD!", 13);
    memcpy(g_scratch + (SCR_XYZ - SCR_BASE), "XYZ", 3);
    memcpy(g_scratch + (SCR_HELLO - SCR_BASE), "hello", 6);
    memcpy(fa, FA, sizeof FA);
    memcpy(miss, "doscheck_missing.tmp", sizeof "doscheck_missing.tmp");

    plat_file_delete(FA);                     /* leftovers from a failed run */

    c = svc21(mk(0x3C00, 0, 0, SCR_FA));
    h = c.Eax & 0xFFFFu;
    ck(!cf(&c), "AH=3C create: CF clear");
    ck(h == 5, "AH=3C create: first free handle == 5");

    c = svc21(mk(0x4000, h, 13, SCR_DATA));
    ck(!cf(&c) && ax(&c) == 13, "AH=40 write 13: CF clear, AX=13");

    c = svc21(mk(0x4200, h, 0, 0));           /* AL=0 SET, CX:DX = 0 */
    ck(!cf(&c) && c.Eax == 0, "AH=42 SET 0: EAX=0");

    memset(buf, 0, 0x40);
    c = svc21(mk(0x3F00, h, 13, SCR_BUF));
    ck(!cf(&c) && ax(&c) == 13 &&
       memcmp(buf, "HELLO, WORLD!", 13) == 0,
       "AH=3F read 13 after seek 0: bytes match");

    /* CX:DX = 0x002A:0x1CF3 must be ONE 32-bit offset (the old code passed
     * CX as SetFilePointer's high half and landed at ~171 GB). */
    c = svc21(mk(0x4200, h, 0x002A, 0x1CF3));
    ck(!cf(&c) && c.Eax == 0x002A1CF3u && (c.Edx & 0xFFFFu) == 0x002A,
       "AH=42 seek CX:DX=002A:1CF3 -> EAX=002A1CF3, DX=002A");

    memset(buf, 0xAA, 0x40);
    c = svc21(mk(0x3F00, h, 13, SCR_BUF));
    ck(!cf(&c) && c.Eax == 0, "AH=3F read past EOF: CF clear, AX=0");

    c = svc21(mk(0x4200, h, 0, 5));           /* back into the middle */
    ck(!cf(&c) && c.Eax == 5, "AH=42 SET 5: EAX=5");
    c = svc21(mk(0x4000, h, 3, SCR_XYZ));
    ck(!cf(&c) && ax(&c) == 3, "AH=40 overwrite at 5: AX=3");

    c = svc21(mk(0x4202, h, 0, 0));           /* AL=2 END */
    ck(!cf(&c) && c.Eax == 13,
       "AH=42 END after mid-file write: size still 13 (pwrite, not append)");

    c = svc21(mk(0x4200, h, 0, 8));           /* position 8 */
    ck(!cf(&c) && c.Eax == 8, "AH=42 SET 8: EAX=8");
    c = svc21(mk(0x4000, h, 0, 0));           /* CX=0 write = truncate */
    ck(!cf(&c) && c.Eax == 0, "AH=40 with CX=0: CF clear, AX=0");

    c = svc21(mk(0x4202, h, 0, 0));           /* how big is it now? */
    ck(!cf(&c) && c.Eax == 8,
       "AH=40 CX=0 truncated to position 8 (size == 8, PITFALLS §8-31)");

    c = svc21(mk(0x4300, 0, 0, SCR_FA));
    ck(!cf(&c) && ((c.Eax & 0xFFu) & 0x20u),
       "AH=43 get attributes: CF clear, archive bit set");

    c = svc21(mk(0x4301, 0, 0x0000u, SCR_FA));
    ck(!cf(&c) && (c.Eax & 0xFFu) == 0, "AH=43 set attributes: CF clear, AL=0");

    c = svc21(mk(0x3E00, h, 0, 0));
    ck(!cf(&c), "AH=3E close: CF clear");

    memset(buf, 0, 0x40);
    c = svc21(mk(0x3F00, h, 8, SCR_BUF));
    ck(!cf(&c) && c.Eax == 0, "AH=3F read on closed handle: CF clear, AX=0");

    c = svc21(mk(0x3D00, 0, 0, SCR_FA));      /* reopen */
    h = c.Eax & 0xFFFFu;
    ck(!cf(&c) && h == 5, "AH=3D reopen: CF clear, handle 5 again");
    c = svc21(mk(0x3F00, h, 8, SCR_BUF));
    ck(!cf(&c) && ax(&c) == 8 && memcmp(buf, "HELLOXYZ", 8) == 0,
       "AH=3F read 8 after reopen: 'HELLOXYZ' persisted");
    c = svc21(mk(0x3E00, h, 0, 0));
    ck(!cf(&c), "AH=3E close again: CF clear");

    c = svc21(mk(0x3C00, 0, 0, SCR_FA));      /* truncate existing */
    h = c.Eax & 0xFFFFu;
    ck(!cf(&c) && h == 5, "AH=3C on existing file: CF clear, handle 5");
    c = svc21(mk(0x4202, h, 0, 0));
    ck(!cf(&c) && c.Eax == 0, "AH=3C truncated the existing file (size == 0)");
    c = svc21(mk(0x3E00, h, 0, 0));
    ck(!cf(&c), "AH=3E close (third handle): CF clear");

    c = svc21(mk(0x3D00, 0, 0, SCR_MISS));
    ck(cf(&c) && ax(&c) == 2, "AH=3D missing file: CF set, AX=2 (ENOENT/FILE_NOT_FOUND)");

    c = svc21(mk(0x4300, 0, 0, SCR_MISS));
    ck(cf(&c) && ax(&c) == 2, "AH=43 missing file: CF set, AX=2");

    c = svc21(mk(0x4100, 0, 0, SCR_FA));
    ck(!cf(&c), "AH=41 delete existing: CF clear");
    c = svc21(mk(0x4100, 0, 0, SCR_FA));
    ck(cf(&c) && ax(&c) == 2, "AH=41 delete again: CF set, AX=2");

    c = svc21(mk(0x4000, 1, 6, SCR_HELLO));
    ck(!cf(&c) && ax(&c) == 6,
       "AH=40 write to handle 1 (stdout): CF clear, AX=6");
}

static void date_tests(void)
{
    dos_ctx c;

    c = svc21(mk(0x2A00, 0, 0, 0));           /* get date */
    ck(!cf(&c) && ((c.Ecx >> 8) & 0xFFFFu) >= 2020u &&
       ((c.Ecx >> 8) & 0xFFFFu) <= 2100u && (c.Ecx & 0xFFu) >= 1u &&
       (c.Ecx & 0xFFu) <= 12u && (c.Eax & 0xFFu) <= 6u,
       "AH=2A get date: CF clear, year 2020..2100, month 1..12, weekday 0..6");

    c = svc21(mk(0x2C00, 0, 0, 0));           /* get time */
    ck(!cf(&c) && (c.Ecx & 0xFFu) <= 59u && (c.Ecx >> 8) <= 23u,
       "AH=2C get time: CF clear, hour 0..23, minute 0..59");
}

/* ---- layer 4: a real fault through the platform entry -------------------
 *
 * The `int 0x21` is executed from a stub *in the low scratch page*, not from
 * this function's own text: dos_ctx carries a 32-bit EIP (the guest is
 * 32-bit), so on a 64-bit host a faulting RIP above 4 GiB would be truncated
 * and the core could not decode the `CD 21` - the same rule the game lives
 * by. The stub is hand-assembled, valid in both 32-bit and long mode, keeps
 * the callee-saved rbx/ebx, stores EAX/EBX/EFLAGS into the scratch page and
 * returns; the imm32 is patched per call. */

static void build_int_stub(void)
{
    static const uint32_t res[3] = { SCR_RES + 0, SCR_RES + 4, SCR_RES + 8 };
    uint8_t *p = g_scratch + (SCR_CODE - SCR_BASE);
    size_t i = 0;
    int k;

    p[i++] = 0xB8; i += 4;                  /* mov eax, imm32  (patched)  */
    p[i++] = 0x53;                          /* push rbx/ebx FIRST: the
                                             * host keeps its call target in
                                             * rbx, so saving after the xor
                                             * below would save 0 and the
                                             * `ret` would hand the C caller
                                             * a zeroed callee-saved register
                                             * (it then did `call *%rbx` ->
                                             * EIP=0 on the second call)   */
    p[i++] = 0x31; p[i++] = 0xDB;           /* xor ebx, ebx (clean EBX for
                                             * the service)                */
    p[i++] = 0xCD; p[i++] = 0x21;           /* int 0x21                  */
    {
        /* mov [abs32], eax ; mov [abs32], ebx */
        static const uint8_t modrm[2] = { 0x04, 0x1C };
        for (k = 0; k < 2; k++) {
            p[i++] = 0x89;
            p[i++] = modrm[k];
            p[i++] = 0x25;                  /* SIB: absolute              */
            memcpy(p + i, &res[k], 4); i += 4;
        }
    }
    p[i++] = 0x9C;                          /* pushf(d/q)  (captures CF)  */
    p[i++] = 0x59;                          /* pop rcx/ecx                */
    p[i++] = 0x89; p[i++] = 0x0C; p[i++] = 0x25;
    memcpy(p + i, &res[2], 4); i += 4;      /* mov [eflags], ecx          */
    p[i++] = 0x5B;                          /* pop rbx/ebx                */
    p[i++] = 0xC3;                          /* ret                        */
    printf("     (int stub: %u bytes at 0x%08X)\n", (unsigned)i, SCR_CODE);
}

static void do_int21(uint32_t ax_val, uint32_t *out_eax, uint32_t *out_ebx,
                     uint32_t *out_efl)
{
    uint8_t *code = g_scratch + (SCR_CODE - SCR_BASE);
    volatile uint32_t *res = (volatile uint32_t *)(uintptr_t)SCR_RES;

    code[1] = (uint8_t)(ax_val);
    code[2] = (uint8_t)(ax_val >> 8);
    code[3] = (uint8_t)(ax_val >> 16);
    code[4] = (uint8_t)(ax_val >> 24);
    res[0] = res[1] = res[2] = 0xDEADBEEFu;
    ((void (*)(void))(uintptr_t)SCR_CODE)();
    *out_eax = res[0];
    *out_ebx = res[1];
    *out_efl = res[2];
}

static void fault_tests(void)
{
    uint32_t eax = 0, ebx = 0, efl = 0;

    do_int21(0x3000, &eax, &ebx, &efl);
    ck(eax == 0x42431606u,
       "int 0x21 (AH=30): serviced by the fault handler, EAX=0x42431606");

    do_int21(0x6200, &eax, &ebx, &efl);
    ck((ebx & 0xFFFFu) == DOS_LOWMEM_SEG,
       "int 0x21 (AH=62): EBX = PSP segment from the mirror");
    ck((efl & 1u) == 0,
       "int 0x21: CF written back through the register frame");

    printf("     (last int: EAX=%08X EBX=%08X EFLAGS=%08X)\n", eax, ebx, efl);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("== doscheck: INT 21h services, low memory, fault dispatch ==\n");

    ck(le_reserve_address_space_early() == 0,
       "reserve guest window 0x10000..0x100000");

    g_scratch = plat_reserve(SCR_BASE, 0x1000);
    ck(g_scratch == (void *)(uintptr_t)SCR_BASE &&
       plat_commit(SCR_BASE, 0x1000, PLAT_PROT_RWX) != NULL,
       "scratch page at 0x30000000 (32-bit pointers for dos_ctx)");
    build_int_stub();

    dos_init_lowmem();                 /* + files_init() */
    ck(1, "dos_init_lowmem completed (0x70000 mirror mapped)");
    dos_install_traps();               /* fault entry + bios tick thread */
    ck(1, "dos_install_traps completed (fault handler + tick thread)");

    lowmem_tests();
    plat_tests();
    file_tests();
    date_tests();
    fault_tests();

    dos_dump_stats();
    printf("doscheck: %d/%d PASS%s\n", g_pass, g_pass + g_fail,
           g_fail ? "  ** FAILURES **" : "");
    return g_fail ? 1 : 0;
}
