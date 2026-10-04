/* ail.c - Miles AIL (Audio Interface Library) replacement layer.
 *
 * Why this exists
 * ---------------
 * FD2.EXE has Miles AIL V3.02 linked in. The library itself is fine, but it
 * plays sound by loading a 16-bit real-mode driver (*.DIG for digital audio,
 * *.MDI for MIDI) and jumping straight into that code, which cannot run inside
 * a Win32 process. With the driver files blocked, every AIL_install_* returned
 * 0, the game set its "no digital audio device" flag and ran silently.
 *
 * How it is replaced
 * ------------------
 * IDA shows the game calls only 16 AIL entry points (port/re/RE_MAP.md §3):
 *
 *     AIL_startup, AIL_shutdown,
 *     AIL_install_DIG_INI, AIL_install_MDI_INI,
 *     AIL_allocate_sample_handle, AIL_allocate_sequence_handle,
 *     AIL_init_sample, AIL_set_sample_address, AIL_set_sample_loop_count,
 *     AIL_start_sample, AIL_stop_sample,
 *     AIL_init_sequence, AIL_start_sequence, AIL_stop_sequence,
 *     AIL_set_sequence_volume, AIL_set_sequence_loop_count
 *
 * Each of those entry points is patched to a 5-byte `jmp rel32` targeting the
 * host implementation below. AIL's public API uses the Watcom cdecl calling
 * convention, which for these signatures is ABI-identical to MSVC __cdecl
 * (arguments on the stack, caller cleans up, return value in EAX), so the
 * patched calls are compatible without any thunking.
 *
 * Digital audio is played with WinMM waveOut: the game hands AIL a pointer and
 * length into its own memory (AIL_set_sample_address), and we copy that block
 * and submit it to the sound device. MIDI sequences are accepted and recorded
 * (the data pointer identifies the XMIDI blob inside FDMUS.DAT) but not yet
 * synthesised - that needs an XMIDI player and is the next step.
 */

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "ail.h"
#include "xmidi.h"

#define AIL_OBJ0_BASE    0x00010000u
#define AIL_MAX_SAMPLES  4
#define AIL_MAX_SEQS     4

/* ------------------------------------------------------------- state ----- */

typedef struct {
    int        used;
    uint8_t   *addr;          /* guest sample pointer (AIL_set_sample_address) */
    uint32_t   len;
    int32_t    loop_count;
    int32_t    volume;
    int32_t    pan;
    int32_t    type;
    uint32_t   rate;          /* playback rate (AIL default unless changed) */
    int32_t    playing;
    int        dumped;        /* sample bytes already written to dump_dir */
    HWAVEOUT   dev;           /* opened lazily on the first play            */
    WAVEHDR    hdr;
    uint8_t   *pcm;           /* host copy: waveOut reads it asynchronously */
} ail_sample;

typedef struct {
    int             used;
    const uint8_t  *data;     /* XMIDI blob inside FDMUS.DAT               */
    uint32_t        len;      /* derived from the IFF FORM header          */
    int32_t         seq_num;
    int32_t         volume;
    int32_t         loop_count;
    int32_t         playing;
} ail_seq;

static ail_sample  g_samples[AIL_MAX_SAMPLES];
static ail_seq     g_seqs[AIL_MAX_SEQS];
static uint32_t    g_rate    = 11025;   /* AIL default playback rate */
static int         g_bits    = 8;       /* AIL default sample type: mono 8-bit */
static int         g_stereo  = 0;
static char        g_dump_dir[MAX_PATH];
static int         g_dump_seq;
static int         g_installed;

/* ------------------------------------------------------------- helpers --- */

static void dump_blob(const char *tag, const void *p, uint32_t len)
{
    char path[MAX_PATH];
    FILE *f;

    if (!g_dump_dir[0] || !p || !len)
        return;
    if (len > 512 * 1024)
        len = 512 * 1024;
    _snprintf(path, sizeof path, "%s\\%s_%d.bin", g_dump_dir, tag, g_dump_seq++);
    f = fopen(path, "wb");
    if (!f)
        return;
    fwrite(p, 1, len, f);
    fclose(f);
    printf("ail: dumped %u bytes of %s to %s\n", (unsigned)len, tag, path);
}

/* Summarise the block the game is about to play: 8-bit unsigned PCM hovers
 * around 0x80 while 16-bit signed data shows high bytes clustered at 0x00 or
 * 0xFF. This is what pins down the format the game assumes. */
static void log_pcm_stats(const char *what, const uint8_t *p, uint32_t len)
{
    uint32_t i, near_128 = 0, hi_zero = 0;
    uint8_t lo = 0xFF, hi = 0x00;

    if (!len)
        return;
    for (i = 0; i < len; i++) {
        if (p[i] >= 0x60 && p[i] <= 0xA0) near_128++;
        if (p[i] < lo) lo = p[i];
        if (p[i] > hi) hi = p[i];
    }
    for (i = 1; i < len; i += 2)
        if (p[i] == 0x00 || p[i] == 0xFF) hi_zero++;
    printf("ail: %s len=%u  range=%02X..%02X  near-0x80=%.0f%%  odd-byte-00/FF=%.0f%%"
           "  head=%02X %02X %02X %02X %02X %02X %02X %02X\n",
           what, (unsigned)len, lo, hi, 100.0 * near_128 / len,
           100.0 * hi_zero / (len / 2 + 1),
           p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
}

static ail_sample *sample_of(void *handle)
{
    ail_sample *s = (ail_sample *)handle;
    if (s < g_samples || s >= g_samples + AIL_MAX_SAMPLES)
        return NULL;
    return s->used ? s : NULL;
}

static ail_seq *seq_of(void *handle)
{
    ail_seq *s = (ail_seq *)handle;
    if (s < g_seqs || s >= g_seqs + AIL_MAX_SEQS)
        return NULL;
    return s->used ? s : NULL;
}

/* ------------------------------------------------------------- waveOut --- */

static void sample_close_device(ail_sample *s)
{
    if (s->dev) {
        /* Closing is the simplest way to stop and release the buffer without
         * racing waveOutReset/waveOutUnprepareHeader. Devices are reopened on
         * the next play; sound effects fire a few times per second at most. */
        waveOutClose(s->dev);
        s->dev = NULL;
    }
    memset(&s->hdr, 0, sizeof s->hdr);
    s->playing = 0;
}

static int sample_open_device(ail_sample *s)
{
    WAVEFORMATEX wf;

    if (s->dev)
        return 1;
    memset(&wf, 0, sizeof wf);
    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = (WORD)(g_stereo ? 2 : 1);
    wf.nSamplesPerSec  = g_rate;
    wf.wBitsPerSample  = (WORD)g_bits;
    wf.nBlockAlign     = (WORD)(wf.nChannels * g_bits / 8);
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    if (waveOutOpen(&s->dev, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        printf("ail: waveOutOpen(%u Hz, %d-bit, %d ch) failed\n",
               (unsigned)g_rate, g_bits, (int)wf.nChannels);
        s->dev = NULL;
        return 0;
    }
    return 1;
}

static void sample_play(ail_sample *s)
{
    uint32_t played;

    sample_close_device(s);
    if (!s->addr || !s->len)
        return;
    if (!sample_open_device(s))
        return;
    if (s->pcm) {
        free(s->pcm);
        s->pcm = NULL;
    }
    s->pcm = (uint8_t *)malloc(s->len);
    if (!s->pcm)
        return;
    memcpy(s->pcm, s->addr, s->len);
    s->hdr.lpData         = (LPSTR)s->pcm;
    s->hdr.dwBufferLength = s->len;
    if (waveOutPrepareHeader(s->dev, &s->hdr, sizeof s->hdr) != MMSYSERR_NOERROR)
        return;
    if (waveOutWrite(s->dev, &s->hdr, sizeof s->hdr) == MMSYSERR_NOERROR) {
        s->playing = 1;
        played = s->len;
        printf("ail: play %u bytes (%.2f s @ %u Hz, loop=%d)\n",
               (unsigned)played, (double)played / (g_rate * (g_stereo ? 2 : 1) * (g_bits / 8)),
               (unsigned)g_rate, (int)s->loop_count);
    }
}

/* ------------------------------------------- host AIL implementations ---- */

/* All of these are __cdecl to match the Watcom cdecl AIL API. */

static int32_t host_AIL_startup(void)
{
    printf("ail: startup -> ok (replacement layer)\n");
    return 1;
}

static void host_AIL_shutdown(void)
{
    int i;
    xmidi_stop();
    for (i = 0; i < AIL_MAX_SAMPLES; i++) {
        if (g_samples[i].used) {
            sample_close_device(&g_samples[i]);
            if (g_samples[i].pcm) free(g_samples[i].pcm);
        }
    }
    printf("ail: shutdown\n");
}

/* The game checks these for non-zero before using any sample/sequence API. */
static int32_t host_AIL_install_DIG_INI(void)
{
    printf("ail: install_DIG_INI -> fake driver handle 1\n");
    return 1;
}

static int32_t host_AIL_install_MDI_INI(void)
{
    printf("ail: install_MDI_INI -> fake driver handle 1\n");
    return 1;
}

static void *host_AIL_allocate_sample_handle(int32_t driver)
{
    int i;
    (void)driver;
    for (i = 0; i < AIL_MAX_SAMPLES; i++) {
        if (!g_samples[i].used) {
            memset(&g_samples[i], 0, sizeof g_samples[i]);
            g_samples[i].used = 1;
            g_samples[i].volume = 127;      /* AIL default */
            g_samples[i].pan = 64;
            g_samples[i].rate = g_rate;
            printf("ail: allocate_sample_handle -> %d\n", i);
            return &g_samples[i];
        }
    }
    printf("ail: allocate_sample_handle -> out of handles\n");
    return NULL;
}

static void *host_AIL_allocate_sequence_handle(int32_t driver)
{
    int i;
    (void)driver;
    for (i = 0; i < AIL_MAX_SEQS; i++) {
        if (!g_seqs[i].used) {
            memset(&g_seqs[i], 0, sizeof g_seqs[i]);
            g_seqs[i].used = 1;
            g_seqs[i].volume = 127;
            printf("ail: allocate_sequence_handle -> %d\n", i);
            return &g_seqs[i];
        }
    }
    return NULL;
}

static void host_AIL_init_sample(void *h)
{
    ail_sample *s = sample_of(h);
    if (!s)
        return;
    s->len = 0;
    s->addr = NULL;
    s->loop_count = 1;
    s->type = 0;                            /* DIG_F_MONO_8 */
    sample_close_device(s);
}

static void host_AIL_set_sample_address(void *h, void *start, uint32_t len)
{
    ail_sample *s = sample_of(h);
    if (!s)
        return;
    s->addr = (uint8_t *)start;
    s->len = len;
    if (s->len) {
        log_pcm_stats("set_sample_address", s->addr, s->len);
        if (!s->dumped) {
            s->dumped = 1;                  /* one copy per distinct sample */
            dump_blob("ail_smp", s->addr, s->len);
        }
    }
}

static void host_AIL_set_sample_loop_count(void *h, int32_t count)
{
    ail_sample *s = sample_of(h);
    if (s)
        s->loop_count = count;
}

static void host_AIL_start_sample(void *h)
{
    ail_sample *s = sample_of(h);
    if (s)
        sample_play(s);
}

static void host_AIL_stop_sample(void *h)
{
    ail_sample *s = sample_of(h);
    if (!s)
        return;
    sample_close_device(s);
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* AIL_init_sequence is handed the address of the XDIR directory inside
 * FDMUS.DAT (its FORM header covers only the 22-byte catalogue), and
 * sequence_num selects which song to use. This mirrors AIL's own walker
 * (obj0 sub_42520): descend FORM/CAT wrappers until the block type is XMID,
 * then pick the sequence_num-th "FORM XMID" child of that CAT. */
static const uint8_t *locate_xmid(const uint8_t *base, int index, uint32_t *out_len)
{
    const uint8_t *p = base;
    uint32_t blk;

    if (!base)
        return NULL;
    for (;;) {
        if (memcmp(p, "FORM", 4) && memcmp(p, "CAT ", 4))
            return NULL;
        blk = 8 + be32(p + 4);
        if (!memcmp(p + 8, "XMID", 4))
            break;
        p += blk;
    }
    if (!memcmp(p, "FORM", 4)) {            /* the blob is a single song */
        if (index != 0)
            return NULL;
        *out_len = 8 + be32(p + 4);
        return p;
    }
    {                                       /* CAT: index-th XMID child */
        const uint8_t *end = p + blk;
        const uint8_t *c = p + 12;
        int n = index;
        while (c + 8 <= end) {
            uint32_t clen = 8 + be32(c + 4);
            if (!memcmp(c + 8, "XMID", 4)) {
                if (n == 0) {
                    *out_len = clen;
                    return c;
                }
                n--;
            }
            if (!clen)
                break;
            c += clen;
        }
    }
    return NULL;
}

static void host_AIL_init_sequence(void *h, void *start, int32_t sequence_num)
{
    ail_seq *q = seq_of(h);
    const uint8_t *blob = NULL;
    uint32_t len = 0;

    if (!q)
        return;
    q->seq_num = sequence_num;
    q->data = NULL;
    q->len = 0;
    if (!start)
        return;
    blob = locate_xmid((const uint8_t *)start, (int)sequence_num, &len);
    if (!blob || len < 16) {
        printf("ail: init_sequence(seq=%d) - no XMID found from %p\n",
               (int)sequence_num, start);
        return;
    }
    q->data = blob;
    q->len = len;
    printf("ail: init_sequence(seq=%d) base=%p song=%p len=%u head=%c%c%c%c\n",
           (int)sequence_num, start, (const void *)blob, (unsigned)len,
           blob[0], blob[1], blob[2], blob[3]);
    dump_blob("ail_seq", blob, len);
}

static void host_AIL_start_sequence(void *h)
{
    ail_seq *q = seq_of(h);
    if (!q || !q->data)
        return;
    q->playing = 1;
    if (!xmidi_play(q->data, q->len, q->loop_count))
        printf("ail: start_sequence(seq=%d) - playback unavailable\n", (int)q->seq_num);
}

static void host_AIL_stop_sequence(void *h)
{
    ail_seq *q = seq_of(h);
    if (q)
        q->playing = 0;
    xmidi_stop();
}

static void host_AIL_set_sequence_volume(void *h, int32_t volume, int32_t ms)
{
    ail_seq *q = seq_of(h);
    if (q)
        q->volume = volume;
    printf("ail: set_sequence_volume(%d, over %d ms)\n", (int)volume, (int)ms);
    xmidi_set_volume((int)volume);
}

static void host_AIL_set_sequence_loop_count(void *h, int32_t count)
{
    ail_seq *q = seq_of(h);
    if (q)
        q->loop_count = count;
}

/* Fallback for the AIL entry points the game never calls directly.
 *
 * They still have to be blocked, not left to run: any AIL function that executes
 * its original body reaches internals that expect the driver/timer tables
 * AIL_startup would have built, and our stubs build none. Execution then wanders
 * off - that is exactly how "continue" crashed: the game reached
 * AIL_install_timbre (0x3B80F, found via the trace string "AIL_install_timbre"),
 * its body called sub_44AF0, and the flow landed in the middle of a `jz`
 * instruction at 0x3B89A where a stray 0xCC raised an unhandled breakpoint. */
static int32_t host_AIL_unused(void)
{
    return 0;
}

/* --------------------------------------------------------------- patch --- */

typedef struct {
    uint32_t    addr;
    const char *name;
    void       *impl;
} ail_entry;

static const ail_entry g_entries[] = {
    { 0x37D3E, "AIL_startup",                   (void *)host_AIL_startup                   },
    { 0x37ED8, "AIL_shutdown",                  (void *)host_AIL_shutdown                  },
    { 0x3908B, "AIL_install_DIG_INI",           (void *)host_AIL_install_DIG_INI           },
    { 0x3AA72, "AIL_install_MDI_INI",           (void *)host_AIL_install_MDI_INI           },
    { 0x392D0, "AIL_allocate_sample_handle",    (void *)host_AIL_allocate_sample_handle    },
    { 0x3ACA3, "AIL_allocate_sequence_handle",  (void *)host_AIL_allocate_sequence_handle  },
    { 0x39521, "AIL_init_sample",               (void *)host_AIL_init_sample               },
    { 0x39694, "AIL_set_sample_address",        (void *)host_AIL_set_sample_address        },
    { 0x39AAE, "AIL_set_sample_loop_count",     (void *)host_AIL_set_sample_loop_count     },
    { 0x39798, "AIL_start_sample",              (void *)host_AIL_start_sample              },
    { 0x39805, "AIL_stop_sample",               (void *)host_AIL_stop_sample               },
    { 0x3ADF5, "AIL_init_sequence",             (void *)host_AIL_init_sequence             },
    { 0x3AEEE, "AIL_start_sequence",            (void *)host_AIL_start_sequence            },
    { 0x3AF5B, "AIL_stop_sequence",             (void *)host_AIL_stop_sequence             },
    { 0x3B124, "AIL_set_sequence_volume",       (void *)host_AIL_set_sequence_volume       },
    { 0x3B1A6, "AIL_set_sequence_loop_count",   (void *)host_AIL_set_sequence_loop_count   },

    /* Everything below is never called by the game directly, but has to be
     * blocked so no original AIL code can run (see host_AIL_unused). */
    { 0x37F70, "AIL_set_preference",            (void *)host_AIL_unused },
    { 0x38074, "AIL_get_real_vect",             (void *)host_AIL_unused },
    { 0x3815F, "AIL_set_real_vect",             (void *)host_AIL_unused },
    { 0x38262, "AIL_restore_USE16_ISR",         (void *)host_AIL_unused },
    { 0x382E9, "AIL_call_driver",               (void *)host_AIL_unused },
    { 0x383F1, "AIL_delay",                     (void *)host_AIL_unused },
    { 0x38463, "AIL_API_read_INI",              (void *)host_AIL_unused },
    { 0x387BC, "AIL_register_timer",            (void *)host_AIL_unused },
    { 0x388A7, "AIL_set_timer_user",            (void *)host_AIL_unused },
    { 0x3899A, "AIL_set_timer_period",          (void *)host_AIL_unused },
    { 0x38A10, "AIL_set_timer_frequency",       (void *)host_AIL_unused },
    { 0x38BD9, "AIL_start_timer",               (void *)host_AIL_unused },
    { 0x38CA8, "AIL_stop_timer",                (void *)host_AIL_unused },
    { 0x38D77, "AIL_release_timer_handle",      (void *)host_AIL_unused },
    { 0x38DE4, "AIL_release_all_timers",        (void *)host_AIL_unused },
    { 0x38E46, "AIL_get_IO_environment",        (void *)host_AIL_unused },
    { 0x38F2B, "AIL_install_driver",            (void *)host_AIL_unused },
    { 0x3901E, "AIL_uninstall_driver",          (void *)host_AIL_unused },
    { 0x39176, "AIL_install_DIG_driver_file",   (void *)host_AIL_unused },
    { 0x394B4, "AIL_release_sample_handle",     (void *)host_AIL_unused },
    { 0x39716, "AIL_set_sample_type",           (void *)host_AIL_unused },
    { 0x398DF, "AIL_end_sample",                (void *)host_AIL_unused },
    { 0x3994C, "AIL_set_sample_playback_rate",  (void *)host_AIL_unused },
    { 0x399C2, "AIL_set_sample_volume",         (void *)host_AIL_unused },
    { 0x39A38, "AIL_set_sample_pan",            (void *)host_AIL_unused },
    { 0x39B24, "AIL_sample_status",             (void *)host_AIL_unused },
    { 0x3A644, "AIL_register_EOS_callback",     (void *)host_AIL_unused },
    { 0x3AB49, "AIL_install_MDI_driver_file",   (void *)host_AIL_unused },
    { 0x3B035, "AIL_end_sequence",              (void *)host_AIL_unused },
    { 0x3BDDF, "AIL_branch_index",              (void *)host_AIL_unused },
    { 0x3C11C, "AIL_register_event_callback",   (void *)host_AIL_unused },
    { 0x3C209, "AIL_register_timbre_callback",  (void *)host_AIL_unused },
    { 0x3C4DB, "AIL_lock_channel",              (void *)host_AIL_unused },
    { 0x3C5C0, "AIL_release_channel",           (void *)host_AIL_unused },
    { 0x3C636, "AIL_map_sequence_channel",      (void *)host_AIL_unused },
    /* Reached through a library path, not a direct call from game code: */
    { 0x3B80F, "AIL_install_timbre",            (void *)host_AIL_unused },
};

void ail_set_format(uint32_t sample_rate, int bits, int stereo)
{
    if (sample_rate >= 4000 && sample_rate <= 192000)
        g_rate = sample_rate;
    if (bits == 8 || bits == 16)
        g_bits = bits;
    g_stereo = stereo ? 1 : 0;
}

void ail_install(uint8_t *obj0_base, const char *dump_dir)
{
    size_t i;

    if (g_installed)
        return;
    g_installed = 1;
    if (dump_dir && dump_dir[0])
        strncpy(g_dump_dir, dump_dir, sizeof g_dump_dir - 1);

    for (i = 0; i < sizeof g_entries / sizeof g_entries[0]; i++) {
        const ail_entry *e = &g_entries[i];
        uint8_t *p = obj0_base + (e->addr - AIL_OBJ0_BASE);
        intptr_t rel = (intptr_t)e->impl - (intptr_t)(p + 5);
        p[0] = 0xE9;                        /* jmp rel32 */
        *(int32_t *)(p + 1) = (int32_t)rel;
    }
    printf("ail: patched %u AIL entry points to host implementations"
           " (%u Hz, %d-bit, %d ch)\n",
           (unsigned)(sizeof g_entries / sizeof g_entries[0]),
           (unsigned)g_rate, g_bits, g_stereo ? 2 : 1);
}
