"""Validate the deliberately small ISO9660 subset used by the BIOS loader."""
import struct
SECTOR = 2048

def kernel_extent(data):
    if len(data) < 17 * SECTOR or data[16 * SECTOR:16 * SECTOR + 7] != b'\1CD001\1':
        raise ValueError('Invalid primary volume descriptor')
    root = data[16 * SECTOR + 156:16 * SECTOR + 190]
    lba = struct.unpack_from('<I', root, 2)[0]
    length = struct.unpack_from('<I', root, 10)[0]
    if length != SECTOR or lba >= len(data) // SECTOR:
        raise ValueError('BIOS requires a one-sector root directory')
    offset = lba * SECTOR
    end = offset + length
    while offset < end and data[offset]:
        size = data[offset]
        if size < 34 or offset + size > end:
            raise ValueError('Invalid directory record')
        rec = data[offset:offset + size]
        if rec[32] > size - 33:
            raise ValueError('Invalid ISO filename')
        if rec[33:33 + rec[32]] == b'KERNEL.ELF;1':
            lba = struct.unpack_from('<I', rec, 2)[0]
            size = struct.unpack_from('<I', rec, 10)[0]
            if rec[25] & 0x82 or size < 64 or size > 0x50000 or lba * SECTOR + size > len(data):
                raise ValueError('Unsupported or truncated kernel extent')
            return lba * SECTOR, size
        offset += size
    raise ValueError('KERNEL.ELF;1 is missing')
