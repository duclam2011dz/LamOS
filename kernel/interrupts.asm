; SPDX-License-Identifier: GPL-3.0-only
bits 64
section .text
extern interrupt_dispatch
global arch_load
arch_load:
 lgdt [rdi]
 push qword 8
 lea rax,[rel .reload]
 push rax
 retfq
.reload:
 mov ax,16
 mov ds,ax
 mov es,ax
 mov ss,ax
 xor eax,eax
 mov fs,ax
 mov gs,ax
 lidt [rsi]
 mov ax,24
 ltr ax
 ret
%assign v 0
%rep 256
isr%+v:
 %if v != 8 && v != 10 && v != 11 && v != 12 && v != 13 && v != 14 && v != 17 && v != 21 && v != 29 && v != 30
 push qword 0
 %endif
 push qword v
 jmp isr_common
%assign v v+1
%endrep
isr_common:
 cld
 push rax
 push rbx
 push rcx
 push rdx
 push rbp
 push rsi
 push rdi
 push r8
 push r9
 push r10
 push r11
 push r12
 push r13
 push r14
 push r15
 mov rdi,rsp
 mov r12,rsp
 and rsp,-16
 call interrupt_dispatch
 mov rsp,r12
 pop r15
 pop r14
 pop r13
 pop r12
 pop r11
 pop r10
 pop r9
 pop r8
 pop rdi
 pop rsi
 pop rbp
 pop rdx
 pop rcx
 pop rbx
 pop rax
 add rsp,16
 iretq
section .rodata
align 8
global isr_table
isr_table:
%assign v 0
%rep 256
 dq isr%+v
%assign v v+1
%endrep
section .note.GNU-stack noalloc noexec nowrite progbits
