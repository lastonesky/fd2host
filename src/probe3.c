/* probe3.c - where does the loader put this image, and is the fixed game
 * window still available? Decides between "self-restart until ASLR is kind"
 * and "pin the image base". */

#include <windows.h>
#include <stdio.h>

int main(void)
{
    MEMORY_BASIC_INFORMATION mbi;
    void *a;
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("image=%p  ", (void *)GetModuleHandleA(NULL));
    a = VirtualAlloc((void *)0x10000, 0x60000, MEM_RESERVE | MEM_COMMIT,
                     PAGE_EXECUTE_READWRITE);
    printf("reserve=%p err=%lu  ", a, a ? 0UL : GetLastError());
    if (VirtualQuery((void *)0x10000, &mbi, sizeof mbi)) {
        printf("0x10000: state=0x%lX type=0x%lX prot=0x%lX size=0x%zX",
               mbi.State, mbi.Type, mbi.Protect, mbi.RegionSize);
    }
    printf("\n");
    return 0;
}
