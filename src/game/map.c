/* map.c - map view / terrain tile rendering (source translation of 0x126F7).
 *
 * The map bitmap lives at *(0x53A49) (a 320x200-ish surface, drawn with
 * stride 456 into a 0x8088 base offset), the tileset resource at *(0x53A4D)
 * (its sub-image offset table starts at +6), and the visible window is
 * (0x53AA9,0x53AAD) + (0x51A87,0x51A8B) cells. One cell is one 24x24 tile.
 *
 * The original bounds-checks the cell against the view and then runs the
 * already-verified sprite24_plain() on it. Layout:
 *
 *     dst = bitmap + (y - oy) * 10944 + (x - ox) * 24 + 32904
 *     src = tileset + *(u32 *)(tileset + 4*index + 6)
 *
 * (10944 = 456 * 24, i.e. 24 rows of the 456-stride bitmap.)
 */
#include "map.h"
#include "sprite24.h"

#include <stdint.h>

#define dword_53A49 (*(uint8_t **)(uintptr_t)0x00053A49u) /* map bitmap     */
#define dword_53A4D (*(uint8_t **)(uintptr_t)0x00053A4Du) /* tileset        */
#define dword_51A87 (*(int32_t  *)(uintptr_t)0x00051A87u) /* view cols      */
#define dword_51A8B (*(int32_t  *)(uintptr_t)0x00051A8Bu) /* view rows      */
#define dword_53AA9 (*(int32_t  *)(uintptr_t)0x00053AA9u) /* view x origin  */
#define dword_53AAD (*(int32_t  *)(uintptr_t)0x00053AADu) /* view y origin  */

void map_blit_tile(int x, int y, int index)
{
    const uint8_t *src;
    uint8_t       *dst;

    if (x < dword_53AA9 || dword_53AA9 + dword_51A87 <= x)
        return;
    if (y < dword_53AAD || dword_53AAD + dword_51A8B <= y)
        return;

    src = dword_53A4D + *(const uint32_t *)(dword_53A4D + 4 * index + 6);
    dst = dword_53A49 + (y - dword_53AAD) * 10944 + (x - dword_53AA9) * 24
        + 32904;
    sprite24_plain(src, dst, 456);
}
