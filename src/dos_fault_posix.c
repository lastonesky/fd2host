/* dos_fault_posix.c - the Linux side of dos_fault.h: the sigaction entry.
 *
 * src/faultprobe32.c measured the compat-mode model this wraps (freestanding
 * -m32 probe, 2026-10-07):
 *
 *   int NN            SIGSEGV, si_code=SI_KERNEL(128), si_addr=NULL,
 *                     EIP *at* the `CD` byte   (Windows: ACCESS_VIOLATION)
 *   in/out/cli/sti/   SIGSEGV, si_code=SI_KERNEL, si_addr=NULL, EIP at the
 *   hlt/lgdt          instruction              (Windows: PRIV_INSTRUCTION)
 *   mov ds,reg (bad)  SIGSEGV, si_code=SI_KERNEL (Windows: ACCESS_VIOLATION)
 *   int 3 / CD 03     SIGTRAP,  si_code=128, EIP *past* the instruction
 *                     -> adjusted back below so the core sees Windows' view
 *   TF single-step    SIGTRAP,  si_code=TRAP_TRACE(2), EIP = next insn
 *   page fault        SIGSEGV,  si_code=1 MAPERR / 2 ACCERR, si_addr = cr2
 *   ud2 / div0        SIGILL(2) / SIGFPE(1), si_addr = the instruction
 *
 * The three "SIGSEGV SI_KERNEL" cases are indistinguishable by signal, which
 * is why the core decodes the instruction at EIP instead of trusting the
 * cause (PITFALLS §8-61): `CD` -> software interrupt, privileged opcode ->
 * emulation, `8E`/other -> segment-load rules. `has_addr` separates the
 * remaining case (a real page fault always carries si_addr).
 *
 * The handler runs on a dedicated alt stack (SA_ONSTACK): a fault can arrive
 * with ESP inside the game's stack, and neither the sigframe nor the handler
 * depth may land on guest memory.
 */
#define _GNU_SOURCE   /* ucontext's REG_* enum and TRAP_* si_codes are
                       * glibc GNU extensions (measured: even -std=gnu11
                       * does not expose them without this) */
#include "dos_fault.h"
#include "platform.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>

/* ---- register frame <-> ucontext, per architecture --------------------- */

#if defined(__x86_64__)

static void ctx_from_uc(dos_ctx *c, ucontext_t *uc)
{
    c->Eax    = (uint32_t)uc->uc_mcontext.gregs[REG_RAX];
    c->Ebx    = (uint32_t)uc->uc_mcontext.gregs[REG_RBX];
    c->Ecx    = (uint32_t)uc->uc_mcontext.gregs[REG_RCX];
    c->Edx    = (uint32_t)uc->uc_mcontext.gregs[REG_RDX];
    c->Esi    = (uint32_t)uc->uc_mcontext.gregs[REG_RSI];
    c->Edi    = (uint32_t)uc->uc_mcontext.gregs[REG_RDI];
    c->Ebp    = (uint32_t)uc->uc_mcontext.gregs[REG_RBP];
    c->Esp    = (uint32_t)uc->uc_mcontext.gregs[REG_RSP];
    c->Eip    = (uint32_t)uc->uc_mcontext.gregs[REG_RIP];
    c->EFlags = (uint32_t)uc->uc_mcontext.gregs[REG_EFL];
    /* Long mode keeps the legacy selectors loaded (cs=0x33, ds/es/ss=0x2B,
     * verified by faultprobe64). glibc's x86-64 ucontext does not expose
     * ds/es, so the measured constants are used - this build is a compile +
     * smoke target only: the game itself needs -m32 (i386 frame below). */
    c->SegCs = 0x0033;
    c->SegDs = c->SegEs = c->SegSs = 0x002B;
    c->SegFs = c->SegGs = 0;
}

