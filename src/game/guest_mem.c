/* guest_mem.c - see guest_mem.h.
 *
 * Two build modes, chosen by the target architecture, not by a runtime flag:
 *
 *   i386 (the game's architecture, -m32 / MSVC x86)
 *       call the game's Watcom CRT allocator at its fixed flat address.
 *       Check harnesses have already overwritten those entry points with
 *       jumps to the host libc, so this is "the host heap" in a check and
 *       "the game heap" in the host - exactly what each context needs.
 *
 *   everything else (the future native engine)
 *       ordinary malloc/free; there is no guest.
 *
 * The address constants match re/funcmap.csv (crt_sym: malloc @ 0x3706E,
 * free @ 0x3776E) and the redirection list in docs/rounds/03-tables-and-plumbing.md §25.2.
 */
#include "guest_mem.h"

#include <stdlib.h>

#if defined(__i386__) || defined(_M_IX86)

typedef void *(*guest_alloc_fn)(size_t);
typedef void  (*guest_free_fn)(void *);

#define GUEST_MALLOC_ADDR 0x0003706Eu
#define GUEST_FREE_ADDR   0x0003776Eu

void *guest_malloc(size_t n)
{
    guest_alloc_fn fn = (guest_alloc_fn)(uintptr_t)GUEST_MALLOC_ADDR;
    return fn(n);
}

void guest_free(void *p)
{
    guest_free_fn fn = (guest_free_fn)(uintptr_t)GUEST_FREE_ADDR;
    if (p)
        fn(p);
}

uint32_t guest_load_u32(uint32_t addr)
{
    return *(volatile uint32_t *)(uintptr_t)addr;
}

void guest_store_u32(uint32_t addr, uint32_t v)
{
    *(volatile uint32_t *)(uintptr_t)addr = v;
}

int guest_mem_is_guest(void)
{
    return 1;
}

#else /* native build: the guest is gone, the heap is just libc's */

void *guest_malloc(size_t n)
{
    return malloc(n);
}

void guest_free(void *p)
{
    free(p);
}

uint32_t guest_load_u32(uint32_t addr)
{
    (void)addr;
    return 0;
}

void guest_store_u32(uint32_t addr, uint32_t v)
{
    (void)addr;
    (void)v;
}

int guest_mem_is_guest(void)
{
    return 0;
}

#endif
