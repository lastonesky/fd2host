/* ev7.h - FD2 menu/party action handlers, batch 7 (see docs/rounds/44-*.md).
 *
 * These are the handlers reachable from the two menu dispatch tables:
 *
 *   0x205DA          ev7_205DA          reload the whole party/map world
 *   0x206C5 .. 0x20B3C  ev7_206C5..ev7_20B3C   "select a record" menu actions
 *   0x3314B .. 0x33AAE  ev7_3314B..ev7_33AAE   script/vm menu actions
 *
 * Every one of them is an ordinary cdecl function in the original (IDA's
 * __fastcall/__usercall annotation is the usual stack-probe artifact, see
 * src/game/vm.h), so the C names take no arguments and the bodies push the
 * vm_run / rec_flag / unit_exists arguments exactly as the machine code does.
 */
#ifndef FD2_GAME_EV7_H
#define FD2_GAME_EV7_H

#include <stdint.h>

/* 0x205DA - reset the world: reload party+map (0x1088D), clear the view
 * origin/counters, refresh the map view and fade back in. */
void ev7_205DA(void);

/* 0x206C5 - every record 5..10 must already carry flag bit0. */
void ev7_206C5(void);
/* 0x20707 - fail if rec_flag(50) or rec_flag(51). */
void ev7_20707(void);
/* 0x2073D - fail if rec_flag(14). */
void ev7_2073D(void);
/* 0x20765 - long "menu 10" validation (records 15..26 + optional 59). */
void ev7_20765(void);
/* 0x20822 - fail if rec_flag(64). */
void ev7_20822(void);
/* 0x2084A - fail if rec_flag(65). */
void ev7_2084A(void);
/* 0x20872 - only when unit 18 is absent: fail if rec_flag(52). */
void ev7_20872(void);
/* 0x20926 - when dword_53BEF>6: fail if rec_flag(64). */
void ev7_20926(void);
/* 0x20957 - long "menu A" validation across records. */
void ev7_20957(void);
/* 0x20A51 - fail if rec_flag(16) or rec_flag(17). */
void ev7_20A51(void);
/* 0x20A87 - fail if rec_flag(1). */
void ev7_20A87(void);
/* 0x20B14 - fail if rec_flag(16). */
void ev7_20B14(void);
/* 0x20B3C - fail if rec_flag(1) or rec_flag(2). */
void ev7_20B3C(void);

/* 0x3314B - reload, then draw menu sub-stream 0. */
void ev7_3314B(void);
/* 0x33219 - reload, slide/step the portrait, draw sub-streams 0 and 1. */
void ev7_33219(void);
/* 0x3332B - reload, set two record bytes to 100, draw sub-stream 0. */
void ev7_3332B(void);
/* 0x3346B - reload and draw sub-stream 0. */
void ev7_3346B(void);
/* 0x3347C - reload, step the portrait, draw sub-stream 0. */
void ev7_3347C(void);
/* 0x335A0 - reload and draw sub-stream 0 (shared body 0x33470). */
void ev7_335A0(void);
/* 0x335AA - reload; if unit 18 is absent rebuild the unit sprites. */
void ev7_335AA(void);
/* 0x33674 - reload and draw sub-stream 0 (shared body 0x33470). */
void ev7_33674(void);
/* 0x3367E - reload, step the portrait, clear it, draw sub-stream 0. */
void ev7_3367E(void);
/* 0x33AAE - reload, step the portrait, draw sub-stream 1. */
void ev7_33AAE(void);

#endif /* FD2_GAME_EV7_H */
