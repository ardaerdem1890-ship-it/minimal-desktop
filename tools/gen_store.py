#!/usr/bin/env python3
"""Generate the embedded Minimal Store repository.

Outputs:
  build/store.img          Multiboot2 module (MDSTORE1 archive)
  website/store/*.mdp      individual verified module images for HTTP install
"""
import argparse
import os
import struct
import sys

FNV_OFFSET = 0x811C9DC5
FNV_PRIME = 0x01000193


def fnv1a(data: bytes) -> int:
    h = FNV_OFFSET
    for b in data:
        h ^= b
        h = (h * FNV_PRIME) & 0xFFFFFFFF
    return h


def pad16(data: bytes) -> bytes:
    rem = len(data) % 16
    return data + (b"\x00" * (16 - rem) if rem else b"")


PACKAGES = [
    {
        "name": "snake",
        "version": "1.2.0",
        "category": "game",
        "desc": "Classic snake with score tracking, pause and keyboard steering",
        "entry": 1,
        "body": (
            "Minimal Desktop module: snake\n"
            "Entry point  : MOD_ENTRY_SNAKE (1)\n"
            "Runtime      : built into the kernel, activated by the store manifest.\n"
            "Controls     : arrow keys steer, P pauses, ESC returns to the shell.\n"
            "Scoring      : 10 points per apple, speed increases after 10 apples.\n"
            "Persistence  : high score is kept for the current boot session.\n"
            "Graphics     : VGA mode 13h, 320x200, 8x8 cell grid.\n"
            "Verification : FNV-1a checksum over this module body.\n"
        ),
    },
    {
        "name": "pong",
        "version": "1.1.0",
        "category": "game",
        "desc": "Pong against a CPU paddle, first player to seven points wins",
        "entry": 2,
        "body": (
            "Minimal Desktop module: pong\n"
            "Entry point  : MOD_ENTRY_PONG (2)\n"
            "Runtime      : built into the kernel, activated by the store manifest.\n"
            "Controls     : up/down arrows or W/S move the left paddle, ESC exits.\n"
            "Opponent     : simple tracking AI with a bounded paddle speed.\n"
            "Match        : first to seven points, R restarts after the result.\n"
            "Graphics     : VGA mode 13h, 320x200, software compositor.\n"
            "Verification : FNV-1a checksum over this module body.\n"
        ),
    },
    {
        "name": "minimal-desktop",
        "version": "1.0.0",
        "category": "desktop",
        "desc": "Composited 320x200 desktop with terminal, notepad, tasks and network",
        "entry": 3,
        "body": (
            "Minimal Desktop module: minimal-desktop\n"
            "Entry point  : MOD_ENTRY_DESKTOP (3)\n"
            "Runtime      : built into the kernel, activated by the store manifest.\n"
            "Features     : draggable windows, taskbar, start menu, clock, cursor.\n"
            "Applications : terminal, notepad, task manager, network/browser panel.\n"
            "Input        : PS/2 mouse (IRQ12) and keyboard (IRQ1).\n"
            "Exit         : ESC or Start menu > Exit GUI returns to the text shell.\n"
            "Graphics     : VGA mode 13h with a custom 20-colour palette.\n"
            "Verification : FNV-1a checksum over this module body.\n"
        ),
    },
    {
        "name": "doom",
        "version": "0.9.0",
        "category": "game",
        "desc": "Bare-metal 2.5D raycasting FPS (DOOM-style) in VGA mode 13h",
        "entry": 4,
        "body": (
            "Minimal Desktop module: doom\n"
            "Entry point  : MOD_ENTRY_DOOM (4)\n"
            "Runtime      : built into the kernel, activated by the store manifest.\n"
            "Launch       : run 'doom' from shell or open from Desktop after install.\n"
            "Controls     : WASD move, arrows/mouse turn, space fire, ESC quit.\n"
            "Features     : textured raycast walls, enemies, HUD, PC speaker SFX.\n"
            "Verification : FNV-1a checksum over this module body.\n"
        ),
    },
    {
        "name": "miniland",
        "version": "0.1.0",
        "category": "desktop",
        "desc": "Miniland: Hyprland-inspired tiling desktop/compositor for VGA mode 13h",
        "entry": 5,
        "body": (
            "Minimal Desktop module: miniland\n"
            "Entry point  : MOD_ENTRY_MINILAND (5)\n"
            "Runtime      : built into the kernel, activated by the store manifest.\n"
            "Launch       : run 'miniland' from shell after install.\n"
            "Features     : tiling layout, gaps, workspaces, top bar, keyboard focus.\n"
            "Controls     : Alt+1-4 workspaces, Alt+Enter terminal, Alt+Q close, ESC exit.\n"
            "Style        : Hyprland-inspired dynamic compositor on bare-metal VGA.\n"
            "Verification : FNV-1a checksum over this module body.\n"
        ),
    },
]


def build_body(spec: dict) -> bytes:
    body = spec["body"].encode("ascii")
    # Repeat the description until the module has a healthy payload size.
    while len(body) < 480:
        body += spec["body"].encode("ascii")
    return body[:512]


def module_image(spec: dict) -> bytes:
    body = build_body(spec)
    name = spec["name"].encode("ascii")[:15].ljust(16, b"\x00")
    header = struct.pack(
        "<8sIIII16s",
        b"MDMOD1\x00\x00",
        1,
        spec["entry"],
        len(body),
        fnv1a(body),
        name,
    )
    return header + body


def archive(packages) -> bytes:
    entries = []
    blobs = []
    header_size = struct.calcsize("<8sIIII")
    entry_size = struct.calcsize("<16s12s12s64sIIII")
    offset = header_size + entry_size * len(packages)
    offset = (offset + 15) & ~15

    cursor = offset
    for spec in packages:
        image = module_image(spec)
        blobs.append((cursor, image))
        entries.append(
            struct.pack(
                "<16s12s12s64sIIII",
                spec["name"].encode("ascii")[:15],
                spec["version"].encode("ascii")[:11],
                spec["category"].encode("ascii")[:11],
                spec["desc"].encode("ascii")[:63],
                spec["entry"],
                len(image),
                fnv1a(image),
                cursor,
            )
        )
        cursor = (cursor + len(image) + 15) & ~15

    total_size = cursor
    entries_blob = b"".join(entries)
    header = struct.pack(
        "<8sIIII",
        b"MDSTORE1",
        1,
        len(packages),
        total_size,
        fnv1a(entries_blob),
    )

    out = bytearray(total_size)
    out[0:len(header)] = header
    out[header_size:header_size + len(entries_blob)] = entries_blob
    for off, image in blobs:
        out[off:off + len(image)] = image
    return bytes(out)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="build/store.img")
    ap.add_argument("--web", default="website/store")
    args = ap.parse_args()

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    os.makedirs(args.web, exist_ok=True)

    for spec in PACKAGES:
        image = module_image(spec)
        path = os.path.join(args.web, spec["name"] + ".mdp")
        with open(path, "wb") as f:
            f.write(image)
        print("  module %-16s %4d bytes  fnv=%08x  -> %s"
              % (spec["name"], len(image), fnv1a(image), path))

    data = archive(PACKAGES)
    with open(args.out, "wb") as f:
        f.write(data)
    print("archive %-18s %4d bytes  entries=%d  -> %s"
          % ("store.img", len(data), len(PACKAGES), args.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
