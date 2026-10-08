/* le.c - Linear Executable loader (see le.h for the established layout). */

#include "le.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Guest window: everything below 1 MiB - the LE objects (including foreign
 * ones such as FDPS's obj2 at 0x70000), the low-memory mirror, the real-mode
 * pool, the VGA window and the "ROM" area. It is reserved as ONE block before
 * the CRT exists: reserving only 0x10000..0x70000 left 0x80000 and 0xA0000
 * free and the CRT heap took them (mirror refused -> files_init() skipped ->
 * every game printf dropped; VGA unmapped -> host_frame() faulted at 0xA1000). */
#define FD2_OBJ_REGION_BASE   0x00010000u
#define FD2_LOW_LIMIT         0x00100000u   /* everything below 1 MiB */
#define FD2_OBJ_REGION_SIZE   (FD2_LOW_LIMIT - FD2_OBJ_REGION_BASE)
#define FD2_VGA_BASE          0x000A0000u
#define FD2_VGA_SIZE          0x00020000u   /* 0xA0000 .. 0xBFFFF */

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

    /* Objects outside the pre-CRT reservation (0x10000..0x6FFFF) have to be
     * reserved *now*, while the CRT heap is still tiny: le_map_and_relocate()
     * runs a few allocations later, and by then the heap may have grown over
     * a foreign image's object (FDPS parks obj2 at 0x70000). Reserving only -
     * map_at() commits later. */
    {
        uint32_t k;
        for (k = 0; k < le->object_count; k++) {
            uint32_t b = le->objects[k].base;
            uint32_t span = le->objects[k].page_count * LE_PAGE_SIZE;
            if (span < le->objects[k].vsize)
                span = le->objects[k].vsize;
            if (span == 0)
                continue;
            if (b >= FD2_OBJ_REGION_BASE && b + span <= FD2_LOW_LIMIT)
                continue;                 /* covered by the early reservation */
            if (!plat_reserve((uintptr_t)b, span)) {
                /* not fatal: map_at() retries and reports if it really fails */
                fprintf(stderr, "le: cannot reserve object %u @0x%X+%X (%u)\n",
                        k, b, span, plat_error());
            } else {
                printf("le: reserved foreign object %u @0x%X+%X\n", k, b, span);
            }
        }
    }

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
        le->last_page_bytes    = rd32(h + 0x2C);   /* tail size, see le.h */

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
    int bad_source = 0;                     /* src offset outside its page */

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

            /* Type 0x02: [02][flags][src:2][obj:1] - the source operand is
             * **16 bits** (LE fixup type 2 = selector/segment), the target is
             * the object itself with no offset field. FD2 never uses it,
             * FDPS does exactly once: at 0x565AF, the imm16 of
             * `mov ax, seg dseg03` / `mov ds, eax` inside its INT 9 handler.
             * Two earlier mistakes are worth remembering:
             *   - skipping the record left a pointer at 0 and the game died
             *     dereferencing it (`mov es,[ebx]`, EBX=0);
             *   - writing the object base as a full 32-bit word (what this
             *     used to do) overwrote the two bytes behind the operand and
             *     turned `8E D8` (mov ds,eax) into `07 00` (pop es; add ah,al)
             *     - the handler then decoded `pusha`, ate 32 bytes it never
             *     pushed and crashed in `pop ds` (PROGRESS.md §18).
             * The value is a *selector*, and in a flat process the only
             * sensible one is our own flat data selector - which is also what
             * a real DOS/4GW loader would have handed out for that object. */
            if (type == 0x02) {
                uint16_t sel;
                if (pos + 5 > end) { unknown++; break; }
                srcoff = rd16(rt + pos + 2);
                tobj   = rt[pos + 4];
                if (tobj == 0 || tobj > le->object_count) { unknown++; break; }
                if ((uint32_t)srcoff + 2u > LE_PAGE_SIZE) {
                    cross_page++;
                    pos += 5;
                    continue;
                }
                sel = (uint16_t)plat_data_selector();
                *(uint16_t *)(uintptr_t)(page_base + srcoff) = sel;
                total++;
                pos += 5;
                continue;
            }

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

            /* Source offsets come in two flavours (the record grammar is
             * fixed, so both look the same on paper):
             *
             *   src <= 0xFFFC the operand sits fully inside this page - always
             *                fine (the write happens below).
             *   0xFFD..0xFFF the operand *starts* here and runs into the next
             *                page. That is legal: the object's pages are
             *                contiguous in our mapping, so writing at
             *                page_base+srcoff lands exactly where Ghidra and
             *                IDA put it. FD2 has 15 of these and 11 of them
             *                change bytes - the raw file holds a placeholder
             *                where the target's high byte goes, so skipping
             *                them left 11 *unrelocated* pointers in the image
             *                (why letest disagreed with both tools).
             *   src > 0xFFF the source is not in this page at all. Ghidra and
             *                IDA leave those alone too - no difference shows
             *                up at the address such a write would hit - and
             *                honouring one scribbles over unrelated code.
             *                That is the half of "fixup 跨页记录必须跳过"
             *                (docs/PITFALLS.md §8-11) that was crashing: the
             *                legal straddles were being skipped with them.
             *
             * A straddling write may only cross into the next page when that
             * page belongs to the same object - across an object boundary the
             * neighbour has a different base and the operand is its business. */
            if ((uint32_t)srcoff >= LE_PAGE_SIZE) {
                bad_source++;
                pos += 5 + tsize;
                continue;
            }
            if ((uint32_t)srcoff + 4u > LE_PAGE_SIZE) {
                uint32_t next_page = page + 1;
                if (!o || next_page < o->page_index ||
                    next_page >= o->page_index + o->page_count) {
                    cross_page++;
                    pos += 5 + tsize;
                    continue;
                }
            }

            *(uint32_t *)(uintptr_t)(page_base + srcoff) =
                le->objects[tobj - 1].base + tgtoff;
            total++;
            pos += 5 + tsize;
        }
        if (pos != end) failed_pages++;
    }

    printf("le: fixups applied=%d, out-of-page sources=%d, boundary writes "
           "refused=%d, pages with leftover data=%d, bad records=%d\n",
           total, bad_source, cross_page, failed_pages, unknown);
    *applied = total;
    return 0;
}

