#!/usr/bin/env python3
"""Boot and exercise the real PS/2 input path; collect serial and RFB screenshots."""
import argparse
import json
import pathlib
import socket
import struct
import subprocess
import time
import zlib
import binascii

ROOT = pathlib.Path(__file__).resolve().parents[1]

class Rfb:
    def __init__(self, port):
        self.s = socket.create_connection(('127.0.0.1', port), timeout=3)
        version = self.read(12)
        self.s.sendall(b'RFB 003.003\n')
        assert version.startswith(b'RFB ')
        assert self.read(4) == b'\0\0\0\1', 'RFB requires authentication'
        self.s.sendall(b'\1')
        header = self.read(24)
        self.w, self.h = struct.unpack('>HH', header[:4])
        self.bpp, _, self.big, self.true = header[4:8]
        self.maxima = struct.unpack('>HHH', header[8:14])
        self.shifts = header[14:17]
        self.read(struct.unpack('>I', header[20:])[0])
        self.pixels = bytearray(self.w * self.h * 3)
        self.s.sendall(struct.pack('>BBHii', 2, 0, 2, 0, -223))
        self.request()
    def read(self, n):
        result = bytearray()
        while len(result) < n:
            part = self.s.recv(n - len(result))
            if not part:
                raise EOFError('RFB connection closed')
            result.extend(part)
        return bytes(result)
    def request(self):
        self.s.sendall(struct.pack('>BBHHHH', 3, 0, 0, 0, self.w, self.h))
    def key(self, key, down):
        self.s.sendall(struct.pack('>BBHI', 4, down, 0, key))
    def pointer(self, x, y, buttons=0):
        self.s.sendall(struct.pack('>BBHH', 5, buttons, x, y))
    def pump(self):
        import select
        if not select.select([self.s], [], [], .02)[0]:
            return
        kind = self.read(1)[0]
        if kind == 0:
            _, count = struct.unpack('>BH', self.read(3))
            for _ in range(count):
                x, y, w, h, encoding = struct.unpack('>HHHHi', self.read(12))
                if encoding == -223:
                    self.w, self.h = w, h
                    self.pixels = bytearray(w * h * 3)
                    continue
                assert encoding == 0, encoding
                size = self.bpp // 8
                data = self.read(w * h * size)
                for yy in range(h):
                    for xx in range(w):
                        off = (yy * w + xx) * size
                        pixel = int.from_bytes(data[off:off + size], 'big' if self.big else 'little')
                        rgb = bytes(((pixel >> shift) & maximum) * 255 // maximum
                                    for shift, maximum in zip(self.shifts, self.maxima))
                        target = ((y + yy) * self.w + x + xx) * 3
                        self.pixels[target:target + 3] = rgb
        elif kind == 1:
            _, _, count = struct.unpack('>BHH', self.read(5))
            self.read(count * 6)
        elif kind == 2:
            pass
        elif kind == 3:
            self.read(3)
            self.read(struct.unpack('>I', self.read(4))[0])
        else:
            raise AssertionError(('RFB message', kind))
    def screenshot(self, path):
        path.write_bytes(f'P6\n{self.w} {self.h}\n255\n'.encode() + self.pixels)
        def chunk(kind, data):
            return struct.pack('>I',len(data))+kind+data+struct.pack('>I',binascii.crc32(kind+data)&0xffffffff)
        raw=b''.join(b'\0'+self.pixels[y*self.w*3:(y+1)*self.w*3] for y in range(self.h))
        png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',self.w,self.h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
        path.with_suffix('.png').write_bytes(png)
    def region(self, x, y, w, h):
        return b''.join(self.pixels[((y + row) * self.w + x) * 3:
                                   ((y + row) * self.w + x + w) * 3] for row in range(h))


