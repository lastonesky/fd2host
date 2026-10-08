/* host.c - FD2 native Windows host (route C, milestone: run the original
 *          32-bit DOS/4GW code natively and show its frame buffer).
 *
 * What this is
 * ------------
 * The original x86 game code is executed *as is* inside a 32-bit Win32
 * process. Nothing emulates a DOS machine: there is no real-mode CPU, no
 * interrupt controller, no DOS. Only the interfaces the game touches are
 * replaced:
 *
 *     int 21h/31h/2f/10h/33h  ->  Win32 calls          (src/dos.c)
 *     in/out port I/O         ->  palette + no-ops     (src/dos.c)
 *     0xA0000 frame buffer    ->  real memory, presented by the sokol backend
 *     BIOS keyboard buffer    ->  Win32 keyboard messages
 *
 * Layout (all reserved before anything else allocates, see le.c):
 *     0x00010000..0x0004EF28   object 1 (game code + data)
 *     0x00050000..0x000556AF   object 2 (data)
 *     0x00060000..0x000634D1   object 3 (data)
 *     0x00070000..0x0007FFFF   low-memory mirror (BDA, PSP, IVT image)
 *     0x00080000..0x0009FFFF   real-mode pool for INT31 0100 (dos_init_lowmem)
 *     0x000A0000..0x000BFFFF   VGA frame buffer
 *     0x000C0000..0x000FFFFF   ROM area on real hardware - committed writable
 *                              so overruns off the VGA window do not fault
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"
#include "le.h"
#include "dos.h"
#include "ail.h"
#include "xmidi.h"
#include "synth.h"
#include "audio.h"
#include "keylog.h"
#include "render.h"
#include "host.h"

/* ------------------------------------------------------------------ state */

static le_image  g_le;
static int       g_scale = 3;
static int       g_show_frame = 1;

static uint32_t  g_rgb[320 * 200];
static volatile int g_running  = 1;
static volatile int g_use_image;         /* load pre-relocated images     */
static volatile int g_frames;

/* Optional frame capture (--screenshot=<file.bmp> [--shot-frame=<n>]):
 * writes exactly the pixels handed to the render backend, so palette and
 * channel-order regressions can be checked without a desktop or a visible
 * window.
 *
 * Two triggers, pick one:
 *   --shot-frame=<n>  fire on frame number n (backend-dependent: the same n
 *                     is a *different instant* at 32 fps and at 155 fps)
 *   --shot-time=<ms>  fire on the first frame whose wall-clock age since
 *                     host_init is >= ms (backend-independent, see below)
 *
 * --shot-time exists because the guest's own clock is real time: the BIOS
 * tick at 0x40:0x6C is advanced by an independent 18.2 Hz thread
 * (dos.c bios_tick_thread) and --autokey schedules on Sleep(ms), so wall
 * clock - not frame count - is the axis both backends and both runs share.
 * A higher frame rate then only makes the sample *finer* (155 fps samples
 * every ~6 ms, 32 fps every ~31 ms), which is why sokol is the better test
 * platform once the trigger is time-based (docs/BACKEND.md §13.7).
 *
 * --shot-tick goes one step further and fires on the guest's own tick
 * counter, which removes even that residual: with --shot-time a 32 fps
 * backend can overshoot the requested instant by a whole frame period
 * (~31 ms) and land one animation step ahead of a 158 fps backend asked for
 * the same millisecond. Same tick counter = same guest state, whatever the
 * frame rate. This is the trigger to use for cross-backend comparison. */
static const char  *g_screenshot_path;
static int          g_screenshot_frame = 300;
static int          g_shot_time_ms;      /* 0 = unused, frame trigger wins  */
static int          g_shot_tick;         /* 0 = unused; beats both above    */
static int          g_screenshot_done;
static uint64_t     g_shot_at_ms;        /* age of the captured frame       */
static uint32_t     g_shot_at_tick;      /* guest tick of the captured frame*/
static const char  *g_wshot_path;      /* --wshot=<bmp>: window capture */

/* AIL replacement layer knobs (see src/ail.c). The game never calls
 * AIL_set_sample_type / _playback_rate, so its samples rely on AIL's defaults:
 * 8-bit unsigned mono at 11025 Hz. These let that assumption be corrected from
 * the command line without a rebuild. */
static const char  *g_ail_dump_dir;
static int          g_ail_mode;          /* 0=auto 1=fd2 (force) 2=none */
static uint32_t     g_ail_rate  = 11025;
static int          g_ail_bits  = 8;
static int          g_ail_stereo;
/* Output volume, --volume=0..100. Default 100 = the game's own level (no
 * attenuation), i.e. what it sounded like before --volume existed. Debugging
 * runs pass --volume=10 explicitly (regress.ps1 does) so a test can run next
 * to a person; 0 still exercises the whole pipeline, just silently. */
static int          g_volume = 100;
/* --no-user-input: the real keyboard is ignored; only --autokey delivers
 * keystrokes. Tests run next to a person using the same machine, and a stray
 * key press in the focused game window goes straight into the BDA ring - that
 * desynchronised the autokey schedule (PROGRESS.md §31.5). */