static void *map_at(uint32_t base, uint32_t size, unsigned prot,
                    const char *what)
{
    /* Commit region by region: a single request spanning several of the
     * early reservation's blocks fails (ERROR_INVALID_ADDRESS on Windows,
     * see le_commit_range). */
    if (le_commit_range(base, size, (int)prot, what) != 0)
        return NULL;
    return (void *)(uintptr_t)base;
}

int le_commit_range(uint32_t base, uint32_t size, int prot, const char *what)
{
    uint32_t done = 0;
    uint32_t end = base + size;

    if (size == 0)
        return 0;
    while (done < size) {
        plat_region q;
        uint32_t addr = base + done;
        uint32_t rend, chunk;
        char     why[128];

        plat_query(addr, &q);              /* -1 == free, which is a valid
                                            * answer here, not an error */
        if (!q.size && q.is_free) {
            /* not reserved here - take a block (64 KiB is the granularity
             * on Windows; on POSIX the anonymous map granularity) */
            uint32_t take = (addr + 0x10000u <= end) ? 0x10000u : (end - addr);
            if (!plat_commit((uintptr_t)addr, take, (unsigned)prot)) {
                unsigned err = plat_error();
                plat_error_text(err, why, sizeof why);
                fprintf(stderr, "le: %s: cannot reserve @0x%X+%X (%u: %s)\n",
                        what, addr, take, err, why);
                return -1;
            }
            chunk = take;
        } else {
            rend = (uint32_t)q.base + (uint32_t)q.size;
            chunk = (rend < end ? rend : end) - addr;
            if (chunk == 0 || rend <= addr) {
                fprintf(stderr, "le: %s: degenerate region at 0x%X\n", what,
                        addr);
                return -1;
            }
            if (!plat_commit((uintptr_t)addr, chunk, (unsigned)prot)) {
                unsigned err = plat_error();
                plat_error_text(err, why, sizeof why);
                fprintf(stderr, "le: %s: cannot commit @0x%X+%X (%u: %s) "
                        "prot=0x%X region=0x%lX\n",
                        what, addr, chunk, err, why, q.prot,
                        (unsigned long)q.size);
                return -1;
            }
        }
        done += chunk;
    }
    return 0;
}

/* Game address space: LE objects + VGA frame buffer + a low-memory mirror.
 * Reserved up front so the CRT heap cannot take these addresses. */