def run(args):
    build = (ROOT / args.build).resolve()
    out = build / 'test-results' / args.machine
    out.mkdir(parents=True, exist_ok=True)
    serial = out / 'serial.log'
    serial.write_text('')
    port = {'bochs':5900,'bios':5907,'uefi':5908}[args.machine]
    if args.machine == 'bochs':
        config = out / 'bochsrc'
        config.write_text(f'''megs: 128
romimage: file=/usr/share/seabios/bios.bin
vgaromimage: file=/usr/share/seabios/vgabios-stdvga.bin
cpu: model=core2_penryn_t9600, ips=50000000
pci: enabled=1, chipset=i440fx, slot1=pcivga
vga: extension=vbe
ata0-master: type=cdrom, path={build / 'LamOS.iso'}, status=inserted
boot: cdrom
display_library: rfb, options="timeout=30"
mouse: type=ps2, enabled=0, toggle=f12
com1: enabled=1, mode=file, dev={serial}
log: {out / 'bochs.log'}
panic: action=fatal
error: action=report
clock: sync=none
''')
        rc = out / 'debugger.rc'
        rc.write_text('c\n')
        cmd = [str(ROOT / '.tools/bochs-src/bochs/bochs'), '-q', '-f', str(config), '-rc', str(rc)]
    else:
        cmd = ['qemu-system-x86_64', '-m', '128M', '-cdrom', str(build / 'LamOS.iso'),
               '-vnc', f'127.0.0.1:{port - 5900}', '-serial', f'file:{serial}', '-no-reboot', '-no-shutdown']
        if args.machine == 'uefi':
            cmd += ['-bios', '/usr/share/OVMF/OVMF_CODE.fd']
    with (out / 'emulator.log').open('w') as logfile:
        p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=logfile, stderr=subprocess.STDOUT)
        rfb = None
        try:
            deadline = time.monotonic() + args.timeout
            while rfb is None and time.monotonic() < deadline:
                if p.poll() is not None:
                    raise RuntimeError(f'Emulator exited: {out / "emulator.log"}')
                try:
                    rfb = Rfb(port)
                except (ConnectionRefusedError, socket.timeout):
                    time.sleep(.05)
            assert rfb is not None, 'RFB startup timeout'
            def log():
                return serial.read_text(errors='replace')
            def wait(seconds=.2):
                stop = time.monotonic() + seconds
                while time.monotonic() < stop:
                    rfb.pump()
            def until(text):
                while text not in log() and time.monotonic() < deadline:
                    wait(.05)
                assert text in log(), f'Missing {text}: {log()[-2000:]}'
            until(args.expect or 'READY:LamOS')
            if args.expect:
                wait(.2)
                assert 'READY:LamOS' not in log()
                (out / 'result.json').write_text(json.dumps({'machine':args.machine,'fault':args.expect,'pass':True}, indent=2))
                print(f'PASS {args.machine}: {args.expect}')
                return
            for marker in ['BOOT:OK','LOG:OK','ARCH:OK','PMM:OK','VMM:OK','HEAP:OK','TIMER:OK','KEYBOARD:OK','GRAPHICS:OK','MOUSE:OK']:
                assert marker in log(), marker
            assert 'PANIC:' not in log() and 'UBSAN:' not in log(), log()
            wait(1)
            rfb.request()
            wait(.6)
            rfb.screenshot(out / 'boot.ppm')
            offset = 32 if args.machine == 'bochs' else 0
            initial = rfb.region(40, 302 + offset, 200, 28)
            def visible_text(value):
                glyphs={'A':[14,17,17,31,17,17,17],'L':[16,16,16,16,16,16,31],
                        'M':[17,27,21,21,17,17,17],'O':[14,17,17,17,17,17,14],
                        'S':[15,16,16,14,1,1,30]}
                for index,ch in enumerate(value):
                    for y,bits in enumerate(glyphs[ch]):
                        for x in range(5):
                            pixel=rfb.region(40+index*12+x*2,306+offset+y*2,1,1)
                            white=all(channel>220 for channel in pixel)
                            if white != bool(bits & (1 << (4-x))):return False
                return True
            def capture_text(value,path):
                stop=time.monotonic()+12
                while time.monotonic()<stop:
                    rfb.request();wait(.5)
                    if visible_text(value):
                        rfb.screenshot(path)
                        return
                raise AssertionError('Framebuffer text missing: '+value)
            def key(k):
                rfb.key(k, 1);wait(.12);rfb.key(k, 0);wait(.12)
            rfb.key(0xffe1, 1);wait(.1);key(ord('A'));rfb.key(0xffe1, 0);wait(.1)
            key(ord('b'));key(0xff08)
            until('buffer=A')
            capture_text('A',out / 'keyboard.ppm')
            assert initial != rfb.region(40, 302 + offset, 200, 28), 'Input area did not change'
            key(0xff0d)
            until('INPUT:SUBMIT A')
            until('INPUT:KEY type=2 code=30')
            # RFB absolute coordinates are converted to PS/2 relative movement by the emulator.
            if args.machine == 'bochs':
                key(0xffc9)  # Enable GUI mouse capture after the Bochs debugger resumes.
            rfb.pointer(160, 160 + offset);wait(.15)
            rfb.pointer(220, 200 + offset, 1);wait(.2)
            until('buttons=1')
            until('INPUT:MOUSE type=4')
            stop=time.monotonic()+12
            while time.monotonic()<stop:
                rfb.request();wait(.5)
                pixel=rfb.region(220,406+offset,1,1)
                if pixel[1]>180 and pixel[1]>pixel[0]+60:break
            else:raise AssertionError('Mouse button indicator did not turn green')
            rfb.screenshot(out / 'mouse.ppm')
            rfb.pointer(220, 200 + offset, 0);wait(.2)
            until('INPUT:MOUSE type=6')
            for char in 'LamOS':
                if char.isupper():rfb.key(0xffe1, 1);wait(.05)
                key(ord(char))
                if char.isupper():rfb.key(0xffe1, 0);wait(.05)
            until('buffer=LamOS')
            capture_text('LAMOS',out / 'desktop.ppm')
            assert len(set(rfb.pixels)) > 4, 'Blank framebuffer'
            result = {'machine':args.machine,'pass':True,'boot':True,'keyboard':True,'mouse':True,
                      'framebuffer':True,'width':rfb.w,'height':rfb.h}
            (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
            print(f'PASS {args.machine}: boot, keyboard, mouse, framebuffer')
        except Exception as error:
            (out / 'result.json').write_text(json.dumps({'machine':args.machine,'pass':False,'error':str(error)}, indent=2))
            raise
        finally:
            if rfb:
                rfb.s.close()
            p.terminate()
            try:p.wait(timeout=3)
            except subprocess.TimeoutExpired:p.kill();p.wait()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', default='build/debug')
    parser.add_argument('--machine', choices=['bios','uefi','bochs'], default='bios')
    parser.add_argument('--timeout', type=float, default=60)
    parser.add_argument('--expect', help='Expected marker in a fault-injection image')
    run(parser.parse_args())
