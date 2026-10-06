/* platform_posix.c - the Linux side of platform.h.
 *
 * The interesting differences from the Win32 backend, because the loader's
 * logic was written against Windows' model:
 *
 *   reserve vs commit   Windows has both; Linux only has "map it". A
 *                       PROT_NONE mapping is the closest thing to a
 *                       reservation: the address is taken, nothing may use
 *                       it, and mprotect() later turns it into the real
 *                       protection (= commit).
 *   occupying an address VirtualAlloc fails when somebody else is already
 *                       there; mmap(MAP_FIXED) would silently *replace* them.
 *                       MAP_FIXED_NOREPLACE (Linux >= 4.17) fails instead,
 *                       which is what every caller here expects.
 *   low addresses       mmap_min_addr is 65536 = 0x10000 on Debian, exactly
 *                       the guest window's first address, so the object
 *                       window is mappable - and unlike Windows' loader,
 *                       nothing in the process' own init maps into
 *                       0x10000..0x100000, so the §8-48 race cannot happen.
 *   regions             /proc/self/maps merges adjacent runs with equal
 *                       protection, so a "region" here can be bigger than
 *                       Windows' - the caller only ever commits *within* one,
 *                       so the walk just makes fewer steps.
 */
#include "platform.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#ifndef MAP_FIXED_NOREPLACE
/* Linux < 4.17. Callers only reach mmap for addresses plat_query() reported
 * as free, so MAP_FIXED cannot clobber anything here in practice - but if you
 * port this to an older kernel, re-check that assumption. */
#define MAP_FIXED_NOREPLACE MAP_FIXED
#endif

/* One /proc/self/maps line. */
typedef struct {
    uintptr_t start;
    uintptr_t end;
    char     perms[8];
    char     path[256];
} map_ent;

/* Find the entry covering `addr`. Returns 1 when there is one, 0 when the
 * address is unoccupied (or the file cannot be read). */
static int maps_lookup(uintptr_t addr, map_ent *e)
{
    FILE *f = fopen("/proc/self/maps", "r");
    char  line[640];

    if (!f)
        return 0;
    memset(e, 0, sizeof *e);
    while (fgets(line, sizeof line, f)) {
        unsigned long long a, b;
        char p[8];

        e->path[0] = 0;
        if (sscanf(line, "%llx-%llx %7s %*s %*s %*s %255[^\n]",
                   &a, &b, p, e->path) < 3)
            continue;
        if ((uintptr_t)a <= addr && addr < (uintptr_t)b) {
            e->start = (uintptr_t)a;
            e->end   = (uintptr_t)b;
            snprintf(e->perms, sizeof e->perms, "%s", p);
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

static int to_prot(unsigned prot)
{
    int p = PROT_NONE;

    if (prot & PLAT_PROT_R)
        p |= PROT_READ;
    if (prot & PLAT_PROT_W)
        p |= PROT_WRITE;
    if (prot & PLAT_PROT_X)
        p |= PROT_EXEC;
    return p;
}

static unsigned from_perms(const char *perms)
{
    unsigned p = 0;

    if (perms[0] == 'r') p |= PLAT_PROT_R;
    if (perms[1] == 'w') p |= PLAT_PROT_W;
    if (perms[2] == 'x') p |= PLAT_PROT_X;
    return p;
}

static void *mmap_fixed(uintptr_t addr, size_t len, int prot)
{
    void *p = mmap((void *)addr, len, prot,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
}

void *plat_reserve(uintptr_t addr, size_t len)
{
    return mmap_fixed(addr, len, PROT_NONE);
}

void *plat_commit(uintptr_t addr, size_t len, unsigned prot)
{
    map_ent e;

    if (maps_lookup(addr, &e)) {
        /* Occupied. We may only grant access to a mapping we could have made
         * ourselves: a file-backed one belongs to somebody else (Windows
         * would refuse MEM_COMMIT too), and a range that leaves our region
         * would need the neighbour to agree. Everything the loader reaches
         * here is one of its own anonymous blocks. */
        if (e.path[0] || e.end < addr + len) {
            errno = EADDRINUSE;
            return NULL;
        }
        return (mprotect((void *)addr, len, to_prot(prot)) == 0)
                   ? (void *)addr : NULL;
    }
    return mmap_fixed(addr, len, to_prot(prot));
}

void plat_release(uintptr_t addr, size_t len)
{
    munmap((void *)addr, len);
}

int plat_query(uintptr_t addr, plat_region *out)
{
    map_ent e;

    memset(out, 0, sizeof *out);
    if (!maps_lookup(addr, &e)) {
        out->base = addr;
        out->size = 0;
        out->is_free = 1;
        return -1;                       /* not occupied == free here       */
    }
    out->base    = e.start;
    out->size    = e.end - e.start;
    out->is_free = 0;
    out->prot    = from_perms(e.perms);
    return 0;
}

unsigned plat_error(void)
{
    return (unsigned)errno;
}

void plat_error_text(unsigned e, char *buf, size_t n)
{
    const char *t = strerror((int)e);

    if (t && t[0])
        snprintf(buf, n, "%s", t);
    else
        snprintf(buf, n, "error %u", (unsigned)e);
}

unsigned plat_data_selector(void)
{
#if defined(__x86_64__) || defined(__i386__)
    unsigned long long v = 0;

    /* What DS actually holds here, truncated to a selector: in long mode the
     * kernel may well leave it 0 (segment bases are ignored anyway), which is
     * NOT the 0x2B the Win32 build writes - so a 0x02 fixup (FDPS has exactly
     * one, FD2 has none) would relocate differently per OS. Unverified until
     * FDPS is run on Linux; see docs/rounds/13-portability.md. */
    __asm__ __volatile__("mov %%ds, %0" : "=a"(v));
    return (unsigned)(v & 0xFFFFu);
#else
    return 0;
#endif
}

void *plat_image_base(void)
{
    map_ent e;

    /* The mapping that holds this function: the PIE base (or the ELF text
     * base when the binary is not PIE). */
    if (maps_lookup((uintptr_t)(const void *)&plat_image_base, &e))
        return (void *)e.start;
    return NULL;
}

void plat_describe(uintptr_t addr, char *buf, size_t n)
{
    map_ent e;

    if (!maps_lookup(addr, &e)) {
        snprintf(buf, n, "0x%lX is FREE (no /proc/self/maps entry)",
                 (unsigned long)addr);
        return;
    }
    snprintf(buf, n, "0x%lX is %s %zX..%zX type=%s mapping: %s",
             (unsigned long)addr, e.perms, (size_t)e.start, (size_t)e.end,
             e.path[0] ? "MAPPED" : "PRIVATE",
             e.path[0] ? e.path : "<anonymous>");
}
