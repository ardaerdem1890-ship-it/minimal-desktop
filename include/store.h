#pragma once
#include "types.h"

#define STORE_MAGIC        "MDSTORE1"
#define MODULE_MAGIC       "MDMOD1"
#define STORE_MAX_PACKAGES 16
#define STORE_REQSIZE      64

#define MOD_ENTRY_SNAKE    1
#define MOD_ENTRY_PONG     2
#define MOD_ENTRY_DESKTOP  3
#define MOD_ENTRY_DOOM     4
#define MOD_ENTRY_MINILAND 5

typedef struct {
    char name[16];
    char version[12];
    char category[12];
    char desc[64];
    u32  entry;
    u32  size;
    u32  checksum;
    const u8 *data;
    bool installed;
    int  task_pid;
    char source[12];
} store_pkg_t;

typedef struct PACKED {
    char magic[8];
    u32  version;
    u32  count;
    u32  total_size;
    u32  checksum;
} store_archive_hdr_t;

typedef struct PACKED {
    char name[16];
    char version[12];
    char category[12];
    char desc[64];
    u32  entry;
    u32  size;
    u32  checksum;
    u32  offset;
} store_archive_entry_t;

typedef struct PACKED {
    char magic[8];
    u32  version;
    u32  entry;
    u32  body_size;
    u32  body_checksum;
    char name[16];
} store_module_hdr_t;

bool store_init(void);
int  store_package_count(void);
store_pkg_t *store_get(int index);
store_pkg_t *store_find(const char *name);
bool store_installed(const char *name);
int  store_install(const char *name);
int  store_remove(const char *name);
void store_list(void);
void store_info(const char *name);
bool store_validate_all(char *err, int errlen);
int  cmd_store(int argc, char **argv);
const char *store_repo_url(void);
const char *store_last_source(void);
bool store_module_bound(int entry);
