/* vmcheck.c - differential test for the script VM (original 0x15F84).
 *
 *   0x15F84  vm_run  (src/game/vm.c) - the int16 word-stream interpreter:
 *            glyphs, paragraph opcodes, portrait/box opcodes, the decimal
 *            number printer and the two recursive sub-streams.
 *
 * Method (same one typecheck.c uses): every service the VM calls is hooked
 * to a *recording stub* before either side runs, so the original machine
 * code and the C both go through the identical stubs with identical scripted
 * return values. What is compared per case is therefore the VM's own
 * behaviour only:
 *
 *   - the complete event sequence: every callee in order with its arguments
 *     (glyph + destination address, box open/close, rec_find ids, resource
 *     indices, blit destinations - i.e. everything the VM could possibly do
 *     to the outside world, since no blitter really runs)
 *   - the globals the VM writes: dword_53C67 (open box), dword_53A85 (DATO
 *     buffer), dword_53C1B (last record)
 *   - the return value: the final VGA write address, which the recursion
 *     threads through
 *
 * Recursion stays honest by construction: the C calls vm_run directly, the
 * original calls 0x15F84 (never hooked here), so each side recurses into
 * itself.
 *
 * One path cannot be compared and is excluded by the generator - see the
 * comment above `gen_stream`: -17 with operand 39 reads the interpreter's
 * `rec` register without assigning it, and at function entry that register
 * is whatever the *caller* left there (no caller sets it: IDA's
 * `a5@<edi>` parameter is the stack-probe artifact). The C falls back to the
 * last rec_find record and prints `vm: -17/39 ...` if it ever happens in
 * the real game (src/game/vm.c, docs/rounds/09-vm.md).
 *
 * Build: pwsh -File build.ps1 -Target vmcheck
 * Run   : build\vmcheck.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "le.h"
#include "game/vm.h"

/* ---------------------------------------------------- game globals ----- */
#define dword_53C67  (*(int32_t *)(uintptr_t)0x00053C67u) /* open box state */
#define dword_53A7D  (*(void   **)(uintptr_t)0x00053A7Du) /* script stream  */
#define dword_53AD9  (*(int32_t *)(uintptr_t)0x00053AD9u) /* -4 sub stream  */
#define dword_53ADD  (*(int32_t *)(uintptr_t)0x00053ADDu) /* -5 sub stream  */
#define dword_53AE1  (*(int32_t *)(uintptr_t)0x00053AE1u) /* number for -6  */
#define dword_53A75  (*(void   **)(uintptr_t)0x00053A75u) /* font table     */
#define dword_53C1B  (*(void   **)(uintptr_t)0x00053C1Bu) /* rec_find result*/
#define dword_53A85  (*(void   **)(uintptr_t)0x00053A85u) /* DATO buffer    */
#define dword_53A45  (*(void   **)(uintptr_t)0x00053A45u) /* record table   */

typedef int (__cdecl *vm_fn)(void *, int, int, int, int, int, int, int, int);
#define ORIG_VM  ((vm_fn)(uintptr_t)0x00015F84u)

/* ------------------------------------------------------ hooking -------- */
static void install_hook(uint32_t addr, const void *dest)
{
    uint8_t *p   = (uint8_t *)(uintptr_t)addr;
    int32_t  rel = (int32_t)((const uint8_t *)dest - (p + 5));
    DWORD    old;

    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    p[0] = 0xE9;
    memcpy(p + 1, &rel, 4);
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
}
#define HOOK(addr, fn) install_hook((uint32_t)(addr), (const void *)(fn))

/* --------------------------------------------------- event log --------- */
enum { EV_DRAW, EV_PENDING, EV_STEP, EV_WAIT, EV_DATO, EV_SCROLL,
       EV_CLOSE, EV_OPEN, EV_REC, EV_RES, EV_RLE_TOP, EV_RLE_BOT };
static const char *const kind_name[] = {
    "draw", "pending", "step", "wait", "dato", "scroll",
    "close", "open", "rec", "res", "rle_top", "rle_bot"
};

