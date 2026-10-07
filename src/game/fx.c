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
 *   0x2B996  fx_dots7   funcs_30469[0]  7 particles
 *   0x2BB33  fx_dots8   funcs_30469[1]  8 particles
 *   0x2BD6C  fx_blob    funcs_30469[2]  no particle array
 *   0x2BF83  fx_advance helper             (called only by fx_blob)
 *   0x2BFD9  fx_dots12  funcs_30469[3]  12 particles
 *   0x2C217  fx_dots6   funcs_30469[4]  6 particles
 *   0x2C441  fx_dots6b  funcs_30469[5]  6 particles
 *   0x2CAFC  fx_dots3   funcs_30469[7]  3 particles
 *   0x2CCF4  fx_dots16  funcs_30469[8]  16 particles
 *   0x2CE1A  fx_toggle  funcs_30469[9]  two-frame flip
 *
 * The machine code of every handler ends by jumping into one of the shared
 * return epilogues of the tail of the sub_2C67D chunk (0x2C93B xor eax,eax /
 * 0x2C93D add esp,..) or into a sibling's epilogue (0x2BB2A, 0x2C439). Those
 * are not calls: the C simply `return`s and never jumps into machine code.
 *
 * fx_advance has its own five-argument cdecl ABI
 * (phase*, counter*, dst, pitch, buf); its second table lookup is relative to
 * `buf` (`*(int32_t *)(buf + 4*phase + 8)` is a sub-offset whose +6 byte is the
 * frame count), exactly as the machine code does.
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

#define dword_53F76 ((int32_t *)(uintptr_t)0x00053F76u)   /* dots7 phase (8 dwords) */
#define dword_53F92 ((int32_t *)(uintptr_t)0x00053F92u)   /* dots8 phase            */

#define byte_53FB2  (*(uint8_t  *)(uintptr_t)0x00053FB2u)      /* blob phase   */
#define byte_53FB3  (*(uint8_t  *)(uintptr_t)0x00053FB3u)      /* blob pad     */
#define byte_53FB4  (*(uint8_t  *)(uintptr_t)0x00053FB4u)      /* blob counter */
#define dword_53FB5 ((int32_t *)(uintptr_t)0x00053FB5u)   /* dots12 phase   */
#define dword_53FE5 ((int32_t *)(uintptr_t)0x00053FE5u)   /* dots12 slot    */
#define byte_54015  (*(uint8_t  *)(uintptr_t)0x00054015u)      /* dots12 rotate  */
#define byte_54016  (*(uint8_t  *)(uintptr_t)0x00054016u)      /* dots12 freeze  */
#define byte_54017  (*(uint8_t  *)(uintptr_t)0x00054017u)      /* dots12 parity  */

#define dword_54050 ((int32_t *)(uintptr_t)0x00054050u)   /* dots6b phase */
#define dword_54068 ((int32_t *)(uintptr_t)0x00054068u)   /* dots6b slot  */
#define byte_54080 ((uint8_t  *)(uintptr_t)0x00054080u)   /* dots6b dir   */
#define byte_54086  (*(uint8_t  *)(uintptr_t)0x00054086u)      /* dots6b rotate */
#define byte_54087  (*(uint8_t  *)(uintptr_t)0x00054087u)      /* dots6b freeze */

/* Read-only offset tables, copied from their original image addresses. */
#define FX_TABLE_DOTS7_DIR  0x000524EEu  /* 7 bytes                   */
#define FX_TABLE_DOTS7_POS  0x000524F5u  /* 7 dwords,  bias +148      */
#define FX_TABLE_DOTS7_ROW  0x00052511u  /* 7 dwords                  */
#define FX_TABLE_DOTS8_POS  0x0005252Du  /* 8 dwords,  bias +148      */
#define FX_TABLE_DOTS8_ROW  0x0005254Du  /* 8 dwords                  */
#define FX_TABLE_DOTS12_V19 0x0005256Du  /* 12 dwords, bias +20       */
#define FX_TABLE_DOTS12_V21 0x0005259Du  /* 12 bytes (3 dwords)       */
#define FX_TABLE_DOTS12_V20 0x000525A9u  /* 12 bytes (3 dwords)       */
#define FX_TABLE_DOTS6  0x000525B5u   /* 10 dwords, bias +143 */
#define FX_TABLE_DOTS6B 0x000525DDu   /* 10 dwords, bias +143 */
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

