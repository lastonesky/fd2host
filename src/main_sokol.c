/* main_sokol.c - sokol_app entry layer for the FD2 native host.
 *
 * Same contract as main_win32.c (see host.h), driven by sokol callbacks
 * instead of a Win32 message pump:
 *
 *     main()  -> host_init() -> sapp_run()
 *                   init_cb    : render_init() + host_start()
 *                   frame_cb   : host_frame() (+ --wshot capture)
 *                   event_cb   : key events -> host_key()
 *                   cleanup_cb : render_shutdown() + host_shutdown()
 *
 * SOKOL_NO_ENTRY (defined in sokol_impl.c as well) keeps sokol from
 * hijacking main(), so entry.c still reserves the game address space before
 * the CRT allocates anything on Windows. On POSIX there is no fd2_entry (the
 * low window is free, le.c maps it explicitly), so a plain main() is used.
 *
 * Key translation has two implementations:
 *
 *   Windows: sapp_keycode -> VK (sapp_to_vk), then the same
 *            MapVirtualKeyA/ToAscii path main_win32.c uses - so both Win32
 *            entry layers generate identical keystrokes.
 *   POSIX:   sapp_keycode -> fr_key (src/keys.h) -> BIOS (scan, ascii); the
 *            character comes from sokol's SAPP_EVENTTYPE_CHAR event (its X11
 *            backend runs XLookupString internally), which replaces ToAscii.
 *
 * The window is sokol_app's: on Linux that is an X11 client, which runs
 * unchanged on a Wayland desktop through XWayland (docs/BACKEND.md §13.11). */

#define SOKOL_NO_ENTRY
#include <stdio.h>
#include <string.h>
#include "host.h"
#include "render.h"
#include "winshot.h"
#include "keys.h"
#include "sokol_app.h"
#include "sokol_log.h"

#if defined(_WIN32)
#include <windows.h>
#endif

static render_desc g_rd;

#if defined(_WIN32)
/* -------------------------------------------------------------------------
 * Win32 path: keep the exact translation main_win32.c uses.
 * ---------------------------------------------------------------------- */

static int g_down[256];              /* VK held state, to drop key repeats */

/* sapp_keycode -> Windows virtual key. The ASCII range maps 1:1 only for
 * letters, digits and space; punctuation and everything >= 256 does not. */
