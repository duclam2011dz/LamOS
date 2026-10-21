/* SPDX-License-Identifier: GPL-3.0-only */
#include "kernel.h"
#include "input.h"
static struct EventQueue queue;
static struct Keyboard keyboard;
static struct Mouse mouse;
static bool wait_write(void) {
    for (unsigned i = 0; i < 100000; i++)
        if (!(inb(0x64) & 2))
            return true;
    return false;
}
static bool command(uint8_t c) {
    if (!wait_write())
        return false;
    outb(0x64, c);
    return true;
}
static bool data(uint8_t c) {
    if (!wait_write())
        return false;
    outb(0x60, c);
    return true;
}
static bool response(bool aux, uint8_t *value) {
    for (unsigned i = 0; i < 1000000; i++) {
        uint8_t status = inb(0x64);
        if (status & 1) {
            uint8_t b = inb(0x60);
            if (((status & 32) != 0) == aux) {
                *value = b;
                return true;
            }
        }
    }
    return false;
}
static bool device(bool aux, uint8_t c) {
    for (unsigned i = 0; i < 3; i++) {
        if (aux && !command(0xd4))
            return false;
        if (!data(c))
            return false;
        uint8_t r;
        if (!response(aux, &r))
            return false;
        if (r == 0xfa)
            return true;
        if (r != 0xfe)
            return false;
    }
    return false;
}
static uint8_t config(void) {
    uint8_t v;
    if (!command(0x20) || !response(false, &v))
        PANIC("i8042 config timeout");
    return v;
}
static void set_config(uint8_t v) {
    if (!command(0x60) || !data(v))
        PANIC("i8042 write config timeout");
}
void input_init(void) {
    uint64_t flags = irq_save();
    uint8_t r;
    if (!command(0xad) || !command(0xa7))
        PANIC("i8042 disable timeout");
    for (unsigned i = 0; i < 64 && (inb(0x64) & 1); i++)
        (void)inb(0x60);
    uint8_t cfg = (uint8_t)((config() | 0x40) & ~3u);
    set_config(cfg);
    if (!command(0xaa) || !response(false, &r) || r != 0x55)
        PANIC("i8042 self-test failed");
    set_config(cfg);
    if (!command(0xab) || !response(false, &r) || r)
        PANIC("Keyboard port test failed");
    if (!command(0xae) || !device(false, 0xf5) || !device(false, 0xf0) || !device(false, 2) ||
        !device(false, 0xf4))
        PANIC("Keyboard initialization failed");
    set_config((uint8_t)((config() | 0x41) & ~0x10u));
    outb(0x21, (uint8_t)(inb(0x21) & ~2u));
    irq_restore(flags);
    print("KEYBOARD:OK US-QWERTY translated-set1\n");
}
void mouse_init(void) {
    uint64_t flags = irq_save();
    uint8_t r;
    if (!command(0xa9) || !response(false, &r) || r)
        PANIC("Mouse port test failed");
    if (!command(0xa8) || !device(true, 0xf6) || !device(true, 0xf4))
        PANIC("Mouse initialization failed");
    set_config((uint8_t)((config() | 3) & ~0x20u));
    outb(0xa1, (uint8_t)(inb(0xa1) & ~0x10u));
    outb(0x21, (uint8_t)(inb(0x21) & ~4u));
    irq_restore(flags);
    print("MOUSE:OK PS2 three-button\n");
}
void input_irq(void) {
    for (unsigned i = 0; i < 32; i++) {
        uint8_t status = inb(0x64);
        if (!(status & 1))
            return;
        uint8_t b = inb(0x60);
        if (status & 0xc0)
            continue;
        if (status & 32)
            mouse_byte(&mouse, &queue, b);
        else
            keyboard_byte(&keyboard, &queue, b);
    }
}
bool input_next(struct Event *e) {
    uint64_t f = irq_save();
    bool ok = event_pop(&queue, e);
    irq_restore(f);
    return ok;
}
uint64_t input_dropped(void) {
    return queue.dropped;
}

struct InputSnapshot input_recover(void) {
    uint64_t flags = irq_save();
    queue.tail = queue.head;
    struct InputSnapshot s = {(keyboard.shifts ? MOD_SHIFT : 0) | (keyboard.ctrl ? MOD_CTRL : 0) |
                                  (keyboard.alt ? MOD_ALT : 0) | (keyboard.caps ? MOD_CAPS : 0),
                              mouse.buttons, queue.dropped};
    irq_restore(flags);
    return s;
}
