# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

"""The Devanagari letters, drawn by hand as centreline strokes.

Our own work: every shape below was drawn for this project, in code. Nothing
here is traced or extracted from a font or from anyone else's stroke data.

Each letter is a function returning its strokes in writing order, registered
with @letter(code point, plan, source). The plan is the stroke count as taught,
one character per stroke, saying what each stroke is (validate.py checks the
drawings against it):

    h  the headline (shirorekha), left to right along y = HL
    b  a vertical bar (khari pai), top to bottom
    d  a dot (a short tick)
    s  any other stroke

STROKE ORDER SOURCES (cited per letter below)
  [C]  Central Hindi Directorate (केंद्रीय हिंदी निदेशालय), "देवनागरी लिपि एवं
       हिंदी वर्तनी का मानकीकरण", revised edition 2024, §2.5 "हिंदी वर्णमाला
       लेखन विधि", book pages 15-18: every vowel and consonant built up stage
       by stage, the finished letter with direction arrows. The Government of
       India's standard: followed wherever it shows the letter. (Saved, with
       the 2016 edition, under data/raw/research/hi/; see
       docs/research/hindi_sources.md §1.3-1.4.)
  [S]  Wikimedia Commons, Category:Devanagari stroke order (SVG): the numbered
       diagrams "Devanagari अ stroke order.svg" ... "Devanagari औ stroke
       order.svg" and "Devanagari झ stroke order.svg" (by Saurmandal, CC BY-SA
       3.0, as used on Wiktionary): the vowels and झ, with stroke counts.
  [O]  The same category (GIF): the animations "Deva-क-order.gif" ...
       "Deva-ह-order.gif" (by Opiaterein, CC BY 3.0): 27 consonants, written
       stroke by stroke over a grey model.
  [J]  The same category: the animations "Devanagari a अ.gif" ... (by
       JackPotte, CC BY-SA 3.0): vowels and consonants, a second, independent
       set.
  [T]  The rules all of these follow, as taught in Indian schools: the
       letter's body first, from the top; then the vertical bar, top to
       bottom; the headline last, left to right, over the whole letter, except
       over अ आ ओ औ ऑ थ ध भ, where it covers only the bar's part ([C]). A mark
       on the headline (the curl of ई, the strokes of े ै ो ौ, the hooks of ि ी)
       starts at the headline and goes up and out; a sign under the letter
       starts where it joins it. A dot or a candra comes after the finished
       letter, headline included ([C] ङ ड़ ढ़).
  [H]  House choice, where none of the sources shows the character (the vowel
       signs alone, ऌ ळ ॐ ऽ, the digits): drawn by [T]'s rules; see README.
Where the sources disagree, [C] is followed; the per-letter notes say who
differs.
Only the order and direction of strokes were taken from these; no shape was
traced. Proportions were compared with Noto Sans Devanagari (SIL OFL) beside
the drawings; nothing was taken from it.

Coordinates: KanjiVG's 109-unit box, Y down. Letters are drawn in a frame of
their own (font-like proportions, the headline at HL, the baseline at BL) and
then narrowed by XS and centred (final()); vowel signs are drawn in place,
around an imaginary consonant in the middle of the box (CONSONANT).
"""

from pen import Stroke, poly, spline

HL = 28.0          # the headline
BL = 84.0          # the baseline
XS = 0.90          # letters are drawn a little narrower than they are designed
CENTRE = 54.5

# The imaginary consonant the vowel signs are placed around (final coordinates):
# its body spans x 32..77, its bar stands at x 75.
CONSONANT = dict(x0=32.0, x1=77.0, bar=75.0)

DOWN, UP, LEFT, RIGHT = (0.0, 1.0), (0.0, -1.0), (-1.0, 0.0), (1.0, 0.0)


class Letter:
    __slots__ = ("cp", "plan", "source", "draw", "kind", "base", "nukta")

    def __init__(self, cp, plan, source, draw, kind, base=None, nukta=None):
        self.cp, self.plan, self.source, self.draw, self.kind = cp, plan, source, draw, kind
        self.base = base        # nukta forms: the base letter's code point
        self.nukta = nukta      # and where the dot goes, in the base's frame


LETTERS = {}


def letter(cp, plan, source, kind="letter"):
    """Register a drawing. kind: 'letter' (centred), 'sign' (drawn in place)."""
    def reg(f):
        assert cp not in LETTERS, hex(cp)
        LETTERS[cp] = Letter(cp, plan, source, f, kind)
        return f
    return reg


def nukta_form(cp, base, at, source="[T] (as [C]'s ड़ ढ़): the base letter, headline included, then the dot"):
    """A letter with a nukta: the base letter's strokes, then the dot."""
    b = LETTERS[base]
    LETTERS[cp] = Letter(cp, b.plan + "d", source, None, "letter", base=base, nukta=at)


# --- helpers ----------------------------------------------------------------------------

def head(x0, x1, y=HL):
    """The headline: one stroke, left to right."""
    return poly([(x0, y), (x1, y)])


def bar(x, y0=HL, y1=BL):
    """A vertical bar, top to bottom."""
    return poly([(x, y0), (x, y1)])


def line(*pts):
    return poly(list(pts))


def S(*pts, t0=None, t1=None):
    """A smooth stroke through the points (the first is where it starts)."""
    return spline(list(pts), t0, t1)


def stub(x, y1, y0=HL):
    """A short straight start hanging from the headline, to be continued."""
    return Stroke(x, y0).line(x, y1)


def dot(x, y, r=2.0):
    """A dot, as a short tick from upper left to lower right (4 units)."""
    return poly([(x - r * 0.6, y - r * 0.8), (x + r * 0.6, y + r * 0.8)])


def candra(cx, top, w=22.0, h=10.0):
    """The candra (half moon) of ॅ ॉ ँ ऍ ऑ: left to right, down and round and up."""
    l, r = cx - w / 2, cx + w / 2
    return S((l, top), (l + 0.14 * w, top + 0.62 * h), (cx, top + h), (r - 0.14 * w, top + 0.62 * h), (r, top),
             t0=(0.15, 1.0), t1=(0.15, -1.0))


# --- vowels -----------------------------------------------------------------------------

