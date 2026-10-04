# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

"""Check data/raw/hi/devanagarivg.xml the way the app's bake will read it.

    python3 apps/hindi/tools/strokes/validate.py [devanagarivg.xml] [--corners]

Errors (exit status 1):
  - the characters: exactly the app's 90 letters (fude/lang/hi/lang.c's table,
    copied below as EXPECTED), each once, ids "kvg:kanji_%05x" (lower-case hex,
    no variant suffix), in code point order; and letters.py draws the same set
  - each has one top <g id="kvg:%05x" kvg:element="<the character>">; nested
    groups (the nukta forms' base letter and nukta) are "kvg:%05x-gN" numbered
    in document order, each kvg:element one of the app's letters
  - paths are "kvg:%05x-sN", N = 1, 2, 3 ... in document order (the writing order)
  - d uses only M/m (once, first), C/c, S/s, and parses with
    fude_bake_parse_path's rules (pen.parse_d); at most 255 segments a stroke
  - every coordinate (control points too) within 0..109
  - no kvg:type (Devanagari strokes are not CJK stroke types; the bake reads a
    missing one as none)
  - stroke counts: each character has as many strokes as its plan in
    letters.py declares (the count as taught, one letter per stroke,
    independent of the drawings); in a nukta form, the base letter's group has
    the base letter's count and the nukta's group one stroke
  - each stroke is what its plan says: a headline (h) runs left to right along
    y 28; a bar (b) runs top to bottom, upright; a dot (d) is short; nothing
    else is shorter than 4 units
  - the headline comes last in its letter: anything after it is a detached
    diacritic, a dot (the nukta) or a mark wholly above the headline (a candra)
  - the parts the bake collects for a nukta form are its base letter and ़

With --corners, also a report (not errors) of every joint inside a stroke where
the line turns by more than CORNER degrees: the drawings are meant to be
smooth, so each of these should be a real corner of the letter.
"""

import math
import os
import re
import sys
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pen import parse_d, PathError, flatten, length, joints   # noqa: E402
import letters as L                                           # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
XML = os.path.join(ROOT, "data", "raw", "hi", "devanagarivg.xml")

KVG = "{http://kanjivg.tagaini.net}"
MAX_STROKES = 64      # FUDE_BAKE_MAX_STROKES
MAX_PARTS = 32        # FUDE_KANJI_MAX_PARTS
CORNER = 35.0
HL, BL = L.HL, L.BL

# fude/lang/hi/lang.c, FUDE_HI_LETTERS: vowels, consonants, vowel signs and
# marks, nukta forms, dandas, digits.
EXPECTED = sorted(
    [0x0901, 0x0902, 0x0903]
    + [0x0905, 0x0906, 0x0907, 0x0908, 0x0909, 0x090A, 0x090B, 0x090C, 0x090D, 0x090F, 0x0910, 0x0911, 0x0913, 0x0914]
    + list(range(0x0915, 0x093A))
    + [0x093C, 0x093D, 0x093E, 0x093F, 0x0940, 0x0941, 0x0942, 0x0943, 0x0945, 0x0947, 0x0948, 0x0949, 0x094B, 0x094C,
       0x094D, 0x0950]
    + list(range(0x0958, 0x0960))
    + [0x0964, 0x0965]
    + list(range(0x0966, 0x0970)))
NUKTA = 0x093C
_CMDS = re.compile(r"[A-Za-z]")


def bake_parts(g, self_cp):
    """fude_bake_collect_parts: kvg:element of every nested group, first seen order."""
    parts = []
    for c in g.iter("g"):
        if c is g:
            continue
        for attr in ("element", "original"):
            v = c.get(KVG + attr)
            if v and len(v) == 1 and ord(v) != self_cp and ord(v) not in parts:
                parts.append(ord(v))
    return parts


def check_role(role, pts):
    """None if the stroke fits its role in the plan, else what is wrong."""
    (x0, y0), (x1, y1) = pts[0], pts[-1]
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    n = length(pts)
    if role == "h":
        if not (x1 - x0 > 10 and max(ys) - min(ys) < 0.5 and abs(y0 - HL) < 0.5):
            return "is not a headline (left to right along y %g)" % HL
    elif role == "b":
        if not (y1 - y0 > 15 and max(xs) - min(xs) < 0.5):
            return "is not a bar (upright, top to bottom)"
    elif role == "d":
        if n > 5.0:
            return "is %.1f long: too long for a dot" % n
    elif n < 4.0:
        return "is %.1f long: too short for a stroke" % n
    return None


