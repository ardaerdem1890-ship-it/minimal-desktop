/* Minimal Desktop - kernel entry point and boot sequence */
#include "types.h"
#include "multiboot2.h"
#include "console.h"
#include "gdt.h"
#include "isr.h"
#include "pit.h"
#include "mm.h"
#include "ramfs.h"
#include "task.h"
#include "keyboard.h"
#include "mouse.h"
#include "rtc.h"
#include "ata.h"
#include "fat.h"
#include "bmp.h"
#include "atapi.h"
#include "iso9660.h"
#include "usb.h"
#include "extras.h"
#include "pci.h"
#include "net.h"
#include "store.h"
#include "shell.h"
#include "selftest.h"
#include "string.h"
#include "io.h"

static void boot_bar(void) {
    console_set_color(VGA_DARK_GREY, VGA_BLACK);
    kprintf("  ");
    for (int i = 0; i < 52; i++) kprintf("-");
    kprintf("\n");
    console_reset_color();
}

void kernel_main(u32 magic, u32 info_addr) {
    serial_init();
    console_init();

    console_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("\n  Minimal Desktop %s \"%s\"  -  booting\n", OS_VERSION, OS_CODENAME);
    console_reset_color();
    boot_bar();

    if (magic != MB2_BOOT_MAGIC) {
        kprintf("[boot] invalid Multiboot2 magic: %p (expected %p)\n",
                (void *)magic, (void *)MB2_BOOT_MAGIC);
        kprintf("[boot] the kernel must be started by a Multiboot2 loader\n");
        for (;;) hlt();
    }

    if (!mb2_init(info_addr)) {
        kprintf("[boot] Multiboot2 information structure is invalid\n");
        for (;;) hlt();
    }
    kprintf("[boot] GRUB Multiboot2 info at %p, %u KiB RAM\n",
            (void *)info_addr, g_boot.total_bytes / 1024);
    kprintf("[boot] cmdline: %s\n", g_boot.cmdline[0] ? g_boot.cmdline : "(none)");
    if (g_boot.module_count)
        kprintf("[boot] module: store.img at %p-%p\n",
                (void *)g_boot.store_start, (void *)g_boot.store_end);

    gdt_init();
    kprintf("[boot] GDT loaded, flat 4 GiB segments\n");
    idt_init();
    pic_remap();
    isr_init();
    kprintf("[boot] IDT + PIC remapped (IRQ0-15 -> vectors 32-47)\n");

    mm_init();
    kprintf("[boot] memory: %u KiB usable, heap %u KiB, %u frames free\n",
            pmm_usable_ram() / 1024, heap_total() / 1024, pmm_free_frames());

    task_init();
    ramfs_init();
    bmp_sample_images();
    kprintf("[boot] RAMFS mounted at / (%u nodes)\n", ramfs_node_count());

    pit_init();
    kprintf("[boot] PIT running at %u Hz\n", pit_get_frequency());
    keyboard_init();
    kprintf("[boot] PS/2 keyboard on IRQ1\n");
    mouse_init();
    mouse_set_bounds(320, 200);
    kprintf("[boot] PS/2 mouse on IRQ12%s\n", mouse_packets() ? "" : " (no packets yet)");

    ata_init();
    if (ata_present()) {
        const ata_disk_t *d = ata_get_disk();
        kprintf("[boot] ATA: %s, %u MB\n", d->model, d->size_mb);
    } else {
        kprintf("[boot] ATA: no disk attached\n");
    }
    fat_automount();
    if (atapi_init()) iso9660_mount();
    usb_init();

    if (mb2_has_flag("--safe")) {
        kprintf("[boot] safe mode: network and store left offline\n");
        net_info()->present = false;
    } else {
        net_init();
    }

    if (store_init())
        kprintf("[boot] store: %d verified packages from embedded archive\n",
                store_package_count());
    else
        kprintf("[boot] store: archive unavailable (install/network features limited)\n");

    shell_init();

    if (mb2_has_flag("--autotest")) {
        kprintf("[boot] --autotest: running non-interactive selftest\n");
        selftest_run(true);
        for (;;) hlt();
    }

    if (mb2_has_flag("--login")) login_run();
    shell_banner();
    kprintf("  Network: %s\n", net_is_up() ? "online" : "offline");
    if (net_is_up()) {
        char ip[20], gw[20];
        net_format_ip(net_info()->ip, ip);
        net_format_ip(net_info()->gateway, gw);
        kprintf("  eth0 %s  gateway %s  DHCP lease\n", ip, gw);
    }
    kprintf("  Store  : %d packages (store list)\n", store_package_count());
    kprintf("\n");
    shell_run();
}
