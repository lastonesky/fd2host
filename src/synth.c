/* synth.c - self-contained software synthesiser for FD2's music.
 *
 * Why not just use the Windows MIDI Mapper?
 * -----------------------------------------
 * The only MIDI device on a stock Windows install is "Microsoft GS Wavetable
 * Synth". It is a *system* component: its level is a system mixer setting
 * ("SW Synth"/MIDI) that can be muted independently of everything else, it
 * needs the gm.dls sound bank, and midiOutShortMsg failures are easy to miss.
 * When it is silent there is nothing the game can do about it.
 *
 * The sound effects already prove that waveOut works, so the music is rendered
 * here instead: the parsed MIDI events drive a small wavetable/FM-ish
 * synthesiser, the result is mixed to 16-bit mono PCM once, and that buffer is
 * handed to waveOut (optionally resubmitted to loop). No system MIDI involved.
 *
 * Sound model: one voice per MIDI channel (the game uses 11 of them), a sine
 * with two harmonics as the waveform, a linear attack/decay/sustain/release
 * envelope, and a per-voice phase accumulator. Sample-accurate note timing
 * comes from the event list's tick stamps.
 */

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "synth.h"
#include "dls.h"

#define PI              3.14159265358979
#define VOICES          64          /* drums + melody + sustained pads          */
#define NOTE_MAX_MS     2500.0      /* auto-release when a note never stops    */
#define PERC_MAX_MS     250.0       /* drums are short - release them early    */
#define TABLE_BITS      10
#define TABLE_SIZE      (1 << TABLE_BITS)
#define TABLE_MASK      (TABLE_SIZE - 1)

#define ATTACK_MS       4.0
#define DECAY_MS        90.0
#define RELEASE_MS      150.0
#define PERC_RELEASE_MS 60.0        /* channel 9: drum-ish, quick decay       */
#define SUSTAIN         0.70

typedef struct {
    int      active;
    int      channel;
    int      note;
    int      volume;
    uint32_t start;                 /* sample index of the note-on            */
    uint32_t release;               /* sample index of note-off (0 = none)    */
    double   phase;                 /* 0..1 (waveform voices)                 */
    double   step;                  /* phase advance per sample               */
    int      percussive;
    double   gain;                  /* channel volume (CC7) * expression (CC11) */
    const dls_wave *w;              /* GM sample, NULL = waveform fallback    */
    double   pos;                   /* position within the sample, in frames  */
    double   sstep;                 /* frames per output sample               */
} voice;

static voice        g_voices[VOICES];
static int16_t      g_wave[TABLE_SIZE];
static int          g_wave_ready;

/* General MIDI bank (gm.dls) and the per-channel program/bank state that
 * decides which sample a note uses. */
static dls_bank     g_bank;
static int          g_bank_tried;
static const char  *g_dls_path;              /* --gm-bank=<path> override  */
static int          g_channel_program[16];
static int          g_channel_bank[16];
static int          g_channel_volume[16];    /* CC7  */
static int          g_channel_expr[16];      /* CC11 */
static long         g_sampled_notes;         /* note-ons that found a sample */
static long         g_drum_notes;            /* note-ons on the percussion channel */

static HWAVEOUT     g_dev;
static uint32_t     g_rate = 22050;
static HANDLE       g_thread;
static volatile LONG g_stop_flag;
static int          g_loop;
static long         g_notes;
static int          g_peak_voices;

/* Host master volume 0..100 (--volume, default 100 = no attenuation).
 * Multiplied with the sequence volume when each slice is rendered - see
 * synth.h and stream_gain(). */
static int          g_master = 100;

/* ------------------------------------------------------------- waveform -- */

static void wave_init(void)
{
    int i;

    if (g_wave_ready)
        return;
    for (i = 0; i < TABLE_SIZE; i++) {
        double ph = (double)i / (double)TABLE_SIZE;
        /* Fundamental plus two harmonics: not exactly any GM instrument, but
         * far more musical than a bare sine. */
        double v = 0.70 * sin(2.0 * PI * ph) +
                   0.20 * sin(4.0 * PI * ph) +
                   0.10 * sin(6.0 * PI * ph);
        g_wave[i] = (int16_t)(v * 7000.0);
    }
    g_wave_ready = 1;
}

static double note_hz(int note)
{
    return 440.0 * pow(2.0, ((double)note - 69.0) / 12.0);
}

