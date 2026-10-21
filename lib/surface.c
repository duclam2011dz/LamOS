/* SPDX-License-Identifier: GPL-3.0-only */
#include "surface.h"
static int64_t maximum(int64_t a, int64_t b) {
    return a > b ? a : b;
}
static int64_t minimum(int64_t a, int64_t b) {
    return a < b ? a : b;
}
struct Rect rect_intersect(struct Rect a, struct Rect b) {
    int64_t x = maximum(a.x, b.x), y = maximum(a.y, b.y),
            r = minimum((int64_t)a.x + a.w, (int64_t)b.x + b.w),
            bot = minimum((int64_t)a.y + a.h, (int64_t)b.y + b.h);
    if (a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0 || r <= x || bot <= y)
        return (struct Rect){0, 0, 0, 0};
    return (struct Rect){(int)x, (int)y, (int)(r - x), (int)(bot - y)};
}
struct Rect rect_union(struct Rect a, struct Rect b) {
    if (a.w <= 0 || a.h <= 0)
        return b;
    if (b.w <= 0 || b.h <= 0)
        return a;
    int64_t x = minimum(a.x, b.x), y = minimum(a.y, b.y),
            r = maximum((int64_t)a.x + a.w, (int64_t)b.x + b.w),
            bot = maximum((int64_t)a.y + a.h, (int64_t)b.y + b.h);
    if (r - x > INT32_MAX || bot - y > INT32_MAX)
        return (struct Rect){0, 0, 0, 0};
    return (struct Rect){(int)x, (int)y, (int)(r - x), (int)(bot - y)};
}
bool rect_contains(struct Rect r, int x, int y) {
    return r.w > 0 && r.h > 0 && x >= r.x && y >= r.y && (int64_t)x < (int64_t)r.x + r.w &&
           (int64_t)y < (int64_t)r.y + r.h;
}
bool surface_view(struct Surface *s, void *pixels, size_t bytes, int w, int h, size_t stride,
                  enum PixelFormat f) {
    if (!s || !pixels || ((uintptr_t)pixels & 3) || w <= 0 || h <= 0 || w > SURFACE_MAX_DIM ||
        h > SURFACE_MAX_DIM || (f != PIXEL_XRGB8888 && f != PIXEL_ARGB8888_PRE) || stride % 4 ||
        stride < (size_t)w * 4 || stride > SIZE_MAX / (size_t)h || bytes < stride * (size_t)h)
        return false;
    *s = (struct Surface){.pixels = pixels,
                          .width = w,
                          .height = h,
                          .stride = stride,
                          .bytes = bytes,
                          .format = f,
                          .clip = {0, 0, w, h}};
    return true;
}
bool surface_create(struct Surface *s, int w, int h, enum PixelFormat f, struct GfxAllocator a) {
    if (!s || !a.alloc || !a.release || w <= 0 || h <= 0 || w > SURFACE_MAX_DIM ||
        h > SURFACE_MAX_DIM)
        return false;
    size_t n = (size_t)w * (size_t)h * 4;
    void *p = a.alloc(n, a.context);
    if (!p)
        return false;
    if (!surface_view(s, p, n, w, h, (size_t)w * 4, f)) {
        a.release(p, a.context);
        return false;
    }
    s->owned = true;
    s->allocator = a;
    surface_rect(s, s->clip, f == PIXEL_XRGB8888 ? 0xff000000 : 0);
    return true;
}
void surface_destroy(struct Surface *s) {
    if (!s)
        return;
    if (s->owned && s->pixels)
        s->allocator.release(s->pixels, s->allocator.context);
    *s = (struct Surface){0};
}
void surface_clip(struct Surface *s, struct Rect r) {
    s->clip = rect_intersect(r, (struct Rect){0, 0, s->width, s->height});
}
uint32_t color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24) | (((uint32_t)r * a + 127) / 255 << 16) |
           (((uint32_t)g * a + 127) / 255 << 8) | ((uint32_t)b * a + 127) / 255;
}
uint32_t color_over(uint32_t s, uint32_t d) {
    uint32_t inv = 255 - (s >> 24), o = 0;
    for (unsigned shift = 0; shift <= 24; shift += 8) {
        uint32_t c = ((s >> shift) & 255) + (((d >> shift) & 255) * inv + 127) / 255;
        if (c > 255)
            c = 255;
        o |= c << shift;
    }
    return o;
}
static uint32_t *row(struct Surface *s, int y) {
    return (void *)(s->pixels + (size_t)y * s->stride);
}
uint32_t surface_get(const struct Surface *s, int x, int y) {
    if (!rect_contains((struct Rect){0, 0, s->width, s->height}, x, y))
        return 0;
    const uint32_t *p = (const void *)(s->pixels + (size_t)y * s->stride);
    return p[x] | (s->format == PIXEL_XRGB8888 ? 0xff000000u : 0);
}
void surface_pixel(struct Surface *s, int x, int y, uint32_t c) {
    if (rect_contains(s->clip, x, y))
        row(s, y)[x] = c | (s->format == PIXEL_XRGB8888 ? 0xff000000u : 0);
}
void surface_rect(struct Surface *s, struct Rect r, uint32_t c) {
    r = rect_intersect(r, s->clip);
    if (s->format == PIXEL_XRGB8888)
        c |= 0xff000000;
    for (int y = r.y; y < r.y + r.h; y++) {
        uint32_t *p = row(s, y);
        for (int x = r.x; x < r.x + r.w; x++)
            p[x] = c;
    }
}
void surface_outline(struct Surface *s, struct Rect r, uint32_t c) {
    if (r.w <= 0 || r.h <= 0)
        return;
    surface_rect(s, (struct Rect){r.x, r.y, r.w, 1}, c);
    int64_t bot = (int64_t)r.y + r.h - 1, right = (int64_t)r.x + r.w - 1;
    if (bot <= INT32_MAX)
        surface_rect(s, (struct Rect){r.x, (int)bot, r.w, 1}, c);
    surface_rect(s, (struct Rect){r.x, r.y, 1, r.h}, c);
    if (right <= INT32_MAX)
        surface_rect(s, (struct Rect){(int)right, r.y, 1, r.h}, c);
}
/* Inputs are differences of signed 32-bit coordinates. Only intersecting edge
 * fractions are evaluated, so the unsigned product fits and the quotient is bounded. */
