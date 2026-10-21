/* SPDX-License-Identifier: GPL-3.0-only */
#include "kernel.h"
struct __attribute__((packed)) Descriptor {
    uint16_t limit;
    uint64_t base;
};
struct __attribute__((packed)) Gate {
    uint16_t low, selector;
    uint8_t ist, attributes;
    uint16_t middle;
    uint32_t high, reserved;
};
struct __attribute__((packed)) Tss {
    uint32_t reserved0;
    uint64_t rsp[3], reserved1, ist[7], reserved2;
    uint16_t reserved3, iomap;
};
struct InterruptFrame {
    uint64_t regs[15], vector, error, rip, cs, flags, rsp, ss;
};
static struct Gate idt[256];
static uint64_t gdt[5];
static struct Tss tss;
static uint8_t df_stack[16384] __attribute__((aligned(16)));
extern void *isr_table[256];
extern void arch_load(const struct Descriptor *, const struct Descriptor *);
static const char *const names[32] = {"Divide error",
                                      "Debug",
                                      "NMI",
                                      "Breakpoint",
                                      "Overflow",
                                      "Bound range",
                                      "Invalid opcode",
                                      "Device unavailable",
                                      "Double fault",
                                      "Coprocessor overrun",
                                      "Invalid TSS",
                                      "Segment absent",
                                      "Stack fault",
                                      "General protection",
                                      "Page fault",
                                      "Reserved",
                                      "x87",
                                      "Alignment check",
                                      "Machine check",
                                      "SIMD",
                                      "Virtualization",
                                      "Control protection",
                                      "Reserved",
                                      "Reserved",
                                      "Reserved",
                                      "Reserved",
                                      "Reserved",
                                      "Reserved",
                                      "Hypervisor",
                                      "VMM communication",
                                      "Security",
                                      "Reserved"};
static volatile uint64_t ticks;
void arch_init(void) {
    gdt[1] = UINT64_C(0x00af9a000000ffff);
    gdt[2] = UINT64_C(0x00cf92000000ffff);
    tss.ist[0] = (uint64_t)(df_stack + sizeof(df_stack));
    tss.iomap = sizeof(tss);
    uint64_t a = (uint64_t)&tss;
    gdt[3] = (sizeof(tss) - 1) | ((a & 0xffffff) << 16) | (UINT64_C(0x89) << 40) |
             (((a >> 24) & 255) << 56);
    gdt[4] = a >> 32;
    for (unsigned i = 0; i < 256; i++) {
        uint64_t p = (uint64_t)isr_table[i];
        idt[i] = (struct Gate){(uint16_t)p,         8, i == 8 ? 1 : 0, 0x8e, (uint16_t)(p >> 16),
                               (uint32_t)(p >> 32), 0};
    }
    struct Descriptor gd = {sizeof(gdt) - 1, (uint64_t)gdt}, id = {sizeof(idt) - 1, (uint64_t)idt};
    arch_load(&gd, &id);
    print("ARCH:OK GDT IDT TSS IST\n");
}
void interrupt_dispatch(struct InterruptFrame *f) {
    if (f->vector < 32) {
        print("EXCEPTION:%u %s error=%x RIP=%x CS=%x RFLAGS=%x RSP=%x\n", f->vector,
              names[f->vector], f->error, f->rip, f->cs, f->flags, f->rsp);
        print("RAX=%x RBX=%x RCX=%x RDX=%x RBP=%x RSI=%x RDI=%x\n", f->regs[14], f->regs[13],
              f->regs[12], f->regs[11], f->regs[10], f->regs[9], f->regs[8]);
        if (f->vector == 14) {
            uint64_t cr2;
            __asm__ volatile("mov %%cr2,%0" : "=r"(cr2));
            print("PAGEFAULT CR2=%x present=%u write=%u user=%u reserved=%u execute=%u\n", cr2,
                  f->error & 1, (f->error >> 1) & 1, (f->error >> 2) & 1, (f->error >> 3) & 1,
                  (f->error >> 4) & 1);
        }
        PANIC(names[f->vector]);
    }
    unsigned irq = (unsigned)f->vector - 32;
    if (irq >= 16)
        return;
    if (irq == 7) {
        outb(0x20, 0x0b);
        if (!(inb(0x20) & 0x80))
            return;
    }
    if (irq == 15) {
        outb(0xa0, 0x0b);
        if (!(inb(0xa0) & 0x80)) {
            outb(0x20, 0x20);
            return;
        }
    }
    if (irq == 0)
        ticks++;
    if (irq == 1 || irq == 12)
        input_irq();
    if (irq >= 8)
        outb(0xa0, 0x20);
    outb(0x20, 0x20);
}
void timer_init(void) {
    outb(0x20, 0x11);
    outb(0xa0, 0x11);
    outb(0x21, 32);
    outb(0xa1, 40);
    outb(0x21, 4);
    outb(0xa1, 2);
    outb(0x21, 1);
    outb(0xa1, 1);
    outb(0x21, 0xfe);
    outb(0xa1, 0xff);
    uint16_t divisor = 1193182 / 100;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)divisor);
    outb(0x40, (uint8_t)(divisor >> 8));
    print("TIMER:ARMED hz=100\n");
}
uint64_t timer_ticks(void) {
    return ticks;
}
