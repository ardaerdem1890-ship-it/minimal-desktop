#pragma once
#include "types.h"

bool atapi_init(void);
bool atapi_present(void);
bool atapi_read_sector(u32 lba, u8 *buf);
bool atapi_selftest(char *err, int errlen);
