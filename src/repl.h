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
#define REPL_DLG      0x20u
#define REPL_REC      0x40u
#define REPL_SVC      0x80u
#define REPL_VM       0x100u
#define REPL_RES      0x200u
#define REPL_BGM      0x400u
#define REPL_SCENE    0x800u
#define REPL_FADE     0x1000u
#define REPL_MAP      0x2000u
#define REPL_FX       0x4000u
#define REPL_EV2      0x8000u   /* funcs_1199C batch 2 (src/game/ev2.c) */
#define REPL_EV3      0x10000u  /* funcs_1199C batch 3 (src/game/ev3.c) */
#define REPL_EV4      0x20000u  /* scene/map helpers batch 4 (src/game/ev4.c) */
#define REPL_EV5      0x40000u  /* small leaves batch 5 (src/game/ev5.c) */
#define REPL_EV6      0x80000u   /* funcs_1199C batch 6 (src/game/ev6.c) */
#define REPL_UNITLD   0x100000u  /* unit sprite builder (src/game/unit_load.c) */
#define REPL_ALL      (REPL_RLE | REPL_GFX | REPL_SPRITE24 | REPL_UTIL | REPL_PATH | REPL_DLG | REPL_REC | REPL_SVC | REPL_VM | REPL_RES | REPL_BGM | REPL_SCENE | REPL_FADE | REPL_MAP | REPL_FX | REPL_EV2 | REPL_EV3 | REPL_EV4 | REPL_EV5 | REPL_EV6 | REPL_UNITLD)

/* "all" | "none" | comma-separated group names -> mask.
 * A token may be prefixed with '-' to clear that group after adding, so
 * `all,-ev2` is the full set minus one batch. */
unsigned repl_parse(const char *spec);

/* Patch the selected entries. Returns the number of functions replaced. */
unsigned repl_install(uint8_t *obj0_base, unsigned mask);

#endif /* FD2_REPL_H */
