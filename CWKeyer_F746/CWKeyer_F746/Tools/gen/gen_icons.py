#!/usr/bin/env python3
"""Generates the outline icons (Gui/Src/icons.c, Gui/Inc/icons.h).

Icons are described on a 24 x 24 grid with stroke width 2 (similar to the
Tabler icon style), rendered with 8x supersampling and stored as 4 bpp
alpha maps in three sizes. Add an icon: write a function, add it to ICONS.
"""
import math, os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
SIZES = [("S", 20), ("M", 28), ("L", 44)]
SS = 8
FONT = os.environ.get("ICONFONT", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf")


class Pen:
    def __init__(self, size):
        self.n = size * SS
        self.k = self.n / 24.0
        self.img = Image.new("L", (self.n, self.n), 0)
        self.d = ImageDraw.Draw(self.img)
        self.w = 2.0

    def p(self, x, y):
        return (x * self.k, y * self.k)

    def dot(self, x, y, r=None):
        r = (r if r is not None else self.w / 2) * self.k
        cx, cy = self.p(x, y)
        self.d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)

    def line(self, *pts, close=False):
        pts = list(pts) + ([pts[0]] if close else [])
        for a, b in zip(pts, pts[1:]):
            self.d.line([self.p(*a), self.p(*b)], fill=255, width=int(self.w * self.k))
        for q in pts:
            self.dot(*q)

    def circle(self, x, y, r, fill=False):
        if fill:
            self.dot(x, y, r)
            return
        cx, cy = self.p(x, y); rr = r * self.k; hw = self.w * self.k / 2
        self.d.ellipse((cx - rr - hw, cy - rr - hw, cx + rr + hw, cy + rr + hw), fill=255)
        self.d.ellipse((cx - rr + hw, cy - rr + hw, cx + rr - hw, cy + rr - hw), fill=0)

    def arc(self, x, y, r, a0, a1):
        """angles in degrees, 0 = right, counter clockwise positive"""
        steps = max(8, int(abs(a1 - a0) / 6))
        pts = [(x + r * math.cos(math.radians(a0 + (a1 - a0) * i / steps)),
                y - r * math.sin(math.radians(a0 + (a1 - a0) * i / steps))) for i in range(steps + 1)]
        self.line(*pts)

    def poly(self, *pts):
        self.d.polygon([self.p(*q) for q in pts], fill=255)

    def rrect(self, x0, y0, x1, y1, r):
        k = self.k; hw = self.w * k / 2
        self.d.rounded_rectangle((x0 * k - hw, y0 * k - hw, x1 * k + hw, y1 * k + hw), r * k + hw, fill=255)
        self.d.rounded_rectangle((x0 * k + hw, y0 * k + hw, x1 * k - hw, y1 * k - hw), max(0, r * k - hw), fill=0)

    def text(self, x, y, s, px):
        f = ImageFont.truetype(FONT, int(px * self.k))
        self.d.text(self.p(x, y), s, font=f, fill=255, anchor="mm")


# ---------------------------------------------------------------- icons --
def key(p):        # morse key: base, lever, knob
    p.rrect(3, 17, 21, 21, 1.5)
    p.line((6, 17), (6, 14), (19, 10))
    p.circle(19, 9.5, 2.2, fill=True)
    p.line((14, 11.5), (14, 17))

def wave(p):
    pts = [(2 + i * 0.5, 12 - 7 * math.sin((i * 0.5) / 20 * 2 * math.pi * 1.5)) for i in range(41)]
    p.line(*pts)

def school(p):
    p.line((2, 9), (12, 4), (22, 9), (12, 14), close=True)
    p.line((6, 11), (6, 16))
    p.arc(12, 16, 6, 180, 360)
    p.line((18, 16), (18, 11))
    p.line((22, 9), (22, 15))

def file_text(p):
    p.line((14, 3), (6, 3), (6, 3))
    p.rrect(5, 3, 19, 21, 2)
    p.d.polygon([p.p(14, 2), p.p(20.5, 2), p.p(20.5, 8.5)], fill=0)
    p.line((14, 3), (14, 8), (19, 8))
    p.line((14, 3), (19, 8))
    p.line((9, 13), (15, 13)); p.line((9, 17), (15, 17))

def floppy(p):
    p.line((6, 4), (16, 4), (20, 8), (20, 18))
    p.rrect(4, 4, 20, 20, 2)
    p.d.polygon([p.p(16.5, 3), p.p(21, 3), p.p(21, 7.5)], fill=0)
    p.line((16, 4), (20, 8))
    p.circle(12, 14, 2)
    p.line((8, 4), (8, 8), (14, 8), (14, 4))

def gear(p):
    pts = []
    n = 8
    for i in range(n * 4):
        a = 2 * math.pi * i / (n * 4)
        r = 9 if (i % 4) in (0, 1) else 6.8
        pts.append((12 + r * math.cos(a + 0.2), 12 + r * math.sin(a + 0.2)))
    p.line(*pts, close=True)
    p.circle(12, 12, 3)

