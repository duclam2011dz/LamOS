/* SPDX-License-Identifier: GPL-3.0-only */
#include "alloc.h"
#define PAGE 4096
#define MAGIC UINT64_C(0x4c414d4845415031)
static bool bit(const uint8_t *b, size_t i) {
    return (b[i / 8] >> (i % 8)) & 1;
}
static void set(uint8_t *b, size_t i, bool v) {
    if (v)
        b[i / 8] |= (uint8_t)(1u << (i % 8));
    else
        b[i / 8] &= (uint8_t) ~(1u << (i % 8));
}
void pool_init(struct FramePool *p, uint8_t *u, uint8_t *e, size_t n) {
    *p = (struct FramePool){u, e, n, 0, 0};
    for (size_t i = 0; i < (n + 7) / 8; i++) {
        u[i] = 255;
        e[i] = 0;
    }
}
void pool_range(struct FramePool *p, uint64_t base, uint64_t length, bool available) {
    uint64_t max = (uint64_t)p->count * PAGE;
    if (base >= max)
        return;
    if (length > max - base)
        length = max - base;
    size_t begin = (size_t)(base / PAGE), end = (size_t)((base + length + PAGE - 1) / PAGE);
    if (available) {
        begin = (size_t)((base + PAGE - 1) / PAGE);
        end = (size_t)((base + length) / PAGE);
    }
    for (size_t i = begin; i < end; i++) {
        bool was = !bit(p->used, i);
        set(p->eligible, i, available);
        set(p->used, i, !available);
        if (available && !was)
            p->free++;
        if (!available && was)
            p->free--;
    }
}
uint64_t pool_alloc(struct FramePool *p) {
    if (!p->free)
        return 0;
    for (size_t n = 0; n < p->count; n++) {
        size_t i = (p->cursor + n) % p->count;
        if (!bit(p->used, i)) {
            set(p->used, i, true);
            p->free--;
            p->cursor = (i + 1) % p->count;
            return (uint64_t)i * PAGE;
        }
    }
    return 0;
}
bool pool_free(struct FramePool *p, uint64_t a) {
    size_t i = (size_t)(a / PAGE);
    if (a % PAGE || i >= p->count || !bit(p->eligible, i) || !bit(p->used, i))
        return false;
    set(p->used, i, false);
    p->free++;
    if (i < p->cursor)
        p->cursor = i;
    return true;
}
void heap_init(struct Heap *h, void *start, size_t size) {
    h->allocations = 0;
    h->used = 0;
    h->peak = 0;
    h->start = start;
    h->size = size;
    h->first = start;
    *h->first = (struct HeapBlock){size - sizeof(struct HeapBlock), 0, MAGIC, true};
}
void *heap_alloc(struct Heap *h, size_t n) {
    if (!n || n > SIZE_MAX - 15)
        return 0;
    n = (n + 15) & ~(size_t)15;
    for (struct HeapBlock *b = h->first; b; b = b->next) {
        if (b->magic != MAGIC)
            return 0;
        if (!b->free || b->size < n)
            continue;
        if (b->size >= n + sizeof(*b) + 16) {
            struct HeapBlock *next = (void *)((uint8_t *)(b + 1) + n);
            *next = (struct HeapBlock){b->size - n - sizeof(*b), b->next, MAGIC, true};
            b->size = n;
            b->next = next;
        }
        b->free = false;
        h->allocations++;
        h->used += b->size;
        if (h->used > h->peak)
            h->peak = h->used;
        return b + 1;
    }
    return 0;
}
bool heap_free(struct Heap *h, const void *ptr) {
    if (!ptr)
        return true;
    struct HeapBlock *found = 0;
    for (struct HeapBlock *b = h->first; b; b = b->next) {
        if (b->magic != MAGIC)
            return false;
        if (b + 1 == ptr) {
            found = b;
            break;
        }
    }
    if (!found || found->free)
        return false;
    h->allocations--;
    h->used -= found->size;
    found->free = true;
    for (struct HeapBlock *b = h->first; b && b->next;) {
        if (b->free && b->next->free) {
            b->size += sizeof(*b) + b->next->size;
            b->next = b->next->next;
        } else
            b = b->next;
    }
    return true;
}

struct HeapStats heap_stats(const struct Heap *h) {
    struct HeapStats s = {h->allocations, h->used, 0, h->peak, 0};
    for (const struct HeapBlock *b = h->first; b; b = b->next)
        if (b->free) {
            s.free += b->size;
            if (b->size > s.largest)
                s.largest = b->size;
        }
    return s;
}
