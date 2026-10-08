/* dos_fault.h - the boundary between "how a fault arrives" and "what it means".
 *
 * dos.c's fault core (former fd2_veh body) is OS-independent: it takes a
 * portable description of the fault plus the guest register frame and
 * decides to continue / pass it on / exit. The two platform files translate
 * their native frame into that description:
 *
 *   dos_fault_win.c   AddVectoredExceptionHandler, EXCEPTION_POINTERS
 *   dos_fault_posix.c sigaction(SA_ONSTACK|SA_SIGINFO), siginfo_t+ucontext_t
 *
 * The mapping itself is measured, not guessed (docs/rounds/15-* and the two
 * probes):
 *
 *   event            Windows                       Linux (i386 compat, -m32)
 *   ---------------  ----------------------------  -----------------------------
 *   `int NN`         ACCESS_VIOLATION, EIP at CD   SIGSEGV si_code=SI_KERNEL(128),
 *                                                  si_addr=NULL, EIP at CD
 *   privileged insn  PRIV_INSTRUCTION, EIP at it   SIGSEGV SI_KERNEL (same as
 *                                                  above - the core decodes the
 *                                                  instruction to tell them apart)
 *   segment load     ACCESS_VIOLATION              SIGSEGV SI_KERNEL
 *   int 3 / CD 03    BREAKPOINT, EIP *at* the byte SIGTRAP  si_code=128, EIP
 *                                                  *past* the bytes -> wrapper
 *                                                  moves EIP back (parity)
 *   TF single-step   SINGLE_STEP                   SIGTRAP si_code=TRAP_TRACE(2)
 *   page fault       ACCESS_VIOLATION + address   SIGSEGV si_code=1/2 + si_addr
 *   illegal/div0     ILLEGAL_INSTRUCTION / ...     SIGILL / SIGFPE / SIGBUS
 *
 * Two Linux facts drive the design (PITFALLS §8-61):
 *   - SIGSEGV SI_KERNEL carries NO fault address (si_addr=NULL): it means
 *     "int/privileged/segment", so the core must decode the instruction at
 *     EIP instead of trusting a per-cause code; feeding 0 into the
 *     "fault < 0x10000 operand rewrite" rule would corrupt code.
 *   - si_addr is only a data address for si_code SEGV_MAPERR/ACCERR.
 */
#ifndef FD2_DOS_FAULT_H
#define FD2_DOS_FAULT_H

#include <stdint.h>
#include "dos.h"

#define FD2_FAULT_STEP    1   /* TF single-step                          */
#define FD2_FAULT_BREAK   2   /* int 3                                   */
#define FD2_FAULT_ACCESS  3   /* memory/segment/privileged access fault  */
#define FD2_FAULT_PRIV    4   /* privileged instruction (Windows only;   */
                             /* Linux folds it into FD2_FAULT_ACCESS)   */
#define FD2_FAULT_OTHER   5   /* anything else (SIGILL/FPE/...)          */

typedef struct fd2_fault {
    int       kind;      /* FD2_FAULT_*                                  */
    int       has_addr;  /* 1 = addr is the faulting *data* address      */
    uintptr_t addr;      /* only valid when has_addr (Linux SI_KERNEL gives none) */
    int       access;    /* 0 = read, 1 = write, 8 = instruction fetch   */
    uint32_t  code;      /* raw platform code for the event ring         */
    uint32_t  info;      /* raw platform detail for the event ring       */
} fd2_fault;

typedef enum fd2_action {
    FD2_ACT_CONTINUE,    /* frame updated: resume the guest              */
    FD2_ACT_SEARCH,      /* not ours: let the platform's default happen  */
    FD2_ACT_EXIT         /* crash report done: stop the process          */
} fd2_action;

/* The portable fault core (dos.c). `exit_code` is set for FD2_ACT_EXIT. */
fd2_action dos_fault_core(dos_ctx *c, const fd2_fault *f, int *exit_code);

/* Install the platform fault entry (the one thing that differs). */
void dos_fault_install(void);

#endif /* FD2_DOS_FAULT_H */
