"""The Arabic letters, drawn by hand as centreline strokes.

Our own work: every shape below was drawn for this project, in code. Nothing
here is traced or extracted from a font, a primer or anyone else's stroke data.
The sources were read for facts only: where a letter starts, which way each
part goes, how many strokes it takes, in what order.

The hand is the Naskh that Arab primary schools teach (the Saudi Ministry's
model letters, print-like, written with a pencil): a letter's body is one
continuous stroke, written right to left from its start; dots, the hamza, the
madda, ك's small mark, the upright of ط ظ and the alif of لا come after it.
Each joined form joins on the baseline: its body stroke (always stroke 1)
starts at the right connection when the form joins the letter before, and ends
with the joining tail at the left connection when it joins the letter after.

FRAME (KanjiVG's 109-unit box, Y down, one frame for every character):

    y = 14          ascender top (ا ل ك ط ظ لا)
    y = 57          a tooth's tip (ب ت ث ن ي initial and medial, س ش's teeth)
    y = 68          the BASELINE, every form, every letter
    y ~ 84..93      descenders (ر ز و ن ل ق س ص ح ع م ي); dots under ي to ~97
    x = 98, y = 68  the right connection (final and medial forms start here)
    x = 11, y = 68  the left connection (initial and medial forms end here)

Isolated letters are centred on x = 54.5; joined forms keep their body near
the centre and run their connecting strokes out to x = 98 and x = 11.
Harakat are drawn where they sit on an imagined letter, which the app shows as
a dotted circle about (54.5, 56), radius 19: marks above it between y 19 and
36, marks below it between y 79 and 92, centred on x = 54.5. Digits stand on
the baseline, 44 high (y 24..68), each centred.

STROKE ORDER SOURCES (cited per letter)
  [M]  Saudi Ministry of Education, لغتي الجميلة, Grade 4 (both terms), the
       "الرسم الكتابي" (handwriting) lessons, as reproduced page by page on
       sahl.io (ien.edu.sa's digital lessons): "ألاحظ طريقة رسم الحرف تبعًا
       لاتجاه الأسهم" (each letter in its forms with arrows, some movements
       numbered ١ ٢ ٣), and the steps each letter is made of ("رسم الحرف (ل)
       يتكون من خطوتين: ألف، كأس تنزل تحت السطر"). Pages seen for ا ء ب ت ث
       د ذ ر ز و ط ظ ف ك ل لا م ه ة ج ح خ ـهـ ع غ س ش ق ي.
  [G]  Saudi Ministry of Education, لغتي, Grade 1, Part 2 (1447/2025), the
       model letters with red arrows and a start circle, four forms each, as
       collected in docs/research/arabic_sources.md ([SA-G1]): ك (all four
       forms), ع, ض, خ, ه (all four forms), ث.
  [K]  Mamoun Sakkal, "Arabic Alphabet Chart (In Naskh Style)" (sakkal.com,
       2007/2016): every letter in its four forms, with arrows and a dot
       where each pen stroke starts; "follow the direction of the arrows
       writing in a clockwise direction in general".
  [W]  J. Wightwick and M. Gaafar, Mastering Arabic Script: a guide to
       handwriting (Palgrave, 2005), as quoted in the research notes: "complete
       the main letter shape first and then add any 'dots'", dots and marks
       right to left; م "a tight circle in a clockwise direction"; ط ظ "loop
       first ... then add the vertical stroke downwards"; hamza "after
       completing the alif".
  [S]  Saudi curriculum exercises on the Naskh dots ("ترسم النقاط في خط النسخ
       منفصلة": true; "في خط النسخ عند كتابة نقاط حرف الثاء ... نفصل كل النقاط
       الثلاث"): in Naskh each dot is written on its own.
  [D]  belarabyapps.com, "تعليم كتابة الأرقام العربية" worksheets: ١ top to
       bottom; ٢ and ٣ start at the top right, go left over the top, then
       down the stem. The other digits: house choices (README).
  [H]  House choice: no source shows it (README, "House choices").
Only the order and direction were taken from these. Proportions were compared
with Noto Naskh Arabic (SIL OFL) on contact sheets; nothing was taken from it.

Directions: headings in pen.py's screen degrees (E 0, S 90, W 180, N 270);
"clockwise" as seen on the screen.
"""

from pen import Stroke, spline, tick, circle, S, W, N

# --- the frame -----------------------------------------------------------------------

B = 68.0        # baseline
A = 14.0        # ascender top
D = 98.0        # lowest descender (dots under ي)
T = 57.0        # a tooth's tip
RX = 98.0       # right connection x (on the baseline)
LX = 11.0       # left connection x (on the baseline)
CX = 54.5       # centre
CIRCLE = (54.5, 56.0, 19.0)     # the app's dotted circle for the harakat
DIG_TOP = 24.0                  # digits' top

ISO, FIN, INI, MED = "iso", "fin", "ini", "med"
JOINS_RIGHT = (FIN, MED)
JOINS_LEFT = (INI, MED)

# --- the characters, as taught ------------------------------------------------------------

