# Minimal Desktop

A complete, from-scratch x86 hobby operating system: 32-bit protected mode,
GRUB Multiboot2 boot, VGA console + graphics, PS/2 keyboard/mouse, PIT, ATA/IDE
RAM filesystem, a command shell, a built-in software store, a GUI desktop, and
a full Ethernet stack (ARP/ICMP/UDP/DHCP/DNS/TCP/HTTP) with drivers for the
RTL8139, AMD PCnet and Intel PRO/1000 NICs.

## What's new in 1.4.0 "Nova"

- **Crash screen that can actually be read.** A fault while the GUI is running
  now restores the text-mode registers, the 8 KiB character generator in VGA
  plane 2 and the DAC palette before drawing the `SYSTEM FAULT` frame, then
  counts down five seconds and resets via i8042, the 0xCF9 reset register and a
  triple fault. `crash` and `crash gui` provoke a fault on purpose for testing.
- **Quitting the GUI no longer garbles the console.** The font, palette and
  cursor are snapshotted on `gfx_enter()` and restored on `gfx_leave()`.
- **Keyboard decoding fix.** Enter, Backspace, Tab and Esc were lost when the
  layout table replaced the old scancode switch; they are back.
- **21 keyboard layouts** (`us uk de fr be es it pt nl se no dk fi pl cz hu ro
  tr-q tr-f az br`), switched with `Alt+Shift`, the taskbar chip or
  `keymap list|next|prev|<id>`.
- **Terminal editor and pager**: `edit <file>` (F2 saves, Esc exits) and
  `less <file>` / `read <file>`.
- **Intel PRO/1000 (e1000) driver** for VirtualBox's "Intel PRO/1000 MT
  Desktop/Server" NICs, next to the existing RTL8139 and PCnet drivers.
- **New boot wordmark**: a 12-band white -> cyan -> blue `MINIMAL OS` built from
  half-block characters by `tools/gen_bootart.py`.
- `minipacket -L` lists packages; `-I/-R/-S` map to install/remove/info.
- **Disk installer**: `install` streams the embedded isohybrid image to the
  first ATA disk (with confirmation and readback of sector 0). The build embeds
  a copy of the base ISO as the `install-source` Multiboot2 module; the resulting
  ISO is ~23 MiB, of which ~12 MiB is the image that gets written to the disk.

## Quick start (one click)

```bash
# in the WSL Ubuntu toolchain, as root:
sudo apt-get install -y build-essential gcc-i686-linux-gnu nasm qemu-system-x86 \
    grub-pc-bin grub-common xorriso mtools python3 dosfstools curl
sudo tools/install_toolchain.sh

cd /mnt/d/Varsayilan Proje/minimal-desktop
chmod +x build_and_run.sh build_and_run.bat
./build_and_run.sh        # or: build_and_run.bat
```

`build_and_run.sh` / `build_and_run.bat`:

1. `make -j4`          -> builds `build/minimal_desktop.iso`
2. `make autotest`      -> headless selftest in QEMU (isa-debug-exit at 0x501:
                           0x10 = PASS / 0x11 = FAIL; QEMU exit 33 = PASS, 35 = FAIL)
3. `make run`           -> `qemu-system-i386 -cdrom build/minimal_desktop.iso -m 256M \
                            -serial stdio -netdev user,id=net0 \
                            -device rtl8139,netdev=net0`

Outputs:

- `build/minimal_desktop.iso`          (12 MB, GRUB Multiboot2, 5861 sectors)
- `build/minimal_desktop_autotest.iso` (headless `--autotest` variant)
- `D:\minimal_desktop.iso`             (published copy for the website download button)

## Manual build / test

```bash
make            # build build/minimal_desktop.iso
make iso        # build + publish to website/downloads/ and D:\minimal_desktop.iso
make autotest   # build + run headless selftest, exit 0 on PASS, 1 on FAIL
make run        # boot the ISO in QEMU
make check      # compile-only check of all C sources
make clean      # remove build output
```

Autotest result contract: all 18 checks must PASS, then

```
[selftest] RESULT: PASS
[autotest] writing isa-debug-exit at 0x501 (0x10)
```

and QEMU exits **33** (mapped to 0 = pass) when running via `make autotest`.

## Boot menu

`boot/grub.cfg` provides three entries:

| Entry | Kernel cmdline | Purpose |
|---|---|---|
| `Minimal Desktop` | *(default)* | interactive shell + GUI (`startgui`) |
| `Minimal Desktop --autotest` | `--autotest` | non-interactive selftest |
| `Minimal Desktop --safe` | `--safe` | safe mode (no store install / no network activity) |

