/* platform.h - the OS seam, so the loader and the host can build on more
 * than one OS (docs/BACKEND.md §13.6 step 4).
 *
 * Slice 1: **memory** (le.c). Slice 2: **files / time / threads / process /
 * fault install** (dos.c) - the fault *dispatcher* itself is module logic and
 * lives in dos.c behind dos_fault.h; only its installation differs per OS
 * (dos_fault_win.c = VEH, dos_fault_posix.c = sigaction).
 * The remaining Win32 surface (threads/time in host.c and the AIL stack,
 * the keyboard translations in the entry layers) is surveyed in
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

/* Can this range be read right now without faulting? (The guest-pointer
 * checks in dos.c: an unimplemented INT 21h service can hand us any address
 * and dereferencing it must not kill the host - PROGRESS.md §8-45.)
 * n == 0 probes the first byte, like a region query would. */
int plat_readable(const void *p, size_t n);

/* ---------------------------------------------------------------- slice 2 -
 * files / time / threads / process (dos.c).
 *
 * File position is owned by the *caller*: dos.c keeps a 32-bit position per
 * DOS handle and every transfer names its offset explicitly (pread/pwrite on
 * POSIX, SetFilePointerEx+ReadFile/WriteFile on Windows). Only dos.c ever
 * touches these handles, so the arithmetic is one shared code path - and it
 * matches DOS, where the position is handle state, not a kernel cursor.
 * Console/log handles (plat_console_file) are the exception: not seekable,
 * sequential writes only.
 *
 * All functions return 0 on success and -1 on failure; plat_error() then
 * holds the platform code (GetLastError / errno), plat_error_to_dos() turns
 * it into a DOS error code (AH=3C/AH=41 semantics).
 */
typedef uintptr_t plat_file;                 /* Win32 HANDLE / POSIX fd */
#define PLAT_FILE_INVALID ((plat_file)(intptr_t)-1)

plat_file plat_console_file(int fd);         /* CRT fd -> OS handle (0/1/2)  */
plat_file plat_file_open(const char *name);  /* existing file, read+write    */
plat_file plat_file_create(const char *name);/* create or truncate           */
void      plat_file_close(plat_file f);
int plat_file_read_at (plat_file f, uint64_t off, void *buf, unsigned n,
                       unsigned *got);
int plat_file_write_at(plat_file f, uint64_t off, const void *buf, unsigned n,
                       unsigned *wrote);
int plat_file_write_seq(plat_file f, const void *buf, unsigned n,
                        unsigned *wrote);    /* console/log handles           */
int plat_file_truncate(plat_file f, uint64_t off);
int plat_file_size(plat_file f, uint64_t *size);
int plat_file_delete(const char *name);

/* Attribute flags in DOS order (AH=43 maps them 1:1); platform-neutral so
 * the DOS bit-shuffling stays in one portable place. */
#define PLAT_FILE_RDONLY 0x01u
#define PLAT_FILE_HIDDEN 0x02u
#define PLAT_FILE_SYSTEM 0x04u
#define PLAT_FILE_DIR    0x10u
int      plat_file_attrs(const char *name, uint32_t *flags);
int      plat_file_set_attrs(const char *name, uint32_t flags);
unsigned plat_error_to_dos(unsigned e);      /* ENOENT/ERROR_FILE_NOT_FOUND -> 2 */

/* Local time for INT 21h AH=2A/2C (weekday: 0 = Sunday, as GetLocalTime). */
typedef struct plat_time {
    unsigned year, month, day, weekday;
    unsigned hour, minute, second, ms;
} plat_time;
void plat_local_time(plat_time *t);

/* Detached thread (bios tick); 0 = started. Thread entry runs as fn(arg). */
int      plat_thread(void (*fn)(void *), void *arg);
void     plat_sleep_ms(unsigned ms);
uint64_t plat_thread_id(void);              /* GetCurrentThreadId / gettid */
void     plat_exit(int code);               /* does not return (ExitProcess / _exit) */

/* GlobalMemoryStatus for INT 31h AX=0500: `avail` = free memory,
 * `span` = address-space figure (see the implementations - on POSIX both
 * come from sysconf and the DPMI meaning of `span` is UNVERIFIED). */
void plat_mem_status(uint64_t *avail, uint64_t *span);

