/* SPDX-License-Identifier: GPL-3.0-only */
#include "kernel.h"
#include "graphics.h"
#include "gui.h"
#include "version.h"
static struct Framebuffer fb;
static struct Gui desktop;
static bool ready;
static char cpu_name[49];
static uint64_t frequency;
static void *allocate(size_t n, void *context) {
    (void)context;
    return kmalloc(n);
}
static void release(void *p, void *context) {
    (void)context;
    kfree(p);
}
static uint64_t timestamp(void *context) {
    (void)context;
    uint32_t a, d;
    __asm__ volatile("lfence; rdtsc" : "=a"(a), "=d"(d)::"memory");
    return ((uint64_t)d << 32) | a;
}
static void cpu_init(void) {
    /* CPUID outputs are read/written by inline assembly, outside cppcheck analysis. */
    // cppcheck-suppress variableScope
    uint32_t a = 0x80000000, b, c, d;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "=c"(c), "=d"(d));
    if (a >= 0x80000004) {
        for (unsigned i = 0; i < 3; i++) {
            a = 0x80000002 + i;
            __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "=c"(c), "=d"(d));
            uint32_t words[] = {a, b, c, d};
            memcpy(cpu_name + (size_t)i * 16, words, 16);
        }
        cpu_name[48] = 0;
    } else
        memcpy(cpu_name, "x86_64", 7);
}
static void info(struct ShellInfo *s, void *context) {
    (void)context;
    *s = (struct ShellInfo){LAMOS_VERSION,      boot_info.firmware ? "UEFI" : "BIOS",
                            cpu_name,           fb.width,
                            fb.height,          timer_ticks(),
                            managed_frames,     free_frames,
                            kernel_heap_stats()};
}
static void event_log(const char *name, uint64_t a, uint64_t b, const char *text, void *context) {
    (void)context;
    print("GUI:%s a=%u b=%u text=%s\n", name, a, b, text);
    if (name[0] == 'O' || name[0] == 'G' || name[0] == 'F') {
        struct Window *w = wm_get(&desktop.wm, (WindowId)a);
        if (w)
            print("GUI:WINDOW id=%u kind=%u x=%u y=%u w=%u h=%u minimized=%u focus=%u\n",
                  (uint64_t)w->id, (uint64_t)w->kind, (uint64_t)w->rect.x, (uint64_t)w->rect.y,
                  (uint64_t)w->rect.w, (uint64_t)w->rect.h, (uint64_t)w->minimized,
                  (uint64_t)desktop.wm.focus);
    }
    if (name[0] == 'C' && name[1] == 'L' && b == 0) {
        struct HeapStats heap = kernel_heap_stats();
        print("GUI:MEM live=%u used=%u largest=%u free_frames=%u\n", (uint64_t)heap.allocations,
              (uint64_t)heap.used, (uint64_t)heap.largest, free_frames);
    }
}
static void present(const struct Surface *s, struct Rect r, void *context) {
    (void)context;
    r = rect_intersect(r, (struct Rect){0, 0, (int)fb.width, (int)fb.height});
    bool normal = fb.red_bits == 8 && fb.green_bits == 8 && fb.blue_bits == 8 &&
                  fb.red_shift == 16 && fb.green_shift == 8 && fb.blue_shift == 0;
    bool swapped = fb.red_bits == 8 && fb.green_bits == 8 && fb.blue_bits == 8 &&
                   fb.red_shift == 0 && fb.green_shift == 8 && fb.blue_shift == 16;
    for (int y = r.y; y < r.y + r.h; y++) {
        const uint32_t *source = (const void *)(s->pixels + (size_t)y * s->stride);
        volatile uint8_t *destination =
            (void *)(DIRECT + fb.address + (uint64_t)y * fb.pitch + (uint64_t)r.x * (fb.bpp / 8));
        for (int x = r.x; x < r.x + r.w; x++) {
            uint32_t p = source[x] & 0xffffff;
            if (swapped)
                p = ((p & 255) << 16) | (p & 0xff00) | ((p >> 16) & 255);
            else if (!normal)
                p = framebuffer_color(p, fb.red_shift, fb.red_bits, fb.green_shift, fb.green_bits,
                                      fb.blue_shift, fb.blue_bits);
            if (fb.bpp == 32) {
                *(volatile uint32_t *)destination = p;
                destination += 4;
            } else {
                destination[0] = (uint8_t)p;
                destination[1] = (uint8_t)(p >> 8);
                destination[2] = (uint8_t)(p >> 16);
                destination += 3;
            }
        }
    }
}
void graphics_init(const struct Framebuffer *f) {
    fb = *f;
    cpu_init();
    uint64_t start = timestamp(0), ticks = timer_ticks();
    while (timer_ticks() < ticks + 10)
        __asm__ volatile("hlt");
    frequency = (timestamp(0) - start) * 10;
    if (!frequency)
        PANIC("TSC calibration failed");
    struct GuiHooks hooks = {info, event_log, present, timestamp, 0, frequency};
    if (!gui_init(&desktop, (int)fb.width, (int)fb.height,
                  (struct GfxAllocator){allocate, release, 0}, hooks))
        PANIC("Cannot allocate compositor buffers");
    ready = true;
    gui_render(&desktop);
    struct HeapStats heap = kernel_heap_stats();
    print("GRAPHICS:OK Surface compositor %ux%u bpp=%u\n", (uint64_t)fb.width, (uint64_t)fb.height,
          (uint64_t)fb.bpp);
    print("GUI:BASELINE live=%u used=%u largest=%u free_frames=%u\n", (uint64_t)heap.allocations,
          (uint64_t)heap.used, (uint64_t)heap.largest, free_frames);
    print("GUI:READY version=%s windows=0 tsc_hz=%u\n", LAMOS_VERSION, frequency);
}
struct Gui *kernel_gui(void) {
    return &desktop;
}
void graphics_present(void) {
    struct ComposeStats stats = gui_render(&desktop);
    if (stats.rectangles && desktop.profile.samples % 120 == 0) {
        struct GuiProfile *p = &desktop.profile;
        print("GUI:PROFILE frames=%u mean_us=%u p95_us=%u max_us=%u render_us=%u compose_us=%u "
              "present_us=%u pixels=%u dirty=%u\n",
              p->samples, p->total / p->samples * 1000000 / frequency,
              gui_p95(&desktop) * 1000000 / frequency, p->maximum * 1000000 / frequency,
              p->render * 1000000 / frequency, p->compose * 1000000 / frequency,
              p->present * 1000000 / frequency, p->pixels, (uint64_t)stats.rectangles);
    }
}
void panic_screen(const char *s) {
    if (ready)
        gui_panic(&desktop, s);
}
