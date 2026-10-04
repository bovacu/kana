# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

"""Stroke geometry for the Devanagari stroke data.

A stroke is a centreline: a start point and a run of cubic Bezier segments, in
KanjiVG's 109 x 109 box (Y down). Straight pieces are cubics too, with their
control points on the line, because the app's KanjiVG reader
(fude/study/chars/bake.c, fude_bake_parse_path) only understands M/m, C/c and
S/s.

Curves are mostly drawn as smooth splines through a few points (centripetal
Catmull-Rom, turned into cubics), so a handwritten shape is a handful of
numbers and comes out without kinks; a corner is where one piece ends and the
next starts without continuing its direction.

This module also holds the reader's rules in Python (parse_d), so the validator
and the contact sheets read the XML back exactly as the bake would.
"""

import math
import re

BOX = 109.0


class Stroke:
    """One stroke: start point, then cubic segments (c1, c2, end)."""

    __slots__ = ("start", "segs")

    def __init__(self, x, y):
        self.start = (float(x), float(y))
        self.segs = []

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

    def through(self, points, t0=None, t1=None, join="smooth"):
        """A smooth curve from the current end through points (a list of (x, y)).

        t0, t1: directions (dx, dy) the curve leaves the start / arrives at the
        last point with; by default the start continues the previous segment
        (join="smooth") or is free (join="corner"), and the end is free."""
        pts = [self.end] + [(float(x), float(y)) for x, y in points]
        if t0 is None and join == "smooth" and self.segs:
            c2, p = self.segs[-1][1], self.segs[-1][2]
            t0 = (p[0] - c2[0], p[1] - c2[1])
            if abs(t0[0]) + abs(t0[1]) < 1e-9:
                t0 = None
        for seg in catmull_rom(pts, t0, t1):
            self.segs.append(seg)
        return self

    def map(self, f):
        """A copy with every point passed through f((x, y)) -> (x, y)."""
        s = Stroke(*f(self.start))
        s.segs = [(f(a), f(b), f(c)) for a, b, c in self.segs]
        return s

    def moved(self, dx, dy=0.0):
        return self.map(lambda p: (p[0] + dx, p[1] + dy))

    def points(self):
        yield self.start
        for a, b, c in self.segs:
            yield a
            yield b
            yield c

    def flat(self, steps=16):
        return flatten(self.start, self.segs, steps)

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

def poly(points):
    """Straight pieces through the points, with sharp corners."""
    s = Stroke(*points[0])
    for p in points[1:]:
        s.line(*p)
    return s


def spline(points, t0=None, t1=None):
    """A smooth stroke through the points, in order: the first is its start."""
    s = Stroke(*points[0])
    return s.through(points[1:], t0, t1, join="corner")


def catmull_rom(pts, t0=None, t1=None, alpha=0.5):
    """Centripetal Catmull-Rom through pts as cubic segments [(c1, c2, end), ...].

    Centripetal (alpha 0.5) never makes cusps or self-intersections inside a
    segment, so the curve stays the shape the points suggest. t0 / t1 give the
    end directions; without them the ends are 'natural' (the end tangent points
    at the neighbour, mirrored)."""
    n = len(pts)
    if n < 2:
        return []
    if n == 2 and t0 is None and t1 is None:
        (x0, y0), (x1, y1) = pts
        dx, dy = x1 - x0, y1 - y0
        return [((x0 + dx / 3.0, y0 + dy / 3.0), (x0 + 2.0 * dx / 3.0, y0 + 2.0 * dy / 3.0), (x1, y1))]

    def unit(v):
        l = math.hypot(v[0], v[1])
        return (v[0] / l, v[1] / l) if l > 1e-12 else (0.0, 0.0)

    # Phantom end points: along the given directions, or natural ends.
    d01 = math.hypot(pts[1][0] - pts[0][0], pts[1][1] - pts[0][1])
    if t0 is not None:
        u = unit(t0)
        p_first = (pts[0][0] - u[0] * d01, pts[0][1] - u[1] * d01)
    elif n > 2:
        q = _reflect_end(pts[0], pts[1], pts[2])
        p_first = q if q is not None else (2 * pts[0][0] - pts[1][0], 2 * pts[0][1] - pts[1][1])
    else:
        p_first = (2 * pts[0][0] - pts[1][0], 2 * pts[0][1] - pts[1][1])
    dnn = math.hypot(pts[-1][0] - pts[-2][0], pts[-1][1] - pts[-2][1])
    if t1 is not None:
        u = unit(t1)
        p_last = (pts[-1][0] + u[0] * dnn, pts[-1][1] + u[1] * dnn)
    elif n > 2:
        p_last = (2 * pts[-1][0] - pts[-2][0], 2 * pts[-1][1] - pts[-2][1])
        q = _reflect_end(pts[-1], pts[-2], pts[-3])
        p_last = q if q is not None else p_last
    else:
        p_last = (2 * pts[-1][0] - pts[-2][0], 2 * pts[-1][1] - pts[-2][1])
    P = [p_first] + list(pts) + [p_last]

    segs = []
    for i in range(1, n):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        d1 = max(1e-9, math.hypot(p1[0] - p0[0], p1[1] - p0[1]) ** alpha)
        d2 = max(1e-9, math.hypot(p2[0] - p1[0], p2[1] - p1[1]) ** alpha)
        d3 = max(1e-9, math.hypot(p3[0] - p2[0], p3[1] - p2[1]) ** alpha)
        c1 = tuple((d1 * d1 * p2[k] - d2 * d2 * p0[k] + (2 * d1 * d1 + 3 * d1 * d2 + d2 * d2) * p1[k]) / (3 * d1 * (d1 + d2))
                   for k in (0, 1))
        c2 = tuple((d3 * d3 * p1[k] - d2 * d2 * p3[k] + (2 * d3 * d3 + 3 * d3 * d2 + d2 * d2) * p2[k]) / (3 * d3 * (d3 + d2))
                   for k in (0, 1))
        segs.append((c1, c2, (float(p2[0]), float(p2[1]))))
    return segs