/* 0x2B996 - funcs_30469[0]. Seven particles driven by a 7-byte direction
 * table (0x524EE): selector 3 seeds eight phases (!), 4 blits the dir==1
 * particles and fires a sample at phase 3, 5 blits the dir==0 particles and
 * advances every particle (done at 9). Table bias +148. */
int fx_dots7(int rec, const void *buf, void *dst, int pitch, int selector)
{
    uint8_t dir[7];
    int32_t pos[7], row[7];
    int     r = 0;
    int     i;

    memcpy(dir, (const void *)(uintptr_t)FX_TABLE_DOTS7_DIR, sizeof dir);
    memcpy(pos, (const void *)(uintptr_t)FX_TABLE_DOTS7_POS, sizeof pos);
    memcpy(row, (const void *)(uintptr_t)FX_TABLE_DOTS7_ROW, sizeof row);
    if (fx_rec_flag(rec) == 0) {
        for (i = 0; i < 7; ++i)
            pos[i] += 148;
    }

    switch ((uint8_t)selector) {
    case 3:
        /* the machine writes eight entries into the seven-particle array - */
        for (i = 0; i < 8; ++i)
            dword_53F76[i] = -2 * i;
        return 28;

    case 4:
        for (i = 0; i < 7; ++i) {
            int32_t v = dword_53F76[i];

            if (v == 3)
                SVC_PLAY_SFX((const void *)(uintptr_t)dword_54153, 1, 1);
            if ((uint32_t)v < 0x10u && dir[i] == 1)
                RES_BLIT(buf, v,
                         (void *)((intptr_t)dst + pos[i] +
                                  (intptr_t)pitch * row[i]),
                         pitch, -1);
        }
        return 0;

    case 5:
        for (i = 0; i < 7; ++i) {
            int32_t v = dword_53F76[i];

            if ((uint32_t)v < 0x10u && dir[i] == 0)
                RES_BLIT(buf, v,
                         (void *)((intptr_t)dst + pos[i] +
                                  (intptr_t)pitch * row[i]),
                         pitch, -1);
            ++dword_53F76[i];
            if (dword_53F76[i] == 9)
                r = 1;
        }
        return r;

    default:
        return 0;
    }
}

/* 0x2BB33 - funcs_30469[1]. Eight particles; the two halves of selector 4/5
 * swap the role of the first and the last four (the last four use particle
 * (i+4)%8 for the destination and the phase+15 sub-image), and every drawn
 * pixel is biased by +80. Done at 9, sample at 5. Table bias +148. */
int fx_dots8(int rec, const void *buf, void *dst, int pitch, int selector)
{
    int32_t pos[8], row[8];
    int     r = 0;
    int     i;

    memcpy(pos, (const void *)(uintptr_t)FX_TABLE_DOTS8_POS, sizeof pos);
    memcpy(row, (const void *)(uintptr_t)FX_TABLE_DOTS8_ROW, sizeof row);
    if (fx_rec_flag(rec) == 0) {
        for (i = 0; i < 8; ++i)
            pos[i] += 148;
    }

    switch ((uint8_t)selector) {
    case 3:
        for (i = 0; i < 8; ++i)
            dword_53F92[i] = -2 * i;
        return 31;

    case 4:
        for (i = 0; i < 4; ++i) {
            int32_t v = dword_53F92[i];

            if ((uint32_t)v < 0xFu)
                RES_BLIT(buf, v,
                         (void *)((intptr_t)dst + pos[i] +
                                  (intptr_t)pitch * row[i] + 80),
                         pitch, -1);
        }
        for (; i < 8; ++i) {
            int32_t v = dword_53F92[i];
            int     j = (i + 4) % 8;

            if ((uint32_t)v < 0xFu)
                RES_BLIT(buf, v + 15,
                         (void *)((intptr_t)dst + pos[j] +
                                  (intptr_t)pitch * row[j] + 80),
                         pitch, -1);
        }
        return 0;

    case 5:
        for (i = 0; i < 4; ++i) {
            int32_t v = dword_53F92[i];
            int     j = (i + 4) % 8;

            if ((uint32_t)v < 0xFu)
                RES_BLIT(buf, v + 15,
                         (void *)((intptr_t)dst + pos[j] +
                                  (intptr_t)pitch * row[j] + 80),
                         pitch, -1);
        }
        for (; i < 8; ++i) {
            int32_t v = dword_53F92[i];

            if ((uint32_t)v < 0xFu)
                RES_BLIT(buf, v,
                         (void *)((intptr_t)dst + pos[i] +
                                  (intptr_t)pitch * row[i] + 80),
                         pitch, -1);
        }
        for (i = 0; i < 8; ++i) {
            ++dword_53F92[i];
            if (dword_53F92[i] == 9)
                r = 1;
            if (dword_53F92[i] == 5)
                SVC_PLAY_SFX((const void *)(uintptr_t)dword_54153, 1, 1);
        }
        return r;

    default:
        return 0;
    }
}

