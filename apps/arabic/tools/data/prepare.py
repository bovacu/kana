#!/usr/bin/env python3
# Arabic's data, prepared for the bake (study/chars/bake.h's PREPARED tables):
# data/raw/ar/prepared/{chars,words,lists,sentences}.tsv, from the sources
# fetch.py downloads. Run from the project root after fetch.py:
#
#   python3 -m pip install msgpack     # once (wordfreq's file)
#   python3 apps/arabic/tools/data/fetch.py
#   python3 apps/arabic/tools/data/prepare.py
#
# The strokes the bake reads, data/raw/ar/arabicvg.xml, are ours
# (apps/arabic/tools/strokes). What each table holds, and from where:
#   chars      every letter, joined form, vowel mark and digit of
#              fude/lang/ar/lang.c's table, at the level it gives it (read from
#              there: one table for both): how common it is (in wordfreq's
#              words), and what it is — a letter's sound (ث: th, as in think), a
#              form's place (initial form), a mark's vowel — in English, Spanish,
#              Portuguese, French and Japanese (ours).
#   words      English Wiktionary's Arabic words (kaikki.org, CC BY-SA 4.0), not
#              names, letters, affixes or inflected forms, written with their
#              vowels (Wiktionary's vowelled form: كِتَاب): their romanization
#              (Wiktionary's, with sh th dh kh gh for its š ṯ ḏ ḵ ḡ, as the
#              letters' names write them), their meaning in English, and in
#              Spanish, French, Portuguese and Japanese where those Wiktionaries
#              have the word; how common (wordfreq, CC BY-SA 4.0: its forms'
#              counts summed, little words and endings on) — common when
#              wordfreq counts it, the main one of the words spelt alike without
#              their vowels; and a Tatoeba sentence with it in.
#   lists      a letter's words (those with it in); a joined form's, the words
#              where the letter takes that form (ب: ﺑ in بَيْت, ـبـ in كِتَابَة);
#              a vowel mark's, the words that write it. The examples first
#              (common, short, the commonest first), then more to add.
#   sentences  the Tatoeba sentences kept (CC BY 2.0 FR: Arabic, then English,
#              Spanish, French, Portuguese), each a word's.
#
# Words and text are compared bare: without their vowel marks or tatweel, with
# أ إ آ ٱ as ا (most text is written without vowels); a word of text is found
# with its little words (و ف, then ال ب ل ك) taken off its front, as itself, or
# as one of a word's inflected forms.

import gzip, math, os, re, sys, unicodedata
from collections import defaultdict

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..'))
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(ROOT, 'tools', 'data'))
import prepared as P

AR  = os.path.join(P.RAW, 'ar')
OUT = os.path.join(AR, 'prepared')

WORD_CHARS    = 20       # longer words are not kept (lang/ar/bake.c's word_chars: code points, harakat too)
EXAMPLE_CHARS = 12
SENT_MIN, SENT_MAX, SENT_IDEAL = 8, 60, 28       # code points, spaces included
SKIP_POS = {'name', 'character', 'prefix', 'suffix', 'interfix', 'infix', 'affix', 'combining_form', 'symbol', 'punct',
            'romanization', 'syllable', 'abbrev', 'proverb', 'root', 'diacritic'}

HARAKAT = set(chr(c) for c in range(0x064B, 0x0660)) | {'ٰ'}
TATWEEL = 'ـ'
ALIFS   = {'آ': 'ا', 'أ': 'ا', 'إ': 'ا', 'ٱ': 'ا'}

def arabic_letter(c):
    return 'ء' <= c <= 'ي' or c in ('ٱ',)

def bare(s):
    return ''.join(ALIFS.get(c, c) for c in s if c not in HARAKAT and c != TATWEEL)

def all_arabic(s):
    return bool(s) and all(arabic_letter(c) or c in HARAKAT for c in s) and any(arabic_letter(c) for c in s)

def nfc(s):
    return unicodedata.normalize('NFC', s.replace('‌', '').replace('‍', '').replace('‏', '').replace('‎', ''))

