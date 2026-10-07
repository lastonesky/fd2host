/* res.h - FD2 LMI container resource loader (source translation of 0x111BA).
 *
 * Original signature (proved from call sites): cdecl, three arguments
 *   void *sub_111BA(const char *filename, void *old_buffer, int index);
 *
 * It frees `old_buffer`, opens the container, reads the 8-byte directory
 * entry at file offset 4*index + 6 (two little-endian u32: start, end),
 * allocates (end - start) bytes, seeks to `start`, reads the resource and
 * returns the buffer. The size is also stored in the original global
 * dword_53BFF (0x53BFF), which the *game* reads - so it is written through
 * guest_store_u32(), not to a C variable.
 *
 * Heap: both the freed `old_buffer` and the returned buffer live on the
 * **game's** heap (guest_malloc/guest_free -> Watcom CRT 0x3706E/0x3776E), so
 * a caller on either side of the C / machine-code boundary frees correctly.
 * See src/game/guest_mem.h.
 *
 * The LMI directory therefore lives at +6 and holds `count+1` u32 offsets;
 * resource i occupies [offset[i], offset[i+1]).
 *
 * On file-not-found / out-of-memory the original prints a message and jumps
 * to exit(1) (0x1005E); the translation does the same (host exit; only the
 * message's destination differs - host.log instead of the game's stdout).
 *
 * Verified against the machine code by src/rescheck.c (the CRT file/memory
 * calls are redirected to the host libc, exactly the substitution the final
 * port performs). Disassembly: re/res_disasm.txt.
 */
#ifndef GAME_RES_H
#define GAME_RES_H

#include <stdint.h>

/* where res_load publishes the resource size (original dword_53BFF) */
#define RES_SIZE_ADDR 0x00053BFFu

void *res_load(const char *filename, void *old_buffer, int index);

/* 0x2EB9F - decode and blit sub-image `index` of an LMI buffer straight to
 * `dst`: the buffer's offset table starts at +8, the sub-image header carries
 * w/h (u16 each) and the RLE stream starts at +9 (see src/leafcheck.c). */
void res_blit(void *buf, int index, void *dst, int pitch, int mode);

#endif /* GAME_RES_H */
