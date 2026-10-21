/* SPDX-License-Identifier: GPL-3.0-only */
#include "kernel.h"
#include "input.h"
#include "gui.h"
struct BootInfo boot_info;
void kmain(const struct BootInfo *b) {
    serial_init();
    print("\nKERNEL:ENTRY\n");
    if (b->magic != BOOT_MAGIC || b->version != 1 || b->size != sizeof(*b) ||
        b->map_count > MAP_MAX)
        PANIC("Invalid BootInfo");
    if (!b->map_count || b->kernel_start != 0x100000 || b->kernel_end <= b->kernel_start ||
        b->kernel_end > 0x2000000 || b->loader_end < b->loader_start)
        PANIC("Invalid boot memory layout");
    const struct Framebuffer *f = &b->fb;
    if (!f->address || f->width < 640 || f->width > 1280 || f->height < 480 || f->height > 1024 ||
        (f->bpp != 24 && f->bpp != 32) || f->pitch < f->width * (f->bpp / 8) || f->pitch > 16384 ||
        f->address > UINT64_C(0x100000000) - (uint64_t)f->pitch * f->height || !f->red_bits ||
        !f->green_bits || !f->blue_bits || f->red_bits > 8 || f->green_bits > 8 ||
        f->blue_bits > 8 || f->red_shift + f->red_bits > f->bpp ||
        f->green_shift + f->green_bits > f->bpp || f->blue_shift + f->blue_bits > f->bpp)
        PANIC("Invalid framebuffer descriptor");
    memcpy(&boot_info, b, sizeof(*b));
    print("BOOT:OK firmware=%u map=%u fb=%ux%u\n", (uint64_t)b->firmware, (uint64_t)b->map_count,
          (uint64_t)b->fb.width, (uint64_t)b->fb.height);
    print("LOG:OK\n");
    arch_init();
    memory_init(&boot_info);
    fault_test();
    timer_init();
    __asm__ volatile("sti");
    while (timer_ticks() < 5)
        __asm__ volatile("hlt");
    print("TIMER:OK ticks=%u\n", timer_ticks());
    input_init();
    graphics_init(&boot_info.fb);
    mouse_init();
    struct Gui *gui = kernel_gui();
#if defined(FAULT_graphics)
    PANIC("Injected graphics panic");
#endif
    print("READY:LamOS\n");
    uint64_t next_frame = timer_ticks(), handled_drop = 0;
    unsigned remainder = 0;
    for (;;) {
        if (input_dropped() != handled_drop) {
            struct InputSnapshot state = input_recover();
            handled_drop = state.dropped;
            gui_recover(gui, state);
        }
        struct Event event;
        for (unsigned i = 0; i < 128 && input_next(&event); i++)
            gui_event(gui, &event);
        uint64_t now = timer_ticks();
        gui_update(gui, now);
        if (now >= next_frame) {
            graphics_present();
            remainder += 10;
            unsigned step = 3;
            if (remainder >= 30) {
                step = 4;
                remainder -= 30;
            }
            next_frame = timer_ticks() + step;
        }
        __asm__ volatile("sti; hlt" ::: "memory");
    }
}
