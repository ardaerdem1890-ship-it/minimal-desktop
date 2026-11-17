#pragma once
#include "types.h"

bool usb_init(void);
bool usb_present(void);
bool usb_read_sector(u32 lba, u8 *buf);
bool usb_selftest(char *err, int errlen);
