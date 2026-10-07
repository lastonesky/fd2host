/* unit.c - FD2 persistent party roster (source translation).
 *
 *   0x1145A  unit_recalc      - sum the eight item slots' bonuses into +48..+4E
 *   0x11506  unit_refresh_all - copy matching character records over the party
 *   0x112A5  unit_add         - build a record from the default/growth tables
 *
 * App-level: the C reads and writes the same data-segment globals the machine
 * code does (unit.h), so src/repl.c uses these functions directly.
 *
 * The record address is `dword_53BF7 + 80 * index` with the 80 computed the
 * way the original does - `(index*5) << 4` in a 32-bit register - so a
 * negative index wraps exactly as the machine code does. Slot `i` occupies
 * bytes +10+2i (state) and +11+2i (item id); the bonus tables are the game's
 * own (tbl_602AD item bonuses, tbl_61DA1 character defaults, tbl_620A1 growth
 * curves), read through game/tables.c - the same memory the machine code reads.
 *
 * ABI note: all three have a Watcom stack probe (`push N; call 0x3702F`) that
 * makes Hex-Rays report __fastcall/register parameters. The real ABI is plain
 * cdecl: unit_recalc/unit_add take one stack argument, unit_refresh_all none.
 *
 * Verified against the machine code by src/reccheck.c.
 */
#include <string.h>

#include "unit.h"
#include "rec.h"
#include "tables.h"

/* IDA names kept so the C reads like the original decompilation.
 * Addresses verified against E:\FD2\FD2.EXE.i64. */
#define dword_53A45 (*(uint32_t *)(uintptr_t)0x00053A45u) /* character table */
#define dword_53BEB (*(int32_t  *)(uintptr_t)0x00053BEBu) /* character count */
#define dword_53BF7 (*(uint32_t *)(uintptr_t)0x00053BF7u) /* party table */
#define dword_53BFB (*(int32_t  *)(uintptr_t)0x00053BFBu) /* party count */

/* The three data tables this module reads, with their fixed FD2 bases
 * (src/repl.c installs the same arithmetic for the machine-code callers). */
static void *tbl_602AD(int i) { return tbl_ptr((void *)(uintptr_t)0x602ADu, 23, i, 0); }
static void *tbl_61DA1(int i) { return tbl_ptr((void *)(uintptr_t)0x61DA1u, 24, i, 0); }
static void *tbl_620A1(int i) { return tbl_ptr((void *)(uintptr_t)0x620A1u, 11, i, 0); }

static uint8_t *party_rec(int index)
{
    return (uint8_t *)((uintptr_t)dword_53BF7 + 80u * (uint32_t)index);
}

/* 0x1145A - start from the record's base AP/DP/DX (+37/+39/+3E, signed
 * 16-bit), add every active slot's item bonus (tbl_602AD[slot[1]] +1/+5/+3/+7)
 * and store the four sums back at +48/+4A/+4C/+4E. The accumulators are
 * 32-bit and only truncated on the store; the returned +4E value is the full
 * 32-bit sum. Slot activity is `state & 0x40` (not bit 7). */
int unit_recalc(int index)
{
    uint8_t *rec = party_rec(index);
    int32_t ap = *(const int16_t *)(rec + 0x37);
    int32_t dp = *(const int16_t *)(rec + 0x39);
    int32_t dx = *(const int16_t *)(rec + 0x3E);
    int32_t de = dx;
    int i;

    for (i = 0; i < 8; i++) {
        const uint8_t *slot = rec + 2u * (uint32_t)i + 10u;
        if ((slot[0] & 0x40) != 0) {
            const uint8_t *it = (const uint8_t *)tbl_602AD(slot[1]);
            ap += *(const int16_t *)(it + 1);
            dp += *(const int16_t *)(it + 5);
            dx += *(const int16_t *)(it + 3);
            de += *(const int16_t *)(it + 7);
        }
    }

    *(uint16_t *)(rec + 0x48) = (uint16_t)ap;
    *(uint16_t *)(rec + 0x4A) = (uint16_t)dp;
    *(uint16_t *)(rec + 0x4C) = (uint16_t)dx;
    *(uint16_t *)(rec + 0x4E) = (uint16_t)de;
    return de;
}

