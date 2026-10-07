/* sokol_impl.c - the single translation unit that instantiates the sokol
 * implementation (PROGRESS.md §13.1).
 *
 * Exactly one .c may define SOKOL_IMPL; every other file includes the same
 * headers *without* it. SOKOL_NO_ENTRY keeps sokol_app from hijacking main()
 * so that fd2_entry (entry.c) can reserve the address space first and the
 * entry layer stays in charge of startup order.
 *
 * The graphics backend is picked at compile time - that is the whole point
 * of "one codebase, per-OS backends": Windows gets D3D11 (system built-in),
 * macOS Metal, Linux GL. */

#define SOKOL_NO_ENTRY
#define SOKOL_IMPL

/* A build system may already have picked the backend (-DSOKOL_GLCORE on
 * Linux, see Makefile.linux); only fill in a default when it did not. */
#if defined(SOKOL_D3D11) || defined(SOKOL_METAL) || defined(SOKOL_GLCORE) ||     defined(SOKOL_GLES3) || defined(SOKOL_VULKAN) || defined(SOKOL_WGPU)
    /* backend chosen by the build */
#elif defined(_WIN32)
    #define SOKOL_D3D11
#elif defined(__APPLE__)
    #define SOKOL_METAL
#else
    #define SOKOL_GLCORE
#endif

#include "sokol_log.h"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_time.h"
