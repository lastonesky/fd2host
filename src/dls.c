/* dls.c - minimal DLS Level 1 loader for gm.dls (the Windows GM sound bank).
 *
 * The music is rendered by our own synthesiser (src/synth.c) because the system
 * MIDI device can be silent without any way for the application to notice
 * (PROGRESS.md §11.1). A waveform-based synthesiser sounds electronic though,
 * so the original GM samples are loaded from gm.dls instead - the same bank the
 * system synthesiser uses.
 *
 * Format notes (verified against the real file with a Python walker before
 * writing this):
 *   RIFF DLS
 *     colh                      instrument count
 *     LIST lins                 one LIST "ins " per instrument
 *       insh                    cRegions, ulBank, ulInstrument
 *       LIST lrgn               region list
 *         LIST "rgn "           rgnh (key/vel range) + wsmp + wlnk
 *     ptbl                      cCues + offsets into the wave pool
 *     LIST wvpl                 one LIST "wave" per sample, in ptbl order
 *       fmt  (PCM 16-bit mono 22050 Hz) + wsmp (unity note, loop) + data
 *
 * Only what the port needs is parsed: bank/program -> regions -> wave, plus the
 * sample's own tuning and loop points. Everything else (articulation, envelopes,
 * LFOs, filter) is handled by the simple envelopes in synth.c.
 */

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "dls.h"

#define FCC(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | \
                         ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

#define ID_RIFF FCC('R','I','F','F')
#define ID_LIST FCC('L','I','S','T')
#define ID_DLS  FCC('D','L','S',' ')
#define ID_COLH FCC('c','o','l','h')
#define ID_LINS FCC('l','i','n','s')
#define ID_INS  FCC('i','n','s',' ')
#define ID_INSH FCC('i','n','s','h')
#define ID_LRGN FCC('l','r','g','n')
#define ID_RGN  FCC('r','g','n',' ')
#define ID_RGNH FCC('r','g','n','h')
#define ID_WLNK FCC('w','l','n','k')
#define ID_WSMP FCC('w','s','m','p')
#define ID_WVPL FCC('w','v','p','l')
#define ID_WAVE FCC('w','a','v','e')
#define ID_FMT  FCC('f','m','t',' ')
#define ID_DATA FCC('d','a','t','a')

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

/* One step through the chunk list in base[off, end). */
static int next_chunk(const uint8_t *base, uint32_t end, uint32_t *pos,
                      uint32_t *id, uint32_t *len, uint32_t *data)
{
    uint32_t p = *pos;

    if (p + 8 > end)
        return 0;
    *id = rd32(base + p);
    *len = rd32(base + p + 4);
    if (*len > end - p - 8)
        return 0;
    *data = p + 8;
    *pos = p + 8 + *len + (*len & 1);
    return 1;
}

/* ------------------------------------------------------------- instruments */

static void parse_region(const uint8_t *b, uint32_t off, uint32_t len, dls_region *r)
{
    uint32_t pos = off, id, clen, data, end = off + len;

    memset(r, 0, sizeof *r);
    r->key_low = 0; r->key_high = 127; r->vel_low = 0; r->vel_high = 127;
    while (next_chunk(b, end, &pos, &id, &clen, &data)) {
        if (id == ID_RGNH && clen >= 8) {
            r->key_low  = rd16(b + data);
            r->key_high = rd16(b + data + 2);
            r->vel_low  = rd16(b + data + 4);
            r->vel_high = rd16(b + data + 6);
        } else if (id == ID_WLNK && clen >= 12) {
            r->table_index = (uint16_t)rd32(b + data + 8);
        } else if (id == ID_WSMP && clen >= 20) {
            r->unity_note = rd16(b + data + 4);
            r->fine_tune  = (int16_t)rd16(b + data + 6);
            if (!r->unity_note)
                r->unity_note = 60;
        }
    }
}

