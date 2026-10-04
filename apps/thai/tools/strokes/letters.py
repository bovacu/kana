# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

"""The Thai letters, drawn by hand as centreline strokes.

Our own work: every shape below was drawn for this project, in code. Nothing
here is traced or extracted from a font or from anyone else's stroke data.

The hand is the school one, the "round head" letters (หัวกลม ตัวมน) of the Thai
Ministry of Education's model: a letter starts at its head, the little circle,
goes once round it and runs on to its end without lifting the pen; the head is
drawn clockwise or counter-clockwise as the model has it, so it sits outside
or inside the letter. ก and ธ have no head and start where the model starts
them. A few letters lift the pen for a last stroke (ญ ฐ ศ ษ ส ๙, and ำ ี ึ ื ะ
แ ๋, which are two or more marks).

Each letter is a function returning its strokes in writing order, for a
109-unit box, Y down, all letters on one frame so they line up:

    y = 12          ascender top (ป ฝ ฟ)
    y = 30          top of the letters' bodies
    y = 84          baseline
    y = 102         descender bottom (ฎ ฏ ญ ฐ ฤ ฦ ๆ ๅ)
    y ~  3 .. 25    marks above a letter (ั ิ ี ึ ื ็ ่ ้ ๊ ๋ ์ ํ ๎); the tops
                    of โ ใ ไ reach about 4
    y ~ 89 .. 105   marks below it (ุ ู ฺ)

Marks are drawn alone but where they sit on a letter whose body fills the
middle band (x ~ 33..76): above it, centred over it; below it, under its
right half. Proportions were compared with Noto Sans Thai (contact sheets),
nothing taken from it.

STROKE ORDER SOURCES (cited per letter)
  [M]  The Ministry of Education's model letters (แบบตัวอักษรไทย กระทรวง-
       ศึกษาธิการ, "หัวกลม ตัวมน"), as reproduced in a school's handwriting
       teaching guide, คู่มือการสอนคัดลายมือ (Bankhai school, bky.ac.th): the
       model alphabet; vowels, tone marks and digits with each movement
       numbered (๑ ๒ ๓ ...); where each consonant's head sits (outside or inside,
       top or bottom); heads of ผ ฝ ค ฅ counter-clockwise, of น ม บ ป clockwise;
       the direction of each kind of line; "start at the head; do not lift the
       pen until the letter is finished".
  [P]  ThaiPod101, "Thai Alphabet Made Easy" #1-#25: a Thai teacher's spoken
       instructions for every consonant, vowel, tone mark and digit (where it
       starts, which way its head goes, the path, where to lift the pen).
  [A]  ActiveThai, consonant worksheets (middle, high and low class): each
       consonant with a start dot, arrows, an end dot and stroke numbers.
  [T]  thai-notes.com's per-letter notes (start, loop direction, pen lifts)
       and the littlefrog writing-practice data (MIT), as reported in
       docs/research/thai_sources.md section 1.4 (used to settle the signs and
       ๙, which [M] [P] [A] leave open).
House choices, where the sources differ or are silent, are in README.md and
noted per letter.

Directions: headings in pen.py's screen degrees (E 0, S 90, W 180, N 270).
cw: a head or loop drawn clockwise as seen on the screen.
"""

import math

from pen import Stroke, head, circle, heading, E, S, W, N

# --- the frame -----------------------------------------------------------------

T = 30.0        # top of the bodies
B = 84.0        # baseline
A = 12.0        # ascender top
D = 102.0       # descender bottom
R = 5.5         # a head's radius (centreline)
RL = 4.6        # a loop's radius (the second loops of ม ฆ ฌ ฒ)
RN = 5.4        # the loop in the corner of น ณ ฉ

# --- the letters, as taught ---------------------------------------------------------

# Strokes per letter, as taught (the validator checks the XML against these,
# independently of the drawings).
STROKES = {
    # consonants
    "ก": 1, "ข": 1, "ฃ": 1, "ค": 1, "ฅ": 1, "ฆ": 1, "ง": 1, "จ": 1, "ฉ": 1, "ช": 1,
    "ซ": 1, "ฌ": 1, "ญ": 2, "ฎ": 1, "ฏ": 1, "ฐ": 2, "ฑ": 1, "ฒ": 1, "ณ": 1, "ด": 1,
    "ต": 1, "ถ": 1, "ท": 1, "ธ": 1, "น": 1, "บ": 1, "ป": 1, "ผ": 1, "ฝ": 1, "พ": 1,
    "ฟ": 1, "ภ": 1, "ม": 1, "ย": 1, "ร": 1, "ล": 1, "ว": 1, "ศ": 2, "ษ": 2, "ส": 2,
    "ห": 1, "ฬ": 1, "อ": 1, "ฮ": 1,
    # vowels
    "ะ": 2, "ั": 1, "า": 1, "ำ": 2, "ิ": 1, "ี": 2, "ึ": 2, "ื": 3, "ุ": 1, "ู": 1,
    "เ": 1, "แ": 2, "โ": 1, "ใ": 1, "ไ": 1, "็": 1, "ฤ": 1, "ฦ": 1, "ๅ": 1,
    # tone marks and signs
    "่": 1, "้": 1, "๊": 1, "๋": 2, "์": 1, "ฯ": 1, "ๆ": 1, "ฺ": 1, "ํ": 1, "๎": 1,
    # digits
    "๐": 1, "๑": 1, "๒": 1, "๓": 1, "๔": 1, "๕": 1, "๖": 1, "๗": 1, "๘": 1, "๙": 2,
}

CONSONANTS = [chr(c) for c in range(0x0E01, 0x0E2F) if c not in (0x0E24, 0x0E26)]
VOWELS = list("ะัาำิีึืุูเแโใไ็ฤฦๅ")
MARKS = list("่้๊๋์ฯๆฺํ๎")
DIGITS = [chr(c) for c in range(0x0E50, 0x0E5A)]
GROUPS = (("consonants", CONSONANTS), ("vowels", VOWELS), ("marks", MARKS), ("digits", DIGITS))

DRAW = {}       # letter -> function returning its strokes
SOURCE = {}     # letter -> where its order comes from


def letter(ch, src):
    def deco(fn):
        DRAW[ch] = fn
        SOURCE[ch] = src
        return fn
    return deco


# --- shared pieces ------------------------------------------------------------------