def main(argv):
    path = XML
    corners = False
    for a in argv[1:]:
        if a == "--corners":
            corners = True
        else:
            path = a
    errors = []
    err = lambda cp, msg: errors.append("U+%04X %s: %s" % (cp, chr(cp), msg)) if len(errors) < 200 else None
    corner_report = []

    if L.CHARACTERS != EXPECTED:
        errors.append("letters.py draws %d characters, the app has %d (missing %s, extra %s)" % (
            len(L.CHARACTERS), len(EXPECTED),
            " ".join("%04X" % c for c in sorted(set(EXPECTED) - set(L.CHARACTERS))) or "none",
            " ".join("%04X" % c for c in sorted(set(L.CHARACTERS) - set(EXPECTED))) or "none"))

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
    total_strokes = 0
    lo, hi = 1e9, -1e9

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
        seen.append(cp)
        hexid = m.group(1)
        tops = list(k)
        if len(tops) != 1 or tops[0].tag != "g" or tops[0].get("id") != "kvg:" + hexid or tops[0].get(KVG + "element") != chr(cp):
            err(cp, "the top group is not <g id=\"kvg:%s\" kvg:element=\"%s\">" % (hexid, chr(cp)))
            continue
        top = tops[0]
        if cp not in L.LETTERS:
            err(cp, "not drawn in letters.py")
            continue
        lt = L.LETTERS[cp]
        plan = lt.plan

        # groups: ids numbered in document order, elements the app's letters
        gn = 0
        for g in top.iter("g"):
            if g is top:
                continue
            gn += 1
            if g.get("id") != "kvg:%s-g%d" % (hexid, gn):
                err(cp, "group id %r, expected kvg:%s-g%d" % (g.get("id"), hexid, gn))
            el = g.get(KVG + "element") or ""
            if len(el) != 1 or ord(el) not in EXPECTED:
                err(cp, "group element %r is not one of the app's letters" % el)

        # strokes: ids in document order, parsable, in the box, no type
        paths = list(top.iter("path"))
        flat = []
        for i, p in enumerate(paths, 1):
            if p.get("id") != "kvg:%s-s%d" % (hexid, i):
                err(cp, "path id %r, expected kvg:%s-s%d" % (p.get("id"), hexid, i))
            if p.get(KVG + "type") is not None:
                err(cp, "s%d has a kvg:type" % i)
            d = p.get("d")
            bad = set(_CMDS.findall(d or "")) - set("MmCcSs")
            if bad:
                err(cp, "s%d uses %s" % (i, "".join(sorted(bad))))
            try:
                start, segs = parse_d(d)
            except PathError as e:
                err(cp, "s%d: %s" % (i, e))
                flat.append(None)
                continue
            if not d.lstrip().startswith(("M", "m")):
                err(cp, "s%d does not start with M" % i)
            for q in [start] + [pt for s in segs for pt in s]:
                lo, hi = min(lo, q[0], q[1]), max(hi, q[0], q[1])
                if not (0.0 <= q[0] <= 109.0 and 0.0 <= q[1] <= 109.0):
                    err(cp, "s%d has a point outside 0..109: (%.2f, %.2f)" % (i, q[0], q[1]))
                    break
            pts = flatten(start, segs, 16)
            flat.append(pts)
            if corners:
                for j, turn in enumerate(joints(start, segs)):
                    if turn > CORNER:
                        corner_report.append((cp, i, j + 1, turn, segs[j][2]))
        n = len(paths)
        total_strokes += n
        if n > MAX_STROKES:
            err(cp, "%d strokes: more than the bake takes" % n)

        # counts, against the plan
        if n != len(plan):
            err(cp, "%d strokes, its plan (%s) has %d" % (n, plan, len(plan)))
        else:
            for i, (role, pts) in enumerate(zip(plan, flat), 1):
                if pts is None:
                    continue
                why = check_role(role, pts)
                if why:
                    err(cp, "s%d (%s) %s" % (i, role, why))
            if "h" in plan:
                hi_ = plan.rindex("h")
                if plan.count("h") != 1:
                    err(cp, "more than one headline")
                for i in range(hi_ + 1, n):
                    pts = flat[i]
                    if pts and not (plan[i] == "d" or max(p[1] for p in pts) < HL - 1):
                        err(cp, "s%d comes after the headline but is neither a dot nor a mark above it" % (i + 1))

        # nukta forms: two groups, the base letter's strokes and the dot
        if lt.base is not None:
            kids = [g for g in top if g.tag == "g"]
            els = [g.get(KVG + "element") for g in kids]
            if els != [chr(lt.base), chr(NUKTA)]:
                err(cp, "groups %r, expected the base letter and the nukta" % els)
            else:
                nb = len(list(kids[0].iter("path")))
                if nb != len(L.LETTERS[lt.base].plan):
                    err(cp, "the base letter's group has %d strokes, %s has %d" % (nb, chr(lt.base), len(L.LETTERS[lt.base].plan)))
                if len(list(kids[1].iter("path"))) != 1:
                    err(cp, "the nukta's group is not one stroke")
            parts = bake_parts(top, cp)
            if parts != [lt.base, NUKTA]:
                err(cp, "the bake's parts are %s" % " ".join("%04X" % c for c in parts))
        elif [g for g in top if g.tag == "g"]:
            err(cp, "has groups, but is not a nukta form")

    if seen != EXPECTED:
        missing = sorted(set(EXPECTED) - set(seen))
        extra = sorted(set(seen) - set(EXPECTED))
        errors.append("characters: %d, expected %d (missing %s; extra %s; order %s)" % (
            len(seen), len(EXPECTED), " ".join("%04X" % c for c in missing) or "none",
            " ".join("%04X" % c for c in extra) or "none", "ok" if seen == sorted(seen) else "wrong"))

    print("validate: %s" % path)
    print("  characters: %d; strokes: %d; coordinates %.2f..%.2f" % (len(seen), total_strokes, lo, hi))
    if corners:
        print("  corners (turns over %g degrees inside a stroke): %d" % (CORNER, len(corner_report)))
        for cp, i, j, turn, at in corner_report:
            print("    U+%04X %s s%d after segment %d: %.0f degrees at (%.1f, %.1f)" % (cp, chr(cp), i, j, turn, at[0], at[1]))
    if errors:
        print("  ERRORS (%d%s):" % (len(errors), "+" if len(errors) >= 200 else ""))
        for e in errors[:200]:
            print("    " + e)
        return 1
    print("  ok")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
