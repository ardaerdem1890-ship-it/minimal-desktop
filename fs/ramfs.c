/* Minimal Desktop - in-memory RAM filesystem */
#include "ramfs.h"
#include "mm.h"
#include "console.h"
#include "string.h"

static fs_node_t *root;
static fs_node_t *cwd;
static u32 next_inode = 1;
static u32 node_count;
static u32 bytes_used;

static fs_node_t *node_alloc(const char *name, u8 type, fs_node_t *parent) {
    fs_node_t *n = (fs_node_t *)kzalloc(sizeof(fs_node_t));
    if (!n) return NULL;
    strncpy(n->name, name, FS_NAME_MAX - 1);
    n->type = type;
    n->parent = parent;
    n->inode = next_inode++;
    node_count++;
    return n;
}

static fs_node_t *find_child(fs_node_t *dir, const char *name) {
    if (!dir || dir->type != FS_DIR) return NULL;
    for (fs_node_t *c = dir->children; c; c = c->next)
        if (strcmp(c->name, name) == 0) return c;
    return NULL;
}

static void attach_child(fs_node_t *parent, fs_node_t *child) {
    child->parent = parent;
    child->next = NULL;
    if (!parent->children) {
        parent->children = child;
        return;
    }
    fs_node_t *last = parent->children;
    while (last->next) last = last->next;
    last->next = child;
}

static bool split_parent(const char *path, char *parent_out, int psize, char *leaf_out, int lsize) {
    const char *last = strrchr(path, '/');
    if (!last) {
        strncpy(parent_out, ".", psize);
        strncpy(leaf_out, path, lsize);
        return true;
    }
    if (last == path) {
        strncpy(parent_out, "/", psize);
    } else {
        int n = (int)(last - path);
        if (n >= psize) n = psize - 1;
        memcpy(parent_out, path, (size_t)n);
        parent_out[n] = 0;
    }
    strncpy(leaf_out, last + 1, lsize);
    return true;
}

static fs_node_t *resolve(const char *path, bool create, u8 type) {
    if (!path || !*path) return NULL;
    char work[256];
    strncpy(work, path, sizeof(work) - 1);
    work[sizeof(work) - 1] = 0;

    fs_node_t *cur = (work[0] == '/') ? root : cwd;
    char *p = work;
    while (*p == '/') p++;
    if (!*p) return cur;

    while (*p) {
        char *slash = strchr(p, '/');
        char token[FS_NAME_MAX];
        size_t len = 0;
        if (slash) {
            len = (size_t)(slash - p);
            if (len >= sizeof(token)) return NULL;
            memcpy(token, p, len);
            token[len] = 0;
            p = slash + 1;
            while (*p == '/') p++;
        } else {
            strncpy(token, p, sizeof(token) - 1);
            token[sizeof(token) - 1] = 0;
            p += strlen(p);
        }

        if (strcmp(token, ".") == 0 || token[0] == 0) continue;
        if (strcmp(token, "..") == 0) {
            cur = cur->parent ? cur->parent : root;
            continue;
        }
        if (cur->type != FS_DIR) return NULL;

        fs_node_t *child = find_child(cur, token);
        bool last = (*p == 0);
        if (!child) {
            if (!create || !last) return NULL;
            child = node_alloc(token, type, cur);
            if (!child) return NULL;
            attach_child(cur, child);
        }
        cur = child;
    }
    return cur;
}

static void free_node(fs_node_t *n) {
    if (!n) return;
    fs_node_t *c = n->children;
    while (c) {
        fs_node_t *next = c->next;
        free_node(c);
        c = next;
    }
    if (n->data) {
        bytes_used -= n->size;
        kfree(n->data);
    }
    if (n != root) {
        node_count--;
        kfree(n);
    }
}

static void write_file(fs_node_t *dir, const char *name, const char *text) {
    fs_node_t *f = find_child(dir, name);
    if (!f) {
        f = node_alloc(name, FS_FILE, dir);
        if (!f) return;
        attach_child(dir, f);
    }
    fs_node_t unused;
    (void)unused;
    ramfs_write(f, text, (u32)strlen(text), 0);
}

