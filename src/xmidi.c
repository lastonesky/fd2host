/* xmidi.c - minimal XMIDI player for the FD2 native port.
 *
 * The game plays music through Miles AIL: AIL_init_sequence(handle, address,
 * num) hands AIL the address of the XDIR directory inside FDMUS.DAT and
 * AIL_start_sequence() starts the selected song. The real AIL forwards that to
 * an *.MDI driver (16-bit real-mode code), which cannot run here, so the
 * sequence is interpreted directly and replayed on the Windows MIDI Mapper
 * ("Microsoft GS Wavetable Synth").
 *
 * Structure of the data (confirmed against AIL's own code in obj0, see
 * port/re/RE_MAP.md):
 *   - AIL_init_sequence's real body is sub_443D0: it walks the IFF tree
 *     (FORM/CAT, big-endian lengths) collecting TIMB / RBRN / EVNT pointers.
 *   - sub_42520 is the directory walker: it descends FORM/CAT until the block
 *     type is XMID, then, for a CAT, returns the sequence_num-th "FORM XMID"
 *     child (sequence_num is 0-based).
 *   - sub_44790 (= AIL_start_sequence) sets the event pointer to EVNT + 8,
 *     i.e. past the 4-byte id and 4-byte big-endian length.
 *
 * EVNT is *not* a standard MIDI track. The differences that broke the first
 * implementation (they were confirmed against WildMIDI's xmi2mid.c and then
 * checked byte for byte on FDMUS.DAT):
 *   - The delta before an event is the SUM of the consecutive bytes below 0x80
 *     up to the next status byte. It is not SMF's shift-and-or VLQ and it is
 *     not a single byte; a zero delta contributes no bytes at all.
 *   - There is no running status: every event writes its status byte, so the
 *     byte after the delta is always the status.
 *   - A Note-On carries its own length: `9n note velocity <VLQ>`, and the
 *     stream contains almost no Note-Off events. The parser expands each
 *     note-on into an explicit note-off at tick + length.
 *   - The resolution is a fixed 120 ticks per beat (not 60).
 */

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "xmidi.h"
#include "synth.h"
#include "audio.h"

#define XMIDI_TICKS_PER_BEAT 60          /* XMI's fixed resolution */
#define XMIDI_DEFAULT_TEMPO  500000u     /* microseconds per beat (120 BPM) */

typedef struct {
    uint32_t tick;          /* absolute time, in ticks                */
    uint8_t  msg[4];        /* MIDI message bytes                     */
    int      len;
} xmidi_event;

static xmidi_event  *g_events;
static int           g_count;
static int           g_capacity;
static HMIDIOUT      g_midi;
static HANDLE        g_thread;
static volatile LONG g_stop_flag;
static int           g_volume = 127;
static int           g_loop_count = 1;
static uint32_t      g_tempo_us = XMIDI_DEFAULT_TEMPO;
static int           g_tick_rate;       /* 0 = derive from tempo          */
static int           g_backend = 1;     /* 0 = MIDI Mapper, 1 = built-in synth */

/* XMI has no division field: its resolution is a fixed 60 ticks per beat, so
 * the wall-clock rate follows the tempo meta event (FF 51 03). The first tempo
 * in the sequence is used; game music in FD2 keeps a constant tempo.
 *
 * Evidence for 60 (this was mis-set to 120 for a while, which played every
 * song at double speed):
 *   - WildMIDI's xmi2mid.c converts an XMI tick to 8.3333 ms at the default
 *     500000 us/beat, which is exactly 60 ticks per beat.
 *   - fd2_re (an independent reverse-engineering of this same game) converts
 *     the same files with PPQN=60 and reports 137 beats for the song with
 *     2256 notes - our parse of it gives 8237 ticks = 137.3 beats. */
static double effective_tick_rate(void)
{
    if (g_tick_rate > 0)
        return (double)g_tick_rate;
    return (double)XMIDI_TICKS_PER_BEAT * 1000000.0 / (double)g_tempo_us;
}

/* ------------------------------------------------------------ parsing --- */

