#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."
make -j4 >/tmp/build.log 2>&1
make build/minimal_desktop_autotest.iso >/dev/null 2>&1
timeout 45 qemu-system-i386 \
  -cdrom build/minimal_desktop_autotest.iso -m 256M \
  -display none -serial stdio \
  -netdev user,id=net0 -device rtl8139,netdev=net0 \
  -object filter-dump,id=f1,netdev=net0,file=/tmp/arp.pcap \
  -device isa-debug-exit,iobase=0x501,iosize=0x04 > /tmp/qemu.log 2>&1 || true
ls -l /tmp/arp.pcap
grep -aE 'rtl|arp|gui|FAIL|RESULT|cmdline|selftest|net\]' /tmp/qemu.log | head -60
