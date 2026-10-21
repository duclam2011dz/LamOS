/* SPDX-License-Identifier: GPL-3.0-only */
#include "kernel.h"
static volatile bool panicking;
void serial_init(void) {
    outb(0x3f9, 0);
    outb(0x3fb, 0x80);
    outb(0x3f8, 1);
    outb(0x3f9, 0);
    outb(0x3fb, 3);
    outb(0x3fa, 0xc7);
    outb(0x3fc, 0x0b);
}
static void put(char c) {
    for (unsigned i = 0; i < 1000000; i++)
        if (inb(0x3fd) & 32) {
            outb(0x3f8, (uint8_t)c);
            return;
        }
}
static void number(uint64_t n, unsigned base) {
    char b[24];
    unsigned i = 0;
    do {
        b[i++] = "0123456789abcdef"[n % base];
        n /= base;
    } while (n);
    while (i)
        put(b[--i]);
}
// Clang 14 cannot model this GCC freestanding varargs ABI with general registers only.
// NOLINTBEGIN(clang-analyzer-valist.Uninitialized)
void print(const char *fmt, ...) {
    uint64_t flags = irq_save();
    va_list ap;
    va_start(ap, fmt);
    while (*fmt) {
        if (*fmt++ != '%') {
            put(fmt[-1]);
            continue;
        }
        switch (*fmt++) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            while (s && *s)
                put(*s++);
            break;
        }
        case 'c':
            put((char)va_arg(ap, int));
            break;
        case 'u':
            number(va_arg(ap, uint64_t), 10);
            break;
        case 'x':
            number(va_arg(ap, uint64_t), 16);
            break;
        case '%':
            put('%');
            break;
        default:
            put('?');
            break;
        }
    }
    va_end(ap);
    irq_restore(flags);
}
// NOLINTEND(clang-analyzer-valist.Uninitialized)
__attribute__((no_sanitize("undefined"))) _Noreturn void panic_at(const char *s, const char *file,
                                                                  unsigned line) {
    __asm__ volatile("cli");
    if (panicking) {
        const char *m = "PANIC:NESTED\n";
        while (*m)
            put(*m++);
    } else {
        panicking = true;
        print("PANIC:%s at %s:%u\n", s, file, (uint64_t)line);
        panic_screen(s);
    }
    for (;;)
        __asm__ volatile("hlt");
}
