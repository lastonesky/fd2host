#ifndef FD2_SYNTH_H
#define FD2_SYNTH_H

#include <stdint.h>

/* One parsed MIDI event (same layout as xmidi_event). */
typedef struct {
    uint32_t tick;
    uint8_t  msg[4];
    int      len;
} synth_event;

/* Render an event list to 16-bit mono PCM and play it through waveOut.
 * tick_rate converts ticks to wall-clock time; loop restarts the sequence when
 * it ends, until synth_stop() is called. Returns 1 on success.
 *
 * Playback is *streamed*: the synthesiser renders one slice at a time and the
 * slices are queued to waveOut as the driver consumes them, the way the
 * original AIL MDI driver fed its device from a timer callback. That is what
 * makes synth_set_sequence_volume() audible mid-piece - with one big
 * pre-rendered buffer the volume was a constant baked in at render time. */
int  synth_play(const synth_event *ev, int count, double tick_rate,
                uint32_t sample_rate, int loop);
void synth_stop(void);

/* AIL_set_sequence_volume(seq, vol, ms): the game's own music level (0..127)
 * and the ramp AIL performs over `ms` milliseconds (0 = immediate). Applied
 * per streamed slice, so a 4 s fade really takes 4 s. */
void synth_set_sequence_volume(int volume, int ms);
int  synth_is_playing(void);
long synth_rendered_notes(void);

/* Override the General MIDI sound bank path (default: the Windows gm.dls). */
void synth_set_bank_path(const char *path);

/* Host master volume 0..100 percent (host CLI --volume, default 100 = the
 * game's own level). Multiplied with synth_set_sequence_volume() when each
 * slice is rendered; the --midi-dump WAV and the render stats stay at full
 * scale so the offline evidence does not depend on it. */
void synth_set_master_volume(int percent);

/* Dump the rendered mix to a WAV file (diagnostics, no sound card involved). */
void synth_set_dump_path(const char *path);

#endif /* FD2_SYNTH_H */
