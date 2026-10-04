"""Contact sheets of the generated stroke data, for looking at it.

    python3 apps/hindi/tools/strokes/sheets.py OUT_DIR [--xml data/raw/hi/devanagarivg.xml]
            [--font apps/hindi/assets/fonts/NotoSansDevanagari-Regular.ttf] [--only अआ...] [--prefix name_]

Needs Pillow. Writes into OUT_DIR (keep it outside the repo):
  strokes_N.png   every character: each stroke in its own colour, a dot where
                  it starts and its number beside it, 30 to a page
  plain.png       every character in black at the app's stroke width
  compare_N.png   with --font: each character beside the font's glyph (to
                  compare proportions only; nothing is taken from the font),
                  24 to a page. A vowel sign is shown on a grey प, as the font
                  places it without the shaping engine (roughly)
  overlay_N.png   with --font: the strokes drawn thin over the font's glyph
                  (pale), narrowed as the letters are, 20 to a page

Paths are read back with the bake's rules (pen.parse_d), so this shows what the
app will get. Labels are in Devanagari when --font is given.
"""

import os
import sys
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pen import parse_d, flatten                              # noqa: E402
import letters as L                                           # noqa: E402

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:                                           # pragma: no cover
    sys.exit("sheets.py needs Pillow (pip install pillow)")

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
XML = os.path.join(ROOT, "data", "raw", "hi", "devanagarivg.xml")

KVG = "{http://kanjivg.tagaini.net}"
WIDTH = 3.2         # the app's stroke width in KanjiVG units (fude/study/chars/glyph.h)
EM = 95.48          # the font's em in box units: its headline at y 28, its baseline at y 84
CONTEXT = "प"  # प: the consonant a vowel sign is shown on

PALETTE = [(220, 50, 47), (38, 139, 210), (133, 153, 0), (211, 54, 130), (203, 75, 22), (42, 161, 152),
           (108, 113, 196), (181, 137, 0), (0, 100, 0), (160, 32, 240), (70, 70, 70), (255, 140, 0)]


def load(path):
    """{code point: [points, ...]} in stroke order."""
    chars = {}
    root = ET.parse(path).getroot()
    for k in root.iter("kanji"):
        cp = int(k.get("id").split("_")[1], 16)
        paths = [p for p in k.iter("path")]
        paths.sort(key=lambda p: int(p.get("id").rsplit("-s", 1)[1]))
        chars[cp] = [flatten(*parse_d(p.get("d")), 24) for p in paths]
    return chars


_fonts = {}
LABEL_FONT = []     # a font with Devanagari for the labels, when one is given


