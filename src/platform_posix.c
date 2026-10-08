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
 *   slice 2 (files/threads/process) follows the same rule: pread/pwrite with
 *                       the caller's position, pthread threads, fork+execv.
 */
#define _GNU_SOURCE              /* process_vm_readv (plat_readable) */
#include "platform.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>    /* strcasecmp (plat_stricmp) */
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

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
    /* addr == 0 means "anywhere", exactly like VirtualAlloc(NULL): callers
     * (INT 21h AH=48 / INT 31h AX=0501) want a block and its address back.
     * mmap(0, ..., MAP_FIXED) would mean "the NULL page" - and fail below
     * mmap_min_addr - so the hint must be dropped, not honored. */
    if (!addr) {
        void *p = mmap(NULL, len, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        return (p == MAP_FAILED) ? NULL : p;
    }
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
    unsigned v = 0;

    /* What DS actually holds here, truncated to a selector: in long mode the
     * kernel may well leave it 0 (segment bases are ignored anyway), which is
     * NOT the 0x2B the Win32 build writes - so a 0x02 fixup (FDPS has exactly
     * one, FD2 has none) would relocate differently per OS. Unverified until
     * FDPS is run on Linux; see docs/rounds/13-portability.md.
     * `unsigned` on purpose: an "=a" constraint is 32-bit (mov %ds, %eax). */
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

/* ------------------------------------------------------- slice 2: the OS
 * primitives dos.c services INT 21h with - POSIX side. Position always
 * comes from the caller (pread/pwrite), so both platforms run the same
 * arithmetic in dos.c. */

int plat_readable(const void *p, size_t n)
{
    /* process_vm_readv(self) answers "would this read fault" with EFAULT
     * instead of raising a signal - one syscall, no /proc/self/maps parse
     * (guest_readable runs on *every* intercepted int, so parsing maps per
     * call would be far too slow). Probe the first and the last byte: the
     * VirtualQuery version this replaces checked the region of p and, when
     * the range crossed a boundary, the region of p+n-1 - same coverage. */
    struct iovec lv, rv;
    unsigned char b[1];
    ssize_t r;

    if (!p)
        return 0;
    if (!n)
        n = 1;
    lv.iov_base = b;
    lv.iov_len  = 1;
    rv.iov_base = (void *)p;
    rv.iov_len  = 1;
    r = process_vm_readv(getpid(), &lv, 1, &rv, 1, 0);
    if (r != 1)
        return 0;
    rv.iov_base = (void *)((const char *)p + n - 1);
    r = process_vm_readv(getpid(), &lv, 1, &rv, 1, 0);
    return r == 1;
}

plat_file plat_console_file(int fd)
{
    return (fd >= 0) ? (plat_file)fd : PLAT_FILE_INVALID;
}

plat_file plat_file_open(const char *name)
{
    int fd = open(name, O_RDWR | O_CLOEXEC);

    return (fd < 0) ? PLAT_FILE_INVALID : (plat_file)fd;
}

plat_file plat_file_create(const char *name)
{
    int fd = open(name, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);

    return (fd < 0) ? PLAT_FILE_INVALID : (plat_file)fd;
}

void plat_file_close(plat_file f)
{
    if (f != PLAT_FILE_INVALID)
        close((int)f);
}

int plat_file_read_at(plat_file f, uint64_t off, void *buf, unsigned n,
                      unsigned *got)
{
    ssize_t r;

    *got = 0;
    do {
        r = pread((int)f, buf, n, (off_t)off);
    } while (r < 0 && errno == EINTR);
    if (r < 0)
        return -1;
    *got = (unsigned)r;               /* 0 == EOF, like ReadFile */
    return 0;
}

int plat_file_write_at(plat_file f, uint64_t off, const void *buf, unsigned n,
                       unsigned *wrote)
{
    ssize_t r;

    *wrote = 0;
    do {
        r = pwrite((int)f, buf, n, (off_t)off);
    } while (r < 0 && errno == EINTR);
    if (r < 0)
        return -1;
    *wrote = (unsigned)r;
    return 0;
}

int plat_file_write_seq(plat_file f, const void *buf, unsigned n,
                        unsigned *wrote)
{
    ssize_t r;

    *wrote = 0;
    do {
        r = write((int)f, buf, n);
    } while (r < 0 && errno == EINTR);
    if (r < 0)
        return -1;
    *wrote = (unsigned)r;
    return 0;
}

int plat_file_truncate(plat_file f, uint64_t off)
{
    return ftruncate((int)f, (off_t)off);
}

int plat_file_size(plat_file f, uint64_t *size)
{
    struct stat st;

    if (fstat((int)f, &st) != 0)
        return -1;
    *size = (uint64_t)st.st_size;
    return 0;
}

int plat_file_delete(const char *name)
{
    return unlink(name);
}

int plat_file_attrs(const char *name, uint32_t *flags)
{
    struct stat st;
    const char *base = strrchr(name, '/');
    uint32_t f = 0;

    if (stat(name, &st) != 0)
        return -1;
    if (!(st.st_mode & 0222))
        f |= PLAT_FILE_RDONLY;
    if (S_ISDIR(st.st_mode))
        f |= PLAT_FILE_DIR;
    base = base ? base + 1 : name;
    if (base[0] == '.')
        f |= PLAT_FILE_HIDDEN;        /* closest thing to FILE_ATTRIBUTE_HIDDEN */
    *flags = f;
    return 0;
}

int plat_file_set_attrs(const char *name, uint32_t flags)
{
    struct stat st;
    mode_t mode;

    if (stat(name, &st) != 0)
        return -1;
    mode = st.st_mode;
    if (flags & PLAT_FILE_RDONLY)
        mode &= ~(mode_t)0222;        /* DOS read-only == no write bits */
    else
        mode |= 0222;                 /* hidden/system have no POSIX equivalent */
    return chmod(name, mode);
}

unsigned plat_error_to_dos(unsigned e)
{
    switch (e) {
    case ENOENT:
    case ENOTDIR:        return 2;    /* file not found */
    case EACCES:
    case EPERM:
    case EROFS:
    case EISDIR:         return 5;    /* access denied  */
    case EBADF:          return 6;    /* invalid handle */
    case EMFILE:
    case ENFILE:         return 4;    /* too many open  */
    default:             return 5;
    }
}

void plat_local_time(plat_time *t)
{
    struct timeval tv;
    struct tm tm;

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm);
    t->year    = (unsigned)(tm.tm_year + 1900);
    t->month   = (unsigned)(tm.tm_mon + 1);
    t->day     = (unsigned)tm.tm_mday;
    t->weekday = (unsigned)tm.tm_wday; /* 0 = Sunday, like wDayOfWeek */
    t->hour    = (unsigned)tm.tm_hour;
    t->minute  = (unsigned)tm.tm_min;
    t->second  = (unsigned)tm.tm_sec;
    t->ms      = (unsigned)(tv.tv_usec / 1000);
}

