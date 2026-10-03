"""Stroke geometry for the Hangul stroke data.

A stroke is a centreline: a start point and a run of cubic Bezier segments, in
KanjiVG's 109 x 109 box (Y down). Straight pieces are cubics too, with their
control points on the line, because Kana's KanjiVG reader (fude/lang/ja/bake.c,
fude_bake_parse_path) only understands M/m, C/c and S/s.

This module also holds the reader's rules in Python (parse_d), so the validator
and the contact sheets read the XML back exactly as the bake would.
"""

import math
import re

BOX = 109.0

# Stroke types: characters of the CJK Strokes block (U+31C0..), as KanjiVG uses
# them in kvg:type. Only the ones the Hangul jamo need.
T_H = "㇐"     # ㇐ horizontal
T_S = "㇑"     # ㇑ vertical
T_P = "㇒"     # ㇒ left-falling
T_N = "㇏"     # ㇏ right-falling
T_D = "㇔"     # ㇔ dot
T_HZ = "㇕"    # ㇕ horizontal then vertical (upright ㄱ)
T_HP = "㇇"    # ㇇ horizontal then left-falling (slanted ㄱ, ㅈ)
T_SZ = "㇗"    # ㇗ vertical then horizontal (ㄴ)


class Stroke:
    """One stroke: start point, then cubic segments (c1, c2, end)."""

    __slots__ = ("start", "segs", "type")

    def __init__(self, x, y, type=None):
        self.start = (float(x), float(y))
        self.segs = []
        self.type = type

    @property
    def end(self):
        return self.segs[-1][2] if self.segs else self.start

    def line(self, x, y):
        """A straight piece to (x, y): a cubic with its control points on the line."""
        x0, y0 = self.end
        dx, dy = x - x0, y - y0
        self.segs.append(((x0 + dx / 3.0, y0 + dy / 3.0), (x0 + 2.0 * dx / 3.0, y0 + 2.0 * dy / 3.0), (float(x), float(y))))
        return self

    def curve(self, c1x, c1y, c2x, c2y, x, y):
        self.segs.append(((float(c1x), float(c1y)), (float(c2x), float(c2y)), (float(x), float(y))))
        return self

    def smooth(self, c2x, c2y, x, y):
        """A curve whose first control point mirrors the previous segment's second (SVG's S)."""
        x0, y0 = self.end
        if self.segs:
            px, py = self.segs[-1][1]
            c1 = (2.0 * x0 - px, 2.0 * y0 - py)
        else:
            c1 = (x0, y0)
        self.segs.append((c1, (float(c2x), float(c2y)), (float(x), float(y))))
        return self

    def map(self, f):
        """A copy with every point passed through f((x, y)) -> (x, y)."""
        s = Stroke(*f(self.start), type=self.type)
        s.segs = [(f(a), f(b), f(c)) for a, b, c in self.segs]
        return s

    def points(self):
        yield self.start
        for a, b, c in self.segs:
            yield a
            yield b
            yield c

    def d(self):
        """KanjiVG-style path data: an absolute M, then relative c segments.

        Points are rounded to 2 decimals first and the relative offsets taken
        between rounded points, so the reader lands exactly on them."""
        r = lambda p: (round(p[0], 2), round(p[1], 2))
        cur = r(self.start)
        out = ["M%s,%s" % (num(cur[0]), num(cur[1]))]
        for a, b, c in self.segs:
            a, b, c = r(a), r(b), r(c)
            vals = (a[0] - cur[0], a[1] - cur[1], b[0] - cur[0], b[1] - cur[1], c[0] - cur[0], c[1] - cur[1])
            out.append("c" + ",".join(num(v) for v in vals))
            cur = c
        return "".join(out)


def num(v):
    """A coordinate as KanjiVG writes them: up to 2 decimals, no trailing zeros."""
    s = "%.2f" % round(v, 2)
    s = s.rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


# --- builders ---------------------------------------------------------------------

def poly(points, type=None):
    """Straight pieces through the points, with sharp corners."""
    s = Stroke(*points[0], type=type)
    for p in points[1:]:
        s.line(*p)
    return s


def bezier_point(p0, p1, p2, p3, t):
    u = 1.0 - t
    return (u * u * u * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t * t * t * p3[0],
            u * u * u * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t * t * t * p3[1])