#define MAXEV 2048
struct event { int kind; int a; int b; int c; int d; };

static struct event g_ev[MAXEV];
static int          g_nev;

static void ev_log(int kind, int a, int b, int c, int d)
{
    if (g_nev >= MAXEV) { printf("FAIL: event log overflow\n"); exit(1); }
    g_ev[g_nev].kind = kind;
    g_ev[g_nev].a = a; g_ev[g_nev].b = b;
    g_ev[g_nev].c = c; g_ev[g_nev].d = d;
    g_nev++;
}

/* --------------------------------------------- scripted service state -- */
static uint8_t *g_dato;      /* DATO.DAT buffer the res stub hands back    */
static uint8_t *g_records;   /* 80-byte record table behind dword_53A45    */
static int      g_pending_i; /* call counter driving "is a key waiting?"   */

#define RECORDS  8
#define DATO_LEN 256

static int __cdecl stub_pending(void)
{
    /* every third call reports a key: exercises the `wait = 0` branch that
     * switches the typewriter off for the rest of the stream */
    int r = (g_pending_i++ % 3) == 0;
    ev_log(EV_PENDING, r, 0, 0, 0);
    return r;
}

static int __cdecl stub_rec_find(int id)
{
    int found = (id % 5) != 4;

    dword_53C1B = g_records + 80 * (unsigned)(id % RECORDS);
    ev_log(EV_REC, id, found ? (id % RECORDS) : -1, 0, 0);
    return found ? id : -1;
}

static void *__cdecl stub_res(const char *name, void *cur, int index)
{
    (void)name;
    ev_log(EV_RES, index, (int)(intptr_t)cur, 0, 0);
    return g_dato;
}

static void *__cdecl stub_open(int x, int y, int mode)
{
    ev_log(EV_OPEN, x, y, mode, 0);
    return (void *)(intptr_t)(0x4000 + mode);
}

static void __cdecl stub_close(void *box, int mode)
{
    ev_log(EV_CLOSE, (int)(intptr_t)box, mode, 0, 0);
}

static void __cdecl stub_dato(int mode)
{
    ev_log(EV_DATO, mode, 0, 0, 0);
}

static void __cdecl stub_wait(int advance)
{
    ev_log(EV_WAIT, advance, 0, 0, 0);
}

static void __cdecl stub_scroll(void)
{
    ev_log(EV_SCROLL, 0, 0, 0, 0);
}

static int __cdecl stub_type_step(void)
{
    ev_log(EV_STEP, 0, 0, 0, 0);
    return 0;
}

static int __cdecl stub_draw(void *font, int glyph, int addr, int pitch,
                             int fg, int shadow, int fill)
{
    (void)font;
    ev_log(EV_DRAW, glyph, addr, pitch, (fg << 16) | (shadow << 8) | fill);
    return 0;
}

static void __cdecl stub_rle_top(void *dst, const void *src, int len)
{
    ev_log(EV_RLE_TOP, (int)(intptr_t)dst, (int)(intptr_t)src, len, 0);
}

static void __cdecl stub_rle_bot(void *dst, const void *src, int len)
{
    ev_log(EV_RLE_BOT, (int)(intptr_t)dst, (int)(intptr_t)src, len, 0);
}

/* ------------------------------------------------------ capture -------- */
static int failures;
static unsigned cases_run;

struct capture {
    int32_t ret;
    int32_t box_state;
    void   *dato;
    void   *rec;
    void   *stream;
    int     nev;
    struct event ev[MAXEV];
};

static void snapshot(struct capture *c, int ret)
{
    c->ret       = ret;
    c->box_state = dword_53C67;
    c->dato      = dword_53A85;
    c->rec       = dword_53C1B;
    c->stream    = dword_53A7D;
    c->nev       = g_nev;
    memcpy(c->ev, g_ev, sizeof g_ev);
}

