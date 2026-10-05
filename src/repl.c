/* repl.c - install the translated C modules over the original machine code.
 *
 * Every entry in the table below is a function whose C translation passed
 * the machine-code differential test (src/*check.c). The entry point is
 * rewritten to a 5-byte `jmp rel32`; callers (direct or through a function
 * pointer) then reach the C implementation.
 *
 * Deliberately NOT wired in yet:
 *   - res.c (0x111BA): the original allocates through the Watcom CRT heap,
 *     and the game frees those buffers with the same heap. Replacing it with
 *     a libc-malloc version would mix heaps. It goes in together with the
 *     CRT heap replacement.
 *
 * See repl.h for why this is safe. Groups: rle, gfx, sprite24, util, path.
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "repl.h"
#include "game/rle.h"
#include "game/gfx.h"
#include "game/sprite24.h"
#include "game/util.h"
#include "game/path.h"

#define OBJ0_BASE 0x00010000u

/* 0x4DED4 hardcodes its table base; the C version takes it as a parameter. */
static const void *__cdecl rep_rec3(int index)
{
    return util_rec3((const void *)(uintptr_t)0x60181u, index);
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

    /* --- movement range + path tracer (src/game/path.c) --------------- */
    { 0x4E390, "path_mark",           (void *)path_mark,           REPL_PATH },
    { 0x4E4F6, "path_find",           (void *)path_find,           REPL_PATH },
};

unsigned repl_parse(const char *spec)
{
    char  buf[128];
    char *tok;
    unsigned mask = 0;

    if (!spec || !spec[0] || !_stricmp(spec, "all"))
        return REPL_ALL;
    if (!_stricmp(spec, "none"))
        return 0;

    strncpy(buf, spec, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';
    for (tok = strtok(buf, ", "); tok; tok = strtok(NULL, ", ")) {
        if      (!_stricmp(tok, "rle"))      mask |= REPL_RLE;
        else if (!_stricmp(tok, "gfx"))      mask |= REPL_GFX;
        else if (!_stricmp(tok, "sprite24")) mask |= REPL_SPRITE24;
        else if (!_stricmp(tok, "util"))     mask |= REPL_UTIL;
        else if (!_stricmp(tok, "path"))     mask |= REPL_PATH;
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
