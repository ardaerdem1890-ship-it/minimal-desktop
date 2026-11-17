/* Minimal Desktop - FAT12/FAT16 filesystem driver
 *
 * The disk image contains a FAT16 data partition after the ISO. On boot the
 * tree is loaded into RAMFS under /disk; writing a file marks its RAMFS node
 * dirty and `fat_sync()` rebuilds the FAT structures from that tree. This
 * keeps the rest of the OS simple: every command keeps using ramfs_*.
 */
#include "fat.h"
#include "ata.h"
#include "ramfs.h"
#include "mm.h"
#include "console.h"
#include "string.h"

#define SECTOR 512
#define MAX_NODES 192
#define MAX_FILE_BYTES (64u * 1024u)

typedef struct PACKED {
    u8  name[11];
    u8  attr;
    u8  ntres;
    u8  ctime_tenth;
    u16 ctime;
    u16 cdate;
    u16 adate;
    u16 cluster_hi;
    u16 mtime;
    u16 mdate;
    u16 cluster_lo;
    u32 size;
} fat_dirent_t;

static bool mounted;
static u32 part_lba;
static u32 total_sectors;
static u32 fat_lba, root_lba, data_lba;
static u16 bytes_per_sector, reserved;
static u8  n_fats, fat_bits;
static u32 sectors_per_fat, sectors_per_cluster, root_entries, root_sectors;
static u32 cluster_count;
static u8 *fat_mirror;
static char mount_path[64];

static u8 sector[SECTOR];

static inline u32 cluster_lba(u32 c) {
    return data_lba + (c - 2) * sectors_per_cluster;
}

static u32 fat_get(u32 c) {
    if (fat_bits == 12) {
        u32 off = c + (c >> 1);
        u16 v = (u16)(fat_mirror[off] | (fat_mirror[off + 1] << 8));
        return (c & 1) ? ((v >> 4) & 0x0FFF) : (v & 0x0FFF);
    }
    return (u32)(fat_mirror[c * 2] | (fat_mirror[c * 2 + 1] << 8));
}

static void fat_set(u32 c, u32 val) {
    if (fat_bits == 12) {
        u32 off = c + (c >> 1);
        u16 v = (u16)(fat_mirror[off] | (fat_mirror[off + 1] << 8));
        if (c & 1) v = (u16)((v & 0x000F) | ((val & 0x0FFF) << 4));
        else       v = (u16)((v & 0xF000) | (val & 0x0FFF));
        fat_mirror[off] = (u8)v;
        fat_mirror[off + 1] = (u8)(v >> 8);
    } else {
        fat_mirror[c * 2] = (u8)val;
        fat_mirror[c * 2 + 1] = (u8)(val >> 8);
    }
}

static bool read_sectors(u32 lba, u32 count, u8 *dst) {
    for (u32 i = 0; i < count; i++) {
        if (!ata_read_sector(lba + i, dst + i * SECTOR)) return false;
    }
    return true;
}

static bool write_sectors(u32 lba, u32 count, const u8 *src) {
    for (u32 i = 0; i < count; i++) {
        if (!ata_write_sector(lba + i, src + i * SECTOR)) return false;
    }
    return true;
}

static bool read_cluster(u32 c, u8 *dst) {
    if (c < 2 || c >= cluster_count + 2) return false;
    for (u32 i = 0; i < sectors_per_cluster; i++) {
        if (!ata_read_sector(cluster_lba(c) + i, dst + i * SECTOR)) return false;
    }
    return true;
}

static bool write_cluster(u32 c, const u8 *src) {
    if (c < 2 || c >= cluster_count + 2) return false;
    for (u32 i = 0; i < sectors_per_cluster; i++) {
        if (!ata_write_sector(cluster_lba(c) + i, src + i * SECTOR)) return false;
    }
    return true;
}

static void fat_name(const char *name, u8 out[11]) {
    memset(out, ' ', 11);
    const char *dot = strrchr(name, '.');
    int base_len = dot ? (int)(dot - name) : (int)strlen(name);
    if (base_len > 8) base_len = 8;
    for (int i = 0; i < base_len; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[i] = (u8)c;
    }
    if (dot) {
        int ext = 0;
        for (const char *p = dot + 1; *p && ext < 3; p++, ext++) {
            char c = *p;
            if (c >= 'a' && c <= 'z') c -= 32;
            out[8 + ext] = (u8)c;
        }
    }
}