# Strokes per character, as taught (the validator checks the XML against
# these, independently of the drawings). Dots count one stroke each (the
# Naskh school dots are separate [S]); ك's mark, ط ظ's upright, the hamza, the
# madda and لا's alif are strokes of their own.
STROKES = {chr(cp): n for cp, n in {
    # hamza, alif and its seats
    0x0621: 1,
    0x0622: 2, 0xFE82: 2,                           # آ ـآ
    0x0623: 2, 0xFE84: 2,                           # أ ـأ
    0x0624: 2, 0xFE86: 2,                           # ؤ ـؤ
    0x0625: 2, 0xFE88: 2,                           # إ ـإ
    0x0626: 2, 0xFE8A: 2, 0xFE8B: 2, 0xFE8C: 2,     # ئ ـئ ئـ ـئـ
    0x0627: 1, 0xFE8E: 1,                           # ا ـا
    # ب ة ت ث
    0x0628: 2, 0xFE90: 2, 0xFE91: 2, 0xFE92: 2,
    0x0629: 3, 0xFE94: 3,
    0x062A: 3, 0xFE96: 3, 0xFE97: 3, 0xFE98: 3,
    0x062B: 4, 0xFE9A: 4, 0xFE9B: 4, 0xFE9C: 4,
    # ج ح خ
    0x062C: 2, 0xFE9E: 2, 0xFE9F: 2, 0xFEA0: 2,
    0x062D: 1, 0xFEA2: 1, 0xFEA3: 1, 0xFEA4: 1,
    0x062E: 2, 0xFEA6: 2, 0xFEA7: 2, 0xFEA8: 2,
    # د ذ ر ز
    0x062F: 1, 0xFEAA: 1, 0x0630: 2, 0xFEAC: 2,
    0x0631: 1, 0xFEAE: 1, 0x0632: 2, 0xFEB0: 2,
    # س ش ص ض
    0x0633: 1, 0xFEB2: 1, 0xFEB3: 1, 0xFEB4: 1,
    0x0634: 4, 0xFEB6: 4, 0xFEB7: 4, 0xFEB8: 4,
    0x0635: 1, 0xFEBA: 1, 0xFEBB: 1, 0xFEBC: 1,
    0x0636: 2, 0xFEBE: 2, 0xFEBF: 2, 0xFEC0: 2,
    # ط ظ ع غ
    0x0637: 2, 0xFEC2: 2, 0xFEC3: 2, 0xFEC4: 2,
    0x0638: 3, 0xFEC6: 3, 0xFEC7: 3, 0xFEC8: 3,
    0x0639: 1, 0xFECA: 1, 0xFECB: 1, 0xFECC: 1,
    0x063A: 2, 0xFECE: 2, 0xFECF: 2, 0xFED0: 2,
    # ف ق ك ل (initial ك: one stroke, the top bar first [G])
    0x0641: 2, 0xFED2: 2, 0xFED3: 2, 0xFED4: 2,
    0x0642: 3, 0xFED6: 3, 0xFED7: 3, 0xFED8: 3,
    0x0643: 2, 0xFEDA: 2, 0xFEDB: 1, 0xFEDC: 2,
    0x0644: 1, 0xFEDE: 1, 0xFEDF: 1, 0xFEE0: 1,
    # م ن ه و ى ي
    0x0645: 1, 0xFEE2: 1, 0xFEE3: 1, 0xFEE4: 1,
    0x0646: 2, 0xFEE6: 2, 0xFEE7: 2, 0xFEE8: 2,
    0x0647: 1, 0xFEEA: 1, 0xFEEB: 1, 0xFEEC: 1,
    0x0648: 1, 0xFEEE: 1,
    0x0649: 1, 0xFEF0: 1,
    0x064A: 3, 0xFEF2: 3, 0xFEF3: 3, 0xFEF4: 3,
    # lam-alif
    0xFEFB: 2, 0xFEFC: 2,
    # harakat: fathatan, dammatan, kasratan, fatha, damma, kasra, shadda, sukun
    0x064B: 2, 0x064C: 2, 0x064D: 2, 0x064E: 1, 0x064F: 1, 0x0650: 1, 0x0651: 1, 0x0652: 1,
    # digits
    0x0660: 1, 0x0661: 1, 0x0662: 1, 0x0663: 1, 0x0664: 1, 0x0665: 1, 0x0666: 1, 0x0667: 1, 0x0668: 1, 0x0669: 1,
}.items()}

# The letters: (isolated code point, the presentation forms it has). A
# dual-joining letter's forms follow its isolated presentation form in
# Unicode's order: final, initial, medial; a right-joining letter has a final.
DUAL = [0x0626, 0x0628, 0x062A, 0x062B, 0x062C, 0x062D, 0x062E, 0x0633, 0x0634, 0x0635, 0x0636,
        0x0637, 0x0638, 0x0639, 0x063A, 0x0641, 0x0642, 0x0643, 0x0644, 0x0645, 0x0646, 0x0647, 0x064A]
RIGHT = [0x0622, 0x0623, 0x0624, 0x0625, 0x0627, 0x0629, 0x062F, 0x0630, 0x0631, 0x0632, 0x0648, 0x0649]
PRES_ISO = {   # the isolated presentation form of each letter (not stored: the base code point is)
    0x0622: 0xFE81, 0x0623: 0xFE83, 0x0624: 0xFE85, 0x0625: 0xFE87, 0x0626: 0xFE89, 0x0627: 0xFE8D,
    0x0628: 0xFE8F, 0x0629: 0xFE93, 0x062A: 0xFE95, 0x062B: 0xFE99, 0x062C: 0xFE9D, 0x062D: 0xFEA1,
    0x062E: 0xFEA5, 0x062F: 0xFEA9, 0x0630: 0xFEAB, 0x0631: 0xFEAD, 0x0632: 0xFEAF, 0x0633: 0xFEB1,
    0x0634: 0xFEB5, 0x0635: 0xFEB9, 0x0636: 0xFEBD, 0x0637: 0xFEC1, 0x0638: 0xFEC5, 0x0639: 0xFEC9,
    0x063A: 0xFECD, 0x0641: 0xFED1, 0x0642: 0xFED5, 0x0643: 0xFED9, 0x0644: 0xFEDD, 0x0645: 0xFEE1,
    0x0646: 0xFEE5, 0x0647: 0xFEE9, 0x0648: 0xFEED, 0x0649: 0xFEEF, 0x064A: 0xFEF1,
}
LAM_ALIF = {0xFEFB: ISO, 0xFEFC: FIN}
HARAKAT = list(range(0x064B, 0x0653))
DIGITS = list(range(0x0660, 0x066A))


def forms_of(base):
    """[(code point, form)] of a letter, the isolated one first."""
    out = [(base, ISO)]
    p = PRES_ISO.get(base)
    if base in DUAL:
        out += [(p + 1, FIN), (p + 2, INI), (p + 3, MED)]
    elif base in RIGHT:
        out.append((p + 1, FIN))
    return out


FORM = {}           # code point -> (letter's base code point, form)
for _b in [0x0621] + sorted(DUAL + RIGHT):
    for _cp, _f in forms_of(_b):
        FORM[_cp] = (_b, _f)
for _cp, _f in LAM_ALIF.items():
    FORM[_cp] = (0xFEFB, _f)

DRAW = {}           # code point -> function returning its strokes
SOURCE = {}         # code point -> where its order comes from


def family(base, src, cps=None):
    """Register fn(form) as the drawing of a letter's forms."""
    def deco(fn):
        for cp, f in (cps or forms_of(base)):
            DRAW[cp] = (lambda fn=fn, f=f: fn(f))
            SOURCE[cp] = src
        return fn
    return deco


def single(cp, src):
    def deco(fn):
        DRAW[cp] = fn
        SOURCE[cp] = src
        return fn
    return deco


# --- shared pieces --------------------------------------------------------------------------

def entry(x):
    """The connecting stroke from the letter before: from the right connection
    leftwards along the baseline to x."""
    return Stroke(RX, B, W).line(x, B)


def to_tail(s, x_turn):
    """From the pen going down to the baseline at about x_turn: curve onto the
    baseline and run left to the left connection (the joining tail)."""
    s.go(x_turn - 4.5, B, W, k=0.5)
    s.line(LX, B)
    return s


def dot(x, y):
    return tick(x, y, 3.2)


def dots(n, x, y, below=False):
    """The dots of a letter, centred on (x, y), in writing order: right to
    left [W]; three dots: the two side by side, right then left, then the
    one above (or below) them [H]."""
    g = 7.0
    if n == 1:
        return [dot(x, y)]
    if n == 2:
        return [dot(x + g / 2, y), dot(x - g / 2, y)]
    if below:
        return [dot(x + g / 2, y - 2.6), dot(x - g / 2, y - 2.6), dot(x, y + 3.0)]
    return [dot(x + g / 2, y + 2.6), dot(x - g / 2, y + 2.6), dot(x, y - 3.0)]


def hamza_shape(cx=55.0, cy=58.0, s=1.0):
    """The hamza [M]: from its top right tip a small crescent counter-clockwise
    (over the top to the left and down, نزول بميل), then right to the waist
    (تداخل), then the slanted dash down to the lower left (نزول). Drawn for a
    centre of (55, 58), then moved and scaled."""
    h = spline([(65.0, 50.5), (59.0, 47.5), (53.2, 49.6), (52.2, 55.2), (56.8, 58.4), (66.0, 57.6)],
               start=205.0, end=-5.0)
    h.turn(152.0)
    h.through([(54.5, 63.0), (44.5, 68.5)], end=158.0)
    return h.map(lambda p: (cx + (p[0] - 55.0) * s, cy + (p[1] - 58.0) * s))


