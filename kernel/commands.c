/* Minimal Desktop - shell command implementations */
#include "shell.h"
#include "console.h"
#include "string.h"
#include "types.h"
#include "io.h"
#include "pit.h"
#include "rtc.h"
#include "mm.h"
#include "ramfs.h"
#include "task.h"
#include "store.h"
#include "net.h"
#include "pci.h"
#include "hwinfo.h"
#include "ata.h"
#include "fat.h"
#include "atapi.h"
#include "iso9660.h"
#include "vga_hw.h"
#include "extras.h"
#include "keyboard.h"
#include "mouse.h"
#include "gfx.h"
#include "gui.h"
#include "multiboot2.h"
#include "selftest.h"
#include "editor.h"

/* ------------------------------------------------------------------------- */
/* helpers                                                                   */
/* ------------------------------------------------------------------------- */
static void print_uptime(void) {
    u64 ms = pit_uptime_ms();
    u32 total = (u32)(ms / 1000);
    u32 d = total / 86400;
    u32 h = (total / 3600) % 24;
    u32 m = (total / 60) % 60;
    u32 s = total % 60;
    if (d) kprintf("%u days, %02u:%02u:%02u\n", d, h, m, s);
    else kprintf("%02u:%02u:%02u\n", h, m, s);
}

static void join_args(char *out, int max, int argc, char **argv, int from) {
    int len = 0;
    out[0] = 0;
    for (int i = from; i < argc; i++) {
        if (i > from) {
            if (len < max - 1) out[len++] = ' ';
        }
        const char *p = argv[i];
        while (*p && len < max - 1) out[len++] = *p++;
    }
    out[len] = 0;
}

static void cpu_vendor(char *out, int max) {
    u32 a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
    char v[13];
    memcpy(v + 0, &b, 4);
    memcpy(v + 4, &d, 4);
    memcpy(v + 8, &c, 4);
    v[12] = 0;
    strncpy(out, v, (size_t)max - 1);
    out[max - 1] = 0;
    (void)a;
}

/* ------------------------------------------------------------------------- */
/* information                                                               */
/* ------------------------------------------------------------------------- */
int cmd_help(int argc, char **argv) {
    if (argc > 1) {
        const command_t *c = shell_find_command(argv[1]);
        if (!c) {
            kprintf("help: no such command '%s'\n", argv[1]);
            return -1;
        }
        kprintf("%s - %s\n", c->name, c->desc);
        kprintf("category: %s\n", c->category);
        kprintf("usage   : %s\n", c->usage);
        return 0;
    }
    kprintf("Minimal Desktop shell commands\n");
    kprintf("  type 'help <command>' for usage\n\n");
    const char *last = "";
    int count = 0;
    const command_t *table = shell_command_table(&count);
    for (int i = 0; i < count; i++) {
        if (strcmp(table[i].category, last) != 0) {
            kprintf("\n[%s]\n", table[i].category);
            last = table[i].category;
        }
        kprintf("  %-10s %s\n", table[i].name, table[i].desc);
    }
    kprintf("\n  %-10s %s\n", "store", "package manager (store list / install / remove)");
    kprintf("\nTry: sysinfo, drivers, hwinfo, store list, startgui, miniland, doom, selftest\n");
    return 0;
}

int cmd_sysinfo(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    char vendor[16];
    cpu_vendor(vendor, sizeof(vendor));
    kprintf("        .----------------------------------.\n");
    kprintf("        |  %-30s  |\n", OS_NAME " " OS_VERSION);
    kprintf("        '----------------------------------'\n");
    kprintf("  system   : %s (%s)\n", OS_NAME, OS_CODENAME);
    kprintf("  version  : %s build %s\n", OS_VERSION, OS_BUILD);
    kprintf("  arch     : %s\n", OS_ARCH);
    kprintf("  cpu      : %s\n", vendor[0] ? vendor : "i386 compatible");
    kprintf("  boot     : %s via %s\n",
            g_boot.cmdline[0] ? g_boot.cmdline : "(no cmdline)",
            g_boot.bootloader[0] ? g_boot.bootloader : "Multiboot2");
    kprintf("  memory   : %u KiB total, %u KiB usable\n",
            pmm_total_ram() / 1024, pmm_usable_ram() / 1024);
    kprintf("             %u frames used, %u free, heap %u/%u KiB\n",
            pmm_used_frames(), pmm_free_frames(),
            heap_used() / 1024, heap_total() / 1024);
    kprintf("  uptime   : ");
    print_uptime();
    kprintf("  tasks    : %d registered\n", task_count());
    kprintf("  files    : %u nodes, %u bytes in RAMFS\n",
            ramfs_node_count(), ramfs_bytes_used());
    kprintf("  network  : %s", net_is_up() ? "up" : (net_info()->present ? "no lease" : "no NIC"));
    if (net_is_up()) {
        char ip[20];
        net_format_ip(net_info()->ip, ip);
        kprintf(" (%s)", ip);
    }
    kprintf("\n  store    : %d packages, repo %s\n",
            store_package_count(), store_repo_url());
    kprintf("  display  : VGA text 80x25 + mode 13h 320x200\n");
    return 0;
}

int cmd_uptime(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    kprintf("up ");
    print_uptime();
    kprintf("%u ticks at %u Hz\n", pit_ticks(), pit_get_frequency());
    return 0;
}

int cmd_uname(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    kprintf("%s %s %s %s\n", OS_NAME, OS_SHORTNAME, OS_VERSION, OS_ARCH);
    return 0;
}

