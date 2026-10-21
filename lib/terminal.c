/* SPDX-License-Identifier: GPL-3.0-only */
#include "terminal.h"
static void copy_line(char *to, const char *from) {
    unsigned i = 0;
    for (; i < TERM_INPUT && from[i]; i++)
        to[i] = from[i];
    to[i] = 0;
}
static unsigned text_length(const char *s) {
    unsigned n = 0;
    while (n < TERM_INPUT && s[n])
        n++;
    return n;
}
static void blank_line(struct Cell *line) {
    for (unsigned i = 0; i < TERM_COLUMNS; i++)
        line[i] = (struct Cell){' ', 7, 0, 0};
}
void terminal_clear(struct Terminal *t) {
    t->first = 0;
    t->count = 1;
    t->column = 0;
    t->scroll = 0;
    blank_line(t->lines[0]);
    t->invalid = true;
}
struct Terminal *terminal_create(struct GfxAllocator a, TerminalSubmit submit, void *ctx) {
    struct Terminal *t = a.alloc(sizeof(*t), a.context);
    if (!t)
        return 0;
    uint8_t *p = (void *)t;
    for (size_t i = 0; i < sizeof(*t); i++)
        p[i] = 0;
    t->allocator = a;
    t->submit = submit;
    t->context = ctx;
    t->columns = 80;
    t->rows = 25;
    terminal_clear(t);
    return t;
}
void terminal_destroy(struct Terminal *t) {
    if (t)
        t->allocator.release(t, t->allocator.context);
}
void terminal_resize(struct Terminal *t, unsigned cols, unsigned rows) {
    if (cols < 1)
        cols = 1;
    if (cols > TERM_COLUMNS)
        cols = TERM_COLUMNS;
    if (rows < 1)
        rows = 1;
    if (rows > TERM_ROWS)
        rows = TERM_ROWS;
    if (t->columns != cols || t->rows != rows) {
        t->columns = cols;
        t->rows = rows;
        t->invalid = true;
    }
}
static void next_line(struct Terminal *t) {
    if (t->count < TERM_SCROLLBACK)
        t->count++;
    else
        t->first = (t->first + 1) % TERM_SCROLLBACK;
    unsigned row = (t->first + t->count - 1) % TERM_SCROLLBACK;
    blank_line(t->lines[row]);
    t->column = 0;
}
void terminal_write(struct Terminal *t, const char *s, uint8_t fg) {
    for (size_t i = 0; s && s[i]; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '\n') {
            next_line(t);
            continue;
        }
        if (ch == '\r') {
            t->column = 0;
            continue;
        }
        if (ch < 32 || ch > 126)
            ch = '?';
        if (t->column >= t->columns)
            next_line(t);
        unsigned row = (t->first + t->count - 1) % TERM_SCROLLBACK;
        t->lines[row][t->column++] = (struct Cell){ch, fg, 0, 0};
    }
    t->scroll = 0;
}
static void history_select(struct Terminal *t, bool older) {
    if (older) {
        if (t->history_age >= t->history_count)
            return;
        if (!t->history_age)
            copy_line(t->draft, t->input);
        t->history_age++;
    } else {
        if (!t->history_age)
            return;
        t->history_age--;
    }
    if (t->history_age)
        copy_line(t->input,
                  t->history[(t->history_next + TERM_HISTORY - t->history_age) % TERM_HISTORY]);
    else
        copy_line(t->input, t->draft);
    t->length = text_length(t->input);
    t->caret = t->length;
}
void terminal_event(struct Terminal *t, const struct Event *e) {
    if (e->type == TEXT_INPUT) {
        unsigned char c = (unsigned char)e->text;
        if (c < 32 || c > 126 || t->length == TERM_INPUT)
            return;
        for (unsigned i = t->length + 1; i > t->caret; i--)
            t->input[i] = t->input[i - 1];
        t->input[t->caret++] = (char)c;
        t->length++;
        t->history_age = 0;
        t->scroll = 0;
        return;
    }
    if (e->type != KEY_DOWN)
        return;
    if (e->code == KEY_PAGE_UP) {
        unsigned n = t->rows > 1 ? t->rows - 1 : 1;
        t->scroll = t->scroll + n < t->count ? t->scroll + n : t->count - 1;
        return;
    }
    if (e->code == KEY_PAGE_DOWN) {
        unsigned n = t->rows > 1 ? t->rows - 1 : 1;
        t->scroll = t->scroll > n ? t->scroll - n : 0;
        return;
    }
    t->scroll = 0;
    if (e->modifiers & MOD_CTRL) {
        if (e->code == KEY_L)
            terminal_clear(t);
        if (e->code == KEY_C) {
            t->input[0] = 0;
            t->length = t->caret = t->history_age = 0;
            terminal_write(t, "^C\n", 7);
        }
        return;
    }
    switch (e->code) {
    case KEY_ENTER:
        if (t->length) {
            copy_line(t->history[t->history_next], t->input);
            t->history_next = (t->history_next + 1) % TERM_HISTORY;
            if (t->history_count < TERM_HISTORY)
                t->history_count++;
        }
        terminal_write(t, "> ", 10);
        terminal_write(t, t->input, 7);
        terminal_write(t, "\n", 7);
        if (t->submit)
            t->submit(t, t->input, t->context);
        t->length = t->caret = t->history_age = 0;
        t->input[0] = 0;
        break;
    case KEY_BACKSPACE:
        if (t->caret) {
            for (unsigned i = t->caret - 1; i < t->length; i++)
                t->input[i] = t->input[i + 1];
            t->caret--;
            t->length--;
        }
        break;
    case KEY_DELETE:
        if (t->caret < t->length) {
            for (unsigned i = t->caret; i < t->length; i++)
                t->input[i] = t->input[i + 1];
            t->length--;
        }
        break;
    case KEY_LEFT:
        if (t->caret)
            t->caret--;
        break;
    case KEY_RIGHT:
        if (t->caret < t->length)
            t->caret++;
        break;
    case KEY_HOME:
        t->caret = 0;
        break;
    case KEY_END:
        t->caret = t->length;
        break;
    case KEY_UP_ARROW:
        history_select(t, true);
        break;
    case KEY_DOWN_ARROW:
        history_select(t, false);
        break;
    default:
        break;
    }
}
static uint32_t ink(uint8_t index, bool light) {
    static const uint32_t palette[16] = {0xff101b2b, 0xffcf6679, 0xff6ed1a7, 0xffe5c07b,
                                         0xff78a6ef, 0xffbc9cea, 0xff70d7df, 0xffdce8f2,
                                         0xff70839b, 0xffed8796, 0xff49ddaf, 0xfff5d77b,
                                         0xffff8095, 0xffd5b7ff, 0xffa5edf2, 0xffffffff};
    if (light && index == 7)
        return 0xff172637;
    if (light && index == 0)
        return 0xffedf3f8;
    return palette[index & 15];
}
void terminal_render(struct Terminal *t, struct Surface *s, struct Rect client, bool focused,
                     bool light, TerminalDamage damage, void *ctx) {
    terminal_resize(t, (unsigned)client.w / 8, (unsigned)client.h / 16);
    unsigned editor_rows = (t->length + 3 + t->columns - 1) / t->columns;
    if (editor_rows > t->rows)
        editor_rows = t->rows;
    unsigned output_rows = t->scroll ? t->rows : t->rows - editor_rows;
    unsigned end = t->count > t->scroll ? t->count - t->scroll : 0;
    unsigned start = end > output_rows ? end - output_rows : 0;
    unsigned top = output_rows > (end - start) ? output_rows - (end - start) : 0;
    struct Rect saved = s->clip;
    surface_clip(s, rect_intersect(saved, client));
    for (unsigned r = 0; r < t->rows; r++) {
        struct Cell desired[TERM_COLUMNS];
        blank_line(desired);
        if (r < output_rows && r >= top && start + r - top < end) {
            unsigned src = (t->first + start + r - top) % TERM_SCROLLBACK;
            for (unsigned x = 0; x < t->columns; x++)
                desired[x] = t->lines[src][x];
        } else if (!t->scroll && r >= output_rows) {
            for (unsigned x = 0; x < t->columns; x++) {
                unsigned offset = (r - output_rows) * t->columns + x;
                if (offset == 0)
                    desired[x] = (struct Cell){'>', 10, 0, 0};
                else if (offset == 1)
                    desired[x] = (struct Cell){' ', 7, 0, 0};
                else if (offset - 2 < t->length)
                    desired[x] = (struct Cell){(uint8_t)t->input[offset - 2], 7, 0, 0};
                if (focused && offset == t->caret + 2)
                    desired[x].flags = 1;
            }
        }
        int first = -1, last = -1;
        for (unsigned x = 0; x < t->columns; x++) {
            struct Cell old = t->painted[r][x], n = desired[x];
            if (t->invalid || old.character != n.character || old.foreground != n.foreground ||
                old.background != n.background || old.flags != n.flags) {
                if (first < 0)
                    first = (int)x;
                last = (int)x;
            }
        }
        if (first >= 0) {
            struct Rect region = {client.x + first * 8, client.y + (int)r * 16,
                                  (last - first + 1) * 8, 16};
            surface_rect(s, region, ink(0, light));
            for (int x = first; x <= last; x++) {
                struct Cell n = desired[x];
                font_draw(s, client.x + x * 8, client.y + (int)r * 16, n.character,
                          ink(n.foreground, light));
                if (n.flags)
                    surface_rect(s,
                                 (struct Rect){client.x + x * 8, client.y + (int)r * 16 + 14, 8, 2},
                                 ink(10, light));
                t->painted[r][x] = n;
            }
            if (damage)
                damage(region, ctx);
        }
    }
    t->invalid = false;
    s->clip = saved;
}
