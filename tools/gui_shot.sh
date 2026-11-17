#!/usr/bin/env bash
# Capture a screenshot of the Minimal Desktop GUI via QEMU monitor screendump.
set -u
cd /mnt/d
cd Varsay*
cd minimal-desktop
ISO=build/minimal_desktop.iso

rm -f /tmp/qemu_in /tmp/gui_ser.log /tmp/gui_out.log /tmp/gui.png /tmp/qemu_mon.sock
mkfifo /tmp/qemu_in
qemu-system-i386 -cdrom "$ISO" -m 256M \
  -display none -serial stdio -monitor unix:/tmp/qemu_mon.sock,server,nowait \
  -netdev user,id=net0 -device rtl8139,netdev=net0 \
  -device isa-debug-exit,iobase=0x501,iosize=0x04 < /tmp/qemu_in > /tmp/gui_out.log 2>&1 &
QPID=$!
exec 9>/tmp/qemu_in
sleep 35
echo "startgui" >&9
sleep 12
python3 - <<'PYEOF'
import socket, time
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect('/tmp/qemu_mon.sock')
s.sendall(b'screendump /tmp/gui.png\n')
time.sleep(4)
s.close()
PYEOF
sleep 6
echo "quit" > /tmp/qemu_in
sleep 2
kill $QPID 2>/dev/null
wait $QPID 2>/dev/null
echo done
