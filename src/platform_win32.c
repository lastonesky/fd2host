/* platform_win32.c - the Windows side of platform.h.
 *
 * Thin wrappers over the kernel32 primitives le.c used directly. The
 * semantics are the ones the loader was written against:
 *
 *   VirtualQuery tells us the region (base/size/state/protection/type),
 *   VirtualAlloc(MEM_RESERVE) takes an address without making it usable,
 *   VirtualAlloc(MEM_COMMIT) makes an *existing* reservation usable - and
 *   refuses (487, ERROR_INVALID_ADDRESS) when the range spans more than one
 *   region, which is why le.c still walks region by region.
 */
#include "platform.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

static DWORD to_page(unsigned prot)
{
    int r = (prot & PLAT_PROT_R) != 0;
    int w = (prot & PLAT_PROT_W) != 0;
    int x = (prot & PLAT_PROT_X) != 0;

    if (x) {
        /* Win32 has no execute-without-read or write-only-execute page: the
         * closest grant is the one the loader always asked for. */
        if (w)
            return PAGE_EXECUTE_READWRITE;
        return r ? PAGE_EXECUTE_READ : PAGE_EXECUTE;
    }
    if (w)
        return PAGE_READWRITE;      /* write implies read in Win32 */
    return r ? PAGE_READONLY : PAGE_NOACCESS;
}

static unsigned from_protect(DWORD p)
{
    unsigned r = 0;

    switch (p) {
    case PAGE_READONLY:                          r = PLAT_PROT_R;   break;
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:                         r = PLAT_PROT_R | PLAT_PROT_W; break;
    case PAGE_EXECUTE:                           r = PLAT_PROT_X;   break;
    case PAGE_EXECUTE_READ:                      r = PLAT_PROT_X | PLAT_PROT_R; break;
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:                 r = PLAT_PROT_X | PLAT_PROT_R | PLAT_PROT_W; break;
    default:                                     r = 0;             break;
    }
    return r;
}

void *plat_reserve(uintptr_t addr, size_t len)
{
    return VirtualAlloc((void *)addr, len, MEM_RESERVE, PAGE_NOACCESS);
}

void *plat_commit(uintptr_t addr, size_t len, unsigned prot)
{
    MEMORY_BASIC_INFORMATION q;

    if (VirtualQuery((void *)addr, &q, sizeof q) == 0)
        return NULL;
    if (q.State == MEM_FREE)
        return VirtualAlloc((void *)addr, len, MEM_RESERVE | MEM_COMMIT,
                            to_page(prot));
    return VirtualAlloc((void *)addr, len, MEM_COMMIT, to_page(prot));
}

void plat_release(uintptr_t addr, size_t len)
{
    VirtualFree((void *)addr, len, MEM_RELEASE);
}

int plat_query(uintptr_t addr, plat_region *out)
{
    MEMORY_BASIC_INFORMATION q;

    memset(out, 0, sizeof *out);
    if (VirtualQuery((void *)addr, &q, sizeof q) == 0 || q.RegionSize == 0)
        return -1;
    out->base = (uintptr_t)q.BaseAddress;
    out->size = (size_t)q.RegionSize;
    if (q.State == MEM_FREE) {
        out->is_free = 1;
        out->prot = 0;
        return -1;                       /* same contract as the POSIX side */
    }
    out->is_free = 0;
    out->prot = from_protect(q.Protect);
    return 0;
}

unsigned plat_error(void)
{
    return (unsigned)GetLastError();
}

void plat_error_text(unsigned e, char *buf, size_t n)
{
    DWORD len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS,
                               NULL, e, 0, buf, (DWORD)n, NULL);
    while (len && (buf[len - 1] == '\r' || buf[len - 1] == '\n' ||
                   buf[len - 1] == ' '))
        buf[--len] = 0;
    if (!len)
        snprintf(buf, n, "error %u", (unsigned)e);
}

unsigned plat_data_selector(void)
{
    unsigned short sel = 0;

    __asm { mov sel, ds }
    return sel;
}

void *plat_image_base(void)
{
    return (void *)GetModuleHandleA(NULL);
}

/* kernel32 export (Win7+): names the file mapping behind an address. The
 * declaration lives here (not psapi.h) exactly as le.c had it. */
BOOL WINAPI K32GetMappedFileNameA(HANDLE, LPVOID, LPSTR, DWORD);

void plat_describe(uintptr_t addr, char *buf, size_t n)
{
    MEMORY_BASIC_INFORMATION q;
    char owner[PLAT_MAX_PATH];
    DWORD got;

    if (VirtualQuery((void *)addr, &q, sizeof q) == 0 || q.RegionSize == 0) {
        snprintf(buf, n, "0x%lX VirtualQuery failed (%u)", (unsigned long)addr,
                 (unsigned)plat_error());
        return;
    }
    owner[0] = 0;
    if (q.State != MEM_FREE) {
        got = K32GetMappedFileNameA(GetCurrentProcess(), q.AllocationBase,
                                    owner, PLAT_MAX_PATH);
        if (!got)
            snprintf(owner, sizeof owner, "<name unavailable, %u>",
                     (unsigned)plat_error());
    }
    snprintf(buf, n,
             "0x%lX is %s%s%s type=%s prot=0x%lX region=0x%zX base=0x%lX "
             "mapping: %s",
             (unsigned long)addr,
             (q.State & MEM_COMMIT)  ? "COMMIT " : "",
             (q.State & MEM_RESERVE) ? "RESERVE " : "",
             (q.State == MEM_FREE)   ? "FREE" : "",
             (q.Type == MEM_IMAGE)  ? "IMAGE" :
             (q.Type == MEM_MAPPED) ? "MAPPED" :
             (q.Type == MEM_PRIVATE) ? "PRIVATE" : "-",
             (unsigned long)((q.State == MEM_FREE) ? 0 : q.Protect),
             (size_t)q.RegionSize,
             (unsigned long)(uintptr_t)q.AllocationBase, owner);
}