static void parse_instrument(const uint8_t *b, uint32_t off, uint32_t len, dls_instrument *inst)
{
    uint32_t pos = off, id, clen, data, end = off + len;

    memset(inst, 0, sizeof *inst);
    while (next_chunk(b, end, &pos, &id, &clen, &data)) {
        if (id == ID_INSH && clen >= 12) {
            inst->bank    = rd32(b + data + 4);     /* keep bit31 (F_DRUMS) */
            inst->program = rd32(b + data + 8) & 0x7Fu;
        } else if (id == ID_LIST && clen >= 4 && rd32(b + data) == ID_LRGN) {
            uint32_t p2 = data + 4, id2, len2, data2, end2 = data + clen;
            while (next_chunk(b, end2, &p2, &id2, &len2, &data2)) {
                if (id2 == ID_LIST && len2 >= 4 && rd32(b + data2) == ID_RGN &&
                    inst->nregions < (int)(sizeof inst->regions / sizeof inst->regions[0])) {
                    parse_region(b, data2 + 4, len2 - 4,
                                 &inst->regions[inst->nregions]);
                    inst->nregions++;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------- waves */

static void parse_wave(const uint8_t *b, uint32_t off, uint32_t len, dls_wave *w)
{
    uint32_t pos = off, id, clen, data, end = off + len;

    memset(w, 0, sizeof *w);
    while (next_chunk(b, end, &pos, &id, &clen, &data)) {
        if (id == ID_FMT && clen >= 16) {
            w->bits        = rd16(b + data + 14);
            w->channels    = rd16(b + data + 2);
            w->sample_rate = rd32(b + data + 4);
        } else if (id == ID_DATA) {
            w->data = b + data;
            w->bits = w->bits ? w->bits : 16;
            w->channels = w->channels ? w->channels : 1;
            w->samples = clen / (uint32_t)((w->bits / 8) * (w->channels ? w->channels : 1));
        } else if (id == ID_WSMP && clen >= 20) {
            w->unity_note  = rd16(b + data + 4);
            w->fine_tune   = (int16_t)rd16(b + data + 6);
            w->attenuation = (int32_t)rd32(b + data + 8);
            if (rd32(b + data + 16) >= 1 && clen >= 28) {
                /* loop 0: cbSize, type, start, length - type 0 = forward loop */
                if (rd32(b + data + 24) == 0) {
                    w->loop_start = rd32(b + data + 28);
                    w->loop_len   = rd32(b + data + 32);
                }
            }
        }
    }
    if (!w->sample_rate)
        w->sample_rate = 22050;
    if (!w->unity_note)
        w->unity_note = 60;
    if (w->data && w->samples)
        w->valid = 1;
}

/* -------------------------------------------------------------- top level */

static int grow(void **ptr, int *count, int elem)
{
    int cap = *count + 1;
    void *n = realloc(*ptr, (size_t)cap * (size_t)elem);
    if (!n)
        return 0;
    *ptr = n;
    return 1;
}

int dls_load(dls_bank *bank, const char *path)
{
    FILE *f;
    long size;
    uint32_t pos, id, clen, data;
    uint32_t lins_off = 0, lins_len = 0, wvpl_off = 0, wvpl_len = 0;
    int have_lins = 0, have_wvpl = 0;

    memset(bank, 0, sizeof *bank);
    if (!path)
        path = "C:\\Windows\\System32\\drivers\\gm.dls";
    strncpy(bank->path, path, sizeof bank->path - 1);

    f = fopen(path, "rb");
    if (!f) {
        printf("dls: cannot open %s\n", path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 1024) {
        fclose(f);
        return 0;
    }
    bank->file = (uint8_t *)malloc((size_t)size);
    if (!bank->file) {
        fclose(f);
        return 0;
    }
    if (fread(bank->file, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        dls_free(bank);
        return 0;
    }
    fclose(f);
    bank->file_size = (uint32_t)size;

    if (rd32(bank->file) != ID_RIFF || rd32(bank->file + 8) != ID_DLS) {
        printf("dls: %s is not a DLS bank\n", path);
        dls_free(bank);
        return 0;
    }

    pos = 12;
    while (next_chunk(bank->file, bank->file_size, &pos, &id, &clen, &data)) {
        if (id == ID_LIST && clen >= 4) {
            uint32_t type = rd32(bank->file + data);
            if (type == ID_LINS) {
                lins_off = data + 4;
                lins_len = clen - 4;
                have_lins = 1;
            } else if (type == ID_WVPL) {
                wvpl_off = data + 4;
                wvpl_len = clen - 4;
                have_wvpl = 1;
            }
        }
    }
    if (!have_lins || !have_wvpl) {
        printf("dls: missing lins/wvpl\n");
        dls_free(bank);
        return 0;
    }

    {
        uint32_t p = lins_off, end = lins_off + lins_len, i2, l2, d2;
        while (next_chunk(bank->file, end, &p, &i2, &l2, &d2)) {
            if (i2 == ID_LIST && l2 >= 4 && rd32(bank->file + d2) == ID_INS) {
                dls_instrument tmp;
                parse_instrument(bank->file, d2 + 4, l2 - 4, &tmp);
                if (tmp.nregions && grow((void **)&bank->insts, &bank->ninsts,
                                         (int)sizeof *bank->insts)) {
                    bank->insts[bank->ninsts] = tmp;
                    bank->ninsts++;
                }
            }
        }
    }
    {
        uint32_t p = wvpl_off, end = wvpl_off + wvpl_len, i2, l2, d2;
        while (next_chunk(bank->file, end, &p, &i2, &l2, &d2)) {
            if (i2 == ID_LIST && l2 >= 4 && rd32(bank->file + d2) == ID_WAVE) {
                dls_wave tmp;
                parse_wave(bank->file, d2 + 4, l2 - 4, &tmp);
                if (tmp.valid && grow((void **)&bank->waves, &bank->nwaves,
                                      (int)sizeof *bank->waves)) {
                    bank->waves[bank->nwaves] = tmp;
                    bank->nwaves++;
                }
            }
        }
    }
    printf("dls: %s loaded - %d instruments, %d waves\n",
           path, bank->ninsts, bank->nwaves);
    return (bank->ninsts && bank->nwaves) ? 1 : 0;
}

void dls_free(dls_bank *bank)
{
    if (bank->insts) free(bank->insts);
    if (bank->waves) free(bank->waves);
    if (bank->file) free(bank->file);
    memset(bank, 0, sizeof *bank);
}

const dls_wave *dls_find(const dls_bank *bank, uint32_t bank_num, int program,
                         int note, int drums)
{
    int i, k;
    const dls_instrument *best = NULL;

    /* Percussion is marked with bit 31 of ulBank, not by a bank number: in the
     * Windows GM bank the 9 drum kits have ulBank = 0x80000000 and programs
     * 0/8/16/... - searching for "bank 128" finds nothing and falls back to
     * melodic instruments, which is what silenced the drums. */
    for (i = 0; i < bank->ninsts; i++) {
        const dls_instrument *in = &bank->insts[i];
        int in_drums = (in->bank & 0x80000000u) ? 1 : 0;
        uint32_t in_bank = in->bank & 0x7FFFFFFFu;

        if (in_drums != (drums ? 1 : 0))
            continue;
        if (drums) {
            if (in->program == (uint32_t)program) {    /* kit number       */
                best = in;
                break;
            }
            if (!best)
                best = in;                             /* first kit so far */
            continue;
        }
        if (in_bank == bank_num && in->program == (uint32_t)program) {
            best = in;
            break;
        }
    }
    if (!best && drums) {
        for (i = 0; i < bank->ninsts; i++)
            if (bank->insts[i].bank & 0x80000000u) { best = &bank->insts[i]; break; }
    }
    if (!best) {
        /* fall back to the melodic bank for this program so odd bank selects
         * still produce a reasonable instrument */
        for (i = 0; i < bank->ninsts; i++) {
            if ((bank->insts[i].bank & 0x80000000u) == 0 &&
                bank->insts[i].program == (uint32_t)program) {
                best = &bank->insts[i];
                break;
            }
        }
    }
    if (!best)
        return NULL;
    for (k = 0; k < best->nregions; k++) {
        const dls_region *r = &best->regions[k];
        if (note >= r->key_low && note <= r->key_high &&
            r->table_index < bank->nwaves)
            return &bank->waves[r->table_index];
    }
    /* last resort: first region of the instrument */
    if (best->nregions && best->regions[0].table_index < bank->nwaves)
        return &bank->waves[best->regions[0].table_index];
    return NULL;
}
