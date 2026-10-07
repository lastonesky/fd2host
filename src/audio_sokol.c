/* audio_sokol.c - the one device and the one software mixer.
 *
 * sokol_audio on Windows is WASAPI (linked in-source via #pragma comment) and
 * does not need sokol_app, so both render backends - and the console *check
 * targets that link ail.c - can share it. It is a *pull* API: the backend
 * thread calls mix_cb() asking for N frames, which is exactly the shape a
 * mixer wants, and it removes the stream thread synth.c used to run.
 *
 * What the mixer sums (see audio.h for why the gains stay upstream):
 *
 *     out = music slice + Σ SFX voices        (mono float)
 *
 * Everything runs under one recursive critical section, so the guest thread
 * can swap the music source or kill a voice while the device is mixing.
 *
 * Telemetry every AUDIO_TELEMETRY_MS is the audio half of "the判据 has to
 * reach the device" (docs/PITFALLS.md §8-54): frame count advancing means
 * the device is consuming, peak > 0 means the data is not silent, and the
 * music/hold state says which level the slices were rendered at.
 */
#define SOKOL_AUDIO_IMPL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sokol_audio.h"
#include "platform.h"
#include "audio.h"

#define MAX_VOICES   8                  /* AIL_MAX_SAMPLES: one per handle  */
#define MUS_FRAMES   4096                /* scratch handed to the music fn   */
#define DUMP_FRAMES  4096

typedef struct {
    float    *pcm;                       /* mono float, gain already baked in */
    unsigned  frames;
    double    pos;                       /* fractional source frame           */
    double    step;                      /* src_rate / device_rate            */
    int       active;
} voice;

static plat_mutex       g_cs;
static int              g_cs_ready;
static int              g_up;
static unsigned         g_rate;

static audio_music_fn   g_music;
static int              g_hold;          /* silence until the game says go    */
static uint64_t         g_arm_t0;        /* when the gate was closed          */
static int16_t          g_mus[MUS_FRAMES];
static unsigned         g_mus_len, g_mus_pos;

static voice            g_voice[MAX_VOICES];

static uint64_t         g_mixed;         /* frames handed to the device       */
static int              g_music_peak;    /* last block, int16 scale, sources  */
static int              g_sfx_peak;
static int              g_sfx;           /* voices still sounding             */
static uint64_t         g_last_tel;
static unsigned long    g_tel_mixed;     /* frames at the previous telemetry  */

/* Optional record of exactly what went to the device (int16 mono WAV). */
static FILE            *g_dump;
static uint64_t         g_dump_frames;
static int16_t          g_dump_buf[DUMP_FRAMES];
static unsigned         g_dump_n;

void audio_lock(void)   { if (g_cs_ready) plat_mutex_lock(&g_cs); }
void audio_unlock(void) { if (g_cs_ready) plat_mutex_unlock(&g_cs); }

/* ---------------------------------------------------------- convert ----- */
/* 8-bit waveOut PCM is unsigned (silence 128), 16-bit signed; both already
 * carry the master x AIL volume gain and the 3 ms ramp ail.c baked in.
 * Normalised to ±1.0 so the mixer can just add. */
static float *pcm_to_mono(const uint8_t *src, unsigned bytes, unsigned bits,
                          unsigned channels, unsigned *out_frames)
{
    unsigned frames, i;
    float   *dst;

    if (!channels)
        channels = 1;
    frames = (bits > 8) ? bytes / (2u * channels) : bytes / channels;
    if (!frames)
        return NULL;
    dst = (float *)malloc(frames * sizeof(float));
    if (!dst)
        return NULL;

    if (bits <= 8) {
        if (channels == 1) {
            for (i = 0; i < frames; i++)
                dst[i] = ((int)src[i] - 128) * (1.0f / 128.0f);
        } else {
            for (i = 0; i < frames; i++) {
                int a = src[i * channels] - 128;
                int b = src[i * channels + 1] - 128;
                dst[i] = (float)((a + b) * 0.5) * (1.0f / 128.0f);
            }
        }
    } else {
        const int16_t *p = (const int16_t *)(const void *)src;
        if (channels == 1) {
            for (i = 0; i < frames; i++)
                dst[i] = (float)p[i] * (1.0f / 32768.0f);
        } else {
            for (i = 0; i < frames; i++) {
                int a = p[i * channels];
                int b = p[i * channels + 1];
                dst[i] = (float)((a + b) / 2) * (1.0f / 32768.0f);
            }
        }
    }
    *out_frames = frames;
    return dst;
}

