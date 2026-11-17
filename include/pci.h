#pragma once
#include "types.h"

typedef struct {
    u8  bus;
    u8  slot;
    u8  func;
    u16 vendor;
    u16 device;
    u8  class_code;
    u8  subclass;
    u8  prog_if;
    u8  irq;
    u32 bar0;
    u32 bar1;
} pci_device_t;

u32 pci_config_read32(u8 bus, u8 slot, u8 func, u8 offset);
u16 pci_config_read16(u8 bus, u8 slot, u8 func, u8 offset);
u8  pci_config_read8(u8 bus, u8 slot, u8 func, u8 offset);
void pci_config_write32(u8 bus, u8 slot, u8 func, u8 offset, u32 value);
void pci_config_write16(u8 bus, u8 slot, u8 func, u8 offset, u16 value);
bool pci_find_device(u16 vendor, u16 device, pci_device_t *out);
bool pci_find_class(u8 class_code, u8 subclass, pci_device_t *out, int index);
void pci_enable_bus_mastering(const pci_device_t *dev);
void pci_enable_io(const pci_device_t *dev);
int  pci_device_count(void);
