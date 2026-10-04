# The half of a language's data tools that Thai's and Hindi's share
# (apps/<app>/tools/data/prepare.py): reading Wiktionary's entries (kaikki.org's
# JSON lines), Tatoeba's sentences and the Volubilis spreadsheet; meanings cut to
# what the bake keeps; each word's sentence; each character's word list; and the
# PREPARED tables written (fude/study/chars/bake.h has their format).
#
# Hangul's and Hanzi's tools came first and keep their own copies of the
# meaning rules; these are the same rules.

import bz2, json, math, os, re, zipfile
import xml.etree.ElementTree as ET
from collections import defaultdict

ROOT  = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
RAW   = os.path.join(ROOT, 'data', 'raw')
LANGS = ['es', 'pt', 'fr', 'ja']        # the bake's other languages, in order (4 at most)

EXAMPLES      = 6        # example words kept per character (bake.h)
WORDS_ALL     = 20       # words kept per character in all
MEANING_CHARS = 48       # glosses after the first while the meaning stays this short
MEANING_LONG  = 64       # a first gloss longer than this is cut at its first clause when it can be
MEANING_MOST  = 96       # ...and one longer than this, where a phrase ends
MEANING_BYTES = 127      # the bake keeps a word's meanings in 128 bytes (UTF-8, NUL included)
READING_BYTES = 63       # ...and its reading in 64

# --- meanings ----------------------------------------------------------------------------

def clean(s):
    return re.sub(r'\s+', ' ', s.replace('​', '').replace(' ', ' ').replace('\t', ' ').replace('\n', ' ')).strip()

def top_split(s, sep):
    # s split at sep, but not inside brackets.
    out, depth, cur = [], 0, ''
    i = 0
    while i < len(s):
        c = s[i]
        depth += 1 if c in '([（' else -1 if c in ')]）' else 0
        if depth <= 0 and s.startswith(sep, i):
            out.append(cur)
            cur, i = '', i + len(sep)
            continue
        cur += c
        i += 1
    out.append(cur)
    return [x.strip() for x in out if x.strip()]

def fit_bytes(s, most=MEANING_BYTES):
    # The bake cuts at 127 bytes, maybe inside a character: glosses dropped from
    # the end instead, a gloss still too long cut at a space.
    while len(s.encode('utf-8')) > most and '; ' in s:
        s = s.rsplit('; ', 1)[0]
    while len(s.encode('utf-8')) > most:
        s = s[:-1]
        s = s.rsplit(' ', 1)[0] if ' ' in s and len(s.encode('utf-8')) > most else s
    return s.rstrip(' ,;:')

def shorten(s):
    # A long first gloss: its notes in brackets left out, cut at its first
    # clause, and one still too long where a phrase ends.
    if len(s) <= MEANING_CHARS:
        return s
    s = re.sub(r'\s+', ' ', re.sub(r'\s*\([^()]*\)', '', s)).strip() or s
    if len(s) > MEANING_LONG:
        m = re.match(r'^(.{3,%d}?)(?::\s|,\s|;\s|\s[–—]\s)' % MEANING_LONG, s)
        if m:
            s = m.group(1)
    if len(s) > MEANING_MOST:
        cut = max(s.rfind(w, 0, MEANING_MOST) for w in (' by ', ' or ', ' such as ', ' (', ' that ', ' which ', ' used ', ' in ', ' when '))
        if cut >= 16:
            s = s[:cut]
    return s.rstrip(' ,;:-.') + ')' * max(0, s.count('(') - s.count(')'))

def join_senses(senses):
    # The first sense's glosses while they fit, the second sense's too while
    # they all still fit; each gloss once; "; " between.
    out, seen = [], set()
    for glosses in senses[:2]:
        glosses = [g for g in glosses if g and g.lower() not in seen]
        if not glosses:
            continue
        if not out:
            out = [shorten(glosses[0])]
            for g in glosses[1:]:
                if len('; '.join(out + [g])) > MEANING_CHARS:
                    break
                out.append(g)
        elif len('; '.join(out + glosses[:1])) <= MEANING_CHARS:
            for g in glosses:
                if len('; '.join(out + [g])) > MEANING_CHARS:
                    break
                out.append(g)
        else:
            break
        seen |= {g.lower() for g in out}
    return fit_bytes('; '.join(out))