def tangent_from(cx, cy, r, px, py, cw):
    """Where a line to (px, py) leaves the circle about (cx, cy) tangentially,
    the circle on the pen's right (cw) or left: (x, y, heading)."""
    dx, dy = px - cx, py - cy
    dist = math.hypot(dx, dy)
    a = math.acos(r / dist)
    base = math.atan2(dy, dx)
    for sgn in (1.0, -1.0):
        t = base + sgn * a
        jx, jy = cx + r * math.cos(t), cy + r * math.sin(t)
        vx, vy = px - jx, py - jy
        right = (-vy) * (cx - jx) + vx * (cy - jy) > 0
        if right == cw:
            return jx, jy, heading(vx, vy)
    raise ValueError("no tangent")


def on_circle(cx, cy, r, ang, cw):
    """The point of the circle where a pen going round it (cw or not) has
    heading ang: (x, y)."""
    a = math.radians(ang - 90.0 if cw else ang + 90.0)
    return cx + r * math.cos(a), cy + r * math.sin(a)


def head_at(cx, cy, r, ang, cw):
    """A stroke starting with a head centred at (cx, cy), leaving it at heading ang."""
    x, y = on_circle(cx, cy, r, ang, cw)
    return head(x, y, ang, r, cw)


def head_to(cx, cy, r, px, py, cw):
    """A stroke starting with a head centred at (cx, cy) whose line leaves it
    straight for (px, py) (the line itself is not drawn)."""
    x, y, ang = tangent_from(cx, cy, r, px, py, cw)
    return head(x, y, ang, r, cw)


def ko_top(s, xl, xr, top=T, shoulder=16.0):
    """ก's top, from the pen going up the left side at x = xl: the notch (a
    little zigzag, หยัก: right, then up to the beak on the left), the round
    top, and the turn down the right side at x = xr."""
    s.line(xl, top + 15.0)
    s.line(xl + 6.5, top + 12.0)
    s.line(xl - 1.5, top + 5.5)
    s.turn(-62.0)
    s.go(xl + 0.42 * (xr - xl), top, E)
    s.go(xr, top + shoulder, S)
    return s


def round_top(s, xl, xr, top=T, shoulder=17.0):
    """A round top from the pen going up the left side (ค ด ศ ...)."""
    s.go(xl + 0.47 * (xr - xl), top, E)
    s.go(xr, top + shoulder, S)
    return s


def notched_top(s, xl, xr, top=T, depth=7.0, shoulder=16.0):
    """A top with a notch in the middle (ต ฅ ฒ): two humps and a V."""
    xm = (xl + xr) / 2.0
    s.go(xl + 0.24 * (xr - xl), top, E)
    s.go(xm, top + depth, 62.0)
    s.turn(-62.0)
    s.go(xr - 0.24 * (xr - xl), top, E)
    s.go(xr, top + shoulder, S)
    return s


def u_bottom(s, xl, xr, rc=7.0, up_to=T):
    """From the pen going down at x = xl: the round bottom and up the right side."""
    s.line(xl, B - rc)
    s.go(xl + rc, B, E)
    s.line(xr - rc, B)
    s.go(xr, B - rc, N)
    s.line(xr, up_to)
    return s


def serrated(s, x0, xs):
    """From a head's top (pen going right): the serrated top of ฃ ซ ฆ ฑ, a peak,
    a V, a peak, then down into the stem at x = xs."""
    w = xs - x0
    s.go(x0 + 0.24 * w, T - 0.3, -60.0)
    s.line(x0 + 0.46 * w, T + 6.0)
    s.line(x0 + 0.68 * w, T - 0.3)
    s.turn(-20.0)
    s.go(xs, T + 9.0, S)
    return s


def bottom_loop(s, x, r=RL):
    """From the pen going down at x: the loop at the foot of ม ฆ ฌ ฒ, drawn
    clockwise on the left of the stem; the pen leaves it heading down-right."""
    s.line(x, B - r - 0.5)
    s.loop(r, True, 315.0)
    return s


def no_loop(s, xb, xv, r=RN, up_to=T):
    """From the pen at the foot of a stem (xb, B): up the diagonal to the loop
    in the bottom right corner of น ณ ฉ, clockwise round it, and up the
    vertical at x = xv rising from it."""
    s.turn(-42.0)
    s.go(xv + r, B - 2.0 * r, E, k=0.45)
    s.arc(xv + r, B - r, 270.0)
    s.line(xv, up_to)
    return s


def wave(s, x0, x1, top):
    """The wavy top bar of ร ธ ฐ (and โ's), from the pen at its left end
    heading up-right: a rise, a slight dip, the end lifting."""
    w = x1 - x0
    s.go(x0 + 0.30 * w, top, E)
    s.go(x0 + 0.66 * w, top + 2.2, E)
    s.go(x1, top - 0.8, -22.0)
    return s


def alpha(s, cx, cy, rx, ry, end, end_ang=-60.0):
    """The curl at the foot of ฎ ฏ ฐ: from the pen heading left, over the
    loop's top, counter-clockwise round it (crossing the way in) and out
    up to the right, to end."""
    s.go(cx, cy - ry, W)
    s.arc(cx, cy, -270.0, ry / rx)
    s.go(end[0], end[1], end_ang)
    return s


def tail(x0, y0, x1, y1):
    """The separate tail of ศ ส, drawn upward to the right."""
    s = Stroke(x0, y0)
    s.dir = heading(x1 - x0, y1 - y0) + 12.0
    s.go(x1, y1, heading(x1 - x0, y1 - y0) - 6.0)
    return s


# --- consonants -----------------------------------------------------------------------

@letter("ก", "[A] [P] #3 [M]: no head; from the foot of the left side, up, the notch, over and down the right side.")
def ko_kai():
    xl, xr = 34.0, 75.5
    s = Stroke(xl, B, N)
    ko_top(s, xl, xr)
    s.line(xr, B)
    return [s]


@letter("ข", "[A] [P] #12 [M] (double head, หัวซ้อน): the head top left, clockwise; over the hump, down, along the bottom, up the right side.")
def kho_khai():
    hx, hy = 37.0, T + 10.5
    s = head_at(hx, hy, R, E, True)
    s.go(45.0, T - 0.3, E, k=0.5)
    s.go(51.0, T + 10.0, S)
    u_bottom(s, 51.0, 75.5)
    return [s]


