/* fx.c - "effect animation" handlers of the funcs_30469[] dispatch table.
 *
 * See fx.h for the family, the ABI and the coverage. Each function below is a
 * direct translation of the machine code at its stated address. Everything
 * that persists between frames lives in the original game data segment and is
 * accessed through its original address (macros), so the C reads and writes
 * exactly the words the rest of the game sees. The read-only offset tables
 * (0x525B5 / 0x5261E / 0x52646) are copied out of the original image, never
 * hardcoded.
 *
 * The services are called through their original addresses on purpose: in the
 * host src/repl.c has already patched res_blit / svc_play_sfx(2) to C, and
 * src/fxcheck.c hooks them (plus util_rand) to recording stubs - one world for
 * both sides of the differential test.
 *
 * Original addresses:
 *   0x2C217  fx_dots6   funcs_30469[4]  6 particles
 *   0x2CAFC  fx_dots3   funcs_30469[7]  3 particles
 *   0x2CCF4  fx_dots16  funcs_30469[8]  16 particles
 *   0x2CE1A  fx_toggle  funcs_30469[9]  two-frame flip
 */
#include "fx.h"

#include <stdint.h>
#include <string.h>

/* --- services (original entry points) ---------------------------------- */
typedef int      (*res_blit_fn)(const void *buf, int index, void *dst,
                                int pitch, int mode);
typedef int      (*sfx_fn)(const void *bank, int index, int loops);
typedef uint32_t (*rand_fn)(void);

#define RES_BLIT      ((res_blit_fn)(uintptr_t)0x0002EB9Fu)
#define SVC_PLAY_SFX  ((sfx_fn)     (uintptr_t)0x00025A96u)
#define SVC_PLAY_SFX2 ((sfx_fn)     (uintptr_t)0x00025B45u)
#define UTIL_RAND     ((rand_fn)    (uintptr_t)0x0004EBE3u)

/* --- game globals (obj1 data segment) ---------------------------------- */
#define dword_53A45 (*(uint32_t *)(uintptr_t)0x00053A45u)  /* record table 1 */
#define dword_54153 (*(uint32_t *)(uintptr_t)0x00054153u)  /* SFX bank (main) */

/* The particle arrays are plain pointers because the original addresses them
 * with an index that can exceed the game-visible count: dots3 case 0 writes
 * four entries into three-particle arrays (dword_540CB[3] at 0x540D7 and
 * dword_540DB[3] at 0x540E7 - each array is laid out with four dwords of
 * room). Using pointers keeps that well-defined here too. */
#define dword_54018 ((int32_t *)(uintptr_t)0x00054018u)   /* dots6 phase */
#define dword_54030 ((int32_t *)(uintptr_t)0x00054030u)   /* dots6 slot  */
#define byte_54048  ((uint8_t  *)(uintptr_t)0x00054048u)  /* dots6 dir   */
#define byte_5404E  (*(uint8_t  *)(uintptr_t)0x0005404Eu) /* dots6 slot  */
#define byte_5404F  (*(uint8_t  *)(uintptr_t)0x0005404Fu) /* dots6 done  */

#define dword_540CB ((int32_t *)(uintptr_t)0x000540CBu)   /* dots3 phase */
#define dword_540DB ((int32_t *)(uintptr_t)0x000540DBu)   /* dots3 slot  */
#define byte_540EB  (*(uint8_t  *)(uintptr_t)0x000540EBu) /* dots3 slot  */
#define byte_540EC  (*(uint8_t  *)(uintptr_t)0x000540ECu) /* dots3 done  */
#define byte_540ED  (*(uint8_t  *)(uintptr_t)0x000540EDu) /* dots3 phase */

#define dword_540EE ((int32_t *)(uintptr_t)0x000540EEu)   /* dots16 phase */

#define byte_5412E  (*(uint8_t  *)(uintptr_t)0x0005412Eu)      /* toggle phase */
#define byte_5412F  (*(uint8_t  *)(uintptr_t)0x0005412Fu)      /* toggle flip  */

