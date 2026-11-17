#!/usr/bin/env bash
# Interactive-ish shell session in QEMU via serial file + monitor socket.
set -u
cd /mnt/d
cd Varsay*
cd minimal-desktop
ISO=build/minimal_desktop.iso

rm -f /tmp/qemu_in /tmp/shell_out.log /tmp/qemu_mon.sock /tmp/qemu_shell.log
mkfifo /tmp/qemu_in
qemu-system-i386 -cdrom "$ISO" -m 256M \
  -display none -serial file:/tmp/shell_out.log -monitor unix:/tmp/qemu_mon.sock,server,nowait \
  -netdev user,id=net0 -device rtl8139,netdev=net0 \
  -device isa-debug-exit,iobase=0x501,iosize=0x04 < /tmp/qemu_in > /tmp/qemu_shell.log 2>&1 &
QPID=$!
exec 9>/tmp/qemu_in

sleep 30
{
  echo "store list"
  sleep 6
  echo "store install snake"
  sleep 12
  echo "snake"
  sleep 8
  echo "store install pong"
  sleep 12
  echo "pong"
  sleep 8
  echo "ifconfig"
  sleep 4
  echo "ping 10.0.2.2"
  sleep 12
  echo "selftest"
  sleep 10
} >&9

sleep 40
python3 - <<'PYEOF'
import socket, time
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect('/tmp/qemu_mon.sock')
s.sendall(b'quit\n')
time.sleep(3)
s.close()
PYEOF
sleep 2
kill $QPID 2>/dev/null
wait $QPID 2>/dev/null
echo done
