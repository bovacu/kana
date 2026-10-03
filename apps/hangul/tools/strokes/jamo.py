"""The Hangul jamo, drawn by hand as centreline strokes.

Our own work: every shape below was drawn for this project, in code. Nothing
here is traced or extracted from a font or from anyone else's stroke data.

Each consonant is a function of the box it must fill and a Style:

    kind  'V'  initial beside a vertical vowel (가): open strokes slant and sweep
          'H'  initial above a horizontal vowel (고)
          'C'  initial top-left of a compound vowel (과): as 'H', a little narrower
          'F'  final consonant (각): wide and low, upright
          'S'  standalone, as in a jamo chart
    role  None, or 'L' / 'R' when it is one half of a double consonant (ㄲ) or a
          final cluster (ㄳ); halves are narrower, and a left half keeps its
          strokes clear of its right neighbour.

and returns its strokes in writing order. Vowels are functions of explicit
positions (the column of their long verticals, the line of their long bar),
chosen by compose.py's layouts.

STROKE ORDER SOURCES (cited per jamo below)
  [C]  Wikimedia Commons, Category:Hangeul_stroke_order: the public-domain
       diagrams only — the per-letter PNGs ("ㄱ (giyeok) stroke order.png" ...
       "ㅣ (i) stroke order.png"), the GIF set ("Giuk stroke order.gif" ...
       "Hiut stroke order.gif") and "Korean vowel strokes.gif". Used for order
       and direction only. They draw printed (명조) letterforms, so for ㅈ ㅊ ㅎ
       they show the printed form; see the house choices.
  [K]  The conventional order taught in Korean primary schools and handwriting
       workbooks: broadly top to bottom, then left to right, an enclosing ㄴ
       last. NIKL (국립국어원) does not regulate jamo stroke order; this school
       order is the one learners meet, and it agrees with [C] letter by letter.
House choices (ㅈ ㅊ ㅎ, ㄱ's shapes, ㅗ, ...) are in README.md.

Coordinates: KanjiVG's 109-unit box, Y down. Directions: horizontals left to
right, verticals top to bottom, ㇒ top-right to bottom-left, ㇏ top-left to
bottom-right, ㅇ from the top, counter-clockwise.
"""

from pen import Stroke, poly, ellipse, bezier_point, T_H, T_S, T_P, T_N, T_HZ, T_HP, T_SZ


# --- tables ---------------------------------------------------------------------------

# Strokes per basic jamo (the validator checks every syllable against these sums).
STROKES = {
    "ㄱ": 1, "ㄴ": 1, "ㄷ": 2, "ㄹ": 3, "ㅁ": 3, "ㅂ": 4, "ㅅ": 2, "ㅇ": 1,
    "ㅈ": 2, "ㅊ": 3, "ㅋ": 2, "ㅌ": 3, "ㅍ": 4, "ㅎ": 3,
    "ㅏ": 2, "ㅐ": 3, "ㅑ": 3, "ㅒ": 4, "ㅓ": 2, "ㅔ": 3, "ㅕ": 3, "ㅖ": 4,
    "ㅗ": 2, "ㅛ": 3, "ㅜ": 2, "ㅠ": 3, "ㅡ": 1, "ㅣ": 1,
}

# Jamo written as two others, in order.
DOUBLE = {"ㄲ": "ㄱㄱ", "ㄸ": "ㄷㄷ", "ㅃ": "ㅂㅂ", "ㅆ": "ㅅㅅ", "ㅉ": "ㅈㅈ"}
CLUSTER = {"ㄳ": "ㄱㅅ", "ㄵ": "ㄴㅈ", "ㄶ": "ㄴㅎ", "ㄺ": "ㄹㄱ", "ㄻ": "ㄹㅁ", "ㄼ": "ㄹㅂ",
           "ㄽ": "ㄹㅅ", "ㄾ": "ㄹㅌ", "ㄿ": "ㄹㅍ", "ㅀ": "ㄹㅎ", "ㅄ": "ㅂㅅ"}
COMPOUND = {"ㅘ": "ㅗㅏ", "ㅙ": "ㅗㅐ", "ㅚ": "ㅗㅣ", "ㅝ": "ㅜㅓ", "ㅞ": "ㅜㅔ", "ㅟ": "ㅜㅣ", "ㅢ": "ㅡㅣ"}

