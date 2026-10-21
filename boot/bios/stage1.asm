; SPDX-License-Identifier: GPL-3.0-only
bits 16
org 0x7c00
jmp short start
nop
times 8-($-$$) db 0
; xorriso boot-info-table: PVD LBA, boot image LBA, size, checksum
 times 64-($-$$) db 0
start:
 cli
 xor ax,ax
 mov ds,ax
 mov es,ax
 mov ss,ax
 mov sp,0x7c00
 sti
 mov [drive],dl
 mov bx,0x55aa
 mov ah,0x41
 int 0x13
 jc fail
 cmp bx,0xaa55
 jne fail
 test cx,1
 jz fail
 mov eax,[0x7c0c]
 inc eax
 mov [dap+8],eax
 mov si,dap
 mov dl,[drive]
 mov ah,0x42
 int 0x13
 jc fail
 mov dl,[drive]
 jmp 0:0x8000
fail:
 mov si,msg
.loop: lodsb
 test al,al
 jz .halt
 mov ah,0x0e
 int 0x10
 jmp .loop
.halt: cli
 hlt
 jmp .halt
drive: db 0
msg: db 'LamOS BIOS Stage 1 disk error',0
align 4
dap: db 16,0
 dw 16 ; Stage 2 occupies sixteen CD sectors (32 KiB)
 dw 0x8000,0
 dq 0
 times 510-($-$$) db 0
 dw 0xaa55
 times 2048-($-$$) db 0
