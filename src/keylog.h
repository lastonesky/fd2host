/* keylog.h - keystroke recording and replay.
 *
 * Diagnostics, not game logic: when a play session shows a bug I cannot
 * reproduce, the input that matters is "which key, and how long after
 * process start". --keylog=<path> writes every keystroke the game sees as
 * `<ms since base>:<key>`, one per line, flushed immediately so a crash keeps
 * the record; --keyplay=<path> feeds that file back through the entry layer's
 * input_post_key() on the same absolute timeline. The key names are the
 * portable ones from src/keys.h, shared with --autokey.
 *
 * host.c owns nothing here beyond parsing the two options and calling the
 * four entry points below. The recording happens in host_key(), which is the
 * single funnel both render backends and both the real keyboard and --autokey
 * pass through, so a recording contains exactly what the game was given.
 *
 * Format note: recorded times are *absolute* (ms since the host's start
 * stamp, the same base as --shot-time/--exit-after), while --autokey's
 * delays are relative to the previous step. The two syntaxes are therefore
 * not interchangeable - replay a recording with --keyplay.
 */
#ifndef FD2_KEYLOG_H
#define FD2_KEYLOG_H

#include <stdint.h>

/* Open the record file and load the replay schedule. `log_path` and
 * `play_path` may be NULL (nothing to write / nothing to replay), `base` is
 * the timestamp recorded times are counted from. Returns the number of keys
 * loaded for replay. */
int  keylog_init(const char *log_path, const char *play_path, uint32_t base);

/* host_key() reports one make code here (break codes are ignored: a replay
 * posts the break after each key just like a real press). `ascii` is the
 * byte that went into the BDA ring: 0xE0 means "extended", which is how a
 * scan code shared by two keys (KP8 / UP) is resolved. */
void keylog_note(uint8_t scan, uint8_t ascii);

/* Start the replay thread if a schedule was loaded; returns 1 when a replay
 * is running, so the caller can skip --autokey. */
int  keylog_start(void);

/* 1 while a replay schedule is still running - the exit triggers
 * (--exit-when-file) must wait for it the way they wait for --autokey. */
int  keylog_replaying(void);

/* Print the schedule and close the record file. Must be called from *both*
 * shutdown paths: host_shutdown() and the watchdog's ExitProcess (the
 * watchdog never reaches host_shutdown - docs/PITFALLS.md §8-56). */
void keylog_finish(void);

#endif /* FD2_KEYLOG_H */
