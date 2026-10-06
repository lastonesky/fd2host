/* keys.c - the one portable key table (see keys.h for why).
 *
 * Everything here is generated from FR_KEY_LIST, so the enum, the name table
 * and the BIOS scan table can never drift apart. Nothing in this file touches
 * an OS API: it is the piece both entry layers share.
 */
#include "keys.h"

#include <string.h>

const struct fr_key_info fr_key_table[] = {
#define FR_KEY_ROW(id, name, scan, ext) { name, id, scan, ext },
    FR_KEY_LIST(FR_KEY_ROW)
#undef FR_KEY_ROW
};

const int fr_key_table_count = (int)(sizeof fr_key_table / sizeof fr_key_table[0]);

/* The enum is generated from the same list, so a linear scan is enough for
 * the reverse direction and keeps the table the single source of truth. */
const char *fr_key_name(fr_key k)
{
    int i;
    for (i = 0; i < fr_key_table_count; i++)
        if (fr_key_table[i].key == k)
            return fr_key_table[i].name;
    return NULL;
}

uint8_t fr_key_scan(fr_key k)
{
    int i;
    for (i = 0; i < fr_key_table_count; i++)
        if (fr_key_table[i].key == k)
            return fr_key_table[i].scan;
    return 0;
}

int fr_key_extended(fr_key k)
{
    int i;
    for (i = 0; i < fr_key_table_count; i++)
        if (fr_key_table[i].key == k)
            return fr_key_table[i].ext;
    return 0;
}

static int name_eq(const char *tab, const char *s, size_t n)
{
    size_t i;
    if (strlen(tab) != n)
        return 0;
    for (i = 0; i < n; i++) {
        char a = tab[i], b = s[i];
        if (a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
        if (b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');
        if (a != b)
            return 0;
    }
    return 1;
}

fr_key fr_key_by_name(const char *s, size_t n)
{
    /* Aliases the existing command lines already accept. They deliberately
     * do not appear in fr_key_table: a recording always writes the canonical
     * spelling, the alias only exists so hand-written --autokey keeps
     * working. */
    static const struct { const char *name; fr_key key; } aliases[] = {
        { "ENTER",  FRK_RETURN },
        { "PGUP",   FRK_PAGE_UP },
        { "PGDN",   FRK_PAGE_DOWN },
        { "INS",    FRK_INSERT },
        { "DEL",    FRK_DELETE },
        { "ESC",    FRK_ESCAPE },
        { "CAPS",   FRK_CAPS_LOCK },
    };
    size_t i;

    if (!s || n == 0)
        return FRK_NONE;
    if (n == 1) {
        char c = s[0];
        if (c == '.') return FRK_PERIOD;
        if (c == ',') return FRK_COMMA;
        if (c == '-') return FRK_MINUS;
        if (c == '=') return FRK_EQUAL;
        if (c == '/') return FRK_SLASH;
    }
    for (i = 0; i < sizeof aliases / sizeof aliases[0]; i++)
        if (name_eq(aliases[i].name, s, n))
            return aliases[i].key;
    for (i = 0; i < (size_t)fr_key_table_count; i++)
        if (name_eq(fr_key_table[i].name, s, n))
            return fr_key_table[i].key;
    return FRK_NONE;
}

fr_key fr_key_from_scan(uint8_t scan, int extended)
{
    int i;
    for (i = 0; i < fr_key_table_count; i++) {
        if (fr_key_table[i].scan != scan)
            continue;
        if (fr_key_table[i].ext == (uint8_t)(extended ? 1 : 0))
            return fr_key_table[i].key;
    }
    /* A make code that only exists in the other row (e.g. 0x47 without E0 is
     * always KP7 on a real 8042) falls back to the non-extended match. */
    for (i = 0; i < fr_key_table_count; i++)
        if (fr_key_table[i].scan == scan && fr_key_table[i].ext == 0)
            return fr_key_table[i].key;
    return FRK_NONE;
}