VERTICAL = "ㅏㅐㅑㅒㅓㅔㅕㅖㅣ"
HORIZONTAL = "ㅗㅛㅜㅠㅡ"


def stroke_count(j):
    """Strokes in any jamo: a basic one, or the sum of its parts."""
    parts = DOUBLE.get(j) or CLUSTER.get(j) or COMPOUND.get(j)
    return sum(STROKES[p] for p in parts) if parts else STROKES[j]


# --- boxes and styles ----------------------------------------------------------------

class Box:
    __slots__ = ("x0", "y0", "x1", "y1")

    def __init__(self, x0, y0, x1, y1):
        self.x0, self.y0, self.x1, self.y1 = float(x0), float(y0), float(x1), float(y1)

    @property
    def w(self):
        return self.x1 - self.x0

    @property
    def h(self):
        return self.y1 - self.y0

    @property
    def cx(self):
        return (self.x0 + self.x1) * 0.5

    @property
    def cy(self):
        return (self.y0 + self.y1) * 0.5

    def x(self, u):
        return self.x0 + u * self.w

    def y(self, v):
        return self.y0 + v * self.h

    def sub(self, u0, v0, u1, v1):
        """A part of the box, in fractions of it."""
        return Box(self.x(u0), self.y(v0), self.x(u1), self.y(v1))

    def inset(self, l, t, r, b):
        """Shrunk by fractions of its width (l, r) and height (t, b)."""
        return Box(self.x0 + l * self.w, self.y0 + t * self.h, self.x1 - r * self.w, self.y1 - b * self.h)

    def __repr__(self):
        return "Box(%.1f, %.1f, %.1f, %.1f)" % (self.x0, self.y0, self.x1, self.y1)


class Style:
    __slots__ = ("kind", "role")

    def __init__(self, kind, role=None):
        self.kind = kind
        self.role = role

    @property
    def slant(self):
        return self.kind == "V"


def _on_curve(s, seg, t):
    """The point at t on segment seg of a stroke."""
    p0 = s.start if seg == 0 else s.segs[seg - 1][2]
    c1, c2, p3 = s.segs[seg]
    return bezier_point(p0, c1, c2, p3, t)


def _t_at_y(s, seg, y):
    """t where segment seg of s (going down) reaches height y."""
    lo, hi = 0.0, 1.0
    for _ in range(40):
        mid = (lo + hi) * 0.5
        if _on_curve(s, seg, mid)[1] < y:
            lo = mid
        else:
            hi = mid
    return (lo + hi) * 0.5


def _falling(p, end):
    """A ㇏ from p to end: steep at first, flattening out as it lands."""
    dx, dy = end[0] - p[0], end[1] - p[1]
    s = Stroke(p[0], p[1], T_N)
    s.curve(p[0] + 0.22 * dx, p[1] + 0.40 * dy, end[0] - 0.40 * dx, end[1] - 0.10 * dy, end[0], end[1])
    return s


# --- consonants -------------------------------------------------------------------------
#
# Each returns its strokes in writing order. b: the box its ink fills.

def giyeok(b, st):
    # ㄱ, 1 stroke: across, then down. [C] "ㄱ (giyeok) stroke order.png", "Giuk
    # stroke order.gif"; [K]. Before a vertical vowel the down stroke sweeps to the
    # lower left (㇇, as 가 is written); elsewhere it falls straight (㇕).
    x0, y0, x1, y1, w, h = b.x0, b.y0, b.x1, b.y1, b.w, b.h
    if st.slant:
        if st.role == "L":
            # ㄲ's first: a short, gentle sweep that keeps clear of the second.
            s = Stroke(x0, y0, T_HP).line(x1, y0)
            s.curve(x1, y0 + 0.40 * h, x1 - 0.10 * w, y0 + 0.75 * h, x1 - 0.55 * w, y1)
            return [s]
        s = Stroke(x0 + 0.07 * w, y0, T_HP).line(x1, y0)
        s.curve(x1, y0 + 0.36 * h, x1 - 0.22 * w, y0 + 0.80 * h, x0, y1)
        return [s]
    s = Stroke(x0, y0, T_HZ).line(x1, y0)
    if st.kind in "HC":
        s.curve(x1, y0 + 0.45 * h, x1, y0 + 0.80 * h, x1 - 0.05 * w, y1)
    else:
        s.line(x1, y1)
    return [s]


