/* faultprobe32.c - freestanding 32-bit Linux fault probe.
 *
 * The port needs the *compat-mode* fault model (the guest is 32-bit x86), and
 * multilib is not installed - so this probe runs -m32 without libc: raw
 * int 0x80 syscalls, hand-rolled sigaction, no stdio. Measured per case in a
 * forked child; the child's SA_SIGINFO handler prints signal / si_code /
 * raw siginfo words / raw ucontext words (so the layout itself is evidence,
 * not an assumption) and exits.
 *
 * Build (Makefile.linux target `faultprobe32`):
 *   gcc -m32 -ffreestanding -nostdlib -static -fno-pie -o build/faultprobe32 \
 *       src/faultprobe32.c
 */
typedef unsigned int   u32;
typedef unsigned short u16;
typedef int            i32;

#define NULL ((void *)0)

/* ---- freestanding helpers ------------------------------------------------ */
unsigned strlen_(const char *s) { unsigned n = 0; while (s[n]) n++; return n; }

void *memset(void *d, int c, unsigned n)
{
    unsigned char *p = d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}

static int sys_write(int fd, const void *b, unsigned n)
{
    int r;
    __asm__ volatile ("int $0x80" : "=a"(r) : "a"(4), "b"(fd), "c"(b), "d"(n));
    return r;
}
static int sys_fork(void)
{
    int r;
    __asm__ volatile ("int $0x80" : "=a"(r) : "a"(2));
    return r;
}
static int sys_waitpid(int pid, int *st, int opt)
{
    int r;
    __asm__ volatile ("int $0x80" : "=a"(r) : "a"(7), "b"(pid), "c"(st), "d"(opt));
    return r;
}
static void sys_exit(int c)
{
    for (;;) __asm__ volatile ("int $0x80" :: "a"(1), "b"(c));
}
static int sys_rt_sigaction(int sig, void *act, void *old, int sz)
{
    int r;
    __asm__ volatile ("int $0x80"
                      : "=a"(r)
                      : "a"(174), "b"(sig), "c"(act), "d"(old), "S"(sz));
    return r;
}

static void wr(const char *s) { sys_write(1, s, strlen_(s)); }

static void hex8(u32 v)
{
    char b[11];
    int i;
    b[0] = '0'; b[1] = 'x';
    for (i = 0; i < 8; i++)
        b[2 + i] = "0123456789ABCDEF"[(v >> ((7 - i) * 4)) & 0xF];
    b[10] = 0;
    wr(b);
}
static void dec(int v)
{
    char b[16];
    int n = 0, neg = 0, i;
    if (v < 0) { neg = 1; v = -v; }
    if (!v) b[n++] = '0';
    while (v) { b[n++] = (char)('0' + v % 10); v /= 10; }
    if (neg) b[n++] = '-';
    for (i = 0; i < n / 2; i++) { char t = b[i]; b[i] = b[n - 1 - i]; b[n - 1 - i] = t; }
    b[n] = 0;
    wr(b);
}

/* ---- handler ------------------------------------------------------------- */

static const char *g_name = "?";

/* kernel sigaction block (i386), packed by hand */
struct ksa {
    void *sa_handler;      /* +0  */
    unsigned long sa_flags;/* +4  */
    void *sa_restorer;     /* +8  */
    unsigned long sa_mask; /* +12 (8 bytes passed to the syscall) */
};

#define SA_SIGINFO  0x00000004
#define SA_RESTORER 0x04000000

/* restorer: never used (the handler exits), but some kernels insist */
static void restorer(void)
{
    __asm__ volatile ("movl $173, %%eax; int $0x80" ::: "eax");
}

static void handler(int sig, void *si, void *uc)
{
    int *iw = (int *)si;
    u32 *uw = (u32 *)uc;
    int i;

    wr(g_name);
    wr(" sig=");
    dec(sig);
    wr(" si_code=");
    dec(iw[2]);
    wr("  siginfo[0..7]=");
    for (i = 0; i < 8; i++) { hex8((u32)iw[i]); wr(" "); }
    wr("\n         ucontext=");
    for (i = 4; i <= 26; i++) {           /* byte offsets 16..104 */
        wr("["); dec(i * 4); wr("]=");
        hex8(uw[i]); wr(" ");
    }
    wr("\n");
    sys_exit(0);
}

