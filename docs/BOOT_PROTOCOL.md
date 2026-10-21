# Boot protocol and ISO format

## ABI v1

`include/boot.h` is authoritative. Fields are little-endian fixed-width integers. BootInfo is 3,200 bytes, naturally aligned to at least eight bytes; both loaders use identical layout. No firmware-managed pointers survive into the ABI. Addresses and ranges are physical unless explicitly stated.

| Offset | Field | Meaning |
| --- | --- | --- |
| 0 | `magic: u64` | `0x00314f534f4d414c` |
| 8 | `version: u32` | 1 |
| 12 | `size: u32` | 3200 |
| 16 | `firmware: u32` | 0 BIOS, 1 UEFI |
| 20 | `map_count: u32` | Number of valid ranges, at most 128 |
| 24 | `kernel_file_size: u64` | ELF file size in bytes |
| 32 | `fb: Framebuffer` | 32-byte linear framebuffer descriptor |
| 64, 72 | `kernel_start`, `kernel_end` | Physical loaded ELF extent, including BSS |
| 80, 88 | `loader_start`, `loader_end` | Explicit loader arena reservation |
| 96, 104 | `stack_start`, `stack_end` | Initial loader handoff stack reservation |
| 112, 120 | `tables_start`, `tables_end` | Initial page-table reservation |
| 128 | `map[128]` | 24-byte memory ranges |

All extents are `[start, end)`. A memory range is `{base:u64, length:u64, type:u32, reserved:u32}`. Type 1 means usable after successful firmware exit/handoff; type 2 means reserved. The reserved field is ignored. BIOS may leave E820 extended attributes there. Ranges may be unsorted or overlap: reserved takes precedence in PMM. Zero-length ranges have no effect. Unrepresented memory is never admitted to PMM.

UEFI conventional, LoaderCode/Data and BootServicesCode/Data ranges become type 1. Runtime Services, ACPI/NVS, MMIO and other types stay reserved. Explicit arena/kernel/framebuffer reservations override type 1 until their ownership changes. BIOS converts nonusable E820 types to reserved.

Framebuffer fields, relative to offset 32:

| Offset | Field |
| --- | --- |
| 0 | physical address, u64 |
| 8, 12 | width, height, u32 pixels |
| 16, 20 | pitch in bytes and bits per pixel, u32 |
| 24, 25 | red shift and bit count, u8 |
| 26, 27 | green shift and bit count, u8 |
| 28, 29 | blue shift and bit count, u8 |
| 30 | two padding bytes |

Pixels are packed little-endian 24/32-bit RGB with explicit channel positions. The kernel validates bounds before memory initialization, honors pitch and converts its RGB backbuffer to this layout. The default preference is 1024×768×32; build-time VIDEO_WIDTH, VIDEO_HEIGHT and VIDEO_BPP select test preferences. UEFI supports the GOP RGB/BGR byte layouts, not PixelBltOnly or arbitrary PixelBitMask modes in v1.

## CPU entry contract

- ELF is static ELF64 little-endian ET_EXEC, machine x86_64.
- `PT_LOAD` segments start at physical 1 MiB or later, end at/below 32 MiB, use 4 KiB alignment and have `vaddr = KBASE + paddr`.
- File bounds, segment sizes/overlap, executable entry containment and W+X rejection are checked before loading. BSS is zeroed. Dynamic/interpreter segments are rejected.
- Long mode, PAE, paging and NXE are active. Interrupts are disabled; DF is clear on kernel C entry.
- `RDI` contains the physical BootInfo address. No other register is an argument.
- The initial root identity maps 0–4 GiB, direct maps the same extent at `0xffff800000000000`, and maps the first physical GiB at the kernel higher-half base.
- Entry replaces the loader stack with the kernel stack and uses System V x64 conventions. The UEFI handoff assembly explicitly translates Microsoft ABI arguments.
- The kernel does not rely on loader GDT/TSS/IDT after installing its own tables and does not call firmware after entry.

## BIOS memory layout

| Physical range | Use |
| --- | --- |
| `0x5000–0x5c80` | BootInfo |
| `0x6000–0x63ff` | VBE controller/mode scratch |
| `0x7c00–0x7fff` | Stage 1 and real-mode stack vicinity |
| `0x8000–0xffff` | Stage 2; C helper linked at `0xc000` |
| `0x6400` | Protected-mode copy job: destination, source, count |
| `0x10000–0x17fff` | 32 KiB streaming payload bounce buffer |
| `0x18000–0x187ff` | ELF header sector |
| `0x20000–0x20fff` | Program-header sectors, including sector-boundary overlap |
| `0x60000–0x6ffff` | Protected/long-mode handoff stack region |
| `0x70000–0x76fff` | Seven initial page-table pages |
| `0x100000+` | Loaded kernel segments |

The kernel keeps the first MiB reserved. Bootstrap pages are not reused as general RAM on BIOS. UEFI allocates a separate 32-page arena below 4 GiB and releases it only after copying BootInfo, switching stack and installing a new CR3.

## ISO layout

The ISO9660 root contains `KERNEL.ELF;1`. BIOS reads the primary volume descriptor at CD LBA 16, follows its root extent and searches the one-sector root directory. Only a contiguous, non-multiextent kernel file is supported. Packaging verifies this layout and kernel extent bounds. ELF files may contain at most 4 MiB. The BIOS loader reads header/program metadata separately, validates all segments before copying and streams sector-aligned PT_LOAD data in chunks of at most 32 KiB. The copy helper enters flat protected mode for high-memory access, then returns through 16-bit descriptors to the saved real-mode stack. It zeroes each BSS tail explicitly; the full ELF is never buffered in conventional memory. UEFI applies the same ELF validation and file-size ceiling.

`/boot/bios.img` contains one Stage 1 CD sector followed by sixteen Stage 2 sectors. The BIOS El Torito entry loads four 512-byte units (one CD sector). xorriso patches Stage 1's boot-info table, including boot-image LBA at byte 12; Stage 1 uses `LBA + 1` for Stage 2.

`/boot/esp.img` is a 16 MiB FAT image with `/EFI/BOOT/BOOTX64.EFI` and `/KERNEL.ELF`. The second El Torito entry has UEFI platform ID 0xEF. The ESP is a FAT filesystem image for optical boot, not a GPT disk. Packaging byte-compares its kernel with the ISO root and build artifact.

See the [UEFI media-access specification](https://uefi.org/specs/UEFI/2.10/13_Protocols_Media_Access.html) for the ESP-in-El-Torito mechanism. The ISO is not advertised as a USB-hybrid image.

## Failure behavior

Stage 1 reports disk failures through BIOS text. Stage 2 reports `BIOS:ERROR` or `BIOS:ELF_ERROR` through COM1 and halts. Its single-letter A–G trace marks entry, memory map, CPU check, mode selection, graphics activation, kernel discovery and completed disk reads. UEFI reports a named `UEFI:*` failure to COM1 plus a console message before halting. Missing video, malformed ELF, exhausted map capacity and firmware-exit failure are fatal; no silent text-only success is reported.