static int          g_no_user_input;
static int          g_midi_rate;       /* XMIDI ticks/s; 0 = follow tempo */
static int          g_midi_test;       /* --midi-test: play a test tone   */
static int          g_midi_backend = 1; /* 1 = built-in synth (default)    */
static const char  *g_gm_bank;          /* --gm-bank=<path>                */
static const char  *g_autokey;          /* --autokey=<schedule>            */
static const char  *g_midi_dump;        /* --midi-dump=<file.wav>          */
static int          g_audio_rate = 22050; /* --audio-rate=<Hz> (mixer rate) */
static const char  *g_audio_dump;       /* --audio-dump=<file.wav>         */
static const char  *g_keylog_path;      /* --keylog=<path>                 */
static const char  *g_keyplay;          /* --keyplay=<path> (src/keylog.c)  */
static const char  *g_cmdtail;          /* --cmdtail=<tail> -> PSP:0x80    */
static int          g_exit_after_secs;  /* --exit-after, for spawned children */
static char         g_exit_when_path[PLAT_MAX_PATH]; /* --exit-when-file=path:minbytes */
static uint32_t     g_exit_when_size;   /* required size of that file      */
static volatile int g_autokey_done = 1; /* 0 while a --autokey schedule runs */
static uint64_t     g_start_tick;       /* when host_init began             */

/* 1 when the real keyboard must be ignored (see the flag above). The entry
 * layers ask this on every key event; --autokey never does - its keystrokes
 * are delivered through the same accessors the entry layer owns. */
int host_no_user_input(void)
{
    return g_no_user_input;
}

/* Seconds left on --exit-after (0 = unlimited). INT 21h AH=4B hands this to
 * the child so a bounded run stays bounded all the way down the process
 * tree - otherwise the parent's watchdog fires while it waits and leaves an
 * orphaned game running. */
int host_exit_after_remaining(void)
{
    uint64_t elapsed;

    if (g_exit_after_secs <= 0)
        return 0;
    elapsed = (plat_now_ms() - g_start_tick) / 1000;
    if ((uint64_t)g_exit_after_secs <= elapsed)
        return 0;
    return g_exit_after_secs - (int)elapsed;
}

/* ---------------------------------------------------- automated keystrokes
 *
 * The interesting failure ("continue" exits the game) only happens deep inside
 * the menus, which a headless 30-second run never reaches. --autokey posts a
 * schedule of keys to the game window so that path can be reproduced and
 * regression-tested from a script:
 *
 *     --autokey="1500:SPACE;1000:RETURN;800:DOWN,RETURN"
 *
 * fields are separated by ';': <delay in ms before this step>:<key,key,...>.
 * Key names are the portable ones in src/keys.h (RETURN ESC SPACE TAB UP DOWN
 * LEFT RIGHT PGUP PGDN INS DEL F1..F12, letters/digits, KP0..KP9, ...), looked
 * up by fr_key_by_name() so --autokey and --keylog share one vocabulary. */

/* post_key() moved to the entry layer as input_post_key(): it owns the window
 * and the platform key translation (see src/host.h). */

static void autokey_thread(void *param)
{
    char *spec = plat_strdup((const char *)param);
    char *step = spec, *next;

    (void)param;
    while (step && *step) {
        char *colon = strchr(step, ':');
        int delay;
        if (!colon) break;
        *colon = 0;
        delay = atoi(step);
        if (delay > 0) plat_sleep_ms((unsigned)delay);
        next = strchr(colon + 1, ';');
        if (next) *next++ = 0;
        {
            char *k = colon + 1;
            while (k && *k) {
                char *comma = strchr(k, ',');
                fr_key key;
                if (comma) *comma = 0;
                key = fr_key_by_name(k, strlen(k));
                if (key) input_post_key(key);
                else    printf("host: autokey: unknown key '%s'\n", k);
                k = comma ? comma + 1 : NULL;
            }
        }
        step = next;
    }
    printf("host: autokey schedule finished\n");
    g_autokey_done = 1;
    free(spec);
}

/* ------------------------------------------------------------------ utils */

/* Last path separator, either style: the host log is written next to the
 * executable and the separator differs per platform ('\\' vs '/'). */
static const char *path_last_sep(const char *p)
{
    const char *fwd = strrchr(p, '/');
    const char *bck = strrchr(p, '\\');

    if (!fwd) return bck;
    if (!bck) return fwd;
    return (fwd > bck) ? fwd : bck;
}

/* `dir` + `name` with exactly one separator (both styles accepted on input).
 * The result is always NUL-terminated; an over-long input is truncated. */
static void path_join(char *out, size_t n, const char *dir, const char *name)
{
    size_t d = strlen(dir), m = strlen(name), pos = 0;

    if (d > n - 1) d = n ? n - 1 : 0;
    memcpy(out, dir, d);
    pos = d;
    if (d && dir[d - 1] != '/' && dir[d - 1] != '\\' && pos + 1 < n)
        out[pos++] = '/';
    if (m > n - 1 - pos) m = (n - 1 > pos) ? n - 1 - pos : 0;
    memcpy(out + pos, name, m);
    out[pos + m] = 0;
}

