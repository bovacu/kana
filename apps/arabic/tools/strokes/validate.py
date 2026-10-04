"""Check data/raw/ar/arabicvg.xml the way Kana's bake will read it.

    python3 apps/arabic/tools/strokes/validate.py [arabicvg.xml] [--corners]

Errors (exit status 1):
  - the characters: exactly the app's 137 (the 36 letters as they stand alone
    U+0621..U+063A and U+0641..U+064A; their 81 joined forms at Arabic
    Presentation Forms-B code points; lam-alif U+FEFB U+FEFC; the 8 harakat
    U+064B..U+0652; the 10 digits U+0660..U+0669), copied below as EXPECTED,
    each once, in code point order; and, when fude/lang/ar/lang.c is there,
    the same set as its table
  - ids: <kanji id="kvg:kanji_%05x"> (lower-case hex, no variant suffix), one
    top <g id="kvg:%05x" kvg:element="<the character>">, no nested groups;
    paths "kvg:%05x-sN", N = 1, 2, 3 ... in document order (the writing order)
  - d is one absolute M, then relative c segments only, and parses with
    fude_bake_parse_path's rules (pen.parse_d); at most 255 segments a stroke
  - every coordinate, control points too, within 0..109
  - no kvg:type (Arabic strokes are not CJK stroke types)
  - stroke counts: each character has as many strokes as letters.STROKES says
    it is written with (the counts as taught, independent of the drawings)
  - no stroke shorter than MIN_LENGTH (a dot's tick is 3.2 long), no segment
    of zero length
  - the joins: a form that joins the letter before (final, medial) starts its
    first stroke (the body) within JOIN units of (98, 68) and runs left along
    the baseline from there; a form that joins the letter after (initial,
    medial) ends its body within JOIN units of (11, 68), arriving along the
    baseline; a side that does not join has no end there; every letter form's
    body meets the baseline
  - the harakat: a mark above the letter lies wholly above the app's dotted
    circle (centre (54.5, 56), radius 19) and within its width; a mark below,
    wholly below it

With --corners, also a report (not errors) of every sharp turn inside a stroke
(where the heading jumps by more than CORNER degrees between two segments):
the cusps the letters are drawn with (a tooth's tip, the turn at the top of a
hairpin, ح's head), listed so that one that should not be there shows.
"""

import math
import os
import re
import sys
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pen import parse_d, PathError, flatten, length          # noqa: E402
import letters as L                                           # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
XML = os.path.join(ROOT, "data", "raw", "ar", "arabicvg.xml")
LANG_C = os.path.join(ROOT, "fude", "lang", "ar", "lang.c")

KVG = "{http://kanjivg.tagaini.net}"
MAX_STROKES = 64      # FUDE_BAKE_MAX_STROKES
MIN_LENGTH = 2.5
CORNER = 30.0
BASELINE = 68.0
RIGHT_JOIN = (98.0, BASELINE)
LEFT_JOIN = (11.0, BASELINE)
JOIN = 2.0            # how far a join may be from its point
ALONG = 6.0           # a join runs along the baseline for at least this far
ALONG_TOL = 1.0       # ... within this of the baseline
CIRCLE = (54.5, 56.0, 19.0)