# --- Wiktionary (kaikki.org) ---------------------------------------------------------------

SKIP_TAGS = {'obsolete', 'archaic', 'dated', 'form-of', 'alt-of', 'misspelling', 'abbreviation', 'initialism'}

def read_kaikki(path):
    with open(path, encoding='utf-8') as f:
        for line in f:
            yield json.loads(line)

def sense_usable(s):
    tags = set(s.get('tags', []))
    return bool(s.get('glosses')) and not (tags & SKIP_TAGS) and 'form_of' not in s and 'alt_of' not in s

def sense_gloss(s):
    # A sense's gloss: a sub-sense's general one (Wiktionary nests "to write" over
    # "to draw (figures)"), unless that is only a heading ("inflection of X:").
    gl = s['glosses']
    return gl[0] if len(gl) > 1 and not gl[0].rstrip().endswith(':') else gl[-1]

def english_glosses(s):
    # A sense's English glosses, split at its semicolons; the article and "to" of
    # a verb kept (to eat).
    g = clean(sense_gloss(s)).rstrip('.')
    return [x for x in top_split(g, ';') if x]

def native_glosses(s, lang):
    # A sense's glosses in another Wiktionary: Japanese 「液、液体。」, French
    # "Manger, avoir.", Portuguese "peixe (animal aquático)".
    g = clean(sense_gloss(s))
    if lang == 'ja':
        g = g.rstrip('。.')
        parts = re.split(r'[、，;；]', g)
    else:
        g = g.rstrip('.')
        parts = top_split(g, ', ') if lang == 'fr' else top_split(g, '; ')
    out = []
    for p in parts:
        p = p.strip()
        if lang in ('fr', 'pt', 'es') and p[:1].isupper() and p[1:2].islower():
            p = p[0].lower() + p[1:]
        if p:
            out.append(p)
    return out

def native_meanings(path, lang, written_ok):
    # Every word's meaning in another Wiktionary, its senses in order.
    senses = defaultdict(list)
    if not os.path.exists(path):
        return {}
    for e in read_kaikki(path):
        w = e.get('word', '')
        if not written_ok(w):
            continue
        for s in e.get('senses', []):
            if sense_usable(s):
                senses[w].append(native_glosses(s, lang))
    return {w: join_senses(ss) for w, ss in senses.items()}

# --- Volubilis (an .xlsx: its first sheet, read with the standard library) -----------------

_NS = '{http://schemas.openxmlformats.org/spreadsheetml/2006/main}'

def xlsx_rows(path):
    z = zipfile.ZipFile(path)
    strings = []
    for _, el in ET.iterparse(z.open('xl/sharedStrings.xml')):
        if el.tag == _NS + 'si':
            strings.append(''.join(t.text or '' for t in el.iter(_NS + 't')))
            el.clear()
    def column(ref):
        n = 0
        for ch in re.match(r'[A-Z]+', ref).group(0):
            n = n * 26 + ord(ch) - 64
        return n - 1
    for _, el in ET.iterparse(z.open('xl/worksheets/sheet1.xml')):
        if el.tag == _NS + 'row':
            row = {}
            for c in el.iter(_NS + 'c'):
                v, t = c.find(_NS + 'v'), c.get('t')
                if t == 's' and v is not None:
                    row[column(c.get('r'))] = strings[int(v.text)]
                elif t == 'inlineStr':
                    row[column(c.get('r'))] = ''.join(x.text or '' for x in c.iter(_NS + 't'))
                else:
                    row[column(c.get('r'))] = v.text if v is not None else ''
            el.clear()
            if row:
                yield [row.get(i, '') for i in range(max(row) + 1)]

def volubilis_glosses(s):
    # "comida [la] ; alimentos [los]", "manger ; bouffer (fam.)": the glosses,
    # without their grammar in brackets or their register in parentheses.
    out = []
    for g in re.split(r'\s*;\s*', clean(s)):
        g = re.sub(r'\s*\[[^\]]*\]', '', g)
        g = re.sub(r'\s*\((fam|inf|pop|vx|loc|fig|vulg|arg|lit|Am|Br|fml|form|sl|coll|péj|pej)[^)]*\)', '', g)
        g = g.strip(' .!')
        if g and g not in out:
            out.append(g)
    return out

