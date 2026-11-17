/* Minimal Desktop - interactive text shell */
#include "shell.h"
#include "console.h"
#include "string.h"
#include "ramfs.h"
#include "pit.h"
#include "keyboard.h"
#include "task.h"
#include "store.h"
#include "fat.h"

static const command_t commands[] = {
    { "help",     "System",   "help [command]",           "list commands or show usage",        cmd_help },
    { "sysinfo",  "System",   "sysinfo",                  "show OS, CPU, memory and device info", cmd_sysinfo },
    { "uptime",   "System",   "uptime",                   "time since boot",                    cmd_uptime },
    { "uname",    "System",   "uname",                    "kernel and build identification",    cmd_uname },
    { "drivers",  "System",   "drivers",                  "list loaded drivers and devices",    cmd_drivers },
    { "hwinfo",   "System",   "hwinfo",                   "detect PCI/GPU/NIC/VM hardware",      cmd_hwinfo },
    { "taskmng",  "System",   "taskmng [kill <pid>]",     "task manager",                       cmd_taskmng },
    { "selftest", "System",   "selftest [--autotest]",    "run the built-in hardware tests",    cmd_selftest },
    { "clear",    "System",   "clear",                    "clear the screen",                   cmd_clear },
    { "reboot",   "System",   "reboot",                   "restart the machine",                cmd_reboot },
    { "shutdown", "System",   "shutdown",                 "power off the machine (ACPI)",       cmd_shutdown },
    { "date",     "Clock",    "date",                     "show the current date",              cmd_date },
    { "time",     "Clock",    "time",                     "show the current time",              cmd_time },
    { "ls",       "Files",    "ls [path]",                "list a directory",                   cmd_ls },
    { "cd",       "Files",    "cd [path]",                "change directory",                   cmd_cd },
    { "pwd",      "Files",    "pwd",                      "print working directory",            cmd_pwd },
    { "mkdir",    "Files",    "mkdir <dir>",              "create a directory",                 cmd_mkdir },
    { "touch",    "Files",    "touch <file>",             "create an empty file",               cmd_touch },
    { "rm",       "Files",    "rm <path>",                "remove a file or directory",         cmd_rm },
    { "cat",      "Files",    "cat <file>",               "print a file",                       cmd_cat },
    { "edit",     "Files",    "edit <file>",              "full-screen text editor",            cmd_edit },
    { "new",      "Files",    "new <file>",               "create and edit a new text file",     cmd_new },
    { "cp",       "Files",    "cp <src> <dst>",           "copy a file",                        cmd_cp },
    { "mount",    "Files",    "mount",                    "mount the disk data partition",      cmd_mount },
    { "cdrom",    "Files",    "cdrom",                    "mount the boot CD at /cd",           cmd_cdrom },
    { "sync",     "Files",    "sync",                     "flush disk file changes",            cmd_sync },
    { "format",   "Files",    "format /disk",             "erase and format the data partition",cmd_format },
    { "mv",       "Files",    "mv <src> <dst>",           "move or rename a file",              cmd_mv },
    { "tree",     "Files",    "tree [dir]",               "recursive directory tree",           cmd_tree },
    { "du",       "Files",    "du [dir]",                 "disk usage for a directory",         cmd_du },
    { "find",     "Files",    "find <name> [dir]",        "find files by name",                 cmd_find },
    { "less",     "Files",    "less <file>",              "scroll through a text file",         cmd_read },
    { "read",     "Files",    "read <file>",              "alias for less",                     cmd_read },
    { "write",    "Files",    "write <file> <text>",      "replace a file with text",           cmd_write },
    { "echo",     "Files",    "echo <text>",              "print text",                         cmd_echo },
    { "memmap",   "Files",    "memmap",                   "physical memory map and heap stats", cmd_memmap },
    { "hexdump",  "Files",    "hexdump <file> [bytes]",   "hex + ASCII dump of a file",         cmd_hexdump },
    { "df",       "Files",    "df",                       "filesystem usage",                   cmd_df },
    { "calc",     "Tools",    "calc <expression>",        "integer calculator",                 cmd_calc },
    { "theme",    "Tools",    "theme <name|fg> [bg]",     "change console colours",             cmd_theme },
    { "matrix",   "Tools",    "matrix [seconds]",         "digital rain effect",                cmd_matrix },
    { "saver",    "Tools",    "saver <mode> [sec]",        "screen saver (0-3)",                  cmd_saver },
    { "play",     "Tools",    "play <notes.txt>",         "play notes via PC speaker",            cmd_play },
    { "backup",   "Files",    "backup <file>",            "back up RAMFS to a /disk file",        cmd_backup },
    { "restore",  "Files",    "restore <file>",           "restore a RAMFS backup",               cmd_restore },
    { "weather",  "Network",  "weather",                  "fetch current weather over HTTP",      cmd_weather },
    { "lynx",     "Network",  "lynx <url>",              "text-mode web browser",               cmd_lynx },
    { "slideshow", "Graphics", "slideshow [sec]",           "full-screen image slideshow",         cmd_slideshow },
    { "chkdsk",   "System",   "chkdsk",                    "check the FAT disk filesystem",       cmd_chkdsk },
    { "beep",     "Tools",    "beep [Hz] [ms]",           "PC speaker tone",                    cmd_beep },
    { "credits",  "Tools",    "credits",                  "about the project",                  cmd_credits },
    { "store",      "Packages", "store list|install|remove|info|repo|status", "package manager", cmd_store },
    { "minipacket", "Packages", "minipacket -L | -I <pkg> | -R <pkg> | -S <pkg>",     "package manager (install: -I)", cmd_minipacket },
    { "snake",      "Games",    "snake",                    "classic snake (needs module)",       cmd_snake },
    { "pong",       "Games",    "pong",                     "pong vs CPU (needs module)",         cmd_pong },
    { "doom",       "Games",    "doom",                     "DOOM raycaster (needs module)",      cmd_doom },
    { "startgui",   "Graphics", "startgui",                 "graphical desktop (needs module)",   cmd_startgui },
    { "imgview",    "Graphics", "imgview",                  "open the GUI image viewer",          cmd_imgview },
    { "miniland",   "Graphics", "miniland",                 "Miniland tiling WM (needs module)",  cmd_miniland },
    { "keymap",     "System",   "keymap list|next|<layout>", "switch keyboard layout (Alt+Shift)", cmd_keymap },
    { "crash",      "System",   "crash",                    "deliberately fault (crash screen test)", cmd_crash },
    { "install",    "System",   "install",                  "write the OS image to the first ATA disk", cmd_install },
    { "ifconfig",   "Network",  "ipconfig [dhcp|set ...]",  "show or configure the network",      cmd_ifconfig },
    { "ipconfig",   "Network",  "ipconfig [dhcp|set ...]",  "alias for ifconfig",                 cmd_ifconfig },
    { "ping",     "Network",  "ping <ip|host> [count]",   "ICMP echo request",                  cmd_ping },
    { "nslookup", "Network",  "nslookup <host>",          "resolve a DNS name",                 cmd_nslookup },
    { "curl",     "Network",  "curl <http://url>",        "fetch an HTTP resource",             cmd_curl },
    { "netstat",  "Network",  "netstat",                  "network statistics",                 cmd_netstat },
};