# The app's characters (the task's list; fude/lang/ar/lang.c has the same).
ISOLATED = list(range(0x0621, 0x063B)) + list(range(0x0641, 0x064B))
RIGHT_JOINING = [0x0622, 0x0623, 0x0624, 0x0625, 0x0627, 0x0629, 0x062F, 0x0630, 0x0631, 0x0632, 0x0648, 0x0649]
FINALS_RJ = [0xFE82, 0xFE84, 0xFE86, 0xFE88, 0xFE8E, 0xFE94, 0xFEAA, 0xFEAC, 0xFEAE, 0xFEB0, 0xFEEE, 0xFEF0]
DUAL_FORMS = [  # final, initial, medial
    (0xFE8A, 0xFE8B, 0xFE8C), (0xFE90, 0xFE91, 0xFE92), (0xFE96, 0xFE97, 0xFE98), (0xFE9A, 0xFE9B, 0xFE9C),
    (0xFE9E, 0xFE9F, 0xFEA0), (0xFEA2, 0xFEA3, 0xFEA4), (0xFEA6, 0xFEA7, 0xFEA8), (0xFEB2, 0xFEB3, 0xFEB4),
    (0xFEB6, 0xFEB7, 0xFEB8), (0xFEBA, 0xFEBB, 0xFEBC), (0xFEBE, 0xFEBF, 0xFEC0), (0xFEC2, 0xFEC3, 0xFEC4),
    (0xFEC6, 0xFEC7, 0xFEC8), (0xFECA, 0xFECB, 0xFECC), (0xFECE, 0xFECF, 0xFED0), (0xFED2, 0xFED3, 0xFED4),
    (0xFED6, 0xFED7, 0xFED8), (0xFEDA, 0xFEDB, 0xFEDC), (0xFEDE, 0xFEDF, 0xFEE0), (0xFEE2, 0xFEE3, 0xFEE4),
    (0xFEE6, 0xFEE7, 0xFEE8), (0xFEEA, 0xFEEB, 0xFEEC), (0xFEF2, 0xFEF3, 0xFEF4)]
LAM_ALIF = {0xFEFB: "iso", 0xFEFC: "fin"}
HARAKAT_ABOVE = [0x064B, 0x064C, 0x064E, 0x064F, 0x0651, 0x0652]
HARAKAT_BELOW = [0x064D, 0x0650]
DIGITS = list(range(0x0660, 0x066A))

FORM = {cp: "iso" for cp in ISOLATED}
FORM.update({cp: "fin" for cp in FINALS_RJ})
for _f, _i, _m in DUAL_FORMS:
    FORM.update({_f: "fin", _i: "ini", _m: "med"})
FORM.update(LAM_ALIF)
EXPECTED = sorted(list(FORM) + HARAKAT_ABOVE + HARAKAT_BELOW + DIGITS)

_PATH = re.compile(r"M\s*-?[\d.]+\s*,\s*-?[\d.]+(?:\s*c(?:\s*,?\s*-?[\d.]+){6}(?:(?:\s*,?\s*-?[\d.]+){6})*)+\s*")


def lang_c_letters(path):
    """The code points fude/lang/ar/lang.c's table holds, or None."""
    try:
        with open(path, encoding="utf-8") as f:
            src = f.read()
    except OSError:
        return None
    i = src.find("FUDE_AR_TABLE[")
    j = src.find("};", i)
    if i < 0 or j < 0:
        return None
    body = re.sub(r"//[^\n]*", "", src[i:j])
    return sorted(int(h, 16) for h in re.findall(r"\{\s*0x([0-9A-Fa-f]{4,5})\s*,", body))


def corners(start, segs):
    """(x, y, turn in degrees) where the heading jumps between two segments."""
    out = []
    prev_end_dir = None
    cur = start
    for c1, c2, p in segs:
        a = c1 if c1 != cur else (c2 if c2 != cur else p)
        b = c2 if c2 != p else (c1 if c1 != p else cur)
        d_in = math.atan2(a[1] - cur[1], a[0] - cur[0])
        d_out = math.atan2(p[1] - b[1], p[0] - b[0])
        if prev_end_dir is not None:
            t = math.degrees(d_in - prev_end_dir)
            t = (t + 180.0) % 360.0 - 180.0
            if abs(t) > CORNER:
                out.append((cur[0], cur[1], t))
        prev_end_dir = d_out
        cur = p
    return out


def along_baseline(pts):
    """How far the polyline pts runs from its first point before leaving the
    baseline band, and whether it goes leftwards meanwhile."""
    run = 0.0
    for a, b in zip(pts, pts[1:]):
        if abs(b[1] - BASELINE) > ALONG_TOL:
            break
        run += math.hypot(b[0] - a[0], b[1] - a[1])
    return run