def _a_body():
    """अ's first two strokes: the 3 and its tongue (shared by आ ओ औ ऑ). The
    headline does not cover the 3 ([C]), so its small lobe rises to the
    headline's height."""
    three = S((29.5, 31.5), (40.5, 27.5), (50.5, 31), (53, 39), (47, 46.5), (38.5, 50))
    three.through([(49.5, 53.5), (55.5, 62), (54, 72), (45, 78.5), (33, 77), (25.5, 68.5), (22, 59), (20.5, 52)],
                  join="corner")
    tongue = line((50.5, 53), (76.5, 52.5))
    return [three, tongue]


@letter(0x0905, "ssbh", "[C] p.15; [S] 'Devanagari अ stroke order.svg' (4); [J] 'Devanagari a अ.gif'")
def a():
    # अ, 4: the 3 (from the top left: the small lobe, then the big one), the
    # tongue out to the bar, the bar, the headline over the bar's part only.
    return _a_body() + [bar(76.5), head(61, 83)]


@letter(0x0906, "ssbbh", "[C] p.15; [S] 'Devanagari आ stroke order.svg' (5); [J] 'Devanagari aa आ.gif'")
def aa():
    # आ, 5: अ's 3 strokes, the second bar (ा), the headline over the bars.
    return _a_body() + [bar(76.5), bar(101), head(61, 106)]


def _i_body():
    """इ's body: from the headline down, left, round the upper bowl, the big
    curve on the right, the knot at the bottom left, the tail down to the right."""
    s = stub(65.5, 37)
    s.through([(62, 41.5), (52, 42.5), (41.5, 43.5), (36, 48.5), (38, 55), (46, 56.5), (57, 54), (66.5, 55.5),
               (71, 62.5), (69, 72), (60, 79), (48.5, 80.5), (39.5, 78), (35.5, 73), (39, 68.5), (45, 71), (50, 79),
               (57, 93)])
    return s


@letter(0x0907, "sh", "[C] p.15; [S] 'Devanagari इ stroke order.svg' (2); [J] 'Devanagari i इ.gif'")
def i():
    # इ, 2: the body (from the headline: down, left, round, the big curve, the
    # knot, the tail), then the headline.
    return [_i_body(), head(31, 76)]


@letter(0x0908, "ssh", "[C] p.15; [S] 'Devanagari ई stroke order.svg' (3); [J] 'Devanagari ii ई.gif'")
def ii():
    # ई, 3: इ's body, the curl (from the headline up, curling over to the
    # right), the headline.
    curl = S((65.5, HL), (62.5, 20), (63, 12.5), (68, 8), (75, 8.5), t0=(-0.25, -1.0))
    return [_i_body(), curl, head(31, 76)]


def _u_body():
    """उ's body: from the top left of the small lobe ([C], [J]), round to the
    right and back, then the big lobe right, down, round to the left and up."""
    s = S((54, 30.5), (61.5, 29.5), (67, 33.5), (68.5, 40.5), (62.5, 47.5), (52, 50.5), t0=(1.0, -0.1))
    s.through([(62, 53), (68, 61.5), (68, 72), (60, 80.5), (47, 81.5), (37, 74.5), (31.5, 63), (29, 53)], join="corner")
    return s


@letter(0x0909, "sh", "[C] p.15; [J] 'Devanagari u उ.gif'; [S] 'Devanagari उ stroke order.svg' (2; starts at the headline)")
def u():
    # उ, 2: the body (hanging from the headline: small lobe, big lobe), the headline.
    return [_u_body(), head(27, 78)]


@letter(0x090A, "ssh", "[C] p.15; [S] 'Devanagari ऊ stroke order.svg' (3); [J] 'Devanagari uu ऊ.gif' (hook after the headline)")
def uu():
    # ऊ, 3: उ's body, the hook on the right (from the big lobe up, over and
    # down), the headline.
    hook = S((67.5, 59), (76, 52), (87, 50), (94.5, 56.5), (95, 68), (89.5, 77), (82, 82))
    return [_u_body(), hook, head(27, 96)]


@letter(0x090B, "sbsh", "[C] p.15; [J] 'Devanagari r ऋ.gif'; [S] 'Devanagari ऋ stroke order.svg' (4; the curl before the bar)")
def vocalic_r():
    # ऋ, 4: the left part (from the left, over to the bar, then the diagonal
    # down to the left), the bar, the right part (from the bar: up into the
    # small loop, down, round and out to the right), the headline. [J] and
    # [O]'s क फ put what stands right of the bar after it; [S] has the right
    # part before the bar.
    left = S((19, 49), (28, 44.5), (38, 44.5), (46.5, 48.5), (52.5, 54))
    left.through([(36.5, 63.5), (21, 72)], join="corner")
    right = S((53.5, 57.5), (60.5, 57), (66, 54), (70, 49.5), (69, 44.5), (64.5, 43.5), (62, 47), (64.5, 51.5),
              (70.5, 55), (73.5, 60), (69, 65.5), (64.5, 71), (65, 78), (71.5, 82), (82, 82.5), (91, 78.5))
    return [left, bar(53), right, head(16, 94)]


@letter(0x090C, "ssh", "[H] (rare; in none of the sources): ल's C first ([C] [O]), then the right part, the headline [T]")
def vocalic_l():
    # ऌ, 3 (rare; no source shows it): the C on the left (from the middle up,
    # over and down to its point, as ल's [O]), then the right part (from the
    # headline down, round the right lobe and the hook below), the headline.
    c = S((56, 60), (55, 51.5), (48.5, 46), (38.5, 46), (30.5, 52), (28, 62), (32.5, 72), (39.5, 79), (46, 83), t0=UP)
    right = stub(70.5, 44)
    right.through([(77, 48), (81, 56.5), (79, 65), (71.5, 70), (65.5, 73.5), (62, 79.5), (62.5, 86), (68.5, 91),
                   (78, 92), (87, 88.5)])
    return [c, right, head(22, 87)]


def _e_body():
    """ए's two strokes."""
    left = stub(38.5, 49)
    left.through([(41.5, 57), (49.5, 63), (58.5, 70), (64.5, 77), (65, 85), (61.5, 92.5)])
    right = stub(68.5, 43)
    right.through([(66, 50.5), (58.5, 55)])
    return [left, right]


