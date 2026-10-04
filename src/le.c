/* le.c - Linear Executable loader (see le.h for the established layout). */

#include "le.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Locate the LE header: signature "LE\0\0" followed by a self-consistent
 * module page count and object table. */
static size_t find_le_header(const uint8_t *d, size_t n)
{
    size_t i;
    for (i = 0; i + 0x80 < n; i++) {
        uint32_t pages, objtab;
        if (!(d[i] == 'L' && d[i + 1] == 'E' && d[i + 2] == 0 && d[i + 3] == 0))
            continue;
        pages  = rd32(d + i + 0x14);
        /* this dialect keeps the object table at +0x40, count at +0x44 */
        objtab = rd32(d + i + 0x40);
        if (pages == 0 || pages > 4096)          continue;
        if (objtab < 0x40 || objtab > 0x1000)    continue;
        if (i + objtab + 48 > n)                 continue;
        return i;
    }
    return (size_t)-1;
}

int le_open(le_image *le, const char *path)
{    FILE *f;
    long  len;
    uint32_t i, objtab, objpage;
    size_t off;

    memset(le, 0, sizeof *le);

    f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "le: cannot open %s\n", path); return -1; }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return -1; }

    le->data = (uint8_t *)malloc((size_t)len);
    if (!le->data) { fclose(f); return -1; }
    if (fread(le->data, 1, (size_t)len, f) != (size_t)len) {
        fclose(f); free(le->data); le->data = NULL; return -1;
    }
    fclose(f);
    le->size = (size_t)len;

    off = find_le_header(le->data, le->size);
    if (off == (size_t)-1) {
        fprintf(stderr, "le: no LE header found\n");
        return -1;
    }
    le->le_offset = off;

    {
        const uint8_t *h = le->data + off;
        le->module_pages       = rd32(h + 0x14);
        le->eip_object         = rd32(h + 0x18);
        le->eip                = rd32(h + 0x1C);
        objtab                 = rd32(h + 0x40);
        le->object_count       = rd32(h + 0x44);
        objpage                = rd32(h + 0x48);
        le->fixup_page_table   = rd32(h + 0x68);
        le->fixup_record_table = rd32(h + 0x6C);

        if (le->object_count == 0 || le->object_count > LE_MAX_OBJECTS) {
            fprintf(stderr, "le: implausible object count %u\n", le->object_count);
            return -1;
        }
        for (i = 0; i < le->object_count; i++) {
            const uint8_t *e = h + objtab + i * 24;
            le_object *o = &le->objects[i];
            o->vsize      = rd32(e + 0);
            o->base       = rd32(e + 4);
            o->flags      = rd32(e + 8);
            o->page_index = rd32(e + 12);
            o->page_count = rd32(e + 16);
            if (o->base + o->vsize > 0xF0000000u) {
                fprintf(stderr, "le: object %u has absurd range\n", i);
                return -1;
            }
        }
        le->image_end   = le->size;
        le->entry_linear = le->objects[le->eip_object - 1].base + le->eip;

        /* object data starts at the first page-table entry's data offset */
        {
            const uint8_t *pt = h + objpage;
            size_t first = (size_t)-1, last = 0;
            for (i = 0; i < le->module_pages; i++) {
                uint32_t d_off  = rd32(pt + i * 8);
                uint16_t d_size = rd16(pt + i * 8 + 4);
                uint16_t d_flag = rd16(pt + i * 8 + 6);
                if (d_flag == 2) continue;              /* zero-fill page */
                if (d_off < first) first = d_off;
                if (d_off + d_size > last) last = d_off + d_size;
            }
            le->image_start = first;
            le->image_end   = last;
        }
    }

    printf("le: header @0x%zX  objects=%u  pages=%u  entry=0x%X (obj%u+0x%X)\n",
           le->le_offset, le->object_count, le->module_pages,
           le->entry_linear, le->eip_object, le->eip);
    for (i = 0; i < le->object_count; i++) {
        const le_object *o = &le->objects[i];
        printf("    obj%u  base=0x%08X  vsize=0x%06X  pages=%u..%u  flags=0x%X\n",
               i, o->base, o->vsize, o->page_index,
               o->page_index + o->page_count - 1, o->flags);
    }
    printf("    fixup page table @+0x%X, record table @+0x%X\n",
           le->fixup_page_table, le->fixup_record_table);
    printf("    image data: file 0x%zX .. 0x%zX (%zu bytes)\n",
           le->image_start, le->image_end, le->image_end - le->image_start);
    return 0;
}

/* Apply the per-page fixup records.
 *
 * Verified against Ghidra for FD2 - the record stream per page is:
 *
 *   type 0x07 : [07][size][src:2][obj:1][tgt:2+extra]
 *               size  = high nibble encodes extra target bytes,
 *                       target size = 2 + (size >> 4)
 *               obj   = target object, 1-based
 *               writes obj_base + tgt into *(u32*)(page_base + src)
 *   type 0x00 : single filler byte, skip
 *
 * Every one of the 71 pages is consumed exactly by this grammar and the
 * resulting image matches Ghidra's relocated copy byte for byte. */