/* 0x11506 - outer loop over the character table, inner over the party table.
 * A party record is overwritten when its identity byte (+8) matches the
 * character's and either the id is non-zero or the character's flag bit 0 is
 * 0 (rec_flag(i) is only called in the latter case - the original short
 * circuits). After the copy it clears the six transient bytes at +22, keeps
 * only bit 0 of +5, syncs +40<-+42 unless that bit is set and +44<-+46, then
 * recalculates. The loop order decides which character wins when several
 * match, so it is preserved. */
void unit_refresh_all(void)
{
    int i, j;

    for (i = 0; i < dword_53BEB; i++) {
        uint8_t *ch = (uint8_t *)((uintptr_t)dword_53A45
                                  + 80u * (uint32_t)i);
        for (j = 0; j < dword_53BFB; j++) {
            uint8_t *un = party_rec(j);
            if (ch[8] == un[8] && (un[8] != 0 || rec_flag(i) == 0)) {
                memmove(un, ch, 0x50);
                memset(un + 0x22, 0, 6);
                un[5] &= 1u;
                if (un[5] != 1)
                    *(uint16_t *)(un + 0x40) = *(uint16_t *)(un + 0x42);
                *(uint16_t *)(un + 0x44) = *(uint16_t *)(un + 0x46);
                unit_recalc(j);
            }
        }
    }
}

/* 0x112A5 - build one record for character `id` and append it.
 *
 * D = tbl_61DA1(id)  (24-byte defaults), G = tbl_620A1(id) (11-byte growth),
 * L = D[2] (level, unsigned byte). The HP/MP formulas use `(L-1)` in 32-bit
 * arithmetic (so L == 0 gives a negative growth) and only the low 16 bits are
 * stored. The two trailing slot values (+17/+19) and every byte not listed
 * below are left as the target slot's residue - the original writes only the
 * fields below. */
int unit_add(int id)
{
    int idx = dword_53BFB;
    uint8_t *un = party_rec(idx);
    const uint8_t *D = (const uint8_t *)tbl_61DA1(id);
    const uint8_t *G = (const uint8_t *)tbl_620A1(id);
    int32_t L = D[2];
    int32_t hp = (int32_t)*(const uint16_t *)(D + 3) + (int32_t)G[6] * (L - 1);
    int32_t mp = (int32_t)*(const uint16_t *)(D + 5) + (int32_t)G[8] * (L - 1);
    int i;

    un[5]  = 0;
    un[6]  = 2;
    un[7]  = (uint8_t)id;
    un[8]  = (uint8_t)id;
    un[9]  = 0;
    un[10] = 0x40;
    un[11] = D[0x0C];
    un[12] = 0x40;
    un[13] = D[0x0D];

    for (i = 0; i < 4; i++) {
        uint8_t *slot = un + 0x0E + 2u * (uint32_t)i;
        slot[0] = (D[0x0E + i] == 0xFF) ? 0x80 : 0x00;
        slot[1] = D[0x0E + i];
    }

    un[0x16] = 0x80;
    un[0x18] = 0x80;
    memmove(un + 0x1A, D + 8, 4);
    un[0x1E] = 0;
    un[0x1F] = D[0];
    un[0x20] = D[1];
    un[0x21] = (uint8_t)L;
    memset(un + 0x22, 0, 6);
    un[0x31] = 0xFF;

    *(uint16_t *)(un + 0x37) = (uint16_t)(*(const uint16_t *)(D + 0x12)
                                          + (uint32_t)G[0] * (uint32_t)L);
    *(uint16_t *)(un + 0x39) = (uint16_t)(*(const uint16_t *)(D + 0x14)
                                          + (uint32_t)G[2] * (uint32_t)L);
    un[0x3B] = D[7];
    un[0x3C] = 0;
    *(uint16_t *)(un + 0x3E) = (uint16_t)(*(const uint16_t *)(D + 0x16)
                                          + (uint32_t)G[4] * (uint32_t)L);
    *(uint16_t *)(un + 0x40) = (uint16_t)hp;
    *(uint16_t *)(un + 0x42) = (uint16_t)hp;
    *(uint16_t *)(un + 0x44) = (uint16_t)mp;
    *(uint16_t *)(un + 0x46) = (uint16_t)mp;

    {
        int result = unit_recalc(idx);
        dword_53BFB = idx + 1;
        return result;
    }
}