/* Directory part of `p`, no trailing separator (empty when p has none).
 * Used to derive the game directory from --exe. */
static void path_dirname(const char *p, char *out, size_t n)
{
    const char *sep = path_last_sep(p);
    size_t len = sep ? (size_t)(sep - p) : 0;

    if (len > n - 1) len = n ? n - 1 : 0;
    memcpy(out, p, len);
    out[len] = 0;
}

/* --------------------------------------------------------------- shutdown
 *
 * The watchdog thread ends a bounded run. Two triggers share one clean
 * shutdown path:
 *
 *   --exit-after=N          hard deadline (original behaviour, same log
 *                           line, so existing checks keep working);
 *   --exit-when-file=P:S    stop as soon as file P holds >= S bytes AND the
 *                           --autokey schedule (if any) has finished, then
 *                           settle EXIT_SETTLE_MS so the last keystrokes
 *                           still play out. Measured on the regress run:
 *                           FD2.TMP is full at ~12 s of a 60 s run - without
 *                           this the host burned ~48 s of static frames and
 *                           the script slept another 15 s past process exit.
 *
 * A requested --screenshot is taken on the very last frame (if the regular
 * --shot-frame has not fired yet), so an early exit still leaves visual
 * evidence. */
#define EXIT_SETTLE_MS 2000

static int exit_file_ok(void)
{
    uint64_t size;

    if (!g_exit_when_path[0])
        return 0;                       /* no file condition: never "ready" */
    /* plat_path_size uses directory metadata: it never fails with a sharing
     * violation while the game still has the file open for writing. */
    if (plat_path_size(g_exit_when_path, &size) != 0)
        return 0;                       /* not created yet */
    return size >= (uint64_t)g_exit_when_size;
}

static void watchdog(void *param)
{
    uint64_t settle_at = 0;

    (void)param;
    for (;;) {
        uint64_t elapsed = plat_now_ms() - g_start_tick;

        if (g_exit_after_secs > 0 &&
            elapsed >= (uint64_t)g_exit_after_secs * 1000) {
            printf("host: watchdog fired after %d s (%d frames drawn)"
                   " - %.1f fps\n",
                   g_exit_after_secs, g_frames,
                   g_frames * 1000.0 / (double)(elapsed ? elapsed : 1));
            break;
        }
        if (g_autokey_done && !keylog_replaying() && exit_file_ok()) {
            if (!settle_at) {
                settle_at = plat_now_ms();
                printf("host: exit condition reached (file >= %u bytes, "
                       "autokey done) - settling %u ms\n",
                       g_exit_when_size, (unsigned)EXIT_SETTLE_MS);
            } else if (plat_now_ms() - settle_at >= EXIT_SETTLE_MS) {
                if (g_screenshot_path && !g_screenshot_done) {
                    /* last frame = evidence: drop whichever trigger has not
                     * fired yet and take the very next frame */
                    g_shot_time_ms = 0;
                    g_shot_tick = 0;
                    g_screenshot_frame = g_frames + 1;
                    plat_sleep_ms(250);                /* let it be drawn */
                }
                printf("host: exit condition met after %u s (%d frames drawn)\n",
                       (unsigned)(elapsed / 1000), g_frames);
                break;
            }
        } else {
            settle_at = 0;              /* condition lost: restart the settle */
        }
        plat_sleep_ms(200);
    }
    dos_terminate_child();        /* a P_WAIT child must not outlive us */
    keylog_finish();              /* both shutdown paths record the keys  */
    audio_close();                /* the watchdog exits straight from here:
                                   * without this the --audio-dump header
                                   * would never get its final sizes */
    dos_dump_stats();
    plat_exit(0);
}

static uint8_t *lowmem(void) { return (uint8_t *)(uintptr_t)DOS_LOWMEM_BASE; }
static uint8_t *vga(void)    { return (uint8_t *)(uintptr_t)DOS_VGA_BASE; }

/* ------------------------------------------------------------- rendering */

static void palette_default(void)
{
    /* If the game never touches the DAC (e.g. it inherits the BIOS palette),
     * a 3-3-2 ramp is a reasonable stand-in so the screen is not black. */
    int i;
    for (i = 0; i < 256; i++) {
        dos_palette[i * 3 + 0] = (uint8_t)(((i >> 5) & 7) * 255 / 7);
        dos_palette[i * 3 + 1] = (uint8_t)(((i >> 2) & 7) * 255 / 7);
        dos_palette[i * 3 + 2] = (uint8_t)((i & 3) * 255 / 3);
    }
    dos_palette_dirty = 1;
}

/* Write the current 32bpp frame as a BMP. BI_RGB 32bpp is BGRA in memory,
 * which is exactly how g_rgb is packed, so the pixels can be written verbatim.
 * The 54-byte header is emitted field by field (little-endian) so this file
 * needs no Win32 BITMAPFILEHEADER/BITMAPINFOHEADER types. */