def split_bezier(p0, p1, p2, p3, t):
    """De Casteljau: the two halves of a cubic at t."""
    lerp = lambda a, b: (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
    a, b, c = lerp(p0, p1), lerp(p1, p2), lerp(p2, p3)
    d, e = lerp(a, b), lerp(b, c)
    f = lerp(d, e)
    return (p0, a, d, f), (f, e, c, p3)


def ellipse(cx, cy, rx, ry, start_deg=90.0, sweep_deg=360.0, type=None):
    """An elliptical arc as cubics, from start_deg, counter-clockwise ON SCREEN
    for a positive sweep (angles in the usual maths sense, 90 = top).

    A full circle from the top, counter-clockwise, is how ㅇ is written."""
    n = max(1, int(math.ceil(abs(sweep_deg) / 90.0 - 1e-9)))
    step = math.radians(sweep_deg) / n
    k = 4.0 / 3.0 * math.tan(step / 4.0)
    a = math.radians(start_deg)
    pt = lambda a: (cx + rx * math.cos(a), cy - ry * math.sin(a))
    tan = lambda a: (-rx * math.sin(a), -ry * math.cos(a))    # d/da of pt
    s = Stroke(*pt(a), type=type)
    for _ in range(n):
        b = a + step
        p0, p3 = pt(a), pt(b)
        t0, t3 = tan(a), tan(b)
        s.curve(p0[0] + k * t0[0], p0[1] + k * t0[1], p3[0] - k * t3[0], p3[1] - k * t3[1], p3[0], p3[1])
        a = b
    return s


# --- reading paths back (the bake's rules) ----------------------------------------

_NUMBER = re.compile(r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?")


class PathError(ValueError):
    pass


def parse_d(d):
    """Parse path data with fude_bake_parse_path's rules: M/m only first and once,
    then C/c and S/s, a command letter repeated implicitly. Returns (start,
    [(c1, c2, end), ...]) in absolute coordinates, or raises PathError."""
    if d is None:
        raise PathError("no d")
    i, n = 0, len(d)
    cmd = None
    started = False
    cur = (0.0, 0.0)
    last_c2 = (0.0, 0.0)
    curve = False
    start = None
    segs = []

    def skip():
        nonlocal i
        while i < n and d[i] in " ,\t\n\r":
            i += 1

    def number():
        nonlocal i
        skip()
        m = _NUMBER.match(d, i)
        if not m:
            raise PathError("number expected at %d in %r" % (i, d))
        i = m.end()
        v = float(m.group(0))
        if not math.isfinite(v):
            raise PathError("not finite")
        return v

    while True:
        skip()
        if i >= n:
            break
        if d[i].isalpha():
            cmd = d[i]
            i += 1
        elif cmd is None:
            raise PathError("coordinates without a command in %r" % d)
        rel = cmd.islower()
        o = cur if rel else (0.0, 0.0)
        if cmd in "Mm":
            if started:
                raise PathError("a second M in %r" % d)
            v = [number(), number()]
            cur = (o[0] + v[0], o[1] + v[1])
            start = cur
            started = True
            curve = False
            cmd = None
        elif cmd in "Cc":
            if not started:
                raise PathError("C before M")
            v = [number() for _ in range(6)]
            c1 = (o[0] + v[0], o[1] + v[1])
            c2 = (o[0] + v[2], o[1] + v[3])
            p = (o[0] + v[4], o[1] + v[5])
            segs.append((c1, c2, p))
            last_c2, cur, curve = c2, p, True
        elif cmd in "Ss":
            if not started:
                raise PathError("S before M")
            v = [number() for _ in range(4)]
            c1 = (2 * cur[0] - last_c2[0], 2 * cur[1] - last_c2[1]) if curve else cur
            c2 = (o[0] + v[0], o[1] + v[1])
            p = (o[0] + v[2], o[1] + v[3])
            segs.append((c1, c2, p))
            last_c2, cur, curve = c2, p, True
        else:
            raise PathError("command %r is not supported" % cmd)
        if len(segs) > 255:
            raise PathError("more than 255 segments")
    if not started:
        raise PathError("empty path")
    return start, segs


def flatten(start, segs, steps=16):
    """Points along the curve, for drawing and measuring."""
    pts = [start]
    cur = start
    for c1, c2, p in segs:
        for k in range(1, steps + 1):
            pts.append(bezier_point(cur, c1, c2, p, k / steps))
        cur = p
    return pts


def length(pts):
    return sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(pts, pts[1:]))