int cmd_drivers(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    kprintf("DRIVER          DEVICE                                   STATUS\n");
    kprintf("--------------  ---------------------------------------  --------\n");
    kprintf("vga_text        VGA 80x25 text, 0xB8000                  %s\n", "OK");
    kprintf("vga_mode13      VGA 320x200x256, 0xA0000                 %s\n", "OK");
    kprintf("serial          COM1 16550 @ 0x3F8, 115200 8N1           %s\n", "OK");
    kprintf("ps2_keyboard    IRQ1, set-1 translation                  %s\n",
            kbd_irq_count() ? "OK" : "ready");
    mouse_state_t m;
    mouse_get_state(&m);
    kprintf("ps2_mouse       IRQ12, 3-byte packets                    %s (%d,%d)\n",
            m.present ? "OK" : "absent", m.x, m.y);
    kprintf("pit             channel 0 @ %u Hz                        OK\n", pit_get_frequency());
    kprintf("rtc             CMOS real-time clock                     OK\n");
    const ata_disk_t *d = ata_get_disk();
    if (ata_present() && d)
        kprintf("ata             %-38s  %u MB\n", d->model, d->size_mb);
    else
        kprintf("ata             primary IDE controller                   no disk\n");
    kprintf("pci             %d device(s) enumerated                   OK\n", pci_device_count());
    kprintf("platform        %-38s  %s\n", hwinfo_vm_platform(), "detected");
    kprintf("display/gpu     %-38s  VGA fallback\n", hwinfo_gpu_name());
    if (net_info()->present)
        kprintf("%-14s I/O 0x%X IRQ %u, %02x:%02x:%02x:%02x:%02x:%02x  %s\n",
                net_info()->name, net_info()->io_base, net_info()->irq,
                net_info()->mac[0], net_info()->mac[1], net_info()->mac[2],
                net_info()->mac[3], net_info()->mac[4], net_info()->mac[5],
                net_is_up() ? "UP" : "DOWN");
    else
        kprintf("nic             no supported PCI network controller     absent\n");
    kprintf("ramfs           in-memory filesystem                     OK\n");
    kprintf("store           embedded MDSTORE1 archive                 %s\n",
            store_package_count() ? "mounted" : "unmounted");
    return 0;
}

int cmd_hwinfo(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    hwinfo_print();
    return 0;
}

int cmd_taskmng(int argc, char **argv) {
    if (argc >= 2 && strcasecmp(argv[1], "kill") == 0) {
        if (argc < 3) { kprintf("usage: taskmng kill <pid>\n"); return -1; }
        int pid = atoi(argv[2]);
        int rc = task_kill(pid);
        if (rc == 0) kprintf("taskmng: killed pid %d\n", pid);
        else if (rc == -2) kprintf("taskmng: pid %d is a kernel task\n", pid);
        else if (rc == -3) kprintf("taskmng: cannot kill the running task\n");
        else kprintf("taskmng: no such pid %d\n", pid);
        return rc == 0 ? 0 : -1;
    }
    kprintf("Minimal Desktop task manager - %d tasks, %u ticks\n",
            task_count(), pit_ticks());
    task_print_table();
    return 0;
}

int cmd_selftest(int argc, char **argv) {
    bool auto_mode = argc > 1 && strcmp(argv[1], "--autotest") == 0;
    int failures = selftest_run(auto_mode);
    if (failures == 0)
        kprintf("\nAll %d checks passed.\n", selftest_last_passes());
    else
        kprintf("\n%d check(s) failed, %d passed.\n",
                failures, selftest_last_passes());
    return failures == 0 ? 0 : 1;
}

int cmd_clear(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    console_clear();
    return 0;
}

static NORETURN void poweroff(void) {
    cli();
    /* QEMU/Bochs ACPI PM1a control, then older fallbacks. */
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    kprintf("shutdown: ACPI poweroff not available, halting CPU\n");
    for (;;) hlt();
}

static NORETURN void reboot_now(void) {
    cli();
    for (int i = 0; i < 20; i++) {
        while (inb(0x64) & 0x02) { }
        outb(0x64, 0xFE);
        pit_sleep_ms(2);
    }
    /* Triple fault as the last resort. */
    kprintf("reboot: controller did not respond, triple faulting\n");
    for (;;) {
        __asm__ volatile("lidt (%0)" : : "r"((u16 *)0));
    }
    for (;;) hlt();
}

int cmd_reboot(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    kprintf("Rebooting Minimal Desktop...\n");
    pit_sleep_ms(250);
    serial_puts("\r\n[reboot]\r\n");
    reboot_now();
}

int cmd_shutdown(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    kprintf("Shutting down Minimal Desktop...\n");
    pit_sleep_ms(250);
    serial_puts("\r\n[shutdown]\r\n");
    poweroff();
}

int cmd_date(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    rtc_time_t t;
    rtc_read(&t);
    char buf[48];
    rtc_format_date(&t, buf, sizeof(buf));
    kprintf("%s (%s)\n", buf, rtc_weekday_name(t.weekday));
    return 0;
}

