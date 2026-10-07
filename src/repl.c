/* repl.c - install the translated C modules over the original machine code.
 *
 * Every entry in the table below is a function whose C translation passed
 * the machine-code differential test (the src/...check.c tools). The entry is
 * rewritten to a 5-byte `jmp rel32`; callers (direct or through a function
 * pointer) then reach the C implementation.
 *
 * Deliberately NOT wired in yet:
 *   - 0x15E71 (restore + free a VGA snapshot block) and 0x15E9E (save one):
 *     heap-coupled - dlg.c calls both at their original addresses instead,
 *     which keeps one heap on every path. They can move to C once their
 *     snapshot buffers also come from guest_mem (docs/TRANSLATION.md §5).
 *
 * res.c (0x111BA) IS wired in: it allocates/frees through guest_mem.h, so it
 * uses the game's heap, and publishes the size to the game's dword_53BFF -
 * one heap, one observable.
 *
 * See repl.h for why this is safe. Groups: rle, gfx, sprite24, util, path,
 * dlg, rec, svc, vm, res, bgm, scene, fade, map, fx.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"
#include "repl.h"
#include "game/rle.h"
#include "game/gfx.h"
#include "game/sprite24.h"
#include "game/util.h"
#include "game/path.h"
#include "game/tables.h"
#include "game/rle2.h"
#include "game/dlg.h"
#include "game/rec.h"
#include "game/svc.h"
#include "game/vm.h"
#include "game/res.h"
#include "game/bgm.h"
#include "game/scene.h"
#include "game/fade.h"
#include "game/kbd.h"
#include "game/map.h"
#include "game/anim.h"
#include "game/tables.h"
#include "game/unit.h"
#include "game/fx.h"
#include "game/msg.h"
#include "game/ev.h"

#define OBJ0_BASE 0x00010000u

/* 0x4DED4 hardcodes its table base; the C version takes it as a parameter. */
static const void *PLAT_CDECL rep_rec3(int index)
{
    return util_rec3((const void *)(uintptr_t)0x60181u, index);
}

/* The eleven table accessors 0x4E7DD..0x4E8BC, with the FD2 data bases. */
static void    *PLAT_CDECL rep_t615FE(int i) { return tbl_ptr((void *)(uintptr_t)0x615FEu,  2, i, -64); }
static void    *PLAT_CDECL rep_t626B3(int i) { return tbl_ptr((void *)(uintptr_t)0x626B3u, 12, i,   0); }
static void    *PLAT_CDECL rep_t6238D(int i) { return tbl_ptr((void *)(uintptr_t)0x6238Du, 31, i, -31); }
static void    *PLAT_CDECL rep_t620A1(int i) { return tbl_ptr((void *)(uintptr_t)0x620A1u, 11, i,   0); }
static void    *PLAT_CDECL rep_t61DA1(int i) { return tbl_ptr((void *)(uintptr_t)0x61DA1u, 24, i,   0); }
static void    *PLAT_CDECL rep_t61AF9(int i) { return tbl_ptr((void *)(uintptr_t)0x61AF9u, 10, i,   0); }
static void    *PLAT_CDECL rep_t619FD(int i) { return tbl_ptr((void *)(uintptr_t)0x619FDu,  7, i,   0); }
static uint32_t PLAT_CDECL rep_t61955(int i) { return tbl_u32((const void *)(uintptr_t)0x61955u, i); }
static void    *PLAT_CDECL rep_t6188A(int i) { return tbl_ptr((void *)(uintptr_t)0x6188Au,  7, i,   0); }
static void    *PLAT_CDECL rep_t61646(int i) { return tbl_ptr((void *)(uintptr_t)0x61646u, 20, i,   0); }
static void    *PLAT_CDECL rep_t602AD(int i) { return tbl_ptr((void *)(uintptr_t)0x602ADu, 23, i,   0); }

