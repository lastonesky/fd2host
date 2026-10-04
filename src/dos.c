/* dos.c - traps and services for the native FD2 host.
 *
 * Strategy
 * --------
 * The game is 32-bit flat-model DOS/4GW code. All of its environment access
 * happens through `int NN` instructions, privileged port instructions and the
 * 0xA0000 frame buffer. None of that may run as-is in a Win32 user process, so
 * this module services the original interrupt with Win32 calls. Privileged
 * IN/OUT instructions are caught as EXCEPTION_PRIV_INSTRUCTION and emulated.
 *
 * The software interrupts are *not* patched any more. They used to be
 * rewritten from `CD xx` to `CC 90` at load time and looked up in a site
 * table, but that rewrite is a blind byte-pattern scan and it also matched
 * operands: the displacement of `call sub_34894` (E8 CD 20 02 00 at 0x127C2)
 * contains `CD 20`, became `E8 CC 90 02 00`, and the call - which happens on
 * the "continue" path - landed at 0x3B893, in the middle of AIL_install_timbre,
 * and the game exited. probe4.c measured what Windows does instead: `int NN`
 * in ring 3 raises EXCEPTION_ACCESS_VIOLATION and `int 3` raises
 * EXCEPTION_BREAKPOINT, both with EIP pointing *at* the instruction, so the
 * vector can simply be read from the faulting instruction and the guest code
 * is left byte-for-byte intact.
 *
 * That is a *replacement* of the DOS/flat-hardware interfaces, not a DOS
 * emulator: there is no real-mode CPU, no interrupt controller, no option ROM.
 */

#include "dos.h"
#include "le.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------- globals */

volatile int dos_exit_requested = 0;
volatile int dos_exit_code      = 0;

static le_image *g_le;
static uint8_t  *g_lowmem;              /* the 64 KiB low-memory window    */

/* Software interrupts serviced since start-up (diagnostic counter). */
static int       g_int_sites;
static int       g_verbose = 1;
static int       g_trace_left = 30;    /* interrupt trace budget */
static int       g_trace_mode;         /* single-step tracing enabled */

/* per-interrupt statistics */
static unsigned  g_calls[256];
static unsigned  g_unknown[256];

/* Ring of the most recent *unusual* VEH events (privileged instructions, access
 * violations, stray breakpoints). Ordinary int3 trap sites are excluded - there
 * are millions of those. When the flow goes off the rails this shows which
 * handler ran, and where the instruction pointer was just before. */
#define VEH_RING 32
static struct { uint32_t eip, code, info; } g_veh_ring[VEH_RING];
static unsigned g_veh_pos;

static void veh_record(uint32_t eip, DWORD code, uint32_t info)
{
    unsigned i = g_veh_pos & (VEH_RING - 1);
    g_veh_ring[i].eip = eip;
    g_veh_ring[i].code = code;
    g_veh_ring[i].info = info;
    g_veh_pos++;
}

static void veh_dump_ring(void)
{
    unsigned n = g_veh_pos < VEH_RING ? g_veh_pos : VEH_RING;
    unsigned i, start = g_veh_pos - n;
    printf("     last %u unusual exceptions (oldest first):\n", n);
    for (i = 0; i < n; i++) {
        unsigned k = (start + i) & (VEH_RING - 1);
        printf("       eip=%08X code=%08X info=%u\n",
               g_veh_ring[k].eip, g_veh_ring[k].code, g_veh_ring[k].info);
    }
}
static unsigned  g_port_ops;

/* ------------------------------------------------------------- file table */

#define DOS_MAX_FILES 64

typedef struct {
    HANDLE h;
    int    used;
    int    is_dev;      /* 1 = console/stdaux/stdprn                   */
    char   name[64];    /* for logs: which file a handle refers to     */
} dos_file;

static dos_file g_files[DOS_MAX_FILES];

/* Allocation ledger: lets a crash report say whether the faulting EIP landed
 * in a block we handed to the game (i.e. code that was generated or copied at
 * runtime) rather than in the linked objects. */
#define ALLOC_LOG_MAX 64
static struct { uint32_t base, size; const char *what; } g_allocs[ALLOC_LOG_MAX];
static int g_alloc_count;
static uint32_t g_last_trap_eip;

/* Ring buffer of the most recent intercepted calls: when the game sends the
 * execution stream somewhere unmapped, this shows the path it fell off. */
#define TRAP_RING 32
static struct {
    uint32_t eip;
    uint32_t eax, ebx, ecx, edx, esi, edi, esp;
    uint8_t  vec;
} g_trap_ring[TRAP_RING];
static int g_trap_ring_pos;

static void trap_note(uint8_t vec, uint32_t eip, const CONTEXT *c)
{
    int i = g_trap_ring_pos++ & (TRAP_RING - 1);
    g_trap_ring[i].vec = vec;
    g_trap_ring[i].eip = eip;
    g_trap_ring[i].eax = (uint32_t)c->Eax;
    g_trap_ring[i].ebx = (uint32_t)c->Ebx;
    g_trap_ring[i].ecx = (uint32_t)c->Ecx;
    g_trap_ring[i].edx = (uint32_t)c->Edx;
    g_trap_ring[i].esi = (uint32_t)c->Esi;
    g_trap_ring[i].edi = (uint32_t)c->Edi;
    g_trap_ring[i].esp = (uint32_t)c->Esp;
}

static void trap_dump(void)
{
    int n = (g_trap_ring_pos < TRAP_RING) ? g_trap_ring_pos : TRAP_RING;
    int k;
    printf("--- last %d intercepted calls (oldest first) ---\n", n);
    for (k = 0; k < n; k++) {
        int i = (g_trap_ring_pos - n + k) & (TRAP_RING - 1);
        printf("    int %02X @0x%X ax=%04X bx=%04X cx=%04X dx=%04X "
               "si=%04X di=%04X esp=%08X\n",
               g_trap_ring[i].vec, g_trap_ring[i].eip,
               (unsigned)(g_trap_ring[i].eax & 0xFFFF),
               (unsigned)(g_trap_ring[i].ebx & 0xFFFF),
               (unsigned)(g_trap_ring[i].ecx & 0xFFFF),
               (unsigned)(g_trap_ring[i].edx & 0xFFFF),
               (unsigned)(g_trap_ring[i].esi & 0xFFFF),
               (unsigned)(g_trap_ring[i].edi & 0xFFFF),
               g_trap_ring[i].esp);
    }
}

/* Any probe that inspects guest memory from inside the handler must first prove
 * the range is readable: the faulting address itself may be unmapped, and a
 * second fault inside the VEH would replace the real one. */
static int guest_readable(const void *p, size_t n);

/* A "jump to 0" is almost always a `ret` whose return address got zeroed, so
 * the top of the stack is the most informative thing to look at. */
static void stack_dump(uint32_t esp)
{
    const uint32_t *sp = (const uint32_t *)(uintptr_t)esp;
    int k;
    printf("     stack @0x%X:", (unsigned)esp);
    if (!guest_readable(sp, 16 * 4)) {
        printf(" (unreadable)\n");
        return;
    }
    for (k = 0; k < 16; k++) printf(" %08X", sp[k]);
    printf("\n");
}

/* Bump allocator for real-mode addressable memory (below 1 MiB). Starts right
 * after the low-memory window. */
static uint32_t g_lo_next = DOS_LOWMEM_BASE + DOS_LOWMEM_SIZE;

static void note_alloc(uint32_t base, uint32_t size, const char *what)
{
    if (g_alloc_count < ALLOC_LOG_MAX) {
        g_allocs[g_alloc_count].base = base;
        g_allocs[g_alloc_count].size = size;
        g_allocs[g_alloc_count].what = what;
        g_alloc_count++;
    }
}

static const char *find_alloc(uint32_t addr)
{
    int i;
    for (i = 0; i < g_alloc_count; i++) {
        if (addr >= g_allocs[i].base && addr < g_allocs[i].base + g_allocs[i].size)
            return g_allocs[i].what;
    }
    return NULL;
}

/* AIL driver files are 16-bit real-mode code; see the open handler. */
static int g_block_drivers = 1;