@letter(0x090D, "sshs", "[C] ए, then the candra over the finished letter [T]")
def candra_e():
    # ऍ, 4: ए's three strokes, then the candra above (left to right).
    return _e_body() + [head(28, 80), candra(55, 6.5)]


@letter(0x090F, "ssh", "[C] p.15; [S] 'Devanagari ए stroke order.svg' (3); [J] 'Devanagari e ए.gif'")
def e():
    # ए, 3: the left stroke (from the headline down, then the long diagonal),
    # the right stroke (from the headline down, curving in), the headline.
    return _e_body() + [head(28, 80)]


@letter(0x0910, "sssh", "[C] p.15; [S] 'Devanagari ऐ stroke order.svg' (4); [J] 'Devanagari ɛ ऐ.gif' (mark drawn downwards)")
def ai():
    # ऐ, 4: ए's two strokes, the mark (from the headline up to the left: [C],
    # [S]; [J] draws it downwards), the headline.
    return _e_body() + [_e_mark(68.5), head(28, 80)]


@letter(0x0911, "ssbbhs", "[C] आ, then the candra over the finished letter [T]")
def candra_o():
    # ऑ, 6: आ's five strokes, then the candra above, over the second bar.
    return _a_body() + [bar(76.5), bar(101), head(61, 106), candra(89, 6.5)]


def _e_mark(x, lower=False):
    """The mark of े ऐ ो ओ: from the headline at x, up to the left ([C], [S]).
    lower: the second mark of ै ौ औ, beside the first, a little to its left
    and lower; it is written first ([C], [S])."""
    if lower:
        return S((x - 9, HL), (x - 12, 22.5), (x - 16.5, 18.5), (x - 22, 16.5), (x - 26, 17.5), t0=(-0.45, -1.0))
    return S((x, HL), (x - 5, 18.5), (x - 10.5, 11.5), (x - 16.5, 8), (x - 21, 9), t0=(-0.45, -1.0))


@letter(0x0913, "ssbbsh", "[C] p.15; [S] 'Devanagari ओ stroke order.svg' (6); [J] 'Devanagari o ओ.gif' (mark drawn downwards)")
def o():
    # ओ, 6: अ's 3, the second bar, the mark (from the top of the bar up to the
    # left), the headline over the bars.
    return _a_body() + [bar(76.5), bar(101), _e_mark(101), head(61, 106)]


@letter(0x0914, "ssbbssh", "[C] p.15; [S] 'Devanagari औ stroke order.svg' (7); [J] 'Devanagari au औ.gif' (marks drawn downwards)")
def au():
    # औ, 7: as ओ, with two marks: the lower one first, then the upper ([C],
    # [S]).
    return _a_body() + [bar(76.5), bar(101), _e_mark(101, True), _e_mark(101), head(61, 106)]


# --- consonants -------------------------------------------------------------------------

@letter(0x0915, "sbsh", "[C] p.16; [O] 'Deva-क-order.gif'; [J] 'Devanagari k क.gif'")
def ka():
    # क, 4: the loop on the left (from the bar, left over the top, round and
    # back to the bar), the bar, the hook on the right (from the bar up, over
    # and down), the headline.
    loop = S((57, 46.5), (49, 42), (38.5, 41), (29, 45.5), (25, 55), (28.5, 63.5), (38, 67.5), (48.5, 66), (57, 60.5))
    hook = S((57.5, 53), (65, 46.5), (73.5, 43.5), (81, 47.5), (84.5, 56), (83, 65.5), (78.5, 72.5), (73, 77.5))
    return [loop, bar(57), hook, head(21, 88)]


@letter(0x0916, "ssbh", "[C] p.16; [O] 'Deva-ख-order.gif'; [J] 'Devanagari kh ख.gif'")
def kha():
    # ख, 4: the left part (from the headline down, round the knot, the long
    # sweep along the bottom to the bar), the loop (from its top right, left,
    # down and round to the bar), the bar, the headline.
    left = stub(38, 44)
    left.through([(36, 49.5), (30, 53), (25, 51), (25.5, 46.5), (30, 46.5), (34, 52), (40.5, 62), (50, 72.5), (63, 79),
                  (73, 80), (80, 78.5)])
    loop = S((73, 40), (63, 39.5), (54.5, 41.5), (50.5, 49), (52, 57), (59, 62.5), (70, 63), (80, 58.5))
    return [left, loop, bar(80.5), head(16, 86)]


@letter(0x0917, "sbh", "[C] p.16; [J] 'Devanagari g ग.gif'; [O] 'Deva-ग-order.gif' (starts at the knot)")
def ga():
    # ग, 3: the left stem (from the headline down, ending in a little loop to
    # the left: [J], top to bottom; [O] starts at the loop), the bar, the headline.
    s = stub(40.5, 52)
    s.through([(40, 59.5), (36, 64.5), (31, 62.5), (31, 56.5), (35.5, 54)])
    return [s, bar(68), head(24, 75)]


@letter(0x0918, "sbh", "[C] p.16; [O] 'Deva-घ-order.gif'; [J] 'Devanagari gh घ.gif'")
def gha():
    # घ, 3: the body (from the headline down, right along the little tongue
    # and back, down round the bowl to the bar), the bar, the headline.
    s = stub(34, 40)
    s.through([(37.5, 45), (46, 46), (53, 46.5), (55, 49), (51, 51.5), (41, 51.5), (35, 56), (34.5, 64), (40.5, 71),
               (51, 72.5), (61, 69), (68.5, 62)])
    return [s, bar(69.5), head(25, 76)]


@letter(0x0919, "shd", "[C] p.16 (the dot after the headline); [O] 'Deva-ङ-order.gif' (the dot before it)")
def nga():
    # ङ, 3: the body (as ड), the headline, then the dot ([C]; [O] puts the dot
    # before the headline).
    return [_da_body(), head(30, 86), dot(82.5, 54)]


@letter(0x091A, "sbh", "[C] p.16; [O] 'Deva-च-order.gif'; [J] 'Devanagari tch च.gif'")
def ca():
    # च, 3: the body (from the left along the top, then down to the left and
    # round along the bottom to the bar), the bar, the headline.
    s = line((26, 45), (57, 45), (45, 52))
    s.through([(40.5, 58), (41.5, 65), (47.5, 69.5), (57, 70.5), (65.5, 67), (71.5, 61)], join="corner")
    return [s, bar(71.5), head(23, 78)]