def check_joins(cp, form, body, err):
    """The baseline and connection rules for a letter form's body stroke."""
    pts = flatten(*body, steps=24)
    first, last = pts[0], pts[-1]
    near = lambda p, q: math.hypot(p[0] - q[0], p[1] - q[1]) <= JOIN
    if form in ("fin", "med"):
        if not near(first, RIGHT_JOIN):
            err(cp, "joins on the right, but its body starts at (%.1f, %.1f), not at (98, 68)" % first)
        elif along_baseline(pts) < ALONG or pts[8][0] >= first[0]:
            err(cp, "its body does not run left along the baseline from the right join")
    elif near(first, RIGHT_JOIN):
        err(cp, "does not join on the right, but its body starts at the right join")
    if form in ("ini", "med"):
        if not near(last, LEFT_JOIN):
            err(cp, "joins on the left, but its body ends at (%.1f, %.1f), not at (11, 68)" % last)
        elif along_baseline(pts[::-1]) < ALONG or pts[-9][0] <= last[0]:
            err(cp, "its body does not arrive along the baseline at the left join")
    elif near(last, LEFT_JOIN):
        err(cp, "does not join on the left, but its body ends at the left join")
    if min(abs(p[1] - BASELINE) for p in pts) > 1.0:
        err(cp, "its body does not meet the baseline (y 68)")


def check_mark(cp, strokes, err):
    """A haraka above or below the app's dotted circle."""
    cx, cy, r = CIRCLE
    pts = [p for st in strokes for p in flatten(*st, steps=12)]
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    if min(xs) < cx - r or max(xs) > cx + r:
        err(cp, "the mark is wider than the dotted circle (x %.1f..%.1f)" % (min(xs), max(xs)))
    if cp in HARAKAT_ABOVE and max(ys) >= cy - r:
        err(cp, "a mark above the letter, but it reaches y %.1f (the circle's top is %.1f)" % (max(ys), cy - r))
    if cp in HARAKAT_BELOW and min(ys) <= cy + r:
        err(cp, "a mark below the letter, but it reaches y %.1f (the circle's bottom is %.1f)" % (min(ys), cy + r))


