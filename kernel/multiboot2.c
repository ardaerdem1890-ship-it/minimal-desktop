/* Minimal Desktop - Multiboot2 information parser */
#include "multiboot2.h"
#include "console.h"
#include "string.h"

boot_info_t g_boot;
static u32 info_address;
static mb2_tag_mmap_t *saved_mmap;
static u32 saved_module_start[4];
static u32 saved_module_end[4];
static char saved_module_name[4][32];
static int saved_modules;

static u32 align8(u32 v) { return (v + 7u) & ~7u; }

bool mb2_init(u32 info_addr) {
    memset(&g_boot, 0, sizeof(g_boot));
    memset(saved_module_start, 0, sizeof(saved_module_start));
    memset(saved_module_end, 0, sizeof(saved_module_end));
    memset(saved_module_name, 0, sizeof(saved_module_name));
    saved_modules = 0;
    saved_mmap = NULL;
    info_address = info_addr;

    if (!info_addr) return false;
    mb2_info_t *info = (mb2_info_t *)info_addr;
    if (info->total_size < 16 || info->total_size > 1024 * 1024) {
        return false;
    }

    u32 off = 8;
    while (off + 8 <= info->total_size) {
        mb2_tag_t *tag = (mb2_tag_t *)(info_addr + off);
        if (tag->type == MB2_TAG_END) break;
        if (tag->size < 8 || off + tag->size > info->total_size) break;

        switch (tag->type) {
            case MB2_TAG_CMDLINE: {
                mb2_tag_string_t *t = (mb2_tag_string_t *)tag;
                u32 n = t->size > 8 ? t->size - 8 : 0;
                if (n >= sizeof(g_boot.cmdline)) n = sizeof(g_boot.cmdline) - 1;
                u32 used = strnlen(t->string, n);
                if (used > n) used = n;
                memcpy(g_boot.cmdline, t->string, used);
                g_boot.cmdline[used] = 0;
                break;
            }
            case MB2_TAG_BOOT_LOADER: {
                mb2_tag_string_t *t = (mb2_tag_string_t *)tag;
                u32 n = t->size > 8 ? t->size - 8 : 0;
                if (n >= sizeof(g_boot.bootloader)) n = sizeof(g_boot.bootloader) - 1;
                u32 used = strnlen(t->string, n);
                if (used > n) used = n;
                memcpy(g_boot.bootloader, t->string, used);
                g_boot.bootloader[used] = 0;
                break;
            }
            case MB2_TAG_MODULE: {
                mb2_tag_module_t *m = (mb2_tag_module_t *)tag;
                if (saved_modules < 4 && m->mod_end > m->mod_start) {
                    int idx = saved_modules++;
                    saved_module_start[idx] = m->mod_start;
                    saved_module_end[idx] = m->mod_end;
                    u32 n = m->size > 16 ? m->size - 16 : 0;
                    if (n >= sizeof(saved_module_name[0])) n = sizeof(saved_module_name[0]) - 1;
                    u32 used = strnlen(m->string, n);
                    if (used > n) used = n;
                    memcpy(saved_module_name[idx], m->string, used);
                    saved_module_name[idx][used] = 0;
                }
                g_boot.module_count++;
                break;
            }
            case MB2_TAG_BASIC_MEMINFO: {
                mb2_tag_basic_mem_t *m = (mb2_tag_basic_mem_t *)tag;
                u32 kb = m->mem_lower + m->mem_upper;
                if (kb > g_boot.total_bytes / 1024) g_boot.total_bytes = kb * 1024;
                break;
            }
            case MB2_TAG_MMAP: {
                mb2_tag_mmap_t *m = (mb2_tag_mmap_t *)tag;
                saved_mmap = m;
                g_boot.mmap_entries = (m->size - 16) / m->entry_size;
                for (u32 i = 0; i < g_boot.mmap_entries; i++) {
                    u8 *ep = (u8 *)m->entries + i * m->entry_size;
                    u64 addr = *(u64 *)(ep + 0);
                    u64 len  = *(u64 *)(ep + 8);
                    u32 type = *(u32 *)(ep + 16);
                    u64 end = addr + len;
                    if (type == MB2_MEMORY_AVAILABLE) {
                        g_boot.usable_bytes += (u32)len;
                        if (end > g_boot.total_bytes && end < 0x100000000ull)
                            g_boot.total_bytes = (u32)end;
                    }
                }
                break;
            }
            default:
                break;
        }
        off += align8(tag->size);
    }

    g_boot.module_count = (u32)saved_modules;
    if (saved_modules > 0) {
        g_boot.has_store_module = true;
        g_boot.store_start = saved_module_start[0];
        g_boot.store_end = saved_module_end[0];
    }
    return true;
}

u32 mb2_info_addr(void) { return info_address; }
mb2_tag_mmap_t *mb2_get_mmap(void) { return saved_mmap; }

bool mb2_get_module(int index, u32 *start, u32 *end, const char **name) {
    if (index < 0 || index >= saved_modules) return false;
    if (start) *start = saved_module_start[index];
    if (end) *end = saved_module_end[index];
    if (name) *name = saved_module_name[index];
    return true;
}

bool mb2_has_flag(const char *flag) {
    size_t flen = strlen(flag);
    const char *p = g_boot.cmdline;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        const char *start = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        size_t len = (size_t)(p - start);
        if (len == flen && strncmp(start, flag, flen) == 0) return true;
    }
    return false;
}

void mb2_dump(void) {
    kprintf("Multiboot2 info @ %p, total_size=%u\n", (void *)info_address, info_address ? ((mb2_info_t *)info_address)->total_size : 0);
    kprintf("  bootloader : %s\n", g_boot.bootloader[0] ? g_boot.bootloader : "(unknown)");
    kprintf("  cmdline    : %s\n", g_boot.cmdline[0] ? g_boot.cmdline : "(none)");
    kprintf("  memory     : %u KiB total, %u KiB usable, %u mmap entries\n",
            g_boot.total_bytes / 1024, g_boot.usable_bytes / 1024, g_boot.mmap_entries);
    kprintf("  modules    : %u", g_boot.module_count);
    for (int i = 0; i < saved_modules; i++) {
        kprintf("\n    [%d] %s  %p-%p (%u KiB)", i, saved_module_name[i],
                (void *)saved_module_start[i], (void *)saved_module_end[i],
                (saved_module_end[i] - saved_module_start[i]) / 1024);
    }
    kprintf("\n");
}
