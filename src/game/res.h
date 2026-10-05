/* res.h - FD2 LMI container resource loader (source translation of 0x111BA).
 *
 * Original signature (proved from call sites): cdecl, three arguments
 *   void *sub_111BA(const char *filename, void *old_buffer, int index);
 *
 * It frees `old_buffer`, opens the container, reads the 8-byte directory
 * entry at file offset 4*index + 6 (two little-endian u32: start, end),
 * allocates (end - start) bytes, seeks to `start`, reads the resource and
 * returns the buffer. The size is also stored in the original global
 * dword_53BFF (mirrored here as res_size).
 *
 * The LMI directory therefore lives at +6 and holds `count+1` u32 offsets;
 * resource i occupies [offset[i], offset[i+1]).
 *
 * On file-not-found / out-of-memory the original prints a message and jumps
 * to exit(1) (0x1005E); the translation does the same.
 *
 * Verified against the machine code by src/rescheck.c (the CRT file/memory
 * calls are redirected to the host libc, exactly the substitution the final
 * port performs). Disassembly: re/res_disasm.txt.
 */
#ifndef GAME_RES_H
#define GAME_RES_H

#include <stdint.h>

extern uint32_t res_size;   /* original dword_53BFF @ 0x53BFF */

void *res_load(const char *filename, void *old_buffer, int index);

#endif /* GAME_RES_H */
