; SPDX-License-Identifier: GPL-3.0-only
bits 64
section .text
global enter_kernel
enter_kernel:
 cli
 mov rax,cr4
 or rax,1<<5
 mov cr4,rax
 mov r10,rcx
 mov r11,rdx
 mov ecx,0xc0000080
 rdmsr
 or eax,1<<11
 wrmsr
 mov cr3,r10
 mov rsp,r9
 and rsp,-16
 mov rdi,r8
 jmp r11
