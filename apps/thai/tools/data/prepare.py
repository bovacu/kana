#!/usr/bin/env python3
# Thai's data, prepared for the bake (study/chars/bake.h's PREPARED tables):
# data/raw/th/prepared/{chars,words,lists,sentences}.tsv, from the sources
# fetch.py downloads. Run from the project root after fetch.py:
#
#   python3 apps/thai/tools/data/fetch.py
#   python3 apps/thai/tools/data/prepare.py
#
# The strokes the bake reads, data/raw/th/thaivg.xml, are ours
# (apps/thai/tools/strokes). What each table holds, and from where:
#   chars      every letter of fude/lang/th/lang.c's table, at the level it gives
#              it (read from there: one table for both): how common it is (its
#              count in the Thai National Corpus's words), and what it means —
#              a consonant's key word (ก ko kai: chicken), a vowel's or sign's
#              part — in English, Spanish, Portuguese, French and Japanese (ours).
#   words      English Wiktionary's Thai words (kaikki.org, CC BY-SA 4.0), not
#              names, letters or affixes: their tone romanization (Wiktionary's:
#              ภาษา paa-sǎa), their meaning in English (Wiktionary's), Spanish,
#              French and Portuguese (Volubilis, by Belisan, CC BY-SA 4.0; else
#              the French and Portuguese Wiktionaries) and Japanese (the Japanese
#              Wiktionary); how common (PyThaiNLP's Thai National Corpus counts,
#              CC0); common when Volubilis gives a level (A0, A1, A2) or it is
#              frequent; and a Tatoeba sentence with it in.
#   lists      each letter's words (those with it in): the examples first
#              (common, short, the easiest level and the commonest first), then
#              more to add.
#   sentences  the Tatoeba sentences kept (CC BY 2.0 FR: Thai, then English,
#              Spanish, French, Portuguese), each a word's.
#
# Thai has no spaces between words: a sentence is split into words by the
# fewest dictionary words that make it (Wiktionary's, Volubilis's and the
# corpus's), never inside a letter's cluster (a consonant and its marks).

import math, os, re, sys
from collections import defaultdict

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..'))
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(ROOT, 'tools', 'data'))
import prepared as P

TH  = os.path.join(P.RAW, 'th')
OUT = os.path.join(TH, 'prepared')

WORD_CHARS    = 16       # longer words are not kept (lang/th/bake.c's word_chars: code points, marks too)
EXAMPLE_CHARS = 10
SENT_MIN, SENT_MAX, SENT_IDEAL = 6, 40, 18      # code points
LEVELS     = {'A0': 1, 'A1': 2, 'A2': 3}         # Volubilis's
LEVEL_FREQ = {1: 520, 2: 600, 3: 700}            # how common, for a word the corpus doesn't count: by its level
SKIP_POS   = {'name', 'character', 'prefix', 'suffix', 'infix', 'affix', 'symbol', 'punct', 'romanization', 'syllable', 'abbrev'}

MATCH_STOP = {'a', 'an', 'the', 'to', 'be', 'of', 'one', 'oneself', 'someone', 'something', 'somebody', 'etc', 's', 'or', 'and'}

def match_words(s):
    return {k for k in re.findall(r'[a-z]+', s.lower()) if k not in MATCH_STOP}

def thai(c):
    return 'ก' <= c <= '๛'

def all_thai(w):
    return bool(w) and all(thai(c) for c in w)

# --- the letters: lang.c's table ---------------------------------------------------------------

def letters():
    # Each code point of lang.c's FUDE_TH_LETTERS, in order from U+0E01: C(level,
    # name), V(...), M(...), D(name) (level 1), X (none).
    src = open(os.path.join(ROOT, 'fude', 'lang', 'th', 'lang.c'), encoding='utf-8').read()
    body = src[src.index('FUDE_TH_LETTERS['):]
    body = body[body.index('{') + 1:body.index('};')]
    body = re.sub(r'//[^\n]*', '', body)
    out = {}
    cp = 0x0E01
    for m in re.finditer(r'\b([CVMD])\(\s*(?:(\d)\s*,\s*)?"([^"]*)"\s*\)|\bX\b', body):
        if m.group(0) != 'X':
            out[chr(cp)] = (m.group(1), int(m.group(2)) if m.group(2) else 1, m.group(3))
        cp += 1
    assert cp == 0x0E5C, 'lang.c table: %x' % cp
    return out