@letter(0x091B, "sh", "[C] p.16; [O] 'Deva-छ-order.gif'; [J] 'Devanagari tchh छ.gif'")
def cha():
    # छ, 2: the body (the little c at the top left, the big bowl round to the
    # right and up, over the top and down inside), the headline.
    s = S((47.5, 40.5), (37.5, 39.5), (29, 43.5), (27.5, 51), (33.5, 56.5), (43, 57.5), (48.5, 58))
    s.through([(40, 61.5), (32.5, 67.5), (34, 76.5), (44, 82), (58, 82.5), (72.5, 77), (81, 65.5), (82, 52.5),
               (77, 43), (69.5, 38.5), (63.5, 42), (61.5, 49), (64, 56.5), (70, 61.5)], join="corner")
    return [s, head(24, 88)]


@letter(0x091C, "ssbh", "[C] p.16 (the bowl from its right arm); [O] 'Deva-ज-order.gif', [J] 'Devanagari dʒ ज.gif' (from its top left)")
def ja():
    # ज, 4: the bowl (from its right arm under the tongue: down, round to the
    # left and up to its point, as [C] draws it; [O] and [J] start at the top
    # left), the tongue out to the bar, the bar, the headline.
    bowl = S((55, 49), (59.5, 55), (59.5, 64), (53.5, 72), (44, 73), (34.5, 67), (27, 56.5), (23, 46))
    return [bowl, line((45, 46), (76.5, 46)), bar(76.5), head(20, 83)]


@letter(0x091D, "ssbh", "[C] p.16; [S] 'Devanagari झ stroke order.svg' (4); [O] 'Deva-झ-order.gif'; [J] 'Devanagari dʒʰ झ.gif'")
def jha():
    # झ, 4: the body (from the headline: down, left, round, the curve on the
    # right, the knot, the tail), the tongue out to the bar, the bar, the headline.
    s = stub(52, 39)
    s.through([(48.5, 43), (38, 43.5), (28.5, 45), (24.5, 50.5), (27.5, 56.5), (36, 58.5), (47, 57.5), (56.5, 61.5),
               (58, 70.5), (49.5, 77), (36, 78.5), (27, 77.5), (25.5, 73.5), (31, 73), (37, 80), (46, 95)])
    return [s, line((49, 56.5), (77.5, 55.5)), bar(77.5), head(18, 83)]


@letter(0x091E, "ssbh", "[C] p.16; [O] 'Deva-ञ-order.gif'; [J] 'Devanagari ɲ ञ.gif'")
def nya():
    # ञ, 4: the curve (from its top left, over, down and round to the left),
    # the tongue out to the bar, the bar, the headline.
    c = S((37, 47.5), (46, 43.5), (56, 45), (62.5, 52), (61, 61), (52, 68), (40, 69.5), (29, 64), (22.5, 52))
    return [c, line((62, 57.5), (77, 57.5)), bar(77), head(19, 83)]


@letter(0x091F, "sh", "[C] p.16; [O] 'Deva-ट-order.gif'; [J] 'Devanagari t̥ ट.gif'")
def tta():
    # ट, 2: the body (from the headline down, round to the left and along the
    # bottom), the headline.
    s = stub(64, 40)
    s.through([(60.5, 43.5), (52, 45), (44.5, 48.5), (39.5, 55), (38, 64), (40.5, 72.5), (47.5, 78.5), (57.5, 80),
               (66, 78), (71, 74.5)])
    return [s, head(31, 79)]


@letter(0x0920, "sh", "[C] p.16; [O] 'Deva-ठ-order.gif'; [J] 'Devanagari ʈʰ ठ.gif'")
def ttha():
    # ठ, 2: the body (from the headline down into the ring, round to the left
    # and back), the headline.
    s = stub(58, 43)
    s.through([(51, 45.5), (41, 49), (35, 56.5), (34, 66.5), (38.5, 75), (48, 80.5), (60, 80.5), (69.5, 75.5),
               (73.5, 66), (71.5, 55.5), (65, 48.5), (58, 45.5)])
    return [s, head(27, 80)]


def _da_body():
    """ड's body (also ङ ड़): from the headline down, left along the top, the
    curve to the right, round the bottom and out to the left."""
    s = stub(67.5, 40)
    s.through([(64, 43.5), (55, 44.5), (46.5, 45.5), (42, 50), (44, 56), (51, 58.5), (61, 59.5), (69.5, 63.5), (72, 71),
               (67.5, 78.5), (57, 81), (45.5, 79.5), (37.5, 75.5), (32.5, 70.5)])
    return s


@letter(0x0921, "sh", "[C] p.16; [O] 'Deva-ड-order.gif'; [J] 'Devanagari ɖ ड.gif'")
def dda():
    # ड, 2: the body, the headline.
    return [_da_body(), head(30, 78)]


def _ddha_body():
    s = stub(65, 40)
    s.through([(60, 43.5), (50, 46), (41, 51), (35.5, 59.5), (35, 69), (40, 77), (50.5, 81), (62, 80.5), (71, 75.5),
               (74.5, 67.5), (71.5, 60.5), (64, 58), (57, 61), (55, 67.5), (58.5, 74), (64, 77)])
    return s


@letter(0x0922, "sh", "[C] p.16; [O] 'Deva-ढ-order.gif'; [J] 'Devanagari ɖʱ ढ.gif'")
def ddha():
    # ढ, 2: the body (from the headline down, round to the left, the bowl, the
    # curl inside it at the right), the headline.
    return [_ddha_body(), head(28, 80)]


@letter(0x0923, "sbh", "[C] p.17; [O] 'Deva-ण-order.gif'; [J] 'Devanagari ɳ ण.gif'")
def nna():
    # ण, 3: the U (from the headline down the left side, round the bottom and
    # up the right side to the headline, as [O] and [J] draw it), the bar, the
    # headline.
    u = stub(26.5, 52)
    u.through([(29, 62), (38.5, 68), (48, 63), (50.5, 53)])
    u.through([(50.5, HL)], join="smooth")
    return [u, bar(76), head(19, 82)]