def small_hamza(cx, cy, s=0.42):
    return hamza_shape(cx, cy, s)


def madda(cx, cy):
    """The madda: a short wave above the alif, right to left [H]."""
    return spline([(cx + 9.5, cy + 1.2), (cx + 3.5, cy - 1.2), (cx - 2.5, cy + 1.6), (cx - 9.0, cy - 1.6)],
                  start=205.0, end=200.0)


# --- ا ء and the alif with hamza or madda ------------------------------------------------------

def alif_body(form, top=A):
    """Isolated: from the top down, leaning a little ("وقفة البداية ... نزول
    بميل") [M]. Final: from the connection along the line, then UP the alif,
    ending at its top ("متصل بما قبله", arrow up) [M] [K]."""
    if form == ISO:
        return Stroke(56.4 - (top - A) * 0.05, top, 93.0).line(53.8, B), 55.1
    s = entry(66.0)
    s.go(61.2, 62.0, N - 2.0, k=0.5)
    s.line(62.6 - (top - A) * 0.05, top)
    return s, 61.9


@single(0x0621, "[M] (ء: نقطة البداية top right, نزول بميل, تداخل, نزول), [G]: one stroke.")
def hamza_alone():
    return [hamza_shape(54.5, 57.5, 1.18)]


@family(0x0627, "[M] ا: top to bottom; ـا: from the connection, then up the alif. [K] agrees.")
def alif(form):
    return [alif_body(form)[0]]


@family(0x0623, "[M] [K] alif as ا; then the hamza above it, after the alif [W].")
def alif_hamza_above(form):
    a, x = alif_body(form, 22.0)
    return [a, small_hamza(x + 1.0, 11.5, 0.46)]


@family(0x0625, "[M] [K] alif as ا; then the hamza below it [W].")
def alif_hamza_below(form):
    a, x = alif_body(form)
    return [a, small_hamza(x - 1.6, 79.0, 0.46)]


@family(0x0622, "[M] [K] alif as ا; then the madda, a wave drawn right to left [H].")
def alif_madda(form):
    a, x = alif_body(form, 22.0)
    return [a, madda(x + 0.8, 13.5)]


# --- the tooth: initial and medial ب ت ث ن ي ئ --------------------------------------------------

def tooth(form, x=50.0):
    """Initial: from the tooth's tip down, then the joining tail [M] [G] [K].
    Medial: from the connection, up into the tooth and back down, then the
    tail (the arrow rises into the tooth) [M] [G] [K]."""
    if form == INI:
        s = Stroke(x + 0.6, T, S + 3.0)
        s.line(x, 63.0)
    else:
        s = entry(x + 5.5)
        s.go(x + 1.1, 62.5, N, k=0.5)
        s.line(x + 0.7, T)
        s.turn(S + 3.0)
        s.line(x - 0.4, 63.0)
    return to_tail(s, x)


# --- ب ت ث ن ي ى ئ -------------------------------------------------------------------------------

def beh_body(form):
    """ب's body. Isolated: the right tip leaning right (يميل يمينا), down to
    the line, left along it (مستقر على السطر), up at the left (قوس إلى
    الأعلى) [M]. Final: from the connection up into the small tooth on its
    right and back down, then the same [M] [K]. Returns (stroke, x of the
    middle, x of the tooth)."""
    if form in (INI, MED):
        return tooth(form, 50.0), 50.0
    if form == ISO:
        s = Stroke(80.5, 55.0, 100.0)
        s.through([(77.8, 64.5), (71.0, B)], end=W)
        mid = 55.0
    else:
        s = entry(88.5)
        s.go(85.2, 62.5, N, k=0.5)
        s.line(84.8, T)
        s.turn(S + 3.0)
        s.through([(83.2, 64.0), (77.0, B)], end=W)
        mid = 58.0
    s.line(36.0, B)
    s.go(29.0, 55.0, N - 12.0, k=0.45)
    return s, mid


def above_y(form):
    return 44.0 if form in (INI, MED) else 45.0


@family(0x0628, "[M] [G] [K]: the body as above; then the dot below.")
def beh(form):
    b, x = beh_body(form)
    return [b] + dots(1, x, 77.0)


@family(0x062A, "[M] [K]: ب's body; then the two dots, right then left [W] [S].")
def teh(form):
    b, x = beh_body(form)
    return [b] + dots(2, x, above_y(form))


@family(0x062B, "[M] [G] [K]: ب's body; then the three dots, each on its own [S]; their order [H].")
def theh(form):
    b, x = beh_body(form)
    return [b] + dots(3, x, above_y(form) - 1.0)


def noon_body(form):
    """ن: like ب, the bowl deep below the line; initial and medial: the tooth
    [K] [W]."""
    if form in (INI, MED):
        return tooth(form, 50.0), 50.0
    if form == ISO:
        s = Stroke(74.5, 55.0, 95.0)
        s.through([(74.0, 64.0)], end=S)
        xr, mid = 74.0, 54.0
    else:
        s = entry(84.0)
        s.go(80.8, 62.5, N, k=0.5)
        s.line(80.4, T)
        s.turn(S + 2.0)
        s.through([(79.6, 64.0)], end=S)
        xr, mid = 79.6, 57.0
    xl = 2 * mid - xr
    s.through([(xr - 2.0, 75.0), (mid + 9.0, 84.0), (mid, 86.0), (mid - 9.0, 84.0), (xl + 1.5, 75.0), (xl, 65.0)],
              end=N - 6.0)
    s.go(xl + 1.2, 60.0, N + 6.0)
    return s, mid


@family(0x0646, "[K] [W]: right tip, the deep bowl clockwise, up at the left; then the dot.")
def noon(form):
    b, x = noon_body(form)
    y = 44.5 if form in (INI, MED) else 47.0
    return [b] + dots(1, x, y)


def yeh_body(form):
    """ى's body. Isolated: from the top right an arc down to the left (قوس
    ينزل إلى اليسار), then right (يميل يمينا), then the bowl like ن's (يشبه
    جسم النون), clockwise, up at the left [M] [K]. Final: from the connection
    up into the arc's top, then the same [M] [K]."""
    if form in (INI, MED):
        return tooth(form, 50.0), 50.0
    if form == ISO:
        s = Stroke(68.0, 46.5, 200.0)
        dx = 0.0
    else:
        s = entry(86.0)
        s.through([(80.0, 63.5), (75.0, 51.0)], end=N + 30.0)
        s.turn(200.0)
        dx = 6.0
    s.through([(58.5 + dx, 49.5), (52.0 + dx, 56.0)], end=70.0)
    s.through([(60.0 + dx, 63.0), (70.5 + dx, 67.0)], end=15.0)
    s.through([(74.0 + dx, 73.5), (66.0 + dx, 83.0), (51.0 + dx, 86.5), (37.0 + dx, 83.5), (29.0 + dx, 75.0)],
              end=N - 18.0)
    s.go(27.5 + dx, 69.0, N + 8.0)
    return s, 49.5 + dx


