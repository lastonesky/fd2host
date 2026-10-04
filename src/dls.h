#ifndef FD2_DLS_H
#define FD2_DLS_H

#include <stdint.h>

/* Minimal DLS Level 1 loader for the General MIDI sound bank that ships with
 * Windows (C:\Windows\System32\drivers\gm.dls, ~3.4 MB). The wavetable
 * synthesiser of the system MIDI device is not usable from here (see
 * PROGRESS.md §11.1), but the bank itself is a plain RIFF file, so the port can
 * play the original instruments itself instead of a generic waveform. */

typedef struct {
    uint16_t key_low, key_high, vel_low, vel_high;
    uint16_t table_index;       /* index into the wave pool               */
    uint16_t unity_note;        /* the note the sample was recorded at    */
    int16_t  fine_tune;         /* cents                                  */
} dls_region;

typedef struct {
    uint32_t   bank;            /* DLS ulBank: bit31 = F_DRUMS, bits = bank */
    uint32_t   program;
    int        nregions;
    dls_region regions[24];
} dls_instrument;

typedef struct {
    const uint8_t *data;        /* 16-bit PCM inside the loaded file      */
    uint32_t samples;           /* number of sample frames                */
    uint32_t sample_rate;
    uint16_t bits;
    uint16_t channels;
    uint16_t unity_note;
    int16_t  fine_tune;
    int32_t  attenuation;       /* in 0.1 dB units                        */
    uint32_t loop_start;        /* sample frames                          */
    uint32_t loop_len;          /* 0 = one-shot                           */
    int      valid;
} dls_wave;

typedef struct {
    uint8_t        *file;
    uint32_t        file_size;
    dls_instrument *insts;
    int             ninsts;
    dls_wave       *waves;
    int             nwaves;
    char            path[260];
} dls_bank;

/* Returns 1 on success. `path` may be NULL to use the standard gm.dls path. */
int  dls_load(dls_bank *bank, const char *path);
void dls_free(dls_bank *bank);

/* Pick the wave for a note.
 *   bank_num  - DLS bank number as stored in the file: (msb << 8) | lsb
 *   drums     - 1 for the GM percussion channel; the bank's drum flag (bit 31
 *               of ulBank) decides, not a bank number
 * Returns NULL when nothing matches. */
const dls_wave *dls_find(const dls_bank *bank, uint32_t bank_num, int program,
                         int note, int drums);

#endif /* FD2_DLS_H */
