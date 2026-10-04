#!/usr/bin/env python3
# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# Hanzi's stroke data, in KanjiVG's form so the data bake reads it as it reads
# KanjiVG: data/raw/zh/hanzivg.xml, from Make Me a Hanzi's strokes as
# hanzi-writer-data packs them (data/raw/zh/hanzi-writer-data-2.0.1.tgz, from
# fetch.py).
#
#   python3 apps/hanzi/tools/data/strokes.py
#
# Each stroke is Make Me a Hanzi's median (its centreline, a few points in a
# 1024 box with y up), turned into cubic Béziers through those points — smooth,
# but a sharp turn (a hook, 乛) stays a corner — in KanjiVG's 109-unit box with
# y down. A character's parts (KanjiVG's group elements, for Browse's Parts) are
# its components from BabelStone's IDS (data/raw/zh/IDS.TXT), unfolded, and the
# radicals' usual variants given their full forms too (讠 brings 言).
#
# LICENCE: the strokes are Make Me a Hanzi's, derived from Arphic's fonts, under
# the Arphic Public License (data/raw/zh/ARPHICPL.TXT). This file is a modified
# version of them; the app ships them in a file of their own, with that licence,
# and they are published (docs/chinese_korean_data.md).

import datetime, json, math, os, re, sys, tarfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..'))
ZH   = os.path.join(ROOT, 'data', 'raw', 'zh')
BOX  = 109.0 / 1024.0          # Make Me a Hanzi's box to KanjiVG's
TOP  = 900.0                   # its y of the box's top (y up)
CORNER = math.radians(55)      # a turn sharper than this is a corner

# The radicals' variants, and the full form each stands for (KanjiVG's
# kvg:original does the same: 亻 brings 人).
ORIGINAL = {
    '亻': '人', '氵': '水', '扌': '手', '忄': '心', '㣺': '心', '纟': '糸', '糹': '糸', '讠': '言', '訁': '言',
    '钅': '金', '釒': '金', '饣': '食', '飠': '食', '犭': '犬', '礻': '示', '衤': '衣', '艹': '艸', '罒': '网',
    '灬': '火', '刂': '刀', '⺈': '刀', '冫': '冰', '辶': '辵', '⻌': '辵', '攵': '攴', '爫': '爪', '牜': '牛',
    '耂': '老', '⺮': '竹', '𥫗': '竹', '⺌': '小', '⺗': '心', '⻊': '足', '𧾷': '足', '⻏': '邑', '⻖': '阜',
    '王': '玉', '⺊': '卜', '彳': '行', '饣': '食', '贝': '貝', '车': '車', '门': '門', '马': '馬', '鸟': '鳥',
    '鱼': '魚', '页': '頁', '见': '見', '风': '風', '韦': '韋', '齿': '齒', '龙': '龍', '龟': '龜',
}
IDC = set('⿰⿱⿲⿳⿴⿵⿶⿷⿸⿹⿺⿻⿼⿽⿾⿿㇯')

def is_component(c):
    o = ord(c)
    return (0x2E80 <= o <= 0x2FDF) or (0x3400 <= o <= 0x4DBF) or (0x4E00 <= o <= 0x9FFF) or (0xF900 <= o <= 0xFAFF) or (0x20000 <= o <= 0x3FFFF)

def read_ids(path):
    ids = {}
    with open(path, encoding='utf-8-sig') as f:
        for line in f:
            if line.startswith('#') or '\t' not in line:
                continue
            cols = line.rstrip('\r\n').split('\t')
            if len(cols) < 3:
                continue
            ch, seq = cols[1], cols[2]
            seq = re.sub(r'\$\([A-Z]*\)', '', seq).strip('^$')
            ids[ch] = [c for c in seq if c not in IDC and c != ch and is_component(c)]
    return ids

def parts_of(ch, ids, limit=32):
    # Every component, unfolded (breadth first: the big ones first), each once.
    out, queue, seen = [], list(ids.get(ch, [])), {ch}
    while queue and len(out) < limit:
        c = queue.pop(0)
        if c in seen:
            continue
        seen.add(c)
        out.append(c)
        o = ORIGINAL.get(c)
        if o and o not in seen and len(out) < limit:
            seen.add(o)
            out.append(o)
        queue.extend(ids.get(c, []))
    return out

