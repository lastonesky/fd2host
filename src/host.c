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
 *     0xA0000 frame buffer    ->  real memory, drawn with GDI/DIB
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

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "dos.h"
#include "ail.h"
#include "xmidi.h"
#include "synth.h"

/* ------------------------------------------------------------------ state */

static le_image  g_le;
static HWND      g_hwnd;
static int       g_scale = 3;
static int       g_show_frame = 1;

static uint32_t  g_rgb[320 * 200];
static BITMAPINFO g_bmi;
static volatile int g_running  = 1;
static volatile int g_use_image;         /* load pre-relocated images     */
static volatile int g_frames;

/* Optional frame capture (--screenshot=<file.bmp> [--shot-frame=<n>]): writes
 * exactly the pixels handed to GDI, so palette and channel-order regressions
 * can be checked without a desktop or a visible window. */
static const char  *g_screenshot_path;
static int          g_screenshot_frame = 300;
static int          g_screenshot_done;

/* AIL replacement layer knobs (see src/ail.c). The game never calls
 * AIL_set_sample_type / _playback_rate, so its samples rely on AIL's defaults:
 * 8-bit unsigned mono at 11025 Hz. These let that assumption be corrected from
 * the command line without a rebuild. */
static const char  *g_ail_dump_dir;
static uint32_t     g_ail_rate  = 11025;
static int          g_ail_bits  = 8;
static int          g_ail_stereo;
static int          g_midi_rate;       /* XMIDI ticks/s; 0 = follow tempo */
static int          g_midi_test;       /* --midi-test: play a test tone   */
static int          g_midi_backend = 1; /* 1 = built-in synth (default)    */
static const char  *g_gm_bank;          /* --gm-bank=<path>                */
static const char  *g_autokey;          /* --autokey=<schedule>            */
static const char  *g_midi_dump;        /* --midi-dump=<file.wav>          */

/* ---------------------------------------------------- automated keystrokes
 *
 * The interesting failure ("continue" exits the game) only happens deep inside
 * the menus, which a headless 30-second run never reaches. --autokey posts a
 * schedule of keys to the game window so that path can be reproduced and
 * regression-tested from a script:
 *
 *     --autokey="1500:SPACE;1000:RETURN;800:DOWN,RETURN"
 *
 * fields are separated by ';': <delay in ms before this step>:<vk,vk,...>.
 * VK names: RETURN ESC SPACE TAB UP DOWN LEFT RIGHT and any letter/digit. */
static int vk_from_name(const char *s, size_t n)
{
    static const struct { const char *n; int vk; } tab[] = {
        { "RETURN", VK_RETURN }, { "ENTER", VK_RETURN }, { "ESC", VK_ESCAPE },
        { "SPACE", VK_SPACE },   { "TAB", VK_TAB },     { "UP", VK_UP },
        { "DOWN", VK_DOWN },     { "LEFT", VK_LEFT },   { "RIGHT", VK_RIGHT },
        { "HOME", VK_HOME },     { "END", VK_END },     { "PGUP", VK_PRIOR },
        { "PGDN", VK_NEXT },     { "INS", VK_INSERT },  { "DEL", VK_DELETE },
    };
    size_t i;
    if (!n) return 0;
    for (i = 0; i < sizeof tab / sizeof tab[0]; i++)
        if (strlen(tab[i].n) == n && !_strnicmp(tab[i].n, s, n))
            return tab[i].vk;
    if (n == 1) {
        if (s[0] >= '0' && s[0] <= '9') return s[0];
        if (s[0] >= 'a' && s[0] <= 'z') return s[0] - 'a' + 'A';
        if (s[0] >= 'A' && s[0] <= 'Z') return s[0];
    }
    return 0;
}

static void post_vk(int vk)
{
    UINT sc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
    LPARAM lp = (LPARAM)((sc << 16) | 1);

    printf("host: autokey vk=%02X (scan %02X)\n", vk, (unsigned)sc);
    PostMessageA(g_hwnd, WM_KEYDOWN, (WPARAM)vk, lp);
    PostMessageA(g_hwnd, WM_KEYUP, (WPARAM)vk, lp | 0xC0000000);
}

static DWORD WINAPI autokey_thread(LPVOID param)
{
    char *spec = _strdup((const char *)param);
    char *step = spec, *next;

    (void)param;
    while (step && *step) {
        char *colon = strchr(step, ':');
        int delay;
        if (!colon) break;
        *colon = 0;
        delay = atoi(step);
        if (delay > 0) Sleep((DWORD)delay);
        next = strchr(colon + 1, ';');
        if (next) *next++ = 0;
        {
            char *k = colon + 1;
            while (k && *k) {
                char *comma = strchr(k, ',');
                int vk;
                if (comma) *comma = 0;
                vk = vk_from_name(k, strlen(k));
                if (vk) post_vk(vk);
                else    printf("host: autokey: unknown key '%s'\n", k);
                k = comma ? comma + 1 : NULL;
            }
        }
        step = next;
    }
    printf("host: autokey schedule finished\n");
    free(spec);
    return 0;
}

/* ------------------------------------------------------------------ utils */