/* ------------------------------------------------------------- envelope -- */

static double envelope(const voice *v, uint32_t t)
{
    uint32_t a = (uint32_t)(ATTACK_MS * (double)g_rate / 1000.0);
    uint32_t d = (uint32_t)(DECAY_MS * (double)g_rate / 1000.0);
    uint32_t r = (uint32_t)((v->percussive ? PERC_RELEASE_MS : RELEASE_MS) *
                            (double)g_rate / 1000.0);
    uint32_t age = t - v->start;
    double e;

    /* A DLS/General MIDI wave already carries the instrument's own envelope:
     * a piano sample decays, strings sustain, drums are one-shots. Shaping it
     * with the synthetic attack/decay/sustain curve below made every
     * instrument sound the same and hollow - it flattened the very thing that
     * makes a timbre recognisable. Samples are therefore played as recorded,
     * with only a 1 ms attack (no click) and the release ramp where the note
     * actually ends. */
    if (v->w) {
        uint32_t wa = (uint32_t)(1.0 * (double)g_rate / 1000.0);
        if (wa && age < wa)
            return (double)(age + 1) / (double)wa;
        e = 1.0;
        if (v->release && t >= v->release) {
            uint32_t rt = t - v->release;
            if (r && rt >= r)
                return 0.0;                     /* finished */
            if (r)
                e *= 1.0 - (double)rt / (double)r;
        }
        return e;
    }

    if (a && age < a)
        return (double)(age + 1) / (double)a;    /* never exactly zero: a
                                                  * voice must survive its
                                                  * own note-on sample */
    e = 1.0;
    if (d && age - a < d)
        e = 1.0 - (1.0 - SUSTAIN) * (double)(age - a) / (double)d;
    else
        e = SUSTAIN;
    if (v->release) {
        if (t >= v->release) {
            uint32_t rt = t - v->release;
            if (r && rt >= r)
                return 0.0;                     /* finished */
            if (r)
                e *= 1.0 - (double)rt / (double)r;
        }
    }
    return e;
}

/* --------------------------------------------------------------- events -- */

/* Find (or free) the voice for a note. The game's XMIDI data contains very few
 * note-offs, so a channel may legitimately carry several notes at once: rather
 * than monophonically stealing the channel, each (channel, note) pair gets its
 * own voice and unstoppable notes are released by timeout in the mixer. */
static voice *voice_alloc(int channel, int note, uint32_t t)
{
    int k, oldest = -1;

    for (k = 0; k < VOICES; k++) {
        voice *v = &g_voices[k];
        if (v->active && v->channel == channel && v->note == note)
            return v;                               /* retrigger same note  */
    }
    for (k = 0; k < VOICES; k++)
        if (!g_voices[k].active)
            return &g_voices[k];
    for (k = 0; k < VOICES; k++)                    /* steal the oldest     */
        if (oldest < 0 || g_voices[k].start < g_voices[oldest].start)
            oldest = k;
    return &g_voices[oldest];
}