def nieun(b, st):
    # ㄴ, 1 stroke: down, then across (㇗). [C] "ㄴ (nieun) stroke order.png", "Niun
    # stroke order.gif"; [K]. Beside a vertical vowel the foot lifts a little at its
    # end, as handwriting does.
    return [_ell(b.x0, b.y0, b.x1, b.y1, b.w, b.h, st.slant)]


def _ell(x0, y0, x1, y1, w, h, lift):
    """The ㄴ shape (also the last stroke of ㄷ ㄹ ㅌ)."""
    s = Stroke(x0, y0, T_SZ).line(x0, y1)
    if lift:
        rise = min(0.10 * h, 4.0)
        s.curve(x0 + 0.45 * w, y1, x0 + 0.80 * w, y1 - 0.15 * rise, x1, y1 - rise)
    else:
        s.line(x1, y1)
    return s


def digeut(b, st):
    # ㄷ, 2 strokes: the top bar, then ㄴ from its left end. [C] "ㄷ (digeut) stroke
    # order.png", "Digut stroke order.gif"; [K].
    x0, y0, x1, y1, w, h = b.x0, b.y0, b.x1, b.y1, b.w, b.h
    top_r = x1 - (0.10 if st.slant else 0.04) * w
    return [poly([(x0, y0), (top_r, y0)], T_H), _ell(x0, y0, x1, y1, w, h, st.slant)]


def rieul(b, st):
    # ㄹ, 3 strokes: ㄱ (across and down), the middle bar, then ㄴ. [C] "ㄹ (rieul)
    # stroke order.png", "Riul stroke order.gif"; [K].
    x0, y0, x1, y1, w, h = b.x0, b.y0, b.x1, b.y1, b.w, b.h
    ym = y0 + 0.5 * h
    xr = x1 - (0.10 if st.slant else 0.04) * w
    return [poly([(x0, y0), (xr, y0), (xr, ym)], T_HZ),
            poly([(x0, ym), (xr, ym)], T_H),
            _ell(x0, ym, x1, y1, w, y1 - ym, st.slant)]


def mieum(b, st):
    # ㅁ, 3 strokes: left side down, top across and right side down (㇕), bottom
    # across. [C] "ㅁ (mieum) stroke order.png", "Mium stroke order.gif"; [K].
    x0, y0, x1, y1, w, h = b.x0, b.y0, b.x1, b.y1, b.w, b.h
    t = 0.015 * w
    return [poly([(x0, y0), (x0 + t, y1)], T_S),
            poly([(x0, y0), (x1, y0), (x1 - t, y1)], T_HZ),
            poly([(x0 + t, y1), (x1 - t, y1)], T_H)]


def bieup(b, st):
    # ㅂ, 4 strokes: left side, right side, middle bar, bottom bar. [C] "ㅂ (bieup)
    # stroke order.png", "Biup stroke order.gif"; [K].
    x0, y0, x1, y1, w, h = b.x0, b.y0, b.x1, b.y1, b.w, b.h
    ym = y0 + 0.50 * h
    return [poly([(x0, y0), (x0, y1)], T_S),
            poly([(x1, y0), (x1, y1)], T_S),
            poly([(x0, ym), (x1, ym)], T_H),
            poly([(x0, y1), (x1, y1)], T_H)]


