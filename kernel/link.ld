ENTRY(_start)
KBASE = 0xffffffff80000000;
PHDRS { text PT_LOAD FLAGS(5); rodata PT_LOAD FLAGS(4); data PT_LOAD FLAGS(6); }
SECTIONS {
 . = KBASE + 0x100000;
 lamos_kernel_start = .;
 .text ALIGN(4096) : AT(ADDR(.text)-KBASE) { *(.text.entry) *(.text*) } :text
 lamos_text_end = ALIGN(4096);
 .rodata ALIGN(4096) : AT(ADDR(.rodata)-KBASE) { *(.rodata*) } :rodata
 lamos_rodata_end = ALIGN(4096);
 .data ALIGN(4096) : AT(ADDR(.data)-KBASE) { *(.data*) } :data
 .bss ALIGN(4096) (NOLOAD) : AT(ADDR(.bss)-KBASE) { *(COMMON) *(.bss*) } :data
 lamos_kernel_end = ALIGN(4096);
 /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
