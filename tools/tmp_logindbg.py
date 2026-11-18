import io, os, subprocess, sys, time
ISO='build/minimal_desktop.iso'
SER='/tmp/dbg.ser'; MON='/tmp/dbg.mon'
for f in (SER,MON):
    if os.path.exists(f): os.unlink(f)
q=subprocess.Popen(['qemu-system-i386','-cdrom',ISO,'-m','256M','-display','none',
    '-serial','file:'+SER,'-monitor','stdio'],
    stdin=subprocess.PIPE,stdout=open(MON,'wb'),stderr=subprocess.STDOUT)
def mon(c):
    q.stdin.write((c+'\n').encode()); q.stdin.flush(); time.sleep(0.2)
def send(t):
    mp={' ':'spc','-':'minus'}
    for c in t: mon('sendkey '+mp.get(c,c)); time.sleep(0.18)
    mon('sendkey ret'); time.sleep(1)
def ser():
    try: return io.open(SER,encoding='utf-8',errors='replace').read()
    except: return ''
try:
    time.sleep(16)
    print('BEFORE:',repr(ser()[-200:]))
    send('ROOT'); time.sleep(1)
    print('AFTER USER:',repr(ser()[-300:]))
    send('root'); time.sleep(1)
    print('AFTER PASS:',repr(ser()[-400:]))
finally:
    if q.poll() is None: q.kill()