static int is_ail_driver_name(const char *name)
{
    size_t len = strlen(name);
    if (len < 4) return 0;
    return (name[len - 4] == '.' &&
            (name[len - 3] == 'D' || name[len - 3] == 'd') &&
            (name[len - 2] == 'I' || name[len - 2] == 'i') &&
            (name[len - 1] == 'G' || name[len - 1] == 'g')) ||
           (name[len - 4] == '.' &&
            (name[len - 3] == 'M' || name[len - 3] == 'm') &&
            (name[len - 2] == 'D' || name[len - 2] == 'd') &&
            (name[len - 1] == 'I' || name[len - 1] == 'i'));
}

/* VGA DAC state. The game drives the palette with out 0x3C8 (index) and
 * out 0x3C9 (R,G,B) - that is how it fades and switches palettes. The
 * hardware registers are 6 bits per channel (0..63); dos_palette stores
 * 8 bits per channel (0..255), i.e. it is a ready-to-display RGB table. */
uint8_t  dos_palette[256 * 3];
volatile int dos_palette_dirty;
static int g_dac_index;
static int g_dac_component;

static void files_init(void)
{
    int i;
    memset(g_files, 0, sizeof g_files);
    /* 0=stdin 1=stdout 2=stderr 3=stdaux 4=stdprn */
    for (i = 0; i < 5; i++) {
        g_files[i].used = 1;
        g_files[i].is_dev = 1;
        g_files[i].h = (i == 0) ? GetStdHandle(STD_INPUT_HANDLE) :
                       (i == 1) ? GetStdHandle(STD_OUTPUT_HANDLE) :
                       (i == 2) ? GetStdHandle(STD_ERROR_HANDLE) : INVALID_HANDLE_VALUE;
    }
}

static int file_alloc(HANDLE h, const char *name)
{
    int i;
    for (i = 5; i < DOS_MAX_FILES; i++) {
        if (!g_files[i].used) {
            g_files[i].used = 1;
            g_files[i].is_dev = 0;
            g_files[i].h = h;
            g_files[i].name[0] = 0;
            if (name) {
                strncpy(g_files[i].name, name, sizeof g_files[i].name - 1);
                g_files[i].name[sizeof g_files[i].name - 1] = 0;
            }
            return i;
        }
    }
    return -1;
}

/* Windows error -> DOS error code. sopen() only really distinguishes
 * 2 (file not found) - it is the trigger for the O_CREAT fallback - but
 * reporting the plausible code makes failures easier to read in the log. */
static uint32_t dos_win_error(DWORD e)
{
    switch (e) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:   return 2;   /* file not found  */
    case ERROR_ACCESS_DENIED:    return 5;   /* access denied   */
    case ERROR_SHARING_VIOLATION:return 5;
    case ERROR_INVALID_HANDLE:   return 6;   /* invalid handle  */
    case ERROR_TOO_MANY_OPEN_FILES: return 4;
    default:                     return 5;
    }
}

/* --------------------------------------------------------- low memory init */

void dos_init_lowmem(void)
{
    g_lowmem = (uint8_t *)VirtualAlloc((void *)(uintptr_t)DOS_LOWMEM_BASE,
                                       DOS_LOWMEM_SIZE,
                                       MEM_RESERVE | MEM_COMMIT,
                                       PAGE_READWRITE);
    if (!g_lowmem) {
        fprintf(stderr, "dos: cannot map low-memory window: %lu\n", GetLastError());
        return;
    }
    memset(g_lowmem, 0, DOS_LOWMEM_SIZE);

    /* Commit the rest of the real-mode addressable RAM outside the VGA
     * window: 0x80000-0x9FFFF (head of the INT31 0100 pool) and
     * 0xC0000-0xFFFFF. On real hardware 0xC0000+ is ROM and stores there are
     * discarded, so the game's blitter may legally walk off the end of the
     * VGA window into it; Windows reports that as an access violation
     * unless the pages are mapped. Committing them here also makes any
     * `seg << 4` pointer land in writable memory. */
    if (!VirtualAlloc((void *)(uintptr_t)0x00080000u, 0x00020000u,
                      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))
        fprintf(stderr, "dos: cannot map 0x80000-0x9FFFF: %lu\n", GetLastError());
    if (!VirtualAlloc((void *)(uintptr_t)0x000C0000u, 0x00040000u,
                      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))
        fprintf(stderr, "dos: cannot map 0xC0000-0xFFFFF: %lu\n", GetLastError());

    /* BIOS data area: point the keyboard buffer head/tail at each other so the
     * game sees an empty buffer, and report a 80x25 text mode. */
    g_lowmem[0x449] = 0x03;          /* video mode                     */
    g_lowmem[0x44A] = 80;            /* columns                        */
    g_lowmem[0x41A] = 0x1E;          /* KB head = 0x041E               */
    g_lowmem[0x41B] = 0x00;
    g_lowmem[0x41C] = 0x1E;          /* KB tail = head -> empty        */
    g_lowmem[0x41D] = 0x00;
    g_lowmem[0x417] = 0x00;          /* keyboard flags                 */
    g_lowmem[0x46C] = 0;             /* timer tick count (kept live)   */

    /* The DOS/4GW startup code reads the "real mode segment" of vector 0x0B
     * from the IVT and later shifts it left by 4 to walk the environment. Make
     * that segment point at this window so `seg << 4 + offset` lands inside
     * our low-memory mirror (which then looks like an empty environment). */
    *(uint16_t *)(g_lowmem + 0x2C) = (uint16_t)DOS_LOWMEM_SEG;

    files_init();
}

/* The BIOS tick counter at 0x40:0x6C is the game's time source (it polls it
 * instead of hooking the timer interrupt). Keep it advancing at the classic
 * 18.2 Hz so animations and delays progress. */
static DWORD WINAPI bios_tick_thread(LPVOID param)
{
    (void)param;
    for (;;) {
        Sleep(55);
        if (g_lowmem) {
            uint32_t t = *(uint32_t *)(g_lowmem + 0x46C);
            *(uint32_t *)(g_lowmem + 0x46C) = t + 1;
        }
    }
    return 0;
}

static void bios_tick_start(void)
{
    CreateThread(NULL, 0, bios_tick_thread, NULL, 0, NULL);
}

/* ------------------------------------------------------- interrupt census */

/* Nothing is patched: an `int NN` faults (probe4.c) and the vector is read
 * from the faulting instruction in the VEH. This entry point is kept for the
 * host's start-up sequence and only reports what it found. */
int dos_patch_interrupts(void)
{
    if (!g_le) return -1;
    printf("dos: software interrupts dispatched from faults"
           " (guest code left unmodified)\n");
    return 0;
}

int dos_patch_lowmem_refs(void)
{
    /* Absolute operands pointing into the BIOS data area cannot work because
     * Windows keeps the low 64 KiB unmapped. Rewrite such immediate operands
     * to the mirror address. Only the forms the game actually uses appear:
     *   B8+r imm32  (mov eax/ecx/edx/ebx/esp/ebp/esi/edi, imm32)
     *   68   imm32  (push imm32)
     * which are all 5 bytes with the immediate last. */
    uint32_t i, off;
    uint8_t *base;
    uint32_t size;
    int n = 0;

    if (!g_le) return -1;
    for (i = 0; i < g_le->object_count; i++) {
        if (!(g_le->objects[i].flags & 0x04))
            continue;
        base = (uint8_t *)(uintptr_t)g_le->objects[i].base;
        size = g_le->objects[i].vsize;
        for (off = 0; off + 5 <= size; off++) {
            uint8_t op = base[off];
            uint32_t imm;
            int is_mov = (op >= 0xB8 && op <= 0xBF);
            if (!is_mov && op != 0x68)
                continue;
            imm = (uint32_t)base[off + 1] | ((uint32_t)base[off + 2] << 8) |
                  ((uint32_t)base[off + 3] << 16) | ((uint32_t)base[off + 4] << 24);
            if (imm >= 0x400 && imm < 0x500) {
                uint32_t fixed = DOS_LOWMEM_BASE + imm;
                if (n < 12)
                    printf("dos: lowmem ref 0x%X -> 0x%X at mem 0x%X\n",
                           imm, fixed, g_le->objects[i].base + off);
                base[off + 1] = (uint8_t)(fixed);
                base[off + 2] = (uint8_t)(fixed >> 8);
                base[off + 3] = (uint8_t)(fixed >> 16);
                base[off + 4] = (uint8_t)(fixed >> 24);
                n++;
            }
        }
    }
    printf("dos: redirected %d low-memory references\n", n);
    return n;
}

/* --------------------------------------------------------------- services */

