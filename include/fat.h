#pragma once
#include "types.h"

/* Minimal FAT12/FAT16 driver. Mounts a partition at a RAMFS path, loads its
 * tree into RAMFS, and writes the tree back with fat_sync(). */
bool fat_mount(u32 part_lba, const char *mount_path);
bool fat_present(void);
void fat_info(void);
int  fat_sync(void);
int  fat_load_tree(const char *path);
bool fat_automount(void);
int  fat_format(u32 part_lba);
u32  fat_part_lba(void);
bool fat_selftest(char *err, int errlen);
