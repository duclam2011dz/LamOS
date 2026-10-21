import socket, struct, time, zlib, binascii
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
                if size==4 and self.maxima==(255,255,255):
                    rgb=bytearray(w*h*3)
                    for channel,shift in enumerate(self.shifts):
                        byte=shift//8 if not self.big else 3-shift//8
                        rgb[channel::3]=data[byte::4]
                elif size==1:
                    lookup=[bytes(((p>>shift)&maximum)*255//maximum for shift,maximum in zip(self.shifts,self.maxima)) for p in range(256)]
                    rgb=b''.join(lookup[p] for p in data)
                else:
                    rgb=bytearray()
                    for off in range(0,len(data),size):
                        pixel=int.from_bytes(data[off:off+size],'big' if self.big else 'little')
                        rgb.extend(((pixel>>shift)&maximum)*255//maximum for shift,maximum in zip(self.shifts,self.maxima))
                for yy in range(h):
                    target=((y+yy)*self.w+x)*3
                    self.pixels[target:target+w*3]=rgb[yy*w*3:(yy+1)*w*3]
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

