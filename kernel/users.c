/* Minimal Desktop - multi-user accounts and permissions
 *
 * Accounts live in /disk/users.cfg as: name uid can_sudo password
 * On a live CD (no /disk) a single ROOT/root account is used.
 */
#include "users.h"
#include "console.h"
#include "keyboard.h"
#include "ramfs.h"
#include "fat.h"
#include "string.h"
#include "pit.h"
#include "isr.h"
#include "extras.h"

static user_t users[16];
static int user_count;
static int current_user;
static bool elevated;
static bool initialized;

static void default_users(void) {
    user_count = 0;
    memset(users, 0, sizeof(users));
    user_t *r = &users[user_count++];
    strncpy(r->name, "ROOT", 23);
    strncpy(r->pass, "root", 23);
    r->uid = 0;
    r->can_sudo = true;
    current_user = 0;
}

void users_init(void) {
    if (initialized) return;
    initialized = true;
    default_users();

    fs_node_t *f = ramfs_lookup("/disk/users.cfg");
    if (!f || f->type != FS_FILE) return;

    char buf[2048];
    int n = ramfs_read(f, buf, sizeof(buf) - 1, 0);
    buf[n > 0 ? n : 0] = 0;
    user_count = 0;
    char *p = buf;
    while (*p && user_count < 16) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        char *sp1 = strchr(p, ' ');
        if (sp1) {
            *sp1 = 0;
            char *sp2 = strchr(sp1 + 1, ' ');
            if (sp2) {
                *sp2 = 0;
                char *sp3 = strchr(sp2 + 1, ' ');
                if (sp3) {
                    *sp3 = 0;
                    user_t *u = &users[user_count];
                    strncpy(u->name, p, 23);
                    u->uid = (u32)atoi(sp1 + 1);
                    u->can_sudo = atoi(sp2 + 1) != 0;
                    strncpy(u->pass, sp3 + 1, 23);
                    user_count++;
                }
            }
        }
        if (!nl) break;
        p = nl + 1;
    }
    if (user_count == 0) default_users();
    current_user = 0;
}

int users_save(void) {
    if (!ramfs_lookup("/disk")) return -1;
    if (ramfs_lookup("/disk/users.cfg")) ramfs_unlink("/disk/users.cfg");
    fs_node_t *f = ramfs_create("/disk/users.cfg", FS_FILE);
    if (!f) return -1;
    char line[128];
    for (int i = 0; i < user_count; i++) {
        user_t *u = &users[i];
        ksnprintf(line, sizeof(line), "%s %u %d %s\n",
                  u->name, u->uid, u->can_sudo ? 1 : 0, u->pass);
        ramfs_append(f, line, (u32)strlen(line));
    }
    return 0;
}

int users_count(void) { return user_count; }
user_t *users_get(int i) { return (i >= 0 && i < user_count) ? &users[i] : NULL; }
user_t *users_current(void) { return &users[current_user]; }
bool users_is_root(void) { return elevated || users[current_user].uid == 0; }

user_t *users_find(const char *name) {
    for (int i = 0; i < user_count; i++)
        if (strcasecmp(users[i].name, name) == 0) return &users[i];
    return NULL;
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

user_t *users_login(void) {
    users_init();
    console_clear();
    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("  ______________________________________________________\n");
    kprintf(" |  MINIMAL DESKTOP 1.8.2  -  SECURE LOGIN              |\n");
    kprintf(" |______________________________________________________|\n\n");
    if (!ramfs_lookup("/disk")) {
        console_set_color(VGA_YELLOW, VGA_BLACK);
        kprintf("  LIVE CD: username = ROOT, password = root\n\n");
        console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    }

    int attempts = 0;
    while (attempts < 3) {
        char name[32], pass[32];
        console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        kprintf("  username: ");
        read_line(name, sizeof(name), false);
        kprintf("  password: ");
        read_line(pass, sizeof(pass), true);
        user_t *u = users_find(name);
        if (u && strcmp(u->pass, pass) == 0) {
            current_user = (int)(u - users);
            elevated = (u->uid == 0);
            console_set_color(VGA_GREEN, VGA_BLACK);
            kprintf("\n  Welcome, %s%s.\n\n", u->name, u->uid == 0 ? " (root)" : "");
            console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            return u;
        }
        attempts++;
        console_set_color(VGA_RED, VGA_BLACK);
        kprintf("\n  Access denied (%d/3)\n\n", attempts);
        console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    }
    console_clear();
    console_set_color(VGA_RED, VGA_BLACK);
    kprintf("  Too many failed attempts. System halted.\n");
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    for (;;) __asm__ volatile("hlt");
    return NULL;
}

int users_add(const char *name, const char *pass, bool can_sudo) {
    if (!name[0]) return -1;
    if (users_find(name)) { kprintf("useradd: %s already exists\n", name); return -1; }
    if (user_count >= 16) { kprintf("useradd: too many users\n"); return -1; }
    user_t *u = &users[user_count++];
    memset(u, 0, sizeof(*u));
    strncpy(u->name, name, 23);
    strncpy(u->pass, pass, 23);
    u->uid = (u32)(1000 + user_count);
    u->can_sudo = can_sudo;
    users_save();
    kprintf("useradd: created %s (uid %u)\n", name, u->uid);
    return 0;
}

int users_del(const char *name) {
    if (!users_is_root()) { kprintf("userdel: root only\n"); return -1; }
    user_t *u = users_find(name);
    if (!u) { kprintf("userdel: %s not found\n", name); return -1; }
    if (u->uid == 0) { kprintf("userdel: cannot delete root\n"); return -1; }
    int idx = (int)(u - users);
    memmove(&users[idx], &users[idx + 1], (size_t)(user_count - idx - 1) * sizeof(user_t));
    user_count--;
    users_save();
    kprintf("userdel: removed %s\n", name);
    return 0;
}

int users_passwd(const char *name, const char *newpass) {
    user_t *u = users_find(name);
    if (!u) return -1;
    if (u->uid != 0 && !users_is_root() && u != users_current()) {
        kprintf("passwd: permission denied\n"); return -1;
    }
    strncpy(u->pass, newpass, 23);
    users_save();
    kprintf("passwd: password updated for %s\n", name);
    return 0;
}

bool users_can_run(const char *cmd) {
    /* commands that require root privileges */
    static const char *restricted[] = {
        "install", "format", "chkdsk", "useradd", "userdel", "passwd",
        "reboot", "shutdown", "mount", "sync", "restore", "backup",
        "crash", "dumpfont", NULL
    };
    for (int i = 0; restricted[i]; i++)
        if (strcasecmp(cmd, restricted[i]) == 0) return users_is_root();
    return true;
}

void users_require_root(const char *cmd) {
    if (!users_can_run(cmd)) {
        kprintf("%s: permission denied (root only). Use 'sudo %s'.\n", cmd, cmd);
    }
}

bool users_sudo_auth(void) {
    user_t *u = users_current();
    if (u->uid == 0) { elevated = true; return true; }
    if (!u->can_sudo) { kprintf("sudo: user %s is not in the sudoers list\n", u->name); return false; }
    char pass[32];
    kprintf("[sudo] password for %s: ", u->name);
    read_line(pass, sizeof(pass), true);
    if (strcmp(pass, u->pass) == 0) {
        elevated = true;
        kprintf("sudo: access granted\n");
        return true;
    }
    kprintf("sudo: incorrect password\n");
    return false;
}

void users_drop_elevation(void) { elevated = false; }
