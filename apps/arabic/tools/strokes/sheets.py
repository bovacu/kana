# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

"""Contact sheets of the generated stroke data, for looking at it.

    python3 apps/arabic/tools/strokes/sheets.py OUT_DIR [--xml data/raw/ar/arabicvg.xml]
            [--font NotoNaskhArabic.ttf] [--only U+FE8B,U+0628...] [--prefix name_]

Needs Pillow; with --font, also fontTools (the glyphs are drawn from their
outlines). Writes into OUT_DIR (keep it outside the repo):

  letters_N.png   one letter a row, its forms in Unicode's order (isolated,
                  final, initial, medial), 6 rows a page; each stroke in its
                  own colour, a ring where it starts with its number,
                  chevrons along it and an arrowhead where it ends; faint
                  lines at the ascender (14), a tooth's tip (57), the baseline
                  (68) and the descender (98); the joins marked at (98, 68)
                  and (11, 68); the harakat's dotted circle; with --font, the
                  font's glyph in grey behind each
  plain.png       everything in black at the app's stroke width
  compare_N.png   with --font: ours in black at the app's stroke width beside
                  the font's glyph, 24 a page (proportions only; nothing is
                  taken from the font)

The font's glyph is scaled so that its alif is as tall as ours and stood on
our baseline; a final form is aligned on its left end, an initial form on its
right end, the others on their middle (the connecting strokes differ in
length). Paths are read back with the bake's rules (pen.parse_d), so this
shows what the app will get.
"""

import math
import os
import sys
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pen import parse_d, flatten                              # noqa: E402
import letters as L                                           # noqa: E402

try:
    from PIL import Image, ImageDraw, ImageFont, ImageChops
except ImportError:                                           # pragma: no cover
    sys.exit("sheets.py needs Pillow (pip install pillow)")

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
XML = os.path.join(ROOT, "data", "raw", "ar", "arabicvg.xml")

WIDTH = 3.2   # the app's stroke width in KanjiVG units (fude/study/chars/glyph.h)

PALETTE = [(220, 50, 47), (38, 139, 210), (133, 153, 0), (211, 54, 130), (203, 75, 22), (42, 161, 152)]
FORM_NAME = {"iso": "isolated", "fin": "final", "ini": "initial", "med": "medial", "mark": "haraka", "digit": "digit"}


def load(path):
    """{code point: [points, ...]} in stroke order."""
    chars = {}
    root = ET.parse(path).getroot()
    for k in root.iter("kanji"):
        cp = int(k.get("id").split("_")[1], 16)
        paths = list(k.iter("path"))
        paths.sort(key=lambda p: int(p.get("id").rsplit("-s", 1)[1]))
        chars[cp] = [flatten(*parse_d(p.get("d")), steps=24) for p in paths]
    return chars


def ink(strokes):
    xs = [p[0] for st in strokes for p in st]
    ys = [p[1] for st in strokes for p in st]
    return min(xs), min(ys), max(xs), max(ys)


# --- the font's glyphs, in the 109 box ------------------------------------------------

class Glyphs:
    def __init__(self, path):
        from fontTools.ttLib import TTFont
        self.tt = TTFont(path)
        self.cmap = self.tt.getBestCmap()
        self.gs = self.tt.getGlyphSet()
        _, _, _, top = self._bounds(0x0627)
        self.scale = (L.B - L.A) / top

    def _polys(self, cp):
        from fontTools.pens.basePen import BasePen

        class Flat(BasePen):
            def __init__(self, gs):
                super().__init__(gs)
                self.polys, self.cur = [], []

            def _moveTo(self, p):
                self.cur = [p]

            def _lineTo(self, p):
                self.cur.append(p)

            def _curveToOne(self, a, b, c):
                p0 = self.cur[-1]
                for i in range(1, 13):
                    t = i / 12.0
                    u = 1.0 - t
                    self.cur.append((u ** 3 * p0[0] + 3 * u * u * t * a[0] + 3 * u * t * t * b[0] + t ** 3 * c[0],
                                     u ** 3 * p0[1] + 3 * u * u * t * a[1] + 3 * u * t * t * b[1] + t ** 3 * c[1]))

            def _qCurveToOne(self, a, b):
                p0 = self.cur[-1]
                for i in range(1, 9):
                    t = i / 8.0
                    u = 1.0 - t
                    self.cur.append((u * u * p0[0] + 2 * u * t * a[0] + t * t * b[0], u * u * p0[1] + 2 * u * t * a[1] + t * t * b[1]))

            def _closePath(self):
                if self.cur:
                    self.polys.append(self.cur)
                    self.cur = []

            _endPath = _closePath

        g = self.cmap.get(cp)
        if g is None:
            return []
        p = Flat(self.gs)
        self.gs[g].draw(p)
        return p.polys

    def _bounds(self, cp):
        pts = [q for poly in self._polys(cp) for q in poly]
        xs, ys = [q[0] for q in pts], [q[1] for q in pts]
        return min(xs), min(ys), max(xs), max(ys)

    def to_box(self, cp, ours):
        """The glyph's outlines as polygons in the 109 box, placed against our
        strokes (see the module's note)."""
        polys = self._polys(cp)
        if not polys:
            return []
        s = self.scale
        x0, y0, x1, y1 = self._bounds(cp)
        ox0, oy0, ox1, oy1 = ink(ours) if ours else (40.0, 40.0, 69.0, 69.0)
        form = L.form_of(cp)
        if form == "fin":
            dx = ox0 - x0 * s
        elif form == "ini":
            dx = ox1 - x1 * s
        else:
            dx = (ox0 + ox1) / 2.0 - (x0 + x1) / 2.0 * s
        if form == "mark":
            dy = (oy0 + oy1) / 2.0 + (y0 + y1) / 2.0 * s
        else:
            dy = L.B
        return [[(dx + x * s, dy - y * s) for x, y in poly] for poly in polys]

    def draw(self, img, cp, ours, ox, oy, scale, fill):
        polys = self.to_box(cp, ours)
        if not polys:
            return
        acc = Image.new("1", img.size, 0)
        for p in polys:
            if len(p) < 3:
                continue
            m = Image.new("1", img.size, 0)
            ImageDraw.Draw(m).polygon([(ox + x * scale, oy + y * scale) for x, y in p], fill=1)
            acc = ImageChops.logical_xor(acc, m)
        img.paste(Image.new("RGB", img.size, fill), (0, 0), acc.convert("L"))


