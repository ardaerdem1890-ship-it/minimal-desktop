/* Minimal Desktop - PCI bus enumeration (legacy 0xCF8/0xCFC mechanism) */
#include "pci.h"
#include "io.h"
#include "console.h"

static u32 pci_address(u8 bus, u8 slot, u8 func, u8 offset) {
    return 0x80000000u |
           ((u32)bus << 16) |
           ((u32)(slot & 0x1F) << 11) |
           ((u32)(func & 0x07) << 8) |
           (offset & 0xFC);
}

u32 pci_config_read32(u8 bus, u8 slot, u8 func, u8 offset) {
    outl(0xCF8, pci_address(bus, slot, func, offset));
    return inl(0xCFC);
}

u16 pci_config_read16(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 v = pci_config_read32(bus, slot, func, offset);
    return (u16)((v >> ((offset & 2) * 8)) & 0xFFFF);
}

u8 pci_config_read8(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 v = pci_config_read32(bus, slot, func, offset);
    return (u8)((v >> ((offset & 3) * 8)) & 0xFF);
}

void pci_config_write32(u8 bus, u8 slot, u8 func, u8 offset, u32 value) {
    outl(0xCF8, pci_address(bus, slot, func, offset));
    outl(0xCFC, value);
}

void pci_config_write16(u8 bus, u8 slot, u8 func, u8 offset, u16 value) {
    u32 old = pci_config_read32(bus, slot, func, offset & 0xFC);
    u32 shift = (offset & 2) * 8;
    old &= ~(0xFFFFu << shift);
    old |= (u32)value << shift;
    pci_config_write32(bus, slot, func, offset & 0xFC, old);
}

bool pci_find_device(u16 vendor, u16 device, pci_device_t *out) {
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                u16 v = pci_config_read16((u8)bus, (u8)slot, (u8)func, 0x00);
                if (v == 0xFFFF) {
                    if (func == 0) break;
                    continue;
                }
                u16 d = pci_config_read16((u8)bus, (u8)slot, (u8)func, 0x02);
                if (v == vendor && d == device) {
                    if (out) {
                        out->bus = (u8)bus;
                        out->slot = (u8)slot;
                        out->func = (u8)func;
                        out->vendor = v;
                        out->device = d;
                        out->class_code = pci_config_read8((u8)bus, (u8)slot, (u8)func, 0x0B);
                        out->subclass = pci_config_read8((u8)bus, (u8)slot, (u8)func, 0x0A);
                        out->prog_if = pci_config_read8((u8)bus, (u8)slot, (u8)func, 0x09);
                        out->irq = pci_config_read8((u8)bus, (u8)slot, (u8)func, 0x3C);
                        out->bar0 = pci_config_read32((u8)bus, (u8)slot, (u8)func, 0x10);
                        out->bar1 = pci_config_read32((u8)bus, (u8)slot, (u8)func, 0x14);
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

bool pci_find_class(u8 class_code, u8 subclass, pci_device_t *out, int index) {
    int found = 0;
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                u16 v = pci_config_read16((u8)bus, (u8)slot, (u8)func, 0x00);
                if (v == 0xFFFF) {
                    if (func == 0) break;
                    continue;
                }
                u8 cc = pci_config_read8((u8)bus, (u8)slot, (u8)func, 0x0B);
                u8 sc = pci_config_read8((u8)bus, (u8)slot, (u8)func, 0x0A);
                if (cc == class_code && sc == subclass) {
                    if (found++ < index) continue;
                    if (out) {
                        out->bus = (u8)bus;
                        out->slot = (u8)slot;
                        out->func = (u8)func;
                        out->vendor = v;
                        out->device = pci_config_read16((u8)bus, (u8)slot, (u8)func, 0x02);
                        out->class_code = cc;
                        out->subclass = sc;
                        out->irq = pci_config_read8((u8)bus, (u8)slot, (u8)func, 0x3C);
                        out->bar0 = pci_config_read32((u8)bus, (u8)slot, (u8)func, 0x10);
                        out->bar1 = pci_config_read32((u8)bus, (u8)slot, (u8)func, 0x14);
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

void pci_enable_bus_mastering(const pci_device_t *dev) {
    u16 cmd = pci_config_read16(dev->bus, dev->slot, dev->func, 0x04);
    cmd |= 0x0004;   /* bus master */
    cmd |= 0x0002;   /* memory space */
    pci_config_write16(dev->bus, dev->slot, dev->func, 0x04, cmd);
}

void pci_enable_io(const pci_device_t *dev) {
    u16 cmd = pci_config_read16(dev->bus, dev->slot, dev->func, 0x04);
    cmd |= 0x0001;
    pci_config_write16(dev->bus, dev->slot, dev->func, 0x04, cmd);
}

int pci_device_count(void) {
    int n = 0;
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            for (int func = 0; func < 8; func++) {
                u16 v = pci_config_read16((u8)bus, (u8)slot, (u8)func, 0x00);
                if (v == 0xFFFF) {
                    if (func == 0) break;
                    continue;
                }
                n++;
            }
        }
    }
    return n;
}
