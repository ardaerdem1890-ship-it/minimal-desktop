import io,re
s=io.open('/tmp/md_serial.log',encoding='utf-8',errors='replace').read()
# split by prompt
parts=re.split(r'(?:ROOT|minimal)@os:[^\n]*\$ ',s)
print('segments:',len(parts))
for i,p in enumerate(parts[1:],1):
    lines=[l.strip() for l in p.split('\n') if l.strip()]
    if lines:
        print('[%02d] %s'%(i,' | '.join(lines[:4])[:180]))
