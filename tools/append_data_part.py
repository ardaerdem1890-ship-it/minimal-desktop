#!/usr/bin/env python3
"""Append an 8 MiB FAT16 data partition to an isohybrid ISO image.

The ISO already has an MBR with one entry covering the ISO itself. We add a
second entry for the data partition; the kernel's fat_automount() finds it and
mounts it as /disk. Files written there survive a reboot after `install`.
"""
import os
import shutil
import struct
import subprocess
import sys

DATA_SECTORS = 16384          # 8 MiB
PART_LBA_INNER = None         # filled at runtime


def run(cmd):
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)


def chs(lba):
    cyl = lba // (16 * 63)
    rem = lba % (16 * 63)
    head = rem // 63
    sector = rem % 63 + 1
    return bytes([head, sector | ((cyl >> 2) & 0xC0), cyl & 0xFF])


def main():
    path = sys.argv[1]
    work = os.path.join(os.path.dirname(path), 'fatdata')
    os.makedirs(work, exist_ok=True)
    part = os.path.join(work, 'data.img')

    # mformat auto-selects FAT16 when the cluster count is >= 4085.
    run(['mformat', '-i', part, '-C', '-T', str(DATA_SECTORS),
         '-h', '16', '-s', '63', '-c', '4', '::'])
    bs = open(part, 'rb').read(64)
    fatsz = bs[22] | (bs[23] << 8)
    print('data partition: %d sectors, FAT size %d sectors' % (DATA_SECTORS, fatsz))
    if fatsz == 0:
        sys.exit('mformat did not create FAT16')

    with open(path, 'rb') as f:
        iso = bytearray(f.read())
    iso_lba = len(iso) // 512
    part_lba = iso_lba

    entry = bytearray(16)
    entry[0] = 0
    entry[1:4] = chs(part_lba)
    entry[4] = 0x0E
    entry[5:8] = chs(part_lba + DATA_SECTORS - 1)
    struct.pack_into('<I', entry, 8, part_lba)
    struct.pack_into('<I', entry, 12, DATA_SECTORS)
    iso[0x1CE:0x1DE] = entry

    iso.extend(open(part, 'rb').read())
    with open(path, 'wb') as f:
        f.write(iso)
    print('appended data partition at LBA %d, new image %d bytes' % (part_lba, len(iso)))


if __name__ == '__main__':
    main()
