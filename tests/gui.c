/* SPDX-License-Identifier: GPL-3.0-only */
#include "gui.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct Allocation {
    size_t size;
    max_align_t alignment;
};
struct State {
    size_t live, bytes, peak;
    bool fail;
};
static void *allocate(size_t n, void *context) {
    struct State *s = context;
    if (s->fail)
        return 0;
    struct Allocation *p = malloc(sizeof(*p) + n);
    if (!p)
        return 0;
    p->size = n;
    s->live++;
    s->bytes += n;
    if (s->bytes > s->peak)
        s->peak = s->bytes;
    return p + 1;
}
static void release(void *ptr, void *context) {
    struct State *s = context;
    struct Allocation *p = (struct Allocation *)ptr - 1;
    s->bytes -= p->size;
    s->live--;
    free(p);
}
static void info(struct ShellInfo *s, void *context) {
    (void)context;
    s->version = "1.0.3-test";
    s->cpu = "host";
    s->firmware = "host";
}
static uint32_t random_state = 0x1a032026;
static uint32_t random_number(void) {
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}
static void close_all(struct Gui *g) {
    for (unsigned i = 0; i < WINDOW_LIMIT; i++)
        if (g->wm.windows[i].live)
            gui_close(g, g->wm.windows[i].id);
}
int main(void) {
    struct State state = {0};
    struct GfxAllocator a = {allocate, release, &state};
    struct Gui *g = malloc(sizeof(*g));
    assert(g);
    int widths[] = {640, 1024, 1280}, heights[] = {480, 768, 1024};
    for (unsigned resolution = 0; resolution < 3; resolution++) {
        assert(gui_init(g, widths[resolution], heights[resolution], a,
                        (struct GuiHooks){.info = info}));
        assert(!g->wm.count);
        size_t base_live = state.live, base_bytes = state.bytes;
        WindowId first = gui_open(g, WINDOW_TERMINAL);
        assert(first);
        WindowId folder = gui_open(g, WINDOW_FOLDER);
        assert(gui_open(g, WINDOW_FOLDER) == folder && g->wm.count == 2);
        WindowId settings = gui_open(g, WINDOW_SETTINGS);
        assert(gui_open(g, WINDOW_SETTINGS) == settings);
        gui_render(g);
        gui_theme(g, true, 2);
        gui_render(g);
        gui_theme(g, false, 0);
        gui_close(g, folder);
        gui_close(g, settings);
        struct Window *w = wm_get(&g->wm, first);
        wm_focus(&g->wm, first);
        struct Event e = {.type = TEXT_INPUT, .text = 'a'};
        gui_event(g, &e);
        assert(((struct Terminal *)w->userdata)->length == 1);
        gui_render(g);
        uint8_t *image = malloc(g->compositor.front.bytes);
        memcpy(image, g->compositor.front.pixels, g->compositor.front.bytes);
        compositor_full(&g->compositor);
        gui_render(g);
        assert(!memcmp(image, g->compositor.front.pixels, g->compositor.front.bytes));
        free(image);
        close_all(g);
        assert(state.live == base_live && state.bytes == base_bytes);
        gui_destroy(g);
        assert(!state.live && !state.bytes);
    }
    assert(gui_init(g, 640, 480, a, (struct GuiHooks){.info = info}));
    size_t baseline_live = state.live, baseline_bytes = state.bytes;
    for (unsigned step = 0; step < 10000; step++) {
        unsigned slot = random_number() % 16;
        struct Window *w = &g->wm.windows[slot];
        WindowId id = w->live ? w->id : 0;
        switch (random_number() % 10) {
        case 0:
        case 1:
            gui_open(g, (enum WindowKind)(random_number() % 3));
            break;
        case 2:
            if (id)
                gui_close(g, id);
            break;
        case 3:
            if (id)
                wm_focus(&g->wm, id);
            break;
        case 4:
            if (id)
                wm_minimize(&g->wm, id);
            break;
        case 5:
            if (id) {
                state.fail = (step % 7) == 0;
                struct Rect old = w->rect;
                bool ok = wm_set_rect(&g->wm, id,
                                      (struct Rect){(int)(random_number() % 80),
                                                    (int)(random_number() % 40),
                                                    320 + (int)(random_number() % 321),
                                                    200 + (int)(random_number() % 249)});
                if (!ok)
                    assert(w->rect.w == old.w && w->rect.h == old.h);
                state.fail = false;
            }
            break;
        case 6:
            if (id)
                wm_maximize(&g->wm, id);
            break;
        case 7:
            if (id && !w->minimized) {
                wm_focus(&g->wm, id);
                wm_pointer(&g->wm, w->rect.x + 40, w->rect.y + 14, 1);
                wm_pointer(&g->wm, w->rect.x + 60, w->rect.y + 30, 1);
                if (step & 1)
                    wm_cancel(&g->wm);
                wm_pointer(&g->wm, g->wm.mouse_x, g->wm.mouse_y, 0);
            }
            break;
        case 8:
            gui_recover(g, (struct InputSnapshot){.buttons = 1, .dropped = step + 1});
            gui_event(g, &(struct Event){.type = BUTTON_UP, .buttons = 0});
            assert(g->wm.capture == CAPTURE_NONE);
            break;
        default:
            wm_cycle(&g->wm);
            break;
        }
        gui_update(g, (uint64_t)step * 4);
        if (step % 256 == 0)
            gui_render(g);
    }
    close_all(g);
    assert(state.live == baseline_live && state.bytes == baseline_bytes);
    gui_destroy(g);
    assert(!state.live && !state.bytes);
    free(g);
    printf("GUI:PASS 10000 deterministic operations, 3 resolutions, dirty/full equality, recovery "
           "and no leaks; peak=%zu\n",
           state.peak);
}
