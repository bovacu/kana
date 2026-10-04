#!/usr/bin/env python3
# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# Hanzi's data, prepared for the bake (study/chars/bake.h's PREPARED tables):
# data/raw/zh/prepared/{chars,words,lists,sentences}.tsv, from the sources
# fetch.py downloads. Run from the project root after fetch.py and strokes.py:
#
#   python3 -m pip install msgpack     # once (wordfreq's file)
#   python3 apps/hanzi/tools/data/prepare.py
#
# Also writes fude/lang/zh/traditional.h, the characters only Traditional Chinese
# writes (lang.c's second group).
#
# What each table holds, and from where:
#   chars      every character with strokes (hanzivg.xml): its HSK 2025 level (the
#              characters to read: levels 1-6, and 7-9 as 7; simplified forms
#              only), its pinyin (CC-CEDICT's entries for it, Unihan's reading
#              first), its English meaning (CC-CEDICT's, else Unihan's), its
#              Spanish, Portuguese and French ones where KANJIDIC2 has the
#              character (or its traditional form), and how common it is (from
#              the words it is in, by wordfreq).
#   words      CC-CEDICT's entries of 1-5 characters whose characters all have
#              strokes: written simplified, and traditional too where that
#              differs; their pinyin with tone marks, their first English senses,
#              how common (wordfreq), common when on an HSK list or frequent, and
#              a Tatoeba sentence with them in.
#   lists      each character's words: the examples first (common, short, the
#              easiest HSK level and the commonest first), then more to add.
#   sentences  the Tatoeba sentences kept (Mandarin, then English, Spanish,
#              French, Portuguese), each a word's.

import bz2, gzip, math, os, re, sys, unicodedata, xml.etree.ElementTree as ET
from collections import defaultdict

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..'))
RAW  = os.path.join(ROOT, 'data', 'raw')
ZH   = os.path.join(RAW, 'zh')
OUT  = os.path.join(ZH, 'prepared')
LANGS = ['es', 'pt', 'fr']                       # the bake's other languages, in order

WORD_CHARS    = 5        # longer words are not kept (bake.h)
EXAMPLE_CHARS = 4
EXAMPLES      = 6
WORDS_ALL     = 20
MEANING_CHARS = 48       # glosses after the first while the meaning stays this short
MEANING_LONG  = 64       # a first gloss longer than this is cut at its first clause when it can be
MEANING_MOST  = 96       # ...and one longer than this, where a phrase ends
SENT_MIN, SENT_MAX, SENT_IDEAL = 5, 30, 12

# --- pinyin --------------------------------------------------------------------------

TONES = {'a': 'āáǎàa', 'e': 'ēéěèe', 'i': 'īíǐìi', 'o': 'ōóǒòo', 'u': 'ūúǔùu', 'ü': 'ǖǘǚǜü'}

def tone_mark(syllable):
    # "hao3" → "hǎo", "lu:4" → "lǜ"; anything without a tone digit as it is.
    m = re.match(r'^([a-zü:]+)([1-5])$', syllable, re.I)
    if not m:
        return syllable.replace('u:', 'ü').replace('U:', 'Ü')
    s, tone = m.group(1).replace('u:', 'ü').replace('U:', 'Ü'), int(m.group(2))
    if tone == 5:
        return s
    low = s.lower()
    if 'a' in low:   at = low.index('a')
    elif 'e' in low: at = low.index('e')
    elif 'ou' in low: at = low.index('o')
    else:
        at = max(i for i, c in enumerate(low) if c in 'aeiouü') if any(c in 'aeiouü' for c in low) else -1
    if at < 0:
        return s
    v = low[at]
    marked = TONES[v][tone - 1]
    if s[at].isupper():
        marked = marked.upper()
    return s[:at] + marked + s[at + 1:]

def pinyin(numbered):
    # CC-CEDICT's "ni3 hao3" → "nǐhǎo" (a word's syllables written together).
    out = []
    for syl in numbered.split():
        if syl in (',', '·', '-'):
            continue
        out.append(tone_mark(syl))
    return ''.join(out)

