#pragma once
#include "types.h"

/* Enumerate PCI and describe detected hardware (GPU, NIC, VM platform). */
void hwinfo_print(void);
const char *hwinfo_vm_platform(void);
const char *hwinfo_gpu_name(void);
const char *hwinfo_nic_name(void);