/* ---- cases --------------------------------------------------------------- */

#define RUN(name, ...) do {                                \
        int st = 0, pid;                                   \
        g_name = name;                                     \
        pid = sys_fork();                                  \
        if (pid == 0) {                                    \
            __VA_ARGS__;                                   \
            sys_exit(99);                                  \
        }                                                  \
        sys_waitpid(pid, &st, 0);                          \
        if ((st & 0x7F) == 0) {                            \
            if (((st >> 8) & 0xFF) == 99) {                \
                wr(name); wr("  no fault (exit 99)\n");    \
            } else {                                       \
                wr(name); wr("  handler exit=");           \
                dec((st >> 8) & 0xFF); wr("\n");           \
            }                                              \
        } else {                                           \
            wr(name); wr("  DIED signal=");                \
            dec(st & 0x7F); wr("\n");                      \
        }                                                  \
    } while (0)

void _start(void)
{
    struct ksa sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = (void *)handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTORER;
    sa.sa_restorer = (void *)restorer;

    wr("== Linux i386 (compat, freestanding) fault model ==\n");
    sys_rt_sigaction(11, &sa, NULL, 8);   /* SIGSEGV */
    sys_rt_sigaction(4,  &sa, NULL, 8);   /* SIGILL  */
    sys_rt_sigaction(5,  &sa, NULL, 8);   /* SIGTRAP */
    sys_rt_sigaction(7,  &sa, NULL, 8);   /* SIGBUS  */
    sys_rt_sigaction(8,  &sa, NULL, 8);   /* SIGFPE  */

    RUN("int $0x21",      __asm__ volatile ("int $0x21"));
    RUN("int $0x2F",      __asm__ volatile ("int $0x2f"));
    RUN("int $0x10",      __asm__ volatile ("int $0x10"));
    RUN("int $0x31",      __asm__ volatile ("int $0x31"));
    RUN("int $0x16",      __asm__ volatile ("int $0x16"));
    RUN("int $0x08",      __asm__ volatile ("int $0x08"));
    RUN("int $0x03",      __asm__ volatile ("int $0x03"));
    RUN("int3 0xCC",      __asm__ volatile (".byte 0xcc"));
    RUN("int $0x80 pid",  __asm__ volatile ("int $0x80" :: "a"(20)));
    RUN("in al,dx",       __asm__ volatile ("inb %%dx, %%al" :: "d"(0x3DA) : "memory"));
    RUN("out dx,al",      __asm__ volatile ("outb %%al, %%dx" :: "a"(0), "d"(0x3C8) : "memory"));
    RUN("in eax,0x60",    __asm__ volatile (".byte 0xE4, 0x60"));
    RUN("cli",            __asm__ volatile ("cli"));
    RUN("sti",            __asm__ volatile ("sti"));
    RUN("hlt",            __asm__ volatile ("hlt"));
    RUN("lgdt [0]",       __asm__ volatile (".byte 0x0f,0x01,0x15,0,0,0,0"));
    RUN("mov ds,ax",      __asm__ volatile ("movw %w0, %%ds" :: "r"((unsigned short)0x24)));
    RUN("mov ds,es",      __asm__ volatile (".byte 0x8e,0xc0"));
    RUN("smsw ax",        __asm__ volatile (".byte 0x0f,0x01,0xe0"));
    RUN("deref 0",        __asm__ volatile ("movl $1, (%0)" :: "r"(0) : "memory"));
    RUN("deref 0x400",    __asm__ volatile ("movl $1, (%0)" :: "r"(0x400) : "memory"));
    RUN("fetch 0x1234000", __asm__ volatile ("jmp *%0" :: "r"(0x1234000)));
    RUN("TF after popf",
        __asm__ volatile ("pushf; orl $0x100, (%esp); popf");
        __asm__ volatile ("nop"));
    RUN("ud2",            __asm__ volatile ("ud2"));
    RUN("div 0",          __asm__ volatile ("movl $0, %ecx; movl $1, %eax; xorl %edx, %edx; divl %ecx"));

    wr("== done ==\n");
    sys_exit(0);
}
