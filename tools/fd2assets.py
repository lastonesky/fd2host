#!/usr/bin/env python3
"""fd2assets.py - export Flame Dragon 2 assets to modern formats.

The formats are the ones documented (and visually verified) in
docs/knowledge-base/01-container-and-asset-formats.md,
05-image-compression-format.md and 07-music-xmidi-format.md:

  container   "LLLLLL" magic, u32 LE offset directory from +6, resources are
              [offsets[i], offsets[i+1]); N = (offsets[0]-6)/4
  palette     FDOTHER.DAT resource 0 is a 768-byte 6-bit VGA palette
  full image  u16 LE width/height at +0; raw if len-4 == w*h, otherwise RLE:
                  c >= 0x80 -> (c & 0x7F) + 1 literal bytes
                  c <  0x80 -> next byte repeated c + 1 times
  music       FDMUS.DAT resources are XMIDI (IFF); converted to standard MIDI

Usage:
    python tools/fd2assets.py list   <file.dat>
    python tools/fd2assets.py unpack <file.dat> [--out DIR] [--recurse]
    python tools/fd2assets.py image  <file.dat> <index> [--out out.png]
    python tools/fd2assets.py music  <FDMUS.DAT> [--out DIR]
    python tools/fd2assets.py all    <gamedir> [--out DIR]      # everything it knows

Only formats that are verified are written; anything it is not sure about is
reported as "skip (<reason>)" rather than silently emitted wrong.
"""
import argparse
import json
import os
import struct
import sys
import zlib

MAGIC = b"LLLLLL"


# ----------------------------------------------------------------- container
def read_container(data, base=0):
    """Return (resources, header_size) where resources[i] = (offset, size)."""
    if data[base:base + 6] != MAGIC:
        raise ValueError("not an LLLLLL container")
    first = struct.unpack_from("<I", data, base + 6)[0]
    n = (first - 6) // 4
    offs = [struct.unpack_from("<I", data, base + 6 + 4 * i)[0] for i in range(n)]
    out = []
    for i in range(n):
        start = base + offs[i]
        end = base + (offs[i + 1] if i + 1 < n else len(data) - base)
        out.append((start, end - start))
    return out, first


def is_container(b):
    return b[:6] == MAGIC


# ------------------------------------------------------------------- palette
def load_palette(gamedir):
    """FDOTHER.DAT resource 0: 256 * RGB, 6-bit per channel -> 8-bit."""
    with open(os.path.join(gamedir, "FDOTHER.DAT"), "rb") as f:
        data = f.read()
    res, _ = read_container(data)
    off, size = res[0]
    pal = data[off:off + size]
    if len(pal) < 768:
        raise ValueError("palette resource is %d bytes" % len(pal))
    return bytes(min(255, v * 4) for v in pal[:768])


# --------------------------------------------------------------------- image
def _rle_doc(src, need):
    """The knowledge-base "§2" byte RLE: c>=0x80 -> (c&0x7F)+1 literals,
    c<0x80 -> next byte repeated c+1 times. Stops at `need` pixels."""
    out = bytearray()
    i = 0
    while len(out) < need and i < len(src):
        c = src[i]; i += 1
        if c >= 0x80:
            n = (c & 0x7F) + 1
            out += src[i:i + n]; i += n
        else:
            if i >= len(src):
                break
            out += bytes([src[i]]) * (c + 1); i += 1
    return bytes(out)


def decode_image(body):
    """Return (w, h, bytes) or raise ValueError if it is not a full image."""
    if len(body) < 4:
        raise ValueError("too short")
    w, h = struct.unpack_from("<HH", body, 0)
    if w == 0 or h == 0 or w > 4096 or h > 4096:
        raise ValueError("bad dimensions %dx%d" % (w, h))
    need = w * h
    if len(body) - 4 == need:                      # uncompressed
        return w, h, body[4:4 + need]
    px = _rle_doc(body[4:], need)
    if len(px) != need:
        raise ValueError("RLE produced %d of %d pixels" % (len(px), need))
    return w, h, px


