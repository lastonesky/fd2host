/* world_load.c - FD2 per-scene world loading (see world_load.h).
 *
 * The original sequences the Watcom CRT (malloc/free/fopen/fclose) and the
 * already-translated resource/RLE/map helpers. Every external call goes
 * through its fixed game address so the host runs the repl-patched C and the
 * harness can hook it.
 */
#include "world_load.h"
#include "guest_mem.h"

#include <stdint.h>
#include <string.h>

/* --- fixed-address helpers --------------------------------------------- */
typedef void *(*resload_fn)(const char *, void *, int);
typedef void  (*rle_fn)(const int16_t *, int, int, void *, int, int);
typedef void  (*h1v_fn)(int);
typedef void  (*h0v_fn)(void);
typedef int   (*entry_fn)(int, void *);
typedef void *(*fopen_fn)(const char *, const char *);
typedef int   (*fclose_fn)(void *);

#define ORIG_RESLOAD   ((resload_fn)(uintptr_t)0x000111BAu)
#define ORIG_RLE       ((rle_fn)    (uintptr_t)0x0004E98Du)
#define ORIG_SCROLL    ((h1v_fn)    (uintptr_t)0x00024D22u)
#define ORIG_FIXREC    ((h1v_fn)    (uintptr_t)0x0004DF4Cu)
#define ORIG_ENTRYDATA ((entry_fn)  (uintptr_t)0x00011019u)
#define ORIG_METRICS   ((h1v_fn)    (uintptr_t)0x0001B750u)
#define ORIG_BUILD     ((h1v_fn)    (uintptr_t)0x00010B4Eu)
#define ORIG_FOPEN     ((fopen_fn)  (uintptr_t)0x00037324u)
#define ORIG_FCLOSE    ((fclose_fn) (uintptr_t)0x0003759Cu)

/* --- globals ----------------------------------------------------------- */
#define dword_53A79 (*(uint32_t *)(uintptr_t)0x00053A79u)
#define dword_53A59 (*(uint32_t *)(uintptr_t)0x00053A59u)
#define dword_53A51 (*(uint32_t *)(uintptr_t)0x00053A51u)
#define dword_53A5D (*(uint32_t *)(uintptr_t)0x00053A5Du)
#define dword_53A69 (*(uint32_t *)(uintptr_t)0x00053A69u)
#define dword_53A45 (*(uint32_t *)(uintptr_t)0x00053A45u)
#define dword_53AFF (*(uint32_t *)(uintptr_t)0x00053AFFu)
#define dword_53B03 (*(uint32_t *)(uintptr_t)0x00053B03u)
#define dword_53BDF (*(uint32_t *)(uintptr_t)0x00053BDFu)
#define dword_53BF7 (*(uint32_t *)(uintptr_t)0x00053BF7u)
#define dword_53BFB (*(uint32_t *)(uintptr_t)0x00053BFBu)
#define dword_53BE7 (*(uint32_t *)(uintptr_t)0x00053BE7u)
#define dword_53BE3 (*(uint32_t *)(uintptr_t)0x00053BE3u)
#define dword_53BEB (*(uint32_t *)(uintptr_t)0x00053BEBu)
#define dword_53AC1 (*(int32_t  *)(uintptr_t)0x00053AC1u)
#define dword_53AC5 (*(int32_t  *)(uintptr_t)0x00053AC5u)
#define dword_53C03 (*(int32_t  *)(uintptr_t)0x00053C03u)
#define p           (*(uint32_t *)(uintptr_t)0x00053A55u)
#define buf         (*(uint32_t *)(uintptr_t)0x00053A61u)

#define REC 0x50u