static void ctx_to_uc(const dos_ctx *c, ucontext_t *uc)
{
    /* Merge, do not replace: dos_ctx is 32-bit (the guest is 32-bit), while
     * the host's registers live at 64-bit addresses (RSP in the stack, RIP
     * in a PIE image). Writing the truncated value back would move the
     * stack/instruction pointer - measured the hard way: after the first
     * serviced `int`, RSP lost its 0x7fff_ prefix and the next push faulted.
     * On -m32 (the game's real build) the high half is zero and this is the
     * plain copy the Windows side does. */
#define MERGE64(reg, v) \
    uc->uc_mcontext.gregs[reg] = \
        ((uint64_t)uc->uc_mcontext.gregs[reg] & ~0xFFFFFFFFull) | (uint64_t)(v)
    MERGE64(REG_RAX, c->Eax);
    MERGE64(REG_RBX, c->Ebx);
    MERGE64(REG_RCX, c->Ecx);
    MERGE64(REG_RDX, c->Edx);
    MERGE64(REG_RSI, c->Esi);
    MERGE64(REG_RDI, c->Edi);
    MERGE64(REG_RBP, c->Ebp);
    MERGE64(REG_RSP, c->Esp);
    MERGE64(REG_RIP, c->Eip);
    MERGE64(REG_EFL, c->EFlags);
#undef MERGE64
}

/* Page-fault error code (read/write/fetch bits) when the kernel reports one. */
static int uc_err(ucontext_t *uc)
{
#ifdef REG_ERR
    return (int)uc->uc_mcontext.gregs[REG_ERR];
#else
    return 0;
#endif
}

#elif defined(__i386__)

/* glibc's i386 mcontext_t *is* the kernel's sigcontext (offsets verified by
 * faultprobe32: eip at ucontext+76, eflags at +84, ds at +32, err at +72). */
static void ctx_from_uc(dos_ctx *c, ucontext_t *uc)
{
    c->Eax    = (uint32_t)uc->uc_mcontext.eax;
    c->Ebx    = (uint32_t)uc->uc_mcontext.ebx;
    c->Ecx    = (uint32_t)uc->uc_mcontext.ecx;
    c->Edx    = (uint32_t)uc->uc_mcontext.edx;
    c->Esi    = (uint32_t)uc->uc_mcontext.esi;
    c->Edi    = (uint32_t)uc->uc_mcontext.edi;
    c->Ebp    = (uint32_t)uc->uc_mcontext.ebp;
    c->Esp    = (uint32_t)uc->uc_mcontext.esp;
    c->Eip    = (uint32_t)uc->uc_mcontext.eip;
    c->EFlags = (uint32_t)uc->uc_mcontext.eflags;
    c->SegCs  = (uint16_t)uc->uc_mcontext.cs;
    c->SegDs  = (uint16_t)uc->uc_mcontext.ds;
    c->SegEs  = (uint16_t)uc->uc_mcontext.es;
    c->SegFs  = (uint16_t)uc->uc_mcontext.fs;
    c->SegGs  = (uint16_t)uc->uc_mcontext.gs;
    c->SegSs  = (uint16_t)uc->uc_mcontext.ss;
}

static void ctx_to_uc(const dos_ctx *c, ucontext_t *uc)
{
    uc->uc_mcontext.eax    = c->Eax;
    uc->uc_mcontext.ebx    = c->Ebx;
    uc->uc_mcontext.ecx    = c->Ecx;
    uc->uc_mcontext.edx    = c->Edx;
    uc->uc_mcontext.esi    = c->Esi;
    uc->uc_mcontext.edi    = c->Edi;
    uc->uc_mcontext.ebp    = c->Ebp;
    uc->uc_mcontext.esp    = c->Esp;
    uc->uc_mcontext.eip    = c->Eip;
    uc->uc_mcontext.eflags = c->EFlags;
    /* Segment registers are not written back: in this host the guest runs
     * flat (ds/es/ss stay at the kernel's 0x2B, probe32-verified) and the
     * core's segment-load rule patches the instruction's *operand*, not the
     * register file. */
}

static int uc_err(ucontext_t *uc)
{
    return (int)uc->uc_mcontext.err;
}

#else
#error "dos_fault_posix.c: unsupported architecture"
#endif

/* ---- signal -> fd2_fault ------------------------------------------------ */

static void break_fix_eip(dos_ctx *c)
{
    /* Linux reports EIP *past* int3 / `CD 03` (faultprobe32), Windows at the
     * instruction (probe4). Move it back so the core decodes the same bytes
     * on both platforms - `CD 03` then dispatches as vector 3 like on
     * Windows, and a stray 0xCC falls into the same crash report. */
    const uint8_t *e = (const uint8_t *)(uintptr_t)c->Eip;

    if (e >= (const uint8_t *)2 &&
        plat_readable(e - 2, 2) && e[-2] == 0xCD && e[-1] == 0x03) {
        c->Eip -= 2;
        return;
    }
    if (e >= (const uint8_t *)1 && plat_readable(e - 1, 1) && e[-1] == 0xCC)
        c->Eip -= 1;
}

