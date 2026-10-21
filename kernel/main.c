/* SPDX-License-Identifier: GPL-3.0-only */
#include "kernel.h"
#include "input.h"
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
    char input[80] = {0};
    size_t length = 0;
    int mx = 100, my = 100;
    unsigned buttons = 0;
    uint64_t painted = UINT64_MAX;
    print("READY:LamOS\n");
    for (;;) {
        struct Event e;
        bool changed = false;
        while (input_next(&e)) {
            changed = true;
            if (e.type == TEXT_INPUT) {
                if (e.text == '\b') {
                    if (length)
                        input[--length] = 0;
                } else if (e.text == '\n') {
                    print("INPUT:SUBMIT %s\n", input);
                    length = 0;
                    input[0] = 0;
                } else if (e.text >= ' ' && length < sizeof(input) - 1) {
                    input[length++] = e.text;
                    input[length] = 0;
                }
                print("INPUT:TEXT code=%u buffer=%s\n", (uint64_t)(uint8_t)e.text, input);
            } else if (e.type == KEY_DOWN || e.type == KEY_UP)
                print("INPUT:KEY type=%u code=%u\n", (uint64_t)e.type, (uint64_t)e.code);
            else {
                mx += e.dx;
                my += e.dy;
                if (mx < 0)
                    mx = 0;
                if (my < 0)
                    my = 0;
                if (mx >= (int)boot_info.fb.width)
                    mx = (int)boot_info.fb.width - 1;
                if (my >= (int)boot_info.fb.height)
                    my = (int)boot_info.fb.height - 1;
                buttons = e.buttons;
                print("INPUT:MOUSE type=%u x=%u y=%u buttons=%u\n", (uint64_t)e.type, (uint64_t)mx,
                      (uint64_t)my, (uint64_t)buttons);
            }
        }
        uint64_t now = timer_ticks();
        if (changed || now / 10 != painted / 10) {
            ui_draw(now, free_frames, input, mx, my, buttons);
            graphics_present();
            painted = now;
        }
        uint64_t flags = irq_save();
        if (timer_ticks() == now)
            __asm__ volatile("sti; hlt" ::: "memory");
        else
            irq_restore(flags);
    }
}
