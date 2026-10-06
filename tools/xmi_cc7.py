#!/usr/bin/env python3
"""Scan an XMID blob (as dumped by --ail-dump, ail_seq_*.bin) and report,
per MIDI channel, which controllers the sequence actually sends.

Why this matters: the original AIL applies AIL_set_sequence_volume() only to
channels that carry a stored CC7 value (FD2.EXE sub_42980 scales the outgoing
CC7 by sequence_volume/127; sub_43230 re-sends CC7 for channels whose slot is
not -1). A channel with no CC7 event never hears the sequence volume ramp.
Our host instead multiplies the whole music mix, so this scan tells us whether
the two are the same in practice.

Usage: python tools/xmi_cc7.py build/aildump/ail_seq_0.bin [more.bin ...]
"""
import sys
import struct
from collections import defaultdict


def read_vlq(buf, i):
    """Standard shift-and-or VLQ (meta / XMI note lengths)."""
    v = 0
    for _ in range(4):
        if i >= len(buf):
            break
        b = buf[i]
        i += 1
        v = (v << 7) | (b & 0x7F)
        if not (b & 0x80):
            break
    return v, i


def find_chunks(buf, pos, end, want, out):
    """Walk the IFF-style chunk tree of an XMID container."""
    while pos + 8 <= end:
        name = buf[pos:pos + 4].decode("latin1")
        size = struct.unpack(">I", buf[pos + 4:pos + 8])[0]
        body = pos + 8
        if body + size > end:
            break
        if name in ("FORM", "XMIT", "CAT "):
            out.append((name, body, body + size))
            find_chunks(buf, body, body + size, want, out)
        elif name in want:
            out.append((name, body, body + size))
        pos = body + size + (size & 1)


def data_bytes(status):
    hi = status & 0xF0
    if hi in (0xC0, 0xD0):
        return 1
    if hi in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
        return 2
    return 0


def scan(path):
    buf = open(path, "rb").read()
    chunks = []
    find_chunks(buf, 0, len(buf), {"EVNT"}, chunks)
    if not chunks:
        print("%s: no EVNT chunk" % path)
        return
    _, start, end = chunks[0]
    i, tick = start, 0
    ctrl = defaultdict(lambda: defaultdict(int))    # channel -> controller -> count
    notes = defaultdict(int)
    while i < end:
        delta = 0
        while i < end and buf[i] < 0x80:
            delta += buf[i]
            i += 1
        if i >= end:
            break
        tick += delta
        if buf[i] == 0xFF:
            if i + 1 >= end:
                break
            mtype = buf[i + 1]
            i += 2
            mlen, i = read_vlq(buf, i)
            if mtype == 0x2F:
                break
            i += mlen
            continue
        status = buf[i]
        i += 1
        n = data_bytes(status)
        if not n:
            continue
        if i + n > end:
            break
        d1, d2 = buf[i], (buf[i + 1] if n > 1 else 0)
        i += n
        ch = status & 0x0F
        if (status & 0xF0) == 0xB0:
            ctrl[ch][d1] += 1
        elif (status & 0xF0) == 0x90:
            if d2:
                notes[ch] += 1
                _, i = read_vlq(buf, i)          # note length
    print(path)
    for ch in sorted(set(list(notes) + list(ctrl))):
        c = ctrl.get(ch, {})
        cc7 = c.get(7, 0)
        print("  ch %2d: notes=%-5d CC7=%-4d %s%s"
              % (ch, notes.get(ch, 0), cc7,
                 "controllers=" + ",".join(str(k) for k in sorted(c)),
                 "" if cc7 else "   <-- NO CC7: original AIL leaves this"
                                 " channel at full volume during a"
                                 " sequence-volume fade"))


if __name__ == "__main__":
    for p in sys.argv[1:]:
        scan(p)