@family(0x0649, "[M] [K]: ي without its dots.")
def alef_maksura(form):
    return [yeh_body(form)[0]]


@family(0x064A, "[M] [K]: the body; then the two dots below, right then left [W] [S].")
def yeh(form):
    b, x = yeh_body(form)
    y = 77.0 if form in (INI, MED) else 95.0
    return [b] + dots(2, x, y)


@family(0x0626, "[M] [K]: ى's body (the tooth when joined to the next letter), never dots; then the hamza [W].")
def yeh_hamza(form):
    b, x = yeh_body(form)
    if form in (INI, MED):
        h = small_hamza(x + 0.5, 45.0)
    elif form == ISO:
        h = small_hamza(57.5, 37.0)
    else:
        h = small_hamza(63.0, 39.5)
    return [b, h]


# --- ه ة ---------------------------------------------------------------------------------------

def heh_body(form):
    """ه in its four shapes [M] [G]:
    isolated: from the top, clockwise all round;
    final: from the connection up the stem, down over the left, then the bottom
    left to right back to the stem;
    initial: from the top down the right side, left along the line, up the
    left side to the top again, then down the middle (the two eyes) and the
    tail;
    medial (the figure eight): from the connection down into the lower loop
    (clockwise), up through the line into the upper loop, over its tip and down
    its right side (clockwise), then the tail."""
    if form == ISO:
        s = Stroke(57.5, 42.0, 52.0)
        s.through([(66.5, 49.0), (70.5, 58.5), (65.0, 66.6), (55.0, 68.3), (45.6, 63.5), (44.2, 54.0),
                   (49.5, 46.0), (56.8, 42.3)], end=-30.0)
        return s
    if form == FIN:
        s = entry(70.0)
        s.go(66.6, 62.0, N, k=0.5)
        s.line(66.0, 45.5)
        s.turn(128.0)
        s.through([(57.5, 53.0), (50.0, 60.5), (51.0, 67.2)], end=10.0)
        s.through([(59.0, 67.6), (65.6, 67.0)], end=-10.0)
        return s
    if form == INI:
        s = Stroke(58.0, 44.5, 55.0)
        s.through([(65.8, 51.5), (69.0, 59.5), (65.0, 66.8), (58.0, 67.9)], end=W)
        s.line(46.0, B)
        s.through([(41.8, 62.0), (44.0, 51.5), (51.0, 46.0), (57.4, 45.0)], end=-15.0)
        s.turn(103.0)
        s.through([(55.5, 55.0), (52.8, 64.5)], end=118.0)
        return to_tail(s, 53.5)
    # medial
    s = entry(64.5)
    s.turn(S)
    s.through([(64.8, 75.0), (59.5, 81.0), (52.5, 79.5), (49.8, 72.0), (51.0, 64.5), (54.5, 56.5), (60.5, 49.5),
               (66.5, 45.5)], end=-55.0)
    s.turn(108.0)
    s.through([(64.6, 54.0), (61.0, 62.0)], end=118.0)
    return to_tail(s, 61.0)


@family(0x0647, "[M] [G]: all four forms as drawn there (see heh_body).")
def heh(form):
    return [heh_body(form)]


@family(0x0629, "[M] (هـ ة on one page): the body of ه or ـه; then the two dots above, right then left [W] [S].")
def teh_marbuta(form):
    b = heh_body(form)
    x = 57.0 if form == ISO else 58.0
    return [b] + dots(2, x, 33.5 if form == ISO else 35.0)


# --- ج ح خ -------------------------------------------------------------------------------------

def hah_body(form):
    """ح's body [M] [G] [K]. The head (حاجب) is drawn left to right, from its
    left tip. Isolated: the head, a sharp turn back down to the left, the bowl
    below the line counter-clockwise, ending up at the right. Initial: the
    head, then back down to the line and the tail. Medial and final: from the
    connection up into the head's right end and along under it to its left
    tip, then the head left to right as before, then down to the line and the
    tail (medial) or the bowl (final)."""
    if form == ISO:
        s = Stroke(43.0, 47.0, -12.0)
        s.go(70.5, 42.5, -2.0, k=0.35)
        dx = 0.0
    elif form == INI:
        s = Stroke(45.0, 55.0, -12.0)
        s.go(70.0, 51.0, -2.0, k=0.35)
        s.turn(212.0)
        s.through([(62.0, 61.5), (54.0, 67.6)], end=W)
        s.line(LX, B)
        return s
    else:
        xr = 70.0 if form == MED else 75.5
        top = 51.0 if form == MED else 42.5
        s = entry(xr + 4.0)
        s.go(xr - 0.5, B - 6.0, N, k=0.5)
        s.through([(xr - 1.5, top + 3.4)], end=200.0)
        s.through([(xr - 13.0, top + 5.0), (xr - 26.0, top + 5.4)], end=192.0)
        s.turn(-14.0)
        s.go(xr + 0.5, top, -2.0, k=0.35)
        if form == MED:
            s.turn(118.0)
            s.through([(xr - 4.5, 62.5), (xr - 10.0, 67.7)], end=W)
            s.line(LX, B)
            return s
        dx = 5.0
    s.turn(212.0)
    s.through([(54.0 + dx, 55.0), (40.0 + dx, 68.5), (41.0 + dx, 83.0), (54.5 + dx, 91.0), (72.5 + dx, 88.0)],
              end=-22.0)
    return s


def hah_dot(form, below):
    """ج's dot: in the bowl (isolated, final) or under the body (initial,
    medial); خ's above the head."""
    if below:
        return {ISO: (57.0, 72.0), FIN: (62.0, 72.0), INI: (60.0, 77.0), MED: (59.0, 77.0)}[form]
    return {ISO: (58.0, 35.5), FIN: (63.5, 36.5), INI: (58.0, 44.5), MED: (59.0, 44.0)}[form]


@family(0x062C, "[M] [K] [G] (as خ): the body; then the dot.")
def jeem(form):
    return [hah_body(form)] + dots(1, *hah_dot(form, True))


@family(0x062D, "[M] (يمر رسم الحاء بخطوتين: انحناء خفيف، شكل نصف دائري; arrows ١ ٢ ٣) [G] (as خ) [K].")
def hah(form):
    return [hah_body(form)]


@family(0x062E, "[G] (all four forms) [M] [K]: the body; then the dot above.")
def khah(form):
    return [hah_body(form)] + dots(1, *hah_dot(form, False))


# --- د ذ ر ز -----------------------------------------------------------------------------------

def dal_body(form):
    """Isolated: from the top tip down to the right (١), then left along the
    line (٢) [M]. Final: from the connection up the back to the tip (١, arrow
    up), back down, then left along the line (٢) [M] [K]."""
    if form == ISO:
        s = Stroke(50.0, 44.0, 52.0)
        dx = 0.0
    else:
        s = entry(73.5)
        s.through([(70.2, 62.0), (67.0, 52.5), (55.6, 43.6)], end=212.0)
        s.turn(46.0)
        dx = 4.0
    s.through([(59.0 + dx, 51.0), (64.6 + dx, 60.5)], end=104.0)
    s.go(58.0 + dx, B, W, k=0.5)
    s.line(37.0 + dx, B)
    return s, 49.0 + dx


