import re, io
s = io.open('include/vga_font.h', encoding='utf-8').read()
data = [int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', s)]
# Look at nonzero regions to understand the layout
nz = [i for i,b in enumerate(data) if b]
print('nonzero count', len(nz), 'first', nz[0] if nz else None, 'last', nz[-1] if nz else None)
# find the 'A' from font8x8_source.h
f = io.open('gui/font8x8.h', encoding='utf-8').read()
m = re.search(r"'A'.*?\{([^}]+)\}", f, re.S)
print('font8x8 A', m.group(1)[:80] if m else 'none')
# search vga data for font8x8 'A' bytes
import re as _re
fa = [int(x,16) for x in _re.findall(r'0x([0-9A-Fa-f]{2})', m.group(1))] if m else []
print('A bytes', fa)
for stride in (8,16,32):
    hits=[i for i in range(0,len(data)-8,stride) if data[i:i+8]==fa[:8]]
    print('stride',stride,'hits',hits[:6])
# dump first 128 bytes in hex rows
print('first 128:', ' '.join('%02X'%b for b in data[:128]))
