/* probe.c - 32-bit host feasibility probe for the FD2 native port.
 *
 * Verifies the core assumptions of the "binary host" route:
 *   1) Can a 32-bit Win32 process reserve/commit memory where the DOS/4GW
 *      objects were linked to live (0x10000 .. 0x6FFFF)?
 *   2) Can it place the VGA frame buffer at 0xA0000 (128 KB)?
 *   3) Can it allocate the BIOS data area at 0x400 (expected: no) so the
 *      loader knows it must relocate those absolute accesses?
 *   4) Can code placed in those regions actually execute (no DEP)?
 */

#include <windows.h>
#include <stdio.h>

static void probe(const char *what, void *want, SIZE_T size, DWORD prot)
{
    MEMORY_BASIC_INFORMATION mbi;
    void *p = VirtualAlloc(want, size, MEM_RESERVE | MEM_COMMIT, prot);
    if (!p) {
        printf("  %-28s want=%08p size=%06X -> FAILED (err %lu)\n",
               what, want, (unsigned)size, GetLastError());
        return;
    }
    VirtualQuery(p, &mbi, sizeof mbi);
    printf("  %-28s want=%08p size=%06X -> got %08p state=%08lX prot=%08lX\n",
           what, want, (unsigned)size, p, mbi.State, mbi.Protect);
}

int main(void)
{
    void *p;
    volatile unsigned char *mem;
    unsigned i;

    printf("FD2 native host feasibility probe (32-bit)\n");
    printf("sizeof(void*)=%u\n\n", (unsigned)sizeof(void *));

    probe("LE object area 0x10000", (void *)0x00010000, 0x60000, PAGE_EXECUTE_READWRITE);
    probe("VGA framebuffer 0xA0000", (void *)0x000A0000, 0x20000, PAGE_READWRITE);
    probe("BIOS data area 0x400", (void *)0x00000400, 0x100, PAGE_READWRITE);
    probe("PSP-ish low 0x1000", (void *)0x00001000, 0x1000, PAGE_READWRITE);

    /* can we actually touch the framebuffer region? */
    p = VirtualAlloc((void *)0x000A0000, 0x10000, MEM_RESERVE | MEM_COMMIT,
                     PAGE_READWRITE);
    if (p == (void *)0x000A0000) {
        mem = (volatile unsigned char *)0x000A0000;
        for (i = 0; i < 0x10000; i++)
            mem[i] = (unsigned char)i;
        printf("\n  0xA0000 writable: [0]=%02X [0xFFFF]=%02X (expect 00 / FF)\n",
               mem[0], mem[0xFFFF]);
    }

    /* can we execute code from a fixed low address? */
    p = VirtualAlloc((void *)0x00010000, 0x1000, MEM_RESERVE | MEM_COMMIT,
                     PAGE_EXECUTE_READWRITE);
    if (p == (void *)0x00010000) {
        unsigned char *code = (unsigned char *)0x00010000;
        int (*fn)(void);
        /* mov eax, 0x1234; ret */
        code[0] = 0xB8; code[1] = 0x34; code[2] = 0x12;
        code[3] = 0x00; code[4] = 0x00; code[5] = 0xC3;
        FlushInstructionCache(GetCurrentProcess(), code, 6);
        fn = (int (*)(void))code;
        printf("  execute code @0x10000 -> returned 0x%X (expect 0x1234)\n", fn());
    }

    printf("\nprobe done\n");
    return 0;
}
