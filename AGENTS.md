# LamOS contributor instructions

LamOS is a GPL-3.0-only, freestanding x86_64 graphics foundation. The v1.0.3 scope extends custom BIOS/UEFI boot, serial/panic, exceptions, memory, PIC/PIT, PS/2 input and framebuffer graphics. It has one CPU and no user processes, USB stack or kernel filesystem.

## Sources of truth

- Read `README.md`, `ARCHITECTURE.md` and `docs/BOOT_PROTOCOL.md` before changing boot or memory code.
- `include/boot.h` defines the shared loader ABI. BIOS assembly writes fixed offsets: update it whenever the C layout changes. Both 32-bit and 64-bit compilation must retain its layout assertions.
- Keep `ARCHITECTURE.md`, relevant specs, `CHANGELOG.md`, build commands and test reports synchronized with every behavior/version change. All project documentation is English.
- Never describe an untested firmware, device or hardware machine as supported. Never mark a release complete while its required acceptance tests fail.

## Build and checks

```sh
. tools/env.sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
python3 tools/check.py
python3 tools/verify.py
```

`tools/bootstrap.sh --bochs` prepares pinned Ubuntu 22.04 tools locally. A working `x86_64-elf-gcc` toolchain must already be on PATH. Verification opens localhost RFB sockets; restricted runners may need explicit execution permissions. Build artifacts and downloaded tools belong in ignored `build/` and `.tools/` only.

## Invariants

- No GRUB, Limine, borrowed loader implementation or host C runtime in the kernel. GNU-EFI headers supply declarations only; no GNU-EFI library is linked.
- Assembly establishes entry stacks and calling conventions. Kernel C uses the System V x64 ABI, no red zone and general registers only. UEFI C uses the Microsoft x64 ABI. Do not mix these at handoff.
- Initial loader maps cover physical 0–4 GiB plus direct and higher-half aliases. Kernel removes identity mappings, enables CR0.WP and enforces NX. Never dereference physical addresses directly after memory initialization; use `DIRECT`.
- `BootInfo` is copied before loader memory is reclaimed. Preserve kernel, tables, stacks, framebuffer, firmware reservations and runtime services. Reserved regions override usable overlaps.
- PIC IRQ handlers do bounded work without heap allocation or rendering. Main-loop event consumption disables interrupts around queue operations. Do not add blocking device commands to ISR paths.
- Heap allocations are 16-byte aligned. PMM and VMM are bootstrap/main-context APIs, not interrupt-safe or SMP-safe services. Mapping ownership and frame ownership are separate.
- The logger's `%u` and `%x` arguments are explicitly `uint64_t`; `%c` takes promoted `int`. These are deliberately not standard printf length rules.
- Fault images are isolated presets; never enable deliberate faults in a normal release. UBSan reporting and serial/panic code are not instrumented.
- Keep timeout bounds on firmware/device polls and emulator tests. Preserve logs and screenshots when debugging failures.

## Implementation order and release evidence

Preserve the order boot → serial/panic → exceptions → memory → timer → keyboard → graphics → mouse/events when introducing dependent changes. Verify foundational changes before continuing to higher layers.

The required normal matrix is debug/release/UBSan × QEMU BIOS/QEMU UEFI/Bochs BIOS. Input tests must send real emulator input and check serial events plus framebuffer pixels. BIOS and UEFI fault tests cover panic, divide, invalid opcode, page fault, double fault, UBSan, read-only writes and NX execution. See `docs/TESTING.md`.

Do not commit generated binaries, third-party build trees, credentials or machine logs. Record dependency versions/checksums and third-party license notices. The original LamOS bitmap alphabet requires no external font assets.

## Phase I GUI invariants

- VERSION is the only current product-version source. Preserve BootInfo ABI v1.
- Default RAM is 256 MiB, heap 128 MiB, ELF file ceiling 4 MiB, and window limit 16.
- GUI libraries use allocator/presentation/time/info adapters; no IRQ or port I/O.
- Surface rectangles are signed and half-open; stride is bytes. ARGB is premultiplied.
- Keep framebuffer writes in presentation/panic adapters. Reconstruct damage from scene surfaces.
- Generation handles reject stale windows. Resize allocates before releasing old storage; failure preserves state.
- Overflow recovery discards queued events, restores device snapshots and cancels capture.
- Run milestone host tests before dependent changes; run the complete nine-configuration GUI matrix, 60-second host-time stress, layout/depth tests, faults and static checks before release.
- Keep Phase I/v1.0.3 separate in CHANGELOG. Group v1.0.3/v1.0.6/v1.0.9 only after future Phase III completion establishes the shared objective.
