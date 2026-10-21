#!/usr/bin/env python3
"""Build isolated fault images and verify exceptions halt with useful diagnostics."""
import argparse
import json
import pathlib
import subprocess
import time
ROOT = pathlib.Path(__file__).resolve().parents[1]
CASES = {'panic':'PANIC:Injected panic','divide':'EXCEPTION:0 Divide error',
         'opcode':'EXCEPTION:6 Invalid opcode','page':'EXCEPTION:14 Page fault',
         'double':'EXCEPTION:8 Double fault','ubsan':'UBSAN:add_overflow',
         'write':'PAGEFAULT CR2=ffffffff80100000 present=1 write=1',
         'nx':'execute=1'}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--machine',choices=['bios','uefi'],default='bios')
    args=parser.parse_args()
    results=[]
    for name,expected in CASES.items():
        build=ROOT/'build'/f'fault-{name}'
        subprocess.run(['cmake','--preset',f'fault-{name}'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['cmake','--build',str(build)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        out=build/'test-results'/args.machine
        out.mkdir(parents=True,exist_ok=True)
        serial=out/'serial.log';serial.write_text('')
        cmd=['qemu-system-x86_64','-m','256M','-cdrom',str(build/'LamOS.iso'),'-display','none','-serial',f'file:{serial}','-no-reboot','-no-shutdown']
        if args.machine=='uefi':cmd+=['-bios','/usr/share/OVMF/OVMF_CODE.fd']
        with (out/'emulator.log').open('w') as log:
            p=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT)
            try:
                deadline=time.monotonic()+20
                text=''
                while time.monotonic()<deadline:
                    text=serial.read_text(errors='replace')
                    if expected in text and 'PANIC:' in text:break
                    if p.poll() is not None:break
                    time.sleep(.05)
                assert expected in text and 'PANIC:' in text,(name,text)
                time.sleep(.2)
                assert serial.read_text(errors='replace').count('KERNEL:ENTRY')==1
                assert p.poll() is None,'Guest unexpectedly exited instead of halting'
                results.append({'case':name,'machine':args.machine,'pass':True,'expected':expected})
                (out/'result.json').write_text(json.dumps(results[-1],indent=2)+'\n')
                print(f'PASS {args.machine}: {name}',flush=True)
            finally:
                p.terminate();p.wait(timeout=3)
    dest=ROOT/'build'/f'fault-results-{args.machine}.json'
    dest.write_text(json.dumps(results,indent=2)+'\n')
if __name__=='__main__':main()