static void put_le16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void dump_frame_bmp(const char *path)
{
    uint8_t  hdr[54];
    uint32_t pix = 320u * 200u * 4u;
    FILE *f = fopen(path, "wb");

    if (!f) {
        printf("host: cannot write frame dump %s\n", path);
        return;
    }
    memset(hdr, 0, sizeof hdr);
    hdr[0] = 'B';
    hdr[1] = 'M';
    put_le32(hdr + 2, 54u + pix);       /* bfSize      */
    put_le32(hdr + 10, 54u);            /* bfOffBits   */
    put_le32(hdr + 14, 40u);            /* biSize      */
    put_le32(hdr + 18, 320u);
    put_le32(hdr + 22, (uint32_t)-200); /* top-down: same order as g_rgb */
    put_le16(hdr + 26, 1u);             /* biPlanes    */
    put_le16(hdr + 28, 32u);            /* biBitCount  */
    put_le32(hdr + 34, pix);            /* biSizeImage */
    fwrite(hdr, 1, sizeof hdr, f);
    fwrite(g_rgb, 1, pix, f);
    fclose(f);
    printf("host: frame %d dumped to %s (age %lu ms, guest tick %lu)\n",
           g_frames, path, (unsigned long)g_shot_at_ms,
           (unsigned long)g_shot_at_tick);
}

/* Milliseconds since host_init: the shared clock for --shot-time, the
 * watchdog and --autokey alike. */
unsigned long host_age_ms(void)
{
    return (unsigned long)(plat_now_ms() - g_start_tick);
}

/* The guest's own clock: the BIOS tick counter at 0x40:0x6C that the game
 * polls for all of its timing. An independent 18.2 Hz host thread advances
 * it (dos.c bios_tick_thread), so - unlike the frame counter - it does not
 * care how often the backend draws. */
uint32_t host_guest_tick(void)
{
    volatile uint32_t *t = (volatile uint32_t *)(void *)(lowmem() + 0x46C);
    return *t;
}

/* Should this frame be captured?  See the comment on g_shot_time_ms: the
 * tick trigger is the backend-independent one, time is the coarser variant
 * and the frame trigger is kept for the existing scripts. */
static int host_shot_due(void)
{
    if (g_shot_tick > 0)
        return (int32_t)host_guest_tick() >= g_shot_tick;
    if (g_shot_time_ms > 0)
        return (int)host_age_ms() >= g_shot_time_ms;
    return g_frames >= g_screenshot_frame;
}

/* One frame: guest framebuffer -> 32bpp BGRA -> backend.
 *
 * The palette conversion and the --screenshot dump deliberately stay in this
 * shared layer: every render backend receives exactly the same pixels, so
 * two backends can be diffed byte for byte (PROGRESS.md §13.6). */
int host_frame(void)
{
    const uint8_t *fb = vga();
    int i, shot = 0;

    if (dos_palette_dirty) {
        /* nothing cached: we translate every frame (320x200 is cheap) */
        dos_palette_dirty = 0;
    }
    for (i = 0; i < 320 * 200; i++) {
        uint8_t idx = fb[i];
        const uint8_t *c = &dos_palette[idx * 3];
        /* 32bpp BI_RGB DIBs are stored as BGRA, so byte0 = blue, byte1 =
         * green, byte2 = red. Getting this backwards swaps red and blue:
         * a yellow (R+G) pixel then shows up as cyan/blue, which is exactly
         * how the first POC looked. dos_palette already holds 8-bit per
         * channel values (the 6-bit DAC values are stretched on capture). */
        g_rgb[i] = ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | c[2];
    }

    render_present(g_rgb, 320, 200);

    if (!g_screenshot_done && host_shot_due()) {
        g_screenshot_done = 1;
        g_shot_at_ms = plat_now_ms() - g_start_tick;
        g_shot_at_tick = host_guest_tick();
        shot = 1;
        if (g_screenshot_path)
            dump_frame_bmp(g_screenshot_path);
    }
    g_frames++;

    /* the entry layer captures the window right after present() on this very
     * frame, so --wshot and --screenshot describe the same instant */
    return shot;
}

const char *host_window_shot_path(void)
{
    return g_wshot_path;
}

/* ---------------------------------------------------------------- keyboard */

/* is_extended_key() and kbd_ascii_for() (Win32 -> BIOS translation) moved to
 * main_sokol.c together with the render loop. */

/* One keystroke for the BIOS keyboard buffer.  The platform-specific parts
 * (scan code translation, extended-key detection) live in main_sokol.c; this
 * side only writes the BDA ring buffer, which is the same work any other
 * input backend would do. */
/* Where the last make code's ascii byte went, so a late CHAR event can patch
 * it (host_key_set_last_ascii). 0xFFFF = nothing patchable. */
static uint16_t g_last_ascii_tail = 0xFFFF;

