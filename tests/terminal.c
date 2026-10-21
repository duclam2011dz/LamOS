/* SPDX-License-Identifier: GPL-3.0-only */
#include "shell.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static void *allocate(size_t n, void *ctx) {
    (void)ctx;
    return malloc(n);
}
static void release(void *p, void *ctx) {
    (void)ctx;
    free(p);
}
static void submit(struct Terminal *t, const char *line, void *ctx) {
    shell_execute(t, line, ctx);
}
static void key(struct Terminal *t, unsigned code) {
    struct Event e = {.type = KEY_DOWN, .code = (uint16_t)code};
    terminal_event(t, &e);
}
static void text(struct Terminal *t, const char *s) {
    while (*s) {
        struct Event e = {.type = TEXT_INPUT, .text = *s++};
        terminal_event(t, &e);
    }
}
static bool contains(const struct Terminal *t, const char *needle) {
    char line[TERM_COLUMNS + 1];
    for (unsigned row = 0; row < t->count; row++) {
        for (unsigned x = 0; x < TERM_COLUMNS; x++)
            line[x] = (char)t->lines[(t->first + row) % TERM_SCROLLBACK][x].character;
        line[TERM_COLUMNS] = 0;
        if (strstr(line, needle))
            return true;
    }
    return false;
}
int main(void) {
    struct ShellInfo info = {.version = "test",
                             .firmware = "BIOS",
                             .cpu = "x86_64",
                             .width = 1024,
                             .height = 768,
                             .ticks = 500};
    struct Terminal *t =
        terminal_create((struct GfxAllocator){allocate, release, 0}, submit, &info);
    assert(t);
    text(t, "helxp");
    key(t, KEY_LEFT);
    key(t, KEY_BACKSPACE);
    assert(!strcmp(t->input, "help"));
    key(t, KEY_HOME);
    key(t, KEY_DELETE);
    assert(!strcmp(t->input, "elp"));
    text(t, "h");
    key(t, KEY_ENTER);
    assert(t->history_count == 1 && !t->length && t->count > 1);
    text(t, "draft");
    key(t, KEY_UP_ARROW);
    assert(!strcmp(t->input, "help"));
    key(t, KEY_DOWN_ARROW);
    assert(!strcmp(t->input, "draft"));
    key(t, KEY_ENTER);
    for (unsigned i = 0; i < 40; i++) {
        text(t, "uptime");
        key(t, KEY_ENTER);
    }
    assert(t->history_count == 32);
    for (unsigned i = 0; i < 1100; i++)
        terminal_write(t, "line\n", 7);
    assert(t->count == 1024);
    key(t, KEY_PAGE_UP);
    assert(t->scroll);
    terminal_resize(t, 38, 9);
    unsigned old = t->count;
    terminal_resize(t, 120, 40);
    assert(t->count == old);
    terminal_clear(t);
    assert(t->count == 1 && t->history_count == 32);
    shell_execute(t, " bad ", &info);
    assert(t->count > 1);
    terminal_clear(t);
    for (unsigned i = 0; i < 300; i++)
        text(t, "x");
    assert(t->length == 256 && t->input[256] == 0);
    terminal_clear(t);
    const char *commands[] = {" \thelp  ", "sysinfo", "meminfo", "uptime", "unknown", "help extra"};
    const char *expected[] = {"Commands:",         "Firmware: BIOS",  "Heap largest block:",
                              "Uptime: 5 seconds", "Unknown command", "take no arguments"};
    for (unsigned i = 0; i < 6; i++) {
        terminal_clear(t);
        shell_execute(t, commands[i], &info);
        assert(contains(t, expected[i]));
    }
    shell_execute(t, "clear", &info);
    assert(t->count == 1 && t->history_count == 32);
    struct Terminal *other =
        terminal_create((struct GfxAllocator){allocate, release, 0}, submit, &info);
    assert(other && !other->history_count && !other->length);
    text(other, "independent");
    assert(other->length == 11 && t->length == 256);
    terminal_clear(other);
    char wide[121];
    memset(wide, 'W', 120);
    wide[120] = 0;
    terminal_resize(other, 120, 20);
    terminal_write(other, wide, 7);
    terminal_resize(other, 38, 9);
    terminal_resize(other, 120, 20);
    assert(other->lines[other->first][100].character == 'W');
    terminal_destroy(other);
    terminal_destroy(t);
    puts("TERMINAL:PASS editor, delete, history/draft, scrollback, resize, commands, bounds");
}
