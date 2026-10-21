; SPDX-License-Identifier: GPL-3.0-only
bits 16
org 0x8000
start:
 cli
 xor ax,ax
 mov ds,ax
 mov es,ax
 mov ss,ax
 mov sp,0x7c00
 sti
 mov [drive],dl
 mov dx,0x3fb
 mov al,3
 out dx,al
 mov al,'A'
 call mark
 ; Clear BootInfo and set E820 extended attributes before each call.
 xor ax,ax
 mov di,0x5000
 mov cx,1600
 rep stosw
 xor ebx,ebx
 mov di,0x5080
.e820:
 mov dword [es:di+20],1
 mov eax,0xe820
 mov edx,0x534d4150
 mov ecx,24
 int 0x15
 jc .map_done
 cmp eax,0x534d4150
 jne fail
 inc dword [0x5014]
 add di,24
 cmp dword [0x5014],128
 jae fail
 test ebx,ebx
 jnz .e820
.map_done:
 cmp dword [0x5014],0
 je fail
 mov al,'B'
 call mark
 ; Verify CPUID availability and required long mode + NX.
 pushfd
 pop eax
 mov ecx,eax
 xor eax,1<<21
 push eax
 popfd
 pushfd
 pop eax
 xor eax,ecx
 test eax,1<<21
 jz fail
 mov eax,0x80000000
 cpuid
 cmp eax,0x80000001
 jb fail
 mov eax,0x80000001
 cpuid
 mov eax,edx
 and eax,(1<<29)|(1<<20)
 cmp eax,(1<<29)|(1<<20)
 jne fail
 mov al,'C'
 call mark
 ; VBE enumerate packed-pixel direct-color LFB modes.
 mov di,0x6000
 mov dword [di],0x32454256
 mov ax,0x4f00
 int 0x10
 cmp ax,0x004f
 jne fail
 mov si,[0x600e]
 mov ax,[0x6010]
 mov fs,ax
.next_mode:
 mov cx,[fs:si]
 add si,2
 cmp cx,0xffff
 je .video_done
 mov [mode],cx
 push si
 push fs
 mov di,0x6200
 mov ax,0x4f01
 int 0x10
 pop fs
 pop si
 cmp ax,0x004f
 jne .next_mode
 mov ax,[0x6200]
 and ax,0x91
 cmp ax,0x91
 jne .next_mode
 cmp byte [0x621b],6
 jne .next_mode
 mov al,[0x6219]
 cmp al,32
 je .bpp_ok
 cmp al,24
 jne .next_mode
.bpp_ok:
 cmp word [0x6212],640
 jb .next_mode
 cmp word [0x6214],480
 jb .next_mode
 ; Keep first usable mode, replace with preferred 1024x768x32.
 cmp word [chosen],0
 je .save
 cmp word [0x6212],1024
 jne .next_mode
 cmp word [0x6214],768
 jne .next_mode
 cmp byte [0x6219],32
 jne .next_mode
.save:
 mov ax,[mode]
 mov [chosen],ax
 mov eax,[0x6228]
 mov [0x5020],eax
 movzx eax,word [0x6212]
 mov [0x5028],eax
 movzx eax,word [0x6214]
 mov [0x502c],eax
 movzx eax,word [0x6210]
 mov [0x5030],eax
 movzx eax,byte [0x6219]
 mov [0x5034],eax
 mov al,[0x6220]
 mov [0x5038],al
 mov al,[0x621f]
 mov [0x5039],al
 mov al,[0x6222]
 mov [0x503a],al
 mov al,[0x6221]
 mov [0x503b],al
 mov al,[0x6224]
 mov [0x503c],al
 mov al,[0x6223]
 mov [0x503d],al
 jmp .next_mode
