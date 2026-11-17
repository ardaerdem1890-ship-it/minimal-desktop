/* Minimal Desktop - physical frame bitmap, kernel heap, identity paging */
#include "mm.h"
#include "multiboot2.h"
#include "console.h"
#include "io.h"
#include "string.h"

#define RESERVED_LOW   (16u * 1024u * 1024u)   /* kernel, modules, low memory */
#define HEAP_MAGIC     0x48454150u             /* "HEAP" */
#define BLOCK_MAGIC    0x424C4B21u             /* "BLK!" */

extern u8 kernel_end;

/* 1 GiB identity map using 4 MiB pages (PSE). */
static u32 page_directory[1024] __attribute__((aligned(4096)));
static bool paging_on;

/* One bit per 4 KiB frame, 32 KiB covers 1 GiB. */
static u8 frame_bitmap[FRAME_COUNT_MAX / 8];
static u32 total_frames;
static u32 free_frames;
static u32 alloc_hint;
static u32 total_ram_bytes;
static u32 usable_ram_bytes;

/* Static kernel heap */
static u8 heap_area[HEAP_SIZE] __attribute__((aligned(16)));
static bool heap_ready;

typedef struct heap_block {
    u32 magic;
    u32 size;
    u32 free;
    u32 pad;
    struct heap_block *next;
} heap_block_t;

static heap_block_t *heap_head;

static u32 align_up(u32 v, u32 a) { return (v + a - 1) & ~(a - 1); }

static bool frame_used(u32 frame) {
    return (frame_bitmap[frame >> 3] & (u8)(1u << (frame & 7))) != 0;
}

static void frame_set(u32 frame, bool used) {
    if (used) frame_bitmap[frame >> 3] |= (u8)(1u << (frame & 7));
    else      frame_bitmap[frame >> 3] &= (u8)~(1u << (frame & 7));
}

void paging_init(void) {
    /* Identity map the first 1 GiB with 4 MiB pages. */
    for (u32 i = 0; i < 256; i++)
        page_directory[i] = (i * 0x400000u) | 0x83u;   /* present | rw | 4MiB */
    for (u32 i = 256; i < 1024; i++)
        page_directory[i] = 0;

    write_cr3((u32)page_directory);
    write_cr4(read_cr4() | 0x10u);          /* PSE */
    write_cr0(read_cr0() | 0x80000000u);    /* PG */
    paging_on = (read_cr0() & 0x80000000u) != 0;
}

bool paging_enabled(void) {
    return paging_on && (read_cr0() & 0x80000000u);
}

u32 paging_directory_phys(void) {
    return (u32)page_directory;
}

void mm_init(void) {
    total_ram_bytes = g_boot.total_bytes;
    usable_ram_bytes = g_boot.usable_bytes;
    if (total_ram_bytes < (16u * 1024u * 1024u)) total_ram_bytes = 128u * 1024u * 1024u;
    if (total_ram_bytes > (1u * 1024u * 1024u * 1024u)) total_ram_bytes = 1u * 1024u * 1024u;
    if (usable_ram_bytes == 0 || usable_ram_bytes > total_ram_bytes)
        usable_ram_bytes = total_ram_bytes > RESERVED_LOW ? total_ram_bytes - RESERVED_LOW : 0;

    total_frames = total_ram_bytes / PAGE_SIZE;
    if (total_frames > FRAME_COUNT_MAX) total_frames = FRAME_COUNT_MAX;
    if (total_frames < 4096) total_frames = 4096;

    memset(frame_bitmap, 0xFF, sizeof(frame_bitmap));

    mb2_tag_mmap_t *mmap = mb2_get_mmap();
    if (mmap) {
        for (u32 i = 0; i < g_boot.mmap_entries; i++) {
            u8 *ep = (u8 *)mmap->entries + i * mmap->entry_size;
            u64 addr = *(u64 *)(ep + 0);
            u64 len  = *(u64 *)(ep + 8);
            u32 type = *(u32 *)(ep + 16);
            if (type != MB2_MEMORY_AVAILABLE) continue;
            u64 end = addr + len;
            if (end <= RESERVED_LOW) continue;
            u64 start = addr < RESERVED_LOW ? RESERVED_LOW : addr;
            u32 first = (u32)(start / PAGE_SIZE);
            u32 last  = (u32)(end / PAGE_SIZE);
            if (last > total_frames) last = total_frames;
            for (u32 f = first; f < last; f++) frame_set(f, false);
        }
    } else {
        for (u32 f = RESERVED_LOW / PAGE_SIZE; f < total_frames; f++) frame_set(f, false);
    }

    /* Protect the store module(s) even if they landed above the reserve line. */
    for (int i = 0; i < 4; i++) {
        u32 s, e;
        if (!mb2_get_module(i, &s, &e, NULL)) break;
        u32 first = s / PAGE_SIZE, last = (e + PAGE_SIZE - 1) / PAGE_SIZE;
        if (last > total_frames) last = total_frames;
        for (u32 f = first; f < last; f++) frame_set(f, true);
    }

    free_frames = 0;
    for (u32 f = 0; f < total_frames; f++) if (!frame_used(f)) free_frames++;
    alloc_hint = RESERVED_LOW / PAGE_SIZE;

    /* Kernel heap: one large free block. */
    heap_head = (heap_block_t *)heap_area;
    heap_head->magic = HEAP_MAGIC;
    heap_head->size = HEAP_SIZE - sizeof(heap_block_t);
    heap_head->free = 1;
    heap_head->pad = 0;
    heap_head->next = NULL;
    heap_ready = true;
}

