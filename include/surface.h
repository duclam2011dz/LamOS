/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMOS_SURFACE_H
#define LAMOS_SURFACE_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define SURFACE_MAX_DIM 4096
struct Point {
    int x, y;
};
struct Rect {
    int x, y, w, h;
};
enum PixelFormat { PIXEL_XRGB8888, PIXEL_ARGB8888_PRE };
struct GfxAllocator {
    void *(*alloc)(size_t, void *);
    void (*release)(void *, void *);
    void *context;
};
struct Surface {
    uint8_t *pixels;
    int width, height;
    size_t stride, bytes;
    enum PixelFormat format;
    struct Rect clip;
    bool owned;
    struct GfxAllocator allocator;
};
struct Rect rect_intersect(struct Rect, struct Rect);
struct Rect rect_union(struct Rect, struct Rect);
bool rect_contains(struct Rect, int, int);
bool surface_view(struct Surface *, void *, size_t, int, int, size_t, enum PixelFormat);
bool surface_create(struct Surface *, int, int, enum PixelFormat, struct GfxAllocator);
void surface_destroy(struct Surface *);
void surface_clip(struct Surface *, struct Rect);
uint32_t color_rgba(uint8_t, uint8_t, uint8_t, uint8_t);
uint32_t color_over(uint32_t, uint32_t);
uint32_t surface_get(const struct Surface *, int, int);
void surface_pixel(struct Surface *, int, int, uint32_t);
void surface_rect(struct Surface *, struct Rect, uint32_t);
void surface_outline(struct Surface *, struct Rect, uint32_t);
void surface_line(struct Surface *, struct Point, struct Point, uint32_t);
void surface_circle(struct Surface *, struct Point, int, uint32_t, bool);
bool surface_polygon(struct Surface *, const struct Point *, size_t, uint32_t, bool);
bool surface_blit(struct Surface *, struct Point, const struct Surface *, struct Rect, bool);
#endif