# Wiktionary's romanization, with the letters' names' digraphs (ث thāʾ, خ khāʾ).
ROMAN = [('ʔ', 'ʾ'), ('ʕ', 'ʿ'), ('š', 'sh'), ('ṯ', 'th'), ('ḏ', 'dh'), ('ḵ', 'kh'), ('ḫ', 'kh'), ('ḡ', 'gh'), ('ġ', 'gh'),
         ('Š', 'Sh'), ('Ṯ', 'Th'), ('Ḏ', 'Dh'), ('Ḵ', 'Kh'), ('Ḡ', 'Gh')]

def roman(r):
    r = P.clean(r.split(',')[0].split(' / ')[0])
    for a, b in ROMAN:
        r = r.replace(a, b)
    return r

# --- the letters: lang.c's table, and its joining ----------------------------------------------

def letters():
    # lang.c's FUDE_AR_TABLE rows: { 0x0628, FUDE_AR_LETTERS, 1, "b..." }.
    src = open(os.path.join(ROOT, 'fude', 'lang', 'ar', 'lang.c'), encoding='utf-8').read()
    body = src[src.index('FUDE_AR_TABLE[] = {'):]
    body = body[:body.index('};')]
    out = {}
    for m in re.finditer(r'\{\s*0x([0-9A-Fa-f]{4}),\s*FUDE_AR_(\w+),\s*(\d)\s*,', body):
        out[chr(int(m.group(1), 16))] = (m.group(2), int(m.group(3)))
    assert len(out) == 137, 'lang.c table: %d' % len(out)
    return out

# Each letter's forms (Unicode's presentation forms), as lang.c's fude_lang_form.
FORMS = defaultdict(dict)
for cp in range(0xFE70, 0xFEFD):
    d = unicodedata.decomposition(chr(cp))
    if d:
        tag, *rest = d.split()
        FORMS[''.join(chr(int(x, 16)) for x in rest)][tag.strip('<>')] = chr(cp)

def form_of(before, c, after):
    # The character c is written as between before and after ('' none), and
    # whether after is written in it (ل then ا: ﻻ).
    f = FORMS.get(c, {})
    joins_after = lambda x: 'initial' in FORMS.get(x, {})
    joins_before = lambda x: 'final' in FORMS.get(x, {})
    joined_before = bool(before) and joins_after(before) and 'final' in f
    if c == 'ل' and after == 'ا':
        return ('ﻼ' if joined_before else 'ﻻ'), True
    joined_after = bool(after) and 'initial' in f and joins_before(after)
    if joined_before and joined_after:
        return f['medial'], False
    if joined_before:
        return f['final'], False
    if joined_after:
        return f['initial'], False
    return c, False

def forms_in(written):
    # Every form a word's letters take in it.
    letters_ = [c for c in written if c not in HARAKAT and c != TATWEEL]
    out = set()
    i = 0
    while i < len(letters_):
        f, with_after = form_of(letters_[i - 1] if i else '', letters_[i], letters_[i + 1] if i + 1 < len(letters_) else '')
        out.add(f)
        i += 2 if with_after else 1
    return out