# --- Tatoeba ---------------------------------------------------------------------------------

def read_tatoeba(code):
    # The language's sentences and, for each, its first English, Spanish, French
    # and Portuguese translation ('' none).
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
        if not os.path.exists(path):
            return out
        with bz2.open(path, 'rt', encoding='utf-8') as f:
            for line in f:
                p = line.rstrip('\n').split('\t')
                if len(p) >= 2:
                    out[int(p[0])].append(int(p[1]))
        return out
    own = sentences(os.path.join(T, '%s_sentences.tsv.bz2' % code))
    trans = {}
    for lang, name in [('en', 'eng'), ('es', 'spa'), ('fr', 'fra'), ('pt', 'por')]:
        ln = links(os.path.join(T, '%s-%s_links.tsv.bz2' % (code, name)))
        wanted = {t for s in own for t in ln.get(s, [])}
        texts = sentences(os.path.join(T, '%s_sentences.tsv' % name), wanted)
        trans[lang] = {s: next((texts[t] for t in ln.get(s, []) if t in texts), '') for s in own}
    return own, trans

# Telling words spelt alike apart by a sentence's English translation.
STOPWORDS = {'a', 'an', 'the', 'of', 'to', 'in', 'on', 'at', 'by', 'or', 'and', 'for', 'with', 'from', 'into', 'about', 'up',
             'out', 'off', 's', 'be', 'have', 'one', 'oneself', 'someone', 'something', 'somebody', 'being', 'its', 'not', 'very',
             'etc', 'act', 'thing', 'things', 'state', 'kind', 'sort', 'way', 'that', 'which', 'when'}
IRREGULAR = {'do': 'does did done', 'go': 'goes went gone',
             'eat': 'ate eaten', 'see': 'saw seen', 'come': 'came', 'take': 'took taken', 'give': 'gave given', 'say': 'said',
             'make': 'made', 'know': 'knew known', 'think': 'thought', 'tell': 'told', 'get': 'got gotten', 'buy': 'bought',
             'teach': 'taught', 'write': 'wrote written', 'sleep': 'slept', 'meet': 'met', 'sit': 'sat', 'stand': 'stood',
             'leave': 'left', 'feel': 'felt', 'find': 'found', 'hear': 'heard', 'speak': 'spoke spoken', 'run': 'ran',
             'drink': 'drank drunk', 'begin': 'began begun', 'forget': 'forgot forgotten', 'lose': 'lost', 'win': 'won',
             'sell': 'sold', 'send': 'sent', 'spend': 'spent', 'build': 'built', 'bring': 'brought', 'catch': 'caught',
             'fall': 'fell fallen', 'fly': 'flew flown', 'swim': 'swam', 'sing': 'sang sung', 'break': 'broke broken',
             'choose': 'chose chosen', 'wear': 'wore worn', 'draw': 'drew drawn', 'grow': 'grew grown', 'throw': 'threw thrown',
             'hold': 'held', 'keep': 'kept', 'pay': 'paid', 'lie': 'lay lain', 'ride': 'rode ridden', 'rise': 'rose risen',
             'shine': 'shone', 'understand': 'understood', 'wake': 'woke woken', 'cry': 'cried', 'die': 'died', 'study': 'studied',
             'person': 'people', 'child': 'children', 'man': 'men', 'woman': 'women', 'tooth': 'teeth', 'foot': 'feet',
             'i': 'me my mine', 'we': 'us our', 'good': 'better best', 'bad': 'worse worst', 'many': 'more most', 'much': 'more most'}
IRREGULAR = {k: set(v.split()) for k, v in IRREGULAR.items()}

def keywords_of(english):
    return {k for k in re.findall(r'[a-z]+', english.lower()) if k not in STOPWORDS}

def mentions(keywords, english_words):
    # Is one of a word's English glosses in a translation's words (eat: eating, ate)?
    for kw in keywords:
        for t in english_words:
            if t == kw or (len(kw) >= 3 and t.startswith(kw)) or (len(kw) >= 5 and t.startswith(kw[:-1])) or t in IRREGULAR.get(kw, ()):
                return True
    return False