void ramfs_init(void) {
    root = NULL;
    cwd = NULL;
    next_inode = 1;
    node_count = 0;
    bytes_used = 0;

    root = node_alloc("/", FS_DIR, NULL);
    if (!root) kpanic("RAMFS: cannot allocate root node");
    cwd = root;

    fs_node_t *home = ramfs_create("/home", FS_DIR);
    fs_node_t *minimal = ramfs_create("/home/minimal", FS_DIR);
    ramfs_create("/usr", FS_DIR);
    ramfs_create("/usr/bin", FS_DIR);
    ramfs_create("/usr/packages", FS_DIR);
    ramfs_create("/etc", FS_DIR);
    ramfs_create("/tmp", FS_DIR);

    write_file(root, "readme.txt",
        "Minimal Desktop 1.0.0 - a from-scratch x86 hobby operating system.\n"
        "Type 'help' for the command list, 'selftest' to verify the kernel.\n"
        "Install optional modules with: minipacket -I snake  (or store install)\n"
        "Desktop environment: minipacket -I minimal-desktop, then startgui\n"
        "Keyboard layout: keymap <us|tr-q|uk|de>\n");
    write_file(home, "README.txt", "Home directory of user minimal.\n");
    write_file(minimal, "welcome.txt",
        "Welcome to Minimal Desktop!\n\n"
        "This desktop and shell are running directly on bare metal (well,\n"
        "on QEMU for now) with no underlying operating system.\n\n"
        "Try the shell commands:\n"
        "  sysinfo, drivers, uptime, memmap, taskmng\n"
        "  minipacket -I snake   (or: store install snake), then snake\n"
        "  minipacket -I doom, then doom\n"
        "  minipacket -I minimal-desktop, then startgui\n"
        "  keymap tr-q   (layouts: us, tr-q, uk, de)\n"
        "  ifconfig, ping 10.0.2.2, nslookup example.com\n"
        "  curl http://example.com/\n\n"
        "The network stack speaks Ethernet, ARP, IPv4, ICMP, UDP, DHCP,\n"
        "DNS and a small TCP client.\n");
    write_file(minimal, "notes.txt",
        "Retro notes\n"
        "-----------\n"
        "- VGA text: 80x25, 16 colours, hardware scrolling\n"
        "- GUI: VGA mode 13h, 320x200x256\n"
        "- Timer: PIT channel 0 at 100 Hz\n"
        "- Packages are verified with FNV-1a checksums\n");
    write_file(ramfs_lookup("/etc"), "hostname", "minimal\n");
    write_file(ramfs_lookup("/etc"), "motd", "minimal@os - boot fast, stay retro.\n");

    cwd = minimal;
}

fs_node_t *ramfs_root(void) { return root; }
fs_node_t *ramfs_cwd(void) { return cwd; }
void ramfs_set_cwd(fs_node_t *node) { if (node && node->type == FS_DIR) cwd = node; }

fs_node_t *ramfs_lookup(const char *path) {
    return resolve(path, false, 0);
}

fs_node_t *ramfs_create(const char *path, u8 type) {
    if (!path || !*path) return NULL;
    char parent_path[256], leaf[FS_NAME_MAX];
    if (!split_parent(path, parent_path, sizeof(parent_path), leaf, sizeof(leaf))) return NULL;
    if (!leaf[0]) return NULL;
    fs_node_t *parent = resolve(parent_path, false, 0);
    if (!parent || parent->type != FS_DIR) return NULL;
    if (find_child(parent, leaf)) return NULL;
    fs_node_t *n = node_alloc(leaf, type, parent);
    if (!n) return NULL;
    attach_child(parent, n);
    return n;
}

fs_node_t *ramfs_parent_of(const char *path) {
    char parent_path[256], leaf[FS_NAME_MAX];
    if (!split_parent(path, parent_path, sizeof(parent_path), leaf, sizeof(leaf))) return NULL;
    return resolve(parent_path, false, 0);
}

int ramfs_read(fs_node_t *node, void *buf, u32 len, u32 offset) {
    if (!node || node->type != FS_FILE) return -1;
    if (offset >= node->size) return 0;
    u32 avail = node->size - offset;
    if (len > avail) len = avail;
    memcpy(buf, node->data + offset, len);
    return (int)len;
}

int ramfs_write(fs_node_t *node, const void *buf, u32 len, u32 offset) {
    if (!node || node->type != FS_FILE) return -1;
    if (offset + len > FS_MAX_FILE) return -2;
    u32 need = offset + len;
    if (need > node->capacity) {
        u32 newcap = (need + 127) & ~127u;
        if (newcap > FS_MAX_FILE) newcap = FS_MAX_FILE;
        u8 *nd = (u8 *)kmalloc(newcap);
        if (!nd) return -3;
        if (node->data) {
            memcpy(nd, node->data, node->size < newcap ? node->size : newcap);
            kfree(node->data);
        }
        node->data = nd;
        node->capacity = newcap;
    }
    bytes_used -= node->size;
    memcpy(node->data + offset, buf, len);
    if (need > node->size) node->size = need;
    bytes_used += node->size;
    return (int)len;
}

int ramfs_append(fs_node_t *node, const void *buf, u32 len) {
    if (!node) return -1;
    return ramfs_write(node, buf, len, node->size);
}

