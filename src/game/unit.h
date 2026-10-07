/* unit.h - FD2 persistent party roster (source translation).
 *
 * A second table of 80-byte records lives in the game's data segment, the
 * "party" mirror of the character table:
 *
 *   dword_53BF7  table base, 0x50-byte records
 *   dword_53BFB  record count (also the next append index)
 *   dword_53BEB  character-table count (see game/rec.h)
 *   dword_53A45  character table base
 *
 * These three functions are the only writers of that table:
 *
 *   0x1145A  unit_recalc      sum the eight item slots' bonuses into +48..+4E
 *   0x11506  unit_refresh_all copy every matching character record over the
 *                             party record (identity byte +8), then recalc
 *   0x112A5  unit_add         build a new party record from the default and
 *                             growth tables and append it
 *   0x33499  unit_exists      report whether a record with identity byte +8
 *                             equal to id exists
 *
 * App-level: the C reads and writes the same data-segment globals the machine
 * code does, so src/repl.c hooks them with no glue.
 *
 * Verified against the machine code by src/reccheck.c.
 */
#ifndef GAME_UNIT_H
#define GAME_UNIT_H

/* 0x1145A - recompute record `index`'s derived stats; returns the +4E value
 * (the accumulated signed 32-bit sum, not the truncated 16-bit field). */
int unit_recalc(int index);

/* 0x11506 - sync the party table from the character table. Void. */
void unit_refresh_all(void);

/* 0x112A5 - append a freshly built record for `id`, returns unit_recalc's
 * value for the appended index. */
int unit_add(int id);

/* 0x33499 - 1 if any party record's identity byte (+8) equals `id`, else 0.
 * Signed count, zero-extended byte compare (id is never truncated). */
int unit_exists(int id);

#endif /* GAME_UNIT_H */
