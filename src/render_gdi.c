/* render_gdi.c - the original presentation path, kept byte-for-byte.
 *
 * GDI StretchDIBits with software scaling: cheap enough for a 320x200 source
 * and, more importantly, it is the reference implementation - --render=gdi
 * must keep producing the exact same pixels that --screenshot dumps, so any
 * newer backend can be diffed against it (PROGRESS.md §13.6).
 *
 * Note the pixel contract: 32bpp BI_RGB DIBs are BGRA in memory, which is how
 * host.c packs g_rgb, so the buffer is handed over verbatim. */

#include <windows.h>
#include <string.h>
#include "render.h"

static HWND       g_wnd;
static int        g_scale = 3;
static int        g_logical_w = 320;
static int        g_logical_h = 200;
static int        g_client_w, g_client_h;   /* remembered for resize */
static BITMAPINFO g_bmi;

int render_init(const render_desc *desc)
{
    if (!desc || !desc->native_window)
        return -1;
    g_wnd       = (HWND)desc->native_window;
    g_scale     = desc->scale > 0 ? desc->scale : 3;
    g_logical_w = desc->logical_w > 0 ? desc->logical_w : 320;
    g_logical_h = desc->logical_h > 0 ? desc->logical_h : 200;
    g_client_w  = g_logical_w * g_scale;
    g_client_h  = g_logical_h * g_scale;

    memset(&g_bmi, 0, sizeof g_bmi);
    g_bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    g_bmi.bmiHeader.biWidth       = g_logical_w;
    g_bmi.bmiHeader.biHeight      = -g_logical_h;   /* top-down */
    g_bmi.bmiHeader.biPlanes      = 1;
    g_bmi.bmiHeader.biBitCount    = 32;
    g_bmi.bmiHeader.biCompression = BI_RGB;
    return 0;
}

void render_present(const uint32_t *bgra, int w, int h)
{
    HDC hdc;

    (void)h;
    if (!g_wnd || !bgra)
        return;
    hdc = GetDC(g_wnd);
    StretchDIBits(hdc, 0, 0, w * g_scale, h * g_scale,
                  0, 0, w, h, bgra, &g_bmi, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(g_wnd, hdc);
}

void render_resize(int w, int h)
{
    /* The GDI path draws at a fixed integer scale, so a resize only matters
     * for backends that scale freely. Remember it anyway. */
    if (w > 0 && h > 0) {
        g_client_w = w;
        g_client_h = h;
    }
}

void render_shutdown(void)
{
    g_wnd = NULL;
}

const char *render_name(void)
{
    return "gdi";
}
