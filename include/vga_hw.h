#pragma once
#include "types.h"

/* Register-level VGA mode switching. No BIOS calls: works in protected mode. */
void vga_hw_set_text_mode(void);
void vga_hw_set_mode13h(void);
void vga_hw_set_text_cursor(int row, int col);
void vga_hw_enable_cursor(void);
void vga_hw_disable_cursor(void);
void vga_hw_text_clear(void);
void vga_hw_read_font(u8 *dst, u32 bytes);
void vga_hw_write_font(const u8 *src, u32 bytes);
