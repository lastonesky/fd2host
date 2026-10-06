#!/usr/bin/env python3
"""fixup_dump.py - walk FD2's LE fixup tables with the same grammar le.c uses
and report the records near a page boundary.

    python tools/fixup_dump.py            # every record that le.c skips
    python tools/fixup_dump.py 0x11001    # records around one address

Used to answer "why does our image differ from Ghidra's at this byte"
(docs/rounds/13-portability.md / rounds/14).
"""
import struct
import sys

PAGE = 0x1000


def rd16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def rd32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def find_le(data):
    i = 0
    while i + 0x80 < len(data):
        if data[i:i + 4] == b"LE\x00\x00":
            pages = rd32(data, i + 0x14)
            objtab = rd32(data, i + 0x40)
            if 0 < pages <= 4096 and 0x40 <= objtab <= 0x1000:
                return i
        i += 1
    return -1


def main():
    exe = sys.argv[1] if len(sys.argv) > 1 else r"E:\FD2\FD2.EXE"
    want = int(sys.argv[2], 0) if len(sys.argv) > 2 else None

    data = open(exe, "rb").read()
    h = find_le(data)
    if h < 0:
        print("no LE header")
        return 1

    objtab = rd32(data, h + 0x40)
    nobj = rd32(data, h + 0x44)
    # FD2 object entry: 5 dwords (vsize, base, flags, page_index, page_count)
    objs = []
    for k in range(nobj):
        e = h + objtab + k * 24
        objs.append(dict(vsize=rd32(data, e), base=rd32(data, e + 4),
                         flags=rd32(data, e + 8), pidx=rd32(data, e + 12),
                         pcnt=rd32(data, e + 16)))
    npages = rd32(data, h + 0x14)
    ptf = rd32(data, h + 0x68)      # same fields le.c reads
    rtf = rd32(data, h + 0x6C)
    pt, rt = h + ptf, h + rtf
    print("LE @0x%X  pages=%d  objects=%d" % (h, npages, nobj))

    rows = []
    for page in range(1, npages + 1):
        begin, end = rd32(data, pt + (page - 1) * 4), rd32(data, pt + page * 4)
        if end <= begin or end > len(data):      # same guard as le.c
            continue
        base = None
        for o in objs:
            if o["pidx"] <= page < o["pidx"] + o["pcnt"]:
                base = o["base"] + (page - o["pidx"]) * PAGE
                break
        if base is None:
            continue
        pos = begin
        while pos < end:
            t = rt + pos
            if t >= len(data):
                print("page %d: begin=0x%X end=0x%X rt=0x%X -> record at "
                      "0x%X past EOF (le.c guards end > filesize, this is "
                      "an offset into the record table)"
                      % (page, begin, end, rt, t))
                break
            typ = data[t]
            if typ == 0x00:
                pos += 1
                continue
            if typ == 0x02:
                srcoff = rd16(data, t + 2)
                rows.append((page, base, "02", srcoff, 1, None, None, pos, end))
                pos += 5
                continue
            if typ != 0x07:
                rows.append((page, base, "%02X" % typ, None, None, None, None,
                             pos, end))
                break
            b1 = data[t + 1]
            tsize = 2 + (b1 >> 4)
            srcoff = rd16(data, t + 2)
            tobj = data[t + 4]
            tgtoff = 0
            for i in range(tsize):
                tgtoff |= data[t + 5 + i] << (8 * i)
            rows.append((page, base, "07", srcoff, tsize, tobj, tgtoff, pos, end))
            pos += 5 + tsize

    def near(r):
        _, base, typ, srcoff = r[0], r[1], r[2], r[3]
        if srcoff is None:
            return True
        if want is not None:                     # around one address
            abs_addr = base + srcoff
            return abs(want - abs_addr) <= 8
        return srcoff + (4 if typ == "07" else 2) > PAGE   # what le.c skips

    print("%-6s %-10s %-4s %-8s %-5s %-4s %-10s %s"
          % ("page", "page_base", "typ", "src", "size", "obj", "tgt", "abs"))
    for r in rows:
        page, base, typ, srcoff, tsize, tobj, tgtoff, pos, end = r
        if not near(r):
            continue
        if typ == "07" and tobj:
            tgt = objs[tobj - 1]["base"] + tgtoff
        else:
            tgt = tgtoff
        print("%-6d 0x%08X %-4s +0x%-6X %-5d %-4s 0x%-8s %s"
              % (page, base, typ, srcoff if srcoff is not None else 0,
                 tsize or 0, tobj if tobj else "-",
                 ("%X" % tgt) if tgt is not None else "-",
                 ("0x%X" % (base + srcoff)) if srcoff is not None else "-"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