int cmd_time(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    rtc_time_t t;
    rtc_read(&t);
    char buf[32];
    rtc_format_time(&t, buf, sizeof(buf));
    kprintf("%s %s\n", buf, "local");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* filesystem                                                                */
/* ------------------------------------------------------------------------- */
int cmd_pwd(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    kprintf("%s\n", shell_cwd_path());
    return 0;
}

int cmd_ls(int argc, char **argv) {
    fs_node_t *dir = argc > 1 ? ramfs_lookup(argv[1]) : ramfs_cwd();
    if (!dir) {
        kprintf("ls: %s: no such file or directory\n", argv[1]);
        return -1;
    }
    if (dir->type == FS_FILE) {
        char path[128];
        kprintf("%-24s %6u B  file\n", ramfs_path(dir, path, sizeof(path)), dir->size);
        return 0;
    }
    kprintf("NAME                             TYPE     SIZE\n");
    kprintf("-------------------------------  -------  --------\n");
    fs_node_t *child;
    int i = 0, files = 0, dirs = 0;
    while (ramfs_readdir(dir, i++, &child) == 0) {
        if (child->type == FS_DIR) {
            kprintf("%-31s  <dir>          \n", child->name);
            dirs++;
        } else {
            kprintf("%-31s  file     %6u B\n", child->name, child->size);
            files++;
        }
    }
    kprintf("\n%d director%s, %d file%s\n", dirs, dirs == 1 ? "y" : "ies",
            files, files == 1 ? "" : "s");
    return 0;
}

int cmd_cd(int argc, char **argv) {
    const char *target = argc > 1 ? argv[1] : "/home/minimal";
    fs_node_t *node;
    if (strcmp(target, "..") == 0) node = ramfs_cwd()->parent;
    else node = ramfs_lookup(target);
    if (!node) {
        kprintf("cd: %s: no such directory\n", target);
        return -1;
    }
    if (node->type != FS_DIR) {
        kprintf("cd: %s: not a directory\n", target);
        return -1;
    }
    ramfs_set_cwd(node);
    return 0;
}

int cmd_mkdir(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: mkdir <dir>\n"); return -1; }
    if (ramfs_lookup(argv[1])) {
        kprintf("mkdir: %s already exists\n", argv[1]);
        return -1;
    }
    if (!ramfs_create(argv[1], FS_DIR)) {
        kprintf("mkdir: cannot create %s\n", argv[1]);
        return -1;
    }
    return 0;
}

int cmd_touch(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: touch <file>\n"); return -1; }
    fs_node_t *n = ramfs_lookup(argv[1]);
    if (n) {
        if (n->type != FS_FILE) { kprintf("touch: %s is a directory\n", argv[1]); return -1; }
        return 0;
    }
    if (!ramfs_create(argv[1], FS_FILE)) {
        kprintf("touch: cannot create %s\n", argv[1]);
        return -1;
    }
    return 0;
}

int cmd_rm(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: rm <path>\n"); return -1; }
    if (ramfs_unlink(argv[1]) != 0) {
        kprintf("rm: cannot remove %s\n", argv[1]);
        return -1;
    }
    return 0;
}

int cmd_cat(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: cat <file>\n"); return -1; }
    fs_node_t *n = ramfs_lookup(argv[1]);
    if (!n) { kprintf("cat: %s: no such file\n", argv[1]); return -1; }
    if (n->type != FS_FILE) { kprintf("cat: %s: is a directory\n", argv[1]); return -1; }
    char chunk[129];
    u32 off = 0;
    while (off < n->size) {
        u32 want = n->size - off;
        if (want > 128) want = 128;
        int got = ramfs_read(n, chunk, want, off);
        if (got <= 0) break;
        chunk[got] = 0;
        kprintf("%s", chunk);
        off += (u32)got;
    }
    if (n->size == 0 || off == 0) kprintf("cat: %s: empty file\n", argv[1]);
    if (off > 0) kprintf("\n");
    return 0;
}

/* Install the OS onto the first ATA disk. The ISO produced by the build is
 * isohybrid (GRUB BIOS boot sector + ISO9660 payload), so installing is just a
 * sector-by-sector copy of that image to LBA 0. The image is shipped as the
 * Multiboot2 module "install-source". */

int cmd_saver(int argc, char **argv) {
    int mode = argc > 1 ? atoi(argv[1]) : 0;
    int secs = argc > 2 ? atoi(argv[2]) : 10;
    if (mode < 0 || mode > 3) { kprintf("usage: saver <0=matrix|1=stars|2=logo|3=plasma> [seconds]\n"); return -1; }
    if (secs < 1) secs = 1;
    screensaver_run(mode, secs);
    return 0;
}

int cmd_play(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: play <notes.txt>   (e.g. C4 E4 G4 C5)\n"); return -1; }
    return music_play(argv[1]);
}

int cmd_backup(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: backup <file on /disk>\n"); return -1; }
    return backup_create(argv[1]);
}

int cmd_restore(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: restore <backup file>\n"); return -1; }
    return backup_restore(argv[1]);
}

/* Convert HTML to plain text in a pager-friendly form. */
static void html_strip(const char *in, char *out, int max) {
    int o = 0;
    bool tag = false;
    while (*in && o < max - 1) {
        if (*in == '<') { tag = true; in++; continue; }
        if (*in == '>') { tag = false; in++; if (o < max-1) out[o++] = '\n'; continue; }
        if (tag) { in++; continue; }
        char c = *in++;
        if (c == '&') {
            if (strncmp(in, "amp;", 4) == 0) { c = '&'; in += 3; }
            else if (strncmp(in, "lt;", 3) == 0) { c = '<'; in += 2; }
            else if (strncmp(in, "gt;", 3) == 0) { c = '>'; in += 2; }
            else if (strncmp(in, "nbsp;", 5) == 0) { c = ' '; in += 4; }
        }
        out[o++] = c;
    }
    out[o] = 0;
}

int cmd_slideshow(int argc, char **argv) {
    int secs = argc > 1 ? atoi(argv[1]) : 20;
    if (secs < 1) secs = 1;
    return slideshow_run(secs);
}

int cmd_chkdsk(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    return chkdsk_fat();
}

int cmd_lynx(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: lynx <url>\n"); return -1; }
    if (strcmp(argv[1], "about:home") == 0) {
        kprintf("Minimal Lynx\nType 'lynx <url>' to fetch a page.\n");
        return 0;
    }
    if (!net_is_up()) { kprintf("lynx: network offline\n"); return -1; }
    static char body[8192];
    static char text[8192];
    int len = 0;
    if (net_http_get(argv[1], body, sizeof(body), &len) != 0 || len <= 0) {
        kprintf("lynx: failed to fetch %s (status %d)\n", argv[1], net_http_status());
        return -1;
    }
    html_strip(body, text, sizeof(text));
    /* print up to 60 lines */
    char *p = text;
    int lines = 0;
    while (*p && lines < 60) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        if (*p) { kprintf("%s\n", p); lines++; }
        if (!nl) break;
        p = nl + 1;
    }
    kprintf("--- %d lines (status %d) ---\n", lines, net_http_status());
    return 0;
}

int cmd_weather(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    if (!net_is_up()) { kprintf("weather: network offline\n"); return -1; }
    char out[512];
    int len = 0;
    if (net_http_get("http://wttr.in/?format=3", out, sizeof(out), &len) != 0 || len <= 0) {
        kprintf("weather: fetch failed\n"); return -1;
    }
    out[len] = 0;
    kprintf("%s\n", out);
    return 0;
}

