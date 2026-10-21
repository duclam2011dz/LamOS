; SPDX-License-Identifier: GPL-3.0-only
; Test-only ELF entry: verify a streamed PT_LOAD BSS tail before kernel entry.
bits 64
org BASE
 lea rsi,[rel bss]
 mov ecx,8192-(bss-$$)
.check:
 cmp byte [rsi],0
 jne .fail
 inc rsi
 loop .check
 lea rsi,[rel success]
 mov dx,0x3f8
.print:
 lodsb
 test al,al
 jz .jump
 out dx,al
 jmp .print
.jump:
 mov rax,ENTRY
 jmp rax
.fail:
 cli
 hlt
 jmp .fail
success: db 'BSS:OK',10,0
bss:
