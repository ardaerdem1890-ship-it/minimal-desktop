/* Minimal Desktop - minimal ISO9660 reader
 *
 * Reads the primary volume descriptor and directory records from the boot CD
 * and populates RAMFS under /cd. Read-only.
 */
#include "iso9660.h"
#include "atapi.h"
#include "ramfs.h"
#include "mm.h"
#include "console.h"
#include "string.h"

#define SECTOR 2048

typedef struct PACKED {
    u8 len;
    u8 ext_attr_len;
    u32 extent_le;
    u32 extent_be;
    u32 size_le;
    u32 size_be;
    u8 date[7];
    u8 flags;
    u8 unit;
    u8 gap[2];
    u16 seq_le, seq_be;
    u8 file_len;
    u8 name[1];
} iso_dirent_t;

static bool mounted;
static u32 root_extent;
static u32 root_size;
static u8 sector_buf[SECTOR];

typedef struct { u8 name[32]; u32 extent; u32 size; bool dir; } iso_entry_t;
static iso_entry_t entries[256];
static int entry_count;

static u32 le32(const u8 *p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

bool iso9660_present(void) { return mounted; }

static void copy_name(u8 *dst, const u8 *src, int len) {
    /* strip trailing ;1 and lowercase */
    while (len > 0 && src[len - 1] == ' ') len--;
    if (len >= 2 && src[len - 2] == ';' && src[len - 1] >= '0' && src[len - 1] <= '9') len -= 2;
    for (int i = 0; i < len && i < 31; i++) {
        char c = (char)src[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        if (c == '_') c = '_';
        dst[i] = c;
    }
    dst[len] = 0;
}

static int read_dir(u32 extent, u32 size) {
    int count = 0;
    u32 sectors = (size + SECTOR - 1) / SECTOR;
    for (u32 s = 0; s < sectors && count < 250; s++) {
        if (!atapi_read_sector(extent + s, sector_buf)) break;
        u32 off = 0;
        while (off + sizeof(iso_dirent_t) <= SECTOR) {
            iso_dirent_t *e = (iso_dirent_t *)(sector_buf + off);
            if (e->len == 0) {
                off = (off + SECTOR) & ~(SECTOR - 1);  /* next sector */
                if (off >= SECTOR) break;
                continue;
            }
            if (off + e->len > SECTOR) break;
            if (e->file_len > 0 && e->file_len <= 31 && e->name[0] != 0) {
                iso_entry_t *out = &entries[count];
                copy_name(out->name, e->name, e->file_len);
                out->extent = le32((const u8 *)&e->extent_le);
                out->size = le32((const u8 *)&e->size_le);
                out->dir = (e->flags & 0x02) != 0;
                /* skip ., .. and unnamed */
                if (out->name[0] && !(out->name[0] == '.' && out->name[1] == 0) &&
                !(out->name[0] == '.' && out->name[1] == '.' && out->name[2] == 0))
                    count++;
            }
            off += e->len;
        }
    }
    return count;
}

bool iso9660_mount(void) {
    mounted = false;
    entry_count = 0;
    if (!atapi_present()) return false;
    if (!atapi_read_sector(16, sector_buf)) return false;
    if (sector_buf[0] != 1 || memcmp(sector_buf + 1, "CD001", 5) != 0) return false;

    iso_dirent_t *root = (iso_dirent_t *)(sector_buf + 156);
    root_extent = le32((const u8 *)&root->extent_le);
    root_size = le32((const u8 *)&root->size_le);

    fs_node_t *cd = ramfs_lookup("/cd");
    if (!cd) cd = ramfs_create("/cd", FS_DIR);
    if (!cd) return false;

    int n = read_dir(root_extent, root_size);
    for (int i = 0; i < n; i++) {
        char path[160];
        ksnprintf(path, sizeof(path), "/cd/%s", entries[i].name);
        if (entries[i].dir) {
            if (!ramfs_lookup(path)) ramfs_create(path, FS_DIR);
        } else {
            fs_node_t *f = ramfs_lookup(path);
            if (!f) f = ramfs_create(path, FS_FILE);
            if (!f || f->type != FS_FILE) continue;
            u32 sz = entries[i].size;
            if (sz > 64 * 1024) sz = 64 * 1024;
            u8 *data = (u8 *)kmalloc(sz ? sz : 1);
            if (!data) continue;
            u32 sectors = (sz + SECTOR - 1) / SECTOR;
            u32 off = 0;
            for (u32 sec = 0; sec < sectors && off < sz; sec++) {
                if (!atapi_read_sector(entries[i].extent + sec, sector_buf)) break;
                u32 chunk = sz - off;
                if (chunk > SECTOR) chunk = SECTOR;
                memcpy(data + off, sector_buf, chunk);
                off += chunk;
            }
            if (off) ramfs_write(f, data, off, 0);
            kfree(data);
        }
    }
    entry_count = n;
    mounted = true;
    kprintf("[iso] mounted /cd (%d entries from PVD)\n", n);
    return true;
}

void iso9660_info(void) {
    if (!mounted) { kprintf("iso9660: not mounted\n"); return; }
    kprintf("iso9660: %d entries, root extent %u (%u bytes)\n",
            entry_count, root_extent, root_size);
}

bool iso9660_selftest(char *err, int errlen) {
    if (!mounted) { ksnprintf(err, errlen, "ISO9660 not mounted"); return false; }
    if (!atapi_present()) { ksnprintf(err, errlen, "no ATAPI device"); return false; }
    return true;
}
