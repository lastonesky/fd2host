#!/usr/bin/env python3
"""preflight.py - static "can the host load this?" check for a DOS/4GW (LE) exe.

Runs nothing: parses the container, prints the object layout, checks it against
the address ranges the host reserves unconditionally (PROGRESS.md §4.1) and
looks for the Miles AIL trace strings that decide whether ail_install()'s
hard-coded patch addresses would be safe.

    python preflight.py <path-to-exe>
"""
import sys
import struct

# fixed ranges the host reserves no matter which game is loaded (src/le.c,
# src/dos.c) - an object landing on top of one of these is a conflict
RESERVED = [
    (0x00010000, 0x00070000, "obj region (le.c FD2_OBJ_REGION)"),
    (0x00070000, 0x00080000, "low-memory mirror (BDA/PSP/IVT)"),
    (0x00080000, 0x000A0000, "real-mode pool (INT31 0100)"),
    (0x000A0000, 0x000C0000, "VGA frame buffer"),
    (0x000C0000, 0x00100000, "ROM area (committed writable)"),
]


def find_le(data):
    """Locate the LE header: "LE\\0\\0" + self-consistent object table."""
    for off in range(0, len(data) - 0x100):
        if data[off:off + 4] != b"LE\x00\x00":
            continue
        obj_off = struct.unpack_from("<I", data, off + 0x40)[0]
        obj_cnt = struct.unpack_from("<I", data, off + 0x44)[0]
        if 1 <= obj_cnt <= 64 and obj_off + obj_cnt * 24 <= len(data) - off:
            return off
    return None


def main(path):
    with open(path, "rb") as f:
        data = f.read()

    print("file      : %s" % path)
    print("size      : %d bytes" % len(data))
    print("mz stub   : %r" % data[0:2])

    off = find_le(data)
    if off is None:
        print("LE header : NOT FOUND -> not a DOS/4GW (LE) image; the host "
              "cannot load it (Causeway/DJGPP/PE are not supported)")
        return 1
    print("LE header : @0x%X" % off)

    cpu_os = struct.unpack_from("<I", data, off + 0x08)[0]
    pages = struct.unpack_from("<I", data, off + 0x14)[0]
    eip_obj = struct.unpack_from("<I", data, off + 0x18)[0]
    eip = struct.unpack_from("<I", data, off + 0x1C)[0]
    obj_off = struct.unpack_from("<I", data, off + 0x40)[0]
    obj_cnt = struct.unpack_from("<I", data, off + 0x44)[0]
    print("cpu/os    : 0x%08X (cpu=%d os=%d)" % (cpu_os, cpu_os & 0xFF,
                                                 (cpu_os >> 8) & 0xFF))
    print("module    : %d pages, entry = object %d + 0x%X" % (pages, eip_obj, eip))

    objs = []
    for i in range(obj_cnt):
        e = off + obj_off + i * 24
        vsize, base, flags, pageidx, npages = struct.unpack_from("<IIIII", data, e)
        objs.append((i, base, vsize, flags, pageidx, npages))
    print("objects   : %d" % obj_cnt)
    print("  #   base       vsize      pages   flags")
    entry = None
    for (i, base, vsize, flags, pageidx, npages) in objs:
        print("  %d   0x%08X 0x%08X %3d..%-3d 0x%08X"
              % (i, base, vsize, pageidx, pageidx + npages - 1, flags))
        if i + 1 == eip_obj:
            entry = base + eip
    if entry is not None:
        print("entry     : 0x%08X" % entry)

    print("\n-- conflicts with the host's fixed reservations --")
    bad = 0
    for (i, base, vsize, flags, pageidx, npages) in objs:
        end = base + max(vsize, npages * 0x1000)
        for (lo, hi, name) in RESERVED:
            if base < hi and end > lo:
                # the low object region is what the host *expects* the game to
                # use; anything else is a genuine collision
                if name.startswith("obj region"):
                    continue
                print("  CONFLICT obj%d [0x%08X..0x%08X) overlaps %s"
                      % (i, base, end, name))
                bad += 1
        if base >= 0x00100000:
            print("  NOTE obj%d starts at 0x%08X (>=1 MiB): the early "
                  "reservation does not cover it, so the CRT heap may steal it"
                  % (i, base))
    if bad == 0:
        print("  none")

    print("\n-- host-side hard-coded assumptions --")
    ail = [s for s in (b"AIL_startup", b"AIL_shutdown", b"AIL_install_DIG_INI",
                       b"AIL_set_preference") if s in data]
    if ail:
        print("  AIL     : FOUND %s" % ", ".join(x.decode() for x in ail))
        print("            -> the host would patch 52 hard-coded addresses "
              "from FD2's layout (src/ail.c); for a different build this "
              "writes jmps into unknown code - must be verified/gated first")
    else:
        print("  AIL     : no trace strings -> host AIL patch should be skipped")
    ext = [s for s in (b"DOS/4GW", b"RATIONAL DOS/4G", b"WATCOM", b"Phar Lap")
           if s in data]
    print("  extender: %s" % (", ".join(x.decode() for x in ext) or "unknown"))
    mode13 = b"\x13"  # nothing conclusive statically; INT10 is checked at runtime
    print("  mode    : determined at runtime from INT 10h AH=0 "
          "(host currently assumes 320x200 - src/host.c)")
    return 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
