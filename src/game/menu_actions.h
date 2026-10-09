/* menu_actions.h - FD2 menu/party action handlers.
 *
 * These are the leaf actions reachable from the two menu dispatch tables:
 *
 *   menu_reload_world            0x205DA  reload the whole party/map world
 *   menu_need_*                  0x206C5..0x20B3C  "option denied" checks:
 *                                each one inspects record flags and, when the
 *                                option is not allowed, sets dword_53ECC (the
 *                                refusal code) and/or draws menu sub-streams
 *   menu_show_*                  0x3314B..0x33AAE  menu-screen actions: reload
 *                                the world, step the portrait, and draw the
 *                                menu's vm sub-streams
 *
 * Real ABI is plain cdecl for all of them (IDA's __fastcall/__usercall is the
 * usual stack-probe artifact, see src/game/vm.h). The C bodies therefore take
 * no arguments and push the rec_flag / unit_exists / vm_run arguments exactly
 * as the machine code does.
 */
#ifndef FD2_GAME_MENU_ACTIONS_H
#define FD2_GAME_MENU_ACTIONS_H

#include <stdint.h>

/* 0x205DA - reset the world: reload party+map (0x1088D), clear the view
 * origin/counters, refresh the map view, fade back in and flush the key
 * buffer. */
void menu_reload_world(void);

/* --- menu A: option-denied checks -------------------------------------- */

/* 0x206C5 - records 5..10 must all carry flag bit0; else refusal 1. */
void menu_need_records_marked(void);
/* 0x20707 - refuse (1) when rec_flag(50) or rec_flag(51). */
void menu_need_flags_clear_50_51(void);
/* 0x2073D - refuse (1) when rec_flag(14). */
void menu_need_flag_clear_14(void);
/* 0x20765 - "menu 10": refuse (1) + draw sub-stream 10 when every
 * rec_flag(15..26) is set; when dword_53BEF>5 and rec_flag(59), refuse (1)
 * + draw sub-stream 2. */
void menu_need_any_clear_15_26(void);
/* 0x20822 - refuse (1) when rec_flag(64). */
void menu_need_flag_clear_64(void);
/* 0x2084A - refuse (1) when rec_flag(65). */
void menu_need_flag_clear_65(void);
/* 0x20872 - only when unit 18 is absent: refuse (1) + draw sub-stream 2 when
 * rec_flag(52). */
void menu_need_flag_clear_52_no_unit18(void);
/* 0x20926 - when dword_53BEF>6: refuse (1) when rec_flag(64). */
void menu_need_flag_clear_64_late(void);
/* 0x20957 - two-range check (records 0x26..0x2D, then 0x2E..0x43 plus
 * rec_flag(0)/rec_flag(0x34)): refusal codes 1 and 2. */
void menu_need_any_clear_26_43(void);
/* 0x20A51 - refuse (1) when rec_flag(16) or rec_flag(17). */
void menu_need_flags_clear_16_17(void);
/* 0x20A87 - refuse (1) when rec_flag(1). */
void menu_need_flag_clear_1(void);
/* 0x20B14 - refuse (1) when rec_flag(16). */
void menu_need_flag_clear_16(void);
/* 0x20B3C - refuse (1) when rec_flag(1) or rec_flag(2). */
void menu_need_flags_clear_1_2(void);

/* --- menu B: menu-screen actions --------------------------------------- */

/* 0x3314B - reload, then draw sub-stream 0. */
void menu_show_reload(void);
/* 0x33219 - reload, step the portrait, draw sub-streams 0 and 1. */
void menu_show_reload_pair(void);
/* 0x3332B - reload, stamp two records with 100, draw sub-stream 0. */
void menu_show_mark_two(void);
/* 0x3346B - reload and draw sub-stream 0 (also the body of 0x335A0/0x33674). */
void menu_show_plain(void);
/* 0x3347C - reload, step the portrait by 20, draw sub-stream 0. */
void menu_show_step20(void);
/* 0x335AA - reload; rebuild the unit sprites when unit 18 is absent. */
void menu_show_or_rebuild_sprites(void);
/* 0x3367E - reload, step the portrait by 16, clear it, draw sub-stream 0. */
void menu_show_step16_then_clear(void);
/* 0x33AAE - reload, step the portrait by 9, then draw sub-stream 0 + clear. */
void menu_show_step9_then_clear(void);

/* 0x33169 - reload, draw sub-stream 0, rebuild the unit sprites, step the
 * portrait twice, draw sub-stream 1. */
void menu_rebuild_sprites_and_show(void);
/* 0x3327D - reload, stamp 11 records with +3=2, draw sub-streams 0 and 1,
 * then clear the portraits. */
void menu_mark_records_and_show(void);
/* 0x33367 - reload, draw sub-stream 0, rebuild the unit sprites, step the
 * portrait, draw sub-stream 1, then clear + draw sub-stream 1 again. */
void menu_rebuild_sprites_and_clear(void);
/* 0x333F5 - reload, reset the portrait, rebuild the unit sprites, step,
 * clear, then draw sub-stream 0. */
void menu_reset_and_show(void);
/* 0x334D9 - reload; pick one of three sub-streams from unit 12's presence,
 * step the portrait, then glide it back. */
void menu_show_gated_by_unit(void);
/* 0x335DA - reload, draw sub-stream 0, step the portrait twice the other
 * way, then draw sub-stream 1 and clear. */
void menu_step_pair_then_clear(void);
/* 0x338C4 - reload, draw sub-stream 0, rebuild the sprites, then walk the
 * portrait through four positions with 400 ms pauses. */
void menu_show_step_pairs(void);
/* 0x3396A - reload, load FDOTHER.DAT#88 as the effect bank, draw sub-stream
 * 1, clear the map, pan it four times with sound, then draw sub-stream 2 and
 * release the bank. */
void menu_show_map_pan(void);
/* 0x1D4F6 - stop the effect bank and free it. */
void menu_stop_and_free_music(void);

#endif /* FD2_GAME_MENU_ACTIONS_H */