static void apply_event(const synth_event *e, uint32_t t)
{
    uint8_t st = e->msg[0];
    int ch = st & 0x0F;

    if ((st & 0xF0) == 0xC0) {                  /* program change        */
        g_channel_program[ch] = e->msg[1];
        return;
    }
    if ((st & 0xF0) == 0xB0) {                  /* controllers           */
        int cc = e->msg[1], val = e->msg[2];
        if (cc == 0)
            g_channel_bank[ch] = val << 7;      /* bank select MSB       */
        else if (cc == 32)
            g_channel_bank[ch] = (g_channel_bank[ch] & 0x3F80) | val;
        else if (cc == 7)
            g_channel_volume[ch] = val;         /* channel volume        */
        else if (cc == 11)
            g_channel_expr[ch] = val;           /* expression            */
        return;
    }

    if ((st & 0xF0) == 0x90 && e->msg[2] != 0) {
        voice *v = voice_alloc(ch, e->msg[1], t);
        v->active = 1;
        v->channel = ch;
        v->note = e->msg[1];
        v->volume = e->msg[2];
        v->start = t;
        v->release = 0;
        v->phase = 0.0;
        v->pos = 0.0;
        v->percussive = (ch == 9);
        /* MIDI bank select is (CC0 << 7) | CC32, but DLS stores (msb << 8) |
         * lsb - and percussion is not a bank number at all, it is the F_DRUMS
         * bit in ulBank (see dls_find). */
        {
            uint32_t midi_bank = (uint32_t)g_channel_bank[ch];
            uint32_t dls_bank = ((midi_bank >> 7) << 8) | (midi_bank & 0x7Fu);
            v->w = g_bank.ninsts
                 ? dls_find(&g_bank, dls_bank, g_channel_program[ch],
                            v->note, ch == 9)
                 : NULL;
        }
        v->gain = ((double)g_channel_volume[ch] / 127.0) *
                  ((double)g_channel_expr[ch] / 127.0);
        if (ch == 9)
            g_drum_notes++;
        if (v->w) {
            /* Resample the GM wave: it was recorded at its own rate and unity
             * note, so the ratio is note/unity (plus the region's fine tune). */
            double ratio = note_hz(v->note) / note_hz(v->w->unity_note) *
                           pow(2.0, (double)v->w->fine_tune / 1200.0);
            v->sstep = ratio * (double)v->w->sample_rate / (double)g_rate;
            g_sampled_notes++;
            /* The first few notes print the whole pitch calculation, so a bad
             * unity note / sample rate / output rate is visible in the log
             * instead of only being audible. */
            if (g_sampled_notes <= 6)
                printf("synth: ch%d note %d vel %d -> wave(unity %d, %u Hz, "
                       "fine %d, %u frames) ratio %.4f sstep %.4f "
                       "(note %.1f Hz, sample sounds %.1f Hz)\n",
                       ch, v->note, v->volume, (int)v->w->unity_note,
                       (unsigned)v->w->sample_rate, (int)v->w->fine_tune,
                       (unsigned)v->w->samples, ratio, v->sstep,
                       note_hz(v->note),
                       note_hz(v->w->unity_note) / pow(2.0,
                           (double)v->w->fine_tune / 1200.0));
        } else {
            v->step = note_hz(v->note) / (double)g_rate;
        }
        g_notes++;
    } else if ((st & 0xF0) == 0x80 ||
               ((st & 0xF0) == 0x90 && e->msg[2] == 0)) {
        int k;
        for (k = 0; k < VOICES; k++) {
            voice *v = &g_voices[k];
            if (v->active && v->channel == ch && v->note == e->msg[1] && !v->release)
                v->release = t ? t : 1;
        }
    }
}

/* -------------------------------------------------------------- render --- */

/* ---------------------------------------------------------- streaming ----
 *
 * The original AIL MDI driver synthesises incrementally: its timer callback
 * renders the next slice and hands it to the sound device, so the music is
 * always "in flight" and a volume change takes effect on the very next slice.
 * Rendering the whole piece into one buffer and handing that to waveOut (what
 * this file used to do) turns the volume into a constant baked in at render
 * time - AIL_set_sequence_volume(seq, vol, ms) then has nothing to act on, and
 * the game's 2 s fade-ins / 4 s fade-outs are silently dropped.
 *
 * The render state below (sample clock, event cursor, voices) lives in globals
 * so any number of slices can be rendered back to back; render() at the end is
 * just "one slice as long as the whole song". */

static const synth_event *g_ev;
static int                g_ev_count;
static double             g_tick_rate;
static uint32_t           g_total;      /* song length in samples            */
static uint32_t           g_t;          /* sample clock                      */
static int                g_ei;         /* next event to apply               */

static void stream_init(const synth_event *ev, int count, double tick_rate,
                        uint32_t rate)
{
    int i;

    g_rate = rate;
    wave_init();
    g_ev        = ev;
    g_ev_count  = count;
    g_tick_rate = tick_rate;
    g_total = (uint32_t)((double)ev[count - 1].tick / tick_rate * (double)rate) + rate;
    if (g_total < rate)
        g_total = rate;
    g_t  = 0;
    g_ei = 0;

    memset(g_voices, 0, sizeof g_voices);
    g_notes = 0;
    g_peak_voices = 0;
    g_sampled_notes = 0;
    g_drum_notes = 0;
    memset(g_channel_program, 0, sizeof g_channel_program);
    memset(g_channel_bank, 0, sizeof g_channel_bank);
    for (i = 0; i < 16; i++) {
        g_channel_volume[i] = 127;      /* GM default: full volume            */
        g_channel_expr[i] = 127;
    }
}

