#pragma once
#include "types.h"

#define PAGE_SIZE       4096u
#define FRAME_COUNT_MAX 262144u          /* 1 GiB of 4 KiB frames */
#define HEAP_SIZE       (4u * 1024u * 1024u)

void mm_init(void);
void paging_init(void);
bool paging_enabled(void);
u32  paging_directory_phys(void);

void *kmalloc(size_t size);
void *kzalloc(size_t size);
void *krealloc(void *ptr, size_t size);
void  kfree(void *ptr);
u32   heap_used(void);
u32   heap_total(void);
u32   heap_free(void);

u32  pmm_alloc_frame(void);
void pmm_free_frame(u32 addr);
u32  pmm_total_frames(void);
u32  pmm_used_frames(void);
u32  pmm_free_frames(void);
u32  pmm_total_ram(void);
u32  pmm_usable_ram(void);

void mm_print_map(void);
bool pmm_selftest(char *err, int errlen);
bool heap_selftest(char *err, int errlen);
void mm_get_stats(u32 *total_ram, u32 *used_frames, u32 *free_frames, u32 *heap_used_kb);