# --- drawing ---------------------------------------------------------------------------

_fonts = {}


def _font(px, path=None):
    key = (px, path)
    if key not in _fonts:
        cands = [path] if path else ["/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/System/Library/Fonts/Helvetica.ttc",
                                     "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"]
        for f in cands:
            if f and os.path.exists(f):
                _fonts[key] = ImageFont.truetype(f, px)
                break
        else:
            _fonts[key] = ImageFont.load_default()
    return _fonts[key]


def frame(d, ox, oy, size, cp=None, guides=True):
    d.rectangle([ox, oy, ox + size - 1, oy + size - 1], outline=(200, 200, 200))
    if not guides:
        return
    s = size / 109.0
    for y, c in ((L.A, (235, 235, 250)), (L.T, (232, 232, 250)), (L.B, (190, 190, 240)), (L.D, (235, 235, 250))):
        d.line([(ox, oy + y * s), (ox + size, oy + y * s)], fill=c)
    d.line([(ox + L.CX * s, oy), (ox + L.CX * s, oy + size)], fill=(244, 244, 244))
    form = L.form_of(cp) if cp is not None else None
    for x, on in ((L.RX, form in L.JOINS_RIGHT), (L.LX, form in L.JOINS_LEFT)):
        c = (120, 190, 120) if on else (225, 225, 225)
        d.line([(ox + x * s, oy + (L.B - 4) * s), (ox + x * s, oy + (L.B + 4) * s)], fill=c, width=2)
    if form == "mark":
        cx, cy, r = L.CIRCLE
        for i in range(20):
            a = i * 2 * math.pi / 20
            b = a + 2 * math.pi / 48
            d.line([(ox + (cx + r * math.cos(a)) * s, oy + (cy + r * math.sin(a)) * s),
                    (ox + (cx + r * math.cos(b)) * s, oy + (cy + r * math.sin(b)) * s)], fill=(170, 170, 170), width=2)


def _chevron(d, x, y, ux, uy, size, colour):
    d.line([(x - ux * size + uy * size * 0.7, y - uy * size - ux * size * 0.7), (x, y),
            (x - ux * size - uy * size * 0.7, y - uy * size + ux * size * 0.7)], fill=colour, width=max(1, int(size / 3)))