/* ------------------------------------------------------------------------- */
/* Physical frame allocator                                                  */
/* ------------------------------------------------------------------------- */
u32 pmm_alloc_frame(void) {
    for (u32 pass = 0; pass < 2; pass++) {
        u32 start = (pass == 0) ? alloc_hint : 0;
        for (u32 f = start; f < total_frames; f++) {
            if (!frame_used(f)) {
                frame_set(f, true);
                if (free_frames) free_frames--;
                alloc_hint = f + 1;
                if (alloc_hint >= total_frames) alloc_hint = 0;
                return f * PAGE_SIZE;
            }
        }
    }
    return 0;
}

void pmm_free_frame(u32 addr) {
    if (addr < RESERVED_LOW || addr >= total_ram_bytes) return;
    u32 f = addr / PAGE_SIZE;
    if (f >= total_frames) return;
    if (frame_used(f)) {
        frame_set(f, false);
        free_frames++;
    }
}

u32 pmm_total_frames(void) { return total_frames; }
u32 pmm_used_frames(void) { return total_frames - free_frames; }
u32 pmm_free_frames(void) { return free_frames; }
u32 pmm_total_ram(void) { return total_ram_bytes; }
u32 pmm_usable_ram(void) { return usable_ram_bytes; }

/* ------------------------------------------------------------------------- */
/* Kernel heap                                                               */
/* ------------------------------------------------------------------------- */
void *kmalloc(size_t size) {
    if (!heap_ready || size == 0) return NULL;
    if (size > heap_head->size && !heap_head->next) return NULL;
    u32 need = align_up((u32)size, 8);
    heap_block_t *b = heap_head;
    while (b) {
        if (b->free && b->size >= need) {
            if (b->size >= need + sizeof(heap_block_t) + 16) {
                heap_block_t *next = (heap_block_t *)((u8 *)b + sizeof(heap_block_t) + need);
                next->magic = BLOCK_MAGIC;
                next->size = b->size - need - sizeof(heap_block_t);
                next->free = 1;
                next->pad = 0;
                next->next = b->next;
                b->next = next;
                b->size = need;
            }
            b->free = 0;
            return (u8 *)b + sizeof(heap_block_t);
        }
        b = b->next;
    }
    return NULL;
}

void *kzalloc(size_t size) {
    void *p = kmalloc(size);
    if (p) memset(p, 0, size);
    return p;
}

void kfree(void *ptr) {
    if (!ptr || !heap_ready) return;
    heap_block_t *b = (heap_block_t *)((u8 *)ptr - sizeof(heap_block_t));
    if (b->magic != BLOCK_MAGIC || b->free) return;
    b->free = 1;

    /* Coalesce the whole list (small and simple, avoids fragmentation). */
    b = heap_head;
    while (b && b->next) {
        if (b->free && b->next->free) {
            b->size += b->next->size + sizeof(heap_block_t);
            b->next = b->next->next;
        } else {
            b = b->next;
        }
    }
}

void *krealloc(void *ptr, size_t size) {
    if (!ptr) return kmalloc(size);
    if (!size) { kfree(ptr); return NULL; }
    heap_block_t *b = (heap_block_t *)((u8 *)ptr - sizeof(heap_block_t));
    if (b->magic != BLOCK_MAGIC) return NULL;
    if (b->size >= size) return ptr;
    void *n = kmalloc(size);
    if (!n) return NULL;
    memcpy(n, ptr, b->size);
    kfree(ptr);
    return n;
}

u32 heap_used(void) {
    u32 used = 0;
    for (heap_block_t *b = heap_head; b; b = b->next)
        if (!b->free) used += b->size;
    return used;
}