@letter("ฃ", "[A] [P] #22 [M] (serrated head, หัวหยัก): as ข with a notch on the head.")
def kho_khuat():
    hx, hy = 37.0, T + 10.5
    s = head_at(hx, hy, R, E, True)
    serrated(s, hx, 52.5)
    u_bottom(s, 52.5, 76.0)
    return [s]


@letter("ค", "[A] [P] #6 [M]: the head inside, counter-clockwise; the line down-left to the foot, up the left side, over and down the right.")
def kho_khwai():
    xl, xr = 33.5, 76.0
    foot = (36.5, B)
    s = head_to(51.5, T + 26.0, R, foot[0], foot[1], False)
    s.line(*foot)
    s.turn(-97.0)
    s.go(xl, T + 23.0, N)
    round_top(s, xl, xr)
    s.line(xr, B)
    return [s]


@letter("ฅ", "[A] [P] #22 [M] (notched top): as ค with the notch of ต on top.")
def kho_khon():
    xl, xr = 33.5, 76.5
    foot = (36.5, B)
    s = head_to(51.5, T + 27.0, R, foot[0], foot[1], False)
    s.line(*foot)
    s.turn(-97.0)
    s.go(xl, T + 22.0, N)
    notched_top(s, xl, xr)
    s.line(xr, B)
    return [s]


@letter("ฆ", "[A] [P] #21: the head top left, clockwise, the serrated top, then ม: down, the loop, across, up the right side.")
def kho_rakhang():
    hx, hy = 35.5, T + 10.5
    s = head_at(hx, hy, R, E, True)
    serrated(s, hx, 50.5)
    bottom_loop(s, 50.5)
    s.go(77.0, B, 18.0)
    s.turn(N)
    s.line(77.0, T)
    return [s]


@letter("ง", "[A] [P] #2 [M] (head outside, top): the head on the right stem's top, clockwise; down, then up to the left.")
def ngo_ngu():
    xs = 71.0
    s = head_at(xs - R, T + R, R, S, True)
    s.line(xs, B)
    s.turn(heading(33.0 - xs, 55.0 - B))
    s.line(33.0, 55.0)
    return [s]


@letter("จ", "[A] [P] #9: the head in the middle, clockwise; down to the right, back up the right side and over to the left.")
def cho_chan():
    xr = 72.5
    s = head_to(46.5, T + 27.0, R, 61.0, B - 4.0, True)
    s.line(61.0, B - 4.0)
    s.go(66.5, B, E, k=0.5)
    s.go(xr, B - 6.0, N, k=0.5)
    s.line(xr, T + 16.0)
    s.go(54.5, T, W)
    s.go(36.0, T + 9.0, 112.0)
    return [s]


@letter("ฉ", "[A] [P] #12: the head at mid height, clockwise; down, across to the loop in the corner, up, and over to the left above the head.")
def cho_ching():
    xs, xv = 41.0, 63.0
    s = head_at(xs - R, T + 21.0, R, S, True)
    s.line(xs, B)
    no_loop(s, xs, xv, up_to=T + 13.0)
    s.go(49.5, T, W)
    s.go(34.0, T + 8.0, 108.0)
    return [s]


@letter("ช", "[A] [P] #4 [M] (long tail): as ข, and the right side runs on into the tail, up to the right.")
def cho_chang():
    hx, hy = 35.5, T + 10.5
    s = head_at(hx, hy, R, E, True)
    s.go(43.5, T - 0.3, E, k=0.5)
    s.go(49.5, T + 10.0, S)
    u_bottom(s, 49.5, 73.5, up_to=T + 8.0)
    s.go(79.0, T - 5.0, -52.0)
    return [s]


@letter("ซ", "[A] [P] #4 [M] (serrated head, long tail): as ช with the notch on the head.")
def so_so():
    hx, hy = 35.5, T + 10.5
    s = head_at(hx, hy, R, E, True)
    serrated(s, hx, 51.0)
    u_bottom(s, 50.5, 74.0, up_to=T + 8.0)
    s.go(80.0, T - 5.0, -52.0)
    return [s]


@letter("ฌ", "[A]: the head bottom left, inside, clockwise; up, the notch, over, down the middle to the loop of ม, across and up the right side.")
def cho_choe():
    xl, xm, xr = 21.0, 53.0, 88.0
    s = head_at(xl + R, B - R, R, N, True)
    ko_top(s, xl, xm)
    bottom_loop(s, xm)
    s.go(xr, B, 18.0)
    s.turn(N)
    s.line(xr, T)
    return [s]


@letter("ญ", "[A] [P] #16: (1) the head bottom left, inside, clockwise; ก's shape, then along the bottom and up the right side; (2) the curl under it, its little head counter-clockwise.")
def yo_ying():
    xl, xm, xr = 21.0, 54.0, 88.0
    s = head_at(xl + R, B - R, R, N, True)
    ko_top(s, xl, xm)
    u_bottom(s, xm, xr)
    c = head_at(68.0, 94.0, 4.4, E, False)
    c.go(84.0, D - 3.0, -15.0)
    c.go(91.5, 89.0, -78.0)
    return [s, c]


@letter("ฎ", "[A] [P] #19: the head bottom left, outside, counter-clockwise; ก's shape, the right side on down below the line, and the curl to the left.")
def do_chada():
    xl, xr = 35.0, 76.0
    s = head_at(xl - R, B - R, R, N, False)
    ko_top(s, xl, xr)
    s.line(xr, D)
    s.turn(196.0)
    alpha(s, 57.0, D - 6.0, 7.0, 5.5, (68.0, 87.5))
    return [s]


@letter("ฏ", "[A] [P] #19: as ฎ, with a bump in the line going left before the curl.")
def to_patak():
    xl, xr = 35.0, 76.0
    s = head_at(xl - R, B - R, R, N, False)
    ko_top(s, xl, xr)
    s.line(xr, D)
    s.turn(222.0)
    s.go(70.0, D - 6.5, W)
    s.go(64.0, D, 118.0)
    s.turn(206.0)
    alpha(s, 52.5, D - 6.0, 6.5, 5.5, (62.0, 87.5))
    return [s]


