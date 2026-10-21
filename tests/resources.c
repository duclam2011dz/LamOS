/* SPDX-License-Identifier: GPL-3.0-only */
#include "resources.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    for (unsigned c = 33; c <= 126; c++) {
        const uint8_t *g = font_glyph((unsigned char)c);
        unsigned ink = 0;
        for (int r = 0; r < 16; r++)
            ink |= g[r];
        assert(ink);
    }
    assert(memcmp(font_glyph('a'), font_glyph('A'), 16));
    assert(font_glyph(0) == font_glyph('?'));
    uint32_t pixels[130];
    for (unsigned i = 0; i < 130; i++)
        pixels[i] = 0xdeadbeef;
    struct Surface s;
    assert(surface_view(&s, pixels + 1, 512, 16, 8, 64, PIXEL_XRGB8888));
    text_draw(&s, (struct Rect){-4, -8, 20, 16}, "Az", 0xffffffff, TEXT_RIGHT, 0);
    assert(pixels[0] == 0xdeadbeef && pixels[129] == 0xdeadbeef);
    struct Point size = text_measure("abc\nx", 2);
    assert(size.x == 24 && size.y == 34);
    for (int i = 0; i < 4; i++)
        icon_draw(&s, (struct Rect){-2, -2, 16, 16}, (enum IconKind)i, 0xff00ff00);
    assert(pixels[0] == 0xdeadbeef && pixels[129] == 0xdeadbeef);
    puts("RESOURCES:PASS ASCII, case, alignment, clipping, icons");
}