void host_key(uint8_t scan, uint8_t ascii)
{
    uint8_t *lm;
    uint16_t tail, head, next;

    g_last_ascii_tail = 0xFFFF;

    /* Record every make code before anything else: this is the one funnel
     * both backends and both the real keyboard and --autokey go through. */
    keylog_note(scan, ascii);

    /* A game that replaced INT 9 owns the key queue: in the original the
     * BIOS handler that fills the ring at 0x41E is *not* chained to, so
     * feeding both paths hands the key over twice (FDPS then walks its menu
     * twice per press and jumps through a table it has not filled -
     * PROGRESS.md §18). */
    if (dos_deliver_key(scan))
        return;

    lm = lowmem();
    tail = (uint16_t)(lm[0x41C] | (lm[0x41D] << 8));
    head = (uint16_t)(lm[0x41A] | (lm[0x41B] << 8));
    next = (uint16_t)(tail + 2);

    if (next >= 0x43E) next = 0x41E;
    if (next == head) return;                 /* buffer full */
    lm[tail]     = ascii;
    lm[tail + 1] = scan;
    lm[0x41C] = (uint8_t)(next & 0xFF);
    lm[0x41D] = (uint8_t)(next >> 8);
    if (!(scan & 0x80))
        g_last_ascii_tail = tail;             /* a make code: char may patch */
}

void host_key_set_last_ascii(uint8_t ascii)
{
    uint8_t *lm;
    uint16_t head;

    if (g_last_ascii_tail == 0xFFFF)
        return;
    lm = lowmem();
    head = (uint16_t)(lm[0x41A] | (lm[0x41B] << 8));
    if (head == g_last_ascii_tail)            /* still unconsumed */
        lm[g_last_ascii_tail] = ascii;
    g_last_ascii_tail = 0xFFFF;
}

/* ------------------------------------------------------------- game thread */

/* The guest entry is a plain cdecl function; PLAT_CDECL is defined in
 * platform.h (no-op outside MSVC/x86). */
typedef void (PLAT_CDECL *game_entry_fn)(void);

static void game_thread(void *param)
{
    game_entry_fn fn = (game_entry_fn)(uintptr_t)g_le.entry_linear;
    (void)param;
    printf("host: entering game code at 0x%X\n", g_le.entry_linear);
    fflush(stdout);
    fn();
    printf("host: game entry returned\n");
    g_running = 0;
}

/* window creation and the render loop live in main_sokol.c, which calls
 * back into host_frame() on every frame. */

/* ------------------------------------------------------------------- main */

/* ------------------------------------------------------- command line parsing
 *
 * Every option accepts both `--opt value` and `--opt=value`. Only the `=`
 * form used to work for most options, and the space form then fell back to
 * the default *silently* - a whole contrast run went against the wrong
 * directory that way. Normalize first so the parser below only has to deal
 * with `--opt=value`. */
static int opt_wants_value(const char *a)
{
    static const char *opts[] = {
        "--exe", "--gamedir", "--exit-after", "--trace", "--screenshot",
        "--wshot", "--shot-frame", "--shot-time", "--shot-tick", "--ail", "--ail-dump", "--ail-rate", "--ail-bits",
        "--midi-rate", "--midi-backend", "--gm-bank", "--autokey",
        "--midi-dump", "--cmdtail", "--log", "--exit-when-file",
        "--volume", "--keylog", "--keyplay", "--audio-rate", "--audio-dump"
    };
    size_t i;
    for (i = 0; i < sizeof opts / sizeof opts[0]; i++)
        if (!strcmp(a, opts[i])) return 1;
    return 0;
}

/* ---------------------------------------------------------- kernel bring-up
 *
 * The process entry point, the window and the render loop live in
 * main_sokol.c; this file keeps only the backend-independent kernel so a
 * different entry layer could drive exactly the same code. */
