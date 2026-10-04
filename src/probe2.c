/* probe2.c - diagnose why the game address space cannot be reserved. */

#include <windows.h>
#include <stdio.h>

static void q(const char *tag, void *addr)
{
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T n = VirtualQuery(addr, &mbi, sizeof mbi);
    if (!n) {
        printf("  %-22s %p : VirtualQuery failed (%lu)\n", tag, addr, GetLastError());
        return;
    }
    printf("  %-22s %p : base=%p size=0x%zX state=%s%s%s type=%s prot=0x%lX\n",
           tag, addr, mbi.BaseAddress, mbi.RegionSize,
           (mbi.State & MEM_COMMIT) ? "CMT" : "",
           (mbi.State & MEM_RESERVE) ? "RSV" : "",
           (mbi.State & MEM_FREE) ? "FREE" : "",
           (mbi.Type == MEM_IMAGE) ? "IMAGE" :
           (mbi.Type == MEM_MAPPED) ? "MAPPED" :
           (mbi.Type == MEM_PRIVATE) ? "PRIVATE" : "-",
           mbi.Protect);
}

int main(void)
{
    void *p;
    int i;
    static const unsigned cand[] = {
        0x10000, 0x20000, 0x30000, 0x40000, 0x50000, 0x60000, 0x70000,
        0x80000, 0x90000, 0xA0000, 0xC0000, 0x100000
    };

    printf("image base = %p  (ASLR must not land here)\n",
           (void *)GetModuleHandleA(NULL));

    /* the real target, before anything else grabs the space */
    p = VirtualAlloc((void *)0x10000, 0x60000, MEM_RESERVE | MEM_COMMIT,
                     PAGE_EXECUTE_READWRITE);
    printf("FIRST alloc 0x10000+0x60000 RWX -> %p  err=%lu\n", p,
           p ? 0UL : GetLastError());
    if (p) VirtualFree(p, 0, MEM_RELEASE);

    q("query 0x10000", (void *)0x10000);
    q("query 0xA0000", (void *)0xA0000);
    q("query 0x100000", (void *)0x100000);

    for (i = 0; i < (int)(sizeof cand / sizeof cand[0]); i++) {
        void *a = (void *)(uintptr_t)cand[i];
        p = VirtualAlloc(a, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        printf("  alloc 0x%06X+0x10000 -> %p  err=%lu\n", cand[i], p,
               p ? 0UL : GetLastError());
    }

    p = VirtualAlloc((void *)0xA0000, 0x20000, MEM_RESERVE | MEM_COMMIT,
                     PAGE_READWRITE);
    printf("  alloc 0xA0000+0x20000 RW  -> %p  err=%lu\n", p,
           p ? 0UL : GetLastError());

    return 0;
}
