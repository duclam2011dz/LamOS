/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMOS_GRAPHICS_H
#define LAMOS_GRAPHICS_H
#include <stdint.h>
#include <stdbool.h>
struct Canvas {
    uint32_t *pixels;
    int width, height;
};
void canvas_pixel(struct Canvas *, int, int, uint32_t);
void canvas_rect(struct Canvas *, int, int, int, int, uint32_t);
void canvas_line(struct Canvas *, int, int, int, int, uint32_t);
void canvas_text(struct Canvas *, int, int, const char *, uint32_t, int);
uint32_t framebuffer_color(uint32_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
#endif
