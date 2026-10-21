/* SPDX-License-Identifier: GPL-3.0-only */
#include "alloc.h"
#include "input.h"
#include "graphics.h"
#include "elf.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void frames(void) {
    uint8_t used[8], eligible[8];
    struct FramePool p;
    pool_init(&p, used, eligible, 64);
    pool_range(&p, 0, 64 * 4096, true);
    pool_range(&p, 0, 4096, false);
    pool_range(&p, 8193, 4096, false);
    assert(p.free == 61);
    uint64_t a[61];
    for (unsigned i = 0; i < 61; i++) {
        a[i] = pool_alloc(&p);
        assert(a[i] && a[i] != 8192 && a[i] != 12288);
        for (unsigned j = 0; j < i; j++)
            assert(a[i] != a[j]);
    }
    assert(!pool_alloc(&p));
    assert(!pool_free(&p, 0));
    assert(!pool_free(&p, 8192));
    assert(!pool_free(&p, 4097));
    for (unsigned i = 0; i < 61; i++)
        assert(pool_free(&p, a[i]));
    assert(!pool_free(&p, a[0]));
    assert(p.free == 61);
    pool_range(&p, UINT64_MAX - 1, 1024, true);
    assert(p.free == 61);
    pool_init(&p, used, eligible, 64);
    pool_range(&p, 4097, 8191, true);
    assert(p.free == 1 && pool_alloc(&p) == 8192);
    puts("PASS frame allocation, reserved overlaps, exhaustion, free validation");
}
static void heaps(void) {
    _Alignas(16) uint8_t storage[4096];
    struct Heap h;
    heap_init(&h, storage, sizeof(storage));
    void *a = heap_alloc(&h, 1), *b = heap_alloc(&h, 96);
    assert(a && b && ((uintptr_t)a % 16) == 0);
    memset(a, 1, 1);
    memset(b, 2, 96);
    assert(!heap_free(&h, (uint8_t *)b + 1));
    assert(heap_free(&h, a));
    assert(!heap_free(&h, a));
    assert(heap_free(&h, b));
    assert(heap_alloc(&h, 4000));
    assert(!heap_alloc(&h, 4000));
    assert(!heap_alloc(&h, 0));
    assert(!heap_alloc(&h, SIZE_MAX));
    puts("PASS heap alignment, split/coalesce, invalid free, bounds");
}
static void inputs(void) {
    struct EventQueue q = {0};
    struct Keyboard k = {0};
    struct Event e;
    keyboard_byte(&k, &q, 42);
    keyboard_byte(&k, &q, 30);
    assert(event_pop(&q, &e) && e.type == KEY_DOWN);
    assert(event_pop(&q, &e) && e.type == KEY_DOWN);
    assert(event_pop(&q, &e) && e.type == TEXT_INPUT && e.text == 'A');
    keyboard_byte(&k, &q, 0x9e);
    keyboard_byte(&k, &q, 0xaa);
    while (event_pop(&q, &e)) {
    }
    keyboard_byte(&k, &q, 58);
    keyboard_byte(&k, &q, 58);
    keyboard_byte(&k, &q, 0xba);
    keyboard_byte(&k, &q, 48);
    char last = 0;
    while (event_pop(&q, &e))
        if (e.type == TEXT_INPUT)
            last = e.text;
    assert(last == 'B');
    keyboard_byte(&k, &q, 0xe0);
    keyboard_byte(&k, &q, 0x1d);
    keyboard_byte(&k, &q, 30);
    while (event_pop(&q, &e))
        assert(e.type != TEXT_INPUT);
    keyboard_byte(&k, &q, 0xe0);
    keyboard_byte(&k, &q, 0x9d);
    assert(!k.ctrl);
    while (event_pop(&q, &e)) {
    }
    struct Mouse m = {0};
    mouse_byte(&m, &q, 0);
    mouse_byte(&m, &q, 0x39);
    mouse_byte(&m, &q, 0xfe);
    mouse_byte(&m, &q, 0xfd);
    assert(event_pop(&q, &e) && e.type == MOUSE_MOVE && e.dx == -2 && e.dy == 3);
    assert(event_pop(&q, &e) && e.type == BUTTON_DOWN && e.code == 1);
    mouse_byte(&m, &q, 0xc8);
    mouse_byte(&m, &q, 0);
    mouse_byte(&m, &q, 0);
    assert(!event_pop(&q, &e));
    for (unsigned i = 0; i < EVENT_CAPACITY + 7; i++)
        event_push(&q, (struct Event){.type = KEY_DOWN});
    assert(q.dropped == 8);
    unsigned n = 0;
    while (event_pop(&q, &e))
        n++;
    assert(n == EVENT_CAPACITY - 1);
    puts("PASS keyboard modifiers, release, extended keys, mouse sync/sign/overflow, queue "
         "overflow");
}
static void graphics(void) {
    uint32_t data[102];
    for (unsigned i = 0; i < 102; i++)
        data[i] = 0xdeadbeef;
    struct Canvas c = {data + 1, 10, 10};
    canvas_rect(&c, -4, -4, 8, 8, 1);
    assert(data[1] == 1 && data[4] == 1 && data[5] == 0xdeadbeef);
    canvas_rect(&c, INT_MAX, INT_MAX, INT_MAX, INT_MAX, 2);
    canvas_pixel(&c, -1, 0, 3);
    canvas_line(&c, INT_MIN, 5, INT_MAX, 5, 4);
    for (unsigned x = 0; x < 10; x++)
        assert(c.pixels[50 + x] == 4);
    canvas_line(&c, -10, -10, -1, -1, 5);
    assert(data[0] == 0xdeadbeef && data[101] == 0xdeadbeef);
    assert(framebuffer_color(0x123456, 16, 8, 8, 8, 0, 8) == 0x123456);
    assert(framebuffer_color(0x123456, 0, 8, 8, 8, 16, 8) == 0x563412);
    puts("PASS clipping, extreme coordinates, pixel channel layouts");
}
static void elf_tests(void) {
    _Alignas(16) uint8_t data[8192] = {0};
    struct ElfHeader *h = (void *)data;
    memcpy(h->ident, "\177ELF\2\1\1", 7);
    h->type = 2;
    h->machine = 62;
    h->version = 1;
    h->entry = UINT64_C(0xffffffff80100000);
    h->phoff = 64;
    h->ehsize = 64;
    h->phentsize = 56;
    h->phnum = 1;
    struct ElfProgram *p = (void *)(data + 64);
    *p = (struct ElfProgram){1, 5, 4096, h->entry, 0x100000, 16, 4096, 4096};
    assert(elf_valid(data, sizeof(data)));
    assert(!elf_valid(data, 10));
    p->filesz = UINT64_MAX;
    assert(!elf_valid(data, sizeof(data)));
    p->filesz = 16;
    p->flags = 7;
    assert(!elf_valid(data, sizeof(data)));
    p->flags = 5;
    h->entry++;
    assert(elf_valid(data, sizeof(data)));
    h->entry = 0;
    assert(!elf_valid(data, sizeof(data)));
    h->entry = p->vaddr;
    h->phnum = 2;
    p[1] = p[0];
    assert(!elf_valid(data, sizeof(data)));
    puts("PASS ELF format, truncation, overflow, W+X rejection, executable entry, overlaps");
}
int main(void) {
    frames();
    heaps();
    inputs();
    graphics();
    elf_tests();
    puts("HOST TESTS:PASS");
}
