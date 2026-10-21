/* SPDX-License-Identifier: GPL-3.0-only */
#include "gui.h"
static void copy_text(char *dst, size_t n, const char *src) {
    size_t i = 0;
    if (!n)
        return;
    for (; src && src[i] && i + 1 < n; i++)
        dst[i] = src[i];
    dst[i] = 0;
}
static void decimal(char *s, uint64_t n) {
    char r[24];
    unsigned i = 0, j = 0;
    do {
        r[i++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);
    while (i)
        s[j++] = r[--i];
    s[j] = 0;
}
static uint64_t cycles(struct Gui *g) {
    return g->hooks.cycles ? g->hooks.cycles(g->hooks.context) : 0;
}
static void log_event(struct Gui *g, const char *s, uint64_t a, uint64_t b, const char *text) {
    if (g->hooks.log)
        g->hooks.log(s, a, b, text ? text : "", g->hooks.context);
}
static uint32_t foreground(const struct Gui *g) {
    return g->light ? 0xff172b40 : 0xffe0ecf5;
}
static uint32_t background(const struct Gui *g) {
    return g->light ? 0xffe7eff6 : 0xff142338;
}
static void notify(struct Gui *g, const char *text) {
    copy_text(g->toast, sizeof(g->toast), text);
    g->toast_until = g->ticks + 300;
    compositor_damage(&g->compositor, (struct Rect){0, g->compositor.back.height - 80,
                                                    g->compositor.back.width, 48});
    log_event(g, "NOTICE", 0, 0, text);
}
static struct Rect icon_rect(int index) {
    return (struct Rect){20, 30 + index * 96, 80, 88};
}
static void wallpaper_draw(struct Gui *g) {
    struct Surface *s = &g->compositor.background;
    for (int y = 0; y < s->height; y++) {
        unsigned v = (unsigned)y * 20 / (unsigned)s->height;
        uint32_t c = g->light ? (0xffe2eaf2 - v * 0x010000) : (0xff0b1829 + v * 0x000102);
        surface_rect(s, (struct Rect){0, y, s->width, 1}, c);
    }
    if (g->wallpaper == 0) {
        struct Point p[] = {{s->width / 3, s->height},
                            {s->width * 3 / 4, s->height / 4},
                            {s->width, s->height / 2},
                            {s->width, s->height}};
        surface_polygon(s, p, 4, g->light ? 0xffc8dce9 : 0xff102b43, true);
    } else if (g->wallpaper == 1) {
        for (int x = 0; x < s->width; x += 48)
            surface_line(s, (struct Point){x, 0}, (struct Point){x, s->height},
                         g->light ? 0xffcfdfeb : 0xff18364a);
        for (int y = 0; y < s->height; y += 48)
            surface_line(s, (struct Point){0, y}, (struct Point){s->width, y},
                         g->light ? 0xffcfdfeb : 0xff18364a);
    } else {
        surface_circle(s, (struct Point){s->width - 200, s->height / 2}, 180,
                       g->light ? 0xffc7dce9 : 0xff16354c, true);
        surface_circle(s, (struct Point){s->width - 160, s->height / 2 - 40}, 100,
                       g->light ? 0xffd8e8ee : 0xff1a4357, true);
    }
    text_draw(s, (struct Rect){s->width - 280, 32, 250, 24}, "LamOS", foreground(g), TEXT_RIGHT, 0);
    text_draw(s, (struct Rect){s->width - 320, 56, 290, 20}, g->info.version, 0xff58ccbb,
              TEXT_RIGHT, 0);
    static const char *const titles[] = {"Terminal", "Folder", "Settings"};
    uint32_t colors[] = {0xff58debb, 0xffecc76f, 0xff83b8ed};
    for (int i = 0; i < 3; i++) {
        struct Rect r = icon_rect(i);
        icon_draw(s, (struct Rect){r.x + 20, r.y + 4, 40, 40}, (enum IconKind)i, colors[i]);
        text_draw(s, (struct Rect){r.x, r.y + 52, r.w, 20}, titles[i], foreground(g), TEXT_CENTER,
                  0);
    }
    compositor_full(&g->compositor);
}
void gui_theme(struct Gui *g, bool light, unsigned wallpaper) {
    g->light = light;
    g->wallpaper = wallpaper % 3;
    wallpaper_draw(g);
    for (unsigned i = 0; i < WINDOW_LIMIT; i++)
        if (g->wm.windows[i].live)
            g->wm.windows[i].redraw = true;
    log_event(g, "THEME", light, wallpaper, "");
}
static unsigned task_capacity(const struct Gui *g) {
    int n = (g->compositor.back.width - 252) / 104;
    return n > 0 ? (unsigned)n : 1;
}
static void reveal_focus(struct Gui *g) {
    unsigned at = 0;
    for (unsigned i = 0; i < WINDOW_LIMIT; i++)
        if (g->wm.windows[i].live) {
            if (g->wm.windows[i].id == g->wm.focus) {
                unsigned cap = task_capacity(g);
                if (at < g->task_offset)
                    g->task_offset = at;
                else if (at >= g->task_offset + cap)
                    g->task_offset = at - cap + 1;
                break;
            }
            at++;
        }
}
static void submit(struct Terminal *t, const char *line, void *context) {
    struct Gui *g = context;
    if (g->hooks.info)
        g->hooks.info(&g->info, g->hooks.context);
    log_event(g, "COMMAND", g->wm.focus, 0, line);
    shell_execute(t, line, &g->info);
}
bool gui_init(struct Gui *g, int w, int h, struct GfxAllocator allocator, struct GuiHooks hooks) {
    *g = (struct Gui){.allocator = allocator,
                      .hooks = hooks,
                      .mouse_x = 100,
                      .mouse_y = 100,
                      .selected_icon = -1,
                      .last_icon = -1};
    if (!compositor_init(&g->compositor, w, h, allocator))
        return false;
    wm_init(&g->wm, &g->compositor, allocator);
    g->info = (struct ShellInfo){.version = "development",
                                 .firmware = "unknown",
                                 .cpu = "x86_64",
                                 .width = (unsigned)w,
                                 .height = (unsigned)h};
    if (hooks.info)
        hooks.info(&g->info, hooks.context);
    wallpaper_draw(g);
    return true;
}
void gui_close(struct Gui *g, WindowId id) {
    struct Window *w = wm_get(&g->wm, id);
    if (!w)
        return;
    if (w->kind == WINDOW_TERMINAL)
        terminal_destroy(w->userdata);
    wm_destroy(&g->wm, id);
    if (g->task_offset && g->task_offset >= g->wm.count)
        g->task_offset = g->wm.count ? g->wm.count - 1 : 0;
    log_event(g, "CLOSE", id, g->wm.count, "");
    log_event(g, "FOCUS", g->wm.focus, 0, "");
}
void gui_destroy(struct Gui *g) {
    for (unsigned i = 0; i < WINDOW_LIMIT; i++)
        if (g->wm.windows[i].live)
            gui_close(g, g->wm.windows[i].id);
    compositor_destroy(&g->compositor);
}
WindowId gui_open(struct Gui *g, enum WindowKind kind) {
    if (kind != WINDOW_TERMINAL)
        for (unsigned i = 0; i < WINDOW_LIMIT; i++) {
            struct Window *w = &g->wm.windows[i];
            if (w->live && w->kind == kind) {
                wm_focus(&g->wm, w->id);
                reveal_focus(g);
                log_event(g, "FOCUS", w->id, 0, "");
                return w->id;
            }
        }
    if (g->wm.count == WINDOW_LIMIT) {
        notify(g, "Window limit reached (16).");
        return 0;
    }
    struct Terminal *terminal = 0;
    if (kind == WINDOW_TERMINAL) {
        terminal = terminal_create(g->allocator, submit, g);
        if (!terminal) {
            notify(g, "Not enough memory to open Terminal.");
            return 0;
        }
    }
    static const char *const titles[] = {"Terminal", "Folder - embedded resources", "Settings"};
    unsigned slot = g->wm.count;
    struct Rect rect = {120 + (int)(slot % 5) * 26, 52 + (int)(slot % 5) * 24,
                        kind == WINDOW_TERMINAL ? 656 : 520, kind == WINDOW_TERMINAL ? 444 : 380};
    WindowId id = wm_create(&g->wm, kind, titles[kind], rect);
    if (!id) {
        terminal_destroy(terminal);
        notify(g, "Not enough memory for a window surface.");
        return 0;
    }
    wm_get(&g->wm, id)->userdata = terminal;
    if (terminal) {
        terminal_write(terminal, "LamOS ", 10);
        terminal_write(terminal, g->info.version, 10);
        terminal_write(terminal, "\nType help for commands.\n", 7);
    }
    reveal_focus(g);
    log_event(g, "OPEN", id, (uint64_t)kind, "");
    log_event(g, "FOCUS", id, 0, "");
    return id;
}
struct PaintContext {
    struct Gui *gui;
    struct Window *window;
};
static void terminal_damage(struct Rect r, void *context) {
    struct PaintContext *p = context;
    r.x += p->window->rect.x;
    r.y += p->window->rect.y;
    compositor_damage(&p->gui->compositor, r);
}
static void wrapped(struct Surface *s, struct Rect box, const char *text, uint32_t c) {
    char line[161];
    unsigned col = 0;
    int y = box.y;
    unsigned width = (unsigned)box.w / 8;
    if (!width)
        return;
    if (width > 160)
        width = 160;
    for (unsigned i = 0;; i++) {
        char ch = text[i];
        if (!ch || ch == '\n' || col == width) {
            line[col] = 0;
            text_draw(s, (struct Rect){box.x, y, box.w, 16}, line, c, TEXT_LEFT, 0);
            y += 18;
            col = 0;
            if (!ch || y >= box.y + box.h)
                break;
            if (ch == '\n')
                continue;
        }
        line[col++] = ch;
    }
}
static void paint_folder(struct Gui *g, struct Window *w) {
    struct Rect r = wm_client(w);
    text_draw(&w->surface, (struct Rect){r.x, r.y, r.w, 20}, "Embedded resources (read-only)",
              foreground(g), TEXT_LEFT, 0);
    static const char *const files[] = {"About.txt", "Commands.txt", "README.txt"};
    for (int i = 0; i < 3; i++) {
        struct Rect item = {r.x, r.y + 30 + i * 28, 112, 24};
        surface_rect(&w->surface, item, g->folder_selection == i ? 0xff245168 : background(g));
        text_draw(&w->surface, (struct Rect){item.x + 4, item.y + 4, item.w - 4, 16}, files[i],
                  g->folder_selection == i ? 0xffffffff : foreground(g), TEXT_LEFT, 0);
    }
    const char *content =
        g->folder_selection == 0 ? "LamOS\nCore GUI Foundation\nSingle CPU, kernel GUI\nCustom "
                                   "BIOS and UEFI loaders\nGPL-3.0-only"
        : g->folder_selection == 1
            ? "help\nclear\nsysinfo\nmeminfo\nuptime\n\nCtrl+Alt+T: Terminal\nAlt+Tab: "
              "switch\nAlt+F4: close"
            : "This is an embedded resource viewer.\nIt does not read a disk filesystem.\nSettings "
              "and terminal sessions are stored in RAM only.";
    wrapped(&w->surface, (struct Rect){r.x + 124, r.y + 30, r.w - 124, r.h - 30}, content,
            foreground(g));
}
static void paint_settings(struct Gui *g, struct Window *w) {
    struct Rect r = wm_client(w);
    text_draw(&w->surface, (struct Rect){r.x, r.y, r.w, 20}, "Appearance (session only)",
              foreground(g), TEXT_LEFT, 0);
    static const char *const names[] = {"Dark", "Light"};
    for (int i = 0; i < 2; i++) {
        struct Rect b = {r.x + i * 108, r.y + 32, 100, 28};
        surface_rect(&w->surface, b, g->light == (i == 1) ? 0xff247b76 : 0xff30475c);
        text_draw(&w->surface, (struct Rect){b.x, b.y + 6, b.w, 16}, names[i], 0xffffffff,
                  TEXT_CENTER, 0);
    }
    text_draw(&w->surface, (struct Rect){r.x, r.y + 80, r.w, 20}, "Background", foreground(g),
              TEXT_LEFT, 0);
    static const char *const bg[] = {"Geometry", "Grid", "Orbits"};
    for (int i = 0; i < 3; i++) {
        struct Rect b = {r.x + i * 96, r.y + 108, 88, 30};
        surface_rect(&w->surface, b, g->wallpaper == (unsigned)i ? 0xff247b76 : 0xff30475c);
        text_draw(&w->surface, (struct Rect){b.x, b.y + 7, b.w, 16}, bg[i], 0xffffffff, TEXT_CENTER,
                  0);
    }
    surface_outline(&w->surface, (struct Rect){r.x, r.y + 160, 16, 16}, foreground(g));
    if (g->profiling)
        surface_rect(&w->surface, (struct Rect){r.x + 3, r.y + 163, 10, 10}, 0xff4ad8b0);
    text_draw(&w->surface, (struct Rect){r.x + 24, r.y + 160, r.w - 24, 20}, "Profiling overlay",
              foreground(g), TEXT_LEFT, 0);
    text_draw(&w->surface, (struct Rect){r.x, r.y + 200, r.w, 50},
              "Changes are not saved across reboot.\nNo filesystem or process isolation.",
              foreground(g), TEXT_LEFT, 2);
}
static void paint_window(struct Gui *g, struct Window *w) {
    if (w->redraw) {
        surface_clip(&w->surface, (struct Rect){0, 0, w->rect.w, w->rect.h});
        surface_rect(&w->surface, w->surface.clip, background(g));
        surface_outline(&w->surface, w->surface.clip,
                        g->wm.focus == w->id ? 0xff4bd4b7 : 0xff41546a);
        surface_rect(&w->surface, (struct Rect){1, 1, w->rect.w - 2, TITLE_HEIGHT - 1},
                     g->wm.focus == w->id ? 0xff245168 : 0xff273a50);
        icon_draw(&w->surface, (struct Rect){5, 5, 18, 18}, (enum IconKind)w->kind, 0xff7de3ca);
        text_draw(&w->surface, (struct Rect){30, 6, w->rect.w - 114, 16}, w->title, 0xffffffff,
                  TEXT_LEFT, 0);
        for (int i = 0; i < 3; i++) {
            struct Rect b = wm_button(w, i);
            bool hover = g->wm.hover == w->id && g->wm.hover_control == i;
            bool pressed = g->wm.captured == w->id && g->wm.control == i;
            surface_rect(&w->surface, b,
                         pressed ? 0xff47758a
                         : hover ? (i == 2 ? 0xffa64355 : 0xff3c6175)
                                 : 0xff293e53);
            if (i == 2)
                icon_draw(&w->surface, (struct Rect){b.x + 6, b.y + 4, 12, 12}, ICON_CLOSE,
                          0xfff0f5fa);
            else if (i == 0)
                surface_rect(&w->surface, (struct Rect){b.x + 6, b.y + 13, 12, 2}, 0xfff0f5fa);
            else
                surface_outline(&w->surface, (struct Rect){b.x + 6, b.y + 5, 12, 10}, 0xfff0f5fa);
        }
        if (w->kind == WINDOW_TERMINAL)
            ((struct Terminal *)w->userdata)->invalid = true;
        else if (w->kind == WINDOW_FOLDER)
            paint_folder(g, w);
        else
            paint_settings(g, w);
        compositor_damage(&g->compositor, w->rect);
    }
    if (w->kind == WINDOW_TERMINAL && (w->redraw || w->content_dirty)) {
        struct PaintContext context = {g, w};
        terminal_render(w->userdata, &w->surface, wm_client(w), g->wm.focus == w->id, g->light,
                        terminal_damage, &context);
    }
    w->redraw = false;
    w->content_dirty = false;
}
uint64_t gui_p95(const struct Gui *g) {
    unsigned n = g->profile.samples < 128 ? (unsigned)g->profile.samples : 128;
    uint64_t sorted[128];
    for (unsigned i = 0; i < n; i++) {
        uint64_t v = g->profile.rolling[i];
        unsigned j = i;
        while (j && sorted[j - 1] > v) {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = v;
    }
    return n ? sorted[(n * 95 + 99) / 100 - 1] : 0;
}
static void overlay(struct Surface *s, void *context) {
    struct Gui *g = context;
    int height = s->height;
    uint32_t fg = foreground(g);
    if (g->selected_icon >= 0)
        surface_outline(s, icon_rect(g->selected_icon), 0xff49d6b1);
    surface_rect(s, (struct Rect){0, height - 32, s->width, 32},
                 g->light ? 0xffcfdee9 : 0xff172d43);
    surface_rect(s, (struct Rect){0, height - 32, 72, 32}, g->launcher ? 0xff287e79 : 0xff21465c);
    text_draw(s, (struct Rect){0, height - 24, 72, 16}, "LamOS", 0xffffffff, TEXT_CENTER, 0);
    unsigned index = 0, shown = 0, cap = task_capacity(g);
    for (unsigned i = 0; i < WINDOW_LIMIT; i++) {
        struct Window *w = &g->wm.windows[i];
        if (!w->live)
            continue;
        if (index++ < g->task_offset)
            continue;
        if (shown == cap)
            break;
        struct Rect r = {76 + (int)shown * 104, height - 28, 100, 24};
        surface_rect(s, r, w->id == g->wm.focus ? 0xff29796f : 0xff2c455a);
        text_draw(s, (struct Rect){r.x + 4, r.y + 4, 92, 16},
                  w->kind == WINDOW_TERMINAL ? "Terminal"
                  : w->kind == WINDOW_FOLDER ? "Folder"
                                             : "Settings",
                  0xfff1f6fa, TEXT_LEFT, 0);
        if (w->minimized)
            surface_rect(s, (struct Rect){r.x + 4, r.y + 21, 16, 1}, 0xff84a6bd);
        shown++;
    }
    if (g->wm.count > cap) {
        text_draw(s, (struct Rect){s->width - 176, height - 24, 24, 16}, "<", fg, TEXT_CENTER, 0);
        text_draw(s, (struct Rect){s->width - 152, height - 24, 24, 16}, ">", fg, TEXT_CENTER, 0);
    }
    char number[24];
    decimal(number, g->ticks / 100);
    text_draw(s, (struct Rect){s->width - 122, height - 24, 116, 16}, number, fg, TEXT_RIGHT, 0);
    if (g->launcher) {
        struct Rect r = {8, height - 164, 204, 132};
        surface_rect(s, r, g->light ? 0xffdce8f0 : 0xff20394e);
        surface_outline(s, r, 0xff4bd4b7);
        static const char *const names[] = {"Terminal", "Folder", "Settings"};
        for (int i = 0; i < 3; i++) {
            struct Rect item = {r.x + 4, r.y + 6 + i * 40, r.w - 8, 38};
            if (g->launcher_selection == i)
                surface_rect(s, item, 0xff2d6b75);
            icon_draw(s, (struct Rect){item.x + 6, item.y + 7, 24, 24}, (enum IconKind)i,
                      0xff71d8bd);
            text_draw(s, (struct Rect){item.x + 40, item.y + 11, item.w - 44, 16}, names[i],
                      0xffffffff, TEXT_LEFT, 0);
        }
    }
    if (g->toast[0]) {
        struct Rect r = {8, height - 76, s->width - 16, 36};
        surface_rect(s, r, 0xff6f3546);
        text_draw(s, (struct Rect){r.x + 8, r.y + 10, r.w - 16, 16}, g->toast, 0xffffffff,
                  TEXT_LEFT, 0);
    }
    if (g->profiling) {
        struct Rect r = {s->width - 280, 90, 264, 90};
        surface_rect(s, r, 0xff142a3c);
        text_draw(s, (struct Rect){r.x + 8, r.y + 6, 248, 16}, "Frame mean/p95/max (us)",
                  0xff7de1c5, TEXT_LEFT, 0);
        const uint64_t values[] = {g->profile.samples ? g->profile.total / g->profile.samples : 0,
                                   gui_p95(g), g->profile.maximum};
        for (unsigned i = 0; i < 3; i++) {
            decimal(number, g->hooks.frequency ? values[i] * 1000000 / g->hooks.frequency : 0);
            text_draw(s, (struct Rect){r.x + 8 + (int)i * 80, r.y + 32, 80, 16}, number, 0xffffffff,
                      TEXT_LEFT, 0);
        }
        decimal(number, g->profile.samples);
        text_draw(s, (struct Rect){r.x + 8, r.y + 58, 248, 16}, number, 0xffb4cadd, TEXT_LEFT, 0);
    }
    if (g->wm.preview_visible) {
        surface_outline(s, g->wm.preview, 0xff74f0d3);
        struct Rect inner = {g->wm.preview.x + 1, g->wm.preview.y + 1, g->wm.preview.w - 2,
                             g->wm.preview.h - 2};
        surface_outline(s, inner, 0xff102b3d);
    }
    struct Point cursor[] = {{g->mouse_x, g->mouse_y},
                             {g->mouse_x, g->mouse_y + 17},
                             {g->mouse_x + 5, g->mouse_y + 12},
                             {g->mouse_x + 11, g->mouse_y + 12}};
    surface_polygon(s, cursor, 4, 0xffffffff, true);
    surface_polygon(s, cursor, 4, 0xff071625, false);
}
static void present(const struct Surface *s, struct Rect r, void *context) {
    struct Gui *g = context;
    uint64_t before = cycles(g);
    if (g->hooks.present)
        g->hooks.present(s, r, g->hooks.context);
    g->profile.present += cycles(g) - before;
}
struct ComposeStats gui_render(struct Gui *g) {
    uint64_t start = cycles(g);
    for (unsigned i = 0; i < WINDOW_LIMIT; i++) {
        struct Window *w = &g->wm.windows[i];
        if (w->live && !w->minimized && (w->redraw || w->content_dirty))
            paint_window(g, w);
    }
    uint64_t after_paint = cycles(g);
    if (!g->compositor.count)
        return (struct ComposeStats){0};
    struct Layer layers[WINDOW_LIMIT];
    size_t n = wm_layers(&g->wm, layers);
    uint64_t old_present = g->profile.present;
    struct ComposeStats stats = compositor_render(&g->compositor, layers, n, overlay, present, g);
    uint64_t end = cycles(g), duration = end - start,
             spent_present = g->profile.present - old_present;
    g->profile.render += after_paint - start;
    g->profile.compose += end - after_paint - spent_present;
    g->profile.samples++;
    g->profile.total += duration;
    if (duration > g->profile.maximum)
        g->profile.maximum = duration;
    g->profile.rolling[g->profile.next++ % 128] = duration;
    g->profile.pixels += stats.pixels;
    g->profile.last_rectangles = stats.rectangles;
    return stats;
}
static void client_click(struct Gui *g, struct Window *w, int x, int y) {
    struct Rect c = wm_client(w);
    x -= w->rect.x + c.x;
    y -= w->rect.y + c.y;
    if (w->kind == WINDOW_FOLDER) {
        if (x >= 0 && x < 112 && y >= 30 && y < 114) {
            g->folder_selection = (y - 30) / 28;
            w->redraw = true;
        }
    }
    if (w->kind == WINDOW_SETTINGS) {
        if (y >= 32 && y < 60 && x >= 0 && x < 208) {
            gui_theme(g, x >= 108, g->wallpaper);
        } else if (y >= 108 && y < 138 && x >= 0 && x < 280) {
            gui_theme(g, g->light, (unsigned)x / 96);
        } else if (y >= 156 && y < 182 && x >= 0) {
            g->profiling = !g->profiling;
            w->redraw = true;
            compositor_damage(&g->compositor,
                              (struct Rect){g->compositor.back.width - 280, 90, 264, 90});
            log_event(g, "PROFILING", g->profiling, 0, "");
        }
    }
}
static void pointer(struct Gui *g, const struct Event *e) {
    int width = g->compositor.back.width, height = g->compositor.back.height;
    compositor_damage(&g->compositor, (struct Rect){g->mouse_x, g->mouse_y, 13, 19});
    g->mouse_x += e->dx;
    g->mouse_y += e->dy;
    if (g->mouse_x < 0)
        g->mouse_x = 0;
    if (g->mouse_y < 0)
        g->mouse_y = 0;
    if (g->mouse_x >= width)
        g->mouse_x = width - 1;
    if (g->mouse_y >= height)
        g->mouse_y = height - 1;
    compositor_damage(&g->compositor, (struct Rect){g->mouse_x, g->mouse_y, 13, 19});
    log_event(g, "POINTER", (uint64_t)g->mouse_x, (uint64_t)g->mouse_y,
              e->buttons ? "held" : "released");
    bool down = (e->buttons & 1) && !(g->buttons & 1);
    g->buttons = e->buttons;
    int x = g->mouse_x, y = g->mouse_y;
    if (g->ignore_held_button) {
        g->wm.buttons = e->buttons;
        if (!(e->buttons & 1))
            g->ignore_held_button = false;
        return;
    }
    if (g->wm.capture != CAPTURE_NONE) {
        struct WmResult result = wm_pointer(&g->wm, x, y, e->buttons);
        if (result.action == WM_CLOSE)
            gui_close(g, result.id);
        if (result.action == WM_OOM)
            notify(g, "Resize failed: previous surface preserved.");
        if (!g->wm.preview_visible)
            log_event(g, "GEOMETRY", result.id, g->wm.count, "");
        return;
    }
    if (g->launcher && down) {
        struct Rect menu = {8, height - 164, 204, 132};
        if (rect_contains(menu, x, y)) {
            int item = (y - menu.y - 6) / 40;
            if (item >= 0 && item < 3)
                gui_open(g, (enum WindowKind)item);
            g->launcher = false;
            compositor_damage(&g->compositor, menu);
            g->wm.buttons = e->buttons;
            return;
        }
        g->launcher = false;
        compositor_damage(&g->compositor, menu);
    }
    if (y >= height - 32) {
        if (down) {
            if (x < 72) {
                g->launcher = !g->launcher;
                compositor_damage(&g->compositor, (struct Rect){0, height - 164, 216, 164});
            } else if (g->wm.count > task_capacity(g) && x >= width - 176 && x < width - 128) {
                if (x < width - 152) {
                    if (g->task_offset)
                        g->task_offset--;
                } else if (g->task_offset + task_capacity(g) < g->wm.count)
                    g->task_offset++;
                compositor_damage(&g->compositor, (struct Rect){0, height - 32, width, 32});
            } else if (x >= 76 && x < 76 + (int)task_capacity(g) * 104) {
                unsigned selected = g->task_offset + (unsigned)(x - 76) / 104, index = 0;
                for (unsigned i = 0; i < WINDOW_LIMIT; i++)
                    if (g->wm.windows[i].live) {
                        if (index++ == selected) {
                            struct Window *w = &g->wm.windows[i];
                            if (g->wm.focus == w->id && !w->minimized)
                                wm_minimize(&g->wm, w->id);
                            else
                                wm_focus(&g->wm, w->id);
                            log_event(g, "FOCUS", g->wm.focus, 0, "");
                            break;
                        }
                    }
            }
        }
        g->wm.buttons = e->buttons;
        return;
    }
    WindowId hit = wm_hit(&g->wm, x, y);
    if (!hit && down) {
        wm_focus(&g->wm, 0);
        int icon = -1;
        for (int i = 0; i < 3; i++)
            if (rect_contains(icon_rect(i), x, y))
                icon = i;
        g->selected_icon = icon;
        if (icon >= 0 && g->last_icon == icon && g->ticks - g->last_click <= 35) {
            gui_open(g, (enum WindowKind)icon);
            g->last_icon = -1;
        } else {
            g->last_icon = icon;
            g->last_click = g->ticks;
        }
        compositor_damage(&g->compositor, (struct Rect){16, 24, 92, 296});
        g->wm.buttons = e->buttons;
        return;
    }
    WindowId before = g->wm.focus;
    struct WmResult result = wm_pointer(&g->wm, x, y, e->buttons);
    if (result.action == WM_CLIENT_DOWN) {
        struct Window *w = wm_get(&g->wm, result.id);
        if (w)
            client_click(g, w, x, y);
    } else if (result.action == WM_CLOSE)
        gui_close(g, result.id);
    else if (result.action == WM_OOM)
        notify(g, "Resize failed: previous surface preserved.");
    if (before != g->wm.focus) {
        reveal_focus(g);
        log_event(g, "FOCUS", g->wm.focus, 0, "");
    }
}
void gui_event(struct Gui *g, const struct Event *e) {
    if (e->type >= MOUSE_MOVE) {
        pointer(g, e);
        return;
    }
    g->modifiers = e->modifiers;
    if (e->type == KEY_DOWN) {
        if (e->code == KEY_ESCAPE) {
            wm_cancel(&g->wm);
            if (g->launcher) {
                g->launcher = false;
                compositor_damage(&g->compositor,
                                  (struct Rect){0, g->compositor.back.height - 164, 216, 132});
            }
            return;
        }
        if ((e->modifiers & (MOD_CTRL | MOD_ALT)) == (MOD_CTRL | MOD_ALT) && e->code == KEY_T) {
            gui_open(g, WINDOW_TERMINAL);
            return;
        }
        if ((e->modifiers & MOD_ALT) && e->code == KEY_TAB) {
            wm_cycle(&g->wm);
            reveal_focus(g);
            log_event(g, "FOCUS", g->wm.focus, 0, "");
            return;
        }
        if ((e->modifiers & MOD_ALT) && e->code == KEY_F4) {
            gui_close(g, g->wm.focus);
            return;
        }
        if (g->launcher) {
            if (e->code == KEY_UP_ARROW)
                g->launcher_selection = (g->launcher_selection + 2) % 3;
            else if (e->code == KEY_DOWN_ARROW)
                g->launcher_selection = (g->launcher_selection + 1) % 3;
            else if (e->code == KEY_ENTER) {
                gui_open(g, (enum WindowKind)g->launcher_selection);
                g->launcher = false;
            }
            compositor_damage(&g->compositor,
                              (struct Rect){0, g->compositor.back.height - 164, 216, 132});
            return;
        }
        if (!g->wm.focus) {
            if (e->code == KEY_DOWN_ARROW)
                g->selected_icon = (g->selected_icon + 1) % 3;
            else if (e->code == KEY_UP_ARROW)
                g->selected_icon = (g->selected_icon + 2) % 3;
            else if (e->code == KEY_ENTER && g->selected_icon >= 0)
                gui_open(g, (enum WindowKind)g->selected_icon);
            compositor_damage(&g->compositor, (struct Rect){16, 24, 92, 296});
            return;
        }
    }
    struct Window *w = wm_get(&g->wm, g->wm.focus);
    if (w && w->kind == WINDOW_TERMINAL) {
        terminal_event(w->userdata, e);
        w->content_dirty = true;
    }
}
void gui_update(struct Gui *g, uint64_t ticks) {
    g->ticks = ticks;
    if (ticks / 100 != g->last_second) {
        g->last_second = ticks / 100;
        compositor_damage(&g->compositor, (struct Rect){g->compositor.back.width - 128,
                                                        g->compositor.back.height - 32, 128, 32});
        if (g->profiling)
            compositor_damage(&g->compositor,
                              (struct Rect){g->compositor.back.width - 280, 90, 264, 90});
    }
    if (g->toast[0] && ticks >= g->toast_until) {
        g->toast[0] = 0;
        compositor_damage(&g->compositor, (struct Rect){0, g->compositor.back.height - 80,
                                                        g->compositor.back.width, 48});
    }
}
void gui_recover(struct Gui *g, struct InputSnapshot state) {
    wm_cancel(&g->wm);
    g->buttons = state.buttons;
    g->wm.buttons = state.buttons;
    g->modifiers = state.modifiers;
    g->ignore_held_button = (state.buttons & 1) != 0;
    g->dropped = state.dropped;
    notify(g, "Input overload: queued events discarded; capture cancelled.");
    log_event(g, "OVERFLOW", state.dropped, 0, "");
}
void gui_panic(struct Gui *g, const char *message) {
    g->launcher = false;
    g->profiling = false;
    g->toast[0] = 0;
    wm_cancel(&g->wm);
    surface_rect(&g->compositor.front, g->compositor.front.clip, 0xff681d33);
    text_draw(&g->compositor.front, (struct Rect){28, 32, g->compositor.front.width - 56, 24},
              "KERNEL PANIC", 0xffffffff, TEXT_LEFT, 0);
    text_draw(&g->compositor.front, (struct Rect){28, 76, g->compositor.front.width - 56, 80},
              message, 0xffffffff, TEXT_LEFT, 2);
    if (g->hooks.present)
        g->hooks.present(&g->compositor.front,
                         (struct Rect){0, 0, g->compositor.front.width, g->compositor.front.height},
                         g->hooks.context);
}
