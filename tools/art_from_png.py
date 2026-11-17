#!/usr/bin/env python3
"""Convert the boot splash reference PNG into VGA text-mode cell art.

Each output cell holds two vertical half-blocks (upper = bg colour, lower =
fg colour), so a 6-row banner reproduces a 12-band gradient exactly like the
reference image.

Usage: python3 tools/art_from_png.py build/splash_ref.png [cols] [rows]
"""
import struct
import sys
import zlib

CW, CH = 8, 16   # VGA text cell

VGA = [
    (0, 0, 0), (0, 0, 170), (0, 170, 0), (0, 170, 170),
    (170, 0, 0), (170, 0, 170), (170, 85, 0), (170, 170, 170),
    (85, 85, 85), (85, 85, 255), (85, 255, 85), (85, 255, 255),
    (255, 85, 85), (255, 85, 255), (255, 255, 85), (255, 255, 255),
]


def read_png(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', 'not a PNG'
    pos = 8
    idat = b''
    w = h = depth = ctype = None
    palette = None
    while pos < len(data):
        ln = struct.unpack('>I', data[pos:pos + 4])[0]
        tag = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + ln]
        pos += 12 + ln
        if tag == b'IHDR':
            w, h, depth, ctype, _, _, interlace = struct.unpack('>IIBBBBB', body)
            assert depth == 8 and interlace == 0, 'only 8-bit non-interlaced'
        elif tag == b'PLTE':
            palette = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif tag == b'IDAT':
            idat += body
        elif tag == b'IEND':
            break
    raw = zlib.decompress(idat)
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    stride = w * channels
    rows = []
    prev = bytearray(stride)
    p = 0
    for _ in range(h):
        f = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        if f == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 0xFF
        elif f == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif f == 3:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif f == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = prev[i]
                c = prev[i - channels] if i >= channels else 0
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        rows.append(bytes(line))
        prev = line
    # normalise to RGB rows
    out = []
    for line in rows:
        px = []
        for x in range(w):
            if ctype == 2:
                px.append((line[x * 3], line[x * 3 + 1], line[x * 3 + 2]))
            elif ctype == 6:
                px.append((line[x * 4], line[x * 4 + 1], line[x * 4 + 2]))
            elif ctype == 3:
                px.append(palette[line[x]])
            elif ctype == 0:
                v = line[x]
                px.append((v, v, v))
            else:
                v = line[x * 2]
                px.append((v, v, v))
        out.append(px)
    return w, h, out


def nearest(rgb):
    r, g, b = rgb
    best, bd = 0, 1 << 30
    for i, (pr, pg, pb) in enumerate(VGA):
        d = (r - pr) ** 2 * 3 + (g - pg) ** 2 * 4 + (b - pb) ** 2 * 2
        if d < bd:
            bd, best = d, i
    return best


def dominant(pixels):
    """Most frequent colour in a pixel list, ignoring near-black."""
    counts = {}
    for px in pixels:
        c = nearest(px)
        counts[c] = counts.get(c, 0) + 1
    if not counts:
        return 0
    return max(counts.items(), key=lambda kv: kv[1])[0]


def main():
    path = sys.argv[1]
    cols = int(sys.argv[2]) if len(sys.argv) > 2 else 66
    rows = int(sys.argv[3]) if len(sys.argv) > 3 else 6
    w, h, img = read_png(path)

    # bounding box of non-black content
    x0, y0, x1, y1 = w, h, -1, -1
    for y in range(h):
        for x in range(w):
            if sum(img[y][x]) > 40:
                if x < x0: x0 = x
                if x > x1: x1 = x
                if y < y0: y0 = y
                if y > y1: y1 = y
    print('image %dx%d, content box %d,%d..%d,%d (%dx%d)' %
          (w, h, x0, y0, x1, y1, x1 - x0 + 1, y1 - y0 + 1))

    bw, bh = x1 - x0 + 1, y1 - y0 + 1
    cells = []
    for cy in range(rows):
        line = []
        for cx in range(cols):
            sx0 = x0 + cx * bw // cols
            sx1 = x0 + (cx + 1) * bw // cols
            sy0 = y0 + cy * bh // rows
            sy1 = y0 + (cy + 1) * bh // rows
            sy1 = max(sy1, sy0 + 1)
            mid = sy0 + (sy1 - sy0) // 2
            top = []
            bot = []
            for y in range(sy0, sy1):
                for x in range(max(sx0, 0), max(sx1, 1)):
                    (top if y < mid else bot).append(img[y][x])
            ct = dominant(top)
            cb = dominant(bot)
            line.append((ct, cb))
        cells.append(line)

    # emit C header
    out = []
    out.append('/* Generated by tools/art_from_png.py from %s - do not edit. */' % path.split('/')[-1])
    out.append('#pragma once')
    out.append('#include "types.h"')
    out.append('')
    out.append('typedef struct { u8 ch; u8 attr; } bootart_cell_t;')
    out.append('')
    for cy, line in enumerate(cells):
        out.append('static const bootart_cell_t bootart_row%d[] = {' % cy)
        for cx, (ct, cb) in enumerate(line):
            if ct == 0 and cb == 0:
                ch, attr = 0x20, 0x00
            elif ct == cb:
                ch, attr = 0xDB, ct
            else:
                # upper half = bg, lower half = fg
                ch, attr = 0xDF, ((ct << 4) | cb)
            out.append('    {0x%02X,0x%02X},' % (ch, attr))
        out.append('};')
        out.append('')
    out.append('static const bootart_cell_t *const bootart_rows[] = {')
    for cy in range(rows):
        out.append('    bootart_row%d,' % cy)
    out.append('};')
    out.append('static const u8 bootart_widths[] = { %s };' % ', '.join([str(cols)] * rows))
    out.append('#define BOOTART_ROWS %d' % rows)
    open('include/bootart.h', 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    print('wrote include/bootart.h (%d x %d cells)' % (cols, rows))


main()
