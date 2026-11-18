#!/usr/bin/env python3
"""Generate include/bootart.h from the user's exact ANSI splash bash."""
import io, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from art_from_ansi import parse

src = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'splash_exact.txt')
raw = io.open(src, encoding='utf-8').read()
# echo -e "\033[...]" -> actual ESC sequences
raw = raw.replace('\\033', '\x1b')
lines = []
for line in raw.split('\n'):
    line = line.strip()
    if not line.startswith('echo'):
        continue
    # strip echo -e " and trailing "
    line = line[line.index('"') + 1:]
    if line.endswith('"'):
        line = line[:-1]
    lines.append(line)
text = '\n'.join(lines)
rows = parse(text)
for r in rows:
    while r and r[-1][0] == 0x20:
        r.pop()
width = max(len(r) for r in rows)

out = io.StringIO()
out.write('/* Generated from the user-provided ANSI splash bash (tower2) */\n')
out.write('#pragma once\n#include "types.h"\n\n')
out.write('typedef struct { u8 ch; u8 attr; } bootart_cell_t;\n\n')
for i, r in enumerate(rows):
    out.write('static const bootart_cell_t bootart_row%d[] = {\n' % i)
    for k in range(0, len(r), 10):
        chunk = r[k:k + 10]
        out.write('    ' + ', '.join('{0x%02X,0x%02X}' % (c, (fg | (bg << 4))) for c, fg, bg in chunk) + ',\n')
    out.write('};\n\n')
out.write('static const bootart_cell_t *const bootart_rows[] = {\n')
for i in range(len(rows)):
    out.write('    bootart_row%d,\n' % i)
out.write('};\n')
out.write('static const u8 bootart_widths[] = { %s };\n' % ', '.join(str(len(r)) for r in rows))
out.write('#define BOOTART_ROWS %d\n' % len(rows))
out.write('#define BOOTART_COLS %d\n' % width)

io.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'include', 'bootart.h'), 'w', encoding='utf-8').write(out.getvalue())
print('bootart from exact bash: %d rows x %d cols' % (len(rows), width))
for r in rows:
    print(''.join('#' if c else '.' for c, _, _ in r))