def draw_char(d, strokes, ox, oy, scale, numbered, colour=None, width=WIDTH):
    w = max(1, int(round(width * scale)))
    r = w / 2.0
    for i, pts in enumerate(strokes):
        c = colour or PALETTE[i % len(PALETTE)]
        xy = [(ox + x * scale, oy + y * scale) for x, y in pts]
        d.line(xy, fill=c, width=w, joint="curve")
        for p in (xy[0], xy[-1]):
            d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=c)
    if not numbered:
        return
    font = _font(max(10, int(scale * 5.5)))
    for i, pts in enumerate(strokes):
        c = PALETTE[i % len(PALETTE)]
        xy = [(ox + x * scale, oy + y * scale) for x, y in pts]
        # chevrons along the stroke, every 14 units, showing the way it goes
        walked, nxt = 0.0, 9.0
        for a, b in zip(pts, pts[1:]):
            seg = math.hypot(b[0] - a[0], b[1] - a[1])
            while seg > 0 and walked + seg >= nxt:
                t = (nxt - walked) / seg
                x, y = a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t
                ux, uy = (b[0] - a[0]) / seg, (b[1] - a[1]) / seg
                _chevron(d, ox + x * scale, oy + y * scale, ux, uy, scale * 1.4, (255, 255, 255))
                nxt += 14.0
            walked += seg
        # an arrowhead at the end, pointing the way the pen goes
        (ax, ay), (bx, by) = xy[max(0, len(xy) - 4)], xy[-1]
        n = math.hypot(bx - ax, by - ay) or 1.0
        ux, uy = (bx - ax) / n, (by - ay) / n
        L1 = scale * 3.2
        d.polygon([(bx + ux * L1 * 0.6, by + uy * L1 * 0.6),
                   (bx - ux * L1 + uy * L1 * 0.55, by - uy * L1 - ux * L1 * 0.55),
                   (bx - ux * L1 - uy * L1 * 0.55, by - uy * L1 + ux * L1 * 0.55)], fill=(40, 40, 40))
        # a ring where it starts, and its number beside it
        x, y = xy[0]
        rr = r * 1.7
        d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=(255, 255, 255), outline=c, width=max(2, w // 3))
        k = min(4, len(pts) - 1)
        dx, dy = pts[k][0] - pts[0][0], pts[k][1] - pts[0][1]
        nn = max(1e-6, math.hypot(dx, dy))
        tx, ty = x - dx / nn * scale * 4.8, y - dy / nn * scale * 4.8
        d.text((tx, ty), str(i + 1), fill=c, font=font, anchor="mm", stroke_width=2, stroke_fill=(255, 255, 255))


def sheet(chars, rows, path, cols, cell, numbered, label=True, colour=None, width=WIDTH, glyphs=None, font_path=None):
    lab = 22 if label else 0
    img = Image.new("RGB", (cols * cell, len(rows) * (cell + lab)), "white")
    d = ImageDraw.Draw(img)
    scale = cell / 109.0
    for ri, row in enumerate(rows):
        for ci, cp in enumerate(row):
            ox, oy = ci * cell, ri * (cell + lab)
            frame(d, ox, oy + lab, cell, cp, guides=numbered)
            if label:
                d.text((ox + 4, oy + 4), "U+%04X %s %d" % (cp, FORM_NAME[L.form_of(cp)], len(chars.get(cp, []))),
                       fill=(90, 90, 90), font=_font(13))
                if font_path:
                    d.text((ox + cell - 8, oy + 2), chr(cp) if L.form_of(cp) != "mark" else "◌" + chr(cp),
                           fill=(60, 60, 60), font=_font(18, font_path), anchor="ra")
            if glyphs is not None:
                glyphs.draw(img, cp, chars.get(cp), ox, oy + lab, scale, (218, 218, 218))
                d = ImageDraw.Draw(img)
            if cp in chars:
                draw_char(d, chars[cp], ox, oy + lab, scale, numbered, colour, width)
    img.save(path)
    return path


def compare(chars, cps, glyphs, path, cols, cell):
    pair = cell * 2 + 8
    rows = (len(cps) + cols - 1) // cols
    img = Image.new("RGB", (cols * pair, rows * cell), "white")
    d = ImageDraw.Draw(img)
    scale = cell / 109.0
    for i, cp in enumerate(cps):
        ox, oy = (i % cols) * pair, (i // cols) * cell
        frame(d, ox, oy, cell, guides=False)
        frame(d, ox + cell, oy, cell, guides=False)
        draw_char(d, chars[cp], ox, oy, scale, False, (0, 0, 0))
        glyphs.draw(img, cp, chars[cp], ox + cell, oy, scale, (0, 0, 0))
        d = ImageDraw.Draw(img)
    img.save(path)
    return path


def pages(items, n):
    return [items[i:i + n] for i in range(0, len(items), n)]


def main(argv):
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--xml", default=XML)
    ap.add_argument("--font")
    ap.add_argument("--only", default="", help="comma-separated code points (U+FE8B or FE8B)")
    ap.add_argument("--prefix", default="")
    ap.add_argument("--cell", type=int, default=270)
    a = ap.parse_args(argv[1:])
    os.makedirs(a.out, exist_ok=True)
    chars = load(a.xml)
    rows = [list(r) for r in L.ROWS]
    if a.only:
        want = [int(t.strip().upper().replace("U+", ""), 16) for t in a.only.split(",") if t.strip()]
        rows = [r for r in ([c for c in row if c in want] for row in rows) if r]
    glyphs = Glyphs(a.font) if a.font else None
    p = lambda n: os.path.join(a.out, a.prefix + n)
    made = []
    for k, rs in enumerate(pages(rows, 6), 1):
        made.append(sheet(chars, rs, p("letters_%d.png" % k), 4, a.cell, True, glyphs=glyphs, font_path=a.font))
    flat = [c for r in rows for c in r]
    made.append(sheet(chars, pages(flat, 16), p("plain.png"), 16, 100, False, False, (0, 0, 0)))
    if glyphs is not None:
        for k, cps in enumerate(pages(flat, 24), 1):
            made.append(compare(chars, cps, glyphs, p("compare_%d.png" % k), 4, 170))
    for m in made:
        print(m)


if __name__ == "__main__":
    main(sys.argv)
