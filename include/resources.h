/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMOS_RESOURCES_H
#define LAMOS_RESOURCES_H
#include "surface.h"
#define FONT_WIDTH 8
#define FONT_HEIGHT 16
enum TextAlign { TEXT_LEFT, TEXT_CENTER, TEXT_RIGHT };
enum IconKind { ICON_TERMINAL, ICON_FOLDER, ICON_SETTINGS, ICON_CLOSE };
const uint8_t *font_glyph(unsigned char);
struct Point text_measure(const char *, int);
void font_draw(struct Surface *, int, int, unsigned char, uint32_t);
void text_draw(struct Surface *, struct Rect, const char *, uint32_t, enum TextAlign, int);
void icon_draw(struct Surface *, struct Rect, enum IconKind, uint32_t);
#endif