static int sapp_to_vk(int k)
{
    if ((k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9') || k == ' ')
        return k;

    switch (k) {
    case SAPP_KEYCODE_ESCAPE:       return VK_ESCAPE;
    case SAPP_KEYCODE_ENTER:        return VK_RETURN;
    case SAPP_KEYCODE_TAB:          return VK_TAB;
    case SAPP_KEYCODE_BACKSPACE:    return VK_BACK;
    case SAPP_KEYCODE_INSERT:       return VK_INSERT;
    case SAPP_KEYCODE_DELETE:       return VK_DELETE;
    case SAPP_KEYCODE_RIGHT:        return VK_RIGHT;
    case SAPP_KEYCODE_LEFT:         return VK_LEFT;
    case SAPP_KEYCODE_DOWN:         return VK_DOWN;
    case SAPP_KEYCODE_UP:           return VK_UP;
    case SAPP_KEYCODE_PAGE_UP:      return VK_PRIOR;
    case SAPP_KEYCODE_PAGE_DOWN:    return VK_NEXT;
    case SAPP_KEYCODE_HOME:         return VK_HOME;
    case SAPP_KEYCODE_END:          return VK_END;
    case SAPP_KEYCODE_CAPS_LOCK:    return VK_CAPITAL;
    case SAPP_KEYCODE_SCROLL_LOCK:  return VK_SCROLL;
    case SAPP_KEYCODE_NUM_LOCK:     return VK_NUMLOCK;
    case SAPP_KEYCODE_PRINT_SCREEN: return VK_SNAPSHOT;
    case SAPP_KEYCODE_PAUSE:        return VK_PAUSE;
    case SAPP_KEYCODE_APOSTROPHE:   return VK_OEM_7;   /* ' */
    case SAPP_KEYCODE_COMMA:        return VK_OEM_COMMA;
    case SAPP_KEYCODE_MINUS:        return VK_OEM_MINUS;
    case SAPP_KEYCODE_PERIOD:       return VK_OEM_PERIOD;
    case SAPP_KEYCODE_SLASH:        return VK_OEM_2;
    case SAPP_KEYCODE_SEMICOLON:    return VK_OEM_1;
    case SAPP_KEYCODE_EQUAL:        return VK_OEM_PLUS;
    case SAPP_KEYCODE_LEFT_BRACKET: return VK_OEM_4;
    case SAPP_KEYCODE_BACKSLASH:    return VK_OEM_5;
    case SAPP_KEYCODE_RIGHT_BRACKET:return VK_OEM_6;
    case SAPP_KEYCODE_GRAVE_ACCENT: return VK_OEM_3;
    case SAPP_KEYCODE_KP_0:         return VK_NUMPAD0;
    case SAPP_KEYCODE_KP_1:         return VK_NUMPAD1;
    case SAPP_KEYCODE_KP_2:         return VK_NUMPAD2;
    case SAPP_KEYCODE_KP_3:         return VK_NUMPAD3;
    case SAPP_KEYCODE_KP_4:         return VK_NUMPAD4;
    case SAPP_KEYCODE_KP_5:         return VK_NUMPAD5;
    case SAPP_KEYCODE_KP_6:         return VK_NUMPAD6;
    case SAPP_KEYCODE_KP_7:         return VK_NUMPAD7;
    case SAPP_KEYCODE_KP_8:         return VK_NUMPAD8;
    case SAPP_KEYCODE_KP_9:         return VK_NUMPAD9;
    case SAPP_KEYCODE_KP_DECIMAL:   return VK_DECIMAL;
    case SAPP_KEYCODE_KP_DIVIDE:    return VK_DIVIDE;
    case SAPP_KEYCODE_KP_MULTIPLY:  return VK_MULTIPLY;
    case SAPP_KEYCODE_KP_SUBTRACT:  return VK_SUBTRACT;
    case SAPP_KEYCODE_KP_ADD:       return VK_ADD;
    case SAPP_KEYCODE_KP_ENTER:     return VK_RETURN;
    case SAPP_KEYCODE_LEFT_SHIFT:   return VK_LSHIFT;
    case SAPP_KEYCODE_LEFT_CONTROL: return VK_LCONTROL;
    case SAPP_KEYCODE_LEFT_ALT:     return VK_LMENU;
    case SAPP_KEYCODE_RIGHT_SHIFT:  return VK_RSHIFT;
    case SAPP_KEYCODE_RIGHT_CONTROL:return VK_RCONTROL;
    case SAPP_KEYCODE_RIGHT_ALT:    return VK_RMENU;
    default: break;
    }
    if (k >= SAPP_KEYCODE_F1 && k <= SAPP_KEYCODE_F12)
        return VK_F1 + (k - SAPP_KEYCODE_F1);
    return 0;
}

/* Keys the BIOS reports with an 0xE0 prefix - the same rule main_win32.c
 * applies, so both entry layers produce identical buffer contents. */
static int is_extended_vk(int vk)
{
    switch (vk) {
    case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT:
    case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
    case VK_INSERT: case VK_DELETE:
        return 1;
    default:
        return 0;
    }
}

static uint8_t ascii_for_vk(int vk)
{
    BYTE ks[256];
    WORD ch = 0;
    UINT sc;

    if (is_extended_vk(vk))
        return 0xE0;
    if (!GetKeyboardState(ks))
        return 0;
    sc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
    if (ToAscii((UINT)vk, sc, ks, &ch, 0) == 1 && ch < 0x100)
        return (uint8_t)ch;
    return 0;
}

static void push_vk(int vk, int down)
{
    UINT sc;

    if (vk <= 0 || vk >= 256)
        return;
    /* drop repeats and unmatched key-ups: sokol reports auto-repeat as
     * KEY_DOWN with key_repeat = true, Win32 as repeated WM_KEYDOWN */
    if (down) {
        if (g_down[vk]) return;
        g_down[vk] = 1;
    } else {
        if (!g_down[vk]) return;
        g_down[vk] = 0;
    }
    sc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
    host_key((uint8_t)(down ? sc : (sc | 0x80)), ascii_for_vk(vk));
}

/* --autokey/--keyplay runs on its own thread and injects by portable key.
 * fr_key -> VK (src/keys_win32.c) -> push_vk(), i.e. exactly the path a real
 * keystroke takes, so a replayed key and a pressed key are the same event. */
void input_post_key(fr_key key)
{
    int vk = fr_key_vk(key);

    if (!vk)
        return;
    printf("host: autokey %s (vk=%02X scan %02X)\n", fr_key_name(key), vk,
           (unsigned)MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC));
    push_vk(vk, 1);
    push_vk(vk, 0);
}

#else /* !_WIN32 */
/* -------------------------------------------------------------------------
 * POSIX path: sokol_keycode -> fr_key -> BIOS (scan, ascii).
 * ---------------------------------------------------------------------- */

static unsigned char g_down[FRK_COUNT]; /* fr_key held state, drop repeats */
static int           g_char_pending;    /* last make code may still be patched */

/* sapp_keycode -> portable key. sokol's X11 backend builds these codes from
 * XKB key *names*, so they identify a physical key regardless of layout
 * (the same property Windows VKs have via MapVirtualKeyA). */
static fr_key sapp_to_fr_key(int k)
{
    if ((k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9')) {
        char c = (char)k;
        return fr_key_by_name(&c, 1);
    }
    switch (k) {
    case SAPP_KEYCODE_SPACE:        return FRK_SPACE;
    case SAPP_KEYCODE_APOSTROPHE:   return FRK_APOSTROPHE;
    case SAPP_KEYCODE_COMMA:        return FRK_COMMA;
    case SAPP_KEYCODE_MINUS:        return FRK_MINUS;
    case SAPP_KEYCODE_PERIOD:       return FRK_PERIOD;
    case SAPP_KEYCODE_SLASH:        return FRK_SLASH;
    case SAPP_KEYCODE_SEMICOLON:    return FRK_SEMICOLON;
    case SAPP_KEYCODE_EQUAL:        return FRK_EQUAL;
    case SAPP_KEYCODE_LEFT_BRACKET: return FRK_LEFT_BRACKET;
    case SAPP_KEYCODE_BACKSLASH:    return FRK_BACKSLASH;
    case SAPP_KEYCODE_RIGHT_BRACKET:return FRK_RIGHT_BRACKET;
    case SAPP_KEYCODE_GRAVE_ACCENT: return FRK_GRAVE;
    case SAPP_KEYCODE_ESCAPE:       return FRK_ESCAPE;
    case SAPP_KEYCODE_ENTER:        return FRK_RETURN;
    case SAPP_KEYCODE_TAB:          return FRK_TAB;
    case SAPP_KEYCODE_BACKSPACE:    return FRK_BACKSPACE;
    case SAPP_KEYCODE_INSERT:       return FRK_INSERT;
    case SAPP_KEYCODE_DELETE:       return FRK_DELETE;
    case SAPP_KEYCODE_RIGHT:        return FRK_RIGHT;
    case SAPP_KEYCODE_LEFT:         return FRK_LEFT;
    case SAPP_KEYCODE_DOWN:         return FRK_DOWN;
    case SAPP_KEYCODE_UP:           return FRK_UP;
    case SAPP_KEYCODE_PAGE_UP:      return FRK_PAGE_UP;
    case SAPP_KEYCODE_PAGE_DOWN:    return FRK_PAGE_DOWN;
    case SAPP_KEYCODE_HOME:         return FRK_HOME;
    case SAPP_KEYCODE_END:          return FRK_END;
    case SAPP_KEYCODE_CAPS_LOCK:    return FRK_CAPS_LOCK;
    case SAPP_KEYCODE_SCROLL_LOCK:  return FRK_SCROLL_LOCK;
    case SAPP_KEYCODE_NUM_LOCK:     return FRK_NUM_LOCK;
    case SAPP_KEYCODE_PAUSE:        return FRK_NONE;  /* no portable key */
    case SAPP_KEYCODE_PRINT_SCREEN: return FRK_NONE;
    case SAPP_KEYCODE_KP_0:         return FRK_KP_0;
    case SAPP_KEYCODE_KP_1:         return FRK_KP_1;
    case SAPP_KEYCODE_KP_2:         return FRK_KP_2;
    case SAPP_KEYCODE_KP_3:         return FRK_KP_3;
    case SAPP_KEYCODE_KP_4:         return FRK_KP_4;
    case SAPP_KEYCODE_KP_5:         return FRK_KP_5;
    case SAPP_KEYCODE_KP_6:         return FRK_KP_6;
    case SAPP_KEYCODE_KP_7:         return FRK_KP_7;
    case SAPP_KEYCODE_KP_8:         return FRK_KP_8;
    case SAPP_KEYCODE_KP_9:         return FRK_KP_9;
    case SAPP_KEYCODE_KP_DECIMAL:   return FRK_KP_DECIMAL;
    case SAPP_KEYCODE_KP_DIVIDE:    return FRK_KP_DIVIDE;
    case SAPP_KEYCODE_KP_MULTIPLY:  return FRK_KP_MULTIPLY;
    case SAPP_KEYCODE_KP_SUBTRACT:  return FRK_KP_SUBTRACT;
    case SAPP_KEYCODE_KP_ADD:       return FRK_KP_ADD;
    case SAPP_KEYCODE_KP_ENTER:     return FRK_KP_ENTER;
    case SAPP_KEYCODE_LEFT_SHIFT:   return FRK_LEFT_SHIFT;
    case SAPP_KEYCODE_LEFT_CONTROL: return FRK_LEFT_CONTROL;
    case SAPP_KEYCODE_LEFT_ALT:     return FRK_LEFT_ALT;
    case SAPP_KEYCODE_LEFT_SUPER:   return FRK_LEFT_SUPER;
    case SAPP_KEYCODE_RIGHT_SHIFT:  return FRK_RIGHT_SHIFT;
    case SAPP_KEYCODE_RIGHT_CONTROL:return FRK_RIGHT_CONTROL;
    case SAPP_KEYCODE_RIGHT_ALT:    return FRK_RIGHT_ALT;
    case SAPP_KEYCODE_RIGHT_SUPER:  return FRK_RIGHT_SUPER;
    case SAPP_KEYCODE_MENU:         return FRK_MENU;
    default: break;
    }
    if (k >= SAPP_KEYCODE_F1 && k <= SAPP_KEYCODE_F12)
        return (fr_key)(FRK_F1 + (k - SAPP_KEYCODE_F1));
    return FRK_NONE;
}

/* BIOS (scan, ascii) for a key. Extended keys have no character, so their
 * ascii byte is 0xE0 - the same convention main_win32.c uses. For the rest
 * the character arrives in a following CHAR event and is patched in; if the
 * key produces none (F-keys, modifiers) ascii stays 0, like ToAscii. */
static uint8_t scan_ascii(fr_key key, uint8_t *scan)
{
    *scan = fr_key_scan(key);
    return fr_key_extended(key) ? 0xE0 : 0;
}

static void push_fr_key(fr_key key, int down)
{
    uint8_t scan, ascii;

    if (key == FRK_NONE)
        return;
    ascii = scan_ascii(key, &scan);
    if (!scan)
        return;
    if (down) {
        if (g_down[key]) return;
        g_down[key] = 1;
    } else {
        if (!g_down[key]) return;
        g_down[key] = 0;
    }
    host_key((uint8_t)(down ? scan : (scan | 0x80)), ascii);
    g_char_pending = down && !fr_key_extended(key);
}

/* fr_key -> BIOS directly: no window is needed, so --autokey/--keyplay work
 * even before sokol_app has delivered its first event. */
void input_post_key(fr_key key)
{
    uint8_t scan, ascii;

    if (key == FRK_NONE)
        return;
    ascii = scan_ascii(key, &scan);
    if (!scan)
        return;
    if (!fr_key_extended(key))
        ascii = fr_key_ascii(key);
    printf("host: autokey %s (scan %02X ascii %02X)\n",
           fr_key_name(key), scan, ascii);
    host_key(scan, ascii);
    host_key((uint8_t)(scan | 0x80), ascii);
}

#endif /* _WIN32 */

/* ------------------------------------------------------------- callbacks */

static void init_cb(void)
{
    if (render_init(&g_rd) != 0) {
        printf("host: render init failed (sokol)\n");
        host_request_quit();
        return;
    }
    printf("host: render backend = %s\n", render_name());
    host_start();
#if defined(_WIN32)
    {
        HWND hw = (HWND)sapp_win32_get_hwnd();
        RECT c, w; POINT pt;
        pt.x = 0; pt.y = 0;
        GetClientRect(hw, &c); GetWindowRect(hw, &w); ClientToScreen(hw, &pt);
        printf("sokol: window=%ldx%ld client=%ldx%ld clientOrigin=(%ld,%ld) sapp=%dx%d\n",
               w.right - w.left, w.bottom - w.top,
               c.right - c.left, c.bottom - c.top, pt.x, pt.y,
               sapp_width(), sapp_height());
    }
#endif
}

static void frame_cb(void)
{
    if (!host_wants_frames())
        return;
    if (host_frame()) {
        const char *shot = host_window_shot_path();
        if (shot) {
#if defined(_WIN32)
            winshot_capture((void *)sapp_win32_get_hwnd(), shot);
#else
            /* --wshot is PrintWindow (Win32 only); --screenshot is backend
             * independent and stays the cross-platform evidence. */
            printf("host: --wshot is not supported on this platform "
                   "(use --screenshot)\n");
#endif
        }
    }
}

static void event_cb(const sapp_event *e)
{
#if defined(_WIN32)
    switch (e->type) {
    case SAPP_EVENTTYPE_KEY_DOWN: {
        int vk;
        if (e->key_repeat)
            break;
        vk = sapp_to_vk((int)e->key_code);
        /* same escape hatch as main_win32.c: Ctrl+Esc quits */
        if (vk == VK_ESCAPE && (e->modifiers & SAPP_MODIFIER_CTRL)) {
            host_request_quit();
            break;
        }
        if (host_no_user_input())
            break;                 /* autokey injects via input_post_key */
        push_vk(vk, 1);
        break;
    }
    case SAPP_EVENTTYPE_KEY_UP:
        if (host_no_user_input())
            break;
        push_vk(sapp_to_vk((int)e->key_code), 0);
        break;
    case SAPP_EVENTTYPE_QUIT_REQUESTED:
        host_request_quit();
        break;
    default:
        break;
    }
#else
    switch (e->type) {
    case SAPP_EVENTTYPE_KEY_DOWN: {
        fr_key key;
        if (e->key_repeat)
            break;
        key = sapp_to_fr_key((int)e->key_code);
        if (key == FRK_ESCAPE && (e->modifiers & SAPP_MODIFIER_CTRL)) {
            host_request_quit();
            break;
        }
        if (key == FRK_NONE || host_no_user_input())
            break;
        push_fr_key(key, 1);
        break;
    }
    case SAPP_EVENTTYPE_KEY_UP:
        if (host_no_user_input())
            break;
        g_char_pending = 0;        /* a break code never carries a char */
        push_fr_key(sapp_to_fr_key((int)e->key_code), 0);
        break;
    case SAPP_EVENTTYPE_CHAR:
        /* sokol's X11 backend runs XLookupString for us: this event carries
         * what ToAscii would have produced on Windows. */
        if (g_char_pending) {
            host_key_set_last_ascii(e->char_code < 0x100
                                        ? (uint8_t)e->char_code : 0);
            g_char_pending = 0;
        }
        break;
    case SAPP_EVENTTYPE_QUIT_REQUESTED:
        host_request_quit();
        break;
    default:
        break;
    }
#endif
}

static void cleanup_cb(void)
{
    render_shutdown();
    host_shutdown();
}

/* ------------------------------------------------------------------ main */

int main(int argc, char **argv)
{
    sapp_desc desc;

    memset(&desc, 0, sizeof desc);
    if (host_init(argc, argv) != 0)
        return 1;

    g_rd = host_render_desc();

    desc.init_cb      = init_cb;
    desc.frame_cb     = frame_cb;
    desc.cleanup_cb   = cleanup_cb;
    desc.event_cb     = event_cb;
    desc.width        = g_rd.logical_w * g_rd.scale;
    desc.height       = g_rd.logical_h * g_rd.scale;
    desc.window_title = "FlameDragon2 - native host (sokol)";
    desc.swap_interval = 1;              /* vsync: replaces the 20 ms timer */
    desc.logger.func  = slog_func;

    sapp_run(&desc);                     /* returns when the app quits */
    return 0;
}
