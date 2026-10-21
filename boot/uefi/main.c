/* SPDX-License-Identifier: GPL-3.0-only */
#include <efi.h>
#include "boot.h"
#include "elf.h"
static EFI_SYSTEM_TABLE *system_table;
static void serial(const char *s) {
    while (*s) {
        __asm__ volatile("outb %0,%1" ::"a"((uint8_t)*s++), "Nd"((uint16_t)0x3f8));
    }
}
static void die(const char *s) {
    serial(s);
    system_table->ConOut->OutputString(system_table->ConOut,
                                       L"LamOS loader error; see serial.\r\n");
    for (;;)
        __asm__ volatile("cli; hlt");
}
void *memcpy(void *d, const void *s, size_t n) {
    for (size_t i = 0; i < n; i++)
        ((uint8_t *)d)[i] = ((const uint8_t *)s)[i];
    return d;
}
void *memset(void *d, int c, size_t n) {
    for (size_t i = 0; i < n; i++)
        ((uint8_t *)d)[i] = (uint8_t)c;
    return d;
}
extern void EFIAPI enter_kernel(uint64_t tables, uint64_t entry, uint64_t info, uint64_t stack);
static __attribute__((aligned(16))) uint8_t file_buffer[512 * 1024];
static uint8_t map_buffer[64 * 1024];
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    system_table = st;
    __asm__ volatile("outb %0,%1" ::"a"((uint8_t)3), "Nd"((uint16_t)0x3fb));
    serial("UEFI:START\n");
    uint32_t eax = 0x80000000, ebx, ecx, edx;
    __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    if (eax < 0x80000001)
        die("UEFI:CPU\n");
    eax = 0x80000001;
    __asm__ volatile("cpuid" : "+a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx));
    if ((edx & ((1u << 29) | (1u << 20))) != ((1u << 29) | (1u << 20)))
        die("UEFI:LONG_MODE_NX\n");
    EFI_BOOT_SERVICES *bs = st->BootServices;
    EFI_GUID loaded_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID,
             fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID,
             gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_LOADED_IMAGE *loaded = 0;
    EFI_FILE_IO_INTERFACE *fs = 0;
    EFI_FILE_HANDLE root = 0, file = 0;
    if (bs->HandleProtocol(image, &loaded_guid, (void **)&loaded) ||
        bs->HandleProtocol(loaded->DeviceHandle, &fs_guid, (void **)&fs) ||
        fs->OpenVolume(fs, &root) ||
        root->Open(root, &file, L"\\KERNEL.ELF", EFI_FILE_MODE_READ, 0))
        die("UEFI:FILE\n");
    UINTN length = sizeof(file_buffer);
    if (file->Read(file, &length, file_buffer))
        die("UEFI:READ\n");
    file->Close(file);
    root->Close(root);
    uint8_t *e = file_buffer;
    if (!elf_valid(e, length))
        die("UEFI:ELF_VALIDATION\n");
    if (length < 64 || *(uint32_t *)e != 0x464c457f || e[4] != 2 || e[5] != 1 ||
        *(uint16_t *)(e + 18) != 62 || *(uint16_t *)(e + 54) != 56)
        die("UEFI:ELF\n");
    uint64_t po = *(uint64_t *)(e + 32), entry = *(uint64_t *)(e + 24), end = 0x100000;
    uint16_t count = *(uint16_t *)(e + 56);
    if (count > 32 || po > length || count * 56 > length - po)
        die("UEFI:PHDR\n");
    for (unsigned i = 0; i < count; i++) {
        uint8_t *p = e + po + i * 56;
        if (*(uint32_t *)p != 1)
            continue;
        uint64_t off = *(uint64_t *)(p + 8), v = *(uint64_t *)(p + 16), a = *(uint64_t *)(p + 24),
                 f = *(uint64_t *)(p + 32), m = *(uint64_t *)(p + 40);
        if (a < 0x100000 || m > 0x1f00000 || a > 0x2000000 - m || f > m || off > length ||
            f > length - off || v != KBASE + a)
            die("UEFI:SEGMENT\n");
        if (a + m > end)
            end = a + m;
    }
    if (entry < KBASE + 0x100000 || entry >= KBASE + end)
        die("UEFI:ENTRY\n");
    EFI_PHYSICAL_ADDRESS k = 0x100000;
    if (bs->AllocatePages(AllocateAddress, EfiLoaderData, (end - k + 4095) / 4096, &k))
        die("UEFI:ALLOC_KERNEL\n");
    for (unsigned i = 0; i < count; i++) {
        uint8_t *p = e + po + i * 56;
        if (*(uint32_t *)p != 1)
            continue;
        uint64_t a = *(uint64_t *)(p + 24), f = *(uint64_t *)(p + 32), m = *(uint64_t *)(p + 40);
        memset((void *)a, 0, m);
        memcpy((void *)a, e + *(uint64_t *)(p + 8), f);
    }
    EFI_PHYSICAL_ADDRESS arena = 0xffffffff;
    if (bs->AllocatePages(AllocateMaxAddress, EfiLoaderData, 32, &arena))
        die("UEFI:ARENA\n");
    memset((void *)arena, 0, 32 * 4096);
    struct BootInfo *b = (void *)arena;
    b->magic = BOOT_MAGIC;
    b->version = 1;
    b->size = sizeof(*b);
    b->firmware = 1;
    b->kernel_file_size = length;
    b->kernel_start = 0x100000;
    b->kernel_end = end;
    b->loader_start = arena;
    b->loader_end = arena + 32 * 4096;
    b->stack_start = arena + 16 * 4096;
    b->stack_end = arena + 32 * 4096;
    b->tables_start = arena + 4096;
    b->tables_end = arena + 8 * 4096;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;
    if (bs->LocateProtocol(&gop_guid, 0, (void **)&gop))
        die("UEFI:GOP\n");
    for (UINT32 i = 0; i < gop->Mode->MaxMode; i++) {
        UINTN s;
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mi = 0;
        if (!gop->QueryMode(gop, i, &s, &mi)) {
            int preferred = mi->HorizontalResolution == 1024 && mi->VerticalResolution == 768 &&
                            mi->PixelFormat < 2;
            bs->FreePool(mi);
            if (preferred) {
                if (gop->SetMode(gop, i))
                    die("UEFI:MODE\n");
                break;
            }
        }
    }
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mi = gop->Mode->Info;
    if (mi->PixelFormat > 1)
        die("UEFI:PIXEL_FORMAT\n");
    b->fb = (struct Framebuffer){
        .address = gop->Mode->FrameBufferBase,
        .width = mi->HorizontalResolution,
        .height = mi->VerticalResolution,
        .pitch = mi->PixelsPerScanLine * 4,
        .bpp = 32,
        .red_shift = mi->PixelFormat == PixelRedGreenBlueReserved8BitPerColor ? 0 : 16,
        .red_bits = 8,
        .green_shift = 8,
        .green_bits = 8,
        .blue_shift = mi->PixelFormat == PixelRedGreenBlueReserved8BitPerColor ? 16 : 0,
        .blue_bits = 8};
    uint64_t *t = (void *)b->tables_start;
    t[0] = (b->tables_start + 4096) | 3;
    t[256] = t[0];
    t[511] = (b->tables_start + 8192) | 3;
    t[1024 + 510] = (b->tables_start + 12288) | 3;
    for (unsigned g = 0; g < 4; g++) {
        t[512 + g] = (b->tables_start + 12288 + g * 4096) | 3;
        for (unsigned j = 0; j < 512; j++)
            t[1536 + g * 512 + j] = ((uint64_t)g * 512 + j) * 0x200000 + 0x83;
    }
    for (unsigned attempt = 0; attempt < 8; attempt++) {
        UINTN size = sizeof(map_buffer), key, ds;
        UINT32 version;
        if (bs->GetMemoryMap(&size, (void *)map_buffer, &key, &ds, &version))
            die("UEFI:MAP\n");
        b->map_count = 0;
        for (UINTN off = 0; off < size; off += ds) {
            EFI_MEMORY_DESCRIPTOR *d = (void *)(map_buffer + off);
            uint32_t type = (d->Type == EfiConventionalMemory || d->Type == EfiLoaderCode ||
                             d->Type == EfiLoaderData || d->Type == EfiBootServicesCode ||
                             d->Type == EfiBootServicesData)
                                ? 1
                                : 2;
            if (b->map_count && b->map[b->map_count - 1].type == type &&
                b->map[b->map_count - 1].base + b->map[b->map_count - 1].length ==
                    d->PhysicalStart) {
                b->map[b->map_count - 1].length += d->NumberOfPages * 4096;
            } else {
                if (b->map_count == MAP_MAX)
                    die("UEFI:MAP_CAPACITY\n");
                b->map[b->map_count++] =
                    (struct MemoryRange){d->PhysicalStart, d->NumberOfPages * 4096, type, 0};
            }
        }
        EFI_STATUS status = bs->ExitBootServices(image, key);
        if (!status) {
            serial("UEFI:EXIT\n");
            enter_kernel(b->tables_start, entry, arena, b->stack_end);
        }
        if (status != EFI_INVALID_PARAMETER)
            die("UEFI:EXIT_FAILED\n");
    }
    die("UEFI:EXIT_RETRY\n");
    return EFI_LOAD_ERROR;
}
