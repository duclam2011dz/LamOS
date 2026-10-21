/* SPDX-License-Identifier: GPL-3.0-only */
#include "resources.h"
#include "font_data.inc"
const uint8_t *font_glyph(unsigned char c) {
    if (c < 32 || c > 126)
        c = '?';
    return ascii_8x16[c - 32];
}
struct Point text_measure(const char *s, int spacing) {
    if (spacing < 0 || spacing > 64)
        spacing = 0;
    int width = 0, line = 0, lines = 1;
    for (unsigned i = 0; s && s[i] && i < 4096; i++) {
        if (s[i] == '\n') {
            if (line > width)
                width = line;
            line = 0;
            lines++;
        } else
            line += 8;
    }
    if (line > width)
        width = line;
    return (struct Point){width, lines * 16 + (lines - 1) * spacing};
}
void font_draw(struct Surface *s, int x, int y, unsigned char ch, uint32_t c) {
    const uint8_t *glyph = font_glyph(ch);
    for (int r = 0; r < 16; r++)
        for (int col = 0; col < 8; col++)
            if (glyph[r] & (128u >> col)) {
                int64_t xx = (int64_t)x + col, yy = (int64_t)y + r;
                if (xx <= INT32_MAX && yy <= INT32_MAX)
                    surface_pixel(s, (int)xx, (int)yy, c);
            }
}
void text_draw(struct Surface *s, struct Rect box, const char *str, uint32_t c,
               enum TextAlign align, int spacing) {
    if (!str)
        return;
    if (spacing < 0 || spacing > 64)
        spacing = 0;
    struct Rect saved = s->clip;
    surface_clip(s, rect_intersect(saved, box));
    unsigned at = 0;
    int64_t y = box.y;
    while (at < 4096 && str[at] && y < (int64_t)box.y + box.h) {
        unsigned end = at;
        while (end < 4096 && str[end] && str[end] != '\n')
            end++;
        int width = (int)(end - at) * 8;
        int64_t x = box.x;
        if (align == TEXT_CENTER)
            x += ((int64_t)box.w - width) / 2;
        if (align == TEXT_RIGHT)
            x += (int64_t)box.w - width;
        for (; at < end; at++, x += 8)
            if (x >= INT32_MIN && x <= INT32_MAX && y >= INT32_MIN && y <= INT32_MAX)
                font_draw(s, (int)x, (int)y, (unsigned char)str[at], c);
        if (str[at] == '\n')
            at++;
        y += 16 + spacing;
    }
    s->clip = saved;
}
void icon_draw(struct Surface *s, struct Rect r, enum IconKind kind, uint32_t c) {
    if (r.w < 8 || r.h < 8)
        return;
    struct Rect saved = s->clip;
    surface_clip(s, rect_intersect(saved, r));
    int x = r.x, y = r.y, w = r.w, h = r.h;
    if (kind == ICON_TERMINAL) {
        surface_rect(s, (struct Rect){x + 1, y + 2, w - 2, h - 4}, 0xff102235);
        surface_outline(s, (struct Rect){x + 1, y + 2, w - 2, h - 4}, c);
        surface_rect(s, (struct Rect){x + 2, y + 3, w - 4, 3}, c);
        surface_line(s, (struct Point){x + w / 5, y + h / 3}, (struct Point){x + w / 3, y + h / 2},
                     c);
        surface_line(s, (struct Point){x + w / 3, y + h / 2},
                     (struct Point){x + w / 5, y + h * 2 / 3}, c);
        surface_rect(s, (struct Rect){x + w / 2, y + h * 2 / 3, w / 4, 2}, c);
    } else if (kind == ICON_FOLDER) {
        struct Point p[] = {{x + 1, y + h / 4},     {x + w / 3, y + h / 4}, {x + w / 2, y + h / 3},
                            {x + w - 2, y + h / 3}, {x + w - 2, y + h - 3}, {x + 1, y + h - 3}};
        surface_polygon(s, p, 6, c, true);
        surface_rect(s, (struct Rect){x + 3, y + h / 2, w - 6, h / 2 - 5}, 0xffc89b35);
    } else if (kind == ICON_SETTINGS) {
        int cx = x + w / 2, cy = y + h / 2, rad = (w < h ? w : h) / 3;
        surface_circle(s, (struct Point){cx, cy}, rad, c, true);
        for (int i = -1; i <= 1; i += 2) {
            surface_rect(s, (struct Rect){cx - 2, cy + i * rad - 2, 4, 5}, c);
            surface_rect(s, (struct Rect){cx + i * rad - 2, cy - 2, 5, 4}, c);
            surface_line(s, (struct Point){cx + i * rad, cy - rad},
                         (struct Point){cx - i * rad, cy + rad}, c);
        }
        surface_circle(s, (struct Point){cx, cy}, rad / 2, 0xff183148, true);
    } else {
        surface_line(s, (struct Point){x + 2, y + 2}, (struct Point){x + w - 3, y + h - 3}, c);
        surface_line(s, (struct Point){x + w - 3, y + 2}, (struct Point){x + 2, y + h - 3}, c);
    }
    s->clip = saved;
}