/* ---- threads ---------------------------------------------------------- */

struct plat_thread_req {
    void (*fn)(void *);
    void *arg;
};

static void *plat_thread_thunk(void *p)
{
    struct plat_thread_req r = *(struct plat_thread_req *)p;

    free(p);
    r.fn(r.arg);
    return NULL;
}

int plat_thread(void (*fn)(void *), void *arg)
{
    return plat_thread_stk(fn, arg, 0);
}

int plat_thread_stk(void (*fn)(void *), void *arg, size_t stack)
{
    struct plat_thread_req *r = malloc(sizeof *r);
    pthread_attr_t at;
    pthread_t th;

    if (!r)
        return -1;
    r->fn = fn;
    r->arg = arg;
    pthread_attr_init(&at);
    if (stack) {
        if (stack < (size_t)PTHREAD_STACK_MIN)
            stack = (size_t)PTHREAD_STACK_MIN;
        pthread_attr_setstacksize(&at, stack);
    }
    if (pthread_create(&th, &at, plat_thread_thunk, r) != 0) {
        pthread_attr_destroy(&at);
        free(r);
        return -1;
    }
    pthread_attr_destroy(&at);
    pthread_detach(th);               /* detached: nobody joins it */
    return 0;
}

void plat_sleep_ms(unsigned ms)
{
    struct timespec ts;

    ts.tv_sec  = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
        ;
}

uint64_t plat_thread_id(void)
{
    return (uint64_t)syscall(SYS_gettid);
}

uint64_t plat_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

uint64_t plat_now_us(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

int plat_set_cwd(const char *dir)
{
    return chdir(dir) == 0 ? 0 : -1;
}

int plat_module_path(char *buf, size_t n)
{
    ssize_t got = readlink("/proc/self/exe", buf, n - 1);

    if (got < 0 || (size_t)got >= n - 1)
        return -1;
    buf[got] = 0;
    return 0;
}

int plat_path_size(const char *path, uint64_t *size)
{
    struct stat st;

    if (stat(path, &st) != 0)
        return -1;
    *size = (uint64_t)st.st_size;
    return 0;
}

int plat_stricmp(const char *a, const char *b)
{
    return strcasecmp(a, b);
}

char *plat_strdup(const char *s)
{
    return strdup(s);
}

void plat_stdio_pin(void)
{
    /* POSIX processes always have fds 0/1/2 open */
}

/* ---- mutex + atomics (slice 4) ---------------------------------------- */

void plat_mutex_init(plat_mutex *m)
{
    pthread_mutex_t *mu = malloc(sizeof *mu);
    pthread_mutexattr_t at;

    if (!mu) return;                  /* m->h stays NULL: lock/unlock no-op */
    pthread_mutexattr_init(&at);
    pthread_mutexattr_settype(&at, PTHREAD_MUTEX_RECURSIVE);
    if (pthread_mutex_init(mu, &at) != 0) {
        pthread_mutexattr_destroy(&at);
        free(mu);
        return;
    }
    pthread_mutexattr_destroy(&at);
    m->h = mu;
}

void plat_mutex_destroy(plat_mutex *m)
{
    if (m->h) {
        pthread_mutex_destroy((pthread_mutex_t *)m->h);
        free(m->h);
        m->h = NULL;
    }
}

void plat_mutex_lock(plat_mutex *m)
{
    if (m->h) pthread_mutex_lock((pthread_mutex_t *)m->h);
}

void plat_mutex_unlock(plat_mutex *m)
{
    if (m->h) pthread_mutex_unlock((pthread_mutex_t *)m->h);
}

int32_t plat_atomic_read(volatile int32_t *p)
{
    return __atomic_load_n(p, __ATOMIC_SEQ_CST);
}

void plat_atomic_write(volatile int32_t *p, int32_t v)
{
    __atomic_store_n(p, v, __ATOMIC_SEQ_CST);
}

int32_t plat_atomic_inc(volatile int32_t *p)
{
    return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST);
}

