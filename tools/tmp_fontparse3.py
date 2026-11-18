import re, io
s = io.open('include/vga_font.h', encoding='utf-8').read()
data = [int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', s)]
# For 8x8 font at 32 bytes/glyph, glyph N row 0 is at N*32. Find 'A' pattern as 8 bytes at stride 32.
# Known CP437 'A' = 0x0C 0x1E 0x33 0x33 0x3F 0x33 0x33 0x00 maybe; search any stride.
for pat in ([0x7E,0x81,0xA5,0x81,0x81,0xBD,0x99,0x81], [0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00]):
    hits=[i for i in range(len(data)-8) if data[i:i+8]==pat]
    print('pattern hits', hits[:8])
# dump around offset 34 (first nonzero) and 34+32
print('34:  ', ' '.join('%02X'%b for b in data[32:64]))
print('66:  ', ' '.join('%02X'%b for b in data[64:96]))
print('98:  ', ' '.join('%02X'%b for b in data[96:128]))
