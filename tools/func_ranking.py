#!/usr/bin/env python3
"""func_ranking.py - rank the machine-code functions by how much they are used.

`re/funcmap.csv` already carries the two static usage counts IDA knows:

    callers_game   direct call sites from game code
    data_xrefs     the function's address referenced as *data* (dispatch
                   tables like funcs_25E23[]/funcs_25E3A[], callbacks)

`usage = callers_game + data_xrefs` is the pragmatic "how hot is this" proxy.
It is *static*: it counts places, not executions. It is still the cheapest
sound ordering, and for state handlers reached only through a table it is the
only static signal (they have 0 direct callers).

Joins src/repl.c's wired set (via re/translation_map.csv) so the output shows
what is left to do.

    python tools/func_ranking.py                 # write re/func_ranking.csv
    python tools/func_ranking.py --top 30        # print the top unwired too
    python tools/func_ranking.py --zone game --top 30

See docs/TRANSLATION.md §7.
"""
import argparse
import csv
import io
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "re", "func_ranking.csv")


def load():
    with io.open(os.path.join(ROOT, "re", "funcmap.csv"), encoding="utf-8-sig") as f:
        funcs = list(csv.DictReader(f))
    wired = set()
    tpath = os.path.join(ROOT, "re", "translation_map.csv")
    if os.path.exists(tpath):
        with io.open(tpath, encoding="utf-8") as f:
            for r in csv.DictReader(f):
                if r["status"].startswith("wired"):
                    wired.add(r["addr"].lower())
    for r in funcs:
        cg = int(r["callers_game"] or 0)
        cl = int(r["callers_lib"] or 0)
        dx = int(r["data_xrefs"] or 0)
        sr = int(r["str_refs"] or 0)
        r["_usage"] = cg + dx
        r["_wired"] = "Y" if r["addr"].lower() in wired else "-"
        r["_ints"] = (cg, cl, dx, sr)
    funcs.sort(key=lambda r: (-r["_usage"], int(r["size"])))
    return funcs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--zone", default="", help="only this zone (game/lib_nosym/ail)")
    ap.add_argument("--top", type=int, default=0, help="print this many unwired")
    ap.add_argument("--all", action="store_true", help="include crt_sym too")
    args = ap.parse_args()

    funcs = load()
    rows = [r for r in funcs if r["zone"] != "crt_sym" or args.all]
    if args.zone:
        rows = [r for r in rows if r["zone"] == args.zone]

    with io.open(OUT, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["addr", "size", "name", "zone", "usage", "callers_game",
                    "callers_lib", "data_xrefs", "str_refs", "wired"])
        for r in rows:
            cg, cl, dx, sr = r["_ints"]
            w.writerow([r["addr"], r["size"], r["name"], r["zone"], r["_usage"],
                        cg, cl, dx, sr, r["_wired"]])

    n = len(rows)
    wired = sum(1 for r in rows if r["_wired"] == "Y")
    print("func_ranking: %d rows (non-crt), %d wired, %d left -> %s"
          % (n, wired, n - wired, os.path.relpath(OUT, ROOT)))

    top = args.top
    if top:
        left = [r for r in rows if r["_wired"] != "Y"]
        print("\ntop %d unwired (usage = callers_game + data_xrefs):" % top)
        for r in left[:top]:
            cg, cl, dx, sr = r["_ints"]
            print("  %-9s size=%-5s usage=%-4d cg=%-4d dx=%-3d %-8s %s"
                  % (r["addr"], r["size"], r["_usage"], cg, dx, r["zone"], r["name"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