static int g_early_reserved;
static uint32_t g_early_failed_mask;   /* bit i: 64 KiB block i could not be reserved */

/* Pre-CRT variant: no stdio, only kernel32 calls. Called from the process
 * entry point before the CRT heap exists, so the window cannot be stolen.
 *
 * Reserve the guest window (0x10000..0x100000) in 64 KiB blocks: one big
 * request fails as soon as *anything* occupies part of the range, and on a
 * busy machine the loader sometimes drops a DLL into low memory - which made
 * booting fail intermittently with a misleading "this image landed at
 * 0x5Fxxxxxx, which overlaps ..." message. Block granularity keeps the
 * mandatory part (the object window 0x10000..0x70000) working and records the
 * rest so a failure that actually matters is reported precisely, later. */
int le_reserve_address_space_early(void)
{
    uint32_t a;
    int critical_ok = 1;

    for (a = FD2_OBJ_REGION_BASE; a < FD2_LOW_LIMIT; a += 0x10000u) {
        if (plat_commit((uintptr_t)a, 0x10000u, PLAT_PROT_RWX))
            continue;
        g_early_failed_mask |= 1u << ((a - FD2_OBJ_REGION_BASE) / 0x10000u);
        if (a < 0x00070000u)                 /* objects live here: mandatory */
            critical_ok = 0;
    }
    if (!critical_ok)
        return -1;
    g_early_reserved = 1;
    return 0;
}

int le_reserve_address_space(void)
{
    if (g_early_reserved) {
        if (g_early_failed_mask)
            fprintf(stderr,
                    "le: guest window blocks 0x%X not reserved (the loader put "
                    "something there) - boot continues, but the game fails if "
                    "it needs those addresses\n", g_early_failed_mask);
        return 0;                       /* already won the race in fd2_entry */
    }
    if (!le_reserve_address_space_early())
        return 0;
    {
        /* Same report as before the platform split (docs/PITFALLS.md -8-48
         * quotes these lines as the retry signature): the code, our own
         * image base, and one line saying who owns the address. */
        unsigned err = plat_error();
        char why[128], desc[512];

        fprintf(stderr, "le: cannot reserve object region @0x%X: %u\n",
                FD2_OBJ_REGION_BASE, err);
        fprintf(stderr, "    this image is loaded at 0x%p\n",
                plat_image_base());
        plat_error_text(err, why, sizeof why);
        fprintf(stderr, "    %s\n", why);
        plat_describe((uintptr_t)FD2_OBJ_REGION_BASE, desc, sizeof desc);
        fprintf(stderr, "    %s\n", desc);
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
 * is consumed sequentially.
 *
 * The one thing that is NOT derivable from the object table is how many bytes
 * of the final page are stored: the object page table in these DOS/4GW files
 * is degenerate (entries are just `(page_index+1) << 16`, no file offsets),
 * so the header field at +0x2C is used instead. That field is "bytes of data
 * in the last page":
 *
 *     FD2  vsize(last)=0x34D2, +0x2C=0x4D2 -> on-disk 0x34D2  start 0x36014
 *     FDPS vsize(last)=0x0054, +0x2C=0x0035 -> on-disk 0x0035  start 0xF000
 *
 * Both match the bytes IDA shows at the entry point (0x43008); using vsize
 * for FDPS shifts the whole image 0x1F bytes early and the game then executes
 * a misaligned instruction stream (`mov es,[ebx]` with EBX=0 at 0x4E01A). */
static size_t compute_image_start(const le_image *le)
{
    size_t total = 0;
    uint32_t i;

    for (i = 0; i < le->object_count; i++) {
        const le_object *o = &le->objects[i];
        size_t span = (size_t)o->page_count * LE_PAGE_SIZE;

        if (i + 1 == le->object_count) {
            size_t tail = le->last_page_bytes;
            if (tail == 0 || tail > LE_PAGE_SIZE) {
                tail = o->vsize % LE_PAGE_SIZE;      /* fallback: vsize */
                if (tail == 0) tail = LE_PAGE_SIZE;
            }
            span = (size_t)(o->page_count - 1) * LE_PAGE_SIZE + tail;
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
        void *p = map_at(o->base, span, PLAT_PROT_RWX, "object");
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
        void *p = map_at(o->base, o->vsize, PLAT_PROT_RWX, "flat");
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
