#!/usr/bin/env python3
"""Render include/bootart.h to a PNG preview (8x16 text cells, VGA palette)."""
import re
import struct
import sys
import zlib

PAL = [
    (0, 0, 0), (0, 0, 170), (0, 170, 0), (0, 170, 170),
    (170, 0, 0), (170, 0, 170), (170, 85, 0), (170, 170, 170),
    (85, 85, 85), (85, 85, 255), (85, 255, 85), (85, 255, 255),
    (255, 85, 85), (255, 85, 255), (255, 255, 85), (255, 255, 255),
]

CW, CH = 8, 16


def load(path):
    src = open(path, encoding='utf-8').read()
    rows = []
    for name, body in re.findall(r'static const bootart_cell_t (\w+)\[\] = \{(.*?)\};',
                                 src, re.S):
        if not name.startswith('bootart_row'):
            continue
        idx = int(name[len('bootart_row'):])
        cells = re.findall(r'\{(0x[0-9A-Fa-f]+),(0x[0-9A-Fa-f]+)\}', body)
        rows.append((idx, [(int(a, 16), int(b, 16)) for a, b in cells]))
    rows.sort()
    widths = re.search(r'bootart_widths\[\] = \{([^}]*)\}', src)
    widths = [int(x) for x in widths.group(1).split(',')] if widths else []
    return [r for _, r in rows], widths


def draw(buf, w, h, x0, y0):
    for ry, row in enumerate(buf):
        for cx, (ch, attr) in enumerate(row):
            fg = attr & 0x0F
            bg = attr >> 4
            px, py = cx * CW, ry * CH
            if px >= w or py >= h:
                continue
            for y in range(CH):
                for x in range(CW):
                    on = False
                    if ch == 0xDB:
                        on = True
                    elif ch == 0xDC:
                        on = y < CH // 2
                    elif ch == 0xDF:
                        on = y >= CH // 2
                    elif ch == 0xDE:
                        on = x < CW // 2
                    elif ch == 0xB0:
                        on = (x + y) % 4 == 0
                    elif ch == 0xB1:
                        on = (x + y) % 2 == 0
                    elif ch == 0xB2:
                        on = (x + y) % 4 != 0
                    r, g, b = PAL[fg if on else bg]
                    X, Y = px + x, py + y
                    if 0 <= X < w and 0 <= Y < h:
                        i = (Y * w + X) * 3
                        img[i] = r
                        img[i + 1] = g
                        img[i + 2] = b


def png(path, w, h, img):
    raw = b''.join(b'\x00' + bytes(img[y * w * 3:(y + 1) * w * 3]) for y in range(h))

    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)

    out = b'\x89PNG\r\n\x1a\n'
    out += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    out += chunk(b'IDAT', zlib.compress(raw, 9))
    out += chunk(b'IEND', b'')
    open(path, 'wb').write(out)


rows, widths = load('include/bootart.h')
print('rows:', len(rows), 'widths:', [len(r) for r in rows])
cols = max(len(r) for r in rows)
w, h = (cols + 8) * CW, (len(rows) + 4) * CH
img = bytearray(w * h * 3)
# paint background black
draw([[(0x20, 0x00)] * (cols + 8) for _ in range(len(rows) + 4)], w, h, 0, 0)
for i in range(0, len(img), 3):
    img[i] = img[i + 1] = img[i + 2] = 0
padded = [r + [(0x20, 0x00)] * (cols - len(r)) for r in rows]
draw(padded, w, h, 2 * CW, 1 * CH)
png('build/bootart_preview.png', w, h, img)
print('wrote build/bootart_preview.png', w, 'x', h)
