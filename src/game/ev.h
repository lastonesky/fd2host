/* ev.h - FD2 scene-event handlers of funcs_1199C (source translation).
 *
 * The eleven table entries below are the dependency-closed half of the
 * `funcs_1199C` dispatch table at 0x51B91 (the other 34 entries need the
 * ~13 KB scene-render core). They are plain integer event sequences: read the
 * record table, set a one-shot status flag, run a VM sub-stream - no heap,
 * no files, no FP, no direct VGA access. Every service they call is already
 * translated (vm_run / rec_status_set / rec_flag / rec_slot_free /
 * rec_slot_claim / rec_status_mask_records).
 *
 *   0x34738  funcs_1199C[4]   ev_rec13_set1
 *   0x348EA  funcs_1199C[12]  ev_status24_27
 *   0x34A6C  funcs_1199C[19]  ev_status7_36
 *   0x34B2F  funcs_1199C[21]  ev_flag8_gate
 *   0x34CF1  funcs_1199C[26]  ev_rec6_gate
 *   0x34D92  funcs_1199C[29]  ev_mask_records
 *   0x34F74  funcs_1199C[33]  ev_clear_status12_13
 *   0x35123  funcs_1199C[8]   ev_slot8_claim
 *   0x35191  funcs_1199C[10]  ev_status16_71
 *   0x351E6  funcs_1199C[13]  ev_clear64_73
 *   0x35258  funcs_1199C[18]  ev_status16_34
 *
 * Real ABI (re-verified against E:\FD2\FD2.EXE.i64): cdecl stack functions.
 * Hex-Rays prints them as `__usercall ...@<eax/edx/...>` only because of the
 * Watcom `push <frame>; call 0x3702F` stack probe (PITFALLS §8-75); 0x3702F
 * ends in `retn 4` and preserves EAX, so the real arguments are the stack
 * slots [esp+4..]. The dispatcher pushes one argument and the caller cleans it
 * (`add esp,4`); only 0x34CF1 and 0x35123 read it.
 *
 * Return values (verified instruction by instruction):
 *   - 0x34738 / 0x34A6C / 0x34B2F / 0x35123: EAX is a called service's result
 *     or the caller's own EAX on the early-out - no game value, and every one
 *     of the nine call sites discards EAX (they all do `add esp,4` and move
 *     on), so these are `void`.
 *   - 0x348EA / 0x34CF1 / 0x35191: the one-shot flag byte, or dword_53AD5 when
 *     the flag was clear and the body ran.
 *   - 0x34D92 / 0x351E6 / 0x35258: forward the callee's EAX.
 *   - 0x34F74: dword_53A45 + 1040 (record 13).
 *
 * Verified against the machine code by src/evcheck.c.
 */
#ifndef GAME_EV_H
#define GAME_EV_H

#include <stdint.h>

/* 0x34738 - record 13 byte +6 = 1, then run sub-stream 7. */
void ev_rec13_set1(void);

/* 0x348EA - one shot: status(24,27,7) + sub-stream 3, set the flag. */
uint32_t ev_status24_27(void);

/* 0x34A6C - status(7,36,7) + sub-stream 8; if any of records 7..36 has flag
 * bit 0 clear, run sub-stream 11 as well. */
void ev_status7_36(void);

/* 0x34B2F - when record 8's flag bit 0 is clear, run sub-stream 2. */
void ev_flag8_gate(void);

/* 0x34CF1 - when record `idx` byte +6 is set: status(9,27,0), set the flag and
 * return dword_53AD5; otherwise return that byte (0). */
uint32_t ev_rec6_gate(int idx);

/* 0x34D92 - sub-stream 2, then status_mask_records(); returns its EAX. */
uint32_t ev_mask_records(void);

/* 0x34F74 - sub-stream 2, clear byte +52 of records 12 and 13;
 * returns dword_53A45 + 1040 (record 13). */
uint32_t ev_clear_status12_13(void);

/* 0x35123 - when idx == 0, fewer than eight slots are free and the one-shot
 * flag is clear: claim slot (0, 89), run sub-stream 11, set the flag. */
void ev_slot8_claim(int idx);

/* 0x35191 - one shot: status(16,71,<flag byte, 0>) + sub-stream 1, set flag. */
uint32_t ev_status16_71(void);

/* 0x351E6 - sub-stream 6, clear byte +53 of records 64..73, then
 * status(64,73,3) and status(35,49,0); returns the second call's EAX. */
uint32_t ev_clear64_73(void);

/* 0x35258 - sub-stream 8, then status(16,34,0); returns its EAX. */
uint32_t ev_status16_34(void);

#endif /* GAME_EV_H */