## Shell commands

```
help  sysinfo  uptime  drivers  taskmng  selftest  clear  reboot  shutdown
date  time  ls  cd  mkdir  touch  rm  cat  write  echo  memmap  hexdump
edit  less  read  keymap  hwinfo  crash  install
calc  theme  matrix  beep  credits
ifconfig  ping  nslookup  curl
store  minipacket  snake  pong  doom  startgui  miniland
```

Prompt: `minimal@os:~$`

## Store

`MDSTORE1` archive: header `{magic[8],version,count,total_size,checksum}` followed
by entries `{name[16],version[12],category[12],desc[64],entry,size,checksum,offset}`.
Module body: `MDMOD1` + `{version,entry,body_size,body_checksum,name[16]}` + body.
Checksums are FNV-1a. Generated with `tools/gen_store.py` from `website/store/*.mdp`.

## Networking

Two NIC drivers are included and auto-detected: **RTL8139** (PCI 10EC:8139)
and **AMD PCnet** (PCI 1022:2000 / 1022:2020, the Am79C973 VirtualBox/QEMU
default). Both use polled DMA/descriptor rings. `hwinfo` reports which one is
active. RTL8139 is tried first; PCnet is used when no RTL8139 is present.

RTL8139 (PCI 10EC:8139) driver at I/O base from PCI BAR0, IRQ from PCI config.
QEMU launch:

```bash
qemu-system-i386 -cdrom build/minimal_desktop.iso -m 256M -serial stdio \
  -netdev user,id=net0 -device rtl8139,netdev=net0 \
  -device isa-debug-exit,iobase=0x501,iosize=0x04
```

Stack: Ethernet II -> ARP -> IPv4 -> ICMP/UDP/DHCP/DNS/TCP. All multi-byte
protocol fields are big-endian on the wire; in-memory address values use the
`NET_IP(a,b,c,d)` byte order.

The GUI Network Connector (open `Network` from the desktop, start menu or
Miniland) shows link/IP/MAC/DHCP/Traffic and provides ping, DHCP renew,
interface info and DNS lookup actions via keys `P/R/I/D` or clickable on-screen
buttons.

The boot banner is generated from ANSI art by `tools/gen_bootart.py` into
`include/bootart.h` and drawn directly into the VGA text buffer on startup.

Stability: keyboard handling now swallows the Pause-key (E1) tail, console
writes are bounds-clamped, and leaving a graphics app restores the standard
VGA text palette so colors stay correct after returning to the shell. CPU
faults in graphics mode switch back to text mode and print register details.

## Desktop, Miniland and games

- **GUI** (`startgui`): double-buffered 320x200x256 compositor with bitmap
  icons, rendered gradient wallpaper, movable/snapping windows, `Alt+Tab`,
  start menu (**Super** key or taskbar button), screenshot (`PrtSc`), tray
  controls and keyboard-layout switcher (`us`, `tr-q`, `uk`, `de`).
- **Miniland** (`miniland`): Hyprland-inspired tiling compositor with master/
  stack layout, gaps, 4 workspaces, top bar, focus cycling, mouse focus,
  **movable/resizable floating windows** (drag title bar / bottom-right grip),
  drop shadows, and a **Super-key start menu**. Tiling controls use
  `Alt+Enter/B/E/N/Q/1-4/J/K/Tab/F`; `Esc` exits.
- **DOOM** (`doom`): enhanced 2.5D raycaster with textured/shaded walls,
  a floor/ceiling gradient, billboard monster sprites with chase AI and health
  bars, health/armor pickups, minimap, damage flash, animated HUD, sprint
  (`Shift`) and mouse look/fire.
- **Store packages** (install with `minipacket -İ <name>`): `snake`, `pong`,
  `minimal-desktop`, `doom`, `miniland`.

## Hardware detection

`hwinfo` enumerates PCI and identifies GPUs (NVIDIA, Intel, AMD/ATI), the VM
platform (VMware, VirtualBox, QEMU/KVM/VirtIO) and network controllers. Full
3D GPU acceleration and paravirtual VMware/VirtualBox drivers are not
implemented; the universally compatible VGA core (text 80x25 and mode 13h)
is used instead. `drivers` prints the loaded-driver table.

If the system ever hits a CPU fault while in graphics mode (which could look
like a freeze or garbled screen), the exception handler now switches back to
80x25 text mode and prints exception, register and fault-address details so
the cause is visible.

## Website

`website/` is a static site (index.html, styles.css, script.js) served by any
static file server. The download button (`#download-iso`, `#download-iso-2`) points
at `downloads/minimal_desktop.iso` and does a `HEAD` check; once the file is built
the button becomes active and shows the size.