/* ------------------------------------------------------------ dump ------ */
static void dump_hdr(int patch)
{
    uint8_t h[44];
    uint32_t data = (uint32_t)(g_dump_frames * 2);
    uint32_t rate = g_rate ? g_rate : 22050;

    memset(h, 0, sizeof h);
    memcpy(h + 0, "RIFF", 4);
    h[4]  = (uint8_t)(36 + data);        h[5]  = (uint8_t)((36 + data) >> 8);
    h[6]  = (uint8_t)((36 + data) >> 16); h[7] = (uint8_t)((36 + data) >> 24);
    memcpy(h + 8, "WAVEfmt ", 8);
    h[16] = 16; h[20] = 1; h[22] = 1;
    h[24] = (uint8_t)rate;          h[25] = (uint8_t)(rate >> 8);
    h[26] = (uint8_t)(rate >> 16);  h[27] = (uint8_t)(rate >> 24);
    h[28] = (uint8_t)(rate * 2);    h[29] = (uint8_t)((rate * 2) >> 8);
    h[30] = (uint8_t)((rate * 2) >> 16); h[31] = (uint8_t)((rate * 2) >> 24);
    h[32] = 2; h[34] = 16;
    memcpy(h + 36, "data", 4);
    h[40] = (uint8_t)data; h[41] = (uint8_t)(data >> 8);
    h[42] = (uint8_t)(data >> 16); h[43] = (uint8_t)(data >> 24);
    fseek(g_dump, 0, SEEK_SET);
    fwrite(h, 1, sizeof h, g_dump);
    (void)patch;
}

static void dump_push(const float *src, int nframes)
{
    int i;

    if (!g_dump)
        return;
    for (i = 0; i < nframes; i++) {
        float v = src[i] * 32767.0f;
        if (v > 32767.0f) v = 32767.0f;
        if (v < -32768.0f) v = -32768.0f;
        g_dump_buf[g_dump_n++] = (int16_t)v;
        if (g_dump_n == DUMP_FRAMES) {
            fwrite(g_dump_buf, 2, g_dump_n, g_dump);
            g_dump_frames += g_dump_n;
            g_dump_n = 0;
        }
    }
}

void audio_dump_open(const char *path)
{
    if (!path || !path[0])
        return;
    g_dump = fopen(path, "wb");
    if (!g_dump) {
        printf("audio: cannot write %s\n", path);
        return;
    }
    dump_hdr(0);
    printf("audio: recording the mixed output to %s\n", path);
}

static void dump_close(void)
{
    if (!g_dump)
        return;
    if (g_dump_n) {
        fwrite(g_dump_buf, 2, g_dump_n, g_dump);
        g_dump_frames += g_dump_n;
        g_dump_n = 0;
    }
    dump_hdr(1);
    fclose(g_dump);
    printf("audio: dumped %.1f s of mixed PCM (%llu frames)\n",
           g_rate ? (double)g_dump_frames / (double)g_rate : 0.0,
           (unsigned long long)g_dump_frames);
    g_dump = NULL;
}

