#!/usr/bin/env bash
set -u
cd "$(dirname "$0")/.."
ISO=build/minimal_desktop.iso
OUT=/tmp/guitest
rm -rf "$OUT"
s=(log:username:)
for k in r o o t; do s+=(key:$k); done; s+=(key:ret)
for k in r o o t; do s+=(key:$k); done; s+=(key:ret wait:1)
# install GUI module
for k in m i n i p a c k e t; do s+=(key:$k); done
s+=(key:spc key:minus key:shift-i key:spc)
for k in m i n i m a l; do s+=(key:$k); done
s+=(key:minus)
for k in d e s k t o p; do s+=(key:$k); done
s+=(key:ret wait:5)
# start GUI
for k in s t a r t g u i; do s+=(key:$k); done
s+=(key:ret wait:7 shot:01_desktop)
# image viewer via v key
s+=(key:v wait:3 shot:02_imgview)
# next image
s+=(key:right wait:1 shot:03_img_next)
# settings via o key
s+=(key:o wait:2 shot:04_settings)
# cursor style change
s+=(key:c wait:1 shot:05_cursor)
# exit GUI
s+=(key:esc wait:3 shot:06_after)
python3 tools/vm_shot.py "$ISO" "$OUT" "${s[@]}" > /tmp/gui_run.log 2>&1
mkdir -p build/shots
cp "$OUT"/*.png build/shots/ 2>/dev/null || true
echo "gui run done"