def back(p):
    p.line((5, 12), (19, 12)); p.line((11, 6), (5, 12), (11, 18))

def play(p):
    p.line((7, 4), (19, 12), (7, 20), close=True)

def stop(p):
    p.rrect(6, 6, 18, 18, 2)

def pause(p):
    p.rrect(6, 5, 9, 19, 1); p.rrect(15, 5, 18, 19, 1)

def plus(p):
    p.line((12, 5), (12, 19)); p.line((5, 12), (19, 12))

def minus(p):
    p.line((5, 12), (19, 12))

def check(p):
    p.line((5, 12), (10, 17), (20, 7))

def xmark(p):
    p.line((6, 6), (18, 18)); p.line((18, 6), (6, 18))

def eraser(p):
    p.line((19, 20), (8.5, 20), (4.3, 15.7), (15, 5), (20.7, 10.7), (11.5, 20))
    p.line((18, 13.3), (11.7, 7))

def skip_fwd(p):
    p.line((5, 5), (15, 12), (5, 19), close=True); p.line((19, 5), (19, 19))

def skip_back(p):
    p.line((19, 5), (9, 12), (19, 19), close=True); p.line((5, 5), (5, 19))

def eye(p):
    p.circle(12, 12, 2.5)
    p.arc(12, 17.3, 11.3, 28, 152)
    p.arc(12, 6.7, 11.3, 208, 332)

def eye_off(p):
    eye(p)
    p.line((3, 3), (21, 21))

def backspace(p):
    p.line((20, 6), (9, 6), (3, 12), (9, 18), (20, 18), close=True)
    p.line((12, 9.5), (17, 14.5)); p.line((17, 9.5), (12, 14.5))

def adjust(p):
    for x, k in ((6, 8), (12, 15), (18, 10)):
        p.line((x, 4), (x, k - 2)); p.line((x, k + 2), (x, 20)); p.circle(x, k, 2)

def gauge(p):
    p.arc(12, 14, 9, -30, 210)
    p.line((12, 14), (16.5, 8.5))
    p.circle(12, 14, 1.3, fill=True)

def volume(p):
    p.line((4, 9), (8, 9), (13, 4), (13, 20), (8, 15), (4, 15), close=True)
    p.arc(14, 12, 4, -45, 45); p.arc(14, 12, 7.5, -50, 50)

def volume_off(p):
    p.line((4, 9), (8, 9), (13, 4), (13, 20), (8, 15), (4, 15), close=True)
    p.line((16, 9.5), (21, 14.5)); p.line((21, 9.5), (16, 14.5))

def ear(p):
    p.arc(12.5, 10, 6.5, 180, -40)
    p.line((17.5, 14.2), (15.5, 16.5), (14.5, 19))
    p.arc(10.5, 18.5, 4, 0, -150)
    p.arc(12.5, 10, 3, 180, -30)

def shuffle(p):
    p.line((3, 7), (7, 7), (16, 17), (21, 17)); p.line((18, 14), (21, 17), (18, 20))
    p.line((3, 17), (7, 17), (10, 13.5)); p.line((13, 10.5), (16, 7), (21, 7)); p.line((18, 4), (21, 7), (18, 10))

def sort_az(p):
    p.text(8, 7, "A", 9); p.text(8, 17.5, "Z", 9)
    p.line((18, 4), (18, 20)); p.line((15, 17), (18, 20), (21, 17))

def repeat(p):
    p.line((4, 12), (4, 9), (7, 6), (20, 6)); p.line((17, 3), (20, 6), (17, 9))
    p.line((20, 12), (20, 15), (17, 18), (4, 18)); p.line((7, 15), (4, 18), (7, 21))

def abc(p):
    p.text(12, 12.5, "ABC", 8.5)

def info(p):
    p.circle(12, 12, 9); p.line((12, 11), (12, 16.5)); p.dot(12, 7.8, 1.3)

def sdcard(p):
    p.line((7, 21), (17, 21), (17.5, 21))
    p.line((5, 8), (9, 3), (19, 3), (19, 21), (5, 21), close=True)
    p.line((11, 6.5), (11, 9)); p.line((14, 6.5), (14, 9)); p.line((17, 6.5), (17, 9))

def antenna(p):
    p.line((12, 9), (12, 21)); p.line((8, 21), (16, 21)); p.circle(12, 8, 1.4, fill=True)
    p.arc(12, 8, 4, -50, 50); p.arc(12, 8, 4, 130, 230)
    p.arc(12, 8, 8, -45, 45); p.arc(12, 8, 8, 135, 225)

def keyboard(p):
    p.rrect(2, 6, 22, 18, 2)
    for x in (6, 10, 14, 18):
        p.dot(x, 10, 1.1)
    p.line((7, 14.5), (17, 14.5))

def refresh(p):
    p.arc(12, 12, 7.5, 30, 300)
    p.line((18.5, 3.5), (18.5, 8.3), (13.7, 8.3))

