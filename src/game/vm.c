/* vm.c - FD2 script/text VM (source translation of 0x15F84).
 *
 * The interpreter walks a signed int16 word stream. Negative words are
 * opcodes, everything else is a glyph index drawn into mode 13h memory:
 *
 *   -1            close the box (if open) and return        [end of stream]
 *   -2            new paragraph, no key wait
 *   -3            new paragraph, wait for a key + typewriter step
 *   -4 / -5       run sub-stream dword_53AD9 / dword_53ADD, keep its return
 *   -6            print the decimal digits of dword_53AE1 as glyphs
 *   -17 / -18     portrait/box for record `operand` in the top/bottom box
 *                 (operand is an id passed to rec_find; 39 = "keep the
 *                 record the interpreter is already holding")
 *   -19 / -20     same, but `operand` indexes the 80-byte record table at
 *                 dword_53A45 directly, no rec_find
 *   >= 0, others  draw one glyph and advance 16 bytes
 *
 * Boxes: dword_53C67 holds the box's VGA offset (0 = closed, 0x728 = top
 * box, 0x9017 = bottom). Opening a box resets the text origin to
 * 0xA0000 + 0xB4F / 0xA0000 + 0x951F and the paragraph counter to 0.
 *
 * The two registers that survive across opcodes matter:
 *   cur   (ebp)  current VGA write address, threaded through the recursion
 *   rec   (edi)  "the record/datum to draw from". It is *not* reset between
 *                opcodes: after every portrait op it is left pointing at
 *                `DATO buffer + its first byte` (that is where the RLE block
 *                handed to 0x4EBFF/0x4EC31 starts), and only -17 with
 *                operand 39 reads it without assigning it first.
 *                At function entry EDI is whatever the caller left there -
 *                IDA's `a5@<edi>` parameter is the stack-probe artifact, no
 *                caller sets it - so that one case cannot be reproduced
 *                exactly and falls back to the last rec_find record with a
 *                log line (vm: -17/39 ...). See docs/rounds/09-vm.md.
 *
 * Verified by src/vmcheck.c against the machine code: same stubs on both
 * sides, whole event sequence + globals + return value compared.
 */
#include "vm.h"
#include <stdio.h>
#include <string.h>

#define VGA_BASE      0x000A0000u
#define BOX_TOP       0x00000728        /* dword_53C67: upper dialog box     */
#define BOX_BOTTOM    0x00009017        /* dword_53C67: lower dialog box     */
#define ADDR_TOP      0x000A0B4Fu       /* text origin inside the top box    */
#define ADDR_BOTTOM   0x000A951Fu       /* ... inside the bottom box         */
#define RECORD_SIZE   80                /* stride of the dword_53A45 table   */
#define FACE_KEEP     39                /* -17 operand: keep the held record */

/* ------------------------------------------------ game data segment ---- */
/* IDA names kept so the C reads like the original decompilation; addresses
 * verified against E:\FD2\FD2.EXE.i64. */
#define dword_53C67  (*(int32_t *)(uintptr_t)0x00053C67u) /* open box state */
#define dword_53A7D  (*(void   **)(uintptr_t)0x00053A7Du) /* script container */
#define dword_53AD9  (*(int32_t *)(uintptr_t)0x00053AD9u) /* -4 sub stream   */
#define dword_53ADD  (*(int32_t *)(uintptr_t)0x00053ADDu) /* -5 sub stream   */
#define dword_53AE1  (*(int32_t *)(uintptr_t)0x00053AE1u) /* number for -6   */
#define dword_53A75  (*(void   **)(uintptr_t)0x00053A75u) /* 16x16 font      */
#define dword_53C1B  (*(void   **)(uintptr_t)0x00053C1Bu) /* rec_find result */
#define dword_53A85  (*(void   **)(uintptr_t)0x00053A85u) /* DATO buffer     */
#define dword_53A45  (*(void   **)(uintptr_t)0x00053A45u) /* record table    */

/* ------------------------------------------- services at original addrs -- */
typedef void *(*open_box_fn)(int face_x, int face_y, int rows);
typedef void  (*close_box_fn)(void *box, int rows);
typedef void  (*dato_fn)(int mode);
typedef void  (*wait_fn)(int advance);
typedef void  (*scroll_fn)(void);
typedef int   (*type_step_fn)(void);
typedef int   (*rec_find_fn)(int id);
typedef void *(*res_fn)(const char *name, void *cur, int index);
typedef int   (*draw_fn)(void *font, int glyph, int addr, int pitch,
                         int fg, int shadow, int fill);
