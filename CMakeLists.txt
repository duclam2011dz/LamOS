cmake_minimum_required(VERSION 3.22)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/x86_64-elf.cmake" CACHE FILEPATH "Cross toolchain")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" LAMOS_VERSION LIMIT_COUNT 1)
project(LamOS VERSION ${LAMOS_VERSION} LANGUAGES C)
configure_file(include/version.h.in generated/version.h @ONLY)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(FAULT "none" CACHE STRING "Isolated fault-injection scenario")
set_property(CACHE FAULT PROPERTY STRINGS none panic divide opcode page double ubsan write nx graphics)
option(UBSAN "Instrument kernel with UBSan" OFF)
if(FAULT STREQUAL "ubsan" AND NOT UBSAN)
    message(FATAL_ERROR "The UBSan fault requires -DUBSAN=ON")
endif()
set(VIDEO_WIDTH 1024 CACHE STRING "Preferred framebuffer width")
set(VIDEO_HEIGHT 768 CACHE STRING "Preferred framebuffer height")
set(VIDEO_BPP 32 CACHE STRING "Preferred BIOS pixel depth")
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
target_include_directories(kernel.elf PRIVATE include ${CMAKE_BINARY_DIR}/generated)
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
    DEPENDS boot/bios/loader.c boot/bios/link.ld include/boot.h include/elf.h include/lamos_limits.h VERBATIM)
add_custom_command(OUTPUT stage1.bin
    COMMAND ${NASM} -f bin ${CMAKE_SOURCE_DIR}/boot/bios/stage1.asm -o stage1.bin
    DEPENDS boot/bios/stage1.asm VERBATIM)
add_custom_command(OUTPUT stage2.bin
    COMMAND ${NASM} -f bin -DVIDEO_WIDTH=${VIDEO_WIDTH} -DVIDEO_HEIGHT=${VIDEO_HEIGHT} -DVIDEO_BPP=${VIDEO_BPP} -I${CMAKE_BINARY_DIR}/ ${CMAKE_SOURCE_DIR}/boot/bios/stage2.asm -o stage2.bin
    DEPENDS boot/bios/stage2.asm loader32.bin VERBATIM)
