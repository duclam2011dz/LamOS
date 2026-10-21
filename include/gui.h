/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMOS_GUI_H
#define LAMOS_GUI_H
#include "wm.h"
#include "shell.h"
struct GuiHooks {
    void (*info)(struct ShellInfo *, void *);
    void (*log)(const char *, uint64_t, uint64_t, const char *, void *);
    PresentRect present;
    uint64_t (*cycles)(void *);
    void *context;
    uint64_t frequency;
};
struct GuiProfile {
    uint64_t samples, total, maximum, render, compose, present, pixels, rolling[128];
    unsigned next, last_rectangles;
};
struct Gui {
    struct Compositor compositor;
    struct WindowManager wm;
    struct GfxAllocator allocator;
    struct GuiHooks hooks;
    struct ShellInfo info;
    struct GuiProfile profile;
    bool light, launcher, profiling, ignore_held_button;
    unsigned wallpaper, task_offset;
    int mouse_x, mouse_y, selected_icon, last_icon, launcher_selection, folder_selection;
    uint8_t buttons, modifiers;
    uint64_t ticks, last_second, last_click, dropped, toast_until;
    char toast[96];
};
bool gui_init(struct Gui *, int, int, struct GfxAllocator, struct GuiHooks);
void gui_destroy(struct Gui *);
WindowId gui_open(struct Gui *, enum WindowKind);
void gui_close(struct Gui *, WindowId);
void gui_event(struct Gui *, const struct Event *);
void gui_update(struct Gui *, uint64_t);
struct ComposeStats gui_render(struct Gui *);
void gui_recover(struct Gui *, struct InputSnapshot);
void gui_theme(struct Gui *, bool, unsigned);
uint64_t gui_p95(const struct Gui *);
void gui_panic(struct Gui *, const char *);
#endif