static uint32_t rd_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* Data bytes that follow a status byte. */
static int data_bytes(uint8_t status)
{
    switch (status & 0xF0) {
    case 0xC0: case 0xD0:
        return 1;
    case 0x80: case 0x90: case 0xA0: case 0xB0: case 0xE0:
        return 2;
    default:
        return 0;
    }
}

static int push_event(uint32_t tick, const uint8_t *msg, int len)
{
    if (g_count == g_capacity) {
        int cap = g_capacity ? g_capacity * 2 : 1024;
        xmidi_event *n = (xmidi_event *)realloc(g_events, (size_t)cap * sizeof *n);
        if (!n)
            return 0;
        g_events = n;
        g_capacity = cap;
    }
    g_events[g_count].tick = tick;
    memcpy(g_events[g_count].msg, msg, (size_t)len);
    g_events[g_count].len = len;
    g_count++;
    return 1;
}

/* ------------------------------------------------------ event stream ---- */

/* Standard shift-and-or VLQ: this *is* the encoding of the XMI note length
 * (and of meta event lengths), unlike the delta - see parse_evnt below. */
static uint32_t read_vlq(const uint8_t *p, uint32_t len, uint32_t *i)
{
    uint32_t v = 0;
    int k;

    for (k = 0; k < 4 && *i < len; k++) {
        uint8_t b = p[(*i)++];
        v = (v << 7) | (uint32_t)(b & 0x7F);
        if (!(b & 0x80))
            break;
    }
    return v;
}

static int is_note_off(const xmidi_event *e)
{
    return (e->msg[0] & 0xF0) == 0x80 ||
           ((e->msg[0] & 0xF0) == 0x90 && e->len > 2 && e->msg[2] == 0);
}

/* Note-offs generated from XMI note lengths are appended after the events that
 * follow them in the file, so the list has to be sorted before it can be
 * played. At equal ticks a note-off must come first, otherwise a note that
 * ends exactly where the next one starts would be cancelled immediately. */
static int cmp_event(const void *a, const void *b)
{
    const xmidi_event *x = (const xmidi_event *)a, *y = (const xmidi_event *)b;

    if (x->tick != y->tick)
        return x->tick < y->tick ? -1 : 1;
    return is_note_off(y) - is_note_off(x);
}

static int parse_evnt(const uint8_t *ev, uint32_t len, int *out_skips)
{
    uint32_t i = 0, tick = 0;
    int skips = 0, notes = 0, offs = 0;

    g_count = 0;
    while (i < len) {
        uint32_t delta = 0;
        uint8_t status;
        int n;
        uint8_t msg[4];

        /* XMIDI delta: the *sum* of the consecutive bytes below 0x80 up to the
         * next status byte (WildMIDI's GetVLQ2). It is neither SMF's
         * shift-and-or VLQ nor a single byte, and a zero delta contributes no
         * bytes at all. */
        while (i < len && ev[i] < 0x80)
            delta += ev[i++];
        if (i >= len)
            break;
        tick += delta;

        if (ev[i] == 0xFF) {                        /* meta event */
            uint8_t type = (i + 1 < len) ? ev[i + 1] : 0;
            uint32_t mlen;
            i += 2;
            mlen = read_vlq(ev, len, &i);
            if (type == 0x2F)                       /* end of track */
                break;
            if (type == 0x51 && mlen == 3 && i + 3 <= len)
                g_tempo_us = ((uint32_t)ev[i] << 16) |
                             ((uint32_t)ev[i + 1] << 8) | (uint32_t)ev[i + 2];
            i += mlen;
            if (i > len)
                break;
            continue;
        }

        /* XMIDI writes a status byte for every event - there is no running
         * status, so whatever comes after the delta *is* the status. */
        status = ev[i++];
        n = data_bytes(status);
        if (!n) {
            skips++;                                /* not modelled: skip   */
            continue;
        }
        if (i + (uint32_t)n > len)
            break;
        msg[0] = status;
        msg[1] = ev[i];
        msg[2] = (n > 1) ? ev[i + 1] : 0;
        i += (uint32_t)n;
        if (!push_event(tick, msg, n + 1))
            return 0;

        if ((status & 0xF0) == 0x90) {
            /* XMI Note-On: `9n note velocity <VLQ length>`. The file contains
             * almost no Note-Off events - the sequencer is expected to
             * schedule one when the length expires. Without this every note
             * hung until the synthesiser aged it out, which is what made the
             * music sound stretched and rhythmless. */
            uint32_t dur = read_vlq(ev, len, &i);
            notes++;
            if (msg[2] != 0 && dur) {
                uint8_t off[3];
                off[0] = (uint8_t)(0x80 | (status & 0x0F));
                off[1] = msg[1];
                off[2] = 0;
                if (!push_event(tick + dur, off, 3))
                    return 0;
                offs++;
            }
        }
    }
    if (g_count > 1)
        qsort(g_events, (size_t)g_count, sizeof g_events[0], cmp_event);
    printf("xmidi: parsed %d events (%d note-ons, %d generated note-offs,"
           " %d unmodelled bytes, last tick %u)\n",
           g_count, notes, offs, skips,
           g_count ? g_events[g_count - 1].tick : 0);
    if (out_skips)
        *out_skips = skips;
    return 1;
}


