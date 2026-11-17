import io, socket, subprocess, sys, time
sys.path.insert(0,'tools')
import vm_shot

ISO='build/minimal_desktop.iso'
SOCK='/tmp/ivmon.sock'
SER='/tmp/iv.ser'
for f in (SOCK,SER):
    if __import__('os').path.exists(f): __import__('os').unlink(f)

q=subprocess.Popen(['qemu-system-i386','-cdrom',ISO,'-m','256M','-display','none',
    '-serial','file:'+SER,'-monitor','unix:%s,server,nowait'%SOCK,
    '-netdev','user,id=n0','-device','rtl8139,netdev=n0',
    '-device','isa-debug-exit,iobase=0x501,iosize=0x04'])
try:
    s=vm_shot.connect()
    vm_shot.mon(s,'')
    time.sleep(20)
    # type login ROOT/root
    for k in ['r','o','o','t','ret','r','o','o','t','ret']:
        vm_shot.mon(s,'sendkey '+k); time.sleep(0.15)
    time.sleep(1)
    # install GUI module
    line='minipacket -I minimal-desktop'
    for c in line:
        key={' ':'spc','-':'minus'}.get(c,c)
        vm_shot.mon(s,'sendkey '+key); time.sleep(0.1)
    vm_shot.mon(s,'sendkey ret'); time.sleep(4)
    # startgui
    for c in 'startgui': vm_shot.mon(s,'sendkey '+c); time.sleep(0.1)
    vm_shot.mon(s,'sendkey ret'); time.sleep(6)
    # open start menu with super
    vm_shot.mon(s,'sendkey super_l'); time.sleep(1)
    vm_shot.mon(s,'screendump /tmp/ivmenu.ppm'); time.sleep(1)
    # image viewer is item 11 -> 6th row, 1st col (0-indexed row=5,col=0)
    # menu x=2 y=48, cells: cellx=5+(i%2)*74, celly=64+(i/2)*18
    # item 11 -> col0 row5 -> x=5+10=15, y=64+90=154
    vm_shot.mon(s,'mouse_move 15 154'); time.sleep(0.3)
    vm_shot.mon(s,'mouse_button 1'); time.sleep(0.3)
    vm_shot.mon(s,'mouse_button 0'); time.sleep(1)
    vm_shot.mon(s,'screendump /tmp/ivwin.ppm'); time.sleep(1)
    print('shots done')
finally:
    try: vm_shot.mon(s,'quit')
    except: pass
    if q.poll() is None: q.kill()