@family(0x062F, "[M] (arrows ١ ٢) [K].")
def dal(form):
    return [dal_body(form)[0]]


@family(0x0630, "[M] [K]: د; then the dot above its tip.")
def thal(form):
    b, x = dal_body(form)
    return [b] + dots(1, x - 2.0, 39.0)


def reh_body(form):
    """Like ب's first part (the small tip), then an arc down below the line to
    the left (كجزء الباء الأول، طرف صغير، قوس ينزل تحت السطر) [M] [K]. Final:
    from the connection up into the tip, back down, the same [M] [K]."""
    if form == ISO:
        s = Stroke(62.5, 48.0, 92.0)
        dx = 0.0
    else:
        s = entry(74.0)
        s.go(70.2, 62.0, N - 6.0, k=0.5)
        s.line(68.0, 49.0)
        s.turn(96.0)
        dx = 5.0
    s.through([(63.4 + dx, 58.0), (62.0 + dx, 68.5), (55.5 + dx, 78.0), (43.0 + dx, 84.5)], end=190.0)
    s.go(35.0 + dx, 85.0, 172.0)
    return s, 62.5 + dx


@family(0x0631, "[M] (arrows ١ ٢ ٣) [K].")
def reh(form):
    return [reh_body(form)[0]]


@family(0x0632, "[M] (يرسم حرف الزاي مثل الراء بزيادة نقطة أعلاه) [K].")
def zain(form):
    b, x = reh_body(form)
    return [b] + dots(1, x + 0.5, 43.5)


# --- س ش ص ض -------------------------------------------------------------------------------------

SEEN_TOP = 54.5     # the tips of س ش's teeth


def teeth(s, xs, top=SEEN_TOP):
    """From the pen at the first tooth's tip: down, a small curve on the line
    and up to the next tip, for each tip in xs (right to left)."""
    for x0, x1 in zip(xs, xs[1:]):
        s.turn(S + 4.0)
        mid = (x0 + x1) / 2.0
        s.through([(x0 - 0.6, B - 4.2), (mid, B - 0.3), (x1 + 0.7, B - 4.5), (x1, top)], end=N - 4.0)
    return s


def bowl(s, xr, xl, depth=18.0, tip=61.0):
    """From the pen going down at xr: the U-bowl below the line, clockwise,
    up at the left to the tip [M] (حرف نون ينزل تحت السطر) [K]."""
    mid = (xr + xl) / 2.0
    w = xr - xl
    s.through([(xr, B + 1.0), (xr - 0.12 * w, B + depth * 0.62), (mid + 0.2 * w, B + depth * 0.97), (mid, B + depth),
               (mid - 0.22 * w, B + depth * 0.94), (xl + 0.06 * w, B + depth * 0.5), (xl, B - 1.0)], end=N - 4.0)
    s.go(xl + 1.0, tip, N + 6.0)
    return s


def seen_body(form):
    """س [M] [K]: from the first tooth's tip, three teeth right to left (the
    second bigger than the first), then the bowl below the line. Initial: the
    teeth, then the tail. Medial and final: from the connection up into the
    first tooth."""
    if form in (ISO, INI):
        x0 = 85.0 if form == ISO else 76.0
        s = Stroke(x0 + 0.5, SEEN_TOP, S + 4.0)
    else:
        x0 = 78.0 if form == MED else 88.0
        s = entry(x0 + 5.0)
        s.go(x0 + 0.8, 62.5, N, k=0.5)
        s.line(x0 + 0.4, SEEN_TOP)
    xs = [x0, x0 - 10.0, x0 - 20.0]
    teeth(s, [s.end[0]] + xs[1:])
    s.turn(S + 3.0)
    if form in (INI, MED):
        s.line(xs[-1] - 0.5, 63.5)
        to_tail(s, xs[-1] - 0.5)
    else:
        s.through([(xs[-1] - 0.6, 64.0)], end=S)
        bowl(s, xs[-1] - 0.8, xs[-1] - 43.0)
    return s, xs[1]


@family(0x0633, "[M] (رسم حرف (س) يتكون من خطوتين: السن الأولى صغيرة، السن الثانية أكبر، حرف نون) [K].")
def seen(form):
    return [seen_body(form)[0]]


@family(0x0634, "[M] (حرف الشين مثل السين بزيادة ثلاث نقط فوق الأسنان) [K]; the dots each on their own [S].")
def sheen(form):
    b, x = seen_body(form)
    return [b] + dots(3, x, 40.5)


def sad_body(form):
    """ص [G] (as ض) [K] [W]: the loop starts on its left side, half way up,
    and goes clockwise: up, over to the right, down, back left along the line;
    then the small tooth; then the bowl (isolated, final) or the tail
    (initial, medial). Medial and final: from the connection left along the
    line to the loop, which is drawn from its left as before, then back
    along the line."""
    x0 = {ISO: 60.0, INI: 62.0, MED: 60.0, FIN: 62.0}[form]
    loop = [(x0 + 6.0, 52.0), (x0 + 16.0, 46.6), (x0 + 25.5, 47.8), (x0 + 29.5, 55.0)]
    if form in (ISO, INI):
        s = Stroke(x0 + 1.0, 61.0, N + 22.0)
        s.through(loop, end=S + 10.0)
    else:
        s = entry(x0)
        s.turn(N - 10.0)
        s.through([(x0 + 1.4, 60.0)] + loop, end=S + 10.0)
    s.through([(x0 + 27.0, 64.0), (x0 + 20.0, 67.4)], end=W)
    s.line(x0 + 0.5, 67.4)
    s.go(x0 - 3.0, 61.0, N, k=0.5)
    s.line(x0 - 3.2, 58.0)
    s.turn(S + 3.0)
    s.line(x0 - 4.2, 63.5)
    if form in (INI, MED):
        to_tail(s, x0 - 4.2)
    else:
        bowl(s, x0 - 4.4, x0 - 45.0)
    return s, x0 + 15.5


@family(0x0635, "[G] (ض's four forms) [K] [W] (\"start the loop on the left, in the centre of the letter\").")
def sad(form):
    return [sad_body(form)[0]]


@family(0x0636, "[G] (all four forms) [K]: ص; then the dot over the loop.")
def dad(form):
    b, x = sad_body(form)
    return [b] + dots(1, x, 38.5)


# --- ط ظ -------------------------------------------------------------------------------------------

def tah_body(form):
    """ط ظ [M] (رأس صاد، طرف صغير; arrow ١ the loop, ٢ the alif) [W] [K]: the
    loop as ص's, clockwise from its left side, ending along the line with a
    small tip to the left (isolated, final) or the tail (initial, medial);
    then the upright, top down, at the loop's left. Returns (body, upright x)."""
    x0 = {ISO: 44.0, INI: 48.0, MED: 48.0, FIN: 46.0}[form]
    loop = [(x0 + 5.0, 52.0), (x0 + 14.0, 46.4), (x0 + 24.0, 47.6), (x0 + 30.0, 55.5)]
    if form in (ISO, INI):
        s = Stroke(x0 + 1.0, 61.0, N + 22.0)
        s.through(loop, end=S + 10.0)
    else:
        s = entry(x0)
        s.turn(N - 10.0)
        s.through([(x0 + 1.4, 60.0)] + loop, end=S + 10.0)
    s.through([(x0 + 27.5, 64.0), (x0 + 20.0, 67.4)], end=W)
    if form in (INI, MED):
        s.line(LX, B)
    else:
        s.line(x0 - 12.0, B)
    return s, x0 + 2.2


