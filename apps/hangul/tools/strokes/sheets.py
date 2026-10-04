# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

"""Contact sheets of the generated stroke data, for looking at it.

    python3 apps/hangul/tools/strokes/sheets.py OUT_DIR [--xml data/raw/hangulvg.xml]
            [--font NotoSansKR-Light.otf] [--all] [--sample 가각...] [--prefix name_]

Needs Pillow. Writes into OUT_DIR (keep it outside the repo):
  jamo.png       the 51 standalone jamo: each stroke in its own colour, a dot
                 where it starts and its number beside the dot
  syllables_N.png  the sample syllables (SAMPLE, or --sample), drawn the same
                 way, 42 to a page
  plain.png      the sample syllables in black at the app's stroke width
  compare_N.png  with --font: each sample syllable beside the font's glyph (for
                 proportions only; nothing is taken from the font), 24 a page
  all_*.png      with --all: every syllable, plain, one sheet per initial

Paths are read back with the bake's rules (pen.parse_d), so this shows what the
app will get.
"""

import os
import sys
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pen import parse_d, flatten                              # noqa: E402

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:                                           # pragma: no cover
    sys.exit("sheets.py needs Pillow (pip install pillow)")

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
XML = os.path.join(ROOT, "data", "raw", "hangulvg.xml")

KVG = "{http://kanjivg.tagaini.net}"
WIDTH = 3.2   # the app's stroke width in KanjiVG units (fude/study/chars/glyph.h)

# Every layout, every initial, medial and final, and the hard ones.
SAMPLE = (
    "가각갔강개걔고과괜괘귀그긔꿈꿰뭘쉡앉않읽흙핥없닭빼쀼쯩홅똠쏟끝뷁됐쇠의위왜웨워"
    "나너녀네노뇨누뉴느니다더도두드디따떠또뚜라러로루르리마머모무미바버보부비"
    "사서소수시싸써쏘쑤아어오우이자저조주지짜쩌쪼쭈차처초추치카커코쿠키타터토투티"
    "파퍼포푸피하허호후히깎넋앉많닫갈닭삶밟곬핥읊싫값갓있강잦꽃부엌밭앞좋"
    "괴뢰퇴희늬쥐줘뒤춰훼퀘뭬쫴쐐꽈롸봐야약계례"
)

PALETTE = [(220, 50, 47), (38, 139, 210), (133, 153, 0), (211, 54, 130), (203, 75, 22), (42, 161, 152),
           (108, 113, 196), (181, 137, 0), (0, 100, 0), (160, 32, 240), (70, 70, 70), (255, 140, 0),
           (0, 128, 128), (128, 0, 0), (0, 0, 128), (128, 128, 0), (255, 20, 147), (0, 191, 255),
           (139, 69, 19), (46, 139, 87)]


def load(path):
    """{code point: [(type, points), ...]} in stroke order."""
    chars = {}
    root = ET.parse(path).getroot()
    for k in root.iter("kanji"):
        cp = int(k.get("id").split("_")[1], 16)
        paths = [p for p in k.iter("path")]
        paths.sort(key=lambda p: int(p.get("id").rsplit("-s", 1)[1]))
        strokes = []
        for p in paths:
            start, segs = parse_d(p.get("d"))
            strokes.append((p.get(KVG + "type"), flatten(start, segs, 24)))
        chars[cp] = strokes
    return chars