/* Render up to `n` samples into `dst`, advancing the clock. `gain` (0..1) is
 * applied per sample, which is what lets the volume change while playing. */
static uint32_t stream_fill(int16_t *dst, uint32_t n, double gain)
{
    uint32_t written = 0;
    uint32_t max_age  = (uint32_t)(NOTE_MAX_MS * (double)g_rate / 1000.0);
    uint32_t perc_age = (uint32_t)(PERC_MAX_MS * (double)g_rate / 1000.0);

    while (written < n) {
        uint32_t next_t;
        int32_t acc = 0;
        int k, nactive = 0;

        if (g_t >= g_total) {
            if (g_loop) {
                /* Rewind the sequence. The ringing voices are dropped rather
                 * than carried over: their start stamps belong to the old
                 * timeline and would produce negative envelope ages. */
                memset(g_voices, 0, sizeof g_voices);
                g_t  = 0;
                g_ei = 0;
            } else {
                memset(&dst[written], 0, (size_t)(n - written) * sizeof(int16_t));
                return n;
            }
        }

        /* Apply every event that is due at or before this sample. */
        while (g_ei < g_ev_count) {
            next_t = (uint32_t)((double)g_ev[g_ei].tick / g_tick_rate * (double)g_rate);
            if (next_t > g_t)
                break;
            apply_event(&g_ev[g_ei], g_t);
            g_ei++;
        }

        for (k = 0; k < VOICES; k++)
            if (g_voices[k].active)
                nactive++;
        if (nactive > g_peak_voices)
            g_peak_voices = nactive;

        if (nactive == 0) {
            /* Silence: skip straight to the next event instead of walking
             * sample by sample (the sequence is ~5 minutes long). */
            next_t = (g_ei < g_ev_count)
                   ? (uint32_t)((double)g_ev[g_ei].tick / g_tick_rate * (double)g_rate)
                   : g_total;
            if (next_t <= g_t)
                next_t = g_t + 1;
            if (next_t > g_total)
                next_t = g_total;
            if (next_t - g_t > n - written)      /* never overrun the slice */
                next_t = g_t + (n - written);
            memset(&dst[written], 0, (size_t)(next_t - g_t) * sizeof(int16_t));
            written += next_t - g_t;
            g_t = next_t;
            continue;
        }

        for (k = 0; k < VOICES; k++) {
            voice *v = &g_voices[k];
            double e;
            int idx;

            if (!v->active)
                continue;
            /* XMI note-ons carry their own length and the parser expands them
             * into note-offs, so a voice normally ends on time. This age-out
             * stays as a safety net for the (few) notes that never get one:
             * without it such a note would sound for the rest of the piece and
             * pile up with every later note. Percussion uses a much shorter
             * age - drums are one-shot sounds, and holding them for seconds
             * used to fill the voice pool and silence the melodic tracks. */
            if (!v->release &&
                (g_t - v->start) > (v->percussive ? perc_age : max_age))
                v->release = g_t;
            e = envelope(v, g_t);
            if (e <= 0.0) {
                if (v->release)         /* the release has finished */
                    v->active = 0;
                continue;
            }
            if (v->w) {
                /* General MIDI sample: linear interpolation + loop points. */
                const int16_t *smp = (const int16_t *)v->w->data;
                uint32_t nsamples = v->w->samples;
                uint32_t i0;
                double frac, sample;

                if (v->w->loop_len) {
                    while (v->pos >= (double)(v->w->loop_start + v->w->loop_len))
                        v->pos -= (double)v->w->loop_len;
                } else if (v->pos >= (double)nsamples - 1.0) {
                    v->active = 0;
                    continue;
                }
                i0 = (uint32_t)v->pos;
                if (i0 >= nsamples)
                    i0 = nsamples - 1;
                frac = v->pos - (double)i0;
                sample = (double)smp[i0];
                if (i0 + 1 < nsamples)
                    sample += ((double)smp[i0 + 1] - sample) * frac;
                acc += (int32_t)(sample * e * ((double)v->volume / 127.0) *
                                 v->gain / 6.0);
                v->pos += v->sstep;
            } else {
                idx = (int)(v->phase * (double)TABLE_SIZE) & TABLE_MASK;
                acc += (int32_t)((double)g_wave[idx] * e *
                                 ((double)v->volume / 127.0) * v->gain / 2.0);
                v->phase += v->step;
                if (v->phase >= 1.0)
                    v->phase -= floor(v->phase);
            }
        }
        if (acc > 32767) acc = 32767;
        if (acc < -32768) acc = -32768;
        dst[written++] = (int16_t)((double)acc * gain);
        g_t++;
    }
    return written;
}