def upright(x):
    return Stroke(x + 1.0, A, 92.0).line(x, 62.0)


@family(0x0637, "[M] [W] [K]: the loop first, then the upright top down.")
def tah(form):
    b, x = tah_body(form)
    return [b, upright(x)]


@family(0x0638, "[M] [W] [K]: ط; then the dot, right of the upright.")
def zah(form):
    b, x = tah_body(form)
    return [b, upright(x)] + dots(1, x + 13.0, 38.5)


# --- ع غ -------------------------------------------------------------------------------------------

def ain_body(form):
    """ع غ [M] (شكل هلال، يميل يمينا، شكل نصف دائري) [G] [K].
    Isolated: the crescent from its top right tip, counter-clockwise; right
    to the waist; then the bowl below the line, counter-clockwise, ending at
    the right. Initial: the crescent from the top right down to the line,
    then the tail. Medial: from the connection up the left side of the
    closed head, over its top to the right, down to the line (clockwise),
    then the tail. Final: the same head, then the bowl."""
    if form == ISO:
        s = spline([(70.0, 39.0), (60.5, 34.0), (50.5, 37.5), (47.5, 45.0), (52.5, 51.5), (62.0, 53.4), (71.0, 51.6)],
                   start=195.0, end=-12.0)
        s.turn(150.0)
        s.through([(52.5, 60.5), (39.5, 72.5), (41.5, 85.5), (56.5, 92.0), (76.5, 88.0)], end=-20.0)
        return s
    if form == INI:
        s = spline([(69.5, 48.0), (60.5, 43.6), (51.5, 47.0), (49.2, 54.5), (53.5, 62.8), (62.0, 65.4), (70.0, 64.6)],
                   start=195.0, end=-8.0)
        s.turn(168.0)
        s.go(61.0, B, W, k=0.5)
        s.line(LX, B)
        return s
    xp = 58.0 if form == MED else 66.0
    s = entry(xp)
    s.turn(N - 18.0)
    s.through([(xp - 6.5, 58.5), (xp - 10.0, 50.0)], end=N + 30.0)
    s.through([(xp - 4.5, 49.0), (xp + 0.5, 50.0), (xp + 5.0, 49.0), (xp + 8.5, 49.2)], end=55.0)
    s.through([(xp + 5.6, 57.0), (xp + 0.3, 66.8)], end=140.0)
    if form == MED:
        s.go(xp - 5.0, B, W, k=0.5)
        s.line(LX, B)
    else:
        s.through([(xp - 11.5, 73.5), (xp - 20.0, 83.0), (xp - 15.5, 91.5), (xp - 1.5, 93.5), (xp + 13.0, 89.0)],
                  end=-20.0)
    return s


@family(0x0639, "[M] [G] (all four forms) [K].")
def ain(form):
    return [ain_body(form)]


@family(0x063A, "[M] [G] [K]: ع; then the dot above.")
def ghain(form):
    p = {ISO: (60.0, 26.0), INI: (60.0, 36.0), MED: (56.5, 40.0), FIN: (64.5, 40.0)}[form]
    return [ain_body(form)] + dots(1, *p)


# --- ف ق -------------------------------------------------------------------------------------------

HEAD = [(0.2, 56.0), (6.6, 47.0), (14.6, 51.6)]     # ف ق's head, from its left side (x from the head's left)


def feh_body(form):
    """ف [M]: the head from its lower left (the start dot), up, over to the
    right and down (clockwise), then the body along the line, under the
    head, rising at the left end (isolated) or running out as the tail
    (initial) [M]; medial and final: from the connection along the line to
    the head's left, the same head, back along the line [M] [K].
    Returns (stroke, head's centre x)."""
    hx = {ISO: 66.0, INI: 64.0, MED: 64.0, FIN: 68.0}[form]
    if form in (ISO, INI):
        s = Stroke(hx + 0.5, 64.5, N + 5.0)
    else:
        s = entry(hx)
        s.turn(N)
    s.through([(hx + dx, y) for dx, y in HEAD], end=S - 5.0)
    s.through([(hx + 15.4, 62.0), (hx + 10.0, 67.4)], end=W)
    if form in (INI, MED):
        s.line(LX, B)
    else:
        s.line(33.0, B)
        s.go(26.0, 56.0, N - 12.0, k=0.45)
    return s, hx + 7.4


@family(0x0641, "[M] (the four forms; start dot at the head's lower left) [K].")
def feh(form):
    b, x = feh_body(form)
    return [b] + dots(1, x, 38.0)


def qaf_body(form):
    """ق [M] (يتكون حرف (ق) من خطوتين: رأس الواو، حرف نون) [K]: the head
    clockwise, then the deep bowl. Isolated: the head starts at its foot on
    the right and goes left along its underside first, so it closes [K];
    final: from the connection along the line under the head. Initial and
    medial: as ف."""
    if form in (INI, MED):
        return feh_body(form)
    if form == ISO:
        hx = 57.0
        s = Stroke(hx + 9.5, 63.6, W)
        s.through([(hx + 3.0, 64.0), (hx, 56.0), (hx + 6.4, 47.0), (hx + 14.4, 51.4)], end=S - 5.0)
    else:
        hx = 63.0
        s = entry(hx)
        s.turn(N)
        s.through([(hx + dx, y) for dx, y in HEAD], end=S - 5.0)
    xr = s.end[0] + 0.6
    s.through([(xr, 62.0)], end=S)
    bowl(s, xr, xr - 39.0, depth=20.0, tip=62.0)
    return s, hx + 7.2


@family(0x0642, "[M] [K]: the body; then the two dots, right then left [W] [S].")
def qaf(form):
    b, x = qaf_body(form)
    return [b] + dots(2, x, 38.0)


# --- ك ل ---------------------------------------------------------------------------------------------

def kaf_mark(cx, cy):
    """The small mark inside ك (شارة الكاف), written last [M] [G]; shaped and
    drawn as a small hamza [H]."""
    return small_hamza(cx, cy, 0.44)