static int64_t fraction(int64_t a, int64_t b, int64_t c) {
    bool neg = (a < 0) ^ (b < 0) ^ (c < 0);
    uint64_t aa = (uint64_t)(a < 0 ? -a : a), bb = (uint64_t)(b < 0 ? -b : b),
             cc = (uint64_t)(c < 0 ? -c : c);
    uint64_t result = aa * bb / cc;
    return neg ? -(int64_t)result : (int64_t)result;
}
static unsigned outside(struct Rect r, int64_t x, int64_t y) {
    return (x < r.x                   ? 1u
            : x >= (int64_t)r.x + r.w ? 2u
                                      : 0u) |
           (y < r.y                   ? 4u
            : y >= (int64_t)r.y + r.h ? 8u
                                      : 0u);
}
void surface_line(struct Surface *s, struct Point a, struct Point b, uint32_t c) {
    if (!s->clip.w || !s->clip.h)
        return;
    int64_t x0 = a.x, y0 = a.y, x1 = b.x, y1 = b.y;
    for (unsigned i = 0; i < 8; i++) {
        unsigned oa = outside(s->clip, x0, y0), ob = outside(s->clip, x1, y1);
        if (!(oa | ob))
            break;
        if (oa & ob)
            return;
        unsigned r = oa ? oa : ob;
        int64_t x, y;
        if (r & 12) {
            if (y1 == y0)
                return;
            y = r & 4 ? s->clip.y : (int64_t)s->clip.y + s->clip.h - 1;
            x = x0 + fraction(x1 - x0, y - y0, y1 - y0);
        } else {
            if (x1 == x0)
                return;
            x = r & 1 ? s->clip.x : (int64_t)s->clip.x + s->clip.w - 1;
            y = y0 + fraction(y1 - y0, x - x0, x1 - x0);
        }
        if (oa) {
            x0 = x;
            y0 = y;
        } else {
            x1 = x;
            y1 = y;
        }
    }
    if (outside(s->clip, x0, y0) || outside(s->clip, x1, y1))
        return;
    int64_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = -(y1 > y0 ? y1 - y0 : y0 - y1),
            sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;) {
        surface_pixel(s, (int)x0, (int)y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        int64_t ee = 2 * e;
        if (ee >= dy) {
            e += dy;
            x0 += sx;
        }
        if (ee <= dx) {
            e += dx;
            y0 += sy;
        }
    }
}
void surface_circle(struct Surface *s, struct Point p, int radius, uint32_t c, bool fill) {
    if (radius < 0)
        return;
    int64_t x0 = maximum(s->clip.x, (int64_t)p.x - radius),
            x1 = minimum((int64_t)s->clip.x + s->clip.w, (int64_t)p.x + radius + 1),
            y0 = maximum(s->clip.y, (int64_t)p.y - radius),
            y1 = minimum((int64_t)s->clip.y + s->clip.h, (int64_t)p.y + radius + 1);
    uint64_t rr = (uint64_t)radius * (unsigned)radius,
             inner = radius ? (uint64_t)(radius - 1) * (unsigned)(radius - 1) : 0;
    for (int64_t y = y0; y < y1; y++)
        for (int64_t x = x0; x < x1; x++) {
            int64_t dx = x - p.x, dy = y - p.y;
            uint64_t d = (uint64_t)(dx * dx) + (uint64_t)(dy * dy);
            if (d <= rr && (fill || d > inner || radius == 0))
                surface_pixel(s, (int)x, (int)y, c);
        }
}
bool surface_polygon(struct Surface *s, const struct Point *p, size_t n, uint32_t c, bool fill) {
    if (!p || n < 3 || n > 64)
        return false;
    if (!fill) {
        for (size_t i = 0; i < n; i++)
            surface_line(s, p[i], p[(i + 1) % n], c);
        return true;
    }
    for (int y = s->clip.y; y < s->clip.y + s->clip.h; y++) {
        int64_t hits[64];
        size_t count = 0;
        for (size_t i = 0; i < n; i++) {
            struct Point a = p[i], b = p[(i + 1) % n];
            if ((a.y <= y && b.y > y) || (b.y <= y && a.y > y))
                hits[count++] = (int64_t)a.x +
                                fraction((int64_t)y - a.y, (int64_t)b.x - a.x, (int64_t)b.y - a.y);
        }
        for (size_t i = 1; i < count; i++) {
            int64_t v = hits[i];
            size_t j = i;
            while (j && hits[j - 1] > v) {
                hits[j] = hits[j - 1];
                j--;
            }
            hits[j] = v;
        }
        for (size_t i = 0; i + 1 < count; i += 2) {
            int64_t left = maximum(hits[i], s->clip.x),
                    right = minimum(hits[i + 1], (int64_t)s->clip.x + s->clip.w);
            if (right > left)
                surface_rect(s, (struct Rect){(int)left, y, (int)(right - left), 1}, c);
        }
    }
    return true;
}
bool surface_blit(struct Surface *d, struct Point at, const struct Surface *s, struct Rect src,
                  bool blend) {
    if (src.w <= 0 || src.h <= 0)
        return true;
    int64_t x0 = maximum(maximum(at.x, d->clip.x), (int64_t)at.x + s->clip.x - src.x),
            y0 = maximum(maximum(at.y, d->clip.y), (int64_t)at.y + s->clip.y - src.y),
            x1 = minimum(minimum((int64_t)at.x + src.w, (int64_t)d->clip.x + d->clip.w),
                         (int64_t)at.x + s->clip.x + s->clip.w - src.x),
            y1 = minimum(minimum((int64_t)at.y + src.h, (int64_t)d->clip.y + d->clip.h),
                         (int64_t)at.y + s->clip.y + s->clip.h - src.y);
    if (x1 <= x0 || y1 <= y0)
        return true;
    bool overlap = (uintptr_t)d->pixels < (uintptr_t)s->pixels + s->bytes &&
                   (uintptr_t)s->pixels < (uintptr_t)d->pixels + d->bytes;
    if (overlap && (blend || d->stride != s->stride || d->format != s->format))
        return false;
    int sx = (int)((int64_t)src.x + x0 - at.x), sy = (int)((int64_t)src.y + y0 - at.y),
        w = (int)(x1 - x0), h = (int)(y1 - y0);
    uintptr_t dp = (uintptr_t)(d->pixels + (size_t)y0 * d->stride + (size_t)x0 * 4),
              sp = (uintptr_t)(s->pixels + (size_t)sy * s->stride + (size_t)sx * 4);
    bool reverse = overlap && dp > sp;
    for (int iy = 0; iy < h; iy++) {
        int y = reverse ? h - 1 - iy : iy;
        uint32_t *dr = row(d, (int)y0 + y);
        const uint32_t *sr = (const void *)(s->pixels + (size_t)(sy + y) * s->stride);
        for (int ix = 0; ix < w; ix++) {
            int x = reverse ? w - 1 - ix : ix;
            uint32_t v = sr[sx + x] | (s->format == PIXEL_XRGB8888 ? 0xff000000u : 0);
            if (blend)
                v = color_over(v,
                               dr[(int)x0 + x] | (d->format == PIXEL_XRGB8888 ? 0xff000000u : 0));
            dr[(int)x0 + x] = v | (d->format == PIXEL_XRGB8888 ? 0xff000000u : 0);
        }
    }
    return true;
}
