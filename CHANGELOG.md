# Changelog

All notable changes to LamOS are recorded here. Version numbers follow semantic versioning. Documentation and validation evidence describe the implemented version, not future desktop features.

## 1.0.3 — Phase I: Core GUI Foundation — 2026-10-21

### Added

- Checked off-screen Surface abstraction, clipping, primitives including circles/even-odd polygons, overlap-safe copy and premultiplied alpha.
- Original complete printable ASCII 8×16 font, aligned/clipped text and procedural icons.
- Dirty-region compositor, separate window surfaces/front/composition buffers and bounded damage fallback.
- Sixteen generation-checked windows, focus/Z-order, pointer capture, cancellable outline move/resize, transactional OOM rollback and window controls.
- Independent Terminal instances with 256-character editor, 32-command history, 1,024-line scrollback, persistent hidden columns and five callback-driven CLI commands.
- Empty procedural desktop, launcher/taskbar, virtual read-only Folder and session-only theme/background/profiling Settings.
- 512-slot input queue with snapshot-based overflow recovery; calibrated TSC profiling and exact allocator leak checks.
- Sanitized host suites and deterministic 10,000-operation GUI stress, actual emulator GUI/pixel/stress tests, layout/depth and post-GUI panic checks.

### Changed

- One VERSION source; 256 MiB guest RAM and 128 MiB tracked heap.
- BIOS ELF loading streams PT_LOAD chunks through a 32 KiB bounce buffer. Both loaders and image validation accept ELF files up to 4 MiB while preserving BootInfo ABI v1.
- Bochs RFB output packets are serialized to prevent input-thread header updates interleaving with framebuffer data.

### Validated

- All nine debug/release/UBSan × QEMU BIOS/QEMU UEFI/Bochs BIOS GUI configurations, each with at least 60 host seconds of stress and exact allocator/frame restoration.
- Eight host suites per preset, 10,000 deterministic GUI operations, eleven loader cases, five layout/depth cases, sixteen preserved faults and two post-GUI panic screens.
- GDB handoff, clang-format, clang-tidy, cppcheck, clangd, unresolved-symbol checks and final visual review.

### Scope

All GUI execution remains inside the single-CPU kernel. Folder is not a disk filesystem browser. There are no user processes, independent applications, USB, networking, audio or persistent Settings. Physical hardware and USB ISO boot remain unvalidated.

Phase I stays a separate version entry. Only after future Phase III/v1.0.9 completion will v1.0.3, v1.0.6 and v1.0.9 be grouped under their then-established shared objective. No later phase is claimed complete.

## 1.0.0 — 2026-10-20

First graphical x86_64 kernel foundation.

### Added

- Entirely custom BIOS Stage 1/2 with El Torito CD reads, E820 memory discovery, CPUID long-mode/NX checks, VBE graphics, A20, protected/long-mode transitions and ELF64 loading.
- Custom Microsoft-ABI UEFI application, GOP selection, the same ELF64 kernel, memory-map normalization and bounded ExitBootServices retries.
- Shared BootInfo v1, kernel-owned stack/page tables and safe post-handoff UEFI memory reclamation.
- COM1 logging, source-aware panic, nested panic handling and a framebuffer panic display.
- Kernel GDT/IDT/TSS, normalized exception frames, page-fault decoding and double-fault IST.
- Bitmap PMM, four-level VMM, higher-half/direct mappings, read-only/NX protections, uncached framebuffer access and an aligned split/coalesce heap.
- PIC remapping/spurious IRQ handling, PIT monotonic ticks, PS/2 keyboard/mouse drivers and bounded input-event queue.
- Framebuffer primitives, clipping, original bitmap alphabet, backbuffer/damaged-region presentation, software cursor and an input dashboard.
- Debug/release/UBSan and isolated fault presets, CMake/Ninja builds, symbolic ELF/map files, GDB and clangd setup.
- Host and ISO validation tests, real emulator keyboard/mouse injection, framebuffer glyph/button checks, static analysis and release packaging/checksums.
- English contributor instructions, architecture/specs, testing/toolchain documentation, GPL-3.0-only license and third-party notices.

### Validated

- Debug, release and UBSan kernels on QEMU BIOS, QEMU UEFI/OVMF and Bochs BIOS with the documented RFB guest-depth patch.
- Host tests under UBSan, malformed ISO/ELF cases, keyboard modifiers/editing/releases, mouse movement/buttons and framebuffer updates.
- BIOS/UEFI panic, divide error, invalid opcode, page fault, double fault, UBSan overflow, text write-protection and NX execution faults.
- Formatting, clang-tidy, cppcheck, clangd kernel parsing and GDB entry/BootInfo inspection.

### Known limits

- One CPU, ring-0 kernel, PIC/PIT and PS/2 only; no applications/processes, windows, USB, kernel filesystem, networking or audio.
- PMM below 4 GiB, fixed 8 MiB kernel heap and framebuffer up to 1280×1024 in supported 24/32-bit RGB layouts.
- BIOS supports the generated CD ISO layout and ELF files up to 320 KiB. The ISO is not a USB-hybrid disk image.
- UEFI Secure Boot, arbitrary GOP PixelBitMask formats and real-hardware compatibility are not validated.
- The compact font renders lowercase input with uppercase shapes; there is no Unicode/Vietnamese input method.
- The bounded input queue may drop events under overload. Intermediate page tables remain allocated for the kernel lifetime.

See `docs/VALIDATION.md` for executed release evidence and artifact locations.
