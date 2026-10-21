cmake_minimum_required(VERSION 3.22)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/x86_64-elf.cmake" CACHE FILEPATH "Cross toolchain")
project(LamOS VERSION 1.0.0 LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(FAULT "none" CACHE STRING "Isolated fault-injection scenario")
set_property(CACHE FAULT PROPERTY STRINGS none panic divide opcode page double ubsan write nx)
option(UBSAN "Instrument kernel with UBSan" OFF)
if(FAULT STREQUAL "ubsan" AND NOT UBSAN)
    message(FATAL_ERROR "The UBSan fault requires -DUBSAN=ON")
endif()
find_program(NASM nasm REQUIRED)
find_program(HOST_CC gcc REQUIRED)
find_program(PYTHON python3 REQUIRED)

set(ARCH_OBJECTS "${CMAKE_BINARY_DIR}/entry.o" "${CMAKE_BINARY_DIR}/interrupts.o")
foreach(asm entry interrupts)
    add_custom_command(OUTPUT ${asm}.o
        COMMAND ${NASM} -f elf64 -g -F dwarf ${CMAKE_SOURCE_DIR}/kernel/${asm}.asm -o ${asm}.o
        DEPENDS kernel/${asm}.asm VERBATIM)
endforeach()
set_source_files_properties(${ARCH_OBJECTS} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
add_executable(kernel.elf ${ARCH_OBJECTS}
    kernel/main.c kernel/log.c kernel/arch.c kernel/memory.c kernel/input.c
    kernel/graphics.c kernel/fault.c kernel/ubsan.c
    lib/string.c lib/alloc.c lib/input.c lib/graphics.c)
target_include_directories(kernel.elf PRIVATE include)
target_compile_options(kernel.elf PRIVATE
    -ffreestanding -fno-stack-protector -fno-pie -fno-builtin -fno-omit-frame-pointer
    -mno-red-zone -mgeneral-regs-only -mcmodel=kernel -Wall -Wextra -Werror -g)
target_compile_definitions(kernel.elf PRIVATE FAULT_${FAULT}=1)
target_link_options(kernel.elf PRIVATE -nostdlib -static -no-pie
    -Wl,--build-id=none,-z,max-page-size=4096,-z,noexecstack
    -Wl,-Map=${CMAKE_BINARY_DIR}/kernel.map -T${CMAKE_SOURCE_DIR}/kernel/link.ld)
set_target_properties(kernel.elf PROPERTIES LINK_DEPENDS ${CMAKE_SOURCE_DIR}/kernel/link.ld)
if(UBSAN)
    target_compile_options(kernel.elf PRIVATE -fsanitize=undefined -fno-sanitize-recover=all)
endif()
# Keep the reporting path independent of sanitizer instrumentation.
set_source_files_properties(kernel/ubsan.c kernel/log.c PROPERTIES COMPILE_OPTIONS "-fno-sanitize=all")

add_custom_command(OUTPUT loader32.bin
    COMMAND x86_64-elf-gcc -m32 -Os -mno-sse -mno-mmx -msoft-float -std=c17
        -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables
        -ffunction-sections -Wall -Wextra -Werror -I${CMAKE_SOURCE_DIR}/include
        -c ${CMAKE_SOURCE_DIR}/boot/bios/loader.c -o loader32.o
    COMMAND x86_64-elf-ld -m elf_i386 -T ${CMAKE_SOURCE_DIR}/boot/bios/link.ld loader32.o -o loader32.elf
    COMMAND x86_64-elf-objcopy -O binary loader32.elf loader32.bin
    DEPENDS boot/bios/loader.c boot/bios/link.ld include/boot.h include/elf.h VERBATIM)
add_custom_command(OUTPUT stage1.bin
    COMMAND ${NASM} -f bin ${CMAKE_SOURCE_DIR}/boot/bios/stage1.asm -o stage1.bin
    DEPENDS boot/bios/stage1.asm VERBATIM)
add_custom_command(OUTPUT stage2.bin
    COMMAND ${NASM} -f bin -I${CMAKE_BINARY_DIR}/ ${CMAKE_SOURCE_DIR}/boot/bios/stage2.asm -o stage2.bin
    DEPENDS boot/bios/stage2.asm loader32.bin VERBATIM)
set(EFI_INCLUDE "${CMAKE_SOURCE_DIR}/.tools/root/usr/include/efi" CACHE PATH "GNU-EFI headers (no library linked)")
add_custom_command(OUTPUT BOOTX64.EFI
    COMMAND clang -target x86_64-pc-win32-coff -ffreestanding -fshort-wchar -mno-red-zone
        -fno-stack-protector -fno-builtin -DGNU_EFI_USE_MS_ABI
        -I${EFI_INCLUDE} -I${EFI_INCLUDE}/x86_64 -I${CMAKE_SOURCE_DIR}/include
        -Wall -Wextra -Werror -c ${CMAKE_SOURCE_DIR}/boot/uefi/main.c -o uefi.obj
    COMMAND ${NASM} -f win64 ${CMAKE_SOURCE_DIR}/boot/uefi/entry.asm -o uefi-entry.obj
    COMMAND lld-link /subsystem:efi_application /entry:efi_main /nodefaultlib /machine:x64
        /out:BOOTX64.EFI uefi.obj uefi-entry.obj
    DEPENDS boot/uefi/main.c boot/uefi/entry.asm include/boot.h include/elf.h VERBATIM)
add_custom_command(OUTPUT LamOS.iso
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/tools/image.py ${CMAKE_BINARY_DIR}
    DEPENDS kernel.elf stage1.bin stage2.bin BOOTX64.EFI tools/image.py tools/iso9660.py VERBATIM)
add_custom_target(iso ALL DEPENDS LamOS.iso)

add_custom_command(OUTPUT host-tests
    COMMAND ${HOST_CC} -std=c17 -Wall -Wextra -Werror -g -fsanitize=undefined
        -fno-sanitize-recover=all -I${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/tests/host.c ${CMAKE_SOURCE_DIR}/lib/alloc.c
        ${CMAKE_SOURCE_DIR}/lib/input.c ${CMAKE_SOURCE_DIR}/lib/graphics.c -o host-tests
    DEPENDS tests/host.c lib/alloc.c lib/input.c lib/graphics.c
        include/alloc.h include/input.h include/graphics.h include/elf.h VERBATIM)
add_custom_target(host-tests-build ALL DEPENDS host-tests)
enable_testing()
add_test(NAME host-unit COMMAND ${CMAKE_BINARY_DIR}/host-tests)
add_test(NAME iso-unit COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/tests/iso_test.py ${CMAKE_BINARY_DIR}/LamOS.iso)
