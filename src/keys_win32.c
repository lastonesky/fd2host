/* keys_win32.c - Windows bridge for the portable key table (keys.h).
 *
 * Only this file knows about Windows virtual-key codes. main_win32.c /
 * main_sokol.c keep using MapVirtualKeyA/ToAscii for the scan and ascii of a
 * *real* keystroke (so the guest sees exactly what it sees today); the
 * portable id is what --autokey names and --keylog records, which is what
 * has to survive the trip to POSIX.
 *
 * `--autokey` currently accepts names like RETURN/SPACE/UP or a single
 * letter/digit. Those all resolve through fr_key_by_name() -> fr_key_vk()
 * here, and src/keyscheck.c proves the triangle (key -> VK -> BIOS scan)
 * matches the reference the entry layers use (MapVirtualKeyA).
 */
#include <windows.h>
#include "keys.h"

/* The keys whose VK is not simply their character. Keypad-Enter is an alias:
 * Windows has no separate VK for it (VK_RETURN covers both), so it is listed
 * after FRK_RETURN and fr_key_from_vk() therefore never returns it - the
 * same one-way behaviour MapVirtualKeyA has. */
static const struct { fr_key key; int vk; } g_vk[] = {
    { FRK_ESCAPE,         VK_ESCAPE },
    { FRK_BACKSPACE,      VK_BACK },
    { FRK_TAB,            VK_TAB },
    { FRK_RETURN,         VK_RETURN },
    { FRK_KP_ENTER,       VK_RETURN },     /* alias, see above */
    { FRK_LEFT_CONTROL,   VK_LCONTROL },
    { FRK_RIGHT_CONTROL,  VK_RCONTROL },
    { FRK_LEFT_SHIFT,     VK_LSHIFT },
    { FRK_RIGHT_SHIFT,    VK_RSHIFT },
    { FRK_LEFT_ALT,       VK_LMENU },
    { FRK_RIGHT_ALT,      VK_RMENU },
    { FRK_LEFT_SUPER,     VK_LWIN },
    { FRK_RIGHT_SUPER,    VK_RWIN },
    { FRK_MENU,           VK_APPS },
    { FRK_SPACE,          VK_SPACE },
    { FRK_CAPS_LOCK,      VK_CAPITAL },
    { FRK_NUM_LOCK,       VK_NUMLOCK },
    { FRK_SCROLL_LOCK,    VK_SCROLL },
    { FRK_MINUS,          VK_OEM_MINUS },
    { FRK_EQUAL,          VK_OEM_PLUS },
    { FRK_LEFT_BRACKET,   VK_OEM_4 },
    { FRK_RIGHT_BRACKET,  VK_OEM_6 },
    { FRK_SEMICOLON,      VK_OEM_1 },
    { FRK_APOSTROPHE,     VK_OEM_7 },
    { FRK_GRAVE,          VK_OEM_3 },
    { FRK_BACKSLASH,      VK_OEM_5 },
    { FRK_COMMA,          VK_OEM_COMMA },
    { FRK_PERIOD,         VK_OEM_PERIOD },
    { FRK_SLASH,          VK_OEM_2 },
    { FRK_KP_0,           VK_NUMPAD0 },
    { FRK_KP_1,           VK_NUMPAD1 },
    { FRK_KP_2,           VK_NUMPAD2 },
    { FRK_KP_3,           VK_NUMPAD3 },
    { FRK_KP_4,           VK_NUMPAD4 },
    { FRK_KP_5,           VK_NUMPAD5 },
    { FRK_KP_6,           VK_NUMPAD6 },
    { FRK_KP_7,           VK_NUMPAD7 },
    { FRK_KP_8,           VK_NUMPAD8 },
    { FRK_KP_9,           VK_NUMPAD9 },
    { FRK_KP_DECIMAL,     VK_DECIMAL },
    { FRK_KP_DIVIDE,      VK_DIVIDE },
    { FRK_KP_MULTIPLY,    VK_MULTIPLY },
    { FRK_KP_SUBTRACT,    VK_SUBTRACT },
    { FRK_KP_ADD,         VK_ADD },
    { FRK_INSERT,         VK_INSERT },
    { FRK_DELETE,         VK_DELETE },
    { FRK_HOME,           VK_HOME },
    { FRK_END,            VK_END },
    { FRK_PAGE_UP,        VK_PRIOR },
    { FRK_PAGE_DOWN,      VK_NEXT },
    { FRK_UP,             VK_UP },
    { FRK_DOWN,           VK_DOWN },
    { FRK_LEFT,           VK_LEFT },
    { FRK_RIGHT,          VK_RIGHT },
    { FRK_F1,             VK_F1 },  { FRK_F2,  VK_F2 },
    { FRK_F3,             VK_F3 },  { FRK_F4,  VK_F4 },
    { FRK_F5,             VK_F5 },  { FRK_F6,  VK_F6 },
    { FRK_F7,             VK_F7 },  { FRK_F8,  VK_F8 },
    { FRK_F9,             VK_F9 },  { FRK_F10, VK_F10 },
    { FRK_F11,            VK_F11 }, { FRK_F12, VK_F12 },

    /* Legacy aliases: the generic modifier VKs. The pre-keys-table keylog
     * recorded a shift as "#16" (MapVirtualKeyA(scan, VSC_TO_VK) returns
     * VK_SHIFT, not VK_LSHIFT), so keep those spellings resolvable - a
     * recording made before the portable table must still replay. They sit
     * last so fr_key_vk() (which uses the specific left/right VKs) is unaffected. */
    { FRK_LEFT_SHIFT,     VK_SHIFT },
    { FRK_LEFT_CONTROL,   VK_CONTROL },
    { FRK_LEFT_ALT,       VK_MENU },
};

int fr_key_vk(fr_key k)
{
    const char *n = fr_key_name(k);
    int i;

    /* Letters and digits are their own VK ('A'..'Z', '0'..'9'), and their
     * canonical name is that one character. */
    if (n && n[1] == 0) {
        if (n[0] >= 'A' && n[0] <= 'Z') return n[0];
        if (n[0] >= '0' && n[0] <= '9') return n[0];
        if (n[0] >= 'a' && n[0] <= 'z') return n[0] - 'a' + 'A';
    }
    for (i = 0; i < (int)(sizeof g_vk / sizeof g_vk[0]); i++)
        if (g_vk[i].key == k)
            return g_vk[i].vk;
    return 0;
}

fr_key fr_key_from_vk(int vk)
{
    int i;

    /* The table comes first: several VKs above 'Z' would otherwise look like
     * letters (VK_F1=0x70='p', VK_MULTIPLY=0x6A='j', VK_NUMPAD7=0x67='g'). */
    for (i = 0; i < (int)(sizeof g_vk / sizeof g_vk[0]); i++)
        if (g_vk[i].vk == vk)
            return g_vk[i].key;             /* first entry wins (aliases) */
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        char buf[2];
        buf[0] = (char)vk;
        buf[1] = 0;
        return fr_key_by_name(buf, 1);
    }
    if (vk >= 'a' && vk <= 'z')
        return fr_key_from_vk(vk - 'a' + 'A');
    return FRK_NONE;
}