# What each letter, form, mark and digit is: English, Spanish, Portuguese, French, Japanese.
SOUND = {
    'ء': ('hamza: a catch in the throat, as in uh-oh', 'hamza: un corte en la garganta, como en «uh-oh»', 'hamza: uma parada na garganta, como em «uh-oh»', 'hamza : un coup de glotte, comme dans « oh-oh »', 'ハムザ：声門閉鎖音（「アッ」の詰まり）'),
    'آ': ('ʾā: hamza and a long a', 'ʾā: hamza y una a larga', 'ʾā: hamza e um a longo', 'ʾā : hamza et un a long', 'ʾā：ハムザと長いa'),
    'أ': ('hamza on alif: ʾa, ʾu', 'hamza sobre alif: ʾa, ʾu', 'hamza sobre alif: ʾa, ʾu', 'hamza sur alif : ʾa, ʾu', 'アリフの上のハムザ：ʾa、ʾu'),
    'إ': ('hamza under alif: ʾi', 'hamza bajo alif: ʾi', 'hamza sob alif: ʾi', 'hamza sous alif : ʾi', 'アリフの下のハムザ：ʾi'),
    'ؤ': ('hamza on wāw', 'hamza sobre wāw', 'hamza sobre wāw', 'hamza sur wāw', 'ワーウの上のハムザ'),
    'ئ': ('hamza on yāʾ', 'hamza sobre yāʾ', 'hamza sobre yāʾ', 'hamza sur yāʾ', 'ヤーの上のハムザ'),
    'ا': ('the long vowel ā, or a seat for hamza', 'la vocal larga ā, o el soporte de la hamza', 'a vogal longa ā, ou o apoio da hamza', 'la voyelle longue ā, ou le support de la hamza', '長母音ā、またはハムザの台'),
    'ب': ('b', 'b', 'b', 'b', 'b'),
    'ة': ('tāʾ marbūṭa: -a at a word’s end (-at before a vowel)', 'tāʾ marbūṭa: -a al final (-at ante vocal)', 'tāʾ marbūṭa: -a no fim (-at antes de vogal)', 'tāʾ marbūṭa : -a en fin de mot (-at devant une voyelle)', 'ター・マルブータ：語末の-a（母音の前では-at）'),
    'ت': ('t', 't', 't', 't', 't'),
    'ث': ('th, as in think', 'z española (c de «cielo»)', 'th do inglês «think»', 'th de l’anglais « think »', '英語thinkのth'),
    'ج': ('j, as in jam', 'y fuerte, como la j inglesa de «jam»', 'dj, como em «adjetivo»', 'dj, comme dans « djinn »', 'ジャのj'),
    'ح': ('ḥ: a deep, breathed h', 'ḥ: una h aspirada y profunda', 'ḥ: um h aspirado e profundo', 'ḥ : un h soufflé et profond', 'ḥ：のどの奥の強いh'),
    'خ': ('kh, as in Bach', 'j española', 'kh, como o j espanhol', 'kh, comme la jota espagnole', 'kh（ドイツ語Bachのch）'),
    'د': ('d', 'd', 'd', 'd', 'd'),
    'ذ': ('dh: th, as in this', 'dh: la d suave de «nada»', 'dh: th do inglês «this»', 'dh : th de l’anglais « this »', 'dh（英語thisのth）'),
    'ر': ('r, rolled', 'r', 'r vibrante, como em «caro»', 'r roulé', '巻き舌のr'),
    'ز': ('z', 'z sonora, como en inglés «zoo»', 'z', 'z', 'z'),
    'س': ('s', 's', 's', 's', 's'),
    'ش': ('sh', 'sh, como en inglés «she»', 'x, como em «xá»', 'ch, comme dans « chat »', 'sh（シャ行）'),
    'ص': ('ṣ: a deep s', 'ṣ: una s profunda', 'ṣ: um s profundo', 'ṣ : un s profond', 'ṣ：重いs'),
    'ض': ('ḍ: a deep d', 'ḍ: una d profunda', 'ḍ: um d profundo', 'ḍ : un d profond', 'ḍ：重いd'),
    'ط': ('ṭ: a deep t', 'ṭ: una t profunda', 'ṭ: um t profundo', 'ṭ : un t profond', 'ṭ：重いt'),
    'ظ': ('ẓ: a deep dh', 'ẓ: una dh profunda', 'ẓ: um dh profundo', 'ẓ : un dh profond', 'ẓ：重いdh'),
    'ع': ('ʿ: a sound squeezed in the throat', 'ʿ: un sonido apretado en la garganta', 'ʿ: um som apertado na garganta', 'ʿ : un son serré dans la gorge', 'ʿ：のどを締めて出す音'),
    'غ': ('gh: a gargled r', 'gh: una r gutural, como la r francesa', 'gh: um r gutural, como o r de «carro»', 'gh : un r grasseyé, comme le r français', 'gh：のどで鳴らすr'),
    'ف': ('f', 'f', 'f', 'f', 'f'),
    'ق': ('q: a k said deep in the throat', 'q: una k profunda en la garganta', 'q: um k no fundo da garganta', 'q : un k au fond de la gorge', 'q：のどの奥のk'),
    'ك': ('k', 'k', 'k', 'k', 'k'),
    'ل': ('l', 'l', 'l', 'l', 'l'),
    'م': ('m', 'm', 'm', 'm', 'm'),
    'ن': ('n', 'n', 'n', 'n', 'n'),
    'ه': ('h', 'h aspirada, como en inglés «house»', 'h aspirado, como no inglês «house»', 'h aspiré, comme dans l’anglais « house »', 'h'),
    'و': ('w, or the long vowel ū', 'w, o la vocal larga ū', 'w, ou a vogal longa ū', 'w, ou la voyelle longue ū', 'w、または長母音ū'),
    'ى': ('alif maqṣūra: a long ā at a word’s end', 'alif maqṣūra: una ā larga al final', 'alif maqṣūra: um ā longo no fim', 'alif maqṣūra : un ā long en fin de mot', 'アリフ・マクスーラ：語末の長いā'),
    'ي': ('y, or the long vowel ī', 'y, o la vocal larga ī', 'i, ou a vogal longa ī', 'y, ou la voyelle longue ī', 'y、または長母音ī'),
    'ﻻ': ('lām and alif written together: lā', 'lām y alif juntas: lā', 'lām e alif juntos: lā', 'lām et alif liés : lā', 'ラームとアリフの合字：lā'),
    'ً': ('tanwīn: -an', 'tanwīn: -an', 'tanwīn: -an', 'tanwīn : -an', 'タンウィーン：-an'),
    'ٌ': ('tanwīn: -un', 'tanwīn: -un', 'tanwīn: -un', 'tanwīn : -un', 'タンウィーン：-un'),
    'ٍ': ('tanwīn: -in', 'tanwīn: -in', 'tanwīn: -in', 'tanwīn : -in', 'タンウィーン：-in'),
    'َ': ('the short vowel a', 'la vocal breve a', 'a vogal breve a', 'la voyelle brève a', '短母音a'),
    'ُ': ('the short vowel u', 'la vocal breve u', 'a vogal breve u', 'la voyelle brève u', '短母音u'),
    'ِ': ('the short vowel i', 'la vocal breve i', 'a vogal breve i', 'la voyelle brève i', '短母音i'),
    'ّ': ('doubles the consonant', 'duplica la consonante', 'dobra a consoante', 'double la consonne', '子音を重ねる'),
    'ْ': ('no vowel after the consonant', 'sin vocal tras la consonante', 'sem vogal depois da consoante', 'pas de voyelle après la consonne', '子音の後に母音なし'),
}
DIGITS = [('zero', 'cero', 'zero', 'zéro', 'ゼロ'), ('one', 'uno', 'um', 'un', '一'), ('two', 'dos', 'dois', 'deux', '二'),
          ('three', 'tres', 'três', 'trois', '三'), ('four', 'cuatro', 'quatro', 'quatre', '四'), ('five', 'cinco', 'cinco', 'cinq', '五'),
          ('six', 'seis', 'seis', 'six', '六'), ('seven', 'siete', 'sete', 'sept', '七'), ('eight', 'ocho', 'oito', 'huit', '八'),
          ('nine', 'nueve', 'nove', 'neuf', '九')]