def to_box(p):
    return (p[0] * BOX, (TOP - p[1]) * BOX)

def angle(a, b, c):
    # The turn at b, between a→b and b→c.
    v1 = (b[0] - a[0], b[1] - a[1]); v2 = (c[0] - b[0], c[1] - b[1])
    n1 = math.hypot(*v1); n2 = math.hypot(*v2)
    if n1 == 0 or n2 == 0:
        return 0.0
    d = max(-1.0, min(1.0, (v1[0] * v2[0] + v1[1] * v2[1]) / (n1 * n2)))
    return math.acos(d)

def path_d(median):
    pts = [to_box(p) for p in median]
    # Points on top of each other add nothing.
    clean = [pts[0]]
    for p in pts[1:]:
        if math.hypot(p[0] - clean[-1][0], p[1] - clean[-1][1]) > 0.05:
            clean.append(p)
    pts = clean if len(clean) >= 2 else [pts[0], (pts[0][0] + 0.1, pts[0][1])]
    n = len(pts)
    corner = [False] * n
    for i in range(1, n - 1):
        corner[i] = angle(pts[i - 1], pts[i], pts[i + 1]) > CORNER
    def tangent(i):
        # Catmull-Rom's, one-sided at the ends and at corners.
        if i == 0 or corner[i]:
            return None
        if i == n - 1:
            return None
        return ((pts[i + 1][0] - pts[i - 1][0]) / 2.0, (pts[i + 1][1] - pts[i - 1][1]) / 2.0)
    d = 'M%.2f,%.2f' % pts[0]
    for i in range(n - 1):
        a, b = pts[i], pts[i + 1]
        ta = tangent(i) or (b[0] - a[0], b[1] - a[1])
        tb = tangent(i + 1) or (b[0] - a[0], b[1] - a[1])
        c1 = (a[0] + ta[0] / 3.0, a[1] + ta[1] / 3.0)
        c2 = (b[0] - tb[0] / 3.0, b[1] - tb[1] / 3.0)
        d += 'C%.2f,%.2f %.2f,%.2f %.2f,%.2f' % (c1 + c2 + b)
    return d

def main():
    src = os.path.join(ZH, 'hanzi-writer-data-2.0.1.tgz')
    if not os.path.exists(src):
        print('missing', src, '(run fetch.py)')
        sys.exit(1)
    ids = read_ids(os.path.join(ZH, 'IDS.TXT'))
    tar = tarfile.open(src)
    out = ['<?xml version="1.0" encoding="UTF-8"?>\n',
           '<!-- Hanzi stroke data: Make Me a Hanzi\'s strokes (via hanzi-writer-data 2.0.1), derived from Arphic\'s\n'
           '     fonts and licensed under the Arphic Public License (ARPHICPL.TXT). Modified %s by\n'
           '     apps/hanzi/tools/data/strokes.py: each stroke\'s median turned into cubic Beziers in KanjiVG\'s\n'
           '     109-unit box (y down); parts from BabelStone\'s IDS added as group elements. -->\n' % datetime.date.today().isoformat(),
           '<kanjivg>\n']
    count = strokes = 0
    for m in sorted(tar.getmembers(), key=lambda m: m.name):
        name = os.path.basename(m.name)
        if not m.isfile() or not name.endswith('.json') or name == 'package.json':
            continue
        ch = name[:-5]
        if len(ch) != 1:
            continue
        data = json.load(tar.extractfile(m))
        medians = data.get('medians') or []
        if not medians:
            continue
        cp = ord(ch)
        hexid = '%05x' % cp
        out.append('<kanji id="kvg:kanji_%s">\n<g id="kvg:%s" kvg:element="%s">\n' % (hexid, hexid, ch))
        for p in parts_of(ch, ids):
            out.append('<g kvg:element="%s"/>\n' % p)
        for i, med in enumerate(medians):
            out.append('<path id="kvg:%s-s%d" d="%s"/>\n' % (hexid, i + 1, path_d(med)))
        out.append('</g>\n</kanji>\n')
        count += 1
        strokes += len(medians)
    out.append('</kanjivg>\n')
    dst = os.path.join(ZH, 'hanzivg.xml')
    with open(dst, 'w', encoding='utf-8') as f:
        f.write(''.join(out))
    print(count, 'characters,', strokes, 'strokes ->', dst)

if __name__ == '__main__':
    main()