## Docs

- `docs/` - design / build notes
- source: `boot/`, `kernel/`, `drivers/`, `fs/`, `net/`, `gui/`
- `tools/` - toolchain installer, compile check, store generator, run_test.sh
## Complete command reference

Every command registered in `kernel/shell.c` (48 total), grouped by category.

### Clock

| Command | Usage | Description |
|---|---|---|
| `date` | `date` | show the current date |
| `time` | `time` | show the current time |

### Files

| Command | Usage | Description |
|---|---|---|
| `cat` | `cat <file>` | print a file |
| `cd` | `cd [path]` | change directory |
| `df` | `df` | filesystem usage |
| `echo` | `echo <text>` | print text |
| `edit` | `edit <file>` | full-screen text editor |
| `hexdump` | `hexdump <file> [bytes]` | hex + ASCII dump of a file |
| `less` | `less <file>` | scroll through a text file |
| `ls` | `ls [path]` | list a directory |
| `memmap` | `memmap` | physical memory map and heap stats |
| `mkdir` | `mkdir <dir>` | create a directory |
| `pwd` | `pwd` | print working directory |
| `read` | `read <file>` | alias for less |
| `rm` | `rm <path>` | remove a file or directory |
| `touch` | `touch <file>` | create an empty file |
| `write` | `write <file> <text>` | replace a file with text |

### Games

| Command | Usage | Description |
|---|---|---|
| `doom` | `doom` | DOOM raycaster (needs module) |
| `pong` | `pong` | pong vs CPU (needs module) |
| `snake` | `snake` | classic snake (needs module) |

### Graphics

| Command | Usage | Description |
|---|---|---|
| `miniland` | `miniland` | Miniland tiling WM (needs module) |
| `startgui` | `startgui` | graphical desktop (needs module) |

### Network

| Command | Usage | Description |
|---|---|---|
| `curl` | `curl <http://url>` | fetch an HTTP resource |
| `ifconfig` | `ifconfig` | interface configuration |
| `netstat` | `netstat` | network statistics |
| `nslookup` | `nslookup <host>` | resolve a DNS name |
| `ping` | `ping <ip\|host> [count]` | ICMP echo request |

### Packages

| Command | Usage | Description |
|---|---|---|
| `minipacket` | `minipacket -L \| -I <pkg> \| -R <pkg> \| -S <pkg>` | package manager (install: -I) |
| `store` | `store list\|install\|remove\|info\|repo\|status` | package manager |

### System

| Command | Usage | Description |
|---|---|---|
| `clear` | `clear` | clear the screen |
| `crash` | `crash` | deliberately fault (crash screen test) |
| `drivers` | `drivers` | list loaded drivers and devices |
| `help` | `help [command]` | list commands or show usage |
| `hwinfo` | `hwinfo` | detect PCI/GPU/NIC/VM hardware |
| `install` | `install` | write the OS image to the first ATA disk |
| `keymap` | `keymap list\|next\|<layout>` | switch keyboard layout (Alt+Shift) |
| `reboot` | `reboot` | restart the machine |
| `selftest` | `selftest [--autotest]` | run the built-in hardware tests |
| `shutdown` | `shutdown` | power off the machine (ACPI) |
| `sysinfo` | `sysinfo` | show OS, CPU, memory and device info |
| `taskmng` | `taskmng [kill <pid>]` | task manager |
| `uname` | `uname` | kernel and build identification |
| `uptime` | `uptime` | time since boot |

### Tools

| Command | Usage | Description |
|---|---|---|
| `beep` | `beep [Hz] [ms]` | PC speaker tone |
| `calc` | `calc <expression>` | integer calculator |
| `credits` | `credits` | about the project |
| `matrix` | `matrix [seconds]` | digital rain effect |
| `theme` | `theme <name\|fg> [bg]` | change console colours |

### Keyboard / GUI shortcuts

| Key | Action |
|---|---|
| `Alt+Shift` | cycle keyboard layouts (21 layouts) |
| `Super` | open/close the start menu |
| `Alt+Tab` | cycle windows |
| `Alt+Enter` | open a GUI terminal |
| `Ctrl+PrtSc` | save a BMP screenshot into RAMFS |
| `Esc` | close the active GUI window / leave Miniland |
| `F1` | toggle PC speaker mute |
| `Ctrl+Alt+Del` | reboot |

### Boot flags

| Flag | Meaning |
|---|---|
| `--autotest` | run the 18-check self test, report on COM1, exit via ISA debug port 0x501 |
| `--safe` | skip store install and network activity |
| `--installtest` | run the disk installer without the confirmation prompt |