@family(0x0643, "[G] (all four forms) [M] (ك: ألف، تشبه الباء، شارة الكاف; initial arrows) [K].")
def kaf(form):
    """Isolated: the upright top down, left along the line, up at the left
    (ألف، تشبه الباء), then the mark [M] [G]. Final: from the connection up
    the upright, back down, the same [G]. Initial: ONE stroke, the top bar
    from its upper right end down to the left, the diagonal down to the right,
    left along the line [G] [M] [K]. Medial: from the connection up the
    diagonal and back down, then the line; then the top bar, from the elbow up
    to the right [G]."""
    if form == ISO:
        s = Stroke(71.5, A, 92.0)
        s.line(70.6, 62.0)
        s.go(64.0, B, W, k=0.5)
        s.line(36.0, B)
        s.go(29.5, 59.0, N - 10.0, k=0.45)
        return [s, kaf_mark(52.0, 48.5)]
    if form == FIN:
        s = entry(78.0)
        s.go(74.6, 62.0, N, k=0.5)
        s.line(75.2, A)
        s.turn(S + 2.0)
        s.line(74.0, 62.0)
        s.go(67.5, B, W, k=0.5)
        s.line(34.0, B)
        s.go(27.5, 59.0, N - 10.0, k=0.45)
        return [s, kaf_mark(52.5, 48.5)]
    if form == INI:
        s = Stroke(75.0, 30.5, 152.0)
        s.line(50.0, 44.5)
        s.turn(40.0)
        s.through([(60.5, 53.0), (66.8, 61.5)], end=110.0)
        s.go(61.5, B, W, k=0.5)
        s.line(LX, B)
        return [s]
    s = entry(70.0)
    s.go(66.0, 63.0, 230.0, k=0.5)
    s.through([(58.0, 53.5), (50.0, 44.5)], end=222.0)
    s.turn(45.0)
    s.through([(58.5, 53.6), (64.6, 61.6)], end=112.0)
    s.go(59.5, B, W, k=0.5)
    s.line(LX, B)
    bar = Stroke(50.5, 44.0, -28.0).line(75.5, 30.5)
    return [s, bar]


@family(0x0644, "[M] (ل: ألف، كأس تنزل تحت السطر; arrows for ل ـل ـلـ) [W] (medial and final: up first, back down the same path) [K].")
def lam(form):
    """Isolated: the upright top down, then the cup below the line,
    clockwise, up at the left [M]. Initial: the upright, then the tail [M].
    Medial and final: from the connection up the upright, back down, then the
    tail or the cup [W] [K] [M]."""
    if form in (ISO, INI):
        x = 64.0 if form == ISO else 52.0
        s = Stroke(x + 0.8, A, 92.0)
        s.line(x, 62.0)
    else:
        x = 70.0 if form == FIN else 56.0
        s = entry(x + 4.0)
        s.go(x + 0.9, 62.0, N, k=0.5)
        s.line(x + 1.5, A)
        s.turn(S + 1.5)
        s.line(x, 62.0)
    if form in (INI, MED):
        return [to_tail(s, x)]
    s.through([(x - 0.2, B)], end=S)
    bowl(s, x - 0.2, x - 36.5, depth=20.0, tip=60.0)
    return [s]


# --- م ---------------------------------------------------------------------------------------------

@family(0x0645, "[M] (م: رأس كالمثلث، انحناء، ألف; arrows for م مـ) [W] [K].")
def meem(form):
    """Isolated: the head from its left, over the top to the right, down,
    back left along the line (clockwise), then the tail straight down [M] [W].
    Initial: from the head's lower left, right along the line, up its right
    side, over the top to the left and down (counter-clockwise, as the
    primer's arrows go), then the tail [M]. Final: from the connection, the
    isolated head, then the tail; medial: from the connection, the initial
    head, then the tail [H]."""
    if form in (ISO, FIN):
        x = 47.0 if form == ISO else 51.0
        top = [(x + 2.0, 55.0), (x + 9.0, 49.6), (x + 16.0, 53.6), (x + 18.0, 60.0)]
        if form == ISO:
            s = Stroke(x - 0.4, 63.5, N + 18.0)
            s.through(top, end=S)
        else:
            s = entry(x - 1.0)
            s.turn(N)
            s.through([(x - 0.6, 60.5)] + top, end=S)
        s.through([(x + 16.5, 65.2), (x + 10.0, 67.5)], end=W)
        s.line(x - 0.5, 67.6)
        s.turn(S + 6.0)
        s.line(x - 3.0, 93.5)
        return [s]
    x = 48.0
    if form == INI:
        s = Stroke(x, 67.5, -8.0)
    else:
        s = entry(x - 1.0)
        s.turn(-8.0)
    s.through([(x + 9.0, 66.8), (x + 15.0, 60.5), (x + 12.0, 52.0), (x + 3.5, 51.6), (x - 1.5, 58.5),
               (x - 0.2, 66.3)], end=115.0)
    return [to_tail(s, x)]


# --- و ؤ ---------------------------------------------------------------------------------------------

def waw_body(form):
    """و [M] (رأس الفاء، حرف الراء; arrows ١ ٢ ٣) [K]: the head clockwise,
    then the tail down below the line to the left. Isolated: the head starts
    at its foot on the right and goes left along its underside first, so it
    closes [K]; final: from the connection along the line under the head."""
    hx = 55.0 if form == ISO else 57.0
    head = [(hx, 56.5), (hx + 5.6, 49.0), (hx + 13.0, 52.6), (hx + 14.0, 60.0)]
    if form == ISO:
        s = Stroke(hx + 10.5, 64.0, W)
        s.through([(hx + 3.0, 64.4)] + head, end=S + 8.0)
    else:
        s = entry(hx)
        s.turn(N)
        s.through(head[1:], end=S + 8.0)
    x = s.end[0]
    s.through([(x - 1.0, 70.0), (x - 6.5, 79.0), (x - 16.0, 84.6), (x - 28.0, 86.0)], end=182.0)
    return s, hx + 6.5


@family(0x0648, "[M] [K].")
def waw(form):
    return [waw_body(form)[0]]


@family(0x0624, "[M] [K]: و; then the hamza above its head [W].")
def waw_hamza(form):
    b, x = waw_body(form)
    return [b, small_hamza(x + 0.5, 40.0)]


# --- لا ---------------------------------------------------------------------------------------------

@family(0xFEFB, "[M] (لا: لام ناقصة الكأس، وصلة، ألف مائلة; arrows for ـلا) [K].", cps=list(LAM_ALIF.items()))
def lam_alif(form):
    """Two strokes [M]: the lam without its cup, its foot running on to the
    left as the joint (وصلة), then the slanted alif from the top left down to
    meet it. Final: from the connection up the lam and back down first [M]
    [W]."""
    if form == ISO:
        x = 65.0
        s = Stroke(x + 1.0, A, 92.0)
        s.line(x, 58.0)
    else:
        x = 70.0
        s = entry(x + 4.0)
        s.go(x + 0.9, 62.0, N, k=0.5)
        s.line(x + 1.5, A)
        s.turn(S + 1.5)
        s.line(x, 58.0)
    s.through([(x - 3.5, 65.2), (x - 12.0, 68.3), (x - 26.0, 69.0)], end=184.0)
    s.line(x - 31.0, 69.2)
    a = spline([(x - 27.0, 18.0), (x - 19.0, 42.5), (x - 11.0, 67.0)], start=64.0, end=72.0)
    return [s, a]


# --- the harakat ------------------------------------------------------------------------------------

def fatha_at(y):
    """A short oblique stroke, upper right to lower left [H] (common
    practice, research notes)."""
    return Stroke(59.6, y, 151.0).line(49.4, y + 5.6)


