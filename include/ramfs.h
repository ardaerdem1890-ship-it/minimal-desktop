#pragma once
#include "types.h"

#define FS_NAME_MAX 32
#define FS_FILE     1
#define FS_DIR      2
#define FS_MAX_FILE (64u * 1024u)

typedef struct fs_node {
    char name[FS_NAME_MAX];
    u8   type;
    u32  size;
    u32  capacity;
    u32  inode;
    u8  *data;
    struct fs_node *parent;
    struct fs_node *children;
    struct fs_node *next;
    u32  dev_id;        /* 0 = RAMFS, 1 = FAT (set by the mount code) */
    u32  dev_ino;       /* FAT starting cluster */
    bool dirty;         /* needs writing back to its backing store */
} fs_node_t;

void  ramfs_init(void);
fs_node_t *ramfs_root(void);
fs_node_t *ramfs_cwd(void);
void  ramfs_set_cwd(fs_node_t *node);
fs_node_t *ramfs_lookup(const char *path);
fs_node_t *ramfs_create(const char *path, u8 type);
fs_node_t *ramfs_parent_of(const char *path);
int   ramfs_read(fs_node_t *node, void *buf, u32 len, u32 offset);
int   ramfs_write(fs_node_t *node, const void *buf, u32 len, u32 offset);
int   ramfs_append(fs_node_t *node, const void *buf, u32 len);
int   ramfs_truncate(fs_node_t *node, u32 size);
int   ramfs_unlink(const char *path);
int   ramfs_readdir(fs_node_t *dir, int index, fs_node_t **out);
char *ramfs_path(fs_node_t *node, char *buf, int max);
u32   ramfs_node_count(void);
u32   ramfs_bytes_used(void);
void  ramfs_print_tree(fs_node_t *dir, int depth);
bool  ramfs_selftest(char *err, int errlen);
void  ramfs_sync_check(void);
