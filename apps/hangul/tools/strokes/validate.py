# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

"""Check data/raw/hangulvg.xml the way Kana's bake will read it.

    python3 apps/hangul/tools/strokes/validate.py [hangulvg.xml] [--clearance]

Errors (exit status 1):
  - the characters: exactly U+3131..U+3163 and U+AC00..U+D7A3, each once, ids
    "kvg:kanji_%05x" (lower-case hex, no variant suffix), in code point order
  - each has one top <g id="kvg:%05x" kvg:element="<the character>">; nested
    groups are "kvg:%05x-gN" numbered in document order, each kvg:element one
    compatibility jamo (U+3131..U+318E)
  - paths are "kvg:%05x-sN", N = 1, 2, 3 ... in document order (the writing order)
  - d uses only M/m (once, first), C/c, S/s, and parses with
    fude_bake_parse_path's rules (pen.parse_d); at most 255 segments a stroke
  - every coordinate (control points too) within 0..109
  - kvg:type, when present, is a CJK Strokes character (U+31C0..U+31EF), with an
    optional variant letter and "/alternative", as fude_bake_parse_type reads it
  - stroke counts: a syllable has the sum of its jamo's (jamo.STROKES, which
    holds the counts as taught, independent of the drawings); a standalone jamo
    its own; each jamo group as many as its jamo has
  - the parts the bake collects (fude_bake_collect_parts) include the syllable's
    initial, medial and (if any) final
  - each stroke goes the way its type says (a ㇐ rightwards, a ㇑ down, ...)

With --clearance, also a report (not errors) of sibling jamo groups whose
centrelines come closer than CLEAR units: at the app's stroke width (3.2) that is
less than about 1.5 units of white between them.
"""

import math
import os
import re
import sys
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from pen import parse_d, PathError, flatten                 # noqa: E402
import jamo as J                                              # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
XML = os.path.join(ROOT, "data", "raw", "hangulvg.xml")

KVG = "{http://kanjivg.tagaini.net}"
INITIALS = "ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ"
MEDIALS = "ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ"
FINALS = [None] + list("ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ")
EXPECTED = list(range(0x3131, 0x3164)) + list(range(0xAC00, 0xD7A4))
MAX_STROKES = 64      # FUDE_BAKE_MAX_STROKES
MAX_PARTS = 32        # FUDE_KANJI_MAX_PARTS
CLEAR = 4.7

_TYPE = re.compile(r"^([㇀-㇯])([a-z])?(?:/([㇀-㇯]))?$")
_CMDS = re.compile(r"[A-Za-z]")


