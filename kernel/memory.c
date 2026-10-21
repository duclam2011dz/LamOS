/* SPDX-License-Identifier: GPL-3.0-only */
#include "kernel.h"
#include "alloc.h"
#define FRAME_COUNT (1u << 20)
#define ADDR_MASK UINT64_C(0x000ffffffffff000)
#define NX (UINT64_C(1) << 63)
static uint8_t used[FRAME_COUNT / 8], eligible[FRAME_COUNT / 8];
static struct FramePool pool;
static struct Heap heap;
static uint64_t root;
uint64_t free_frames;
extern char lamos_kernel_start[], lamos_kernel_end[], lamos_text_end[], lamos_rodata_end[];
static void *physical(uint64_t a) {
    return (void *)(DIRECT + a);
}
uint64_t frame_alloc(void) {
    uint64_t a = pool_alloc(&pool);
    free_frames = pool.free;
    return a;
}
void frame_free(uint64_t a) {
    if (!pool_free(&pool, a))
        PANIC("Invalid/double frame free");
    free_frames = pool.free;
}
static uint64_t new_table(void) {
    uint64_t p = frame_alloc();
    if (!p)
        PANIC("No memory for page tables");
    memset(physical(p), 0, 4096);
    return p;
}
static uint64_t *walk(uint64_t v, bool create) {
    uint64_t *t = physical(root);
    for (int level = 3; level > 0; level--) {
        unsigned index = (unsigned)((v >> (12 + level * 9)) & 511);
        uint64_t e = t[index];
        if (!(e & 1)) {
            if (!create)
                return 0;
            e = new_table() | 3;
            t[index] = e;
        }
        if (e & 128) {
            if (!create)
                return &t[index];
            uint64_t a = new_table();
            uint64_t *sub = physical(a);
            uint64_t base = e & ADDR_MASK;
            for (unsigned i = 0; i < 512; i++)
                sub[i] = (base + (uint64_t)i * 4096) | ((e & ~ADDR_MASK) & ~UINT64_C(128));
            t[index] = a | 3;
            e = t[index];
        }
        t = physical(e & ADDR_MASK);
    }
    return &t[(v >> 12) & 511];
}
static bool canonical(uint64_t v) {
    return (v >> 48) == ((v & (UINT64_C(1) << 47)) ? 0xffff : 0);
}
bool vm_map(uint64_t v, uint64_t p, uint64_t flags) {
    if (!canonical(v) || p >= UINT64_C(0x100000000) || ((v | p) & 4095))
        return false;
    uint64_t *e = walk(v, true);
    if (*e & 1)
        return false;
    *e = p | (flags & (NX | 2 | 8 | 16)) | 1;
    __asm__ volatile("invlpg (%0)" ::"r"(v) : "memory");
    return true;
}
uint64_t vm_query(uint64_t v) {
    if (!canonical(v))
        return 0;
    uint64_t *e = walk(v, false);
    if (!e || !(*e & 1))
        return 0;
    if (*e & 128)
        return (*e & ~UINT64_C(0x1fffff) & ADDR_MASK) | (v & 0x1fffff);
    return (*e & ADDR_MASK) | (v & 4095);
}
void vm_unmap(uint64_t v) {
    if (!canonical(v) || (v & 4095))
        return;
    uint64_t *e = walk(v, false);
    if (!e)
        return;
    if (*e & 128)
        e = walk(v, true);
    *e = 0;
    __asm__ volatile("invlpg (%0)" ::"r"(v) : "memory");
}
static void protect_alias(uint64_t p, uint64_t flags) {
    uint64_t *e = walk(DIRECT + p, true);
    *e = p | flags | 1;
}
void *kmalloc(size_t n) {
    return heap_alloc(&heap, n);
}
void kfree(void *p) {
    if (!heap_free(&heap, p))
        PANIC("Invalid/double heap free");
}
void memory_init(const struct BootInfo *b) {
    pool_init(&pool, used, eligible, FRAME_COUNT);
    for (unsigned i = 0; i < b->map_count; i++)
        if (b->map[i].type == 1)
            pool_range(&pool, b->map[i].base, b->map[i].length, true);
    for (unsigned i = 0; i < b->map_count; i++)
        if (b->map[i].type != 1)
            pool_range(&pool, b->map[i].base, b->map[i].length, false);
    pool_range(&pool, 0, 0x100000, false);
    pool_range(&pool, b->kernel_start, b->kernel_end - b->kernel_start, false);
    pool_range(&pool, b->loader_start, b->loader_end - b->loader_start, false);
    pool_range(&pool, b->fb.address, (uint64_t)b->fb.pitch * b->fb.height, false);
    root = new_table();
    uint64_t *r = physical(root);
    uint64_t pdpt = new_table();
    r[256] = pdpt | 3;
    uint64_t *pd = physical(pdpt);
    for (unsigned g = 0; g < 4; g++) {
        uint64_t page = new_table();
        pd[g] = page | 3;
        uint64_t *t = physical(page);
        for (unsigned j = 0; j < 512; j++) {
            uint64_t a = ((uint64_t)g * 512 + j) * 0x200000;
            t[j] = a | 0x83 | NX;
        }
    }
    for (uint64_t v = (uint64_t)lamos_kernel_start; v < (uint64_t)lamos_kernel_end; v += 4096) {
        uint64_t flags = v < (uint64_t)lamos_text_end     ? 0
                         : v < (uint64_t)lamos_rodata_end ? NX
                                                          : NX | 2;
        if (!vm_map(v, v - KBASE, flags))
            PANIC("Kernel map collision");
        protect_alias(v - KBASE, (flags & 2) | NX);
    }
    uint64_t fb_start = b->fb.address & ~UINT64_C(4095),
             fb_end =
                 (b->fb.address + (uint64_t)b->fb.pitch * b->fb.height + 4095) & ~UINT64_C(4095);
    for (uint64_t p = fb_start; p < fb_end; p += 4096)
        protect_alias(p, 2 | NX | 16 | 8);
    __asm__ volatile("mov %0,%%cr3" ::"r"(root) : "memory");
    uint64_t cr0;
    __asm__ volatile("mov %%cr0,%0" : "=r"(cr0));
    cr0 |= 1 << 16;
    __asm__ volatile("mov %0,%%cr0" ::"r"(cr0) : "memory");
    if (b->firmware == 1) {
        pool_range(&pool, b->loader_start, b->loader_end - b->loader_start, true);
        free_frames = pool.free;
        print("BOOT:RECLAIMED UEFI arena and boot-services memory\n");
    }
    print("PMM:OK free_frames=%u limit=4GiB\n", free_frames);
    print("VMM:OK higher-half NX WP identity-removed\n");
    const uint64_t hv = UINT64_C(0xffffc00000000000);
    const size_t hs = (size_t)8 * 1024 * 1024;
    for (size_t i = 0; i < hs; i += 4096) {
        uint64_t p = frame_alloc();
        if (!p || !vm_map(hv + i, p, NX | 2))
            PANIC("Heap mapping failed");
    }
    heap_init(&heap, (void *)hv, hs);
    uint64_t f = frame_alloc();
    if (!f || !vm_map(hv + hs, f, NX | 2) || vm_query(hv + hs) != f)
        PANIC("VMM self-test");
    *(volatile uint64_t *)(hv + hs) = 0x12345678;
    vm_unmap(hv + hs);
    if (vm_query(hv + hs))
        PANIC("Unmap self-test");
    frame_free(f);
    void *a = kmalloc(37), *c = kmalloc(8192);
    if (a && c && !((uintptr_t)a & 15)) {
        memset(a, 0xa5, 37);
        kfree(a);
        kfree(c);
    } else {
        PANIC("Heap self-test");
    }
    if (kmalloc(SIZE_MAX))
        PANIC("Heap overflow self-test");
    print("HEAP:OK split coalesce alignment allocation-tests\n");
}
