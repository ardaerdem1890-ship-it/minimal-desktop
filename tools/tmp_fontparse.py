import re, io
s = io.open('include/vga_font.h', encoding='utf-8').read()
data = [int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', s)]
print('bytes', len(data))
# find 'A' pattern 0x0C 0x1E 0x33 0x33 0x3F ...
pats = {
 'A':[0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00],
 'a':[0x00,0x00,0x1E,0x33,0x3F,0x33,0x3F,0x00],
 'space':[0]*8,
}
for name,p in pats.items():
    hits=[i for i in range(len(data)-8) if data[i:i+8]==p]
    print(name, 'offsets', hits[:5], 'char ids', [h//8 for h in hits[:5]])
# print bytes around char 0x41 assuming 8 bytes/glyph
o=0x41*8
print('0x41*8 =',hex(o), data[o:o+8])
# Try 16-byte glyph layout
o2=0x41*16
print('0x41*16 =',hex(o2), data[o2:o2+16])
