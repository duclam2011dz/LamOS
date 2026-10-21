; SPDX-License-Identifier: GPL-3.0-only
bits 64
section .text.entry
global _start
extern kmain
_start:
 cli
 cld
 ; BootInfo stays in RDI, replace loader stack with kernel-owned stack.
 lea rsp,[rel kernel_stack_top]
 and rsp,-16
 xor ebp,ebp
 mov rax,cr0
 and ax,0xfffb
 or ax,2
 mov cr0,rax
 mov rax,cr4
 or rax,(1<<9)|(1<<10)
 mov cr4,rax
 call kmain
.halt: cli
 hlt
 jmp .halt
section .bss
align 16
kernel_stack: resb 65536
kernel_stack_top:
section .note.GNU-stack noalloc noexec nowrite progbits
