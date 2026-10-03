"""Compose every Hangul syllable from the hand-drawn jamo and write them out as
KanjiVG-style XML for Kana's bake (fude/lang/ja/bake.c, fude_bake_kanjivg).

    python3 apps/hangul/tools/strokes/compose.py [out.xml]

writes data/raw/hangulvg.xml (git-ignored) by default: the 51 compatibility jamo
U+3131..U+3163 in their chart shapes, then the 11,172 syllables U+AC00..U+D7A3.

Layouts (all coordinates in KanjiVG's 109-unit box, Y down), each without and
with a final consonant at the bottom:
  V  vertical vowel (ㅏㅐㅑㅒㅓㅔㅕㅖㅣ): initial on the left, vowel on the right
  H  horizontal vowel (ㅗㅛㅜㅠㅡ): initial on top, vowel below
  C  compound vowel (ㅘㅙㅚㅝㅞㅟㅢ): initial top-left, the horizontal part below
     it, the vertical part on the right
"""

import os
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import jamo as J                                             # noqa: E402
from jamo import Box, Style                                  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
OUT = os.path.join(ROOT, "data", "raw", "hangulvg.xml")

INITIALS = "ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ"
MEDIALS = "ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ"
FINALS = [None] + list("ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ")

# Horizontals rise slightly to the right, as Korean handwriting is taught (가로획은
# 오른쪽이 약간 올라가게). Done as a vertical shear of the finished character about
# its centre, so every joint stays where it was and verticals stay vertical.
RISE = 0.03
CENTRE = 54.5

# The frame a full syllable fills.
X0, X1 = 13.0, 96.0
Y0, Y1 = 11.0, 98.0