#define COMMAND_COUNT ((int)(sizeof(commands) / sizeof(commands[0])))
#define LINE_MAX 256
#define HISTORY_MAX 8

static char linebuf[LINE_MAX];
static char history[HISTORY_MAX][LINE_MAX];
static int history_count;
static int history_pos;

/* ------------------------------------------------------------------------- */
/* parsing                                                                   */
/* ------------------------------------------------------------------------- */
int shell_tokenize(char *line, char **argv, int max) {
    int argc = 0;
    char *p = line;
    while (*p && argc < max) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (*p == '#') break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (!*p) break;
        *p++ = 0;
    }
    return argc;
}

const command_t *shell_find_command(const char *name) {
    for (int i = 0; i < COMMAND_COUNT; i++)
        if (strcasecmp(commands[i].name, name) == 0) return &commands[i];
    return NULL;
}

const command_t *shell_command_table(int *count) {
    if (count) *count = COMMAND_COUNT;
    return commands;
}

const char *shell_cwd_path(void) {
    static char path[160];
    return ramfs_path(ramfs_cwd(), path, sizeof(path));
}

int shell_execute(const char *line) {
    char copy[LINE_MAX];
    strncpy(copy, line, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = 0;
    char *argv[24];
    int argc = shell_tokenize(copy, argv, 24);
    if (argc == 0) return 0;
    const command_t *c = shell_find_command(argv[0]);
    if (!c) {
        kprintf("%s: command not found. Try 'help'.\n", argv[0]);
        return -1;
    }
    int rc = c->fn(argc, argv);
    return rc;
}

int shell_execute_silent(const char *line) {
    static char sink[512];
    console_capture_begin(sink, sizeof(sink));
    int rc = shell_execute(line);
    console_capture_end();
    return rc;
}

/* ------------------------------------------------------------------------- */
/* banner                                                                    */
/* ------------------------------------------------------------------------- */
void shell_banner(void) {
    /* Fresh screen so the wordmark is not painted over the kernel boot log
     * (that log is still available on COM1 and via 'dmesg'). */
    console_clear();
    int col = (80 - console_bootart_cols()) / 2;
    if (col < 0) col = 0;
    console_draw_bootart(1, col);

    console_gotoxy(8, 0);
    kprintf("  %s %s \"%s\"  -  %s\n", OS_NAME, OS_VERSION, OS_CODENAME, OS_ARCH);
    kprintf("  %s\n", OS_BUILD);
    kprintf("\n");
    kprintf("  Type 'help' for commands, 'selftest' to test hardware, 'hwinfo' for devices.\n");
    kprintf("  Packages: minipacket -I minimal-desktop   (GUI desktop)\n");
    kprintf("            minipacket -I miniland          (tiling WM)\n");
    kprintf("            minipacket -I doom              (raycaster FPS)\n");
    kprintf("\n");

    u8 fg, bg;
    console_get_theme(&fg, &bg);
    console_set_color(VGA_GREEN, bg);
    kprintf("  boot ok: VGA text, PIT %u Hz, RAMFS mounted, store %d packages\n",
            pit_get_frequency(), store_package_count());
    console_reset_color();
    kprintf("\n");
}

/* ------------------------------------------------------------------------- */
/* interactive loop                                                          */
/* ------------------------------------------------------------------------- */
static void prompt(void) {
    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("minimal@os");
    console_set_color(VGA_WHITE, VGA_BLACK);
    kprintf(":");
    console_set_color(VGA_LIGHT_BLUE, VGA_BLACK);
    kprintf("%s", shell_cwd_path());
    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("$ ");
    console_reset_color();
}

static void history_add(const char *line) {
    if (!line[0]) return;
    if (history_count > 0 &&
        strcmp(history[(history_count - 1) % HISTORY_MAX], line) == 0) return;
    strncpy(history[history_count % HISTORY_MAX], line, LINE_MAX - 1);
    history[history_count % HISTORY_MAX][LINE_MAX - 1] = 0;
    history_count++;
}

static void set_line(const char *src) {
    int cur = (int)strlen(linebuf);
    while (cur-- > 0) {
        kprintf("\b \b");
    }
    strncpy(linebuf, src, LINE_MAX - 1);
    linebuf[LINE_MAX - 1] = 0;
    kprintf("%s", linebuf);
}

void shell_run(void) {
    int len = 0;
    u32 layout_seen = kbd_layout_generation();
    linebuf[0] = 0;
    history_pos = history_count;
    prompt();

    for (;;) {
        pit_sleep_ms(4);
        int key = kbd_getchar();
        if (kbd_layout_generation() != layout_seen) {
            layout_seen = kbd_layout_generation();
            kprintf("\n  [keymap] %s (%s)\n", kbd_layout_label(), kbd_layout_name());
            kprintf("%s%s", shell_cwd_path(), "$ ");
            kprintf("%s", linebuf);
        }
        if (key < 0) continue;

        if (key == KEY_ENTER) {
            kprintf("\n");
            linebuf[len] = 0;
            history_add(linebuf);
            history_pos = history_count;
            if (len > 0) {
                shell_execute(linebuf);
                fat_sync();          /* write /disk changes back to the FAT partition */
            }
            len = 0;
            linebuf[0] = 0;
            prompt();
        } else if (key == KEY_BACKSPACE) {
            if (len > 0) {
                linebuf[--len] = 0;
                kprintf("\b \b");
            }
        } else if (key == KEY_UP) {
            if (history_count > 0 && history_pos > 0) {
                history_pos--;
                char tmp[LINE_MAX];
                strncpy(tmp, history[history_pos % HISTORY_MAX], LINE_MAX - 1);
                tmp[LINE_MAX - 1] = 0;
                set_line(tmp);
                len = (int)strlen(linebuf);
            }
        } else if (key == KEY_DOWN) {
            if (history_pos < history_count) {
                history_pos++;
                if (history_pos == history_count) set_line("");
                else {
                    char tmp[LINE_MAX];
                    strncpy(tmp, history[history_pos % HISTORY_MAX], LINE_MAX - 1);
                    tmp[LINE_MAX - 1] = 0;
                    set_line(tmp);
                }
                len = (int)strlen(linebuf);
            }
        } else if (key == KEY_TAB) {
            if (len < LINE_MAX - 2) {
                linebuf[len++] = ' ';
                linebuf[len] = 0;
                kprintf(" ");
            }
        } else if (key >= 0x20 && key < 0x7F) {
            if (len < LINE_MAX - 2) {
                linebuf[len++] = (char)key;
                linebuf[len] = 0;
                kprintf("%c", (char)key);
            }
        }
    }
}

void shell_init(void) {
    ramfs_set_cwd(ramfs_lookup("/home/minimal"));
    linebuf[0] = 0;
    history_count = 0;
    history_pos = 0;
    memset(history, 0, sizeof(history));
    task_register("shell", 48, false);
}

bool shell_selftest(char *err, int errlen) {
    char text[] = "echo  hello   world";
    char *argv[8];
    int argc = shell_tokenize(text, argv, 8);
    if (argc != 3) {
        ksnprintf(err, errlen, "tokenizer returned %d args, expected 3", argc);
        return false;
    }
    if (strcmp(argv[0], "echo") != 0 || strcmp(argv[2], "world") != 0) {
        ksnprintf(err, errlen, "tokenizer split arguments incorrectly");
        return false;
    }
    if (!shell_find_command("sysinfo") || !shell_find_command("startgui")) {
        ksnprintf(err, errlen, "command table is incomplete");
        return false;
    }
    if (shell_execute_silent("echo selftest") != 0) {
        ksnprintf(err, errlen, "silent command execution failed");
        return false;
    }
    const char *cwd = shell_cwd_path();
    if (!cwd || cwd[0] != '/') {
        ksnprintf(err, errlen, "cwd path is invalid");
        return false;
    }
    return true;
}
