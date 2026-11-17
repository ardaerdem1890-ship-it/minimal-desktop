# Handoff note (2026-11-17)

## Current state
- On `main`, latest commit = v1.6.0 "Users" (login screen, ROOT/root live CD, sudo, user accounts).
- Working tree has UNCOMMITTED fixes that are built into the ISO but not yet committed/pushed.

## Uncommitted fixes (all verified working)
1. **GUI image viewer** — was broken; fixed:
   - `gui/gui.c`: start-menu click rejected item 11 (`item > 10`); changed to `> 12`. Added Settings item 12. Added plain-key shortcuts: `v` opens image viewer, `o` opens settings (Ctrl+letter gets translated to control codes in the keyboard driver, so Ctrl+I never matched).
   - `drivers/bmp.c`: BMP header written wrong — height only 2 of 4 bytes, bpp at offset 26 instead of 28. Also pixel generators truncated packed RGB through `(u8)`. All fixed.
   - Verified in QEMU: image viewer opens and renders `sunset.bmp` correctly (screenshot build/shots/ivfinal.png).
2. **GUI mouse cursor settings + window resizing** — added:
   - Cursor styles (arrow/cross/block), sensitivity, clock toggle in a Settings window (`o` key / start menu).
   - Drag window edges/corners to resize (resize_hit/apply_resize).
3. **GUI exit to text** — verified clean in QEMU (font restore working).

## Not yet done
- Bump version to 1.7.0, update versions.json / README / website, rebuild final ISO, push GitHub release v1.7.0 with new ISO.
- Commit + push the uncommitted fixes.
- The VirtualBox `minimal-1.6.0` VM has the OLD ISO (before these fixes); attach a fresh one if testing again.

## Quick resume
```bash
cd '/mnt/d/Varsayılan Proje/minimal-desktop'
git add -A && git commit -m "Fix image viewer, add cursor settings and window resizing"
git push origin main
make autotest                 # must be 18/18 PASS
# bump OS_VERSION in include/console.h to 1.7.0, make clean && make iso
# update versions.json + README, then tools/upload via gh token
```

## Known quirks
- `make clean` deletes build/gh_token.txt; recover from git remote URL:
  `git remote get-url origin` contains `x-access-token:<token>@...`.
- QEMU monitor socket (unix) sometimes fails; `tools/vm_shot.py` uses it; FIFO fallback in `gui_shot.sh`.
- VirtualBox e1000 DHCP fails (PCnet works). VM `minimal-1.6.0` uses PCnet.
- Header changes don't trigger recompile (no dependency tracking) — run `make clean` after version bumps.