def draw_char(d, strokes, ox, oy, scale, numbered, colour=None, width=WIDTH):
    w = max(1, int(round(width * scale)))
    r = w / 2.0
    for i, (_, pts) in enumerate(strokes):
        c = colour or PALETTE[i % len(PALETTE)]
        xy = [(ox + x * scale, oy + y * scale) for x, y in pts]
        d.line(xy, fill=c, width=w, joint="curve")
        for p in (xy[0], xy[-1]):
            d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=c)
    if numbered:
        font = _font(int(scale * 7))
        for i, (_, pts) in enumerate(strokes):
            c = PALETTE[i % len(PALETTE)]
            x, y = ox + pts[0][0] * scale, oy + pts[0][1] * scale
            rr = r * 1.9
            d.ellipse([x - rr, y - rr, x + rr, y + rr], fill=(255, 255, 255), outline=c, width=max(2, w // 3))
            # the number just off the start, away from the stroke's direction
            dx, dy = pts[min(3, len(pts) - 1)][0] - pts[0][0], pts[min(3, len(pts) - 1)][1] - pts[0][1]
            n = max(1e-6, (dx * dx + dy * dy) ** 0.5)
            tx, ty = x - dx / n * scale * 5.5, y - dy / n * scale * 5.5
            d.text((tx, ty), str(i + 1), fill=c, font=font, anchor="mm", stroke_width=2, stroke_fill=(255, 255, 255))


_fonts = {}
LABEL_FONT = []     # a font with Hangul for the labels, when one is given


def _font(px):
    if px not in _fonts:
        for f in tuple(LABEL_FONT) + ("/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/System/Library/Fonts/Helvetica.ttc",
                  "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"):
            if os.path.exists(f):
                _fonts[px] = ImageFont.truetype(f, px)
                break
        else:
            _fonts[px] = ImageFont.load_default()
    return _fonts[px]


def frame(d, ox, oy, size):
    d.rectangle([ox, oy, ox + size - 1, oy + size - 1], outline=(200, 200, 200))
    m = size / 2
    for t in range(0, size, 6):
        d.point((ox + m, oy + t), fill=(225, 225, 225))
        d.point((ox + t, oy + m), fill=(225, 225, 225))


def sheet(chars, cps, path, cols, cell, numbered, label=True, colour=None, width=WIDTH, per_page=0):
    if per_page and len(cps) > per_page:
        stem, ext = os.path.splitext(path)
        return [sheet(chars, cps[i:i + per_page], "%s_%d%s" % (stem, i // per_page + 1, ext), cols, cell, numbered, label, colour, width)
                for i in range(0, len(cps), per_page)]
    rows = (len(cps) + cols - 1) // cols
    lab = 18 if label else 0
    img = Image.new("RGB", (cols * cell, rows * (cell + lab)), "white")
    d = ImageDraw.Draw(img)
    scale = cell / 109.0
    for i, cp in enumerate(cps):
        ox, oy = (i % cols) * cell, (i // cols) * (cell + lab)
        frame(d, ox, oy + lab, cell)
        if label:
            d.text((ox + 4, oy + 2), "%s U+%04X %d" % (chr(cp), cp, len(chars.get(cp, []))), fill=(90, 90, 90), font=_font(13))
        if cp in chars:
            draw_char(d, chars[cp], ox, oy + lab, scale, numbered, colour, width)
    img.save(path)
    return path


def compare(chars, cps, font_path, path, cols, cell, per_page=0):
    """Ours (left) beside the font's glyph (right), em box mapped to the 109 box.
    With per_page, several numbered files (path's stem + _1, _2, ...)."""
    if per_page and len(cps) > per_page:
        stem, ext = os.path.splitext(path)
        return [compare(chars, cps[i:i + per_page], font_path, "%s_%d%s" % (stem, i // per_page + 1, ext), cols, cell)
                for i in range(0, len(cps), per_page)]
    pair = cell * 2 + 8
    rows = (len(cps) + cols - 1) // cols
    img = Image.new("RGB", (cols * pair, rows * cell), "white")
    d = ImageDraw.Draw(img)
    f = ImageFont.truetype(font_path, cell)
    scale = cell / 109.0
    for i, cp in enumerate(cps):
        ox, oy = (i % cols) * pair, (i // cols) * cell
        frame(d, ox, oy, cell)
        frame(d, ox + cell, oy, cell)
        draw_char(d, chars[cp], ox, oy, scale, False, (0, 0, 0))
        adv = f.getlength(chr(cp))
        d.text((ox + cell + (cell - adv) / 2, oy + 0.88 * cell), chr(cp), font=f, fill=(0, 0, 0), anchor="ls")
    img.save(path)
    return path


def main(argv):
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--xml", default=XML)
    ap.add_argument("--font")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--sample", default=SAMPLE)
    ap.add_argument("--prefix", default="")
    a = ap.parse_args(argv[1:])
    os.makedirs(a.out, exist_ok=True)
    if a.font:
        LABEL_FONT.append(a.font)
    chars = load(a.xml)
    seen = set()
    sample = [ord(c) for c in a.sample if not (c in seen or seen.add(c))]
    p = lambda n: os.path.join(a.out, a.prefix + n)
    made = [sheet(chars, list(range(0x3131, 0x3164)), p("jamo.png"), 10, 200, True)]
    r = sheet(chars, sample, p("syllables.png"), 7, 220, True, per_page=42)
    made += r if isinstance(r, list) else [r]
    made.append(sheet(chars, sample, p("plain.png"), 16, 120, False, False, (0, 0, 0)))
    if a.font:
        r = compare(chars, sample, a.font, p("compare.png"), 4, 200, 24)
        made.extend(r if isinstance(r, list) else [r])
    if a.all:
        for i in range(19):     # one sheet per initial
            cps = list(range(0xAC00 + i * 588, 0xAC00 + (i + 1) * 588))
            made.append(sheet(chars, cps, p("all_%02d.png" % i), 28, 64, False, False, (0, 0, 0)))
    for m in made:
        print(m)


if __name__ == "__main__":
    main(sys.argv)
