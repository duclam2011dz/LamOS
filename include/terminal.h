/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMOS_TERMINAL_H
#define LAMOS_TERMINAL_H
#include "resources.h"
#include "input.h"
#define TERM_COLUMNS 160
#define TERM_SCROLLBACK 1024
#define TERM_HISTORY 32
#define TERM_INPUT 256
#define TERM_ROWS 64
struct Cell {
    uint8_t character, foreground, background, flags;
};
struct Terminal;
typedef void (*TerminalSubmit)(struct Terminal *, const char *, void *);
typedef void (*TerminalDamage)(struct Rect, void *);
struct Terminal {
    struct Cell lines[TERM_SCROLLBACK][TERM_COLUMNS];
    struct Cell painted[TERM_ROWS][TERM_COLUMNS];
    unsigned first, count, column, columns, rows, scroll;
    char input[TERM_INPUT + 1], draft[TERM_INPUT + 1], history[TERM_HISTORY][TERM_INPUT + 1];
    unsigned length, caret, history_next, history_count, history_age;
    bool invalid;
    struct GfxAllocator allocator;
    TerminalSubmit submit;
    void *context;
};
struct Terminal *terminal_create(struct GfxAllocator, TerminalSubmit, void *);
void terminal_destroy(struct Terminal *);
void terminal_resize(struct Terminal *, unsigned, unsigned);
void terminal_write(struct Terminal *, const char *, uint8_t);
void terminal_clear(struct Terminal *);
void terminal_event(struct Terminal *, const struct Event *);
void terminal_render(struct Terminal *, struct Surface *, struct Rect, bool, bool, TerminalDamage,
                     void *);
#endif
