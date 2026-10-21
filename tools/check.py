#!/usr/bin/env python3
"""Run source formatting and static analysis; fail on unsuppressed diagnostics."""
import pathlib
import subprocess
root=pathlib.Path(__file__).resolve().parents[1]
files=[str(p.relative_to(root)) for directory in ['include','kernel','lib','boot/bios','boot/uefi','tests'] for p in (root/directory).iterdir() if p.suffix in ['.c','.h']]
def run(args):subprocess.run(args,cwd=root,check=True)
run(['clang-format','--dry-run','--Werror',*sorted(files)])
sources=sorted(str(p.relative_to(root)) for directory in ['kernel','lib'] for p in (root/directory).glob('*.c'))
run(['clang-tidy','-p','build/debug',*sources])
# cppcheck 2.7 supports the C11 grammar, also valid for the C17 sources here.
run(['cppcheck','--enable=warning,style,performance,portability','--error-exitcode=1',
     '--inline-suppr','--library=tools/cppcheck.cfg','--std=c11','--platform=unix64',
     '-Iinclude','--suppress=missingIncludeSystem','kernel','lib'])
print('STATIC CHECKS:PASS')