def decode_sprite24(body, cols, rows):
    """The 24x24 "four-mode" sprite RLE - the same token scheme the game's
    sprite24.c / rle_decode() implement (tok>>6: 0 solid run, 1 odd run,
    2 literal, 3 special). Colour mapping is identity here; the game may blit
    a tileset through a ramp/palette mode to recolour terrain, which the
    exporter deliberately does not guess. Returns exactly cols*rows bytes.
    Raises ValueError if the stream does not fill the block exactly."""
    out = bytearray()
    i = 0
    src = body
    for _r in range(rows):
        left = cols
        while left != 0:
            if i >= len(src):
                raise ValueError("sprite24 stream ran out")
            tok = src[i]; i += 1
            cnt = (tok & 63) + 1
            m = tok >> 6
            if m == 0:                       # solid run
                if i >= len(src):
                    raise ValueError("sprite24 run without colour")
                out += bytes([src[i]]) * cnt; i += 1; left -= cnt
            elif m == 1:                     # odd run: writes p[1] only
                if i >= len(src):
                    raise ValueError("sprite24 odd run without colour")
                v = src[i]; i += 1
                for _ in range(cnt):
                    out += bytes([0, v])     # p[0] left untouched
                left -= 2 * cnt
            elif m == 2:                     # literal
                if i + cnt > len(src):
                    raise ValueError("sprite24 literal overruns")
                out += src[i:i + cnt]; i += cnt; left -= cnt
            else:                            # special: skipped pixels
                out += bytes(cnt); left -= cnt
    if len(out) != cols * rows:
        raise ValueError("sprite24 produced %d of %d pixels" % (len(out), cols * rows))
    return bytes(out), i


def decode_dato_frame(body):
    """DATO portrait frame: u16 W, u16 H, then the portrait RLE
    (b <= 0xC0 -> one literal pixel of value b; b > 0xC0 -> next byte repeated
    b-0xC0 times). Returns (w, h, bytes)."""
    if len(body) < 4:
        raise ValueError("too short")
    w, h = struct.unpack_from("<HH", body, 0)
    need = w * h
    src = body[4:]
    out = bytearray()
    i = 0
    while len(out) < need and i < len(src):
        b = src[i]; i += 1
        if b <= 0xC0:
            out.append(b)
        else:
            if i >= len(src):
                break
            out += bytes([src[i]]) * (b - 0xC0); i += 1
    if len(out) != need:
        raise ValueError("portrait RLE produced %d of %d pixels" % (len(out), need))
    return w, h, bytes(out)


def write_sheet(path, tiles, tw, th, cols, palette, indices=None):
    """Lay `tiles` (list of bytes, each tw*th) out in a `cols`-wide grid.
    `indices` optionally selects which tiles to place (default: all)."""
    sel = list(range(len(tiles))) if indices is None else indices
    rows = (len(sel) + cols - 1) // cols
    sw, sh = cols * tw, rows * th
    sheet = bytearray(sw * sh)
    for n, t in enumerate(sel):
        gx, gy = (n % cols) * tw, (n // cols) * th
        for y in range(th):
            sheet[(gy + y) * sw + gx:(gy + y) * sw + gx + tw] = \
                tiles[t][y * tw:(y + 1) * tw]
    write_png(path, sw, sh, bytes(sheet), palette)
    return sw, sh

def write_png(path, w, h, indices, palette):
    """8-bit palette PNG (color type 3)."""
    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload +
                struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))
    raw = bytearray()
    for y in range(h):
        raw.append(0)                              # filter: none
        raw += indices[y * w:(y + 1) * w]
    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0)) +
           chunk(b"PLTE", palette) +
           chunk(b"IDAT", zlib.compress(bytes(raw), 9)) +
           chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


# --------------------------------------------------------------------- music
def _vlq(n):
    """Standard MIDI variable-length quantity."""
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append((n & 0x7F) | 0x80)
        n >>= 7
    return bytes(reversed(out))


def _read_vlq(b, i):
    v = 0
    while i < len(b):
        c = b[i]; i += 1
        v = (v << 7) | (c & 0x7F)
        if not (c & 0x80):
            break
    return v, i


def _find_chunks(b, start, end):
    """Yield (id, payload_start, payload_len) for IFF chunks in [start,end)."""
    i = start
    while i + 8 <= end:
        cid = b[i:i + 4]
        ln = struct.unpack_from(">I", b, i + 4)[0]
        yield cid, i + 8, ln
        i += 8 + ln + (ln & 1)