# What each letter means: a consonant its key word's meaning (ก ไก่ ko kai:
# chicken), a vowel or sign its part, a digit its number. English, Spanish,
# Portuguese, French, Japanese.
MEANING = {
    'ก': ('chicken', 'gallina', 'galinha', 'poule', '鶏'),
    'ข': ('egg', 'huevo', 'ovo', 'œuf', '卵'),
    'ฃ': ('bottle (no longer used)', 'botella (ya no se usa)', 'garrafa (não se usa mais)', 'bouteille (plus utilisée)', '瓶（今は使われない）'),
    'ค': ('buffalo', 'búfalo', 'búfalo', 'buffle', '水牛'),
    'ฅ': ('person (no longer used)', 'persona (ya no se usa)', 'pessoa (não se usa mais)', 'personne (plus utilisée)', '人（今は使われない）'),
    'ฆ': ('bell', 'campana', 'sino', 'cloche', '鐘'),
    'ง': ('snake', 'serpiente', 'cobra', 'serpent', '蛇'),
    'จ': ('plate', 'plato', 'prato', 'assiette', '皿'),
    'ฉ': ('small cymbals', 'platillos pequeños', 'pratinhos (címbalos)', 'petites cymbales', '小さなシンバル'),
    'ช': ('elephant', 'elefante', 'elefante', 'éléphant', '象'),
    'ซ': ('chain', 'cadena', 'corrente', 'chaîne', '鎖'),
    'ฌ': ('tree', 'árbol', 'árvore', 'arbre', '木'),
    'ญ': ('woman', 'mujer', 'mulher', 'femme', '女性'),
    'ฎ': ('dancer’s headdress', 'tocado de danza', 'coroa de dança', 'coiffe de danse', '舞踊の冠'),
    'ฏ': ('goad', 'aguijón', 'aguilhão', 'aiguillon', '突き棒'),
    'ฐ': ('pedestal', 'pedestal', 'pedestal', 'piédestal', '台座'),
    'ฑ': ('Montho (in the Ramakien)', 'Montho (del Ramakien)', 'Montho (do Ramakien)', 'Montho (du Ramakien)', 'モントー（ラーマキエン）'),
    'ฒ': ('old man', 'anciano', 'ancião', 'vieillard', '老人'),
    'ณ': ('novice monk', 'monje novicio', 'monge noviço', 'moine novice', '見習い僧'),
    'ด': ('child', 'niño', 'criança', 'enfant', '子ども'),
    'ต': ('turtle', 'tortuga', 'tartaruga', 'tortue', '亀'),
    'ถ': ('bag', 'bolsa', 'saco', 'sac', '袋'),
    'ท': ('soldier', 'soldado', 'soldado', 'soldat', '兵士'),
    'ธ': ('flag', 'bandera', 'bandeira', 'drapeau', '旗'),
    'น': ('mouse', 'ratón', 'rato', 'souris', 'ネズミ'),
    'บ': ('leaf', 'hoja', 'folha', 'feuille', '葉'),
    'ป': ('fish', 'pez', 'peixe', 'poisson', '魚'),
    'ผ': ('bee', 'abeja', 'abelha', 'abeille', '蜂'),
    'ฝ': ('lid', 'tapa', 'tampa', 'couvercle', 'ふた'),
    'พ': ('offering tray', 'bandeja de ofrendas', 'bandeja de oferendas', 'plateau d’offrandes', '供物の台'),
    'ฟ': ('tooth', 'diente', 'dente', 'dent', '歯'),
    'ภ': ('junk (a ship)', 'junco (barco)', 'junco (barco)', 'jonque', 'ジャンク船'),
    'ม': ('horse', 'caballo', 'cavalo', 'cheval', '馬'),
    'ย': ('giant', 'gigante', 'gigante', 'géant', '鬼（巨人）'),
    'ร': ('boat', 'barco', 'barco', 'bateau', '船'),
    'ล': ('monkey', 'mono', 'macaco', 'singe', '猿'),
    'ว': ('ring', 'anillo', 'anel', 'bague', '指輪'),
    'ศ': ('pavilion', 'pabellón', 'pavilhão', 'pavillon', 'あずまや'),
    'ษ': ('hermit', 'ermitaño', 'eremita', 'ermite', '仙人'),
    'ส': ('tiger', 'tigre', 'tigre', 'tigre', '虎'),
    'ห': ('chest (a box)', 'cofre', 'baú', 'coffre', '箱'),
    'ฬ': ('star kite', 'cometa de estrella', 'pipa em estrela', 'cerf-volant étoile', '星形の凧'),
    'อ': ('basin', 'palangana', 'bacia', 'bassine', 'たらい'),
    'ฮ': ('owl', 'búho', 'coruja', 'hibou', 'フクロウ'),
    'ฤ': ('vowel letter rue (ri, roe)', 'letra vocal rue (ri, roe)', 'letra vogal rue (ri, roe)', 'lettre voyelle rue (ri, roe)', '母音字 ルー（リ、ルー）'),
    'ฦ': ('vowel letter lue (no longer used)', 'letra vocal lue (ya no se usa)', 'letra vogal lue (não se usa mais)', 'lettre voyelle lue (plus utilisée)', '母音字 ルー（今は使われない）'),
    'ฯ': ('abbreviation sign', 'signo de abreviatura', 'sinal de abreviação', 'signe d’abréviation', '省略記号'),
    'ะ': ('short vowel a', 'vocal a breve', 'vogal a breve', 'voyelle a brève', '短母音 a'),
    'ั': ('short a, inside a syllable', 'a breve, dentro de la sílaba', 'a breve, dentro da sílaba', 'a bref, dans la syllabe', '音節の中の短い a'),
    'า': ('long vowel aa', 'vocal a larga', 'vogal a longa', 'voyelle a longue', '長母音 aa'),
    'ำ': ('vowel am', 'vocal am', 'vogal am', 'voyelle am', '母音 am'),
    'ิ': ('short vowel i', 'vocal i breve', 'vogal i breve', 'voyelle i brève', '短母音 i'),
    'ี': ('long vowel ii', 'vocal i larga', 'vogal i longa', 'voyelle i longue', '長母音 ii'),
    'ึ': ('short vowel ue', 'vocal ue breve', 'vogal ue breve', 'voyelle ue brève', '短母音 ue'),
    'ื': ('long vowel uee', 'vocal ue larga', 'vogal ue longa', 'voyelle ue longue', '長母音 uee'),
    'ุ': ('short vowel u', 'vocal u breve', 'vogal u breve', 'voyelle ou brève', '短母音 u'),
    'ู': ('long vowel uu', 'vocal u larga', 'vogal u longa', 'voyelle ou longue', '長母音 uu'),
    'ฺ': ('no vowel after it (Pali)', 'sin vocal detrás (pali)', 'sem vogal depois (páli)', 'sans voyelle après (pali)', '母音なし（パーリ語）'),
    'เ': ('vowel e, written before', 'vocal e, escrita delante', 'vogal e, escrita antes', 'voyelle é, écrite avant', '母音 e（前に書く）'),
    'แ': ('vowel ae, written before', 'vocal ae, escrita delante', 'vogal ae, escrita antes', 'voyelle è, écrite avant', '母音 ae（前に書く）'),
    'โ': ('vowel o, written before', 'vocal o, escrita delante', 'vogal o, escrita antes', 'voyelle o, écrite avant', '母音 o（前に書く）'),
    'ใ': ('vowel ai, written before (in 20 words)', 'vocal ai, escrita delante (en 20 palabras)', 'vogal ai, escrita antes (em 20 palavras)', 'voyelle aï, écrite avant (dans 20 mots)', '母音 ai（前に書く、20語のみ）'),
    'ไ': ('vowel ai, written before', 'vocal ai, escrita delante', 'vogal ai, escrita antes', 'voyelle aï, écrite avant', '母音 ai（前に書く）'),
    'ๅ': ('lengthens ฤ and ฦ', 'alarga ฤ y ฦ', 'alonga ฤ e ฦ', 'allonge ฤ et ฦ', 'ฤ・ฦ を長くする'),
    'ๆ': ('repeat the word before', 'repite la palabra anterior', 'repete a palavra anterior', 'répète le mot d’avant', '前の語をくり返す'),
    '็': ('shortens the vowel', 'acorta la vocal', 'encurta a vogal', 'abrège la voyelle', '母音を短くする'),
    '่': ('first tone mark', 'primera marca de tono', 'primeira marca de tom', 'première marque de ton', '第1声調記号'),
    '้': ('second tone mark', 'segunda marca de tono', 'segunda marca de tom', 'deuxième marque de ton', '第2声調記号'),
    '๊': ('third tone mark', 'tercera marca de tono', 'terceira marca de tom', 'troisième marque de ton', '第3声調記号'),
    '๋': ('fourth tone mark', 'cuarta marca de tono', 'quarta marca de tom', 'quatrième marque de ton', '第4声調記号'),
    '์': ('the letter under it is silent', 'la letra de debajo no suena', 'a letra embaixo não soa', 'la lettre dessous est muette', '下の字を読まない'),
    'ํ': ('nikhahit (in ำ, and in Pali)', 'nikhahit (en ำ y en pali)', 'nikhahit (em ำ e em páli)', 'nikhahit (dans ำ, et en pali)', 'ニッカヒット（ำ の中、パーリ語）'),
    '๎': ('yamakkan (no longer used)', 'yamakkan (ya no se usa)', 'yamakkan (não se usa mais)', 'yamakkan (plus utilisé)', 'ヤマッカーン（今は使われない）'),
    '๐': ('zero', 'cero', 'zero', 'zéro', 'ゼロ'),
    '๑': ('one', 'uno', 'um', 'un', '一'),
    '๒': ('two', 'dos', 'dois', 'deux', '二'),
    '๓': ('three', 'tres', 'três', 'trois', '三'),
    '๔': ('four', 'cuatro', 'quatro', 'quatre', '四'),
    '๕': ('five', 'cinco', 'cinco', 'cinq', '五'),
    '๖': ('six', 'seis', 'seis', 'six', '六'),
    '๗': ('seven', 'siete', 'sete', 'sept', '七'),
    '๘': ('eight', 'ocho', 'oito', 'huit', '八'),
    '๙': ('nine', 'nueve', 'nove', 'neuf', '九'),
}