/* 0x16559 / 0x16E24 read the FD2 globals dword_53A85 / dword_53C67. */
static void PLAT_CDECL rep_dlg_blit(int idx)
{
    dlg_blit_dato((const void *)(uintptr_t)(*(const uint32_t *)(uintptr_t)0x53A85u),
                  (int)(*(const uint32_t *)(uintptr_t)0x53C67u), idx);
}
static void PLAT_CDECL rep_dlg_scroll(void)
{
    dlg_scroll_text((int)(*(const uint32_t *)(uintptr_t)0x53C67u));
}

struct repl_entry {
    uint32_t    addr;     /* linear address of the original function */
    const char *name;
    void       *impl;
    unsigned    group;
};

static const struct repl_entry g_repl[] = {
    /* --- RLE decoder (src/game/rle.c) --------------------------------- */
    { 0x4E98D, "rle_decode",          (void *)rle_decode,          REPL_RLE },
    { 0x4E8D3, "rle_decode_lut",      (void *)rle_decode_lut,      REPL_RLE },

    /* --- 0xC0-range RLE blits (src/game/rle2.c) ----------------------- */
    { 0x4EBFF, "rle2_blit",           (void *)rle2_blit,           REPL_RLE },
    { 0x4EC31, "rle2_blit_mirror",    (void *)rle2_blit_mirror,    REPL_RLE },
    { 0x4EBAB, "rle2_blit_trans",     (void *)rle2_blit_trans,     REPL_RLE },

    /* --- graphics blitter helpers (src/game/gfx.c) -------------------- */
    { 0x4EC7C, "gfx_restore_rect",    (void *)gfx_restore_rect,    REPL_GFX },
    { 0x4ECBF, "gfx_save_rect",       (void *)gfx_save_rect,       REPL_GFX },
    { 0x4ED0B, "gfx_blit_block",      (void *)gfx_blit_block,      REPL_GFX },
    { 0x4ED34, "gfx_blit_transparent",(void *)gfx_blit_transparent,REPL_GFX },
    { 0x4ED7A, "gfx_draw_glyph",      (void *)gfx_draw_glyph,      REPL_GFX },
    { 0x4EEE0, "gfx_expand_scanlines",(void *)gfx_expand_scanlines,REPL_GFX },

    /* --- 24x24 sprite RLE family (src/game/sprite24.c) ---------------- */
    { 0x4DF84, "sprite24_ramp",       (void *)sprite24_ramp,       REPL_SPRITE24 },
    { 0x4E016, "sprite24_pal_recolor",(void *)sprite24_pal_recolor,REPL_SPRITE24 },
    { 0x4E0A2, "sprite24_pal",        (void *)sprite24_pal,        REPL_SPRITE24 },
    { 0x4E127, "sprite24_const",      (void *)sprite24_const,      REPL_SPRITE24 },
    { 0x4E1A6, "sprite24_ramp24",     (void *)sprite24_ramp24,     REPL_SPRITE24 },
    { 0x4E22A, "sprite24_plain",      (void *)sprite24_plain,      REPL_SPRITE24 },
    { 0x4E29C, "sprite24_plain49",    (void *)sprite24_plain49,    REPL_SPRITE24 },

    /* --- byte / palette utilities (src/game/util.c) ------------------- */
    { 0x4DED4, "util_rec3",           (void *)rep_rec3,            REPL_UTIL },
    { 0x4DEEC, "util_translate",      (void *)util_translate,      REPL_UTIL },
    { 0x4DF09, "util_sum_tail4",      (void *)util_sum_tail4,      REPL_UTIL },
    { 0x4DF28, "util_deobfuscate",    (void *)util_deobfuscate,    REPL_UTIL },
    { 0x4DF4C, "util_fix_records",    (void *)util_fix_records,    REPL_UTIL },
    { 0x4E795, "util_mask_recolor",   (void *)util_mask_recolor,   REPL_UTIL },

    /* --- table accessors (src/game/tables.c) -------------------------- */
    { 0x4E7DD, "tbl_615FE",           (void *)rep_t615FE,          REPL_UTIL },
    { 0x4E7F2, "tbl_626B3",           (void *)rep_t626B3,          REPL_UTIL },
    { 0x4E809, "tbl_6238D",           (void *)rep_t6238D,          REPL_UTIL },
    { 0x4E821, "tbl_620A1",           (void *)rep_t620A1,          REPL_UTIL },
    { 0x4E838, "tbl_61DA1",           (void *)rep_t61DA1,          REPL_UTIL },
    { 0x4E84F, "tbl_61AF9",           (void *)rep_t61AF9,          REPL_UTIL },
    { 0x4E866, "tbl_619FD",           (void *)rep_t619FD,          REPL_UTIL },
    { 0x4E87D, "tbl_61955_dword",     (void *)rep_t61955,          REPL_UTIL },
    { 0x4E88E, "tbl_6188A",           (void *)rep_t6188A,          REPL_UTIL },
    { 0x4E8A5, "tbl_61646",           (void *)rep_t61646,          REPL_UTIL },
    { 0x4E8BC, "tbl_602AD",           (void *)rep_t602AD,          REPL_UTIL },

    /* --- movement range + path tracer (src/game/path.c) --------------- */
    { 0x4E390, "path_mark",           (void *)path_mark,           REPL_PATH },
    { 0x4E4F6, "path_find",           (void *)path_find,           REPL_PATH },

    /* --- dialogue box helpers (src/game/dlg.c) ------------------------ */
    { 0x16559, "dlg_blit_dato",       (void *)rep_dlg_blit,         REPL_DLG },
    { 0x16E24, "dlg_scroll_text",     (void *)rep_dlg_scroll,       REPL_DLG },

    /* --- portrait snapshot save/restore (src/game/dlg.c) ---------------
     * 0x15E9E allocates its record with guest_malloc and 0x15E71 restores and
     * guest_free()s it, so the records stay on the game heap and the many
     * machine-code callers of 0x15E71 keep working (source translation is
     * case-by-case; the heap is shared). */
    { 0x15E9E, "dlg_snap_save",       (void *)dlg_snap_save,        REPL_DLG },
    { 0x15E71, "dlg_snap_restore",    (void *)dlg_snap_restore,     REPL_DLG },

    /* --- box open/close animation (src/game/dlg.c) ---------------------
     * App-level routines: the C reads/writes the original globals itself,
     * so no wrapper is needed - the signature matches the machine code
     * (cdecl stack args) one to one. Both ends of the CRT heap pairing are
     * in this group (open allocates through 0x3706E, close frees through
     * 0x15E71), so it can only be enabled as a whole - which repl_parse
     * guarantees: the group is the unit. */
    { 0x165AC, "dlg_open_box",        (void *)dlg_open_box,         REPL_DLG },
    { 0x16B43, "dlg_close_box",       (void *)dlg_close_box,        REPL_DLG },
    { 0x168B6, "dlg_box_stage",       (void *)dlg_box_stage,        REPL_DLG },
    { 0x1685C, "dlg_frame_tile",      (void *)dlg_frame_tile,       REPL_DLG },

    /* --- dialogue portrait compositor (src/game/msg.c) ------------------
     * 0x1956B / 0x1974C / 0x26996 share the three screen-pair globals
     * dword_53C5B/F/63, so they must be enabled as a unit - REPL_DLG is
     * atomic and already owns the box helpers they call. The buffers are
     * allocated/freed through guest_mem (one heap); the services go through
     * their original addresses (dlg_box_stage / res_load / rle2_blit_mirror
     * are already C in the host). */
    { 0x1956B, "msg_open_portrait",   (void *)msg_open_portrait,    REPL_DLG },
    { 0x1974C, "msg_blit_band",       (void *)msg_blit_band,        REPL_DLG },
    { 0x26996, "msg_close_portrait",  (void *)msg_close_portrait,   REPL_DLG },
    { 0x16C57, "dlg_wait_key",        (void *)dlg_wait_key,         REPL_DLG },
    { 0x164E8, "dlg_type_step",       (void *)dlg_type_step,        REPL_DLG },

    /* --- character record table (src/game/rec.c) -----------------------
     * Both entries read/write the game data segment directly, so the C
     * signature matches the machine code one to one (cdecl, one stack arg). */
    { 0x34894, "rec_flag",            (void *)rec_flag,             REPL_REC },
    { 0x12C60, "rec_find",            (void *)rec_find,             REPL_REC },

    /* The character-record 2-byte slot accessors (src/game/rec.c): plain cdecl
     * stack args reading/writing dword_53A45 directly, so no glue is needed. */
    { 0x1B722, "rec_field_byte",     (void *)rec_field_byte,       REPL_REC },
    { 0x344F2, "rec_status_set",     (void *)rec_status_set,       REPL_REC },
    { 0x1BB8C, "rec_slot_claim",     (void *)rec_slot_claim,       REPL_REC },
    { 0x1B8E7, "rec_slot_remove",    (void *)rec_slot_remove,      REPL_REC },

    /* The round-34 record leaves: plain cdecl readers/writers of the record
     * table, so the C signature matches the machine code one to one. 0x34D64 /
     * 0x35009 take no argument even though the dispatch in sub_117E7 pushes one
     * (the caller cleans up), so an argument-less C function is exact. */
    { 0x1B8A6, "rec_slot_free",        (void *)rec_slot_free,           REPL_REC },
    { 0x1B83D, "rec_slot_find",        (void *)rec_slot_find,           REPL_REC },
    { 0x1CA89, "rec_sub_table5",       (void *)rec_sub_table5,          REPL_REC },
    { 0x13512, "rec_flag_or80",        (void *)rec_flag_or80,           REPL_REC },
    { 0x32975, "rec_flag_set1",        (void *)rec_flag_set1,           REPL_REC },
    { 0x34D64, "rec_status_mask_records", (void *)rec_status_mask_records, REPL_REC },
    { 0x35009, "rec_status_set_record14", (void *)rec_status_set_record14, REPL_REC },

    /* The funcs_1199C event handlers (src/game/ev.c): the dependency-closed
     * half of the dispatch table at 0x51B91. They only touch the record table
     * / dword_53AD5 and call the already-translated vm_run / record services
     * at their original addresses, so they share the `rec` group. */
    { 0x34738, "ev_rec13_set1",       (void *)ev_rec13_set1,       REPL_REC },
    { 0x348EA, "ev_status24_27",      (void *)ev_status24_27,      REPL_REC },
    { 0x34A6C, "ev_status7_36",       (void *)ev_status7_36,       REPL_REC },
    { 0x34B2F, "ev_flag8_gate",       (void *)ev_flag8_gate,       REPL_REC },
    { 0x34CF1, "ev_rec6_gate",        (void *)ev_rec6_gate,        REPL_REC },
    { 0x34D92, "ev_mask_records",     (void *)ev_mask_records,     REPL_REC },
    { 0x34F74, "ev_clear_status12_13",(void *)ev_clear_status12_13,REPL_REC },
    { 0x35123, "ev_slot8_claim",      (void *)ev_slot8_claim,      REPL_REC },
    { 0x35191, "ev_status16_71",      (void *)ev_status16_71,      REPL_REC },
    { 0x351E6, "ev_clear64_73",       (void *)ev_clear64_73,       REPL_REC },
    { 0x35258, "ev_status16_34",      (void *)ev_status16_34,      REPL_REC },

    /* The persistent party roster (src/game/unit.c): the three functions
     * that build/sync/recalc its 80-byte records. They call each other, so
     * they share one group and are enabled atomically. */
    { 0x1145A, "unit_recalc",        (void *)unit_recalc,          REPL_REC },
    { 0x11506, "unit_refresh_all",   (void *)unit_refresh_all,     REPL_REC },
    { 0x112A5, "unit_add",           (void *)unit_add,             REPL_REC },
    /* The identity lookup shared by several record/workflow functions; it
     * only reads the party table, so it belongs with its three siblings. */
    { 0x33499, "unit_exists",        (void *)unit_exists,          REPL_REC },

    /* --- tick wait + PCM SFX playback (src/game/svc.c) -------------------
     * App-level too: the globals they need are the game's, and they talk to
     * AIL through the original entry points, which in the host are already
     * the replacements src/ail.c installed - so nothing here depends on
     * whether ail_install ran before repl_install. svc_wait_ticks is reached
     * from 45 call sites, svc_play_sfx from 15 and its twin svc_play_sfx2
     * (0x25B45, byte-for-byte the same body on handle dword_53EE8) from 11,
     * all of them ordinary cdecl callers. */
    { 0x17AA9, "svc_wait_ticks",      (void *)svc_wait_ticks,       REPL_SVC },
    { 0x25A96, "svc_play_sfx",        (void *)svc_play_sfx,         REPL_SVC },
    { 0x25B45, "svc_play_sfx2",       (void *)svc_play_sfx2,        REPL_SVC },

    /* --- script/text VM (src/game/vm.c) --------------------------------
     * 1380 bytes, 126 direct call sites - the interpreter every line of
     * dialogue, portrait swap and printed number goes through. Nine cdecl
     * stack parameters (the "14 register parameters" in re/RE_MAP.md were
     * the 0x3702F stack probe's artifact, see docs/rounds/08 §37.3).
     * It reaches the services through their original addresses, which is
     * how src/game/dlg.c and src/game/res.c are reached from here too. */
    { 0x15F84, "vm_run",             (void *)vm_run,              REPL_VM },

    /* --- LMI resource loader (src/game/res.c) ---------------------------
     * Was held back until the heap question was settled: res_load frees the
     * caller's old_buffer and returns a buffer the game later frees, so both
     * must be the game's heap - guest_mem.h now provides exactly that, and
     * the size is published to dword_53BFF (0x53BFF). */
    { 0x111BA, "res_load",           (void *)res_load,            REPL_RES },

    /* --- background music entry (src/game/bgm.c) ------------------------
     * 0x25977, the only "change track" entry (32 call sites). Calls res_load
     * and the AIL sequence entry points through their original addresses, so
     * this replaces exactly what the machine code did. */
    { 0x25977, "bgm_play",           (void *)bgm_play,            REPL_BGM },

    /* --- main state machine scenes (src/game/scene.c) --------------------
     * First scene: 0x22E5C (the dword_53ECC==1 transition card), a pure
     * service sequence; it calls the services through their original
     * addresses (bgm_play / svc_wait_ticks / res_load are already C here). */
    { 0x22E5C, "scene_card",         (void *)scene_card,          REPL_SCENE },

    /* The six funcs_25E23[] main-state-machine transition handlers reached
     * through `call funcs_25E23[dword_53C03]` in main (0x25BF4). Each is a
     * pure service sequence: vm_run + unit_refresh_all (+ unit_add) then
     * advance dword_53C03 - no VGA writes of its own, so the services are
     * called through their original addresses for the same reason as above.
     * 0x239BD first asks unit_exists(12) which of the two sub-streams to
     * draw. */
    { 0x22EF6, "scene_state_00",     (void *)scene_state_00,      REPL_SCENE },
    { 0x231BC, "scene_state_03",     (void *)scene_state_03,      REPL_SCENE },
    { 0x23790, "scene_state_10",     (void *)scene_state_10,      REPL_SCENE },
    { 0x2389F, "scene_state_12",     (void *)scene_state_12,      REPL_SCENE },
    { 0x239BD, "scene_state_14",     (void *)scene_state_14,      REPL_SCENE },
    { 0x23E39, "scene_state_18",     (void *)scene_state_18,      REPL_SCENE },

    /* --- palette fades (src/game/fade.c) --------------------------------
     * Used by scene_card and a long list of state handlers; `outp`/`delay`
     * are still called by original address (dos.c owns the DAC/tick). */
    { 0x11D40, "pal_fade_range",     (void *)pal_fade_range,      REPL_FADE },
    { 0x1F882, "pal_fade_out",       (void *)pal_fade_out,        REPL_FADE },
    { 0x1F525, "pal_fade_in",        (void *)pal_fade_in,         REPL_FADE },
    { 0x4E310, "pal_tick_word",      (void *)pal_tick_word,       REPL_FADE },
    { 0x4E31C, "pal_anim_step",      (void *)pal_anim_step,       REPL_FADE },

    /* --- hot leaves (round 52, src/leafcheck.c) --------------------------
     * Small, frequently called helpers found by tools/func_ranking.py. */
    { 0x4E381, "kbd_flush",          (void *)kbd_flush,           REPL_SVC },
    { 0x10620, "kbd_pending",        (void *)kbd_pending,         REPL_SVC },
    { 0x4EBE3, "util_rand",          (void *)util_rand,           REPL_UTIL },
    { 0x11EB0, "gfx_copy_rows",      (void *)gfx_copy_rows,       REPL_GFX },
    { 0x2EB9F, "res_blit",           (void *)res_blit,            REPL_RES },
    { 0x12D7B, "dlg_portrait_glide", (void *)dlg_portrait_glide,  REPL_DLG },
    { 0x134E4, "dlg_portrait_clear", (void *)dlg_portrait_clear,  REPL_DLG },
    { 0x11DF2, "pal_fade_add",       (void *)pal_fade_add,        REPL_FADE },
    { 0x16886, "res_blit6",          (void *)res_blit6,           REPL_RES },
    { 0x126F7, "map_blit_tile",      (void *)map_blit_tile,       REPL_MAP },
    { 0x12E38, "map_cell_info",      (void *)map_cell_info,       REPL_MAP },
    { 0x1297D, "anim_frame_step",    (void *)anim_frame_step,     REPL_MAP },
    { 0x12C0D, "dlg_portrait_find",  (void *)dlg_portrait_find,   REPL_DLG },
    { 0x4EB48, "tbl_off627D8",       (void *)tbl_off627D8,        REPL_UTIL },
    { 0x187D6, "dlg_draw_number",    (void *)dlg_draw_number,      REPL_DLG },
    { 0x1875D, "dlg_draw_number_pair", (void *)dlg_draw_number_pair, REPL_DLG },
    { 0x1AEB1, "dlg_draw_number_signed", (void *)dlg_draw_number_signed, REPL_DLG },
    { 0x1F183, "rec_skip",           (void *)rec_skip,             REPL_REC },
    { 0x12AC6, "map_blit_cell_sprite", (void *)map_blit_cell_sprite, REPL_MAP },
    { 0x129EC, "map_refresh_records", (void *)map_refresh_records,  REPL_MAP },
    { 0x11EEE, "map_render_view",     (void *)map_render_view,      REPL_MAP },
    { 0x24D22, "map_scroll_lines",    (void *)map_scroll_lines,     REPL_MAP },
    { 0x122DC, "map_reveal_cursor",   (void *)map_reveal_cursor,    REPL_MAP },
    { 0x1ACF3, "map_draw_cursor",     (void *)map_draw_cursor,      REPL_MAP },
    { 0x32230, "map_unit_ping",       (void *)map_unit_ping,        REPL_MAP },
    { 0x11CAC, "map_view_update",     (void *)map_view_update,      REPL_MAP },

    /* --- portrait icon draw chain (src/game/dlg.c) -----------------------
     * 0x127E0 draws one record's 24x24 icon (sprite24 plain/ramp24) and
     * 0x127A9 sweeps every unflagged record then refreshes the map cells.
     * The atlas is dword_53A61, a 32-bit offset table filled by 0x11019. */
    { 0x127E0, "dlg_portrait_draw",  (void *)dlg_portrait_draw,    REPL_DLG },
    { 0x127A9, "dlg_portraits_refresh", (void *)dlg_portraits_refresh, REPL_DLG },

    /* --- effect-animation handlers (src/game/fx.c) -----------------------
     * funcs_30469[] (0x524C6) table entries [4]/[7]/[8]/[9]. Pure per-frame
     * particle emitters that read/write the game's own data segment and call
     * res_blit / svc_play_sfx(2) / util_rand through their original addresses
     * (already C in the host), so no wrapper is needed - plain cdecl, the
     * five stack arguments map one to one. */
    { 0x2B996, "fx_dots7",           (void *)fx_dots7,            REPL_FX },
    { 0x2BB33, "fx_dots8",           (void *)fx_dots8,            REPL_FX },
    { 0x2BD6C, "fx_blob",            (void *)fx_blob,             REPL_FX },
    { 0x2BFD9, "fx_dots12",          (void *)fx_dots12,           REPL_FX },
    { 0x2C217, "fx_dots6",           (void *)fx_dots6,            REPL_FX },
    { 0x2C441, "fx_dots6b",          (void *)fx_dots6b,           REPL_FX },
    { 0x2CAFC, "fx_dots3",           (void *)fx_dots3,            REPL_FX },
    { 0x2CCF4, "fx_dots16",          (void *)fx_dots16,           REPL_FX },
    { 0x2CE1A, "fx_toggle",          (void *)fx_toggle,           REPL_FX },
};

