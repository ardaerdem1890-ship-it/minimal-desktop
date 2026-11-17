#!/usr/bin/env bash
set -u
cd "$(dirname "$0")/.."
ISO=build/minimal_desktop.iso
rm -f /tmp/qemu_in /tmp/int_out.log /tmp/qemu_mon.sock
mkfifo /tmp/qemu_in
qemu-system-i386 -cdrom "$ISO" -m 256M -display none -serial file:/tmp/int_out.log -monitor unix:/tmp/qemu_mon.sock,server,nowait -netdev user,id=net0 -device rtl8139,netdev=net0 < /tmp/qemu_in > /tmp/qemu_run.log 2>&1 &
QPID=$!
exec 9>/tmp/qemu_in
sleep 25
{
  echo 'store list'; sleep 5
  echo 'store install snake'; sleep 10
  echo 'snake'; sleep 6
  echo 'store install pong'; sleep 10
  echo 'pong'; sleep 6
  echo 'ifconfig'; sleep 4
  echo 'ping 10.0.2.2'; sleep 10
  echo 'nslookup example.com'; sleep 12
  echo 'curl http://10.0.2.2:8000/store'; sleep 12
} >&9
sleep 15
python3 - <<'PY'
import socket, time
s=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
s.connect('/tmp/qemu_mon.sock')
s.sendall(b'quit\n')
time.sleep(2)
s.close()
PY
sleep 2
kill $QPID 2>/dev/null
wait $QPID 2>/dev/null
echo '---LOG---'
tail -n 300 /tmp/int_out.log
