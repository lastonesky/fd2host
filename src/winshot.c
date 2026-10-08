/* winshot.c - window content capture (Win32).
 *
 * Two methods, tried in order:
 *
 *   1. BitBlt of the client area from the screen DC - exact and
 *      DPI-agnostic (origin from ClientToScreen). Needs the window on top,
 *      which is why it is brought to the foreground first.
 *   2. PrintWindow(PW_CLIENTONLY|PW_RENDERFULLCONTENT) - works even when the
 *      window is occluded, but sokol's DPI-aware window ignores PW_CLIENTONLY
 *      and hands back the whole window frame, which shifted the content and
 *      cost a 10.9% pixel mismatch before.
 *
 * The result is written as a 32bpp BMP, the same layout --screenshot uses,
 * which makes the two directly comparable. */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "winshot.h"

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif
#ifndef PW_CLIENTONLY
#define PW_CLIENTONLY        0x00000001
#endif

static int write_bmp(const char *path, const uint32_t *bgra, int w, int h)
{
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    DWORD pix = (DWORD)(w * h * 4);
    FILE *f = fopen(path, "wb");

    if (!f) return -1;
    memset(&fh, 0, sizeof fh);
    memset(&ih, 0, sizeof ih);
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof fh + sizeof ih;
    fh.bfSize = fh.bfOffBits + pix;
    ih.biSize = sizeof ih;
    ih.biWidth = w;
    ih.biHeight = -h;                 /* top-down, like the pixel buffer */
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = pix;
    fwrite(&fh, sizeof fh, 1, f);
    fwrite(&ih, sizeof ih, 1, f);
    fwrite(bgra, pix, 1, f);
    fclose(f);
    return 0;
}

/* A capture that came back uniformly black is a failed capture, not a black
 * frame: sample every 64th pixel and require some non-zero byte. */
static int is_blank(const uint32_t *bgra, int n)
{
    int i;
    for (i = 0; i < n; i += 16)
        if ((bgra[i] & 0x00FFFFFFu) != 0)
            return 0;
    return 1;
}

int winshot_capture(void *hwndv, const char *path)
{
    HWND hwnd = (HWND)hwndv;
    RECT rc;
    int w, h;
    HDC memdc = NULL, srcdc = NULL;
    HBITMAP bmp = NULL;
    BITMAPINFO bi;
    void *bits = NULL;
    const char *method = NULL;

    if (!hwnd || !path)
        return -1;
    if (!GetClientRect(hwnd, &rc))
        return -1;
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
        return -1;

    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    memdc = CreateCompatibleDC(NULL);
    bmp   = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!memdc || !bmp || !bits) {
        printf("host: window shot: CreateDIBSection failed (%lu)\n", GetLastError());
        goto fail;
    }
    SelectObject(memdc, bmp);

    /* Bring the window above everything for the capture. HWND_TOPMOST works
     * without foreground rights - SetForegroundWindow() is refused whenever
     * another process owns the foreground (which is the normal case when a
     * human is at the machine: the first BitBlt attempt captured the user's
     * editor instead of the game). */
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

    {
        /* client area origin in screen coordinates (rc still holds the
         * client rect here, so it must not be overwritten) */
        POINT origin;
        origin.x = 0;
        origin.y = 0;
        ClientToScreen(hwnd, &origin);
        srcdc = GetDC(NULL);
        if (srcdc &&
            BitBlt(memdc, 0, 0, w, h, srcdc, origin.x, origin.y, SRCCOPY) &&
            !is_blank(bits, w * h))
            method = "screen BitBlt (topmost)";
    }

    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

    /* PW_CLIENTONLY is essential - without it PrintWindow returns the whole
     * window (title bar + borders) into a client-sized DC. */
    if (!method && PrintWindow(hwnd, memdc,
                               PW_CLIENTONLY | PW_RENDERFULLCONTENT) &&
        !is_blank(bits, w * h))
        method = "PrintWindow(client)";

    if (!method) {
        printf("host: window shot: both capture methods came back blank (%dx%d)\n", w, h);
        goto fail;
    }

    if (write_bmp(path, bits, w, h) != 0) {
        printf("host: window shot: cannot write %s\n", path);
        goto fail;
    }
    printf("host: window shot via %s -> %s (%dx%d)\n", method, path, w, h);

    if (srcdc)   ReleaseDC(NULL, srcdc);
    if (memdc)   DeleteDC(memdc);
    if (bmp)     DeleteObject(bmp);
    return 0;

fail:
    if (srcdc) ReleaseDC(NULL, srcdc);
    if (memdc) DeleteDC(memdc);
    if (bmp)   DeleteObject(bmp);
    return -1;
}
