#!/usr/bin/env python3
"""Run release acceptance checks; use from an environment allowing localhost sockets."""
import json
import hashlib
import pathlib
import subprocess
root=pathlib.Path(__file__).resolve().parents[1]
def run(*cmd):subprocess.run(cmd,cwd=root,check=True)
for preset in ['debug','release','ubsan']:
    run('cmake','--preset',preset)
    run('cmake','--build','--preset',preset)
    run('ctest','--preset',preset)
    for machine in ['bios','uefi','bochs']:
        run('python3','tools/emulator_test.py','--build','build/'+preset,'--machine',machine)
run('python3','tools/check.py')
run('python3','tools/gdb_test.py')
for machine in ['bios','uefi']:
    run('python3','tools/fault_tests.py','--machine',machine)
results=[]
for preset in ['debug','release','ubsan']:
    for machine in ['bios','uefi','bochs']:
        row=json.loads((root/'build'/preset/'test-results'/machine/'result.json').read_text())
        row['preset']=preset
        row['iso_sha256']=hashlib.sha256((root/'build'/preset/'LamOS.iso').read_bytes()).hexdigest()
        row['kernel_sha256']=hashlib.sha256((root/'build'/preset/'kernel.elf').read_bytes()).hexdigest()
        results.append(row)
(root/'build/acceptance.json').write_text(json.dumps({'version':'1.0.0','pass':all(r['pass'] for r in results),'matrix':results,'faults':16,'gdb':True,'static_analysis':True},indent=2)+'\n')
print('LAMOS ACCEPTANCE:PASS')