/* Whole-song render, used for the --midi-dump WAV and the render statistics:
 * both are evidence of what the synthesiser produced and must stay at full
 * scale regardless of the output volume. */
static int render(const synth_event *ev, int count, double tick_rate,
                  uint32_t rate, int16_t **out_pcm, uint32_t *out_samples)
{
    uint32_t total;
    int16_t *pcm;

    stream_init(ev, count, tick_rate, rate);
    total = g_total;
    pcm = (int16_t *)malloc((size_t)total * sizeof(int16_t));
    if (!pcm) {
        printf("synth: cannot allocate %u samples\n", (unsigned)total);
        return 0;
    }
    stream_fill(pcm, total, 1.0);
    *out_pcm = pcm;
    *out_samples = total;
    return 1;
}

/* ------------------------------------------------------------ waveOut ---- */

/* Several small buffers are queued and refilled as the driver finishes them,
 * so the synthesiser only ever runs a couple of slices ahead of what is
 * audible. Volume is applied per slice, which is what makes
 * AIL_set_sequence_volume(seq, vol, ms) audible at all. */
#define STREAM_SLICE    2048            /* samples per buffer (~93 ms @22050) */
#define STREAM_BUFFERS  4

static WAVEHDR  g_hdrs[STREAM_BUFFERS];
static int16_t *g_slices[STREAM_BUFFERS];

/* Sequence volume ramp - AIL_set_sequence_volume's `ms` argument. Stored in
 * the AIL 0..127 domain and interpolated in wall-clock time. */
static double   g_seq_from = 127.0;
static double   g_seq_to   = 127.0;
static DWORD    g_ramp_t0;
static int      g_ramp_ms;

static double seq_volume_now(void)
{
    DWORD e;

    if (g_ramp_ms <= 0)
        return g_seq_to;
    e = GetTickCount() - g_ramp_t0;
    if (e >= (DWORD)g_ramp_ms)
        return g_seq_to;
    return g_seq_from + (g_seq_to - g_seq_from) * (double)e / (double)g_ramp_ms;
}

static void stream_free_buffers(void)
{
    int i;

    for (i = 0; i < STREAM_BUFFERS; i++) {
        if (g_slices[i]) {
            free(g_slices[i]);
            g_slices[i] = NULL;
        }
        memset(&g_hdrs[i], 0, sizeof g_hdrs[i]);
    }
}

void synth_set_sequence_volume(int volume, int ms)
{
    if (volume < 0)
        volume = 0;
    if (volume > 127)
        volume = 127;
    /* Start from wherever the current ramp has got to, so a new request in the
     * middle of a fade continues smoothly instead of jumping back. */
    g_seq_from = seq_volume_now();
    g_seq_to   = (double)volume;
    g_ramp_ms  = ms > 0 ? ms : 0;
    g_ramp_t0  = GetTickCount();
}

static double stream_gain(void)
{
    return seq_volume_now() / 127.0 * (double)g_master / 100.0;
}

static DWORD WINAPI stream_thread(LPVOID param)
{
    int i;
    DWORD now, last_fade_log = 0;
    int ramp_active = 0;

    (void)param;
    for (;;) {
        int idle = 1;

        if (InterlockedCompareExchange(&g_stop_flag, 0, 0))
            break;
        for (i = 0; i < STREAM_BUFFERS; i++) {
            if (!(g_hdrs[i].dwFlags & WHDR_DONE))
                continue;
            waveOutUnprepareHeader(g_dev, &g_hdrs[i], sizeof g_hdrs[i]);
            stream_fill(g_slices[i], STREAM_SLICE, stream_gain());
            waveOutPrepareHeader(g_dev, &g_hdrs[i], sizeof g_hdrs[i]);
            waveOutWrite(g_dev, &g_hdrs[i], sizeof g_hdrs[i]);
            idle = 0;
        }
        /* Make a running fade visible in the log: it is otherwise only
         * audible, and "did the ramp actually happen" is the whole point. */
        now = GetTickCount();
        ramp_active = (g_ramp_ms > 0) &&
                      (now - g_ramp_t0 < (DWORD)g_ramp_ms);
        if (ramp_active && now - last_fade_log >= 500) {
            last_fade_log = now;
            printf("synth: fading %.0f -> %.0f over %d ms: now %.1f/127 "
                   "(%lu ms in)\n",
                   g_seq_from, g_seq_to, g_ramp_ms, seq_volume_now(),
                   (unsigned long)(now - g_ramp_t0));
        }
        Sleep(idle ? 5 : 0);
    }
    return 0;
}

