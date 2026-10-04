/* render.h - backend-agnostic presentation seam.
 *
 * Everything above this interface works with plain pixels: the palette ->
 * 32bpp conversion and the --screenshot dump stay in host.c, so *every*
 * backend is judged by the exact same buffer (PROGRESS.md §13). A backend
 * only receives a 320x200 BGRA image plus the logical size and has to put it
 * on screen.
 *
 * Backends:
 *   render_gdi.c     current GDI StretchDIBits path (kept as the reference
 *                    implementation and the --render=gdi fallback)
 *   render_sokol.c   sokol_gfx (step 2): Win=D3D11, mac=Metal, Linux=GL
 *
 * The entry/input layer (main_win32.c, later main_sokol.c) owns the window;
 * it passes the native handle in render_desc.native_window. Backends that
 * create their own window simply ignore it. */
#ifndef FD2_RENDER_H
#define FD2_RENDER_H

#include <stdint.h>

typedef struct {
    void *native_window;  /* platform window handle; may be NULL           */
    int   logical_w;      /* guest framebuffer width  (320)                */
    int   logical_h;      /* guest framebuffer height (200)                */
    int   scale;          /* window = logical * scale; GDI stretches to it */
} render_desc;

/* Returns 0 on success. Must be called with the window already created. */
int render_init(const render_desc *desc);

/* Present one frame. `bgra` is logical_w * logical_h 32bpp pixels in
 * BI_RGB (BGRA) order, stride = w * 4, top-down. */
void render_present(const uint32_t *bgra, int w, int h);

/* Window/client size changed. Default: remember it (GDI ignores it). */
void render_resize(int w, int h);

void render_shutdown(void);

/* Short name for logs, e.g. "gdi" / "sokol". */
const char *render_name(void);

#endif /* FD2_RENDER_H */
