#!/usr/bin/env bash
# Stress the GUI: many Super presses, then long typing in the terminal.
set -u
cd "$(dirname "$0")/.."
ISO=build/minimal_desktop.iso
OUT=${1:-/tmp/shots8}
rm -rf "$OUT"

s=(log:minimal@os)
for k in m i n i p a c k e t; do s+=(key:$k); done
s+=(key:spc key:minus key:shift-i key:spc)
for k in m i n i m a l; do s+=(key:$k); done
s+=(key:minus)
for k in d e s k t o p; do s+=(key:$k); done
s+=(key:ret wait:5)
for k in s t a r t g u i; do s+=(key:$k); done
s+=(key:ret wait:8)

for i in $(seq 40); do s+=(key:meta_l); done
s+=(wait:2 shot:40-super-spam)

for k in a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a; do s+=(key:$k); done
s+=(wait:1 shot:41-long-text)

python3 tools/vm_shot.py "$ISO" "$OUT" "${s[@]}"
mkdir -p build/shots
cp "$OUT"/*.png build/shots/ 2>/dev/null || true
echo "--- serial tail ---"
tail -c 200 /tmp/md_serial.log
