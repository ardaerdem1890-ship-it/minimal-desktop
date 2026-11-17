#pragma once
#include "types.h"

#define ATA_PRIMARY_DATA   0x1F0
#define ATA_PRIMARY_CTRL   0x3F6

typedef struct {
    bool present;
    char model[41];
    char serial[21];
    u32 sectors;
    u32 size_mb;
    u16 io_base;
} ata_disk_t;

void ata_init(void);
bool ata_present(void);
const ata_disk_t *ata_get_disk(void);
bool ata_read_sector(u32 lba, u8 *buf);
bool ata_write_sector(u32 lba, const u8 *buf);
void ata_print_info(void);