# --- sources -------------------------------------------------------------------------

def read_strokes_chars():
    chars = set()
    for m in re.finditer(r'<kanji id="kvg:kanji_([0-9a-f]+)">', open(os.path.join(ZH, 'hanzivg.xml'), encoding='utf-8').read()):
        chars.add(chr(int(m.group(1), 16)))
    return chars

def read_unihan():
    info = defaultdict(dict)
    for name, fields in [('Unihan_Readings.txt', ('kMandarin', 'kDefinition')), ('Unihan_IRGSources.txt', ('kRSUnicode', 'kTotalStrokes'))]:
        with open(os.path.join(ZH, 'unihan', name), encoding='utf-8') as f:
            for line in f:
                if line.startswith('#') or not line.strip():
                    continue
                cp, field, value = line.rstrip('\n').split('\t', 2)
                if field in fields:
                    info[chr(int(cp[2:], 16))][field] = value
    return info

def read_opencc(name):
    m = {}
    with open(os.path.join(ZH, name), encoding='utf-8') as f:
        for line in f:
            if '\t' in line:
                k, v = line.rstrip('\n').split('\t', 1)
                m[k] = v.split(' ')
    return m

def read_cedict():
    entries = []
    with gzip.open(os.path.join(ZH, 'cedict_1_0_ts_utf-8_mdbg.txt.gz'), 'rt', encoding='utf-8') as f:
        for line in f:
            if line.startswith('#'):
                continue
            m = re.match(r'^(\S+) (\S+) \[([^\]]*)\] /(.*)/\s*$', line)
            if not m:
                continue
            trad, simp, py, defs = m.groups()
            senses = [d for d in defs.split('/') if d and not d.startswith('CL:') and not d.startswith('see ') and not d.startswith('variant of')
                      and not d.startswith('old variant of') and not d.startswith('surname ') and not d.startswith('erhua variant')]
            senses = [x for x in (clean_sense(d) for d in senses) if x]
            entries.append((trad, simp, py, senses, defs))
    return entries

def read_wordfreq():
    import msgpack
    data = msgpack.unpackb(gzip.open(os.path.join(ZH, 'large_zh.msgpack.gz')).read(), raw=False)
    zipf = {}
    for i, bucket in enumerate(data[1:], start=1):
        for w in bucket:
            zipf.setdefault(w, 9.0 - i / 100.0)
    return zipf

def read_hsk():
    chars, words = {}, {}
    for level, name in [(1, '1'), (2, '2'), (3, '3'), (4, '4'), (5, '5'), (6, '6'), (7, '7-9')]:
        for line in open(os.path.join(ZH, 'hsk2025', 'hanzi_%s.txt' % name), encoding='utf-8'):
            c = line.strip()
            if len(c) == 1:
                chars.setdefault(c, level)
        for line in open(os.path.join(ZH, 'hsk2025', 'words_%s.txt' % name), encoding='utf-8'):
            w = re.sub(r'[0-9]+$', '', line.strip().split('|')[0])   # "本1" → 本 (the syllabus numbers words spelt alike)
            w = re.sub(r'[（(].*?[）)]', '', w).strip()
            if w:
                words.setdefault(w, level)
    return chars, words

def read_kanjidic():
    # Meanings in Spanish, Portuguese and French by character (KANJIDIC2's m_lang).
    out = {}
    path = os.path.join(RAW, 'kanjidic2.xml')
    if not os.path.exists(path):
        return out
    for _, el in ET.iterparse(path):
        if el.tag != 'character':
            continue
        lit = el.findtext('literal')
        ms = defaultdict(list)
        for m in el.iter('meaning'):
            lang = m.get('m_lang')
            if lang in LANGS and m.text:
                ms[lang].append(m.text)
        if lit and ms:
            out[lit] = {l: ', '.join(v) for l, v in ms.items()}
        el.clear()
    return out

