/* keys.h - portable key identity for the entry layers (cross-platform slice).
 *
 * Why this exists
 * ---------------
 * The Win32 entry layers translate a keystroke with MapVirtualKeyA() (VK ->
 * BIOS set-1 make code) and ToAscii() (VK -> ASCII byte). On Linux neither
 * exists: sokol_app hands us a layout-independent SAPP_KEYCODE_* (XKB key
 * names, GLFW-style) plus a SAPP_EVENTTYPE_CHAR with the produced character
 * (its X11 backend runs XLookupString for us). --autokey and --keylog,
 * however, name keys as *text*, and the BIOS keyboard ring only understands
 * (scan, ascii) - so both platforms need one shared identity for a key and
 * one shared scan-code table. That is this file.
 *
 * FR_KEY_LIST below is the single source of truth: the enum, the name table
 * and the scan-code table are all generated from it. src/keyscheck.c pins it
 * against the Windows reference: for every key, MapVirtualKeyA(vk, VK_TO_VSC)
 * must equal the table's scan code, and the VK <- key mapping must round-trip
 * (see docs/rounds/16-entry-layer.md).
 *
 * Scan codes are BIOS set 1 (the make code the 8042 puts in the BDA ring).
 * "ext" means the BIOS prefixes the key with 0xE0; the entry layer does not
 * put that byte in the scan position (it goes in the ascii byte, exactly as
 * main_win32.c does today). The "ext" flag in the table is therefore *the
 * entry layers' 0xE0 rule*, not the raw 8042 stream: only UP/DOWN/LEFT/RIGHT/
 * HOME/END/PGUP/PGDN/INS/DEL set it, because those are the only VKs in
 * is_extended_vk(). RCTRL/RALT/KPENTER/LWIN/... do carry 0xE0 on a real
 * keyboard, but the current host does not mark them, and the Linux layer must
 * produce the *same* (scan, ascii) pair as Windows - so they stay ext=0.
 * src/keyscheck.c pins the table against MapVirtualKeyA and that rule.
 *
 * PRINT_SCREEN and PAUSE are deliberately absent: MapVirtualKeyA returns no
 * usable make code for them (0 and 0x54 on the reference machine) and the
 * guest never receives them.
 */
#ifndef FD2_KEYS_H
#define FD2_KEYS_H

#include <stddef.h>
#include <stdint.h>