@letter("ฐ", "[A] [P] #19: (1) the head clockwise, จ's shape, then the hook and wavy top of ร; (2) the foot: its head clockwise, down, left with a bump, the curl.")
def tho_than():
    xl, xr = 36.5, 70.0
    s = head_to(47.0, T + 29.0, 4.6, 59.5, B - 4.0, True)
    s.line(59.5, B - 4.0)
    s.go(64.5, B, E, k=0.5)
    s.go(xr, B - 6.0, N, k=0.5)
    s.line(xr, T + 20.0)
    s.go(53.5, T + 11.0, W)
    s.go(xl, T + 15.0, 200.0)
    s.turn(N)
    s.line(xl, T + 2.0)
    s.go(xl + 4.0, T - 2.5, E, k=0.5)
    wave(s, xl + 4.0, 74.0, T - 2.5)
    f = head_at(67.5, B + 7.5, 3.8, S, True)
    f.line(71.3, D - 3.0)
    f.go(68.0, D, W, k=0.5)
    f.go(61.0, D - 6.5, W)
    f.go(55.0, D, 118.0)
    f.turn(206.0)
    alpha(f, 44.0, D - 6.0, 7.0, 5.5, (53.5, B + 4.5))
    return [s, f]


@letter("ฑ", "[P] #20 [A] [M] (serrated head): the head clockwise, the serrated top, down, up the diagonal and over, down the right side.")
def tho_montho():
    hx, hy = 34.5, T + 10.5
    xs, xr = 49.0, 78.0
    s = head_at(hx, hy, R, E, True)
    serrated(s, hx, xs)
    s.line(xs, B)
    s.turn(-66.0)
    s.line(62.0, T + 12.0)
    s.go(70.0, T, E)
    s.go(xr, T + 11.0, S)
    s.line(xr, B)
    return [s]


@letter("ฒ", "[P] #20 [A]: the head clockwise, ต's shape, then ม's: down the middle, the loop, across, up the right side.")
def tho_phuthao():
    xl, xm, xr = 18.5, 52.0, 89.0
    foot = (21.0, B)
    s = head_to(36.0, T + 26.0, R, foot[0], foot[1], True)
    s.line(*foot)
    s.turn(-97.0)
    s.go(xl, T + 22.0, N)
    notched_top(s, xl, xm, shoulder=15.0)
    bottom_loop(s, xm)
    s.go(xr, B, 18.0)
    s.turn(N)
    s.line(xr, T)
    return [s]


@letter("ณ", "[A] [P] #16: the head bottom left, inside, clockwise; ก's shape, then the loop on the right and the vertical up from it.")
def no_nen():
    xl, xm, xv = 21.0, 53.0, 80.0
    s = head_at(xl + R, B - R, R, N, True)
    ko_top(s, xl, xm)
    s.line(xm, B)
    no_loop(s, xm, xv)
    return [s]


@letter("ด", "[A] [P] #3 #6 [M] (head inside, outward): the head inside, clockwise; the line under it to the foot, up the left side, over and down the right.")
def do_dek():
    xl, xr = 33.5, 76.5
    foot = (35.5, B)
    s = head_to(52.5, T + 25.0, R, foot[0], foot[1], True)
    s.line(*foot)
    s.turn(-97.0)
    s.go(xl, T + 22.0, N)
    round_top(s, xl, xr)
    s.line(xr, B)
    return [s]


@letter("ต", "[A] [P] #9 [M] (notched top): as ด with the notch on top.")
def to_tao():
    xl, xr = 33.0, 77.0
    foot = (35.5, B)
    s = head_to(52.5, T + 26.0, R, foot[0], foot[1], True)
    s.line(*foot)
    s.turn(-97.0)
    s.go(xl, T + 22.0, N)
    notched_top(s, xl, xr)
    s.line(xr, B)
    return [s]


@letter("ถ", "[A] [P] #19 [M] (head inside, bottom): the head bottom left, inside, clockwise; then ก's shape.")
def tho_thung():
    xl, xr = 34.0, 76.0
    s = head_at(xl + R, B - R, R, N, True)
    ko_top(s, xl, xr)
    s.line(xr, B)
    return [s]


@letter("ท", "[A] [P] #7 [M] (head outside, top): the head top left, clockwise; down, up the diagonal into the arch, down the right side.")
def tho_thahan():
    xs, xr = 42.0, 76.5
    s = head_at(xs - R, T + R, R, S, True)
    s.line(xs, B)
    s.turn(-66.0)
    s.line(58.5, T + 12.0)
    s.go(67.5, T, E)
    s.go(xr, T + 11.0, S)
    s.line(xr, B)
    return [s]


@letter("ธ", "[A] [P] #16: no head; down the left side from its top, along the bottom, up the right side, over to the left, the hook and the wavy top of ร.")
def tho_thong():
    xl, xr = 38.0, 73.0
    s = Stroke(xl, T + 15.0, S)
    u_bottom(s, xl, xr, rc=10.0, up_to=T + 22.0)
    s.go(56.0, T + 11.0, W)
    s.go(xl - 1.0, T + 12.5, 192.0)
    s.turn(-58.0)
    wave(s, xl - 1.0, 76.0, T + 0.5)
    return [s]


@letter("น", "[A] [P] #1 [M] (head outside, top; clockwise): the head top left; down, up the diagonal, the loop in the corner, up the vertical.")
def no_nu():
    xs, xv = 43.0, 65.0
    s = head_at(xs - R, T + R, R, S, True)
    s.line(xs, B)
    no_loop(s, xs, xv)
    return [s]


@letter("บ", "[P] #3 #9 [M] (clockwise head; one line): the head top left; down, along the bottom, up the right side. [A] lifts the pen at the corner; not followed.")
def bo_baimai():
    xs, xr = 43.0, 76.0
    s = head_at(xs - R, T + R, R, S, True)
    u_bottom(s, xs, xr)
    return [s]


@letter("ป", "[P] #9 [M]: as บ, the right side on up past the head. [A] lifts the pen at the corner; not followed.")
def po_pla():
    xs, xr = 43.0, 76.0
    s = head_at(xs - R, T + R, R, S, True)
    u_bottom(s, xs, xr, up_to=A)
    return [s]


@letter("ผ", "[A] [P] #14 [M] (head inside, top; counter-clockwise): the head inside; down, up and down the zigzag, up the right side.")
def pho_phueng():
    xs, xr = 34.0, 77.0
    s = head_at(xs + R, T + R, R, S, False)
    s.line(xs, B)
    s.turn(heading(55.5 - xs, T + 21.0 - B))
    s.line(55.5, T + 21.0)
    s.turn(heading(xr - 1.5 - 55.5, B - T - 21.0))
    s.line(xr - 1.5, B)
    s.turn(heading(1.5, T - B))
    s.line(xr, T)
    return [s]


