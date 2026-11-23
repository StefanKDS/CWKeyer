#!/usr/bin/env python3
"""Creates FAT16/FAT32 test images (mkfs.vfat + own directory writer, incl. LFN).
Usage: python3 mkimg.py <out_dir>"""
import struct, subprocess, sys, os

def build(path, fat, files, subdir_files):
    if os.path.exists(path): os.remove(path)
    size_kb = 40 * 1024 if fat == 32 else 20 * 1024
    subprocess.run(['mkfs.vfat', '-C', '-F', str(fat), path, str(size_kb)], check=True, capture_output=True)
    img = bytearray(open(path, 'rb').read())
    b = img[:512]
    bps, spc, rsvd, nf, rootent = struct.unpack_from('<HBHBH', b, 11)
    fatsz = struct.unpack_from('<H', b, 22)[0] or struct.unpack_from('<I', b, 36)[0]
    fat_start = rsvd; root_start = fat_start + nf * fatsz; rootsecs = (rootent * 32 + 511) // 512
    data = root_start + rootsecs
    rootclus = struct.unpack_from('<I', b, 44)[0] if fat == 32 else 0
    nxt = [3 if fat == 32 else 2]
    EOC = 0xFFFF if fat == 16 else 0x0FFFFFFF
    csize = spc * 512
    def setfat(c, v):
        for i in range(nf):
            off = (fat_start + i * fatsz) * 512
            if fat == 16: struct.pack_into('<H', img, off + c * 2, v & 0xFFFF)
            else: struct.pack_into('<I', img, off + c * 4, v)
    def alloc(d):
        n = max(1, (len(d) + csize - 1) // csize); first = nxt[0]
        for i in range(n):
            c = first + i; setfat(c, EOC if i == n - 1 else c + 1)
            off = (data + (c - 2) * spc) * 512; chunk = d[i * csize:(i + 1) * csize]
            img[off:off + len(chunk)] = chunk
        nxt[0] += n; return first
    def entries(name, short, attr, clus, size):
        chk = 0
        for ch in short: chk = (((chk & 1) << 7) + (chk >> 1) + ch) & 0xFF
        u = name.encode('utf-16-le') + b'\x00\x00'
        chars = [u[i:i + 2] for i in range(0, len(u), 2)]
        while len(chars) % 13: chars.append(b'\xff\xff')
        nparts = len(chars) // 13; out = []
        pos = [1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30]
        for p in range(nparts, 0, -1):
            c = chars[(p - 1) * 13:p * 13]; e = bytearray(32)
            e[0] = p | (0x40 if p == nparts else 0); e[11] = 0x0F; e[13] = chk
            for k in range(13): e[pos[k]:pos[k] + 2] = c[k]
            out.append(bytes(e))
        e = bytearray(32); e[0:11] = short; e[11] = attr
        struct.pack_into('<H', e, 20, clus >> 16); struct.pack_into('<H', e, 26, clus & 0xFFFF); struct.pack_into('<I', e, 28, size)
        out.append(bytes(e)); return b''.join(out)
    idx = [0]
    def blob(lst):
        r = b''
        for n, d in lst:
            idx[0] += 1; c = alloc(d); r += entries(n, ('FILE%04d' % idx[0]).encode() + b'TXT', 0x20, c, len(d))
        return r
    sb = blob(subdir_files); sb += b'\x00' * (csize - len(sb))
    subc = alloc(sb)
    rb = blob(files) + entries('CW', b'CW         ', 0x10, subc, 0)
    off = root_start * 512 if fat == 16 else (data + (rootclus - 2) * spc) * 512
    img[off:off + len(rb)] = rb
    open(path, 'wb').write(img)

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else '.'
    big = ('CQ CQ DE DL1ABC ' * 400).encode()
    for fat in (16, 32):
        build(os.path.join(out, f'fat{fat}.img'), fat,
              [('Übung_lange_Datei_Name.txt', 'Hallo Welt, äöü ß!\r\nZeile 2'.encode('utf-8')), ('big.TXT', big), ('readme.md', b'x')],
              [('qso_01.txt', b'GM OM UR RST 599')])
    print('ok')
