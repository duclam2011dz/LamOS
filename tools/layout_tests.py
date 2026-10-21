#!/usr/bin/env python3
"""Build reproducible alternate VBE/GOP preferences and check rendered layouts."""
import json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[1]
results=[]
for width,height,bpp in [(640,480,32),(1280,1024,32),(1024,768,24)]:
    build=f'build/layout-{width}x{height}-{bpp}'
    subprocess.run(['cmake','-S','.', '-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Debug','-DCMAKE_C_COMPILER=x86_64-elf-gcc',f'-DVIDEO_WIDTH={width}',f'-DVIDEO_HEIGHT={height}',f'-DVIDEO_BPP={bpp}'],cwd=root,check=True,stdout=subprocess.DEVNULL)
    subprocess.run(['cmake','--build',build],cwd=root,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    for machine in (['bios','uefi'] if bpp==32 else ['bios']):
        subprocess.run(['python3','tools/emulator_test.py','--build',build,'--machine',machine,'--layout-only'],cwd=root,check=True)
        row=json.loads((root/build/'test-results'/machine/'result.json').read_text())
        assert row['width']==width and row['height']==height,row
        serial=(root/build/'test-results'/machine/'serial.log').read_text()
        assert f'bpp={bpp}' in serial,serial
        row['bpp']=bpp;results.append(row)
(root/'build/layout-results.json').write_text(json.dumps(results,indent=2)+'\n')