set(EFI_INCLUDE "${CMAKE_SOURCE_DIR}/.tools/root/usr/include/efi" CACHE PATH "GNU-EFI headers (no library linked)")
add_custom_command(OUTPUT BOOTX64.EFI
    COMMAND clang -target x86_64-pc-win32-coff -ffreestanding -fshort-wchar -mno-red-zone
        -fno-stack-protector -fno-builtin -DGNU_EFI_USE_MS_ABI -DVIDEO_WIDTH=${VIDEO_WIDTH} -DVIDEO_HEIGHT=${VIDEO_HEIGHT}
        -I${EFI_INCLUDE} -I${EFI_INCLUDE}/x86_64 -I${CMAKE_SOURCE_DIR}/include
        -Wall -Wextra -Werror -c ${CMAKE_SOURCE_DIR}/boot/uefi/main.c -o uefi.obj
    COMMAND ${NASM} -f win64 ${CMAKE_SOURCE_DIR}/boot/uefi/entry.asm -o uefi-entry.obj
    COMMAND lld-link /subsystem:efi_application /entry:efi_main /nodefaultlib /machine:x64
        /out:BOOTX64.EFI uefi.obj uefi-entry.obj
    DEPENDS boot/uefi/main.c boot/uefi/entry.asm include/boot.h include/elf.h include/lamos_limits.h VERBATIM)
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
target_sources(kernel.elf PRIVATE lib/surface.c)
add_custom_command(OUTPUT surface-tests
 COMMAND ${HOST_CC} -std=c17 -Wall -Wextra -Werror -g -fsanitize=undefined -fno-sanitize-recover=all -I${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests/surface.c ${CMAKE_SOURCE_DIR}/lib/surface.c -o surface-tests
 DEPENDS tests/surface.c lib/surface.c include/surface.h VERBATIM)
add_custom_target(surface-tests-build ALL DEPENDS surface-tests)
add_test(NAME surface-unit COMMAND ${CMAKE_BINARY_DIR}/surface-tests)
target_sources(kernel.elf PRIVATE lib/resources.c)
add_custom_command(OUTPUT resources-tests
 COMMAND ${HOST_CC} -std=c17 -Wall -Wextra -Werror -g -fsanitize=undefined -fno-sanitize-recover=all -I${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests/resources.c ${CMAKE_SOURCE_DIR}/lib/resources.c ${CMAKE_SOURCE_DIR}/lib/surface.c -o resources-tests
 DEPENDS tests/resources.c lib/resources.c lib/font_data.inc lib/surface.c include/resources.h VERBATIM)
add_custom_target(resources-tests-build ALL DEPENDS resources-tests)
add_test(NAME resources-unit COMMAND ${CMAKE_BINARY_DIR}/resources-tests)
target_sources(kernel.elf PRIVATE lib/compositor.c)
add_custom_command(OUTPUT compositor-tests
 COMMAND ${HOST_CC} -std=c17 -Wall -Wextra -Werror -g -fsanitize=undefined -fno-sanitize-recover=all -I${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests/compositor.c ${CMAKE_SOURCE_DIR}/lib/compositor.c ${CMAKE_SOURCE_DIR}/lib/surface.c -o compositor-tests
 DEPENDS tests/compositor.c lib/compositor.c lib/surface.c include/compositor.h VERBATIM)
add_custom_target(compositor-tests-build ALL DEPENDS compositor-tests)
add_test(NAME compositor-unit COMMAND ${CMAKE_BINARY_DIR}/compositor-tests)
target_sources(kernel.elf PRIVATE lib/wm.c)
add_custom_command(OUTPUT wm-tests
 COMMAND ${HOST_CC} -std=c17 -Wall -Wextra -Werror -g -fsanitize=undefined -fno-sanitize-recover=all -I${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests/wm.c ${CMAKE_SOURCE_DIR}/lib/wm.c ${CMAKE_SOURCE_DIR}/lib/compositor.c ${CMAKE_SOURCE_DIR}/lib/surface.c -o wm-tests
 DEPENDS tests/wm.c lib/wm.c lib/compositor.c lib/surface.c include/wm.h VERBATIM)
add_custom_target(wm-tests-build ALL DEPENDS wm-tests)
add_test(NAME wm-unit COMMAND ${CMAKE_BINARY_DIR}/wm-tests)
target_sources(kernel.elf PRIVATE lib/terminal.c lib/shell.c)
add_custom_command(OUTPUT terminal-tests
 COMMAND ${HOST_CC} -std=c17 -Wall -Wextra -Werror -g -fsanitize=undefined -fno-sanitize-recover=all -I${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests/terminal.c ${CMAKE_SOURCE_DIR}/lib/terminal.c ${CMAKE_SOURCE_DIR}/lib/shell.c ${CMAKE_SOURCE_DIR}/lib/resources.c ${CMAKE_SOURCE_DIR}/lib/surface.c -o terminal-tests
 DEPENDS tests/terminal.c lib/terminal.c lib/shell.c lib/resources.c lib/surface.c include/terminal.h include/input.h VERBATIM)
add_custom_target(terminal-tests-build ALL DEPENDS terminal-tests)
add_test(NAME terminal-unit COMMAND ${CMAKE_BINARY_DIR}/terminal-tests)
target_sources(kernel.elf PRIVATE lib/gui.c)
set(GUI_LIBRARY_SOURCES lib/surface.c lib/resources.c lib/compositor.c lib/wm.c lib/terminal.c lib/shell.c lib/gui.c)
file(GLOB GUI_HEADERS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/include/*.h")
set(GUI_HOST_SOURCES)
foreach(source IN LISTS GUI_LIBRARY_SOURCES)
 list(APPEND GUI_HOST_SOURCES ${CMAKE_SOURCE_DIR}/${source})
endforeach()
add_custom_command(OUTPUT gui-tests
 COMMAND ${HOST_CC} -O1 -std=c17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all -I${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests/gui.c ${GUI_HOST_SOURCES} -o gui-tests
 DEPENDS tests/gui.c ${GUI_LIBRARY_SOURCES} ${GUI_HEADERS} lib/font_data.inc VERBATIM)
add_custom_target(gui-tests-build ALL DEPENDS gui-tests)
add_test(NAME gui-stress COMMAND ${CMAKE_BINARY_DIR}/gui-tests)
# LeakSanitizer cannot enumerate threads in restricted/ptraced runners. The
# stress allocator checks exact live bytes/counts; ASan bounds checks stay on.
set_tests_properties(gui-stress PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
