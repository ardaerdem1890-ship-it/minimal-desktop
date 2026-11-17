#!/usr/bin/env python3
"""Build a complete, bootable raw disk image used by the `install` command.

Layout (512-byte sectors):
    0            GRUB boot.img MBR (patched: kernel sector = 1, drive = 0x80)
    1..          core.img (diskboot + kernel modules), contiguous
    2048..6143   FAT12 partition (1 MiB .. 3 MiB) holding /boot

The kernel does not have to understand FAT or GRUB: it just streams this image
sector by sector onto the target ATA disk with PIO writes.

Requires: grub-mkimage, mtools (mformat/mmd/mcopy), python3.
"""
import io
import os
import shutil
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, 'build')
KERNEL = os.path.join(BUILD, 'kernel.elf')
STORE = os.path.join(BUILD, 'store.img')
OUT = os.path.join(BUILD, 'install_disk.img')
TMP = os.path.join(BUILD, 'installtmp')

PART_LBA = 2048
PART_SECTORS = 4096          # 2 MiB FAT12 volume
DISK_SECTORS = PART_LBA + PART_SECTORS

GRUB_MODULES = ('biosdisk part_msdos fat normal configfile echo test linux '
                'boot search search_fs_file search_label').split()

GRUB_CFG = """set timeout=3
set default=0

menuentry "Minimal Desktop (installed)" {
    multiboot2 /boot/kernel.elf
    module2 /boot/store.img store-repository
    boot
}

menuentry "Minimal Desktop (safe text mode)" {
    multiboot2 /boot/kernel.elf --safe
    module2 /boot/store.img store-repository
    boot
}
"""


def run(cmd, **kw):
    print('+', ' '.join(cmd))
    subprocess.run(cmd, check=True, **kw)


def main():
    if not os.path.exists(KERNEL):
        sys.exit('missing %s (run make first)' % KERNEL)
    if shutil.which('grub-mkimage') is None:
        sys.exit('grub-mkimage not found')
    if shutil.which('mformat') is None:
        sys.exit('mtools not found')

    if os.path.isdir(TMP):
        shutil.rmtree(TMP)
    os.makedirs(os.path.join(TMP, 'boot', 'grub'))

    part = os.path.join(TMP, 'part.img')
    run(['mformat', '-i', part, '-C', '-T', str(PART_SECTORS),
         '-h', '16', '-s', '32', '-c', '4', '::'])
    run(['mmd', '-i', part, '::/boot', '::/boot/grub'])
    run(['mcopy', '-i', part, KERNEL, '::/boot/kernel.elf'])
    if os.path.exists(STORE):
        run(['mcopy', '-i', part, STORE, '::/boot/store.img'])

    cfg_path = os.path.join(TMP, 'grub.cfg')
    io.open(cfg_path, 'w', encoding='utf-8').write(GRUB_CFG)
    run(['mcopy', '-i', part, cfg_path, '::/boot/grub/grub.cfg'])

    core = os.path.join(TMP, 'core.img')
    run(['grub-mkimage', '-O', 'i386-pc',
         '-p', '(hd0,1)/boot/grub', '-o', core] + GRUB_MODULES)

    boot_img = None
    for cand in ('/usr/lib/grub/i386-pc/boot.img',
                 '/usr/share/grub/i386-pc/boot.img'):
        if os.path.exists(cand):
            boot_img = open(cand, 'rb').read(512)
            break
    if boot_img is None:
        sys.exit('boot.img not found')

    mbr = bytearray(boot_img)
    # GRUB_BOOT_MACHINE_KERNEL_SECTOR (0x5C) is already 1 in stock boot.img, but
    # write it explicitly so the image is correct even if GRUB changes defaults.
    # Do NOT touch 0x64/0x65/0x66: 0x65 is the `cli` the entry jump lands on and
    # 0x66 is the drive-check stub -- patching them silently kills the MBR.
    struct.pack_into('<I', mbr, 0x5C, 1)

    # One primary partition, bootable, LBA type 0x0E (FAT12/FAT16 LBA).
    # GRUB's biosdisk falls back to CHS when the BIOS reports odd geometry, so
    # the CHS fields must be plausible -- 0xFEFFFF makes it abort with
    # "error: hd0 cannot get C/H/S values".
    def chs(lba):
        cyl = lba // (16 * 32)
        rem = lba % (16 * 32)
        head = rem // 32
        sector = rem % 32 + 1
        return bytes([head, sector | ((cyl >> 2) & 0xC0), cyl & 0xFF])

    entry = bytearray(16)
    entry[0] = 0x80                                  # active
    entry[1:4] = chs(PART_LBA)
    entry[4] = 0x0E                                  # type
    entry[5:8] = chs(PART_LBA + PART_SECTORS - 1)
    struct.pack_into('<I', entry, 8, PART_LBA)
    struct.pack_into('<I', entry, 12, PART_SECTORS)
    mbr[446:462] = entry
    mbr[510:512] = b'\x55\xAA'

    core_data = open(core, 'rb').read()
    if len(core_data) > (PART_LBA - 1) * 512:
        sys.exit('core.img too large for the gap before the partition')
    part_data = open(part, 'rb').read()
    if len(part_data) > PART_SECTORS * 512:
        sys.exit('filesystem image does not fit in the partition window')

    disk = bytearray(DISK_SECTORS * 512)
    disk[0:512] = mbr
    disk[512:512 + len(core_data)] = core_data
    off = PART_LBA * 512
    disk[off:off + len(part_data)] = part_data

    with open(OUT, 'wb') as f:
        f.write(disk)
    print('install disk: %s (%d bytes, %d sectors)' %
          (OUT, len(disk), DISK_SECTORS))


if __name__ == '__main__':
    main()
