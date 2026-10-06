#!/usr/bin/env python3
"""ghidra_objects.py - (re)export the reference images for letest from Ghidra.

The reference images (build/object1.bin .. object3.bin) are gitignored, so a
fresh clone has none and `letest` can only print its FNV-1a hashes. This tool
pulls the three relocated objects straight out of the Ghidra HTTP bridge
(docs/ENVIRONMENT.md), which is the source of the original judge
("byte-identical to Ghidra's relocated image", AGENTS.md).

    python tools/ghidra_objects.py                 # defaults: local bridge
    python tools/ghidra_objects.py --out /some/dir # write elsewhere

Ghidra is expected to have FD2.EXE open (its own LE loader applies the fixups,
independently of ours). The object ranges come from /list_segments, so the
sizes are whatever Ghidra mapped - for FD2 they are the vsize values
(0x3EF29 / 0x56B0 / 0x34D2).
"""
import argparse
import json
import os
import struct
import sys
import urllib.request

CHUNK = 65536


def get(url):
    with urllib.request.urlopen(url, timeout=30) as r:
        return r.read()


def fnv1a(data):
    h = 1469598103934665603
    for b in data:
        h ^= b
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def list_segments(base):
    txt = get(base + "/list_segments").decode("utf-8", "replace")
    segs = []
    for line in txt.splitlines():
        # ".object1: 00010000 - 0004ef28"
        if not line.startswith(".object"):
            continue
        name, rest = line.split(":", 1)
        lo, hi = rest.split("-")
        segs.append((name.strip("."), int(lo, 16), int(hi, 16)))
    return segs


def read_memory(base, addr, length):
    out = bytearray()
    while len(out) < length:
        n = min(CHUNK, length - len(out))
        url = "%s/read_memory?address=0x%X&length=%d" % (base, addr + len(out), n)
        blob = get(url)
        try:
            obj = json.loads(blob)
            chunk = bytes(obj["data"])
        except Exception:
            sys.stderr.write("unexpected reply for 0x%X (%r)\n"
                             % (addr + len(out), blob[:80]))
            raise
        if len(chunk) != n:
            raise RuntimeError("short read: wanted %d got %d" % (n, len(chunk)))
        out += chunk
    return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default="http://127.0.0.1:8089")
    ap.add_argument("--out", default=os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build"))
    args = ap.parse_args()

    segs = list_segments(args.base)
    if not segs:
        sys.stderr.write("no .objectN segments found - is FD2.EXE open?\n")
        return 1
    os.makedirs(args.out, exist_ok=True)

    for i, (name, lo, hi) in enumerate(segs, start=1):
        size = hi - lo + 1
        data = read_memory(args.base, lo, size)
        path = os.path.join(args.out, "object%d.bin" % i)
        with open(path, "wb") as f:
            f.write(data)
        print("%-8s 0x%X..0x%X  %6d bytes  fnv1a=0x%016X  -> %s"
              % (name, lo, hi + 1, size, fnv1a(data), path))
    return 0


if __name__ == "__main__":
    sys.exit(main())
