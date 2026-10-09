/* map.h - map view / terrain tile rendering (main state machine family). */
#ifndef GAME_MAP_H
#define GAME_MAP_H

#include <stdint.h>

/* 0x126F7 - blit terrain tile `index` (24x24) at view cell (x, y).
 * Positions outside the current view (origin 0x53AA9/0x53AAD, size
 * 0x51A87/0x51A8B) are dropped; the tile is drawn into the map bitmap
 * (*(0x53A49) + (y-oy)*10944 + (x-ox)*24 + 32904, stride 456). */
void map_blit_tile(int x, int y, int index);

/* 0x11EEE - render the w x h tile window at (ox, oy) into `dst` */
void map_render_view(uint8_t *dst, int pitch, int w, int h, int ox, int oy);

/* 0x24D22 - latch (n != 0) or perform (n == 0) the screen-buffer scroll */
void map_scroll_lines(int n);

/* 0x122DC - reveal the tiles around the map cursor (dword_51A83 radius) */
void map_reveal_cursor(void);

/* 0x1ACF3 - draw the map cursor / selected-record icon and HP bar */
void map_draw_cursor(uint8_t *dst, int pitch);

/* 0x12E38 - read one map cell (x,y) into 8 bytes: tile, flags, 4 table bytes */
void map_cell_info(int x, int y, uint8_t *out);
/* 0x12AC6 / 0x129EC - cell object sprites */
void map_blit_cell_sprite(void *dst, int x, int y);
void map_refresh_records(void);

/* 0x32230 - per-record movement blip (SFX + byte_54132 counter). */
void map_unit_ping(int idx);

/* 0x11CAC - full map-view refresh; `flag != 0` skips the DAC palette
 * animation step. */
void map_view_update(int flag);

/* 0x14818 - mark the cells reachable from (x,y) with the given flood range
 * (or a diamond radius for range >= 16) and collect every unflagged record
 * standing on them, filtered by its +6 status class. Returns how many were
 * collected; `out` may be NULL. */
int map_reveal_reachable(int x, int y, uint8_t *out, int range, int radius,
                         int filter);

/* 0x1E0DB - append the four digit glyphs of `value` to the status icon queue
 * for record `rec_index`; `char_base` is the '0' glyph index. */
void map_enqueue_status(int value, int char_base, int rec_index);

/* 0x12CEA - step the map view cursor to (target_x, target_y), one line at a
 * time, waiting for a tick between steps. */
void map_slide_view(int target_x, int target_y);

/* 0x1DF58 - animate the queued status numbers: for 22 frames, redraw the
 * queued digit glyphs at rising rows and blit the view to 0xA0504. */
void map_draw_status_popups(void);

/* 0x1C2DA - draw each record in `list` (count) as a 24x24 icon in the map
 * bitmap, then flash between the current and saved bitmap five times. */
void map_draw_party_icons(int unused, int table_idx, int count, const uint8_t *list);

#endif /* GAME_MAP_H */
