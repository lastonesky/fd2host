/* ail.c - Miles AIL (Audio Interface Library) replacement layer.
 *
 * Why this exists
 * ---------------
 * FD2.EXE has Miles AIL V3.02 linked in. The library itself is fine, but it
 * plays sound by loading a 16-bit real-mode driver (*.DIG for digital audio,
 * *.MDI for MIDI) and jumping straight into that code, which cannot run inside
 * a Win32 process. With the driver files blocked, every AIL_install_* returned
 * 0, the game set its "no digital audio device" flag and ran silently.
 *
 * How it is replaced
 * ------------------
 * IDA shows the game calls only 16 AIL entry points (port/re/RE_MAP.md ��3):
 *
 *     AIL_startup, AIL_shutdown,
 *     AIL_install_DIG_INI, AIL_install_MDI_INI,
 *     AIL_allocate_sample_handle, AIL_allocate_sequence_handle,
 *     AIL_init_sample, AIL_set_sample_address, AIL_set_sample_loop_count,
 *     AIL_start_sample, AIL_stop_sample,
 *     AIL_init_sequence, AIL_start_sequence, AIL_stop_sequence,
 *     AIL_set_sequence_volume, AIL_set_sequence_loop_count
 *
 * Each of those entry points is patched to a 5-byte `jmp rel32` targeting the
 * host implementation below. AIL's public API uses the Watcom cdecl calling
 * convention, which for these signatures is ABI-identical to MSVC __cdecl
 * (arguments on the stack, caller cleans up, return value in EAX), so the
 * patched calls are compatible without any thunking.
 *
 * Digital audio is played with WinMM waveOut: the game hands AIL a pointer and
 * length into its own memory (AIL_set_sample_address), and we copy that block
 * and submit it to the sound device. MIDI sequences are accepted and recorded
 * (the data pointer identifies the XMIDI blob inside FDMUS.DAT) but not yet
 * synthesised - that needs an XMIDI player and is the next step.
 */

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "ail.h"
#include "xmidi.h"
#include "synth.h"

#define AIL_OBJ0_BASE    0x00010000u
/* FDPS asks for 8 sample handles in one go (sub_30270 loops 8 times), FD2 for 2. */
#define AIL_MAX_SAMPLES  8
#define AIL_MAX_SEQS     4

/* ------------------------------------------------------------- state ----- */

typedef struct {
    int        used;
    uint8_t   *addr;          /* guest sample pointer (AIL_set_sample_address) */
    uint32_t   len;
    int32_t    loop_count;
    int32_t    volume;
    int32_t    pan;
    int32_t    type;
    uint32_t   rate;          /* playback rate (AIL default unless changed) */
    int        channels;      /* from AIL_set_sample_type (FDPS sets it)     */
    int        bits;
    int32_t    playing;
    int        dumped;        /* sample bytes already written to dump_dir */
    HWAVEOUT   dev;           /* opened lazily on the first play            */
    WAVEHDR    hdr;
    uint8_t   *pcm;           /* host copy: waveOut reads it asynchronously */
} ail_sample;

typedef struct {
    int             used;
    const uint8_t  *data;     /* XMIDI blob inside FDMUS.DAT               */
    uint32_t        len;      /* derived from the IFF FORM header          */
    int32_t         seq_num;
    int32_t         volume;
    int32_t         loop_count;
    int32_t         playing;
} ail_seq;

static ail_sample  g_samples[AIL_MAX_SAMPLES];
static ail_seq     g_seqs[AIL_MAX_SEQS];
static uint32_t    g_rate    = 11025;   /* AIL default playback rate */
static int         g_bits    = 8;       /* AIL default sample type: mono 8-bit */
static int         g_stereo  = 0;
static char        g_dump_dir[MAX_PATH];
static int         g_dump_seq;
static int         g_installed;
/* How often a sample was cut off while still playing (AIL semantics: stop
 * then start). A cut is inherent to retriggering - what we removed is the
 * *device* teardown that used to go with it. Counted so the residual is
 * visible instead of being guessed at (docs/AUDIO.md). */
static unsigned long g_sample_cuts;

/* ------------------------------------------------------------ AIL timers ---
 *
 * AIL's timer API is backed by a real-mode ISR that reprograms the PIT and
 * fires guest callbacks from interrupt context. None of that exists here, so
 * the whole subsystem is replaced by one host thread that ticks every 1 ms and
 * invokes the guest callback directly (it is an ordinary near function -
 * FDPS's is `inc dword_69D64; call rand; ret` - and needs no game registers).
 *
 * Semantics copied from FDPS's own AIL (re/fdps_ail_*.c, re/fdps_timer_core_*.c):
 *
 *   - 15 slots, the handle *is* the byte offset into the tables (0,4,8,...),
 *     -1 means "no such timer"; AIL_register_timer returns -1 when full.
 *   - slot state: 0 free, 1 allocated, 2 running (start: 1->2, stop: 2->1).
 *   - AIL_set_timer_frequency(hz) is sugar for period = 1000000 / hz.
 *   - the ISR accumulates elapsed time per running timer and drains the
 *     pending count with `while (pend) { --pend; cb(user); }`, so a stalled
 *     host catches up rather than skipping ticks (capped here at
 *     AIL_TIMER_MAXPEND so a long stall cannot spin the game clock forward).
 */

#define AIL_MAX_TIMERS     15
#define AIL_TIMER_MAXPEND  8
#define AIL_TIMER_TICK_US  1000          /* host thread sleep granularity */

typedef void (*ail_timer_cb)(uint32_t user);

typedef struct {
    int          used;        /* 0 free / 1 allocated+stopped / 2 running */
    ail_timer_cb cb;
    uint32_t     user;
    uint32_t     period_us;
    uint32_t     acc_us;
    uint32_t     pend;
} ail_timer;

static ail_timer       g_timers[AIL_MAX_TIMERS];
static CRITICAL_SECTION g_timer_cs;
static int              g_timer_cs_ready;
static HANDLE           g_timer_thread;
static volatile LONG    g_timer_run;
static volatile LONG    g_timer_fires;
static DWORD            g_timer_t0;
static int              g_timer_log;
static int              g_timer_fired_logged;

static void timer_lock(void)
{
    if (!g_timer_cs_ready) {           /* ail_install_* runs before the game */
        InitializeCriticalSection(&g_timer_cs);
        g_timer_cs_ready = 1;
    }
    EnterCriticalSection(&g_timer_cs);
}

static void timer_unlock(void)
{
    LeaveCriticalSection(&g_timer_cs);
}

/* Handles are byte offsets, exactly like AIL's; -1 and misaligned handles are
 * ignored the way the original does (it tests `h != -1` then indexes). */