@letter("ฝ", "[A] [P] #14 [M]: as ผ, the right side on up to the ascender.")
def fo_fa():
    xs, xr = 34.0, 77.0
    s = head_at(xs + R, T + R, R, S, False)
    s.line(xs, B)
    s.turn(heading(55.5 - xs, T + 21.0 - B))
    s.line(55.5, T + 21.0)
    s.turn(heading(xr - 1.5 - 55.5, B - T - 21.0))
    s.line(xr - 1.5, B)
    s.turn(heading(1.5, A - B))
    s.line(xr, A)
    return [s]


def _pho_phan(top_r):
    xs, xr = 37.0, 81.0
    xm = (xs + xr) / 2.0
    s = head_at(xs - R, T + R, R, S, True)
    s.line(xs, B)
    s.turn(heading(xm - xs, T + 1.0 - B))
    s.line(xm, T + 1.0)
    s.turn(heading(xr - 2.0 - xm, B - T - 1.0))
    s.line(xr - 2.0, B)
    s.turn(heading(2.0, top_r - B))
    s.line(xr, top_r)
    return s, xr


@letter("พ", "[A] [P] #5 [M] (head outside, top; clockwise): the head top left; down, up to the middle peak, down, up the right side.")
def pho_phan():
    return [_pho_phan(T)[0]]


@letter("ฟ", "[A] [P] #5 [M]: as พ, the right side on up to the ascender.")
def fo_fan():
    return [_pho_phan(A)[0]]


@letter("ภ", "[A] [P] #16 [M] (head outside, bottom; counter-clockwise): the head bottom left; then ก's shape.")
def pho_samphao():
    xl, xr = 36.0, 77.0
    s = head_at(xl - R, B - R, R, N, False)
    ko_top(s, xl, xr)
    s.line(xr, B)
    return [s]


@letter("ม", "[A] [P] #1 [M] (clockwise head): the head top left; down, the loop at the foot, across to the corner, up the right side.")
def mo_ma():
    xs, xr = 41.0, 76.0
    s = head_at(xs - R, T + R, R, S, True)
    bottom_loop(s, xs)
    s.go(xr, B, 18.0)
    s.turn(N)
    s.line(xr, T)
    return [s]


@letter("ย", "[A] [P] #2: the head inside, counter-clockwise; down the left side with its notch (two bumps), along the bottom, up the right side.")
def yo_yak():
    xs, xr = 36.0, 76.0
    s = head_at(xs + R, T + R + 0.5, R, S, False)
    s.go(33.5, T + 14.0, S, k=0.5)
    s.go(42.5, T + 21.0, 25.0, k=0.45)
    s.turn(heading(-8.0, 6.0))
    s.go(35.0, T + 32.0, S, k=0.5)
    u_bottom(s, 35.0, xr)
    return [s]


@letter("ร", "[A] [P] #8: the head at the foot, counter-clockwise; up the right side, over to the left, the hook back to the right along the wavy top.")
def ro_ruea():
    xr, xl = 66.0, 39.0
    s = head_at(xr - R, B - R, R, N, False)
    s.line(xr, T + 25.0)
    s.go(53.0, T + 17.0, W)
    s.go(xl, T + 20.0, 178.0)
    s.turn(N)
    s.line(xl, T + 4.5)
    s.go(xl + 4.0, T + 0.5, E, k=0.5)
    wave(s, xl + 4.0, 75.0, T + 0.5)
    return [s]


@letter("ล", "[A] [P] #8: the head bottom left, clockwise; a small curve to the right and down the right side, back up it, and the large curve over to the left.")
def lo_ling():
    hx, hy = 39.0, B - R
    xr = 74.0
    s = head_at(hx, hy, R, -15.0, True)
    s.go(55.5, T + 20.0, E)
    s.go(xr, T + 33.0, S)
    s.line(xr, B)
    s.turn(N)
    s.line(xr + 0.6, T + 16.0)
    s.go(55.5, T, W)
    s.go(36.0, T + 9.0, 112.0)
    return [s]


@letter("ว", "[A] [P] #2: the head at the foot, counter-clockwise; up the right side and over to the left.")
def wo_waen():
    xr = 70.5
    s = head_at(xr - R, B - R, R, N, False)
    s.line(xr, T + 14.0)
    s.go(54.0, T, W)
    s.go(37.0, T + 5.5, 162.0)
    return [s]


@letter("ศ", "[A] [P] #13 [M] (head inside; long tail): (1) as ค; (2) the tail, lifting the pen, up to the right through the shoulder.")
def so_sala():
    xl, xr = 33.5, 75.0
    foot = (36.5, B)
    s = head_to(51.0, T + 26.0, R, foot[0], foot[1], False)
    s.line(*foot)
    s.turn(-97.0)
    s.go(xl, T + 23.0, N)
    round_top(s, xl, xr)
    s.line(xr, B)
    return [s, tail(64.0, T + 14.0, 81.0, T - 7.0)]


@letter("ษ", "[A] [P] #13 [M] (the inside, ไส้): (1) as บ; (2) the inside, lifting the pen: its little head counter-clockwise, then out to the right through the right side.")
def so_ruesi():
    xs, xr = 41.0, 73.0
    s = head_at(xs - R, T + R, R, S, True)
    u_bottom(s, xs, xr)
    c = head_at(53.5, T + 22.0, 4.0, E, False)
    c.go(66.0, T + 28.0, -20.0)
    c.go(80.0, T + 13.0, -50.0)
    return [s, c]


@letter("ส", "[A] [P] #13 [M] (long tail): (1) as ล, its top reaching a little further left; (2) the tail, lifting the pen, up to the right through the shoulder.")
def so_suea():
    hx, hy = 40.0, B - R
    xr = 74.5
    s = head_at(hx, hy, R, -15.0, True)
    s.go(56.0, T + 21.0, E)
    s.go(xr, T + 34.0, S)
    s.line(xr, B)
    s.turn(N)
    s.line(xr + 0.6, T + 16.0)
    s.go(55.0, T, W)
    s.go(33.0, T + 11.0, 118.0)
    return [s, tail(64.5, T + 14.0, 81.0, T - 7.0)]


