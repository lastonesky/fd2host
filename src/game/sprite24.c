/* sprite24.c - FD2 24x24 sprite RLE family (source translation).
 *
 * Translation of seven original routines that share one state machine:
 *   0x4DF84, 0x4E016, 0x4E0A2, 0x4E127, 0x4E1A6, 0x4E22A, 0x4E29C
 *
 * The original duplicated the loop seven times (one per colour mode); here
 * the loop exists once and the differences are described by a small mode
 * struct. The per-pixel colour mapping and the transparent-token action are
 * the only variables, matching the disassembly in re/sprite24_disasm.txt.
 *
 * Verified byte-for-byte against the machine code by src/sprite24check.c.
 * Format / ABI documentation: game/sprite24.h.
 */
#include "sprite24.h"
#include <string.h>

#define S24_ROWS 24
#define S24_COLS 24

enum s24_map_kind { S24_IDENT, S24_RAMP, S24_PAL, S24_CONST, S24_RAMP24 };
enum s24_t11_kind { S24_T11_SKIP, S24_T11_RECOLOR, S24_T11_FILL49 };

struct s24_mode {
    unsigned            kind;      /* colour map                       */
    unsigned char       base;      /* RAMP: base colour                */
    unsigned char       rot;       /* RAMP: index rotation             */
    const unsigned char *pal;      /* PAL: 256-byte table              */
    unsigned char       constant;  /* CONST: frozen colour             */
    unsigned            t11;       /* special-token behaviour          */
};

static unsigned char s24_map(const struct s24_mode *m, unsigned char c)
{
    switch (m->kind) {
    case S24_RAMP:   return (unsigned char)(m->base + ((m->rot + c) & 7));
    case S24_PAL:    return m->pal[c];
    case S24_CONST:  return m->constant;
    case S24_RAMP24: return (unsigned char)((c & 7) + 24);
    default:         return c;
    }
}

/* The original loop: rows of exactly 24 pixels, `stride - 24` skipped at
 * the end of each row. `left` is an 8-bit counter and the stream is
 * guaranteed to fill the row exactly (as in the game data). */
static void s24_run(const uint8_t *src, uint8_t *dst, int stride,
                    const struct s24_mode *m)
{
    uint8_t *p    = dst;
    int      skip = stride - S24_COLS;
    int      rows = S24_ROWS;

    do {
        uint8_t left = S24_COLS;
        do {
            unsigned tok   = *src++;
            unsigned count = (tok & 63) + 1;

            switch (tok >> 6) {
            case 0: {                                   /* solid run */
                unsigned char v = s24_map(m, *src++);
                memset(p, v, count);
                p    += count;
                left  = (uint8_t)(left - count);
                break;
            }
            case 1: {                                   /* odd run */
                unsigned char v = s24_map(m, *src++);
                unsigned n = count;
                left = (uint8_t)(left - 2 * count);
                do {
                    p[1] = v;
                    p   += 2;
                } while (--n);
                break;
            }
            case 2: {                                   /* literal */
                unsigned n = count;
                do {
                    *p++ = s24_map(m, *src++);
                } while (--n);
                left = (uint8_t)(left - count);
                break;
            }
            default:                                    /* special */
                switch (m->t11) {
                case S24_T11_RECOLOR: {
                    unsigned n = count;
                    do {
                        *p = s24_map(m, *p);
                        ++p;
                    } while (--n);
                    break;
                }
                case S24_T11_FILL49:
                    memset(p, 0x49, count);
                    p += count;
                    break;
                default:                                /* skip */
                    p += count;
                    break;
                }
                left = (uint8_t)(left - count);
                break;
            }
        } while (left != 0);

        p += skip;
    } while (--rows != 0);
}

void sprite24_ramp(const void *src, void *dst, int stride, int base, int rot)
{
    struct s24_mode m;
    memset(&m, 0, sizeof m);
    m.kind = S24_RAMP;
    m.base = (unsigned char)base;
    m.rot  = (unsigned char)rot;
    m.t11  = S24_T11_SKIP;
    s24_run((const uint8_t *)src, (uint8_t *)dst, stride, &m);
}

void sprite24_pal_recolor(const void *src, void *dst, int stride,
                          const void *palette)
{
    struct s24_mode m;
    memset(&m, 0, sizeof m);
    m.kind = S24_PAL;
    m.pal  = (const unsigned char *)palette;
    m.t11  = S24_T11_RECOLOR;
    s24_run((const uint8_t *)src, (uint8_t *)dst, stride, &m);
}

void sprite24_pal(const void *src, void *dst, int stride, const void *palette)
{
    struct s24_mode m;
    memset(&m, 0, sizeof m);
    m.kind = S24_PAL;
    m.pal  = (const unsigned char *)palette;
    m.t11  = S24_T11_SKIP;
    s24_run((const uint8_t *)src, (uint8_t *)dst, stride, &m);
}

void sprite24_const(const void *src, void *dst, int stride)
{
    struct s24_mode m;
    memset(&m, 0, sizeof m);
    m.kind     = S24_CONST;
    m.constant = (unsigned char)stride;   /* original: colour = (u8)arg2 */
    m.t11      = S24_T11_SKIP;
    s24_run((const uint8_t *)src, (uint8_t *)dst, stride, &m);
}

void sprite24_ramp24(const void *src, void *dst, int stride)
{
    struct s24_mode m;
    memset(&m, 0, sizeof m);
    m.kind = S24_RAMP24;
    m.t11  = S24_T11_SKIP;
    s24_run((const uint8_t *)src, (uint8_t *)dst, stride, &m);
}

void sprite24_plain(const void *src, void *dst, int stride)
{
    struct s24_mode m;
    memset(&m, 0, sizeof m);
    m.kind = S24_IDENT;
    m.t11  = S24_T11_SKIP;
    s24_run((const uint8_t *)src, (uint8_t *)dst, stride, &m);
}

void sprite24_plain49(const void *src, void *dst, int stride)
{
    struct s24_mode m;
    memset(&m, 0, sizeof m);
    m.kind = S24_IDENT;
    m.t11  = S24_T11_FILL49;
    s24_run((const uint8_t *)src, (uint8_t *)dst, stride, &m);
}
