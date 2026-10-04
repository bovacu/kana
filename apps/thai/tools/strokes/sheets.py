"""Contact sheets of the generated stroke data, for looking at it.

    python3 apps/thai/tools/strokes/sheets.py OUT_DIR [--xml data/raw/th/thaivg.xml]
            [--font apps/thai/assets/fonts/NotoSansThaiLooped-Regular.ttf] [--only กขค...] [--prefix name_]

Needs Pillow; with --font, also fontTools if it is installed (the glyphs are
then drawn from their outlines, so the marks sit where they sit on a letter;
without it Pillow draws them, and marks may land off). Writes into OUT_DIR
(keep it outside the repo):

  letters_N.png   20 letters a page, large: each stroke in its own colour, a
                  ring where it starts with its number, an arrowhead where it
                  ends; faint lines at the ascender, body top, baseline and
                  descender; with --font, the font's glyph beside each, grey
  plain.png       all the letters in black at the app's stroke width
  compare_N.png   with --font: ours in black at the app's stroke width beside
                  the font's glyph, 24 a page (proportions only; nothing is
                  taken from the font)

Paths are read back with the bake's rules (pen.parse_d), so this shows what the
app will get.
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
XML = os.path.join(ROOT, "data", "raw", "th", "thaivg.xml")

WIDTH = 3.2   # the app's stroke width in KanjiVG units (fude/study/chars/glyph.h)
CENTRE = 54.5

PALETTE = [(220, 50, 47), (38, 139, 210), (133, 153, 0), (211, 54, 130), (203, 75, 22), (42, 161, 152)]


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


# --- the font's glyphs, in the 109 box ------------------------------------------------

class Glyphs:
    """The font's glyphs mapped onto letters.py's frame: the top of ก at the
    bodies' top, the baseline on the baseline; a spacing glyph centred on its
    ink, a mark placed on a ก centred in the box."""

    def __init__(self, path):
        self.path = path
        self.tt = None
        try:
            from fontTools.ttLib import TTFont
            self.tt = TTFont(path)
            self.cmap = self.tt.getBestCmap()
            self.gs = self.tt.getGlyphSet()
            self.hmtx = self.tt["hmtx"]
            x0, y0, x1, y1 = self._bounds(0x0E01)
            self.scale = (L.B - L.T) / y1
            self.base_adv = self.hmtx[self.cmap[0x0E01]][0]
            self.base_ink = (x0, x1)
        except ImportError:
            pass

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

    def to_box(self, cp):
        """The glyph's outlines as polygons in the 109 box."""
        polys = self._polys(cp)
        if not polys:
            return []
        s = self.scale
        adv = self.hmtx[self.cmap[cp]][0]
        if adv == 0:
            bx0, bx1 = self.base_ink
            origin = CENTRE - (bx0 + bx1) / 2.0 * s + self.base_adv * s
        else:
            x0, _, x1, _ = self._bounds(cp)
            origin = CENTRE - (x0 + x1) / 2.0 * s
        return [[(origin + x * s, L.B - y * s) for x, y in poly] for poly in polys]

    def draw(self, img, cp, ox, oy, scale, fill):
        if self.tt is None:
            f = ImageFont.truetype(self.path, int(scale * (L.B - L.T) / 0.56))
            ImageDraw.Draw(img).text((ox + CENTRE * scale, oy + L.B * scale), chr(cp), font=f, fill=fill, anchor="ms")
            return
        polys = self.to_box(cp)
        if not polys:
            return
        area = lambda p: sum(p[i][0] * p[(i + 1) % len(p)][1] - p[(i + 1) % len(p)][0] * p[i][1] for i in range(len(p)))
        areas = [area(p) for p in polys]
        big = max(areas, key=abs)
        pos = Image.new("L", img.size, 0)
        neg = Image.new("L", img.size, 0)
        for p, a in zip(polys, areas):
            if len(p) < 3:
                continue
            xy = [(ox + x * scale, oy + y * scale) for x, y in p]
            ImageDraw.Draw(pos if (a > 0) == (big > 0) else neg).polygon(xy, fill=255)
        img.paste(Image.new("RGB", img.size, fill), (0, 0), ImageChops.subtract(pos, neg))


# --- drawing ---------------------------------------------------------------------------

_fonts = {}