@letter("ห", "[A] [P] #15 [M] (head outside, top): the head top left, clockwise; down, up the diagonal, the loop on top of the right side, counter-clockwise, and down.")
def ho_hip():
    xs, xr = 43.0, 72.5
    rl = 4.6
    s = head_at(xs - R, T + R, R, S, True)
    s.line(xs, B)
    s.turn(heading(xr - 6.0 - xs, T + 2.0 * rl + 6.0 - B))
    s.line(xr - 6.0, T + 2.0 * rl + 6.0)
    s.go(xr - 1.0, T + 2.0 * rl + 0.5, -10.0, k=0.5)
    s.loop(rl, False)
    s.go(xr + 1.5, T + 2.0 * rl + 6.0, S, k=0.55)
    s.line(xr + 1.5, B)
    return [s]


@letter("ฬ", "[A] [P] #21 [M] (curled tail): as พ, the right side on up into a loop, counter-clockwise, and out in a tail.")
def lo_chula():
    s, xr = _pho_phan(T - 1.0)
    s.loop(5.0, False)
    s.go(xr + 6.0, T - 15.5, -62.0)
    return [s]


@letter("อ", "[A] [P] #11 [M] (head inside; counter-clockwise): the head inside; down the left side, along the bottom, up the right and over to the left.")
def o_ang():
    xl, xr = 36.0, 75.5
    s = head_at(xl + R, T + 27.0, R, S, False)
    s.line(xl, B - 9.0)
    s.go(xl + 9.0, B, E)
    s.line(xr - 9.0, B)
    s.go(xr, B - 9.0, N)
    s.line(xr, T + 14.0)
    s.go(56.5, T, W)
    s.go(36.5, T + 8.0, 112.0)
    return [s]


@letter("ฮ", "[A] [P] #7 [M] (head inside; curled tail): as อ, then over the top into a loop and the tail out to the upper right.")
def ho_nokhuk():
    xl, xr = 35.5, 76.0
    s = head_at(xl + R, T + 30.0, R, S, False)
    s.line(xl, B - 9.0)
    s.go(xl + 9.0, B, E)
    s.line(xr - 9.0, B)
    s.go(xr, B - 9.0, N)
    s.line(xr, T + 14.0)
    s.go(57.0, T + 0.5, W)
    s.go(39.0, T + 8.5, S)
    s.go(55.0, T + 15.5, E)
    s.go(81.0, T - 4.0, -55.0)
    return [s]


# --- vowels -------------------------------------------------------------------------------

def _curl(cx, cy, r, w, h):
    """The little curl of ะ and ั: a head, counter-clockwise, then right along
    its foot and up, ending above it on the right (w, h: how far)."""
    s = head_at(cx, cy, r, E, False)
    s.go(cx + 0.62 * w, cy + r - 0.4, -12.0, k=0.42)
    s.go(cx + w, cy + r - h, -80.0, k=0.45)
    return s


@letter("ะ", "[M] [P] #4: two small curls, the upper first; each its head, counter-clockwise, then out to the right and up.")
def sara_a():
    return [_curl(47.0, 46.0, 4.4, 17.0, 13.5), _curl(47.0, 69.0, 4.4, 17.0, 13.5)]


@letter("ั", "[M] [P] #4: one curl above the letter: the head, counter-clockwise, then out to the right and up.")
def mai_han_akat():
    return [_curl(42.0, 19.0, 3.8, 27.0, 15.0)]


def _aa(x0, xr, bottom=B):
    s = Stroke(x0, T + 9.0, -72.0)
    s.go(x0 + 0.5 * (xr - x0), T, E)
    s.go(xr, T + 12.0, S)
    s.line(xr, bottom)
    return s


@letter("า", "[M] [P] #1: no head; from the left end, over to the right and straight down.")
def sara_aa():
    return [_aa(40.0, 68.5)]


@letter("ำ", "[P] #16 [M]: the little circle over the letter first (clockwise), then า.")
def sara_am():
    return [circle(34.0, 15.5, 4.0, 225.0, True), _aa(53.0, 81.0)]


def _i(right=76.5, left=35.0, base=24.5, top=13.0):
    """ิ: from the right tip, the lower line to the left, then the arch back over to the tip."""
    s = Stroke(right, base, 182.0)
    s.go(left, base - 1.0, 175.0)
    s.turn(-62.0)
    s.go(left + 0.54 * (right - left), top, E)
    s.go(right, base, 72.0)
    return s


@letter("ิ", "[M] [P] #4 #3: from the right, the lower line to the left, then the arch back to the right.")
def sara_i():
    return [_i()]


@letter("ี", "[M] [P] #3: ิ, then the short line at its right end, downward.")
def sara_ii():
    return [_i(), Stroke(76.5, 7.5, S).line(76.5, 21.0)]


@letter("ึ", "[M] [P] #6: ิ, then a tiny clockwise circle at its right end.")
def sara_ue():
    return [_i(), circle(75.5, 11.5, 3.2, 225.0, True)]


@letter("ื", "[M] [P] #6: ิ, the right line, then the left one, both downward.")
def sara_uee():
    return [_i(), Stroke(76.5, 7.5, S).line(76.5, 21.0), Stroke(70.0, 7.5, S).line(70.0, 17.0)]


@letter("ุ", "[M] [P] #5: the little head on the left, clockwise, then the line down (on the letter's back line).")
def sara_u():
    s = head_at(70.3, 93.0, 3.2, S, True)
    s.line(73.5, 105.0)
    return [s]


@letter("ู", "[M] [P] #5: the little head, clockwise; down, along the bottom, up the right side (on the letter's back line).")
def sara_uu():
    xs, xr = 57.5, 74.0
    s = head_at(xs - 3.2, 93.0, 3.2, S, True)
    s.line(xs, 101.0)
    s.go(xs + 3.5, 104.5, E)
    s.line(xr - 3.5, 104.5)
    s.go(xr, 101.0, N)
    s.line(xr, 92.0)
    return [s]


def _e(x):
    """เ: the head at the foot, clockwise, the line rising from its left side."""
    s = head_at(x + R, B - R, R, N, True)
    s.line(x, T + 1.0)
    return s


@letter("เ", "[M] [P] #7: the head at the foot, clockwise; straight up.")
def sara_e():
    return [_e(50.0)]


