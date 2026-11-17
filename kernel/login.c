/* Minimal Desktop - simple login gate with users stored on /disk */
#include "extras.h"
#include "console.h"
#include "keyboard.h"
#include "ramfs.h"
#include "string.h"
#include "fat.h"
#include "pit.h"
#include "isr.h"

typedef struct { char name[24]; char pass[24]; } user_t;
static user_t users[8];
static int user_count;

static void load_users(void) {
    user_count = 0;
    fs_node_t *f = ramfs_lookup("/disk/users.cfg");
    if (!f || f->type != FS_FILE) {
        /* default user */
        strncpy(users[0].name, "user", 23);
        strncpy(users[0].pass, "password", 23);
        user_count = 1;
        return;
    }
    char buf[1024];
    int n = ramfs_read(f, buf, sizeof(buf) - 1, 0);
    buf[n > 0 ? n : 0] = 0;
    char *p = buf;
    while (*p && user_count < 8) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        char *sp = strchr(p, ' ');
        if (sp) {
            *sp = 0;
            strncpy(users[user_count].name, p, 23);
            strncpy(users[user_count].pass, sp + 1, 23);
            user_count++;
        }
        if (!nl) break;
        p = nl + 1;
    }
    if (user_count == 0) {
        strncpy(users[0].name, "user", 23);
        strncpy(users[0].pass, "password", 23);
        user_count = 1;
    }
}

static int read_line(char *buf, int max, bool hide) {
    int len = 0;
    for (;;) {
        int k = kbd_getchar();
        if (k < 0) { pit_sleep_ms(4); continue; }
        if (k == KEY_ENTER) { buf[len] = 0; kprintf("\n"); return len; }
        if (k == KEY_BACKSPACE) {
            if (len > 0) { len--; if (!hide) kprintf("\b \b"); }
            continue;
        }
        if (k >= 32 && k < 127 && len < max - 1) {
            buf[len++] = (char)k;
            if (!hide) kprintf("%c", (char)k);
        }
    }
}

int login_run(void) {
    load_users();
    console_clear();
    int attempts = 0;
    while (attempts < 3) {
        char user[32], pass[32];
        kprintf("  === Minimal Desktop Login ===\n\n");
        kprintf("  Username: ");
        read_line(user, sizeof(user), false);
        kprintf("  Password: ");
        read_line(pass, sizeof(pass), true);
        for (int i = 0; i < user_count; i++) {
            if (strcmp(user, users[i].name) == 0 && strcmp(pass, users[i].pass) == 0) {
                kprintf("\n  Welcome, %s!\n\n", users[i].name);
                return 0;
            }
        }
        attempts++;
        kprintf("\n  Invalid credentials. Attempt %d/3\n\n", attempts);
    }
    kprintf("  Too many attempts. Halting.\n");
    for (;;) __asm__ volatile("hlt");
}
