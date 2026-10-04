#!/usr/bin/env python3
# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# Hindi's data, prepared for the bake (study/chars/bake.h's PREPARED tables):
# data/raw/hi/prepared/{chars,words,lists,sentences}.tsv, from the sources
# fetch.py downloads. Run from the project root after fetch.py:
#
#   python3 -m pip install msgpack     # once (wordfreq's file)
#   python3 apps/hindi/tools/data/fetch.py
#   python3 apps/hindi/tools/data/prepare.py
#
# The strokes the bake reads, data/raw/hi/devanagarivg.xml, are ours
# (apps/hindi/tools/strokes). What each table holds, and from where:
#   chars      every letter of fude/lang/hi/lang.c's table, at the level it gives
#              it (read from there: one table for both): how common it is (in
#              wordfreq's words), and what a vowel sign or mark does (ि: the
#              vowel i, written before its consonant) in English, Spanish,
#              Portuguese, French and Japanese (ours).
#   words      English Wiktionary's Hindi words (kaikki.org, CC BY-SA 4.0), not
#              names, letters, affixes or inflected forms: their romanization
#              (Wiktionary's, which drops the silent a: कमल kamal), their meaning
#              in English (Wiktionary's), French, Portuguese and Japanese (those
#              Wiktionaries, where they have the word; none in Spanish yet); how
#              common (wordfreq, CC BY-SA 4.0): common when wordfreq counts it;
#              and a Tatoeba sentence with it, or one of its forms, in.
#   lists      each letter's words (those with it in; a letter with a dot, क़,
#              also its base's, क): the examples first (common, short, the
#              commonest first), then more to add.
#   sentences  the Tatoeba sentences kept (CC BY 2.0 FR: Hindi, then English,
#              Spanish, French, Portuguese), each a word's.
#
# Text is compared in Unicode's NFC, which writes क़ as क and its dot (U+093C):
# Wiktionary, Tatoeba and wordfreq meet there whichever way they typed it.

import gzip, math, os, re, sys, unicodedata
from collections import defaultdict

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..'))
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(ROOT, 'tools', 'data'))
import prepared as P

HI  = os.path.join(P.RAW, 'hi')
OUT = os.path.join(HI, 'prepared')

WORD_CHARS    = 16       # longer words are not kept (lang/hi/bake.c's word_chars: code points, signs too)
EXAMPLE_CHARS = 10
SENT_MIN, SENT_MAX, SENT_IDEAL = 8, 50, 24       # code points, spaces included
SKIP_POS = {'name', 'character', 'prefix', 'suffix', 'interfix', 'combining_form', 'infix', 'affix', 'symbol', 'punct',
            'romanization', 'syllable', 'abbrev', 'proverb'}
NUKTA = '़'
# A letter with a dot, as NFC writes it (its base and the dot) → its own code point.
NUKTA_LETTERS = {unicodedata.normalize('NFC', chr(c)): chr(c) for c in range(0x0958, 0x0960)}

def nfc(s):
    return unicodedata.normalize('NFC', s.replace('‌', '').replace('‍', ''))

def deva(c):
    return 'ऀ' <= c <= 'ॿ'

def all_deva(w):
    return bool(w) and all(deva(c) for c in w)

# --- the letters: lang.c's table ---------------------------------------------------------------

def letters():
    # lang.c's FUDE_HI_LETTERS rows: { 0x0905, FUDE_HI_VOWELS, 1, "a" }.
    src = open(os.path.join(ROOT, 'fude', 'lang', 'hi', 'lang.c'), encoding='utf-8').read()
    out = {}
    for m in re.finditer(r'\{\s*0x([0-9A-Fa-f]{4}),\s*FUDE_HI_(\w+),\s*(\d)\s*,', src):
        out[chr(int(m.group(1), 16))] = (m.group(2), int(m.group(3)))
    assert len(out) > 80, 'lang.c table: %d' % len(out)
    return out