static int cmp_capture(unsigned id, const struct capture *o,
                       const struct capture *t)
{
    int bad = 0;
    int i;

    if (o->ret != t->ret) {
        printf("FAIL case %u: return orig=%d ours=%d\n", id, o->ret, t->ret);
        failures++; bad = 1;
    }
    if (o->box_state != t->box_state || o->dato != t->dato ||
        o->rec != t->rec || o->stream != t->stream) {
        printf("FAIL case %u: globals orig=(%d,%p,%p,%p) ours=(%d,%p,%p,%p)\n",
               id, o->box_state, o->dato, o->rec, o->stream,
               t->box_state, t->dato, t->rec, t->stream);
        failures++; bad = 1;
    }
    if (o->nev != t->nev) {
        int upto = (o->nev < t->nev) ? o->nev : t->nev;
        int at   = upto;
        int lo, hi;

        for (i = 0; i < upto; i++)
            if (memcmp(&o->ev[i], &t->ev[i], sizeof o->ev[0]) != 0) {
                at = i;
                break;
            }
        printf("FAIL case %u: event count orig=%d ours=%d, first diff at "
               "event %d\n", id, o->nev, t->nev, at);
        lo = (at > 3) ? at - 3 : 0;
        hi = at + 4;
        if (hi > o->nev) hi = o->nev;
        if (hi > t->nev) hi = t->nev;
        for (i = lo; i < hi; i++)
            printf("   %s[%d] orig=%-8s(%d,%d,%d,%06X)  ours=%-8s(%d,%d,%d,%06X)%s\n",
                   (i == at) ? "->" : "  ", i,
                   kind_name[o->ev[i].kind], o->ev[i].a, o->ev[i].b,
                   o->ev[i].c, o->ev[i].d,
                   kind_name[t->ev[i].kind], t->ev[i].a, t->ev[i].b,
                   t->ev[i].c, t->ev[i].d,
                   (i == at) ? "   <-- first difference" : "");
        failures++; bad = 1;
    } else {
        for (i = 0; i < o->nev; i++) {
            const struct event *eo = &o->ev[i], *et = &t->ev[i];
            if (memcmp(eo, et, sizeof *eo) != 0) {
                printf("FAIL case %u: event %d orig=%s(%d,%d,%d,%06X) "
                       "ours=%s(%d,%d,%d,%06X)\n", id, i,
                       kind_name[eo->kind], eo->a, eo->b, eo->c, eo->d,
                       kind_name[et->kind], et->a, et->b, et->c, et->d);
                failures++; bad = 1;
                break;
            }
        }
    }
    cases_run++;
    return bad;
}

/* ------------------------------------------------- input preparation --- */
#define STREAM_WORDS  512
#define SUB_A          1     /* dword_53AD9: where -4 goes */
#define SUB_B          2     /* dword_53ADD: where -5 goes */
#define N_SUB          3

static int16_t *g_buf;       /* the script container both sides parse */

static uint32_t seed = 0xC0FFEEu;
static uint32_t rnd(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}

static void reset_world(void *stream, int sub)
{
    dword_53A7D  = stream;
    dword_53AD9  = SUB_A;
    dword_53ADD  = SUB_B;
    dword_53A75  = (void *)(uintptr_t)0x60001000u;   /* canned font        */
    dword_53A45  = g_records;
    dword_53A85  = g_dato;
    dword_53C1B  = g_records;
    dword_53AE1  = 12345;
    dword_53C67  = 0;
    (void)sub;
    g_pending_i  = 0;
    g_nev        = 0;
}

/* Build one sub-stream of random *legal* words, always ending in -1.
 *
 * `allow_recurse` is 0 for the two sub-streams so -4/-5 cannot nest (the
 * machine code has no depth limit either; the generator just refuses to
 * build an unbounded case).
 *
 * `seen_portrait` keeps -17/operand 39 out of the *first* portrait slot of a
 * stream: that combination reads the interpreter's `rec` register without
 * assigning it, and at entry that register is the caller's leftover (see the
 * file header). Every other -17 operand, and 39 after any other portrait
 * opcode, is exercised.
 */
