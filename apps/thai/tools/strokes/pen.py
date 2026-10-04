"""Stroke geometry for the Thai stroke data.

A stroke is a centreline: a start point and a run of cubic Bezier segments, in
KanjiVG's 109 x 109 box (Y down). Straight pieces are cubics too, with their
control points on the line, because Kana's KanjiVG reader
(fude/study/chars/bake.c, fude_bake_parse_path) only understands M/m, C/c and
S/s.

Thai letters are drawn as the pen moves, so a stroke here also keeps its
heading: each piece leaves in the direction the last one arrived, unless a
corner is asked for (turn). Headings are screen angles in degrees, Y down:

    E = 0 (right)   S = 90 (down)   W = 180 (left)   N = 270 (up)

so a positive change of heading turns clockwise as seen on the screen.

This module also holds the reader's rules in Python (parse_d), so the validator
and the contact sheets read the XML back exactly as the bake would.
"""

import math
import re

BOX = 109.0

E, S, W, N = 0.0, 90.0, 180.0, 270.0

# Handle length of a Hermite piece, as a fraction of its chord. 0.39 draws a
# quarter circle almost exactly; straight lines want 1/3.
K = 0.39
KAPPA = 4.0 / 3.0 * math.tan(math.pi / 8.0)     # quarter circle in one cubic


def unit(ang):
    a = math.radians(ang)
    return (math.cos(a), math.sin(a))


def heading(dx, dy):
    return math.degrees(math.atan2(dy, dx))


class Stroke:
    """One stroke: start point, then cubic segments (c1, c2, end), and the
    heading the pen has at its end."""

    __slots__ = ("start", "segs", "dir")

    def __init__(self, x, y, ang=None):
        self.start = (float(x), float(y))
        self.segs = []
        self.dir = ang

    @property
    def end(self):
        return self.segs[-1][2] if self.segs else self.start

    # --- pieces -------------------------------------------------------------------

    def turn(self, ang):
        """A corner: the next piece leaves at this heading."""
        self.dir = float(ang)
        return self

    def line(self, x, y):
        """A straight piece to (x, y): a cubic with its control points on the line."""
        x0, y0 = self.end
        dx, dy = x - x0, y - y0
        self.segs.append(((x0 + dx / 3.0, y0 + dy / 3.0), (x0 + 2.0 * dx / 3.0, y0 + 2.0 * dy / 3.0), (float(x), float(y))))
        self.dir = heading(dx, dy)
        return self

    def curve(self, c1x, c1y, c2x, c2y, x, y):
        self.segs.append(((float(c1x), float(c1y)), (float(c2x), float(c2y)), (float(x), float(y))))
        if (x, y) != (c2x, c2y):
            self.dir = heading(x - c2x, y - c2y)
        return self

    def go(self, x, y, ang, k=K, k2=None):
        """A curve to (x, y) that leaves at the current heading and arrives at
        heading ang (a cubic Hermite piece). k, k2: handle lengths at either
        end, as fractions of the chord."""
        x0, y0 = self.end
        L = math.hypot(x - x0, y - y0)
        k2 = k if k2 is None else k2
        d0 = unit(self.dir if self.dir is not None else heading(x - x0, y - y0))
        d1 = unit(ang)
        self.segs.append(((x0 + d0[0] * L * k, y0 + d0[1] * L * k),
                          (x - d1[0] * L * k2, y - d1[1] * L * k2),
                          (float(x), float(y))))
        self.dir = float(ang)
        return self

    def arc(self, cx, cy, sweep, ry_scale=1.0):
        """An arc about (cx, cy) from the current point, sweep degrees
        (positive: clockwise on the screen). The current point fixes the radius;
        ry_scale squashes it vertically (an elliptical arc)."""
        x0, y0 = self.end
        rx = math.hypot(x0 - cx, (y0 - cy) / ry_scale)
        a0 = math.atan2((y0 - cy) / ry_scale, x0 - cx)
        n = max(1, int(math.ceil(abs(sweep) / 90.0 - 1e-9)))
        step = math.radians(sweep) / n
        k = 4.0 / 3.0 * math.tan(step / 4.0)
        pt = lambda a: (cx + rx * math.cos(a), cy + rx * ry_scale * math.sin(a))
        tan = lambda a: (-rx * math.sin(a), rx * ry_scale * math.cos(a))
        a = a0
        for _ in range(n):
            b = a + step
            p0, p3 = pt(a), pt(b)
            t0, t3 = tan(a), tan(b)
            self.segs.append(((p0[0] + k * t0[0], p0[1] + k * t0[1]),
                              (p3[0] - k * t3[0], p3[1] - k * t3[1]), p3))
            a = b
        t = tan(a)
        sgn = 1.0 if sweep > 0 else -1.0
        self.dir = heading(sgn * t[0], sgn * t[1])
        return self

    def loop(self, r, cw, sweep=360.0, ry_scale=1.0):
        """A loop from the current point, tangent to the current heading: the
        circle of radius r on the right of the pen (cw) or on its left. A whole
        turn by default; the pen goes on at the heading it arrives with."""
        x0, y0 = self.end
        dx, dy = unit(self.dir)
        nx, ny = (-dy, dx) if cw else (dy, -dx)          # right / left of the pen
        cx, cy = x0 + r * nx, y0 + r * ny * ry_scale
        return self.arc(cx, cy, sweep if cw else -sweep, ry_scale)

    # --- the rest -------------------------------------------------------------------

    def map(self, f):
        """A copy with every point passed through f((x, y)) -> (x, y)."""
        s = Stroke(*f(self.start), ang=self.dir)
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


# --- builders -----------------------------------------------------------------------

def head(x, y, ang, r, cw):
    """A stroke that starts with a head (หัว): the pen starts at (x, y) where the
    head joins the body, goes once round the little circle (clockwise on the
    screen, or not) and leaves at heading ang. The circle lies on the pen's right
    when cw, on its left otherwise, tangent to the body at (x, y)."""
    s = Stroke(x, y, ang)
    return s.loop(r, cw)


def circle(cx, cy, r, start_deg, cw, ry_scale=1.0):
    """A closed circle (or ellipse) about (cx, cy), starting at screen angle
    start_deg (0 = right, 90 = bottom, 270 = top)."""
    a = math.radians(start_deg)
    s = Stroke(cx + r * math.cos(a), cy + r * ry_scale * math.sin(a))
    s.dir = 0.0
    return s.arc(cx, cy, 360.0 if cw else -360.0, ry_scale)


def poly(points):
    """Straight pieces through the points, with sharp corners."""
    s = Stroke(*points[0])
    for p in points[1:]:
        s.line(*p)
    return s


def bezier_point(p0, p1, p2, p3, t):
    u = 1.0 - t
    return (u * u * u * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t * t * t * p3[0],
            u * u * u * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t * t * t * p3[1])


# --- reading paths back (the bake's rules) --------------------------------------------

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
