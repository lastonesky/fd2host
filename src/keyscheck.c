/* keyscheck.c - differential test for the portable key table (src/keys.h).
 *
 * The POSIX entry layer cannot call MapVirtualKeyA(), so src/keys.c carries a
 * hand-written BIOS set-1 make-code table. A hand-written table is only worth
 * trusting if it is pinned against the reference the Windows entry layers
 * already use. This test is that pin:
 *
 *   1. every key in FR_KEY_LIST resolves (name -> key -> name)
 *   2. MapVirtualKeyA(fr_key_vk(key), VK_TO_VSC) == the table's scan code
 *   3. the reverse direction is consistent:
 *        fr_key_scan(fr_key_from_vk(vk)) == scan
 *        fr_key_scan(fr_key_from_scan(scan, ext)) == scan
 *      (rows that share a make code - KP7/HOME, RETURN/KP_ENTER, NUM_LOCK/
 *      PAUSE, KPMUL/PRINT - are reported as aliases, not failures)
 *   4. the E0 flag matches the entry layers' is_extended_vk() rule, so the
 *      (scan, ascii) pairs the Linux layer will build are the same ones
 *      main_win32.c/main_sokol.c build today
 *
 * Run: build\keyscheck.exe
 *      build\keyscheck.exe --dump    (print the whole table as a reference)
 *
 * See docs/rounds/16-entry-layer.md.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "keys.h"

static int g_fail;

/* The rule copied verbatim from main_win32.c / main_sokol.c: these are the
 * VKs the BIOS reports with an 0xE0 prefix, i.e. the ones whose ascii byte
 * must be 0xE0 instead of a translated character. */
static int is_extended_vk(int vk)
{
    switch (vk) {
    case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT:
    case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
    case VK_INSERT: case VK_DELETE:
        return 1;
    default:
        return 0;
    }
}

static void fail(const char *what, fr_key k, const char *detail, int a, int b)
{
    printf("FAIL  %-24s key=%s  %s (%d / %d)\n",
           what, fr_key_name(k) ? fr_key_name(k) : "?", detail, a, b);
    g_fail = 1;
}

int main(int argc, char **argv)
{
    int dump = (argc > 1 && !strcmp(argv[1], "--dump"));
    int i, n = fr_key_table_count, aliases = 0, checked = 0;

    /* The enum is generated from FR_KEY_LIST; the table must have one row
     * per key and the last row must be FRK_COUNT-1. If a row were added to
     * the list but the enum were hand-edited, this catches it. */
    if (n != FRK_COUNT - 1) {   /* FRK_NONE + one entry per list row */
        printf("FAIL  table count %d != enum count %d\n", n, FRK_COUNT - 1);
        return 1;
    }

    printf("keyscheck: portable key table vs MapVirtualKeyA (Windows reference)\n");
    for (i = 0; i < n; i++) {
        fr_key  k    = fr_key_table[i].key;
        uint8_t scan = fr_key_table[i].scan;
        int     ext  = fr_key_table[i].ext;
        int     vk   = fr_key_vk(k);
        UINT    vsc;
        fr_key  back, byscan;
        const char *nm;

        if (dump)
            printf("%-20s 0x%02X  0x%02X   %d\n",
                   fr_key_table[i].name, vk, scan, ext);

        /* 1. name round-trip */
        nm = fr_key_name(k);
        if (!nm || strcmp(nm, fr_key_table[i].name) != 0) {
            fail("name", k, "fr_key_name disagrees with the table", 0, 0);
            continue;
        }
        if (fr_key_by_name(fr_key_table[i].name,
                           strlen(fr_key_table[i].name)) != k) {
            fail("lookup", k, "fr_key_by_name does not find its own name", 0, 0);
            continue;
        }

        /* 2. the scan code must be the Windows reference */
        if (!vk) { fail("vk", k, "no VK for key", 0, 0); continue; }
        vsc = MapVirtualKeyA((UINT)vk, MAPVK_VK_TO_VSC);
        if (vsc != scan) {
            fail("scan", k, "MapVirtualKeyA mismatch", (int)vsc, scan);
            continue;
        }
        checked++;

        /* 3. reverse directions (aliases are legal, silent mismatches are not) */
        back = fr_key_from_vk(vk);
        if (back != k) {
            aliases++;
            if (fr_key_scan(back) != scan)
                fail("from_vk", k, "alias resolves to a different scan", 0, 0);
            else
                printf("      alias: VK 0x%02X also feeds %s (shared make code)\n",
                       vk, fr_key_name(back));
        }
        byscan = fr_key_from_scan(scan, ext);
        if (byscan != k) {
            aliases++;
            if (fr_key_scan(byscan) != scan)
                fail("from_scan", k, "scan resolves to a different key", 0, 0);
            else
                printf("      alias: scan 0x%02X%s feeds %s too\n",
                       scan, ext ? "+E0" : "", fr_key_name(byscan));
        }

        /* 4. the E0 rule the entry layers use */
        if (fr_key_extended(k) != is_extended_vk(vk))
            fail("extended", k, "ext flag != is_extended_vk()", 0, 0);
    }

    /* How does the *old* keylog naming see each scan code? It used
     * MapVirtualKeyA(scan, MAPVK_VSC_TO_VK) + its own VK name table; the new
     * keylog uses fr_key_from_scan(scan, ascii==0xE0) intentionally, so this
     * prints where the two disagree (extended keys: VSC_TO_VK has no E0 bit
     * to go by, so it cannot tell KP8 from UP). */
    {
        int diff = 0, i2;
        for (i2 = 0; i2 < n; i2++) {
            uint8_t scan = fr_key_table[i2].scan;
            int vsc_vk = (int)MapVirtualKeyA((UINT)scan, MAPVK_VSC_TO_VK);
            int our_vk = fr_key_vk(fr_key_table[i2].key);
            if (vsc_vk != our_vk) {
                diff++;
                printf("      VSC_TO_VK: scan 0x%02X%s -> VK 0x%02X, portable key says VK 0x%02X (%s)\n",
                       scan, fr_key_table[i2].ext ? "+E0" : "", vsc_vk, our_vk,
                       fr_key_table[i2].name);
            }
        }
        printf("keyscheck: %d scan codes where MapVirtualKeyA(VSC_TO_VK) disagrees with the portable key\n", diff);
    }

    /* A few names --autokey/--keylog already shipped must keep working. */
    {
        static const struct { const char *n; fr_key k; } must[] = {
            { "RETURN", FRK_RETURN }, { "ENTER", FRK_RETURN },
            { "ESC",    FRK_ESCAPE }, { "SPACE", FRK_SPACE },
            { "UP",     FRK_UP },     { "DOWN",  FRK_DOWN },
            { "LEFT",   FRK_LEFT },   { "RIGHT", FRK_RIGHT },
            { "PGUP",   FRK_PAGE_UP },{ "PGDN",  FRK_PAGE_DOWN },
            { "INS",    FRK_INSERT }, { "DEL",   FRK_DELETE },
            { "A",      FRK_A },      { "5",     FRK_5 },
        };
        for (i = 0; i < (int)(sizeof must / sizeof must[0]); i++)
            if (fr_key_by_name(must[i].n, strlen(must[i].n)) != must[i].k) {
                printf("FAIL  legacy name '%s' no longer resolves\n", must[i].n);
                g_fail = 1;
            }
    }

    printf("keyscheck: %d keys pinned against MapVirtualKeyA, %d allowed aliases, %s\n",
           checked, aliases, g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
