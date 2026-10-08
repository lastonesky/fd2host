/* faultprobe64.c - measure what Linux does with the traps the guest raises.
 *
 * The Win32 side measured its model in probe4.c (int NN / int3 / privileged
 * instructions all arrive at the VEH as exceptions with EIP at the
 * instruction). This is the Linux analogue - run it for each case in a forked
 * child, print signal / si_code / si_addr / RIP from the sigaction handler.
 *
 * NOTE: 64-bit run. The real target is -m32 (compat mode); results that may
 * differ are re-measured by the freestanding 32-bit probe.
 *
 *   cc -O0 -g -o build/faultprobe64 build/faultprobe64.c && build/faultprobe64
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/mman.h>

static const char *g_name = "?";

/* Read memory without faulting: process_vm_readv returns EFAULT instead of
 * raising a signal, so it is safe even when RIP points into unmapped space. */
static int safe_read(unsigned long addr, unsigned char *out, size_t n)
{
    struct iovec lv = { out, n }, rv = { (void *)addr, n };
    ssize_t r = syscall(SYS_process_vm_readv, getpid(), &lv, 1, &rv, 1, 0);
    return r == (ssize_t)n;
}

static void handler(int sig, siginfo_t *si, void *ucv)
{
    ucontext_t *uc = ucv;
    unsigned long ip = (unsigned long)uc->uc_mcontext.gregs[REG_RIP];
    unsigned long sp = (unsigned long)uc->uc_mcontext.gregs[REG_RSP];
    unsigned char b[16];
    char hex[48] = "";
    int i, ok = safe_read(ip, b, sizeof b);

    if (ok)
        for (i = 0; i < 16; i++) sprintf(hex + i * 3, "%02X ", b[i]);
    printf("%-18s SIG=%-2d si_code=%-4d si_addr=%-14p rip=%016lx sp=%016lx bytes=%s\n",
           g_name, sig, si->si_code, si->si_addr, ip, sp, ok ? hex : "(unreadable)");
    fflush(stdout);
    _exit(0);
}

#define RUN(name, ...) do {                     \
        fflush(stdout);                         \
        pid_t p = fork();                       \
        if (p == 0) {                           \
            g_name = name;                      \
            __VA_ARGS__;                        \
            _exit(99);                          \
        }                                       \
        int st = 0; waitpid(p, &st, 0);         \
        if (WIFEXITED(st) && WEXITSTATUS(st) == 99)                     \
            printf("%-18s no fault (exit 99)\n", name);                 \
        else if (WIFSIGNALED(st))                                       \
            printf("%-18s DIED sig=%d (no handler ran)\n", name, WTERMSIG(st)); \
    } while (0)

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGTRAP, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);

    printf("== Linux x86-64 fault model (faultprobe64) ==\n");

    RUN("int $0x21",  asm volatile("int $0x21"));
    RUN("int $0x2F",  asm volatile("int $0x2f"));
    RUN("int $0x10",  asm volatile("int $0x10"));
    RUN("int $0x31",  asm volatile("int $0x31"));
    RUN("int $0x16",  asm volatile("int $0x16"));
    RUN("int $0x03",  asm volatile("int $0x03"));
    RUN("int3 (0xCC)", asm volatile(".byte 0xcc"));
    RUN("int $0x01",  asm volatile("int $0x01"));
    RUN("int $0x08",  asm volatile("int $0x08"));   /* timer vector, kernel only? */
    RUN("int $0x80",  asm volatile("int $0x80" :: "a"(20)));  /* getpid = sanity */
    RUN("in al,dx",   asm volatile("inb %%dx, %%al" :: "d"(0x3DA) : "memory"));
    RUN("out dx,al",  asm volatile("outb %%al, %%dx" :: "a"(0), "d"(0x3C8) : "memory"));
    RUN("cli",        asm volatile("cli"));
    RUN("sti",        asm volatile("sti"));
    RUN("hlt",        asm volatile("hlt"));
    RUN("smsw ax",    asm volatile(".byte 0x0f,0x01,0xe0"));
    RUN("lgdt [x]",   asm volatile(".byte 0x0f,0x01,0x15,0,0,0,0"));
    RUN("ud2",        asm volatile("ud2"));
    RUN("mov ds,ax",  asm volatile("movw %w0, %%ds" :: "r"((unsigned short)0x24)));
    RUN("deref NULL", asm volatile("movl $1, %0" :: "m"(*(volatile int *)0)));
    {
        void *p = mmap((void *)0x10000000, 0x1000, PROT_NONE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        (void)p;
        RUN("write PROT_NONE", asm volatile("movl $1, %0" :: "m"(*(volatile int *)0x10000000)));
    }
    RUN("fetch 0x1234000", asm volatile("jmp *%0" :: "r"(0x1234000UL)));
    {
        /* TF single-step: one instruction with the trap flag set */
        RUN("TF single-step", ({
            unsigned long f;
            __asm__ volatile("pushfq; popq %0; orq $0x100, %0; pushq %0; popfq"
                             : "=r"(f) :: "memory");
            __asm__ volatile("nop");
        }));
    }
    RUN("div by zero", asm volatile("movl $0, %%ecx; movl $1, %%eax; xorl %%edx, %%edx; divl %%ecx"
                                    :: "c"((unsigned)0) : "eax","edx"));
    printf("== done ==\n");
    return 0;
}