static ail_timer *timer_at(int32_t h)
{
    if (h < 0 || h >= AIL_MAX_TIMERS * 4 || (h & 3))
        return NULL;
    return &g_timers[h >> 2];
}

static void timer_log(const char *what, int32_t h)
{
    if (g_timer_log++ > 24)
        return;
    printf("ail: %s(h=%d)\n", what, (int)h);
}

struct ail_fire {
    ail_timer_cb cb;
    uint32_t     user;
    int          n;
};

static DWORD WINAPI ail_timer_thread(LPVOID arg)
{
    LARGE_INTEGER fq, last, now;
    struct ail_fire batch[AIL_MAX_TIMERS];
    (void)arg;

    QueryPerformanceFrequency(&fq);
    QueryPerformanceCounter(&last);
    g_timer_t0 = GetTickCount();
    while (InterlockedCompareExchange(&g_timer_run, 1, 1) == 1) {
        uint32_t us;
        int i, nb = 0, j, k;

        Sleep(AIL_TIMER_TICK_US / 1000);
        QueryPerformanceFrequency(&fq);
        QueryPerformanceCounter(&now);
        us = (uint32_t)(((now.QuadPart - last.QuadPart) * 1000000) / fq.QuadPart);
        last = now;
        if (us > 250000)                /* clamp a long stall (breakpoint, hitch) */
            us = 250000;

        timer_lock();
        for (i = 0; i < AIL_MAX_TIMERS; i++) {
            ail_timer *t = &g_timers[i];
            if (t->used != 2 || !t->period_us)
                continue;
            t->acc_us += us;
            while (t->acc_us >= t->period_us) {
                t->acc_us -= t->period_us;
                if (t->pend < AIL_TIMER_MAXPEND)
                    t->pend++;
                else
                    t->acc_us = 0;      /* give up catching up */
            }
            if (t->pend) {
                batch[nb].cb = t->cb;
                batch[nb].user = t->user;
                batch[nb].n = (int)t->pend;
                t->pend = 0;
                nb++;
            }
        }
        timer_unlock();

        /* Callbacks run outside the lock: guest code may call straight back
         * into AIL (register/stop/release) and must not deadlock. */
        for (j = 0; j < nb; j++) {
            LONG n = 0;
            for (k = 0; k < batch[j].n; k++) {
                batch[j].cb(batch[j].user);
                n = InterlockedIncrement(&g_timer_fires);
                if (n <= 3 || (n % 100) == 0)
                    printf("ail: timer fire #%ld at +%lu ms (cb=%p)\n",
                           (long)n, GetTickCount() - g_timer_t0,
                           (void *)batch[j].cb);
            }
            if (!g_timer_fired_logged) {
                g_timer_fired_logged = 1;
                printf("ail: timer callback fired (cb=%p, %d pending tick(s))\n",
                       (void *)batch[j].cb, batch[j].n);
            }
        }
    }
    return 0;
}

static void timer_start_thread(void)
{
    if (g_timer_thread)
        return;
    timer_lock();
    InterlockedExchange(&g_timer_run, 1);
    g_timer_thread = CreateThread(NULL, 0, ail_timer_thread, NULL, 0, NULL);
    timer_unlock();
    printf("ail: timer thread started (1 ms tick)\n");
}

static void timer_stop_thread(void)
{
    if (!g_timer_thread)
        return;
    InterlockedExchange(&g_timer_run, 0);
    WaitForSingleObject(g_timer_thread, 1000);
    CloseHandle(g_timer_thread);
    g_timer_thread = NULL;
    memset(g_timers, 0, sizeof g_timers);
    printf("ail: timer thread stopped\n");
}

/* ------------------------------------------------- AIL timer entry points --- */

static int32_t host_AIL_register_timer(void *callback)
{
    int32_t h = -1;
    int i;

    timer_lock();
    for (i = 0; i < AIL_MAX_TIMERS; i++) {
        if (g_timers[i].used)
            continue;
        memset(&g_timers[i], 0, sizeof g_timers[i]);
        g_timers[i].used = 1;
        g_timers[i].cb = (ail_timer_cb)callback;
        g_timers[i].period_us = 54925;  /* AIL default: the 18.2 Hz PC timer */
        h = i * 4;
        break;
    }
    timer_unlock();

    printf("ail: register_timer(cb=%p) -> handle %d%s\n", callback, (int)h,
           h < 0 ? " (table full)" : "");
    if (h >= 0)
        timer_start_thread();
    return h;
}

static void host_AIL_set_timer_user(int32_t h, int32_t user)
{
    ail_timer *t = timer_at(h);
    if (t) {
        timer_lock();
        t->user = (uint32_t)user;
        timer_unlock();
        timer_log("set_timer_user", h);
    }
}

static void host_AIL_set_timer_period(int32_t h, int32_t usec)
{
    ail_timer *t = timer_at(h);
    if (t) {
        timer_lock();
        t->period_us = usec > 0 ? (uint32_t)usec : 1;
        t->acc_us = 0;
        timer_unlock();
        timer_log("set_timer_period", h);
    }
}

static void host_AIL_set_timer_frequency(int32_t h, int32_t hz)
{
    ail_timer *t = timer_at(h);
    if (t) {
        timer_lock();
        t->period_us = hz > 0 ? (uint32_t)(1000000 / hz) : 1000000;
        t->acc_us = 0;
        timer_unlock();
        printf("ail: set_timer_frequency(h=%d, %d Hz -> period %u us)\n",
               (int)h, (int)hz, (unsigned)t->period_us);
    }
}

/* AIL_start_all_timers()/AIL_stop_all_timers() share the single-argument
 * wrappers in FDPS's build (IDA shows both trace strings inside one function),
 * so -1 selects every timer here as well. */
static int32_t host_AIL_start_timer(int32_t h)
{
    ail_timer *t = timer_at(h);
    int i;

    timer_lock();
    if (!t) {
        for (i = 0; i < AIL_MAX_TIMERS; i++)
            if (g_timers[i].used == 1)
                g_timers[i].used = 2;
    } else if (t->used == 1) {
        t->used = 2;
        t->acc_us = 0;
    }
    timer_unlock();
    timer_log("start_timer", h);
    timer_start_thread();
    return 1;
}

static int32_t host_AIL_stop_timer(int32_t h)
{
    ail_timer *t = timer_at(h);
    int i;

    timer_lock();
    if (!t) {
        for (i = 0; i < AIL_MAX_TIMERS; i++)
            if (g_timers[i].used == 2)
                g_timers[i].used = 1;
    } else if (t->used == 2) {
        t->used = 1;
    }
    timer_unlock();
    timer_log("stop_timer", h);
    return 1;
}

