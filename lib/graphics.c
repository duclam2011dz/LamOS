/* SPDX-License-Identifier: GPL-3.0-only */
#include "graphics.h"
#include <stddef.h>
void canvas_pixel(struct Canvas *c, int x, int y, uint32_t color) {
    if (x >= 0 && y >= 0 && x < c->width && y < c->height)
        c->pixels[(size_t)y * (size_t)c->width + (size_t)x] = color;
}
void canvas_rect(struct Canvas *c, int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0)
        return;
    int64_t right = (int64_t)x + w, bottom = (int64_t)y + h;
    if (right > c->width)
        right = c->width;
    if (bottom > c->height)
        bottom = c->height;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    for (int yy = y; yy < bottom; yy++)
        for (int xx = x; xx < right; xx++)
            c->pixels[(size_t)yy * (size_t)c->width + (size_t)xx] = color;
}
static unsigned region(const struct Canvas *c, int64_t x, int64_t y) {
    return (x < 0 ? 1u : x >= c->width ? 2u : 0u) | (y < 0 ? 4u : y >= c->height ? 8u : 0u);
}
void canvas_line(struct Canvas *c, int ax, int ay, int bx, int by, uint32_t color) {
    int64_t x0 = ax, y0 = ay, x1 = bx, y1 = by;
    for (unsigned n = 0; n < 8; n++) {
        unsigned a = region(c, x0, y0), b = region(c, x1, y1);
        if (!(a | b))
            break;
        if (a & b)
            return;
        unsigned r = a ? a : b;
        int64_t x, y;
        if (r & 8) {
            if (y1 == y0)
                return;
            y = c->height - 1;
            x = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
        } else if (r & 4) {
            if (y1 == y0)
                return;
            y = 0;
            x = x0 + (x1 - x0) * (-y0) / (y1 - y0);
        } else if (r & 2) {
            if (x1 == x0)
                return;
            x = c->width - 1;
            y = y0 + (y1 - y0) * (x - x0) / (x1 - x0);
        } else {
            if (x1 == x0)
                return;
            x = 0;
            y = y0 + (y1 - y0) * (-x0) / (x1 - x0);
        }
        if (a) {
            x0 = x;
            y0 = y;
        } else {
            x1 = x;
            y1 = y;
        }
    }
    if (region(c, x0, y0) || region(c, x1, y1))
        return;
    int64_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = -(y1 > y0 ? y1 - y0 : y0 - y1),
            sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        canvas_pixel(c, (int)x0, (int)y0, color);
        if (x0 == x1 && y0 == y1)
            break;
        int64_t e = 2 * err;
        if (e >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}
/* Original LamOS 5x7 alphabet; lowercase deliberately shares uppercase shapes. */
static const uint8_t letters[26][7] = {
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {14, 4, 4, 4, 4, 4, 14},
    {7, 2, 2, 2, 18, 18, 12},     {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31}};
static const uint8_t digits[10][7] = {{14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},
                                      {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
                                      {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},
                                      {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
                                      {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14}};
static uint8_t row(char ch, int y) {
    if (ch >= 'a' && ch <= 'z')
        ch = (char)(ch - 'a' + 'A');
    if (ch >= 'A' && ch <= 'Z')
        return letters[ch - 'A'][y];
    if (ch >= '0' && ch <= '9')
        return digits[ch - '0'][y];
    switch (ch) {
    case '|':
        return 4;
    case ',':
        return y == 5 ? 4 : y == 6 ? 8 : 0;
    case '(':
        return y == 0 || y == 6 ? 2 : y == 1 || y == 5 ? 4 : 8;
    case ')':
        return y == 0 || y == 6 ? 8 : y == 1 || y == 5 ? 4 : 2;
    case '.':
        return y == 6 ? 4 : 0;
    case ':':
        return y == 2 || y == 5 ? 4 : 0;
    case '-':
        return y == 3 ? 14 : 0;
    case '_':
        return y == 6 ? 31 : 0;
    case '/':
        return (uint8_t)(1u << (y < 5 ? y : 4));
    case '!':
        return y < 4 || y == 6 ? 4 : 0;
    case '[':
        return y == 0 || y == 6 ? 14 : 8;
    case ']':
        return y == 0 || y == 6 ? 14 : 2;
    case '>':
        return (uint8_t)(1u << (y < 4 ? y : 6 - y));
    case ' ':
        return 0;
    default:
        return y == 0 ? 14 : y == 1 ? 17 : y == 2 ? 1 : y == 3 ? 2 : y == 4 || y == 6 ? 4 : 0;
    }
}
void canvas_text(struct Canvas *c, int x, int y, const char *s, uint32_t color, int scale) {
    if (scale < 1 || scale > 4)
        return;
    int start = x;
    while (*s) {
        char ch = *s++;
        if (ch == '\n') {
            x = start;
            y += 9 * scale;
            continue;
        }
        if (x + 5 * scale > c->width)
            break;
        for (int r = 0; r < 7; r++) {
            uint8_t bits = row(ch, r);
            for (int col = 0; col < 5; col++)
                if (bits & (1u << (4 - col)))
                    canvas_rect(c, x + col * scale, y + r * scale, scale, scale, color);
        }
        x += 6 * scale;
    }
}
uint32_t framebuffer_color(uint32_t rgb, uint8_t rs, uint8_t rb, uint8_t gs, uint8_t gb, uint8_t bs,
                           uint8_t bb) {
    uint32_t r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
    return ((r * ((1u << rb) - 1) / 255) << rs) | ((g * ((1u << gb) - 1) / 255) << gs) |
           ((b * ((1u << bb) - 1) / 255) << bs);
}
