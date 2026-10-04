#!/usr/bin/env python3
# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# A study app's AppIcon.svg with its script written in its own strokes: another
# app's icon as the template (the moon and the open book), its background colour
# and the letters on the moon replaced by TEXT's, from a strokes file (KanjiVG's
# format), side by side; with '|' in TEXT, each cell's characters drawn over each
# other (a consonant and its signs: हिं|दी). Then tools/icons/app_icon.py.
#
#   python3 tools/icons/script_icon.py data/raw/th/thaivg.xml apps/hangul/platform/ios/AppIcon.svg \
#       apps/thai/platform/ios/AppIcon.svg '#8C2635' '#7A1E2C' 'ไทย' 10
#   python3 tools/icons/script_icon.py data/raw/hi/devanagarivg.xml apps/hangul/platform/ios/AppIcon.svg \
#       apps/hindi/platform/ios/AppIcon.svg '#C2571A' '#A64912' 'हिं|दी' -6
#   python3 tools/icons/script_icon.py data/raw/ar/arabicvg.xml apps/hangul/platform/ios/AppIcon.svg \
#       apps/arabic/platform/ios/AppIcon.svg '#1E6B45' '#18573A' 'ﻲ|~|ﺑ|ﺮ|~|ﻋ' 6 440
#
# Arguments: STROKES TEMPLATE OUT BACKGROUND INK TEXT [GAP between cells, in stroke
# units] [WIDEST the text may be, of the 1024: 350].
import re, sys
xml, template, out, bg, ink, text = sys.argv[1:7]
gap = float(sys.argv[7]) if len(sys.argv) > 7 else 8.0
most = float(sys.argv[8]) if len(sys.argv) > 8 else 350.0   # the text's widest, of the 1024
src = open(xml, encoding='utf-8').read()
def paths(c):
    m = re.search(r'<kanji id="kvg:kanji_%05x">(.*?)</kanji>' % ord(c), src, re.S)
    assert m, c
    return re.findall(r' d="([^"]+)"', m.group(1))
def points(d):
    nums = [float(x) for x in re.findall(r'-?\d+(?:\.\d+)?', d)]
    x, y = nums[0], nums[1]
    pts = [(x, y)]
    i = 2
    # Only M then relative c (the stroke files' form): 6 numbers a segment.
    while i + 6 <= len(nums):
        cx1, cy1, cx2, cy2, ex, ey = nums[i:i + 6]
        pts += [(x + cx1, y + cy1), (x + cx2, y + cy2), (x + ex, y + ey)]
        x, y = x + ex, y + ey
        i += 6
    return pts
# Cells split by '|': the characters of a cell drawn over each other (a
# consonant and its signs); without '|', a cell each. A '~' before a cell joins
# it to the one on its left at their joins (Arabic's joined forms, written in
# TEXT from left to right as they show: the strokes join at x 98 on the left
# letter and x 11 on the right one).
JOIN = 98.0 - 11.0
cells = text.split('|') if '|' in text else list(text)
glyphs = []
joined = False
for cell in cells:
    if cell == '~':
        joined = True
        continue
    ps = [d for c in cell for d in paths(c)]
    pts = [p for d in ps for p in points(d)]
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    glyphs.append((ps, min(xs), max(xs), min(ys), max(ys), joined))
    joined = False
# Each cell's box's left edge, in stroke units: after the one before's ink and
# the gap, or at its join.
lefts = []
for k, (ps, x1, x2, y1, y2, j) in enumerate(glyphs):
    if k == 0:
        lefts.append(-x1)
    elif j:
        lefts.append(lefts[-1] + JOIN)
    else:
        lefts.append(lefts[-1] + glyphs[k - 1][2] + gap - x1)
ink_left = min(l + g[1] for l, g in zip(lefts, glyphs))
ink_right = max(l + g[2] for l, g in zip(lefts, glyphs))
width = ink_right - ink_left
top, bottom = min(g[3] for g in glyphs), max(g[4] for g in glyphs)
scale = min(most / width, 260.0 / (bottom - top))
stroke_px = 24.0
x0 = 512.0 - width * scale / 2.0 - ink_left * scale
y0 = 330.0 - (top + bottom) / 2.0 * scale
groups = ''
for (ps, x1, x2, y1, y2, j), left in zip(glyphs, lefts):
    groups += '<g transform="translate(%.1f %.1f) scale(%.4f)" fill="none" stroke="%s" stroke-width="%.2f" stroke-linecap="round" stroke-linejoin="round">' % (x0 + left * scale, y0, scale, ink, stroke_px / scale)
    groups += ''.join('<path d="%s"/>' % d for d in ps) + '</g>'
t = open(template, encoding='utf-8').read()
t = re.sub(r'<rect width="1024" height="1024" fill="#[0-9A-Fa-f]+"/>', '<rect width="1024" height="1024" fill="%s"/>' % bg, t, count=1)
t = re.sub(r'(<circle[^>]*/>)(<g transform.*</g>)(<path d="M86 911)', lambda m: m.group(1) + groups + m.group(3), t, count=1, flags=re.S)
open(out, 'w', encoding='utf-8').write(t)
print(out, 'scale %.2f' % scale)
