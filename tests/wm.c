/* SPDX-License-Identifier: GPL-3.0-only */
#include "wm.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
struct State {
    size_t live;
    bool fail;
};
static void *allocate(size_t n, void *ctx) {
    struct State *s = ctx;
    if (s->fail)
        return 0;
    void *p = malloc(n);
    if (p)
        s->live++;
    return p;
}
static void release(void *p, void *ctx) {
    struct State *s = ctx;
    s->live--;
    free(p);
}
int main(void) {
    struct State state = {0};
    struct GfxAllocator a = {allocate, release, &state};
    struct Compositor c;
    assert(compositor_init(&c, 640, 480, a));
    struct WindowManager wm;
    wm_init(&wm, &c, a);
    WindowId id = wm_create(&wm, WINDOW_TERMINAL, "Terminal", (struct Rect){10, 10, 400, 300});
    assert(id && wm.focus == id);
    wm_pointer(&wm, 50, 20, 1);
    wm_pointer(&wm, 100, 60, 1);
    assert(wm_get(&wm, id)->rect.x == 10 && wm.preview_visible);
    wm_pointer(&wm, 100, 60, 0);
    assert(wm_get(&wm, id)->rect.x == 60);
    struct Rect old = wm_get(&wm, id)->rect;
    state.fail = true;
    assert(!wm_set_rect(&wm, id, (struct Rect){0, 0, 500, 400}));
    assert(wm_get(&wm, id)->rect.w == old.w);
    state.fail = false;
    assert(wm_maximize(&wm, id) && wm_get(&wm, id)->rect.w == 640);
    assert(wm_maximize(&wm, id) && wm_get(&wm, id)->rect.w == old.w);
    assert(wm_minimize(&wm, id) && !wm.focus);
    assert(wm_focus(&wm, id) && !wm_get(&wm, id)->minimized);
    assert(wm_destroy(&wm, id) && !wm_get(&wm, id));
    WindowId ids[16];
    for (int i = 0; i < 16; i++) {
        ids[i] = wm_create(&wm, WINDOW_TERMINAL, "Test", (struct Rect){0, 0, 320, 200});
        assert(ids[i] && ids[i] != id);
    }
    assert(!wm_create(&wm, WINDOW_TERMINAL, "Full", (struct Rect){0, 0, 320, 200}));
    for (int i = 0; i < 16; i++)
        assert(wm_destroy(&wm, ids[i]));
    assert(state.live == 3);
    compositor_destroy(&c);
    assert(!state.live);
    puts("WM:PASS capture, outline commit, OOM rollback, maximize/minimize, stale handles, 16 "
         "windows, no leaks");
}