static void display_name(const u8 raw[11], char out[16], u8 ntres) {
    (void)ntres;
    int n = 0;
    /* Always lowercase: FAT stores names in upper case, but every other part
     * of this OS uses lower-case file names, so normalize on mount. */
    for (int i = 0; i < 8 && raw[i] != ' '; i++) {
        char c = (char)raw[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        out[n++] = c;
    }
    if (raw[8] != ' ') {
        out[n++] = '.';
        for (int i = 8; i < 11 && raw[i] != ' '; i++) {
            char c = (char)raw[i];
            if (c >= 'A' && c <= 'Z') c += 32;
            out[n++] = c;
        }
    }
    out[n] = 0;
}


bool fat_present(void) { return mounted; }
u32  fat_part_lba(void) { return part_lba; }

void fat_info(void) {
    if (!mounted) { kprintf("fat: not mounted\n"); return; }
    kprintf("FAT%d: LBA %u, %u sectors, %u MiB, cluster %u sectors, %u clusters\n",
            fat_bits, part_lba, total_sectors, total_sectors >> 11,
            sectors_per_cluster, cluster_count);
    kprintf("  FAT @ %u, root @ %u (%u entries), data @ %u, mount %s\n",
            fat_lba, root_lba, root_entries, data_lba, mount_path);
}

bool fat_mount(u32 lba, const char *path) {
    mounted = false;
    if (!ata_present() || !lba) return false;
    if (!ata_read_sector(lba, sector)) return false;
    bytes_per_sector = (u16)(sector[11] | (sector[12] << 8));
    if (bytes_per_sector != SECTOR) return false;
    sectors_per_cluster = sector[13];
    reserved = (u16)(sector[14] | (sector[15] << 8));
    n_fats = sector[16];
    root_entries = (u16)(sector[17] | (sector[18] << 8));
    u16 total16 = (u16)(sector[19] | (sector[20] << 8));
    total_sectors = total16 ? total16 : (u32)(sector[32] | (sector[33] << 8) |
                     (sector[34] << 16) | ((u32)sector[35] << 24));
    u16 fatsz16 = (u16)(sector[22] | (sector[23] << 8));
    sectors_per_fat = fatsz16 ? fatsz16 : (u32)(sector[36] | (sector[37] << 8) |
                     (sector[38] << 16) | ((u32)sector[39] << 24));
    if (!sectors_per_cluster || !sectors_per_fat || !total_sectors ||
        sectors_per_fat > 512 || n_fats < 1 || n_fats > 2) return false;

    root_sectors = (root_entries * sizeof(fat_dirent_t) + SECTOR - 1) / SECTOR;
    fat_lba = lba + reserved;
    root_lba = fat_lba + n_fats * sectors_per_fat;
    data_lba = root_lba + root_sectors;
    cluster_count = (total_sectors - (reserved + n_fats * sectors_per_fat + root_sectors))
                    / sectors_per_cluster;
    if (cluster_count < 4085) fat_bits = 12;
    else if (cluster_count < 65525) fat_bits = 16;
    else return false; /* FAT32 not supported by this minimal driver */

    u32 mirror_bytes = sectors_per_fat * SECTOR;
    fat_mirror = (u8 *)kmalloc(mirror_bytes);
    if (!fat_mirror) return false;
    if (!read_sectors(fat_lba, sectors_per_fat, fat_mirror)) {
        kfree(fat_mirror); fat_mirror = NULL; return false;
    }

    part_lba = lba;
    strncpy(mount_path, path, sizeof(mount_path) - 1);
    mount_path[sizeof(mount_path) - 1] = 0;
    mounted = true;
    return true;
}

/* ------------------------- read a FAT directory ------------------------- */
static u32 read_dir(u32 cluster, fat_dirent_t *entries, u32 max) {
    u8 *buf = (u8 *)kmalloc(SECTOR * 8);
    if (!buf) return 0;
    u32 found = 0;
    u32 c = cluster;
    int guard = 0;
    while (c >= 2 && c < cluster_count + 2 && guard++ < 256) {
        for (u32 s = 0; s < sectors_per_cluster && s < 8; s++) {
            if (!ata_read_sector(cluster_lba(c) + s, buf + s * SECTOR)) goto done;
        }
        fat_dirent_t *e = (fat_dirent_t *)buf;
        u32 per_sector = SECTOR / sizeof(fat_dirent_t);
        u32 total = per_sector * (sectors_per_cluster < 8 ? sectors_per_cluster : 8);
        for (u32 i = 0; i < total && found < max; i++, e++) {
            if (e->name[0] == 0) { kfree(buf); return found; }
            if (e->name[0] == 0xE5) continue;
            if (e->attr == 0x0F) continue;          /* long file name */
            entries[found++] = *e;
        }
        c = fat_get(c);
        if (c >= 0xFFF8 || c == 0) break;
    }
done:
    kfree(buf);
    return found;
}

static u32 read_root(fat_dirent_t *entries, u32 max) {
    u8 *buf = (u8 *)kmalloc(root_sectors * SECTOR);
    if (!buf) return 0;
    if (!read_sectors(root_lba, root_sectors, buf)) { kfree(buf); return 0; }
    u32 found = 0;
    fat_dirent_t *e = (fat_dirent_t *)buf;
    u32 total = root_sectors * (SECTOR / sizeof(fat_dirent_t));
    for (u32 i = 0; i < total && found < max; i++, e++) {
        if (e->name[0] == 0) break;
        if (e->name[0] == 0xE5 || e->attr == 0x0F) continue;
        if (e->attr & 0x08) continue;               /* volume label */
        entries[found++] = *e;
    }
    kfree(buf);
    return found;
}

static void load_dir(const char *parent_path, u32 cluster) {
    fat_dirent_t entries[128];
    u32 n = cluster ? read_dir(cluster, entries, 128) : read_root(entries, 128);
    for (u32 i = 0; i < n; i++) {
        char name[16];
        display_name(entries[i].name, name, entries[i].ntres);
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        char full[160];
        if (strcmp(parent_path, "/") == 0) ksnprintf(full, sizeof(full), "/%s", name);
        else ksnprintf(full, sizeof(full), "%s/%s", parent_path, name);

        u32 c = (u32)entries[i].cluster_lo | ((u32)entries[i].cluster_hi << 16);
        if (entries[i].attr & 0x10) {
            if (ramfs_lookup(full)) { load_dir(full, c); continue; }
            fs_node_t *d = ramfs_create(full, FS_DIR);
            if (!d) continue;
            d->dev_id = 1; d->dev_ino = c; d->dirty = false;
            load_dir(full, c);
        } else {
            fs_node_t *f = ramfs_lookup(full);
            bool created = false;
            if (!f) { f = ramfs_create(full, FS_FILE); created = true; }
            if (!f || f->type != FS_FILE) continue;
            u32 size = entries[i].size;
            if (size > MAX_FILE_BYTES) size = MAX_FILE_BYTES;
            if (size) {
                u32 cluster_bytes = sectors_per_cluster * SECTOR;
                u8 *data = (u8 *)kmalloc(cluster_bytes);
                if (!data) continue;
                u32 cur = c, off = 0, guard = 0;
                while (cur >= 2 && cur < cluster_count + 2 && off < size && guard++ < 256) {
                    if (!read_cluster(cur, data)) break;   /* reads a whole cluster */
                    u32 chunk = size - off;
                    if (chunk > cluster_bytes) chunk = cluster_bytes;
                    int wr = ramfs_write(f, data, chunk, off);
                    if (wr != (int)chunk) break;
                    off += chunk;
                    u32 next = fat_get(cur);
                    if (next >= 0xFFF8 || next == 0 || next == 0xFFF) break;
                    cur = next;
                }
                f->dirty = false;
                kfree(data);
            } else {
                ramfs_truncate(f, 0);
            }
            f->dev_id = 1; f->dev_ino = c; f->dirty = false;
            (void)created;
        }
    }
}

/* ---------------------------- write-back -------------------------------- */
typedef struct {
    fs_node_t *node;
    fs_node_t *parent;
    u32 start;
    u32 clusters;
    char name[16];
} sync_entry_t;

static sync_entry_t sync_nodes[MAX_NODES];
static int sync_count;

static void collect_tree(fs_node_t *dir) {
    fs_node_t *child;
    for (int i = 0; ramfs_readdir(dir, i, &child) == 0; i++) {
        if (sync_count >= MAX_NODES) return;
        sync_entry_t *e = &sync_nodes[sync_count++];
        e->node = child; e->parent = dir;
        fat_name(child->name, (u8 *)e->name);
        e->name[11] = 0;
        e->start = 0;
        if (child->type == FS_DIR) e->clusters = 1;
        else {
            u32 bytes = child->size ? child->size : 1;
            u32 per = sectors_per_cluster * SECTOR;
            e->clusters = (bytes + per - 1) / per;
        }
        if (child->type == FS_DIR) collect_tree(child);
    }
}

static u32 cluster_of_node(fs_node_t *n, u32 *start_out) {
    for (int i = 0; i < sync_count; i++) {
        if (sync_nodes[i].node == n) {
            if (start_out) *start_out = sync_nodes[i].start;
            return sync_nodes[i].start;
        }
    }
    if (start_out) *start_out = 0;
    return 0;
}

static u32 write_dir(fs_node_t *dir, u32 cluster) {
    fat_dirent_t entries[64];
    u32 n = 0;
    /* "." and ".." */
    memset(&entries[n], 0, sizeof(fat_dirent_t));
    memcpy(entries[n].name, ".          ", 11);
    entries[n].attr = 0x10;
    entries[n].cluster_lo = (u16)cluster;
    entries[n++].cluster_hi = (u16)(cluster >> 16);
    memset(&entries[n], 0, sizeof(fat_dirent_t));
    memcpy(entries[n].name, "..         ", 11);
    entries[n].attr = 0x10;
    u32 parent_cluster = 0;
    cluster_of_node(dir, &parent_cluster);
    entries[n].cluster_lo = (u16)parent_cluster;
    entries[n++].cluster_hi = (u16)(parent_cluster >> 16);

    fs_node_t *child;
    for (int i = 0; ramfs_readdir(dir, i, &child) == 0 && n < 62; i++) {
        fat_dirent_t *e = &entries[n++];
        memset(e, 0, sizeof(*e));
        for (int j = 0; j < sync_count; j++) {
            if (sync_nodes[j].node == child) {
                memcpy(e->name, sync_nodes[j].name, 11); break;
            }
        }
        e->attr = child->type == FS_DIR ? 0x10 : 0x20;
        u32 c = 0;
        cluster_of_node(child, &c);
        e->cluster_lo = (u16)c;
        e->cluster_hi = (u16)(c >> 16);
        e->size = child->type == FS_FILE ? child->size : 0;
    }
    memset(entries + n, 0, sizeof(fat_dirent_t));

    u8 *buf = (u8 *)kmalloc(SECTOR * 8);
    if (!buf) return 0;
    memset(buf, 0, SECTOR * 8);
    memcpy(buf, entries, (n + 1) * sizeof(fat_dirent_t));
    write_cluster(cluster, buf);
    kfree(buf);
    return cluster;
}

int fat_sync(void) {
    if (!mounted) return -1;
    fs_node_t *root_node = ramfs_lookup(mount_path);
    if (!root_node || root_node->type != FS_DIR) return -1;

    bool any_dirty = false;
    sync_count = 0;
    collect_tree(root_node);
    /* assign contiguous cluster runs starting just after the reserved pair */
    u32 next_cluster = 2;
    for (int i = 0; i < sync_count; i++) {
        sync_nodes[i].start = next_cluster;
        next_cluster += sync_nodes[i].clusters;
    }
    for (int i = 0; i < sync_count; i++) if (sync_nodes[i].node->dirty) any_dirty = true;
    if (!any_dirty) return 0;

    u32 needed = 2;
    for (int i = 0; i < sync_count; i++) needed += sync_nodes[i].clusters;
    if (needed >= cluster_count + 2) {
        kprintf("fat: disk full (%u of %u clusters)\n", needed - 2, cluster_count);
        return -1;
    }

    /* clear FAT, reserve clusters 0 and 1 */
    memset(fat_mirror, 0, sectors_per_fat * SECTOR);
    if (fat_bits == 12) {
        fat_mirror[0] = 0xF8; fat_mirror[1] = 0xFF; fat_mirror[2] = 0xFF;
    } else {
        fat_mirror[0] = 0xF8; fat_mirror[1] = 0xFF;
        fat_mirror[2] = 0xFF; fat_mirror[3] = 0xFF;
    }

    u8 *buf = (u8 *)kmalloc(sectors_per_cluster * SECTOR);
    if (!buf) return -1;

    for (int i = 0; i < sync_count; i++) {
        sync_entry_t *e = &sync_nodes[i];
        u32 c = e->start;
        for (u32 k = 0; k < e->clusters; k++) {
            u32 next = (k + 1 < e->clusters) ? (c + k + 1) : 0x0FFFFFFF;
            fat_set(c + k, fat_bits == 12 ? (next & 0xFFF) : (next & 0xFFFF));
        }
        if (e->node->type == FS_DIR) {
            write_dir(e->node, c);
        } else {
            memset(buf, 0, sectors_per_cluster * SECTOR);
            u32 sz = e->node->size;
            if (sz > sectors_per_cluster * SECTOR * e->clusters)
                sz = sectors_per_cluster * SECTOR * e->clusters;
            if (sz) memcpy(buf, e->node->data, sz);
            for (u32 k = 0; k < e->clusters; k++) {
                const u8 *src = buf + k * sectors_per_cluster * SECTOR;
                write_cluster(c + k, src);
            }
        }
    }
    kfree(buf);

    /* root directory */
    fat_dirent_t root_entries_buf[64];
    u32 rn = 0;
    for (int i = 0; i < sync_count && rn < 62; i++) {
        if (sync_nodes[i].parent != root_node) continue;
        fat_dirent_t *e = &root_entries_buf[rn++];
        memset(e, 0, sizeof(*e));
        memcpy(e->name, sync_nodes[i].name, 11);
        e->attr = sync_nodes[i].node->type == FS_DIR ? 0x10 : 0x20;
        u32 c = sync_nodes[i].start;
        e->cluster_lo = (u16)c;
        e->cluster_hi = (u16)(c >> 16);
        e->size = sync_nodes[i].node->type == FS_FILE ? sync_nodes[i].node->size : 0;
    }
    u8 *rootbuf = (u8 *)kmalloc(root_sectors * SECTOR);
    if (!rootbuf) return -1;
    memset(rootbuf, 0, root_sectors * SECTOR);
    if (rn) memcpy(rootbuf, root_entries_buf, rn * sizeof(fat_dirent_t));
    write_sectors(root_lba, root_sectors, rootbuf);
    kfree(rootbuf);

    /* write both FAT copies */
    for (u8 f = 0; f < n_fats; f++)
        write_sectors(fat_lba + f * sectors_per_fat, sectors_per_fat, fat_mirror);

    for (int i = 0; i < sync_count; i++) sync_nodes[i].node->dirty = false;
    kprintf("fat: wrote %d nodes, %u clusters\n", sync_count, needed - 2);
    return 0;
}

int fat_load_tree(const char *path) {
    if (!mounted) return -1;
    load_dir(path, 0);
    return 0;
}

/* Find the data partition in the MBR and mount it as /disk. */
bool fat_automount(void) {
    if (!ata_present() || !ata_read_sector(0, sector)) return false;
    for (int i = 1; i < 4; i++) {
        const u8 *e = sector + 446 + i * 16;
        u8 type = e[4];
        if (type != 0x01 && type != 0x04 && type != 0x06 &&
            type != 0x0B && type != 0x0C && type != 0x0E) continue;
        u32 lba = (u32)(e[8] | (e[9] << 8) | (e[10] << 16) | ((u32)e[11] << 24));
        if (!lba) continue;
        if (fat_mount(lba, "/disk")) {
            if (!ramfs_lookup("/disk")) ramfs_create("/disk", FS_DIR);
            fat_load_tree("/disk");
            kprintf("[fat] mounted /disk at LBA %u (%s%d, %u clusters)\n",
                    lba, fat_bits == 12 ? "12" : "16", fat_bits, cluster_count);
            return true;
        }
    }
    return false;
}

int fat_format(u32 lba) {
    u8 *tmp = (u8 *)kmalloc(SECTOR);
    if (!tmp) return -1;
    memset(tmp, 0, SECTOR);
    for (u32 s = 0; s < 32; s++) {
        if (!ata_write_sector(lba + s, tmp)) { kfree(tmp); return -1; }
    }
    kfree(tmp);
    return 0;
}

bool fat_selftest(char *err, int errlen) {
    if (!mounted) { ksnprintf(err, errlen, "FAT not mounted"); return false; }
    if (!fat_mirror) { ksnprintf(err, errlen, "FAT mirror missing"); return false; }
    u32 c0 = fat_bits == 12 ? 0xFF8 : 0xFFF8;
    if ((fat_get(0) & c0) != c0) { ksnprintf(err, errlen, "FAT reserved cluster invalid"); return false; }
    return true;
}
