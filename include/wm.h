/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMOS_WM_H
#define LAMOS_WM_H
#include "compositor.h"
#define WINDOW_LIMIT 16
#define TITLE_HEIGHT 28
#define TASKBAR_HEIGHT 32
typedef uint32_t WindowId;
enum WindowKind { WINDOW_TERMINAL, WINDOW_FOLDER, WINDOW_SETTINGS };
enum WmAction { WM_NONE, WM_CLIENT_DOWN, WM_CLIENT_UP, WM_CLOSE, WM_OOM };
struct WmResult {
    WindowId id;
    enum WmAction action;
};
struct Window {
    WindowId id;
    bool live, minimized, maximized, redraw, content_dirty;
    enum WindowKind kind;
    struct Rect rect, restore;
    struct Surface surface;
    char title[40];
    void *userdata;
};
enum CaptureKind { CAPTURE_NONE, CAPTURE_MOVE, CAPTURE_RESIZE, CAPTURE_CONTROL };
struct WindowManager {
    struct Window windows[WINDOW_LIMIT];
    uint32_t generations[WINDOW_LIMIT];
    unsigned z[WINDOW_LIMIT], count;
    WindowId focus, captured;
    enum CaptureKind capture;
    int control, edges, start_x, start_y, mouse_x, mouse_y;
    uint8_t buttons;
    struct Rect original, preview;
    bool preview_visible;
    WindowId hover;
    int hover_control;
    struct Compositor *compositor;
    struct GfxAllocator allocator;
};
void wm_init(struct WindowManager *, struct Compositor *, struct GfxAllocator);
struct Window *wm_get(struct WindowManager *, WindowId);
WindowId wm_create(struct WindowManager *, enum WindowKind, const char *, struct Rect);
bool wm_destroy(struct WindowManager *, WindowId);
bool wm_set_rect(struct WindowManager *, WindowId, struct Rect);
bool wm_focus(struct WindowManager *, WindowId);
void wm_cycle(struct WindowManager *);
bool wm_minimize(struct WindowManager *, WindowId);
bool wm_maximize(struct WindowManager *, WindowId);
WindowId wm_hit(struct WindowManager *, int, int);
struct WmResult wm_pointer(struct WindowManager *, int, int, uint8_t);
void wm_cancel(struct WindowManager *);
size_t wm_layers(struct WindowManager *, struct Layer *);
struct Rect wm_work_area(const struct WindowManager *);
struct Rect wm_client(const struct Window *);
struct Rect wm_button(const struct Window *, int);
#endif