@letter(0x0924, "sbh", "[C] p.17; [O] 'Deva-त-order.gif'; [J] 'Devanagari t त.gif'")
def ta():
    # त, 3: the body (from the bar, left along the top, then curving down to
    # the left), the bar, the headline.
    s = line((67.5, 51.5), (52, 51.5))
    s.through([(43, 53.5), (37.5, 60.5), (38, 70), (45.5, 81.5)])
    return [s, bar(68.5), head(24, 75)]


@letter(0x0925, "sbh", "[C] p.17; [O] 'Deva-थ-order.gif'; [J] 'Devanagari tʰ थ.gif'")
def tha():
    # थ, 3: the body (from inside the little loop: up, over, down, then down to
    # the left, and round the bowl to the bar), the bar, the headline over the
    # bar's part only ([C]).
    s = S((38.5, 44), (32.5, 40), (31, 33), (36, 28.5), (45.5, 28), (52, 33.5), (51.5, 43), (44.5, 49.5), (34.5, 52.5),
          (27.5, 53))
    s.through([(30.5, 61.5), (40, 69), (52, 71), (61.5, 67), (68, 60)], join="corner")
    return [s, bar(69), head(57, 77)]


def _da2_body():
    """द's body: from the headline down, left, round, the knot, the tail."""
    s = stub(66, 40)
    s.through([(62.5, 43.5), (53, 44.5), (44, 46.5), (38.5, 52.5), (37.5, 61), (41.5, 68.5), (50, 72), (59.5, 71),
               (67, 66.5), (69, 61.5), (65.5, 58.5), (61, 61), (61, 67.5), (64, 75), (69.5, 87)])
    return s


@letter(0x0926, "sh", "[C] p.17; [O] 'Deva-द-order.gif'; [J] 'Devanagari d द.gif'")
def da():
    # द, 2: the body (from the headline down, left, round, the knot, the tail
    # down to the right), the headline.
    return [_da2_body(), head(31, 79)]


@letter(0x0927, "sbh", "[C] p.17; [O] 'Deva-ध-order.gif', 'ध-order-example.gif'; [J] 'Devanagari dʰ ध.gif'")
def dha():
    # ध, 3: the body (from inside the little loop: up, over, down the left
    # side, along the tongue and back, round the bowl to the bar), the bar,
    # the headline over the bar's part only ([C]).
    s = S((47, 41.5), (51.5, 35), (48.5, 29), (40, 27.5), (31.5, 31.5), (29, 40), (32, 46.5), (41, 48.5), (52.5, 49),
          (53.5, 51.5), (45.5, 52.5), (36.5, 54), (32.5, 60), (34, 67.5), (42, 72), (53, 72), (62, 67.5), (67.5, 61))
    return [s, bar(68.5), head(57, 76)]


def _na_body():
    """न's body: the knot (from its top, down its right side and round: [J]),
    then out to the bar."""
    s = S((36, 53), (41.5, 57), (40, 63.5), (34.5, 65), (30, 60), (32, 54.5), (40, 53))
    s.through([(67, 52.5)], join="smooth")
    return s


@letter(0x0928, "sbh", "[C] p.17; [O] 'Deva-न-order.gif'; [J] 'Devanagari n न.gif'")
def na():
    # न, 3: the body (the knot: from its top, down its right side and round,
    # then out to the bar), the bar, the headline.
    return [_na_body(), bar(68), head(27, 75)]


@letter(0x092A, "sbh", "[C] p.17; [O] 'Deva-प-order.gif'; [J] 'Devanagari p प.gif'")
def pa():
    # प, 3: the body (from the headline down the left side, round the bottom
    # and up to the bar), the bar, the headline.
    s = stub(32.5, 52)
    s.through([(34.5, 61), (42, 66), (52.5, 67), (61.5, 63.5), (67.5, 57.5)])
    return [s, bar(68.5), head(26, 75)]


@letter(0x092B, "sbsh", "[C] p.17; [J] 'Devanagari pʰ फ.gif'")
def pha():
    # फ, 4: प's body, the bar, the hook on the right (from the bar up, over and
    # down), the headline.
    s = stub(23, 52)
    s.through([(25, 61), (32.5, 66), (43, 67), (51, 63.5), (56.5, 57.5)])
    hook = S((57.5, 52.5), (64.5, 47), (72.5, 44.5), (80, 47.5), (84.5, 55), (83.5, 64), (79, 71.5), (73.5, 76))
    return [s, bar(57), hook, head(16, 87)]


def _ba_loop():
    return S((61, 43.5), (51, 42.5), (41, 43.5), (35, 50), (34.5, 58.5), (39.5, 65.5), (49.5, 68), (60, 64.5), (68, 58))


@letter(0x092C, "sbsh", "[C] p.17; [J] 'Devanagari b ब.gif'")
def ba():
    # ब, 4: the loop (from its top right, left, round and back to the bar),
    # the bar, the diagonal inside (from the top left down to the right), the
    # headline.
    return [_ba_loop(), bar(68.5), line((41, 47), (62, 61.5)), head(28, 76)]


@letter(0x092D, "sbh", "[C] p.17; [J] 'Devanagari bʰ भ.gif' (its loop's direction; [C] draws a loop the other way round)")
def bha():
    # भ, 3: the body (from inside the little loop: up, over, down the stem,
    # round the knot, out to the bar; [J]), the bar, the headline over the
    # bar's part only ([C]).
    s = S((36, 43), (28.5, 38.5), (27, 32), (32, 28), (41, 28.5), (45.5, 35), (45.5, 46), (45.5, 55))
    s.through([(44.5, 62), (40, 66.5), (35, 63.5), (35.5, 57.5), (41, 55), (52, 55)])
    s.through([(72, 55)], join="smooth")
    return [s, bar(73), head(61, 80)]


@letter(0x092E, "sbh", "[C] p.17; [J] 'Devanagari m म.gif'")
def ma():
    # म, 3: the body (from the headline down, round the knot, out to the bar),
    # the bar, the headline.
    s = stub(38, 53)
    s.through([(38, 60), (35.5, 65.5), (30.5, 64), (30, 57.5), (35, 54), (44, 53.5)])
    s.through([(68.5, 53.5)], join="smooth")
    return [s, bar(70), head(25, 77)]