static const uint8_t *find_evnt(const uint8_t *p, uint32_t len, uint32_t *out_len)
{
    uint32_t i;

    for (i = 0; i + 8 <= len; i++) {
        if (!memcmp(p + i, "EVNT", 4)) {
            uint32_t l = rd_be32(p + i + 4);
            if (l && i + 8 + l <= len) {
                *out_len = l;
                return p + i + 8;
            }
        }
    }
    return NULL;
}

/* ------------------------------------------------------------ playback -- */

static int g_midi_err_reported;
static int g_midi_test;

/* Every MIDI message goes through here so failures are visible: earlier the
 * return value was ignored, which made "device present but silent" impossible
 * to diagnose. */
static void midi_send(DWORD msg)
{
    MMRESULT r;

    if (!g_midi)
        return;
    r = midiOutShortMsg(g_midi, msg);
    if (r != MMSYSERR_NOERROR && g_midi_err_reported < 6) {
        printf("xmidi: midiOutShortMsg(0x%08X) failed: %u\n",
               (unsigned)msg, (unsigned)r);
        g_midi_err_reported++;
    }
}

static void midi_all_notes_off(void)
{
    int ch;
    if (!g_midi)
        return;
    for (ch = 0; ch < 16; ch++)
        midi_send((DWORD)(0xB0 | ch) | (123u << 8));                 /* CC123 */
}

static void midi_set_volume(int vol)
{
    int ch;
    if (!g_midi)
        return;
    if (vol < 0) vol = 0;
    if (vol > 127) vol = 127;
    for (ch = 0; ch < 16; ch++)
        midi_send((DWORD)(0xB0 | ch) | (7u << 8) | ((DWORD)vol << 16));
}

/* Replay the event list. XMIDI sequences in this game carry very few explicit
 * note-offs, so each channel is treated as monophonic: a new note-on releases
 * the previous note on that channel. Without this the music turns into stuck
 * notes. */