/* fields: enum id, canonical name, BIOS set-1 make code, 0xE0-prefixed flag */
#define FR_KEY_LIST(X) \
    X(FRK_ESCAPE,       "ESC",        0x01, 0) \
    X(FRK_1,            "1",          0x02, 0) \
    X(FRK_2,            "2",          0x03, 0) \
    X(FRK_3,            "3",          0x04, 0) \
    X(FRK_4,            "4",          0x05, 0) \
    X(FRK_5,            "5",          0x06, 0) \
    X(FRK_6,            "6",          0x07, 0) \
    X(FRK_7,            "7",          0x08, 0) \
    X(FRK_8,            "8",          0x09, 0) \
    X(FRK_9,            "9",          0x0A, 0) \
    X(FRK_0,            "0",          0x0B, 0) \
    X(FRK_MINUS,        "MINUS",      0x0C, 0) \
    X(FRK_EQUAL,        "EQUAL",      0x0D, 0) \
    X(FRK_BACKSPACE,    "BACKSPACE",  0x0E, 0) \
    X(FRK_TAB,          "TAB",        0x0F, 0) \
    X(FRK_Q,            "Q",          0x10, 0) \
    X(FRK_W,            "W",          0x11, 0) \
    X(FRK_E,            "E",          0x12, 0) \
    X(FRK_R,            "R",          0x13, 0) \
    X(FRK_T,            "T",          0x14, 0) \
    X(FRK_Y,            "Y",          0x15, 0) \
    X(FRK_U,            "U",          0x16, 0) \
    X(FRK_I,            "I",          0x17, 0) \
    X(FRK_O,            "O",          0x18, 0) \
    X(FRK_P,            "P",          0x19, 0) \
    X(FRK_LEFT_BRACKET, "LBRACKET",   0x1A, 0) \
    X(FRK_RIGHT_BRACKET,"RBRACKET",   0x1B, 0) \
    X(FRK_RETURN,       "RETURN",     0x1C, 0) \
    X(FRK_LEFT_CONTROL, "LCTRL",      0x1D, 0) \
    X(FRK_A,            "A",          0x1E, 0) \
    X(FRK_S,            "S",          0x1F, 0) \
    X(FRK_D,            "D",          0x20, 0) \
    X(FRK_F,            "F",          0x21, 0) \
    X(FRK_G,            "G",          0x22, 0) \
    X(FRK_H,            "H",          0x23, 0) \
    X(FRK_J,            "J",          0x24, 0) \
    X(FRK_K,            "K",          0x25, 0) \
    X(FRK_L,            "L",          0x26, 0) \
    X(FRK_SEMICOLON,    "SEMICOLON",  0x27, 0) \
    X(FRK_APOSTROPHE,   "APOSTROPHE", 0x28, 0) \
    X(FRK_GRAVE,        "GRAVE",      0x29, 0) \
    X(FRK_LEFT_SHIFT,   "LSHIFT",     0x2A, 0) \
    X(FRK_BACKSLASH,    "BACKSLASH",  0x2B, 0) \
    X(FRK_Z,            "Z",          0x2C, 0) \
    X(FRK_X,            "X",          0x2D, 0) \
    X(FRK_C,            "C",          0x2E, 0) \
    X(FRK_V,            "V",          0x2F, 0) \
    X(FRK_B,            "B",          0x30, 0) \
    X(FRK_N,            "N",          0x31, 0) \
    X(FRK_M,            "M",          0x32, 0) \
    X(FRK_COMMA,        "COMMA",      0x33, 0) \
    X(FRK_PERIOD,       "PERIOD",     0x34, 0) \
    X(FRK_SLASH,        "SLASH",      0x35, 0) \
    X(FRK_RIGHT_SHIFT,  "RSHIFT",     0x36, 0) \
    X(FRK_KP_MULTIPLY,  "KPMUL",      0x37, 0) \
    X(FRK_LEFT_ALT,     "LALT",       0x38, 0) \
    X(FRK_SPACE,        "SPACE",      0x39, 0) \
    X(FRK_CAPS_LOCK,    "CAPS",       0x3A, 0) \
    X(FRK_F1,           "F1",         0x3B, 0) \
    X(FRK_F2,           "F2",         0x3C, 0) \
    X(FRK_F3,           "F3",         0x3D, 0) \
    X(FRK_F4,           "F4",         0x3E, 0) \
    X(FRK_F5,           "F5",         0x3F, 0) \
    X(FRK_F6,           "F6",         0x40, 0) \
    X(FRK_F7,           "F7",         0x41, 0) \
    X(FRK_F8,           "F8",         0x42, 0) \
    X(FRK_F9,           "F9",         0x43, 0) \
    X(FRK_F10,          "F10",        0x44, 0) \
    X(FRK_NUM_LOCK,     "NUMLOCK",    0x45, 0) \
    X(FRK_SCROLL_LOCK,  "SCROLLLOCK", 0x46, 0) \
    X(FRK_KP_7,         "KP7",        0x47, 0) \
    X(FRK_KP_8,         "KP8",        0x48, 0) \
    X(FRK_KP_9,         "KP9",        0x49, 0) \
    X(FRK_KP_SUBTRACT,  "KPSUB",      0x4A, 0) \
    X(FRK_KP_4,         "KP4",        0x4B, 0) \
    X(FRK_KP_5,         "KP5",        0x4C, 0) \
    X(FRK_KP_6,         "KP6",        0x4D, 0) \
    X(FRK_KP_ADD,       "KPADD",      0x4E, 0) \
    X(FRK_KP_1,         "KP1",        0x4F, 0) \
    X(FRK_KP_2,         "KP2",        0x50, 0) \
    X(FRK_KP_3,         "KP3",        0x51, 0) \
    X(FRK_KP_0,         "KP0",        0x52, 0) \
    X(FRK_KP_DECIMAL,   "KPDOT",      0x53, 0) \
    X(FRK_F11,          "F11",        0x57, 0) \
    X(FRK_F12,          "F12",        0x58, 0) \
    X(FRK_KP_ENTER,     "KPENTER",    0x1C, 0) \
    X(FRK_RIGHT_CONTROL,"RCTRL",      0x1D, 0) \
    X(FRK_KP_DIVIDE,    "KPDIV",      0x35, 0) \
    X(FRK_RIGHT_ALT,    "RALT",       0x38, 0) \
    X(FRK_HOME,         "HOME",       0x47, 1) \
    X(FRK_UP,           "UP",         0x48, 1) \
    X(FRK_PAGE_UP,      "PGUP",       0x49, 1) \
    X(FRK_LEFT,         "LEFT",       0x4B, 1) \
    X(FRK_RIGHT,        "RIGHT",      0x4D, 1) \
    X(FRK_END,          "END",        0x4F, 1) \
    X(FRK_DOWN,         "DOWN",       0x50, 1) \
    X(FRK_PAGE_DOWN,    "PGDN",       0x51, 1) \
    X(FRK_INSERT,       "INS",        0x52, 1) \
    X(FRK_DELETE,       "DEL",        0x53, 1) \
    X(FRK_LEFT_SUPER,   "LWIN",       0x5B, 0) \
    X(FRK_RIGHT_SUPER,  "RWIN",       0x5C, 0) \
    X(FRK_MENU,         "MENU",       0x5D, 0)

