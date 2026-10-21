/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMOS_SHELL_H
#define LAMOS_SHELL_H
#include "terminal.h"
#include "alloc.h"
struct ShellInfo {
    const char *version, *firmware, *cpu;
    unsigned width, height;
    uint64_t ticks, frame_total, frame_free;
    struct HeapStats heap;
};
void shell_execute(struct Terminal *, const char *, const struct ShellInfo *);
void terminal_number(struct Terminal *, uint64_t, uint8_t);
#endif
