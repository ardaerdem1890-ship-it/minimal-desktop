#pragma once
#include "types.h"

#define PIT_FREQUENCY 100   /* Hz */

void pit_init(void);
u32  pit_ticks(void);
u64  pit_uptime_ms(void);
u32  pit_seconds(void);
void pit_sleep_ms(u32 ms);
void pit_sleep_ticks(u32 ticks);
u32  pit_get_frequency(void);
bool pit_selftest(char *err, int errlen);
