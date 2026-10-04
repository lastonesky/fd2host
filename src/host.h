/* host.h - seam between the process entry/input layer and the host kernel.
 *
 *   host.c        kernel: argument parsing, LE load, DOS/AIL bring-up, the
 *                 game thread, palette -> BGRA conversion, screenshots,
 *                 watchdog/autokey.  Backend and OS independent (as far as
 *                 Win32 threads/stdio go; the POSIX port replaces those).
 *   main_win32.c  entry layer: process entry point, window, message pump,
 *                 Win32 keyboard translation, timer -> host_frame().
 *   main_sokol.c  (step 2) same contract driven by sokol_app callbacks.
 *
 * The contract an entry layer must fulfil:
 *   1. host_init()            - everything up to "ready for a window"
 *   2. create the window, call render_init(host_render_desc() + native handle)
 *   3. host_start()           - starts the game thread (+ autokey)
 *   4. pump frames            - call host_frame() on every tick/paint
 *   5. feed input             - host_key(scan, ascii) for each keystroke
 *   6. host_shutdown()        - statistics
 * and it must provide input_post_vk() so --autokey can inject keys.
 */
#ifndef FD2_HOST_H
#define FD2_HOST_H

#include <stdint.h>
#include "render.h"

/* ---- kernel (host.c) ---- */

/* Parses argv, redirects the log, reserves the address space, loads the LE
 * image, brings up DOS/AIL.  Returns 0 when a window can be created. */
int host_init(int argc, char **argv);

/* Logical framebuffer size and scale for window creation. */
render_desc host_render_desc(void);

/* Starts the game thread and the optional --autokey thread. */
int host_start(void);

/* Renders one frame: VGA buffer + palette -> BGRA -> render_present(),
 * then the --screenshot dump.  Safe to call from WM_TIMER and WM_PAINT. */
void host_frame(void);

/* 0 in --headless mode: the entry layer may skip timer-driven frames. */
int host_wants_frames(void);

/* One keystroke for the BIOS keyboard buffer.  `scan` already carries the
 * 0x80 break bit for key releases, `ascii` is 0xE0 for extended keys. */
void host_key(uint8_t scan, uint8_t ascii);

/* The game asked to stop (window close / Ctrl+Esc). */
void host_request_quit(void);

/* Logs the frame count and the interrupt statistics. */
void host_shutdown(void);

/* ---- entry/input layer (main_win32.c) ---- */

/* Injects a virtual-key keystroke into the window (used by --autokey).
 * Implemented by the entry layer because only it owns the window. */
void input_post_vk(int vk);

#endif /* FD2_HOST_H */
