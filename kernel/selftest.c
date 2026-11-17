/* Minimal Desktop - built-in hardware and software selftest */
#include "selftest.h"
#include "console.h"
#include "string.h"
#include "types.h"
#include "io.h"
#include "gdt.h"
#include "isr.h"
#include "pit.h"
#include "mm.h"
#include "ramfs.h"
#include "task.h"
#include "store.h"
#include "keyboard.h"
#include "mouse.h"
#include "pci.h"
#include "ata.h"
#include "net.h"
#include "gui.h"
#include "shell.h"

#define MAX_TESTS 20
#define ISA_DEBUG_EXIT 0x501

typedef bool (*test_fn_t)(char *err, int errlen);

typedef struct {
    const char *name;
    test_fn_t fn;
} test_desc_t;

static int last_passes;
static int last_failures;
static bool did_run;

/* ------------------------------------------------------------------------- */
/* wrappers for tests that need a little glue                                */
/* ------------------------------------------------------------------------- */
static bool test_store(char *err, int errlen) {
    if (!store_validate_all(err, errlen)) return false;
    if (store_package_count() < 3) {
        ksnprintf(err, errlen, "store has %d packages", store_package_count());
        return false;
    }
    return true;
}

static bool test_irq(char *err, int errlen) {
    if (isr_irq_count(0) == 0) {
        ksnprintf(err, errlen, "PIT IRQ0 never fired");
        return false;
    }
    if ((pic_get_mask() & 0x0001) != 0) {
        ksnprintf(err, errlen, "IRQ0 is masked");
        return false;
    }
    return true;
}

static bool test_pci(char *err, int errlen) {
    int n = pci_device_count();
    if (n <= 0) {
        ksnprintf(err, errlen, "no PCI devices enumerated");
        return false;
    }
    return true;
}

static bool test_ata(char *err, int errlen) {
    UNUSED(err); UNUSED(errlen);
    /* A missing disk is valid: QEMU is often started without one. The test
       verifies that the controller probe completed without hanging. */
    const ata_disk_t *d = ata_get_disk();
    if (ata_present() && d) {
        if (d->sectors == 0) {
            ksnprintf(err, errlen, "disk reported zero sectors");
            return false;
        }
    }
    return true;
}

static bool test_icmp(char *err, int errlen) {
    if (!net_is_up()) {
        ksnprintf(err, errlen, "network is not configured");
        return false;
    }
    char gw[20];
    net_format_ip(net_info()->gateway, gw);
    if (net_ping(net_info()->gateway, 1) != 0) {
        ksnprintf(err, errlen, "no ICMP reply from gateway %s", gw);
        return false;
    }
    return true;
}

static bool test_vga(char *err, int errlen) {
    u16 *buf = console_buffer();
    if (!buf || (u32)buf != 0xB8000u) {
        ksnprintf(err, errlen, "console buffer is not the VGA text buffer");
        return false;
    }
    return true;
}

static const test_desc_t tests[] = {
    { "GDT segments",        gdt_selftest },
    { "IDT gates",           idt_selftest },
    { "PIT timer",           pit_selftest },
    { "IRQ delivery (PIC)",  test_irq },
    { "Physical memory",     pmm_selftest },
    { "Kernel heap",         heap_selftest },
    { "RAMFS",               ramfs_selftest },
    { "Task registry",       task_selftest },
    { "Shell parser",        shell_selftest },
    { "Store archive",       test_store },
    { "PS/2 keyboard",       keyboard_selftest },
    { "PS/2 mouse",          mouse_selftest },
    { "PCI enumeration",     test_pci },
    { "ATA/IDE probe",       test_ata },
    { "Network + DHCP",      net_selftest },
    { "ICMP gateway",        test_icmp },
    { "VGA console",         test_vga },
    { "GUI renderer",        gui_selftest },
};

#define TEST_COUNT ((int)(sizeof(tests) / sizeof(tests[0])))

static void line(char c, int n) {
    char buf[80];
    if (n > 79) n = 79;
    memset(buf, c, (size_t)n);
    buf[n] = 0;
    kprintf("%s\n", buf);
}

int selftest_run(bool autotest) {
    int passed = 0, failed = 0;
    did_run = true;

    kprintf("\n");
    line('=', 66);
    kprintf("  Minimal Desktop selftest  -  %s %s\n", OS_VERSION, OS_CODENAME);
    if (autotest) kprintf("  mode: non-interactive (--autotest)\n");
    line('=', 66);

    for (int i = 0; i < TEST_COUNT; i++) {
        char err[128];
        err[0] = 0;
        kprintf("  [%2d/%2d] %-20s ", i + 1, TEST_COUNT, tests[i].name);
        bool ok = tests[i].fn(err, sizeof(err));
        if (ok) {
            kprintf("PASS\n");
            passed++;
        } else {
            kprintf("FAIL  %s\n", err[0] ? err : "(no detail)");
            failed++;
        }
    }

    line('-', 66);
    kprintf("  %d checks: %d passed, %d failed\n", TEST_COUNT, passed, failed);
    line('=', 66);

    last_passes = passed;
    last_failures = failed;

    if (failed == 0) {
        kprintf("RESULT: PASS\n");
        serial_puts("[selftest] RESULT: PASS\r\n");
    } else {
        kprintf("RESULT: FAIL\n");
        serial_puts("[selftest] RESULT: FAIL\r\n");
    }

    if (autotest) {
        kprintf("[autotest] writing isa-debug-exit at 0x%X (%s)\n",
                ISA_DEBUG_EXIT, failed == 0 ? "0x10" : "0x11");
        outl(ISA_DEBUG_EXIT, failed == 0 ? 0x10 : 0x11);
        for (;;) hlt();
    }
    return failed;
}

int selftest_last_failures(void) { return last_failures; }
int selftest_last_passes(void) { return last_passes; }
bool selftest_did_run(void) { return did_run; }
