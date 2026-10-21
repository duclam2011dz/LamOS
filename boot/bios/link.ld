ENTRY(load)
SECTIONS { . = 0xc000; .text : { *(.text.load) *(.text*) } .rodata : { *(.rodata*) } .data : { *(.data*) } .bss : { *(.bss*) *(COMMON) } /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) } }
