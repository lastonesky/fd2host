#!/usr/bin/env python3
"""translation_map.py - the record/map of machine code that has been turned into C.

Sources of truth, in order:

  1. src/repl.c   - the wired entries ({ addr, "c_name", impl, REPL_GROUP }).
                    If it is not in this table it is not replacing machine code
                    in the running host.
  2. src/game/*.c - the translations themselves (the map names their file).
  3. re/funcmap.csv - the full FD2 function list (the denominator).

This script renders those into re/translation_map.csv (one row per translated
function: address, C name, file, group, which check proves it, status) so the
work is reviewable and cannot silently drift from repl.c:

    python tools/translation_map.py            # regenerate re/translation_map.csv
    python tools/translation_map.py --check    # exit 1 if the file is stale

See docs/TRANSLATION.md §4/§6 and docs/rounds/16-entry-layer.md §46.8.
"""
import csv
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "re", "translation_map.csv")

# module file -> (check tool(s), cases, note). Counts come from the round docs
# via docs/TRANSLATION.md §4; update both when a check grows.
MODULE_INFO = {
    "rle.c":      ("rlecheck",      "1900",  "pure compute, machine-code differential"),
    "rle2.c":     ("rle2check",     "1200",  "0xC0-range RLE blits"),
    "gfx.c":      ("gfxcheck",      "1450",  "rect save/restore, blits, glyphs"),
    "sprite24.c": ("sprite24check", "2100",  "24x24 sprite RLE family"),
    "util.c":     ("utilcheck",     "2200",  "byte/palette utilities"),
    "tables.c":   ("tablescheck",   "4528",  "table accessors"),
    "path.c":     ("pathcheck",     "1000",  "terrain cost flood + path trace"),
    "dlg.c":      ("dlgcheck+boxcheck+keycheck+typecheck", "800+240+100+1176",
                   "dialogue helpers, box animation, wait-key, typewriter"),
    "rec.c":      ("reccheck",      "42225", "character record table (incl. round-34 leaves)"),
    "unit.c":     ("reccheck",      "36327", "persistent party roster (0x1145A/0x11506/0x112A5)"),
    "svc.c":      ("typecheck",     "1176",  "BIOS tick wait + PCM SFX (also in typecheck)"),
    "vm.c":       ("vmcheck",       "5512",  "script/text VM 0x15F84"),
    "res.c":      ("rescheck",      "160",   "LMI resource loader; heap via guest_mem"),
    "bgm.c":      ("bgmcheck",      "6000",  "play_bgm 0x25977; services hooked, event log compared"),
    "scene.c":    ("scenecheck",    "100",   "scene_card 0x22E5C; service sequence compared"),
    "fade.c":     ("fadecheck",     "4000",  "palette fades 0x11D40/0x1F882/0x1F525; DAC writes compared"),
    "msg.c":      ("msgcheck",      "465",   "portrait compositor 0x1956B/0x1974C/0x26996; event log + screen pair compared"),
    "ev.c":       ("evcheck",       "2940",  "funcs_1199C event handlers; record table + event log + normalised returns compared"),
}

# Translated and checked, but deliberately *not* in repl.c yet. Keep the reason.
NOT_WIRED = []

ENTRY = re.compile(
    r'\{\s*(0x[0-9A-Fa-f]+),\s*"([^"]+)",\s*\(void\s*\*\)\s*([A-Za-z0-9_]+),\s*(REPL_[A-Z0-9_]+)\s*\}')
