/* map.h - map view / terrain tile rendering (main state machine family). */
#ifndef GAME_MAP_H
#define GAME_MAP_H

#include <stdint.h>

/* 0x126F7 - blit terrain tile `index` (24x24) at view cell (x, y).
 * Positions outside the current view (origin 0x53AA9/0x53AAD, size
 * 0x51A87/0x51A8B) are dropped; the tile is drawn into the map bitmap
 * (*(0x53A49) + (y-oy)*10944 + (x-ox)*24 + 32904, stride 456). */
void map_blit_tile(int x, int y, int index);

#endif /* GAME_MAP_H */

/* 0x12E38 - read one map cell (x,y) into 8 bytes: tile, flags, 4 table bytes */
void map_cell_info(int x, int y, uint8_t *out);

/* 0x12AC6 / 0x129EC - cell object sprites */
void map_blit_cell_sprite(void *dst, int x, int y);
void map_refresh_records(void);
