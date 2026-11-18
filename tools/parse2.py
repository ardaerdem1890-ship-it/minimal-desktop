import io,re
s=io.open('build/sweep2_serial.log',encoding='utf-8',errors='replace').read()
parts=re.split(r'\S+@os:[^\n]*\$ ',s)
n=0
for p in parts[1:]:
    lines=[l.strip() for l in p.split('\n') if l.strip()]
    if lines:
        n+=1
        print('[%02d] %s'%(n,' | '.join(lines[:4])[:190]))
