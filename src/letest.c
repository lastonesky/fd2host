/* letest.c - validate the LE loader against an independently relocated image
 * (build/object1.bin .. object3.bin - gitignored, see AGENTS.md).
 *
 * Two extra judges, because a reference file may not be around:
 *   - fnv1a of each mapped object: the *same loader* must produce the same
 *     hash on every OS, which is how the portability claim is proven
 *     (docs/rounds/13-portability.md);
 *   - a non-zero exit code when a byte differs from the reference outside the
 *     two explained classes below, so scripts can assert on it.
 *
 * argv[1] = FD2.EXE, argv[2] = directory holding object1.bin..object3.bin. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"

/* FNV-1a 64 over a mapped object: the same loader must produce the same
 * hash on every OS - that is the judge when the reference images are not
 * around (they are gitignored, see AGENTS.md). */
static unsigned long long fnv1a(const unsigned char *p, unsigned long n)
{
    unsigned long long h = 1469598103934665603ULL;
    unsigned long i;

    for (i = 0; i < n; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

/* Two classes of difference against the reference are *explained*, not loader
 * bugs (docs/rounds/13-portability.md §43):
 *
 *   tail - bytes past page_count * 0x1000: the BSS tail of an object whose
 *          vsize runs beyond its pages. We map the pages; the rest of the
 *          window is the zero-filled reservation. IDA maps the whole vsize
 *          and fills that gap with 0xFF instead.
 *   edge - bytes at a page boundary (offset 0, 1 or 0xFFF): those are the
 *          cross-page fixup records we skip on purpose - writing them would
 *          touch the next page's first bytes (docs/PITFALLS.md §8-11).
 *          IDA's loader resolves them instead.
 *
 * Anything else is a real mismatch. */
static int diff_explained(unsigned long k, unsigned long page_span)
{
    unsigned long off = k % LE_PAGE_SIZE;

    if (k >= page_span)
        return 1;
    return off <= 1 || off == LE_PAGE_SIZE - 1;
}

int main(int argc, char **argv)
{
    le_image le;
    int applied = 0;
    const char *exe = (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE";
    const char *defref[3] = {
        "E:\\FD2\\port\\build\\object1.bin",
        "E:\\FD2\\port\\build\\object2.bin",
        "E:\\FD2\\port\\build\\object3.bin"
    };
    const char *ref[3];
    char       refpath[3][640];
    unsigned   i;
    int        unexpected = 0;
    int        diff_seen = 0;

    /* argv[2] = the directory holding object1.bin..object3.bin; a missing or
     * empty argument keeps the Windows defaults (the POSIX build passes
     * /mnt/e/FD2/port/build). A doubled separator is harmless on both OSes. */
    for (i = 0; i < 3; i++) {
        if (argc > 2 && argv[2][0]) {
            snprintf(refpath[i], sizeof refpath[i], "%s/object%u.bin",
                     argv[2], i + 1);
            ref[i] = refpath[i];
        } else {
            ref[i] = defref[i];
        }
    }

    if (le_reserve_address_space() != 0) return 2;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    if (le_open(&le, exe) != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;

    for (i = 0; i < le.object_count && i < 3; i++) {
        FILE *f = fopen(ref[i], "rb");
        unsigned char *refbuf;
        const unsigned char *live;
        unsigned long n = le.objects[i].vsize, k, diff = 0;
        unsigned long firstdiff = 0;

        printf("  obj%u: base=0x%X vsize=0x%lX fnv1a=0x%016llX\n",
               i, le.objects[i].base, n,
               fnv1a((const unsigned char *)(uintptr_t)le.objects[i].base, n));
        if (!f) { printf("  obj%u: no reference image (%s)\n", i, ref[i]); continue; }
        refbuf = malloc(n);
        if (fread(refbuf, 1, n, f) != n) { fclose(f); free(refbuf); continue; }
        fclose(f);

        live = (const unsigned char *)(uintptr_t)le.objects[i].base;
        for (k = 0; k < n; k++) {
            if (live[k] != refbuf[k]) {
                if (!diff) firstdiff = k;
                if (!diff_explained(k, (unsigned long)le.objects[i].page_count *
                                       LE_PAGE_SIZE))
                    unexpected++;
                if (diff < 25)
                    printf("        diff @ +0x%05lX (mem 0x%X): ours=%02X ghidra=%02X\n",
                           k, (unsigned)(le.objects[i].base + k),
                           live[k], refbuf[k]);
                diff++;
            }
        }
        printf("  obj%u: %lu/%lu bytes differ from the reference image%s\n",
               i, diff, n, diff ? "" : "  <-- exact match");
        if (diff) {
            diff_seen = 1;
            printf("        first diff @ +0x%lX: ours=%02X ghidra=%02X\n",
                   firstdiff, live[firstdiff], refbuf[firstdiff]);
            printf("        %s\n", (unexpected == 0)
                   ? "all in the explained classes (page-edge fixups / BSS tail)"
                   : "UNEXPECTED - see the class list above");
        }
        free(refbuf);
    }

    printf("entry would be 0x%X\n", le.entry_linear);
    le_close(&le);
    if (unexpected) {
        printf("FAIL: %d bytes differ from the reference outside the two "
               "explained classes\n", unexpected);
        return 1;
    }
    printf("reference check OK%s\n",
           diff_seen ? " (explained differences only)" : ", exact match");
    return 0;
}