.video_done:
 mov al,'D'
 call mark
 mov bx,[chosen]
 test bx,bx
 jz fail
 or bx,0x4000
 mov ax,0x4f02
 int 0x10
 cmp ax,0x004f
 jne fail
 mov al,'E'
 call mark
 ; Primary volume descriptor -> root directory, read one directory sector.
 mov eax,16
 mov bx,0x1000
 call read_sector
 mov ax,0x1000
 mov fs,ax
 cmp dword [fs:1],0x30304443 ; CD00
 jne fail
 mov eax,[fs:158]
 mov bx,0x1000
 call read_sector
 xor si,si
.find:
 cmp si,2048
 jae fail
 mov al,[fs:si]
 test al,al
 jz fail
 cmp byte [fs:si+32],12
 jne .skip
 cmp dword [fs:si+33],0x4e52454b ; KERN
 jne .skip
 cmp dword [fs:si+37],0x452e4c45 ; EL.E
 jne .skip
 cmp dword [fs:si+41],0x313b464c ; LF;1
 je .found
.skip:
 movzx ax,byte [fs:si]
 add si,ax
 jmp .find
.found:
 mov al,'F'
 call mark
 mov eax,[fs:si+2]
 mov [file_lba],eax
 mov eax,[fs:si+10]
 mov [0x5018],eax
 cmp eax,0x50000
 ja fail
 add eax,2047
 shr eax,11
 mov [sectors],ax
 mov word [load_segment],0x1000
.load:
 mov eax,[file_lba]
 mov bx,[load_segment]
 call read_sector
 inc dword [file_lba]
 add word [load_segment],0x80
 dec word [sectors]
 jnz .load
 mov al,'G'
 call mark
 ; A20, flat protected mode, C ELF loader.
 in al,0x92
 and al,0xfe
 or al,2
 out 0x92,al
 cli
 lgdt [gdt_desc]
 mov eax,cr0
 or eax,1
 mov cr0,eax
 jmp 8:protected
mark:
 push dx
 push ax
 mov dx,0x3fd
.wait: in al,dx
 test al,0x20
 jz .wait
 pop ax
 mov dx,0x3f8
 out dx,al
 pop dx
 ret
read_sector:
 mov [dap+8],eax
 mov [dap+6],bx
 pushad
 push ds
 push es
 push fs
 mov si,dap
 mov dl,[drive]
 mov ah,0x42
 int 0x13
 jc fail
 pop fs
 pop es
 pop ds
 popad
 ret
fail:
 mov si,error_message
.error:
 lodsb
 test al,al
 jz .halt
 call mark
 jmp .error
.halt:
 cli
 hlt
 jmp .halt
error_message: db 'BIOS:ERROR',10,0
align 8
gdt: dq 0,0x00cf9a000000ffff,0x00cf92000000ffff,0x00af9a000000ffff
gdt_desc: dw $-gdt-1
 dd gdt
mode: dw 0
chosen: dw 0
drive: db 0
align 4
file_lba: dd 0
sectors: dw 0
load_segment: dw 0
dap: db 16,0
 dw 1,0,0
 dq 0
bits 32
protected:
 mov ax,16
 mov ds,ax
 mov es,ax
 mov ss,ax
 mov fs,ax
 mov gs,ax
 mov esp,0x6f000
 cld
 call 0xc000
 mov edi,eax ; entry low 32 bits saved in EDI; high constant
 mov eax,cr4
 or eax,1<<5
 mov cr4,eax
 mov eax,0x70000
 mov cr3,eax
 mov ecx,0xc0000080
 rdmsr
 or eax,(1<<8)|(1<<11)
 wrmsr
 mov eax,cr0
 or eax,0x80010000
 mov cr0,eax
 jmp 24:long_mode
bits 64
long_mode:
 mov ax,16
 mov ds,ax
 mov es,ax
 mov ss,ax
 mov rsp,0x6f000
 mov eax,edi
 mov rdx,0xffffffff00000000
 or rax,rdx
 mov edi,0x5000
 jmp rax
times 0x4000-($-$$) db 0
incbin 'loader32.bin'
times 32768-($-$$) db 0