static int gen_stream(int16_t *w, int max, int allow_recurse)
{
    int n = 0;
    int seen_portrait = 0;
    /* 4..63 words: long enough to reach every opcode, short enough that the
     * event log of one case stays well under MAXEV even with -4/-5 */
    int limit = 4 + (int)(rnd() % 60);
    (void)max;

    while (n < limit) {
        uint32_t r = rnd() % 100;
        int16_t  op;

        if (r < 45) {
            op = (int16_t)(rnd() % 64);              /* a glyph            */
        } else if (r < 53) {
            op = -2;                                 /* new paragraph      */
        } else if (r < 61) {
            op = -3;                                 /* paragraph + wait   */
        } else if (r < 67) {
            op = -6;                                 /* the number printer */
        } else if (r < 74) {
            op = -17;                                /* top box, by id     */
        } else if (r < 81) {
            op = -18;                                /* bottom box, by id  */
        } else if (r < 86) {
            op = -19;                                /* top box, by index  */
        } else if (r < 91) {
            op = -20;                                /* bottom box, index  */
        } else if (r < 95 && allow_recurse) {
            op = -4;
        } else if (allow_recurse) {
            op = -5;
        } else {
            op = (int16_t)(rnd() % 64);
        }

        w[n++] = op;
        if (op == -17 || op == -18 || op == -19 || op == -20) {
            int16_t operand;
            if (op == -17 || op == -18) {
                operand = (seen_portrait && (rnd() % 4) == 0)
                        ? 39 : (int16_t)(rnd() % 64);
                /* the first portrait op of a stream must not be 39: that
                 * reads `rec` before anything assigned it (see the header)
                 * - rnd() % 64 can produce 39 on its own */
                if (!seen_portrait && operand == 39)
                    operand = 38;
            } else {
                operand = (int16_t)(rnd() % RECORDS);   /* table index      */
            }
            seen_portrait = 1;
            w[n++] = operand;
        }
    }
    w[n++] = -1;
    return n;
}

/* Container layout: int16 byte-offsets (one per sub-stream) then the
 * sub-streams themselves, all relative to the container base. */
static int build_container(uint32_t nseed)
{
    int16_t  off[N_SUB];
    int      n = 0;
    int      i;
    int      words_hdr = N_SUB;          /* the offset table itself       */

    seed = nseed;
    for (i = 0; i < N_SUB; i++) {
        off[i] = (int16_t)((words_hdr + n) * 2);
        n += gen_stream(&g_buf[words_hdr + n], STREAM_WORDS - words_hdr - n,
                        i == 0);
    }
    for (i = 0; i < N_SUB; i++)
        g_buf[i] = off[i];
    return words_hdr + n;
}

/* ----------------------------------------------------------- runs ------ */
/* VMONLY=<id> runs a single case while hunting a crash */
static int want(unsigned id)
{
    const char *s = getenv("VMONLY");
    return !s || (unsigned)atoi(s) == id;
}

static void dump_container(int words)
{
    int i;
    for (i = 0; i < words; i++)
        printf("%d%s", g_buf[i], ((i + 1) % 16) ? " " : "\n");
    printf("\n");
}

static void run_case(unsigned id, uint32_t nseed, int sub, int addr,
                     int wait)
{
    static struct capture co, ct;
    int ro, rc, nwords;

    if (!want(id))
        return;
    nwords = build_container(nseed);
    if (getenv("VMTRACE")) {
        printf("[case %u random sub=%d addr=0x%X wait=%d]\n", id, sub,
               addr, wait);
        dump_container(nwords);
    }

    reset_world(g_buf, sub);
    ro = ORIG_VM(g_buf, sub, addr, 320, 205, 76, 74, 19, wait);
    snapshot(&co, ro);

    reset_world(g_buf, sub);
    rc = vm_run(g_buf, sub, addr, 320, 205, 76, 74, 19, wait);
    snapshot(&ct, rc);

    cmp_capture(id, &co, &ct);
}