/* Read-only offset tables, copied from their original image addresses. */
#define FX_TABLE_DOTS6  0x000525B5u   /* 10 dwords, bias +143 */
#define FX_TABLE_DOTS3  0x0005261Eu   /* 10 dwords, bias +130 */
#define FX_TABLE_DOTS16 0x00052646u   /* 16 bytes                  */

/* Byte 6 of character record `rec` (80-byte records; 0 biases the table). */
static uint8_t fx_rec_flag(int rec)
{
    return *(const uint8_t *)(uintptr_t)
           (dword_53A45 + 80u * (uint32_t)rec + 6u);
}

/* The game's random generator is signed-idiv'd by 2 in the machine code; keep
 * the signed remainder so the translation matches for any 32-bit input. */
static int fx_rand_mod2(void)
{
    return (int)((int32_t)UTIL_RAND() % 2);
}

/* 0x2C217 - funcs_30469[4]. selector 0 seeds six particles, 2/5/8 advance
 * them (blit while 0 <= phase < 7, sample at phase 0, "done" once a particle
 * passes 3 and its slot rotates every 10 frames), 3/6 are timing delays. */
int fx_dots6(int rec, const void *buf, void *dst, int pitch, int selector)
{
    int32_t pos[10];
    int     r = 0;
    int     i;

    memcpy(pos, (const void *)(uintptr_t)FX_TABLE_DOTS6, sizeof pos);
    if (fx_rec_flag(rec) == 0) {
        for (i = 0; i < 10; ++i)
            pos[i] += 143;
    }

    switch ((uint8_t)selector) {
    case 0:
        for (i = 0; i < 6; ++i) {
            dword_54018[i] = -2 * i;
            dword_54030[i] = i;
            byte_54048[i]  = (uint8_t)(7 * fx_rand_mod2());
        }
        byte_5404E = (uint8_t)i;      /* 6 */
        byte_5404F = 0;
        return 2;

    case 2:
    case 5:
    case 8:
        for (i = 0; i < 6; ++i) {
            int32_t v = dword_54018[i];

            if ((uint32_t)v < 7u)
                RES_BLIT(buf, (int)((uint8_t)byte_54048[i] + v),
                         (void *)((intptr_t)dst + pos[dword_54030[i]]),
                         pitch, -1);
            if (v == 0)
                SVC_PLAY_SFX((const void *)(uintptr_t)dword_54153, 1, 1);
            ++dword_54018[i];
            if (dword_54018[i] == 3)
                r = 1;
            if (dword_54018[i] == 8 && byte_5404F == 0) {
                byte_5404E = (uint8_t)(((uint8_t)(byte_5404E + 1)) % 10);
                dword_54030[i] = byte_5404E;
                dword_54018[i] = 0;
                byte_54048[i]  = (uint8_t)(7 * fx_rand_mod2());
            }
        }
        return r;

    case 3:
        return 12;
    case 6:
        byte_5404F = 1;
        return 8;
    default:
        return 0;
    }
}

/* 0x2CAFC - funcs_30469[7]. Same shape as fx_dots6 with three particles and
 * a phase gate (byte_540ED): the advance/sample half runs only on even
 * phases, the blit runs every phase. */
