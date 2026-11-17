import io, re

lines = io.open('/tmp/md_serial.log', encoding='utf-8', errors='replace').read().split('\n')
data = bytearray()
for line in lines:
    m = re.match(r'^[0-9A-F]{4}:((?: [0-9A-F]{2}){16})', line.strip())
    if m:
        for h in m.group(1).split():
            data.append(int(h, 16))
print('font bytes:', len(data))
assert len(data) == 8192

out = io.StringIO()
out.write('/* Standard IBM VGA 8x8 font (8192 bytes) - captured from QEMU.\n')
out.write(' * Written back after leaving mode 13h so the font never depends on the\n')
out.write(' * emulated VGA implementing plane-2 reads correctly (VirtualBox does not).\n')
out.write(' */\n#pragma once\n#include "types.h"\n\nstatic const u8 vga_builtin_font[8192] = {\n')
for i in range(0, 8192, 16):
    out.write('    ' + ', '.join('0x%02X' % b for b in data[i:i+16]) + ',\n')
out.write('};\n')
io.open('include/vga_font.h', 'w', encoding='utf-8').write(out.getvalue())
print('wrote include/vga_font.h')
