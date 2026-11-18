import io, os, subprocess, sys, time

ISO='build/minimal_desktop.iso'
FIFO='/tmp/mdtest.fifo'
MON='/tmp/mdtest.mon'
SER='/tmp/mdtest.ser'
for f in (FIFO,MON,SER):
    if os.path.exists(f): os.unlink(f)
os.mkfifo(FIFO)

q=subprocess.Popen(['qemu-system-i386','-cdrom',ISO,'-m','256M','-display','none',
    '-serial','file:'+SER,'-monitor','stdio',
    '-netdev','user,id=n0','-device','rtl8139,netdev=n0',
    '-device','isa-debug-exit,iobase=0x501,iosize=0x04'],
    stdin=subprocess.PIPE, stdout=open(MON,'wb'), stderr=subprocess.STDOUT)

def mon(cmd):
    q.stdin.write((cmd+'\n').encode()); q.stdin.flush(); time.sleep(0.12)

def key(k):
    mon('sendkey '+k); time.sleep(0.06)

def send(text):
    keymap={' ':'spc','-':'minus','.':'dot','/':'slash',':':'shift-semicolon','=':'equal','_':'shift-minus'}
    for c in text:
        key(keymap.get(c,c))
    mon('sendkey ret'); time.sleep(0.9)

def ser():
    try: return io.open(SER,encoding='utf-8',errors='replace').read()
    except: return ''

try:
    time.sleep(14)
    send('ROOT'); send('root'); time.sleep(1)
    tests=[
      'help','sysinfo','uptime','date','time','drivers','taskmng','hwinfo',
      'calc 6*7','fortune','banner HELLO','charmap','keymap list','whoami','users',
      'ls /','ls /pictures','ls /home','tree /home','du /home','find txt /',
      'memmap','store list','store status','minipacket -L',
      'ifconfig','ipconfig status','ping 10.0.2.2 1','nslookup example.com',
      'mkdir /tmp/testdir','touch /tmp/testfile','write /tmp/testfile hello123',
      'cat /tmp/testfile','cp /tmp/testfile /tmp/copy.txt','mv /tmp/copy.txt /tmp/moved.txt',
      'ls /tmp','rm /tmp/testfile','rm /tmp/moved.txt',
      'cd /tmp','pwd','cd /',
      'less /etc/motd','read /etc/motd',
      'mount','sync','chkdsk','cdrom',
    ]
    for t in tests:
        before=len(ser()); send(t); time.sleep(0.4)
        chunk=ser()[before:]
        print('===== %s ====='%t)
        print(chunk[-600:] if len(chunk)>600 else chunk)
        sys.stdout.flush()
finally:
    if q.poll() is None: q.kill()
