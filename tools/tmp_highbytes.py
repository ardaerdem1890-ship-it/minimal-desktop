import io

# shell.c: accept high bytes as printable
p='kernel/shell.c'; s=io.open(p,encoding='utf-8').read()
old='''        } else if (key >= 0x20 && key < 0x7F) {
            if (len < LINE_MAX - 1) {
                linebuf[len++] = (char)key;
                kprintf("%c", (char)key);
            }
        }'''
new='''        } else if ((key >= 0x20 && key < 0x7F) || (key >= 0x80)) {
            if (len < LINE_MAX - 1) {
                linebuf[len++] = (char)key;
                kprintf("%c", (char)key);
            }
        }'''
assert old in s
s=s.replace(old,new,1)
io.open(p,'w',encoding='utf-8').write(s)

# editor.c
p='kernel/editor.c'; s=io.open(p,encoding='utf-8').read()
s=s.replace('''        } else if (key >= 0x20 && key < 0x7F) {
            if (elen < (int)FS_MAX_FILE) {
                memmove(ebuf + epos + 1, ebuf + epos, (size_t)(elen - epos));
                ebuf[epos] = (char)key;''',
'''        } else if ((key >= 0x20 && key < 0x7F) || (key >= 0x80)) {
            if (elen < (int)FS_MAX_FILE) {
                memmove(ebuf + epos + 1, ebuf + epos, (size_t)(elen - epos));
                ebuf[epos] = (char)key;''',1)
io.open(p,'w',encoding='utf-8').write(s)
print('ok')
