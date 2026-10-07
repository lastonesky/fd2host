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
#include <io.h>        /* _get_osfhandle: CRT fd -> OS handle */
#include <stdio.h>
#include <stdlib.h>
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
    /* MEM_RELEASE requires dwSize == 0 (the region must have been reserved
     * as a whole) - passing `len` here makes VirtualFree fail with
     * ERROR_INVALID_PARAMETER *silently*, so the pages stayed mapped: caught
     * by doscheck's "released page is not readable" check (round §45). */
    (void)len;
    VirtualFree((void *)addr, 0, MEM_RELEASE);
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

/* ------------------------------------------------------- slice 2: the OS
 * primitives dos.c services INT 21h with. The semantics are the ones the
 * interrupt handlers were written against (docs/rounds/01-*.md §12,
 * PITFALLS §8-31): position is owned by dos.c, these only move bytes. */

int plat_readable(const void *p, size_t n)
{
    MEMORY_BASIC_INFORMATION mbi;
    const char *b = (const char *)p;

    if (!p)
        return 0;
    if (!n)
        n = 1;
    if (VirtualQuery(b, &mbi, sizeof mbi) != sizeof mbi)
        return 0;
    if (mbi.State != MEM_COMMIT)
        return 0;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
        return 0;
    if (b + n <= (const char *)mbi.BaseAddress + mbi.RegionSize)
        return 1;
    /* crosses a region boundary: the last byte must be committed too */
    if (VirtualQuery(b + n - 1, &mbi, sizeof mbi) != sizeof mbi)
        return 0;
    return mbi.State == MEM_COMMIT &&
           !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
}

plat_file plat_console_file(int fd)
{
    intptr_t h = (fd >= 0) ? _get_osfhandle(fd) : -1;

    return (h == -1) ? PLAT_FILE_INVALID : (plat_file)h;
}

plat_file plat_file_open(const char *name)
{
    HANDLE h = CreateFileA(name, GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);

    return (h == INVALID_HANDLE_VALUE) ? PLAT_FILE_INVALID : (plat_file)h;
}

plat_file plat_file_create(const char *name)
{
    HANDLE h = CreateFileA(name, GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);

    return (h == INVALID_HANDLE_VALUE) ? PLAT_FILE_INVALID : (plat_file)h;
}

void plat_file_close(plat_file f)
{
    if (f != PLAT_FILE_INVALID)
        CloseHandle((HANDLE)f);
}

int plat_file_read_at(plat_file f, uint64_t off, void *buf, unsigned n,
                      unsigned *got)
{
    LARGE_INTEGER o;
    DWORD r = 0;

    *got = 0;
    o.QuadPart = (LONGLONG)off;      /* unsigned 32-bit DOS position */
    if (!SetFilePointerEx((HANDLE)f, o, NULL, FILE_BEGIN))
        return -1;
    if (!ReadFile((HANDLE)f, buf, n, &r, NULL))
        return -1;
    *got = r;
    return 0;
}

int plat_file_write_at(plat_file f, uint64_t off, const void *buf, unsigned n,
                       unsigned *wrote)
{
    LARGE_INTEGER o;
    DWORD r = 0;

    *wrote = 0;
    o.QuadPart = (LONGLONG)off;
    if (!SetFilePointerEx((HANDLE)f, o, NULL, FILE_BEGIN))
        return -1;
    if (!WriteFile((HANDLE)f, buf, n, &r, NULL))
        return -1;
    *wrote = r;
    return 0;
}

int plat_file_write_seq(plat_file f, const void *buf, unsigned n,
                        unsigned *wrote)
{
    DWORD r = 0;

    *wrote = 0;
    if (!WriteFile((HANDLE)f, buf, n, &r, NULL))
        return -1;
    *wrote = r;
    return 0;
}

int plat_file_truncate(plat_file f, uint64_t off)
{
    LARGE_INTEGER o;

    o.QuadPart = (LONGLONG)off;
    if (!SetFilePointerEx((HANDLE)f, o, NULL, FILE_BEGIN))
        return -1;
    return SetEndOfFile((HANDLE)f) ? 0 : -1;
}