/* 0x10652 */
void world_load_tiles(void)
{
    int w = 462, h = 226;
    uint8_t tbl = 0x10;

    if (dword_53AFF) guest_free((void *)(uintptr_t)dword_53AFF);
    dword_53AFF = 0;
    if (dword_53B03) guest_free((void *)(uintptr_t)dword_53B03);
    dword_53B03 = 0;

    switch (dword_53C03) {
    case 9: case 24: case 25:
        dword_53AFF = (uint32_t)(uintptr_t)ORIG_RESLOAD(
            "FDOTHER.DAT", (void *)(uintptr_t)dword_53AFF, 15);
        dword_53B03 = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
        break;
    case 17: case 21: case 22: case 27:
        if (dword_53C03 == 21)      { w = 408; h = 276; tbl = 35; }
        else if (dword_53C03 == 22) { w = 408; h = 256; tbl = 40; }
        else if (dword_53C03 == 27) { h = 244; tbl = 46; }
        dword_53AFF = (uint32_t)(uintptr_t)guest_malloc((size_t)h * (size_t)w);
        dword_53B03 = (uint32_t)(uintptr_t)ORIG_RESLOAD(
            "FDOTHER.DAT", (void *)(uintptr_t)dword_53B03, tbl);
        ORIG_RLE((const int16_t *)(uintptr_t)dword_53B03, 0, 0,
                 (void *)(uintptr_t)dword_53AFF, w, -1);
        dword_53B03 = (uint32_t)(uintptr_t)ORIG_RESLOAD(
            "FDOTHER.DAT", (void *)(uintptr_t)dword_53B03, tbl + 1);
        ORIG_RLE((const int16_t *)(uintptr_t)dword_53B03, 0, h / 2,
                 (void *)(uintptr_t)dword_53AFF, w, -1);
        guest_free((void *)(uintptr_t)dword_53B03);
        dword_53B03 = 0;
        break;
    case 23:
        dword_53AFF = (uint32_t)(uintptr_t)guest_malloc(0xEA00u);
        dword_53B03 = (uint32_t)(uintptr_t)ORIG_RESLOAD(
            "FDOTHER.DAT", (void *)(uintptr_t)dword_53B03, 42);
        ORIG_RLE((const int16_t *)(uintptr_t)dword_53B03, 0, 0,
                 (void *)(uintptr_t)dword_53AFF, 312, -1);
        guest_free((void *)(uintptr_t)dword_53B03);
        dword_53B03 = 0;
        ORIG_SCROLL(0);
        break;
    case 28: case 29:
        dword_53AFF = (uint32_t)(uintptr_t)ORIG_RESLOAD(
            "FDOTHER.DAT", (void *)(uintptr_t)dword_53AFF, 55);
        dword_53B03 = (uint32_t)(uintptr_t)guest_malloc(0xFA00u);
        break;
    default:
        break;
    }
}

/* 0x1088D */
void world_load_party(int slot)
{
    uint8_t *fp, *dst, *src;
    int i, n = 0;

    world_load_tiles();

    dword_53A79 = (uint32_t)(uintptr_t)ORIG_RESLOAD(
        "FDTXT.DAT", (void *)(uintptr_t)dword_53A79, slot + 1);
    dword_53A59 = (uint32_t)(uintptr_t)ORIG_RESLOAD(
        "FDFIELD.DAT", (void *)(uintptr_t)dword_53A59, 3 * slot + 2);
    p = (uint32_t)(uintptr_t)ORIG_RESLOAD(
        "FDFIELD.DAT", (void *)(uintptr_t)p, 3 * slot + 1);
    dword_53A51 = (uint32_t)(uintptr_t)ORIG_RESLOAD(
        "FDFIELD.DAT", (void *)(uintptr_t)dword_53A51, 3 * slot);
    dword_53AC1 = *(int16_t *)(uintptr_t)dword_53A51;
    dword_53AC5 = *((int16_t *)(uintptr_t)dword_53A51 + 1);
    dword_53A5D = (uint32_t)(uintptr_t)ORIG_RESLOAD(
        "FDSHAP.DAT", (void *)(uintptr_t)dword_53A5D, 2 * *(uint8_t *)(uintptr_t)p);
    dword_53A69 = (uint32_t)(uintptr_t)ORIG_RESLOAD(
        "FDSHAP.DAT", (void *)(uintptr_t)dword_53A69,
        2 * *(uint8_t *)(uintptr_t)p + 1);
    ORIG_FIXREC((int)(uintptr_t)dword_53A51);
    dword_53BE7 = *((uint8_t *)(uintptr_t)p + 1);
    dword_53BE3 = *((uint8_t *)(uintptr_t)p + 2);
    dword_53BEB = dword_53BE7;

    if (buf) guest_free((void *)(uintptr_t)buf);
    if (dword_53A45) guest_free((void *)(uintptr_t)dword_53A45);
    dword_53A45 = (uint32_t)(uintptr_t)guest_malloc(0x1E00u);
    if (dword_53A45 == 0)
        return;
    dword_53BDF = 0;
    fp = (uint8_t *)ORIG_FOPEN("FDICON.B24", "rb");
    if (fp == NULL)
        return;

    src = (uint8_t *)(uintptr_t)(dword_53A59 + 6 * dword_53BE3 + 2);
    dst = (uint8_t *)(uintptr_t)dword_53A45;
    for (i = 0; i < (int)dword_53BE7; i++) {
        uint8_t *list = (uint8_t *)(uintptr_t)dword_53BF7;
        if ((slot >= 13 || i != 6 || list[80 * i + 8] == 2) &&
            n < (int)dword_53BFB) {
            uint8_t v24, v25;
            memmove(dst, list + 80 * i, 80);
            v24 = src[0];
            v25 = src[2];
            dst[0] = v24;
            dst[1] = v25;
            src += 6;
            dst[2] = (uint8_t)ORIG_ENTRYDATA(dst[7], fp);
            dst[3] = 0;
            dst[4] = 0;
            dst[6] = 2;
            dst[49] = 0xFF;
            memset(dst + 34, 0, 6);
            ORIG_METRICS(i);
            n++;
        } else {
            memset(dst, 0, 80);
            dst[5] = 1;
        }
        dst += 80;
    }
    ORIG_FCLOSE(fp);
    guest_free((void *)(uintptr_t)dword_53A59);
    dword_53A59 = 0;
    ORIG_BUILD(0);
}
