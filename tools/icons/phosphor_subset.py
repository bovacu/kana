# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.
#
# The icon fonts every app ships (assets/fonts/Phosphor-Regular.ttf and Phosphor-Fill.ttf): Phosphor's full fonts
# (data/raw/phosphor-icons/, not shipped) cut down to the icons fude/drawing/base/icons.h names — each line's
# "U+XXXX". Add an icon there, then run this:
#
#   python3 -m pip install fonttools      # once (a virtual environment is fine)
#   python3 tools/icons/phosphor_subset.py
#
# It writes every app's two fonts (apps/*/assets/fonts/ that have them) and says how many icons each holds.

import glob, os, re, sys
from fontTools import subset
from fontTools.ttLib import TTFont

K = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
HEADER = os.path.join(K, 'fude/drawing/base/icons.h')
FULL = {
    'Phosphor-Regular.ttf': os.path.join(K, 'data/raw/phosphor-icons/Fonts/regular/Phosphor.ttf'),
    'Phosphor-Fill.ttf':    os.path.join(K, 'data/raw/phosphor-icons/Fonts/fill/Phosphor-Fill.ttf'),
}

def main():
    points = sorted(set(int(c, 16) for c in re.findall(r'U\+([0-9A-Fa-f]{4,5})', open(HEADER, encoding='utf-8').read())))
    if not points:
        sys.exit('no icons in ' + HEADER)
    for name, full in FULL.items():
        if not os.path.exists(full):
            sys.exit('missing ' + full)
        have = TTFont(full, recalcBBoxes=False).getBestCmap()
        lost = [hex(p) for p in points if p not in have]
        if lost:
            sys.exit('%s has no %s' % (os.path.basename(full), ', '.join(lost)))
        options = subset.Options()
        options.layout_features = []
        options.name_IDs = ['*']
        options.name_languages = ['*']
        options.name_legacy = True
        options.hinting = False
        options.notdef_outline = True
        options.recalc_bounds = False
        # (each glyph's box kept as Phosphor gives it — the whole icon's frame, its left side bearing 0: recalculated
        # tight, the boxes would no longer match the bearings and every icon would sit left of its place)
        font = TTFont(full, recalcBBoxes=False)
        cutter = subset.Subsetter(options=options)
        cutter.populate(unicodes=points)
        cutter.subset(font)
        targets = sorted(glob.glob(os.path.join(K, 'apps', '*', 'assets', 'fonts', name)))
        for target in targets:
            font.save(target)
        print('%s: %d icons into %d apps' % (name, len(points), len(targets)))

if __name__ == '__main__':
    main()
