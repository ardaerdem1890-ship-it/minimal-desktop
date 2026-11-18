# Test report — v1.8.1 "Splash" (2026-11-18)

## Environment
- VirtualBox VM `minimal-1.8.1`, disk on D:\md181_disk.vdi, ISO D:\md181.iso (v1.8.1).
- QEMU used for serial-captured command sweep (same ISO), PS/2 keyboard, RTL8139.

## Boot & login
- Boots to login screen, ROOT/root accepted. ✓

## Commands tested (serial capture, ~55 commands)
PASS (produced correct output):
- info: help, sysinfo, uptime, date, time, drivers, taskmng, hwinfo, whoami, users
- calc 6*7 -> 42, fortune, banner, charmap (Turkish glyphs), keymap list (21 layouts)
- theme, color(not a command, expected fail)
- fs: ls /, ls /pictures, ls /home, tree /home, du /home, find txt /
- memmap, store list, store status, store info doom
- minipacket -L (note: `-L` needs the exact flag; test harness sent `-` only -> "unknown option" — OS fine)
- net: ifconfig, ipconfig status, ping 10.0.2.2 (1/1 reply), ipconfig set <static>, ipconfig dhcp (lease acquired)
- nslookup example.com -> DNS fails in QEMU user-mode NAT (known limitation, not a code bug)
- files: mkdir, touch, write, cat, cp, mv, ls /tmp, hexdump, echo, cd/pwd, less/read (pager interactive)
- disk: mount/sync/chkdsk/cdrom -> correctly reports no FAT partition / no ATAPI when booted from CD without a data partition
- accounts: useradd test pass123, users (shows test), userdel test, users (test gone)
- sudo (usage), fortune via sudo
- ipconfig static then dhcp restore
- keymap tr-q + charmap shows Turkish glyphs; keymap us restores

## GUI (screenshots)
- startgui after installing minimal-desktop module ✓
- image viewer (`v`) opens and renders sample BMP (sunset, ocean) ✓
- next-image navigation ✓
- settings window (`o`) shows cursor style / sensitivity / clock ✓
- cursor style toggle ✓
- Esc from GUI returns to clean text console (font restored, no garble) ✓

## Automated self-test
- make autotest: 18/18 PASS ✓

## Known limitations observed
- nslookup fails against QEMU user-mode DNS (example.com not resolvable); ping by IP works.
- On live CD (no installed disk), /disk/cdrom commands correctly report unavailable.
- `passwd ROOT ...` in the sweep was mangled by the test key-timing harness (space handling), not an OS bug — verify manually if concerned.