static DWORD WINAPI watchdog(LPVOID param)
{
    int secs = (int)(intptr_t)param;
    Sleep((DWORD)secs * 1000);
    printf("host: watchdog fired after %d s (%d frames drawn)\n", secs, g_frames);
    dos_dump_stats();
    ExitProcess(0);
    return 0;
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
 * which is exactly how g_rgb is packed, so the file can be written verbatim. */
static void dump_frame_bmp(const char *path)
{
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    DWORD pix = (DWORD)(320 * 200 * 4);
    FILE *f = fopen(path, "wb");

    if (!f) {
        printf("host: cannot write frame dump %s\n", path);
        return;
    }
    memset(&fh, 0, sizeof fh);
    memset(&ih, 0, sizeof ih);
    fh.bfType = 0x4D42;                 /* 'BM'                          */
    fh.bfOffBits = sizeof fh + sizeof ih;
    fh.bfSize = fh.bfOffBits + pix;
    ih.biSize = sizeof ih;
    ih.biWidth = 320;
    ih.biHeight = -200;                 /* top-down: same order as g_rgb */
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = pix;
    fwrite(&fh, sizeof fh, 1, f);
    fwrite(&ih, sizeof ih, 1, f);
    fwrite(g_rgb, pix, 1, f);
    fclose(f);
    printf("host: frame %d dumped to %s\n", g_frames, path);
}

static void blit(void)
{
    HDC hdc = GetDC(g_hwnd);
    const uint8_t *fb = vga();
    int i;

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
    StretchDIBits(hdc, 0, 0, 320 * g_scale, 200 * g_scale,
                  0, 0, 320, 200, g_rgb, &g_bmi, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(g_hwnd, hdc);

    if (g_screenshot_path && !g_screenshot_done &&
        g_frames >= g_screenshot_frame) {
        g_screenshot_done = 1;
        dump_frame_bmp(g_screenshot_path);
    }
    g_frames++;
}

/* ---------------------------------------------------------------- keyboard */

/* Keys the BIOS reports with an 0xE0 prefix (arrows, editing keypad). The game
 * compares scan codes coming out of int 16h, but keeping the buffer faithful to
 * the BIOS costs nothing and avoids surprises. */
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

static void kbd_push(uint8_t scan, uint8_t ascii)
{
    uint8_t *lm = lowmem();
    uint16_t tail = (uint16_t)(lm[0x41C] | (lm[0x41D] << 8));
    uint16_t head = (uint16_t)(lm[0x41A] | (lm[0x41B] << 8));
    uint16_t next = (uint16_t)(tail + 2);

    if (next >= 0x43E) next = 0x41E;
    if (next == head) return;                 /* buffer full */
    lm[tail]     = ascii;
    lm[tail + 1] = scan;
    lm[0x41C] = (uint8_t)(next & 0xFF);
    lm[0x41D] = (uint8_t)(next >> 8);
}

/* ------------------------------------------------------------- game thread */

typedef void (__cdecl *game_entry_fn)(void);

static DWORD WINAPI game_thread(LPVOID param)
{
    game_entry_fn fn = (game_entry_fn)(uintptr_t)g_le.entry_linear;
    printf("host: entering game code at 0x%X\n", g_le.entry_linear);
    fflush(stdout);
    fn();
    printf("host: game entry returned\n");
    g_running = 0;
    return 0;
}

/* --------------------------------------------------------------- window */

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CLOSE:
        g_running = 0;
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        uint8_t scan = (uint8_t)MapVirtualKeyA((UINT)w, MAPVK_VK_TO_VSC);
        kbd_push(scan, kbd_ascii_for(w, l));
        if (w == VK_ESCAPE && (GetKeyState(VK_CONTROL) & 0x8000)) {
            g_running = 0;
            PostQuitMessage(0);
        }
        return 0;
    }

    case WM_KEYUP:
    case WM_SYSKEYUP: {
        uint8_t scan = (uint8_t)MapVirtualKeyA((UINT)w, MAPVK_VK_TO_VSC);
        kbd_push((uint8_t)(scan | 0x80), kbd_ascii_for(w, l));
        return 0;
    }

    case WM_TIMER:
        if (g_show_frame) blit();
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        blit();
        EndPaint(h, &ps);
        return 0;
        }
    }
    return DefWindowProcA(h, m, w, l);
}

static int create_window(void)
{
    WNDCLASSA wc;
    RECT r = { 0, 0, 320 * g_scale, 200 * g_scale };

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "FD2NATIVE";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClassA(&wc)) return -1;

    g_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_bmi.bmiHeader.biWidth = 320;
    g_bmi.bmiHeader.biHeight = -200;          /* top-down */
    g_bmi.bmiHeader.biPlanes = 1;
    g_bmi.bmiHeader.biBitCount = 32;
    g_bmi.bmiHeader.biCompression = BI_RGB;

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

/* ------------------------------------------------------------------- main */

