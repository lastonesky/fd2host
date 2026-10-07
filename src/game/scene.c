/* scene.c - scene/state routines of the main state machine family.
 *
 * First one: 0x22E5C, reached from main (0x25BF4) when dword_53ECC == 1. It is
 * a pure sequence of services, which is why it is a good first piece of the
 * family (the state handlers in funcs_25E23[]/funcs_25E3A[] are much bigger).
 *
 * The services are called through their **original addresses** on purpose:
 *   - in the host, 0x25977 (bgm_play), 0x17AA9 (svc_wait_ticks) and 0x111BA
 *     (res_load) are already patched to C by src/repl.c, and the rest run as
 *     machine code - exactly what the original did;
 *   - src/scenecheck.c hooks them to record the call sequence, so the same C
 *     can be compared against the original 0x22E5C call by call.
 *
 * The buffer is freed with guest_free (0x3776E), the same heap res_load used -
 * the original's tail does `push ebx; jmp free`.
 */
#include "scene.h"
#include "guest_mem.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define VGA_BASE 0x000A0000u

/* --- service entry points ---------------------------------------------- */
typedef void  (*bgm_fn)(int track, int loop_count);
typedef int   (*wait_fn)(int ticks);
typedef void  (*fade_fn)(void);
typedef void *(*res_load_fn)(const char *name, void *old_buffer, int index);
typedef int   (*rle_blit_fn)(void *buf, int index, void *dst, int pitch,
                             int mode);

#define BGM_PLAY   ((bgm_fn)      (uintptr_t)0x00025977u)
#define WAIT_TICKS ((wait_fn)     (uintptr_t)0x00017AA9u)
#define FADE_OUT   ((fade_fn)     (uintptr_t)0x0001F882u)  /* 0..63: darken   */
#define FADE_IN    ((fade_fn)     (uintptr_t)0x0001F525u)  /* 64..0: restore  */
#define RES_LOAD   ((res_load_fn) (uintptr_t)0x000111BAu)
#define RLE_BLIT   ((rle_blit_fn) (uintptr_t)0x0002EB9Fu)

void scene_card(void)
{
    void *buf;

    BGM_PLAY(-1, 1);                       /* fade the music out          */
    WAIT_TICKS(1);
    FADE_OUT();                            /* darken the palette          */
    buf = RES_LOAD("FDOTHER.DAT", NULL, 79);
    memset((void *)(uintptr_t)VGA_BASE, 0, 320 * 200);
    RLE_BLIT(buf, 0, (void *)(uintptr_t)VGA_BASE, 320, -1);
    FADE_IN();
    WAIT_TICKS(9);
    RLE_BLIT(buf, 1, (void *)(uintptr_t)VGA_BASE, 320, -1);
    WAIT_TICKS(36);
    guest_free(buf);
}
