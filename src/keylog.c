/* keylog.c - keystroke recording and replay (see keylog.h for the contract).
 *
 * Three pieces:
 *
 *   record   keylog_note() is called from host_key() for every make code:
 *            keep it in memory for the shutdown summary, print one line, and
 *            if --keylog was given append "<ms>:<key>" to the file at once
 *            (fflush, so an access violation still leaves the keystrokes).
 *
 *   replay   keyplay_load() parses the file at startup (a broken recording
 *            fails before the game starts, not half-way in), and a thread
 *            waits for each event's *absolute* time before posting it
 *            through input_post_key() - absolute waits, so the delays of an
 *            early key cannot shift the ones after it.
 *
 *   summary  keylog_finish() prints the whole schedule as one replayable
 *            line, so a recording survives even without --keylog (it is in
 *            host.log). Called from both shutdown paths.
 *
 * Key names round-trip through src/keys.h: the recording uses the portable
 * key vocabulary (RETURN/SPACE/UP/... plus single letters and digits), which
 * is exactly what fr_key_by_name() understands, and is shared with --autokey -
 * so a recording is replayable on any platform. A scan code with no portable
 * name is written as #s<scan>; the legacy Windows "#<decimal vk>" form is
 * still accepted on Win32.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "keylog.h"
#include "host.h"                      /* input_post_key()                 */
#include "keys.h"                      /* portable key vocabulary          */

#define KEYLOG_MAX 4096

static struct { DWORD ms; fr_key key; } g_ev[KEYLOG_MAX];
static int       g_n;
static DWORD     g_base;               /* host start stamp                 */
static FILE     *g_fp;                 /* --keylog file, or NULL           */
static char      g_exe_dir[MAX_PATH];  /* where host.log lives             */

static struct { DWORD ms; fr_key key; } g_play[KEYLOG_MAX];
static int       g_play_n;
static HANDLE    g_thread;

/* ------------------------------------------------------------- naming ----
 * One vocabulary for --autokey and --keylog: the portable names in keys.h.
 * A recording written today replays on any platform; the legacy "#<decimal
 * vk>" form (what the pre-keys-table recorder wrote for keys its own name
 * table lacked) is still accepted on Windows, where it was produced. */
static void key_name(fr_key key, uint8_t scan, int extended, char *buf, size_t n)
{
    const char *name = fr_key_name(key);

    if (name) {
        strncpy(buf, name, n - 1);
        buf[n - 1] = 0;
        return;
    }
    (void)extended;
    /* A scan code with no portable name (vendor keys, etc.). Record the
     * make code so the entry still round-trips on the same platform. */
    snprintf(buf, n, "#s%u", (unsigned)scan);
}

static fr_key key_from_text(const char *s)
{
    fr_key key = fr_key_by_name(s, strlen(s));

    if (key)
        return key;
    if (s[0] == '#' && s[1] == 's')
        return fr_key_from_scan((uint8_t)atoi(s + 2), 1);
#if defined(_WIN32)
    if (s[0] == '#') {
        int vk = atoi(s + 1);
        if (vk > 0 && vk < 256)
            return fr_key_from_vk(vk);
    }
#endif
    return FRK_NONE;
}

/* ------------------------------------------------------------- record ---- */
/* `ascii` carries the extended flag (0xE0) the entry layer put in the BDA
 * ring, so the scan code can be resolved back to the right key (KP8 vs UP). */
void keylog_note(uint8_t scan, uint8_t ascii)
{
    DWORD  ms;
    char   name[16];
    fr_key key = fr_key_from_scan((uint8_t)(scan & 0x7F), ascii == 0xE0);

    if (scan & 0x80)                   /* break codes: a replay posts them  */
        return;
    ms = GetTickCount() - g_base;
    key_name(key, (uint8_t)(scan & 0x7F), ascii == 0xE0, name, sizeof name);
    printf("host: key @%lu ms %s\n", (unsigned long)ms, name);

    if (g_n < KEYLOG_MAX) {
        g_ev[g_n].ms = ms;
        g_ev[g_n].key = key;
        g_n++;
    }
    if (g_fp) {
        fprintf(g_fp, "%lu:%s\n", (unsigned long)ms, name);
        fflush(g_fp);                   /* a crash must not lose the record */
    }
}

void keylog_finish(void)
{
    int i, shown = g_n;

    if (shown > 200)
        shown = 200;
    if (g_n > 0) {
        printf("host: key schedule (%d keys, absolute ms since start, replay "
               "with --keyplay): ", g_n);
        for (i = 0; i < shown; i++) {
            char name[16];
            key_name(g_ev[i].key, 0, 0, name, sizeof name);
            printf("%lu:%s%s", (unsigned long)g_ev[i].ms, name,
                   (i + 1 < g_n) ? ";" : "");
        }
        if (g_n > shown)
            printf("...(%d more, see --keylog)", g_n - shown);
        printf("\n");
    }
    if (g_fp) {
        fclose(g_fp);
        g_fp = NULL;
    }
}