int ramfs_truncate(fs_node_t *node, u32 size) {
    if (!node || node->type != FS_FILE) return -1;
    bytes_used -= node->size;
    if (size == 0) {
        if (node->data) kfree(node->data);
        node->data = NULL;
        node->capacity = 0;
        node->size = 0;
        return 0;
    }
    if (size > node->capacity) {
        u8 *nd = (u8 *)kzalloc(size);
        if (!nd) { bytes_used += node->size; return -2; }
        if (node->data) {
            memcpy(nd, node->data, node->size < size ? node->size : size);
            kfree(node->data);
        }
        node->data = nd;
        node->capacity = size;
    }
    node->size = size;
    bytes_used += node->size;
    return 0;
}

int ramfs_unlink(const char *path) {
    fs_node_t *n = ramfs_lookup(path);
    if (!n || n == root) return -1;
    if (n == cwd) cwd = n->parent ? n->parent : root;
    fs_node_t *p = n->parent;
    if (!p) return -1;
    if (p->children == n) p->children = n->next;
    else {
        fs_node_t *prev = p->children;
        while (prev && prev->next != n) prev = prev->next;
        if (!prev) return -1;
        prev->next = n->next;
    }
    free_node(n);
    return 0;
}

int ramfs_readdir(fs_node_t *dir, int index, fs_node_t **out) {
    if (!dir || dir->type != FS_DIR) return -1;
    fs_node_t *c = dir->children;
    for (int i = 0; c && i < index; i++) c = c->next;
    if (!c) return -1;
    *out = c;
    return 0;
}

char *ramfs_path(fs_node_t *node, char *buf, int max) {
    if (!node) { strncpy(buf, "?", (size_t)max); return buf; }
    if (node == root) { strncpy(buf, "/", (size_t)max); return buf; }
    char tmp[512];
    int pos = 0;
    fs_node_t *n = node;
    tmp[0] = 0;
    while (n && n != root) {
        char part[FS_NAME_MAX + 1];
        ksnprintf(part, sizeof(part), "/%s", n->name);
        int plen = (int)strlen(part);
        if (pos + plen >= (int)sizeof(tmp)) break;
        memmove(tmp + plen, tmp, (size_t)pos + 1);
        memcpy(tmp, part, (size_t)plen);
        pos += plen;
        n = n->parent;
    }
    if (pos == 0) strcpy(tmp, "/");
    strncpy(buf, tmp, (size_t)max - 1);
    buf[max - 1] = 0;
    return buf;
}

u32 ramfs_node_count(void) { return node_count; }
u32 ramfs_bytes_used(void) { return bytes_used; }

void ramfs_print_tree(fs_node_t *dir, int depth) {
    if (!dir) return;
    for (fs_node_t *c = dir->children; c; c = c->next) {
        for (int i = 0; i < depth; i++) kprintf("  ");
        kprintf("%s%s  %u bytes\n", c->name, c->type == FS_DIR ? "/" : "", c->size);
        if (c->type == FS_DIR) ramfs_print_tree(c, depth + 1);
    }
}

bool ramfs_selftest(char *err, int errlen) {
    const char *path = "/tmp/.ramfs_selftest";
    const char *text = "ramfs-selftest-payload-0123456789";
    const char *text2 = "APPENDED";

    fs_node_t *f = ramfs_create(path, FS_FILE);
    if (!f) {
        /* clean stale test file */
        ramfs_unlink(path);
        f = ramfs_create(path, FS_FILE);
    }
    if (!f) {
        ksnprintf(err, errlen, "create failed");
        return false;
    }
    if (ramfs_write(f, text, (u32)strlen(text), 0) != (int)strlen(text)) {
        ksnprintf(err, errlen, "write failed");
        return false;
    }
    if (ramfs_append(f, text2, (u32)strlen(text2)) != (int)strlen(text2)) {
        ksnprintf(err, errlen, "append failed");
        return false;
    }
    char buf[128];
    memset(buf, 0, sizeof(buf));
    int n = ramfs_read(f, buf, sizeof(buf) - 1, 0);
    if (n <= 0 || strcmp(buf, "ramfs-selftest-payload-0123456789APPENDED") != 0) {
        ksnprintf(err, errlen, "readback mismatch (%d bytes)", n);
        ramfs_unlink(path);
        return false;
    }
    fs_node_t *look = ramfs_lookup(path);
    if (look != f) {
        ksnprintf(err, errlen, "lookup returned wrong node");
        ramfs_unlink(path);
        return false;
    }
    if (ramfs_unlink(path) != 0) {
        ksnprintf(err, errlen, "unlink failed");
        return false;
    }
    if (ramfs_lookup(path) != NULL) {
        ksnprintf(err, errlen, "deleted file still resolves");
        return false;
    }
    return true;
}

void ramfs_sync_check(void) {
    /* Nothing to flush: RAMFS is volatile by design. */
}
