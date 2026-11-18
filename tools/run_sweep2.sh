#!/usr/bin/env bash
set -u
cd "$(dirname "$0")/.."
ISO=build/minimal_desktop.iso
OUT=/tmp/sweep2_steps
rm -rf "$OUT"
s=(log:username:)
for k in r o o t; do s+=(key:$k); done; s+=(key:ret)
for k in r o o t; do s+=(key:$k); done; s+=(key:ret wait:1)
addcmd() {
  local line="$1" i ch key
  for ((i=0;i<${#line};i++)); do
    ch="${line:$i:1}"
    case "$ch" in
      ' ') key=spc;; '-') key=minus;; '.') key=dot;; '/') key=slash;;
      ':') key=shift-semicolon;; '=') key=equal;; '*') key=shift-8;;
      ',') key=comma;; *) key="$ch";;
    esac
    s+=(key:$key)
  done
  s+=(key:ret wait:1)
}
# skip interactive: less, read, saver, matrix, slideshow
for c in mount sync chkdsk cdrom "ipconfig set 10.0.2.50 255.255.255.0 10.0.2.2 10.0.2.3" ifconfig "ipconfig dhcp" "backup /disk/b.img" "restore /disk/b.img" "useradd test pass123" users "userdel test" users "passwd ROOT newroot" "passwd ROOT root" sudo fortune "keymap tr-q" charmap "keymap us" "store info doom"; do addcmd "$c"; done
python3 tools/vm_shot.py "$ISO" "$OUT" "${s[@]}" > /tmp/sweep2_run.log 2>&1
cp /tmp/md_serial.log build/sweep2_serial.log
echo done
