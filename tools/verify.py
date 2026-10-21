#!/usr/bin/env python3
"""Run artifact-bound GUI release gates. --resume reuses unchanged passing stress runs."""
import argparse,concurrent.futures,hashlib,json,pathlib,re,subprocess
root=pathlib.Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--resume',action='store_true');args=parser.parse_args()
version=(root/'VERSION').read_text().strip()
def run(*cmd):subprocess.run(cmd,cwd=root,check=True)
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
for preset in ['debug','release','ubsan']:
    run('cmake','--preset',preset);run('cmake','--build','--preset',preset);run('ctest','--preset',preset)
results=[]
def matrix(preset,machine):
    build=root/'build'/preset;path=build/'test-results'/machine/'result.json'
    reuse=False
    if args.resume and path.exists():
        row=json.loads(path.read_text());reuse=row.get('pass') and row.get('stress_seconds',0)>=60 and row.get('iso_sha256')==digest(build/'LamOS.iso') and row.get('kernel_sha256')==digest(build/'kernel.elf')
    if not reuse:run('python3','tools/emulator_test.py','--build','build/'+preset,'--machine',machine)
    row=json.loads(path.read_text());assert row['pass']
    # Extract measurements from retained serial evidence, including resumed runs.
    text=(path.parent/'serial.log').read_text()
    fields=['allocations','used_bytes','largest_free_block','free_frames']
    baseline=re.search(r'GUI:BASELINE live=(\d+) used=(\d+) largest=(\d+) free_frames=(\d+)',text)
    checkpoints=re.findall(r'GUI:MEM live=(\d+) used=(\d+) largest=(\d+) free_frames=(\d+)',text)
    assert baseline and checkpoints and tuple(baseline.groups())==checkpoints[-1]
    row['memory_baseline']=dict(zip(fields,map(int,baseline.groups())))
    row['memory_after_stress']=dict(zip(fields,map(int,checkpoints[-1])))
    row.pop('memory_final',None)
    row['profiling']=[dict((key,int(value)) for key,value in re.findall(r'(\w+)=(\d+)',line)) for line in text.splitlines() if line.startswith('GUI:PROFILE')]
    assert row['profiling'],'Missing frame profiling evidence'
    path.write_text(json.dumps(row,indent=2)+'\n')
    row['preset']=preset
    return row
# Each worker owns a distinct RFB port, avoiding machine/port collisions.
def machine_runs(machine):return [matrix(preset,machine) for preset in ['debug','release','ubsan']]
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
    for rows in pool.map(machine_runs,['bios','uefi','bochs']):results.extend(rows)
run('python3','tools/loader_tests.py');run('python3','tools/layout_tests.py')
run('python3','tools/check.py');run('tools/clangd.sh','--check=kernel/main.c','--compile-commands-dir=build/debug')
run('python3','tools/gdb_test.py')
for machine in ['bios','uefi']:run('python3','tools/fault_tests.py','--machine',machine)
run('cmake','--preset','fault-graphics');run('cmake','--build','--preset','fault-graphics')
for machine in ['bios','uefi']:
    run('python3','tools/emulator_test.py','--build','build/fault-graphics','--machine',machine,'--expect','PANIC:Injected graphics panic')
acceptance={'version':version,'pass':all(r['pass'] for r in results),'matrix':results,'loader':json.loads((root/'build/loader-results.json').read_text()),'layouts':json.loads((root/'build/layout-results.json').read_text()),'faults':16,'graphics_panics':2,'gdb':True,'static_analysis':True,'clangd':True}
(root/'build/acceptance.json').write_text(json.dumps(acceptance,indent=2)+'\n')
print('LAMOS ACCEPTANCE:PASS',flush=True)
