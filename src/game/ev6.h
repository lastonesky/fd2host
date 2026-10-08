/* ev6.h - FD2 scene-script handlers of funcs_1199C, first batch (indices 0..37).
 *
 * `funcs_1199C` (@0x51B91) is the 90-entry dispatch table.  Rounds 39-42
 * already covered indices 38..90 (`game/ev2.c`..`ev5.c`); this module takes
 * the remaining low entries, the "unit / battle prep" handlers:
 *
 *   0x34531 idx[0]   0x3460B idx[1]   0x34673 idx[2]   0x346CD idx[3]
 *   0x34778 idx[6]   0x350BE idx[5]   0x350C8 idx[7]   0x34818 idx[9]
 *   0x348BB idx[11]  0x34940 idx[14]  0x34984 idx[15]  0x349EC idx[16]
 *   0x34A1E idx[17]  0x34B07 idx[20]  0x34B6F idx[22]  0x34B9A idx[23]
 *   0x34C52 idx[24]  0x34C7A idx[25]  0x34D2F idx[27]  0x34DD0 idx[30]
 *   0x34EB3 idx[31]  0x34F38 idx[32]  0x34FC2 idx[34]  0x34FCC idx[35]
 *   0x35022 idx[37]
 *
 * Uniform ABI: the dispatcher pushes one dword and cleans it; the Watcom stack
 * probe (0x3702F) is ignored.  The compiler tail-merged the `vm_run` call
 * (0x3486C/0x34885/0x34663/0x34F65/0x34750/0x35F6E) and the `vm_run(3)` body
 * (0x34C52/0x34FC2); the C inlines those tails.
 *
 * Services stay at their original addresses; the differential harness hooks
 * 0x10B4E and 0x32999 to stubs so the handler logic is compared on its own.
 *
 * Verified against the machine code by src/ev2check.c (batch-6 entries).
 */
#ifndef GAME_EV6_H
#define GAME_EV6_H

/* Every handler takes the dispatcher's one (mostly ignored) dword argument. */
void ev6_34531(int arg);
void ev6_3460B(int arg);
void ev6_34673(int arg);
void ev6_346CD(int arg);
void ev6_34778(int arg);
void ev6_350BE(int arg);
void ev6_350C8(int arg);
void ev6_34818(int arg);
void ev6_348BB(int arg);
void ev6_34940(int arg);
void ev6_34984(int arg);
void ev6_349EC(int arg);
void ev6_34A1E(int arg);
void ev6_34B07(int arg);
void ev6_34B6F(int arg);
void ev6_34B9A(int arg);
void ev6_34C52(int arg);
void ev6_34C7A(int arg);
void ev6_34D2F(int arg);
void ev6_34DD0(int arg);
void ev6_34EB3(int arg);
void ev6_34F38(int arg);
void ev6_34FC2(int arg);
void ev6_34FCC(int arg);
void ev6_35022(int arg);

#endif /* GAME_EV6_H */