/* INT 21h AH=4B exec: spawn *this* host against a new exe.
 * `guest_path` is already a plain string read out of guest memory, `cmdtail`
 * is the PSP tail contents (no length byte / CR). wait=1 waits and reports.
 * Implemented with CreateProcessA (verbatim from the old dos.c) and
 * fork+execv; the POSIX side is UNVERIFIED until the game runs under -m32. */
typedef struct plat_exec_req {
    const char *guest_path;   /* path the guest asked for (may be relative) */
    const char *cmdtail;      /* PSP:0x81 contents                           */
    int wait;                 /* 1 = wait for the child                      */
    int exit_after;           /* >= 0: pass --exit-after=N to bound the child */
} plat_exec_req;
int  plat_exec_child(const plat_exec_req *r, int *exit_code);
int  plat_child_present(void);   /* 1 = a spawned child is currently waited on */
void plat_child_kill(void);

/* ---------------------------------------------------------------- slice 3 -
 * clock / threads with a stack size / process context / path lookup (host.c).
 *
 * host.c used to call the Win32 primitives directly; these are the pieces a
 * POSIX entry layer needs too. `plat_now_ms` is *monotonic* (GetTickCount64 /
 * CLOCK_MONOTONIC), so it is safe for --shot-time, --autokey, --keyplay and
 * the watchdog - unlike plat_local_time(), which jumps with the wall clock.
 */
uint64_t plat_now_ms(void);
uint64_t plat_now_us(void);   /* same clock, microsecond resolution */

/* Detached thread with an explicit stack size (the game thread asks for
 * 4 MiB; the guest uses a lot of stack). stack == 0 -> platform default. */
int plat_thread_stk(void (*fn)(void *), void *arg, size_t stack);

int  plat_set_cwd(const char *dir);        /* 0 = ok (SetCurrentDirectory)  */
int  plat_module_path(char *buf, size_t n);/* path of this executable, 0=ok */

/* Size of a path without opening it (--exit-when-file watches a file the guest
 * still holds open, so directory metadata is the right source). -1 = missing. */
int  plat_path_size(const char *path, uint64_t *size);

/* Portable spelling helpers (_stricmp/_strdup). */
int   plat_stricmp(const char *a, const char *b);
char *plat_strdup(const char *s);

/* Calling convention of the original guest code (cdecl on 32-bit x86; the
 * keyword does not exist on x86-64 / non-MSVC compilers, where it is a no-op). */
#if defined(_MSC_VER)
#define PLAT_CDECL __cdecl
#else
#define PLAT_CDECL
#endif

/* Windows GUI-subsystem processes start with fds 0/1/2 closed, so freopen()
 * may land anywhere and the guest's DOS handle 1 derives from a dead fd
 * (PITFALLS S8-35). Re-point fd 1/2 at the (already redirected) stdio
 * streams. No-op on POSIX. */
void plat_stdio_pin(void);

/* ---------------------------------------------------------------- slice 4 -
 * recursive mutex + 32-bit atomics (the AIL/audio stack: ail.c, synth.c,
 * xmidi.c, audio_sokol.c). `plat_mutex` is a handle (zero initialised = not
 * yet created); every mutex here is *recursive* because the audio device
 * callback re-enters the producer side (docs/AUDIO.md §11.10).
 */
typedef struct plat_mutex { void *h; } plat_mutex;

void plat_mutex_init(plat_mutex *m);
void plat_mutex_destroy(plat_mutex *m);
void plat_mutex_lock(plat_mutex *m);
void plat_mutex_unlock(plat_mutex *m);

int32_t plat_atomic_read(volatile int32_t *p);              /* seq-cst load    */
void    plat_atomic_write(volatile int32_t *p, int32_t v); /* seq-cst store   */
int32_t plat_atomic_inc(volatile int32_t *p);              /* returns new val  */

/* Base address of this executable (GetModuleHandleA(NULL) / the PIE base). */
void *plat_image_base(void);

/* One line describing who owns `addr`, for failure reports. Windows keeps
 * the fields the old hand-rolled report used (`type=`, `prot=`, `region=`,
 * `mapping:` - docs/PITFALLS.md §8-48 quotes them), POSIX prints the
 * /proc/self/maps entry. */
void plat_describe(uintptr_t addr, char *buf, size_t n);

#endif /* FD2_PLATFORM_H */