typedef void  (*blit_fn)(void *dst, const void *src, int len);
typedef int   (*pending_fn)(void);

#define ORIG_OPEN      ((open_box_fn) (uintptr_t)0x000165ACu)
#define ORIG_CLOSE     ((close_box_fn)(uintptr_t)0x00016B43u)
#define ORIG_DATO      ((dato_fn)     (uintptr_t)0x00016559u)
#define ORIG_WAIT      ((wait_fn)     (uintptr_t)0x00016C57u)
#define ORIG_SCROLL    ((scroll_fn)   (uintptr_t)0x00016E24u)
#define ORIG_TYPE_STEP ((type_step_fn)(uintptr_t)0x000164E8u)
#define ORIG_REC_FIND  ((rec_find_fn) (uintptr_t)0x00012C60u)
#define ORIG_RES       ((res_fn)      (uintptr_t)0x000111BAu)
#define ORIG_DRAW      ((draw_fn)     (uintptr_t)0x0004ED7Au)
#define ORIG_RLE_TOP   ((blit_fn)     (uintptr_t)0x0004EBFFu)
#define ORIG_RLE_BOT   ((blit_fn)     (uintptr_t)0x0004EC31u)
#define ORIG_PENDING   ((pending_fn)  (uintptr_t)0x00010620u)

/* Close whatever box is open - the same three calls in the same order the
 * machine code uses at the top of -1/-17/-18/-19/-20. */
static void close_open_box(int box, int mode)
{
    if (!box)
        return;
    ORIG_DATO(0);
    ORIG_WAIT(0);
    ORIG_CLOSE((void *)(intptr_t)box, mode);
}

/* Shared tail of -17/-19 (top box, RLE 0x4EBFF) and -18/-20 (bottom box,
 * RLE 0x4EC31): open the box on `rec`, blit the DATO image, reset the text
 * origin. `rec` is deliberately left pointing at the RLE block, exactly
 * where the machine code leaves EDI. */
static void open_and_blit(int box_id, uint8_t *rec, int mode, int rle_top,
                          int *box, uint8_t **rec_io, int *start, int *cur)
{
    uint8_t *buf;
    int      origin = (box_id == BOX_TOP) ? (int)ADDR_TOP : (int)ADDR_BOTTOM;

    *box = (int)(intptr_t)ORIG_OPEN(rec[0], rec[1], mode);

    buf    = (uint8_t *)dword_53A85;
    *rec_io = buf + *buf;                       /* EDI after the blit setup */
    if (rle_top)
        ORIG_RLE_TOP((void *)(uintptr_t)(VGA_BASE + (uint32_t)dword_53C67),
                     *rec_io, 320);
    else
        ORIG_RLE_BOT((void *)(uintptr_t)(VGA_BASE + (uint32_t)dword_53C67),
                     *rec_io, 320);

    *start = origin;
    *cur   = origin;
}

