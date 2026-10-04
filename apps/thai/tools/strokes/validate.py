# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

"""Check data/raw/th/thaivg.xml the way Kana's bake will read it.

    python3 apps/thai/tools/strokes/validate.py [thaivg.xml] [--corners]

Errors (exit status 1):
  - the characters: exactly the app's 83 letters (the 44 consonants U+0E01..
    U+0E2E but ฤ ฦ; the 19 vowels; the 10 tone marks and signs; the 10 digits),
    each once, in code point order; and, when fude/lang/th/lang.c is there, the
    same set as the letters its table puts in a group
  - ids: <kanji id="kvg:kanji_%05x"> (lower-case hex, no variant suffix), one
    top <g id="kvg:%05x" kvg:element="<the letter>">, no nested groups; paths
    "kvg:%05x-sN", N = 1, 2, 3 ... in document order (the writing order)
  - d uses only M/m (once, first), then C/c and S/s, and parses with
    fude_bake_parse_path's rules (pen.parse_d); at most 255 segments a stroke
  - every coordinate, control points too, within 0..109
  - no kvg:type (Thai strokes are not CJK stroke types); the bake would accept
    one, but these strokes have none
  - stroke counts: each letter has as many strokes as letters.STROKES says it
    is written with (the counts as taught, independent of the drawings)
  - no stroke shorter than MIN_LENGTH, no segment of zero length

With --corners, also a report (not errors) of every sharp turn inside a stroke
(where the heading jumps by more than CORNER degrees between two segments):
the corners the letters are drawn with (ก's notch, the zigzags of พ ผ, a
stroke's turn at a foot...), listed so that one that should not be there shows.
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
XML = os.path.join(ROOT, "data", "raw", "th", "thaivg.xml")
LANG_C = os.path.join(ROOT, "fude", "lang", "th", "lang.c")

KVG = "{http://kanjivg.tagaini.net}"
MAX_STROKES = 64      # FUDE_BAKE_MAX_STROKES
MIN_LENGTH = 2.5
CORNER = 25.0

CONSONANTS = [c for c in range(0x0E01, 0x0E2F) if c not in (0x0E24, 0x0E26)]
VOWELS = [0x0E30, 0x0E31, 0x0E32, 0x0E33, 0x0E34, 0x0E35, 0x0E36, 0x0E37, 0x0E38, 0x0E39,
          0x0E40, 0x0E41, 0x0E42, 0x0E43, 0x0E44, 0x0E47, 0x0E24, 0x0E26, 0x0E45]
MARKS = [0x0E48, 0x0E49, 0x0E4A, 0x0E4B, 0x0E4C, 0x0E2F, 0x0E46, 0x0E3A, 0x0E4D, 0x0E4E]
DIGITS = list(range(0x0E50, 0x0E5A))
EXPECTED = sorted(CONSONANTS + VOWELS + MARKS + DIGITS)

_CMDS = re.compile(r"[A-Za-z]")


def lang_c_letters(path):
    """The code points fude/lang/th/lang.c's table puts in a group, or None."""
    try:
        with open(path, encoding="utf-8") as f:
            src = f.read()
    except OSError:
        return None
    i = src.find("FUDE_TH_LETTERS[")
    j = src.find("};", i)
    if i < 0 or j < 0:
        return None
    body = re.sub(r"//[^\n]*", "", src[i:j])
    body = body[body.index("{") + 1:]
    items = re.findall(r"\b([CVMDX])\b", body)
    return sorted(0x0E01 + k for k, g in enumerate(items) if g != "X")


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
            err(cp, "something other than paths in the group")

        paths = list(top.iter("path"))
        for i, p in enumerate(paths, 1):
            if p.get("id") != "kvg:%s-s%d" % (hexid, i):
                err(cp, "path id %r, expected kvg:%s-s%d" % (p.get("id"), hexid, i))
            if p.get(KVG + "type") is not None:
                err(cp, "s%d has a kvg:type" % i)
            d = p.get("d") or ""
            bad = set(_CMDS.findall(d)) - set("MmCcSs")
            if bad:
                err(cp, "s%d uses %s" % (i, "".join(sorted(bad))))
            if not d.lstrip().startswith(("M", "m")):
                err(cp, "s%d does not start with M" % i)
            try:
                start, segs = parse_d(d)
            except PathError as e:
                err(cp, "s%d: %s" % (i, e))
                continue
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
        if n > MAX_STROKES:
            err(cp, "%d strokes: more than the bake takes" % n)
        want = L.STROKES.get(chr(cp))
        if want is None:
            err(cp, "not in letters.STROKES")
        elif n != want:
            err(cp, "%d strokes, it is written with %d" % (n, want))

    if seen != EXPECTED:
        missing = sorted(set(EXPECTED) - set(seen))
        extra = sorted(set(seen) - set(EXPECTED))
        errors.append("characters: %d, expected %d (missing %s, extra %s, order %s)" % (
            len(seen), len(EXPECTED), "".join(chr(c) for c in missing) or "none",
            "".join(chr(c) for c in extra) or "none", "ok" if seen == sorted(seen) else "wrong"))
    if sorted(L.STROKES, key=ord) != [chr(c) for c in EXPECTED]:
        errors.append("letters.STROKES is not the expected set")
    app = lang_c_letters(LANG_C)
    if app is not None and app != EXPECTED:
        errors.append("fude/lang/th/lang.c's letters differ from the expected set: %s" %
                      "".join(chr(c) for c in sorted(set(app) ^ set(EXPECTED))))

    print("validate: %s" % path)
    print("  characters: %d (consonants %d, vowels %d, marks %d, digits %d)%s" % (
        len(seen), sum(c in CONSONANTS for c in seen), sum(c in VOWELS for c in seen),
        sum(c in MARKS for c in seen), sum(c in DIGITS for c in seen),
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