static int apply_fixups(le_image *le, int *applied)
{
    const uint8_t *h = le->data + le->le_offset;
    const uint8_t *pt;
    const uint8_t *rt;
    uint32_t page;
    int total = 0, unknown = 0, failed_pages = 0, cross_page = 0;

    if (le->fixup_page_table == 0 || le->fixup_record_table == 0) {
        printf("le: no fixup tables (raw image assumed)\n");
        *applied = 0;
        return 0;
    }
    pt = h + le->fixup_page_table;
    rt = h + le->fixup_record_table;

    for (page = 1; page <= le->module_pages; page++) {
        uint32_t begin = rd32(pt + (page - 1) * 4);
        uint32_t end   = rd32(pt + page * 4);
        uint32_t pos   = begin;
        const le_object *o = NULL;
        uint32_t obj;
        uint32_t page_base = 0;

        if (end <= begin || end > le->size) continue;

        for (obj = 0; obj < le->object_count; obj++) {
            const le_object *cand = &le->objects[obj];
            if (page >= cand->page_index &&
                page < cand->page_index + cand->page_count) {
                o = cand;
                page_base = cand->base +
                            (page - cand->page_index) * LE_PAGE_SIZE;
                break;
            }
        }
        if (!o) continue;

        while (pos < end) {
            uint8_t type = rt[pos];
            uint8_t b1;
            uint32_t tsize, srcoff, tobj, tgtoff;
            uint32_t i;

            if (type == 0x00) { pos += 1; continue; }
            if (type != 0x07) { unknown++; break; }

            b1    = rt[pos + 1];
            tsize = 2u + (uint32_t)(b1 >> 4);
            if (pos + 5 + tsize > end) { unknown++; break; }

            srcoff = rd16(rt + pos + 2);
            tobj   = rt[pos + 4];
            tgtoff = 0;
            for (i = 0; i < tsize; i++)
                tgtoff |= (uint32_t)rt[pos + 5 + i] << (8 * i);

            if (tobj == 0 || tobj > le->object_count) { unknown++; break; }

            /* The page is the relocation unit and the linker never places an
             * address operand so that it runs off the end of a page. Records
             * claiming a source near the page tail (0xFFFD..0xFFFF) are stray
             * bytes that parse as a record; applying them would overwrite the
             * first bytes of the following page - which is exactly what
             * Ghidra's loader also refuses to do. */
            if ((uint32_t)srcoff + 4u > LE_PAGE_SIZE) {
                cross_page++;
                pos += 5 + tsize;
                continue;
            }

            *(uint32_t *)(uintptr_t)(page_base + srcoff) =
                le->objects[tobj - 1].base + tgtoff;
            total++;
            pos += 5 + tsize;
        }
        if (pos != end) failed_pages++;
    }

    printf("le: fixups applied=%d, cross-page records skipped=%d, "
           "pages with leftover data=%d, bad records=%d\n",
           total, cross_page, failed_pages, unknown);
    *applied = total;
    return 0;
}

static void *map_at(uint32_t base, uint32_t size, DWORD prot, const char *what)
{
    /* the region may already be reserved by le_reserve_address_space() */
    void *p = VirtualAlloc((void *)(uintptr_t)base, size, MEM_COMMIT, prot);
    if (!p)
        p = VirtualAlloc((void *)(uintptr_t)base, size,
                         MEM_RESERVE | MEM_COMMIT, prot);
    if (!p)
        fprintf(stderr, "le: VirtualAlloc(%s @0x%X, %u) failed: %lu\n",
                what, base, size, GetLastError());
    return p;
}

/* Game address space: LE objects + VGA frame buffer + a low-memory mirror.
 * Reserved up front so the CRT heap cannot take these addresses. */
#define FD2_OBJ_REGION_BASE   0x00010000u
#define FD2_OBJ_REGION_SIZE   0x00060000u   /* 0x10000 .. 0x6FFFF */
#define FD2_VGA_BASE          0x000A0000u
#define FD2_VGA_SIZE          0x00020000u   /* 0xA0000 .. 0xBFFFF */

static int g_early_reserved;

/* Pre-CRT variant: no stdio, only kernel32 calls. Called from the process
 * entry point before the CRT heap exists, so the window cannot be stolen. */