int plat_file_size(plat_file f, uint64_t *size)
{
    LARGE_INTEGER s;

    if (!GetFileSizeEx((HANDLE)f, &s))
        return -1;
    *size = (uint64_t)s.QuadPart;
    return 0;
}

int plat_file_delete(const char *name)
{
    return DeleteFileA(name) ? 0 : -1;
}

int plat_file_attrs(const char *name, uint32_t *flags)
{
    DWORD a = GetFileAttributesA(name);
    uint32_t f = 0;

    if (a == INVALID_FILE_ATTRIBUTES)
        return -1;
    if (a & FILE_ATTRIBUTE_READONLY)  f |= PLAT_FILE_RDONLY;
    if (a & FILE_ATTRIBUTE_HIDDEN)    f |= PLAT_FILE_HIDDEN;
    if (a & FILE_ATTRIBUTE_SYSTEM)    f |= PLAT_FILE_SYSTEM;
    if (a & FILE_ATTRIBUTE_DIRECTORY) f |= PLAT_FILE_DIR;
    *flags = f;
    return 0;
}

int plat_file_set_attrs(const char *name, uint32_t flags)
{
    /* Exactly the mapping dos.c used to do inline (INT 21h AH=43 AL=01):
     * read-only *replaces* the normal attribute, hidden/system OR onto it. */
    DWORD a = (flags & PLAT_FILE_RDONLY) ? FILE_ATTRIBUTE_READONLY
                                         : FILE_ATTRIBUTE_NORMAL;

    if (flags & PLAT_FILE_HIDDEN) a |= FILE_ATTRIBUTE_HIDDEN;
    if (flags & PLAT_FILE_SYSTEM) a |= FILE_ATTRIBUTE_SYSTEM;
    return SetFileAttributesA(name, a) ? 0 : -1;
}

unsigned plat_error_to_dos(unsigned e)
{
    switch (e) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:      return 2;   /* file not found  */
    case ERROR_ACCESS_DENIED:       return 5;   /* access denied   */
    case ERROR_SHARING_VIOLATION:   return 5;
    case ERROR_INVALID_HANDLE:      return 6;   /* invalid handle  */
    case ERROR_TOO_MANY_OPEN_FILES: return 4;
    default:                        return 5;
    }
}

void plat_local_time(plat_time *t)
{
    SYSTEMTIME st;

    GetLocalTime(&st);
    t->year   = st.wYear;
    t->month  = st.wMonth;
    t->day    = st.wDay;
    t->weekday = st.wDayOfWeek;      /* 0 = Sunday */
    t->hour   = st.wHour;
    t->minute = st.wMinute;
    t->second = st.wSecond;
    t->ms     = st.wMilliseconds;
}

/* ---- threads ---------------------------------------------------------- */

struct plat_thread_req {
    void (*fn)(void *);
    void *arg;
};

static DWORD WINAPI plat_thread_thunk(LPVOID p)
{
    struct plat_thread_req r = *(struct plat_thread_req *)p;

    free(p);
    r.fn(r.arg);
    return 0;
}

int plat_thread(void (*fn)(void *), void *arg)
{
    return plat_thread_stk(fn, arg, 0);
}

int plat_thread_stk(void (*fn)(void *), void *arg, size_t stack)
{
    struct plat_thread_req *r = malloc(sizeof *r);
    HANDLE h;

    if (!r)
        return -1;
    r->fn = fn;
    r->arg = arg;
    h = CreateThread(NULL, (SIZE_T)stack, plat_thread_thunk, r, 0, NULL);
    if (!h) {
        free(r);
        return -1;
    }
    CloseHandle(h);                   /* detached: nobody joins it */
    return 0;
}

void plat_sleep_ms(unsigned ms)
{
    Sleep(ms);
}

uint64_t plat_now_ms(void)
{
    return (uint64_t)GetTickCount64();
}

int plat_set_cwd(const char *dir)
{
    return SetCurrentDirectoryA(dir) ? 0 : -1;
}

int plat_module_path(char *buf, size_t n)
{
    DWORD d = GetModuleFileNameA(NULL, buf, (DWORD)n);

    if (d == 0 || d >= (DWORD)n)
        return -1;                    /* truncated or failed */
    return 0;
}