def _xmidi_forms(body):
    """Return the list of (timb, evnt) for every FORM XMID inside an XMI blob."""
    forms = []
    # FORM <len> XDIR ...  /  CAT <len> XMID ...
    def walk(start, end):
        for cid, ps, ln in _find_chunks(body, start, end):
            if cid == b"FORM" and ln >= 4:
                typ = body[ps:ps + 4]
                if typ == b"XMID":
                    timb = evnt = None
                    for c2, p2, l2 in _find_chunks(body, ps + 4, ps + ln):
                        if c2 == b"TIMB":
                            timb = body[p2:p2 + l2]
                        elif c2 == b"EVNT":
                            evnt = body[p2:p2 + l2]
                    if evnt is not None:
                        forms.append((timb, evnt))
                elif typ == b"XDIR":
                    walk(ps + 4, ps + ln)
            elif cid == b"CAT " and ln >= 4:
                walk(ps + 4, ps + ln)
    walk(0, len(body))
    if not forms:                                  # a bare FORM XMID?
        for cid, ps, ln in _find_chunks(body, 0, len(body)):
            if cid == b"FORM":
                typ = body[ps:ps + 4]
                if typ == b"XMID":
                    timb = evnt = None
                    for c2, p2, l2 in _find_chunks(body, ps + 4, ps + ln):
                        if c2 == b"TIMB":
                            timb = body[p2:p2 + l2]
                        elif c2 == b"EVNT":
                            evnt = body[p2:p2 + l2]
                    if evnt is not None:
                        forms.append((timb, evnt))
    return forms


def xmi_to_mid(body, track_name="FD2"):
    """Convert one XMI blob to standard MIDI bytes (one track, or merged)."""
    forms = _xmidi_forms(body)
    if not forms:
        raise ValueError("no FORM XMID/EVNT found")
    tracks = []
    for timb, evnt in forms:
        ev = []                                    # (tick, order, bytes)
        i = 0
        tick = 0
        running = 0
        ended = False
        while i < len(evnt) and not ended:
            delta = 0
            while i < len(evnt) and evnt[i] < 0x80:   # XMIDI: explicit sum
                delta += evnt[i]; i += 1
            tick += delta
            if i >= len(evnt):
                break
            status = evnt[i]
            if status & 0x80:
                i += 1
                running = status
            else:
                status = running
            if status == 0xFF:                     # meta
                mtype = evnt[i]; i += 1
                ln, i = _read_vlq(evnt, i)
                payload = evnt[i:i + ln]; i += ln
                ev.append((tick, 2, bytes([0xFF, mtype]) + _vlq(ln) + payload))
                if mtype == 0x2F:
                    ended = True
                continue
            hi = status & 0xF0
            if hi in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
                d1 = evnt[i]; i += 1
                if hi == 0x90:
                    dur, i = _read_vlq(evnt, i)     # XMIDI note length
                else:
                    dur = 0
                d2 = evnt[i]; i += 1
                ev.append((tick, 1, bytes([status, d1, d2])))
                if hi == 0x90 and d2 != 0:
                    ev.append((tick + dur, 3, bytes([0x80 | (status & 0x0F), d1, 0])))
            elif hi in (0xC0, 0xD0):
                d1 = evnt[i]; i += 1
                ev.append((tick, 1, bytes([status, d1])))
            else:
                break                               # unknown -> stop here
        ev.sort(key=lambda e: (e[0], e[1]))
        trk = bytearray()
        prev = 0
        for t, _order, payload in ev:
            trk += _vlq(max(0, t - prev)) + payload
            prev = t
        trk += _vlq(0) + b"\xff\x2f\x00"
        tracks.append(bytes(trk))

    hdr = b"MThd" + struct.pack(">IHHH", 6, 1, len(tracks), 120)
    out = bytearray(hdr)
    for trk in tracks:
        out += b"MTrk" + struct.pack(">I", len(trk)) + trk
    return bytes(out)


# ---------------------------------------------------------------------- CLI
def cmd_list(args):
    data = open(args.file, "rb").read()
    res, hsize = read_container(data)
    print("%s: %d resources, header %d bytes, file %d bytes"
          % (os.path.basename(args.file), len(res), hsize, len(data)))
    for i, (off, size) in enumerate(res):
        body = data[off:off + 8]
        kind = "container" if is_container(data[off:off + 6]) else \
               "image" if size >= 4 and 0 < struct.unpack_from("<H", data, off)[0] <= 4096 else \
               "xmi" if body[4:8] == b"FORM" or body[:4] == b"FORM" else \
               "?"
        print("  [%3d] off=%7d size=%7d %s %s" % (i, off - res[0][0], size, kind, body.hex()))
    return 0


