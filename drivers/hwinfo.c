/* Minimal Desktop - PCI hardware detection / reporting
 *
 * Full 3D drivers for NVIDIA / Intel / AMD GPUs and paravirtual VMware /
 * VirtualBox devices are large undertakings. This module performs safe
 * enumeration and identification so the OS can report what is present and
 * choose compatible fallbacks (VGA mode 13h is universally available).
 */
#include "hwinfo.h"
#include "pci.h"
#include "console.h"
#include "string.h"

typedef struct { u16 vendor; const char *name; } vendor_t;

static const vendor_t vendors[] = {
    { 0x8086, "Intel" },
    { 0x10DE, "NVIDIA" },
    { 0x1002, "AMD/ATI" },
    { 0x1022, "AMD" },
    { 0x15AD, "VMware" },
    { 0x80EE, "VirtualBox" },
    { 0x1AF4, "VirtIO" },
    { 0x10EC, "Realtek" },
    { 0x1028, "Dell" },
    { 0x1033, "NEC" },
    { 0x106B, "Apple" },
    { 0x1179, "Toshiba" },
    { 0x0ECD, "QEMU" },
    { 0x0000, NULL }
};

static const char *vendor_name(u16 v) {
    for (int i = 0; vendors[i].name; i++)
        if (vendors[i].vendor == v) return vendors[i].name;
    return "unknown";
}

static const char *device_hint(u16 v, u16 d) {
    if (v == 0x10DE) {
        if (d <= 0x0FFF) return "GeForce/Quadro (VGA fallback)";
        return "NVIDIA GPU (VGA fallback)";
    }
    if (v == 0x8086) {
        if (d >= 0x1000 && d <= 0x3FFF) return "Intel GPU (VGA fallback)";
        if (d == 0x100E || d == 0x100F || d == 0x10D3) return "Intel PRO/1000 (e1000) - not yet bound";
        return "Intel device";
    }
    if (v == 0x1002 || v == 0x1022) return "AMD/ATI GPU - VGA fallback";
    if (v == 0x15AD) {
        if (d == 0x0405) return "VMware SVGA II (VGA compatible)";
        if (d == 0x0710) return "VMware VMXNET Ethernet - not yet bound";
        if (d == 0x07B0) return "VMware VMXNET3 - not yet bound";
        if (d == 0x0740) return "VMware virtual machine platform";
        return "VMware device";
    }
    if (v == 0x80EE) {
        if (d == 0xBEEF) return "VirtualBox Graphics Adapter (VGA compatible)";
        if (d == 0xCAFE) return "VirtualBox Guest Additions";
        return "VirtualBox device";
    }
    if (v == 0x1AF4) {
        if (d >= 0x1000 && d <= 0x100F) return "VirtIO device - not yet bound";
        return "VirtIO device";
    }
    if (v == 0x10EC && d == 0x8139) return "RTL8139 - driver active";
    if (v == 0x1022 && d == 0x2020) return "PCnet-FAST III - driver active";
    if (v == 0x1022 && d == 0x2000) return "PCnet-PCI II - driver active";
    return "device";
}

const char *hwinfo_vm_platform(void) {
    static char buf[64];
    pci_device_t dev;
    if (pci_find_device(0x15AD, 0x0740, &dev)) return "VMware";
    if ((pci_config_read16(0, 0, 0, 0) == 0x80EE) ||
        pci_find_device(0x80EE, 0xBEEF, &dev)) return "VirtualBox";
    if (pci_find_device(0x1AF4, 0x1000, &dev) ||
        pci_find_device(0x1AF4, 0x1001, &dev)) return "QEMU/KVM (VirtIO)";
    ksnprintf(buf, sizeof(buf), "bare metal / other");
    return buf;
}

const char *hwinfo_gpu_name(void) {
    static char buf[80];
    pci_device_t dev;
    if (pci_find_class(0x03, 0x00, &dev, 0) || pci_find_class(0x03, 0x01, &dev, 0) ||
        pci_find_class(0x03, 0x02, &dev, 0)) {
        ksnprintf(buf, sizeof(buf), "%s %04x", vendor_name(dev.vendor), dev.device);
        return buf;
    }
    return "VGA (integrated / legacy)";
}

const char *hwinfo_nic_name(void) {
    pci_device_t dev;
    if (pci_find_device(0x10EC, 0x8139, &dev)) return "RTL8139 (active)";
    if (pci_find_device(0x1022, 0x2020, &dev)) return "PCnet-FAST III / Am79C973 (active)";
    if (pci_find_device(0x1022, 0x2000, &dev)) return "PCnet-PCI II (active)";
    if (pci_find_class(0x02, 0x00, &dev, 0))
        return device_hint(dev.vendor, dev.device);
    return "none detected";
}

void hwinfo_print(void) {
    kprintf("Platform      : %s\n", hwinfo_vm_platform());
    kprintf("Display       : %s (using VGA mode 13h / text)\n", hwinfo_gpu_name());
    kprintf("Network       : %s\n", hwinfo_nic_name());
    kprintf("\nPCI devices:\n");
    kprintf("  BDF       VID:DID   CLASS  VENDOR / DESCRIPTION\n");
    kprintf("  --------  --------  -----  -------------------------------\n");
    int shown = 0;
    for (int bus = 0; bus < 8 && shown < 32; bus++) {
        for (int slot = 0; slot < 32 && shown < 32; slot++) {
            u16 v = pci_config_read16((u8)bus, (u8)slot, 0, 0x00);
            if (v == 0xFFFF) continue;
            u16 d = pci_config_read16((u8)bus, (u8)slot, 0, 0x02);
            u8 cc = pci_config_read8((u8)bus, (u8)slot, 0, 0x0B);
            u8 sc = pci_config_read8((u8)bus, (u8)slot, 0, 0x0A);
            kprintf("  %02x:%02x.%x    %04x:%04x  %02x.%02x  %-8s %s\n",
                    bus, slot, 0, v, d, cc, sc, vendor_name(v), device_hint(v, d));
            shown++;
        }
    }
    if (!shown) kprintf("  (none)\n");
    kprintf("\nNote: GPU 3D acceleration and VMware/VirtualBox paravirtual drivers are\n");
    kprintf("      not implemented; the VESA-compatible VGA core is used instead.\n");
}
