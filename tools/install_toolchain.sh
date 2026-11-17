#!/usr/bin/env bash
# Minimal Desktop - toolchain installer (run as root in Ubuntu/WSL)
set -e
export DEBIAN_FRONTEND=noninteractive
dpkg --configure -a || true
apt-get update -qq
apt-get install -y build-essential gcc-i686-linux-gnu nasm qemu-system-x86 \
    grub-pc-bin grub-common xorriso mtools python3 dosfstools curl
echo "--- toolchain ---"
for t in i686-linux-gnu-gcc nasm qemu-system-i386 grub-mkrescue xorriso mtools make python3; do
    printf '%-22s ' "$t"
    command -v "$t" || echo MISSING
done
