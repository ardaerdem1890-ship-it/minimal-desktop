#pragma once
#include "types.h"

bool iso9660_mount(void);
bool iso9660_present(void);
void iso9660_info(void);
bool iso9660_selftest(char *err, int errlen);
