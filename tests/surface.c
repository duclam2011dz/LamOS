/* SPDX-License-Identifier: GPL-3.0-only */
#include "surface.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
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
    uint32_t guard[16 * 20 + 2];
    for (unsigned i = 0; i < 322; i++)
        guard[i] = 0xdeadbeef;
    struct Surface s;
    assert(!surface_view(&s, guard + 1, 1280, 16, 16, 63, PIXEL_XRGB8888));
    assert(surface_view(&s, guard + 1, 1280, 16, 16, 80, PIXEL_XRGB8888));
    surface_rect(&s, (struct Rect){-10, -10, 15, 15}, 0x123456);
    assert(surface_get(&s, 0, 0) == 0xff123456 && guard[0] == 0xdeadbeef &&
           guard[321] == 0xdeadbeef && guard[17] == 0xdeadbeef);
    surface_line(&s, (struct Point){INT_MIN, 8}, (struct Point){INT_MAX, 8}, 0xffffff);
    for (int x = 0; x < 16; x++)
        assert(surface_get(&s, x, 8) == 0xffffffff);
    surface_rect(&s, (struct Rect){INT_MAX, INT_MAX, INT_MAX, INT_MAX}, 0);
    surface_circle(&s, (struct Point){4, 4}, 3, 0x00ff00, true);
    assert(surface_get(&s, 4, 4) == 0xff00ff00);
    surface_circle(&s, (struct Point){INT_MIN, INT_MIN}, INT_MAX, 1, true);
    struct Point triangle[] = {{0, 0}, {10, 0}, {0, 10}};
    assert(surface_polygon(&s, triangle, 3, 0xff0000, true));
    assert(surface_get(&s, 1, 1) == 0xffff0000);
    assert(!surface_polygon(&s, triangle, 65, 0, true));
    surface_clip(&s, (struct Rect){2, 2, 2, 2});
    surface_rect(&s, (struct Rect){0, 0, 16, 16}, 0);
    assert(surface_get(&s, 2, 2) == 0xff000000 && surface_get(&s, 1, 1) == 0xffff0000);
    surface_clip(&s, (struct Rect){0, 0, 16, 16});
    for (int x = 0; x < 16; x++)
        surface_pixel(&s, x, 12, (uint32_t)x);
    assert(surface_blit(&s, (struct Point){2, 12}, &s, (struct Rect){0, 12, 14, 1}, false));
    for (int x = 2; x < 16; x++)
        assert((surface_get(&s, x, 12) & 255) == (unsigned)x - 2);
    assert(!surface_blit(&s, (struct Point){0, 0}, &s, s.clip, true));
    assert(color_over(0, 0xff0000ff) == 0xff0000ff);
    assert(color_over(0xffff0000, 0xff0000ff) == 0xffff0000);
    assert(color_over(color_rgba(255, 0, 0, 128), 0xff0000ff) == 0xff80007f);
    struct Surface owned;
    assert(surface_create(&owned, 32, 32, PIXEL_ARGB8888_PRE,
                          (struct GfxAllocator){allocate, release, 0}));
    assert(surface_get(&owned, 0, 0) == 0);
    surface_destroy(&owned);
    assert(!owned.pixels);
    assert(guard[0] == 0xdeadbeef && guard[321] == 0xdeadbeef);
    puts("SURFACE:PASS clipping, stride, overflow, circles, polygons, overlap, alpha, ownership");
}
