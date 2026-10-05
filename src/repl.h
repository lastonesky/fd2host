/* repl.h - run translated C in place of the original machine code.
 *
 * The translated modules in src/game/ have been verified byte-for-byte
 * against the original machine code (see PROGRESS.md §19..§25). This layer
 * installs them into a running game by overwriting each original function's
 * entry with a 5-byte `jmp rel32` to the C implementation - the same
 * mechanism src/ail.c already uses for the Miles AIL entry points.
 *
 * It is safe here because:
 *   - the patched functions are ordinary cdecl functions, and the MSVC C
 *     implementations preserve the callee-saved registers the Watcom caller
 *     expects (a superset of what the originals guarantee);
 *   - no game code outside the replaced set reads the scratch globals the
 *     originals write (verified with IDA: all xrefs to 0x627A3..0x627B6,
 *     0x6017B, 0x60060..0x6017A are inside the replaced set);
 *   - the patch is applied after le_map_and_relocate (image is RWX) and the
 *     FD2 layout is known, so it is gated like the AIL patch.
 *
 * The on-disk EXE and the letest reference image are untouched.
 */
#ifndef FD2_REPL_H
#define FD2_REPL_H

#include <stdint.h>

#define REPL_RLE      0x01u
#define REPL_GFX      0x02u
#define REPL_SPRITE24 0x04u
#define REPL_UTIL     0x08u
#define REPL_PATH     0x10u
#define REPL_ALL      (REPL_RLE | REPL_GFX | REPL_SPRITE24 | REPL_UTIL | REPL_PATH)

/* "all" | "none" | comma-separated group names -> mask */
unsigned repl_parse(const char *spec);

/* Patch the selected entries. Returns the number of functions replaced. */
unsigned repl_install(uint8_t *obj0_base, unsigned mask);

#endif /* FD2_REPL_H */