unsigned repl_parse(const char *spec)
{
    char  buf[128];
    char *tok;
    unsigned mask = 0;

    if (!spec || !spec[0] || !plat_stricmp(spec, "all"))
        return REPL_ALL;
    if (!plat_stricmp(spec, "none"))
        return 0;

    strncpy(buf, spec, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';
    for (tok = strtok(buf, ", "); tok; tok = strtok(NULL, ", ")) {
        if      (!plat_stricmp(tok, "rle"))      mask |= REPL_RLE;
        else if (!plat_stricmp(tok, "gfx"))      mask |= REPL_GFX;
        else if (!plat_stricmp(tok, "sprite24")) mask |= REPL_SPRITE24;
        else if (!plat_stricmp(tok, "util"))     mask |= REPL_UTIL;
        else if (!plat_stricmp(tok, "path"))     mask |= REPL_PATH;
        else if (!plat_stricmp(tok, "dlg"))      mask |= REPL_DLG;
        else if (!plat_stricmp(tok, "rec"))      mask |= REPL_REC;
        else if (!plat_stricmp(tok, "svc"))      mask |= REPL_SVC;
        else if (!plat_stricmp(tok, "vm"))       mask |= REPL_VM;
        else if (!plat_stricmp(tok, "res"))      mask |= REPL_RES;
        else if (!plat_stricmp(tok, "bgm"))      mask |= REPL_BGM;
        else if (!plat_stricmp(tok, "scene"))    mask |= REPL_SCENE;
        else if (!plat_stricmp(tok, "fade"))     mask |= REPL_FADE;
        else if (!plat_stricmp(tok, "fx"))       mask |= REPL_FX;
        else if (!plat_stricmp(tok, "map"))      mask |= REPL_MAP;
        else printf("repl: unknown group '%s'\n", tok);
    }
    return mask;
}

unsigned repl_install(uint8_t *obj0_base, unsigned mask)
{
    unsigned i, n = 0;

    for (i = 0; i < sizeof g_repl / sizeof g_repl[0]; i++) {
        const struct repl_entry *e = &g_repl[i];
        uint8_t *p;
        intptr_t rel;

        if (!(mask & e->group))
            continue;
        p = obj0_base + (e->addr - OBJ0_BASE);
        rel = (intptr_t)e->impl - (intptr_t)(p + 5);
        p[0] = 0xE9;                       /* jmp rel32 */
        *(int32_t *)(p + 1) = (int32_t)rel;
        n++;
    }

    printf("repl: installed %u translated function(s) (mask 0x%X)\n",
           n, mask);
    return n;
}