@letter(0x092F, "sbh", "[C] p.17; [O] 'Deva-य-order.gif'; [J] 'Devanagari j य.gif'")
def ya():
    # य, 3: the body (the little curve at the top left, then round the bowl to
    # the bar), the bar, the headline.
    s = S((28.5, 32), (37, 32), (43.5, 37.5), (43, 45.5), (36.5, 50.5), (30, 52))
    s.through([(34, 61), (42, 68), (53, 70.5), (62.5, 66.5), (68.5, 59)], join="corner")
    return [s, bar(69), head(24, 76)]


def _ra_body():
    """र's body: from the headline down, the knot, the diagonal down to the right."""
    s = stub(62, 40)
    s.through([(59.5, 46.5), (53.5, 51.5), (46, 54.5), (40.5, 53), (39.5, 48.5), (44, 48.5), (49, 56), (57, 67),
               (66.5, 82)])
    return s


@letter(0x0930, "sh", "[C] p.17; [O] 'Deva-र-order.gif'; [J] 'Devanagari r र.gif'")
def ra():
    # र, 2: the body (from the headline down, round the knot, the diagonal down
    # to the right), the headline.
    return [_ra_body(), head(36, 72)]


@letter(0x0932, "ssbh", "[C] p.17; [O] 'Deva-ल-order.gif'; [J] 'Devanagari l ल.gif' (the bar first)")
def la():
    # ल, 4: the C (from its top right end, left over the top, down and round
    # to its point), the stroke from inside it up to the bar, the bar, the
    # headline. ([J] draws the bar first; [O] and the rule [T]: the body first.)
    c = S((55, 49.5), (49, 44), (40, 43.5), (32, 48.5), (28.5, 57.5), (30.5, 67), (36.5, 75), (44.5, 82))
    return [c, S((49, 62), (59, 55.5), (73, 50)), bar(74), head(24, 81)]


@letter(0x0933, "sh", "[H] (rare; in none of the sources): one stroke round both loops, the headline [T]")
def lla():
    # ळ, 2 (rare; no source shows it): from the headline down into the right
    # loop, round it, across into the left loop, round it and back across; the
    # headline.
    s = stub(68.5, 44)
    s.through([(77, 47.5), (83, 56), (82, 67.5), (75, 76.5), (64.5, 77), (57.5, 70), (53.5, 61), (48.5, 51.5),
               (39.5, 45.5), (28.5, 47.5), (22.5, 58), (25, 71), (34.5, 78.5), (45, 76.5), (52, 67.5), (57, 56),
               (62, 47.5), (68.5, 44.5)])
    return [s, head(19, 87)]


@letter(0x0935, "sbh", "[C] p.18; [J] 'Devanagari v व.gif'")
def va():
    # व, 3: the loop (from its top right, left, round and back to the bar),
    # the bar, the headline.
    return [_ba_loop(), bar(68.5), head(28, 76)]


@letter(0x0936, "sbh", "[C] p.18; [O] 'Deva-श-order.gif'; [J] 'Devanagari ç श.gif'")
def sha():
    # श, 3: the body (from inside the loop: up, over, down, round the knot, the
    # diagonal down to the right), the bar, the headline.
    s = S((43, 44), (36, 41.5), (30.5, 36.5), (32, 31), (39.5, 29.5), (48, 31.5), (53, 38.5), (52, 47), (45, 54),
          (36, 58), (28.5, 56.5), (25.5, 60), (28.5, 64), (35.5, 63.5), (42.5, 70.5), (50.5, 81.5))
    return [s, bar(72.5), head(27, 79)]


@letter(0x0937, "sbsh", "[C] p.18; [J] 'Devanagari ʂ ष.gif'; [O] 'Deva-ष-order.gif' (the diagonal before the bar)")
def ssa():
    # ष, 4: प's body, the bar, the diagonal inside (from the top left down to
    # the right), the headline.
    s = stub(31.5, 52)
    s.through([(33.5, 61), (41, 66.5), (52, 68), (61.5, 64), (68, 58)])
    return [s, bar(69), line((37.5, 33), (62.5, 60)), head(25, 76)]


@letter(0x0938, "ssbh", "[C] p.18; [J] 'Devanagari s स.gif'")
def sa():
    # स, 4: the left part (from the headline down, round the knot, the
    # diagonal down to the right, as र), the stroke out to the bar, the bar,
    # the headline.
    s = stub(47.5, 43)
    s.through([(44.5, 49), (36.5, 52.5), (29.5, 51), (30.5, 46.5), (36, 48), (39.5, 56), (45, 66), (52, 82)])
    return [s, S((42, 55.5), (52, 59), (63, 59), (72.5, 55.5)), bar(73.5), head(24, 80)]


@letter(0x0939, "ssh", "[C] p.18 (the belly, then the tail); [O] 'Deva-ह-order.gif'; [J] 'Devanagari 42 h, ɦ ह.gif' (another form)")
def ha():
    # ह, 3: the body (from the headline down, left along the top, down the
    # left side and round the belly to the right), the tail (from the left
    # side down and round to the right), the headline.
    s = stub(67, 41)
    s.through([(62, 43.5), (49, 43.5), (40.5, 45.5), (37.5, 51.5), (42, 57), (54, 57.5), (64.5, 61), (69.5, 69),
               (66, 77)])
    tail = S((39, 57.5), (37, 65.5), (39.5, 76), (48, 85), (62, 92))
    return [s, tail, head(28, 78)]


# --- vowel signs and marks (drawn in place, final coordinates) ---------------------------

_C = CONSONANT


@letter(0x093E, "b", "[T] after [C]: a bar, top to bottom", kind="sign")
def sign_aa():
    # ा, 1: the bar, right of the consonant.
    return [bar(90)]


@letter(0x093F, "bs", "[T] after [C]: the bar, then the hook from its top [H]", kind="sign")
def sign_i():
    # ि, 2: the bar left of the consonant, then the hook from its top, up and
    # over to the right, coming down onto the headline over the consonant.
    return [bar(21), S((21, HL), (21, 17), (25, 9), (34, 6), (46, 7), (58, 12), (66, 19), (70, 26.5), t0=UP)]


