/* svc.h - FD2 platform service wrappers (source translation).
 *
 *   0x17AA9  svc_wait_ticks   wait until the BIOS tick has advanced by N
 *   0x25A96  svc_play_sfx     start a PCM sound effect
 *   0x25B45  svc_play_sfx2    the same routine on the second handle
 *
 * Both are app-level routines: they read and write the game's own
 * data-segment globals by their original addresses, exactly like the machine
 * code does, so src/repl.c can hook them without any wrapper (cdecl, identical
 * signatures).
 *
 * svc_wait_ticks(n) - 0x17AA9:
 *   Busy-waits on the BIOS timer tick word (BDA 0x46C, 18.2 Hz) until it has
 *   moved forward by at least `n`. The original compares 16-bit readings with
 *   sign extension and adds 0x10000 when the difference is negative, so the
 *   0xFFFF -> 0x0000 wrap-around counts as +1 rather than -65535; the C does
 *   the same. The last reading is left in the global dword_53A2C and returned
 *   in EAX. Every one of the ~45 call sites ignores the return value (IDA:
 *   they all overwrite EAX immediately), so nothing depends on it.
 *   n <= 0 falls straight through; there is no upper bound -
 *   the caller's loop terminates only when the tick moves.
 *
 * svc_play_sfx(bank, index, loops) - 0x25A96:
 *   Plays one sample of the SFX container through the game's sample handle
 *   dword_53EE4 (allocated in main at 0x25C43). `bank` is what main keeps in
 *   dword_53EEC: FDOTHER.DAT container #0x1F (main at 0x25C65..0x25C78).
 *   Its layout is what the code reads: entry `index` is the byte range
 *   [off[i], off[i+1]) where off[i] is the dword at bank + 6 + 4*i - a six
 *   byte header followed by an offset table one dword longer than the sample
 *   count.
 *
 *   Nothing plays unless the digital driver came up (byte_53EF1, set in main
 *   at 0x25C3B once AIL handed back a driver handle), sound effects are on
 *   (byte_51E62, the options panel item documented in
 *   docs/data/ida/fd2_system_overlay_options_ida.txt) and the busy gate
 *   dword_54133 is clear. All three stay original globals, read exactly where
 *   the machine code reads them.
 *   index == -1 stops the running sample and does not start a new one.
 *
 * Verified against the machine code by src/typecheck.c.
 */
#ifndef GAME_SVC_H
#define GAME_SVC_H

#include <stdint.h>

/* 0x17AA9 - wait N BIOS ticks. Returns the last tick reading (callers ignore
 * it) and leaves the same value in the global dword_53A2C. */
int svc_wait_ticks(int n);

/* 0x25A96 - play sample `index` of `bank`; `loops` is forwarded to
 * AIL_set_sample_loop_count unchanged (the typewriter step passes 1). */
int svc_play_sfx(const void *bank, int index, int loops);

/* 0x25B45 - byte-for-byte the same routine on the second sample handle
 * dword_53EE8 (11 call sites); only the handle differs. */
int svc_play_sfx2(const void *bank, int index, int loops);

#endif /* GAME_SVC_H */