def decompose(cp):
    i = cp - 0xAC00
    return INITIALS[i // 588], MEDIALS[(i % 588) // 28], FINALS[i % 28]


class Group:
    """A <g> with a kvg:element: a jamo, holding its strokes or its parts."""
    __slots__ = ("element", "position", "children")

    def __init__(self, element, position=None, children=None):
        self.element = element
        self.position = position
        self.children = children or []

    def map(self, f):
        return Group(self.element, self.position, [c.map(f) for c in self.children])


# --- consonants in a box ------------------------------------------------------------------

# Optical sizing: how much of its nominal box each consonant leaves empty, as
# fractions (left, top, right, bottom). Round and boxed letters look bigger than
# open ones of the same extent, so they are drawn smaller.
INSET = {
    "V": {
        "ㄱ": (0, 0, 0, 0), "ㄴ": (0.06, 0.02, 0.02, 0.10), "ㄷ": (0.04, 0.08, 0.02, 0.10),
        "ㄹ": (0.04, 0.04, 0.02, 0.06), "ㅁ": (0.05, 0.08, 0.04, 0.10), "ㅂ": (0.05, 0.04, 0.05, 0.06),
        "ㅅ": (0, 0, 0, 0), "ㅇ": (0.08, 0.17, 0.08, 0.15), "ㅈ": (0, 0.02, 0, 0), "ㅊ": (0, -0.04, 0, 0),
        "ㅋ": (0, 0, 0, 0), "ㅌ": (0.04, 0.06, 0.02, 0.08), "ㅍ": (0.02, 0.08, 0.04, 0.10),
        "ㅎ": (0.04, -0.04, 0.04, 0.12),
    },
    "H": {
        "ㄱ": (0.02, 0, 0.02, 0), "ㄴ": (0.06, 0, 0.04, 0.02), "ㄷ": (0.04, 0.04, 0.04, 0.02),
        "ㄹ": (0.04, 0, 0.04, 0), "ㅁ": (0.08, 0.04, 0.08, 0.02), "ㅂ": (0.08, 0.02, 0.08, 0.02),
        "ㅅ": (0, 0, 0, 0), "ㅇ": (0.08, 0.06, 0.08, 0.04), "ㅈ": (0.02, 0, 0.02, 0), "ㅊ": (0.02, -0.06, 0.02, 0),
        "ㅋ": (0.02, 0, 0.02, 0), "ㅌ": (0.04, 0.02, 0.04, 0.02), "ㅍ": (0.04, 0.04, 0.04, 0.02),
        "ㅎ": (0.06, -0.06, 0.06, 0),
    },
    "F": {
        "ㄱ": (0.04, 0.04, 0.04, 0), "ㄴ": (0.06, 0, 0.02, 0.04), "ㄷ": (0.04, 0.06, 0.04, 0.04),
        "ㄹ": (0.04, 0, 0.04, 0), "ㅁ": (0.10, 0.08, 0.10, 0.04), "ㅂ": (0.10, 0.04, 0.10, 0.02),
        "ㅅ": (0.08, 0.02, 0.08, 0), "ㅇ": (0.14, 0.08, 0.14, 0.04), "ㅈ": (0.08, 0.02, 0.08, 0), "ㅊ": (0.08, -0.02, 0.08, 0),
        "ㅋ": (0.04, 0.02, 0.04, 0), "ㅌ": (0.04, 0.04, 0.04, 0.02), "ㅍ": (0.04, 0.04, 0.04, 0.02),
        "ㅎ": (0.06, -0.02, 0.06, 0),
    },
}
INSET["S"] = INSET["C"] = INSET["H"]

# A round letter's box is kept within these proportions (width / height).
ROUND_ASPECT = {"V": (0.75, 1.00), "H": (1.0, 1.55), "C": (0.95, 1.35), "F": (1.15, 1.60), "S": (0.9, 1.25)}


def _fit_round(b, kind):
    lo, hi = ROUND_ASPECT[kind]
    a = b.w / b.h
    if a > hi:
        w = b.h * hi
        return Box(b.cx - w / 2, b.y0, b.cx + w / 2, b.y1)
    if a < lo:
        h = b.w / lo
        return Box(b.x0, b.cy - h / 2, b.x1, b.cy + h / 2)
    return b


def consonant(c, b, kind, position=None):
    """A Group for consonant c (basic, double or cluster) filling box b."""
    parts = J.DOUBLE.get(c) or J.CLUSTER.get(c)
    if not parts:
        return Group(c, position, _basic(c, b, Style(kind)))
    gap = max(6.0, 0.14 * b.w) if kind != "F" else max(5.5, 0.09 * b.w)
    # ㅅ and ㅈ face a neighbour with the low end of a diagonal: they can come closer.
    gap *= {0: 1.0, 1: 0.6, 2: 0.3}[(parts[0] in "ㅅㅈ") + (parts[1] in "ㅅㅈ")]
    split = _split(parts)
    xm = b.x0 + split * (b.w - gap)
    lb = Box(b.x0, b.y0, xm, b.y1)
    rb = Box(xm + gap, b.y0, b.x1, b.y1)
    return Group(c, position, [Group(parts[0], "left", _basic(parts[0], lb, Style(kind, "L"))),
                               Group(parts[1], "right", _basic(parts[1], rb, Style(kind, "R")))])


def _split(parts):
    """The left half's share of a pair's width."""
    a, b = parts
    if a == b:
        return 0.48
    if a == "ㄹ":
        return 0.47
    if a == "ㄴ":
        return 0.42
    return 0.5


def _basic(c, b, st):
    ins = INSET[st.kind][c]
    if st.role is not None:
        # Halves of a pair: less optical shrinking sideways, there is no room for it.
        ins = (ins[0] * 0.5, ins[1], ins[2] * 0.5, ins[3])
    box = b.inset(*ins)
    if c == "ㅇ":
        box = _fit_round(box, st.kind)
    return J.CONSONANT[c](box, st)


# --- vowels ---------------------------------------------------------------------------------

# Columns of the vertical vowels: the long verticals' x, the short strokes'
# length, and where the vowel's ink starts on the left (the initial stops short
# of it by GAP_V or GAP_STUB).
COLUMN = {
    "ㅏ": dict(xa=75.0, sl=18.0), "ㅑ": dict(xa=75.0, sl=18.0),
    "ㅓ": dict(xa=84.0, sl=19.0), "ㅕ": dict(xa=84.0, sl=19.0),
    "ㅐ": dict(xa=69.0, xb=87.0, sl=0.0), "ㅒ": dict(xa=69.0, xb=87.0, sl=0.0),
    "ㅔ": dict(xa=74.0, xb=89.0, sl=15.0), "ㅖ": dict(xa=74.0, xb=89.0, sl=15.0),
    "ㅣ": dict(xa=79.0, sl=0.0),
}
GAP_V = 11.0      # initial to a vertical (ㅏ ㅑ ㅐ ㅒ ㅣ)
GAP_STUB = 6.0    # initial to a short stroke on the left (ㅓ ㅕ ㅔ ㅖ)


def _left_ink(v):
    col = COLUMN[v]
    return col["xa"] - (col["sl"] if v in "ㅓㅕㅔㅖ" else 0.0)


def _initial_right(v):
    return _left_ink(v) - (GAP_STUB if v in "ㅓㅕㅔㅖ" else GAP_V)


def _vertical_geom(v, yt, yb, ys_one, ys_two, inner):
    g = dict(COLUMN[v])
    g.update(yt=yt, yb=yb, inner=inner)
    if v in "ㅑㅒㅕㅖ":
        g["ys"], g["ys2"] = ys_two
    else:
        g["ys"] = ys_one
    return g


# --- layouts ----------------------------------------------------------------------------------

def layout_v(ini, v, fin):
    """Initial left, vertical vowel right; final (if any) across the bottom."""
    groups = []
    right = _initial_right(v)
    if fin is None:
        ib = Box(15.5, 19.0, right, 83.0)
        g = _vertical_geom(v, Y0, Y1, 48.0, (38.0, 59.0), 7.0)
    else:
        ib = Box(15.5, 13.0, right, 57.0)
        g = _vertical_geom(v, Y0, 62.0, 35.0, (27.0, 44.0), 4.0)
    groups.append(consonant(ini, ib, "V", "initial"))
    groups.append(Group(v, "medial", J.vowel_vertical(v, g)))
    if fin is not None:
        groups.append(final(fin, 69.0))
    return groups


def layout_h(ini, v, fin):
    """Initial on top, horizontal vowel below; final (if any) under that."""
    cx = 54.5
    if fin is None:
        if v in "ㅗㅛ":
            ib, y, tl = Box(23.0, 14.0, 86.0, 60.0), 85.0, 19.0
        elif v in "ㅜㅠ":
            ib, y, tl = Box(23.0, 14.0, 86.0, 50.0), 60.0, 37.0
        else:
            ib, y, tl = Box(23.0, 16.0, 86.0, 67.0), 80.0, 0.0
    else:
        if v in "ㅗㅛ":
            ib, y, tl = Box(26.0, 12.5, 83.0, 39.0), 56.0, 10.5
        elif v in "ㅜㅠ":
            ib, y, tl = Box(26.0, 12.5, 83.0, 36.0), 46.5, 13.5
        else:
            ib, y, tl = Box(26.0, 13.0, 83.0, 43.0), 55.0, 0.0
    g = dict(x0=X0, x1=X1, y=y, tl=tl, tx=cx + (1.5 if v == "ㅜ" else 0.0))
    if v in "ㅛㅠ":
        g["tx"], g["tx2"] = cx - 12.5, cx + 12.5
    groups = [consonant(ini, ib, "H", "initial"),
              Group(v, "medial", J.vowel_horizontal(v, g))]
    if fin is not None:
        groups.append(final(fin, 66.0 if v in "ㅗㅛㅡ" else 67.0))
    return groups


def layout_c(ini, v, fin):
    """Initial top-left, the horizontal part under it, the vertical part right."""
    hv, vv = J.COMPOUND[v]
    col_left = _left_ink(vv)
    bar_right = (col_left - 5.0) if vv in "ㅏㅐㅣ" else (COLUMN[vv]["xa"] - 5.0)
    if fin is None:
        if hv == "ㅗ":
            ib, y, tl = Box(17.0, 15.0, bar_right - 5.0, 55.5), 75.0, 13.5
        elif hv == "ㅜ":
            ib, y, tl = Box(17.0, 15.0, bar_right - 5.0, 47.0), 57.0, 37.0
        else:
            ib, y, tl = Box(17.0, 16.0, bar_right - 5.0, 62.0), 75.0, 0.0
        yb = Y1
        ys = 50.0 if hv != "ㅜ" else 68.0
        ys2 = (40.0, 58.0)
        inner = 7.0
    else:
        if hv == "ㅗ":
            ib, y, tl = Box(18.0, 12.0, bar_right - 6.0, 34.0), 48.5, 9.0
        elif hv == "ㅜ":
            ib, y, tl = Box(18.0, 12.0, bar_right - 6.0, 32.5), 41.5, 18.5
        else:
            ib, y, tl = Box(18.0, 12.0, bar_right - 6.0, 37.0), 48.0, 0.0
        yb = 62.0 if hv != "ㅜ" else 63.0
        ys = 33.0 if hv != "ㅜ" else 51.0
        ys2 = (27.0, 42.0)
        inner = 4.0
    tx = X0 + (bar_right - X0) * (0.50 if hv == "ㅗ" else 0.46)
    hgeom = dict(x0=X0 + 1.0, x1=bar_right, y=y, tl=tl, tx=tx)
    vgeom = _vertical_geom(vv, Y0, yb, ys, ys2, inner)
    if vv in "ㅓㅔ":
        # The short stroke of ㅓ/ㅔ sits under the bar, right of ㅜ's stroke.
        vgeom["sl"] = min(vgeom["sl"], COLUMN[vv]["xa"] - (tx + 6.0))
    hs = J.vowel_horizontal(hv, hgeom)
    vs = J.vowel_vertical(vv, vgeom)
    groups = [consonant(ini, ib, "C", "initial"),
              Group(v, "medial", [Group(hv, "left", hs), Group(vv, "right", vs)])]
    if fin is not None:
        groups.append(final(fin, 69.0))
    return groups


def final(fin, top):
    """The final consonant across the bottom."""
    pair = fin in J.DOUBLE or fin in J.CLUSTER
    if pair:
        b = Box(17.0, top, 92.0, Y1 - 1.0)
    else:
        b = Box(24.0, top, 85.0, Y1 - 1.0)
    return consonant(fin, b, "F", "final")


def syllable(cp):
    ini, v, fin = decompose(cp)
    if v in J.VERTICAL:
        groups = layout_v(ini, v, fin)
    elif v in J.HORIZONTAL:
        groups = layout_h(ini, v, fin)
    else:
        groups = layout_c(ini, v, fin)
    return _shear(groups)


def _shear(groups):
    f = lambda p: (p[0], p[1] - RISE * (p[0] - CENTRE))
    return [g.map(f) for g in groups]


# --- standalone jamo (chart shapes) -----------------------------------------------------------

def standalone(ch):
    """The children of a standalone jamo's top group."""
    if ch in J.COMPOUND:
        # As in a syllable with ㅇ, without the ㅇ: the parts where they sit.
        groups = layout_c("ㅇ", ch, None)
        return _shear([groups[1]])[0].children
    if ch in J.VERTICAL:
        return _shear([Group(ch, None, J.vowel_vertical(ch, _chart_vertical(ch)))])[0].children
    if ch in J.HORIZONTAL:
        return _shear([Group(ch, None, J.vowel_horizontal(ch, _chart_horizontal(ch)))])[0].children
    pair = ch in J.DOUBLE or ch in J.CLUSTER
    b = Box(16.0, 22.0, 93.0, 88.0) if pair else Box(25.0, 20.0, 84.0, 89.0)
    g = consonant(ch, b, "S")
    return _shear([g])[0].children


def _chart_vertical(v):
    # The vowel centred on its own: its ink spans about 26..83 across.
    widths = {"ㅏ": (44.0, None, 22.0), "ㅑ": (44.0, None, 22.0), "ㅓ": (65.0, None, 22.0), "ㅕ": (65.0, None, 22.0),
              "ㅐ": (42.0, 66.0, 0.0), "ㅒ": (42.0, 66.0, 0.0), "ㅔ": (55.0, 74.0, 18.0), "ㅖ": (55.0, 74.0, 18.0),
              "ㅣ": (54.5, None, 0.0)}
    xa, xb, sl = widths[v]
    g = dict(xa=xa, sl=sl, yt=14.0, yb=95.0, ys=52.0, ys2=42.0, inner=7.0)
    if xb is not None:
        g["xb"] = xb
    if v in "ㅑㅒㅕㅖ":
        g["ys"], g["ys2"] = 42.0, 62.0
    return g


def _chart_horizontal(v):
    g = dict(x0=14.0, x1=95.0, tx=54.5, tx2=None)
    if v in "ㅗㅛ":
        g.update(y=70.0, tl=30.0)
    elif v in "ㅜㅠ":
        g.update(y=40.0, tl=30.0)
    else:
        g.update(y=55.0, tl=0.0)
    if v in "ㅛㅠ":
        g["tx"], g["tx2"] = 41.0, 68.0
    return g


# --- XML ----------------------------------------------------------------------------------------

HEADER = """<?xml version="1.0" encoding="UTF-8"?>
<!--
Hangul stroke order data in KanjiVG's format, for Kana's bake.
Generated by apps/hangul/tools/strokes/compose.py from the hand-drawn jamo in
apps/hangul/tools/strokes/jamo.py. Do not edit: regenerate.
Our own work; licence to be decided (see apps/hangul/tools/strokes/README.md).
Centrelines in a 109 x 109 box, Y down; strokes numbered in writing order.
kvg:position on a syllable's jamo: initial, medial or final; inside a double
consonant, final cluster or compound vowel: left or right.
-->
<kanjivg xmlns:kvg='http://kanjivg.tagaini.net'>
"""


def character_xml(cp, children):
    """One <kanji> element: children are Groups (and, for single jamo, Strokes)."""
    hexid = "%05x" % cp
    out = ['<kanji id="kvg:kanji_%s">' % hexid,
           '<g id="kvg:%s" kvg:element="%s">' % (hexid, chr(cp))]
    counter = {"s": 0, "g": 0}

    def emit(node, depth):
        tab = "\t" * depth
        if isinstance(node, Group):
            counter["g"] += 1
            attrs = 'id="kvg:%s-g%d" kvg:element="%s"' % (hexid, counter["g"], node.element)
            if node.position:
                attrs += ' kvg:position="%s"' % node.position
            out.append("%s<g %s>" % (tab, attrs))
            for c in node.children:
                emit(c, depth + 1)
            out.append("%s</g>" % tab)
        else:
            counter["s"] += 1
            t = ' kvg:type="%s"' % node.type if node.type else ""
            out.append('%s<path id="kvg:%s-s%d"%s d="%s"/>' % (tab, hexid, counter["s"], t, node.d()))

    for c in children:
        emit(c, 1)
    out.append("</g>")
    out.append("</kanji>")
    return "\n".join(out) + "\n", counter["s"]


def characters():
    """(code point, children) for every character, in code point order."""
    for cp in range(0x3131, 0x3164):
        yield cp, standalone(chr(cp))
    for cp in range(0xAC00, 0xD7A4):
        yield cp, syllable(cp)


def main(argv):
    out = argv[1] if len(argv) > 1 else OUT
    os.makedirs(os.path.dirname(out), exist_ok=True)
    n = strokes = 0
    tmp = out + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        f.write(HEADER)
        for cp, children in characters():
            xml, k = character_xml(cp, children)
            f.write(xml)
            n += 1
            strokes += k
        f.write("</kanjivg>\n")
    os.replace(tmp, out)
    print("compose: %d characters, %d strokes -> %s" % (n, strokes, out))


if __name__ == "__main__":
    main(sys.argv)
