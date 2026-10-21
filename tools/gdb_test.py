#!/usr/bin/env python3
"""Verify source symbols, a hardware entry breakpoint and BootInfo over QEMU gdbstub."""
import pathlib
import subprocess
root=pathlib.Path(__file__).resolve().parents[1]
out=root/'build/debug/test-results/gdb'
out.mkdir(parents=True,exist_ok=True)
with (out/'emulator.log').open('w') as log:
    p=subprocess.Popen(['qemu-system-x86_64','-m','256M','-cdrom',str(root/'build/debug/LamOS.iso'),'-display','none','-serial',f'file:{out/"serial.log"}','-no-reboot','-no-shutdown','-S','-gdb','tcp:127.0.0.1:1234'],stdout=log,stderr=subprocess.STDOUT)
    try:
        result=subprocess.run(['gdb','-q','-batch',str(root/'build/debug/kernel.elf'),
          '-ex','set architecture i386:x86-64','-ex','target remote :1234',
          '-ex','hbreak kmain','-ex','continue',
          '-ex','set $boot=(struct BootInfo *)$rdi',
          '-ex','printf "GDB:MAGIC=%lx VERSION=%u\\n", $boot->magic, $boot->version',
          '-ex','detach'],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=20)
        (out/'gdb.log').write_text(result.stdout)
        assert result.returncode==0 and 'GDB:MAGIC=314f534f4d414c VERSION=1' in result.stdout,result.stdout
        print('GDB:PASS breakpoint, symbols, BootInfo')
    finally:
        p.terminate();p.wait(timeout=3)
