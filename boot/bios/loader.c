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
uint32_t load(uint32_t command) {
    struct BootInfo *b = (void *)0x5000;
    const struct ElfHeader *h = (void *)0x18000;
    const struct ElfProgram *programs = (void *)(0x20000 + ((uint32_t)h->phoff & 2047));
    if (command == 0) {
        if (!elf_header_valid(h, (size_t)b->kernel_file_size))
            stop();
        return 1;
    }
    if (command == 1) {
        if (!elf_programs_valid(h, programs, (size_t)b->kernel_file_size))
            stop();
        b->kernel_start = 0x100000;
        b->kernel_end = 0x100000;
        for (unsigned i = 0; i < h->phnum; i++)
            if (programs[i].type == 1 && programs[i].paddr + programs[i].memsz > b->kernel_end)
                b->kernel_end = programs[i].paddr + programs[i].memsz;
        return 1;
    }
    if (command == 2) {
        volatile uint32_t *job = (void *)0x6400;
        uint8_t *dst = (void *)job[0];
        const uint8_t *src = (void *)job[1];
        for (uint32_t i = 0; i < job[2]; i++)
            dst[i] = src ? src[i] : 0;
        return 1;
    }
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
    return (uint32_t)h->entry;
}