int cmd_install(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);

    u32 src = 0, src_end = 0;
    const char *modname = NULL;
    bool found = false;
    for (int i = 0; i < (int)g_boot.module_count; i++) {
        u32 a = 0, b = 0;
        const char *n = NULL;
        if (mb2_get_module(i, &a, &b, &n) && n && strcmp(n, "install-source") == 0) {
            src = a; src_end = b; modname = n; found = true; break;
        }
    }
    if (!found || !src || src_end <= src) {
        kprintf("install: no embedded install-source module (boot the full ISO)\n");
        return -1;
    }

    const ata_disk_t *disk = ata_get_disk();
    u32 bytes = src_end - src;
    u32 sectors = (bytes + 511) / 512;
    if (!ata_present() || !disk || !disk->sectors) {
        kprintf("install: no ATA disk found\n");
        return -1;
    }
    if (disk->sectors < sectors) {
        kprintf("install: disk too small (%u sectors, image needs %u)\n",
                disk->sectors, sectors);
        return -1;
    }

    kprintf("install: writing %s to ATA disk 0 (%u sectors, %u MiB)\n",
            modname ? modname : "install-source", sectors, bytes >> 20);
    kprintf("WARNING: this overwrites the entire first disk. Continue? (y/N)\n");

    /* Wait for a 'y' with a short timeout so --installtest can run unattended. */
    int key = kbd_wait_key(5000);
    if (key != 'y' && key != 'Y' && !mb2_has_flag("--installtest")) {
        kprintf("install: aborted\n");
        return -1;
    }

    for (u32 lba = 0; lba < sectors; lba++) {
        if (!ata_write_sector(lba, (const u8 *)(uintptr_t)(src + lba * 512))) {
            kprintf("\ninstall: write failed at LBA %u\n", lba);
            return -1;
        }
        if ((lba & 1023) == 0) {
            kprintf("\r  writing: %u/%u sectors (%u%%)", lba, sectors,
                    (lba * 100) / sectors);
        }
    }
    kprintf("\r  writing: %u/%u sectors (100%%)\n", sectors, sectors);

    u8 written[512], expected[512];
    memcpy(expected, (const void *)src, 512);
    if (!ata_read_sector(0, written) || memcmp(written, expected, 512) != 0) {
        kprintf("install: verification of sector 0 FAILED\n");
        return -1;
    }
    kprintf("install: verified sector 0. Remove the CD/USB and reboot;\n");
    kprintf("         the disk now boots Minimal Desktop on its own.\n");
    return 0;
}

/* Deliberate fault used to exercise the crash screen. Address 0x80000000 is
 * outside the identity-mapped first gigabyte, so the write raises a page fault
 * with a meaningful CR2. */
int cmd_crash(int argc, char **argv) {
    /* "crash gui" enters mode 13h first, which clobbers the plane-2 font just
     * like a real GUI fault, so the crash screen's font recovery gets tested. */
    bool gui = (argc >= 2 && (argv[1][0] == 'g' || argv[1][0] == 'G'));
    kprintf("crash: provoking a divide-by-zero in 1 second%s...\n",
            gui ? " (from graphics mode)" : "");
    pit_sleep_ms(1000);
    if (gui) {
        gfx_enter();
        u8 *fb = gfx_framebuffer();
        for (int i = 0; i < 320 * 200; i += 37)
            fb[i] = (u8)(i * 7 + 13);
        gfx_present();
        pit_sleep_ms(200);
    }
    /* Paging is not enabled on this kernel, so a wild write would succeed.
     * A divide by zero always raises exception 0 and exercises the same
     * handler path (including the font/palette recovery). */
    volatile int denom = 0;
    volatile int boom = 1000 / denom;
    (void)boom;
    kprintf("crash: no fault?!\n");
    return -1;
}

int cmd_edit(int argc, char **argv) {
    if (argc < 2) {
        kprintf("usage: edit <file>   (F2 or Ctrl+S saves, Esc exits)\n");
        return -1;
    }
    return editor_run(argv[1]);
}

int cmd_read(int argc, char **argv) {
    if (argc < 2) {
        kprintf("usage: less <file>   (arrows scroll, space pages, q quits)\n");
        return -1;
    }
    return pager_run(argv[1]);
}

/* copy a file (RAMFS, or the RAM mirror of a mounted filesystem) */
int cmd_cdrom(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    if (!atapi_present()) { kprintf("cdrom: no ATAPI device on secondary IDE\n"); return -1; }
    if (iso9660_mount()) { iso9660_info(); return 0; }
    kprintf("cdrom: could not read ISO9660 filesystem\n"); return -1;
}

int cmd_mount(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    if (fat_present()) {
        fat_info();
        kprintf("mounted at %s\n", "/disk");
        return 0;
    }
    if (fat_automount()) return 0;
    kprintf("mount: no FAT data partition found\n");
    return -1;
}

int cmd_sync(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    if (!fat_present()) { kprintf("sync: /disk is not mounted\n"); return -1; }
    return fat_sync();
}

int cmd_format(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "/disk") != 0) {
        kprintf("usage: format /disk   (erases the data partition)\n"); return -1;
    }
    if (!fat_present()) { kprintf("format: /disk is not mounted\n"); return -1; }
    kprintf("format: this erases all files on /disk. Type YES to continue: ");
    int k = kbd_wait_key(5000);
    if (k != 'Y') { kprintf("\nformat: aborted\n"); return -1; }
    kprintf("\n");
    if (fat_format(fat_part_lba()) != 0) { kprintf("format: failed\n"); return -1; }
    kprintf("format: done. Remount to use the empty filesystem.\n");
    return 0;
}