static DWORD WINAPI player_thread(LPVOID param)
{
    DWORD start;
    int i, loops = 0;
    int active[16];
    double eff = effective_tick_rate();

    (void)param;
    for (i = 0; i < 16; i++)
        active[i] = -1;

    for (;;) {
        start = GetTickCount();
        for (i = 0; i < g_count; i++) {
            DWORD target = (DWORD)((double)g_events[i].tick * 1000.0 / eff);
            uint8_t st = g_events[i].msg[0];
            int ch = st & 0x0F;

            for (;;) {
                DWORD elapsed = GetTickCount() - start;
                if (elapsed >= target)
                    break;
                if (InterlockedCompareExchange(&g_stop_flag, 0, 0))
                    break;
                Sleep(target - elapsed > 4 ? 2 : 1);
            }
            if (InterlockedCompareExchange(&g_stop_flag, 0, 0))
                break;

            if ((st & 0xF0) == 0x90 && g_events[i].msg[2] != 0) {
                if (active[ch] >= 0)                    /* release previous   */
                    midi_send((DWORD)(0x80 | ch) | ((DWORD)active[ch] << 8));
                active[ch] = g_events[i].msg[1];
            } else if ((st & 0xF0) == 0x80 ||
                       ((st & 0xF0) == 0x90 && g_events[i].msg[2] == 0)) {
                if (active[ch] == g_events[i].msg[1])
                    active[ch] = -1;
            }

            midi_send((DWORD)g_events[i].msg[0] |
                      ((DWORD)g_events[i].msg[1] << 8) |
                      ((g_events[i].len > 2 ? (DWORD)g_events[i].msg[2] : 0) << 16));
        }
        loops++;
        if (InterlockedCompareExchange(&g_stop_flag, 0, 0))
            break;
        if (g_loop_count > 0 && loops >= g_loop_count)
            break;
        midi_all_notes_off();
    }
    midi_all_notes_off();
    return 0;
}