def damma_at(cx, s=1.0):
    """A small و: its head clockwise from its foot, then the tail down to the
    left [W] [H]."""
    d = spline([(56.6, 28.6), (53.6, 28.7), (52.3, 25.6), (54.4, 22.9), (57.4, 23.4), (58.7, 26.4), (58.1, 29.5)],
               start=W, end=S + 12.0)
    d.through([(56.4, 32.6), (52.6, 35.0), (48.6, 35.6)], end=182.0)
    return d.map(lambda p: (cx + (p[0] - 55.5) * s, 35.6 - (35.6 - p[1]) * s))


@single(0x064E, "[H] (research notes 1.4.3: upper right to lower left), above the letter.")
def fatha():
    return [fatha_at(28.0)]


@single(0x0650, "[H] (research notes 1.4.3), below the letter.")
def kasra():
    return [fatha_at(80.5)]


@single(0x064F, "[W] (a small و) [H]: the head clockwise, then the tail.")
def damma():
    return [damma_at(55.5, 1.25)]


@single(0x0652, "[W] (a small circle) [H]: from the top, clockwise, as ه.")
def sukun():
    return [circle(54.5, 29.0, 3.8, 270.0, True)]


@single(0x0651, "[W] (a small \"w\", right to left) [H]: from the right tip, three tips leftwards.")
def shadda():
    s = Stroke(61.6, 24.6, S + 8.0)
    s.through([(60.6, 30.8), (57.8, 32.4), (55.6, 29.6), (55.0, 26.2)], end=N - 8.0)
    s.turn(S + 6.0)
    s.through([(54.0, 31.2), (51.2, 32.4), (48.8, 29.6), (48.0, 25.4)], end=N - 12.0)
    return [s]


@single(0x064B, "[H] (the doubled mark, the upper one first; research notes 1.4.3).")
def fathatan():
    return [fatha_at(22.5), fatha_at(29.5)]


@single(0x064D, "[H] (the doubled mark, the upper one first).")
def kasratan():
    return [fatha_at(79.0), fatha_at(86.0)]


@single(0x064C, "[H] (two dammas side by side, right then left).")
def dammatan():
    return [damma_at(61.0, 1.0), damma_at(48.0, 1.0)]


# --- the digits ---------------------------------------------------------------------------------------

@single(0x0660, "[H]: a dot (a short tick, larger than a letter's dot).")
def d0():
    return [tick(54.5, 50.0, 5.5)]


@single(0x0661, "[D]: top to bottom.")
def d1():
    return [Stroke(56.6, DIG_TOP, 93.0).line(53.6, B)]


@single(0x0662, "[D]: from the top right, left over the top, then down the stem.")
def d2():
    s = spline([(67.0, 24.6), (61.5, 28.0), (55.5, 27.2), (51.0, 24.6)], start=165.0, end=200.0)
    s.turn(95.0)
    s.through([(51.8, 46.0), (54.6, B)], end=82.0)
    return [s]


@single(0x0663, "[D]: from the top right, the two teeth leftwards, then down the stem.")
def d3():
    s = Stroke(70.5, 24.6, S + 10.0)
    s.through([(69.0, 30.0), (66.0, 31.6), (62.8, 29.0), (62.0, 25.0)], end=N - 8.0)
    s.turn(S + 10.0)
    s.through([(60.8, 30.2), (57.6, 31.6), (54.0, 28.8), (51.0, 24.4)], end=N - 30.0)
    s.turn(95.0)
    s.through([(51.8, 46.0), (54.4, B)], end=82.0)
    return [s]


@single(0x0664, "[H] (research notes 1.4.4): from the top right, two bulges to the left, the foot to the right.")
def d4():
    s = spline([(66.5, 26.0), (57.5, 25.4), (52.0, 30.6), (55.8, 36.6), (64.5, 39.0)], start=195.0, end=5.0)
    s.turn(160.0)
    s.through([(52.0, 44.0), (46.0, 54.5), (50.0, 64.0), (60.0, 67.5), (69.5, 66.6)], end=-8.0)
    return [s]


@single(0x0665, "[H] (research notes 1.4.4): a closed loop from the top, clockwise (as ه).")
def d5():
    s = Stroke(55.0, 30.0, 55.0)
    s.through([(62.6, 39.0), (65.5, 51.0), (60.6, 61.6), (51.0, 62.6), (44.8, 54.0), (46.2, 42.0), (51.6, 33.2),
               (54.6, 30.2)], end=-35.0)
    return [s]


@single(0x0666, "[H] (research notes 1.4.4): the top from the left to the right, then down.")
def d6():
    s = spline([(44.0, 28.0), (49.0, 24.8), (56.5, 25.0), (60.5, 30.0), (62.4, 46.0), (65.0, B)],
               start=-55.0, end=80.0)
    return [s]


@single(0x0667, "[H] (research notes 1.4.4): top left down to the point, up to the top right.")
def d7():
    s = spline([(42.0, 24.6), (47.2, 45.0), (54.5, B)], start=72.0, end=70.0)
    s.turn(-70.0)
    s.through([(61.8, 45.0), (67.0, 24.6)], end=-72.0)
    return [s]


@single(0x0668, "[H] (research notes 1.4.4): bottom left up to the apex, down to the bottom right.")
def d8():
    s = spline([(42.0, B), (47.2, 47.6), (54.5, 24.6)], start=-72.0, end=-70.0)
    s.turn(70.0)
    s.through([(61.8, 47.6), (67.0, B)], end=72.0)
    return [s]


@single(0x0669, "[H] (research notes 1.4.4): the head clockwise from its foot (as و's), then the stem down.")
def d9():
    s = spline([(58.5, 42.0), (50.0, 41.6), (45.6, 33.6), (50.6, 25.2), (59.0, 25.6), (62.2, 33.4), (61.4, 42.0)],
               start=W, end=S + 2.0)
    s.through([(61.8, 55.0), (62.6, B)], end=86.0)
    return [s]


# --- the groups, for the contact sheets -------------------------------------------------------------------

def _letters_in_order():
    out = [[0x0621]]
    for b in [0x0627, 0x0623, 0x0625, 0x0622, 0x0628, 0x062A, 0x062B, 0x0646, 0x064A, 0x0649, 0x0626,
              0x062C, 0x062D, 0x062E, 0x062F, 0x0630, 0x0631, 0x0632, 0x0633, 0x0634, 0x0635, 0x0636,
              0x0637, 0x0638, 0x0639, 0x063A, 0x0641, 0x0642, 0x0643, 0x0644, 0x0645, 0x0647, 0x0629,
              0x0648, 0x0624]:
        out.append([cp for cp, _ in forms_of(b)])
    out.append([0xFEFB, 0xFEFC])
    return out


ROWS = _letters_in_order() + [HARAKAT[:4], HARAKAT[4:], DIGITS[:4], DIGITS[4:8], DIGITS[8:]]
GROUPS = (("letters", [chr(c) for row in ROWS[:-5] for c in row]),
          ("harakat", [chr(c) for c in HARAKAT]),
          ("digits", [chr(c) for c in DIGITS]))


def strokes_of(ch):
    return DRAW[ord(ch)]()


def form_of(cp):
    """'iso', 'fin', 'ini', 'med' for a letter form; 'mark' or 'digit'."""
    if cp in FORM:
        return FORM[cp][1]
    if cp in HARAKAT:
        return "mark"
    if cp in DIGITS:
        return "digit"
    raise KeyError(hex(cp))