@letter(0x0940, "bs", "[T] after [C]: the bar, then the hook from its top [H]", kind="sign")
def sign_ii():
    # ी, 2: the bar right of the consonant, then the hook from its top, up and
    # over to the left, coming down onto the headline over the consonant.
    return [bar(90), S((90, HL), (90, 17), (86, 9), (78, 6.5), (68, 9), (61, 16), (57, 24.5), t0=UP)]


@letter(0x0941, "s", "[T] after [C]: from where it joins the consonant, as उ's big lobe [H]", kind="sign")
def sign_u():
    # ु, 1: from the foot of the consonant's bar: right, down, round along the
    # bottom to the left and up.
    return [S((73, 86), (79.5, 88.5), (81.5, 94), (77, 99.5), (67.5, 101.5), (58.5, 99), (52.5, 94), (50, 88.5))]


@letter(0x0942, "s", "[T] after [C]: from where it joins the consonant [H]", kind="sign")
def sign_uu():
    # ू, 1: from the foot of the consonant's bar: a little loop to the left,
    # then over to the right and down.
    return [S((73, 86), (66.5, 88), (62.5, 93), (65, 98), (70.5, 96.5), (74, 90.5), (79.5, 88), (85.5, 92), (88, 98.5),
              (86.5, 103.5))]


@letter(0x0943, "s", "[T] after [C]: from where it joins the consonant [H]", kind="sign")
def sign_vocalic_r():
    # ृ, 1: from under the consonant: left, round and out to the right.
    return [S((72, 85), (66.5, 88), (63.5, 93), (67.5, 98), (78, 99.5))]


@letter(0x0945, "s", "[T]: the candra, left to right [H]", kind="sign")
def sign_candra_e():
    # ॅ, 1: the candra over the consonant.
    return [candra(60, 8)]


@letter(0x0947, "s", "[C] [S] (the mark of ऐ ओ: from the headline up)", kind="sign")
def sign_e():
    # े, 1: from the headline over the consonant, up to the left.
    return [_e_mark(69)]


@letter(0x0948, "ss", "[C] [S] (the marks of औ: the lower, then the upper)", kind="sign")
def sign_ai():
    # ै, 2: the lower mark, then the upper one (े's), as औ ([C], [S]).
    return [_e_mark(69, True), _e_mark(69)]


@letter(0x0949, "bs", "[T]: ा's bar, then the candra [H]", kind="sign")
def sign_candra_o():
    # ॉ, 2: ा's bar, then the candra above it.
    return [bar(90), candra(82, 7)]


@letter(0x094B, "bs", "[C] [S] (ओ: the bar, then the mark from its top)", kind="sign")
def sign_o():
    # ो, 2: the bar, then the mark from its top up to the left.
    return [bar(90), _e_mark(90)]


@letter(0x094C, "bss", "[C] [S] (औ: the bar, the lower mark, the upper mark)", kind="sign")
def sign_au():
    # ौ, 3: the bar, the lower mark, the upper mark (from the bar's top).
    return [bar(90), _e_mark(90, True), _e_mark(90)]


@letter(0x094D, "s", "[T]: a short stroke under the consonant, down to the right [H]", kind="sign")
def virama():
    # ्, 1: from under the foot of the consonant's bar, down to the right.
    return [S((63, 89), (70.5, 92), (76, 97), (79, 102.5))]


@letter(0x0901, "sd", "[T]: the candra, then the dot [H]", kind="sign")
def candrabindu():
    # ँ, 2: the candra over the consonant, then the dot in it.
    return [candra(60, 11, h=9.5), dot(60, 8.5)]


@letter(0x0902, "d", "[T]: a dot [H]", kind="sign")
def anusvara():
    # ं, 1: the dot over the consonant.
    return [dot(66, 13)]


@letter(0x0903, "dd", "[T]: the upper dot, then the lower [H]", kind="sign")
def visarga():
    # ः, 2: two dots right of the consonant, the top one first.
    return [dot(85, 43), dot(85, 68)]


@letter(0x093C, "d", "[T]: a dot [H]", kind="sign")
def nukta():
    # ़, 1: the dot under the consonant.
    return [dot(50, 94)]


# --- other signs --------------------------------------------------------------------------

@letter(0x093D, "s", "[H] one stroke from the top, as ऽ is printed")
def avagraha():
    # ऽ, 1: from the top right, left along the top, down and round, the S down
    # to the bottom left.
    return [S((66, 29.5), (52, 29), (44, 32), (40.5, 39), (44, 46.5), (53.5, 53), (61.5, 62), (62.5, 71.5), (56, 79),
              (46, 80), (37.5, 72.5))]


@letter(0x0950, "sssd", "[H] the 3 (as उ), the right lobe, the candra, the dot")
def om():
    # ॐ, 4: the 3 (from the top left, the small lobe, the big lobe), the lobe on
    # the right (from the waist, up, over and round), the candra, the dot.
    three = S((25, 32), (36, 27.5), (47.5, 30), (51, 38.5), (45.5, 46), (36, 49.5))
    three.through([(48, 53), (54.5, 62), (51.5, 72.5), (41.5, 78.5), (29.5, 75.5), (21.5, 65.5), (17, 53)], join="corner")
    lobe = S((48.5, 51.5), (58.5, 46), (69.5, 36.5), (81.5, 33.5), (91, 40), (92.5, 52), (86.5, 62.5), (75.5, 65.5),
             (67, 60))
    return [three, lobe, candra(68.5, 9, w=26, h=11), dot(68.5, 8)]


@letter(0x0964, "b", "[T] a bar, top to bottom")
def danda():
    # ।, 1: the bar.
    return [bar(54.5)]


@letter(0x0965, "bb", "[T] two bars, left first")
def double_danda():
    # ॥, 2: the left bar, the right bar.
    return [bar(46), bar(63)]


# --- digits --------------------------------------------------------------------------------
# One stroke each, from the top [H]. No headline.

@letter(0x0966, "s", "[H] from the top, anticlockwise, as 0")
def d0():
    # ०, 1: a ring from the top, anticlockwise.
    from pen import ellipse
    return [ellipse(54.5, 53.5, 15.5, 17, 90, 360)]


