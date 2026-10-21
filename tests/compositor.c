/* SPDX-License-Identifier: GPL-3.0-only */
#include "compositor.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void *allocate(size_t n, void *ctx) {
    (void)ctx;
    return malloc(n);
}
static void release(void *p, void *ctx) {
    (void)ctx;
    free(p);
}
int main(void) {
    struct GfxAllocator a = {allocate, release, 0};
    struct Compositor c;
    assert(compositor_init(&c, 160, 100, a));
    struct Surface one, two;
    assert(surface_create(&one, 50, 40, PIXEL_XRGB8888, a) &&
           surface_create(&two, 50, 40, PIXEL_XRGB8888, a));
    surface_rect(&one, one.clip, 0xffff0000);
    surface_rect(&two, two.clip, 0xff00ff00);
    struct Layer layers[] = {{&one, {10, 10, 50, 40}}, {&two, {30, 20, 50, 40}}};
    compositor_render(&c, layers, 2, 0, 0, 0);
    assert(surface_get(&c.front, 40, 30) == 0xff00ff00);
    compositor_damage(&c, layers[1].rect);
    layers[1].rect.x = 80;
    compositor_damage(&c, (struct Rect){30, 20, 54, 44});
    compositor_damage(&c, (struct Rect){80, 20, 54, 44});
    compositor_render(&c, layers, 2, 0, 0, 0);
    uint8_t expected[160 * 100 * 4];
    memcpy(expected, c.front.pixels, sizeof(expected));
    compositor_full(&c);
    compositor_render(&c, layers, 2, 0, 0, 0);
    assert(!memcmp(expected, c.front.pixels, sizeof(expected)));
    assert(surface_get(&c.front, 40, 30) == 0xffff0000);
    for (int i = 0; i < 65; i++)
        compositor_damage(&c, (struct Rect){(i % 16) * 10, (i / 16) * 10, 1, 1});
    assert(c.count == 1);
    compositor_destroy(&c);
    surface_destroy(&one);
    surface_destroy(&two);
    puts("COMPOSITOR:PASS z-order, damage/full equivalence, overflow, ownership");
}
