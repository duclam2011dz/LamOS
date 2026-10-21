# Toolchain and development setup

## Tested components

| Component | Purpose |
| --- | --- |
| NASM | BIOS real/protected/long-mode code, kernel entry and interrupt stubs |
| x86_64-elf GCC 12.2.0 and GNU binutils | Freestanding C17 kernel, 32-bit BIOS C helper, ELF64 linking |
| Clang/LLD 14 | UEFI COFF/PE32+ application with Microsoft x64 ABI |
| CMake 3.22.1 / Ninja 1.10.1 | Presets, dependency tracking, ISO and host tests |
| GDB | Symbolic kernel debugging through QEMU gdbstub |
| clangd 14 | Editor parsing/indexing using generated compilation commands |
| clang-format / clang-tidy 14 | Formatting and static analysis |
| cppcheck 2.7 | Additional C checks using its supported C11 grammar |
| QEMU / SeaBIOS / OVMF | BIOS and UEFI acceptance |
| Bochs 2.7 with local RFB patch | Independent BIOS acceptance and input/screenshot tests |
| GCC ASan / UBSan | Host GUI bounds checks and freestanding kernel undefined-behavior instrumentation |
| xorriso / mtools / Python 3 | ISO, FAT ESP, checksums and test harnesses |

The cross compiler must support `-m32` code generation and binutils must support `elf_i386`. The BIOS helper does not link a 32-bit C runtime. A native system GCC cannot replace the kernel cross compiler.

`tools/packages.lock.json` pins local Ubuntu package versions and SHA-256 hashes. Downloaded packages are extracted under `.tools/root`; they are not installed into the host. System prerequisites not supplied there must already be installed: native build tools, the cross toolchain, NASM, Clang/LLD, xorriso/mtools, QEMU/SeaBIOS/OVMF, static analyzers and GDB.

```sh
bash tools/bootstrap.sh --bochs
. tools/env.sh
cmake --preset debug
cmake --build --preset debug
```

If GNU-EFI headers are installed system-wide, pass `-DEFI_INCLUDE=/usr/include/efi` when configuring. No GNU-EFI archive, startup object or wrapper library is linked. CMake invokes Clang/LLD directly for UEFI and GCC/binutils for BIOS/kernel.

## Bochs RFB compatibility

Upstream Bochs 2.7 RFB advertises only 8-bit guest modes and rejects 24/32-bit `dimension_update` calls. This prevents VBE from offering the required framebuffer modes. `tools/bochs-rfb-depth.patch` updates these depth guards: guest 24/32-bit mode is accepted and the advertised maximum is 32. It also serializes output packets as described below. The existing new-graphics API converts guest RGB pixels to the RFB server's 8-bit BGR233 representation. This is a test-display compatibility patch, not LamOS graphics code.

Bootstrap pins Bochs commit `0e45b736ef9792eb9b752b0a35db49eaf2faea47` (`REL_2_7_FINAL`), applies the visible patch and builds a local executable. Tests advertise RFB desktop-size updates so 1024×768 is available. Screenshots include Bochs's 32-pixel header and status bar, with reduced host color depth. The guest framebuffer remains 32-bit.

The tested configuration uses SeaBIOS plus its standard PCI VGA ROM, an explicitly assigned `pcivga` slot and a supported `core2_penryn_t9600` CPU model. The debugger starts with `c`; the harness enables GUI mouse capture with F12 after resume. These details are emitted into each test's `bochsrc`.

## Editor and debugger

`.clangd` selects a freestanding x86_64 Clang parser and `build/debug/compile_commands.json`. `tools/clangd.sh` supplies the matching installed Clang resource headers and local shared libraries. This avoids mixing GCC-only builtin macros with Clang's standard headers. VS Code settings use that wrapper; other editors can invoke it directly.

```sh
tools/clangd.sh --check=kernel/main.c --compile-commands-dir=build/debug
python3 tools/run.py --gdb
gdb -x tools/kernel.gdb
```

QEMU stops at reset for GDB; use a hardware breakpoint if debugging before kernel code is loaded. Symbols are linked at higher-half virtual addresses. BIOS helper symbols can be inspected separately from `loader32.elf`, linked at physical `0xc000`.

## Analysis configuration

`tools/check.py` checks formatting, clang-tidy and cppcheck. The only deliberate analysis exceptions are documented alongside their configuration/code:

- Annex K `memcpy_s`/`memset_s` suggestions are disabled: those APIs do not exist in the freestanding runtime.
- The easily-swappable-parameters heuristic is disabled for conventional port/geometry/runtime positional APIs.
- GCC-mandated UBSan handler names are allowed reserved identifiers in that runtime only.
- Clang 14's false-positive varargs initialization diagnostic is suppressed within the logger; `va_start`/`va_end` are explicit and exercised across firmware tests.
- The intentional signed-overflow expression in the isolated UBSan fault is suppressed for cppcheck only.
- cppcheck variable-scope suggestions are suppressed for CPUID output variables because its inline-assembly analysis does not see their uses.
- cppcheck uses a no-return declaration for panic and omits missing host/system-header reports; its C11 grammar is sufficient for the project's C17 sources.

## RFB output serialization

The pinned Bochs RFB patch also serializes whole update packets with a recursive mutex. Headerbar updates triggered by the RFB input thread must not interleave with framebuffer updates on the emulation thread. The patch changes emulator display transport only; guest PS/2 drivers remain exercised. Test mouse delivery waits for guest semantic feedback because host/emulated time progresses at different rates.

## Version and display configuration

`VERSION` supplies CMake metadata, generated kernel version, shell information, validation metadata and release filenames. Alternate test builds use `-DVIDEO_WIDTH=640 -DVIDEO_HEIGHT=480` or `1280/1024`; BIOS also accepts `-DVIDEO_BPP=24`. Defaults remain 1024×768×32. All run/debug/fault tools allocate 256 MiB guest RAM.