for _i, _d in enumerate(DIGITS):
    SOUND[chr(0x0660 + _i)] = _d
PLACE = {'initial': ('initial form', 'forma inicial', 'forma inicial', 'forme initiale', '語頭形'),
         'medial':  ('medial form', 'forma media', 'forma medial', 'forme médiane', '語中形'),
         'final':   ('final form', 'forma final', 'forma final', 'forme finale', '語末形')}

def char_meaning(c):
    # A letter's sound; a joined form's place in a word.
    if c in SOUND:
        return SOUND[c]
    for letter, f in FORMS.items():
        for tag, cp in f.items():
            if cp == c and tag in PLACE:
                return PLACE[tag]
    raise KeyError('no meaning for %04X' % ord(c))

def read_wordfreq():
    # wordfreq's buckets: the i-th holds the words of zipf 9 - i/100.
    import msgpack
    data = msgpack.unpackb(gzip.open(os.path.join(AR, 'large_ar.msgpack.gz')).read(), raw=False)
    zipf = {}
    for i, bucket in enumerate(data[1:], start=1):
        for w in bucket:
            zipf.setdefault(bare(nfc(w)), 9.0 - i / 100.0)
    return zipf

PROCLITICS = ['و', 'ف']
ARTICLES   = ['ال', 'لل', 'بال', 'كال', 'ب', 'ل', 'ك']

