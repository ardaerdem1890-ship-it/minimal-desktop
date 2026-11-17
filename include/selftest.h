#pragma once
#include "types.h"

int selftest_run(bool autotest);
int selftest_last_failures(void);
int selftest_last_passes(void);
bool selftest_did_run(void);
