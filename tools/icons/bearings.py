#!/usr/bin/env python3
# The icons' left bearings (FUDE_KIT_ICON_BEARINGS in fude/drawing/widgets/kit.c), measured
# from Phosphor-Regular.ttf (Kana's assets) for every icon of fude/drawing/widgets/icons.h.
# Run after adding an icon (and cutting the fonts again: COMMANDS.txt, ICONS):
#
#   python3 -m pip install fonttools      # once
#   python3 tools/icons/bearings.py
import os, re
from fontTools.ttLib import TTFont
from fontTools.pens.boundsPen import BoundsPen

K = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
s = open(os.path.join(K, 'fude/drawing/widgets/icons.h')).read()
cps = set()
for m in re.findall(r'#define\s+FUDE_ICON_\w+\s+"([^"]+)"', s):
    b = bytes(int(h, 16) for h in re.findall(r'\\x([0-9A-Fa-f]{2})', m))
    for c in b.decode('utf-8'):
        if 0xE000 <= ord(c) <= 0xF8FF:
            cps.add(ord(c))
f = TTFont(os.path.join(K, 'apps/kana/assets/fonts/Phosphor-Regular.ttf'))
cmap, gs = f.getBestCmap(), f.getGlyphSet()
rows = []
for cp in sorted(cps):
    g = cmap.get(cp)
    if g is None:
        print('not in the font:', hex(cp))
        continue
    bp = BoundsPen(gs)
    gs[g].draw(bp)
    rows.append('    { 0x%04Xu, %3d },' % (cp, round(bp.bounds[0] if bp.bounds else 0)))
p = os.path.join(K, 'fude/drawing/widgets/kit.c')
t = open(p).read()
head = 'static const struct { u32 codepoint; u16 bearing; } FUDE_KIT_ICON_BEARINGS[] = {\n'
i = t.index(head) + len(head)
j = t.index('};', i)
t = t[:i] + '\n'.join(rows) + '\n' + t[j:]
open(p, 'w').write(t)
print(len(rows), 'bearings')