/* 0x2BF83 - blit the current frame of a sub-image strip and advance the
 * (phase, counter) pair. `buf` is both the resource base and the offset-table
 * base: the frame count of sub-image `phase` sits at
 *   *(uint8_t *)(buf + *(int32_t *)(buf + 4*phase + 8) + 6).
 * Returns that frame count. cdecl ABI, five stack arguments. */
int fx_advance(uint8_t *phase, uint8_t *counter, void *dst, int pitch,
               const void *buf)
{
    const uint8_t *b = (const uint8_t *)buf;
    int            ret;

    RES_BLIT(buf, (int)*phase, dst, pitch, -1);
    ret = *(const uint8_t *)(b + *(const int32_t *)(b + 4 * (int)*phase + 8) + 6);
    if ((uint8_t)(++*counter) == (uint8_t)ret) {
        *counter = 0;
        ++*phase;
    }
    return ret;
}

/* 0x2BD6C - funcs_30469[2]. No particle array: three bytes of state
 * (byte_53FB2 length/phase, byte_53FB3 pad, byte_53FB4 sub-image counter).
 * record[+6]==0 biases the two res_blit destinations by one pixel and gates
 * selectors 1/7 and 2/8 on fx_advance. */
int fx_blob(int rec, const void *buf, void *dst, int pitch, int selector)
{
    int v15 = (fx_rec_flag(rec) == 0);

    switch ((uint8_t)selector) {
    case 0:
        byte_53FB2 = 0;
        byte_53FB3 = 0;
        byte_53FB4 = 0;
        return 29;

    case 3:
        byte_53FB2 = 0x10;
        return 12;

    case 6:
        SVC_PLAY_SFX((const void *)(uintptr_t)dword_54153, 3, 1);
        byte_53FB2 = 10;
        return 10;

    case 1:
    case 7:
        if (v15 != 0)
            return 0;
        if (byte_53FB2 == 10 && (uint8_t)selector == 1)
            byte_53FB2 = 15;
        fx_advance(&byte_53FB2, &byte_53FB4, dst, pitch, buf);
        return 0;

    case 2:
    case 8:
        if (byte_53FB2 == 7)
            SVC_PLAY_SFX((const void *)(uintptr_t)dword_54153, 1, 1);
        if (v15 != 0) {
            if (byte_53FB2 == 10 && (uint8_t)selector == 2)
                byte_53FB2 = 15;
            fx_advance(&byte_53FB2, &byte_53FB4, dst, pitch, buf);
        }
        if (byte_53FB2 == 0x10)
            RES_BLIT(buf, 0x10, dst, pitch, -1);
        return 0;

    case 4:
        if (v15 == 0)
            RES_BLIT(buf, 15, (void *)((intptr_t)dst + 1 - pitch), pitch, -1);
        return 0;

    case 5:
        if (v15 != 0)
            RES_BLIT(buf, 15, (void *)((intptr_t)dst - 1 - pitch), pitch, -1);
        RES_BLIT(buf, (int)byte_53FB2, (void *)((intptr_t)dst + 1 - pitch),
                 pitch, -1);
        ++byte_53FB2;
        if (byte_53FB2 == 0x11) {
            SVC_PLAY_SFX((const void *)(uintptr_t)dword_54153, 2, 1);
            return 1;
        }
        if (byte_53FB2 == 0x12)
            byte_53FB2 = 0x10;
        return 0;

    default:
        return 0;
    }
}

/* 0x2BFD9 - funcs_30469[3]. Twelve particles over three rotating slots; the
 * slot picks a destination offset (v19), a sub-image base (v20) and a vertical
 * bias (v21). Parity byte_54017 gates the advance half (even frames only);
 * done at 3 only when the slot's vertical bias is 0. Record bias +20. */