void plat_exit(int code)
{
    fflush(NULL);                     /* crash reports must reach the log */
    _exit(code);
}

void plat_mem_status(uint64_t *avail, uint64_t *span)
{
    long page = sysconf(_SC_PAGESIZE);
    long av   = sysconf(_SC_AVPHYS_PAGES);
    long tot  = sysconf(_SC_PHYS_PAGES);

    if (page < 0) page = 4096;
    *avail = (av   > 0) ? (uint64_t)av   * (uint64_t)page : 0;
    /* "span" feeds DPMI AX=0500's ECX (address-space figure). Windows gave
     * dwAvailVirtual; there is no cheap POSIX equivalent, so total RAM
     * stands in - FD2 never calls this (UNVERIFIED). */
    *span  = (tot > 0) ? (uint64_t)tot * (uint64_t)page : 0;
}

/* ---- exec (INT 21h AH=4B) --------------------------------------------- */

static pid_t g_child_pid;

int plat_exec_child(const plat_exec_req *r, int *exit_code)
{
    char self[PLAT_MAX_PATH];
    char cwd[PLAT_MAX_PATH];
    char child[PLAT_MAX_PATH];
    char clog[PLAT_MAX_PATH];
    char exitarg[40];
    char argvbuf[PLAT_MAX_PATH * 4 + 640];
    const char *argv[8];
    int argc = 0;
    size_t len = 0;
    ssize_t n;
    pid_t pid;
    int i;

    n = readlink("/proc/self/exe", self, sizeof self - 1);
    if (n <= 0) {
        printf("dos:   exec: cannot resolve /proc/self/exe\n");
        return -1;
    }
    self[n] = 0;
    if (!getcwd(cwd, sizeof cwd))
        cwd[0] = 0;
    if (!realpath(r->guest_path, child))
        snprintf(child, sizeof child, "%s", r->guest_path);

    /* One log per generation, next to our own image (the Windows rule: a
     * child freopen()ing host.log would truncate the parent's file). */
    {
        char *slash = strrchr(self, '/');
        if (slash) {
            size_t dirlen = (size_t)(slash - self);
            snprintf(clog, sizeof clog, "%.*s/host.%d.log",
                     (int)dirlen, self, (int)getpid());
        } else {
            snprintf(clog, sizeof clog, "host.%d.log", (int)getpid());
        }
    }

    argv[argc++] = self;              /* argv strings are packed below */
    snprintf(argvbuf + len, sizeof argvbuf - len, "--exe=%s", child);
    argv[argc++] = argvbuf + len; len += strlen(argvbuf + len) + 1;
    snprintf(argvbuf + len, sizeof argvbuf - len, "--gamedir=%s", cwd);
    argv[argc++] = argvbuf + len; len += strlen(argvbuf + len) + 1;
    snprintf(argvbuf + len, sizeof argvbuf - len, "--log=%s", clog);
    argv[argc++] = argvbuf + len; len += strlen(argvbuf + len) + 1;
    snprintf(argvbuf + len, sizeof argvbuf - len, "--cmdtail=%s",
             r->cmdtail);
    argv[argc++] = argvbuf + len; len += strlen(argvbuf + len) + 1;
    if (r->wait && r->exit_after > 0) {
        snprintf(exitarg, sizeof exitarg, "--exit-after=%d", r->exit_after);
        argv[argc++] = exitarg;
    }
    argv[argc] = NULL;

    printf("dos:   child:");
    for (i = 0; argv[i]; i++)
        printf(" %s", argv[i]);
    printf("\n");

    pid = fork();
    if (pid < 0) {
        printf("dos:   fork failed (%d)\n", errno);
        return -1;
    }
    if (pid == 0) {
        execv(self, (char *const *)argv);
        printf("dos:   execv failed (%d)\n", errno);
        _exit(127);
    }
    g_child_pid = pid;
    if (r->wait) {
        int st = 0;

        while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
            ;
        *exit_code = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
        printf("dos:   child exited with %d\n", *exit_code);
        g_child_pid = 0;
    }
    return 0;
}

int plat_child_present(void)
{
    return g_child_pid != 0;
}

void plat_child_kill(void)
{
    if (!g_child_pid)
        return;
    kill(g_child_pid, SIGKILL);
    waitpid(g_child_pid, NULL, 0);
    g_child_pid = 0;
}
