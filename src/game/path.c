/* path.c - FD2 terrain-cost flood fill and path tracer (source translation).
 *
 * Translation of the 0x4E390..0x4E751 cluster:
 *   0x4E390 path_mark   entry: one DFS flood fill
 *   0x4E4F6 path_find   entry: path tracing + best-path commit
 *   0x4E42C rec1         first-pass recursion (right, left, down, up)
 *   0x4E4BE check1       first-pass reachability / remaining-points update
 *   0x4E5CC rec2         second-pass recursion (records direction + depth)
 *   0x4E680 check2       second-pass check with turn-count tie-break
 *   0x4E703 mark_target  mode 2: write the found cell to the output
 *   0x4E71F count_dirs   number of direction changes in the current stack
 *   0x4E751 commit_path  commit the current stack as the best path
 *
 * The original passes registers around (usercall) and keeps an explicit
 * 8-byte record stack in `unk_60079` for the second pass; the C version
 * uses an ordinary typed stack, which is observationally equivalent (only
 * the map, the output buffer and byte_60078 escape).
 *
 * Verified by src/pathcheck.c. Documentation: game/path.h.
 */
#include "path.h"
#include <string.h>

/* ---- mirror of the original globals (0x60060..0x6017A) ---- */
static uint8_t        g_6006E;      /* start x            */
static uint8_t        g_6006F;      /* start y            */
static uint8_t        g_60070;      /* start value        */
static uint8_t       *g_out;        /* 0x60073 output     */
static uint16_t       g_target;     /* 0x60071 target     */
static int            g_mode;       /* 0x6017A            */
static uint8_t       *g_map;        /* 0x60064            */
static uint8_t        g_W, g_H;     /* 0x60068/0x60069    */
static const uint8_t *g_costtable;  /* 0x60060            */

int path_depth;                     /* 0x60077 */
int path_best;                      /* 0x60078 */

/* second-pass record stack (original unk_60079, 8-byte records, ~32 deep) */
struct path_rec { uint16_t dx; uint8_t cl; uint8_t ch; uint8_t *cell; };
#define PATH_STACK_MAX 64
static struct path_rec g_stack[PATH_STACK_MAX];

/* 0x4E71F: count direction changes among the first byte_60077 records. */
static uint8_t count_dirs(void)
{
    int i;
    uint8_t n = 0, prev = 0xFF;
    int depth = (uint8_t)path_depth;    /* original: 8-bit loop counter */

    for (i = 0; i < depth; i++) {
        if (g_stack[i].ch != prev) {
            n++;
            prev = g_stack[i].ch;
        }
    }
    return (uint8_t)(n * 4);
}

/* 0x4E751: if dx == target, commit the current stack as the best path. */
static void commit_path(uint16_t dx)
{
    if (dx == g_target) {
        if ((uint8_t)path_depth <= (uint8_t)path_best) {
            path_best = (uint8_t)path_depth;
            if (path_depth != 0) {
                int i;
                for (i = 0; i < path_depth; i++)
                    g_out[i] = g_stack[i].ch;
            }
        }
    }
}

/* 0x4E703: mode-2 target marking. */
static void mark_target(uint16_t dx, const uint8_t *cell)
{
    if (cell[-1] & 0x40) {
        g_out[0] = (uint8_t)dx;
        g_out[1] = (uint8_t)(dx >> 8);
        path_best = 1;
    }
}

/* 0x4E4BE: first-pass reachability check. Returns 1 when the neighbour is
 * not taken (original CF set), 0 when it is. *out_cl is the new remaining
 * movement points on the taken path. */
static int check1(uint8_t cl, uint8_t *cell, const uint8_t *esi, uint8_t *out_cl)
{
    unsigned t   = ((unsigned)(cell[-2] & 3) << 8) | (unsigned char)cell[-3];
    unsigned idx = g_costtable[4 * t + 1];
    unsigned cost = esi[idx];
    uint8_t  rem;

    if (cl < cost) { *out_cl = cl; return 1; }
    rem = (uint8_t)(cl - cost);
    if ((int8_t)rem <= (int8_t)*cell) { *out_cl = rem; return 1; }
    {
        uint8_t fl = cell[-1];
        if (fl & 0x40) { *out_cl = rem; return 1; }
        if (fl & 0x80) rem = 0;
        *cell = rem;
        *out_cl = rem;
        return 0;
    }
}

/* 0x4E42C: first-pass recursion, order right=+4, left=-4, down=+4W, up=-4W */
static void rec1(uint8_t *cell, uint16_t dx, uint8_t cl, int ebp,
                 const uint8_t *esi)
{
    uint8_t x = (uint8_t)dx, y = (uint8_t)(dx >> 8);
    uint8_t ncl;

    if ((uint8_t)(x + 1) < g_W) {
        if (!check1(cl, cell + 4, esi, &ncl))
            rec1(cell + 4, (uint16_t)((uint8_t)(x + 1) | (y << 8)), ncl, ebp, esi);
    }
    if (x != 0) {
        if (!check1(cl, cell - 4, esi, &ncl))
            rec1(cell - 4, (uint16_t)((uint8_t)(x - 1) | (y << 8)), ncl, ebp, esi);
    }
    if ((uint8_t)(y + 1) < g_H) {
        if (!check1(cl, cell + ebp, esi, &ncl))
            rec1(cell + ebp, (uint16_t)(x | ((uint8_t)(y + 1) << 8)), ncl, ebp, esi);
    }
    if (y != 0) {
        if (!check1(cl, cell - ebp, esi, &ncl))
            rec1(cell - ebp, (uint16_t)(x | ((uint8_t)(y - 1) << 8)), ncl, ebp, esi);
    }
}

