/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LAMOS_KERNEL_H
#define LAMOS_KERNEL_H
#include "boot.h"
#include <stdbool.h>
#include <stdarg.h>
static inline void outb(uint16_t p, uint8_t v) {
    __asm__ volatile("outb %0,%1" ::"a"(v), "Nd"(p));
}
static inline uint8_t inb(uint16_t p) {
    uint8_t v;
    __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p));
    return v;
}
static inline uint64_t irq_save(void) {
    uint64_t f;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(f)::"memory");
    return f;
}
static inline void irq_restore(uint64_t f) {
    if (f & (1 << 9))
        __asm__ volatile("sti" ::: "memory");
}
void *memcpy(void *, const void *, size_t);
void *memset(void *, int, size_t);
size_t strlen(const char *);
void serial_init(void);
void print(const char *, ...);
_Noreturn void panic_at(const char *, const char *, unsigned);
#define PANIC(s) panic_at(s, __FILE__, __LINE__)
void panic_screen(const char *);
void arch_init(void);
void timer_init(void);
uint64_t timer_ticks(void);
void input_irq(void);
void input_init(void);
void memory_init(const struct BootInfo *);
uint64_t frame_alloc(void);
void frame_free(uint64_t);
bool vm_map(uint64_t, uint64_t, uint64_t);
uint64_t vm_query(uint64_t);
void vm_unmap(uint64_t);
void *kmalloc(size_t);
void kfree(void *);
void graphics_init(const struct Framebuffer *);
void ui_draw(uint64_t, uint64_t, const char *, int, int, unsigned);
void graphics_present(void);
void fault_test(void);
extern uint64_t free_frames;
extern struct BootInfo boot_info;
#endif