# What a sign does, and the two vowels English words brought. English, Spanish,
# Portuguese, French, Japanese.
MEANING = {
    'ऍ': ('the vowel ê (in English words)', 'la vocal ê (en palabras inglesas)', 'a vogal ê (em palavras inglesas)', 'la voyelle ê (dans les mots anglais)', '母音 ê（英語からの語）'),
    'ऑ': ('the vowel ô (in English words)', 'la vocal ô (en palabras inglesas)', 'a vogal ô (em palavras inglesas)', 'la voyelle ô (dans les mots anglais)', '母音 ô（英語からの語）'),
    'ँ': ('makes the vowel nasal', 'nasaliza la vocal', 'nasaliza a vogal', 'nasalise la voyelle', '母音を鼻音にする'),
    'ं': ('a nasal sound (n, m)', 'un sonido nasal (n, m)', 'um som nasal (n, m)', 'un son nasal (n, m)', '鼻音（n、m）'),
    'ः': ('a breathed h after the vowel', 'una h aspirada tras la vocal', 'um h aspirado após a vogal', 'un h soufflé après la voyelle', '母音のあとの息の h'),
    '़': ('the dot for other languages’ sounds (क़ q, ज़ z, फ़ f)', 'el punto de los sonidos de otras lenguas (क़ q, ज़ z, फ़ f)', 'o ponto dos sons de outras línguas (क़ q, ज़ z, फ़ f)', 'le point des sons d’autres langues (क़ q, ज़ z, फ़ f)', '外来の音の点（क़ q、ज़ z、फ़ f）'),
    'ऽ': ('a vowel left out or drawn out', 'una vocal omitida o alargada', 'uma vogal omitida ou alongada', 'une voyelle omise ou allongée', '省かれた・のばした母音'),
    'ा': ('vowel sign ā, after the consonant', 'signo vocálico ā, tras la consonante', 'sinal vocálico ā, depois da consoante', 'signe voyelle ā, après la consonne', '母音記号 ā（子音のあと）'),
    'ि': ('vowel sign i, written before the consonant', 'signo vocálico i, escrito antes de la consonante', 'sinal vocálico i, escrito antes da consoante', 'signe voyelle i, écrit avant la consonne', '母音記号 i（子音の前に書く）'),
    'ी': ('vowel sign ī, after the consonant', 'signo vocálico ī, tras la consonante', 'sinal vocálico ī, depois da consoante', 'signe voyelle ī, après la consonne', '母音記号 ī（子音のあと）'),
    'ु': ('vowel sign u, below the consonant', 'signo vocálico u, bajo la consonante', 'sinal vocálico u, sob a consoante', 'signe voyelle u, sous la consonne', '母音記号 u（子音の下）'),
    'ू': ('vowel sign ū, below the consonant', 'signo vocálico ū, bajo la consonante', 'sinal vocálico ū, sob a consoante', 'signe voyelle ū, sous la consonne', '母音記号 ū（子音の下）'),
    'ृ': ('vowel sign ṛ (ri), below the consonant', 'signo vocálico ṛ (ri), bajo la consonante', 'sinal vocálico ṛ (ri), sob a consoante', 'signe voyelle ṛ (ri), sous la consonne', '母音記号 ṛ（ri、子音の下）'),
    'ॅ': ('vowel sign ê (in English words)', 'signo vocálico ê (en palabras inglesas)', 'sinal vocálico ê (em palavras inglesas)', 'signe voyelle ê (dans les mots anglais)', '母音記号 ê（英語からの語）'),
    'े': ('vowel sign e, above the consonant', 'signo vocálico e, sobre la consonante', 'sinal vocálico e, sobre a consoante', 'signe voyelle e, sur la consonne', '母音記号 e（子音の上）'),
    'ै': ('vowel sign ai, above the consonant', 'signo vocálico ai, sobre la consonante', 'sinal vocálico ai, sobre a consoante', 'signe voyelle ai, sur la consonne', '母音記号 ai（子音の上）'),
    'ॉ': ('vowel sign ô (in English words)', 'signo vocálico ô (en palabras inglesas)', 'sinal vocálico ô (em palavras inglesas)', 'signe voyelle ô (dans les mots anglais)', '母音記号 ô（英語からの語）'),
    'ो': ('vowel sign o, after the consonant', 'signo vocálico o, tras la consonante', 'sinal vocálico o, depois da consoante', 'signe voyelle o, après la consonne', '母音記号 o（子音のあと）'),
    'ौ': ('vowel sign au, after the consonant', 'signo vocálico au, tras la consonante', 'sinal vocálico au, depois da consoante', 'signe voyelle au, après la consonne', '母音記号 au（子音のあと）'),
    '्': ('takes away the consonant’s a', 'quita la a de la consonante', 'tira o a da consoante', 'ôte le a de la consonne', '子音の a を消す'),
    'ॐ': ('om, the sacred syllable', 'om, la sílaba sagrada', 'om, a sílaba sagrada', 'om, la syllabe sacrée', 'オーム（聖音）'),
    '।': ('full stop', 'punto final', 'ponto final', 'point final', '句点'),
    '॥': ('end of a verse', 'fin de un verso', 'fim de um verso', 'fin d’un verset', '詩節の終わり'),
}

def read_wordfreq():
    # wordfreq's buckets: the i-th holds the words of zipf 9 - i/100.
    import msgpack
    data = msgpack.unpackb(gzip.open(os.path.join(HI, 'small_hi.msgpack.gz')).read(), raw=False)
    zipf = {}
    for i, bucket in enumerate(data[1:], start=1):
        for w in bucket:
            zipf.setdefault(nfc(w), 9.0 - i / 100.0)
    return zipf

def listed_in(w):
    # The letters a word lists under: its code points, and a letter with a dot's own.
    out = set(w)
    for pair, letter in NUKTA_LETTERS.items():
        if pair in w:
            out.add(letter)
    return out