@letter("แ", "[M] [P] #8: two of เ, the left one first.")
def sara_ae():
    return [_e(38.0), _e(61.0)]


@letter("โ", "[M] [P] #9: the head at the foot, clockwise; up past the letters' top, over to the left, the hook back to the right along the wavy top.")
def sara_o():
    x = 52.0
    s = head_at(x + R, B - R, R, N, True)
    s.line(x, 19.0)
    s.go(44.5, 10.0, W)
    s.go(39.0, 12.5, 150.0)
    s.turn(-42.0)
    wave(s, 39.0, 70.0, 5.0)
    return [s]


@letter("ใ", "[M] [P] #10: the head at the foot, clockwise; up, and the top curled round to the left (counter-clockwise), into a little spiral.")
def sara_ai_maimuan():
    x = 56.0
    s = head_at(x + R, B - R, R, N, True)
    s.line(x, 17.0)
    s.go(48.0, 5.0, W)
    s.go(41.0, 11.5, S)
    s.go(47.5, 17.5, E)
    s.go(51.5, 12.0, N, k=0.5)
    return [s]


@letter("ไ", "[M] [P] #10: the head at the foot, clockwise; up past the letters' top, then the zigzag: down to the left, up to the left.")
def sara_ai_maimalai():
    x = 56.0
    s = head_at(x + R, B - R, R, N, True)
    s.line(x, 7.0)
    s.turn(heading(-6.5, 8.0))
    s.line(49.5, 15.0)
    s.turn(heading(-6.5, -9.0))
    s.line(43.0, 6.0)
    return [s]


@letter("็", "[P] #7 #25 (the shape of ๘): the head on the right, clockwise; left over a bump, round the left side and over the top, out to the right.")
def mai_taikhu():
    hx, hy, r = 66.0, 19.0, 3.2
    s = head_at(hx, hy, r, N, True)
    s.go(59.5, 13.5, W, k=0.5)
    s.go(53.5, 23.5, 115.0, k=0.45)
    s.go(42.5, 15.0, N)
    s.go(53.0, 7.0, E)
    s.go(71.0, 3.5, -38.0)
    return [s]


def _thue(xr, bottom):
    """The line of ฤ: ถ's shape, the right side on down to the descender."""
    xl = 34.0
    s = head_at(xl + R, B - R, R, N, True)
    ko_top(s, xl, xr)
    s.line(xr, bottom)
    return s


@letter("ฤ", "[P] #22: as ถ (the head bottom left, inside, clockwise), the right side on down below the line.")
def rue():
    return [_thue(76.0, D)]


@letter("ฦ", "[P] #22: as ภ (the head bottom left, outside, counter-clockwise), the right side on down below the line.")
def lue():
    xl, xr = 36.0, 77.0
    s = head_at(xl - R, B - R, R, N, False)
    ko_top(s, xl, xr)
    s.line(xr, D)
    return [s]


@letter("ๅ", "[P] #22 (\"an extra long สระ อา\"): as า, down to the descender.")
def lakkhangyao():
    return [_aa(40.0, 68.5, D)]


# --- tone marks and signs ------------------------------------------------------------------

@letter("่", "[M] [P] #17: a short line, downward.")
def mai_ek():
    return [Stroke(56.0, 7.0, S).line(56.0, 23.0)]


@letter("้", "[M] [P] #17: the little head on the left, clockwise; over to the right and down, the end turning right.")
def mai_tho():
    s = head_at(45.0, 19.0, 3.2, E, True)
    s.go(55.0, 8.5, E, k=0.55)
    s.go(64.0, 19.0, S)
    s.go(69.5, 22.0, -25.0, k=0.5)
    return [s]


@letter("๊", "[M] [P] #18 (๗ small): the little head, clockwise; up, the two humps (a heart), down, then the tail up to the right.")
def mai_tri():
    hx, hy, r = 42.5, 20.5, 3.0
    s = head_at(hx, hy, r, N, True)
    s.go(45.5, 9.5, E)
    s.go(51.0, 14.5, 62.0)
    s.turn(-62.0)
    s.go(56.5, 9.5, E)
    s.go(61.5, 16.5, S)
    s.line(61.5, 23.5)
    s.turn(heading(9.0, -18.0))
    s.go(71.0, 5.0, -72.0)
    return [s]


@letter("๋", "[M] [P] #18: a plus: the bar across first, then the line down.")
def mai_chattawa():
    return [Stroke(47.0, 15.0, E).line(65.0, 15.0), Stroke(56.0, 6.5, S).line(56.0, 23.5)]


@letter("์", "[T] (loop clockwise): the little head at the foot, clockwise; up from its left side and over to the right.")
def thanthakhat():
    s = head_at(50.5, 20.5, 3.0, N, True)
    s.go(54.0, 11.0, E, k=0.5)
    s.go(65.5, 10.0, -12.0)
    return [s]


@letter("ฯ", "[T] (loop counter-clockwise): the head top left, inside; down the left side into the bowl, up to the right, down the right side.")
def paiyannoi():
    s = head_at(41.5, T + 8.5, 4.5, S, False)
    s.go(46.0, T + 22.5, 30.0, k=0.5)
    s.go(58.0, T + 20.0, -40.0)
    s.go(66.5, T + 1.0, -80.0)
    s.turn(S)
    s.line(68.0, B - 4.0)
    s.go(64.5, B, 140.0, k=0.5)
    return [s]


@letter("ๆ", "[T] (loop clockwise; the foot bends left): the head top left, clockwise; over to the right, down the long right side, the foot turning left.")
def mai_yamok():
    s = head_at(40.5, T + 12.0, 4.5, -18.0, True)
    s.go(58.0, T, E)
    s.go(71.0, T + 13.0, S)
    s.line(71.0, D - 7.0)
    s.go(65.0, D, 145.0, k=0.5)
    return [s]


@letter("ฺ", "[T] (a dot): drawn as a tiny clockwise circle, under the letter's back line.")
def phinthu():
    return [circle(70.5, 97.0, 2.2, 225.0, True)]


@letter("ํ", "[P] #6 [T] (as ึ's and ำ's circle): a small clockwise circle.")
def nikhahit():
    return [circle(56.0, 15.5, 3.6, 225.0, True)]


