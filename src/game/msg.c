/* msg.c - dialogue portrait compositor (source translation).
 *
 *   0x1956B  msg_open_portrait   (352 B, 52 call sites)
 *   0x1974C  msg_blit_band       (153 B,  9 call sites)
 *   0x26996  msg_close_portrait  (119 B, 42 call sites)
 *
 * The three functions share a 320x200 (64000-byte) screen-pair held in the
 * *original* data-segment globals dword_53C5B / dword_53C5F / dword_53C63:
 *
 *   dword_53C63  staging buffer  - the box is drawn here, then the portrait
 *   dword_53C5F  saved screen    - the VGA snapshot close_portrait restores
 *   dword_53C5B  working screen  - band compositor destination
 *
 * Only three globals + dword_53A85 (the DATO resource) + dword_53C67 (the
 * DATO sub-image offset) are involved; all stay at their machine addresses
 * because dozens of not-yet-translated sites read them. This module writes
 * the real dwords.
 *
 * Heap: the original allocates the three buffers with the Watcom CRT
 * (`0x3706E malloc`) and frees them with `0x3776E free`; the C goes through
 * src/game/guest_mem.h so there is exactly one heap on every path. That is
 * the only reason this cluster was postponed past round 28.
 *
 * Services are called through their **original addresses** (the scene.c
 * pattern): in the host src/repl.c has already patched dlg_box_stage /
 * res_load / rle2_blit_mirror / msg_blit_band to C, and src/msgcheck.c hooks
 * them to recording stubs - either way the call does what the machine code did.
 *
 * ABI: all three are ordinary cdecl stack functions. Hex-Rays prints them as
 * `__usercall ...(a1..a7)` only because of the Watcom `push <frame>; call
 * 0x3702F` stack probe (PITFALLS §8-75); the real arguments are the stack
 * slots read below. 0x1974C jumps to the shared epilogue 0x16F04
 * (`add esp,0Ch; pop edi/esi/ebx; retn`) - spelled here as a plain return.
 *
 * The original overwrites the three globals without freeing the previous
 * buffers and leaves them dangling after close; the C copies that behaviour
 * byte for byte (no "fix the leak").
 */
#include "msg.h"
#include "guest_mem.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* --- service entry points (called at their original addresses) --------- */
typedef void  (*box_stage_fn)(void *surface, int stride, int x0, int y0,
                              int cols, int lines);
typedef void *(*res_load_fn)(const char *name, void *old_buffer, int index);
typedef void  (*rle2_mirror_fn)(void *dst, const void *src, int stride);
typedef void  (*blit_band_fn)(int y, void *dst, void *src);

#define BOX_STAGE   ((box_stage_fn) (uintptr_t)0x000168B6u)   /* 0x168B6 */
#define RES_LOAD    ((res_load_fn)  (uintptr_t)0x000111BAu)   /* 0x111BA */
#define RLE2_MIRROR ((rle2_mirror_fn)(uintptr_t)0x0004EC31u)  /* 0x4EC31 */
#define BLIT_BAND   ((blit_band_fn) (uintptr_t)0x0001974Cu)   /* 0x1974C */

/* --- original data-segment globals ------------------------------------- */
#define dword_53C5B (*(uint32_t *)(uintptr_t)0x00053C5Bu)  /* working screen  */
#define dword_53C5F (*(uint32_t *)(uintptr_t)0x00053C5Fu)  /* saved screen    */
#define dword_53C63 (*(uint32_t *)(uintptr_t)0x00053C63u)  /* staging buffer  */
#define dword_53A85 (*(uint32_t *)(uintptr_t)0x00053A85u)  /* DATO resource   */
#define dword_53C67 (*(uint32_t *)(uintptr_t)0x00053C67u)  /* portrait offset */

#define VGA_BASE      0x000A0000u
#define SCREEN_BYTES  64000        /* 320 * 200 */
#define BAND_ROWS     86           /* 0x56: nominal band height */
#define BAND_SRC_OFF  0x8C05       /* 35845 = 112*320 + 5 (portrait top) */
#define BAND_WIDTH    310          /* 0x136 */

void msg_open_portrait(int id)
{
    dword_53C5B = (uint32_t)(uintptr_t)guest_malloc(SCREEN_BYTES);
    dword_53C5F = (uint32_t)(uintptr_t)guest_malloc(SCREEN_BYTES);
    dword_53C63 = (uint32_t)(uintptr_t)guest_malloc(SCREEN_BYTES);

    memmove((void *)(uintptr_t)dword_53C5F,
            (const void *)(uintptr_t)VGA_BASE, SCREEN_BYTES);
    memmove((void *)(uintptr_t)dword_53C63,
            (const void *)(uintptr_t)dword_53C5F, SCREEN_BYTES);

    /* box frame: 19 columns x 5 lines of 16px tiles at (5,112) */
    BOX_STAGE((void *)(uintptr_t)dword_53C63, 320, 5, 112, 19, 5);

    /* portrait id -> DATO sub-image offset (immediates, not a table) */
    switch (id) {
    case 0x80: dword_53C67 = 0x10BB; break;
    case 0x81: dword_53C67 = 0x06AB; break;
    case 0x82: dword_53C67 = 0x0F63; break;
    case 0x83: dword_53C67 = 0x0576; break;
    case 0x84: dword_53C67 = 0x0E3C; break;
    default:   dword_53C67 = 0x9017; break;
    }

    dword_53A85 = (uint32_t)(uintptr_t)
        RES_LOAD("DATO.DAT", (void *)(uintptr_t)dword_53A85, id);

    /* the resource's first byte is the header length; the decode stream
     * starts right after it (movzx ebx,byte[eax]; add eax,ebx) */
    RLE2_MIRROR((void *)(uintptr_t)(dword_53C63 + dword_53C67),
                (const void *)(uintptr_t)
                    (dword_53A85 + *(const uint8_t *)(uintptr_t)dword_53A85),
                320);

    /* top to bottom: rows 5,4,3,2,1,0 at y = 13*i + 112 */
    {
        int i;
        for (i = 5; i >= 0; --i)
            BLIT_BAND(13 * i + 112,
                      (void *)(uintptr_t)dword_53C5B,
                      (void *)(uintptr_t)dword_53C63);
    }
}

void msg_blit_band(int y, void *dst, void *src)
{
    int n = BAND_ROWS;
    int i;

    /* start from the saved screen, then overlay the portrait strip */
    memmove(dst, (const void *)(uintptr_t)dword_53C5F, SCREEN_BYTES);

    if (y + BAND_ROWS >= 200)
        n = 200 - y;                            /* clip at the bottom edge */

    for (i = 0; i < n; ++i)
        memmove((uint8_t *)dst + 320 * i + 5 + 320 * y,
                (const uint8_t *)src + 320 * i + BAND_SRC_OFF,
                BAND_WIDTH);

    memmove((void *)(uintptr_t)VGA_BASE, dst, SCREEN_BYTES);
}

void msg_close_portrait(void)
{
    int i;

    /* rows 1..5 only: row 0 was already composed by open */
    for (i = 1; i < 6; ++i)
        BLIT_BAND(13 * i + 112,
                  (void *)(uintptr_t)dword_53C5B,
                  (void *)(uintptr_t)dword_53C63);

    memmove((void *)(uintptr_t)VGA_BASE,
            (const void *)(uintptr_t)dword_53C5F, SCREEN_BYTES);

    guest_free((void *)(uintptr_t)dword_53C5B);
    guest_free((void *)(uintptr_t)dword_53C5F);
    guest_free((void *)(uintptr_t)dword_53C63);
}
