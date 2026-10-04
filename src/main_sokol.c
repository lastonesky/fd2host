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
 * the CRT allocates anything.
 *
 * Why translation goes through Windows VK codes: the BIOS keyboard buffer
 * wants (scan code, ascii) exactly like the GDI entry layer produces, and
 * MapVirtualKeyA/ToAscii are the same calls main_win32.c uses - so both
 * entry layers generate *identical* keystrokes for the game. */

#define SOKOL_NO_ENTRY
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "host.h"
#include "render.h"
#include "winshot.h"
#include "sokol_app.h"
#include "sokol_log.h"

static render_desc g_rd;
static int         g_down[256];      /* VK held state, to drop key repeats */

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

/* --autokey runs on its own thread and injects by virtual key. */
void input_post_vk(int vk)
{
    printf("host: autokey vk=%02X (scan %02X)\n", vk,
           (unsigned)MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC));
    push_vk(vk, 1);
    push_vk(vk, 0);
}

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
    const char *shot;

    if (!host_wants_frames())
        return;
    if (host_frame()) {
        shot = host_window_shot_path();
        if (shot) {
#if defined(_WIN32)
            winshot_capture((void *)sapp_win32_get_hwnd(), shot);
#endif
        }
    }
}

static void event_cb(const sapp_event *e)
{
    int vk;

    switch (e->type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
        if (e->key_repeat)
            break;
        vk = sapp_to_vk((int)e->key_code);
        /* same escape hatch as main_win32.c: Ctrl+Esc quits */
        if (vk == VK_ESCAPE && (e->modifiers & SAPP_MODIFIER_CTRL)) {
            host_request_quit();
            break;
        }
        push_vk(vk, 1);
        break;

    case SAPP_EVENTTYPE_KEY_UP:
        push_vk(sapp_to_vk((int)e->key_code), 0);
        break;

    case SAPP_EVENTTYPE_QUIT_REQUESTED:
        host_request_quit();
        break;

    default:
        break;
    }
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