/* ------------------------------------------------------------ mixer ----- */
static void mix_cb(float *out, int nframes, int nch)
{
    audio_music_fn fn;
    int held, i, ch, v, sfx = 0;
    float peak_m = 0.0f, peak_s = 0.0f;
    int    tel = 0;

    if (nch < 1)
        nch = 1;

    audio_lock();
    if (!g_up) {
        memset(out, 0, (size_t)nframes * (size_t)nch * sizeof(float));
        audio_unlock();
        return;
    }
    /* Start gate safety net: play_bgm asks for a level within microseconds,
     * so a gate still closed here means nobody ever asked (docs/AUDIO.md
     * §11.10). 200 ms of silence is the worst case; forever would be a bug. */
    if (g_hold && (plat_now_ms() - g_arm_t0) >= AUDIO_ARM_MS) {
        printf("audio: no volume request after %d ms - opening the music gate "
               "anyway\n", AUDIO_ARM_MS);
        g_hold = 0;
    }
    fn   = g_music;
    held = g_hold;

    for (i = 0; i < nframes; i++) {
        float m = 0.0f, s = 0.0f, x;

        /* music: pull on demand - the slice is rendered with the level the
         * game asked for *now*, which is what killed the stale-level burst */
        if (fn && !held) {
            if (g_mus_pos >= g_mus_len) {
                g_mus_len = fn(g_mus, MUS_FRAMES);
                g_mus_pos = 0;
            }
            if (g_mus_pos < g_mus_len) {
                m = (float)g_mus[g_mus_pos] * (1.0f / 32768.0f);
                g_mus_pos++;
            }
        }

        for (v = 0; v < MAX_VOICES; v++) {
            voice   *p = &g_voice[v];
            unsigned i0;
            double   frac;
            if (!p->active || !p->pcm || p->frames < 2)
                continue;
            i0 = (unsigned)p->pos;
            if (i0 + 1 >= p->frames) {
                p->active = 0;
                continue;
            }
            frac = p->pos - (double)i0;
            s += (float)(p->pcm[i0] +
                         (p->pcm[i0 + 1] - p->pcm[i0]) * frac);
            p->pos += p->step;
            if (p->pos >= (double)(p->frames - 1))
                p->active = 0;
        }

        x = m + s;
        if (x > 1.0f)  x = 1.0f;
        if (x < -1.0f) x = -1.0f;
        for (ch = 0; ch < nch; ch++)
            out[i * nch + ch] = x;
        if (fabsf(m) > peak_m)
            peak_m = fabsf(m);
        if (fabsf(s) > peak_s)
            peak_s = fabsf(s);
    }

    /* voices that ran out during this block: free now, under the lock */
    for (v = 0; v < MAX_VOICES; v++)
        if (!g_voice[v].active && g_voice[v].pcm) {
            free(g_voice[v].pcm);
            g_voice[v].pcm = NULL;
        }
    for (v = 0; v < MAX_VOICES; v++)
        if (g_voice[v].active)
            sfx++;

    g_mixed      += (uint64_t)nframes;
    g_music_peak  = (int)(peak_m * 32767.0f);
    g_sfx_peak    = (int)(peak_s * 32767.0f);
    g_sfx         = sfx;
    {
        uint64_t now = plat_now_ms();
        if (now - g_last_tel >= AUDIO_TELEMETRY_MS) {
            g_last_tel  = now;
            g_tel_mixed = (unsigned long)g_mixed;
            tel = 1;
        }
    }
    if (tel) {
        /* Device-level evidence, split by source: frames advancing = the
         * device is consuming, music peak > 0 = the music really sounds,
         * sfx peak/voices = the effects do too (docs/PITFALLS.md §8-54). */
        printf("audio: mixed %llu frames @%u Hz, music=%s%s peak %d/32767, "
               "sfx voices=%d peak %d/32767\n",
               (unsigned long long)g_mixed, g_rate,
               fn ? "on" : "off", held ? " (held)" : "",
               g_music_peak, sfx, g_sfx_peak);
    }
    if (g_dump)
        dump_push(out, nframes);
    audio_unlock();
}

