/* audio.h - one output device, one software mixer (docs/AUDIO.md §11.10).
 *
 * Before this layer existed every sound effect owned its own waveOut device
 * (open once, torn down on a format change) and the music synthesiser owned
 * a third one, so volume, ramps and "is anything still playing" had to be
 * implemented three times and every device start/stop was a potential click.
 * Now there is exactly one device (WASAPI through sokol_audio - it works
 * without sokol_app, so both render backends share it) and one callback that
 * sums:
 *
 *     out[n] = music[n] + Σ sfx voices[n]
 *
 * Gain stays where it already was - the music slice has the sequence volume
 * and --volume baked in by synth.c, the SFX copy has --volume x
 * AIL_set_sample_volume plus the 3 ms start/stop ramp baked in by ail.c - so
 * the mixer only adds. That keeps every existing volume behaviour bit-for-bit
 * while removing the device churn (docs/AUDIO.md §11.6/§11.8/§11.9).
 *
 * LOCKING: one *recursive* critical section guards all audio state. The
 * device callback takes it while mixing; producers (synth.c, ail.c) take it
 * around their own state. Windows CRITICAL_SECTION lets the callback call
 * back into synth.c's music source while it already holds the lock.
 *
 * START GATE: audio_hold_music() makes the mixer output silence until
 * audio_release_music(). The game states a sequence's level microseconds
 * after AIL_start_sequence returns, and a slice rendered before that would
 * carry the *previous* track's level for its whole lifetime (the 371 ms
 * full-volume burst fixed in round 35/§35, see docs/PITFALLS.md §8-52).
 */
#ifndef FD2_AUDIO_H
#define FD2_AUDIO_H

#include <stdint.h>

/* The music source: called from the device callback whenever the mixer needs
 * more music. Returns the number of int16 frames written (0 = "no music
 * right now", the mixer outputs silence for that stretch). */
typedef unsigned (*audio_music_fn)(int16_t *dst, unsigned nframes);

/* Open the device once. `want_rate` is a hint (the WASAPI backend converts
 * to the mix format with AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM); the rate we
 * actually got is in audio_rate(). Returns 0 if there is no device - the
 * host keeps running, but audio_isup() stays 0 so the log says so. */
int      audio_init(unsigned want_rate);
void     audio_close(void);
int      audio_isup(void);
unsigned audio_rate(void);

/* Recursive lock guarding every piece of audio state. */
void     audio_lock(void);
void     audio_unlock(void);

/* Music stream: NULL stops it. audio_hold_music() forces silence until the
 * first audio_release_music() (see the start gate above). A gate nobody
 * opens after AUDIO_ARM_MS is released by the mixer anyway - play_bgm always
 * asks for a level within microseconds, so this only matters if a future
 * caller forgets, and silence forever would be worse than 200 ms of it. */
#define AUDIO_ARM_MS 200
void     audio_set_music(audio_music_fn fn);
audio_music_fn audio_music_src(void);
void     audio_hold_music(void);
void     audio_release_music(void);
int      audio_music_held(void);

/* Sound effects: one voice slot per AIL sample handle (AIL_MAX_SAMPLES).
 * `pcm` is the host copy ail.c already built (gain + ramp baked in); the
 * mixer takes ownership, converts it to mono float and frees it when the
 * voice runs out. Returns 0 on allocation failure (the copy is then freed
 * here, the caller must not). */
int  audio_sfx_play(int voice, uint8_t *pcm, unsigned bytes,
                    unsigned rate, unsigned channels, unsigned bits);
int  audio_sfx_stop(int voice);           /* 1 = it was still playing      */
int  audio_sfx_active(int voice);

/* Telemetry: called from the callback every AUDIO_TELEMETRY_MS so the log
 * proves data is reaching the device (frame count advances) and how loud it
 * is (peak) - "it looked fine in the log" must never again stand in for
 * "sound came out" (docs/PITFALLS.md §8-54). */
#define AUDIO_TELEMETRY_MS 10000

/* Optional: record exactly what the mixer hands the device, so an audio
 * regression can be measured instead of listened to. */
void audio_dump_open(const char *path);

#endif /* FD2_AUDIO_H */