int xmidi_play(const uint8_t *blob, uint32_t len, int loop_count)
{
    const uint8_t *evnt;
    uint32_t evnt_len = 0;
    int skips = 0;

    xmidi_stop();
    if (!blob || len < 16)
        return 0;
    evnt = find_evnt(blob, len, &evnt_len);
    if (!evnt) {
        printf("xmidi: no EVNT chunk in %u bytes\n", (unsigned)len);
        return 0;
    }
    if (!parse_evnt(evnt, evnt_len, &skips)) {
        printf("xmidi: failed to parse EVNT (%u bytes)\n", (unsigned)evnt_len);
        return 0;
    }
    if (!g_count)
        return 0;

    if (!g_midi) {
        if (midiOutOpen(&g_midi, MIDI_MAPPER, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            printf("xmidi: midiOutOpen failed - no MIDI synthesiser available\n");
            g_midi = NULL;
            return 0;
        }
        /* The GS Wavetable Synth's output level is a *system* mixer setting
         * ("SW Synth" / MIDI), independent of the game. If it is muted the
         * sequence plays into silence, so read it back and raise it if needed. */
        {
            DWORD voldw = 0;
            MMRESULT gr = midiOutGetVolume(g_midi, &voldw);
            printf("xmidi: device volume = 0x%08X (get ret %u)\n",
                   (unsigned)voldw, (unsigned)gr);
            if (gr == MMSYSERR_NOERROR && voldw == 0) {
                MMRESULT sr = midiOutSetVolume(g_midi, 0xFFFFFFFFu);
                printf("xmidi: device was muted -> raised to maximum (ret %u)\n",
                       (unsigned)sr);
            }
        }
    }
    g_loop_count = loop_count;
    midi_set_volume(g_volume);
    InterlockedExchange(&g_stop_flag, 0);

    /* Default path: render the sequence with our own synthesiser and play it
     * through the software mixer (src/audio.h) - one device shared with the
     * sound effects, known to work on this machine. The Windows MIDI Mapper
     * stays available with --midi-backend=winmidi. The music is rendered at
     * the *device* rate so the mixer never has to resample it. */
    if (g_backend == 1) {
        unsigned dev_rate = audio_rate();
        if (synth_play((const synth_event *)g_events, g_count,
                       effective_tick_rate(), dev_rate ? dev_rate : 22050,
                       g_loop_count == 0))
            return 1;
        printf("xmidi: built-in synth failed, falling back to the MIDI Mapper\n");
    }

    if (g_midi_test) {
        printf("xmidi: MIDI test tone (middle C, 1 s) - if this is inaudible the "
               "system synthesiser is muted or unavailable\n");
        midi_send(0x007B3C90);              /* note on ch0, note 60, vel 123 */
        Sleep(1000);
        midi_send(0x00003C80);              /* note off */
    }

    /* Diagnostics: which MIDI synthesiser are we actually talking to, and does
     * the parsed stream look like music (notes long enough to be heard)? */
    {
        UINT ndev = midiOutGetNumDevs();
        UINT k;
        printf("xmidi: %u MIDI output device(s)\n", ndev);
        for (k = 0; k < ndev && k < 4; k++) {
            MIDIOUTCAPS caps;
            if (midiOutGetDevCaps(k, &caps, sizeof caps) == MMSYSERR_NOERROR)
                printf("xmidi:   [%u] %s (tech %u, voices %u)\n",
                       k, caps.szPname, (unsigned)caps.wTechnology,
                       (unsigned)caps.wVoices);
        }
    }
    {
        int per_ch[16] = { 0 };
        double gap_sum[16] = { 0.0 };
        uint32_t last_on[16] = { 0 };
        int i2, notes = 0, gaps = 0;
        for (i2 = 0; i2 < g_count; i2++) {
            uint8_t st = g_events[i2].msg[0];
            if ((st & 0xF0) == 0x90 && g_events[i2].msg[2] != 0) {
                int ch = st & 0x0F;
                if (per_ch[ch])
                    gap_sum[ch] += (double)(g_events[i2].tick - last_on[ch]);
                if (per_ch[ch])
                    gaps++;
                last_on[ch] = g_events[i2].tick;
                per_ch[ch]++;
                notes++;
            }
        }
        printf("xmidi: %d note-on events across channels:", notes);
        for (i2 = 0; i2 < 16; i2++)
            if (per_ch[i2])
                printf(" ch%d=%d", i2, per_ch[i2]);
        printf("\n");
        if (gaps) {
            printf("xmidi: mean note gap per channel (ticks):");
            for (i2 = 0; i2 < 16; i2++)
                if (per_ch[i2] > 1)
                    printf(" ch%d=%.1f", i2, gap_sum[i2] / (per_ch[i2] - 1));
            printf("  (at %.0f ticks/s, 10 ticks = %.0f ms)\n",
                   effective_tick_rate(), 10.0 * 1000.0 / effective_tick_rate());
        }
        for (i2 = 0; i2 < g_count && i2 < 12; i2++)
            printf("xmidi:   ev[%d] tick=%u %02X %02X %02X\n", i2,
                   (unsigned)g_events[i2].tick, g_events[i2].msg[0],
                   g_events[i2].msg[1], g_events[i2].len > 2 ? g_events[i2].msg[2] : 0);
    }

    g_thread = CreateThread(NULL, 0, player_thread, NULL, 0, NULL);
    if (!g_thread)
        return 0;
    printf("xmidi: %d events, %u ticks, tempo %u us/beat (%.1f BPM) -> %.1f s "
           "at %.1f ticks/s, %d skipped bytes, loop=%d\n",
           g_count, (unsigned)g_events[g_count - 1].tick, (unsigned)g_tempo_us,
           60000000.0 / (double)g_tempo_us,
           (double)g_events[g_count - 1].tick * (double)g_tempo_us /
               (XMIDI_TICKS_PER_BEAT * 1000000.0),
           effective_tick_rate(), skips, loop_count);
    return 1;
}

void xmidi_stop(void)
{
    synth_stop();
    if (g_thread) {
        InterlockedExchange(&g_stop_flag, 1);
        WaitForSingleObject(g_thread, 2000);
        CloseHandle(g_thread);
        g_thread = NULL;
    }
    midi_all_notes_off();
}

void xmidi_set_volume(int volume)
{
    g_volume = volume < 0 ? 0 : (volume > 127 ? 127 : volume);
    midi_set_volume(g_volume);
}

void xmidi_set_tick_rate(int ticks_per_second)
{
    /* 0 = derive the rate from the sequence's tempo meta event. */
    if (ticks_per_second == 0 ||
        (ticks_per_second >= 10 && ticks_per_second <= 1000))
        g_tick_rate = ticks_per_second;
}

void xmidi_set_test(int on)
{
    g_midi_test = on ? 1 : 0;
}

/* 0 = Windows MIDI Mapper, 1 = built-in software synthesiser (default). */
void xmidi_set_backend(int backend)
{
    g_backend = backend ? 1 : 0;
}

int xmidi_is_playing(void)
{
    return g_thread != NULL;
}