static void map_signal(fd2_fault *f, int sig, siginfo_t *si, int err)
{
    f->has_addr = 0;
    f->addr = 0;
    f->access = 0;
    /* One ring word for both raw values; the Windows side stores the
     * exception code there instead. Diagnostics only. */
    f->code = ((uint32_t)(unsigned)sig << 16) | ((uint32_t)si->si_code & 0xFFFFu);
    f->info = 0;

    switch (sig) {
    case SIGSEGV:
        f->kind = FD2_FAULT_ACCESS;
        if (si->si_code == SEGV_MAPERR || si->si_code == SEGV_ACCERR) {
            f->has_addr = 1;
            f->addr = (uintptr_t)si->si_addr;
        }
        /* SI_KERNEL (128): `int`/privileged/segment - no address. The core
         * decodes the instruction at EIP to tell them apart. */
        break;
    case SIGBUS:
        f->kind = FD2_FAULT_ACCESS;
        f->has_addr = 1;
        f->addr = (uintptr_t)si->si_addr;
        break;
    case SIGTRAP:
        if (si->si_code == TRAP_TRACE)
            f->kind = FD2_FAULT_STEP;        /* TF single-step            */
        else
            f->kind = FD2_FAULT_BREAK;       /* int 3 / 0xCC              */
        break;
    case SIGILL:
    case SIGFPE:
        f->kind = FD2_FAULT_OTHER;
        f->has_addr = 1;
        f->addr = (uintptr_t)si->si_addr;
        break;
    default:
        f->kind = FD2_FAULT_OTHER;
        break;
    }

    if (f->has_addr) {
        /* #PF error code: bit4 = instruction fetch, bit1 = write. */
        f->access = (err & 0x10) ? 8 : (err & 0x02) ? 1 : 0;
    }
}

static void fd2_sigaction(int sig, siginfo_t *si, void *ucv)
{
    ucontext_t *uc = (ucontext_t *)ucv;
    dos_ctx c;
    fd2_fault f;
    int exit_code = 0;
    fd2_action a;

    ctx_from_uc(&c, uc);
    map_signal(&f, sig, si, uc_err(uc));
    if (f.kind == FD2_FAULT_BREAK)
        break_fix_eip(&c);

    a = dos_fault_core(&c, &f, &exit_code);

    if (a == FD2_ACT_EXIT)
        plat_exit(exit_code);
    if (a == FD2_ACT_SEARCH) {
        /* "Not ours" (the faulting code is the host's, not the guest's):
         * restore the default action and return. The kernel then re-executes
         * the instruction, which faults again and kills the process - the
         * equivalent of Windows' EXCEPTION_CONTINUE_SEARCH reaching the
         * unhandled-exception path. */
        struct sigaction dfl;

        memset(&dfl, 0, sizeof dfl);
        dfl.sa_handler = SIG_DFL;
        sigemptyset(&dfl.sa_mask);
        sigaction(sig, &dfl, NULL);
        return;
    }
    ctx_to_uc(&c, uc);
}

void dos_fault_install(void)
{
    static int installed;
    struct sigaction sa;
    stack_t ss;
    size_t altsize = 64u * 1024u;

    if (installed)
        return;
    installed = 1;

    ss.ss_sp = mmap(NULL, altsize, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ss.ss_sp != MAP_FAILED) {
        ss.ss_size = altsize;
        ss.ss_flags = 0;
        if (sigaltstack(&ss, NULL) != 0)
            printf("dos: sigaltstack failed (%d) - handler runs on the "
                   "current stack\n", errno);
    } else {
        printf("dos: alternate signal stack alloc failed (%d) - handler runs "
               "on the current stack\n", errno);
    }

    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = fd2_sigaction;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGSEGV, &sa, NULL) != 0 ||
        sigaction(SIGILL,  &sa, NULL) != 0 ||
        sigaction(SIGTRAP, &sa, NULL) != 0 ||
        sigaction(SIGBUS,  &sa, NULL) != 0 ||
        sigaction(SIGFPE,  &sa, NULL) != 0) {
        fprintf(stderr, "dos: sigaction failed (%d)\n", errno);
        return;
    }
    printf("dos: sigaction fault handler installed "
           "(SIGSEGV/SIGILL/SIGTRAP/SIGBUS/SIGFPE, alt stack %zu KiB)\n",
           altsize / 1024u);
}
