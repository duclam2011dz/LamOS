#!/usr/bin/env python3
"""Check boot-image packaging and reject malformed ISO metadata."""
import pathlib
import struct
import sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from iso9660 import kernel_extent

def rejected(data):
    try:kernel_extent(data)
    except ValueError:return
    raise AssertionError('Malformed ISO accepted')
data=pathlib.Path(sys.argv[1]).read_bytes()
offset,size=kernel_extent(data)
assert data[offset:offset+4]==b'\x7fELF'
rejected(data[:100])
bad=bytearray(data);bad[16*2048+1]=0;rejected(bad)
bad=bytearray(data);struct.pack_into('<I',bad,16*2048+166,4096);rejected(bad)
rejected(data[:offset+size-1])
bad=bytearray(data);pos=bad.index(b'KERNEL.ELF;1',19*2048);bad[pos]=ord('X');rejected(bad)
print('PASS ISO descriptor, root bounds, kernel presence and truncation')
