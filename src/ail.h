#ifndef FD2_AIL_H
#define FD2_AIL_H

#include <stdint.h>

/* Miles AIL (Audio Interface Library) replacement layer.
 *
 * The real AIL is linked into FD2.EXE but plays sound through 16-bit real-mode
 * drivers (SB16.DIG / SBPRO2.MDI) that cannot execute in a Win32 process, so
 * every driver install failed and the game ran silently. This module patches
 * the AIL entry points the game actually calls (16 of them, verified with IDA)
 * with 5-byte `jmp`s into host implementations built on WinMM waveOut.
 *
 * obj0 must already be mapped and relocated (RWX) when this is called, and it
 * must happen before the game thread starts.
 *
 * dump_dir may be NULL; when set, the PCM of the first samples and the XMIDI
 * blobs handed to AIL are written there as *.bin for format analysis.
 */
void ail_install(uint8_t *obj0_base, const char *dump_dir);

/* Same, for Ñ×ÁúÍâ´« FDPS.EXE: its own AIL build, 90 entry points at
 * 0x3D488..0x41FFE (re/fdps_ail_patchset.csv). Picking the wrong table writes
 * five bytes into unrelated code, so the caller selects by executable name. */
void ail_install_fdps(uint8_t *obj0_base, const char *dump_dir);

/* Override the sample format used for waveOut. The game never calls
 * AIL_set_sample_type / AIL_set_sample_playback_rate, so AIL's defaults (8-bit
 * mono, 11025 Hz) are assumed; these knobs exist so the assumption can be
 * corrected from the command line without a rebuild. */
void ail_set_format(uint32_t sample_rate, int bits, int stereo);

#endif /* FD2_AIL_H */
