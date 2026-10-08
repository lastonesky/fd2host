/* dos_fault_win.c - the Windows side of dos_fault.h: the VEH entry.
 *
 * probe4.c measured the model this wraps (src/probe4.c): from ring 3,
 * `int NN` raises EXCEPTION_ACCESS_VIOLATION and `int 3` raises
 * EXCEPTION_BREAKPOINT, both with ContextRecord->Eip pointing *at* the
 * instruction; privileged instructions raise EXCEPTION_PRIV_INSTRUCTION;
 * the TF trace raises EXCEPTION_SINGLE_STEP. This file is a mechanical
 * CONTEXT <-> dos_ctx copy plus that code -> FD2_FAULT_* mapping; all
 * decisions stay in dos_fault_core() (dos.c) so both platforms run the
 * identical service logic.
 */
#include "dos_fault.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

static void ctx_from_context(dos_ctx *c, const CONTEXT *x)
{
    c->Eax = x->Eax;    c->Ebx = x->Ebx;
    c->Ecx = x->Ecx;    c->Edx = x->Edx;
    c->Esi = x->Esi;    c->Edi = x->Edi;
    c->Ebp = x->Ebp;    c->Esp = x->Esp;
    c->Eip = x->Eip;    c->EFlags = x->EFlags;
    c->SegCs = (uint16_t)x->SegCs; c->SegDs = (uint16_t)x->SegDs;
    c->SegEs = (uint16_t)x->SegEs; c->SegFs = (uint16_t)x->SegFs;
    c->SegGs = (uint16_t)x->SegGs; c->SegSs = (uint16_t)x->SegSs;
}

static void ctx_to_context(CONTEXT *x, const dos_ctx *c)
{
    x->Eax = c->Eax;    x->Ebx = c->Ebx;
    x->Ecx = c->Ecx;    x->Edx = c->Edx;
    x->Esi = c->Esi;    x->Edi = c->Edi;
    x->Ebp = c->Ebp;    x->Esp = c->Esp;
    x->Eip = c->Eip;    x->EFlags = c->EFlags;
    x->SegCs = c->SegCs; x->SegDs = c->SegDs;
    x->SegEs = c->SegEs; x->SegFs = c->SegFs;
    x->SegGs = c->SegGs; x->SegSs = c->SegSs;
}

static void map_fault(fd2_fault *f, const EXCEPTION_RECORD *er)
{
    /* veh_record() always printed ExceptionInformation[0], so keep feeding
     * `info` unconditionally - for exceptions without extended information
     * that word simply holds whatever the record had, as before. */
    f->info = (uint32_t)er->ExceptionInformation[0];
    f->code = (uint32_t)er->ExceptionCode;
    f->has_addr = 0;
    f->addr = 0;
    f->access = 0;

    switch (er->ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
        f->kind = FD2_FAULT_ACCESS;
        f->has_addr = 1;
        f->addr = (uintptr_t)er->ExceptionInformation[1];
        f->access = (int)er->ExceptionInformation[0];   /* 0 r / 1 w / 8 fetch */
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        f->kind = FD2_FAULT_PRIV;
        break;
    case EXCEPTION_BREAKPOINT:
        f->kind = FD2_FAULT_BREAK;
        break;
    case EXCEPTION_SINGLE_STEP:
        f->kind = FD2_FAULT_STEP;
        break;
    default:
        /* illegal instruction, int divide-by-zero, ... - the core's final
         * block reports them and, when the EIP is not the guest's, passes
         * them on (MSVC/D3D11 thread-name exceptions, §8-34). */
        f->kind = FD2_FAULT_OTHER;
        break;
    }
}

static LONG CALLBACK fd2_veh(EXCEPTION_POINTERS *ep)
{
    dos_ctx c;
    fd2_fault f;
    int exit_code = 0;
    fd2_action a;

    ctx_from_context(&c, ep->ContextRecord);
    map_fault(&f, ep->ExceptionRecord);
    a = dos_fault_core(&c, &f, &exit_code);

    if (a == FD2_ACT_EXIT)
        ExitProcess((UINT)exit_code);       /* the old dos.c crash paths */
    if (a == FD2_ACT_SEARCH)
        return EXCEPTION_CONTINUE_SEARCH;
    ctx_to_context(ep->ContextRecord, &c);
    return EXCEPTION_CONTINUE_EXECUTION;
}

void dos_fault_install(void)
{
    if (!AddVectoredExceptionHandler(1, fd2_veh)) {
        fprintf(stderr, "dos: AddVectoredExceptionHandler failed: %lu\n",
                (unsigned long)GetLastError());
    } else {
        printf("dos: vectored exception handler installed\n");
    }
}