static int32_t host_AIL_release_timer_handle(int32_t h)
{
    ail_timer *t = timer_at(h);
    if (t) {
        timer_lock();
        memset(t, 0, sizeof *t);
        timer_unlock();
        timer_log("release_timer_handle", h);
    }
    return 1;
}

static int32_t host_AIL_release_all_timers(void)
{
    timer_lock();
    memset(g_timers, 0, sizeof g_timers);
    timer_unlock();
    timer_log("release_all_timers", -1);
    return 1;
}

/* ------------------------------------------------------------- helpers --- */

static void dump_blob(const char *tag, const void *p, uint32_t len)
{
    char path[MAX_PATH];
    FILE *f;

    if (!g_dump_dir[0] || !p || !len)
        return;
    if (len > 512 * 1024)
        len = 512 * 1024;
    _snprintf(path, sizeof path, "%s\\%s_%d.bin", g_dump_dir, tag, g_dump_seq++);
    f = fopen(path, "wb");
    if (!f)
        return;
    fwrite(p, 1, len, f);
    fclose(f);
    printf("ail: dumped %u bytes of %s to %s\n", (unsigned)len, tag, path);
}

/* Summarise the block the game is about to play: 8-bit unsigned PCM hovers
 * around 0x80 while 16-bit signed data shows high bytes clustered at 0x00 or
 * 0xFF. This is what pins down the format the game assumes. */
