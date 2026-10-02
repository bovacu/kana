#!/usr/bin/env python3
# The icons' left bearings (KANA_KIT_ICON_BEARINGS in src/widgets/kit.c), measured
# from assets/fonts/Phosphor-Regular.ttf for every icon of src/widgets/icons.h.
# Run after adding an icon (and cutting the fonts again: COMMANDS.txt, ICONS):
#
#   python3 -m pip install fonttools      # once
#   python3 tools/icons/bearings.py
import os, re
from fontTools.ttLib import TTFont
from fontTools.pens.boundsPen import BoundsPen

K = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
s = open(os.path.join(K, 'src/widgets/icons.h')).read()
cps = set()
for m in re.findall(r'#define\s+KANA_ICON_\w+\s+"([^"]+)"', s):
    b = bytes(int(h, 16) for h in re.findall(r'\\x([0-9A-Fa-f]{2})', m))
    for c in b.decode('utf-8'):
        if 0xE000 <= ord(c) <= 0xF8FF:
            cps.add(ord(c))
f = TTFont(os.path.join(K, 'assets/fonts/Phosphor-Regular.ttf'))
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
p = os.path.join(K, 'src/widgets/kit.c')
t = open(p).read()
head = 'static const struct { u32 codepoint; u16 bearing; } KANA_KIT_ICON_BEARINGS[] = {\n'
i = t.index(head) + len(head)
j = t.index('};', i)
t = t[:i] + '\n'.join(rows) + '\n' + t[j:]
open(p, 'w').write(t)
print(len(rows), 'bearings')
