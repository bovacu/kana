#!/usr/bin/env python3
"""Where two screenshots differ: how many pixels, the box around them, and the largest difference in any channel.

    python3 tools/check/pngdiff.py before.png after.png [--out diff.png]

With --out, writes a picture of the differences: the after picture dimmed, the changed pixels in red. Reads the PNGs
the look harness writes (8-bit RGB or RGBA, not interlaced) with nothing but Python's own zlib.
"""
import struct
import sys
import zlib


def read_png(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', f'{path}: not a PNG'
    pos, idat, width, height, kind = 8, b'', 0, 0, 0
    while pos < len(data):
        length, tag = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if tag == b'IHDR':
            width, height, depth, kind, _, _, interlace = struct.unpack('>IIBBBBB', body)
            assert depth == 8 and kind in (2, 6) and interlace == 0, f'{path}: only 8-bit RGB/RGBA, not interlaced'
        elif tag == b'IDAT':
            idat += body
        pos += 12 + length
    channels = 4 if kind == 6 else 3
    raw = zlib.decompress(idat)
    stride = width * channels
    pixels = bytearray(height * stride)
    prev = bytearray(stride)
    at = 0
    for y in range(height):
        filt = raw[at]
        line = bytearray(raw[at + 1:at + 1 + stride])
        at += 1 + stride
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b = prev[i]
            c = prev[i - channels] if i >= channels else 0
            if filt == 1:
                line[i] = (line[i] + a) & 255
            elif filt == 2:
                line[i] = (line[i] + b) & 255
            elif filt == 3:
                line[i] = (line[i] + ((a + b) >> 1)) & 255
            elif filt == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))) & 255
        pixels[y * stride:(y + 1) * stride] = line
        prev = line
    return width, height, channels, pixels


def write_png(path, width, height, rgb):
    raw = b''.join(b'\x00' + bytes(rgb[y * width * 3:(y + 1) * width * 3]) for y in range(height))
    def chunk(tag, body):
        return struct.pack('>I', len(body)) + tag + body + struct.pack('>I', zlib.crc32(tag + body) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) +
                           chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


def main():
    a, b = sys.argv[1], sys.argv[2]
    out = sys.argv[sys.argv.index('--out') + 1] if '--out' in sys.argv else None
    wa, ha, ca, pa = read_png(a)
    wb, hb, cb, pb = read_png(b)
    if (wa, ha) != (wb, hb):
        print(f'different sizes: {wa}x{ha} and {wb}x{hb}')
        return 1
    changed, worst = 0, 0
    x0, y0, x1, y1 = wa, ha, -1, -1
    marks = bytearray(wa * ha * 3) if out else None
    for y in range(ha):
        for x in range(wa):
            ia, ib = (y * wa + x) * ca, (y * wa + x) * cb
            d = max(abs(pa[ia + k] - pb[ib + k]) for k in range(3))
            if d > 0:
                changed += 1
                worst = max(worst, d)
                x0, y0, x1, y1 = min(x0, x), min(y0, y), max(x1, x), max(y1, y)
            if marks is not None:
                o = (y * wa + x) * 3
                if d > 0:
                    marks[o:o + 3] = b'\xff\x20\x20'
                else:
                    marks[o:o + 3] = bytes(v // 3 for v in pb[ib:ib + 3])
    if changed == 0:
        print('identical pixels')
    else:
        print(f'{changed} pixels differ, in x {x0}..{x1}, y {y0}..{y1}; the largest difference {worst} of 255')
    if out:
        write_png(out, wa, ha, marks)
    return 0 if changed == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
