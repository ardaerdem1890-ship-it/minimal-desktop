import re

src = open('drivers/keyboard.c', encoding='utf-8').read()
block = src[src.index('static const kbd_layout_t layouts[] = {'):]
block = block[:block.index('\n};')]
entries = re.findall(r'L\("([^"]+)",\s*"([^"]*)",\s*(.*?)\)\s*,?\s*(?=\n\s*L\(|\Z)', block, re.S)
want = [12, 12, 12, 12, 11, 11, 10, 10, 2, 2]
names = ['digits', 'digits_s', 'row1', 'row1_s', 'row2', 'row2_s', 'row3', 'row3_s', 'extra', 'extra_s']
ok = True
for name, label, rest in entries:
    # split the C string literals in order
    lits = re.findall(r'"((?:[^"\\]|\\.)*)"', rest)
    if len(lits) != 10:
        print('BAD', name, 'literal count', len(lits))
        ok = False
        continue
    for lit, w, fname in zip(lits, want, names):
        val = lit.encode().decode('unicode_escape')
        if len(val) != w:
            print('BAD %-6s %-8s len=%d want=%d  %r' % (name, fname, len(val), w, val))
            ok = False
print('layouts:', len(entries))
print('OK' if ok else 'ERRORS')
