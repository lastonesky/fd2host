/* bgm.c - background music entry (source translation of 0x25977).
 *
 * The only "change track" entry point (32 call sites). It keeps the current
 * track number in byte_51A11, reloads FDMUS.DAT entry `track` when it changes,
 * and hands the buffer to AIL. The 2 s fade-in on every change is the game's
 * own doing (docs/rounds/06-audio-fade.md): 0 ms of silence, 127 over 2000 ms -
 * except tracks 16/17 (the victory fanfares), which go to full volume at once.
 *
 * Services are called through their **original addresses** on purpose:
 *   - in the running host those AIL entries are already patched to src/ail.c
 *     and 0x111BA to src/game/res.c, so this is the same code path the
 *     machine-code callers take;
 *   - src/bgmcheck.c hooks exactly these addresses to stubs, so the same C
 *     can be compared against the original 0x25977 call by call.
 * 0x3666C is DPMI "lock linear region" (it calls int 0x31 AX=0600 via
 * 0x365DA); in a flat host it is a no-op service, kept as a call for fidelity.
 *
 * Verified byte-behaviour by src/bgmcheck.c (event sequence + globals).
 */
#include "bgm.h"

#include <stdint.h>
#include <stddef.h>

/* --- the game's data segment (IDA names kept) --------------------------- */
#define byte_51A11  (*(uint8_t  *)(uintptr_t)0x00051A11u) /* current track  */
#define byte_53EF0  (*(uint8_t  *)(uintptr_t)0x00053EF0u) /* music enabled  */
#define byte_51E61  (*(uint8_t  *)(uintptr_t)0x00051E61u) /* fade-in on/off */
#define dword_53ED0 (*(void   **)(uintptr_t)0x00053ED0u) /* AIL seq handle */
#define dword_53EE0 (*(void   **)(uintptr_t)0x00053EE0u) /* FDMUS.DAT buf  */
#define dword_53BFF (*(uint32_t *)(uintptr_t)0x00053BFFu) /* res size      */

/* --- service entry points ---------------------------------------------- */
typedef void  *(*res_load_fn)(const char *name, void *old_buffer, int index);
typedef void   (*lock_fn)(void *buf, uint32_t len);
typedef void  *(*ail_init_seq_fn)(void *h, void *start, int32_t seq_num);
typedef void   (*ail_seq_fn)(void *h);
typedef void   (*ail_vol_fn)(void *h, int32_t volume, int32_t ms);
typedef int32_t (*ail_loop_fn)(void *h, int32_t count);

#define RES_LOAD     ((res_load_fn)  (uintptr_t)0x000111BAu)
#define RES_LOCK     ((lock_fn)      (uintptr_t)0x0003666Cu)
#define AIL_INIT     ((ail_init_seq_fn)(uintptr_t)0x0003ADF5u)
#define AIL_START    ((ail_seq_fn)   (uintptr_t)0x0003AEEEu)
#define AIL_STOP     ((ail_seq_fn)   (uintptr_t)0x0003AF5Bu)
#define AIL_VOLUME   ((ail_vol_fn)   (uintptr_t)0x0003B124u)
#define AIL_LOOP     ((ail_loop_fn)  (uintptr_t)0x0003B1A6u)

void bgm_play(int track, int loop_count)
{
    /* The original zero-extends the stored byte and compares it against the
     * full 32-bit argument (`movzx eax,byte_51A11; cmp eax,arg_0`), so
     * track==-1 is *never* "already playing" even when the byte is 0xFF. */
    if ((unsigned)byte_51A11 == (unsigned)track)
        return;                                  /* already playing it      */
    byte_51A11 = (uint8_t)track;

    if (track == -1) {
        AIL_VOLUME(dword_53ED0, 0, 4000);        /* fade the current one out */
        return;
    }
    if (!byte_53EF0)
        return;                                  /* music switched off       */

    if (dword_53EE0)
        AIL_STOP(dword_53ED0);

    dword_53EE0 = RES_LOAD("FDMUS.DAT", dword_53EE0, track);
    RES_LOCK(dword_53EE0, dword_53BFF);          /* res_load set 53BFF       */

    AIL_INIT(dword_53ED0, dword_53EE0, 0);
    AIL_START(dword_53ED0);

    if (byte_51E61) {
        if (track == 16 || track == 17) {
            AIL_VOLUME(dword_53ED0, 127, 0);     /* fanfare: full volume now */
        } else {
            AIL_VOLUME(dword_53ED0, 0, 0);       /* silence, then            */
            AIL_VOLUME(dword_53ED0, 127, 2000);  /* ramp up over 2 s         */
        }
    } else {
        AIL_VOLUME(dword_53ED0, 0, 0);
    }
    AIL_LOOP(dword_53ED0, loop_count);
}