/* ------------------------------------------------------------- api ------ */
int audio_init(unsigned want_rate)
{
    saudio_desc desc;

    if (g_up)
        return 1;
    plat_mutex_init(&g_cs);
    g_cs_ready = 1;
    memset(&desc, 0, sizeof desc);
    desc.sample_rate    = (int)(want_rate ? want_rate : 22050);
    desc.num_channels   = 1;
    desc.buffer_frames  = 2048;         /* ~93 ms @22050: one music slice   */
    desc.stream_cb      = mix_cb;
    saudio_setup(&desc);
    if (!saudio_isvalid()) {
        printf("audio: saudio_setup(%u Hz) failed - no sound this run "
               "(the game keeps running)\n",
               (unsigned)(want_rate ? want_rate : 22050));
        return 0;
    }
    g_up      = 1;
    g_rate    = (unsigned)saudio_sample_rate();
    g_last_tel = plat_now_ms();
    printf("audio: device up - WASAPI via sokol_audio, %u Hz, mono, "
           "%d frames/buffer (opened once, music + SFX mixed here)\n",
           g_rate, saudio_buffer_frames());
    return 1;
}

void audio_close(void)
{
    int v;

    if (!g_up) {
        dump_close();
        return;
    }
    audio_lock();
    g_up    = 0;
    g_music = NULL;
    for (v = 0; v < MAX_VOICES; v++) {
        free(g_voice[v].pcm);
        memset(&g_voice[v], 0, sizeof g_voice[v]);
    }
    audio_unlock();

    saudio_shutdown();
    printf("audio: device closed (%llu frames mixed)\n",
           (unsigned long long)g_mixed);
    dump_close();
    plat_mutex_destroy(&g_cs);
    g_cs_ready = 0;
}

int audio_isup(void)      { return g_up; }
unsigned audio_rate(void) { return g_rate; }

void audio_set_music(audio_music_fn fn)
{
    audio_lock();
    g_music   = fn;
    g_mus_len = g_mus_pos = 0;      /* drop whatever the old track left     */
    audio_unlock();
}

audio_music_fn audio_music_src(void)
{
    audio_music_fn fn;
    audio_lock();
    fn = g_music;
    audio_unlock();
    return fn;
}

void audio_hold_music(void)
{
    audio_lock();
    g_hold   = 1;
    g_arm_t0 = plat_now_ms();
    audio_unlock();
}

void audio_release_music(void)
{
    audio_lock();
    g_hold = 0;
    audio_unlock();
}

int audio_music_held(void)
{
    int h;
    audio_lock();
    h = g_hold;
    audio_unlock();
    return h;
}

int audio_sfx_play(int voice, uint8_t *pcm, unsigned bytes,
                   unsigned rate, unsigned channels, unsigned bits)
{
    float   *f;
    unsigned frames = 0;
    double   step;

    if (voice < 0 || voice >= MAX_VOICES || !pcm || !bytes) {
        free(pcm);
        return 0;
    }
    if (!rate)
        rate = 11025;
    f = pcm_to_mono(pcm, bytes, bits, channels, &frames);
    free(pcm);                          /* the mixer owns its own float copy */
    if (!f || !frames) {
        free(f);
        return 0;
    }
    step = g_rate ? ((double)rate / (double)g_rate) : 1.0;

    audio_lock();
    free(g_voice[voice].pcm);
    g_voice[voice].pcm    = f;
    g_voice[voice].frames = frames;
    g_voice[voice].pos    = 0.0;
    g_voice[voice].step   = step;
    g_voice[voice].active = 1;
    audio_unlock();
    return 1;
}

int audio_sfx_stop(int voice)
{
    int was;

    if (voice < 0 || voice >= MAX_VOICES)
        return 0;
    audio_lock();
    was = g_voice[voice].active;
    if (was) {
        free(g_voice[voice].pcm);
        memset(&g_voice[voice], 0, sizeof g_voice[voice]);
    }
    audio_unlock();
    return was;
}

int audio_sfx_active(int voice)
{
    int a;

    if (voice < 0 || voice >= MAX_VOICES)
        return 0;
    audio_lock();
    a = g_voice[voice].active;
    audio_unlock();
    return a;
}
