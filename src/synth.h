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
 * tick_rate converts ticks to wall-clock time; loop resubmits the buffer until
 * synth_stop() is called. Returns 1 on success. */
int  synth_play(const synth_event *ev, int count, double tick_rate,
                uint32_t sample_rate, int loop);
void synth_stop(void);
int  synth_is_playing(void);
long synth_rendered_notes(void);

/* Override the General MIDI sound bank path (default: the Windows gm.dls). */
void synth_set_bank_path(const char *path);

/* Master output volume 0..100 percent (host CLI --volume, default 10).
 * Applied to the rendered buffer *after* the --midi-dump WAV tap and the
 * render stats, immediately before waveOutWrite - the loop thread resubmits
 * the already-scaled buffer, so nothing else needs to know about it. */
void synth_set_master_volume(int percent);

/* Dump the rendered mix to a WAV file (diagnostics, no sound card involved). */
void synth_set_dump_path(const char *path);

#endif /* FD2_SYNTH_H */
