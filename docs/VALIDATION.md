# LamOS 1.0.3 validation report

Release acceptance completed on **2026-10-21 (Asia/Ho_Chi_Minh)** in the Ubuntu 22.04 development environment. This report records executed emulator/host checks. It does not establish physical-hardware or USB-boot support.

## Normal GUI matrix

All nine artifact-bound configurations passed at **256 MiB RAM**, with the **128 MiB heap** and 1024×768×32 guest framebuffer:

| Preset | Machine | Stress host seconds | Completed cycles | Result |
| --- | --- | ---: | ---: | --- |
| debug | bios | 63.4 | 16 | PASS |
| release | bios | 65.6 | 17 | PASS |
| ubsan | bios | 65.7 | 15 | PASS |
| debug | uefi | 65.2 | 17 | PASS |
| release | uefi | 65.2 | 17 | PASS |
| ubsan | uefi | 63.1 | 15 | PASS |
| debug | bochs | 77.9 | 2 | PASS |
| release | bochs | 66.8 | 3 | PASS |
| ubsan | bochs | 113.1 | 2 | PASS |

Each run booted into an empty desktop, reached all foundation markers, opened Terminal and executed all five commands, edited/history/scrolled input, opened Folder/Settings, changed theme/background/profiling, switched focus, previewed/committed move and resize, minimized/maximized/restored and closed windows. Input was delivered through real emulator RFB keyboard/PS/2 paths. Terminal glyphs were decoded from actual framebuffer pixels; serial markers alone were not accepted as graphics evidence.

Every stress run used the host monotonic clock for at least 60 seconds. After create/resize/command/destroy cycles, live allocation count, used bytes, largest free block and PMM free frames exactly matched that run's empty-desktop baseline. The retained compositor owns three allocations (9,437,184 used bytes at this resolution). The final demonstration screenshot opens a fresh Terminal after the leak checkpoint.

The public `acceptance.json` includes ISO/kernel SHA-256 hashes, per-run memory checkpoints and frame profiling samples. Local evidence is under `build/<preset>/test-results/<machine>/`: serial/emulator logs, result JSON, PNG/PPM screenshots and `profile.log`. Profiles contain sample count, mean, rolling-128 p95, maximum, render/composite/present costs and dirty pixels/rectangles. These are emulator measurements; no absolute Bochs FPS minimum is claimed.

## Host, loader and layout gates

- Eight CTest suites passed in each normal preset: allocator/input/ELF, ISO, Surface, resources, compositor, window manager, Terminal/shell and GUI stress.
- Host GUI tests ran with ASan/UBSan, exercised three display sizes and executed 10,000 deterministic operations including lifecycle/focus/geometry, allocation-failure injection and input recovery. Canary, clipping, stride padding, overlapping copy, alpha, stale handles, damage overflow, history/scrollback bounds and exact allocation accounting passed.
- LeakSanitizer thread enumeration is disabled for this restricted/ptraced runner. ASan bounds checks remain active; exact tracked allocation counts/bytes plus real kernel heap/frame restoration provide the leak gates.
- All 11 loader cases passed: streamed payloads relocated beyond 512 KiB, a sector-crossing program table, explicit pre-kernel `BSS:OK`, malformed/truncated ELF rejection, exactly 4 MiB acceptance and larger-file rejection on BIOS/UEFI, plus BIOS physical short-read failure. The oversized negative image was also rejected by image validation.
- All 5 layout/depth cases passed: 640×480 and 1280×1024 on QEMU BIOS/UEFI, plus 1024×768×24 on QEMU BIOS. Each checked rendered Terminal output. Host layout tests covered 640×480, 1024×768 and 1280×1024.

## Diagnostics and tool gates

All **16** preserved BIOS/UEFI fault checks passed: explicit panic, divide, invalid opcode, page fault, double fault/IST, UBSan overflow, read-only write and NX execution. Their harnesses verified expected diagnostics, a single entry and a halted guest rather than reboot. Two additional post-GUI panic tests verified the red framebuffer through RFB pixels.

GDB verified a hardware breakpoint at BIOS `kmain`, source symbols and BootInfo magic/version. clang-format, clang-tidy, cppcheck and clangd kernel parsing passed. The kernel linked without unresolved symbols, including GCC UBSan handlers. Suppressions and emulator patches are explained in `docs/TOOLCHAIN.md`.

## Visual review and reproducibility

Final release desktop screenshots from QEMU BIOS, QEMU UEFI and Bochs were visually reviewed, along with Settings, the 640×480 layout and the 24-bit presentation path. Text, controls, layering and cursor were visible without framebuffer corruption. Bochs uses the pinned, patched RFB backend: its screenshots include a 32-pixel header and 18-pixel status bar and BGR233 color quantization, while the guest remains 32-bit.

Run `. tools/env.sh`, then `python3 tools/verify.py` to repeat all gates. `--resume` reuses only passing >=60-second normal runs with unchanged ISO/ELF hashes. `python3 tools/package.py` refuses stale or incomplete acceptance and creates the source archive, release metadata and checksums.

The verified original v1.0.0 source archive was preserved before changes. Its SHA-256 is `f815e57bb2893d0ba8d23520fbff1034348e9d568f347f31893acea7a152d007`. The baseline tag preserves its source history; this checksum identifies the original local archive, not GitHub's automatically generated tarball.

## Remaining limits

All GUI execution is inside a single-CPU ring-0 kernel. There are no independent applications, processes, syscalls, kernel filesystem, USB stack, network or audio services. Folder contains embedded read-only resources. Settings persist only in RAM. Input is US QWERTY printable ASCII with a three-button PS/2 mouse. UEFI supports GOP RGB/BGR byte layouts with Secure Boot disabled. Physical computers, USB ISO boot, arbitrary GOP bit masks and alternate Bochs resolutions remain unvalidated. PMM stays below 4 GiB; intermediate page tables remain allocated for the kernel lifetime.
