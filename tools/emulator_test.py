#!/usr/bin/env python3
"""Exercise the actual desktop/input/compositor and collect artifact-bound evidence."""
import argparse
import hashlib
import json
import pathlib
import re
import socket
import struct
import subprocess
import time
from rfb import Rfb
ROOT=pathlib.Path(__file__).resolve().parents[1]

def run(args):
    build=(ROOT/args.build).resolve()
    suffix=args.machine if not args.label else args.machine+'-'+args.label
    out=build/'test-results'/suffix;out.mkdir(parents=True,exist_ok=True)
    serial=out/'serial.log';serial.write_text('')
    port={'bochs':5900,'bios':5907,'uefi':5908}[args.machine]
    if args.machine=='bochs':
        config=out/'bochsrc';config.write_text(f'''megs: 256
romimage: file=/usr/share/seabios/bios.bin
vgaromimage: file=/usr/share/seabios/vgabios-stdvga.bin
cpu: model=core2_penryn_t9600, ips=50000000
pci: enabled=1, chipset=i440fx, slot1=pcivga
vga: extension=vbe
ata0-master: type=cdrom, path={build/'LamOS.iso'}, status=inserted
boot: cdrom
display_library: rfb, options="timeout=30"
mouse: type=ps2, enabled=0, toggle=f12
com1: enabled=1, mode=file, dev={serial}
log: {out/'bochs.log'}
panic: action=fatal
error: action=report
clock: sync=none
''');rc=out/'debugger.rc';rc.write_text('c\n')
        cmd=[str(ROOT/'.tools/bochs-src/bochs/bochs'),'-q','-f',str(config),'-rc',str(rc)]
    else:
        cmd=['qemu-system-x86_64','-m','256M','-cdrom',str(build/'LamOS.iso'),'-vnc',f'127.0.0.1:{port-5900}','-serial',f'file:{serial}','-no-reboot','-no-shutdown']
        if args.machine=='uefi':cmd+=['-bios','/usr/share/OVMF/OVMF_CODE.fd']
    start=time.monotonic();deadline=start+args.timeout
    with (out/'emulator.log').open('w') as logfile:
        process=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=logfile,stderr=subprocess.STDOUT)
        rfb=None
        try:
            while rfb is None and time.monotonic()<deadline:
                if process.poll() is not None:raise RuntimeError((out/'emulator.log').read_text())
                try:rfb=Rfb(port)
                except (ConnectionRefusedError,socket.timeout):time.sleep(.05)
            assert rfb,'RFB startup timeout'
            def log():return serial.read_text(errors='replace')
            last_request=0
            def wait(seconds=.1):
                nonlocal last_request
                end=time.monotonic()+seconds
                while time.monotonic()<end:
                    if time.monotonic()>deadline:raise TimeoutError('Global emulator deadline exceeded')
                    if time.monotonic()-last_request>(.5 if args.machine=='bochs' else .2):
                        rfb.request();last_request=time.monotonic()
                    rfb.pump()
            def until(text,timeout=20):
                end=min(deadline,time.monotonic()+timeout)
                while text not in log() and time.monotonic()<end:wait(.05)
                assert text in log(),f'Missing {text}: {log()[-2500:]}'
            until(args.expect or 'READY:LamOS',120)
            if args.expect:
                wait(1);rfb.screenshot(out/'fault.ppm')
                if 'graphics panic' in args.expect:
                    pixel=rfb.region(10,10,1,1)
                    assert pixel[0]>70 and pixel[0]>pixel[1]*2,'Panic background missing'
                result={'machine':args.machine,'fault':args.expect,'pass':True}
                (out/'result.json').write_text(json.dumps(result,indent=2));print('PASS',suffix,args.expect,flush=True);return
            for marker in ['BOOT:OK','LOG:OK','ARCH:OK','PMM:OK','VMM:OK','HEAP:OK','TIMER:OK','KEYBOARD:OK','GRAPHICS:OK','MOUSE:OK','GUI:READY']:
                assert marker in log(),marker
            assert 'windows=0' in log()
            wait(.5);rfb.screenshot(out/'desktop-empty.ppm')
            offset=32 if args.machine=='bochs' else 0
            width=rfb.w;height=rfb.h-50 if args.machine=='bochs' else rfb.h
            def key(k):rfb.key(k,1);wait(.045);rfb.key(k,0);wait(.045)
            def hotkey(modifiers,keycode):
                for m in modifiers:rfb.key(m,1)
                wait(.04);key(keycode)
                for m in reversed(modifiers):rfb.key(m,0)
                wait(.05)
            def type_text(text):
                for ch in text:key(ord(ch))
            if args.machine=='bochs':key(0xffc9)
            cx=3000;cy=3000;buttons=0
            rfb.pointer(cx,cy);wait(.1);cx+=2;rfb.pointer(cx,cy);wait(.15)
            def cursor():
                matches=re.findall(r'GUI:POINTER a=(\d+) b=(\d+)',log())
                assert matches,'No mouse input after RFB initialization'
                return tuple(map(int,matches[-1]))
            def move(x,y):
                nonlocal cx,cy
                for _ in range(60):
                    gx,gy=cursor();dx=max(-60,min(60,x-gx));dy=max(-60,min(60,y-gy))
                    if not dx and not dy:return
                    # A half-gain controller tolerates delayed PS/2 delivery in slow UBSan frames.
                    scale=1
                    cx+=dx*scale;cy+=dy*scale
                    assert 0<cx<65535 and 0<cy<65535
                    before=len(re.findall(r'GUI:POINTER',log()))
                    rfb.pointer(cx,cy,buttons)
                    end=time.monotonic()+5
                    while len(re.findall(r'GUI:POINTER',log()))<=before and time.monotonic()<end:wait(.05)
                    wait(.15 if args.machine=='bochs' else .02)
                raise AssertionError(('Pointer failed to converge',cursor(),(x,y)))
            def press(value):
                nonlocal buttons
                before=len(re.findall(r'GUI:POINTER',log()))
                buttons=value;rfb.pointer(cx,cy,buttons)
                end=time.monotonic()+10
                while len(re.findall(r'GUI:POINTER',log()))<=before and time.monotonic()<end:wait(.05)
                wait(.25 if args.machine=='bochs' else .06)
            def click(x,y):move(x,y);press(1);press(0)
            def ids():return [(int(a),int(b)) for a,b in re.findall(r'GUI:OPEN a=(\d+) b=(\d+)',log())]
            def window(id):
                end=time.monotonic()+10
                while True:
                    matches=re.findall(rf'GUI:WINDOW id={id} kind=\d+ x=(\d+) y=(\d+) w=(\d+) h=(\d+) minimized=(\d+) focus=(\d+)',log())
                    if matches or time.monotonic()>=end:break
                    wait(.05)
                assert matches,('Missing window geometry',id)
                return tuple(map(int,matches[-1]))
            def window_state(id,predicate):
                end=time.monotonic()+20
                while True:
                    state=window(id)
                    if predicate(state):return state
                    if time.monotonic()>=end:raise AssertionError(('Window state timeout',id,state))
                    wait(.05)
            def capture(name):wait(.45);rfb.request();wait(.3);rfb.screenshot(out/(name+'.ppm'))
            def open_terminal():
                before=len(ids());hotkey([0xffe3,0xffe9],ord('t'))
                end=time.monotonic()+10
                while len(ids())<=before and time.monotonic()<end:wait(.1)
                assert len(ids())==before+1,'Terminal shortcut failed'
                return ids()[-1][0]
            first=open_terminal();until(f'GUI:OPEN a={first}')
            type_text('helxp');key(0xff51);key(0xff08);key(0xff0d);until('text=help')
            # Read rendered cells using the actual embedded font, independent of firmware colors.
            glyphs=[]
            for line in (ROOT/'lib/font_data.inc').read_text().splitlines():
                values=re.findall(r'0x([0-9a-f]{2})',line)
                if len(values)==16:glyphs.append(bytes(int(v,16) for v in values))
            def terminal_text(id):
                x,y,w,h,_,_=window(id);x+=8;y+=36+offset;cols=(w-16)//8;rows=(h-44)//16
                lines=[]
                for row in range(rows):
                    chars=[]
                    for col in range(cols):
                        xx=x+col*8;yy=y+row*16;bg=rfb.region(xx,yy,1,1);bitmap=[]
                        for gy in range(14):
                            bits=0
                            for gx in range(8):
                                pixel=rfb.region(xx+gx,yy+gy,1,1)
                                if max(abs(pixel[i]-bg[i]) for i in range(3))>50:bits|=128>>gx
                            bitmap.append(bits)
                        scores=[sum((a^b).bit_count() for a,b in zip(bitmap,glyph[:14])) for glyph in glyphs]
                        chars.append(chr(32+min(range(95),key=scores.__getitem__)))
                    lines.append(''.join(chars))
                return '\n'.join(lines)
            def visible(id,text):
                end=time.monotonic()+15
                while time.monotonic()<end:
                    wait(.2)
                    rendered=terminal_text(id)
                    if text in rendered:return
                (out/'ocr.txt').write_text(rendered)
                raise AssertionError('Rendered terminal output missing: '+text)
            visible(first,'Commands:');capture('terminal-help')
            if args.layout_only:
                type_text('sysinfo');key(0xff0d);visible(first,'LamOS');capture('layout-terminal')
                assert 'PANIC:' not in log() and 'UBSAN:' not in log()
                result={'machine':args.machine,'pass':True,'width':width,'height':height,'layout':True,'iso_sha256':hashlib.sha256((build/'LamOS.iso').read_bytes()).hexdigest()}
                (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS layout',suffix,width,height,flush=True);return

            for command in ['sysinfo','meminfo','uptime']:
                type_text(command);key(0xff0d);until('text='+command)
            key(0xff52);key(0xff54)  # history and draft restoration
            type_text('helxp');key(0xff50);key(0xff53);key(0xff53);key(0xff53);key(0xffff);key(0xff0d)
            until('text=help')
            # Scrollback is populated through actual shell commands.
            for _ in range(5):type_text('help');key(0xff0d)
            key(0xff55);capture('terminal-scrollback');key(0xff56)
            type_text('clear');key(0xff0d);until('text=clear');type_text('sysinfo');key(0xff0d)
            visible(first,'LamOS');capture('terminal-sysinfo')
            # Launcher keyboard navigation opens Folder; a click opens Settings.
            click(30,height-16);key(0xff54);key(0xff0d);wait(.2)
            folder=next(id for id,kind in ids() if kind==1)
            fx,fy,_,_,_,_=window(folder);click(fx+30,fy+134);capture('folder')
            click(30,height-16);click(90,height-164+6+2*40+18);wait(.2)
            settings=next(id for id,kind in ids() if kind==2)
            sx,sy,_,_,_,_=window(settings)
            click(sx+8+150,sy+36+44);until('GUI:THEME a=1')
            click(sx+8+120,sy+36+120);until('GUI:THEME a=1 b=1')
            click(sx+8+8,sy+36+166);until('GUI:PROFILING a=1');capture('settings-light')
            click(sx+8+40,sy+36+44);click(sx+8+40,sy+36+120)
            # Switch back to Terminal and drag its titlebar with an outline.
            hotkey([0xffe9],0xff09);wait(.15)
            x,y,w,h,_,_=window(first);move(x+50,y+14);press(1);move(min(width-w,x+48)+50,min(height-32-h,y+32)+14)
            assert window(first)[:4]==(x,y,w,h),'Move committed before release'
            capture('move-outline');press(0);wait(.2)
            nx,ny,nw,nh,_,_=window_state(first,lambda r:r[:2]!=(x,y))
            move(nx+nw-2,ny+nh-2);press(1);move(min(width-2,nx+nw+30),min(height-34,ny+nh+22));capture('resize-outline');press(0);wait(.2)
            x,y,w,h,_,_=window_state(first,lambda r:r[2]!=nw or r[3]!=nh)
            click(x+w-68,y+14);window_state(first,lambda r:r[4]==1);click(126,height-16);window_state(first,lambda r:r[4]==0 and r[5]==first)
            x,y,w,h,_,_=window(first);click(x+w-42,y+14);wait(.2)
            x,y,w,h,_,_=window_state(first,lambda r:r[2]==width and r[3]==height-32)
            click(x+w-42,y+14);wait(.2);capture('desktop-windows')
            # Close everything and prove exact heap restoration before stress.
            for _ in range(len(ids())):hotkey([0xffe9],0xffc1)
            until('GUI:MEM live=3')
            baseline=re.search(r'GUI:BASELINE live=(\d+) used=(\d+) largest=(\d+) free_frames=(\d+)',log()).groups()
            def assert_no_leak():
                matches=re.findall(r'GUI:MEM live=(\d+) used=(\d+) largest=(\d+) free_frames=(\d+)',log());assert matches and matches[-1]==baseline,('Heap/frame leak',baseline,matches[-1] if matches else None)
            assert_no_leak()
            stress_start=time.monotonic();cycles=0
            while time.monotonic()-stress_start<args.stress_seconds:
                id=open_terminal();x,y,w,h,_,_=window(id)
                move(x+45,y+14);press(1);move(min(width-w,x+12)+45,min(height-32-h,y+10)+14);press(0)
                x,y,w,h,_,_=window(id);move(x+w-2,y+h-2);press(1);move(min(width-2,x+w+6),min(height-34,y+h+6));press(0)
                type_text('uptime');key(0xff0d)
                before=len(re.findall(r'GUI:MEM live=',log()))
                hotkey([0xffe9],0xffc1)
                end=time.monotonic()+15
                while len(re.findall(r'GUI:MEM live=',log()))<=before and time.monotonic()<end:wait(.05)
                assert len(re.findall(r'GUI:MEM live=',log()))>before,'Missing stress allocator checkpoint'
                assert_no_leak();cycles+=1
            assert time.monotonic()-stress_start>=args.stress_seconds
            final=open_terminal();type_text('help');key(0xff0d);visible(final,'Commands:');capture('desktop')
            assert 'PANIC:' not in log() and 'UBSAN:' not in log(),log()[-3000:]
            result={'machine':args.machine,'pass':True,'version':(ROOT/'VERSION').read_text().strip(),'boot':True,'keyboard':True,'mouse':True,'framebuffer':True,'window_manager':True,'terminal':True,'shell':True,'desktop':True,'stress_seconds':time.monotonic()-stress_start,'stress_cycles':cycles,'no_leaks':True,'width':width,'height':height,'iso_sha256':hashlib.sha256((build/'LamOS.iso').read_bytes()).hexdigest(),'kernel_sha256':hashlib.sha256((build/'kernel.elf').read_bytes()).hexdigest()}
            names=['allocations','used_bytes','largest_free_block','free_frames']
            result['memory_baseline']=dict(zip(names,map(int,baseline)))
            result['memory_after_stress']=dict(zip(names,map(int,re.findall(r'GUI:MEM live=(\d+) used=(\d+) largest=(\d+) free_frames=(\d+)',log())[-1])))
            result['profiling']=[dict((key,int(value)) for key,value in re.findall(r'(\w+)=(\d+)',line)) for line in log().splitlines() if line.startswith('GUI:PROFILE')]
            (out/'profile.log').write_text('\n'.join(line for line in log().splitlines() if 'GUI:PROFILE' in line or 'GUI:MEM' in line))
            (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(f'PASS {suffix}: GUI/input/pixels, stress={cycles} cycles, no leaks',flush=True)
        except Exception as error:
            if rfb:
                try:rfb.screenshot(out/'failure.ppm')
                except Exception:pass
            (out/'result.json').write_text(json.dumps({'machine':args.machine,'pass':False,'error':str(error)},indent=2));raise
        finally:
            if rfb:rfb.s.close()
            process.terminate()
            try:process.wait(timeout=3)
            except subprocess.TimeoutExpired:process.kill();process.wait()
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',default='build/debug');p.add_argument('--machine',choices=['bios','uefi','bochs'],default='bios');p.add_argument('--timeout',type=float,default=600);p.add_argument('--stress-seconds',type=float,default=60);p.add_argument('--expect');p.add_argument('--label');p.add_argument('--layout-only',action='store_true');run(p.parse_args())
