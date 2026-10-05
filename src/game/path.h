/* path.h - FD2 terrain-cost flood fill and path tracer (source translation).
 *
 * Original entry points in our build (FD2.EXE md5 a6e341a8...):
 *
 *   0x4E390  path_mark   one DFS pass: mark every cell reachable with the
 *                        remaining movement points (recursive flood fill)
 *   0x4E4F6  path_find   a second pass that traces movement paths and,
 *                        on reaching the target, commits the best one
 *
 * The cluster also contains 0x4E42C/0x4E4BE (first pass recursion/check),
 * 0x4E5CC/0x4E680 (second pass recursion/check), 0x4E703/0x4E71F/0x4E751
 * (target marking, turn counting, path commit).
 *
 * Map layout (the "grid" argument):
 *   +0  u8 width  W
 *   +1  (unused)
 *   +2  u8 height H
 *   +3  (unused)
 *   +4  W*H cells, 4 bytes each
 * The cell pointer used by the original is `map + 7 + 4*(y*W + x)`, i.e. it
 * points at the cell's LAST byte:
 *   cell[-3] byte0   low byte of the terrain id
 *   cell[-2] byte1   bits0-1: terrain id high bits, later turn-count (bits2-7)
 *   cell[-1] byte2   flags: 0x40 = target, 0x80 = clamp remaining to 0
 *   cell[ 0] byte3   remaining movement points
 *
 * Terrain cost: t = ((cell[-2] & 3) << 8) | cell[-3];
 *               cost = cost_row[ cost_table[4*t + 1] ].
 * Neighbour order explored: right, left, down, up (stride 4 and 4*W).
 * A path is only extended when the cell's remaining points strictly improve
 * (second pass additionally allows equal points with fewer direction turns).
 *
 * Verified byte-for-byte against the machine code by src/pathcheck.c
 * (build.ps1 -Target pathcheck). Disassembly: re/path_disasm.txt.
 */
#ifndef GAME_PATH_H
#define GAME_PATH_H

#include <stdint.h>

/* 0x4E390 - mark the reachable region from (x,y). `cost_row` is the
 * per-unit movement cost row (original arg0), `map` the grid, `cost_table`
 * the terrain -> cost-row-index table (original arg5). */
void path_mark(int cost_row, int x, int y, int start_mp, void *map,
               const void *cost_table);

/* 0x4E4F6 - trace a path from (x,y) to (target_x,target_y) and write the
 * best route's direction codes to `out`. `mode` (original byte_6017A):
 *   0 = plain, 1 = prefer fewer turns on equal MP, 2 = mark target.
 * Returns the best path length (original byte_60078, 0xFF if none). */
int path_find(int cost_row, int x, int y, int start_mp, void *out,
              int target_x, int target_y, int mode, void *map,
              const void *cost_table);

/* exposed for the differential test */
extern int path_depth;   /* original byte_60077 */
extern int path_best;    /* original byte_60078 */

#endif /* GAME_PATH_H */