typedef enum {
    FRK_NONE = 0,          /* "no key": distinct from every FRK_* below    */
#define FR_KEY_ENUM(id, name, scan, ext) id,
    FR_KEY_LIST(FR_KEY_ENUM)
#undef FR_KEY_ENUM
    FRK_COUNT
} fr_key;

/* Iteration helper for tests and for dumping the table. */
#define FR_KEY_ROW(id, name, scan, ext) { name, id, scan, ext },
struct fr_key_info {
    const char *name;
    fr_key      key;
    uint8_t     scan;      /* BIOS set-1 make code                          */
    uint8_t     ext;       /* 1 = the BIOS prefixes it with 0xE0            */
};

/* The one table (generated from FR_KEY_LIST in keys.c). */
extern const struct fr_key_info fr_key_table[];
extern const int               fr_key_table_count;

const char *fr_key_name(fr_key k);
uint8_t     fr_key_scan(fr_key k);       /* 0 for FRK_NONE               */
int         fr_key_extended(fr_key k);

/* US-layout, unshifted ASCII for a key - what ToAscii() would produce on
 * Windows with no modifier held (letters lowercase, digits, space, return,
 * ...). 0 for keys that produce no character (arrows, F-keys, modifiers).
 * Only used where there is no ToAscii (the POSIX entry layer, --autokey);
 * Windows keeps using ToAscii so its behaviour is bit-identical. */
uint8_t     fr_key_ascii(fr_key k);

/* Case-insensitive; accepts the canonical names above plus a few aliases
 * ("ENTER" -> RETURN, single "." / "," etc.). Returns FRK_NONE if unknown. */
fr_key      fr_key_by_name(const char *s, size_t n);

/* Reverse lookup: BIOS make code (+ E0 flag) -> key. Several keys share a
 * make code (KP7 / HOME, RETURN / KPENTER, ...); the 0xE0 flag disambiguates
 * exactly the pairs the guest can tell apart. */
fr_key      fr_key_from_scan(uint8_t scan, int extended);

/* ---- Windows bridge (src/keys_win32.c) ----
 * Plain ints on purpose: this header stays free of windows.h so the POSIX
 * build can include it. Only Windows builds link keys_win32.c. */
int         fr_key_vk(fr_key k);          /* 0 = no VK for this key */
fr_key      fr_key_from_vk(int vk);

#endif /* FD2_KEYS_H */