def decompose(cp):
    i = cp - 0xAC00
    return INITIALS[i // 588], MEDIALS[(i % 588) // 28], FINALS[i % 28]


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


def direction_ok(t, pts):
    """The stroke runs the way its type says. pts: flattened centreline."""
    (x0, y0), (x1, y1) = pts[0], pts[-1]
    dx, dy = x1 - x0, y1 - y0
    base = t[0] if t else None
    if base == "㇐":            # ㇐
        return dx > 0 and abs(dy) < 0.35 * dx
    if base == "㇑":            # ㇑
        return dy > 0 and abs(dx) < 0.35 * dy
    if base == "㇒":            # ㇒
        return dy > 0 and dx < 0
    if base == "㇏":            # ㇏
        return dy > 0 and dx > 0
    if base in ("㇕", "㇗"):   # ㇕ ㇗: overall down and right
        return dy > 0 and dx > 0
    if base == "㇇":            # ㇇: right, then down to the left
        xs = [p[0] for p in pts]
        return dy > 0 and max(xs) > x0 + 1 and x1 < max(xs) - 1
    return True


def main(argv):
    path = XML
    clearance = False
    for a in argv[1:]:
        if a == "--clearance":
            clearance = True
        else:
            path = a
    errors = []
    err = lambda cp, msg: errors.append("U+%04X %s: %s" % (cp, chr(cp), msg)) if len(errors) < 200 else None

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
    types = {}
    clear_report = []

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

        # groups: ids numbered in document order, elements single jamo
        gn = 0
        for g in top.iter("g"):
            if g is top:
                continue
            gn += 1
            if g.get("id") != "kvg:%s-g%d" % (hexid, gn):
                err(cp, "group id %r, expected kvg:%s-g%d" % (g.get("id"), hexid, gn))
            el = g.get(KVG + "element") or ""
            if len(el) != 1 or not (0x3131 <= ord(el) <= 0x318E):
                err(cp, "group element %r is not one compatibility jamo" % el)

        # strokes: ids in document order, parsable, in the box, typed sanely
        paths = list(top.iter("path"))
        strokes = []
        for i, p in enumerate(paths, 1):
            if p.get("id") != "kvg:%s-s%d" % (hexid, i):
                err(cp, "path id %r, expected kvg:%s-s%d" % (p.get("id"), hexid, i))
            d = p.get("d")
            bad = set(_CMDS.findall(d or "")) - set("MmCcSs")
            if bad:
                err(cp, "s%d uses %s" % (i, "".join(sorted(bad))))
            try:
                start, segs = parse_d(d)
            except PathError as e:
                err(cp, "s%d: %s" % (i, e))
                continue
            if not d.lstrip().startswith(("M", "m")):
                err(cp, "s%d does not start with M" % i)
            for q in [start] + [pt for s in segs for pt in s]:
                lo, hi = min(lo, q[0], q[1]), max(hi, q[0], q[1])
                if not (0.0 <= q[0] <= 109.0 and 0.0 <= q[1] <= 109.0):
                    err(cp, "s%d has a point outside 0..109: (%.2f, %.2f)" % (i, q[0], q[1]))
                    break
            t = p.get(KVG + "type")
            if t is not None and not _TYPE.match(t):
                err(cp, "s%d has kvg:type %r" % (i, t))
            types[t] = types.get(t, 0) + 1
            pts = flatten(start, segs, 12)
            if not direction_ok(t, pts):
                err(cp, "s%d (%s) runs the wrong way" % (i, t))
            strokes.append(pts)
        n = len(paths)
        total_strokes += n
        if n > MAX_STROKES:
            err(cp, "%d strokes: more than the bake takes" % n)

        # counts
        if 0xAC00 <= cp <= 0xD7A3:
            ini, v, fin = decompose(cp)
            jamo = [ini, v] + ([fin] if fin else [])
            want = sum(J.stroke_count(j) for j in jamo)
            if n != want:
                err(cp, "%d strokes, its jamo have %d" % (n, want))
            kids = [g for g in top if g.tag == "g"]
            els = [g.get(KVG + "element") for g in kids]
            if els != jamo:
                err(cp, "top-level groups %s, expected %s" % ("".join(els), "".join(jamo)))
            parts = bake_parts(top, cp)
            if len(parts) > MAX_PARTS:
                err(cp, "%d parts" % len(parts))
            for j in jamo:
                if ord(j) not in parts:
                    err(cp, "the bake's parts lack %s" % j)
        else:
            want = J.stroke_count(chr(cp))
            if n != want:
                err(cp, "%d strokes, the jamo has %d" % (n, want))
        for g in top.iter("g"):
            if g is top:
                continue
            el = g.get(KVG + "element")
            if el in J.STROKES or el in J.DOUBLE or el in J.CLUSTER or el in J.COMPOUND:
                got = len(list(g.iter("path")))
                if got != J.stroke_count(el):
                    err(cp, "group %s has %d strokes, expected %d" % (el, got, J.stroke_count(el)))

        if clearance and strokes:
            clear_report.extend(_clearance(cp, top, paths, strokes))

    if seen != EXPECTED:
        missing = sorted(set(EXPECTED) - set(seen))
        extra = sorted(set(seen) - set(EXPECTED))
        errors.append("characters: %d, expected %d (missing %d, extra %d, order %s)" % (
            len(seen), len(EXPECTED), len(missing), len(extra), "ok" if seen == sorted(seen) else "wrong"))

    print("validate: %s" % path)
    print("  characters: %d (jamo %d, syllables %d)" % (len(seen), sum(1 for c in seen if c < 0xAC00), sum(1 for c in seen if c >= 0xAC00)))
    print("  strokes: %d; coordinates %.2f..%.2f" % (total_strokes, lo, hi))
    print("  types: " + ", ".join("%s %d" % (t or "(none)", c) for t, c in sorted(types.items(), key=lambda x: -x[1])))
    if clearance:
        _print_clearance(clear_report)
    if errors:
        print("  ERRORS (%d%s):" % (len(errors), "+" if len(errors) >= 200 else ""))
        for e in errors[:200]:
            print("    " + e)
        return 1
    print("  ok")
    return 0


# --- clearance ------------------------------------------------------------------------------

def _resample(pts, step=0.7):
    out = [pts[0]]
    acc = 0.0
    for a, b in zip(pts, pts[1:]):
        d = math.hypot(b[0] - a[0], b[1] - a[1])
        acc += d
        if acc >= step:
            out.append(b)
            acc = 0.0
    out.append(pts[-1])
    return out


def _min_dist(pa, pb):
    cell = 6.0
    grid = {}
    for q in pb:
        grid.setdefault((int(q[0] // cell), int(q[1] // cell)), []).append(q)
    best = 1e9
    for p in pa:
        cx, cy = int(p[0] // cell), int(p[1] // cell)
        for gx in (cx - 1, cx, cx + 1):
            for gy in (cy - 1, cy, cy + 1):
                for q in grid.get((gx, gy), ()):
                    d = (p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2
                    if d < best:
                        best = d
    return math.sqrt(best)


def _clearance(cp, top, paths, strokes):
    """Closest approach between sibling groups (initial/medial/final; the halves of
    a pair; the parts of a compound vowel)."""
    index = {id(p): i for i, p in enumerate(paths)}
    out = []

    def pts_of(g):
        r = []
        for p in g.iter("path"):
            i = index.get(id(p))
            if i is not None and i < len(strokes):
                r.extend(_resample(strokes[i]))
        return r

    def visit(g):
        kids = [c for c in g if c.tag == "g"]
        for a in range(len(kids)):
            for b in range(a + 1, len(kids)):
                d = _min_dist(pts_of(kids[a]), pts_of(kids[b]))
                if d < CLEAR:
                    out.append((d, cp, kids[a].get(KVG + "element"), kids[b].get(KVG + "element")))
        for c in kids:
            visit(c)

    visit(top)
    return out


def _print_clearance(rep):
    print("  clearance: %d sibling pairs closer than %.1f" % (len(rep), CLEAR))
    by = {}
    for d, cp, a, b in rep:
        by.setdefault((a, b), []).append((d, cp))
    rows = sorted(by.items(), key=lambda kv: (min(x[0] for x in kv[1])))
    for (a, b), lst in rows[:60]:
        lst.sort()
        print("    %s|%s  %4d  worst %.2f %s  e.g. %s" % (a, b, len(lst), lst[0][0], chr(lst[0][1]),
                                                       "".join(chr(c) for _, c in lst[:12])))


if __name__ == "__main__":
    sys.exit(main(sys.argv))
