import io
p='gui/gui.c'; s=io.open(p,encoding='utf-8').read()
s=s.replace('if (item < 0 || item > 10) { start_open = false; return; }',
            'if (item < 0 || item > 11) { start_open = false; return; }')
io.open(p,'w',encoding='utf-8').write(s)
print('ok')