# --- splitting a sentence into words ------------------------------------------------------------

# A word never starts at a mark above or below, nor at a vowel written after
# its consonant (ะ า ำ ๅ), and never ends at a vowel written before (เ แ โ ใ ไ).
NO_START = set('ะัาำิีึืฺุูๅ็่้๊๋์ํ๎')
NO_END   = set('เแโใไ')

def splitter(dictionary):
    longest = max(len(w) for w in dictionary)
    def split(text):
        # The fewest pieces, a letter outside every word costing as much as two.
        n = len(text)
        best = [(0, -1)] + [(10 ** 9, -1)] * n
        for i in range(n):
            if best[i][0] >= 10 ** 9 or (i > 0 and (text[i] in NO_START or text[i - 1] in NO_END)):
                continue
            for j in range(i + 1, min(n, i + longest) + 1):
                if j < n and (text[j] in NO_START or text[j - 1] in NO_END):
                    continue
                piece = text[i:j]
                cost = 1 if piece in dictionary else (2 * (j - i) if j - i <= 3 else None)
                if cost is not None and best[i][0] + cost < best[j][0]:
                    best[j] = (best[i][0] + cost, i)
        if best[n][0] >= 10 ** 9:
            return [text]
        out, j = [], n
        while j > 0:
            i = best[j][1]
            out.append(text[i:j])
            j = i
        return out[::-1]
    return split