def siot(b, st):
    # ㅅ, 2 strokes: ㇒ from the top, then ㇏ from partway down it. [C] "ㅅ (siot)
    # stroke order.png", "Siut stroke order.gif"; [K]. Beside a vertical vowel the
    # ㇏ is short and starts lower; elsewhere the two legs are nearly symmetric.
    # The left one of ㅆ keeps its ㇏ short, clear of the right one.
    x0, y0, x1, y1, w, h = b.x0, b.y0, b.x1, b.y1, b.w, b.h
    if st.slant:
        xa = x0 + (0.55 if st.role != "L" else 0.60) * w
        s1 = Stroke(xa, y0, T_P).curve(xa, y0 + 0.32 * h, xa - 0.12 * w, y0 + 0.72 * h, x0, y1)
        p = _on_curve(s1, 0, 0.42)
        end = (x1, y1 - 0.02 * h) if st.role != "L" else (x1, y0 + 0.80 * h)
        return [s1, _falling(p, end)]
    xa = x0 + (0.50 if st.role != "R" else 0.54) * w
    xe = x0 if st.role != "R" else x0 + 0.06 * w
    s1 = Stroke(xa, y0, T_P).curve(xa, y0 + 0.30 * h, xa - 0.18 * w, y0 + 0.78 * h, xe, y1)
    p = _on_curve(s1, 0, 0.30)
    end = (x1, y1) if st.role != "L" else (x1 - 0.04 * w, y0 + 0.82 * h)
    return [s1, _falling(p, end)]


def ieung(b, st):
    # ㅇ, 1 stroke: from the top, counter-clockwise, all the way round. [C] "ㅇ
    # (ieung) stroke order.png", "Iung stroke order.gif"; [K]. No CJK stroke type
    # fits a circle, so it has none.
    return [ellipse(b.cx, b.cy, b.w * 0.5, b.h * 0.5, 90.0, 360.0)]


def _jieut_strokes(b, st):
    """ㅈ's two strokes in box b: ㇇ (a short bar, then a long sweep down-left)
    and ㇏ from the middle of the sweep. Also the bar's middle (for ㅊ's top)."""
    x0, y0, x1, y1, w, h = b.x0, b.y0, b.x1, b.y1, b.w, b.h
    if st.slant:
        bar0, bar1, t, end = 0.08, 0.80, 0.42, (x1, y1 - 0.02 * h)
        if st.role == "L":
            bar1, end = 0.86, (x1, y0 + 0.80 * h)
        c1 = (x0 + (bar1 - 0.03) * w, y0 + 0.30 * h)
        c2 = (x0 + 0.30 * w, y0 + 0.78 * h)
        xe = x0
    else:
        bar0, bar1, t, end = 0.16, 0.72, 0.36, (x1, y1)
        if st.role == "L":
            end = (x1 - 0.04 * w, y0 + 0.82 * h)
        xe = x0 if st.role != "R" else x0 + 0.06 * w
        c1 = (x0 + (bar1 - 0.04) * w, y0 + 0.28 * h)
        c2 = (x0 + 0.34 * w, y0 + 0.76 * h)
    xb = x0 + bar1 * w
    s1 = Stroke(x0 + bar0 * w, y0, T_HP).line(xb, y0)
    s1.curve(c1[0], c1[1], c2[0], c2[1], xe, y1)
    p = _on_curve(s1, 1, t)
    return [s1, _falling(p, end)], (x0 + bar0 * w + xb) * 0.5


def jieut(b, st):
    # ㅈ, 2 strokes (house choice: the handwritten form taught in school): ㇇ —
    # across, then a long sweep down to the left — then ㇏ from the middle of the
    # sweep. [C] "ㅈ (jieut) stroke order.png", "Jigut stroke order.gif"; [K]. The
    # printed form (ㅡ, ㇒, ㇏: 3 strokes) is not used; see README.
    return _jieut_strokes(b, st)[0]


def chieut(b, st):
    # ㅊ, 3 strokes: the short top stroke (꼭지, written downward), then ㅈ's two.
    # [C] "ㅊ (chieut) stroke order.png", "Chiut stroke order.gif"; [K]. House
    # choice: the top stroke is a short vertical (see README).
    x0, y0, x1, y1, w, h = b.x0, b.y0, b.x1, b.y1, b.w, b.h
    gap = min(max(0.19 * h, 4.0), 10.0)
    body, xm = _jieut_strokes(Box(x0, y0 + gap, x1, y1), st)
    tick = poly([(xm, y0), (xm, y0 + gap)], T_S)
    return [tick] + body