int main(int argc, char **argv)
{
    MSG msg;
    HANDLE th;
    int fixups = 0, i;
    const char *exe = "E:\\FD2\\FD2.EXE";
    const char *gamedir = "E:\\FD2";
    char logpath[MAX_PATH];

    /* Diagnostics first: this dumps everything to a file, never to a console
     * window. On failure the process exits silently so the user is not left
     * with a stray window. */
    GetModuleFileNameA(NULL, logpath, MAX_PATH);
    {
        char *slash = strrchr(logpath, '\\');
        if (slash) *(slash + 1) = 0; else logpath[0] = 0;
        strcat(logpath, "host.log");
    }
    freopen(logpath, "w", stdout);
    {
        char errpath[MAX_PATH];
        strcpy(errpath, logpath);
        { char *dot = strrchr(errpath, '.'); if (dot) strcpy(dot, ".err"); }
        freopen(errpath, "w", stderr);
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("FD2 native host - POC\n");
    printf("image base 0x%p\n", (void *)GetModuleHandleA(NULL));

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--image")) g_use_image = 1;
        /* Both "--gamedir <dir>" and "--gamedir=<dir>" are accepted: an
         * "=" form that is silently ignored falls back to the default game
         * directory and the whole test run quietly tests the wrong thing. */
        else if (!strcmp(argv[i], "--exe") && i + 1 < argc) exe = argv[++i];
        else if (!strncmp(argv[i], "--exe=", 6)) exe = argv[i] + 6;
        else if (!strcmp(argv[i], "--gamedir") && i + 1 < argc) gamedir = argv[++i];
        else if (!strncmp(argv[i], "--gamedir=", 10)) gamedir = argv[i] + 10;
        else if (!strncmp(argv[i], "--exit-after=", 13)) {
            int secs = atoi(argv[i] + 13);
            if (secs > 0)
                CreateThread(NULL, 0, watchdog, (LPVOID)(intptr_t)secs, 0, NULL);
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
        else if (!strncmp(argv[i], "--shot-frame=", 13)) {
            g_screenshot_frame = atoi(argv[i] + 13);
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
        }
        else if (!strncmp(argv[i], "--midi-dump=", 12)) {
            g_midi_dump = argv[i] + 12;
        }
    }

    /* Must be the very first allocation: the CRT heap grows from 0x10000. */
    if (le_reserve_address_space() != 0) {
        printf("host: fixed address space unavailable - this image landed at "
               "0x%p, which overlaps 0x10000..0x6FFFF (ASLR). Retry, or "
               "rebuild with /DYNAMICBASE:NO /BASE:0x10000000.\n",
               (void *)GetModuleHandleA(NULL));
        return 9;
    }
    printf("host: address space reserved\n");

    /* The game opens its data files by bare name (DIG.INI, FDOTHER.DAT, ...),
     * so the working directory has to be the game directory. */
    if (!SetCurrentDirectoryA(gamedir))
        printf("host: warning: cannot chdir to %s (%lu)\n", gamedir, GetLastError());
    else
        printf("host: working directory = %s\n", gamedir);

    if (le_open(&g_le, exe) != 0) { getchar(); return 1; }

    if (g_use_image) {
        if (le_map_flat(&g_le, "E:\\FD2\\port\\build\\objects.bin") != 0)
            return 1;
    } else if (le_map_and_relocate(&g_le, &fixups) != 0) {
        return 1;
    }

    dos_init_lowmem();
    dos_set_image(&g_le);
    dos_install_traps();
    dos_patch_interrupts();
    dos_patch_lowmem_refs();
    palette_default();

    /* Replace the Miles AIL entry points the game uses with host
     * implementations before the game thread starts (the real AIL would try to
     * execute 16-bit real-mode drivers). */
    ail_set_format(g_ail_rate, g_ail_bits, g_ail_stereo);
    xmidi_set_tick_rate(g_midi_rate);
    xmidi_set_test(g_midi_test);
    xmidi_set_backend(g_midi_backend);
    synth_set_bank_path(g_gm_bank);
    synth_set_dump_path(g_midi_dump);
    ail_install((uint8_t *)(uintptr_t)0x00010000u, g_ail_dump_dir);

    if (create_window() != 0) {
        fprintf(stderr, "host: cannot create window: %lu\n", GetLastError());
        return 1;
    }

    th = CreateThread(NULL, 4 * 1024 * 1024, game_thread, NULL, 0, NULL);
    if (!th) { fprintf(stderr, "host: cannot start game thread\n"); return 1; }
    printf("host: game thread started\n");

    if (g_autokey && g_autokey[0]) {
        printf("host: autokey schedule: %s\n", g_autokey);
        CreateThread(NULL, 0, autokey_thread, (LPVOID)g_autokey, 0, NULL);
    }

    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    printf("host: shutting down (%d frames drawn)\n", g_frames);
    dos_dump_stats();
    return 0;
}

/* ------------------------------------------------------- process entry point
 *
 * The CRT heap starts at 0x10000 and would take the window the DOS/4GW objects
 * must live in, so the reservation has to happen before CRT initialisation.
 * Zero CRT usage is allowed here. */
int __cdecl mainCRTStartup(void);

void __cdecl fd2_entry(void)
{
    le_reserve_address_space_early();
    ExitProcess((UINT)mainCRTStartup());
}
