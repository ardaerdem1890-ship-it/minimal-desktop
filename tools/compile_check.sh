#!/usr/bin/env bash
# Quick syntax/warning check for every C file in the tree.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
CFLAGS="-m32 -std=gnu11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-builtin -Wall -Wextra -Wno-unused-parameter -Iinclude"
fail=0
for src in kernel/*.c drivers/*.c fs/*.c net/*.c gui/*.c; do
    [ -e "$src" ] || continue
    out=$(mktemp)
    if ! i686-linux-gnu-gcc $CFLAGS -c "$src" -o /dev/null 2>"$out"; then
        echo "=== FAIL $src"
        cat "$out"
        fail=1
    else
        if [ -s "$out" ]; then
            echo "=== WARN $src"
            cat "$out"
        fi
    fi
    rm -f "$out"
done
exit $fail