int fx_dots12(int rec, const void *buf, void *dst, int pitch, int selector)
{
    int32_t v19[12];
    uint8_t v20[12], v21[12];
    int     r = 0;
    int     i;

    memcpy(v19, (const void *)(uintptr_t)FX_TABLE_DOTS12_V19, sizeof v19);
    memcpy(v21, (const void *)(uintptr_t)FX_TABLE_DOTS12_V21, sizeof v21);
    memcpy(v20, (const void *)(uintptr_t)FX_TABLE_DOTS12_V20, sizeof v20);
    if (fx_rec_flag(rec) == 0) {
        for (i = 0; i < 12; ++i)
            v19[i] += 20;
    }

    switch ((uint8_t)selector) {
    case 0:
        for (i = 0; i < 12; ++i) {
            dword_53FB5[i] = -2 * i;
            dword_53FE5[i] = i;
        }
        byte_54015 = 12;
        byte_54016 = 0;
        byte_54017 = 0;
        return 2;

    case 3:
        return 40;
    case 6:
        byte_54016 = 1;
        return 20;

    case 2:
    case 5:
    case 8:
        byte_54017 = (uint8_t)(((uint8_t)(byte_54017 + 1)) % 2);
        for (i = 0; i < 12; ++i) {
            int32_t v    = dword_53FB5[i];
            int     slot = dword_53FE5[i];

            if ((uint32_t)v < 0xBu)
                RES_BLIT(buf, v + v20[slot],
                         (void *)((intptr_t)dst + v19[slot] -
                                  (intptr_t)pitch * v21[slot]),
                         pitch, -1);
            if (byte_54017 == 0) {
                if (v == 0 && v20[slot] != 0)
                    SVC_PLAY_SFX((const void *)(uintptr_t)dword_54153, 2, 1);
                ++dword_53FB5[i];
                if (dword_53FB5[i] == 3) {
                    /* the machine always reports "done" here; the second
                     * sample only fires when the slot's vertical bias is 0 */
                    if (v20[slot] == 0)
                        SVC_PLAY_SFX2((const void *)(uintptr_t)dword_54153, 1, 1);
                    r = 1;
                }
                if (dword_53FB5[i] == 0xB && byte_54016 == 0) {
                    byte_54015 = (uint8_t)(((uint8_t)(byte_54015 + 1)) % 12);
                    dword_53FE5[i] = byte_54015;
                    dword_53FB5[i] = 0;
                }
            }
        }
        return r;

    default:
        return 0;
    }
}

/* 0x2C441 - funcs_30469[5]. Six particles with a per-particle direction base
 * (6*(rand%2)) and a ten-slot rotation. Same shape as fx_dots6 with a table
 * bias of +143; sample bank 1 at particle 0 and bank 2 at particle 3. */
int fx_dots6b(int rec, const void *buf, void *dst, int pitch, int selector)
{
    int32_t pos[10];
    int     r = 0;
    int     i;

    memcpy(pos, (const void *)(uintptr_t)FX_TABLE_DOTS6B, sizeof pos);
    if (fx_rec_flag(rec) == 0) {
        for (i = 0; i < 10; ++i)
            pos[i] += 143;
    }

    switch ((uint8_t)selector) {
    case 0:
        for (i = 0; i < 6; ++i) {
            dword_54050[i] = -2 * i;
            dword_54068[i] = i;
            byte_54080[i]  = (uint8_t)(6 * fx_rand_mod2());
        }
        byte_54086 = 6;
        byte_54087 = 0;
        return 1;

    case 3:
        return 12;
    case 6:
        byte_54087 = 1;
        return 8;

    case 2:
    case 5:
    case 8:
        for (i = 0; i < 6; ++i) {
            int32_t v    = dword_54050[i];
            int     slot = dword_54068[i];

            if ((uint32_t)v < 6u)
                RES_BLIT(buf, (int)((uint8_t)byte_54080[i] + v),
                         (void *)((intptr_t)dst + pos[slot]), pitch, -1);
            if (v == 0) {
                if (i == 0)
                    SVC_PLAY_SFX((const void *)(uintptr_t)dword_54153, 1, 1);
                else if (i == 3)
                    SVC_PLAY_SFX2((const void *)(uintptr_t)dword_54153, 1, 1);
            }
            ++dword_54050[i];
            if (dword_54050[i] == 2)
                r = 1;
            if (dword_54050[i] == 7 && byte_54087 == 0) {
                byte_54086   = (uint8_t)(((uint8_t)(byte_54086 + 1)) % 10);
                dword_54068[i] = byte_54086;
                dword_54050[i] = 0;
                byte_54080[i]  = (uint8_t)(6 * fx_rand_mod2());
            }
        }
        return r;

    default:
        return 0;
    }
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