int vm_run(void *stream, int sub, int addr, int pitch,
           int fg, int shadow, int bgfill, int line_step, int wait)
{
    const uint8_t *s   = (const uint8_t *)stream;
    const int16_t *p   = (const int16_t *)(s +
                          *(const int16_t *)(s + 2 * sub));
    int      start = addr;                      /* arg_8: text origin      */
    int      cur   = addr;                      /* ebp: write address      */
    int      line  = 0;                         /* var_18: paragraph index */
    int      box   = 0;                         /* var_1C: open box handle */
    int      mode  = 0;                         /* var_20: box row count   */
    uint8_t *rec   = NULL;                      /* edi: record/datum       */
    char     num[12];
    int      op, i, len;

    for (;;) {
        const int16_t *next = p + 1;            /* var_28                  */

        op = *p;
        switch (op) {

        case -1:                                /* end of stream           */
            /* the original clears dword_53C67 only inside the `box != 0`
             * branch (0x164B1 jumps straight past the `mov ...,0`) */
            if (box) {
                close_open_box(box, mode);
                dword_53C67 = 0;
            }
            return cur;

        case -2:                                /* new paragraph, no wait  */
            if ((dword_53C67 == BOX_TOP || dword_53C67 == BOX_BOTTOM) &&
                line == 3) {
                ORIG_SCROLL();
                line--;
            }
            line++;
            cur = start + pitch * line_step * line;
            p = next;
            continue;

        case -3:                                /* new paragraph + wait    */
            if ((dword_53C67 == BOX_TOP || dword_53C67 == BOX_BOTTOM) &&
                line == 3) {
                ORIG_SCROLL();
                line--;
            }
            line++;
            cur = start + pitch * line_step * line;
            p = next;
            if (dword_53C67 == BOX_TOP || dword_53C67 == BOX_BOTTOM)
                ORIG_DATO(0);
            ORIG_WAIT(1);
            wait = 1;
            continue;

        case -4:                                /* sub-stream (branch A)   */
            cur = vm_run(dword_53A7D, dword_53AD9, cur, pitch,
                         205, 76, 74, 19, 1);
            p = next;
            continue;

        case -5:                                /* sub-stream (branch B)   */
            cur = vm_run(dword_53A7D, dword_53ADD, cur, pitch,
                         205, 76, 74, 19, 1);
            p = next;
            continue;

        case -6:                                /* decimal number as glyphs*/
            sprintf(num, "%d", dword_53AE1);
            len = (int)strlen(num);
            for (i = 0; i < len; i++) {
                ORIG_DRAW(dword_53A75, num[i] - '0', cur, pitch,
                          fg, shadow, bgfill);
                if (ORIG_PENDING())
                    wait = 0;
                if (wait)
                    ORIG_TYPE_STEP();
                cur += 16;
            }
            p = next;
            continue;

        case -17: {                             /* top box, by record id   */
            int id = (uint16_t)p[1];

            close_open_box(box, mode);
            dword_53C67 = BOX_TOP;
            mode = (ORIG_REC_FIND(id) == -1) ? 0 : 2;
            if (id != FACE_KEEP) {
                rec = (uint8_t *)dword_53C1B;
                id  = rec[7];
            } else if (rec == NULL) {
                /* the only read of EDI before any write inside this call */
                printf("vm: -17/%d as the first portrait op - the original"
                       " reads the caller's EDI; using the last rec_find"
                       " record\n", id);
                rec = (uint8_t *)dword_53C1B;
            }
            dword_53A85 = ORIG_RES("DATO.DAT", dword_53A85, id);
            p = next + 1;                       /* opcode + operand        */
            open_and_blit(BOX_TOP, rec, mode, 1, &box, &rec, &start, &cur);
            wait = 1;
            line = 0;
            continue;
        }

        case -18: {                             /* bottom box, by record id*/
            int id = (uint16_t)p[1];

            close_open_box(box, mode);
            dword_53C67 = BOX_BOTTOM;
            mode = (ORIG_REC_FIND(id) == -1) ? 0 : 0x70;
            rec  = (uint8_t *)dword_53C1B;
            id   = rec[7];
            dword_53A85 = ORIG_RES("DATO.DAT", dword_53A85, id);
            p = next + 1;
            open_and_blit(BOX_BOTTOM, rec, mode, 0, &box, &rec, &start, &cur);
            wait = 1;
            line = 0;
            continue;
        }

        case -19: {                             /* top box, table index    */
            int idx = (uint16_t)p[1];

            close_open_box(box, mode);
            dword_53C67 = BOX_TOP;
            mode = 2;
            rec  = (uint8_t *)dword_53A45 + RECORD_SIZE * idx;
            dword_53A85 = ORIG_RES("DATO.DAT", dword_53A85, rec[7]);
            p = next + 1;
            open_and_blit(BOX_TOP, rec, mode, 1, &box, &rec, &start, &cur);
            wait = 1;
            line = 0;
            continue;
        }

        case -20: {                             /* bottom box, table index */
            int idx = (uint16_t)p[1];

            close_open_box(box, mode);
            dword_53C67 = BOX_BOTTOM;
            mode = 0x70;
            rec  = (uint8_t *)dword_53A45 + RECORD_SIZE * idx;
            dword_53A85 = ORIG_RES("DATO.DAT", dword_53A85, rec[7]);
            p = next + 1;
            open_and_blit(BOX_BOTTOM, rec, mode, 0, &box, &rec, &start, &cur);
            wait = 1;
            line = 0;
            continue;
        }

        default:                                /* one glyph              */
            ORIG_DRAW(dword_53A75, op, cur, pitch, fg, shadow, bgfill);
            cur += 16;
            p = next;
            if (ORIG_PENDING())
                wait = 0;
            if (wait)
                ORIG_TYPE_STEP();
            continue;
        }
    }
}
