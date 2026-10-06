/* platform.h - the OS seam, so the loader and the host can build on more
 * than one OS (docs/BACKEND.md §13.6 step 4).
 *
 * This is slice 1: **memory**. The full survey of Win32 usage across src/
 * (what still has to be extracted: threads/time in ~6 files, file services
 * and the VEH in dos.c, the keyboard translations in the entry layers) is in
 * docs/rounds/13-portability.md. New OS-specific code belongs behind a
 * function here, not behind an #include <windows.h> in a module.
 *
 * Why a seam instead of #ifdefs sprinkled around: every call site in le.c was
 * written against Windows' *three-state* model (free / reserved / committed)
 * and its region granularity, so the abstraction has to preserve those
 * semantics - a POSIX "just mmap it" translation silently clobbers whoever
 * owns the address (VirtualAlloc would have failed instead). The contract
 * below is therefore written in terms of what the caller needs to know:
 *
 *   plat_query   - what occupies this address right now (region + protection)
 *   plat_reserve - take the address, no access yet (nothing may use it)
 *   plat_commit  - make it accessible: reserve first if it is free, commit
 *                  into the existing reservation otherwise, fail if a
 *                  *foreign* mapping is in the way (as Windows does)
 *   plat_release - give the range back
 */
#ifndef FD2_PLATFORM_H
#define FD2_PLATFORM_H

#include <stdint.h>
#include <stddef.h>

#define PLAT_MAX_PATH 260     /* Windows MAX_PATH; plenty for our own paths */

/* Protection of a committed range: three independent access bits, so RWX is
 * R|W|X rather than a third exclusive state - POSIX maps them 1:1 onto
 * PROT_READ/WRITE/EXEC, Win32 has to translate combinations onto PAGE_*
 * (it has no write-without-read). le_commit_range() takes these. */
#define PLAT_PROT_R    0x1u
#define PLAT_PROT_W    0x2u
#define PLAT_PROT_X    0x4u
#define PLAT_PROT_RO   (PLAT_PROT_R)
#define PLAT_PROT_RW   (PLAT_PROT_R | PLAT_PROT_W)
#define PLAT_PROT_RWX  (PLAT_PROT_R | PLAT_PROT_W | PLAT_PROT_X)

typedef struct plat_region {
    uintptr_t base;      /* first address of the region                     */
    size_t    size;      /* its length                                      */
    int       is_free;   /* 1 = nothing occupies the queried address        */
    unsigned  prot;      /* PLAT_PROT_* (0 when is_free)                    */
} plat_region;

/* All three return NULL on failure; plat_error() then explains why. */
void *plat_reserve(uintptr_t addr, size_t len);
void *plat_commit(uintptr_t addr, size_t len, unsigned prot);
void  plat_release(uintptr_t addr, size_t len);

/* Describe the region containing `addr`. Returns 0 on success, -1 when the
 * address is not occupied (is_free = 1 is still a valid answer - callers
 * branch on it, they do not treat it as an error). */
int plat_query(uintptr_t addr, plat_region *out);

/* Why the last plat_* call failed: GetLastError() / errno. */
unsigned plat_error(void);
void     plat_error_text(unsigned e, char *buf, size_t n);

/* Value a "segment selector" fixup (LE type 0x02) must be patched with: in a
 * flat process the only sensible selector is our own flat data one - what a
 * real DOS/4GW loader would have handed out for that object (docs/PITFALLS.md
 * §8-49). x86 only; 0 elsewhere (no such fixup can be satisfied then). */
unsigned plat_data_selector(void);

/* Base address of this executable (GetModuleHandleA(NULL) / the PIE base). */
void *plat_image_base(void);

/* One line describing who owns `addr`, for failure reports. Windows keeps
 * the fields the old hand-rolled report used (`type=`, `prot=`, `region=`,
 * `mapping:` - docs/PITFALLS.md §8-48 quotes them), POSIX prints the
 * /proc/self/maps entry. */
void plat_describe(uintptr_t addr, char *buf, size_t n);

#endif /* FD2_PLATFORM_H */