def _font(px):
    if px not in _fonts:
        for f in ("/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/System/Library/Fonts/Helvetica.ttc",
                  "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"):
            if os.path.exists(f):
                _fonts[px] = ImageFont.truetype(f, px)
                break
        else:
            _fonts[px] = ImageFont.load_default()
    return _fonts[px]


def frame(d, ox, oy, size, guides=True):
    d.rectangle([ox, oy, ox + size - 1, oy + size - 1], outline=(200, 200, 200))
    if guides:
        s = size / 109.0
        for y, c in ((L.A, (235, 235, 250)), (L.T, (210, 210, 245)), (L.B, (210, 210, 245)), (L.D, (235, 235, 250))):
            d.line([(ox, oy + y * s), (ox + size, oy + y * s)], fill=c)
        d.line([(ox + CENTRE * s, oy), (ox + CENTRE * s, oy + size)], fill=(240, 240, 240))


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
    font = _font(max(10, int(scale * 6.5)))
    for i, pts in enumerate(strokes):
        c = PALETTE[i % len(PALETTE)]
        xy = [(ox + x * scale, oy + y * scale) for x, y in pts]
        # an arrowhead at the end, pointing the way the pen goes
        (ax, ay), (bx, by) = xy[max(0, len(xy) - 4)], xy[-1]
        n = math.hypot(bx - ax, by - ay) or 1.0
        ux, uy = (bx - ax) / n, (by - ay) / n
        L1 = scale * 4.0
        d.polygon([(bx + ux * L1 * 0.6, by + uy * L1 * 0.6),
                   (bx - ux * L1 + uy * L1 * 0.55, by - uy * L1 - ux * L1 * 0.55),
                   (bx - ux * L1 - uy * L1 * 0.55, by - uy * L1 + ux * L1 * 0.55)], fill=(40, 40, 40))
        # a ring where it starts, and its number beside it
        x, y = xy[0]
        rr = r * 1.9
        d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=(255, 255, 255), outline=c, width=max(2, w // 3))
        k = min(4, len(pts) - 1)
        dx, dy = pts[k][0] - pts[0][0], pts[k][1] - pts[0][1]
        nn = max(1e-6, math.hypot(dx, dy))
        tx, ty = x - dx / nn * scale * 6.0, y - dy / nn * scale * 6.0
        d.text((tx, ty), str(i + 1), fill=c, font=font, anchor="mm", stroke_width=2, stroke_fill=(255, 255, 255))


def sheet(chars, cps, path, cols, cell, numbered, label=True, colour=None, width=WIDTH, glyphs=None):
    rows = (len(cps) + cols - 1) // cols
    lab = 20 if label else 0
    img = Image.new("RGB", (cols * cell, rows * (cell + lab)), "white")
    d = ImageDraw.Draw(img)
    scale = cell / 109.0
    for i, cp in enumerate(cps):
        ox, oy = (i % cols) * cell, (i // cols) * (cell + lab)
        frame(d, ox, oy + lab, cell, guides=numbered)
        if label:
            d.text((ox + 4, oy + 3), "U+%04X  %d" % (cp, len(chars.get(cp, []))), fill=(90, 90, 90), font=_font(13))
        if glyphs is not None:
            glyphs.draw(img, cp, ox, oy + lab, scale, (222, 222, 222))
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
        glyphs.draw(img, cp, ox + cell, oy, scale, (0, 0, 0))
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
    ap.add_argument("--only", default="")
    ap.add_argument("--prefix", default="")
    a = ap.parse_args(argv[1:])
    os.makedirs(a.out, exist_ok=True)
    chars = load(a.xml)
    order = [ord(c) for grp in L.GROUPS for c in grp[1]]
    if a.only:
        order = [ord(c) for c in a.only if ord(c) in chars]
    glyphs = Glyphs(a.font) if a.font else None
    p = lambda n: os.path.join(a.out, a.prefix + n)
    made = []
    for k, cps in enumerate(pages(order, 20), 1):
        made.append(sheet(chars, cps, p("letters_%d.png" % k), 5, 300, True, glyphs=glyphs))
    made.append(sheet(chars, order, p("plain.png"), 17, 110, False, False, (0, 0, 0)))
    if glyphs is not None:
        for k, cps in enumerate(pages(order, 24), 1):
            made.append(compare(chars, cps, glyphs, p("compare_%d.png" % k), 4, 180))
    for m in made:
        print(m)


if __name__ == "__main__":
    main(sys.argv)
