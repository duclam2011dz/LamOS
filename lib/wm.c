/* SPDX-License-Identifier: GPL-3.0-only */
#include "wm.h"
static int clamp64(int64_t n, int low, int high) {
    if (n < low)
        return low;
    if (n > high)
        return high;
    return (int)n;
}
struct Rect wm_work_area(const struct WindowManager *wm) {
    return (struct Rect){0, 0, wm->compositor->back.width,
                         wm->compositor->back.height - TASKBAR_HEIGHT};
}
struct Rect wm_client(const struct Window *w) {
    return (struct Rect){8, TITLE_HEIGHT + 8, w->rect.w - 16, w->rect.h - TITLE_HEIGHT - 16};
}
struct Rect wm_button(const struct Window *w, int button) {
    return (struct Rect){w->rect.w - 26 * (3 - button) - 2, 4, 24, 20};
}
static void damage(struct WindowManager *wm, struct Rect r) {
    r.w += 4;
    r.h += 4;
    compositor_damage(wm->compositor, r);
}
void wm_init(struct WindowManager *wm, struct Compositor *c, struct GfxAllocator a) {
    *wm =
        (struct WindowManager){.compositor = c, .allocator = a, .control = -1, .hover_control = -1};
}
struct Window *wm_get(struct WindowManager *wm, WindowId id) {
    if (!id)
        return 0;
    struct Window *w = &wm->windows[(id - 1) % WINDOW_LIMIT];
    return w->live && w->id == id ? w : 0;
}
static struct Rect constrain(struct WindowManager *wm, struct Rect r) {
    struct Rect area = wm_work_area(wm);
    r.w = clamp64(r.w, 320, area.w);
    r.h = clamp64(r.h, 200, area.h);
    r.x = clamp64(r.x, 0, area.w - r.w);
    r.y = clamp64(r.y, 0, area.h - r.h);
    return r;
}
bool wm_focus(struct WindowManager *wm, WindowId id) {
    struct Window *w = wm_get(wm, id);
    if (id && !w)
        return false;
    struct Window *old = wm_get(wm, wm->focus);
    if (old) {
        old->redraw = true;
        damage(wm, old->rect);
    }
    wm->focus = id;
    if (w) {
        w->minimized = false;
        w->redraw = true;
        unsigned index = (id - 1) % WINDOW_LIMIT;
        for (unsigned i = 0; i < wm->count; i++)
            if (wm->z[i] == index) {
                for (unsigned j = i; j + 1 < wm->count; j++)
                    wm->z[j] = wm->z[j + 1];
                wm->z[wm->count - 1] = index;
                break;
            }
        damage(wm, w->rect);
    }
    compositor_damage(wm->compositor,
                      (struct Rect){0, wm_work_area(wm).h, wm_work_area(wm).w, TASKBAR_HEIGHT});
    return true;
}
WindowId wm_create(struct WindowManager *wm, enum WindowKind kind, const char *title,
                   struct Rect r) {
    if (wm->count == WINDOW_LIMIT)
        return 0;
    unsigned slot = 0;
    while (slot < WINDOW_LIMIT && wm->windows[slot].live)
        slot++;
    if (slot == WINDOW_LIMIT)
        return 0;
    r = constrain(wm, r);
    struct Surface surface;
    if (!surface_create(&surface, r.w, r.h, PIXEL_XRGB8888, wm->allocator))
        return 0;
    uint32_t gen = ++wm->generations[slot];
    if (gen > UINT32_MAX / WINDOW_LIMIT - 1) {
        surface_destroy(&surface);
        return 0;
    }
    WindowId id = gen * WINDOW_LIMIT + slot + 1;
    struct Window *w = &wm->windows[slot];
    *w = (struct Window){.id = id,
                         .live = true,
                         .redraw = true,
                         .kind = kind,
                         .rect = r,
                         .restore = r,
                         .surface = surface};
    for (unsigned i = 0; title && title[i] && i < sizeof(w->title) - 1; i++)
        w->title[i] = title[i];
    wm->z[wm->count++] = slot;
    wm_focus(wm, id);
    return id;
}
void wm_cancel(struct WindowManager *wm) {
    if (wm->preview_visible) {
        struct Rect r = wm->preview;
        r.w++;
        r.h++;
        damage(wm, r);
    }
    struct Window *w = wm_get(wm, wm->captured);
    if (w) {
        w->redraw = true;
        damage(wm, w->rect);
    }
    wm->capture = CAPTURE_NONE;
    wm->captured = 0;
    wm->preview_visible = false;
    wm->control = -1;
}
static void focus_top(struct WindowManager *wm) {
    wm->focus = 0;
    for (unsigned i = wm->count; i > 0; i--) {
        struct Window *w = &wm->windows[wm->z[i - 1]];
        if (!w->minimized) {
            wm_focus(wm, w->id);
            return;
        }
    }
    wm_focus(wm, 0);
}
bool wm_destroy(struct WindowManager *wm, WindowId id) {
    struct Window *w = wm_get(wm, id);
    if (!w)
        return false;
    if (wm->captured == id)
        wm_cancel(wm);
    damage(wm, w->rect);
    surface_destroy(&w->surface);
    w->live = false;
    unsigned slot = (id - 1) % WINDOW_LIMIT;
    for (unsigned i = 0; i < wm->count; i++)
        if (wm->z[i] == slot) {
            for (unsigned j = i; j + 1 < wm->count; j++)
                wm->z[j] = wm->z[j + 1];
            wm->count--;
            break;
        }
    if (wm->focus == id)
        focus_top(wm);
    else
        compositor_damage(wm->compositor,
                          (struct Rect){0, wm_work_area(wm).h, wm_work_area(wm).w, TASKBAR_HEIGHT});
    return true;
}
bool wm_set_rect(struct WindowManager *wm, WindowId id, struct Rect r) {
    struct Window *w = wm_get(wm, id);
    if (!w)
        return false;
    r = constrain(wm, r);
    struct Surface next = {0};
    bool resize = r.w != w->rect.w || r.h != w->rect.h;
    if (resize && !surface_create(&next, r.w, r.h, PIXEL_XRGB8888, wm->allocator))
        return false;
    damage(wm, w->rect);
    if (resize) {
        surface_destroy(&w->surface);
        w->surface = next;
        w->redraw = true;
    }
    w->rect = r;
    damage(wm, r);
    return true;
}
bool wm_minimize(struct WindowManager *wm, WindowId id) {
    struct Window *w = wm_get(wm, id);
    if (!w)
        return false;
    if (wm->captured == id)
        wm_cancel(wm);
    w->minimized = true;
    damage(wm, w->rect);
    if (wm->focus == id)
        focus_top(wm);
    return true;
}
bool wm_maximize(struct WindowManager *wm, WindowId id) {
    struct Window *w = wm_get(wm, id);
    if (!w)
        return false;
    struct Rect target = w->maximized ? w->restore : wm_work_area(wm), old = w->rect;
    if (!wm_set_rect(wm, id, target))
        return false;
    if (!w->maximized)
        w->restore = old;
    w->maximized = !w->maximized;
    return true;
}
void wm_cycle(struct WindowManager *wm) {
    unsigned start = wm->focus ? (wm->focus - 1) % WINDOW_LIMIT : WINDOW_LIMIT - 1;
    for (unsigned i = 1; i <= WINDOW_LIMIT; i++) {
        struct Window *w = &wm->windows[(start + i) % WINDOW_LIMIT];
        if (w->live) {
            wm_focus(wm, w->id);
            return;
        }
    }
}
WindowId wm_hit(struct WindowManager *wm, int x, int y) {
    for (unsigned i = wm->count; i > 0; i--) {
        struct Window *w = &wm->windows[wm->z[i - 1]];
        if (!w->minimized && rect_contains(w->rect, x, y))
            return w->id;
    }
    return 0;
}
struct WmResult wm_pointer(struct WindowManager *wm, int x, int y, uint8_t buttons) {
    bool down = (buttons & 1) && !(wm->buttons & 1), up = !(buttons & 1) && (wm->buttons & 1);
    wm->buttons = buttons;
    wm->mouse_x = x;
    wm->mouse_y = y;
    struct WmResult result = {0, WM_NONE};
    WindowId hit = wm_hit(wm, x, y);
    struct Window *hover = wm_get(wm, hit);
    int button = -1;
    if (hover) {
        for (int i = 0; i < 3; i++)
            if (rect_contains(wm_button(hover, i), x - hover->rect.x, y - hover->rect.y))
                button = i;
    }
    if (wm->hover != hit || wm->hover_control != button) {
        struct Window *old = wm_get(wm, wm->hover);
        if (old) {
            old->redraw = true;
            damage(wm, old->rect);
        }
        if (hover) {
            hover->redraw = true;
            damage(wm, hover->rect);
        }
        wm->hover = hit;
        wm->hover_control = button;
    }
    struct Window *captured = wm_get(wm, wm->captured);
    if (captured && (wm->capture == CAPTURE_MOVE || wm->capture == CAPTURE_RESIZE)) {
        damage(wm, wm->preview);
        struct Rect r = wm->original, area = wm_work_area(wm);
        int64_t dx = (int64_t)x - wm->start_x, dy = (int64_t)y - wm->start_y;
        if (wm->capture == CAPTURE_MOVE) {
            r.x = clamp64((int64_t)r.x + dx, 0, area.w - r.w);
            r.y = clamp64((int64_t)r.y + dy, 0, area.h - r.h);
        } else {
            int right = r.x + r.w, bottom = r.y + r.h;
            if (wm->edges & 1)
                r.x = clamp64((int64_t)r.x + dx, 0, right - 320);
            if (wm->edges & 4)
                r.y = clamp64((int64_t)r.y + dy, 0, bottom - 200);
            r.w = (wm->edges & 1)   ? right - r.x
                  : (wm->edges & 2) ? clamp64((int64_t)r.w + dx, 320, area.w - r.x)
                                    : r.w;
            r.h = (wm->edges & 4)   ? bottom - r.y
                  : (wm->edges & 8) ? clamp64((int64_t)r.h + dy, 200, area.h - r.y)
                                    : r.h;
        }
        wm->preview = r;
        wm->preview_visible = true;
        damage(wm, r);
        if (up) {
            WindowId id = captured->id;
            result.id = id;
            if (!wm_set_rect(wm, id, r))
                result = (struct WmResult){id, WM_OOM};
            else
                captured->maximized = false;
            wm_cancel(wm);
        }
        return result;
    }
    if (captured && wm->capture == CAPTURE_CONTROL) {
        if (up) {
            WindowId id = captured->id;
            int action = wm->control;
            bool activate = hit == id && button == action;
            wm_cancel(wm);
            if (activate) {
                result.id = id;
                if (action == 2)
                    result = (struct WmResult){id, WM_CLOSE};
                else if (action == 0)
                    wm_minimize(wm, id);
                else if (!wm_maximize(wm, id))
                    result = (struct WmResult){id, WM_OOM};
            }
        }
        return result;
    }
    if (down) {
        if (!hover) {
            wm_focus(wm, 0);
            return result;
        }
        wm_focus(wm, hit);
        int lx = x - hover->rect.x, ly = y - hover->rect.y;
        wm->captured = hit;
        wm->original = hover->rect;
        wm->preview = hover->rect;
        wm->start_x = x;
        wm->start_y = y;
        wm->edges = (lx < 5 ? 1 : 0) | (lx >= hover->rect.w - 5 ? 2 : 0) | (ly < 4 ? 4 : 0) |
                    (ly >= hover->rect.h - 5 ? 8 : 0);
        if (button >= 0) {
            wm->capture = CAPTURE_CONTROL;
            wm->control = button;
            hover->redraw = true;
            damage(wm, hover->rect);
        } else if (wm->edges && !hover->maximized) {
            wm->capture = CAPTURE_RESIZE;
            wm->preview_visible = true;
            damage(wm, hover->rect);
        } else if (ly < TITLE_HEIGHT && !hover->maximized) {
            wm->capture = CAPTURE_MOVE;
            wm->preview_visible = true;
            damage(wm, hover->rect);
        } else {
            wm->captured = 0;
            result = (struct WmResult){hit, WM_CLIENT_DOWN};
        }
    } else if (up && hit)
        result = (struct WmResult){hit, WM_CLIENT_UP};
    return result;
}
size_t wm_layers(struct WindowManager *wm, struct Layer *layers) {
    size_t n = 0;
    for (unsigned i = 0; i < wm->count; i++) {
        struct Window *w = &wm->windows[wm->z[i]];
        if (!w->minimized)
            layers[n++] = (struct Layer){&w->surface, w->rect};
    }
    return n;
}
