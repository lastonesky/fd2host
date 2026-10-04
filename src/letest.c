/* letest.c - validate the LE loader against the relocated images exported
 * from Ghidra (build/object1.bin .. object3.bin). */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "le.h"

int main(int argc, char **argv)
{
    le_image le;
    int applied = 0;
    const char *exe = (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE";
    const char *ref[3] = {
        "E:\\FD2\\port\\build\\object1.bin",
        "E:\\FD2\\port\\build\\object2.bin",
        "E:\\FD2\\port\\build\\object3.bin"
    };
    unsigned i;

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

        if (!f) { printf("  obj%u: no reference image (%s)\n", i, ref[i]); continue; }
        refbuf = malloc(n);
        if (fread(refbuf, 1, n, f) != n) { fclose(f); free(refbuf); continue; }
        fclose(f);

        live = (const unsigned char *)(uintptr_t)le.objects[i].base;
        for (k = 0; k < n; k++) {
            if (live[k] != refbuf[k]) {
                if (!diff) firstdiff = k;
                if (diff < 25)
                    printf("        diff @ +0x%05lX (mem 0x%X): ours=%02X ghidra=%02X\n",
                           k, (unsigned)(le.objects[i].base + k),
                           live[k], refbuf[k]);
                diff++;
            }
        }
        printf("  obj%u: %lu/%lu bytes differ from Ghidra image%s\n",
               i, diff, n, diff ? "" : "  <-- exact match");
        if (diff) {
            printf("        first diff @ +0x%lX: ours=%02X ghidra=%02X\n",
                   firstdiff, live[firstdiff], refbuf[firstdiff]);
        }
        free(refbuf);
    }

    printf("entry would be 0x%X\n", le.entry_linear);
    le_close(&le);
    return 0;
}