static const char *g_dump_wav_path;

void synth_set_dump_path(const char *path)
{
    g_dump_wav_path = path;
}

void synth_set_master_volume(int percent)
{
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;
    g_master = percent;
}

/* Write the rendered mix as a plain 16-bit mono PCM WAV. Playing the file
 * outside the game separates "is the sequence parsed and synthesised
 * correctly" from "does waveOut work", and makes tempo problems obvious
 * without needing a stopwatch. */
static void dump_wav(const char *path, const int16_t *pcm, uint32_t samples,
                     uint32_t rate)
{
    uint8_t h[44];
    FILE *f = fopen(path, "wb");
    uint32_t data = samples * 2;

    if (!f) {
        printf("synth: cannot write %s\n", path);
        return;
    }
    memset(h, 0, sizeof h);
    memcpy(h + 0, "RIFF", 4);
    h[4]  = (uint8_t)(36 + data);       h[5]  = (uint8_t)((36 + data) >> 8);
    h[6]  = (uint8_t)((36 + data) >> 16); h[7] = (uint8_t)((36 + data) >> 24);
    memcpy(h + 8, "WAVEfmt ", 8);
    h[16] = 16;                         /* fmt chunk size                */
    h[20] = 1;                          /* PCM                           */
    h[22] = 1;                          /* mono                          */
    h[24] = (uint8_t)rate;              h[25] = (uint8_t)(rate >> 8);
    h[26] = (uint8_t)(rate >> 16);      h[27] = (uint8_t)(rate >> 24);
    h[28] = (uint8_t)(rate * 2);        h[29] = (uint8_t)((rate * 2) >> 8);
    h[30] = (uint8_t)((rate * 2) >> 16); h[31] = (uint8_t)((rate * 2) >> 24);
    h[32] = 2;                          /* block align                   */
    h[34] = 16;                         /* bits per sample               */
    memcpy(h + 36, "data", 4);
    h[40] = (uint8_t)data;              h[41] = (uint8_t)(data >> 8);
    h[42] = (uint8_t)(data >> 16);      h[43] = (uint8_t)(data >> 24);
    fwrite(h, 1, sizeof h, f);
    fwrite(pcm, 2, samples, f);
    fclose(f);
    printf("synth: dumped %.1f s of PCM to %s\n",
           (double)samples / (double)rate, path);
}

