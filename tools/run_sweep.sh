#!/usr/bin/env bash
set -u
cd "$(dirname "$0")/.."
ISO=build/minimal_desktop.iso
OUT=/tmp/sweep_steps
rm -rf "$OUT"

s=(log:username:)
for k in r o o t; do s+=(key:$k); done; s+=(key:ret)
for k in r o o t; do s+=(key:$k); done; s+=(key:ret wait:1)

addcmd() {
  local line="$1"
  local i ch key
  for ((i=0; i<${#line}; i++)); do
    ch="${line:$i:1}"
    case "$ch" in
      ' ') key=spc ;;
      '-') key=minus ;;
      '.') key=dot ;;
      '/') key=slash ;;
      ':') key=shift-semicolon ;;
      '=') key=equal ;;
      '*') key=shift-8 ;;
      ',') key=comma ;;
      *) key="$ch" ;;
    esac
    s+=(key:$key)
  done
  s+=(key:ret wait:1)
}

for c in help sysinfo uptime date time drivers taskmng hwinfo whoami users; do addcmd "$c"; done
addcmd "calc 6*7"
for c in fortune banner charmap; do addcmd "$c"; done
addcmd "keymap list"
for c in theme color; do addcmd "$c"; done
for c in "ls /" "ls /pictures" "ls /home" "tree /home" "du /home" "find txt /"; do addcmd "$c"; done
for c in memmap "store list" "store status" "minipacket -L"; do addcmd "$c"; done
for c in ifconfig "ipconfig status" "ping 10.0.2.2 1" "nslookup example.com"; do addcmd "$c"; done
addcmd "mkdir /tmp/testdir"
addcmd "touch /tmp/testfile"
addcmd "write /tmp/testfile hello123"
for c in "cat /tmp/testfile" "cp /tmp/testfile /tmp/copy.txt" "mv /tmp/copy.txt /tmp/moved.txt" "ls /tmp" "hexdump /tmp/testfile" "echo hello world" "cd /tmp" pwd "cd /" "less /etc/motd" "read /etc/motd" mount sync chkdsk cdrom "saver 0 1" "matrix 1" "beep 440 50" "slideshow 1"; do addcmd "$c"; done

python3 tools/vm_shot.py "$ISO" "$OUT" "${s[@]}" > /tmp/sweep_run.log 2>&1
cp /tmp/md_serial.log build/sweep_serial.log
echo "run done"