def _unpack_one(data, res, outdir, recurse, depth=0):
    os.makedirs(outdir, exist_ok=True)
    manifest = []
    for i, (off, size) in enumerate(res):
        body = data[off:off + size]
        name = "%03d" % i
        if recurse and is_container(body):
            sub = os.path.join(outdir, name)
            try:
                subres, _ = read_container(body)
                _unpack_one(body, subres, sub, True, depth + 1)
                manifest.append({"index": i, "size": size, "kind": "container",
                                 "path": os.path.relpath(sub, outdir)})
            except ValueError as e:
                manifest.append({"index": i, "size": size, "kind": "error:%s" % e})
            continue
        with open(os.path.join(outdir, name + ".bin"), "wb") as f:
            f.write(body)
        manifest.append({"index": i, "size": size, "kind": "raw"})
    with open(os.path.join(outdir, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=1)
    return manifest


def cmd_unpack(args):
    data = open(args.file, "rb").read()
    res, _ = read_container(data)
    out = args.out or os.path.join("build", "assets",
                                   os.path.basename(args.file) + ".d")
    _unpack_one(data, res, out, args.recurse)
    print("unpacked %d resources -> %s" % (len(res), out))
    return 0


def cmd_image(args):
    data = open(args.file, "rb").read()
    res, _ = read_container(data)
    if not (0 <= args.index < len(res)):
        print("index out of range (0..%d)" % (len(res) - 1)); return 1
    off, size = res[args.index]
    try:
        w, h, px = decode_image(data[off:off + size])
    except ValueError as e:
        print("skip %s[%d]: %s" % (args.file, args.index, e)); return 1
    pal = load_palette(args.gamedir)
    out = args.out or "build/assets/%s_%03d.png" % (
        os.path.splitext(os.path.basename(args.file))[0], args.index)
    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    write_png(out, w, h, px, pal)
    print("wrote %s (%dx%d)" % (out, w, h))
    return 0


def cmd_dato(args):
    data = open(args.file, "rb").read()
    res, _ = read_container(data)
    pal = load_palette(args.gamedir)
    out = args.out or os.path.join("build", "assets", "portraits")
    os.makedirs(out, exist_ok=True)
    index, n = [], 0
    for i, (off, size) in enumerate(res):
        body = data[off:off + size]
        if size < 16:
            continue
        offs = struct.unpack_from("<IIII", body, 0)
        if not (16 <= offs[0] < offs[1] < offs[2] < offs[3] < size):
            continue
        frames = []
        for k in range(4):
            s = offs[k]
            e = offs[k + 1] if k < 3 else size
            try:
                w, h, px = decode_dato_frame(body[s:e])
            except ValueError as ex:
                frames.append({"frame": k, "error": str(ex)})
                continue
            f = "DATO_%03d_%d.png" % (i, k)
            write_png(os.path.join(out, f), w, h, px, pal)
            frames.append({"frame": k, "w": w, "h": h, "file": f})
            n += 1
        index.append({"index": i, "size": size, "frames": frames})
    with open(os.path.join(out, "portraits.json"), "w", encoding="utf-8") as f:
        json.dump(index, f, indent=1)
    print("dato: %d frames from %d portraits -> %s" % (n, len(res), out))
    return 0


def cmd_tileset(args):
    data = open(args.file, "rb").read()
    res, _ = read_container(data)
    pal = load_palette(args.gamedir)
    out = args.out or os.path.join("build", "assets", "tilesets")
    os.makedirs(out, exist_ok=True)
    sets = terrains = 0
    for i, (off, size) in enumerate(res):
        body = data[off:off + size]
        if i % 2 == 0:                             # tileset
            if size < 6:
                continue
            tw, th, cnt = struct.unpack_from("<HHH", body, 0)
            if not (0 < tw <= 64 and 0 < th <= 64 and 0 < cnt <= 8192):
                continue
            if 6 + 4 * cnt > size:
                continue
            toffs = [struct.unpack_from("<I", body, 6 + 4 * k)[0] for k in range(cnt)]
            tiles, ok = [], True
            for k in range(cnt):
                s = toffs[k]
                e = toffs[k + 1] if k + 1 < cnt else size
                if s >= e or e > size:
                    ok = False; break
                try:
                    px, used = decode_sprite24(body[s:e], tw, th)
                except ValueError:
                    ok = False; break
                if used != e - s:
                    ok = False; break
                tiles.append(px)
            if not ok:
                print("skip FDSHAP[%d]: tile decode" % i)
                continue
            cols = 16
            p = os.path.join(out, "FDSHAP_%03d.png" % i)
            sw, sh = write_sheet(p, tiles, tw, th, cols, pal)
            with open(os.path.join(out, "FDSHAP_%03d.json" % i), "w",
                      encoding="utf-8") as f:
                json.dump({"tile_w": tw, "tile_h": th, "count": cnt,
                           "cols": cols, "sheet_w": sw, "sheet_h": sh}, f, indent=1)
            sets += 1
        else:                                      # terrain control table
            cells = [list(body[4 * k:4 * k + 4]) for k in range(size // 4)]
            with open(os.path.join(out, "FDSHAP_%03d_terrain.json" % i), "w",
                      encoding="utf-8") as f:
                json.dump(cells, f)
            terrains += 1
    print("tileset: %d tilesets, %d terrain tables -> %s" % (sets, terrains, out))
    return 0


def cmd_music(args):
    data = open(args.file, "rb").read()
    res, _ = read_container(data)
    out = args.out or "build/assets/music"
    os.makedirs(out, exist_ok=True)
    n = 0
    for i, (off, size) in enumerate(res):
        body = data[off:off + size]
        try:
            mid = xmi_to_mid(body)
        except ValueError:
            continue
        path = os.path.join(out, "%03d.mid" % i)
        with open(path, "wb") as f:
            f.write(mid)
        n += 1
        print("wrote %s (%d bytes, xmi %d)" % (path, len(mid), size))
    print("music: %d tracks" % n)
    return 0


def cmd_all(args):
    gamedir = args.gamedir
    out = args.out or os.path.join("build", "assets")
    files = sorted(f for f in os.listdir(gamedir) if f.upper().endswith(".DAT"))
    total_img = 0
    for fn in files:
        path = os.path.join(gamedir, fn)
        data = open(path, "rb").read()
        if data[:6] != MAGIC:
            continue
        res, _ = read_container(data)
        _unpack_one(data, res, os.path.join(out, fn + ".d"), True)
        print("%s: %d resources unpacked" % (fn, len(res)))
        stem = os.path.splitext(fn)[0]
        if stem in ("BG", "FDOTHER", "TITLE", "TAI"):
            for i, (off, size) in enumerate(res):
                try:
                    w, h, px = decode_image(data[off:off + size])
                except ValueError:
                    continue
                p = os.path.join(out, "images", "%s_%03d.png" % (stem, i))
                os.makedirs(os.path.dirname(p), exist_ok=True)
                write_png(p, w, h, px, load_palette(gamedir))
                total_img += 1
        if stem == "FDMUS":
            for i, (off, size) in enumerate(res):
                try:
                    mid = xmi_to_mid(data[off:off + size])
                except ValueError:
                    continue
                p = os.path.join(out, "music", "%03d.mid" % i)
                os.makedirs(os.path.dirname(p), exist_ok=True)
                with open(p, "wb") as f:
                    f.write(mid)
        if stem == "DATO":
            class _A: pass
            a = _A(); a.file = path; a.gamedir = gamedir
            a.out = os.path.join(out, "portraits")
            cmd_dato(a)
        if stem == "FDSHAP":
            class _A2: pass
            a2 = _A2(); a2.file = path; a2.gamedir = gamedir
            a2.out = os.path.join(out, "tilesets")
            cmd_tileset(a2)
    print("all: %d images exported -> %s" % (total_img, out))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("list");   p.add_argument("file"); p.set_defaults(fn=cmd_list)
    p = sub.add_parser("unpack"); p.add_argument("file"); p.add_argument("--out")
    p.add_argument("--recurse", action="store_true"); p.set_defaults(fn=cmd_unpack)
    p = sub.add_parser("image");  p.add_argument("file"); p.add_argument("index", type=int)
    p.add_argument("--out"); p.add_argument("--gamedir", default="E:\\FD2")
    p.set_defaults(fn=cmd_image)
    p = sub.add_parser("music");  p.add_argument("file"); p.add_argument("--out")
    p.set_defaults(fn=cmd_music)
    p = sub.add_parser("dato");   p.add_argument("file"); p.add_argument("--out")
    p.add_argument("--gamedir", default="E:\\FD2"); p.set_defaults(fn=cmd_dato)
    p = sub.add_parser("tileset"); p.add_argument("file"); p.add_argument("--out")
    p.add_argument("--gamedir", default="E:\\FD2"); p.set_defaults(fn=cmd_tileset)
    p = sub.add_parser("all");    p.add_argument("gamedir"); p.add_argument("--out")
    p.set_defaults(fn=cmd_all)

    args = ap.parse_args()
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
