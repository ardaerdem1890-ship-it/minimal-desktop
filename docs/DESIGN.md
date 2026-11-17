# Minimal Desktop - design notes

## Boot chain

1. `grub-mkrescue` ISO with `boot/grub.cfg`:
   - `Minimal Desktop` (default) - interactive
   - `Minimal Desktop --autotest` - `--autotest`
   - `Minimal Desktop --safe` - `--safe`
2. `boot/boot.asm` -> GRUB Multiboot2 handoff, sets up GDT/IDT/PIC/PIT/RAMFS/store.
3. Modules: kernel, store.img.

## Selftest contract

- `kernel/selftest.c` runs 18 checks; PASS/FAIL printed to VGA and COM1.
- On completion writes `0x10` (PASS) or `0x11` (FAIL) to the isa-debug-exit port
  `0x501` (8-bit, iosize 4), QEMU device `isa-debug-exit,iobase=0x501,iosize=0x04`.
- QEMU exits 33 on guest exit 0, 35 on guest exit 1; `make autotest` maps to
  shell exit 0 (PASS) / 1 (FAIL).

## Network RTL8139 notes

- PCI device 10EC:8139, I/O access (BAR0), IRQ from PCI.
- Register map (offsets from I/O base): MAC 0x00, MAR 0x08, TxStatus 0x10,
  TxAddr 0x20, RxBuf 0x30, ChipCmd 0x37, RxBufPtr 0x38, RxBufAddr 0x3A,
  IntrMask 0x3C, IntrStatus 0x3E, TxConfig 0x40, RxConfig 0x44, Timer 0x48,
  RxMissed 0x4C, ...
- Receive: RxConfig must include the "accept" bits (unicast to us, broadcast,
  multicast, runt, error) plus ring size / no-wrap. Receiving unicast frames
  requires AcceptMyPhys/AcceptAllPhys - without them the card filters them out.
- Ring: 8K receive ring, no wrap; descriptors consumed via RxBufPtr/RxBufAddr.
- Polled RX (no NIC IRQ): `net_poll()` drains via `rtl8139_receive()`.
- Tx: 4-slot ring, own-bit handling via TxStatus.

## Wire protocol

- Ethernet II: dst[6] src[6] ethertype(BE16).
- ARP: hwtype/proto/op (BE16), sha[6]/spa/tha[6]/tpa (BE32).
- IPv4: ver_ihl/tot_len/id/frag/ttl/proto/checksum (BE16 fields), src/dst (BE32).
- ICMP/UDP/TCP: BE16/BE32 fields; checksums as computed by the stack.
- Address values kept in `NET_IP(a,b,c,d)` byte order internally.

## Store format

See README ("Store") for `MDSTORE1` / `MDMOD1` layout and FNV-1a checksums.