int cmd_cp(int argc, char **argv) {
    if (argc != 3) { kprintf("usage: cp <source> <destination>\n"); return -1; }
    fs_node_t *src = ramfs_lookup(argv[1]);
    if (!src || src->type != FS_FILE) {
        kprintf("cp: %s: no such file\n", argv[1]); return -1;
    }
    fs_node_t *dst = ramfs_lookup(argv[2]);
    if (dst && dst->type == FS_DIR) {
        kprintf("cp: %s is a directory (give a full file name)\n", argv[2]); return -1;
    }
    if (!dst) {
        fs_node_t *parent = ramfs_parent_of(argv[2]);
        if (!parent || parent->type != FS_DIR) {
            kprintf("cp: bad destination path\n"); return -1;
        }
        dst = ramfs_create(argv[2], FS_FILE);
    }
    if (!dst) { kprintf("cp: cannot create %s\n", argv[2]); return -1; }

    char buf[512];
    u32 off = 0;
    if (ramfs_truncate(dst, 0) != 0) { kprintf("cp: truncate failed\n"); return -1; }
    while (off < src->size) {
        u32 want = src->size - off;
        if (want > sizeof(buf)) want = sizeof(buf);
        int got = ramfs_read(src, buf, want, off);
        if (got <= 0) break;
        if (ramfs_write(dst, buf, (u32)got, off) != got) {
            kprintf("cp: short write\n"); return -1;
        }
        off += (u32)got;
    }
    kprintf("cp: %s -> %s (%u bytes)\n", argv[1], argv[2], off);
    return 0;
}

int cmd_mv(int argc, char **argv) {
    if (argc != 3) { kprintf("usage: mv <source> <destination>\n"); return -1; }
    fs_node_t *src = ramfs_lookup(argv[1]);
    if (!src || src->type != FS_FILE) {
        kprintf("mv: %s: no such file\n", argv[1]); return -1;
    }
    if (cmd_cp(argc, argv) != 0) return -1;
    if (ramfs_unlink(argv[1]) != 0) {
        kprintf("mv: could not remove source %s\n", argv[1]); return -1;
    }
    return 0;
}

static void tree_walk(fs_node_t *dir, int depth, int *files, int *dirs) {
    if (depth > 12) return;
    fs_node_t *child;
    for (int i = 0; ramfs_readdir(dir, i, &child) == 0; i++) {
        for (int d = 0; d < depth; d++) kprintf(d == depth - 1 ? "|-- " : "|   ");
        if (child->type == FS_DIR) {
            kprintf("%s/\n", child->name);
            (*dirs)++;
            tree_walk(child, depth + 1, files, dirs);
        } else {
            kprintf("%s (%u B)\n", child->name, child->size);
            (*files)++;
        }
    }
}

int cmd_tree(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : ".";
    fs_node_t *dir = ramfs_lookup(path);
    if (!dir || dir->type != FS_DIR) {
        kprintf("tree: %s: no such directory\n", path); return -1;
    }
    kprintf("%s\n", path);
    int files = 0, dirs = 0;
    tree_walk(dir, 1, &files, &dirs);
    kprintf("\n%d directories, %d files\n", dirs, files);
    return 0;
}

static u32 du_walk(fs_node_t *dir) {
    u32 total = 0;
    fs_node_t *child;
    for (int i = 0; ramfs_readdir(dir, i, &child) == 0; i++) {
        if (child->type == FS_DIR) total += du_walk(child);
        else total += child->size;
    }
    return total;
}

int cmd_du(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : ".";
    fs_node_t *dir = ramfs_lookup(path);
    if (!dir || dir->type != FS_DIR) {
        kprintf("du: %s: no such directory\n", path); return -1;
    }
    u32 n = du_walk(dir);
    kprintf("%s: %u bytes (%u KiB)\n", path, n, n / 1024);
    return 0;
}

static void find_walk(fs_node_t *dir, const char *needle, const char *prefix, int *count) {
    fs_node_t *child;
    for (int i = 0; ramfs_readdir(dir, i, &child) == 0; i++) {
        if (strstr(child->name, needle)) {
            if (prefix[0] == '/') kprintf("%s/%s\n", prefix, child->name);
            else kprintf("%s\n", child->name);
            (*count)++;
        }
        if (child->type == FS_DIR) {
            char sub[160];
            if (prefix[0] == '/') ksnprintf(sub, sizeof(sub), "%s/%s", prefix, child->name);
            else ksnprintf(sub, sizeof(sub), "%s/%s", prefix, child->name);
            find_walk(child, needle, sub, count);
        }
    }
}

int cmd_find(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: find <name> [directory]\n"); return -1; }
    const char *path = argc > 2 ? argv[2] : "/";
    fs_node_t *dir = ramfs_lookup(path);
    if (!dir || dir->type != FS_DIR) {
        kprintf("find: %s: no such directory\n", path); return -1;
    }
    int count = 0;
    find_walk(dir, argv[1], path, &count);
    kprintf("find: %d match(es)\n", count);
    return 0;
}

int cmd_new(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: new <file>   (create and open in the editor)\n"); return -1; }
    fs_node_t *n = ramfs_lookup(argv[1]);
    if (n && n->type != FS_FILE) { kprintf("new: %s is a directory\n", argv[1]); return -1; }
    if (!n) {
        fs_node_t *parent = ramfs_parent_of(argv[1]);
        if (!parent || parent->type != FS_DIR) { kprintf("new: bad path\n"); return -1; }
        n = ramfs_create(argv[1], FS_FILE);
        if (!n) { kprintf("new: cannot create %s\n", argv[1]); return -1; }
    }
    return editor_run(argv[1]);
}

int cmd_write(int argc, char **argv) {
    if (argc < 3) { kprintf("usage: write <file> <text>\n"); return -1; }
    char text[512];
    join_args(text, sizeof(text), argc, argv, 2);
    fs_node_t *n = ramfs_lookup(argv[1]);
    if (!n) n = ramfs_create(argv[1], FS_FILE);
    if (!n || n->type != FS_FILE) { kprintf("write: cannot open %s\n", argv[1]); return -1; }
    if (ramfs_truncate(n, 0) != 0) { kprintf("write: truncate failed\n"); return -1; }
    size_t len = strlen(text);
    if (ramfs_write(n, text, (u32)len, 0) != (int)len) {
        kprintf("write: short write\n");
        return -1;
    }
    if (ramfs_append(n, "\n", 1) != 1) {
        kprintf("write: short write\n");
        return -1;
    }
    kprintf("wrote %u bytes to %s\n", (unsigned)(len + 1), argv[1]);
    return 0;
}

int cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        kprintf("%s", argv[i]);
        if (i + 1 < argc) kprintf(" ");
    }
    kprintf("\n");
    return 0;
}

int cmd_memmap(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    mb2_dump();
    kprintf("\n");
    mm_print_map();
    u32 total, used, free_frames, heap_kb;
    mm_get_stats(&total, &used, &free_frames, &heap_kb);
    kprintf("\nframes: %u total, %u used, %u free\n", total, used, free_frames);
    kprintf("heap  : %u KiB used of %u KiB\n", heap_kb, heap_total() / 1024);
    return 0;
}

int cmd_hexdump(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: hexdump <file> [bytes]\n"); return -1; }
    fs_node_t *n = ramfs_lookup(argv[1]);
    if (!n || n->type != FS_FILE) { kprintf("hexdump: %s: no such file\n", argv[1]); return -1; }
    u32 limit = n->size;
    if (argc > 2) {
        u32 want = (u32)atoi(argv[2]);
        if (want && want < limit) limit = want;
    }
    if (limit > 4096) limit = 4096;
    u8 buf[16];
    for (u32 off = 0; off < limit; off += 16) {
        u32 want = limit - off > 16 ? 16 : limit - off;
        int got = ramfs_read(n, buf, want, off);
        if (got <= 0) break;
        kprintf("%08x  ", off);
        for (int i = 0; i < 16; i++) {
            if (i < got) kprintf("%02x ", buf[i]);
            else kprintf("   ");
            if (i == 7) kprintf(" ");
        }
        kprintf(" |");
        for (int i = 0; i < got; i++)
            kprintf("%c", buf[i] >= 0x20 && buf[i] < 0x7F ? (char)buf[i] : '.');
        kprintf("|\n");
    }
    return 0;
}

int cmd_df(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    kprintf("Filesystem      Size      Used     Use%%  Mounted on\n");
    kprintf("ramfs        %6u K  %6u K    %2u%%  /\n",
            FS_MAX_FILE / 1024, ramfs_bytes_used() / 1024,
            (unsigned)(ramfs_bytes_used() * 100 / FS_MAX_FILE));
    return 0;
}

/* ------------------------------------------------------------------------- */
/* calculator                                                                */
/* ------------------------------------------------------------------------- */
static const char *calc_p;
static bool calc_error;

static void calc_ws(void) {
    while (*calc_p == ' ' || *calc_p == '\t') calc_p++;
}

static long calc_expr(void);

static long calc_number(void) {
    calc_ws();
    if (*calc_p == '(') {
        calc_p++;
        long v = calc_expr();
        calc_ws();
        if (*calc_p == ')') calc_p++;
        else calc_error = true;
        return v;
    }
    if (*calc_p == '-') { calc_p++; return -calc_number(); }
    if (*calc_p == '+') { calc_p++; return calc_number(); }
    if (!isdigit((unsigned char)*calc_p)) { calc_error = true; return 0; }
    long v = 0;
    while (isdigit((unsigned char)*calc_p)) {
        v = v * 10 + (*calc_p - '0');
        calc_p++;
    }
    return v;
}

static long calc_term(void) {
    long v = calc_number();
    for (;;) {
        calc_ws();
        char op = *calc_p;
        if (op != '*' && op != '/' && op != '%') break;
        calc_p++;
        long r = calc_number();
        if ((op == '/' || op == '%') && r == 0) { calc_error = true; return 0; }
        if (op == '*') v *= r;
        else if (op == '/') v /= r;
        else v %= r;
    }
    return v;
}

static long calc_expr(void) {
    long v = calc_term();
    for (;;) {
        calc_ws();
        char op = *calc_p;
        if (op != '+' && op != '-') break;
        calc_p++;
        long r = calc_term();
        if (op == '+') v += r;
        else v -= r;
    }
    return v;
}

