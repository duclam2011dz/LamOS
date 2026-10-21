/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMOS_BOOT_H
#define LAMOS_BOOT_H
#include <stdint.h>
#include <stddef.h>
#define BOOT_MAGIC UINT64_C(0x314f534f4d414c)
#define KBASE UINT64_C(0xffffffff80000000)
#define DIRECT UINT64_C(0xffff800000000000)
#define MAP_MAX 128
struct MemoryRange {
    uint64_t base, length;
    uint32_t type, reserved;
};
struct Framebuffer {
    uint64_t address;
    uint32_t width, height, pitch, bpp;
    uint8_t red_shift, red_bits, green_shift, green_bits, blue_shift, blue_bits;
    uint8_t pad[2];
};
struct BootInfo {
    uint64_t magic;
    uint32_t version, size;
    uint32_t firmware, map_count;
    uint64_t kernel_file_size;
    struct Framebuffer fb;
    uint64_t kernel_start, kernel_end, loader_start, loader_end, stack_start, stack_end,
        tables_start, tables_end;
    struct MemoryRange map[MAP_MAX];
};
_Static_assert(offsetof(struct BootInfo, fb) == 32, "framebuffer ABI");
_Static_assert(offsetof(struct BootInfo, map) == 128, "memory map ABI");
#endif
