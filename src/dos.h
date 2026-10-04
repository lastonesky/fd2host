/* dos.h - DOS / DPMI / BIOS environment emulation for the FD2 native host.
 *
 * The game's 32-bit DOS/4GW code is executed natively in this 32-bit process.
 * Everything the original called through software interrupts or port I/O is
 * trapped here and serviced with Win32 facilities. No real-mode emulation and
 * no DOS emulator is involved: only the handful of services the game actually
 * uses are reimplemented. */

#ifndef FD2_DOS_H
#define FD2_DOS_H

#include <windows.h>
#include <stdint.h>
#include "le.h"

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

/* Software interrupts are *not* patched: `int NN` faults in ring 3 and the
 * VEH reads the vector from the faulting instruction (see src/dos.c). This
 * entry point only reports that the strategy is in effect; returns 0. */
int  dos_patch_interrupts(void);

/* Low-memory absolute references inside the code (0x400..0x4FF) are redirected
 * into the low-memory window. Returns the number of patched operands. */
int  dos_patch_lowmem_refs(void);

/* Installs the vectored exception handler that services the trapped
 * interrupts and privileged instructions. */
void dos_install_traps(void);

void dos_init_lowmem(void);

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