int host_init(int argc, char **argv)
{
    static char  merged[48][512];
    static char *av[64];
    int fixups = 0, i, ac = 0;
    int exe_given = 0, gamedir_given = 0;
    static char  selfdir[PLAT_MAX_PATH];   /* directory of this executable   */
    static char  exebuf[PLAT_MAX_PATH];    /* <gamedir>/FD2.EXE              */
    static char  dirbuf[PLAT_MAX_PATH];    /* dirname(--exe)                 */
    const char *exe = NULL;
    const char *gamedir = selfdir;
    char logpath[PLAT_MAX_PATH];

    g_start_tick = plat_now_ms();

    /* Defaults live next to *this* binary, never at a hard-coded E:\FD2: the
     * release ships one executable the user drops into the game directory, so
     * the game directory is simply wherever the host was launched from. */
    if (plat_module_path(selfdir, sizeof selfdir) != 0)
        selfdir[0] = 0;
    {
        char *slash = (char *)path_last_sep(selfdir);
        if (slash) *(slash + 1) = 0;       /* keep the separator: "dir/"     */
        else strcpy(selfdir, "./");        /* no directory part -> the cwd   */
    }

    /* Diagnostics first: this dumps everything to a file, never to a console
     * window. On failure the process exits silently so the user is not left
     * with a stray window. */
    if (plat_module_path(logpath, sizeof logpath) != 0)
        logpath[0] = 0;
    {
        char *slash = (char *)path_last_sep(logpath);
        if (slash) *(slash + 1) = 0; else logpath[0] = 0;
        strcat(logpath, "host.log");
    }
    /* --log=<path> has to be honoured *here*, before anything is written:
     * INT 21h AH=4B starts a child host and points it at its own file, since
     * freopen("w") on the parent's host.log would erase the parent's log.
     * The option normalizer runs after the redirect, so both spellings are
     * recognised manually. */
    for (i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "--log=", 6)) {
            strncpy(logpath, argv[i] + 6, sizeof logpath - 1);
            logpath[sizeof logpath - 1] = 0;
        } else if (!strcmp(argv[i], "--log") && i + 1 < argc) {
            strncpy(logpath, argv[i + 1], sizeof logpath - 1);
            logpath[sizeof logpath - 1] = 0;
        }
    }
    freopen(logpath, "w", stdout);
    {
        char errpath[PLAT_MAX_PATH];
        strcpy(errpath, logpath);
        { char *dot = strrchr(errpath, '.'); if (dot) strcpy(dot, ".err"); }
        freopen(errpath, "w", stderr);
    }
    setvbuf(stdout, NULL, _IONBF, 0);

    /* A WINDOWS-subsystem process has no console: fds 0/1/2 start out closed
     * and freopen() may land on any free fd. The guest writes its own printf()
     * through DOS handle 1, which the host derives from fd 1
     * (_get_osfhandle(1) in files_init) - without this remap every game
     * message went to an invalid handle (WriteFile error 6, 0 bytes written).
     * plat_stdio_pin() is a no-op on POSIX. */
    plat_stdio_pin();
    printf("FD2 native host - POC\n");
    printf("image base 0x%p\n", plat_image_base());

    /* rewrite `--opt value` into `--opt=value` (see opt_wants_value above).
     * av[0] must stay the program name: the parser below starts at i = 1, so
     * dropping it would silently skip the first merged option (the symptom:
     * --exit-after works while --gamedir falls back to the default). */
    av[0] = argv[0];
    ac = 1;
    for (i = 1; i < argc && ac < 63; i++) {
        if (argv[i][0] == '-' && argv[i][1] == '-' &&
            !strchr(argv[i], '=') && opt_wants_value(argv[i]) &&
            i + 1 < argc && ac < 47) {
            snprintf(merged[ac], sizeof merged[ac], "%s=%s", argv[i], argv[i + 1]);
            av[ac] = merged[ac];
            ac++;
            i++;                       /* consume the value token */
        } else {
            av[ac] = argv[i];
            ac++;
        }
    }
    av[ac] = NULL;
    argc = ac;
    argv = av;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--image")) g_use_image = 1;
        /* Both "--gamedir <dir>" and "--gamedir=<dir>" are accepted: an
         * "=" form that is silently ignored falls back to the default game
         * directory and the whole test run quietly tests the wrong thing. */
        else if (!strcmp(argv[i], "--exe") && i + 1 < argc) { exe = argv[++i]; exe_given = 1; }
        else if (!strncmp(argv[i], "--exe=", 6)) { exe = argv[i] + 6; exe_given = 1; }
        else if (!strcmp(argv[i], "--gamedir") && i + 1 < argc) { gamedir = argv[++i]; gamedir_given = 1; }
        else if (!strncmp(argv[i], "--gamedir=", 10)) { gamedir = argv[i] + 10; gamedir_given = 1; }
        else if (!strncmp(argv[i], "--exit-after=", 13)) {
            int secs = atoi(argv[i] + 13);
            if (secs > 0)
                g_exit_after_secs = secs;
        }
        /* --exit-when-file=<path>:<minbytes> - bounded run that ends as soon
         * as the tested path is complete instead of always burning the whole
         * --exit-after budget. The last ':' separates the size, so "E:\..."
         * keeps its own colon; without a numeric suffix the file just has to
         * exist (min 1 byte). */
        else if (!strncmp(argv[i], "--exit-when-file=", 17)) {
            const char *spec = argv[i] + 17;
            const char *colon = strrchr(spec, ':');
            size_t n = strlen(spec);
            if (n >= sizeof g_exit_when_path) n = sizeof g_exit_when_path - 1;
            memcpy(g_exit_when_path, spec, n);
            g_exit_when_path[n] = 0;
            g_exit_when_size = 1;
            if (colon && colon != spec + 1) {
                const char *q;
                int digits = colon[1] != 0;
                for (q = colon + 1; *q; q++)
                    if (*q < '0' || *q > '9') digits = 0;
                if (digits) {
                    g_exit_when_size = (uint32_t)strtoul(colon + 1, NULL, 10);
                    g_exit_when_path[colon - spec] = 0;
                }
            }
        }
        else if (!strncmp(argv[i], "--cmdtail=", 10)) {
            g_cmdtail = argv[i] + 10;
        }
        else if (!strncmp(argv[i], "--trace=", 8)) {
            dos_enable_trace(atoi(argv[i] + 8));
        }
        else if (!strcmp(argv[i], "--headless")) {
            g_show_frame = 0;
        }
        else if (!strncmp(argv[i], "--screenshot=", 13)) {
            g_screenshot_path = argv[i] + 13;
        }
        else if (!strncmp(argv[i], "--wshot=", 8)) {
            g_wshot_path = argv[i] + 8;
        }
        else if (!strncmp(argv[i], "--shot-frame=", 13)) {
            g_screenshot_frame = atoi(argv[i] + 13);
            g_shot_time_ms = 0;             /* last one wins */
            g_shot_tick = 0;
        }
        else if (!strncmp(argv[i], "--shot-time=", 12)) {
            g_shot_time_ms = atoi(argv[i] + 12);
            g_shot_tick = 0;
        }
        else if (!strncmp(argv[i], "--shot-tick=", 12)) {
            g_shot_tick = atoi(argv[i] + 12);
        }
        else if (!strncmp(argv[i], "--ail=", 6)) {
            const char *m = argv[i] + 6;
            g_ail_mode = !strcmp(m, "none") ? 2 : (!strcmp(m, "fd2") ? 1 : 0);
        }
        else if (!strncmp(argv[i], "--ail-dump=", 11)) {
            g_ail_dump_dir = argv[i] + 11;
        }
        else if (!strncmp(argv[i], "--ail-rate=", 11)) {
            g_ail_rate = (uint32_t)atoi(argv[i] + 11);
        }
        else if (!strncmp(argv[i], "--ail-bits=", 11)) {
            g_ail_bits = atoi(argv[i] + 11);
        }
        else if (!strcmp(argv[i], "--ail-stereo")) {
            g_ail_stereo = 1;
        }
        else if (!strncmp(argv[i], "--midi-rate=", 12)) {
            g_midi_rate = atoi(argv[i] + 12);
        }
        else if (!strcmp(argv[i], "--midi-test")) {
            g_midi_test = 1;
        }
        else if (!strncmp(argv[i], "--midi-backend=", 15)) {
            const char *b = argv[i] + 15;
            g_midi_backend = (!strcmp(b, "winmidi") || !strcmp(b, "0")) ? 0 : 1;
        }
        else if (!strncmp(argv[i], "--gm-bank=", 10)) {
            g_gm_bank = argv[i] + 10;
        }
        else if (!strncmp(argv[i], "--autokey=", 10)) {
            g_autokey = argv[i] + 10;
            if (g_autokey[0])
                g_autokey_done = 0;     /* the completion trigger must wait */
        }
        else if (!strncmp(argv[i], "--midi-dump=", 12)) {
            g_midi_dump = argv[i] + 12;
        }
        else if (!strncmp(argv[i], "--audio-rate=", 13)) {
            g_audio_rate = atoi(argv[i] + 13);
            if (g_audio_rate < 8000 || g_audio_rate > 192000)
                g_audio_rate = 22050;
        }
        else if (!strncmp(argv[i], "--audio-dump=", 13)) {
            g_audio_dump = argv[i] + 13;
        }
        else if (!strncmp(argv[i], "--keylog=", 9)) {
            g_keylog_path = argv[i] + 9;
        }
        else if (!strncmp(argv[i], "--keyplay=", 10)) {
            g_keyplay = argv[i] + 10;
        }
        else if (!strcmp(argv[i], "--volume") && i + 1 < argc) {
            g_volume = atoi(argv[++i]);
        }
        else if (!strncmp(argv[i], "--volume=", 9)) {
            g_volume = atoi(argv[i] + 9);
        }
        else if (!strcmp(argv[i], "--no-user-input")) {
            g_no_user_input = 1;
        }
    }

    /* Only one of the pair has to be given: --gamedir alone finds
     * <gamedir>/FD2.EXE, --exe alone uses that file's directory as the game
     * directory (the game opens its data by bare name from the cwd). With
     * neither, both default to the host's own directory. */
    if (!exe_given) {
        path_join(exebuf, sizeof exebuf, gamedir, "FD2.EXE");
        exe = exebuf;
    } else if (!gamedir_given) {
        path_dirname(exe, dirbuf, sizeof dirbuf);
        if (dirbuf[0])                     /* relative name -> keep default  */
            gamedir = dirbuf;
    }

    /* Must be the very first allocation: the CRT heap grows from 0x10000. */
    if (le_reserve_address_space() != 0) {
        printf("host: fixed address space unavailable - the object window "
               "0x10000..0x70000 is already taken (this image is at 0x%p). "
               "Retry; if it persists, rebuild with /BASE:0x60000000.",
               (void *)plat_image_base());
        printf("\n");
        return 9;
    }
    printf("host: address space reserved\n");

    /* All watchdog inputs are parsed now - start the exit watcher (deadline
     * and/or completion trigger). Started after the reservation so the
     * low-window reservation still wins the race. */
    if (g_exit_after_secs > 0 || g_exit_when_path[0])
        plat_thread(watchdog, NULL);

    /* The game opens its data files by bare name (DIG.INI, FDOTHER.DAT, ...),
     * so the working directory has to be the game directory. */
    if (plat_set_cwd(gamedir) != 0)
        printf("host: warning: cannot chdir to %s (%u)\n", gamedir, plat_error());
    else
        printf("host: working directory = %s\n", gamedir);

    if (le_open(&g_le, exe) != 0) { getchar(); return 1; }

    /* Where can the low-memory mirror go? FD2 keeps its objects below 0x70000
     * so the mirror sits at 0x70000; FDPS parks obj2 exactly there. Decide
     * from the parsed object table before anything is mapped. */
    {
        uint32_t k, end = 0;
        for (k = 0; k < g_le.object_count; k++) {
            uint32_t span = g_le.objects[k].page_count * LE_PAGE_SIZE;
            uint32_t e;
            if (span < g_le.objects[k].vsize)
                span = g_le.objects[k].vsize;
            e = g_le.objects[k].base + span;
            if (e > end) end = e;
        }
        dos_choose_lowmem(end);
    }

    if (g_use_image) {
        char imgpath[PLAT_MAX_PATH];
        path_join(imgpath, sizeof imgpath, selfdir, "objects.bin");
        if (le_map_flat(&g_le, imgpath) != 0)
            return 1;
    } else if (le_map_and_relocate(&g_le, &fixups) != 0) {
        return 1;
    }

    dos_init_lowmem();
    /* PSP:0x80 = the command tail. Empty for a plain run; a child spawned by
     * INT 21h AH=4B gets the arguments its parent was handed (FD.EXE expects
     * its video/audio config paths there). */
    dos_set_cmdtail(g_cmdtail);
    dos_set_image(&g_le);
    dos_install_traps();
    dos_patch_interrupts();
    dos_patch_lowmem_refs();
    palette_default();

    /* Replace the Miles AIL entry points the game uses with host
     * implementations before the game thread starts (the real AIL would try to
     * execute 16-bit real-mode drivers). */
    ail_set_format(g_ail_rate, g_ail_bits, g_ail_stereo);
    ail_set_master_volume(g_volume);   /* prints "ail: master output volume" */
    xmidi_set_tick_rate(g_midi_rate);
    xmidi_set_test(g_midi_test);
    xmidi_set_backend(g_midi_backend);
    synth_set_bank_path(g_gm_bank);
    synth_set_dump_path(g_midi_dump);
    /* The patch addresses are per-build: FD2's 52 and FDPS's 90 are two
     * different AIL linkings (re/RE_MAP.md §3 vs re/fdps_ail_patchset.csv).
     * On an unknown build they would point into unrelated code and corrupt it,
     * so the patch is gated: auto mode only patches a build we know, --ail=fd2
     * forces FD2's table, --ail=none disables patching. */
    if (g_ail_mode == 2) {
        printf("ail: patching disabled (--ail=none)\n");
    } else {
        const char *bn = path_last_sep(exe);
        bn = bn ? bn + 1 : exe;
        if (g_ail_mode == 1 || plat_stricmp(bn, "FD2.EXE") == 0)
            ail_install((uint8_t *)(uintptr_t)g_le.objects[0].base, g_ail_dump_dir);
        else if (plat_stricmp(bn, "FDPS.EXE") == 0)
            ail_install_fdps((uint8_t *)(uintptr_t)g_le.objects[0].base,
                             g_ail_dump_dir);
        else {
            printf("ail: '%s' has no AIL table - skipping the hard-coded "
                   "patches (original Miles code runs; --ail=fd2 forces "
                   "FD2's table)\n", bn);
        }
    }

    /* Keystroke recording / replay (src/keylog.c): open the recorder first so
     * even the keys pressed while the game is still starting are captured,
     * and fail the replay load here rather than half-way into a run. */
    keylog_init(g_keylog_path, g_keyplay, g_start_tick);

    /* One output device and one software mixer for music + sound effects
     * (docs/AUDIO.md §11.10). It has to exist before the game thread can
     * reach any AIL entry point; if there is no device the host still runs
     * and says so in the log. */
    if (audio_init((unsigned)g_audio_rate)) {
        if (g_audio_dump && g_audio_dump[0])
            audio_dump_open(g_audio_dump);
    }

    return 0;
}