def swap(p):
    p.line((4, 8), (20, 8)); p.line((16, 4), (20, 8), (16, 12))
    p.line((20, 16), (4, 16)); p.line((8, 12), (4, 16), (8, 20))

def usb(p):
    p.line((12, 4), (12, 19))
    p.poly((12, 2), (9.5, 5.5), (14.5, 5.5))
    p.circle(12, 20, 1.8, fill=True)
    p.line((12, 15), (7, 11.5), (7, 9))
    p.d.rectangle((p.p(5.5, 6.5)[0], p.p(5.5, 6.5)[1], p.p(8.5, 9.5)[0], p.p(8.5, 9.5)[1]), fill=255)
    p.line((12, 13), (17, 10), (17, 8.5))
    p.circle(17, 7.5, 1.6, fill=True)

def home(p):
    p.line((5, 12), (3, 12), (12, 3), (21, 12), (19, 12))
    p.line((5, 12), (5, 20), (19, 20), (19, 12))
    p.line((10, 20), (10, 15), (14, 15), (14, 20))


ICONS = [("KEY", key), ("WAVE", wave), ("SCHOOL", school), ("FILE", file_text),
         ("FLOPPY", floppy), ("GEAR", gear), ("BACK", back), ("PLAY", play),
         ("STOP", stop), ("PAUSE", pause), ("PLUS", plus), ("MINUS", minus),
         ("CHECK", check), ("X", xmark), ("ERASER", eraser), ("SKIP_FWD", skip_fwd),
         ("SKIP_BACK", skip_back), ("EYE", eye), ("EYE_OFF", eye_off),
         ("BACKSPACE", backspace), ("ADJUST", adjust), ("GAUGE", gauge),
         ("VOLUME", volume), ("VOLUME_OFF", volume_off), ("EAR", ear),
         ("SHUFFLE", shuffle), ("SORT_AZ", sort_az), ("REPEAT", repeat),
         ("ABC", abc), ("INFO", info), ("SD", sdcard), ("ANTENNA", antenna),
         ("KEYBOARD", keyboard), ("REFRESH", refresh), ("SWAP", swap), ("HOME", home), ("USB", usb)]


def render(fn, size):
    p = Pen(size)
    fn(p)
    img = p.img.resize((size, size), Image.LANCZOS)
    data = bytearray()
    for y in range(size):
        row = [img.getpixel((x, y)) >> 4 for x in range(size)]
        if len(row) % 2:
            row.append(0)
        for i in range(0, len(row), 2):
            data.append((row[i] << 4) | row[i + 1])
    return img, data


def main():
    c = ['/* Generated by Tools/gen/gen_icons.py - do not edit. */', '#include "icons.h"', '']
    h = ['/* Generated by Tools/gen/gen_icons.py - do not edit. */', '#ifndef ICONS_H',
         '#define ICONS_H', '', '#include <stdint.h>', '',
         'typedef struct', '{', '    uint8_t        size;   /**< width = height */',
         '    const uint8_t *data;   /**< 4 bpp alpha, rows */', '} icon_bitmap_t;', '',
         'typedef enum', '{']
    h += [f'    ICON_{n},' for n, _ in ICONS] + ['    ICON_COUNT', '} icon_id_t;', '',
          'typedef enum { ICON_S = 0, ICON_M, ICON_L, ICON_SIZES } icon_size_t;', '',
          f'#define ICON_PX_S {SIZES[0][1]}', f'#define ICON_PX_M {SIZES[1][1]}', f'#define ICON_PX_L {SIZES[2][1]}', '',
          'extern const icon_bitmap_t g_icons[ICON_COUNT][ICON_SIZES];', '', '#endif', '']
    sheet = Image.new("L", (len(ICONS) * 48, 48 * 3), 0)
    total = 0
    for i, (name, fn) in enumerate(ICONS):
        for j, (sn, sz) in enumerate(SIZES):
            img, data = render(fn, sz)
            sheet.paste(img, (i * 48, j * 48))
            total += len(data)
            c.append(f'static const uint8_t ic_{name.lower()}_{sn}[{len(data)}] = {{')
            for k in range(0, len(data), 24):
                c.append('    ' + ','.join(f'0x{b:02X}' for b in data[k:k + 24]) + ',')
            c.append('};')
    c.append('')
    c.append('const icon_bitmap_t g_icons[ICON_COUNT][ICON_SIZES] = {')
    for name, _ in ICONS:
        c.append('    { ' + ', '.join(f'{{{sz}, ic_{name.lower()}_{sn}}}' for sn, sz in SIZES) + ' },')
    c.append('};')
    open(os.path.join(ROOT, 'Gui/Src/icons.c'), 'w').write('\n'.join(c) + '\n')
    open(os.path.join(ROOT, 'Gui/Inc/icons.h'), 'w').write('\n'.join(h))
    sheet.save('/tmp/icon_sheet.png')
    print(f'icons: {len(ICONS)}, {total/1024:.1f} KiB')


if __name__ == '__main__':
    main()
