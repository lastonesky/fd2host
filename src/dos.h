/* dos.h - DOS / DPMI / BIOS environment emulation for the FD2 native host.
 *
 * The game's 32-bit DOS/4GW code is executed natively in this process.
 * Everything the original called through software interrupts or port I/O is
 * trapped here and serviced with host OS facilities behind src/platform.h
 * (the fault handler entry differs per OS: dos_fault_win.c / dos_fault_posix.c,
 * contract in dos_fault.h). No real-mode emulation and no DOS emulator is
 * involved: only the handful of services the game actually uses are
 * reimplemented. */

#ifndef FD2_DOS_H
#define FD2_DOS_H

#include <stdint.h>
#include "le.h"

/* The guest register frame as the services see it. The platform fault entry
 * fills it from the CPU's own frame (Win32 CONTEXT / Linux ucontext_t), the
 * services run against it, and the entry writes the result back before the
 * instruction stream continues. Field names follow the Win32 CONTEXT
 * spelling on purpose: int21()/int31()/... were written once and must read
 * identically on both platforms. */
typedef struct dos_ctx {
    uint32_t Eax, Ebx, Ecx, Edx, Esi, Edi, Ebp, Esp, Eip, EFlags;
    uint16_t SegCs, SegDs, SegEs, SegFs, SegGs, SegSs;
} dos_ctx;

/* Low-memory window that stands in for the first 64 KiB (which Windows will
 * not map in a user process). Holds the BIOS data area, the PSP and the
 * interrupt vector table image the game pokes at.
 *
 * The base is a *runtime* value, not a constant: FD2 parks its objects in
 * 0x10000..0x6FFFF so the mirror can live at 0x70000, but other titles of the
 * same family put an object there too (FDPS has obj2 at 0x70000, only 0x54
 * bytes). dos_choose_lowmem() slides the window above the game's objects -
 * every use goes through these macros, so call sites stay unchanged. */
extern uint32_t dos_lowmem_base;          /* chosen before dos_init_lowmem() */

#define DOS_LOWMEM_BASE   (dos_lowmem_base)
#define DOS_LOWMEM_SIZE   0x00010000u
#define DOS_LOWMEM_SEG    (DOS_LOWMEM_BASE >> 4)   /* e.g. 0x7000 */

/* Pick a window that clears the game's objects (paragraph aligned, below the
 * VGA window when possible). `game_end` = first address above all objects.
 * Call after the LE header is parsed and before dos_init_lowmem(). */
void dos_choose_lowmem(uint32_t game_end);

/* Software interrupts are *not* patched: `int NN` faults in ring 3 (Windows:
 * EXCEPTION_ACCESS_VIOLATION with EIP at the instruction; Linux: SIGSEGV /
 * si_code=SI_KERNEL, same EIP - measured by src/probe4.c and
 * src/faultprobe32.c) and the fault entry reads the vector from the
 * instruction (see src/dos.c). This entry point only reports that the
 * strategy is in effect; returns 0. */
int  dos_patch_interrupts(void);

/* Low-memory absolute references inside the code (0x400..0x4FF) are redirected
 * into the low-memory window. Returns the number of patched operands. */
int  dos_patch_lowmem_refs(void);

/* Installs the fault handler that services the trapped interrupts and
 * privileged instructions (VEH on Windows, sigaction+sigaltstack on Linux;
 * see dos_fault_win.c / dos_fault_posix.c), then starts the BIOS tick. */
void dos_install_traps(void);

/* Service one software interrupt on `c` - the switch dispatch_swint() calls
 * after the trap bookkeeping (EIP advance, trace, ring). doscheck also uses
 * it to drive INT 21h without executing a real `int` instruction. */
void dos_service(uint8_t vec, dos_ctx *c);

void dos_init_lowmem(void);

/* PSP:0x80 command tail for the guest (empty by default). Call after
 * dos_init_lowmem(); INT 21h AH=4B passes the parent's tail to the child. */
void dos_set_cmdtail(const char *tail);

/* Kill a process started by INT 21h AH=4B (watchdog / shutdown path). */
void dos_terminate_child(void);

/* One keystroke, both delivery paths:
 *   - always: the BIOS ring buffer at 0x41E (INT 16h / BDA polling games);
 *   - additionally, when the game installed its own INT 9 handler with
 *     INT 21h AH=25h AL=09h: queue the scan code; the fault handler injects it as a
 *     real interrupt frame on the guest thread at the next instruction
 *     boundary, with `in al,60h` returning `scan`. Best effort, like
 *     hardware: keys are dropped when the queue is full.
 * Returns 1 when the game's own handler owns the key queue (the BIOS ring
 * must then be left alone - the original does not chain to the BIOS), 0 when
 * the caller should fill the BIOS ring instead. */
int dos_deliver_key(uint8_t scan);

/* VGA DAC palette captured from the game's port writes (0x3C8/0x3C9). */
extern uint8_t  dos_palette[256 * 3];
extern volatile int dos_palette_dirty;

/* The VGA frame buffer lives at its original address. */
#define DOS_VGA_BASE  0x000A0000u
#define DOS_VGA_WIDTH 320u
#define DOS_VGA_HEIGHT 200u
#define DOS_VGA_SIZE  (DOS_VGA_WIDTH * DOS_VGA_HEIGHT)

void dos_set_image(le_image *le);

/* Single-step trace mode (diagnostics): logs the next N instructions. */
void dos_enable_trace(int instructions);

/* Statistics / diagnostics */
void dos_dump_stats(void);

/* Set when the game executes INT 21h AH=4Ch (DOS terminate). */
extern volatile int dos_exit_requested;
extern volatile int dos_exit_code;

#endif /* FD2_DOS_H */
