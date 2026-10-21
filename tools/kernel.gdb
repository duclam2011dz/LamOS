set pagination off
set architecture i386:x86-64
file build/debug/kernel.elf
target remote :1234
hbreak kmain
continue
