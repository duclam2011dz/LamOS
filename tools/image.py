#!/usr/bin/env python3
"""Build the dual-firmware ISO from a single kernel artifact."""
import hashlib
import pathlib
import shutil
import subprocess
import sys
from iso9660 import kernel_extent
b = pathlib.Path(sys.argv[1]).resolve()
stage = b / 'iso-root'
(stage / 'boot').mkdir(parents=True, exist_ok=True)
shutil.copyfile(b / 'kernel.elf', stage / 'KERNEL.ELF')
(stage / 'boot/bios.img').write_bytes((b / 'stage1.bin').read_bytes() + (b / 'stage2.bin').read_bytes())
esp = stage / 'boot/esp.img'
with esp.open('wb') as f:
    f.truncate(16 * 1024 * 1024)
def run(*args):
    subprocess.run(args, check=True)
run('mformat', '-i', str(esp), '::')
run('mmd', '-i', str(esp), '::/EFI', '::/EFI/BOOT')
run('mcopy', '-i', str(esp), str(b / 'BOOTX64.EFI'), '::/EFI/BOOT/BOOTX64.EFI')
run('mcopy', '-i', str(esp), str(b / 'kernel.elf'), '::/KERNEL.ELF')
embedded = subprocess.check_output(['mtype', '-i', str(esp), '::/KERNEL.ELF'])
assert embedded == (stage / 'KERNEL.ELF').read_bytes()
run('xorriso', '-as', 'mkisofs', '-R', '-V', 'LAMOS', '-b', 'boot/bios.img', '-no-emul-boot', '-boot-load-size', '4', '-boot-info-table', '-eltorito-alt-boot', '-e', 'boot/esp.img', '-no-emul-boot', '-o', str(b / 'LamOS.iso'), str(stage))
iso = b / 'LamOS.iso'
offset,size=kernel_extent(iso.read_bytes())
assert iso.read_bytes()[offset:offset+size] == (b/'kernel.elf').read_bytes()
(b / 'SHA256SUMS').write_text(hashlib.sha256(iso.read_bytes()).hexdigest() + '  LamOS.iso\n')