def read_tatoeba():
    T = os.path.join(RAW, 'tatoeba')
    def sentences(path, wanted=None):
        out = {}
        opener = bz2.open if path.endswith('.bz2') else open
        with opener(path, 'rt', encoding='utf-8') as f:
            for line in f:
                p = line.rstrip('\n').split('\t')
                if len(p) >= 3:
                    sid = int(p[0])
                    if wanted is None or sid in wanted:
                        out[sid] = p[2]
        return out
    def links(path):
        out = defaultdict(list)
        with bz2.open(path, 'rt', encoding='utf-8') as f:
            for line in f:
                p = line.rstrip('\n').split('\t')
                if len(p) >= 2:
                    out[int(p[0])].append(int(p[1]))
        return out
    cmn = sentences(os.path.join(T, 'cmn_sentences.tsv.bz2'))
    trans = {}
    for code, name in [('en', 'eng'), ('es', 'spa'), ('fr', 'fra'), ('pt', 'por')]:
        ln = links(os.path.join(T, 'cmn-%s_links.tsv.bz2' % name))
        wanted = {t for s in cmn for t in ln.get(s, [])}
        texts = sentences(os.path.join(T, '%s_sentences.tsv' % name), wanted)
        trans[code] = {s: next((texts[t] for t in ln.get(s, []) if t in texts), '') for s in cmn}
    return cmn, trans

# --- the tables ----------------------------------------------------------------------

def clean(s):
    return s.replace('\t', ' ').replace('\n', ' ').strip()

def clean_sense(s):
    # CC-CEDICT's cross-references' pinyin off (十天干[shi2 tian1 gan1] → 十天干),
    # their traditional form too (塊|块 → 块), its classifiers (CL:個|个[ge4]) and
    # its "(bound form)" note and other pronunciations' notes; a sense only
    # pointing elsewhere is none.
    s = re.sub(r'\(?\bCL:[^()]*\)?', '', s)
    s = re.sub(r'\s*\([^()]*\bpr\.\s*\[[^\]]*\][^()]*\)', '', s)   # (Taiwan pr. [zhao1]): pinyin it would show without
    s = re.sub(r'[,;]?\s*\b(?:[A-Za-z-]+ )?pr\.\s*\[[^\]]*\]', '', s)
    s = re.sub(r'[^\s|\[\]()]+\|([^\s|\[\]()]+)', r'\1', s)
    s = re.sub(r'\[[^\]]*\]', '', s)
    s = re.sub(r'^\(bound form\)\s*', '', s)
    s = re.sub(r'\s+', ' ', s).strip()
    return '' if s.startswith('used in ') or s.startswith('see also') or s.startswith('(Note:') else s

def meaning_of(senses):
    out = ''
    for s in senses:
        s = clean_sense(clean(s))
        if not out and len(s) > MEANING_CHARS:
            # A long first sense: its notes in brackets left out (汉服: "traditional
            # Han Chinese attire (including various styles ...)").
            s = re.sub(r'\s+', ' ', re.sub(r'\s*\([^()]*\)', '', s)).strip() or s
            # ...and of its glosses (one sense, "a; b; c"), those that fit.
            parts = s.split('; ')
            s = parts[0]
            for x in parts[1:]:
                if len(s) + 2 + len(x) > MEANING_CHARS:
                    break
                s += '; ' + x
            # ...and one gloss still too long for a row: up to its first comma or
            # colon ("cheongsam, a traditional Chinese dress ...": cheongsam).
            if len(s) > MEANING_LONG:
                m = re.match(r'^(.{3,%d}?)(?::\s|,\s|\s[–—]\s)' % MEANING_LONG, s)
                if m:
                    s = m.group(1)
            # A very long one (a row fits about this much, smaller): up to where a
            # phrase ends — " by", " or", " such as", " (" — before that.
            if len(s) > MEANING_MOST:
                cut = max(s.rfind(w, 0, MEANING_MOST) for w in (' by ', ' or ', ' such as ', ' (', ' indicating ', ' used ', ' in '))
                if cut >= 16:
                    s = s[:cut].rstrip(' ,;:')
            s = s.rstrip(' ,;:-') + ')' * max(0, s.count('(') - s.count(')'))   # a bracket the cut left open, closed
        if not s:
            continue
        if not out:
            out = s
        elif len(out) + 2 + len(s) <= MEANING_CHARS:
            out += '; ' + s
    return out

