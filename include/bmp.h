#pragma once
#include "types.h"

/* Decode an 8-bit or 24-bit BMP into the 256-colour software framebuffer.
 * Returns 0 on success. */
int bmp_load(const u8 *data, u32 size, u8 *out, int out_w, int out_h);
int bmp_sample_images(void);
