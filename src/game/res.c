/* res.c - FD2 LMI container resource loader (source translation of 0x111BA).
 *
 * See game/res.h for the contract. The original is 235 bytes that only
 * sequence the Watcom CRT (fopen/fseek/fread/malloc/free/fclose) plus the
 * `sub_3702F` stack probe; this is the straight C equivalent.
 */
#include "res.h"
#include <stdio.h>
#include <stdlib.h>

uint32_t res_size;   /* original dword_53BFF @ 0x53BFF */

void *res_load(const char *filename, void *old_buffer, int index)
{
    FILE     *f;
    uint32_t *entry;
    uint32_t  start;
    void     *buf;

    if (old_buffer)
        free(old_buffer);

    f = fopen(filename, "rb");
    if (!f) {
        printf("\n\n File not found %s!!! \n\n", filename);
        exit(1);
    }

    entry = (uint32_t *)malloc(8);
    fseek(f, 4L * index + 6, SEEK_SET);
    fread(entry, 1, 8, f);

    start    = entry[0];
    res_size = entry[1] - entry[0];
    free(entry);

    buf = malloc(res_size);
    if (!buf) {
        printf("Out of Memory at Load %s Number:%d!!\n", filename, index);
        exit(1);
    }

    fseek(f, (long)start, SEEK_SET);
    fread(buf, 1, res_size, f);
    fclose(f);

    return buf;
}
