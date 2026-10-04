#!/usr/bin/env python3
"""fixup_scan.py - replay le.c's fixup grammar and show where it breaks down.

Mirrors apply_fixups() in src/le.c exactly (type 0x00 filler, type 0x07
records, cross-page skip) and reports the first record it cannot parse per
page, so we can see which fixup type a new title uses that FD2 never did.

    python fixup_scan.py <exe>
"""
import sys
import struct


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def find_le(data):
    for off in range(0, len(data) - 0x100):
        if data[off:off + 4] != b"LE\x00\x00":
            continue
        obj_off = u32(data, off + 0x40)
        obj_cnt = u32(data, off + 0x44)
        if 1 <= obj_cnt <= 64 and obj_off + obj_cnt * 24 <= len(data) - off:
            return off
    return None


def main(path):
    data = open(path, "rb").read()
    h = find_le(data)
    if h is None:
        print("no LE header")
        return 1

    pages = u32(data, h + 0x14)
    obj_off = u32(data, h + 0x40)
    obj_cnt = u32(data, h + 0x44)
    fix_pt = u32(data, h + 0x68)
    fix_rt = u32(data, h + 0x6C)

    objs = []
    for i in range(obj_cnt):
        e = h + obj_off + i * 24
        vsize, base, flags, pageidx, npages = struct.unpack_from("<IIIII", data, e)
        objs.append(dict(base=base, page_index=pageidx, page_count=npages,
                         vsize=vsize))

    pt = h + fix_pt
    rt = h + fix_rt
    print("pages=%d objects=%d fixup_pt@+0x%X fixup_rt@+0x%X"
          % (pages, obj_cnt, fix_pt, fix_rt))

    applied = skipped = bad = leftover = 0
    for page in range(1, pages + 1):
        begin = u32(data, pt + (page - 1) * 4)
        end = u32(data, pt + page * 4)
        pos = begin
        if end <= begin or end > len(data):
            continue
        o = None
        for c in objs:
            if c["page_index"] <= page < c["page_index"] + c["page_count"]:
                o = c
                break
        if o is None:
            continue
        while pos < end:
            t = data[rt + pos]
            if t == 0x00:
                pos += 1
                continue
            if t == 0x02:
                # [02][flags][src:2][obj:1] - target is the object base, no
                # target-offset field (FDPS page 71, one record)
                src = u16(data, rt + pos + 2)
                tobj = data[rt + pos + 4]
                if tobj == 0 or tobj > obj_cnt:
                    bad += 1
                    print("!! page %d: bad object %d in 0x02 record" % (page, tobj))
                    break
                if src + 4 > 0x1000:
                    skipped += 1
                else:
                    applied += 1
                    print("   type 0x02 -> write obj%d base 0x%X at 0x%X+0x%X"
                          % (tobj, objs[tobj-1]['base'], 0x10000, src))
                pos += 5
                continue
            if t != 0x07:
                bad += 1
                print("\n!! page %d: unknown fixup type 0x%02X at record "
                      "offset %d (record file offset 0x%X)"
                      % (page, t, pos, rt + pos))
                print("   bytes: %s"
                      % " ".join("%02X" % x for x in data[rt + pos:rt + pos + 24]))
                # decode the common layouts for comparison
                if pos + 4 < end:
                    src = u16(data, rt + pos + 2)
                    print("   if read as 0x07-like: src=0x%04X obj=%d"
                          % (src, data[rt + pos + 4] if pos + 4 < end else -1))
                break
            b1 = data[rt + pos + 1]
            tsize = 2 + (b1 >> 4)
            if pos + 5 + tsize > end:
                bad += 1
                print("\n!! page %d: truncated record at offset %d "
                      "(size nibble gives tsize=%d, only %d bytes left)"
                      % (page, pos, tsize, end - pos))
                break
            src = u16(data, rt + pos + 2)
            tobj = data[rt + pos + 4]
            tgtoff = 0
            for i in range(tsize):
                tgtoff |= data[rt + pos + 5 + i] << (8 * i)
            if tobj == 0 or tobj > obj_cnt:
                bad += 1
                print("\n!! page %d: bad target object %d at offset %d"
                      % (page, tobj, pos))
                break
            if src + 4 > 0x1000:
                skipped += 1
            else:
                applied += 1
            pos += 5 + tsize
        if pos != end:
            leftover += 1
            print("   (page %d stops at %d, expected %d -> %d record bytes "
                  "never parsed)" % (page, pos, end, end - pos))
    print("\napplied=%d cross_page_skipped=%d leftover_pages=%d bad=%d"
          % (applied, skipped, leftover, bad))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