render_desc host_render_desc(void)
{
    render_desc d;
    d.native_window = NULL;        /* the entry layer fills this in */
    d.logical_w     = 320;
    d.logical_h     = 200;
    d.scale         = g_scale;
    return d;
}

int host_start(void)
{
    if (plat_thread_stk(game_thread, NULL, 4u * 1024u * 1024u) != 0) {
        fprintf(stderr, "host: cannot start game thread\n");
        return -1;
    }
    printf("host: game thread started\n");

    if (keylog_start()) {               /* --keyplay wins over --autokey    */
        if (g_autokey && g_autokey[0])
            printf("host: --keyplay given, ignoring --autokey\n");
    } else if (g_autokey && g_autokey[0]) {
        printf("host: autokey schedule: %s\n", g_autokey);
        plat_thread(autokey_thread, (void *)g_autokey);
    }
    return 0;
}

int host_wants_frames(void)
{
    return g_show_frame;
}

void host_request_quit(void)
{
    g_running = 0;
}

void host_shutdown(void)
{
    printf("host: shutting down (%d frames drawn)\n", g_frames);
    keylog_finish();
    audio_close();
    dos_dump_stats();
}

/* The process entry point (fd2_entry) and the render loop live in
 * main_sokol.c - see the comment above host_init(). */

