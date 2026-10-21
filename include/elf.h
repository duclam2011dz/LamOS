/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMOS_ELF_H
#define LAMOS_ELF_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
struct ElfHeader {
    uint8_t ident[16];
    uint16_t type, machine;
    uint32_t version;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};
struct ElfProgram {
    uint32_t type, flags;
    uint64_t offset, vaddr, paddr, filesz, memsz, align;
};
static inline bool elf_valid(const void *data, size_t size) {
    if (size < sizeof(struct ElfHeader))
        return false;
    const struct ElfHeader *h = data;
    if (h->ident[0] != 0x7f || h->ident[1] != 'E' || h->ident[2] != 'L' || h->ident[3] != 'F' ||
        h->ident[4] != 2 || h->ident[5] != 1 || h->ident[6] != 1 || h->type != 2 ||
        h->machine != 62 || h->version != 1 || h->ehsize != 64 || h->phentsize != 56 || !h->phnum ||
        h->phnum > 32 || h->phoff > size || h->phnum * 56 > size - h->phoff || h->phoff % 8)
        return false;
    const struct ElfProgram *p = (const void *)((const uint8_t *)data + (size_t)h->phoff);
    bool entry = false;
    for (unsigned i = 0; i < h->phnum; i++) {
        if (p[i].type == 2 || p[i].type == 3)
            return false;
        if (p[i].type != 1)
            continue;
        const struct ElfProgram *s = &p[i];
        if (s->paddr < 0x100000 || s->memsz > 0x1f00000 || s->paddr > 0x2000000 - s->memsz ||
            s->filesz > s->memsz || s->offset > size || s->filesz > size - s->offset ||
            s->vaddr != UINT64_C(0xffffffff80000000) + s->paddr || (s->flags & 3) == 3)
            return false;
        if (s->align != 4096 || s->paddr % 4096 || s->offset % 4096)
            return false;
        if ((s->flags & 1) && h->entry >= s->vaddr && h->entry - s->vaddr < s->memsz)
            entry = true;
        for (unsigned j = 0; j < i; j++)
            if (p[j].type == 1 && s->paddr < p[j].paddr + p[j].memsz &&
                p[j].paddr < s->paddr + s->memsz)
                return false;
    }
    return entry;
}
#endif