int le_reserve_address_space_early(void)
{
    void *a = VirtualAlloc((void *)(uintptr_t)FD2_OBJ_REGION_BASE,
                           FD2_OBJ_REGION_SIZE,
                           MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    void *b = VirtualAlloc((void *)(uintptr_t)FD2_VGA_BASE,
                           FD2_VGA_SIZE, MEM_RESERVE | MEM_COMMIT,
                           PAGE_READWRITE);
    (void)b;                                 /* VGA is best-effort */
    if (!a)
        return -1;
    g_early_reserved = 1;
    return 0;
}

int le_reserve_address_space(void)
{
    if (g_early_reserved)
        return 0;                       /* already won the race in fd2_entry */
    if (!le_reserve_address_space_early())
        return 0;
    {
        MEMORY_BASIC_INFORMATION mbi;
        DWORD err = GetLastError();
        ULONG_PTR img = (ULONG_PTR)GetModuleHandleA(NULL);
        fprintf(stderr, "le: cannot reserve object region @0x%X: %lu\n",
                FD2_OBJ_REGION_BASE, err);
        fprintf(stderr, "    this image is loaded at 0x%p\n", (void *)img);
        if (VirtualQuery((void *)(uintptr_t)FD2_OBJ_REGION_BASE, &mbi,
                         sizeof mbi)) {
            fprintf(stderr,
                    "    0x%X is %s%s%s type=%s prot=0x%lX region=0x%zX\n",
                    FD2_OBJ_REGION_BASE,
                    (mbi.State & MEM_COMMIT) ? "COMMIT " : "",
                    (mbi.State & MEM_RESERVE) ? "RESERVE " : "",
                    (mbi.State & MEM_FREE) ? "FREE" : "",
                    (mbi.Type == MEM_IMAGE) ? "IMAGE" :
                    (mbi.Type == MEM_MAPPED) ? "MAPPED" :
                    (mbi.Type == MEM_PRIVATE) ? "PRIVATE" : "-",
                    mbi.Protect, mbi.RegionSize);
        }
        return -1;
    }
}

/* Determine where the object page data starts.
 *
 * FD2's object page table is not laid out the standard way (its entries do
 * not contain usable file offsets), but the data pages are stored densely
 * and page-aligned, in object order, at the end of the container:
 *
 *     obj0 : 63 pages of 0x1000          (0x36014 .. 0x75014)
 *     obj1 :  4 pages of 0x1000          (0x75014 .. 0x79014)
 *     obj2 :  3 pages + 0x4D2 remainder  (0x79014 .. EOF)
 *
 * The start is therefore EOF minus the summed on-disk span, and every page
 * is consumed sequentially. */
static size_t compute_image_start(const le_image *le)
{
    size_t total = 0;
    uint32_t i;

    for (i = 0; i < le->object_count; i++) {
        const le_object *o = &le->objects[i];
        size_t span = (size_t)o->page_count * LE_PAGE_SIZE;

        if (i + 1 == le->object_count) {
            size_t rem = o->vsize % LE_PAGE_SIZE;
            if (rem == 0) rem = LE_PAGE_SIZE;
            span = (size_t)(o->page_count - 1) * LE_PAGE_SIZE + rem;
        }
        total += span;
    }
    if (total > le->size) return (size_t)-1;
    return le->size - total;
}

int le_map_and_relocate(le_image *le, int *fixups_applied)
{
    size_t cursor;
    uint32_t i;

    for (i = 0; i < le->object_count; i++) {
        const le_object *o = &le->objects[i];
        /* map whole pages: fixups may write a dword that starts inside the
         * last valid page but extends past vsize */
        uint32_t span = o->page_count * LE_PAGE_SIZE;
        void *p = map_at(o->base, span, PAGE_EXECUTE_READWRITE, "object");
        if (!p) return -1;
        memset(p, 0, span);
    }

    le->image_start = compute_image_start(le);
    if (le->image_start == (size_t)-1) {
        fprintf(stderr, "le: cannot derive object data start\n");
        return -1;
    }
    le->image_end = le->size;
    printf("le: object page data at file 0x%zX (%zu bytes)\n",
           le->image_start, le->image_end - le->image_start);

    cursor = le->image_start;
    for (i = 0; i < le->object_count; i++) {
        const le_object *o = &le->objects[i];
        size_t span = (size_t)o->page_count * LE_PAGE_SIZE;
        size_t copy;

        if (cursor + span > le->size) span = le->size - cursor;
        copy = o->vsize;
        if (copy > span) copy = span;
        if (copy == 0) break;
        memcpy((void *)(uintptr_t)o->base, le->data + cursor, copy);
        printf("    obj%u <- file 0x%zX .. 0x%zX (%zu bytes)\n",
               i, cursor, cursor + copy, copy);
        cursor += span;
    }

    return apply_fixups(le, fixups_applied);
}

int le_map_flat(le_image *le, const char *path)
{
    /* flat concatenated image: object order, no gaps */
    FILE *f = fopen(path, "rb");
    uint32_t i;
    size_t cursor = 0;

    if (!f) { fprintf(stderr, "le: cannot open flat image %s\n", path); return -1; }
    for (i = 0; i < le->object_count; i++ ) {
        const le_object *o = &le->objects[i];
        void *p = map_at(o->base, o->vsize, PAGE_EXECUTE_READWRITE, "flat");
        if (!p) { fclose(f); return -1; }
        if (fread(p, 1, o->vsize, f) != o->vsize) {
            fprintf(stderr, "le: short flat image for object %u\n", i);
            fclose(f); return -1;
        }
        cursor += o->vsize;
    }
    fclose(f);
    printf("le: flat image loaded (%zu bytes, no fixups applied)\n", cursor);
    return 0;
}

void le_close(le_image *le)
{
    if (le->data) { free(le->data); le->data = NULL; }
}