@letter("๎", "House choice (no head; written like ε): from the top right, round to the left into the middle, round again, out to the right.")
def yamakkan():
    s = Stroke(66.0, 5.5, 130.0)
    s.go(55.0, 7.0, W, k=0.5)
    s.go(51.5, 11.5, S, k=0.5)
    s.go(57.5, 15.0, E, k=0.5)
    s.turn(170.0)
    s.go(50.0, 19.5, S, k=0.5)
    s.go(56.0, 24.5, E, k=0.5)
    s.go(64.5, 22.5, -22.0, k=0.5)
    return [s]


# --- digits ----------------------------------------------------------------------------------

DT = 34.0       # top of a digit's body (its tail may rise above)


@letter("๐", "[T] (\"start at the top right, anticlockwise\"): a circle from the top right, counter-clockwise.")
def sun():
    return [circle(54.5, (DT + B) / 2.0, 22.0, 300.0, False, 24.5 / 22.0)]


@letter("๑", "[M] [P] #23: the head clockwise, running on into a clockwise spiral that ends at the bottom right.")
def nueng():
    s = head_at(53.0, 70.0, 4.6, W, True)
    s.go(36.0, 62.0, N)
    s.go(55.0, DT, E)
    s.go(74.5, 58.0, S)
    s.go(70.0, B, 122.0, k=0.45)
    return [s]


@letter("๒", "[M] [P] #23: the head clockwise; the two bumps on top, down the right side, along the bottom, up the left side and on above the top.")
def song():
    xl, xr = 33.5, 76.5
    s = head_at(45.5, 49.0, 4.6, N, True)
    s.go(52.5, DT + 0.5, E)
    s.go(58.5, 41.5, 62.0)
    s.turn(-62.0)
    s.go(66.5, DT + 0.5, E)
    s.go(xr, DT + 13.0, S)
    s.line(xr, B)
    s.turn(W)
    s.line(xl, B)
    s.turn(N)
    s.line(xl, DT - 2.0)
    s.go(xl - 3.0, DT - 9.0, 235.0, k=0.5)
    return [s]


@letter("๓", "[M] [P] #23: the head bottom left, clockwise; up, the two bumps going right, down the right side.")
def sam():
    xl, xr = 34.5, 75.0
    s = head_at(xl + 4.6, B - 4.6, 4.6, N, True)
    s.line(xl, 56.0)
    s.go(44.5, DT, E)
    s.go(54.5, 45.0, 64.0)
    s.turn(-64.0)
    s.go(64.5, DT, E)
    s.go(xr, 52.0, S)
    s.line(xr, B)
    return [s]


@letter("๔", "[M] [P] #24: the head in the middle, counter-clockwise; down to the bottom right corner, back along the bottom, up the left side, over the top and up.")
def si():
    xl, xr = 35.0, 74.5
    s = head_to(55.0, 57.0, 4.6, xr, B, False)
    s.line(xr, B)
    s.turn(W)
    s.line(xl + 9.0, B)
    s.go(xl, B - 10.0, N)
    s.line(xl, 52.0)
    s.go(52.0, DT + 2.0, E)
    s.go(72.0, DT - 2.0, -40.0)
    s.go(76.0, DT - 9.0, -80.0, k=0.5)
    return [s]


@letter("๕", "[M] [P] #24: as ๔, with a loop at the top before the tail.")
def ha():
    xl, xr = 35.0, 74.5
    s = head_to(55.0, 57.0, 4.6, xr, B, False)
    s.line(xr, B)
    s.turn(W)
    s.line(xl + 9.0, B)
    s.go(xl, B - 10.0, N)
    s.line(xl, 52.0)
    s.go(50.0, DT + 2.5, E)
    s.go(63.0, DT + 1.5, -15.0)
    s.loop(3.6, False)
    s.go(76.0, DT - 9.0, -70.0)
    return [s]


@letter("๖", "[M] [P] #24: the head counter-clockwise; along the bottom, up the right side, over to the left (a backward C), the tail out to the upper left.")
def hok():
    xr = 75.5
    s = head_at(45.0, B - 6.5, 4.6, E, False)
    s.go(63.0, B, E, k=0.45)
    s.go(xr, 63.0, N)
    s.go(56.0, DT, W)
    s.go(38.5, DT + 5.5, 150.0)
    s.go(32.5, DT - 4.0, 238.0, k=0.5)
    return [s]


@letter("๗", "[M] [P] #25: the head clockwise; up, the two humps (a heart open at the bottom), down the right, then the tail up to the right.")
def chet():
    xl = 31.0
    s = head_at(xl + 4.6, B - 5.0, 4.6, N, True)
    s.line(xl, 52.0)
    s.go(40.5, DT, E)
    s.go(50.0, 45.0, 64.0)
    s.turn(-64.0)
    s.go(59.5, DT, E)
    s.go(68.5, 52.0, S)
    s.line(68.5, B)
    s.turn(heading(11.0, DT - 8.0 - B))
    s.go(82.0, DT - 8.0, -75.0)
    return [s]


@letter("๘", "[M] [P] #25: the head bottom right, clockwise; left over a bump, round the left side and over the top, the end up to the right.")
def paet():
    hx, hy, r = 63.5, 76.0, 4.6
    s = head_at(hx, hy, r, N, True)
    s.go(54.0, 67.0, W, k=0.5)
    s.go(47.0, B, 118.0, k=0.5)
    s.go(34.0, 64.0, N)
    s.go(52.0, DT + 3.0, E)
    s.go(70.0, DT, -30.0)
    s.go(75.0, DT - 8.0, -80.0, k=0.5)
    return [s]


@letter("๙", "[M] [P] #25 [T] (\"lift your pen before adding the tail\"): (1) the head at the foot, clockwise; round the left and over the top, straight down to the right; (2) the tail, from that line: a zigzag up to the upper right.")
def kao():
    s = head_at(45.0, 77.5, 4.6, W, True)
    s.go(33.5, 62.0, N)
    s.go(48.0, DT, E)
    s.go(58.0, 41.0, 64.0, k=0.5)
    s.line(71.0, B)
    x0 = 58.0 + 13.0 * (47.0 - 41.0) / (B - 41.0)
    t = Stroke(x0, 47.0, heading(67.0 - x0, 5.5))
    t.line(67.0, 52.5)
    t.turn(-72.0)
    t.go(77.0, DT - 8.0, -82.0)
    return [s, t]


def strokes_of(ch):
    return DRAW[ch]()
