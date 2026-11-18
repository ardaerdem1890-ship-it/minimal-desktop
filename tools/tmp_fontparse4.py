import re, io
s = io.open('include/vga_font.h', encoding='utf-8').read()
data = [int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', s)]
# glyph 0x41 at offset 0x41*32 = 0x820; plane2 bytes are +8..+15
for ch in (0x41,0x61,0x20,0xDB):
    o=ch*32
    print(hex(ch),'plane2 rows:',[hex(b) for b in data[o+8:o+16]])
