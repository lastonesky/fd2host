/* dos.c - traps and services for the native FD2 host.
 *
 * Strategy
 * --------
 * The game is 32-bit flat-model DOS/4GW code. All of its environment access
 * happens through `int NN` instructions, privileged port instructions and the
 * 0xA0000 frame buffer. None of that may run as-is in a user process, so
 * this module services the original interrupt with host OS facilities - files,
 * time, threads and process control all go through src/platform.h. Privileged
 * IN/OUT instructions are caught by the fault handler and emulated.
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
 * is left byte-for-byte intact. src/faultprobe32.c measured the Linux side:
 * `int NN` arrives as SIGSEGV with si_code=SI_KERNEL, si_addr=NULL and EIP at
 * the instruction too - the same rule works (see dos_fault.h for the full
 * mapping; the instruction bytes, not the signal, tell int/privileged/
 * segment faults apart).
 *
 * That is a *replacement* of the DOS/flat-hardware interfaces, not a DOS
 * emulator: there is no real-mode CPU, no interrupt controller, no option ROM.
 */

#include "dos.h"
#include "le.h"
#include "dos_fault.h"
#include "host.h"        /* host_exit_after_remaining: bound a spawned child */
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
static int       g_child_exit;     /* exit code of an AH=4B child (AH=4D)     */

/* The game's own keyboard ISR (INT 9), installed with INT 21h AH=25h AL=09h.
 * Windows never raises hardware interrupts in this process, so a game that
 * hooks INT 9 itself - FDPS does (sub_56560 -> sub_565A7, which reads port
 * 0x60 and pushes the make code into its own 10-entry queue) - would wait
 * forever for a key. dos_deliver_key() queues the scan code and the VEH
 * injects it as a real interrupt on the next guest instruction boundary.
 * 0 = not hooked (deliver through the BIOS buffer instead, which is what
 * FD2 uses). */
static uint32_t  g_guest_int9;
static uint8_t   g_kbd_last_sc;    /* what `in al,60h` returns next        */
static uint32_t  g_isr_lo, g_isr_hi;  /* EIP window of the injected handler  */
static int       g_isr_log_left;   /* exceptions left to log after a key    */
static uint64_t  g_guest_tid;      /* the thread that executes guest code  */

/* Ring of the most recent *unusual* VEH events (privileged instructions, access
 * violations, stray breakpoints). Ordinary int3 trap sites are excluded - there
 * are millions of those. When the flow goes off the rails this shows which
 * handler ran, and where the instruction pointer was just before. */
#define VEH_RING 32
static struct { uint32_t eip, code, info; } g_veh_ring[VEH_RING];
static unsigned g_veh_pos;

static void veh_record(uint32_t eip, uint32_t code, uint32_t info)
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
    plat_file h;
    uint32_t  pos;      /* DOS file position: owned here, not by the OS  */
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

static void trap_note(uint8_t vec, uint32_t eip, const dos_ctx *c)
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
static uint32_t g_lo_next;                 /* set by dos_choose_lowmem()   */
uint32_t dos_lowmem_base = 0x00070000u;    /* FD2 layout; may be moved */

/* Choose where the 64 KiB low-memory mirror goes (see dos.h). Kept above the
 * game's objects so an LE image that owns 0x70000 (FDPS obj2) does not make
 * dos_init_lowmem()'s VirtualAlloc fail - which used to return early and left
 * g_lowmem NULL *and* the file table uninitialised. */
void dos_choose_lowmem(uint32_t game_end)
{
    uint32_t base = 0x00070000u;

    /* VirtualAlloc's allocation granularity is 64 KiB: an address that is not
     * a multiple of 0x10000 is rounded DOWN, so a mirror at 0x71000 would
     * collide with an object at 0x70000 and fail with ERROR 487 (that is
     * exactly how FDPS broke it - its obj2 lives at 0x70000, 0x54 bytes). */
    if (game_end > base)
        base = (game_end + 0xFFFFu) & ~0xFFFFu;        /* 64 KiB aligned */

    if (base >= 0x000A0000u && base < 0x000C0000u)
        base = 0x000C0000u;          /* no room before VGA: use the ROM area */
    if (base + DOS_LOWMEM_SIZE > 0x00100000u)
        base = 0x00070000u;          /* last resort: the FD2 location */

    if (base != dos_lowmem_base)
        printf("dos: low-memory window moved 0x%X -> 0x%X (seg 0x%X) to clear "
               "the game's objects\n",
               (unsigned)dos_lowmem_base, (unsigned)base, (unsigned)(base >> 4));

    dos_lowmem_base = base;
    g_lo_next = base + DOS_LOWMEM_SIZE;  /* real-mode pool starts after it */
}

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
static int g_dac_logged;

static void files_init(void)
{
    int i;
    memset(g_files, 0, sizeof g_files);
    /* 0=stdin 1=stdout 2=stderr 3=stdaux 4=stdprn
     *
     * Ask the CRT for the OS handle behind fd 0/1/2 rather than the STD_*
     * slots: main() freopen()s stdout/stderr onto host.log, which re-points
     * fd 1/2 but does NOT update them - on Windows GetStdHandle() handed the
     * guest a NULL handle and every game printf was written nowhere
     * (observed as `dos: write h=1 want=39 n=0`). On POSIX the fd *is* the
     * handle, so plat_console_file(i) is exactly what the CRT redirected. */
    for (i = 0; i < 5; i++) {
        g_files[i].used = 1;
        g_files[i].is_dev = 1;
        g_files[i].h = (i < 3) ? plat_console_file(i) : PLAT_FILE_INVALID;
    }
    printf("dos: console handles stdin=%p stdout=%p stderr=%p\n",
           (void *)(uintptr_t)g_files[0].h,
           (void *)(uintptr_t)g_files[1].h,
           (void *)(uintptr_t)g_files[2].h);
}