FILE_HINT = re.compile(r'\(src/game/([A-Za-z0-9_]+\.c)\)')
CASE_OVERRIDE = {            # addr -> (check, cases) when a module needs a one-off
    0x11DF2: ("fadecheck", "4000"),
    0x126F7: ("mapcheck", "3000"),
    0x12E38: ("mapcheck", "3000"),
    0x1297D: ("mapcheck", "3000"),
    0x12C0D: ("mapcheck", "3000"),
    0x4EB48: ("mapcheck", "3000"),
    0x187D6: ("mapcheck", "3000"),
    0x1875D: ("mapcheck", "3000"),
    0x1AEB1: ("mapcheck", "3000"),
    0x1F183: ("mapcheck", "3000"),
    0x12AC6: ("mapcheck", "3000"),
    0x129EC: ("mapcheck", "3000"),
    0x127E0: ("mapcheck", "4000"),
    0x127A9: ("mapcheck", "500"),
    0x16886: ("mapcheck", "3000"),
    0x134E4: ("mapcheck", "3000"),
    0x4E381: ("leafcheck", "72000"),
    0x10620: ("leafcheck", "72000"),
    0x4EBE3: ("leafcheck", "72000"),
    0x11EB0: ("leafcheck", "72000"),
    0x2EB9F: ("leafcheck", "72000"),
    0x12D7B: ("leafcheck", "72000"),
}


def repl_src():
    with io.open(os.path.join(ROOT, "src", "repl.c"), encoding="utf-8") as f:
        return f.read()


def module_for(name, file_hint):
    """The .c file that defines `name`, using the section hint first (the
    wrappers like rep_dlg_blit live in repl.c but belong to that module)."""
    if file_hint:
        return file_hint
    for fn in os.listdir(os.path.join(ROOT, "src", "game")):
        if not fn.endswith(".c"):
            continue
        with io.open(os.path.join(ROOT, "src", "game", fn), encoding="utf-8",
                     errors="replace") as f:
            body = f.read()
        if re.search(r'\b' + re.escape(name) + r'\s*\(', body):
            return fn
    return ""


def rows():
    src = repl_src()
    out = []
    file_hint = ""
    pos = 0
    for line in src.splitlines():
        hint = FILE_HINT.search(line)
        if hint:
            file_hint = hint.group(1)
        m = ENTRY.search(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        cname = m.group(2)
        group = m.group(4)[len("REPL_"):].lower()
        mod = module_for(cname, file_hint)
        check, cases, note = MODULE_INFO.get(mod, ("?", "?", ""))
        if addr in CASE_OVERRIDE:
            check, cases = CASE_OVERRIDE[addr]
        out.append({
            "addr": "0x%X" % addr,
            "c_name": cname,
            "source_file": "src/game/" + mod if mod else "?",
            "group": group,
            "check": check,
            "cases": cases,
            "status": "wired (repl.c)",
            "note": note,
        })
    for addr, cname, path, group, check, cases, note in NOT_WIRED:
        out.append({
            "addr": "0x%X" % addr,
            "c_name": cname,
            "source_file": path,
            "group": group,
            "check": check,
            "cases": cases,
            "status": "translated, NOT wired",
            "note": note,
        })
    out.sort(key=lambda r: int(r["addr"], 16))
    return out


def funcmap_total():
    path = os.path.join(ROOT, "re", "funcmap.csv")
    if not os.path.exists(path):
        return None
    with io.open(path, encoding="utf-8", errors="replace") as f:
        n = sum(1 for line in f if line.strip())
    return max(n - 1, 0)   # minus header


def render(rs):
    buf = io.StringIO()
    w = csv.DictWriter(buf, fieldnames=["addr", "c_name", "source_file", "group",
                                        "check", "cases", "status", "note"],
                       lineterminator="\n")
    w.writeheader()
    for r in rs:
        w.writerow(r)
    return buf.getvalue()


def main():
    rs = rows()
    text = render(rs)
    total = funcmap_total()
    wired = sum(1 for r in rs if r["status"] == "wired (repl.c)")

    if "--check" in sys.argv:
        cur = ""
        if os.path.exists(OUT):
            with io.open(OUT, encoding="utf-8") as f:
                cur = f.read()
        if cur != text:
            print("translation_map.csv is stale - run tools/translation_map.py")
            return 1
        print("translation_map: up to date (%d wired)" % wired)
        return 0

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with io.open(OUT, "w", encoding="utf-8", newline="") as f:
        f.write(text)
    if total:
        print("translation_map: %d wired / %d functions (%.1f%%), %d translated-not-wired -> %s"
              % (wired, total, 100.0 * wired / total, len(rs) - wired,
                 os.path.relpath(OUT, ROOT)))
    else:
        print("translation_map: %d wired -> %s" % (wired, os.path.relpath(OUT, ROOT)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
