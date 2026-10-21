#!/usr/bin/env python3
"""Exercise streaming ELF metadata and cross-sector/chunk payloads on both firmware paths."""
import hashlib,json,pathlib,shutil,struct,subprocess,time
ROOT=pathlib.Path(__file__).resolve().parents[1]
source=ROOT/'build/debug'
original=(source/'kernel.elf').read_bytes()
phoff=struct.unpack_from('<Q',original,32)[0];phnum=struct.unpack_from('<H',original,56)[0]
programs=bytearray(original[phoff:phoff+phnum*56])
stream=bytearray(original)
for i in range(phnum):
    if struct.unpack_from('<I',programs,i*56)[0]!=1:continue
    offset=struct.unpack_from('<Q',programs,i*56+8)[0];size=struct.unpack_from('<Q',programs,i*56+32)[0]
    target=(len(stream)+4095)&~4095
    target=max(target,0x80000)
    stream.extend(bytes(target+size-len(stream)))
    stream[target:target+size]=original[offset:offset+size]
    struct.pack_into('<Q',programs,i*56+8,target)
stub=ROOT/'build/loader-bss.bin'
entry=struct.unpack_from('<Q',original,24)[0]
physical=max(struct.unpack_from('<Q',programs,i*56+24)[0]+struct.unpack_from('<Q',programs,i*56+40)[0] for i in range(phnum) if struct.unpack_from('<I',programs,i*56)[0]==1)
physical=(physical+4095)&~4095
base=0xffffffff80000000+physical
subprocess.run(['nasm','-f','bin',f'-DENTRY={entry}',f'-DBASE={base}',str(ROOT/'tests/loader_bss.asm'),'-o',str(stub)],check=True)
code=stub.read_bytes();target=(len(stream)+4095)&~4095
stream.extend(bytes(target+len(code)-len(stream)));stream[target:target+len(code)]=code
programs.extend(struct.pack('<IIQQQQQQ',1,5,target,base,physical,len(code),8192,4096))
struct.pack_into('<H',stream,56,phnum+1)
struct.pack_into('<Q',stream,24,base)
struct.pack_into('<Q',stream,32,2032)
stream[2032:2032+len(programs)]=programs
cases={'stream-boundaries':bytes(stream),'invalid-elf':b'BAD!'+original[4:],'truncated-segment':bytes(stream[:0x80010]),'oversize-elf':original+bytes(0x400001-len(original)),'limit-exact':original+bytes(0x400000-len(original))}
results=[]
for name,data in cases.items():
    build=ROOT/'build/loader-tests'/name;build.mkdir(parents=True,exist_ok=True)
    for f in ['stage1.bin','stage2.bin','BOOTX64.EFI']:shutil.copyfile(source/f,build/f)
    (build/'kernel.elf').write_bytes(data)
    packaged=subprocess.run(['python3','tools/image.py',str(build)],cwd=ROOT,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    assert packaged.returncode==(1 if name=='oversize-elf' else 0),(name,packaged.returncode)
    for machine in ['bios','uefi']:
        log=build/(machine+'.serial.log');log.write_text('')
        cmd=['qemu-system-x86_64','-m','256M','-cdrom',str(build/'LamOS.iso'),'-display','none','-serial',f'file:{log}','-no-reboot','-no-shutdown']
        if machine=='uefi':cmd+=['-bios','/usr/share/OVMF/OVMF_CODE.fd']
        with (build/(machine+'.emulator.log')).open('w') as output:
            process=subprocess.Popen(cmd,stdout=output,stderr=subprocess.STDOUT)
            try:
                end=time.monotonic()+35
                expected='READY:LamOS' if name in ['stream-boundaries','limit-exact'] else (('BIOS:ERROR' if machine=='bios' else 'UEFI:SIZE') if name=='oversize-elf' else ('BIOS:ELF_ERROR' if machine=='bios' else 'UEFI:ELF'))
                while expected not in log.read_text(errors='replace') and time.monotonic()<end:time.sleep(.05)
                text=log.read_text(errors='replace');assert expected in text,(name,machine,text)
                assert 'PANIC:' not in text
                if name=='stream-boundaries':assert 'BSS:OK' in text and text.index('BSS:OK')<text.index('KERNEL:ENTRY'),text
                results.append({'case':name,'machine':machine,'pass':True,'iso_sha256':hashlib.sha256((build/'LamOS.iso').read_bytes()).hexdigest()})
                print('PASS loader',name,machine,flush=True)
            finally:process.terminate();process.wait(timeout=3)
# A valid directory/header advertises payload sectors missing from the optical medium.
# The UEFI ESP remains intact; this deliberately tests BIOS INT13 failure handling.
from iso9660 import kernel_extent
build=ROOT/'build/loader-tests/short-read';build.mkdir(parents=True,exist_ok=True)
raw=(ROOT/'build/loader-tests/stream-boundaries/LamOS.iso').read_bytes()
offset,_=kernel_extent(raw)
(build/'LamOS.iso').write_bytes(raw[:offset+0x80800])
log=build/'bios.serial.log';log.write_text('')
with (build/'emulator.log').open('w') as output:
    process=subprocess.Popen(['qemu-system-x86_64','-m','256M','-cdrom',str(build/'LamOS.iso'),'-display','none','-serial',f'file:{log}','-no-reboot','-no-shutdown'],stdout=output,stderr=subprocess.STDOUT)
    try:
        end=time.monotonic()+30
        while 'BIOS:ERROR' not in log.read_text(errors='replace') and time.monotonic()<end:time.sleep(.05)
        assert 'BIOS:ERROR' in log.read_text(errors='replace'),log.read_text(errors='replace')
        results.append({'case':'short-read','machine':'bios','pass':True,'iso_sha256':hashlib.sha256((build/'LamOS.iso').read_bytes()).hexdigest()})
        print('PASS loader physical short read bios',flush=True)
    finally:process.terminate();process.wait(timeout=3)
(ROOT/'build/loader-results.json').write_text(json.dumps(results,indent=2)+'\n')
