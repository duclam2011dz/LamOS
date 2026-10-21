/* SPDX-License-Identifier: GPL-3.0-only */
#include "kernel.h"
void fault_test(void) {
#if defined(FAULT_divide)
    __asm__ volatile("xor %%edx,%%edx; mov $1,%%eax; xor %%ecx,%%ecx; div %%ecx" ::
                         : "rax", "rcx", "rdx");
#elif defined(FAULT_opcode)
    __asm__ volatile("ud2");
#elif defined(FAULT_page)
    (void)*(volatile uint64_t *)UINT64_C(0x40000000);
#elif defined(FAULT_double)
    __asm__ volatile("xor %%rsp,%%rsp; mov (%%rsp),%%rax" ::: "memory", "rax");
#elif defined(FAULT_write)
    *(volatile uint8_t *)UINT64_C(0xffffffff80100000) = 0;
#elif defined(FAULT_nx)
    void *p = kmalloc(16);
    __asm__ volatile("jmp *%0" ::"r"(p) : "memory");
#elif defined(FAULT_panic)
    PANIC("Injected panic");
#elif defined(FAULT_ubsan)
    volatile int a = 2147483647;
    /* Deliberate undefined behavior: this image validates the UBSan panic path. */
    // cppcheck-suppress integerOverflow
    volatile int b = a + 1;
    (void)b;
#endif
}