/* 0x4E680: second-pass check (turn-count tie-break + target commit). */
static int check2(uint16_t dx, uint8_t cl, uint8_t *cell, const uint8_t *esi,
                  uint8_t *out_cl)
{
    unsigned t   = ((unsigned)(cell[-2] & 3) << 8) | (unsigned char)cell[-3];
    unsigned idx = g_costtable[4 * t + 1];
    unsigned cost = esi[idx];
    uint8_t  rem, al;

    if (cl < cost) { *out_cl = cl; return 1; }
    rem = (uint8_t)(cl - cost);

    if ((int8_t)rem < (int8_t)*cell) { *out_cl = rem; return 1; }
    if ((int8_t)rem == (int8_t)*cell) {
        if (g_mode != 1) { *out_cl = rem; return 1; }
        al = count_dirs();
        if (al <= (uint8_t)(cell[-2] & 0xFC)) { *out_cl = rem; return 1; }
    } else {
        al = count_dirs();
    }

    al |= (uint8_t)(cell[-2] & 3);
    cell[-2] = al;

    if (g_mode == 2) {
        *cell = rem;
        mark_target(dx, cell);
        *out_cl = rem;
        return 0;
    }
    {
        uint8_t fl = cell[-1];
        if (fl & 0x40) { *out_cl = rem; return 1; }
        if (fl & 0x80) rem = 0;
        *cell = rem;
        commit_path(dx);
        *out_cl = rem;
        return 0;
    }
}

/* 0x4E5CC: second-pass recursion, records [dx][cl][ch][cell], 8 bytes each.
 * Direction codes: right=3, left=1, down=0, up=2. */
static void rec2(int depth, uint8_t *cell, uint16_t dx, uint8_t cl, int ebp,
                 const uint8_t *esi)
{
    uint8_t x = (uint8_t)dx, y = (uint8_t)(dx >> 8);
    uint8_t ncl;

    if (depth >= PATH_STACK_MAX) return;    /* original stack is ~32 deep */

    g_stack[depth].dx = dx;
    g_stack[depth].cl = cl;
    g_stack[depth].ch = 3;
    g_stack[depth].cell = cell;
    path_depth++;

    if ((uint8_t)(x + 1) < g_W) {
        if (!check2((uint16_t)((uint8_t)(x + 1) | (y << 8)), cl, cell + 4, esi, &ncl))
            rec2(depth + 1, cell + 4, (uint16_t)((uint8_t)(x + 1) | (y << 8)), ncl, ebp, esi);
    }
    g_stack[depth].ch = 1;
    if (x != 0) {
        if (!check2((uint16_t)((uint8_t)(x - 1) | (y << 8)), cl, cell - 4, esi, &ncl))
            rec2(depth + 1, cell - 4, (uint16_t)((uint8_t)(x - 1) | (y << 8)), ncl, ebp, esi);
    }
    g_stack[depth].ch = 0;
    if ((uint8_t)(y + 1) < g_H) {
        if (!check2((uint16_t)(x | ((uint8_t)(y + 1) << 8)), cl, cell + ebp, esi, &ncl))
            rec2(depth + 1, cell + ebp, (uint16_t)(x | ((uint8_t)(y + 1) << 8)), ncl, ebp, esi);
    }
    g_stack[depth].ch = 2;
    if (y != 0) {
        if (!check2((uint16_t)(x | ((uint8_t)(y - 1) << 8)), cl, cell - ebp, esi, &ncl))
            rec2(depth + 1, cell - ebp, (uint16_t)(x | ((uint8_t)(y - 1) << 8)), ncl, ebp, esi);
    }
    path_depth--;
}

void path_mark(int cost_row, int x, int y, int start_mp, void *map,
               const void *cost_table)
{
    uint8_t *m = (uint8_t *)map;
    int      ebp;
    uint8_t *cell;

    g_6006E = (uint8_t)x;
    g_6006F = (uint8_t)y;
    g_60070 = (uint8_t)start_mp;
    g_map   = m;
    g_W     = m[0];
    g_H     = m[2];
    g_costtable = (const uint8_t *)cost_table;

    ebp  = 4 * g_W;
    cell = m + 7 + 4 * ((uint8_t)y * g_W + (uint8_t)x);
    *cell = (uint8_t)start_mp;

    rec1(cell, (uint16_t)((uint8_t)x | ((uint8_t)y << 8)), (uint8_t)start_mp,
         ebp, (const uint8_t *)(uintptr_t)cost_row);
}

int path_find(int cost_row, int x, int y, int start_mp, void *out,
              int target_x, int target_y, int mode, void *map,
              const void *cost_table)
{
    uint8_t *m = (uint8_t *)map;
    int      ebp;
    uint8_t *cell;

    g_6006E = (uint8_t)x;
    g_6006F = (uint8_t)y;
    g_60070 = (uint8_t)start_mp;
    g_out   = (uint8_t *)out;
    g_target = (uint16_t)((uint8_t)target_x | ((uint8_t)target_y << 8));
    g_mode  = mode;
    g_map   = m;
    g_W     = m[0];
    g_H     = m[2];
    g_costtable = (const uint8_t *)cost_table;

    ebp  = 4 * g_W;
    cell = m + 7 + 4 * ((uint8_t)y * g_W + (uint8_t)x);
    *cell = (uint8_t)start_mp;

    path_depth = 0;
    path_best  = 0xFF;

    commit_path((uint16_t)((uint8_t)x | ((uint8_t)y << 8)));
    rec2(0, cell, (uint16_t)((uint8_t)x | ((uint8_t)y << 8)), (uint8_t)start_mp,
         ebp, (const uint8_t *)(uintptr_t)cost_row);

    return (uint8_t)path_best;
}
