import io, socket, subprocess, sys, time
sys.path.insert(0, 'tools')
import vm_shot

ISO='build/minimal_desktop.iso'
SOCK='/tmp/alltest.sock'
SER='/tmp/alltest.ser'
import os
for f in (SOCK,SER):
    if os.path.exists(f): os.unlink(f)

q=subprocess.Popen(['qemu-system-i386','-cdrom',ISO,'-m','256M','-display','none',
    '-serial','file:'+SER,'-monitor','unix:%s,server,nowait'%SOCK,
    '-netdev','user,id=n0','-device','rtl8139,netdev=n0',
    '-device','isa-debug-exit,iobase=0x501,iosize=0x04'])

def send(s, text):
    # type a command string using sendkey (letters, digits, space, minus, etc.)
    keymap={' ':'spc','-':'minus','.':'dot','/':'slash',':':'shift-semicolon',
            '=':'equal','_':'shift-minus'}
    for c in text:
        k=keymap.get(c,c)
        vm_shot.mon(s,'sendkey '+k)
        time.sleep(0.06)
    vm_shot.mon(s,'sendkey ret')
    time.sleep(0.8)

def get_serial():
    try:
        return io.open(SER,encoding='utf-8',errors='replace').read()
    except: return ''

try:
    s=vm_shot.connect()
    vm_shot.mon(s,'')
    time.sleep(14)
    # login ROOT/root
    send(s,'ROOT'); send(s,'root'); time.sleep(1)

    tests = [
      'help','sysinfo','uptime','date','time','drivers','taskmng','hwinfo',
      'calc 6*7','fortune','banner HELLO','charmap','keymap list','whoami','users',
      'ls /','ls /pictures','ls /home','tree /home','du /home','find txt /',
      'memmap','hexdump /etc/motd','df','echo hello test','cat /etc/motd',
      'store list','store status','minipacket -L',
      'ifconfig','ipconfig status','ping 10.0.2.2 1','nslookup example.com',
      'curl http://10.0.2.2:8000/','weather',
      'saver 0 1','matrix 1','beep 440 50','mkdir /tmp/testdir','touch /tmp/testfile',
      'write /tmp/testfile hello123','cat /tmp/testfile','cp /tmp/testfile /tmp/copy.txt',
      'mv /tmp/copy.txt /tmp/moved.txt','ls /tmp','rm /tmp/testfile','rm /tmp/moved.txt',
      'rmdir /tmp/testdir','cd /tmp','pwd','cd /',
      'edit --help','less /etc/motd','read /etc/motd',
      'mount','sync','chkdsk','cdrom',
      'backup /disk/backup.img','restore /disk/backup.img',
    ]
    for t in tests:
        before=len(get_serial())
        send(s,t)
        time.sleep(0.5)
        after=get_serial()
        chunk=after[before:]
        print('===== %s =====' % t)
        print(chunk[-700:] if len(chunk)>700 else chunk)
        sys.stdout.flush()
finally:
    try: vm_shot.mon(s,'quit')
    except: pass
    time.sleep(0.3)
    if q.poll() is None: q.kill()
