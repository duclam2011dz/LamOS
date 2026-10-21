/* SPDX-License-Identifier: GPL-3.0-only */
#include "kernel.h"
#include "graphics.h"
static struct Framebuffer fb;
static struct Canvas canvas;
static bool ready;
static int cursor_x = 100, cursor_y = 100, old_x = 100, old_y = 100;
struct Dirty {
    int x, y, w, h;
};
static struct Dirty dirty[16];
static unsigned dirty_count;
static void damage(int x, int y, int w, int h) {
    if (dirty_count < 16)
        dirty[dirty_count++] = (struct Dirty){x, y, w, h};
    else {
        dirty_count = 1;
        dirty[0] = (struct Dirty){0, 0, canvas.width, canvas.height};
    }
}
static void pixel_hw(int x, int y, uint32_t rgb) {
    if (x < 0 || y < 0 || x >= canvas.width || y >= canvas.height)
        return;
    uint32_t p = framebuffer_color(rgb, fb.red_shift, fb.red_bits, fb.green_shift, fb.green_bits,
                                   fb.blue_shift, fb.blue_bits);
    volatile uint8_t *d =
        (void *)(DIRECT + fb.address + (uint64_t)y * fb.pitch + (uint64_t)x * (fb.bpp / 8));
    for (unsigned i = 0; i < fb.bpp / 8; i++)
        d[i] = (uint8_t)(p >> (i * 8));
}
static void text(int x, int y, const char *s, uint32_t color, int scale) {
    canvas_text(&canvas, x, y, s, color, scale);
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
void graphics_init(const struct Framebuffer *f) {
    fb = *f;
    if (!fb.address || fb.width < 640 || fb.height < 480 || fb.width > 1280 || fb.height > 1024 ||
        (fb.bpp != 24 && fb.bpp != 32) || fb.pitch < fb.width * (fb.bpp / 8) ||
        fb.address + (uint64_t)fb.pitch * fb.height > UINT64_C(0x100000000) || fb.red_bits > 8 ||
        fb.green_bits > 8 || fb.blue_bits > 8)
        PANIC("Unsupported framebuffer");
    canvas =
        (struct Canvas){kmalloc((size_t)fb.width * fb.height * 4), (int)fb.width, (int)fb.height};
    if (!canvas.pixels)
        PANIC("No memory for backbuffer");
    canvas_rect(&canvas, 0, 0, canvas.width, canvas.height, 0x101a2d);
    canvas_rect(&canvas, 0, 0, canvas.width, 64, 0x18334d);
    canvas_rect(&canvas, 24, 28, 6, 16, 0x42dfc0);
    text(44, 24, "LamOS", 0xffffff, 3);
    text(164, 34, "1.0.0 / GRAPHICS FOUNDATION", 0xa8c5d6, 1);
    text(28, 92, "SYSTEM ONLINE", 0x42dfc0, 2);
    text(28, 124, boot_info.firmware ? "BOOT: UEFI / ELF64" : "BOOT: BIOS / ELF64", 0xb8cadd, 1);
    text(28, 170, "UPTIME / SECONDS", 0x91a9c0, 1);
    text(300, 170, "FREE MEMORY / KiB", 0x91a9c0, 1);
    canvas_rect(&canvas, 24, 230, canvas.width - 48, 120, 0x1b2b43);
    text(40, 248, "KEYBOARD INPUT", 0x42dfc0, 1);
    text(40, 274, "Type here. Shift, Enter and Backspace work.", 0x91a9c0, 1);
    text(28, 380, "MOUSE / THREE BUTTON PS2", 0x42dfc0, 1);
    text(28, canvas.height - 32, "x86_64 | SINGLE CPU | PIC + PIT | NO USER APPLICATIONS", 0x91a9c0,
         1);
    canvas_line(&canvas, 24, canvas.height - 50, canvas.width - 24, canvas.height - 50, 0x30455f);
    ready = true;
    damage(0, 0, canvas.width, canvas.height);
    print("GRAPHICS:OK backbuffer clipped-primitives pitch=%u bpp=%u\n", (uint64_t)fb.pitch,
          (uint64_t)fb.bpp);
}
void ui_draw(uint64_t ticks, uint64_t frames, const char *input, int mx, int my, unsigned buttons) {
    char num[24];
    canvas_rect(&canvas, 28, 190, 200, 22, 0x101a2d);
    decimal(num, ticks / 100);
    text(28, 190, num, 0xffffff, 2);
    damage(28, 190, 200, 22);
    canvas_rect(&canvas, 300, 190, 250, 22, 0x101a2d);
    decimal(num, frames * 4);
    text(300, 190, num, 0xffffff, 2);
    damage(300, 190, 250, 22);
    canvas_rect(&canvas, 40, 302, canvas.width - 80, 28, 0x1b2b43);
    size_t visible = (size_t)(canvas.width - 80) / 12;
    size_t length = strlen(input);
    text(40, 306, input + (length > visible ? length - visible : 0), 0xffffff, 2);
    damage(40, 302, canvas.width - 80, 28);
    canvas_rect(&canvas, 28, 402, canvas.width - 56, 24, 0x101a2d);
    text(28, 406, "X", 0x91a9c0, 1);
    decimal(num, (uint64_t)mx);
    text(46, 406, num, 0xffffff, 1);
    text(120, 406, "Y", 0x91a9c0, 1);
    decimal(num, (uint64_t)my);
    text(138, 406, num, 0xffffff, 1);
    text(220, 406, buttons ? "BUTTON PRESSED" : "BUTTONS RELEASED", buttons ? 0x42dfc0 : 0x91a9c0,
         1);
    damage(28, 402, canvas.width - 56, 24);
    old_x = cursor_x;
    old_y = cursor_y;
    cursor_x = mx;
    cursor_y = my;
    damage(old_x, old_y, 12, 18);
    damage(mx, my, 12, 18);
}
void graphics_present(void) {
    if (!ready)
        return;
    for (unsigned i = 0; i < dirty_count; i++) {
        struct Dirty d = dirty[i];
        for (int y = d.y; y < d.y + d.h && y < canvas.height; y++)
            for (int x = d.x; x < d.x + d.w && x < canvas.width; x++)
                if (x >= 0 && y >= 0)
                    pixel_hw(x, y, canvas.pixels[(size_t)y * (size_t)canvas.width + (size_t)x]);
    }
    dirty_count = 0;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x <= y / 2; x++)
            pixel_hw(cursor_x + x, cursor_y + y,
                     x == 0 || x == y / 2 || y == 15 ? 0x07111f : 0xffffff);
}
void panic_screen(const char *message) {
    if (!ready)
        return;
    canvas_rect(&canvas, 0, 0, canvas.width, canvas.height, 0x681d33);
    text(28, 32, "KERNEL PANIC", 0xffffff, 3);
    text(28, 84, message, 0xffffff, 1);
    text(28, 112, "See serial output for diagnostic details.", 0xffffff, 1);
    damage(0, 0, canvas.width, canvas.height);
    graphics_present();
}