/* Constructed cases: the branches a random stream reaches rarely. */
static void fixed_case(unsigned id, const int16_t *words, int nwords,
                       int addr, int wait, int32_t number)
{
    static struct capture co, ct;
    int i, ro, rc;

    if (!want(id))
        return;
    if (getenv("VMTRACE")) {
        printf("[case %u fixed addr=0x%X wait=%d]\n", id, addr, wait);
        for (i = 0; i < nwords; i++)
            printf("%d%s", words[i], ((i + 1) % 16) ? " " : "\n");
        printf("\n");
    }

    for (i = 0; i < N_SUB; i++)
        g_buf[i] = (int16_t)((N_SUB + 4) * 2);
    /* sub A must *not* be the main stream: -4/-5 would recurse forever */
    g_buf[0] = (int16_t)((N_SUB + 4) * 2);      /* word 7: the fixed stream */
    g_buf[1] = (int16_t)(N_SUB * 2);             /* word 3: one glyph, end   */
    g_buf[2] = (int16_t)((N_SUB + 2) * 2);       /* word 5: ends immediately */
    g_buf[N_SUB]     = 10;
    g_buf[N_SUB + 1] = -1;                       /* sub A: ends here         */
    g_buf[N_SUB + 2] = -1;                       /* sub B                    */
    g_buf[N_SUB + 3] = -1;
    if (getenv("VMTRACE")) printf("[case %u: fixed]\n", id);
    for (i = 0; i < nwords; i++)
        g_buf[N_SUB + 4 + i] = words[i];

    reset_world(g_buf, 0);
    dword_53AE1 = number;
    ro = ORIG_VM(g_buf, 0, addr, 320, 205, 76, 74, 19, wait);
    snapshot(&co, ro);

    reset_world(g_buf, 0);
    dword_53AE1 = number;
    rc = vm_run(g_buf, 0, addr, 320, 205, 76, 74, 19, wait);
    snapshot(&ct, rc);

    cmp_capture(id, &co, &ct);
}