def zipf_freq(z, level_freq):
    # How common, as the bake keeps it: (8 - zipf) x 100, lower the commoner; a
    # word not counted, about where its level sits.
    return max(0, min(0xFFFE, int(round((8.0 - z) * 100)) if z > 0 else level_freq))

# --- sentences, lists, the tables -------------------------------------------------------------

def assign_sentences(words, found, own, trans, ideal):
    # Each word's best sentence (a comfortable length, translated into more
    # languages), each sentence one word's: the easiest and commonest words
    # choose first. found: word index → the sentences it was found in.
    def score(sid):
        t = clean(own[sid])
        return abs(len(t) - ideal) - 3 * sum(1 for l in ('es', 'fr', 'pt') if trans[l].get(sid)) + (8 if re.search(r'[A-Za-z]', t) else 0)
    word_sentence, used = {}, set()
    for i in sorted(found, key=lambda i: (words[i]['level'] or 9, words[i]['freq'], i)):
        free = [sid for sid in found[i] if sid not in used]
        if free:
            best = min(free, key=lambda sid: (score(sid), sid))
            word_sentence[i] = best
            used.add(best)
    return word_sentence

def build_lists(words, chars, example_chars, score, in_word=None):
    # Each character's list: the words with it in (in_word(word) → the characters
    # it lists under; its letters by default), the examples first (common, short,
    # by score), then more to add; no two alike.
    cands = defaultdict(list)
    for i, w in enumerate(words):
        for c in (in_word(w) if in_word else set(w['written'])):
            if c in chars:
                cands[c].append(i)
    lists = {}
    for c, ids in cands.items():
        ids.sort(key=lambda i: (score(i, c), i))
        kept = []
        for i in ids:
            w = words[i]
            if len(kept) >= EXAMPLES:
                break
            if not w['common'] or w['chars'] > example_chars:
                continue
            if any(words[k]['written'] == w['written'] or (len(words[k]['written']) > 2 and words[k]['written'] in w['written']) for k in kept):
                continue
            kept.append(i)
        examples_n = len(kept)
        for i in ids:
            if len(kept) >= WORDS_ALL:
                break
            if i not in kept and all(words[k]['written'] != words[i]['written'] for k in kept):
                kept.append(i)
        if kept:
            lists[c] = (examples_n, kept)
    return lists

def write_tables(out, char_rows, words, lists, word_sentence, own, trans, header):
    # The four tables. char_rows: [code point, grade, level, radical, frequency,
    # readings 0, readings 1, English, then LANGS'], one a character.
    kept_sentences = sorted(set(word_sentence.values()))
    sentence_line = {sid: n for n, sid in enumerate(kept_sentences)}
    keep = sorted({i for _, ks in lists.values() for i in ks} | {i for i, w in enumerate(words) if w['common']})
    line_of = {i: n for n, i in enumerate(keep)}
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, 'chars.tsv'), 'w', encoding='utf-8') as f:
        f.write('# ' + header + '\n')
        for row in char_rows:
            f.write('\t'.join(clean(str(x)) for x in row) + '\n')
    with open(os.path.join(out, 'words.tsv'), 'w', encoding='utf-8') as f:
        f.write('# written, reading, English, ' + ', '.join(LANGS) + ', freq, common, sentence\n')
        for i in keep:
            w = words[i]
            s = sentence_line[word_sentence[i]] if i in word_sentence else -1
            f.write('\t'.join([w['written'], w['reading'], clean(w['meaning'])] + [clean(w['in'].get(l, '')) for l in LANGS] +
                              [str(w['freq']), '1' if w['common'] else '0', str(s)]) + '\n')
    with open(os.path.join(out, 'lists.tsv'), 'w', encoding='utf-8') as f:
        f.write('# code point, examples, word lines\n')
        for c in sorted(lists):
            examples_n, ks = lists[c]
            f.write('\t'.join(['%x' % ord(c), str(examples_n)] + [str(line_of[k]) for k in ks]) + '\n')
    with open(os.path.join(out, 'sentences.tsv'), 'w', encoding='utf-8') as f:
        for sid in kept_sentences:
            f.write('\t'.join(clean(x) for x in [own[sid], trans['en'].get(sid, ''), trans['es'].get(sid, ''), trans['fr'].get(sid, ''), trans['pt'].get(sid, '')]) + '\n')
    return keep, kept_sentences
