/* vm.h - FD2 script/text VM (source translation).
 *
 *   0x15F84  vm_run  - the int16 word-stream interpreter behind every line of
 *                      dialogue, every portrait swap and every number the
 *                      game prints: 1380 bytes, 126 direct call sites.
 *
 * Real ABI (re-verified in §37, corrections below): **9 cdecl stack
 * parameters** - each caller does nine `push`es and `add esp,24h`. IDA's
 * `__usercall ... 14 register parameters` was an artifact: the function opens
 * with `push 5Ch; call 0x3702F`, and 0x3702F -> 0x37042 is the Watcom stack
 * probe (_chkstk: `&retaddr - size` vs the stack limit in dword_52814). The
 * probe's EAX-in/EAX-out prototype propagated to every function it guards,
 * so IDA invented register parameters that callers never set.
 *
 *   vm_run(stream, sub, addr, pitch, fg, shadow, bgfill, line_step, wait)
 *
 *     stream    script container: int16 byte-offsets at +2*sub select a
 *               sub-stream, the interpreter starts at stream + offset[sub]
 *     sub       which sub-stream of `stream` to run
 *     addr      VGA write address for the text (callers pass 0xA0000,
 *               0xA9F23, 0xAB6E3 ... - it is a mode 13h linear address)
 *     pitch     bytes per scanline (always 320)
 *     fg        colour written for a set font pixel          (always 205)
 *     shadow    colour of the drop shadow pixel              (always 76)
 *     bgfill    byte填充 of the 16x16 cell before drawing    (always 74)
 *     line_step scanlines a "-2/-3 new paragraph" advances by (always 19)
 *     wait      non-zero: draw one character, then dlg_type_step per char
 *               (typewriter); zero: draw without stepping
 *
 *   Returns the final VGA write address (the recursion chains it through).
 *   The entry point's `case -1` looks like `JUMPOUT(0x15309)` in IDA: that is
 *   a *shared epilogue* living at the tail of sub_15055 (identical frame:
 *   `add esp,24h; pop ebp/edi/esi/ebx; retn`) - i.e. `return cur;`.
 *
 * Verified against the machine code by src/vmcheck.c: both sides run through
 * the same hooked stubs and the whole event sequence (every callee with its
 * arguments), the globals the VM owns and the return value are compared.
 */
#ifndef GAME_VM_H
#define GAME_VM_H

#include <stdint.h>

/* 0x15F84 - run the word stream; returns the final write address. */
int vm_run(void *stream, int sub, int addr, int pitch,
           int fg, int shadow, int bgfill, int line_step, int wait);

#endif /* GAME_VM_H */