int main(int argc, char **argv)
{
    static const int16_t only_end[]     = { -1 };
    static const int16_t glyphs[]       = { 5, 10, 63, 0, 12, -1 };
    static const int16_t number_neg[]   = { -6, -6, -1 };
    /* portrait, then three paragraphs: the line==3 scroll branch needs
     * dword_53C67 to hold a box, which only a portrait opcode sets */
    static const int16_t box_lines[]    = {
        -19, 3, -3, -3, -3, -3, -3, -1
    };
    /* top box then bottom box then -17 with operand 39 (rec already held) */
    static const int16_t keep_face[]    = {
        -19, 1, -18, 2, -17, 39, -17, 39, -1
    };
    static const int16_t recursion[]    = { -4, -5, -4, -1 };
    static const int16_t wait_off[]     = { 20, 21, -3, 22, -1 };
    le_image le;
    int      applied = 0;
    unsigned i;

    if (le_reserve_address_space() != 0) { printf("reserve failed\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (le_open(&le, (argc > 1) ? argv[1] : "E:\\FD2\\FD2.EXE") != 0) return 1;
    if (le_map_and_relocate(&le, &applied) != 0) return 1;
    printf("mapped: %u fixups applied, original code ready\n", (unsigned)applied);

    g_buf     = (int16_t *)calloc(STREAM_WORDS, sizeof(int16_t));
    g_dato    = (uint8_t *)calloc(DATO_LEN, 1);
    g_records = (uint8_t *)calloc(RECORDS * 80, 1);
    if (!g_buf || !g_dato || !g_records) return 2;

    /* A DATO buffer shaped like the real one: byte 0 is the offset of the
     * RLE block the VM hands to the blitter. */
    for (i = 0; i < DATO_LEN; i++)
        g_dato[i] = (uint8_t)(i * 7 + 3);
    g_dato[0] = 6;
    /* record table: [0]=x, [1]=y, [7]=resource index */
    for (i = 0; i < RECORDS; i++) {
        g_records[80 * i + 0] = (uint8_t)(10 + i);
        g_records[80 * i + 1] = (uint8_t)(20 + i);
        g_records[80 * i + 7] = (uint8_t)(30 + i);
    }

    /* every service the VM reaches, hooked before either side runs */
    HOOK(0x00010620u, stub_pending);        /* key waiting? (BDA)          */
    HOOK(0x000111BAu, stub_res);            /* load DATO.DAT resource      */
    HOOK(0x00012C60u, stub_rec_find);       /* character record lookup     */
    HOOK(0x000164E8u, stub_type_step);      /* typewriter step             */
    HOOK(0x00016559u, stub_dato);           /* box palette helper          */
    HOOK(0x000165ACu, stub_open);           /* open the dialog box         */
    HOOK(0x00016B43u, stub_close);          /* close it                    */
    HOOK(0x00016C57u, stub_wait);           /* wait for a key              */
    HOOK(0x00016E24u, stub_scroll);         /* scroll the text             */
    HOOK(0x0004EBFFu, stub_rle_top);        /* blit into the top box       */
    HOOK(0x0004EC31u, stub_rle_bot);        /* blit into the bottom box    */
    HOOK(0x0004ED7Au, stub_draw);           /* draw one glyph              */

    /* ---- random streams ------------------------------------------------ */
    for (i = 0; i < 5000 && !failures; i++)
        run_case(1000 + i, 0xA1000000u + i, 0,
                 ((rnd() % 3) ? 0xA00000 : 0xA9F23), (int)(rnd() & 1));
    /* sub-streams reached only through -4/-5 */
    for (i = 0; i < 500 && !failures; i++)
        run_case(7000 + i, 0xB2000000u + i, 1 + (int)(rnd() % 2), 0xA00000,
                 1);

    /* ---- constructed --------------------------------------------------- */
    fixed_case(3000, only_end,   1, 0xA00000, 1, 0);
    fixed_case(3001, glyphs,     6, 0xA9F23, 1, 0);
    fixed_case(3002, glyphs,     6, 0xA00000, 0, 0);       /* wait off      */
    fixed_case(3003, number_neg, 3, 0xA00000, 1, -98765);  /* '-' + digits  */
    fixed_case(3004, number_neg, 3, 0xA00000, 1, 0);
    fixed_case(3005, box_lines,  8, 0xA00000, 1, 0);       /* line==3 scroll*/
    fixed_case(3006, keep_face,  9, 0xA00000, 1, 0);       /* -17/39 held   */
    fixed_case(3007, recursion,  4, 0xA00000, 1, 0);       /* -4/-5 chains  */
    fixed_case(3008, wait_off,   5, 0xA00000, 1, 0);       /* key kills wait*/
    /* INT_MIN is the widest string "%d" can produce: 11 chars + NUL in the
     * original's 12-byte buffer - the exact edge the C must match */
    fixed_case(3010, number_neg, 3, 0xA00000, 1,
               (int32_t)0x80000000);
    /* no portrait opcode at all: -2/-3 must not call scroll/dato */
    {
        static int16_t paras[12];
        int n = 0;
        while (n < 10) paras[n++] = -2;
        paras[n++] = -3;
        paras[n++] = -1;
        fixed_case(3011, paras, n, 0xA9F23, 1, 0);
    }
    /* extreme paragraph counts: line walks well past 3 and wraps the scroll */
    {
        static int16_t many[40];
        int n = 0;
        many[n++] = -19; many[n++] = 0;
        while (n < 38) many[n++] = -3;
        many[n++] = -1;
        fixed_case(3009, many, n, 0xA00000, 1, 0);
    }

    printf("%s: %u cases, %d failures\n", failures ? "FAILED" : "PASS",
           cases_run, failures);
    le_close(&le);
    return failures ? 1 : 0;
}
