#pragma once
#include "types.h"

#define MB2_BOOT_MAGIC 0x36D76289u

/* Multiboot2 tag types */
#define MB2_TAG_END           0
#define MB2_TAG_CMDLINE       1
#define MB2_TAG_BOOT_LOADER   2
#define MB2_TAG_MODULE        3
#define MB2_TAG_BASIC_MEMINFO 4
#define MB2_TAG_BOOTDEV       5
#define MB2_TAG_MMAP          6
#define MB2_TAG_FRAMEBUFFER   8

typedef struct PACKED {
    u32 total_size;
    u32 reserved;
    /* tags follow */
} mb2_info_t;

typedef struct PACKED {
    u32 type;
    u32 size;
} mb2_tag_t;

typedef struct PACKED {
    u32 type;
    u32 size;
    char string[];
} mb2_tag_string_t;

typedef struct PACKED {
    u32 type;
    u32 size;
    u32 mod_start;
    u32 mod_end;
    char string[];
} mb2_tag_module_t;

typedef struct PACKED {
    u32 type;
    u32 size;
    u32 mem_lower;
    u32 mem_upper;
} mb2_tag_basic_mem_t;

typedef struct PACKED {
    u32 type;
    u32 size;
    u32 entry_size;
    u32 entry_version;
    struct PACKED {
        u64 addr;
        u64 len;
        u32 type;
        u32 zero;
    } entries[];
} mb2_tag_mmap_t;

#define MB2_MEMORY_AVAILABLE 1

typedef struct {
    u32 total_bytes;
    u32 usable_bytes;
    u32 mmap_entries;
    u32 module_count;
    bool has_store_module;
    u32 store_start;
    u32 store_end;
    char cmdline[256];
    char bootloader[64];
} boot_info_t;

extern boot_info_t g_boot;

bool mb2_init(u32 info_addr);
void mb2_dump(void);
bool mb2_has_flag(const char *flag);
u32 mb2_info_addr(void);
mb2_tag_mmap_t *mb2_get_mmap(void);
bool mb2_get_module(int index, u32 *start, u32 *end, const char **name);