@letter(0x0967, "s", "[H] the loop (up its left side, over, down), across to the left, down to the right")
def d1():
    # १, 1.
    return [S((47.5, 45), (42.5, 39), (43, 31.5), (50.5, 27), (60, 28), (65.5, 35), (63.5, 44), (55, 51), (43.5, 59.5))
            .through([(52.5, 66), (61, 72.5), (63.5, 79), (61, 85)], join="corner")]


@letter(0x0968, "s", "[H] from the top left, over and round, the knot, the tail")
def d2():
    # २, 1.
    return [S((41, 31), (50, 26.5), (60.5, 28), (66, 36), (63.5, 45.5), (55, 53), (46.5, 57.5), (40.5, 57), (40.5, 52.5),
              (45, 53.5), (49.5, 62), (54.5, 72.5), (58.5, 83))]


@letter(0x0969, "s", "[H] from the top left, two lobes, the knot, the tail")
def d3():
    # ३, 1.
    s = S((38.5, 31), (48.5, 26.5), (59, 28.5), (63, 36.5), (58, 44), (45.5, 47))
    s.through([(58, 49.5), (67, 56), (68, 65.5), (60, 72), (48.5, 74), (40.5, 71), (39.5, 66.5), (44, 67), (48.5, 74.5),
               (53.5, 82), (58, 92)], join="corner")
    return [s]


@letter(0x096A, "s", "[H] from the top left down through the middle, round the loop, up to the right")
def d4():
    # ४, 1.
    return [S((38, 28.5), (42, 39), (48.5, 47), (57, 55.5), (63.5, 65), (61.5, 74.5), (53, 79.5), (44.5, 76.5),
              (40.5, 67), (44, 57.5), (51.5, 49), (59, 40), (63.5, 32.5), (66, 28))]


@letter(0x096B, "s", "[H] from the top, down round to the left and up into the knot, the tail")
def d5():
    # ५, 1.
    return [S((49.5, 26.5), (43, 32.5), (39.5, 41.5), (42, 51), (49.5, 57), (58, 56.5), (64.5, 52), (64, 46), (59.5, 45.5),
              (58, 51), (61.5, 60), (65, 72), (67.5, 84.5))]


@letter(0x096C, "s", "[H] from the top right, the upper curve, the tongue, the lower bowl, the knot, the tail")
def d6():
    # ६, 1.
    return [S((68, 28), (56, 26.5), (44.5, 27.5), (38, 33.5), (38, 42.5), (45.5, 47.5), (61, 48), (63, 50.5), (53, 51.5),
              (42, 52.5), (37, 59.5), (37.5, 69.5), (45, 77), (56, 78.5), (64, 75.5), (69, 70), (68, 65), (63.5, 64.5),
              (62.5, 71), (66, 80), (70, 90))]


@letter(0x096D, "s", "[H] from the top, down the left, round the bottom, up the right and curling in")
def d7():
    # ७, 1.
    return [S((52, 25.5), (44, 35.5), (40, 49.5), (41.5, 64), (48.5, 75.5), (59.5, 79), (70, 74.5), (75, 63), (73.5, 50.5),
              (65.5, 42), (56, 44.5), (52.5, 53), (55.5, 60), (61.5, 62.5))]


@letter(0x096E, "s", "[H] from the top right, down to the left and round to the bottom right")
def d8():
    # ८, 1.
    return [S((65.5, 26), (55.5, 37), (47, 50), (45, 62), (49.5, 72.5), (58.5, 76.5), (68.5, 72))]


@letter(0x096F, "s", "[H] the loop (up its left side, over, down), the tail down")
def d9():
    # ९, 1.
    return [S((53, 46.5), (46.5, 40.5), (46.5, 31.5), (54, 26.5), (63, 27.5), (68, 35.5), (65.5, 44.5), (57.5, 50.5),
              (60, 58), (64.5, 68), (63, 78.5), (57.5, 86))]


# --- nukta forms --------------------------------------------------------------------------
# The base letter's strokes (its headline included), then the dot [T]. The dot's
# place is in the base letter's frame.

nukta_form(0x0929, 0x0928, (34.5, 79))    # ऩ
nukta_form(0x0931, 0x0930, (44.5, 80))    # ऱ
nukta_form(0x0934, 0x0933, (52, 89.5))    # ऴ
nukta_form(0x0958, 0x0915, (30, 81))      # क़
nukta_form(0x0959, 0x0916, (18, 79))      # ख़
nukta_form(0x095A, 0x0917, (36.5, 80))    # ग़
nukta_form(0x095B, 0x091C, (46.5, 86))    # ज़
nukta_form(0x095C, 0x0921, (54.5, 95.5), "[C] p.16: ड, the headline, then the nukta")    # ड़
nukta_form(0x095D, 0x0922, (51, 96), "[C] p.16: ढ, the headline, then the nukta")        # ढ़
nukta_form(0x095E, 0x092B, (26, 80))      # फ़
nukta_form(0x095F, 0x092F, (40.5, 80))    # य़


# --- the character set ----------------------------------------------------------------------

# The app's letters (fude/lang/hi/lang.c), in code point order.
CHARACTERS = sorted(LETTERS)


def strokes_of(cp):
    """The strokes as designed (before narrowing), and how many belong to the
    base letter (all of them, except in nukta forms)."""
    l = LETTERS[cp]
    if l.base is None:
        s = l.draw()
        return s, len(s)
    s = LETTERS[l.base].draw()
    x, y = l.nukta
    return s + [dot(x, y)], len(s)


def final(cp):
    """The strokes in the box: letters narrowed by XS about the middle of their
    (base letter's) ink and centred; signs as drawn."""
    l = LETTERS[cp]
    s, n = strokes_of(cp)
    if l.kind == "sign":
        return s, n
    xs = [p[0] for st in s[:n] for p in st.flat(12)]
    c = (min(xs) + max(xs)) * 0.5
    f = lambda p: (CENTRE + (p[0] - c) * XS, p[1])
    return [st.map(f) for st in s], n


def plan(cp):
    return LETTERS[cp].plan


def stroke_count(cp):
    return len(LETTERS[cp].plan)