int fx_dots3(int rec, const void *buf, void *dst, int pitch, int selector)
{
    int32_t pos[10];
    int     r = 0;
    int     k;

    memcpy(pos, (const void *)(uintptr_t)FX_TABLE_DOTS3, sizeof pos);
    if (fx_rec_flag(rec) == 0) {
        for (k = 0; k < 10; ++k)
            pos[k] += 130;
    }

    switch ((uint8_t)selector) {
    case 0:
        for (k = 0; k < 4; ++k) {
            dword_540CB[k] = -3 * k;
            dword_540DB[k] = k;
        }
        byte_540EB = (uint8_t)k;      /* 4 */
        byte_540EC = 0;
        byte_540ED = 0;
        return 2;

    case 2:
    case 5:
    case 8:
        byte_540ED = (uint8_t)(((uint8_t)(byte_540ED + 1)) % 2);
        for (k = 0; k < 3; ++k) {
            int32_t v = dword_540CB[k];

            if ((uint32_t)v < 5u)
                RES_BLIT(buf, (int)v,
                         (void *)((intptr_t)dst + pos[dword_540DB[k]]),
                         pitch, -1);
            if (byte_540ED == 0) {
                /* particle 0 uses the first handle, particle 1 the second */
                if (v == 1) {
                    if (k == 0)
                        SVC_PLAY_SFX((const void *)(uintptr_t)dword_54153, 1, 1);
                    else if (k == 1)
                        SVC_PLAY_SFX2((const void *)(uintptr_t)dword_54153, 1, 1);
                }
                if (++dword_540CB[k] == 2)
                    r = 1;
                if (dword_540CB[k] == 7 && byte_540EC == 0) {
                    byte_540EB = (uint8_t)(((uint8_t)(byte_540EB + 1)) % 10);
                    dword_540DB[k] = byte_540EB;
                    dword_540CB[k] = 0;
                }
            }
        }
        return r;

    case 3:
        return 32;
    case 6:
        byte_540EC = 1;
        return 16;
    default:
        return 0;
    }
}

/* 0x2CCF4 - funcs_30469[8]. Sixteen particles over a 16-byte direction table;
 * blit while (unsigned)phase < 8, sample at 0 and 4 ("done" at 4). */
int fx_dots16(int rec, const void *buf, void *dst, int pitch, int selector)
{
    uint8_t tbl[16];
    int     r = 0;
    int     j;

    (void)rec;
    memcpy(tbl, (const void *)(uintptr_t)FX_TABLE_DOTS16, sizeof tbl);

    switch ((uint8_t)selector) {
    case 0:
        for (j = 0; j < 16; ++j)
            dword_540EE[j] = -2 * j;
        return 3;

    case 2:
    case 5:
        for (j = 0; j < 16; ++j) {
            int32_t v = dword_540EE[j];

            if ((uint32_t)v < 8u)
                RES_BLIT(buf, tbl[j] + v, dst, pitch, -1);
            if (v == 0)
                SVC_PLAY_SFX((const void *)(uintptr_t)dword_54153, 1, 1);
            if (v == 4)
                SVC_PLAY_SFX2((const void *)(uintptr_t)dword_54153, 2, 1);
            if (++dword_540EE[j] == 4)
                r = 1;
        }
        return r;

    case 3:
        return 34;
    case 6:
        return 2;
    default:
        return 0;
    }
}

/* 0x2CE1A - funcs_30469[9]. A single sprite that alternates between two
 * frames; selector 5 walks the sub-image index up every two frames and fires
 * a sample at the two boundaries. The return value is "still animating". */
int fx_toggle(int rec, const void *buf, void *dst, int pitch, int selector)
{
    (void)rec;

    switch ((uint8_t)selector) {
    case 0:
        byte_5412F = 0;
        byte_5412E = 1;
        return 20;

    case 1:
    case 7:
        if (byte_5412F == 0)
            RES_BLIT(buf, 0, dst, pitch, -1);
        byte_5412F ^= 1;
        return 0;

    case 3:
        return 60;

    case 4:
        /* machine code pushes index 0 here, not 4 (Hex-Rays' 9-arg view hides it) */
        RES_BLIT(buf, 0, dst, pitch, -1);
        return 0;

    case 5: {
        uint8_t k = byte_5412E;

        RES_BLIT(buf, (int)(k >> 1), dst, pitch, -1);
        if (byte_5412E == 6)
            SVC_PLAY_SFX((const void *)(uintptr_t)dword_54153, 1, 1);
        else if (byte_5412E == 36)
            SVC_PLAY_SFX2((const void *)(uintptr_t)dword_54153, 2, 1);
        ++byte_5412E;
        return (byte_5412E > 0x10u && byte_5412E < 0x2Cu) ? 1 : 0;
    }

    case 6:
        return 20;
    default:
        return 0;
    }
}