def kieuk(b, st):
    # ㅋ, 2 strokes: ㄱ, then the middle bar into it. [C] "ㅋ (kieuk) stroke
    # order.png", "Kiuk stroke order.gif"; [K].
    x0, y0, x1, y1, w, h = b.x0, b.y0, b.x1, b.y1, b.w, b.h
    g = giyeok(b, st)[0]
    ym = y0 + (0.48 if st.slant else 0.50) * h
    if st.slant:
        t = _t_at_y(g, 1, ym)
        xe = _on_curve(g, 1, t)[0]
        bar = poly([(x0 + 0.04 * w, ym), (xe, ym)], T_H)
    else:
        bar = poly([(x0, ym), (x1, ym)], T_H)
    return [g, bar]


def tieut(b, st):
    # ㅌ, 3 strokes: top bar, middle bar, then ㄴ. [C] "ㅌ (tieut) stroke order.png",
    # "Tiut stroke order.gif"; [K].
    x0, y0, x1, y1, w, h = b.x0, b.y0, b.x1, b.y1, b.w, b.h
    ym = y0 + 0.48 * h
    xr = x1 - (0.10 if st.slant else 0.04) * w
    return [poly([(x0, y0), (xr, y0)], T_H),
            poly([(x0, ym), (xr - 0.04 * w, ym)], T_H),
            _ell(x0, y0, x1, y1, w, h, st.slant)]


def pieup(b, st):
    # ㅍ, 4 strokes: top bar, left inner stroke, right inner stroke, bottom bar.
    # [C] "ㅍ (pieup) stroke order.png", "Piup stroke order.gif"; [K].
    x0, y0, x1, y1, w, h = b.x0, b.y0, b.x1, b.y1, b.w, b.h
    a, c = x0 + 0.29 * w, x1 - 0.29 * w
    return [poly([(x0 + 0.06 * w, y0), (x1 - 0.06 * w, y0)], T_H),
            poly([(a, y0), (a, y1)], T_S),
            poly([(c, y0), (c, y1)], T_S),
            poly([(x0, y1), (x1, y1)], T_H)]


def hieut(b, st):
    # ㅎ, 3 strokes: the short top stroke (꼭지, downward), the bar, then ㅇ. [C] "ㅎ
    # (hieut) stroke order.png", "Hiut stroke order.gif"; [K]. House choice: the top
    # stroke is a short vertical, as for ㅊ.
    x0, y0, x1, y1, w, h = b.x0, b.y0, b.x1, b.y1, b.w, b.h
    th = min(max(0.20 * h, 4.0), 9.0)
    gap = min(max(0.10 * h, 3.6), 6.0)
    lo, hi = RING_ASPECT[st.kind]
    avail = h - th - gap
    rw = min((0.78 if st.role is None else 0.86) * w, avail * hi)
    rh = min(avail, rw / lo)
    # Whatever height the ring cannot use is split above and below the letter.
    y0 += (avail - rh) * 0.5
    yb = y0 + th
    top = yb + gap
    xm = b.cx
    ring = Box(xm - rw / 2, top, xm + rw / 2, top + rh)
    return [poly([(xm, y0), (xm, yb)], T_S),
            poly([(x0, yb), (x1, yb)], T_H),
            ieung(ring, st)[0]]


# ㅎ's ring: its narrowest and widest (width / height).
RING_ASPECT = {"V": (0.86, 1.05), "H": (1.1, 1.75), "C": (1.0, 1.45), "F": (1.2, 1.9), "S": (0.95, 1.25)}


CONSONANT = {
    "ㄱ": giyeok, "ㄴ": nieun, "ㄷ": digeut, "ㄹ": rieul, "ㅁ": mieum, "ㅂ": bieup, "ㅅ": siot,
    "ㅇ": ieung, "ㅈ": jieut, "ㅊ": chieut, "ㅋ": kieuk, "ㅌ": tieut, "ㅍ": pieup, "ㅎ": hieut,
}


