import io, os, subprocess, sys, time
sys.path.insert(0,'tools')
import vm_shot

ISO='build/minimal_desktop.iso'
SOCK='/tmp/sweep.sock'; SER='/tmp/sweep.ser'
for f in (SOCK,SER):
    if os.path.exists(f): os.unlink(f)
q=subprocess.Popen(['qemu-system-i386','-cdrom',ISO,'-m','256M','-display','none',
    '-serial','file:'+SER,'-monitor','unix:%s,server,nowait'%SOCK,
    '-netdev','user,id=n0','-device','rtl8139,netdev=n0',
    '-device','isa-debug-exit,iobase=0x501,iosize=0x04'])

KM={' ':'spc','-':'minus','.':'dot','/':'slash',':':'shift-semicolon','=':'equal','_':'shift-minus','*':'shift-8'}
def send(s,text):
    for c in text:
        vm_shot.mon(s,'sendkey '+KM.get(c,c)); time.sleep(0.09)
    vm_shot.mon(s,'sendkey ret'); time.sleep(0.7)
def ser():
    try: return io.open(SER,encoding='utf-8',errors='replace').read()
    except: return ''

TESTS=[
 'help','sysinfo','uptime','date','time','drivers','taskmng','hwinfo','whoami','users',
 'calc 6*7','fortune','banner HI','charmap','keymap list','color','theme',
 'ls /','ls /pictures','ls /home','tree /home','du /home','find txt /',
 'memmap','store list','store status','minipacket -L',
 'ifconfig','ipconfig status','ping 10.0.2.2 1','nslookup example.com',
 'mkdir /tmp/testdir','touch /tmp/testfile','write /tmp/testfile hello123',
 'cat /tmp/testfile','cp /tmp/testfile /tmp/copy.txt','mv /tmp/copy.txt /tmp/moved.txt',
 'ls /tmp','hexdump /tmp/testfile','echo hello world','cd /tmp','pwd','cd /',
 'less /etc/motd','read /etc/motd','mount','sync','chkdsk','cdrom',
 'saver 0 1','matrix 1','beep 440 50','slideshow 1',
]

try:
    s=vm_shot.connect()
    vm_shot.mon(s,'')
    # wait for login prompt
    deadline=time.time()+60
    while 'username:' not in ser() and time.time()<deadline: time.sleep(0.5)
    send(s,'ROOT'); send(s,'root'); time.sleep(1)
    print('LOGIN:', 'Welcome' in ser())

    results=[]
    for t in TESTS:
        before=len(ser()); send(s,t); time.sleep(0.5)
        out=ser()[before:]
        # take last meaningful lines
        lines=[l.strip() for l in out.split('\n') if l.strip() and l.strip()!=t]
        summary=' | '.join(lines[:6])[:300]
        ok = bool(lines)
        results.append((t,ok,summary))
        print(('%-24s %s  %s'%(t,'PASS' if ok else 'FAIL',summary)))
        sys.stdout.flush()
finally:
    try: vm_shot.mon(s,'quit')
    except: pass
    if q.poll() is None: q.kill()
