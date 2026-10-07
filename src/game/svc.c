/* svc.c - FD2 platform service wrappers (source translation).
 *
 *   0x17AA9  svc_wait_ticks   wait until the BIOS tick has advanced by N
 *   0x25A96  svc_play_sfx     start a PCM sound effect
 *   0x25B45  svc_play_sfx2    same, on the second sample handle
 *
 * See game/svc.h for the contract. The originals are 68 and 116 bytes; each
 * opens with the `push <frame size>; call sub_3702F` stack probe, which is a
 * no-op for the caller (0x3702F restores EAX from the argument slot and pops
 * the size before returning), so it has no counterpart in the C.
 *
 * Everything else stays exactly where the machine code left it: the globals
 * live in the game's data segment and the helper services are still reached at
 * their original addresses, which in the running host are the ones
 * src/ail.c has already replaced with the WinMM implementation:
 *
 *   0x4E310  read the BIOS tick word (BDA 0x46C - the C cannot reach the BDA
 *            itself: Windows keeps page 0 unmappable, and dos.c maps the
 *            64 KiB mirror at 0x70000 that this reads)
 *   0x39521  AIL_init_sample
 *   0x39694  AIL_set_sample_address
 *   0x39AAE  AIL_set_sample_loop_count
 *   0x39798  AIL_start_sample
 *   0x39805  AIL_stop_sample
 *
 * Verified against the machine code by src/typecheck.c: the whole tick sequence
 * (every reading, because the harness hands both sides the same clock), the
 * full AIL call log with arguments, every global and the VGA frame.
 */
#include "svc.h"
#include "../platform.h"    /* PLAT_CDECL (no __cdecl keyword outside MSVC) */

/* --- the game's own data segment (IDA names kept) ---------------------- */
#define dword_53A2C (*(int32_t *)(uintptr_t)0x00053A2Cu) /* last tick reading  */
#define byte_53EF1  (*(uint8_t  *)(uintptr_t)0x00053EF1u) /* digital driver up  */
#define byte_51E62  (*(uint8_t  *)(uintptr_t)0x00051E62u) /* SFX enabled option */
#define dword_54133 (*(int32_t *)(uintptr_t)0x00054133u) /* sample busy gate   */
#define dword_53EE4 (*(void   **)(uintptr_t)0x00053EE4u) /* SFX sample handle  */
#define dword_53EE8 (*(void   **)(uintptr_t)0x00053EE8u) /* 2nd sample handle  */

typedef uint32_t (PLAT_CDECL *tick_fn)(void);
typedef int32_t  (PLAT_CDECL *ail_h_fn)(void *handle);
typedef int32_t  (PLAT_CDECL *ail_addr_fn)(void *handle, const void *start,
                                        uint32_t len);
typedef int32_t  (PLAT_CDECL *ail_loop_fn)(void *handle, int32_t loops);

#define ORIG_TICK   ((tick_fn)    (uintptr_t)0x0004E310u)
#define ORIG_INIT   ((ail_h_fn)   (uintptr_t)0x00039521u)
#define ORIG_ADDR   ((ail_addr_fn)(uintptr_t)0x00039694u)
#define ORIG_LOOP   ((ail_loop_fn)(uintptr_t)0x00039AAEu)
#define ORIG_START  ((ail_h_fn)   (uintptr_t)0x00039798u)
#define ORIG_STOP   ((ail_h_fn)   (uintptr_t)0x00039805u)

/* 0x17AA9
 *
 *   dword_53A2C = (int16)tick;
 *   do {
 *       elapsed = (int16)tick - dword_53A2C;
 *       if (elapsed < 0) elapsed += 0x10000;
 *   } while (elapsed < n);
 *   dword_53A2C = (int16)tick;
 *
 * The sign extension is what makes the wrap-around work: consecutive readings
 * are at most 0x8000 apart in either direction, so the correction always lands
 * in 0..0xFFFF and `elapsed` never comes out negative twice. It is also why
 * the C must cast the reading to int16_t before subtracting - 0x4E310 hands
 * it back zero-extended.
 */
int svc_wait_ticks(int n)
{
    int32_t elapsed;

    dword_53A2C = (int16_t)ORIG_TICK();
    do {
        elapsed = (int16_t)ORIG_TICK() - dword_53A2C;
        if (elapsed < 0)
            elapsed += 0x10000;
    } while (elapsed < n);

    dword_53A2C = (int16_t)ORIG_TICK();
    return dword_53A2C;
}

/* 0x25A96 and its twin 0x25B45
 *
 *   if (!byte_53EF1 || !byte_51E62 || dword_54133)      nothing may play
 *       return;
 *   AIL_stop_sample(handle);
 *   if (index == -1)
 *       return;
 *   row   = bank + 4*index;                  entry i: off[i] at row+6,
 *   start = bank + *(u32)(row + 6);                     off[i+1] at row+0xA
 *   len   = *(u32)(row + 0xA) - *(u32)(row + 6);
 *   AIL_init_sample(handle);
 *   AIL_set_sample_address(handle, start, len);
 *   AIL_set_sample_loop_count(handle, loops);
 *   AIL_start_sample(handle);
 *
 * The two entry points are the same 175 bytes with 17 bytes different: the
 * rel32 of the stack probe and of the five AIL calls (they sit at different
 * addresses) plus the five `push handle` immediates - 0x25A96 uses
 * dword_53EE4, 0x25B45 uses dword_53EE8 (both allocated back to back at
 * 0x25C43/0x25C57). The machine code re-reads the handle global for every AIL
 * call, so the shared helper takes the *slot* and dereferences it each time.
 *
 * The return value is the result of the last AIL call (stop, when only the
 * stop happened); the branch that returns before calling anything leaves EAX
 * holding whatever was live there, which no caller reads either.
 */
static int play_sfx(void **slot, const void *bank, int index, int loops)
{
    const uint8_t *row;
    const uint8_t *start;
    uint32_t       beg, end;
    int32_t        result;

    if (byte_53EF1 == 0 || byte_51E62 == 0 || dword_54133 != 0)
        return 0;

    result = ORIG_STOP(*slot);
    if (index == -1)
        return result;

    row = (const uint8_t *)bank + 4 * index;
    beg = *(const uint32_t *)(row + 6);
    end = *(const uint32_t *)(row + 0x0A);
    start = (const uint8_t *)bank + beg;

    ORIG_INIT(*slot);
    ORIG_ADDR(*slot, start, end - beg);
    ORIG_LOOP(*slot, loops);
    return ORIG_START(*slot);
}

int svc_play_sfx(const void *bank, int index, int loops)
{
    return play_sfx(&dword_53EE4, bank, index, loops);
}

/* 0x25B45 - the same routine on the second sample handle; 11 call sites,
 * mostly the same kind of trigger as svc_play_sfx's. */
int svc_play_sfx2(const void *bank, int index, int loops)
{
    return play_sfx(&dword_53EE8, bank, index, loops);
}
