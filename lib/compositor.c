/* SPDX-License-Identifier: GPL-3.0-only */
#include "compositor.h"
bool compositor_init(struct Compositor *c, int w, int h, struct GfxAllocator a) {
    *c = (struct Compositor){0};
    if (!surface_create(&c->background, w, h, PIXEL_XRGB8888, a) ||
        !surface_create(&c->back, w, h, PIXEL_XRGB8888, a) ||
        !surface_create(&c->front, w, h, PIXEL_XRGB8888, a)) {
        compositor_destroy(c);
        return false;
    }
    compositor_full(c);
    return true;
}
void compositor_destroy(struct Compositor *c) {
    surface_destroy(&c->background);
    surface_destroy(&c->back);
    surface_destroy(&c->front);
    c->count = 0;
}
void compositor_full(struct Compositor *c) {
    c->count = 1;
    c->damage[0] = (struct Rect){0, 0, c->back.width, c->back.height};
}
void compositor_damage(struct Compositor *c, struct Rect r) {
    r = rect_intersect(r, (struct Rect){0, 0, c->back.width, c->back.height});
    if (!r.w || !r.h)
        return;
    for (unsigned i = 0; i < c->count;) {
        struct Rect old = c->damage[i], expanded = {old.x - 1, old.y - 1, old.w + 2, old.h + 2};
        if (rect_intersect(r, expanded).w) {
            r = rect_union(r, old);
            c->damage[i] = c->damage[--c->count];
            i = 0;
        } else
            i++;
    }
    if (c->count == DAMAGE_CAPACITY) {
        compositor_full(c);
        return;
    }
    c->damage[c->count++] = r;
}
static void shadow(struct Surface *s, struct Rect r) {
    r = rect_intersect(r, s->clip);
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++)
            surface_pixel(s, x, y, color_over(0x40000000, surface_get(s, x, y)));
}
struct ComposeStats compositor_render(struct Compositor *c, const struct Layer *layers,
                                      size_t count, OverlayDraw overlay, PresentRect present,
                                      void *context) {
    struct ComposeStats stats = {0};
    if (count > 16)
        return stats;
    for (unsigned i = 0; i < c->count; i++) {
        struct Rect r = c->damage[i];
        surface_clip(&c->back, r);
        surface_blit(&c->back, (struct Point){0, 0}, &c->background,
                     (struct Rect){0, 0, c->back.width, c->back.height}, false);
        for (size_t j = 0; j < count; j++) {
            struct Rect rect = layers[j].rect;
            shadow(&c->back, (struct Rect){rect.x + 4, rect.y + 4, rect.w, rect.h});
            surface_blit(&c->back, (struct Point){rect.x, rect.y}, layers[j].surface,
                         (struct Rect){0, 0, rect.w, rect.h}, false);
        }
        if (overlay)
            overlay(&c->back, context);
        surface_clip(&c->front, r);
        surface_blit(&c->front, (struct Point){r.x, r.y}, &c->back, r, false);
        if (present)
            present(&c->front, r, context);
        stats.pixels += (uint64_t)(unsigned)r.w * (unsigned)r.h;
        stats.rectangles++;
    }
    c->count = 0;
    surface_clip(&c->back, (struct Rect){0, 0, c->back.width, c->back.height});
    surface_clip(&c->front, (struct Rect){0, 0, c->front.width, c->front.height});
    return stats;
}
