# LamOS 1.0.0 validation report

Release validation completed on **2026-10-20 (Asia/Saigon)** using the workspace's Ubuntu 22.04 development environment. This report records executed checks, not proposed support for physical machines.

## Normal boot and interaction matrix

All nine configurations passed the complete automated boot, keyboard, mouse and framebuffer checks:

| Kernel preset | QEMU BIOS | QEMU UEFI / OVMF | Bochs BIOS / patched RFB |
| --- | --- | --- | --- |
| Debug | PASS | PASS | PASS |
| Release | PASS | PASS | PASS |
| UBSan | PASS | PASS | PASS |

Every configuration reached `READY:LamOS` with BootInfo, serial, GDT/IDT/TSS, PMM/VMM/heap, real timer ticks, keyboard, framebuffer and mouse milestones. There were no unexpected kernel panics or UBSan reports during normal tests. UEFI also reported post-handoff arena/Boot Services reclamation.

Actual PS/2 input paths were exercised through emulator RFB input: Shift+A, typing, Backspace, Enter submission, key release, mouse movement and button press/release. Rendered `A` and `LAMOS` glyph pixels were checked before screenshots were accepted. The mouse test waited for the visible button indicator to turn green.

Guest framebuffer size was 1024×768×32 on all three boot paths. Bochs RFB screenshots are 1024×818 because they include its 32-pixel header and 18-pixel status bar; the host stream is quantized to BGR233. Final release screenshots from all three machines were visually inspected: labels, entered text, cursor, uptime and memory status were visible without framebuffer corruption.

## Memory and fault evidence

Host allocator tests verified reserved/overlapping/partial ranges, unique allocations, exhaustion, invalid/double free rejection, heap alignment and split/coalesce behavior. Kernel self-tests verified a mapped frame could be written, queried, unmapped and freed. The direct text alias is read-only/NX and the higher-half kernel uses segment-specific permissions.

Eight isolated fault cases passed on each QEMU firmware path, **16/16 total**:

- Explicit source-aware panic.
- Divide error and invalid opcode exception dumps.
- Page fault including CR2/access decoding.
- Double fault after stack invalidation, handled through the dedicated IST stack.
- Instrumented signed overflow reporting `UBSAN:add_overflow` with source position before panic.
- Present/write fault on kernel text.
- Instruction-fetch fault on an NX heap page.

Fault tests observed the expected diagnostics, a single kernel entry and a halted guest without reboot. The graphics panic code is implemented; these deliberate fault cases run before graphics initialization and therefore validate its serial/exception path rather than a rendered panic screen.

## Build, analysis and developer tools

- Debug/release/UBSan CMake+Ninja builds completed; both ISO kernel copies matched the built ELF byte-for-byte.
- CTest passed host-library and actual-ISO malformed-input tests for every normal preset. Host C tests ran under GCC's UBSan runtime.
- clang-format, clang-tidy and cppcheck passed with the documented narrowly scoped exceptions.
- clangd checked the kernel entry translation unit with **0 errors** using the wrapper/resource configuration.
- GDB stopped at a hardware breakpoint at `kmain` and read `magic=0x314f534f4d414c`, `version=1` through BootInfo debug types and entry register RDI. The parameter's stack location is not initialized until the C prologue executes; inspect `$rdi` or step past the prologue at this breakpoint.
- The final verification script completed with `LAMOS ACCEPTANCE:PASS`.

## Evidence and artifacts

Generated evidence is retained locally, excluded from source control:

| Location | Contents |
| --- | --- |
| `build/verification.log` | Complete final build/test transcript |
| `build/acceptance.json` | Normal matrix summary |
| `build/<preset>/test-results/<machine>/` | Result JSON, serial/emulator logs and PNG/PPM screenshots |
| `build/fault-*/test-results/{bios,uefi}/` | Fault diagnostics and per-case results |
| `build/fault-results-{bios,uefi}.json` | Aggregate fault results |
| `build/debug/test-results/gdb/gdb.log` | GDB breakpoint/type/register transcript |
| `build/debug/static-analysis.log` | Formatting/static-analysis transcript |
| `build/debug/clangd-check.log` | clangd validation transcript |
| `build/release/` | Release ISO, symbolic ELF, source archive, licenses and checksums |

`tools/package.py` creates `LamOS-1.0.0-source.tar.gz`, copies notices and writes release metadata/SHA256SUMS after successful acceptance. The source archive excludes downloaded tools, build outputs and workspace account/config directories. Artifacts are local; no repository push, external publishing or hardware installation was performed.

## Remaining compatibility limits

Acceptance covers the documented emulator configurations only. Physical machines, Secure Boot, USB boot/input, SMP/APIC, applications/user mode and arbitrary GOP bitmask modes remain outside v1 support. The unmodified Bochs RFB plugin cannot supply the required guest pixel depths; its visible source patch is necessary for this test configuration. Intermediate page tables remain allocated, the heap is fixed at 8 MiB, managed physical memory is below 4 GiB, and the input queue is bounded rather than lossless under overload.