static void log_pcm_stats(const char *what, const uint8_t *p, uint32_t len)
{
    uint32_t i, near_128 = 0, hi_zero = 0;
    uint8_t lo = 0xFF, hi = 0x00;

    if (!len)
        return;
    for (i = 0; i < len; i++) {
        if (p[i] >= 0x60 && p[i] <= 0xA0) near_128++;
        if (p[i] < lo) lo = p[i];
        if (p[i] > hi) hi = p[i];
    }
    for (i = 1; i < len; i += 2)
        if (p[i] == 0x00 || p[i] == 0xFF) hi_zero++;
    printf("ail: %s len=%u  range=%02X..%02X  near-0x80=%.0f%%  odd-byte-00/FF=%.0f%%"
           "  head=%02X %02X %02X %02X %02X %02X %02X %02X\n",
           what, (unsigned)len, lo, hi, 100.0 * near_128 / len,
           100.0 * hi_zero / (len / 2 + 1),
           p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
}

static ail_sample *sample_of(void *handle)
{
    ail_sample *s = (ail_sample *)handle;
    if (s < g_samples || s >= g_samples + AIL_MAX_SAMPLES)
        return NULL;
    return s->used ? s : NULL;
}

static ail_seq *seq_of(void *handle)
{
    ail_seq *s = (ail_seq *)handle;
    if (s < g_seqs || s >= g_seqs + AIL_MAX_SEQS)
        return NULL;
    return s->used ? s : NULL;
}

/* ------------------------------------------------------------- waveOut --- */

/* Unprepare + free the submitted buffer. The driver owns a WAVEHDR until it
 * is marked WHDR_DONE, so this waits for that flag (bounded, same guard as
 * synth_stop) - it must never free memory the driver is still reading. */
static void sample_release_buffer(ail_sample *s)
{
    if (s->dev && (s->hdr.dwFlags & WHDR_PREPARED)) {
        int guard = 0;
        while (!(s->hdr.dwFlags & WHDR_DONE) && guard++ < 200)
            Sleep(1);
        waveOutUnprepareHeader(s->dev, &s->hdr, sizeof s->hdr);
    }
    memset(&s->hdr, 0, sizeof s->hdr);
    if (s->pcm) {
        free(s->pcm);
        s->pcm = NULL;
    }
}

/* Stop playback but *keep the device open*.
 *
 * This used to be waveOutClose + waveOutOpen on every single sound effect
 * (AIL_stop_sample -> close, AIL_start_sample -> open, and AIL_init_sample
 * closed it once more), i.e. the whole audio device was torn down and
 * rebuilt a couple of times a second. Besides being slow, each teardown
 * pops. The device is now opened once per sample and closed only when the
 * format changes or in ail_shutdown (docs/AUDIO.md, round 33 33.7). */
static void sample_stop(ail_sample *s)
{
    if (s->dev && (s->hdr.dwFlags & WHDR_PREPARED) &&
        !(s->hdr.dwFlags & WHDR_DONE)) {
        waveOutReset(s->dev);           /* only if it is still playing */
        g_sample_cuts++;                /* a real cut: documented, not silent */
    }
    sample_release_buffer(s);
    s->playing = 0;
}

static void sample_close_device(ail_sample *s)
{
    sample_stop(s);
    if (s->dev) {
        waveOutClose(s->dev);
        s->dev = NULL;
    }
}

static int sample_open_device(ail_sample *s)
{
    WAVEFORMATEX wf;
    if (s->dev)
        return 1;
    /* Per-sample format: AIL_set_sample_type / _playback_rate fill it in
     * (FDPS derives both from the WAV header it is about to play); samples
     * that never get those calls keep the AIL defaults captured at
     * allocation time, which is what FD2 relies on. */
    memset(&wf, 0, sizeof wf);
    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = (WORD)(s->channels ? s->channels : (g_stereo ? 2 : 1));
    wf.nSamplesPerSec  = s->rate ? s->rate : g_rate;
    wf.wBitsPerSample  = (WORD)(s->bits ? s->bits : g_bits);
    wf.nBlockAlign     = (WORD)(wf.nChannels * wf.wBitsPerSample / 8);
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    if (waveOutOpen(&s->dev, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        printf("ail: waveOutOpen(%u Hz, %d-bit, %d ch) failed\n",
               (unsigned)wf.nSamplesPerSec, (int)wf.wBitsPerSample, (int)wf.nChannels);
        s->dev = NULL;
        return 0;
    }
    return 1;
}

/* Master output volume (host --volume, 0..100, default 100 = no attenuation,
 * the game's own level): attenuates what actually reaches waveOut, nothing
 * else. The pipeline above it - AIL volume tracking, the copy of the guest
 * PCM, waveOut submission - is untouched, so audio bugs still show up at any
 * volume. Debug runs pass --volume=10 to stay quiet. */
static int g_master_volume = 100;

void ail_set_master_volume(int percent)
{
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;
    g_master_volume = percent;
    synth_set_master_volume(percent);
    printf("ail: master output volume = %d%% (music + SFX scaled at waveOut)\n",
           percent);
}

/* Scale a host-side PCM copy by `g` (0..1). 8-bit waveOut PCM is unsigned,
 * 16-bit is signed little-endian; channels are interleaved and all share the
 * same gain, so the loop is per sample either way. */
static void pcm_apply_gain(uint8_t *pcm, uint32_t bytes, int bits, double g)
{
    uint32_t i, n;

    if (bits <= 8) {
        for (i = 0; i < bytes; i++) {
            double v = ((int)pcm[i] - 128) * g;
            int    q = (int)(v + (v >= 0.0 ? 0.5 : -0.5)) + 128;
            pcm[i] = (uint8_t)(q < 0 ? 0 : (q > 255 ? 255 : q));
        }
    } else {
        int16_t *p = (int16_t *)(void *)pcm;
        n = bytes / 2;
        for (i = 0; i < n; i++) {
            double v = (double)p[i] * g;
            int    q = (int)(v + (v >= 0.0 ? 0.5 : -0.5));
            p[i] = (int16_t)(q < -32768 ? -32768 : (q > 32767 ? 32767 : q));
        }
    }
}

/* Fade the first and last few milliseconds of a PCM copy to silence.
 *
 * A one-shot waveOutWrite starts and stops wherever the waveform happens to
 * be: an 8-bit buffer that begins at, say, 200 instead of the 128 silence
 * level steps the output the instant the device starts, and the same step
 * happens again when the device goes idle at the end. Those two steps are
 * the "click" (docs/AUDIO.md). The DOS hardware fed the same waveform to a
 * Sound Blaster, which was far more forgiving; waveOut is not.
 *
 * 3 ms is below the threshold where a level change is audible as such, but
 * it turns both steps into ramps. Applied to the host copy only - guest
 * memory is never touched. */
#define AIL_RAMP_MS 3

static void pcm_apply_ramp(uint8_t *pcm, uint32_t bytes, int bits,
                           uint32_t rate, int channels)
{
    uint32_t nsamp, ramp, i;
    int16_t *p16;

    nsamp = bytes / (bits > 8 ? 2u : 1u);
    if (nsamp < 8 || rate == 0)
        return;
    ramp = (uint32_t)((double)rate * AIL_RAMP_MS / 1000.0) *
           (uint32_t)(channels > 0 ? channels : 1);
    if (ramp == 0)
        return;
    if (ramp > nsamp / 2)
        ramp = nsamp / 2;               /* never eat more than half a sample */

    if (bits <= 8) {
        for (i = 0; i < ramp; i++) {
            double g = (double)i / (double)ramp;
            int head = (int)pcm[i] - 128;
            int tail = (int)pcm[nsamp - 1 - i] - 128;
            pcm[i]               = (uint8_t)((int)(head * g) + 128);
            pcm[nsamp - 1 - i]   = (uint8_t)((int)(tail * g) + 128);
        }
    } else {
        p16 = (int16_t *)(void *)pcm;
        for (i = 0; i < ramp; i++) {
            double g = (double)i / (double)ramp;
            p16[i]             = (int16_t)(p16[i] * g);
            p16[nsamp - 1 - i] = (int16_t)(p16[nsamp - 1 - i] * g);
        }
    }
}

static void sample_play(ail_sample *s)
{
    uint32_t played;
    int      bits;
    int      cut;

    if (!s->addr || !s->len)
        return;
    if (!sample_open_device(s))
        return;

    /* The driver owns the submitted header until it is WHDR_DONE, so release
     * it before the copy is rebuilt. Only reset when the previous sound is
     * genuinely still running - a finished sample needs no cut at all. */
    cut = (s->hdr.dwFlags & WHDR_PREPARED) && !(s->hdr.dwFlags & WHDR_DONE);
    sample_stop(s);

    s->pcm = (uint8_t *)malloc(s->len);
    if (!s->pcm)
        return;
    memcpy(s->pcm, s->addr, s->len);
    bits = s->bits ? s->bits : g_bits;
    {
        /* master volume x the game's AIL_set_sample_volume (0..127, the AIL
         * default is 127): applied to the copy, never to guest memory. */
        double g = (double)g_master_volume / 100.0 *
                   (double)s->volume / 127.0;
        if (g < 1.0)
            pcm_apply_gain(s->pcm, s->len, bits, g);
    }
    pcm_apply_ramp(s->pcm, s->len, bits,
                   s->rate ? s->rate : g_rate, s->channels);

    s->hdr.lpData         = (LPSTR)s->pcm;
    s->hdr.dwBufferLength = s->len;
    if (waveOutPrepareHeader(s->dev, &s->hdr, sizeof s->hdr) != MMSYSERR_NOERROR)
        return;
    if (waveOutWrite(s->dev, &s->hdr, sizeof s->hdr) == MMSYSERR_NOERROR) {
        s->playing = 1;
        played = s->len;
        printf("ail: play %u bytes (%.2f s @ %u Hz, loop=%d)%s\n",
               (unsigned)played, (double)played / (g_rate * (g_stereo ? 2 : 1) * (g_bits / 8)),
               (unsigned)g_rate, (int)s->loop_count, cut ? " (cut)" : "");
    }
}

/* ------------------------------------------- host AIL implementations ---- */

/* All of these are __cdecl to match the Watcom cdecl AIL API. */

static int32_t host_AIL_startup(void)
{
    printf("ail: startup -> ok (replacement layer)\n");
    return 1;
}

static void host_AIL_shutdown(void)
{
    int i;
    timer_stop_thread();
    xmidi_stop();
    for (i = 0; i < AIL_MAX_SAMPLES; i++) {
        if (g_samples[i].used) {
            sample_close_device(&g_samples[i]);
            if (g_samples[i].pcm) free(g_samples[i].pcm);
            memset(&g_samples[i], 0, sizeof g_samples[i]);  /* FDPS re-inits audio in-process */
        }
    }
    for (i = 0; i < AIL_MAX_SEQS; i++)
        memset(&g_seqs[i], 0, sizeof g_seqs[i]);
    printf("ail: shutdown (timer callbacks fired %lu, %lu samples cut short)\n",
           g_timer_fires, g_sample_cuts);
}

/* The game checks these for non-zero before using any sample/sequence API. */
static int32_t host_AIL_install_DIG_INI(void)
{
    printf("ail: install_DIG_INI -> fake driver handle 1\n");
    return 1;
}

static int32_t host_AIL_install_MDI_INI(void)
{
    printf("ail: install_MDI_INI -> fake driver handle 1\n");
    return 1;
}

static void *host_AIL_allocate_sample_handle(int32_t driver)
{
    int i;
    (void)driver;
    for (i = 0; i < AIL_MAX_SAMPLES; i++) {
        if (!g_samples[i].used) {
            memset(&g_samples[i], 0, sizeof g_samples[i]);
            g_samples[i].used = 1;
            g_samples[i].volume = 127;      /* AIL default */
            g_samples[i].pan = 64;
            g_samples[i].rate = g_rate;
            g_samples[i].channels = g_stereo ? 2 : 1;
            g_samples[i].bits = g_bits;
            printf("ail: allocate_sample_handle -> %d\n", i);
            return &g_samples[i];
        }
    }
    printf("ail: allocate_sample_handle -> out of handles\n");
    return NULL;
}

static void *host_AIL_allocate_sequence_handle(int32_t driver)
{
    int i;
    (void)driver;
    for (i = 0; i < AIL_MAX_SEQS; i++) {
        if (!g_seqs[i].used) {
            memset(&g_seqs[i], 0, sizeof g_seqs[i]);
            g_seqs[i].used = 1;
            g_seqs[i].volume = 127;
            printf("ail: allocate_sequence_handle -> %d\n", i);
            return &g_seqs[i];
        }
    }
    return NULL;
}

static void host_AIL_init_sample(void *h)
{
    ail_sample *s = sample_of(h);
    if (!s)
        return;
    s->len = 0;
    s->addr = NULL;
    s->loop_count = 1;
    s->type = 0;                            /* DIG_F_MONO_8 */
    /* Stop, but do NOT tear the device down: AIL_init_sample is called on
     * every single sound effect (svc_play_sfx: init/addr/loop/start), so
     * closing here was one waveOutClose per effect (round 33 33.7). */
    sample_stop(s);
}

static void host_AIL_set_sample_address(void *h, void *start, uint32_t len)
{
    ail_sample *s = sample_of(h);
    if (!s)
        return;
    s->addr = (uint8_t *)start;
    s->len = len;
    if (s->len) {
        log_pcm_stats("set_sample_address", s->addr, s->len);
        if (!s->dumped) {
            s->dumped = 1;                  /* one copy per distinct sample */
            dump_blob("ail_smp", s->addr, s->len);
        }
    }
}

static void host_AIL_set_sample_loop_count(void *h, int32_t count)
{
    ail_sample *s = sample_of(h);
    if (s)
        s->loop_count = count;
}

static void host_AIL_start_sample(void *h)
{
    ail_sample *s = sample_of(h);
    if (s)
        sample_play(s);
}

static void host_AIL_stop_sample(void *h)
{
    ail_sample *s = sample_of(h);
    if (!s)
        return;
    sample_close_device(s);
    printf("ail: stop_sample\n");
}

/* AIL_set_sample_type(handle, type, flags) - type encodes channels+bits
 * (0=mono/8, 1=mono/16, 2=stereo/8, 3=stereo/16). FDPS computes it from the
 * WAV header it is about to play, so this is what decides the waveOut format
 * here; FD2 never calls it and keeps the AIL defaults. */
static int32_t host_AIL_set_sample_type(void *h, int32_t type, int32_t flags)
{
    ail_sample *s = sample_of(h);
    (void)flags;
    if (!s)
        return 0;
    s->type = type;
    s->channels = (type == 2 || type == 3) ? 2 : 1;
    s->bits = (type == 1 || type == 3) ? 16 : 8;
    if (s->playing)
        sample_close_device(s);        /* format changed: reopen on next play */
    printf("ail: set_sample_type(%d ch, %d-bit)\n", s->channels, s->bits);
    return 1;
}

static int32_t host_AIL_set_sample_playback_rate(void *h, int32_t rate)
{
    ail_sample *s = sample_of(h);
    if (!s)
        return 0;
    if (rate >= 4000 && rate <= 192000)
        s->rate = (uint32_t)rate;
    if (s->playing)
        sample_close_device(s);
    printf("ail: set_sample_playback_rate(%d)\n", (int)rate);
    return 1;
}

/* The game's own digital volume: recorded on the handle and applied in
 * sample_play together with the master volume (the original comment here said
 * "not applied yet"; FD2 never calls it, FDPS does). */
static int32_t host_AIL_set_sample_volume(void *h, int32_t volume, int32_t ms)
{
    ail_sample *s = sample_of(h);
    (void)ms;
    if (!s)
        return 0;
    s->volume = volume;
    printf("ail: set_sample_volume(%d)\n", (int)volume);
    return 1;
}

/* AIL_sample_status returns the DIG driver's state word at handle+4. The
 * values observed in FDPS's copy of the driver (re/fdps_digcore_*.c):
 *
 *     1 = playing   2 = playing, looping   4 = done/available   8 = stopped
 *
 * The game scans its 8 handles for `status == 4` to find a free one
 * (sub_303C0/sub_30790) and waits on `status == 4` to know a sound ended
 * (sub_304D0), so 4 has to mean "not busy" in every non-playing state. */
static int32_t host_AIL_sample_status(void *h)
{
    ail_sample *s = sample_of(h);

    if (!s)
        return 4;
    if (s->playing && (!s->dev || (s->hdr.dwFlags & WHDR_DONE))) {
        if (s->loop_count) {
            sample_play(s);            /* keep a looping sample going */
        } else {
            s->playing = 0;
            printf("ail: sample done\n");
        }
    }
    if (!s->playing)
        return 4;
    return s->loop_count ? 2 : 1;
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* AIL_init_sequence is handed the address of the XDIR directory inside
 * FDMUS.DAT (its FORM header covers only the 22-byte catalogue), and
 * sequence_num selects which song to use. This mirrors AIL's own walker
 * (obj0 sub_42520): descend FORM/CAT wrappers until the block type is XMID,
 * then pick the sequence_num-th "FORM XMID" child of that CAT. */
static const uint8_t *locate_xmid(const uint8_t *base, int index, uint32_t *out_len)
{
    const uint8_t *p = base;
    uint32_t blk;

    if (!base)
        return NULL;
    for (;;) {
        if (memcmp(p, "FORM", 4) && memcmp(p, "CAT ", 4))
            return NULL;
        blk = 8 + be32(p + 4);
        if (!memcmp(p + 8, "XMID", 4))
            break;
        p += blk;
    }
    if (!memcmp(p, "FORM", 4)) {            /* the blob is a single song */
        if (index != 0)
            return NULL;
        *out_len = 8 + be32(p + 4);
        return p;
    }
    {                                       /* CAT: index-th XMID child */
        const uint8_t *end = p + blk;
        const uint8_t *c = p + 12;
        int n = index;
        while (c + 8 <= end) {
            uint32_t clen = 8 + be32(c + 4);
            if (!memcmp(c + 8, "XMID", 4)) {
                if (n == 0) {
                    *out_len = clen;
                    return c;
                }
                n--;
            }
            if (!clen)
                break;
            c += clen;
        }
    }
    return NULL;
}

static void host_AIL_init_sequence(void *h, void *start, int32_t sequence_num)
{
    ail_seq *q = seq_of(h);
    const uint8_t *blob = NULL;
    uint32_t len = 0;

    if (!q)
        return;
    q->seq_num = sequence_num;
    q->data = NULL;
    q->len = 0;
    if (!start)
        return;
    blob = locate_xmid((const uint8_t *)start, (int)sequence_num, &len);
    if (!blob || len < 16) {
        printf("ail: init_sequence(seq=%d) - no XMID found from %p\n",
               (int)sequence_num, start);
        return;
    }
    q->data = blob;
    q->len = len;
    printf("ail: init_sequence(seq=%d) base=%p song=%p len=%u head=%c%c%c%c\n",
           (int)sequence_num, start, (const void *)blob, (unsigned)len,
           blob[0], blob[1], blob[2], blob[3]);
    dump_blob("ail_seq", blob, len);
}

static void host_AIL_start_sequence(void *h)
{
    ail_seq *q = seq_of(h);
    if (!q || !q->data)
        return;
    q->playing = 1;
    if (!xmidi_play(q->data, q->len, q->loop_count))
        printf("ail: start_sequence(seq=%d) - playback unavailable\n", (int)q->seq_num);
}

static void host_AIL_stop_sequence(void *h)
{
    ail_seq *q = seq_of(h);
    if (q)
        q->playing = 0;
    xmidi_stop();
}

static void host_AIL_set_sequence_volume(void *h, int32_t volume, int32_t ms)
{
    ail_seq *q = seq_of(h);
    if (q)
        q->volume = volume;
    printf("ail: set_sequence_volume(%d, over %d ms)%s\n", (int)volume, (int)ms,
           ms > 0 ? " - ramped" : "");
    /* The built-in synthesiser streams its output, so the ramp lands on the
     * next slice and really takes `ms` milliseconds (AIL semantics). The MIDI
     * Mapper backend gets the level as a channel-volume message instead. */
    synth_set_sequence_volume((int)volume, (int)ms);
    xmidi_set_volume((int)volume);
}

static void host_AIL_set_sequence_loop_count(void *h, int32_t count)
{
    ail_seq *q = seq_of(h);
    if (q)
        q->loop_count = count;
}

/* Fallback for the AIL entry points the game never calls directly.
 *
 * They still have to be blocked, not left to run: any AIL function that executes
 * its original body reaches internals that expect the driver/timer tables
 * AIL_startup would have built, and our stubs build none. Execution then wanders
 * off - that is exactly how "continue" crashed: the game reached
 * AIL_install_timbre (0x3B80F, found via the trace string "AIL_install_timbre"),
 * its body called sub_44AF0, and the flow landed in the middle of a `jz`
 * instruction at 0x3B89A where a stray 0xCC raised an unhandled breakpoint. */
static int32_t host_AIL_unused(void)
{
    return 0;
}

/* --------------------------------------------------------------- patch --- */

typedef struct {
    uint32_t    addr;
    const char *name;
    void       *impl;
} ail_entry;

static const ail_entry g_entries[] = {
    { 0x37D3E, "AIL_startup",                   (void *)host_AIL_startup                   },
    { 0x37ED8, "AIL_shutdown",                  (void *)host_AIL_shutdown                  },
    { 0x3908B, "AIL_install_DIG_INI",           (void *)host_AIL_install_DIG_INI           },
    { 0x3AA72, "AIL_install_MDI_INI",           (void *)host_AIL_install_MDI_INI           },
    { 0x392D0, "AIL_allocate_sample_handle",    (void *)host_AIL_allocate_sample_handle    },
    { 0x3ACA3, "AIL_allocate_sequence_handle",  (void *)host_AIL_allocate_sequence_handle  },
    { 0x39521, "AIL_init_sample",               (void *)host_AIL_init_sample               },
    { 0x39694, "AIL_set_sample_address",        (void *)host_AIL_set_sample_address        },
    { 0x39AAE, "AIL_set_sample_loop_count",     (void *)host_AIL_set_sample_loop_count     },
    { 0x39798, "AIL_start_sample",              (void *)host_AIL_start_sample              },
    { 0x39805, "AIL_stop_sample",               (void *)host_AIL_stop_sample               },
    { 0x3ADF5, "AIL_init_sequence",             (void *)host_AIL_init_sequence             },
    { 0x3AEEE, "AIL_start_sequence",            (void *)host_AIL_start_sequence            },
    { 0x3AF5B, "AIL_stop_sequence",             (void *)host_AIL_stop_sequence             },
    { 0x3B124, "AIL_set_sequence_volume",       (void *)host_AIL_set_sequence_volume       },
    { 0x3B1A6, "AIL_set_sequence_loop_count",   (void *)host_AIL_set_sequence_loop_count   },

    /* Everything below is never called by the game directly, but has to be
     * blocked so no original AIL code can run (see host_AIL_unused). */
    { 0x37F70, "AIL_set_preference",            (void *)host_AIL_unused },
    { 0x38074, "AIL_get_real_vect",             (void *)host_AIL_unused },
    { 0x3815F, "AIL_set_real_vect",             (void *)host_AIL_unused },
    { 0x38262, "AIL_restore_USE16_ISR",         (void *)host_AIL_unused },
    { 0x382E9, "AIL_call_driver",               (void *)host_AIL_unused },
    { 0x383F1, "AIL_delay",                     (void *)host_AIL_unused },
    { 0x38463, "AIL_API_read_INI",              (void *)host_AIL_unused },
    { 0x387BC, "AIL_register_timer",            (void *)host_AIL_unused },
    { 0x388A7, "AIL_set_timer_user",            (void *)host_AIL_unused },
    { 0x3899A, "AIL_set_timer_period",          (void *)host_AIL_unused },
    { 0x38A10, "AIL_set_timer_frequency",       (void *)host_AIL_unused },
    { 0x38BD9, "AIL_start_timer",               (void *)host_AIL_unused },
    { 0x38CA8, "AIL_stop_timer",                (void *)host_AIL_unused },
    { 0x38D77, "AIL_release_timer_handle",      (void *)host_AIL_unused },
    { 0x38DE4, "AIL_release_all_timers",        (void *)host_AIL_unused },
    { 0x38E46, "AIL_get_IO_environment",        (void *)host_AIL_unused },
    { 0x38F2B, "AIL_install_driver",            (void *)host_AIL_unused },
    { 0x3901E, "AIL_uninstall_driver",          (void *)host_AIL_unused },
    { 0x39176, "AIL_install_DIG_driver_file",   (void *)host_AIL_unused },
    { 0x394B4, "AIL_release_sample_handle",     (void *)host_AIL_unused },
    { 0x39716, "AIL_set_sample_type",           (void *)host_AIL_unused },
    { 0x398DF, "AIL_end_sample",                (void *)host_AIL_unused },
    { 0x3994C, "AIL_set_sample_playback_rate",  (void *)host_AIL_unused },
    { 0x399C2, "AIL_set_sample_volume",         (void *)host_AIL_unused },
    { 0x39A38, "AIL_set_sample_pan",            (void *)host_AIL_unused },
    { 0x39B24, "AIL_sample_status",             (void *)host_AIL_unused },
    { 0x3A644, "AIL_register_EOS_callback",     (void *)host_AIL_unused },
    { 0x3AB49, "AIL_install_MDI_driver_file",   (void *)host_AIL_unused },
    { 0x3B035, "AIL_end_sequence",              (void *)host_AIL_unused },
    { 0x3BDDF, "AIL_branch_index",              (void *)host_AIL_unused },
    { 0x3C11C, "AIL_register_event_callback",   (void *)host_AIL_unused },
    { 0x3C209, "AIL_register_timbre_callback",  (void *)host_AIL_unused },
    { 0x3C4DB, "AIL_lock_channel",              (void *)host_AIL_unused },
    { 0x3C5C0, "AIL_release_channel",           (void *)host_AIL_unused },
    { 0x3C636, "AIL_map_sequence_channel",      (void *)host_AIL_unused },
    /* Reached through a library path, not a direct call from game code: */
    { 0x3B80F, "AIL_install_timbre",            (void *)host_AIL_unused },
};

/* FDPS (������ʿ���⴫) has its own build of the same Miles AIL: identical
 * API, different addresses. The table was rebuilt from its IDA database
 * (E:\Games\FDCollection\Game\FDPS\FDPS.EXE.i64) by walking the
 * "AIL_xxx(...)\n" trace strings and every direct `call` into the library
 * region 0x3D488..0x41FFE - see re/fdps_ail_patchset.csv.
 *
 * 47 of the 90 entries are real call targets; the rest are patched so no
 * original AIL code can be entered indirectly (the same lesson FD2 taught with
 * AIL_install_timbre). 18 are called by game code itself; everything else is
 * either dead API surface or AIL calling its own internals. */
static const ail_entry g_entries_fdps[] = {
    { 0x3D488, "AIL_startup",                     (void *)host_AIL_startup,      },
    { 0x3D622, "AIL_shutdown",                    (void *)host_AIL_shutdown,     },
    { 0x3D6BA, "AIL_set_preference",              (void *)host_AIL_unused,       },
    { 0x3D7BE, "AIL_get_real_vect",               (void *)host_AIL_unused,       },
    { 0x3D8A9, "AIL_set_real_vect",               (void *)host_AIL_unused,       },
    { 0x3D922, "AIL_set_USE16_ISR",               (void *)host_AIL_unused,       },
    { 0x3D9AC, "AIL_restore_USE16_ISR",           (void *)host_AIL_unused,       },
    { 0x3DA33, "AIL_call_driver",                 (void *)host_AIL_unused,       },
    { 0x3DB3B, "AIL_delay",                       (void *)host_AIL_unused,       },
    { 0x3DBAD, "AIL_API_read_INI",                (void *)host_AIL_unused,       },
    { 0x3DF06, "AIL_register_timer",              (void *)host_AIL_register_timer, },
    { 0x3DFF1, "AIL_set_timer_user",              (void *)host_AIL_set_timer_user, },
    { 0x3E0E4, "AIL_set_timer_period",            (void *)host_AIL_set_timer_period, },
    { 0x3E15A, "AIL_set_timer_frequency",         (void *)host_AIL_set_timer_frequency, }, /* also AIL_set_timer_divisor */
    { 0x3E246, "AIL_interrupt_divisor",           (void *)host_AIL_unused,       },
    { 0x3E323, "AIL_start_timer",                 (void *)host_AIL_start_timer,  }, /* also AIL_start_all_timers */
    { 0x3E3F2, "AIL_stop_timer",                  (void *)host_AIL_stop_timer,   }, /* also AIL_stop_all_timers */
    { 0x3E4C1, "AIL_release_timer_handle",        (void *)host_AIL_release_timer_handle, },
    { 0x3E52E, "AIL_release_all_timers",          (void *)host_AIL_release_all_timers, },
    { 0x3E590, "AIL_get_IO_environment",          (void *)host_AIL_unused,       },
    { 0x3E675, "AIL_install_driver",              (void *)host_AIL_unused,       },
    { 0x3E768, "AIL_uninstall_driver",            (void *)host_AIL_unused,       },
    { 0x3E7D5, "AIL_install_DIG_INI",             (void *)host_AIL_install_DIG_INI, },
    { 0x3E8C0, "AIL_install_DIG_driver_file",     (void *)host_AIL_unused,       }, /* also AIL_uninstall_DIG_driver */
    { 0x3EA1A, "AIL_allocate_sample_handle",      (void *)host_AIL_allocate_sample_handle, },
    { 0x3EAFF, "AIL_allocate_file_sample",        (void *)host_AIL_unused,       },
    { 0x3EBFE, "AIL_release_sample_handle",       (void *)host_AIL_unused,       },
    { 0x3EC6B, "AIL_init_sample",                 (void *)host_AIL_init_sample,  },
    { 0x3ECD8, "AIL_set_sample_file",             (void *)host_AIL_unused,       },
    { 0x3EDDE, "AIL_set_sample_address",          (void *)host_AIL_set_sample_address, },
    { 0x3EE60, "AIL_set_sample_type",             (void *)host_AIL_set_sample_type, },
    { 0x3EEE2, "AIL_start_sample",                (void *)host_AIL_start_sample, },
    { 0x3EF4F, "AIL_stop_sample",                 (void *)host_AIL_stop_sample,  }, /* also AIL_resume_sample */
    { 0x3F029, "AIL_end_sample",                  (void *)host_AIL_unused,       },
    { 0x3F096, "AIL_set_sample_playback_rate",    (void *)host_AIL_set_sample_playback_rate, },
    { 0x3F10C, "AIL_set_sample_volume",           (void *)host_AIL_set_sample_volume, },
    { 0x3F182, "AIL_set_sample_pan",              (void *)host_AIL_unused,       },
    { 0x3F1F8, "AIL_set_sample_loop_count",       (void *)host_AIL_set_sample_loop_count, },
    { 0x3F26E, "AIL_sample_status",               (void *)host_AIL_sample_status, },
    { 0x3F353, "AIL_sample_playback_rate",        (void *)host_AIL_unused,       },
    { 0x3F444, "AIL_sample_volume",               (void *)host_AIL_unused,       },
    { 0x3F529, "AIL_sample_pan",                  (void *)host_AIL_unused,       },
    { 0x3F60E, "AIL_sample_loop_count",           (void *)host_AIL_unused,       },
    { 0x3F6F3, "AIL_install_DIG_driver_image",    (void *)host_AIL_unused,       },
    { 0x3F7EC, "AIL_minimum_sample_buffer_size",  (void *)host_AIL_unused,       },
    { 0x3F8E5, "AIL_sample_buffer_ready",         (void *)host_AIL_unused,       },
    { 0x3F9CA, "AIL_load_sample_buffer",          (void *)host_AIL_unused,       }, /* also AIL_set_sample_position */
    { 0x3FACF, "AIL_sample_position",             (void *)host_AIL_unused,       },
    { 0x3FBB4, "AIL_register_SOB_callback",       (void *)host_AIL_unused,       },
    { 0x3FCA1, "AIL_register_EOB_callback",       (void *)host_AIL_unused,       },
    { 0x3FD8E, "AIL_register_EOS_callback",       (void *)host_AIL_unused,       },
    { 0x3FE7B, "AIL_register_EOF_callback",       (void *)host_AIL_unused,       },
    { 0x3FF68, "AIL_set_sample_user_data",        (void *)host_AIL_unused,       },
    { 0x3FFEA, "AIL_sample_user_data",            (void *)host_AIL_unused,       },
    { 0x400D7, "AIL_active_sample_count",         (void *)host_AIL_unused,       },
    { 0x401BC, "AIL_install_MDI_INI",             (void *)host_AIL_install_MDI_INI, },
    { 0x40293, "AIL_install_MDI_driver_file",     (void *)host_AIL_unused,       }, /* also AIL_uninstall_MDI_driver */
    { 0x403ED, "AIL_allocate_sequence_handle",    (void *)host_AIL_allocate_sequence_handle, }, /* also AIL_release_sequence_handle */
    { 0x4053F, "AIL_init_sequence",               (void *)host_AIL_init_sequence, }, /* also AIL_start_sequence */
    { 0x406A5, "AIL_stop_sequence",               (void *)host_AIL_stop_sequence, }, /* also AIL_resume_sequence */
    { 0x4077F, "AIL_end_sequence",                (void *)host_AIL_unused,       },
    { 0x407EC, "AIL_set_sequence_tempo",          (void *)host_AIL_unused,       },
    { 0x4086E, "AIL_set_sequence_volume",         (void *)host_AIL_set_sequence_volume, }, /* also AIL_set_sequence_loop_count */
    { 0x40966, "AIL_sequence_status",             (void *)host_AIL_unused,       },
    { 0x40A4B, "AIL_sequence_tempo",              (void *)host_AIL_unused,       },
    { 0x40B30, "AIL_sequence_volume",             (void *)host_AIL_unused,       },
    { 0x40C15, "AIL_sequence_loop_count",         (void *)host_AIL_unused,       },
    { 0x40CFA, "AIL_install_MDI_driver_image",    (void *)host_AIL_unused,       }, /* also AIL_set_GTL_filename_prefix */
    { 0x40E60, "AIL_timbre_status",               (void *)host_AIL_unused,       },
    { 0x40F59, "AIL_install_timbre",              (void *)host_AIL_unused,       },
    { 0x41052, "AIL_protect_timbre",              (void *)host_AIL_unused,       },
    { 0x410D4, "AIL_unprotect_timbre",            (void *)host_AIL_unused,       },
    { 0x41156, "AIL_active_sequence_count",       (void *)host_AIL_unused,       },
    { 0x4123B, "AIL_controller_value",            (void *)host_AIL_unused,       },
    { 0x41334, "AIL_channel_notes",               (void *)host_AIL_unused,       },
    { 0x41421, "AIL_sequence_position",           (void *)host_AIL_unused,       },
    { 0x41529, "AIL_branch_index",                (void *)host_AIL_unused,       },
    { 0x4159F, "AIL_register_prefix_callback",    (void *)host_AIL_unused,       },
    { 0x4168C, "AIL_register_trigger_callback",   (void *)host_AIL_unused,       },
    { 0x41779, "AIL_register_sequence_callback",  (void *)host_AIL_unused,       },
    { 0x41866, "AIL_register_event_callback",     (void *)host_AIL_unused,       },
    { 0x41953, "AIL_register_timbre_callback",    (void *)host_AIL_unused,       },
    { 0x41A40, "AIL_set_sequence_user_data",      (void *)host_AIL_unused,       },
    { 0x41AC2, "AIL_sequence_user_data",          (void *)host_AIL_unused,       }, /* also AIL_register_ICA_array */
    { 0x41C25, "AIL_lock_channel",                (void *)host_AIL_unused,       },
    { 0x41D0A, "AIL_release_channel",             (void *)host_AIL_unused,       },
    { 0x41D80, "AIL_map_sequence_channel",        (void *)host_AIL_unused,       },
    { 0x41E02, "AIL_true_sequence_channel",       (void *)host_AIL_unused,       },
    { 0x41EEF, "AIL_send_channel_voice_message",  (void *)host_AIL_unused,       }, /* also AIL_send_sysex_message */
    { 0x41FFE, "AIL_create_wave_synthesizer",     (void *)host_AIL_unused,       }, /* also AIL_destroy_wave_synthesizer */
};

void ail_set_format(uint32_t sample_rate, int bits, int stereo)
{
    if (sample_rate >= 4000 && sample_rate <= 192000)
        g_rate = sample_rate;
    if (bits == 8 || bits == 16)
        g_bits = bits;
    g_stereo = stereo ? 1 : 0;
}

static void install_table(uint8_t *obj0_base, const char *dump_dir,
                          const ail_entry *tab, size_t ntab, const char *what)
{
    size_t i;

    if (g_installed)
        return;
    g_installed = 1;
    if (dump_dir && dump_dir[0])
        strncpy(g_dump_dir, dump_dir, sizeof g_dump_dir - 1);

    for (i = 0; i < ntab; i++) {
        const ail_entry *e = &tab[i];
        uint8_t *p = obj0_base + (e->addr - AIL_OBJ0_BASE);
        intptr_t rel = (intptr_t)e->impl - (intptr_t)(p + 5);
        p[0] = 0xE9;                        /* jmp rel32 */
        *(int32_t *)(p + 1) = (int32_t)rel;
    }
    if (!g_timer_cs_ready) {
        InitializeCriticalSection(&g_timer_cs);
        g_timer_cs_ready = 1;
    }
    printf("ail: patched %u AIL entry points (%s) to host implementations"
           " (%u Hz, %d-bit, %d ch)\n",
           (unsigned)ntab, what,
           (unsigned)g_rate, g_bits, g_stereo ? 2 : 1);
}

void ail_install(uint8_t *obj0_base, const char *dump_dir)
{
    install_table(obj0_base, dump_dir, g_entries,
                  sizeof g_entries / sizeof g_entries[0], "FD2 layout");
}

void ail_install_fdps(uint8_t *obj0_base, const char *dump_dir)
{
    install_table(obj0_base, dump_dir, g_entries_fdps,
                  sizeof g_entries_fdps / sizeof g_entries_fdps[0],
                  "FDPS layout");
}