def candidates(token):
    # A word of text without its little words, longest first: والكتاب → كتاب.
    out = [token]
    for p in [''] + PROCLITICS:
        if p and not token.startswith(p):
            continue
        rest = token[len(p):]
        if rest and rest not in out:
            out.append(rest)
        for a in ARTICLES:
            if rest.startswith(a) and len(rest) > len(a) + 1 and rest[len(a):] not in out:
                out.append(rest[len(a):])
    return out

# A sense that says what the word is, not what it means: a letter, a form of
# another word, a verbal noun.
NOT_MEANING = re.compile(r"^(nom d.action|forme isolée|lettre|vingt|pluriel|féminin|plural|femenino|feminino|letra)|の複数形|の女性形|の動名詞|文字", re.I)

def read_natives(path, lang):
    # Another Wiktionary's Arabic words: each entry's vowelled form (its own, or
    # its canonical one), its bare form, its part of speech, its meaning.
    out = []
    if not os.path.exists(path):
        return out
    for e in P.read_kaikki(path):
        w = nfc(e.get('word', ''))
        if not all_arabic(w) or e.get('pos') in ('character', 'symbol', 'letter', 'name', 'prefix', 'suffix'):
            continue
        canonical = next((nfc(f.get('form', '')) for f in e.get('forms', []) if 'canonical' in f.get('tags', [])), '')
        vowelled = w if any(c in HARAKAT for c in w) else canonical if all_arabic(canonical) else ''
        senses = []
        for sense in e.get('senses', []):
            if P.sense_usable(sense):
                gs = [g for g in P.native_glosses(sense, lang) if not NOT_MEANING.search(g)]
                gs = [g[0].lower() + g[1:] if len(g) == 1 or (g[:1].isupper() and g[1:2].islower()) else g for g in gs]
                if gs:
                    senses.append(gs)
        meaning = P.join_senses(senses)
        if meaning:
            out.append({'vowelled': vowelled, 'bare': bare(w), 'pos': e.get('pos'), 'meaning': meaning})
    return out

def native_lookup(natives):
    # A word's meaning in that Wiktionary: by its vowelled form; else by its bare
    # form and part of speech (a noun's meaning is not its verb's).
    by_vowelled, by_bare_pos, by_bare = {}, {}, defaultdict(list)
    for n in natives:
        if n['vowelled']:
            by_vowelled.setdefault(n['vowelled'], n['meaning'])
        by_bare_pos.setdefault((n['bare'], n['pos']), n['meaning'])
        by_bare[n['bare']].append(n['meaning'])
    def find(written, b, pos):
        if written in by_vowelled:
            return by_vowelled[written]
        for p in pos:
            if (b, p) in by_bare_pos:
                return by_bare_pos[(b, p)]
        return ''
    return find

def english_of(s):
    # Wiktionary's glosses, without "verbal noun of شَمِسَ (šamisa) (form I)," before them.
    gs = P.english_glosses(s)
    out = []
    for g in gs:
        g = re.sub(r'^(verbal noun|active participle|passive participle|elative degree) of \S+( \([^)]*\))*,?\s*', '', g)
        if g:
            out.append(g)
    return out