static int file_alloc(plat_file h, const char *name)
{
    int i;
    for (i = 5; i < DOS_MAX_FILES; i++) {
        if (!g_files[i].used) {
            g_files[i].used = 1;
            g_files[i].is_dev = 0;
            g_files[i].h = h;
            g_files[i].pos = 0;        /* a fresh handle starts at BOF -
                                        * the OS position used to provide
                                        * this for free (ReadFile/CreateFile) */
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

/* Platform error -> DOS error code, mapped per OS in platform.h
 * (plat_error_to_dos): sopen() only really distinguishes 2 (file not found) -
 * it is the trigger for the O_CREAT fallback - but reporting the plausible
 * code makes failures easier to read in the log. */

/* --------------------------------------------------------- low memory init */

/* Map a range inside the guest window. The whole 0x10000..0xFFFFF window is
 * already reserved by le_reserve_address_space_early(), so a RESERVE request
 * there fails with ERROR_INVALID_ADDRESS (487) and only COMMIT works - try
 * both, like le.c's map_at(). */
static uint8_t *map_low(uint32_t base, uint32_t size, const char *what)
{
    /* Region by region: a single MEM_COMMIT spanning the early reservation's
     * separate 64 KiB blocks fails with 487 (see le_commit_range). */
    if (le_commit_range(base, size, PLAT_PROT_RW, what) != 0)
        return NULL;
    return (uint8_t *)(uintptr_t)base;
}

void dos_init_lowmem(void)
{
    if (!g_lo_next)
        g_lo_next = dos_lowmem_base + DOS_LOWMEM_SIZE;   /* no dos_choose_lowmem() call */

    /* Console handles first: if the mirror mapping fails we must still have
     * stdin/stdout/stderr, otherwise every game printf is silently dropped
     * (that is exactly what happened when the mirror at 0x80000 was refused). */
    files_init();

    g_lowmem = map_low(DOS_LOWMEM_BASE, DOS_LOWMEM_SIZE, "low-memory window");
    if (!g_lowmem) {
        /* last resort: swap with the other end of the low window */
        uint32_t alt = (dos_lowmem_base == 0x00070000u) ? 0x000C0000u
                                                        : 0x00070000u;
        printf("dos: low-memory window unavailable at 0x%X, trying 0x%X\n",
               (unsigned)dos_lowmem_base, (unsigned)alt);
        g_lowmem = map_low(alt, DOS_LOWMEM_SIZE, "low-memory window (alt)");
        if (g_lowmem) {
            dos_lowmem_base = alt;
            g_lo_next = alt + DOS_LOWMEM_SIZE;
        }
    }
    if (!g_lowmem)
        return;
    memset(g_lowmem, 0, DOS_LOWMEM_SIZE);

    /* Commit the rest of the real-mode addressable RAM outside the VGA
     * window: 0x80000-0x9FFFF (head of the INT31 0100 pool) and
     * 0xC0000-0xFFFFF. On real hardware 0xC0000+ is ROM and stores there are
     * discarded, so the game's blitter may legally walk off the end of the
     * VGA window into it; Windows reports that as an access violation
     * unless the pages are mapped. Committing them here also makes any
     * `seg << 4` pointer land in writable memory.  The mirror may itself have
     * been moved into 0x80000-0x9FFFF (it has to sit on a 64 KiB boundary, see
     * dos_choose_lowmem), so only commit the part that is still free. */
    {
        uint32_t lo = 0x00080000u, hi = 0x000A0000u;
        uint32_t mend = dos_lowmem_base + DOS_LOWMEM_SIZE;
        if (dos_lowmem_base < hi && mend > lo)
            lo = mend;
        if (lo < hi && !map_low(lo, hi - lo, "real-mode pool"))
            ;                                   /* map_low already reported */
    }
    map_low(0x000C0000u, 0x00040000u, "ROM area 0xC0000-0xFFFFF");

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
}

/* The BIOS tick counter at 0x40:0x6C is the game's time source (it polls it
 * instead of hooking the timer interrupt). Keep it advancing at the classic
 * 18.2 Hz so animations and delays progress. */
static void bios_tick_thread(void *param)
{
    (void)param;
    for (;;) {
        plat_sleep_ms(55);
        if (g_lowmem) {
            uint32_t t = *(uint32_t *)(g_lowmem + 0x46C);
            *(uint32_t *)(g_lowmem + 0x46C) = t + 1;
        }
    }
}

static void bios_tick_start(void)
{
    plat_thread(bios_tick_thread, NULL);   /* detached */
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

static void log_call(const char *what, dos_ctx *c)
{
    g_verbose = g_verbose;
    printf("  %-28s eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X\n",
           what, (unsigned)c->Eax, (unsigned)c->Ebx, (unsigned)c->Ecx,
           (unsigned)c->Edx, (unsigned)c->Esi, (unsigned)c->Edi);
}

static void set_cf(dos_ctx *c, int on)
{
    if (on) c->EFlags |= 1u;
    else    c->EFlags &= ~1u;
}

/* Windows encodes the access kind in ExceptionInformation[0]: 0=read,
 * 1=write, 8=execute (instruction fetch). Linux derives the same three
 * values from the page-fault error code (dos_fault_posix.c). */
static const char *fault_kind(int kind)
{
    switch (kind) {
    case 0: return "read from";
    case 1: return "write to";
    case 8: return "instruction fetch at";
    default: return "access to";
    }
}

/* INT 21h - DOS services */
/* Small transfers are the interesting ones: printf goes to handle 1 and the
 * CRT reads DISK.NO 13 bytes at a time, while asset loads move kilobytes.
 * Logging only <=512 byte transfers (capped) keeps the log readable and
 * answers "did the read return data / did the message actually get written". */
static int g_rw_logged;

static void log_small_io(const char *rw, int hnd, unsigned want, unsigned n,
                         const void *buf)
{
    const uint8_t *p = (const uint8_t *)buf;
    char txt[41];
    unsigned i;

    if (g_rw_logged >= 40 || want > 512)
        return;
    g_rw_logged++;
    for (i = 0; i < 40; i++) {
        uint8_t ch = (i < n) ? p[i] : 0;
        if (i >= n)          txt[i] = 0;
        else if (ch >= 0x20 && ch < 0x7F) txt[i] = (char)ch;
        else                 txt[i] = '.';
    }
    printf("dos: %s h=%d want=%u n=%u  \"%s\"\n",
           rw, hnd, (unsigned)want, (unsigned)n, txt);
}

/* ------------------------------------------------------- guest pointers ---
 *
 * Guest code runs natively, so its pointers are ordinary pointers in this
 * process - but an argument handed to an unimplemented INT 21h service can be
 * anything, and dereferencing garbage kills the host instead of the game.
 * Every string we read out of a guest structure therefore goes through the
 * platform readability probe first (PROGRESS.md §8-45: VirtualQuery on
 * Windows, process_vm_readv(self) on Linux). */

static int guest_ptr_ok(const void *p, size_t len)
{
    if (!p || !len)
        return 0;
    return plat_readable(p, len);
}

static void guest_str(char *dst, size_t cap, const void *src)
{
    size_t i;

    if (!dst || !cap)
        return;
    dst[0] = 0;
    for (i = 0; i + 1 < cap; i++) {
        const char *q = (const char *)src + i;
        if (!guest_ptr_ok(q, 1) || !*q)
            break;
        dst[i] = *q;
        dst[i + 1] = 0;
    }
}

/* The DOS command tail handed to INT 21h AH=4B is *not* a C string: Watcom
 * builds it PSP-style, `[len][chars][0x0D]`, and the pointer in the exec
 * parameter block points at the length byte. Verified byte for byte against
 * FDPS's spawn of FD.EXE: `13 2E 5C 46 44 31 2E 56 69 64 ... 0D` =
 * len 19 + ".\FD1.Vid .\FD1.Aud" + CR. Reading it as a C string pulled in
 * 106 bytes of stack garbage and put them in the child's PSP. */
static void guest_cmdtail(char *dst, size_t cap, const void *src)
{
    const uint8_t *b = (const uint8_t *)src;
    size_t n, len;

    if (!dst || !cap)
        return;
    dst[0] = 0;
    if (!guest_ptr_ok(b, 1))
        return;
    len = b[0];
    if (len && len < cap && guest_ptr_ok(b + 1 + len, 1) && b[1 + len] == 0x0D) {
        memcpy(dst, b + 1, len);
        dst[len] = 0;
        return;
    }
    for (n = 0; n + 1 < cap && guest_ptr_ok(b + n, 1); n++) {
        if (b[n] == 0 || b[n] == 0x0D)
            break;
        dst[n] = (char)b[n];
        dst[n + 1] = 0;
    }
}

/* PSP:0x80 command tail (the game/CRT reads it to build argv). The mirror is
 * all zeroes otherwise, i.e. an empty command line - which is right for a
 * plain run and wrong for a child spawned by AH=4B (FD.EXE is handed two
 * arguments: video/audio config paths). */
void dos_set_cmdtail(const char *tail)
{
    size_t n = tail ? strlen(tail) : 0;

    if (!g_lowmem)
        return;
    if (!n)
        return;
    if (n > 126)
        n = 126;
    g_lowmem[0x80] = (uint8_t)n;
    memcpy(g_lowmem + 0x81, tail, n);
    g_lowmem[0x81 + n] = 0x0D;
    printf("dos: PSP:0x80 command tail (%u bytes) = '%s'\n",
           (unsigned)n, tail);
}

/* Called by the watchdog before plat_exit: a P_WAIT child is normally
 * reaped by AH=4B itself, but if the parent dies first the child would keep
 * running with nobody watching it. */
void dos_terminate_child(void)
{
    if (!plat_child_present())
        return;
    printf("dos: terminating child process before shutdown\n");
    plat_child_kill();
}

/* A block for the guest's INT 21h AH=48 / INT 31h AX=0501/0503 requests:
 * reserve anywhere + commit RW, like the old VirtualAlloc(NULL, ...,
 * MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE). The result becomes a 32-bit
 * linear address the guest dereferences directly, so a block above 4 GiB
 * (possible only in a 64-bit build - the game itself needs -m32) is
 * released and refused instead of being handed over truncated. */
static void *dos_alloc_block(uint32_t bytes)
{
    void *p = plat_reserve(0, bytes);

    if (!p)
        return NULL;
    if ((uintptr_t)p > 0xFFFFFFFFu) {
        printf("dos:   block at 0x%zX does not fit a 32-bit linear address "
               "(a 64-bit build cannot host the game)\n",
               (size_t)(uintptr_t)p);
        plat_release((uintptr_t)p, bytes);
        return NULL;
    }
    if (!plat_commit((uintptr_t)p, bytes, PLAT_PROT_RW)) {
        plat_release((uintptr_t)p, bytes);
        return NULL;
    }
    return p;
}

static void int21(dos_ctx *c)
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
            /* Deliberately still 0:0 even for INT 9. The game saves this as
             * "the old vector" and writes it back to uninstall (sub_56588),
             * and 0 then means "nobody's handler - stop delivering". */
            c->Ebx = 0; c->Eax = 0;
        } else if ((c->Eax & 0xFF) == 0x09) {
            /* AH=25 AL=09: the handler is DS:DX with a flat DS (base 0), so
             * the linear address is the *full* EDX - FDPS loads it with
             * `mov edx, offset sub_565A7`, i.e. 0x565A7, not a 16-bit DX. */
            uint32_t h = (uint32_t)c->Edx;
            if (h != g_guest_int9) {
                printf("dos: INT 9 vector := 0x%X%s\n", h,
                       h ? " (game ISR - keys will be delivered there)"
                         : " (restored to BIOS - keys go to the BDA buffer)");
            }
            g_guest_int9 = h;
        }
        set_cf(c, 0);
        break;

    case 0x3D: {                                /* open file */
        const char *name = (const char *)(uintptr_t)c->Edx;
        plat_file h;
        unsigned err;
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
        h = plat_file_open(name);
        err = (h == PLAT_FILE_INVALID) ? plat_error() : 0;
        printf("dos: open '%s' -> %p (%u)\n", name, (void *)(uintptr_t)h, err);
        if (h == PLAT_FILE_INVALID) { set_cf(c, 1); c->Eax = 2; break; }
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
        unsigned err = 0;
        plat_file h = plat_file_create(name);
        int hnd;
        if (h == PLAT_FILE_INVALID) {
            err = plat_error();
            printf("dos: create '%s' -> FAILED (%u)\n", name, err);
            set_cf(c, 1);
            c->Eax = (c->Eax & 0xFFFF0000u) | plat_error_to_dos(err);
            break;
        }
        hnd = file_alloc(h, name);
        printf("dos: create '%s' -> %p (dos handle %d)\n", name,
               (void *)(uintptr_t)h, hnd);
        if (hnd < 0) {
            plat_file_close(h);
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
        if (plat_file_delete(name) == 0) {
            printf("dos: delete '%s'\n", name);
            set_cf(c, 0);
        } else {
            unsigned err = plat_error();
            printf("dos: delete '%s' -> FAILED (%u)\n", name, err);
            set_cf(c, 1);
            c->Eax = (c->Eax & 0xFFFF0000u) | plat_error_to_dos(err);
        }
        break;
    }

    case 0x3E: {                                /* close file */
        int hnd = (int)(c->Ebx & 0xFFFF);
        if (hnd > 4 && hnd < DOS_MAX_FILES && g_files[hnd].used) {
            plat_file_close(g_files[hnd].h);
            g_files[hnd].used = 0;
        }
        set_cf(c, 0);
        break;
    }

    case 0x3F: {                                /* read */
        int hnd = (int)(c->Ebx & 0xFFFF);
        void *buf = (void *)(uintptr_t)c->Edx;
        unsigned want = c->Ecx, got = 0;
        if (hnd < DOS_MAX_FILES && g_files[hnd].used && !g_files[hnd].is_dev) {
            if (plat_file_read_at(g_files[hnd].h, g_files[hnd].pos,
                                  buf, want, &got) != 0) {
                set_cf(c, 1); c->Eax = 5; break;
            }
            g_files[hnd].pos += got;
            log_small_io("read ", hnd, want, got, buf);
            set_cf(c, 0); c->Eax = got;
        } else {
            set_cf(c, 0); c->Eax = 0;
        }
        break;
    }

    case 0x40: {                                /* write */
        int hnd = (int)(c->Ebx & 0xFFFF);
        void *buf = (void *)(uintptr_t)c->Edx;
        unsigned want = c->Ecx, wrote = 0;
        if (hnd < DOS_MAX_FILES && g_files[hnd].used) {
            if (want == 0) {
                /* DOS: a zero-length write truncates the file at the current
                 * file position. Watcom's sopen() implements O_TRUNC (the
                 * "wb" mode of fopen) with exactly this call, right after
                 * opening the file - i.e. it expects the whole file to be
                 * dropped. WriteFile(h, ..., 0, ...) and write(fd, ..., 0)
                 * are both no-ops, so the truncation has to be done
                 * explicitly (PITFALLS §8-31). Without it a save that
                 * shrinks leaves the tail of the old save behind. */
                if (!g_files[hnd].is_dev) {
                    if (plat_file_truncate(g_files[hnd].h,
                                           g_files[hnd].pos) != 0) {
                        unsigned err = plat_error();
                        printf("dos: truncate '%s' to %ld FAILED (%u)\n",
                               g_files[hnd].name, (long)g_files[hnd].pos, err);
                        set_cf(c, 1);
                        c->Eax = (c->Eax & 0xFFFF0000u) | plat_error_to_dos(err);
                        break;
                    }
                    printf("dos: truncate '%s' to %ld bytes\n",
                           g_files[hnd].name, (long)g_files[hnd].pos);
                }
                set_cf(c, 0); c->Eax = 0;
                break;
            }
            if (g_files[hnd].is_dev)
                plat_file_write_seq(g_files[hnd].h, buf, want, &wrote);
            else if (plat_file_write_at(g_files[hnd].h, g_files[hnd].pos,
                                        buf, want, &wrote) != 0)
                wrote = 0;
            if (want && !wrote)
                printf("dos: write h=%d -> 0 bytes (handle %p, err %lu)\n",
                       hnd, (void *)(uintptr_t)g_files[hnd].h,
                       (unsigned long)plat_error());
            g_files[hnd].pos += wrote;
            log_small_io("write", hnd, want, wrote, buf);
        }
        set_cf(c, 0); c->Eax = wrote;
        break;
    }

    case 0x42: {                                /* lseek */
        int hnd = (int)(c->Ebx & 0xFFFF);
        /* DOS: CX:DX = 32-bit offset, AL = origin. The previous code passed
         * CX as SetFilePointer's *high 32 bits*, so seeking to
         * CX:DX = 0x002A:1CF3 landed at (0x2A<<32)|0x1CF3 (~171 GB) - a
         * legal 64-bit position past EOF. No error was raised, but the next
         * read returned 0 bytes and the game decompressed stale garbage
         * (crash: RLE run walked off the VGA window at 0xC0005).
         *
         * The position now lives here (one arithmetic path on both
         * platforms), only SEEK_END's file size comes from the platform.
         * Offsets are added *signed*, exactly like the old (LONG) cast, so a
         * negative SEEK_CUR/END still works and a negative SEEK_SET still
         * fails with AX=6. */
        uint32_t pos = ((c->Ecx & 0xFFFFu) << 16) | (c->Edx & 0xFFFFu);
        unsigned meth = c->Eax & 0xFF;
        if (hnd < DOS_MAX_FILES && g_files[hnd].used && !g_files[hnd].is_dev) {
            int64_t base;
            int64_t np;
            uint32_t r;
            if (meth == 0) {
                base = 0;
            } else if (meth == 1) {
                base = g_files[hnd].pos;
            } else if (meth == 2) {
                uint64_t sz;
                if (plat_file_size(g_files[hnd].h, &sz) != 0) {
                    set_cf(c, 1); c->Eax = 6; break;
                }
                base = (int64_t)sz;
            } else {
                set_cf(c, 1); c->Eax = 6; break;   /* invalid origin */
            }
            np = base + (int32_t)pos;
            if (np < 0) {
                set_cf(c, 1); c->Eax = 6; break;   /* before BOF, like Win32 */
            }
            r = (uint32_t)(np & 0xFFFFFFFFu);
            g_files[hnd].pos = r;
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

    case 0x43: {                                /* get/set file attributes */
        /* FD2 never calls this; FDPS does right after startup and treats a
         * failure as "the data file is missing" -> exit(1) (PROGRESS.md §13.7). */
        const char *name = (const char *)(uintptr_t)c->Edx;
        uint8_t al = (uint8_t)(c->Eax & 0xFF);

        if (al == 0x00) {                       /* get attributes -> AL  */
            uint32_t flags = 0;
            uint32_t d;
            if (plat_file_attrs(name, &flags) != 0) {
                printf("dos: get attributes '%s' -> not found\n", name);
                set_cf(c, 1);
                c->Eax = (c->Eax & 0xFFFF0000u) | 2;
                break;
            }
            d = 0x20;                           /* default: archive      */
            if (flags & PLAT_FILE_RDONLY)  d |= 0x01;
            if (flags & PLAT_FILE_HIDDEN)   d |= 0x02;
            if (flags & PLAT_FILE_SYSTEM)   d |= 0x04;
            if (flags & PLAT_FILE_DIR)      d = (d & ~0x20u) | 0x10;
            printf("dos: get attributes '%s' -> 0x%X\n", name, (unsigned)d);
            c->Eax = (c->Eax & 0xFFFFFF00u) | d;
            set_cf(c, 0);
        } else if (al == 0x01) {                /* set attributes        */
            uint8_t cl = (uint8_t)(c->Ecx & 0xFF);
            uint32_t flags = 0;
            if (cl & 0x01) flags |= PLAT_FILE_RDONLY;
            if (cl & 0x02) flags |= PLAT_FILE_HIDDEN;
            if (cl & 0x04) flags |= PLAT_FILE_SYSTEM;
            if (plat_file_set_attrs(name, flags) != 0) {
                printf("dos: set attributes '%s' = 0x%X -> failed (%lu)\n",
                       name, cl, (unsigned long)plat_error());
                set_cf(c, 1);
                c->Eax = (c->Eax & 0xFFFF0000u) | 5;
                break;
            }
            printf("dos: set attributes '%s' = 0x%X\n", name, cl);
            c->Eax &= 0xFFFFFF00u;              /* AL = 0 on success     */
            set_cf(c, 0);
        } else {
            set_cf(c, 1);
            c->Eax = (c->Eax & 0xFFFF0000u) | 1; /* invalid function     */
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
        p = dos_alloc_block(bytes);
        if (p) {
            printf("dos: INT21 alloc %u paras -> linear 0x%p\n", paras, p);
            note_alloc((uint32_t)(uintptr_t)p, bytes, "INT21 alloc");
            c->Eax = (uint32_t)(uintptr_t)p;
            set_cf(c, 0);
        } else {
            printf("dos: INT21 alloc %u paras -> FAILED (%u)\n",
                   paras, plat_error());
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
        plat_time t;
        plat_local_time(&t);
        c->Eax = (c->Eax & 0xFFFFFF00u) | t.weekday;      /* AL = weekday */
        c->Ecx = (t.year << 8) | t.month;
        c->Edx = (t.day << 8) | 0;
        set_cf(c, 0);
        break;
    }
    case 0x2C: {                                /* get time */
        plat_time t;
        plat_local_time(&t);
        c->Ecx = (t.hour << 8) | t.minute;
        c->Edx = (t.second << 8) | (t.ms / 10);
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

    case 0x4B: {                               /* exec (load or execute) */
        /* FDPS's title flow spawns the real game (re/fdps_30CB0_spawn.c:
         * `spawnlp(0, "<dir>FD.EXE", ...)`, CRT ends up here via
         * `__dospawn`). There is no way to run a second LE image in this
         * address space (obj0 is already taken), so the child is *this host*
         * pointed at the new exe - which is also how a real DOS process tree
         * behaves: the parent stops, the child runs, the parent resumes.
         *
         * The parameter block written by __dospawn (re/fdps_dospawn.c) is
         * offset:selector pairs, flat selectors with base 0:
         *   +0 dword env, +4 word seg | +6 dword cmd tail, +10 word seg ...
         * so the tail's linear address is the dword at ES:BX+6. */
        uint8_t al = (uint8_t)(c->Eax & 0xFF);
        char path[PLAT_MAX_PATH] = "";
        char tail[128] = "";
        const uint8_t *blk;
        plat_exec_req req;
        int child_code = 0;

        guest_str(path, sizeof path, (const void *)(uintptr_t)c->Edx);
        blk = (const uint8_t *)(uintptr_t)c->Ebx;
        if (guest_ptr_ok(blk + 6, 4)) {
            uint32_t tp = *(const uint32_t *)(blk + 6);
            guest_cmdtail(tail, sizeof tail, (const void *)(uintptr_t)tp);
        }
        if (!path[0]) {
            printf("dos: UNHANDLED INT21 AH=4B exec (unreadable path at %X)\n",
                   (unsigned)c->Edx);
            set_cf(c, 1);
            c->Eax = (c->Eax & 0xFFFFFF00u) | 2;   /* file not found */
            break;
        }

        /* Everything OS from here on lives in the platform seam: resolving
         * our own image/cwd, the per-generation child log, CreateProcess vs
         * fork+execv, and the wait. */
        req.guest_path = path;
        req.cmdtail    = tail;
        req.wait       = (al == 0);              /* AL=00: P_WAIT       */
        req.exit_after = -1;
        if (req.wait) {                           /* bound the child too */
            int rem = host_exit_after_remaining();
            if (rem > 0)
                req.exit_after = rem;
        }

        printf("dos: INT 21h AH=4B exec al=%u '%s' tail='%s'\n",
               (unsigned)al, path, tail);

        if (plat_exec_child(&req, &child_code) != 0) {
            set_cf(c, 1);
            c->Eax = (c->Eax & 0xFFFFFF00u) | 1;
            break;
        }
        if (req.wait)
            g_child_exit = child_code;
        set_cf(c, 0);
        c->Eax = c->Eax & 0xFFFFFF00u;
        break;
    }

    case 0x4D:                                  /* get exit code of subprogram */
        /* AX = AH:AL, AH = return type (0 = normal), AL = exit code. */
        c->Eax = (c->Eax & 0xFFFF0000u) | (uint32_t)(g_child_exit & 0xFF);
        set_cf(c, 0);
        break;

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
static void int31(dos_ctx *c)
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
        uint64_t avail = 0, span = 0;
        plat_mem_status(&avail, &span);
        c->Ebx = (uint32_t)(avail / 0x10000);
        c->Edx = 0;
        c->Ecx = (uint32_t)(span / 0x10000);
        set_cf(c, 0);
        break;
    }
    case 0x0501: {                              /* allocate memory block */
        /* DPMI: size in BX:CX (bytes), result linear address in BX:CX. */
        uint32_t size = ((c->Ebx & 0xFFFFu) << 16) | (c->Ecx & 0xFFFFu);
        void *p;
        if (size == 0) size = 0x1000;
        p = dos_alloc_block(size);
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
        void *p = dos_alloc_block(size ? size : 0x1000);
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
static void int10(dos_ctx *c)
{
    uint8_t ah = (uint8_t)(c->Eax >> 8);
    g_calls[0x10]++;

    switch (ah) {
    case 0x00: {
        uint8_t mode = (uint8_t)(c->Eax & 0xFF);
        printf("dos: INT10 set video mode 0x%02X\n", mode);
        if (mode != 0x13)
            printf("dos: WARNING: the host presents a 320x200/8bpp frame "
                   "buffer (0xA0000) - mode 0x%02X is something else, display "
                   "and/or the VGA window may be wrong\n", mode);
        break;
    }
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
        plat_sleep_ms(2);
        waited += 2;
    }
}

static void int16(dos_ctx *c)
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
static void int33(dos_ctx *c)
{
    uint16_t ax = (uint16_t)c->Eax;
    g_calls[0x33]++;
    switch (ax) {
    case 0x0000: c->Eax = (c->Eax & 0xFFFF0000u) | 0xFFFF; c->Ebx = 0; break;
    case 0x0003: c->Ebx = 0; break;
    default: break;
    }
}

/* INT 2Fh - multiplex.
 *
 * The only caller in FDPS is the CD-ROM check: sub_3C3A6 does
 * `int386(0x2F, {AX=0x1500})` (MSCDEX installation check) and returns 0 when
 * BX comes back as 0 -> main prints "Fatal error: CDROM is not install!!!"
 * and exit(1). Everything after that (INT 31h AX=0100 DOS memory, AX=0300
 * simulate-real-mode-int) is already handled by the host, so reporting a
 * plausible MSCDEX 2.10 is enough to get past it. */
static void int2f(dos_ctx *c)
{
    uint16_t ax = (uint16_t)c->Eax;

    g_calls[0x2F]++;

    if (ax == 0x1500) {
        printf("dos: INT2F AX=1500 (MSCDEX install check) -> present, API 2.10\n");
        c->Eax = (c->Eax & 0xFFFF0000u) | 0x00FFu;   /* AL = FFh: installed */
        c->Ebx = (c->Ebx & 0xFFFF0000u) | 0x0210u;   /* BX = API version    */
        c->Ecx &= 0xFFFF0000u;                       /* CX = 0              */
        return;
    }
    c->Eax &= 0xFFFF0000u;                           /* nobody home */
}

/* --------------------------------------------------- privileged port I/O */

static int emulate_priv_instr(dos_ctx *c, const uint8_t *p)
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
        plat_sleep_ms(1);
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
            if (g_dac_logged < 6) {
                g_dac_logged++;
                printf("dos: DAC index := %u (op %02X)\n",
                       (unsigned)g_dac_index, p[0]);
            }
        } else if (g_dac_logged < 16) {
            g_dac_logged++;
            printf("dos: DAC index write with opcode %02X ignored (port 0x3C8)\n", p[0]);
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
                if (g_dac_logged < 12) {
                    g_dac_logged++;
                    printf("dos: DAC[%u] = %02X %02X %02X\n",
                           (unsigned)((g_dac_index + 255) & 0xFF),
                           dos_palette[((g_dac_index + 255) & 0xFF) * 3 + 0],
                           dos_palette[((g_dac_index + 255) & 0xFF) * 3 + 1],
                           dos_palette[((g_dac_index + 255) & 0xFF) * 3 + 2]);
                }
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
    case 0x03DA: {          /* input status register C/D */
        /* Bit 3 = vertical retrace, bit 0 = display disable: these are
         * *status* bits that change over time. Returning a constant value
         * deadlocks any code that waits for both edges - which is the standard
         * "wait for retrace" idiom:
         *     while ((inp(0x3DA) & 8) == 0) ;   // wait until retrace starts
         *     while ((inp(0x3DA) & 8) != 0) ;   // wait until it ends  <- hangs
         * That pair is in FDPS's title loop (sub_2A280) and made it spin
         * forever with a black screen (PROGRESS.md §13.7). Toggling on every
         * read lets both waits finish in <=2 reads on every game; the first
         * read still returns 0x09, matching the old behaviour. */
        static uint8_t vga_status = 0x00;
        vga_status ^= 0x09;
        c->Eax = (c->Eax & 0xFFFFFF00u) | vga_status;
        break;
    }
    case 0x0040: case 0x0043:   /* PIT */
        break;
    case 0x0020: case 0x0021:   /* PIC */
        break;
    case 0x0060: case 0x0061: case 0x0064:   /* keyboard controller */
        /* IN al,60h is how the game's INT 9 handler reads the scan code
         * (dos_deliver_key() latches it first); IN al,64h is the status
         * byte. OUT 60h/61h/64h is the acknowledge dance - a no-op here,
         * but it has to stay quiet: FDPS's ISR does it on every keystroke. */
        if (p[0] == 0xEC || p[0] == 0xED || p[0] == 0xE4 || p[0] == 0xE5) {
            if (port == 0x0060)
                c->Eax = (c->Eax & 0xFFFFFF00u) | g_kbd_last_sc;
            else if (port == 0x0064 && p[0] == 0xEC)
                c->Eax = (c->Eax & 0xFFFFFF00u) | 0x14;
        }
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

static uint32_t *ctx_reg(dos_ctx *c, int reg)
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
    return plat_readable(p, n);
}

/* String instructions (A4..AF: movs/stos/lods/cmps/scas) run a whole
 * iteration per address and touch memory through [E]SI/[E]DI, so they cannot
 * be stepped over byte by byte the way `mov cl,es:[edi-1]` can. First one
 * seen in practice: the Watcom CRT scanning the PSP command tail with
 * `rep(re/ne) scasb` starting at 0x81 - unreachable while the tail was empty
 * (FD2), hit as soon as a child host gets --cmdtail= (FD.EXE).
 * Execute the entire iteration here against the low-memory window. */
static int emulate_lowmem_string(dos_ctx *c, const uint8_t *start)
{
    const uint8_t *p = start;
    int rep = 0, addr32 = 1, op32 = 1, df, zf;
    int step, k, guard = 0, any = 0;
    uint8_t op, b;
    uint32_t count, si, di, delta, mask;
    static unsigned s_log;

    for (;;) {
        b = *p;
        if      (b == 0xF3) { rep = 1; p++; }
        else if (b == 0xF2) { rep = 2; p++; }
        else if (b == 0x66) { op32 = 0; p++; }
        else if (b == 0x67) { addr32 = 0; p++; }
        else if (b == 0x26 || b == 0x2E || b == 0x36 || b == 0x3E ||
                 b == 0x64 || b == 0x65) p++;      /* segment override: flat here */
        else break;
    }
    op = *p;
    if (op < 0xA4 || op > 0xAF)
        return 0;
    p++;

    step = (op & 1) ? (op32 ? 4 : 2) : 1;          /* odd opcodes are word/dword */
    mask = addr32 ? 0xFFFFFFFFu : 0xFFFFu;
    df   = (c->EFlags >> 10) & 1;
    zf   = (c->EFlags >> 6) & 1;
    delta = df ? (uint32_t)(-(int)step) : (uint32_t)step;
    count = rep ? (c->Ecx & mask) : 1;
    si    = c->Esi & mask;
    di    = c->Edi & mask;

    while (count && guard++ < 0x100000) {
        uint32_t memv = 0, regv = 0, rhs = 0;

        /* read the source byte(s) - DS:[SI] for movs/lods/cmps */
        if (op == 0xA4 || op == 0xA5 || op == 0xA6 || op == 0xA7) {
            for (k = 0; k < step; k++) {
                uint32_t ea = (si + (uint32_t)k) & mask;
                uint8_t v;
                if (ea < 0x10000) { if (!g_lowmem) return 0; v = g_lowmem[ea]; }
                else {
                    if (!guest_readable((const void *)(uintptr_t)ea, 1)) return 0;
                    v = *(const uint8_t *)(uintptr_t)ea;
                }
                memv |= (uint32_t)v << (8 * k);
            }
        }

        if (op == 0xA4 || op == 0xA5) {            /* movs: [DI] <- [SI] */
            for (k = 0; k < step; k++) {
                uint32_t ea = (di + (uint32_t)k) & mask;
                uint8_t v = (uint8_t)(memv >> (8 * k));
                if (ea < 0x10000) { if (!g_lowmem) return 0; g_lowmem[ea] = v; }
                else {
                    if (!guest_readable((const void *)(uintptr_t)ea, 1)) return 0;
                    *(uint8_t *)(uintptr_t)ea = v;
                }
            }
        } else if (op == 0xAA || op == 0xAB) {      /* stos: [DI] <- A */
            regv = op32 ? c->Eax : (c->Eax & 0xFFFF);
            for (k = 0; k < step; k++) {
                uint32_t ea = (di + (uint32_t)k) & mask;
                uint8_t v = (uint8_t)(regv >> (8 * k));
                if (ea < 0x10000) { if (!g_lowmem) return 0; g_lowmem[ea] = v; }
                else {
                    if (!guest_readable((const void *)(uintptr_t)ea, 1)) return 0;
                    *(uint8_t *)(uintptr_t)ea = v;
                }
            }
        } else if (op == 0xAC || op == 0xAD) {      /* lods: A <- [SI] */
            regv = memv;
            if (op32) c->Eax = (c->Eax & 0xFFFFFF00u) | regv;
            else      c->Eax = (c->Eax & 0xFFFF0000u) | (c->Eax & 0x0000FF00u)
                                 | (regv & 0xFF);
        } else {                                    /* cmps / scas */
            if (op == 0xA6 || op == 0xA7) {         /* cmps compares [SI] with [DI] */
                for (k = 0; k < step; k++) {
                    uint32_t ea = (di + (uint32_t)k) & mask;
                    uint8_t v;
                    if (ea < 0x10000) { if (!g_lowmem) return 0; v = g_lowmem[ea]; }
                    else {
                        if (!guest_readable((const void *)(uintptr_t)ea, 1)) return 0;
                        v = *(const uint8_t *)(uintptr_t)ea;
                    }
                    rhs |= (uint32_t)v << (8 * k);
                }
                memv = rhs;
            } else {
                memv = 0;                           /* scas reads [DI] */
                for (k = 0; k < step; k++) {
                    uint32_t ea = (di + (uint32_t)k) & mask;
                    uint8_t v;
                    if (ea < 0x10000) { if (!g_lowmem) return 0; v = g_lowmem[ea]; }
                    else {
                        if (!guest_readable((const void *)(uintptr_t)ea, 1)) return 0;
                        v = *(const uint8_t *)(uintptr_t)ea;
                    }
                    memv |= (uint32_t)v << (8 * k);
                }
            }
            regv = op32 ? c->Eax : (c->Eax & 0xFFFF);
            if (step == 1)      { memv &= 0xFF;   regv &= 0xFF; }
            else if (step == 2) { memv &= 0xFFFF; regv &= 0xFFFF; }
            zf = (memv == regv);
            c->EFlags = zf ? (c->EFlags | (1u << 6)) : (c->EFlags & ~(1u << 6));
        }
        any = 1;

        if (op == 0xA6 || op == 0xA7 || op == 0xAE || op == 0xAF)
            di = (di + delta) & mask;               /* cmps/scas step DI */
        else if (op == 0xA4 || op == 0xA5)
            { si = (si + delta) & mask; di = (di + delta) & mask; }
        else if (op == 0xAC || op == 0xAD)
            si = (si + delta) & mask;

        count--;
        if (!rep)
            break;
        if (op >= 0xA6) {                           /* F3 = repeat while ZF=1 */
            if (rep == 1 && !zf) break;
            if (rep == 2 &&  zf) break;
        }
    }
    if (!any)
        return 0;
    if (count && guard >= 0x100000)                /* runaway: refuse */
        return 0;

    c->Ecx = (c->Ecx & ~mask) | (count & mask);
    c->Esi = (c->Esi & ~mask) | (si & mask);
    c->Edi = (c->Edi & ~mask) | (di & mask);
    c->Eip = (uint32_t)(uintptr_t)p;
    if (s_log++ < 8)
        printf("dos: lowmem string %s%02X, %u left (si=%X di=%X) at 0x%X\n",
               rep == 1 ? "rep " : rep == 2 ? "repne " : "",
               (unsigned)op, (unsigned)count, (unsigned)si, (unsigned)di,
               (unsigned)(uintptr_t)start);
    return 1;
}

/* -------------------------------------------------- guest keyboard ISR ---
 *
 * A keystroke reaches a game one of two ways:
 *
 *   - the BIOS path (host.c writes the BDA ring at 0x41E, and INT 16h reads
 *     it) - FD2's intro and menus work like this;
 *   - the game's *own* INT 9 handler, which it installs with AH=25 AL=09.
 *     Then the BIOS path is dead in the original too (nobody chains to it),
 *     and only the handler's private queue is fed. FDPS's title menu reads
 *     exactly that queue, so without this it never sees a key.
 *
 * How it is delivered matters. Running the handler from a host thread with a
 * hand-made `call`/`iret` frame crashed reliably: the PRIV exceptions the
 * handler raises itself (sti, in/out) are dispatched differently on a thread
 * that never runs guest code, and 28 bytes of exception frame stayed on the
 * stack, so its `pop ds` popped garbage (PROGRESS.md §18). So the key is
 * queued here and *injected as a real interrupt* by the VEH on the thread
 * that is executing guest code - push EFLAGS/CS/EIP, point EIP at the
 * handler - which is exactly what the CPU would have done, and the handler's
 * own `iret` returns to the instruction that was about to run. */
static unsigned s_key_log;
static unsigned s_key_drop;
static int      s_dumped_handler;

#define KBD_RING 16
static volatile uint8_t g_kbd_ring[KBD_RING];
static volatile int32_t g_kbd_w, g_kbd_r;    /* monotonic counters */

/* Queue one keystroke for the game's own INT 9 handler (installed with
 * INT 21h AH=25h AL=09h). Returns 1 when the game owns the key queue, 0 when
 * nobody hooked INT 9 and the caller should use the BIOS buffer instead. */
int dos_deliver_key(uint8_t scan)
{
    int32_t w;

    if (!g_guest_int9)
        return 0;                               /* nobody hooked INT 9 */
    w = g_kbd_w;
    if (w - g_kbd_r >= KBD_RING) {
        if (s_key_drop++ < 8)
            printf("dos: key 0x%02X dropped (guest ISR queue full)\n",
                   (unsigned)scan);
        return 1;
    }
    g_kbd_ring[w & (KBD_RING - 1)] = scan;
    g_kbd_w = w + 1;
    if (s_key_log++ < 16)
        printf("dos: INT 9 queued scan=0x%02X -> handler 0x%X\n",
               (unsigned)scan, g_guest_int9);
    return 1;
}

/* Called from the fault handler just before it resumes guest code: turn a
 * queued scan code into an interrupt frame on the *guest's* stack. */
static void inject_int9(dos_ctx *c)
{
    int32_t r = g_kbd_r;
    uint32_t isr = g_guest_int9, sp;
    uint8_t scan;

    if (!isr || r == g_kbd_w)
        return;
    /* Only on the thread that actually executes the game: a host thread
     * (AIL timer callbacks, autokey) would get its own stack treatment from
     * the exception dispatcher and the handler's pops would misalign - that
     * is the crash this design replaced. Keys stay queued until the game
     * thread raises its next exception (it does that constantly: int 21h and
     * port reads are both exceptions here). */
    if (!g_guest_tid || plat_thread_id() != g_guest_tid)
        return;
    /* Never re-enter the handler itself: injecting while it is still running
     * restarts it from the top (the return address saved is inside the
     * handler) and the frames stack up - that is what crashed before. The
     * next key waits until the handler's `iret` has put EIP back in game
     * code, exactly like the PIC waiting for IF to be set again. */
    if (g_isr_lo && c->Eip >= g_isr_lo && c->Eip < g_isr_hi)
        return;
    if (!guest_readable((const void *)(uintptr_t)(c->Esp - 12), 12) ||
        !guest_readable((const void *)(uintptr_t)isr, 4)) {
        if (s_key_drop++ < 8)
            printf("dos: cannot inject INT 9 (stack 0x%X or handler 0x%X "
                   "unusable) - key dropped\n", (unsigned)c->Esp, isr);
        g_kbd_r = r + 1;
        return;
    }
    scan = g_kbd_ring[r & (KBD_RING - 1)];
    g_kbd_r = r + 1;
    g_kbd_last_sc = scan;                        /* `in al,60h` in the ISR */

    sp = c->Esp - 12;                            /* iret pops EIP, CS, EFLAGS */
    printf("dos: INT 9 injected scan=0x%02X (eip 0x%X -> 0x%X, esp 0x%X "
           "-> 0x%X)\n", (unsigned)scan, (unsigned)c->Eip, isr,
           (unsigned)c->Esp, (unsigned)sp);
    *(uint32_t *)(uintptr_t)(sp)     = c->Eip;
    *(uint32_t *)(uintptr_t)(sp + 4) = c->SegCs;
    *(uint32_t *)(uintptr_t)(sp + 8) = c->EFlags;
    c->Esp = sp;
    c->Eip = isr;
    g_isr_lo = isr;
    g_isr_hi = isr + 0x100;
    g_isr_log_left = 12;                        /* a few faults for the record */
    if (!s_dumped_handler) {                    /* one-shot: is the code intact? */
        int k;
        s_dumped_handler = 1;
        printf("isr: handler bytes: ");
        for (k = 0; k < 24; k++)
            printf("%02X ", ((const uint8_t *)(uintptr_t)isr)[k]);
        printf("\n");
    }
}

/* Minimal decoder for the forms the DOS/4GW startup code uses to reach PSP and
 * BIOS data through register-indirect addressing such as `mov cl,es:[edi-1]`.
 * Those cannot be fixed by rewriting an immediate, so the access is executed
 * against the low-memory window and the instruction is stepped over. */
static int emulate_lowmem_access(dos_ctx *c)
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
    c->Eip = (uint32_t)(uintptr_t)p;
    return 1;
}

/* Service a software interrupt found at EIP: `int NN` occupies `len` bytes
 * (prefixes + 2). Counted and traced exactly like the old trap sites. */
static void dispatch_swint(dos_ctx *c, uint8_t vec, uint32_t len)
{
    uint32_t addr = (uint32_t)(uintptr_t)c->Eip;

    if (!g_guest_tid)
        g_guest_tid = plat_thread_id();       /* the game thread */

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
    dos_service(vec, c);
}

/* The service switch itself (declared in dos.h): the fault handler reaches
 * it through dispatch_swint, doscheck drives INT 21h through it directly
 * without executing a real `int` instruction. The default branch logs
 * g_last_trap_eip, which dispatch_swint just set to the instruction's
 * address (stale only for a direct doscheck call, which uses it too). */
void dos_service(uint8_t vec, dos_ctx *c)
{
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
                   vec, (unsigned)g_last_trap_eip);
        g_unknown[vec]++;
        break;
    }
}

fd2_action dos_fault_core(dos_ctx *c, const fd2_fault *f, int *exit_code)
{
    /* Everything that happens for a while after an INT 9 injection: the
     * reported EIP is part of the evidence, so do not filter on it. */
    if (g_isr_log_left > 0) {
        const uint8_t *q = (const uint8_t *)(uintptr_t)c->Eip;
        g_isr_log_left--;
        printf("isr: evt code=%08lX eip=%08X esp=%08X eax=%08X ds=%04X "
               "bytes=%02X %02X\n",
               (unsigned long)f->code, (unsigned)c->Eip,
               (unsigned)c->Esp, (unsigned)c->Eax, (unsigned)c->SegDs,
               guest_readable(q, 2) ? q[0] : 0, guest_readable(q + 1, 1) ? q[1] : 0);
    }

    /* A software interrupt is a fault whose EIP points *at* the `CD` byte
     * (measured by probe4.c: EXCEPTION_ACCESS_VIOLATION for every vector,
     * EXCEPTION_BREAKPOINT for int 3 on Windows; faultprobe32.c: SIGSEGV /
     * si_code=SI_KERNEL, EIP at the instruction on Linux - dos_fault.h has
     * the full mapping, and the Linux int3 wrapper already moved EIP back).
     * Take it before the ring recording and before the access-violation
     * rules, otherwise the millions of int 21h the game makes would flood the
     * ring and be mistaken for real memory faults. */
    if (f->kind == FD2_FAULT_ACCESS || f->kind == FD2_FAULT_PRIV ||
        f->kind == FD2_FAULT_BREAK) {
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
                    *exit_code = 0;
                    return FD2_ACT_EXIT;
                }
                if (g_trace_mode && g_trace_left > 0)
                    c->EFlags |= 0x100u;      /* start single-stepping */
                inject_int9(c);                /* a key may have arrived */
                return FD2_ACT_CONTINUE;
            }
        }
    }

    /* Record everything that is not an interrupt service: privileged
     * instructions, access violations and stray breakpoints are what a crash
     * report needs in order to show how the flow went off the rails. */
    veh_record((uint32_t)c->Eip, f->code, f->info);

    /* instruction trace mode: single-step and log, used to follow the DOS/4GW
     * startup code where the disassembly assumptions break down */
    if (f->kind == FD2_FAULT_STEP) {
        if (g_trace_left > 0) {
            g_trace_left--;
            printf("t %08X esp=%08X eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X ebp=%08X\n",
                   (unsigned)c->Eip, (unsigned)c->Esp, (unsigned)c->Eax,
                   (unsigned)c->Ebx, (unsigned)c->Ecx, (unsigned)c->Edx,
                   (unsigned)c->Esi, (unsigned)c->Edi, (unsigned)c->Ebp);
            c->EFlags |= 0x100u;                /* keep single-stepping */
        } else {
            c->EFlags &= ~0x100u;               /* stop tracing */
        }
        return FD2_ACT_CONTINUE;
    }

    /* A breakpoint that is not an `int NN` is a stray 0xCC (execution landed
     * on data): it falls through to the crash report below. */

    /* Privileged instruction. Windows raises EXCEPTION_PRIV_INSTRUCTION for
     * these; Linux delivers the *same* SIGSEGV/SI_KERNEL as an `int`, a
     * segment load and a plain #GP - told apart here by has_addr: a page
     * fault carries si_addr, a privilege/segment fault does not
     * (PITFALLS §8-61). An addressless ACCESS that is not privileged is the
     * segment-load case and falls through to the rules below. */
    if (f->kind == FD2_FAULT_PRIV ||
        (f->kind == FD2_FAULT_ACCESS && !f->has_addr)) {
        const uint8_t *p = (const uint8_t *)(uintptr_t)c->Eip;
        int len = guest_readable(p, 8) ? emulate_priv_instr(c, p) : 0;
        if (len) {
            c->Eip += len;
            inject_int9(c);                    /* port ops are guest boundaries too */
            return FD2_ACT_CONTINUE;
        }
        if (f->kind == FD2_FAULT_PRIV)
            printf("cpu: unimplemented privileged instruction %02X at 0x%X\n",
                   p[0], (unsigned)c->Eip);
    }

    if (f->kind == FD2_FAULT_ACCESS) {
        const uint8_t *p = (const uint8_t *)(uintptr_t)c->Eip;
        uintptr_t fault = f->has_addr ? f->addr : 0;
        int has_fault = f->has_addr;

        /* If the game jumped somewhere unmapped we cannot inspect the
         * instruction at all - report honestly instead of faulting inside the
         * handler (that is what produced the bogus "EIP is in fd2_veh"). */
        if (!guest_readable(p, 16)) {
            if (has_fault)
                printf("cpu: fault at unreadable EIP=0x%X (%s address 0x%zX)\n",
                       (unsigned)c->Eip, fault_kind(f->access), (size_t)fault);
            else
                printf("cpu: fault at unreadable EIP=0x%X (%s, no address reported)\n",
                       (unsigned)c->Eip, fault_kind(f->access));
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
            *exit_code = 6;
            return FD2_ACT_EXIT;
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
                    if (g_isr_lo && c->Eip >= g_isr_lo && c->Eip < g_isr_hi)
                        printf("isr: segsub eip=%08X esp=%08X modrm=%02X\n",
                               (unsigned)c->Eip, (unsigned)c->Esp, modrm);
                    *r = (*r & 0xFFFF0000u) | flat;
                    return FD2_ACT_CONTINUE;
                }
            } else if ((modrm & 0xC7) == 0x05) {
                uint32_t disp = *(const uint32_t *)(p + 2);
                if (disp < 0x100000) {
                    if (g_trace_left > 0)
                        printf("dos: seg load [0x%X]=0x%04X -> flat 0x%04X at 0x%X\n",
                               disp, *(const uint16_t *)(uintptr_t)disp, flat,
                               (unsigned)c->Eip);
                    *(uint16_t *)(uintptr_t)disp = flat;
                    return FD2_ACT_CONTINUE;
                }
            }
        }

        /* (2) Absolute access into the first 64 KiB. Windows keeps that range
         * unmapped, so rewrite the operand to point at the low-memory window
         * and retry. The immediate is searchable around EIP because absolute
         * operands in this flat-model code are always 32-bit. Gated on a
         * *reported* address: Linux's SI_KERNEL faults carry none, and
         * treating 0 as the fault address would match and corrupt an
         * unrelated immediate (PITFALLS §8-61). */
        if (has_fault && fault < 0x10000) {
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
                    return FD2_ACT_CONTINUE;
                }
            }
            /* (3) register-indirect access to low memory: emulate the load */
            if (emulate_lowmem_access(c)) {
                inject_int9(c);
                return FD2_ACT_CONTINUE;
            }
            /* (4) string instructions over low memory (PSP tail scan, ...) */
            if (emulate_lowmem_string(c, p)) {
                inject_int9(c);
                return FD2_ACT_CONTINUE;
            }
            printf("cpu: unmatched low-memory access: fault=0x%zX eip=0x%X "
                   "bytes=%02X %02X %02X %02X %02X %02X %02X %02X "
                   "esi=%08X edi=%08X ecx=%08X\n",
                   (size_t)fault, (unsigned)c->Eip,
                   p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
                   (unsigned)c->Esi, (unsigned)c->Edi, (unsigned)c->Ecx);
        }

        if (has_fault)
            printf("cpu: ACCESS VIOLATION at 0x%X (Eip=0x%X) %s address 0x%zX\n",
                   (unsigned)c->Eip, (unsigned)c->Eip,
                   fault_kind(f->access), (size_t)fault);
        else
            printf("cpu: ACCESS VIOLATION at 0x%X (Eip=0x%X) %s, no address "
                   "reported (segment/privilege fault)\n",
                   (unsigned)c->Eip, (unsigned)c->Eip, fault_kind(f->access));
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
        *exit_code = 3;
        return FD2_ACT_EXIT;
    }

    {
        const uint8_t *p = (const uint8_t *)(uintptr_t)c->Eip;

        /* Not one of the exceptions we own. If the faulting instruction is
         * the guest's, report it as a crash. Otherwise it belongs to the host
         * runtime and must reach the next handler: MSVC's 0x406D1388 "name
         * this thread" exception (raised by D3D11 worker threads) and friends
         * are raised on purpose and caught by their own SEH frame - killing
         * the process here made merely starting a sokol/D3D11 backend look
         * like a game crash (PROGRESS.md §13.6 step 2, §8-34).
         * Guest code lives below 1 MiB or in a block we handed out. */
        if (c->Eip >= 0x00100000u && !find_alloc((uint32_t)c->Eip)) {
            if (g_unknown[0xFE] < 8) {
                printf("host: exception %08lX at 0x%X - not ours, passed to the next handler\n",
                       (unsigned long)f->code, (unsigned)c->Eip);
            }
            g_unknown[0xFE]++;
            return FD2_ACT_SEARCH;
        }

        printf("cpu: unhandled exception %08lX at 0x%X\n",
               (unsigned long)f->code, (unsigned)c->Eip);
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
        *exit_code = 4;
        return FD2_ACT_EXIT;
    }
}

void dos_install_traps(void)
{
    dos_fault_install();               /* platform-specific entry */
    bios_tick_start();
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