u32 heap_total(void) { return HEAP_SIZE; }
u32 heap_free(void) { return HEAP_SIZE - heap_used(); }

void mm_get_stats(u32 *total_ram, u32 *used_frames, u32 *free_fr, u32 *heap_kb) {
    if (total_ram) *total_ram = total_ram_bytes;
    if (used_frames) *used_frames = pmm_used_frames();
    if (free_fr) *free_fr = pmm_free_frames();
    if (heap_kb) *heap_kb = heap_used() / 1024;
}

void mm_print_map(void) {
    kprintf("Physical memory map (Multiboot2 E820):\n");
    kprintf("  #  base              length            type\n");
    mb2_tag_mmap_t *mmap = mb2_get_mmap();
    if (mmap) {
        for (u32 i = 0; i < g_boot.mmap_entries && i < 24; i++) {
            u8 *ep = (u8 *)mmap->entries + i * mmap->entry_size;
            u64 addr = *(u64 *)(ep + 0);
            u64 len  = *(u64 *)(ep + 8);
            u32 type = *(u32 *)(ep + 16);
            kprintf("  %2u %08x%08x  %08x%08x  %s\n", i,
                    (u32)(addr >> 32), (u32)addr, (u32)(len >> 32), (u32)len,
                    type == MB2_MEMORY_AVAILABLE ? "available" : "reserved");
        }
    } else {
        kprintf("  (no memory map tag; using basic memory info)\n");
    }
    kprintf("\nAllocator state:\n");
    kprintf("  physical RAM  : %u MiB (%u frames of %u bytes)\n",
            total_ram_bytes / (1024 * 1024), total_frames, PAGE_SIZE);
    kprintf("  free frames   : %u (%u MiB)\n", free_frames, free_frames * PAGE_SIZE / (1024 * 1024));
    kprintf("  used frames   : %u\n", pmm_used_frames());
    kprintf("  kernel end    : %p\n", (void *)&kernel_end);
    kprintf("  kernel heap   : %u / %u KiB used, base %p\n",
            heap_used() / 1024, HEAP_SIZE / 1024, (void *)heap_area);
    kprintf("  paging        : %s, page dir @ %p, 4 MiB identity pages, 1 GiB mapped\n",
            paging_enabled() ? "enabled" : "disabled", (void *)page_directory);
}

bool pmm_selftest(char *err, int errlen) {
    u32 before = pmm_free_frames();
    u32 a = pmm_alloc_frame();
    u32 b = pmm_alloc_frame();
    if (!a || !b || (a & 0xFFF) || (b & 0xFFF) || a == b) {
        ksnprintf(err, errlen, "alloc returned %p / %p", (void *)a, (void *)b);
        return false;
    }
    if (a >= total_ram_bytes || b >= total_ram_bytes) {
        ksnprintf(err, errlen, "frame outside RAM: %p %p", (void *)a, (void *)b);
        return false;
    }
    memset((void *)a, 0xA5, PAGE_SIZE);
    memset((void *)b, 0x5A, PAGE_SIZE);
    if (((u8 *)a)[0] != 0xA5 || ((u8 *)b)[0] != 0x5A) {
        ksnprintf(err, errlen, "frame write/readback failed");
        return false;
    }
    pmm_free_frame(a);
    pmm_free_frame(b);
    if (pmm_free_frames() != before) {
        ksnprintf(err, errlen, "free count %u != %u", pmm_free_frames(), before);
        return false;
    }
    return true;
}

bool heap_selftest(char *err, int errlen) {
    u32 used_before = heap_used();
    u8 *p = (u8 *)kmalloc(1024);
    if (!p) { ksnprintf(err, errlen, "kmalloc(1024) returned NULL"); return false; }
    for (int i = 0; i < 1024; i++) p[i] = (u8)(i & 0xFF);
    for (int i = 0; i < 1024; i++) {
        if (p[i] != (u8)(i & 0xFF)) {
            ksnprintf(err, errlen, "heap data mismatch at %d", i);
            kfree(p);
            return false;
        }
    }
    u8 *q = (u8 *)kmalloc(65536);
    if (!q) { ksnprintf(err, errlen, "kmalloc(65536) returned NULL"); kfree(p); return false; }
    memset(q, 0x77, 65536);
    if (q[0] != 0x77 || q[65535] != 0x77) {
        ksnprintf(err, errlen, "large allocation failed");
        kfree(q); kfree(p);
        return false;
    }
    kfree(q);
    kfree(p);
    if (heap_used() != used_before) {
        ksnprintf(err, errlen, "heap leak: %u != %u", heap_used(), used_before);
        return false;
    }
    return true;
}