int synth_play(const synth_event *ev, int count, double tick_rate,
               uint32_t sample_rate, int loop)
{
    WAVEFORMATEX wf;
    int16_t *pcm = NULL;
    uint32_t samples = 0;
    DWORD t0;
    int i;

    synth_stop();
    if (!ev || count <= 0 || tick_rate <= 0.0)
        return 0;
    if (sample_rate < 8000 || sample_rate > 48000)
        sample_rate = 22050;
    if (!g_bank_tried) {
        g_bank_tried = 1;
        if (dls_load(&g_bank, g_dls_path))
            printf("synth: using General MIDI samples from %s\n", g_bank.path);
        else
            printf("synth: no GM sound bank available - waveform synthesis only\n");
    }
    t0 = GetTickCount();

    /* Offline full render, exactly as before streaming existed: the --midi-dump
     * WAV and the render statistics document what the synthesiser produced, so
     * they stay at full scale and are not affected by the output volume. */
    g_loop = 0;
    if (!render(ev, count, tick_rate, sample_rate, &pcm, &samples))
        return 0;

    if (g_dump_wav_path && g_dump_wav_path[0])
        dump_wav(g_dump_wav_path, pcm, samples, sample_rate);

    {
        uint32_t k, nonzero = 0;
        int16_t peak = 0;
        for (k = 0; k < samples; k++) {
            if (pcm[k])
                nonzero++;
            if (pcm[k] > peak)
                peak = pcm[k];
            else if ((int16_t)-pcm[k] > peak)
                peak = (int16_t)-pcm[k];
        }
        printf("synth: rendered %ld notes (%ld GM samples, %ld drums) -> %.1f s of "
               "16-bit PCM @ %u Hz (%.1f MB), loop=%d, non-silent %.1f%%, peak %d/32767, "
               "polyphony %d, took %u ms\n",
               g_notes, g_sampled_notes, g_drum_notes,
               (double)samples / (double)sample_rate,
               (unsigned)sample_rate,
               (double)(samples * 2) / (1024.0 * 1024.0), loop,
               100.0 * (double)nonzero / (double)samples, (int)peak,
               g_peak_voices, (unsigned)(GetTickCount() - t0));
    }
    free(pcm);
    pcm = NULL;

    memset(&wf, 0, sizeof wf);
    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = 1;
    wf.nSamplesPerSec  = sample_rate;
    wf.wBitsPerSample  = 16;
    wf.nBlockAlign     = 2;
    wf.nAvgBytesPerSec = sample_rate * 2;
    if (waveOutOpen(&g_dev, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        printf("synth: waveOutOpen(%u Hz) failed\n", (unsigned)sample_rate);
        g_dev = NULL;
        return 0;
    }

    /* Streamed playback: render slice by slice and queue them, so the gain in
     * force at submission time is what the listener hears. */
    g_loop = loop;
    stream_init(ev, count, tick_rate, sample_rate);
    for (i = 0; i < STREAM_BUFFERS; i++) {
        g_slices[i] = (int16_t *)malloc(STREAM_SLICE * sizeof(int16_t));
        if (!g_slices[i]) {
            printf("synth: cannot allocate the stream buffers\n");
            break;
        }
        memset(&g_hdrs[i], 0, sizeof g_hdrs[i]);
        g_hdrs[i].lpData         = (LPSTR)g_slices[i];
        g_hdrs[i].dwBufferLength = STREAM_SLICE * 2;
        stream_fill(g_slices[i], STREAM_SLICE, stream_gain());
        if (waveOutPrepareHeader(g_dev, &g_hdrs[i], sizeof g_hdrs[i]) != MMSYSERR_NOERROR ||
            waveOutWrite(g_dev, &g_hdrs[i], sizeof g_hdrs[i]) != MMSYSERR_NOERROR) {
            printf("synth: waveOutWrite failed (buffer %d)\n", i);
            break;
        }
    }
    if (i < STREAM_BUFFERS) {
        /* Could not queue every buffer: stop the device and report failure. */
        waveOutReset(g_dev);
        waveOutClose(g_dev);
        g_dev = NULL;
        stream_free_buffers();
        return 0;
    }

    InterlockedExchange(&g_stop_flag, 0);
    g_thread = CreateThread(NULL, 0, stream_thread, NULL, 0, NULL);
    printf("synth: streaming %u x %u-sample slices (%u ms each, %u ms queued), "
           "sequence volume %.0f/127\n",
           STREAM_BUFFERS, STREAM_SLICE,
           (unsigned)((double)STREAM_SLICE * 1000.0 / (double)sample_rate),
           (unsigned)((double)STREAM_SLICE * STREAM_BUFFERS * 1000.0 /
                      (double)sample_rate),
           seq_volume_now());
    return 1;
}

void synth_set_bank_path(const char *path)
{
    g_dls_path = path;
}

void synth_stop(void)
{
    if (g_thread) {
        InterlockedExchange(&g_stop_flag, 1);
        WaitForSingleObject(g_thread, 1000);
        CloseHandle(g_thread);
        g_thread = NULL;
    }
    if (g_dev) {
        int i, tries = 0;
        waveOutReset(g_dev);
        /* The driver may still own a buffer; never free one before its header
         * is no longer playing. */
        for (i = 0; i < STREAM_BUFFERS; i++) {
            if (!g_slices[i])
                continue;
            tries = 0;
            while (!(g_hdrs[i].dwFlags & WHDR_DONE) && tries++ < 100)
                Sleep(10);
            if (g_hdrs[i].dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(g_dev, &g_hdrs[i], sizeof g_hdrs[i]);
        }
        waveOutClose(g_dev);
        g_dev = NULL;
    }
    stream_free_buffers();
}

int synth_is_playing(void)
{
    return g_dev != NULL;
}

long synth_rendered_notes(void)
{
    return g_notes;
}