int plat_path_size(const char *path, uint64_t *size)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(path, &fd);

    if (h == INVALID_HANDLE_VALUE)
        return -1;
    FindClose(h);                     /* metadata: fine while it is open for
                                       * writing (that is the point) */
    *size = ((uint64_t)fd.nFileSizeHigh << 32) | (uint64_t)fd.nFileSizeLow;
    return 0;
}

int plat_stricmp(const char *a, const char *b)
{
    return _stricmp(a, b);
}

char *plat_strdup(const char *s)
{
    return _strdup(s);
}

void plat_stdio_pin(void)
{
    _dup2(_fileno(stdout), 1);
    _dup2(_fileno(stderr), 2);
}

uint64_t plat_thread_id(void)
{
    return (uint64_t)GetCurrentThreadId();
}

void plat_exit(int code)
{
    ExitProcess((UINT)code);          /* the old dos.c crash paths */
}

void plat_mem_status(uint64_t *avail, uint64_t *span)
{
    MEMORYSTATUS ms;

    GlobalMemoryStatus(&ms);          /* verbatim from INT 31h AX=0500 */
    *avail = (uint64_t)ms.dwAvailPhys;
    *span  = (uint64_t)ms.dwAvailVirtual;
}

/* ---- exec (INT 21h AH=4B) --------------------------------------------- */

static HANDLE g_child;

int plat_exec_child(const plat_exec_req *r, int *exit_code)
{
    char self[PLAT_MAX_PATH] = "";
    char selfdir[PLAT_MAX_PATH] = "";
    char cwd[PLAT_MAX_PATH] = "";
    char child[PLAT_MAX_PATH] = "";
    char clog[PLAT_MAX_PATH] = "";
    char exitarg[40] = "";
    char cmdline[PLAT_MAX_PATH * 4 + 640];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    GetModuleFileNameA(NULL, self, PLAT_MAX_PATH);
    GetCurrentDirectoryA(PLAT_MAX_PATH, cwd);
    if (!GetFullPathNameA(r->guest_path, PLAT_MAX_PATH, child, NULL))
        strncpy(child, r->guest_path, sizeof child - 1)[sizeof child - 1] = 0;
    strncpy(selfdir, self, sizeof selfdir - 1);
    selfdir[sizeof selfdir - 1] = 0;
    {
        char *slash = strrchr(selfdir, '\\');
        if (slash) *slash = 0;
    }
    /* A child that freopen()s host.log would truncate the parent's log
     * ("w" mode), so every generation gets its own file. */
    snprintf(clog, sizeof clog, "%s\\host.%lu.log", selfdir,
             (unsigned long)GetCurrentProcessId());
    if (r->wait && r->exit_after > 0)
        snprintf(exitarg, sizeof exitarg, " --exit-after=%d", r->exit_after);
    snprintf(cmdline, sizeof cmdline,
             "\"%s\" --exe=\"%s\" --gamedir=\"%s\" --log=\"%s\" "
             "--cmdtail=\"%s\"%s",
             self, child, cwd, clog, r->cmdtail, exitarg);
    cmdline[sizeof cmdline - 1] = 0;

    printf("dos:   child: %s\n", cmdline);

    ZeroMemory(&si, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL,
                        &si, &pi)) {
        printf("dos:   CreateProcess failed (%lu)\n", GetLastError());
        return -1;
    }
    CloseHandle(pi.hThread);
    g_child = pi.hProcess;
    if (r->wait) {
        DWORD code = 0;

        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &code);
        *exit_code = (int)code;
        printf("dos:   child exited with %d\n", *exit_code);
    }
    CloseHandle(pi.hProcess);
    g_child = NULL;                   /* as before: only a waiting child exists */
    return 0;
}

int plat_child_present(void)
{
    return g_child != NULL;
}

void plat_child_kill(void)
{
    if (!g_child)
        return;
    TerminateProcess(g_child, 0);
    WaitForSingleObject(g_child, 2000);
    CloseHandle(g_child);
    g_child = NULL;
}