def _font(px, devanagari=False):
    key = (px, devanagari)
    if key not in _fonts:
        cands = (tuple(LABEL_FONT) if devanagari else ()) + (
            "/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/System/Library/Fonts/Helvetica.ttc",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf")
        for f in cands:
            if os.path.exists(f):
                _fonts[key] = ImageFont.truetype(f, px)
                break
        else:
            _fonts[key] = ImageFont.load_default()
    return _fonts[key]


def frame(d, ox, oy, size):
    d.rectangle([ox, oy, ox + size - 1, oy + size - 1], outline=(200, 200, 200))
    for y in (L.HL, L.BL):     # the headline and the baseline
        yy = oy + y * size / 109.0
        for t in range(0, size, 4):
            d.point((ox + t, yy), fill=(170, 210, 255))
    m = size / 2
    for t in range(0, size, 6):
        d.point((ox + m, oy + t), fill=(225, 225, 225))


def draw_char(d, strokes, ox, oy, scale, numbered, colour=None, width=WIDTH):
    w = max(1, int(round(width * scale)))
    r = w / 2.0
    for i, pts in enumerate(strokes):
        c = colour or PALETTE[i % len(PALETTE)]
        xy = [(ox + x * scale, oy + y * scale) for x, y in pts]
        d.line(xy, fill=c, width=w, joint="curve")
        for p in (xy[0], xy[-1]):
            d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=c)
    if numbered:
        font = _font(max(10, int(scale * 6)))
        for i, pts in enumerate(strokes):
            c = PALETTE[i % len(PALETTE)]
            x, y = ox + pts[0][0] * scale, oy + pts[0][1] * scale
            rr = max(r * 1.7, 3)
            d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=(255, 255, 255), outline=c, width=max(2, w // 3))
            # the number just off the start, away from the stroke's direction
            k = min(4, len(pts) - 1)
            dx, dy = pts[k][0] - pts[0][0], pts[k][1] - pts[0][1]
            n = max(1e-6, (dx * dx + dy * dy) ** 0.5)
            tx, ty = x - dx / n * scale * 5.5, y - dy / n * scale * 5.5
            d.text((tx, ty), str(i + 1), fill=c, font=font, anchor="mm", stroke_width=2, stroke_fill=(255, 255, 255))


def label(d, x, y, cp, n):
    if LABEL_FONT:
        d.text((x, y), chr(cp), fill=(60, 60, 60), font=_font(15, True))
        d.text((x + 26, y + 3), "U+%04X  %d" % (cp, n), fill=(110, 110, 110), font=_font(12))
    else:
        d.text((x, y + 3), "U+%04X  %d" % (cp, n), fill=(110, 110, 110), font=_font(12))


def pages(cps, per_page):
    return [cps[i:i + per_page] for i in range(0, len(cps), per_page)] if per_page else [cps]


def sheet(chars, cps, path, cols, cell, numbered, labelled=True, colour=None, width=WIDTH):
    rows = (len(cps) + cols - 1) // cols
    lab = 22 if labelled else 0
    img = Image.new("RGB", (cols * cell, rows * (cell + lab)), "white")
    d = ImageDraw.Draw(img)
    scale = cell / 109.0
    for i, cp in enumerate(cps):
        ox, oy = (i % cols) * cell, (i // cols) * (cell + lab)
        frame(d, ox, oy + lab, cell)
        if labelled:
            label(d, ox + 4, oy + 2, cp, len(chars.get(cp, [])))
        if cp in chars:
            draw_char(d, chars[cp], ox, oy + lab, scale, numbered, colour, width)
    img.save(path)
    return path


# --- the font's glyph, for comparison ------------------------------------------------------

def glyph_image(font_path, cp, cell, narrow=1.0, centre=None):
    """The font's glyph in a cell-sized greyscale mask (255 = ink), placed as the
    stroke data places the character: baseline at y 84, headline at y 28; a
    letter centred, a vowel sign on the context consonant centred (the sign's
    mask and the consonant's, separately). narrow: the letters' XS; centre:
    where the middle of a letter's ink goes, in box units (default the box's)."""
    scale = cell / 109.0
    f = ImageFont.truetype(font_path, max(8, int(round(EM * scale))))
    base_y = L.BL * scale
    big = cell * 2

    def render(text):
        im = Image.new("L", (big, cell), 0)
        ImageDraw.Draw(im).text((cell / 2, base_y), text, font=f, fill=255, anchor="ls")
        return im

    sign = L.LETTERS[cp].kind == "sign"
    if not sign:
        m = render(chr(cp))
        box = m.getbbox()
        if box is None:
            return Image.new("L", (cell, cell), 0), None
        if narrow != 1.0:
            m = m.resize((max(1, int(round(big * narrow))), cell), Image.LANCZOS)
            box = m.getbbox()
        out = Image.new("L", (cell, cell), 0)
        cx = (box[0] + box[2]) / 2.0
        want = (L.CENTRE if centre is None else centre) * scale
        out.paste(m, (int(round(want - cx)), 0))
        return out, None
    # A sign on the context consonant: shown as the font draws the pair
    # without shaping (the i-sign is stored after the consonant but drawn
    # before it, so its pair is written sign first).
    text = chr(cp) + CONTEXT if cp == 0x093F else CONTEXT + chr(cp)
    pair = render(text)
    cons = Image.new("L", (big, cell), 0)
    adv = f.getlength(chr(cp)) if cp == 0x093F else 0.0
    ImageDraw.Draw(cons).text((cell / 2 + adv, base_y), CONTEXT, font=f, fill=255, anchor="ls")
    if narrow != 1.0:
        w = max(1, int(round(big * narrow)))
        pair, cons = pair.resize((w, cell), Image.LANCZOS), cons.resize((w, cell), Image.LANCZOS)
    cbox = cons.getbbox()
    cx = (cbox[0] + cbox[2]) / 2.0
    want = (L.CONSONANT["x0"] + L.CONSONANT["x1"]) / 2.0 * scale
    shift = int(round(want - cx))
    out_pair = Image.new("L", (cell, cell), 0)
    out_cons = Image.new("L", (cell, cell), 0)
    out_pair.paste(pair, (shift, 0))
    out_cons.paste(cons, (shift, 0))
    return out_pair, out_cons


def compare(chars, cps, font_path, path, cols, cell):
    """Ours (left) beside the font's glyph (right)."""
    pair = cell * 2 + 8
    lab = 22
    rows = (len(cps) + cols - 1) // cols
    img = Image.new("RGB", (cols * pair, rows * (cell + lab)), "white")
    d = ImageDraw.Draw(img)
    scale = cell / 109.0
    for i, cp in enumerate(cps):
        ox, oy = (i % cols) * pair, (i // cols) * (cell + lab)
        label(d, ox + 4, oy + 2, cp, len(chars.get(cp, [])))
        oy += lab
        frame(d, ox, oy, cell)
        frame(d, ox + cell, oy, cell)
        draw_char(d, chars[cp], ox, oy, scale, False, (0, 0, 0))
        g, cons = glyph_image(font_path, cp, cell)
        img.paste((0, 0, 0), (ox + cell, oy), g)
        if cons is not None:
            img.paste((185, 185, 185), (ox + cell, oy), cons)
    img.save(path)
    return path


# The letters drawn on another one's frame (letters.py builds them from its
# strokes): the font's glyph is placed where it falls in that letter's frame.
FRAME = {0x0906: 0x0905, 0x0911: 0x0905, 0x0913: 0x0905, 0x0914: 0x0905, 0x0908: 0x0907, 0x090A: 0x0909,
         0x090D: 0x090F, 0x0910: 0x090F}


def _font_centre(font_path, cp, cell):
    """Where the middle of the font's glyph falls in the box, once narrowed and
    moved with the drawing. The letters were designed on the font's glyph
    with its ink centred in the box (or on the glyph of the letter in FRAME)."""
    f = ImageFont.truetype(font_path, 1000)
    nat = lambda c: sum(f.getbbox(chr(c), anchor="ls")[0::2]) / 2.0 * EM / 1000.0
    lt = L.LETTERS[cp]
    frame_cp = FRAME.get(cp, lt.base or cp)
    font_c = L.CENTRE + nat(cp) - nat(frame_cp)
    strokes, n = L.strokes_of(cp)
    xs = [p[0] for st in strokes[:n] for p in st.flat(12)]
    ours = (min(xs) + max(xs)) * 0.5
    return L.CENTRE + (font_c - ours) * L.XS


def overlay(chars, cps, font_path, path, cols, cell):
    """The strokes, thin and numbered, over the font's glyph (pale), narrowed
    and moved as the letters are; vowel signs over the font's sign on a paler
    consonant."""
    lab = 22
    rows = (len(cps) + cols - 1) // cols
    img = Image.new("RGB", (cols * cell, rows * (cell + lab)), "white")
    d = ImageDraw.Draw(img)
    scale = cell / 109.0
    for i, cp in enumerate(cps):
        ox, oy = (i % cols) * cell, (i // cols) * (cell + lab)
        label(d, ox + 4, oy + 2, cp, len(chars.get(cp, [])))
        oy += lab
        sign = L.LETTERS[cp].kind == "sign"
        g, cons = glyph_image(font_path, cp, cell, 1.0 if sign else L.XS,
                              None if sign else _font_centre(font_path, cp, cell))
        img.paste((205, 205, 205), (ox, oy), g)
        if cons is not None:
            img.paste((235, 235, 235), (ox, oy), cons)
        frame(d, ox, oy, cell)
        draw_char(d, chars[cp], ox, oy, scale, True, None, 1.6)
    img.save(path)
    return path


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
    if a.font:
        LABEL_FONT.append(a.font)
    chars = load(a.xml)
    cps = sorted(chars)
    if a.only:
        want = {ord(c) for c in a.only if ord(c) in chars}
        cps = [c for c in cps if c in want]
    p = lambda n: os.path.join(a.out, a.prefix + n)
    made = []
    for k, page in enumerate(pages(cps, 30), 1):
        made.append(sheet(chars, page, p("strokes_%d.png" % k), 6, 220, True))
    made.append(sheet(chars, cps, p("plain.png"), 15, 110, False, True, (0, 0, 0)))
    if a.font:
        for k, page in enumerate(pages(cps, 24), 1):
            made.append(compare(chars, page, a.font, p("compare_%d.png" % k), 4, 200))
        for k, page in enumerate(pages(cps, 20), 1):
            made.append(overlay(chars, page, a.font, p("overlay_%d.png" % k), 5, 260))
    for m in made:
        print(m)


if __name__ == "__main__":
    main(sys.argv)