/* ------------------------------------------------------------- replay ---- */
static void keyplay_load(const char *path)
{
    FILE *f;
    char *buf, *p;
    long  sz;

    if (!path || !path[0])
        return;
    f = fopen(path, "r");
    if (!f) {
        printf("host: keyplay: cannot open %s\n", path);
        return;
    }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 1024L * 1024L) {
        fclose(f);
        printf("host: keyplay: %s is empty or too big\n", path);
        return;
    }
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return;
    }
    sz = (long)fread(buf, 1, (size_t)sz, f);
    buf[sz] = 0;
    fclose(f);

    p = buf;
    while (*p && g_play_n < KEYLOG_MAX) {
        char *tok = p;
        char *end = strpbrk(p, ";\r\n");
        char *colon;
        int   ms;
        fr_key key;

        if (end) { *end = 0; p = end + 1; }
        else     p = tok + strlen(tok);
        while (*tok == ' ' || *tok == '\t')
            tok++;
        if (!*tok)
            continue;
        colon = strchr(tok, ':');
        if (!colon) {
            printf("host: keyplay: bad entry '%s' (want <ms>:<key>)\n", tok);
            continue;
        }
        *colon = 0;
        ms = atoi(tok);
        key = key_from_text(colon + 1);
        if (!key) {
            printf("host: keyplay: unknown key '%s'\n", colon + 1);
            continue;
        }
        g_play[g_play_n].ms = (DWORD)(ms < 0 ? 0 : ms);
        g_play[g_play_n].key = key;
        g_play_n++;
    }
    free(buf);
    printf("host: keyplay: %d keys from %s (%lu .. %lu ms after start)\n",
           g_play_n, path,
           g_play_n ? (unsigned long)g_play[0].ms : 0,
           g_play_n ? (unsigned long)g_play[g_play_n - 1].ms : 0);
}

static DWORD WINAPI keyplay_thread(LPVOID param)
{
    int i;

    (void)param;
    for (i = 0; i < g_play_n; i++) {
        LONG target = (LONG)(g_base + g_play[i].ms);
        while ((LONG)(GetTickCount() - target) < 0)
            Sleep(1);
        input_post_key(g_play[i].key);
    }
    printf("host: keyplay finished (%d keys)\n", g_play_n);
    return 0;
}

/* ---------------------------------------------------------------- api ---- */
int keylog_init(const char *log_path, const char *play_path, uint32_t base)
{
    g_base = (DWORD)base;
    g_exe_dir[0] = 0;

    if (log_path && log_path[0]) {
        char full[MAX_PATH];
        char mod[MAX_PATH];

        /* The host chdirs to the game directory, so a bare name would land
         * in the wrong place (the same trap as --screenshot): a relative path
         * is resolved next to host.log, an absolute one is used as given. */
        if (log_path[0] == '\\' || strchr(log_path, ':') != NULL) {
            strncpy(full, log_path, sizeof full - 1);
            full[sizeof full - 1] = 0;
        } else if (GetModuleFileNameA(NULL, mod, MAX_PATH)) {
            char *slash = strrchr(mod, '\\');
            if (slash)
                *slash = 0;
            _snprintf(full, sizeof full, "%s\\%s", mod, log_path);
            full[sizeof full - 1] = 0;
        } else {
            strncpy(full, log_path, sizeof full - 1);
            full[sizeof full - 1] = 0;
        }
        g_fp = fopen(full, "w");
        if (!g_fp)
            printf("host: keylog: cannot write %s (keys still go to the log)\n",
                   full);
        else
            printf("host: keylog -> %s (one '<ms>:<key>' per line)\n", full);
    }

    if (play_path && play_path[0])
        keyplay_load(play_path);
    return g_play_n;
}

int keylog_start(void)
{
    if (g_play_n <= 0)
        return 0;
    g_thread = CreateThread(NULL, 0, keyplay_thread, NULL, 0, NULL);
    printf("host: keyplay: replaying %d keys on their recorded times\n",
           g_play_n);
    return g_thread != NULL;
}

int keylog_replaying(void)
{
    if (!g_thread)
        return 0;
    if (WaitForSingleObject(g_thread, 0) == WAIT_OBJECT_0) {
        CloseHandle(g_thread);
        g_thread = NULL;
        return 0;
    }
    return 1;
}
