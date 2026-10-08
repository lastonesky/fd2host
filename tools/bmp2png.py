#!/usr/bin/env python3
"""bmp2png.py <in.bmp> [out.png] - convert a 32bpp BI_RGB bottom-up BMP
(the format the Win32 host's --screenshot dumps) into a PNG, and print the
bounding box of the non-black pixels.

Used to compare where the Win32 host puts the Han-tang publisher logo against
the reference resource and against the DOSBox guest's own output.
"""
import struct
import sys
import zlib


def read_bmp(path):
    d = open(path, "rb").read()
    assert d[:2] == b"BM", path
    off = struct.unpack_from("<I", d, 10)[0]
    w, h = struct.unpack_from("<ii", d, 18)
    bpp = struct.unpack_from("<H", d, 28)[0]
    assert bpp == 32, bpp
    flip = h > 0
    H = abs(h)
    rows = []
    for y in range(H):
        sy = (H - 1 - y) if flip else y
        base = off + sy * w * 4
        rows.append(d[base:base + w * 4])
    return w, H, rows


def write_png(path, w, h, rows):
    raw = b"".join(b"\x00" + row for row in rows)

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    out = b"\x89PNG\r\n\x1a\n"
    out += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    out += chunk(b"IDAT", zlib.compress(raw, 9))
    out += chunk(b"IEND", b"")
    open(path, "wb").write(out)


def main():
    for path in sys.argv[1:]:
        w, h, rows = read_bmp(path)
        rgb = []
        xs, ys, n = [], [], 0
        for y in range(h):
            line = rows[y]
            out = bytearray(w * 3)
            for x in range(w):
                b, g, r, _ = line[x * 4:x * 4 + 4]
                out[x * 3] = r
                out[x * 3 + 1] = g
                out[x * 3 + 2] = b
                if r + g + b > 24:
                    xs.append(x)
                    ys.append(y)
                    n += 1
            rgb.append(bytes(out))
        out = path.rsplit(".", 1)[0] + ".png"
        write_png(out, w, h, rgb)
        if xs:
            print("%-24s %dx%d bbox x[%3d..%3d] y[%3d..%3d] cx=%6.1f "
                  "cy=%6.1f ink=%d -> %s"
                  % (path, w, h, min(xs), max(xs), min(ys), max(ys),
                     (min(xs) + max(xs)) / 2.0, (min(ys) + max(ys)) / 2.0, n, out))
        else:
            print("%-24s %dx%d EMPTY -> %s" % (path, w, h, out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
