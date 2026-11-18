import re, io
s = io.open('include/vga_font.h', encoding='utf-8').read()
data = [int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', s)]
for ch in (0x01,0x20,0x41,0x61,0xDB,0xDF,0xDC):
    o=ch*32
    print(hex(ch),'offset',hex(o),'rows:',[hex(b) for b in data[o:o+8]])