def main():
    print('reading the sources...')
    table = letters()
    chars = sorted(table)
    zipf = read_wordfreq()
    natives = {lang: native_lookup(read_natives(os.path.join(AR, 'kaikki-%s.jsonl' % lang), lang)) for lang in ('es', 'fr', 'pt', 'ja')}

    # Words: Wiktionary's lemmas, a vowelled form once, its entries' senses in
    # order; and every inflected form, bare → its lemmas.
    entries = defaultdict(lambda: {'senses': [], 'pos': [], 'bare': ''})
    order = []
    forms = defaultdict(set)
    for e in P.read_kaikki(os.path.join(AR, 'kaikki-en.jsonl')):
        w = nfc(e.get('word', ''))
        if not all_arabic(w):
            continue
        canonical = next((nfc(f.get('form', '')) for f in e.get('forms', []) if 'canonical' in f.get('tags', [])), w)
        canonical = canonical.split(' / ')[0].strip()
        for s in e.get('senses', []):
            for f in s.get('form_of', []) + s.get('alt_of', []):
                lemma = nfc(f.get('word', ''))
                if all_arabic(lemma):
                    forms[bare(w)].add(bare(lemma))
        for f in e.get('forms', []):
            form = nfc(f.get('form', ''))
            if all_arabic(form) and 'romanization' not in f.get('tags', []):
                forms[bare(form)].add(bare(canonical))
        if e.get('pos') in SKIP_POS or not all_arabic(canonical) or len(canonical) > WORD_CHARS:
            continue
        r = next((f.get('form', '') for f in e.get('forms', []) if 'romanization' in f.get('tags', []) and f.get('form')), '')
        r = roman(r)
        senses = [english_of(s) for s in e.get('senses', []) if P.sense_usable(s)]
        senses = [s for s in senses if s]
        if not r or not senses or len(r.encode('utf-8')) > P.READING_BYTES:
            continue
        key = (canonical, r)
        if key not in entries:
            order.append(key)
            entries[key]['bare'] = bare(canonical)
        entries[key]['senses'] += senses
        entries[key]['pos'].append(e.get('pos'))
    words = []
    for (w, r) in order:
        english = P.join_senses(entries[(w, r)]['senses'])
        if not english:
            continue
        b = entries[(w, r)]['bare']
        words.append({'written': w, 'bare': b, 'reading': r, 'meaning': english,
                      'in': {l: natives[l](w, b, entries[(w, r)]['pos']) for l in ('es', 'pt', 'fr', 'ja')},
                      'zipf': 0.0, 'freq': 0, 'level': 0, 'common': False, 'chars': len(w),
                      'pos': entries[(w, r)]['pos'], 'keywords': P.keywords_of(english)})
    by_bare = defaultdict(list)
    for i, w in enumerate(words):
        by_bare[w['bare']].append(i)

    # wordfreq counts each written form as it stands, its little words on
    # (والكتاب) and inflected (الكتب): a word's count is its forms' summed — a
    # form of several words shared between them.
    total = defaultdict(float)
    for token, z in zipf.items():
        if not all(arabic_letter(c) for c in token):
            continue
        for c in candidates(token):
            if c in by_bare:
                total[c] += 10 ** z
                break
            lemmas = [l for l in sorted(forms.get(c, ())) if l in by_bare]
            if lemmas:
                for l in lemmas:
                    total[l] += 10 ** z / len(lemmas)
                break
    for w in words:
        w['zipf'] = math.log10(total[w['bare']]) if total.get(w['bare'], 0.0) > 0.0 else 0.0

    # Each word of each sentence (its little words off): the words it can be.
    own, trans = P.read_tatoeba('ara')
    own = {sid: nfc(t) for sid, t in own.items()}
    usable = {}
    for sid, t in own.items():
        t = P.clean(t)
        if not (SENT_MIN <= len(t) <= SENT_MAX) or not trans['en'].get(sid) or t.startswith('#'):
            continue
        tokens = []
        for token in re.findall('[ء-ٰٟٱ]+', t):
            token = bare(token)
            ids = []
            for c in candidates(token):
                ids = list(by_bare.get(c, []))
                if not ids:
                    ids = [i for lemma in sorted(forms.get(c, ())) for i in by_bare.get(lemma, [])]
                if ids:
                    break
            tokens.append(ids)
        usable[sid] = (re.findall('[a-z]+', trans['en'][sid].lower()), tokens)

    # Words spelt alike without their vowels (قَلَم pen, قَلَمَ to cut) share
    # wordfreq's count: the one the sentences' English says most is the main
    # one, common when wordfreq counts it (ties: Wiktionary's first); another is
    # common only when the sentences say it often.
    said = defaultdict(int)
    for english, tokens in usable.values():
        for ids in tokens:
            if len(ids) > 1:
                for i in ids:
                    said[i] += 1 if P.mentions(words[i]['keywords'], english) else 0
    for b, ids in by_bare.items():
        main = max(ids, key=lambda i: (said[i], -i))
        for i in ids:
            w = words[i]
            w['freq']   = P.zipf_freq(w['zipf'], 1060) + (0 if i == main else 40)
            w['common'] = w['zipf'] >= 3.0 and (i == main or said[i] >= 3)
    print(len(words), 'words,', sum(1 for w in words if w['common']), 'common;', len(forms), 'inflected forms')

    # Sentences: a word of text is a common word or a form of one; words spelt
    # alike told apart by the English translation, or the sentence goes to none.
    found = defaultdict(set)
    for sid, (english, tokens) in usable.items():
        for ids in tokens:
            ids = [i for i in ids if words[i]['common']]
            if len(ids) > 1:
                told = [i for i in ids if P.mentions(words[i]['keywords'], english)]
                ids = told if len(told) == 1 else []
            for i in ids:
                found[i].add(sid)
    word_sentence = P.assign_sentences(words, found, own, trans, SENT_IDEAL)
    print(len(word_sentence), 'words with a sentence')

    # Lists: a letter's words, a form's (the words where its letter takes it), a mark's.
    def listed_in(w):
        return set(w['bare']) | forms_in(w['written']) | (set(w['written']) & HARAKAT)
    def score(i, c):
        w = words[i]
        s = w['freq'] / 2.0
        s += 0 if w['common'] else 5000
        s += 15 * max(0, len(w['bare']) - 4)
        s += 0 if i in word_sentence else 30
        return s
    lists = P.build_lists(words, set(chars), EXAMPLE_CHARS, score, listed_in)

    # How common each is: in wordfreq's words (forms by where they fall in them).
    usage = defaultdict(float)
    for b, z in zipf.items():
        if not all(arabic_letter(c) for c in b):
            continue
        for c in set(b) | forms_in(b):
            if c in table:
                usage[c] += 10 ** (z - 3.0)
    ranked = sorted(chars, key=lambda c: (-usage.get(c, 0.0), c))
    rank = {c: k + 1 for k, c in enumerate(ranked)}

    char_rows = []
    for c in chars:
        group, level = table[c]
        m = char_meaning(c)
        char_rows.append(['%x' % ord(c), 0, level, 0, rank[c], '', '', m[0], m[1], m[2], m[3], m[4]])

    keep, kept_sentences = P.write_tables(OUT, char_rows, words, lists, word_sentence, own, trans,
                                          'code point, grade, level, radical, frequency, -, -, English, ' + ', '.join(P.LANGS))
    by = defaultdict(int)
    for c in chars:
        by[table[c][1]] += 1
    print('wrote', OUT, ':', len(chars), 'letters, forms, marks, digits (levels %s),' % ', '.join('%d: %d' % kv for kv in sorted(by.items())),
          len(keep), 'words (', sum(1 for i in keep if words[i]['common']), 'common,', sum(1 for i in keep if i in word_sentence),
          'with a sentence ),', len(lists), 'lists,', len(kept_sentences), 'sentences')
    print('meanings in es/pt/fr/ja:', ', '.join('%s %d' % (l, sum(1 for i in keep if words[i]['in'][l])) for l in P.LANGS))
    print('no list:', ' '.join(c for c in chars if c not in lists))

if __name__ == '__main__':
    main()
