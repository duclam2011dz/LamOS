# Testing and release acceptance

Run from the project root after sourcing `tools/env.sh`. Normal tests never enable deliberate fault injection.

## Fast checks

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
python3 tools/check.py
tools/clangd.sh --check=kernel/main.c --compile-commands-dir=build/debug
```

Host tests use native GCC with UBSan and no sanitizer recovery. They exercise frame uniqueness/exhaustion/reservations, partial/overlapping memory ranges, invalid frees, heap splitting/coalescing/alignment, keyboard modifiers/releases/extended keys, mouse packet signs/synchronization/overflow, event-ring overflow, graphics clipping/channel layouts and malformed ELF headers/segments. ISO tests validate the actual artifact and reject malformed descriptors, root extents, missing kernel names and truncation.

Kernel boot self-tests allocate/map/write/query/unmap/free a frame, exercise heap allocation/coalescing/alignment/oversized failure, verify BootInfo and observe actual PIT ticks. Normal boot must reach `READY:LamOS` without panic or UBSan reports.

## Emulator matrix

```sh
python3 tools/emulator_test.py --machine bios
python3 tools/emulator_test.py --machine uefi
python3 tools/emulator_test.py --machine bochs
# Repeat with --build build/release and --build build/ubsan.
```

The required matrix is three presets × three machines. Every test boots its ISO, connects RFB, waits for explicit serial milestones and injects real emulator input. Tests verify Shift+A, a second character, Backspace, Enter submission, key release, mouse movement and button press/release. The final editable line is `LamOS`.

Framebuffer checks wait for exact foreground/background samples of the rendered `A` and `LAMOS` glyphs instead of assuming serial events prove rendering. Screenshots are `boot`, `keyboard`, `mouse` and `desktop`, saved as both PNG and PPM. Bochs images include its header/status UI and color quantization; whole-image equality across firmware is intentionally not required.

Every run saves `serial.log`, `emulator.log` and `result.json` under `build/<preset>/test-results/<machine>/`; Bochs also saves `bochsrc`, debugger commands and its internal log. Normal startup has a 60-second deadline, individual socket reads have timeouts, framebuffer text retries are bounded, and subprocess cleanup terminates the emulator. A failed run writes a failing result and raises an error.

RFB uses localhost ports 5907 for QEMU BIOS, 5908 for UEFI and 5900 for Bochs. Do not run two tests using the same port concurrently. A runner must permit local listening sockets. Read `docs/TOOLCHAIN.md` before replacing the local Bochs backend with a system executable.

## Exception and UBSan images

```sh
python3 tools/fault_tests.py
python3 tools/fault_tests.py --machine uefi
```

Each scenario uses a separate `fault-*` preset and ISO:

| Preset | Required diagnostic |
| --- | --- |
| fault-panic | Injected panic with source location |
| fault-divide | Vector 0, divide error |
| fault-opcode | Vector 6, invalid opcode |
| fault-page | Vector 14, CR2 and access flags |
| fault-double | Vector 8 on the dedicated IST stack after invalidating RSP |
| fault-ubsan | UBSan signed-add overflow, source location, panic |
| fault-write | Present/write page fault on kernel text |
| fault-nx | Instruction-fetch page fault on an NX heap page |

Tests require the expected marker followed by panic, check the emulator remains alive while the guest halts, and confirm a single kernel entry rather than a reboot loop. Each boot has a 20-second timeout. Fault output and result JSON are saved beneath its build directory. The UBSan fault preset requires instrumentation; normal builds never evaluate its deliberate overflow.

## GDB and full verification

```sh
python3 tools/gdb_test.py
python3 tools/verify.py
```

GDB verification uses localhost port 1234, breaks at `kmain` with a hardware breakpoint and checks BootInfo magic/version from debug symbols. Its transcript is saved under `build/debug/test-results/gdb/`.

The verification script configures/builds/tests all normal presets, runs the nine emulator/input checks, static analysis, GDB and sixteen BIOS/UEFI fault checks. A successful run writes `build/acceptance.json`. Rebuilding alone does not update prior test evidence: rerun affected tests whenever code, image layout, toolchain or emulator configuration changes.

## Manual visual review

Open final dashboard screenshots for QEMU BIOS, QEMU UEFI and Bochs. Confirm readable labels, visible entered text, sensible cursor position, timer/memory information and absence of corruption. For interactive inspection, use `tools/run.py`; automation checks event delivery but does not judge aesthetic quality or all key combinations.

Release completion requires a green matrix, expected fault diagnostics, clean static checks and visual review. Actual results and limits belong in `docs/VALIDATION.md`; do not substitute this test specification for a report of executed tests.
