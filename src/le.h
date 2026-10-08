/* le.h - Linear Executable (LE/LX) loader for the FD2 native port.
 *
 * Layout established empirically for FD2.EXE (DOS/4GW + Watcom, LE image at
 * file offset 0x27ACC) and cross-checked against Ghidra's own LE loader:
 *
 *   objects  : base 0x10000 / 0x50000 / 0x60000, 63 / 4 / 4 pages of 4 KiB
 *   entry    : object #1 (1-based) + 0x2CCB4  =>  linear 0x3CCB4
 *   fixups   : 7-byte records, block per page:
 *                [0]=type/flag(0x07) [1]=? [2..3]=src page offset
 *                [4]=target object (1-based) [5..6]=target object offset
 *              loader writes: *(u32*)src = target_obj_base + target_off
 */
#ifndef FD2_LE_H
#define FD2_LE_H

#include <stdint.h>
#include <stddef.h>
#include "platform.h"

#define LE_MAX_OBJECTS 64
#define LE_PAGE_SIZE   0x1000

typedef struct {
    uint32_t vsize;
    uint32_t base;        /* link-time linear base address            */
    uint32_t flags;
    uint32_t page_index;
    uint32_t page_count;
} le_object;

typedef struct {
    uint8_t *data;        /* whole file, host-allocated               */
    size_t   size;
    size_t   le_offset;   /* offset of the LE header inside data      */

    uint32_t module_pages;
    uint32_t eip_object;  /* 1-based                                  */
    uint32_t eip;         /* offset relative to that object's base    */
    uint32_t object_count;

    le_object objects[LE_MAX_OBJECTS];

    uint32_t fixup_page_table;   /* offset relative to LE header      */
    uint32_t fixup_record_table; /* offset relative to LE header      */

    /* Bytes of the *final* page actually stored in the file - LE header field
     * +0x2C. It differs from vsize % 0x1000 when the tail of the last object
     * is BSS: FD2 stores the whole 0x4D2 tail of its 0x34D2 object, FDPS
     * stores only 0x35 of a 0x54 object (the other 0x1F bytes are
     * zero-initialised). Assuming vsize for both shifts FDPS's image 0x1F
     * bytes early and the game executes a misaligned instruction stream. */
    uint32_t last_page_bytes;

    uint32_t entry_linear;       /* computed absolute entry address   */
    size_t   image_start;        /* file offset of first object page  */
    size_t   image_end;
} le_image;

/* Reserves the whole fixed address space the game needs BEFORE any other
 * allocation happens. The 32-bit CRT heap grows from 0x10000 upwards, so it
 * will otherwise steal the addresses the DOS/4GW objects were linked for.
 * Call this as the very first statement of the host's main/WinMain. */
int le_reserve_address_space(void);

/* Same, but safe to call before the CRT is initialised (no stdio). Used from
 * the custom process entry point so the reservation wins the race against
 * every CRT and loader allocation. */
int le_reserve_address_space_early(void);

/* Parses the container. Returns 0 on success. */
int  le_open(le_image *le, const char *path);

/* Maps every object at its link-time base (VirtualAlloc + copy) and applies
 * the LE fixups. Must be called once from the host before execution.
 * Returns 0 on success; *fixups_applied receives the record count. */
int le_map_and_relocate(le_image *le, int *fixups_applied);

/* Loads an already-relocated flat image instead of parsing the exe
 * (used to cross-check the loader against Ghidra's relocated copy). */
int  le_map_flat(le_image *le, const char *path);

/* Commit a range inside the guest window *region by region*.
 *
 * VirtualAlloc(MEM_COMMIT) validates the request against the ONE region that
 * contains lpAddress, so a range spanning several reservations fails with
 * ERROR_INVALID_ADDRESS (487) even when every page is already committed -
 * which is what happened after the early reservation was split into per-64KiB
 * blocks. Free sub-blocks (the loader may own a neighbour) are reserved first.
 * `prot` is a PLAT_PROT_* value (platform.h). Returns 0 on success, and
 * it is safe to call before CRT init. */
int  le_commit_range(uint32_t base, uint32_t size, int prot, const char *what);

void le_close(le_image *le);

#endif /* FD2_LE_H */
