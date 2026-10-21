/* SPDX-License-Identifier: GPL-3.0-only */
#include "boot.h"
#include "elf.h"
static void stop(void) {
    const char *s = "BIOS:ELF_ERROR\n";
    while (*s) {
        uint8_t status;
        do {
            __asm__ volatile("inb %1,%0" : "=a"(status) : "Nd"((uint16_t)0x3fd));
        } while (!(status & 32));
        __asm__ volatile("outb %0,%1" ::"a"((uint8_t)*s++), "Nd"((uint16_t)0x3f8));
    }
    for (;;)
        __asm__ volatile("cli; hlt");
}
uint32_t load(void) {
    struct BootInfo *b = (void *)0x5000;
    const uint8_t *elf = (void *)0x10000;
    if (!elf_valid(elf, (size_t)b->kernel_file_size))
        stop();
    uint64_t phoff = *(const uint64_t *)(elf + 32), entry = *(const uint64_t *)(elf + 24);
    uint16_t n = *(const uint16_t *)(elf + 56);
    if (phoff > 0x40000 || n > 32 || *(const uint16_t *)(elf + 54) != 56)
        stop();
    b->kernel_start = 0x100000;
    b->kernel_end = 0x100000;
    for (unsigned i = 0; i < n; i++) {
        const uint8_t *p = elf + (uint32_t)phoff + i * 56;
        if (*(const uint32_t *)p != 1)
            continue;
        uint64_t off = *(const uint64_t *)(p + 8), v = *(const uint64_t *)(p + 16),
                 a = *(const uint64_t *)(p + 24), f = *(const uint64_t *)(p + 32),
                 m = *(const uint64_t *)(p + 40);
        if (a < 0x100000 || a + m > 0x2000000 || m < f || off + f > 0x50000 || v != KBASE + a)
            stop();
        for (uint32_t j = 0; j < (uint32_t)m; j++)
            ((uint8_t *)(uint32_t)a)[j] = j < f ? elf[(uint32_t)off + j] : 0;
        if (a + m > b->kernel_end)
            b->kernel_end = a + m;
    }
    if (entry < KBASE + b->kernel_start || entry >= KBASE + b->kernel_end)
        stop();
    for (unsigned i = 0; i < b->map_count; i++)
        if (b->map[i].type != 1)
            b->map[i].type = 2;
    uint64_t *t = (void *)0x70000;
    for (unsigned i = 0; i < 7 * 512; i++)
        t[i] = 0;
    t[0] = 0x71003;
    t[256] = 0x71003;
    t[511] = 0x72003;
    t[1024 + 510] = 0x73003;
    for (unsigned g = 0; g < 4; g++) {
        t[512 + g] = 0x73003 + g * 4096;
        for (unsigned j = 0; j < 512; j++)
            t[1536 + g * 512 + j] = ((uint64_t)g * 512 + j) * 0x200000 + 0x83;
    }
    b->magic = BOOT_MAGIC;
    b->version = 1;
    b->size = sizeof(*b);
    b->firmware = 0;
    b->loader_start = 0;
    b->loader_end = 0x100000;
    b->stack_start = 0x60000;
    b->stack_end = 0x70000;
    b->tables_start = 0x70000;
    b->tables_end = 0x77000;
    return (uint32_t)entry;
}
