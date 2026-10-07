/* guest_mem.h - the one seam onto the game's heap.
 *
 * Why this exists: some translated modules hand buffers across the C / machine
 * code boundary. The game allocates with the Watcom CRT (`0x3706E malloc`) and
 * frees with the same heap (`0x3776E free`, and the `free` inside `0x15E71`),
 * so a C module that used the host's malloc/free would create two heaps and
 * corrupt memory the moment a pointer crossed over.
 *
 * On the 32-bit host these two functions call the *game's* allocator, so there
 * is exactly one heap. Check harnesses (rescheck/boxcheck) already redirect
 * those CRT entry points to the host libc, so the differential tests keep
 * working unchanged. On a native 64-bit build (no guest) they are simply
 * malloc/free - that is the whole point of keeping this a single seam:
 * docs/TRANSLATION.md §6 phase C swaps the implementation, not the callers.
 *
 * Precedent: src/game/dlg.c has been calling `0x3706E` directly since round 30;
 * that is the same rule, just not yet routed through this header.
 */
#ifndef FD2_GAME_GUEST_MEM_H
#define FD2_GAME_GUEST_MEM_H

#include <stddef.h>
#include <stdint.h>

/* Allocate/free on the game's heap (host libc on a native build). */
void *guest_malloc(size_t n);
void  guest_free(void *p);

/* Read/write a 32-bit game global (e.g. dword_53BFF). A translated module that
 * must publish a value the *game* reads has to write the guest address, not a
 * C variable. No-ops on a native build (no guest memory). */
uint32_t guest_load_u32(uint32_t addr);
void     guest_store_u32(uint32_t addr, uint32_t v);

/* 1 when a real guest image is present (32-bit host); 0 on a native build.
 * Callers rarely need this - it exists so tests can assert which mode ran. */
int guest_mem_is_guest(void);

#endif /* FD2_GAME_GUEST_MEM_H */
