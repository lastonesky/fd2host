/* platprobe.c - platform self-test: reserve the guest window, map and touch
 * every object range, report what the OS says. The portable successor of the
 * old Windows-only probe.c - run it on a new OS before trusting le.c there.
 *
 *   Windows: platprobe.exe          Linux: make -f Makefile.linux platprobe */
#include <stdio.h>
#include <string.h>
#include "platform.h"
#include "le.h"

#define NL "\n"

static void report(const char *what, uintptr_t addr, size_t len)
{
    plat_region q;
    char desc[512];
    int r = plat_query(addr, &q);
    printf("%s: query ret=%d free=%d base=0x%zX size=0x%zX prot=0x%X" NL,
           what, r, q.is_free, (size_t)q.base, q.size, q.prot);
    plat_describe(addr, desc, sizeof desc);
    printf("   describe: %s" NL, desc);
    fflush(stdout);
}

int main(void)
{
    plat_region q;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("image base = %p" NL, plat_image_base());
    printf("data selector = 0x%X" NL, plat_data_selector());

    report("before reservation", 0x10000, 0x10000);

    printf("le_reserve_address_space() -> %d" NL, le_reserve_address_space());
    report("after  reservation @0x10000", 0x10000, 0x10000);
    report("after  reservation @0x50000", 0x50000, 0x10000);
    report("after  reservation @0x60000", 0x60000, 0x10000);
    report("after  reservation @0xA0000", 0xA0000, 0x10000);

    /* The object ranges through le_commit_range - the loader's own path. It
     * commits region by region, because one request spanning several of the
     * 64 KiB blocks fails on Windows (487, see le.h). */
    if (le_commit_range(0x10000, 0x3F000, PLAT_PROT_RWX, "obj0") ||
        le_commit_range(0x50000, 0x6000,  PLAT_PROT_RWX, "obj1") ||
        le_commit_range(0x60000, 0x3000,  PLAT_PROT_RWX, "obj2")) {
        printf("le_commit_range failed" NL);
        return 1;
    }
    memset((void *)(uintptr_t)0x10000, 0, 0x3F000);
    memset((void *)(uintptr_t)0x50000, 0, 0x6000);
    memset((void *)(uintptr_t)0x60000, 0, 0x3000);
    printf("object windows committed through le_commit_range and touched" NL);

    /* the whole window, the way dos.c maps the mirror and the VGA
     * (le_commit_range returns 0 on success) */
    if (le_commit_range(0x10000, 0xF0000, PLAT_PROT_RWX, "window") != 0) {
        printf("whole window 0x10000+0xF0000 via le_commit_range FAILED" NL);
        return 1;
    }
    printf("whole window 0x10000+0xF0000 via le_commit_range -> ok" NL);
    plat_query(0x10000, &q);
    printf("final: free=%d base=0x%zX size=0x%zX prot=0x%X" NL,
           q.is_free, (size_t)q.base, q.size, q.prot);
    return 0;
}