def main():
    print('reading the sources...')
    table = letters()
    chars = sorted(table)
    zipf = read_wordfreq()
    ja = P.native_meanings(os.path.join(HI, 'kaikki-ja.jsonl'), 'ja', lambda w: all_deva(nfc(w)))
    fr = P.native_meanings(os.path.join(HI, 'kaikki-fr.jsonl'), 'fr', lambda w: all_deva(nfc(w)))
    pt = P.native_meanings(os.path.join(HI, 'kaikki-pt.jsonl'), 'pt', lambda w: all_deva(nfc(w)))
    ja = {nfc(w): m for w, m in ja.items()}
    fr = {nfc(w): m for w, m in fr.items()}
    pt = {nfc(w): m for w, m in pt.items()}

    # Words: Wiktionary's lemmas, a written form and reading once, its entries'
    # senses in order; and every inflected form → its lemmas (लड़के → लड़का).
    entries = defaultdict(lambda: {'senses': [], 'pos': []})
    order = []
    forms = defaultdict(set)
    for e in P.read_kaikki(os.path.join(HI, 'kaikki-en.jsonl')):
        w = nfc(e.get('word', ''))
        if not all_deva(w):
            continue
        for s in e.get('senses', []):
            for f in s.get('form_of', []) + s.get('alt_of', []):
                lemma = nfc(f.get('word', ''))
                if all_deva(lemma) and lemma != w:
                    forms[w].add(lemma)
        for f in e.get('forms', []):
            form = nfc(f.get('form', ''))
            if all_deva(form) and form != w and 'romanization' not in f.get('tags', []):
                forms[form].add(w)
        if e.get('pos') in SKIP_POS or len(w) > WORD_CHARS:
            continue
        roman = next((f.get('form', '') for f in e.get('forms', []) if 'romanization' in f.get('tags', []) and f.get('form')), '')
        roman = P.clean(roman.split(',')[0])
        senses = [P.english_glosses(s) for s in e.get('senses', []) if P.sense_usable(s)]
        senses = [s for s in senses if s]
        if not roman or not senses or len(roman.encode('utf-8')) > P.READING_BYTES:
            continue
        key = (w, roman)
        if key not in entries:
            order.append(key)
        entries[key]['senses'] += senses
        entries[key]['pos'].append(e.get('pos'))
    words = []
    for (w, roman) in order:
        english = P.join_senses(entries[(w, roman)]['senses'])
        if not english:
            continue
        z = zipf.get(w, 0.0) or zipf.get(w.replace(NUKTA, ''), 0.0)
        words.append({'written': w, 'reading': roman, 'meaning': english,
                      'in': {'es': '', 'pt': pt.get(w, ''), 'fr': fr.get(w, ''), 'ja': ja.get(w, '')},
                      'zipf': z, 'freq': P.zipf_freq(z, 1060), 'level': 0, 'common': z >= 3.0, 'chars': len(w),
                      'pos': entries[(w, roman)]['pos'], 'keywords': P.keywords_of(english)})
    by_written = defaultdict(list)
    for i, w in enumerate(words):
        by_written[w['written']].append(i)
    print(len(words), 'words,', sum(1 for w in words if w['common']), 'common;', len(forms), 'inflected forms')

    # Sentences: a word of text is a word, or a form of one (गया: जाना); a word
    # spelt alike told apart by the English translation (or the sentence goes to
    # none of them).
    own, trans = P.read_tatoeba('hin')
    own = {sid: nfc(t) for sid, t in own.items()}
    found = defaultdict(set)
    for sid, t in own.items():
        t = P.clean(t)
        if not (SENT_MIN <= len(t) <= SENT_MAX) or not trans['en'].get(sid) or t.startswith('#'):
            continue
        english = re.findall(r'[a-z]+', trans['en'][sid].lower())
        for token in re.findall(r'[ऀ-ॣ०-ॿ]+', t):
            ids = [i for i in by_written.get(token, []) if words[i]['common']]
            if not ids:
                ids = [i for lemma in sorted(forms.get(token, ())) for i in by_written.get(lemma, []) if words[i]['common']]
            if len(ids) > 1:
                told = [i for i in ids if P.mentions(words[i]['keywords'], english)]
                ids = told if len(told) == 1 else []
            for i in ids:
                found[i].add(sid)
    word_sentence = P.assign_sentences(words, found, own, trans, SENT_IDEAL)
    print(len(word_sentence), 'words with a sentence')

    def score(i, c):
        w = words[i]
        s = w['freq'] / 2.0
        s += 0 if w['common'] else 5000
        s += 20 * max(0, w['chars'] - 6)
        s += 0 if i in word_sentence else 30
        return s
    lists = P.build_lists(words, set(chars), EXAMPLE_CHARS, score, lambda w: listed_in(w['written']))

    # How common each letter is: in wordfreq's words, by how common they are.
    usage = defaultdict(float)
    for w, z in zipf.items():
        for c in listed_in(w):
            if c in table:
                usage[c] += 10 ** (z - 3.0)
    ranked = sorted(chars, key=lambda c: (-usage.get(c, 0.0), c))
    rank = {c: k + 1 for k, c in enumerate(ranked)}

    char_rows = []
    for c in chars:
        group, level = table[c]
        m = MEANING.get(c, ('', '', '', '', ''))
        char_rows.append(['%x' % ord(c), 0, level, 0, rank[c], '', '', m[0], m[1], m[2], m[3], m[4]])

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