int cmd_calc(int argc, char **argv) {
    if (argc < 2) {
        kprintf("usage: calc <expression>   e.g. calc (2+3)*4\n");
        return -1;
    }
    char expr[256];
    join_args(expr, sizeof(expr), argc, argv, 1);
    calc_p = expr;
    calc_error = false;
    long v = calc_expr();
    calc_ws();
    if (*calc_p || calc_error) {
        kprintf("calc: syntax error near '%s'\n", calc_p);
        return -1;
    }
    kprintf("%ld\n", v);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* appearance and toys                                                       */
/* ------------------------------------------------------------------------- */
int cmd_theme(int argc, char **argv) {
    if (argc < 2) {
        u8 fg, bg;
        console_get_theme(&fg, &bg);
        kprintf("theme: fg=%u bg=%u\n", fg, bg);
        kprintf("usage: theme <name|fg> [bg]\n");
        kprintf("names: default green amber white cyan matrix\n");
        return 0;
    }
    const char *name = argv[1];
    u8 fg = VGA_LIGHT_GREY, bg = VGA_BLACK;
    if (strcasecmp(name, "default") == 0) { fg = VGA_LIGHT_GREY; bg = VGA_BLACK; }
    else if (strcasecmp(name, "green") == 0) { fg = VGA_LIGHT_GREEN; bg = VGA_BLACK; }
    else if (strcasecmp(name, "amber") == 0) { fg = VGA_YELLOW; bg = VGA_BLACK; }
    else if (strcasecmp(name, "white") == 0) { fg = VGA_WHITE; bg = VGA_BLUE; }
    else if (strcasecmp(name, "cyan") == 0) { fg = VGA_LIGHT_CYAN; bg = VGA_BLACK; }
    else if (strcasecmp(name, "matrix") == 0) { fg = VGA_GREEN; bg = VGA_BLACK; }
    else if (isdigit((unsigned char)name[0])) {
        fg = (u8)atoi(name);
        if (argc > 2) bg = (u8)atoi(argv[2]);
    } else {
        kprintf("theme: unknown name '%s'\n", name);
        return -1;
    }
    console_set_theme(fg, bg);
    theme_save();
    kprintf("theme: fg=%u bg=%u applied (saved to /disk)\n", fg, bg);
    return 0;
}

int cmd_matrix(int argc, char **argv) {
    UNUSED(argv);
    int seconds = argc > 1 ? atoi(argv[1]) : 3;
    if (seconds <= 0) seconds = 3;
    u32 end = pit_ticks() + (u32)seconds * 100;
    u8 old_fg, old_bg;
    console_get_theme(&old_fg, &old_bg);
    console_clear();
    while ((s32)(pit_ticks() - end) < 0) {
        for (int i = 0; i < 24; i++) {
            int row = (int)(rand() % 25);
            int col = (int)(rand() % 80);
            char c = (char)('!' + (rand() % 90));
            console_put_at(row, col, c, console_make_attr(VGA_GREEN, VGA_BLACK));
        }
        int k = kbd_getchar();
        if (k == KEY_ESC) break;
        pit_sleep_ms(60);
    }
    console_clear();
    console_set_theme(old_fg, old_bg);
    kprintf("matrix: digital rain stopped\n");
    return 0;
}

int cmd_beep(int argc, char **argv) {
    u32 freq = argc > 1 ? (u32)atoi(argv[1]) : 880;
    u32 ms = argc > 2 ? (u32)atoi(argv[2]) : 180;
    if (freq < 20) freq = 20;
    if (freq > 20000) freq = 20000;
    u32 div = 1193180u / freq;
    outb(0x43, 0xB6);
    outb(0x42, (u8)(div & 0xFF));
    outb(0x42, (u8)((div >> 8) & 0xFF));
    u8 port = inb(0x61);
    outb(0x61, (u8)(port | 0x03));
    pit_sleep_ms(ms);
    outb(0x61, (u8)(port & 0xFC));
    kprintf("beep: %u Hz for %u ms\n", freq, ms);
    return 0;
}

int cmd_credits(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    kprintf("\n");
    kprintf("  ##     ##  ###  #    #  #####  ###  #        #####\n");
    kprintf("  # #   # #   #   ##   #    #     #   #        #    \n");
    kprintf("  #  # #  #   #   # #  #    #     #   #        #### \n");
    kprintf("  #   #   #   #   #  # #    #     #   #        #    \n");
    kprintf("  #       #  ###  #   ##  #####  ###  #####    #####\n");
    kprintf("\n");
    kprintf("  %s %s \"%s\" - a hobby x86 desktop\n", OS_NAME, OS_VERSION, OS_CODENAME);
    kprintf("  kernel      : custom i386, Multiboot2, IRQ driven\n");
    kprintf("  graphics    : VGA text + mode 13h compositor\n");
    kprintf("  network     : RTL8139 + ARP/IPv4/ICMP/UDP/DHCP/DNS/TCP\n");
    kprintf("  package store: embedded MDSTORE1 archive, FNV-1a verified\n");
    kprintf("  built with  : GCC (i686-linux-gnu), NASM, GRUB, QEMU\n");
    kprintf("\n  Thanks for booting. Type 'selftest' to see the machine prove itself.\n\n");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* graphics / games front-ends                                               */
/* ------------------------------------------------------------------------- */
int cmd_imgview(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    if (!store_module_bound(MOD_ENTRY_DESKTOP)) {
        kprintf("imgview: install the GUI first: minipacket -I minimal-desktop\n");
        return -1;
    }
    return cmd_startgui(0, NULL);
}

int cmd_startgui(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    if (!store_module_bound(MOD_ENTRY_DESKTOP)) {
        kprintf("startgui: desktop module not installed.\n");
        kprintf("          run: minipacket -I minimal-desktop\n");
        return -1;
    }
    for (;;) {
        int r = gui_run();
        if (r == GUI_LAUNCH_SNAKE) { char *a[] = {"snake"}; cmd_snake(1, a); continue; }
        if (r == GUI_LAUNCH_PONG)  { char *a[] = {"pong"};  cmd_pong(1, a);  continue; }
        if (r == GUI_LAUNCH_DOOM)  { char *a[] = {"doom"};  cmd_doom(1, a);  continue; }
        return 0;
    }
}

int cmd_miniland(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    if (!store_module_bound(MOD_ENTRY_MINILAND)) {
        kprintf("miniland: not installed.\n");
        kprintf("          run: minipacket -I miniland\n");
        return -1;
    }
    return gui_run_miniland();
}

int cmd_keymap(int argc, char **argv) {
    if (argc < 2) {
        kprintf("current layout: %s (Alt+Shift cycles, Ctrl+Alt+K lists)\n",
                kbd_layout_label());
        kprintf("usage: keymap <layout> | keymap list | keymap next | keymap prev\n");
        return 0;
    }
    if (strcmp(argv[1], "list") == 0) {
        kprintf("%d layouts available:\n", kbd_layout_count());
        for (int i = 0; i < kbd_layout_count(); i++) {
            kprintf("  %-7s %-22s%s\n",
                    kbd_layout_name_at(i), kbd_layout_label_at(i),
                    i == kbd_layout_index() ? "  <- current" : "");
        }
        return 0;
    }
    if (strcmp(argv[1], "next") == 0) {
        kbd_cycle_layout(1);
    } else if (strcmp(argv[1], "prev") == 0) {
        kbd_cycle_layout(-1);
    } else if (!kbd_set_layout(argv[1])) {
        kprintf("keymap: unknown layout '%s' (try: keymap list)\n", argv[1]);
        return -1;
    }
    kprintf("keymap: layout set to %s (%s), %d layouts\n",
            kbd_layout_label(), kbd_layout_name(), kbd_layout_count());
    return 0;
}

int cmd_minipacket(int argc, char **argv) {
    /* Unix-style flags: -L list, -I/-i install, -R remove, -S info, -U update */
    static const struct { const char *flag; const char *verb; } map[] = {
        { "-L", "list" }, { "-l", "list" }, { "--list", "list" },
        { "-I", "install" }, { "-i", "install" }, { "--install", "install" },
        { "-R", "remove" }, { "-r", "remove" }, { "--remove", "remove" },
        { "-S", "info" }, { "-s", "info" }, { "--info", "info" },
        { "-U", "update" }, { "-u", "update" }, { "--update", "update" },
    };

    if (argc < 2) return cmd_store(1, (char *[]){ "store" });

    char *sargv[8];
    int sargc = 0;
    sargv[sargc++] = "store";

    const char *verb = NULL;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (sargc == 1 && a[0] == '-') {
            for (int m = 0; m < (int)(sizeof(map) / sizeof(map[0])); m++) {
                if (strcmp(a, map[m].flag) == 0) { verb = map[m].verb; break; }
            }
            if (verb) continue;
            kprintf("minipacket: unknown option '%s' (try -L to list)\n", a);
            return -1;
        }
        if (sargc < 8) sargv[sargc++] = argv[i];
    }
    if (verb) {
        if (sargc > 7) sargc = 7;
        sargv[sargc++] = (char *)verb;
        /* store expects ["store", <verb>, <pkg>]; the package was collected
         * before the verb, so rotate it behind it */
        if (sargc >= 3) {
            char *pkg = sargv[1];
            sargv[1] = sargv[sargc - 1];
            sargv[sargc - 1] = pkg;
        }
    }
    return cmd_store(sargc, sargv);
}

/* ------------------------------------------------------------------------- */
/* networking                                                                */
/* ------------------------------------------------------------------------- */
int cmd_ifconfig(int argc, char **argv) {
    net_info_t *ni = net_info();
    if (!ni->present) {
        kprintf("eth0: no supported NIC found (RTL8139 / PCnet / e1000)\n");
        kprintf("QEMU: -netdev user,id=net0 -device rtl8139,netdev=net0\n");
        kprintf("      -device e1000,netdev=net0   or   -device pcnet,netdev=net0\n");
        return -1;
    }
    if (argc < 2) { ifconfig_print(); return 0; }

    const char *sub = argv[1];
    if (strcasecmp(sub, "dhcp") == 0 || strcasecmp(sub, "renew") == 0) {
        kprintf("ifconfig: requesting DHCP lease via %s...\n", ni->name);
        if (net_dhcp_renew()) { kprintf("ifconfig: lease acquired\n"); ifconfig_print(); return 0; }
        kprintf("ifconfig: DHCP failed\n"); return -1;
    }
    if (strcasecmp(sub, "status") == 0) { ifconfig_print(); return 0; }
    if (strcasecmp(sub, "up") == 0) {
        ni->up = true; kprintf("ifconfig: %s up\n", ni->name); return 0;
    }
    if (strcasecmp(sub, "down") == 0) {
        ni->up = false; kprintf("ifconfig: %s down\n", ni->name); return 0;
    }
    if (strcasecmp(sub, "set") == 0) {
        if (argc < 6) {
            kprintf("usage: ipconfig set <ip> <netmask> <gateway> <dns>\n"); return -1;
        }
        u32 ip = net_parse_ip(argv[2]);
        u32 mask = net_parse_ip(argv[3]);
        u32 gw = net_parse_ip(argv[4]);
        u32 dns = net_parse_ip(argv[5]);
        if (!ip) { kprintf("ifconfig: invalid IP %s\n", argv[2]); return -1; }
        net_set_static(ip, mask, gw, dns);
        kprintf("ifconfig: static configuration applied\n");
        ifconfig_print();
        return 0;
    }
    kprintf("usage: ipconfig [dhcp|status|up|down|set <ip> <mask> <gw> <dns>]\n");
    return -1;
}

int cmd_ping(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: ping <ip|host>\n"); return -1; }
    u32 ip = net_resolve(argv[1]);
    if (!ip) { kprintf("ping: cannot resolve %s\n", argv[1]); return -1; }
    int count = argc > 2 ? atoi(argv[2]) : 4;
    if (count <= 0) count = 4;
    return net_ping(ip, count);
}

int cmd_nslookup(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: nslookup <host>\n"); return -1; }
    u32 ip = net_resolve(argv[1]);
    if (!ip) {
        kprintf("nslookup: cannot resolve %s (dns ", argv[1]);
        char dns[20];
        net_format_ip(net_info()->dns, dns);
        kprintf("%s)\n", dns);
        return -1;
    }
    char buf[20];
    net_format_ip(ip, buf);
    kprintf("Name:    %s\nAddress: %s\n", argv[1], buf);
    return 0;
}

int cmd_curl(int argc, char **argv) {
    if (argc < 2) { kprintf("usage: curl <http://url>\n"); return -1; }
    static char body[8192];
    int len = 0;
    int rc = net_http_get(argv[1], body, sizeof(body), &len);
    if (rc != 0) return rc;
    int show = len;
    if (show > 4096) {
        show = 4096;
        kprintf("(response truncated to 4096 of %d bytes)\n", len);
    }
    for (int i = 0; i < show; i++) kputc(body[i]);
    if (show && body[show - 1] != '\n') kprintf("\n");
    return 0;
}

int cmd_netstat(int argc, char **argv) {
    UNUSED(argc); UNUSED(argv);
    net_poll();
    net_info_t *n = net_info();
    char ip[20], gw[20], dns[20], mask[20];
    net_format_ip(n->ip, ip);
    net_format_ip(n->gateway, gw);
    net_format_ip(n->dns, dns);
    net_format_ip(n->netmask, mask);
    kprintf("Interface eth0\n");
    kprintf("  address   %s\n  netmask   %s\n  gateway   %s\n  dns       %s\n",
            ip, mask, gw, dns);
    kprintf("  link      %s, %s\n", n->link_up ? "up" : "down",
            n->dhcp ? "DHCP" : "static");
    kprintf("  rx        %u packets / %u bytes\n", n->rx_packets, n->rx_bytes);
    kprintf("  tx        %u packets / %u bytes\n", n->tx_packets, n->tx_bytes);
    kprintf("  store     %s\n", store_repo_url());
    kprintf("  last http %d\n", net_http_status());
    return 0;
}
