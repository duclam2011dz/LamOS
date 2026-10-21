#!/usr/bin/env python3
"""Run the ISO interactively in QEMU; use emulator_test.py for automated Bochs."""
import argparse
import pathlib
import subprocess
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--uefi',action='store_true')
p.add_argument('--gdb',action='store_true')
p.add_argument('--build',default='build/debug')
a=p.parse_args()
cmd=['qemu-system-x86_64','-m','128M','-cdrom',str(root/a.build/'LamOS.iso'),'-serial','stdio','-no-reboot','-no-shutdown']
if a.uefi:cmd+=['-bios','/usr/share/OVMF/OVMF_CODE.fd']
if a.gdb:cmd+=['-S','-gdb','tcp:127.0.0.1:1234']
raise SystemExit(subprocess.call(cmd))
