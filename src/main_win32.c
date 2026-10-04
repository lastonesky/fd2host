/* main_win32.c - Win32 entry layer for the FD2 native host.
 *
 * Owns everything that is specific to "a Windows process with a window":
 * the custom process entry point, window creation, the message pump, the
 * timer that drives frames, and Win32 -> BIOS keyboard translation.
 *
 * It talks to the kernel (host.c) only through host.h:
 *
 *     host_init(argc, argv)            bring-up up to "ready for a window"
 *     host_render_desc() + render_init create and hand over the window
 *     host_start()                     start the game thread (+ --autokey)
 *     host_frame()                     one frame (timer / paint)
 *     host_key(scan, ascii)            one keystroke for the BDA buffer
 *     host_request_quit() / host_shutdown()
 *
 * A different entry layer (main_sokol.c, step 2) implements the same
 * contract with sokol_app callbacks and no code in host.c changes.
 *
 * The CRT heap starts at 0x10000 and would take the window the DOS/4GW
 * objects must live in, so the reservation has to happen before CRT
 * initialisation - that is what fd2_entry() below is for. */

#include <windows.h>
#include <stdio.h>
#include "host.h"
#include "render.h"
#include "winshot.h"
#include "host.h"
#include "render.h"

static HWND g_hwnd;

/* ------------------------------------------------------------- keyboard */

/* Keys the BIOS reports with an 0xE0 prefix (arrows, editing keypad). The
 * game compares scan codes coming out of int 16h, but keeping the buffer
 * faithful to the BIOS costs nothing and avoids surprises. */
static int is_extended_key(UINT vk)
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

/* The ascii byte the BIOS would place in the keyboard buffer for this key:
 * 0xE0 for extended keys, the translated character otherwise (0 when the key
 * does not produce one, e.g. F-keys or with the wrong modifier state). */
static uint8_t kbd_ascii_for(WPARAM w, LPARAM l)
{
    BYTE ks[256];
    WORD ch = 0;
    UINT sc;

    if (is_extended_key((UINT)w))
        return 0xE0;
    if (!GetKeyboardState(ks))
        return 0;
    sc = (UINT)(((UINT_PTR)l >> 16) & 0xFF);
    if (ToAscii((UINT)w, sc, ks, &ch, 0) == 1 && ch < 0x100)
        return (uint8_t)ch;
    return 0;
}

/* --autokey: inject a keystroke by posting it to our own window. Only the
 * entry layer can do that because only it owns the window. */
void input_post_vk(int vk)
{
    UINT sc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
    LPARAM lp = (LPARAM)((sc << 16) | 1);

    printf("host: autokey vk=%02X (scan %02X)\n", vk, (unsigned)sc);
    PostMessageA(g_hwnd, WM_KEYDOWN, (WPARAM)vk, lp);
    PostMessageA(g_hwnd, WM_KEYUP, (WPARAM)vk, lp | 0xC0000000);
}

/* ------------------------------------------------------------------ window */

/* One frame + the optional window capture for backend verification. */
static void frame_or_shot(HWND h)
{
    if (host_frame()) {
        const char *shot = host_window_shot_path();
        if (shot)
            winshot_capture(h, shot);
    }
}

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CLOSE:
        host_request_quit();
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        uint8_t scan = (uint8_t)MapVirtualKeyA((UINT)w, MAPVK_VK_TO_VSC);
        host_key(scan, kbd_ascii_for(w, l));
        if (w == VK_ESCAPE && (GetKeyState(VK_CONTROL) & 0x8000)) {
            host_request_quit();
            PostQuitMessage(0);
        }
        return 0;
    }

    case WM_KEYUP:
    case WM_SYSKEYUP: {
        uint8_t scan = (uint8_t)MapVirtualKeyA((UINT)w, MAPVK_VK_TO_VSC);
        host_key((uint8_t)(scan | 0x80), kbd_ascii_for(w, l));
        return 0;
    }

    case WM_TIMER:
        if (host_wants_frames())
            frame_or_shot(h);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        frame_or_shot(h);
        EndPaint(h, &ps);
        return 0;
        }

    case WM_SIZE:
        render_resize(LOWORD(l), HIWORD(l));
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

static int create_window(void)
{
    WNDCLASSA wc;
    render_desc d = host_render_desc();
    RECT r = { 0, 0, d.logical_w * d.scale, d.logical_h * d.scale };

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc   = wndproc;
    wc.hInstance     = GetModuleHandleA(NULL);
    wc.lpszClassName = "FD2NATIVE";
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClassA(&wc)) return -1;

    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowA("FD2NATIVE", "FlameDragon2 - native host (POC)",
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           r.right - r.left, r.bottom - r.top,
                           NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!g_hwnd) return -1;
    ShowWindow(g_hwnd, SW_SHOW);
    SetTimer(g_hwnd, 1, 20, NULL);
    return 0;
}

/* ------------------------------------------------------------------ main */

int main(int argc, char **argv)
{
    MSG msg;
    render_desc d;

    if (host_init(argc, argv) != 0)
        return 1;

    if (create_window() != 0) {
        fprintf(stderr, "host: cannot create window: %lu\n", GetLastError());
        return 1;
    }

    d = host_render_desc();
    d.native_window = g_hwnd;
    if (render_init(&d) != 0) {
        fprintf(stderr, "host: render init failed\n");
        return 1;
    }
    printf("host: render backend = %s\n", render_name());

    if (host_start() != 0)
        return 1;

    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    render_shutdown();
    host_shutdown();
    return 0;
}

/* The process entry point lives in entry.c (shared by both entry layers) so
 * that the address-space reservation runs before CRT initialisation no matter
 * which backend is linked. */