def _reflect_end(a, b, c):
    """A phantom point before the end point a (b, c: the next two points), for
    a natural end: the curve leaves a along the circle through a, b and c, so
    an end bends the way the next stretch does, without overshooting. None
    when the three points are in line."""
    # The tangent at a of the circle through a, b, c: a gentle, natural start.
    ax, ay = a
    bx, by = b
    cx, cy = c
    d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by))
    if abs(d) < 1e-9:
        return None
    ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) + (cx * cx + cy * cy) * (ay - by)) / d
    uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) + (cx * cx + cy * cy) * (bx - ax)) / d
    # tangent at a, oriented towards b
    tx, ty = -(ay - uy), ax - ux
    if tx * (bx - ax) + ty * (by - ay) < 0:
        tx, ty = -tx, -ty
    l = math.hypot(tx, ty)
    if l < 1e-12:
        return None
    dist = math.hypot(bx - ax, by - ay)
    return (ax - tx / l * dist, ay - ty / l * dist)


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


def ellipse(cx, cy, rx, ry, start_deg=90.0, sweep_deg=360.0):
    """An elliptical arc as cubics, from start_deg, counter-clockwise ON SCREEN
    for a positive sweep (angles in the usual maths sense, 90 = top)."""
    n = max(1, int(math.ceil(abs(sweep_deg) / 90.0 - 1e-9)))
    step = math.radians(sweep_deg) / n
    k = 4.0 / 3.0 * math.tan(step / 4.0)
    a = math.radians(start_deg)
    pt = lambda a: (cx + rx * math.cos(a), cy - ry * math.sin(a))
    tan = lambda a: (-rx * math.sin(a), -ry * math.cos(a))    # d/da of pt
    s = Stroke(*pt(a))
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


def joints(start, segs):
    """The turn, in degrees, at each joint between two segments: 0 where the
    curve goes straight on (smooth), up to 180 where it doubles back."""
    out = []
    cur = start
    for k in range(len(segs) - 1):
        c1, c2, p = segs[k]
        n1, n2, n3 = segs[k + 1]
        a = _direction(p, (c2, c1, cur), arriving=True)
        b = _direction(p, (n1, n2, n3), arriving=False)
        if a is None or b is None:
            out.append(0.0)
        else:
            dot = max(-1.0, min(1.0, a[0] * b[0] + a[1] * b[1]))
            out.append(math.degrees(math.acos(dot)))
        cur = p
    return out


def _direction(p, others, arriving):
    """The unit tangent at p: from the first of others distinct from p."""
    for q in others:
        dx, dy = (p[0] - q[0], p[1] - q[1]) if arriving else (q[0] - p[0], q[1] - p[1])
        l = math.hypot(dx, dy)
        if l > 1e-6:
            return (dx / l, dy / l)
    return None