def main():
    print('reading the sources...')
    have     = read_strokes_chars()
    unihan   = read_unihan()
    ts       = read_opencc('TSCharacters.txt')
    st       = read_opencc('STCharacters.txt')
    cedict   = read_cedict()
    zipf     = read_wordfreq()
    hsk_c, hsk_w = read_hsk()
    kd       = read_kanjidic()
    cmn, trans = read_tatoeba()
    print(len(have), 'characters,', len(cedict), 'CC-CEDICT entries,', len(cmn), 'Mandarin sentences')

    # Traditional-only characters: a traditional form (TSCharacters) that is not
    # itself written in Simplified Chinese.
    simplified_set = set(st.keys()) | {s for v in ts.values() for s in v}
    trad_only = sorted(c for c, v in ts.items() if c not in v and c not in simplified_set and len(c) == 1)

    # Single-character entries: a character's readings and meanings — its usual
    # reading's (Unihan's first) first.
    preferred = {c: u.get('kMandarin', '').split(' ')[0].lower() for c, u in unihan.items() if u.get('kMandarin')}
    char_pinyin, char_entries = defaultdict(list), defaultdict(list)
    for trad, simp, py, senses, _ in cedict:
        for form in {trad, simp}:
            if len(form) == 1:
                p = pinyin(py)
                if p and p.lower() not in [x.lower() for x in char_pinyin[form]]:
                    char_pinyin[form].append(p)
                if senses:
                    char_entries[form].append((p.lower(), senses))
    char_senses = {}
    for form, entries in char_entries.items():
        entries.sort(key=lambda e: 0 if e[0] == preferred.get(form, '') else 1)   # stable: CC-CEDICT's order otherwise
        out = []
        for _, senses in entries:
            out.extend(x for x in senses if x not in out)
        char_senses[form] = out

    # Words: CC-CEDICT's, each written form once (its first entry, the commoner
    # reading coming first in CC-CEDICT less reliably: the one wordfreq can't
    # tell apart keeps the first).
    words, seen = [], {}
    for trad, simp, py, senses, _ in cedict:
        if not senses:
            continue
        for form, script in ((simp, 's'), (trad, 't')):
            if script == 't' and trad == simp:
                continue
            n = len(form)
            if n == 0 or n > WORD_CHARS or any(c not in have for c in form) or re.search(r'[A-Za-z0-9·，]', form):
                continue
            reading = pinyin(py)
            key = (form, reading.lower())
            if key in seen:
                continue
            simp_form = simp
            z = zipf.get(simp_form, 0.0)
            level = hsk_w.get(simp_form, 0)
            if py[:1].isupper() and not level:
                z -= 1.5            # a name: after the everyday words
            common = level > 0 or z >= 3.2
            freq = max(0, min(0xFFFE, int(round((8.0 - z) * 100)) if z > 0 else 1000 + 60))
            seen[key] = len(words)
            words.append({'written': form, 'reading': reading, 'py': py, 'meaning': meaning_of(senses), 'freq': freq, 'common': common,
                          'level': level, 'zipf': z, 'script': script, 'chars': n, 'name': py[:1].isupper()})
    # A form read more than one way (这 zhè, zhèi; 行 xíng, háng): wordfreq counts
    # the form, not the reading, so the others rank after its usual one — the
    # reading whose syllables are most often its characters' Unihan readings
    # (这些 zhèxiē, not zhèixiē), else CC-CEDICT's first entry — and a sentence's
    # words (wordsplit.c) show that one.
    def usual_score(w):
        syllables = [pinyin(x).lower() for x in w['py'].split(' ')]
        return sum(1 for c, x in zip(w['written'], syllables) if preferred.get(c, '') == x)
    usual, best = {}, {}
    for w in words:
        sc = usual_score(w) if len(w['py'].split(' ')) == w['chars'] else 0
        if w['written'] not in usual or sc > best[w['written']]:
            usual[w['written']], best[w['written']] = w['reading'], sc
    for w in words:
        if w['reading'] != usual[w['written']]:
            w['zipf'] -= 1.5
            w['freq'] = min(0xFFFE, w['freq'] + 150)
    print(len(words), 'words')

    # How common each character is: by the words it is in, as written in
    # Simplified Chinese (wordfreq counts those: a traditional form has its
    # simplified one's count, and 瞭 would rank with 了). A traditional-only
    # character, by its own words, well after (a hundredth: past the common ones).
    trad_set = set(trad_only)
    weight = defaultdict(float)
    for w in words:
        if w['zipf'] > 0:
            for c in set(w['written']):
                if w['script'] == 's':
                    weight[c] += 10 ** w['zipf']
                elif c in trad_set:
                    weight[c] += 10 ** w['zipf'] * 0.01
    rank = {c: i + 1 for i, (c, _) in enumerate(sorted(weight.items(), key=lambda kv: -kv[1]))}

    # Sentences: each common word's best (it in it, written in its script, a
    # comfortable length, translated into more languages).
    by_len = sorted(((sid, t) for sid, t in cmn.items() if SENT_MIN <= len(t) <= SENT_MAX and trans['en'].get(sid)), key=lambda x: x[0])
    word_sentence = {}
    index = defaultdict(list)   # a character → the sentences with it
    for sid, t in by_len:
        for c in set(t):
            index[c].append(sid)
    for i, w in enumerate(words):
        if not w['common'] or w['chars'] > EXAMPLE_CHARS + 1:
            continue
        if w['chars'] == 1 and len(char_pinyin.get(w['written'], [])) > 1:
            continue    # one character of several readings: which the text has, it does not say (行 háng, xíng)
        rare = min(w['written'], key=lambda c: len(index.get(c, [])))
        best, best_score = None, None
        for sid in index.get(rare, []):
            t = cmn[sid]
            if w['written'] not in t:
                continue
            score = abs(len(t) - SENT_IDEAL) - 3 * sum(1 for l in ('es', 'fr', 'pt') if trans[l].get(sid))
            if best_score is None or score < best_score:
                best, best_score = sid, score
        if best is not None:
            word_sentence[i] = best
    kept_sentences = sorted(set(word_sentence.values()))
    sentence_line = {sid: n for n, sid in enumerate(kept_sentences)}
    print(len(word_sentence), 'words with a sentence,', len(kept_sentences), 'sentences')

    # Each character's list.
    cands = defaultdict(list)
    for i, w in enumerate(words):
        for c in set(w['written']):
            cands[c].append(i)
    def score(i, c):
        w = words[i]
        s = (w['level'] if w['level'] else 9) * 100 + w['freq'] / 4.0
        s += 0 if w['common'] else 5000
        s += 40 * max(0, w['chars'] - 2)
        s -= 300 if w['chars'] == 1 and w['written'] == c else 0     # the character on its own first...
        s -= 50 if w['chars'] == 1 and w['reading'].lower() == preferred.get(c, '') else 0   # ...in its usual reading
        s += 200 if w['name'] else 0
        return s
    lists = {}
    for c, ids in cands.items():
        trad_char = c in trad_only
        ids = [i for i in ids if (words[i]['script'] == 't') == trad_char or words[i]['written'] == c]
        ids.sort(key=lambda i: (score(i, c), i))
        kept, examples = [], 0
        for i in ids:
            w = words[i]
            if len(kept) >= EXAMPLES:
                break
            if not w['common'] or w['chars'] > EXAMPLE_CHARS:
                continue
            if any(words[k]['written'] == w['written'] or (len(words[k]['written']) > 1 and words[k]['written'] in w['written']) for k in kept):
                continue
            kept.append(i)
        examples = len(kept)
        for i in ids:
            if len(kept) >= WORDS_ALL:
                break
            if i not in kept and all(words[k]['written'] != words[i]['written'] for k in kept):
                kept.append(i)
        if kept:
            lists[c] = (examples, kept)

    # Words kept: every listed one and every common one (text is read with them).
    keep = sorted({i for _, ks in lists.values() for i in ks} | {i for i, w in enumerate(words) if w['common']})
    line_of = {i: n for n, i in enumerate(keep)}

    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, 'chars.tsv'), 'w', encoding='utf-8') as f:
        f.write('# code point, grade, level, radical, frequency, pinyin, (none), English, ' + ', '.join(LANGS) + '\n')
        for c in sorted(have):
            u = unihan.get(c, {})
            readings = list(char_pinyin.get(c, []))
            first = u.get('kMandarin', '').split(' ')[0]
            if first:
                readings = [first] + [r for r in readings if r.lower() != first.lower()]
            english = meaning_of(char_senses.get(c, [])) or clean(u.get('kDefinition', ''))
            radical = u.get('kRSUnicode', '').split(' ')[0].split('.')[0].rstrip("'")
            level = hsk_c.get(c, 0)
            jp = kd.get(c) or next((kd[x] for x in st.get(c, []) + ts.get(c, []) if x in kd), None) or {}
            row = ['%x' % ord(c), '0', str(level), radical if radical.isdigit() and int(radical) < 256 else '0',
                   str(rank[c] if rank.get(c, 99999) < 65535 else 0), '、'.join(readings), '', english] + [clean(jp.get(l, '')) for l in LANGS]
            f.write('\t'.join(row) + '\n')
    with open(os.path.join(OUT, 'words.tsv'), 'w', encoding='utf-8') as f:
        f.write('# written, reading, English, ' + ', '.join(LANGS) + ', freq, common, sentence\n')
        for i in keep:
            w = words[i]
            s = sentence_line.get(word_sentence.get(i), -1) if i in word_sentence else -1
            f.write('\t'.join([w['written'], w['reading'], w['meaning']] + [''] * len(LANGS) + [str(w['freq']), '1' if w['common'] else '0', str(s)]) + '\n')
    with open(os.path.join(OUT, 'lists.tsv'), 'w', encoding='utf-8') as f:
        f.write('# code point, examples, word lines\n')
        for c in sorted(lists):
            examples, ks = lists[c]
            f.write('\t'.join(['%x' % ord(c), str(examples)] + [str(line_of[k]) for k in ks]) + '\n')
    with open(os.path.join(OUT, 'sentences.tsv'), 'w', encoding='utf-8') as f:
        for sid in kept_sentences:
            f.write('\t'.join(clean(x) for x in [cmn[sid], trans['en'].get(sid, ''), trans['es'].get(sid, ''), trans['fr'].get(sid, ''), trans['pt'].get(sid, '')]) + '\n')

    # The traditional-only characters, for lang.c.
    with open(os.path.join(ROOT, 'fude', 'lang', 'zh', 'traditional.h'), 'w', encoding='utf-8') as f:
        f.write('// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.\n\n'
                '// The characters only Traditional Chinese writes (lang.c\'s second group): OpenCC\'s\n'
                '// traditional forms that Simplified Chinese does not write. Generated by\n'
                '// apps/hanzi/tools/data/prepare.py: do not edit.\n\n')
        f.write('static const u32 FUDE_ZH_TRADITIONAL_ONLY[] = {\n')
        for k in range(0, len(trad_only), 12):
            f.write('    ' + ', '.join('0x%04Xu' % ord(c) for c in trad_only[k:k + 12]) + ',\n')
        f.write('};\n')
    print('wrote', OUT, ':', len(have), 'characters,', len(keep), 'words,', len(lists), 'lists,', len(kept_sentences), 'sentences;',
          len(trad_only), 'traditional-only characters')

if __name__ == '__main__':
    main()
