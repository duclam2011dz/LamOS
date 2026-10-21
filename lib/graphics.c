/* SPDX-License-Identifier: GPL-3.0-only */
#include "graphics.h"
uint32_t framebuffer_color(uint32_t rgb, uint8_t rs, uint8_t rb, uint8_t gs, uint8_t gb, uint8_t bs,
                           uint8_t bb) {
    uint32_t r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
    return ((r * ((1u << rb) - 1) / 255) << rs) | ((g * ((1u << gb) - 1) / 255) << gs) |
           ((b * ((1u << bb) - 1) / 255) << bs);
}
