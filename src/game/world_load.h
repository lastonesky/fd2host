/* world_load.h - FD2 per-scene world (map tiles + party records) loading.
 *
 *   0x10652  world_load_tiles   pick the tile set for the current view mode
 *                               (dword_53C03) and decode it into dword_53AFF
 *   0x1088D  world_load_party   load one party/scene slot: the text stream
 *                               (FDTXT), the field/cell table + shape bank
 *                               (FDFIELD/FDSHAP), rebuild the 80-byte records
 *                               from FDFIELD and the FDICON.B24 sprite atlas,
 *                               then build the unit atlas (0x10B4E).
 *
 * Both are reached from menu_reload_world (0x205DA) and from the main state
 * machine. They use the game heap (guest_mem) and the real data files, so the
 * differential harness (src/worldcheck.c) redirects the Watcom CRT entries to
 * the host libc.
 */
#ifndef FD2_GAME_WORLD_LOAD_H
#define FD2_GAME_WORLD_LOAD_H

#include <stdint.h>

/* 0x10652 */
void world_load_tiles(void);

/* 0x1088D - `slot` is the caller's scene id (0x205DA passes dword_53C03). */
void world_load_party(int slot);

#endif /* FD2_GAME_WORLD_LOAD_H */