# --- vowels -----------------------------------------------------------------------------
#
# Vertical vowels take a dict:
#   xa     the (first) long vertical's x
#   xb     the second long vertical's x (ㅐ ㅒ ㅔ ㅖ)
#   yt yb  the long vertical's top and bottom
#   ys     the short stroke's y; ys2 the second's (ㅑ ㅒ ㅕ ㅖ)
#   sl     the short stroke's length (out from the vertical)
#   inner  how much shorter the inner vertical of ㅐ ㅒ ㅔ ㅖ is at each end
#
# Order: [C] "ㅏ (a) stroke order.png" ... "ㅣ (i) stroke order.png", "Korean vowel
# strokes.gif"; [K]: a vowel whose short strokes stand left of its vertical writes
# them first (ㅓ ㅕ ㅔ ㅖ); one whose short strokes stand to the right writes the
# vertical first (ㅏ ㅑ ㅐ ㅒ); in ㅐ ㅒ ㅔ ㅖ the right vertical is last.

def _v(x, yt, yb):
    return poly([(x, yt), (x, yb)], T_S)


def _h(x0, x1, y):
    return poly([(x0, y), (x1, y)], T_H)


def vowel_vertical(v, g):
    xa, yt, yb, ys, sl = g["xa"], g["yt"], g["yb"], g["ys"], g["sl"]
    ys2 = g.get("ys2")
    xb = g.get("xb")
    inner = g.get("inner", 0.0)
    if v == "ㅏ":    # 2: vertical, short stroke right
        return [_v(xa, yt, yb), _h(xa, xa + sl, ys)]
    if v == "ㅑ":    # 3: vertical, upper short, lower short
        return [_v(xa, yt, yb), _h(xa, xa + sl, ys), _h(xa, xa + sl, ys2)]
    if v == "ㅓ":    # 2: short stroke left, vertical
        return [_h(xa - sl, xa, ys), _v(xa, yt, yb)]
    if v == "ㅕ":    # 3: upper short, lower short, vertical
        return [_h(xa - sl, xa, ys), _h(xa - sl, xa, ys2), _v(xa, yt, yb)]
    if v == "ㅐ":    # 3: inner vertical, short stroke across, right vertical
        return [_v(xa, yt + inner, yb - inner), _h(xa, xb, ys), _v(xb, yt, yb)]
    if v == "ㅒ":    # 4: inner vertical, two short strokes, right vertical
        return [_v(xa, yt + inner, yb - inner), _h(xa, xb, ys), _h(xa, xb, ys2), _v(xb, yt, yb)]
    if v == "ㅔ":    # 3: short stroke left, inner vertical, right vertical
        return [_h(xa - sl, xa, ys), _v(xa, yt + inner, yb - inner), _v(xb, yt, yb)]
    if v == "ㅖ":    # 4: two short strokes, inner vertical, right vertical
        return [_h(xa - sl, xa, ys), _h(xa - sl, xa, ys2), _v(xa, yt + inner, yb - inner), _v(xb, yt, yb)]
    if v == "ㅣ":    # 1
        return [_v(xa, yt, yb)]
    raise KeyError(v)


# Horizontal vowels take a dict:
#   x0 x1  the long bar's ends;  y  its height
#   tx     the short stroke's x (ㅗ ㅜ), or tx, tx2 (ㅛ ㅠ)
#   tl     the short strokes' length (up from the bar for ㅗ ㅛ, down for ㅜ ㅠ)
#
# Order: [C], [K]: ㅗ ㅛ write the short stroke(s) first, down onto the bar; ㅜ ㅠ
# write the bar first, the short stroke(s) down from it; left before right.

def vowel_horizontal(v, g):
    x0, x1, y = g["x0"], g["x1"], g["y"]
    if v == "ㅡ":    # 1
        return [_h(x0, x1, y)]
    tx, tl = g["tx"], g["tl"]
    if v == "ㅗ":    # 2: short stroke down onto the bar, bar
        return [_v(tx, y - tl, y), _h(x0, x1, y)]
    if v == "ㅛ":    # 3: left short, right short, bar
        return [_v(tx, y - tl, y), _v(g["tx2"], y - tl, y), _h(x0, x1, y)]
    if v == "ㅜ":    # 2: bar, short stroke down from it
        return [_h(x0, x1, y), _v(tx, y, y + tl)]
    if v == "ㅠ":    # 3: bar, left short, right short
        return [_h(x0, x1, y), _v(tx, y, y + tl), _v(g["tx2"], y, y + tl)]
    raise KeyError(v)