def main(argv):
    path = XML
    show_corners = False
    for a in argv[1:]:
        if a == "--corners":
            show_corners = True
        else:
            path = a
    errors = []
    err = lambda cp, msg: errors.append("U+%04X %s: %s" % (cp, chr(cp), msg))

    with open(path, "rb") as f:
        raw = f.read()
    if not raw.startswith(b"<?xml"):
        errors.append("no XML declaration")
    if b"&" in raw:
        errors.append("an entity or '&': RDE's parser does not decode them")
    root = ET.fromstring(raw)
    if root.tag != "kanjivg":
        errors.append("root is <%s>, not <kanjivg>" % root.tag)

    seen = []
    total = 0
    lo, hi = 1e9, -1e9
    report = []
    counts = {}

    for k in root:
        if k.tag != "kanji":
            errors.append("a <%s> under the root" % k.tag)
            continue
        kid = k.get("id", "")
        m = re.fullmatch(r"kvg:kanji_([0-9a-f]{5})", kid)
        if not m:
            errors.append("bad kanji id %r" % kid)
            continue
        cp = int(m.group(1), 16)
        hexid = m.group(1)
        seen.append(cp)
        tops = list(k)
        if len(tops) != 1 or tops[0].tag != "g" or tops[0].get("id") != "kvg:" + hexid or tops[0].get(KVG + "element") != chr(cp):
            err(cp, "the top group is not <g id=\"kvg:%s\" kvg:element=\"%s\">" % (hexid, chr(cp)))
            continue
        top = tops[0]
        if any(c.tag != "path" for c in top):
            err(cp, "something other than paths in the group (nested groups?)")

        paths = list(top.iter("path"))
        parsed = []
        for i, p in enumerate(paths, 1):
            if p.get("id") != "kvg:%s-s%d" % (hexid, i):
                err(cp, "path id %r, expected kvg:%s-s%d" % (p.get("id"), hexid, i))
            if p.get(KVG + "type") is not None:
                err(cp, "s%d has a kvg:type" % i)
            d = p.get("d") or ""
            if not _PATH.fullmatch(d):
                err(cp, "s%d is not one absolute M followed by relative c segments" % i)
            try:
                start, segs = parse_d(d)
            except PathError as e:
                err(cp, "s%d: %s" % (i, e))
                continue
            parsed.append((start, segs))
            for q in [start] + [pt for s in segs for pt in s]:
                lo, hi = min(lo, q[0], q[1]), max(hi, q[0], q[1])
                if not (0.0 <= q[0] <= 109.0 and 0.0 <= q[1] <= 109.0):
                    err(cp, "s%d has a point outside 0..109: (%.2f, %.2f)" % (i, q[0], q[1]))
                    break
            cur = start
            for c1, c2, p3 in segs:
                if p3 == cur and c1 == cur and c2 == cur:
                    err(cp, "s%d has a segment of zero length" % i)
                cur = p3
            if length(flatten(start, segs, 12)) < MIN_LENGTH:
                err(cp, "s%d is shorter than %.1f" % (i, MIN_LENGTH))
            if show_corners:
                for x, y, t in corners(start, segs):
                    report.append("U+%04X %s s%d  corner at (%.1f, %.1f), turn %+.0f" % (cp, chr(cp), i, x, y, t))
        n = len(paths)
        total += n
        counts[cp] = n
        if n > MAX_STROKES:
            err(cp, "%d strokes: more than the bake takes" % n)
        want = L.STROKES.get(chr(cp))
        if want is None:
            err(cp, "not in letters.STROKES")
        elif n != want:
            err(cp, "%d strokes, it is written with %d" % (n, want))
        if not parsed or len(parsed) != n:
            continue
        if cp in FORM:
            check_joins(cp, FORM[cp], parsed[0], err)
        elif cp in HARAKAT_ABOVE or cp in HARAKAT_BELOW:
            check_mark(cp, parsed, err)

    if seen != EXPECTED:
        missing = sorted(set(EXPECTED) - set(seen))
        extra = sorted(set(seen) - set(EXPECTED))
        errors.append("characters: %d, expected %d (missing %s, extra %s, order %s)" % (
            len(seen), len(EXPECTED), " ".join("%04X" % c for c in missing) or "none",
            " ".join("%04X" % c for c in extra) or "none", "ok" if seen == sorted(seen) else "wrong"))
    if sorted(L.STROKES, key=ord) != [chr(c) for c in EXPECTED]:
        errors.append("letters.STROKES is not the expected set")
    app = lang_c_letters(LANG_C)
    if app is not None and app != EXPECTED:
        errors.append("fude/lang/ar/lang.c's table differs from the expected set: %s" %
                      " ".join("%04X" % c for c in sorted(set(app) ^ set(EXPECTED))))

    by = lambda pred: sum(1 for c in seen if pred(c))
    print("validate: %s" % path)
    print("  characters: %d (letters %d: isolated %d, final %d, initial %d, medial %d; harakat %d; digits %d)%s" % (
        len(seen), by(lambda c: c in FORM), by(lambda c: FORM.get(c) == "iso"), by(lambda c: FORM.get(c) == "fin"),
        by(lambda c: FORM.get(c) == "ini"), by(lambda c: FORM.get(c) == "med"),
        by(lambda c: c in HARAKAT_ABOVE or c in HARAKAT_BELOW), by(lambda c: c in DIGITS),
        "; same set as lang.c's table" if app == EXPECTED else ""))
    print("  strokes: %d; coordinates %.2f..%.2f" % (total, lo, hi))
    if show_corners:
        print("  corners (%d):" % len(report))
        for r in report:
            print("    " + r)
    if errors:
        print("  ERRORS (%d):" % len(errors))
        for e in errors:
            print("    " + e)
        return 1
    print("  ok")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
