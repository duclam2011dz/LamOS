/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMOS_COMPOSITOR_H
#define LAMOS_COMPOSITOR_H
#include "surface.h"
#define DAMAGE_CAPACITY 64
struct Layer {
    const struct Surface *surface;
    struct Rect rect;
};
struct ComposeStats {
    uint64_t pixels;
    unsigned rectangles;
};
typedef void (*OverlayDraw)(struct Surface *, void *);
typedef void (*PresentRect)(const struct Surface *, struct Rect, void *);
struct Compositor {
    struct Surface background, back, front;
    struct Rect damage[DAMAGE_CAPACITY];
    unsigned count;
};
bool compositor_init(struct Compositor *, int, int, struct GfxAllocator);
void compositor_destroy(struct Compositor *);
void compositor_damage(struct Compositor *, struct Rect);
void compositor_full(struct Compositor *);
struct ComposeStats compositor_render(struct Compositor *, const struct Layer *, size_t,
                                      OverlayDraw, PresentRect, void *);
#endif