# --- main ---------------------------------------------------------------------------------------

def main():
    print('reading the sources...')
    table = letters()
    chars = sorted(table)

    # Volubilis: each Thai word's rows, the levelled first: its level, and its
    # Spanish, Portuguese, French glosses.
    rows = P.xlsx_rows(os.path.join(TH, 'VOLUBILIS-Mundo.xlsx'))
    next(rows); header = [h.split('\n')[0].strip() for h in next(rows)]
    col = {name.split(' ')[0]: i for i, name in enumerate(header)}
    vol = defaultdict(list)
    vol_words = set()
    for r in rows:
        r = r + [''] * (len(header) - len(r))
        for t in re.split(r'\s*[;=]\s*', P.clean(r[col['THA']]).strip('[] ')):
            t = t.strip('[] ')
            if not all_thai(t):
                continue
            vol_words.add(t)
            lv = LEVELS.get(r[col['LEVEL']].strip().upper()[:2], 0)
            vol[t].append((lv or 9, len(vol[t]), {'en': r[col['ENG']], 'es': r[col['SPA']], 'pt': r[col['POR']], 'fr': r[col['FRA']]}))
    level_of = {t: min(x[0] for x in v) for t, v in vol.items()}

    # The English of a word's easiest Volubilis row: Wiktionary's senses that say
    # it come first (กับ "with", not its first etymology's "trap").
    def volubilis_english(t):
        rows_ = sorted(vol.get(t, []), key=lambda x: (x[0], x[1]))
        return match_words(rows_[0][2]['en']) if rows_ else set()

    def volubilis_meaning(t, lang):
        senses = [P.volubilis_glosses(x[2][lang]) for x in sorted(vol.get(t, []), key=lambda x: (x[0], x[1])) if x[2][lang].strip()]
        return P.join_senses(senses)

    # The corpus's counts: zipf (log10 per billion words).
    counts = {}
    with open(os.path.join(TH, 'tnc_freq.txt'), encoding='utf-8') as f:
        for line in f:
            p = line.rstrip('\n').split('\t')
            if len(p) == 2 and p[1].isdigit():
                counts[p[0]] = counts.get(p[0], 0) + int(p[1])
    total = sum(counts.values())
    def zipf(w):
        n = counts.get(w, 0)
        return math.log10(n * 1e9 / total) if n else 0.0

    ja = P.native_meanings(os.path.join(TH, 'kaikki-ja.jsonl'), 'ja', all_thai)
    fr = P.native_meanings(os.path.join(TH, 'kaikki-fr.jsonl'), 'fr', all_thai)
    pt = P.native_meanings(os.path.join(TH, 'kaikki-pt.jsonl'), 'pt', all_thai)

    # Words: Wiktionary's, a written form and reading once, its entries' senses in order.
    entries = defaultdict(lambda: {'senses': [], 'pos': []})
    order = []
    for e in P.read_kaikki(os.path.join(TH, 'kaikki-en.jsonl')):
        w = e.get('word', '')
        if e.get('pos') in SKIP_POS or not all_thai(w) or len(w) > WORD_CHARS:
            continue
        roman = next((s.get('roman', '') for s in e.get('sounds', []) if 'Paiboon' in s.get('raw_tags', []) and s.get('roman')), '')
        if not roman:
            continue
        roman = P.clean(roman.split(',')[0])
        senses = [P.english_glosses(s) for s in e.get('senses', []) if P.sense_usable(s)]
        senses = [s for s in senses if s]
        if not senses or len(roman.encode('utf-8')) > P.READING_BYTES:
            continue
        key = (w, roman)
        if key not in entries:
            order.append(key)
        entries[key]['senses'] += senses
        entries[key]['pos'].append(e.get('pos'))
    words = []
    for (w, roman) in order:
        senses = entries[(w, roman)]['senses']
        top = volubilis_english(w)
        if top:
            senses = sorted(senses, key=lambda g: 0 if match_words(' '.join(g)) & top else 1)
        english = P.join_senses(senses)
        if not english:
            continue
        z = zipf(w)
        level = level_of.get(w, 9)
        level = level if level < 9 else 0
        meanings = {'es': volubilis_meaning(w, 'es'),
                    'pt': volubilis_meaning(w, 'pt') or pt.get(w, ''),
                    'fr': volubilis_meaning(w, 'fr') or fr.get(w, ''),
                    'ja': ja.get(w, '')}
        words.append({'written': w, 'reading': roman, 'meaning': english, 'in': meanings, 'zipf': z,
                      'freq': P.zipf_freq(z, LEVEL_FREQ.get(level, 1060)), 'level': level,
                      'common': level > 0 or z >= 3.2, 'chars': len(w), 'pos': entries[(w, roman)]['pos'],
                      'keywords': P.keywords_of(english)})
    by_written = defaultdict(list)
    for i, w in enumerate(words):
        by_written[w['written']].append(i)
    print(len(words), 'words,', sum(1 for w in words if w['common']), 'common,', sum(1 for w in words if w['level']), 'with a level')

    # Sentences: split into words; a word found in one counts for it, words spelt
    # alike told apart by the English translation (or the sentence goes to none).
    own, trans = P.read_tatoeba('tha')
    dictionary = {w for w in by_written} | vol_words | {w for w in counts if all_thai(w) and len(w) <= 24}
    split = splitter(dictionary)
    found = defaultdict(set)
    for sid, t in own.items():
        t = P.clean(t)
        if not (SENT_MIN <= len(t) <= SENT_MAX) or not trans['en'].get(sid) or t.startswith('#'):
            continue
        english = re.findall(r'[a-z]+', trans['en'][sid].lower())
        for run in re.findall(r'[ก-๛]+', t):
            for piece in split(run):
                ids = [i for i in by_written.get(piece, []) if words[i]['common']]
                if len(ids) > 1:
                    told = [i for i in ids if P.mentions(words[i]['keywords'], english)]
                    ids = told if len(told) == 1 else []
                for i in ids:
                    found[i].add(sid)
    word_sentence = P.assign_sentences(words, found, own, trans, SENT_IDEAL)
    print(len(word_sentence), 'words with a sentence')

    # Lists: a letter's words.
    def score(i, c):
        w = words[i]
        s = (w['level'] or 9) * 100 + w['freq'] / 4.0
        s += 0 if w['common'] else 5000
        s += 20 * max(0, w['chars'] - 5)
        s += 0 if i in word_sentence else 30
        return s
    lists = P.build_lists(words, set(chars), EXAMPLE_CHARS, score)

    # How common each letter is: its count in the corpus's words, rank 1 the commonest.
    usage = defaultdict(int)
    for w, n in counts.items():
        for c in w:
            if c in table:
                usage[c] += n
    ranked = sorted(chars, key=lambda c: (-usage.get(c, 0), c))
    rank = {c: k + 1 for k, c in enumerate(ranked)}

    char_rows = []
    for c in chars:
        kind, level, name = table[c]
        m = MEANING.get(c, ('', '', '', '', ''))
        # English, then LANGS' order: es, pt, fr, ja.
        char_rows.append(['%x' % ord(c), 0, level, 0, rank[c], '', '', m[0], m[1], m[2], m[3], m[4]])
    missing = [c for c in chars if c not in MEANING]
    assert not missing, 'no meaning for ' + ' '.join(missing)

    keep, kept_sentences = P.write_tables(OUT, char_rows, words, lists, word_sentence, own, trans,
                                          'code point, grade, level, radical, frequency, -, -, English, ' + ', '.join(P.LANGS))
    by = defaultdict(int)
    for c in chars:
        by[table[c][1]] += 1
    print('wrote', OUT, ':', len(chars), 'letters (levels %s),' % ', '.join('%d: %d' % kv for kv in sorted(by.items())),
          len(keep), 'words (', sum(1 for i in keep if words[i]['common']), 'common,', sum(1 for i in keep if i in word_sentence),
          'with a sentence ),', len(lists), 'lists,', len(kept_sentences), 'sentences')
    print('meanings in es/pt/fr/ja:', ', '.join('%s %d' % (l, sum(1 for i in keep if words[i]['in'][l])) for l in P.LANGS))

if __name__ == '__main__':
    main()
