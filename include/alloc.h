/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMOS_ALLOC_H
#define LAMOS_ALLOC_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
struct FramePool {
    uint8_t *used, *eligible;
    size_t count, cursor, free;
};
void pool_init(struct FramePool *, uint8_t *, uint8_t *, size_t);
void pool_range(struct FramePool *, uint64_t, uint64_t, bool);
uint64_t pool_alloc(struct FramePool *);
bool pool_free(struct FramePool *, uint64_t);
struct HeapBlock {
    size_t size;
    struct HeapBlock *next;
    uint64_t magic;
    bool free;
};
struct Heap {
    void *start;
    size_t size;
    struct HeapBlock *first;
};
void heap_init(struct Heap *, void *, size_t);
void *heap_alloc(struct Heap *, size_t);
bool heap_free(struct Heap *, const void *);
#endif