static void log_call(const char *what, CONTEXT *c)
{
    g_verbose = g_verbose;
    printf("  %-28s eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X\n",
           what, (unsigned)c->Eax, (unsigned)c->Ebx, (unsigned)c->Ecx,
           (unsigned)c->Edx, (unsigned)c->Esi, (unsigned)c->Edi);
}

static void set_cf(CONTEXT *c, int on)
{
    if (on) c->EFlags |= 1u;
    else    c->EFlags &= ~1u;
}

/* Windows encodes the access kind in ExceptionInformation[0]: 0=read,
 * 1=write, 8=execute (instruction fetch). */
static const char *fault_kind(ULONG_PTR kind)
{
    switch (kind) {
    case 0: return "read from";
    case 1: return "write to";
    case 8: return "instruction fetch at";
    default: return "access to";
    }
}

/* INT 21h - DOS services */
static void int21(CONTEXT *c)
{
    uint8_t ah = (uint8_t)(c->Eax >> 8);
    g_calls[0x21]++;

    switch (ah) {
    case 0x30: {
        /* DOS version. Deliberately NOT answering with an extender signature:
         * the startup code's "extender known" branches assume state set up by
         * an extender that is not here, while the generic path queries the
         * extender explicitly (INT 21h AH=0xFF) and then reads PSP:0x2C for the
         * environment segment - which is what the host provides. */
        c->Eax = 0x42431606u;      /* extender signature 'BC' + DOS 6.22 */
        c->Ebx = 0;
        c->Ecx = 0x0A04;
        break;
    }

    case 0x4C:                                  /* terminate */
        dos_exit_code = (int)(c->Eax & 0xFF);
        dos_exit_requested = 1;
        printf("dos: INT 21h AH=4Ch terminate, code=%d\n", dos_exit_code);
        break;

    case 0x25:                                  /* set interrupt vector */
    case 0x35:                                  /* get interrupt vector */
        if (g_verbose) log_call(ah == 0x25 ? "INT21 set vector" : "INT21 get vector", c);
        if (ah == 0x35) {
            c->Ebx = 0; c->Eax = 0;
        }
        set_cf(c, 0);
        break;

    case 0x3D: {                                /* open file */
        const char *name = (const char *)(uintptr_t)c->Edx;
        HANDLE h;
        /* Miles AIL loads 16-bit real-mode sound-card drivers (*.DIG / *.MDI)
         * and jumps straight into them. That code cannot run in this process,
         * so the files are reported as missing: AIL then starts with no digital
         * audio device - the game keeps running silently instead of crashing
         * inside driver code. Real audio comes from a modern backend later. */
        if (g_block_drivers && name && is_ail_driver_name(name)) {
            printf("dos: open '%s' -> blocked (16-bit AIL driver)\n", name);
            set_cf(c, 1);
            c->Eax = 2;                         /* file not found */
            break;
        }
        h = CreateFileA(name, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ, NULL, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL, NULL);
        printf("dos: open '%s' -> %p (%lu)\n", name, h, GetLastError());
        if (h == INVALID_HANDLE_VALUE) { set_cf(c, 1); c->Eax = 2; break; }
        c->Eax = (uint32_t)file_alloc(h, name);
        set_cf(c, c->Eax == (uint32_t)-1);
        break;
    }

    case 0x3C: {                                /* create file (truncate) */
        /* Watcom's sopen() opens first (AH=3D) and, when that reports
         * "file not found" while O_CREAT is set, falls back to CREAT
         * (AH=3C), then closes and re-opens the file. That is exactly what
         * fopen("wb") does for a file that does not exist yet - FD2.SAV on
         * a first save, FD2.TMP after a fresh install. With AH=3C missing
         * the CRT returned NULL FILE* and the game crashed dereferencing
         * it (AV at 0x377B2 reading address 0xC, see PROGRESS.md §12). */
        const char *name = (const char *)(uintptr_t)c->Edx;
        DWORD err = 0;
        HANDLE h = CreateFileA(name, GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
        int hnd;
        if (h == INVALID_HANDLE_VALUE) {
            err = GetLastError();
            printf("dos: create '%s' -> FAILED (%lu)\n", name, err);
            set_cf(c, 1);
            c->Eax = (c->Eax & 0xFFFF0000u) | dos_win_error(err);
            break;
        }
        hnd = file_alloc(h, name);
        printf("dos: create '%s' -> %p (dos handle %d)\n", name, h, hnd);
        if (hnd < 0) {
            CloseHandle(h);
            set_cf(c, 1);
            c->Eax = (c->Eax & 0xFFFF0000u) | 4;      /* too many open files */
            break;
        }
        c->Eax = (c->Eax & 0xFFFF0000u) | (uint32_t)hnd;
        set_cf(c, 0);
        break;
    }

    case 0x41: {                                /* delete file */
        const char *name = (const char *)(uintptr_t)c->Edx;
        if (DeleteFileA(name)) {
            printf("dos: delete '%s'\n", name);
            set_cf(c, 0);
        } else {
            DWORD err = GetLastError();
            printf("dos: delete '%s' -> FAILED (%lu)\n", name, err);
            set_cf(c, 1);
            c->Eax = (c->Eax & 0xFFFF0000u) | dos_win_error(err);
        }
        break;
    }

    case 0x3E: {                                /* close file */
        int hnd = (int)(c->Ebx & 0xFFFF);
        if (hnd > 4 && hnd < DOS_MAX_FILES && g_files[hnd].used) {
            CloseHandle(g_files[hnd].h);
            g_files[hnd].used = 0;
        }
        set_cf(c, 0);
        break;
    }

    case 0x3F: {                                /* read */
        int hnd = (int)(c->Ebx & 0xFFFF);
        void *buf = (void *)(uintptr_t)c->Edx;
        DWORD want = c->Ecx, got = 0;
        if (hnd < DOS_MAX_FILES && g_files[hnd].used && !g_files[hnd].is_dev) {
            if (!ReadFile(g_files[hnd].h, buf, want, &got, NULL)) {
                set_cf(c, 1); c->Eax = 5; break;
            }
            set_cf(c, 0); c->Eax = got;
        } else {
            set_cf(c, 0); c->Eax = 0;
        }
        break;
    }

    case 0x40: {                                /* write */
        int hnd = (int)(c->Ebx & 0xFFFF);
        void *buf = (void *)(uintptr_t)c->Edx;
        DWORD want = c->Ecx, wrote = 0;
        if (hnd < DOS_MAX_FILES && g_files[hnd].used) {
            if (want == 0) {
                /* DOS: a zero-length write truncates the file at the current
                 * file position. Watcom's sopen() implements O_TRUNC (the
                 * "wb" mode of fopen) with exactly this call, right after
                 * opening the file - i.e. it expects the whole file to be
                 * dropped. Windows' WriteFile(h, ..., 0, ...) is a no-op, so
                 * the truncation has to be done explicitly. Without it a
                 * save that shrinks leaves the tail of the old save behind. */
                if (!g_files[hnd].is_dev) {
                    LONG pos = SetFilePointer(g_files[hnd].h, 0, NULL, FILE_CURRENT);
                    if (pos == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
                        set_cf(c, 1);
                        c->Eax = (c->Eax & 0xFFFF0000u) | dos_win_error(GetLastError());
                        break;
                    }
                    if (!SetEndOfFile(g_files[hnd].h)) {
                        DWORD err = GetLastError();
                        printf("dos: truncate '%s' to %ld FAILED (%lu)\n",
                               g_files[hnd].name, (long)pos, err);
                        set_cf(c, 1);
                        c->Eax = (c->Eax & 0xFFFF0000u) | dos_win_error(err);
                        break;
                    }
                    printf("dos: truncate '%s' to %ld bytes\n",
                           g_files[hnd].name, (long)pos);
                }
                set_cf(c, 0); c->Eax = 0;
                break;
            }
            WriteFile(g_files[hnd].h, buf, want, &wrote, NULL);
        }
        set_cf(c, 0); c->Eax = wrote;
        break;
    }

    case 0x42: {                                /* lseek */
        int hnd = (int)(c->Ebx & 0xFFFF);
        /* DOS: CX:DX = unsigned 32-bit offset, AL = origin. The previous
         * code passed CX as SetFilePointer's *high 32 bits*, so seeking to
         * CX:DX = 0x002A:1CF3 landed at (0x2A<<32)|0x1CF3 (~171 GB) - a
         * legal 64-bit position past EOF. No error was raised, but the next
         * read returned 0 bytes and the game decompressed stale garbage
         * (crash: RLE run walked off the VGA window at 0xC0005). */
        uint32_t pos = ((c->Ecx & 0xFFFFu) << 16) | (c->Edx & 0xFFFFu);
        DWORD meth = c->Eax & 0xFF;
        if (hnd < DOS_MAX_FILES && g_files[hnd].used && !g_files[hnd].is_dev) {
            DWORD r = SetFilePointer(g_files[hnd].h, (LONG)pos, NULL, meth);
            if (r == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
                set_cf(c, 1); c->Eax = 6; break;
            }
            /* New position DX:AX (for callers assembling DX:AX); leaving
             * the full 32-bit value in EAX serves callers that read EAX as
             * one register - for files < 4 GB both readings agree. */
            c->Eax = r;
            c->Edx = (c->Edx & 0xFFFF0000u) | ((r >> 16) & 0xFFFFu);
            set_cf(c, 0);
        } else {
            set_cf(c, 1); c->Eax = 6;
        }
        break;
    }

    case 0x44:                                  /* IOCTL */
        if (g_verbose) log_call("INT21 IOCTL", c);
        if ((c->Eax & 0xFF) == 0x00) {          /* get device info */
            int hnd = (int)(c->Ebx & 0xFFFF);
            c->Edx = (hnd < 5) ? 0x80D3 : 0x0002;
        }
        set_cf(c, 0);
        break;

    case 0x48: {                                /* allocate DOS memory */
        /* Read the request from full EBX, not just BX: Watcom's
         * _ExpandDGROUP does `mov ebx, esi` with a byte count of 0x10000
         * (64 KiB segment), and taking only BX truncated that to 0. The old
         * "0 paras -> 16 bytes" fallback then handed out a 16-byte block
         * that the allocator wrote past (AV at 0x3DA0E -> address
         * 0x48FFFF8). A genuinely empty request fails on DOS as well, so
         * EBX = 0 is an error instead of a 16-byte block. */
        uint32_t paras = c->Ebx;
        uint32_t bytes;
        void *p;
        if (!paras) {
            printf("dos: INT21 alloc 0 paras -> rejected (DOS fails EBX=0)\n");
            c->Eax = 8; c->Ebx = 0;            /* insufficient memory */
            set_cf(c, 1);
            break;
        }
        bytes = paras << 4;
        /* The protected-mode caller dereferences the returned value directly,
         * so it must be a linear address - which also means the block does not
         * have to live below 1 MiB. AIL asks for 512 KiB at a time. */
        p = VirtualAlloc(NULL, bytes, MEM_RESERVE | MEM_COMMIT,
                         PAGE_READWRITE);
        if (p) {
            printf("dos: INT21 alloc %u paras -> linear 0x%p\n", paras, p);
            note_alloc((uint32_t)(uintptr_t)p, bytes, "INT21 alloc");
            c->Eax = (uint32_t)(uintptr_t)p;
            set_cf(c, 0);
        } else {
            printf("dos: INT21 alloc %u paras -> FAILED (%lu)\n",
                   paras, GetLastError());
            c->Eax = 8; c->Ebx = 0;
            set_cf(c, 1);
        }
        break;
    }
    case 0x49:
    case 0x4A:
        set_cf(c, 0);
        break;

    case 0x47:                                  /* get current directory */
        strcpy((char *)(uintptr_t)c->Esi, "\\");
        set_cf(c, 0);
        break;

    case 0x19: c->Eax = 2; set_cf(c, 0); break; /* current drive = C: */
    case 0x0E: c->Eax = 3; set_cf(c, 0); break; /* select drive      */

    case 0x1A:                                  /* set DTA */
        set_cf(c, 0);
        break;
    case 0x2F:
        c->Ebx = DOS_LOWMEM_SEG; c->Esi = 0x80; /* DTA in the low window */
        set_cf(c, 0);
        break;

    case 0x2A: {                                /* get date */
        SYSTEMTIME st;
        GetLocalTime(&st);
        c->Eax = (c->Eax & 0xFFFFFF00u) | (uint32_t)st.wDayOfWeek; /* AL = weekday */
        c->Ecx = ((uint32_t)st.wYear << 8) | st.wMonth;
        c->Edx = ((uint32_t)st.wDay << 8) | 0;
        set_cf(c, 0);
        break;
    }
    case 0x2C: {                                /* get time */
        SYSTEMTIME st;
        GetLocalTime(&st);
        c->Ecx = ((uint32_t)st.wHour << 8) | st.wMinute;
        c->Edx = ((uint32_t)st.wSecond << 8) | (st.wMilliseconds / 10);
        c->Eax &= 0xFFFFFF00u;                  /* DOS returns AL = 0 */
        set_cf(c, 0);
        break;
    }

    case 0x33: c->Eax = 0; set_cf(c, 0); break; /* ctrl-break state */
    case 0x51:
    case 0x62: c->Ebx = DOS_LOWMEM_SEG; set_cf(c, 0); break;  /* get PSP */

    case 0x0B:
    case 0x06:
    case 0x07:
    case 0x08:                                  /* console input */
        c->Eax &= 0xFFFFFF00u;                  /* no key waiting */
        set_cf(c, ah == 0x0B);
        break;

    case 0x67:                                  /* set handle count */
        set_cf(c, 0);
        break;

    case 0xFF: {
        /* DOS/4GW private probe. The startup code uses the result to decide
         * how to locate the PSP and the environment: AL=0 makes it assume an
         * unknown extender (and it then walks a null environment), non-zero
         * makes it read PSP:0x2C - which is exactly the path that works
         * against our low-memory mirror. */
        printf("dos: INT21 AH=FF probe -> reporting extender present\n");
        c->Eax = (c->Eax & 0xFFFFFF00u) | 0x01;
        set_cf(c, 0);
        break;
    }

    default:
        if (g_unknown[0x21] < 40) {
            printf("dos: UNHANDLED INT21 AH=%02X (cx=%X dx=%X si=%X di=%X)\n",
                   ah, (unsigned)c->Ecx, (unsigned)c->Edx,
                   (unsigned)c->Esi, (unsigned)c->Edi);
        }
        g_unknown[0x21]++;
        set_cf(c, 1);
        break;
    }
}

/* INT 31h - DPMI */
static void int31(CONTEXT *c)
{
    uint16_t ax = (uint16_t)c->Eax;
    g_calls[0x31]++;

    switch (ax) {
    case 0x0000:                                /* allocate LDT descriptors */
    case 0x0001:                                /* free descriptor          */
    case 0x0002:                                /* real mode seg -> desc    */
    case 0x0006:
    case 0x0007:
    case 0x0008:
    case 0x0009:
    case 0x000A:
    case 0x000B:
    case 0x000C:
    case 0x000D:
    case 0x000E:
        if (g_verbose) log_call("INT31 descriptor op", c);
        c->Eax = (c->Eax & 0xFFFF0000u) | 0x8001;   /* unsupported function */
        set_cf(c, 1);
        break;

    case 0x0100: {                              /* allocate DOS memory */
        /* Returns a real-mode paragraph in AX and a protected-mode selector in
         * DX. The selector cannot be a genuine LDT descriptor here, so the
         * host's flat data selector is handed out instead; callers that turn
         * the paragraph into a linear address (seg << 4) still get correct
         * memory, and callers that load DX get a valid selector. */
        uint32_t paras = c->Ebx & 0xFFFFu;
        uint32_t bytes = paras << 4;
        if (!bytes) bytes = 16;
        /* Reject allocations that would run past 1 MB or spill into the VGA
         * window (0xA0000-0xBFFFF). The pool pages themselves (0x80000-0x9FFFF
         * and 0xC0000-0xFFFFF) are committed up front by dos_init_lowmem(),
         * so no per-call VirtualAlloc is needed - and calling it would fail
         * on the pre-mapped pages. */
        if (g_lo_next + bytes > 0x00100000u ||
            (g_lo_next < 0xC0000u && g_lo_next + bytes > 0xA0000u)) {
            printf("dos: INT31 0100 alloc %u paras -> out of real-mode memory\n",
                   paras);
            c->Eax = 0x8011;                    /* out of memory */
            set_cf(c, 1);
            break;
        }
        printf("dos: INT31 0100 alloc %u paras -> seg 0x%X (linear 0x%X)\n",
               paras, g_lo_next >> 4, g_lo_next);
        note_alloc(g_lo_next, bytes, "INT31 0100");
        c->Eax = g_lo_next >> 4;
        c->Edx = c->SegDs ? (uint16_t)c->SegDs : 0x2B;
        g_lo_next = (g_lo_next + bytes + 0xFFFu) & ~0xFFFu;
        if (g_lo_next >= 0xA0000u && g_lo_next < 0xC0000u)
            g_lo_next = 0xC0000u;               /* keep clear of the VGA window */
        set_cf(c, 0);
        break;
    }
    case 0x0101:
        set_cf(c, 0);
        break;

    case 0x0200:                                /* get real mode vector */
    case 0x0201:
        c->Eax = 0; c->Ebx = 0;
        set_cf(c, 0);
        break;

    case 0x0300:                                /* simulate real mode int */
        if (g_verbose) log_call("INT31 0300 sim realmode int", c);
        c->Eax = 0x8001; set_cf(c, 1);
        break;

    case 0x0400:                                /* get DPMI version */
        c->Eax = (c->Eax & 0xFFFF0000u) | 0x0001;   /* v1.0 host */
        c->Ebx = 0x0001;                            /* 32-bit    */
        c->Ecx = 0;
        c->Edx = 0;
        set_cf(c, 0);
        break;
    case 0x0401:
        set_cf(c, 0);
        break;

    case 0x0500: {                              /* get free memory info */
        MEMORYSTATUS ms;
        GlobalMemoryStatus(&ms);
        c->Ebx = (uint32_t)(ms.dwAvailPhys / 0x10000);
        c->Edx = 0;
        c->Ecx = (uint32_t)(ms.dwAvailVirtual / 0x10000);
        set_cf(c, 0);
        break;
    }
    case 0x0501: {                              /* allocate memory block */
        /* DPMI: size in BX:CX (bytes), result linear address in BX:CX. */
        uint32_t size = ((c->Ebx & 0xFFFFu) << 16) | (c->Ecx & 0xFFFFu);
        void *p;
        if (size == 0) size = 0x1000;
        p = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (p) {
            uint32_t lin = (uint32_t)(uintptr_t)p;
            printf("dos: INT31 0501 alloc %u bytes -> linear 0x%X\n", size, lin);
            note_alloc(lin, size, "INT31 0501");
            c->Ebx = lin >> 16;
            c->Ecx = lin & 0xFFFFu;
            c->Esi = 1;                         /* 32-bit block */
            c->Eax = 0;
            set_cf(c, 0);
        } else {
            printf("dos: INT31 0501 alloc %u bytes FAILED\n", size);
            c->Eax = 0x8013;                    /* out of memory */
            set_cf(c, 1);
        }
        break;
    }
    case 0x0502:                                /* free memory block */
        set_cf(c, 0);
        break;
    case 0x0503: {                              /* resize memory block */
        uint32_t size = ((c->Ebx & 0xFFFFu) << 16) | (c->Ecx & 0xFFFFu);
        void *p = VirtualAlloc(NULL, size ? size : 0x1000,
                               MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (p) {
            uint32_t lin = (uint32_t)(uintptr_t)p;
            c->Ebx = lin >> 16;
            c->Ecx = lin & 0xFFFFu;
            c->Esi = 1;
            set_cf(c, 0);
        } else {
            c->Eax = 0x8013;
            set_cf(c, 1);
        }
        break;
    }

    case 0x0600:
    case 0x0601:
        set_cf(c, 0);
        break;

    case 0x0702:
    case 0x0703:
        set_cf(c, 0);
        break;

    case 0x0800:                                /* physical addr mapping */
        c->Eax = 0x8001; set_cf(c, 1);
        break;

    case 0x0900:                                /* get descriptor */ {
        uint16_t sel = (uint16_t)c->Ebx;
        c->Eax = 0x8001; set_cf(c, 1);
        (void)sel;
        break;
    }
    case 0x0901:
    case 0x0902:
        set_cf(c, 0);
        break;

    case 0x0A00:                                /* get vendor info */
        c->Eax = 0;
        c->Ebx = 0;
        c->Ecx = 0x46443231;                    /* "FD21" */
        c->Edx = 0;
        set_cf(c, 0);
        break;

    case 0x0B00:
        c->Eax = 0x8001; set_cf(c, 1);
        break;

    case 0x0E00:                                /* coprocessor status */
        c->Eax = 0; set_cf(c, 0);
        break;

    default:
        if (g_unknown[0x31] < 40)
            printf("dos: UNHANDLED INT31 AX=%04X ebx=%X ecx=%X edx=%X\n",
                   ax, (unsigned)c->Ebx, (unsigned)c->Ecx, (unsigned)c->Edx);
        g_unknown[0x31]++;
        c->Eax = 0x8001; set_cf(c, 1);
        break;
    }
}

/* INT 10h - video BIOS. The frame buffer is real (mapped at 0xA0000), only the
 * mode setting has to be answered. */
static void int10(CONTEXT *c)
{
    uint8_t ah = (uint8_t)(c->Eax >> 8);
    g_calls[0x10]++;

    switch (ah) {
    case 0x00:
        printf("dos: INT10 set video mode 0x%02X\n", (unsigned)(c->Eax & 0xFF));
        break;
    case 0x0F:
        c->Eax = (c->Eax & 0xFFFFFF00u) | 0x13;
        c->Ebx = (c->Ebx & 0xFFFF0000u) | 0x0000;
        break;
    case 0x10:
        break;
    case 0x1C:
        break;
    case 0x12: {
        uint32_t bx = c->Ebx & 0xFFFF;
        if (bx == 0x0000) { /* no SVGA: report unsupported */ }
        break;
    }
    default:
        if (g_unknown[0x10] < 20)
            printf("dos: UNHANDLED INT10 AH=%02X\n", ah);
        g_unknown[0x10]++;
        break;
    }
}

/* INT 16h - BIOS keyboard services.
 *
 * The game's menus read keys with int 16h AH=10h (read extended key) and
 * compare the scan code that comes back in AH against arrow keys (0x4B left /
 * 0x4D right / ...). With no handler for vector 0x16 the call left AX
 * untouched, so the game kept seeing the function number 0x10 in AH as if it
 * were a scan code and every menu key looked dead - while the title-screen
 * skip still worked, because that path only polls the BDA head/tail pointers
 * (sub_10620). */
static uint16_t kbd_fetch(int wait_ms)
{
    uint8_t *lm = g_lowmem;
    int waited = 0;

    for (;;) {
        uint16_t head = (uint16_t)(lm[0x41A] | (lm[0x41B] << 8));
        uint16_t tail = (uint16_t)(lm[0x41C] | (lm[0x41D] << 8));

        if (head != tail) {
            /* Buffer layout is [ascii][scan], exactly as the BIOS leaves it:
             * for extended keys the ascii byte holds 0xE0. */
            uint16_t key = (uint16_t)(lm[head] | (lm[head + 1] << 8));
            head = (uint16_t)(head + 2);
            if (head >= 0x43E)
                head = 0x41E;
            lm[0x41A] = (uint8_t)(head & 0xFF);
            lm[0x41B] = (uint8_t)(head >> 8);
            return key;
        }
        if (waited >= wait_ms)
            return 0;                           /* caller may retry     */
        Sleep(2);
        waited += 2;
    }
}

static void int16(CONTEXT *c)
{
    uint8_t ah = (uint8_t)(c->Eax >> 8);
    g_calls[0x16]++;

    switch (ah) {
    case 0x00:                                  /* read key              */
    case 0x10: {                                /* read extended key     */
        uint16_t key = kbd_fetch(100);
        c->Eax = (c->Eax & 0xFFFF0000u) | key;
        break;
    }
    case 0x01:                                  /* key available?        */
    case 0x11: {                                /* extended, available?  */
        uint8_t *lm = g_lowmem;
        uint16_t head = (uint16_t)(lm[0x41A] | (lm[0x41B] << 8));
        uint16_t tail = (uint16_t)(lm[0x41C] | (lm[0x41D] << 8));
        if (head != tail) {
            uint16_t key = (uint16_t)(lm[head] | (lm[head + 1] << 8));
            c->Eax = (c->Eax & 0xFFFF0000u) | key;
            c->EFlags &= ~0x40u;                /* ZF=0: key ready       */
        } else {
            c->Eax &= 0xFFFF0000u;
            c->EFlags |= 0x40u;                 /* ZF=1: nothing pending */
        }
        break;
    }
    case 0x02:                                  /* shift/status flags    */
        c->Eax = (c->Eax & 0xFFFF0000u) | g_lowmem[0x417];
        break;
    default:
        break;
    }
}

/* INT 33h - mouse */
static void int33(CONTEXT *c)
{
    uint16_t ax = (uint16_t)c->Eax;
    g_calls[0x33]++;
    switch (ax) {
    case 0x0000: c->Eax = (c->Eax & 0xFFFF0000u) | 0xFFFF; c->Ebx = 0; break;
    case 0x0003: c->Ebx = 0; break;
    default: break;
    }
}

/* INT 2Fh - multiplex */
static void int2f(CONTEXT *c)
{
    g_calls[0x2F]++;
    c->Eax = (c->Eax & 0xFFFF0000u) | 0x0000;   /* not supported */
}

/* --------------------------------------------------- privileged port I/O */

static int emulate_priv_instr(CONTEXT *c, const uint8_t *p)
{
    /* handles the forms DOS/4GW and AIL use: in/out with immediate or DX port */
    int handled = 0;
    uint16_t port = 0;
    switch (p[0]) {
    /* interrupt flag manipulation and halt: nothing to mask or wait for in a
     * user-mode host, so they become no-ops. */
    case 0xFA:                                  /* cli */
    case 0xFB:                                  /* sti */
        return 1;
    case 0xF4:                                  /* hlt */
        Sleep(1);
        return 1;

    case 0xE4: port = p[1];                       handled = 2; break; /* in  al, imm8  */
    case 0xE5: port = p[1];                       handled = 2; break; /* in  ax/eax, imm8 */
    case 0xE6: port = p[1];                       handled = 2; break; /* out imm8, al  */
    case 0xE7: port = p[1];                       handled = 2; break; /* out imm8, ax/eax */
    case 0xEC: port = (uint16_t)c->Edx;           handled = 1; break; /* in  al, dx    */
    case 0xED: port = (uint16_t)c->Edx;           handled = 1; break; /* in  ax/eax, dx */
    case 0xEE: port = (uint16_t)c->Edx;           handled = 1; break; /* out dx, al    */
    case 0xEF: port = (uint16_t)c->Edx;           handled = 1; break; /* out dx, ax/eax */

    case 0x0F:                                  /* system instructions */
        if (p[1] == 0x06) return 2;              /* clts   */
        if (p[1] == 0x08 || p[1] == 0x09) return 2; /* invd/wbinvd */
        if (p[1] == 0x01) {                      /* lgdt/lidt/lmsw/smsw/... */
            switch ((p[2] >> 3) & 7) {
            case 2: return 3;                    /* lgdt - ignore descriptor table */
            case 3: return 3;                    /* lidt - ignore idt              */
            case 6: return 3;                    /* lmsw - ignore machine status   */
            default: break;
            }
        }
        return 0;

    default: return 0;
    }

    switch (port) {
    /* VGA DAC palette: 0x3C8 index, 0x3C9 data (R,G,B triplets) */
    case 0x03C8:
        if (p[0] == 0xE6 || p[0] == 0xEE) {          /* out 3C8, al */
            g_dac_index = c->Eax & 0xFF;
            g_dac_component = 0;
        }
        break;
    case 0x03C9:
        if (p[0] == 0xE6 || p[0] == 0xEE) {          /* out 3C9, al */
            /* The VGA DAC is 6 bits per channel (0..63); the register only
             * honours the low 6 bits. The stored table is kept at 8 bits per
             * channel, so stretch 0..63 -> 0..255 here - reading the raw
             * value as 8 bits made every frame a quarter as bright (and half
             * as saturated) as the original game. */
            uint8_t v = (uint8_t)(c->Eax & 0xFF) & 0x3Fu;
            if (g_dac_index >= 0 && g_dac_index < 256) {
                dos_palette[g_dac_index * 3 + g_dac_component] =
                    (uint8_t)((v << 2) | (v >> 4));
                dos_palette_dirty = 1;
            }
            if (++g_dac_component == 3) {
                g_dac_component = 0;
                g_dac_index = (g_dac_index + 1) & 0xFF;
            }
        }
        break;
    case 0x03C0:
    case 0x03C2:
    case 0x03C4:
    case 0x03CE:
    case 0x03D4:
    case 0x03D5:
        /* mode/timing registers are irrelevant with a real frame buffer */
        break;
    case 0x03DA:            /* input status: return "display enabled" */
        c->Eax = (c->Eax & 0xFFFFFF00u) | 0x09;
        break;
    case 0x0040: case 0x0043:   /* PIT */
        break;
    case 0x0020: case 0x0021:   /* PIC */
        break;
    case 0x0060: case 0x0064:   /* keyboard controller */
        if (port == 0x0064 && p[0] == 0xEC) c->Eax = (c->Eax & 0xFFFFFF00u) | 0x14;
        break;
    default:
        printf("dos: port %s 0x%04X (ignored) al/ax=%02X\n",
               (p[0] == 0xEC || p[0] == 0xED || p[0] == 0xE4 || p[0] == 0xE5) ? "IN " : "OUT",
               port, (unsigned)(c->Eax & 0xFF));
        break;
    }
    g_port_ops++;
    return handled;
}

/* --------------------------------------------------------------- VEH hook */

static uint32_t *ctx_reg(CONTEXT *c, int reg)
{
    switch (reg) {
    case 0: return &c->Eax;
    case 1: return &c->Ecx;
    case 2: return &c->Edx;
    case 3: return &c->Ebx;
    case 4: return &c->Esp;
    case 5: return &c->Ebp;
    case 6: return &c->Esi;
    case 7: return &c->Edi;
    }
    return NULL;
}

/* The handler must never fault while inspecting a fault. Every read of guest
 * memory goes through this: a wrong EIP (the game jumping to 0, say) used to
 * make the scan around EIP read 0xFFFFFFF8 and crash the handler itself, which
 * then looked like "EIP is inside fd2_veh". */
static int guest_readable(const void *p, size_t n)
{
    MEMORY_BASIC_INFORMATION mbi;
    const char *base, *end;
    if (!p) return 0;
    if (!VirtualQuery(p, &mbi, sizeof mbi)) return 0;
    if (mbi.State != MEM_COMMIT) return 0;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return 0;
    base = (const char *)mbi.BaseAddress;
    end  = base + mbi.RegionSize;
    return ((const char *)p + n) <= end;
}

/* Minimal decoder for the forms the DOS/4GW startup code uses to reach PSP and
 * BIOS data through register-indirect addressing such as `mov cl,es:[edi-1]`.
 * Those cannot be fixed by rewriting an immediate, so the access is executed
 * against the low-memory window and the instruction is stepped over. */
static int emulate_lowmem_access(CONTEXT *c)
{
    const uint8_t *start = (const uint8_t *)(uintptr_t)c->Eip;
    const uint8_t *p = start;
    int size = 0;
    uint8_t modrm, mod, rm;
    int32_t disp = 0;
    uint32_t ea, baseval = 0;
    uint32_t *dst;
    uint32_t v = 0;
    int base = -1;

    while (*p == 0x26 || *p == 0x2E || *p == 0x36 || *p == 0x3E ||
           *p == 0x64 || *p == 0x65 || *p == 0x66 || *p == 0x67)
        p++;

    if (*p == 0x8A)      { size = 1; p++; }
    else if (*p == 0x8B) { size = 4; p++; }
    else if (p[0] == 0x0F && p[1] == 0xB6) { size = 1; p += 2; }
    else if (p[0] == 0x0F && p[1] == 0xB7) { size = 2; p += 2; }
    else return 0;

    modrm = *p++;
    mod = (uint8_t)(modrm >> 6);
    rm  = (uint8_t)(modrm & 7);
    if (mod == 3) return 0;
    if (rm == 4) return 0;                  /* SIB not needed here */

    if (rm == 5 && mod == 0) {
        disp = *(const int32_t *)p; p += 4;
    } else {
        base = rm;
    }
    if (mod == 1)      { disp = *(const int8_t *)p; p += 1; }
    else if (mod == 2) { disp = *(const int32_t *)p; p += 4; }

    if (base >= 0) {
        uint32_t *r = ctx_reg(c, base);
        if (!r) return 0;
        baseval = *r;
    }
    ea = baseval + (uint32_t)disp;
    if (ea >= 0x10000) return 0;

    memcpy(&v, g_lowmem + ea, (size_t)size);
    dst = ctx_reg(c, (modrm >> 3) & 7);
    if (!dst) return 0;
    if (size == 1)      *dst = (*dst & 0xFFFFFF00u) | (v & 0xFF);
    else if (size == 2) *dst = (*dst & 0xFFFF0000u) | (v & 0xFFFF);
    else                *dst = v;

    printf("dos: lowmem read 0x%X (size %d) -> 0x%X, skipping %d bytes at 0x%X\n",
           ea, size, v, (int)(p - start), (unsigned)(uintptr_t)start);
    c->Eip = (DWORD)(uintptr_t)p;
    return 1;
}

/* Service a software interrupt found at EIP: `int NN` occupies `len` bytes
 * (prefixes + 2). Counted and traced exactly like the old trap sites. */
static void dispatch_swint(CONTEXT *c, uint8_t vec, uint32_t len)
{
    DWORD addr = (DWORD)(uintptr_t)c->Eip;

    g_last_trap_eip = addr;
    g_int_sites++;
    trap_note(vec, addr, c);
    c->Eip += len;                            /* skip prefixes + int NN */
    if (g_trace_left > 0) {
        g_trace_left--;
        printf("[int %02X] at 0x%X ax=%04X bx=%04X cx=%04X dx=%04X "
               "si=%04X di=%04X bp=%04X ds=%04X es=%04X\n",
               vec, addr, (unsigned)(c->Eax & 0xFFFF),
               (unsigned)(c->Ebx & 0xFFFF), (unsigned)(c->Ecx & 0xFFFF),
               (unsigned)(c->Edx & 0xFFFF), (unsigned)(c->Esi & 0xFFFF),
               (unsigned)(c->Edi & 0xFFFF), (unsigned)(c->Ebp & 0xFFFF),
               (unsigned)c->SegDs, (unsigned)c->SegEs);
    }
    switch (vec) {
    case 0x21: int21(c); break;
    case 0x31: int31(c); break;
    case 0x10: int10(c); break;
    case 0x16: int16(c); break;
    case 0x33: int33(c); break;
    case 0x2F: int2f(c); break;
    case 0x08: case 0x09: case 0x1A:
        g_calls[vec] = g_calls[vec];          /* ignore hardware vectors */
        break;
    default:
        if (!g_calls[vec] && !g_unknown[vec])
            printf("dos: servicing unhandled int %02X at 0x%X - skipping\n",
                   vec, addr);
        g_unknown[vec]++;
        break;
    }
}

static LONG CALLBACK fd2_veh(EXCEPTION_POINTERS *ep)
{
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    CONTEXT *c = ep->ContextRecord;

    /* A software interrupt is a fault whose EIP points *at* the `CD` byte
     * (measured by probe4.c: EXCEPTION_ACCESS_VIOLATION for every vector,
     * EXCEPTION_BREAKPOINT for int 3). Take it before the ring recording and
     * before the access-violation rules, otherwise the millions of int 21h the
     * game makes would flood the ring and be mistaken for real memory faults. */
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
        er->ExceptionCode == EXCEPTION_PRIV_INSTRUCTION ||
        er->ExceptionCode == EXCEPTION_BREAKPOINT) {
        const uint8_t *q = (const uint8_t *)(uintptr_t)c->Eip;
        if (guest_readable(q, 8)) {
            const uint8_t *s = q;
            while (*s == 0x26 || *s == 0x2E || *s == 0x36 || *s == 0x3E ||
                   *s == 0x64 || *s == 0x65 || *s == 0x66 || *s == 0x67)
                s++;
            if (*s == 0xCD) {
                dispatch_swint(c, s[1], (uint32_t)(s - q) + 2);
                if (dos_exit_requested) {
                    printf("dos: game requested exit\n");
                    ExitProcess(0);
                }
                if (g_trace_mode && g_trace_left > 0)
                    c->EFlags |= 0x100u;      /* start single-stepping */
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
    }

    /* Record everything that is not an interrupt service: privileged
     * instructions, access violations and stray breakpoints are what a crash
     * report needs in order to show how the flow went off the rails. */
    veh_record((uint32_t)c->Eip, er->ExceptionCode,
               (uint32_t)er->ExceptionInformation[0]);

    /* instruction trace mode: single-step and log, used to follow the DOS/4GW
     * startup code where the disassembly assumptions break down */
    if (er->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        if (g_trace_left > 0) {
            g_trace_left--;
            printf("t %08X eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X ebp=%08X\n",
                   (unsigned)c->Eip, (unsigned)c->Eax, (unsigned)c->Ebx,
                   (unsigned)c->Ecx, (unsigned)c->Edx, (unsigned)c->Esi,
                   (unsigned)c->Edi, (unsigned)c->Ebp);
            c->EFlags |= 0x100u;                /* keep single-stepping */
        } else {
            c->EFlags &= ~0x100u;               /* stop tracing */
        }
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    /* A breakpoint that is not an `int NN` is a stray 0xCC (execution landed
     * on data): it falls through to the crash report below. */

    if (er->ExceptionCode == EXCEPTION_PRIV_INSTRUCTION) {
        const uint8_t *p = (const uint8_t *)(uintptr_t)c->Eip;
        int len = emulate_priv_instr(c, p);
        if (len) {
            c->Eip += len;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        printf("cpu: unimplemented privileged instruction %02X at 0x%X\n",
               p[0], (unsigned)c->Eip);
    }

    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        const uint8_t *p = (const uint8_t *)(uintptr_t)c->Eip;
        ULONG_PTR fault = er->ExceptionInformation[1];

        /* If the game jumped somewhere unmapped we cannot inspect the
         * instruction at all - report honestly instead of faulting inside the
         * handler (that is what produced the bogus "EIP is in fd2_veh"). */
        if (!guest_readable(p, 16)) {
            printf("cpu: fault at unreadable EIP=0x%X (%s address 0x%zX)\n",
                   (unsigned)c->Eip,
                   fault_kind(er->ExceptionInformation[0]),
                   (size_t)fault);
            printf("     the game jumped into memory that is not mapped\n");
            printf("     eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X ebp=%08X esp=%08X\n",
                   (unsigned)c->Eax, (unsigned)c->Ebx, (unsigned)c->Ecx,
                   (unsigned)c->Edx, (unsigned)c->Esi, (unsigned)c->Edi,
                   (unsigned)c->Ebp, (unsigned)c->Esp);
            printf("     last trapped call was at 0x%X\n",
                   (unsigned)g_last_trap_eip);
            stack_dump((uint32_t)c->Esp);
            {
                /* A jump to 0 is usually `call dword ptr [reg]` through a table
                 * slot that was never filled - show what EAX/EDX point at. */
                uint32_t regs[2];
                int r;
                regs[0] = (uint32_t)c->Eax;
                regs[1] = (uint32_t)c->Edx;
                for (r = 0; r < 2; r++) {
                    const uint32_t *q = (const uint32_t *)(uintptr_t)regs[r];
                    int k;
                    if (!guest_readable(q, 16 * 4)) continue;
                    printf("     [%s=0x%08X]:", r ? "EDX" : "EAX", regs[r]);
                    for (k = 0; k < 8; k++) printf(" %08X", q[k]);
                    printf("\n");
                }
            }
            trap_dump();
            dos_dump_stats();
            ExitProcess(6);
        }

        /* (1) Loading a DOS/4GW-private selector. The extender owns its own
         * GDT/LDT layout, so its flat data selector (usually 0x24) and the
         * "real mode window" selector do not exist here - and in this process
         * GDT slot 4 is the code segment. Substitute the host's flat data
         * selector and let the instruction run again. */
        while (*p == 0x26 || *p == 0x2E || *p == 0x36 || *p == 0x3E ||
               *p == 0x64 || *p == 0x65 || *p == 0x66 || *p == 0x67)
            p++;
        if (p[0] == 0x8E) {
            uint8_t modrm = p[1];
            uint16_t flat = c->SegDs ? (uint16_t)c->SegDs : 0x2B;
            if ((modrm & 0xC0) == 0xC0) {
                uint32_t *r = ctx_reg(c, modrm & 7);
                if (r) {
                    if (g_trace_left > 0)
                        printf("dos: seg load reg 0x%04X -> flat 0x%04X at 0x%X\n",
                               (unsigned)(*r & 0xFFFF), flat, (unsigned)c->Eip);
                    *r = (*r & 0xFFFF0000u) | flat;
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
            } else if ((modrm & 0xC7) == 0x05) {
                uint32_t disp = *(const uint32_t *)(p + 2);
                if (disp < 0x100000) {
                    if (g_trace_left > 0)
                        printf("dos: seg load [0x%X]=0x%04X -> flat 0x%04X at 0x%X\n",
                               disp, *(const uint16_t *)(uintptr_t)disp, flat,
                               (unsigned)c->Eip);
                    *(uint16_t *)(uintptr_t)disp = flat;
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
            }
        }

        /* (2) Absolute access into the first 64 KiB. Windows keeps that range
         * unmapped, so rewrite the operand to point at the low-memory window
         * and retry. The immediate is searchable around EIP because absolute
         * operands in this flat-model code are always 32-bit. */
        if (fault < 0x10000) {
            int off;
            for (off = -8; off <= 4; off++) {
                uint32_t *w = (uint32_t *)(uintptr_t)(p + off);
                if (!guest_readable(w, 4))
                    continue;
                if (*w == (uint32_t)fault) {
                    *w = (uint32_t)fault + DOS_LOWMEM_BASE;
                    printf("dos: lowmem 0x%zX -> 0x%zX (operand at 0x%X)\n",
                           (size_t)fault, (size_t)fault + DOS_LOWMEM_BASE,
                           (unsigned)(c->Eip + off));
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
            }
            /* (3) register-indirect access to low memory: emulate the load */
            if (emulate_lowmem_access(c))
                return EXCEPTION_CONTINUE_EXECUTION;
            printf("cpu: unmatched low-memory access: fault=0x%zX eip=0x%X "
                   "bytes=%02X %02X %02X %02X %02X %02X %02X %02X "
                   "esi=%08X edi=%08X ecx=%08X\n",
                   (size_t)fault, (unsigned)c->Eip,
                   p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
                   (unsigned)c->Esi, (unsigned)c->Edi, (unsigned)c->Ecx);
        }

        printf("cpu: ACCESS VIOLATION at 0x%X (Eip=0x%X) %s address 0x%zX\n",
               (unsigned)(uintptr_t)er->ExceptionAddress, (unsigned)c->Eip,
               fault_kind(er->ExceptionInformation[0]),
               (size_t)fault);
        {
            const char *owner = find_alloc((uint32_t)c->Eip);
            printf("     Eip is %s; last trapped call was at 0x%X\n",
                   owner ? owner : "NOT in any allocated block",
                   (unsigned)g_last_trap_eip);
        }
        {
            uint32_t start = (c->Eip >= 16) ? (c->Eip - 16) : 0;
            const uint8_t *q = (const uint8_t *)(uintptr_t)(start & ~0xFu);
            int k;
            if (guest_readable(q, 48)) {
                printf("     memory @0x%X:", (unsigned)(uintptr_t)q);
                for (k = 0; k < 48; k++) printf(" %02X", q[k]);
                printf("\n");
            } else {
                printf("     memory around Eip is not readable\n");
            }
        }
        {
            int k;
            /* RLE decompressor state (obj2 globals written by the image
             * blitter): w=320 h=200 means the resource header itself is
             * sane; anything else points at bad source data. */
            if (guest_readable((const void *)(uintptr_t)0x627B4u, 4))
                printf("     RLE w=%u h=%u (@0x627B4)\n",
                       *(const uint16_t *)(uintptr_t)0x627B4u,
                       *(const uint16_t *)(uintptr_t)0x627B6u);
            if (guest_readable((const void *)(uintptr_t)c->Esi, 16)) {
                const uint8_t *s = (const uint8_t *)(uintptr_t)c->Esi;
                printf("     [ESI] source:");
                for (k = 0; k < 16; k++) printf(" %02X", s[k]);
                printf("\n");
            }
            if (guest_readable((const void *)(uintptr_t)c->Esp, 4))
                printf("     [ESP]=0x%X (return address)\n",
                       *(const uint32_t *)(uintptr_t)c->Esp);
            if (guest_readable((const void *)(uintptr_t)(c->Ebp + 8), 24)) {
                const uint32_t *a = (const uint32_t *)(uintptr_t)(c->Ebp + 8);
                printf("     frame args: %08X %08X %08X %08X %08X %08X\n",
                       a[0], a[1], a[2], a[3], a[4], a[5]);
            }
        }
        {
            int k;
            printf("     allocated blocks (%d):", g_alloc_count);
            for (k = 0; k < g_alloc_count && k < 12; k++)
                printf(" [%X+%X %s]", g_allocs[k].base, g_allocs[k].size,
                       g_allocs[k].what);
            printf("\n");
        }
        printf("     eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X ebp=%08X\n",
               (unsigned)c->Eax, (unsigned)c->Ebx, (unsigned)c->Ecx,
               (unsigned)c->Edx, (unsigned)c->Esi, (unsigned)c->Edi,
               (unsigned)c->Ebp);
        printf("     segs: cs=%04X ds=%04X es=%04X fs=%04X gs=%04X ss=%04X\n",
               (unsigned)c->SegCs, (unsigned)c->SegDs, (unsigned)c->SegEs,
               (unsigned)c->SegFs, (unsigned)c->SegGs, (unsigned)c->SegSs);
        {
            const uint8_t *p = (const uint8_t *)(uintptr_t)c->Eip;
            printf("     code @Eip: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                   p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        }
        printf("     [0x52810]=%04X [0x527F0]=%08X [0x52832]=%04X\n",
               *(const uint16_t *)(uintptr_t)0x52810,
               *(const uint32_t *)(uintptr_t)0x527F0,
               *(const uint16_t *)(uintptr_t)0x52832);
        dos_dump_stats();
        ExitProcess(3);
    }

    {
        const uint8_t *p = (const uint8_t *)(uintptr_t)c->Eip;
        printf("cpu: unhandled exception %08lX at 0x%X\n",
               (unsigned long)er->ExceptionCode, (unsigned)c->Eip);
        printf("     eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X ebp=%08X esp=%08X\n",
               (unsigned)c->Eax, (unsigned)c->Ebx, (unsigned)c->Ecx,
               (unsigned)c->Edx, (unsigned)c->Esi, (unsigned)c->Edi,
               (unsigned)c->Ebp, (unsigned)c->Esp);
        printf("     segs: cs=%04X ds=%04X es=%04X ss=%04X\n",
               (unsigned)c->SegCs, (unsigned)c->SegDs, (unsigned)c->SegEs,
               (unsigned)c->SegSs);
        if (guest_readable(p, 16))
            printf("     code @Eip: %02X %02X %02X %02X %02X %02X %02X %02X "
                   "%02X %02X %02X %02X %02X %02X %02X %02X\n",
                   p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
                   p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
        else
            printf("     code @Eip is not readable\n");
        if (guest_readable((const void *)(uintptr_t)c->Esp, 8 * 4)) {
            const uint32_t *sp = (const uint32_t *)(uintptr_t)c->Esp;
            printf("     stack: %08X %08X %08X %08X %08X %08X %08X %08X\n",
                   sp[0], sp[1], sp[2], sp[3], sp[4], sp[5], sp[6], sp[7]);
        }
        printf("     last trapped call at 0x%X\n", (unsigned)g_last_trap_eip);
        veh_dump_ring();
        dos_dump_stats();
        ExitProcess(4);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void dos_install_traps(void)
{
    if (!AddVectoredExceptionHandler(1, fd2_veh)) {
        fprintf(stderr, "dos: AddVectoredExceptionHandler failed: %lu\n",
                GetLastError());
    } else {
        printf("dos: vectored exception handler installed\n");
        bios_tick_start();
    }
}

void dos_set_image(le_image *le)
{
    g_le = le;
}

void dos_enable_trace(int instructions)
{
    g_trace_mode = 1;
    g_trace_left = instructions;
}

void dos_dump_stats(void)
{
    int i;
    printf("--- interrupt statistics (software ints serviced: %d, port ops: %u) ---\n",
           g_int_sites, g_port_ops);
    for (i = 0; i < 256; i++) {
        if (g_calls[i] || g_unknown[i])
            printf("    int %02X: calls=%u unknown=%u\n", i, g_calls[i], g_unknown[i]);
    }
}
