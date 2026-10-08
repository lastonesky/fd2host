/* ev2.h - FD2 scene-script handlers of funcs_1199C, second batch.
 *
 * `funcs_1199C` (@0x51B91) is a 91-entry dispatch table; src/game/ev.c already
 * covers the 11 dependency-closed entries at indices 4..36. This module takes
 * the *script/animation* half, indices 38..90 (linear addresses
 * 0x35298..0x3644E), which the earlier round had left behind as a
 * "~13 KB scene-render core". IDA re-reading showed the table is longer than
 * assumed: indices 38..90 are one compact run of 53 handlers, most of them
 * tiny "run VM sub-stream / set a byte / call a service" steps. This batch is
 * the first 30 of them; the remaining 23 follow the same pattern.
 *
 * Uniform ABI: the dispatcher pushes one dword argument and the caller cleans
 * it (`add esp,4`). The `push <frame>; call 0x3702F` prefix that Hex-Rays
 * shows as arguments is the Watcom stack probe (PITFALLS §8-75): 0x3702F ends
 * in `retn 4` and keeps EAX, so the real arguments start at [esp+4]. Only the
 * functions that read `[esp+arg_0]` declare `int arg`; the rest declare it and
 * ignore it so the C signature matches `void (*)(int)`.
 *
 * Services stay at their original addresses (the scene.c / msg.c / ev.c
 * pattern): the host has already patched the translated ones, and the
 * differential harness hooks them to recording stubs. Heap/file services
 * (0x10B4E loads FDICON/FDFIELD and rewrites FD2.TMP, 0x35B78 plays a fade
 * sequence) remain machine code here - the host's call lands on the original,
 * the harness's hook records it.
 *
 * Verified against the machine code by src/ev2check.c.
 *
 *   0x35298  funcs_1199C[38]      0x35321  funcs_1199C[41]
 *   0x353B5  funcs_1199C[43]      0x353E7  funcs_1199C[44]
 *   0x353FA  funcs_1199C[45]      0x3540F  funcs_1199C[46]
 *   0x35422  funcs_1199C[47]      0x3551C  funcs_1199C[49]
 *   0x3553F  funcs_1199C[50]      0x355B7  funcs_1199C[51]
 *   0x35638  funcs_1199C[53]      0x35677  funcs_1199C[54]
 *   0x35997  funcs_1199C[60]      0x359CB  funcs_1199C[61]
 *   0x35BEE  funcs_1199C[63]      0x35C1D  funcs_1199C[64]
 *   0x35D85  funcs_1199C[68]      0x35F79  funcs_1199C[74]
 *   0x36214  funcs_1199C[78]      0x36228  funcs_1199C[79]
 *   0x362B0  funcs_1199C[81]      0x362C5  funcs_1199C[82]
 *   0x363DE  funcs_1199C[84]      0x36416  funcs_1199C[85]
 *   0x3642E..0x3644E funcs_1199C[86..90]
 *
 * plus the shared core helper 0x135DD (map scroll animation).
 */
#ifndef GAME_EV2_H
#define GAME_EV2_H

#include <stdint.h>

/* 0x135DD - step the visible map window to (x, y) one cell at a time, one
 * 0x11CAC view update + key flush per step. The low dword of qword_53AB1
 * counts the x moves, the high dword the y moves. */
void ev2_135DD(int x, int y);

/* Every handler below takes the dispatcher's one dword argument (ignored by
 * most of them) and returns nothing observable. */
void ev2_35298(int arg);
void ev2_35321(int arg);
void ev2_353B5(int arg);
void ev2_353E7(int arg);
void ev2_353FA(int arg);
void ev2_3540F(int arg);
void ev2_35422(int arg);
void ev2_3551C(int arg);
void ev2_3553F(int arg);
void ev2_355B7(int arg);
void ev2_35638(int arg);
void ev2_35677(int arg);
void ev2_35997(int arg);
void ev2_359CB(int arg);
void ev2_35BEE(int arg);
void ev2_35C1D(int arg);
void ev2_35D85(int arg);
void ev2_35F79(int arg);
void ev2_36214(int arg);
void ev2_36228(int arg);
void ev2_362B0(int arg);
void ev2_362C5(int arg);
void ev2_363DE(int arg);
void ev2_36416(int arg);
void ev2_3642E(int arg);
void ev2_36439(int arg);
void ev2_36440(int arg);
void ev2_36447(int arg);
void ev2_3644E(int arg);

#endif /* GAME_EV2_H */
