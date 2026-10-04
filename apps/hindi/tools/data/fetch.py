#!/usr/bin/env python3
# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# Hindi's raw data: every source its data bake reads, downloaded into data/raw/
# (git-ignored), each from where docs/hindi_thai_data.md says, under the
# licence it says. Run from the project root; a file already there is kept
# (delete it to fetch it again).
#
#   python3 apps/hindi/tools/data/fetch.py
#
# A generic User-Agent: nothing personal goes with the requests.
#
# Already in data/raw/tatoeba/ from Kana, and read from there: the English,
# Spanish, French, Portuguese and Japanese Tatoeba sentences. The letters'
# strokes, devanagarivg.xml, are ours (apps/hindi/tools/strokes).

import os, sys, urllib.parse, urllib.request

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..'))
HI   = os.path.join(ROOT, 'data', 'raw', 'hi')
TAT  = os.path.join(ROOT, 'data', 'raw', 'tatoeba')
UA   = 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Safari/537.36'

def kaikki(edition, language):
    # A Wiktionary edition's Hindi entries, as wiktextract's JSON lines (kaikki.org).
    q = urllib.parse.quote(language)
    return 'https://kaikki.org/%s/%s/kaikki.org-dictionary-%s.jsonl' % (edition, q, q)

TATOEBA = 'https://downloads.tatoeba.org/exports/per_language/hin/'
SOURCES = [
    # Words, romanization, IPA, English (English Wiktionary, CC BY-SA 4.0), and
    # the inflected forms that lead to them. kaikki marks its per-language files
    # deprecated: the raw dump and wiktextract make the same file if this one goes.
    (HI,  'kaikki-en.jsonl',          kaikki('dictionary', 'Hindi')),
    # Japanese, French and Portuguese meanings (those Wiktionaries, CC BY-SA 4.0).
    (HI,  'kaikki-ja.jsonl',          kaikki('jawiktionary', 'ヒンディー語')),
    (HI,  'kaikki-fr.jsonl',          kaikki('frwiktionary', 'Hindi')),
    (HI,  'kaikki-pt.jsonl',          kaikki('ptwiktionary', 'Hindi')),
    # Word frequency (wordfreq's data, CC BY-SA 4.0): Hindi has only the small list.
    (HI,  'small_hi.msgpack.gz',      'https://github.com/rspeer/wordfreq/raw/master/wordfreq/data/small_hi.msgpack.gz'),
    # Example sentences (Tatoeba, CC BY 2.0 FR): the Hindi ones and their links.
    (TAT, 'hin_sentences.tsv.bz2',    TATOEBA + 'hin_sentences.tsv.bz2'),
    (TAT, 'hin-eng_links.tsv.bz2',    TATOEBA + 'hin-eng_links.tsv.bz2'),
    (TAT, 'hin-spa_links.tsv.bz2',    TATOEBA + 'hin-spa_links.tsv.bz2'),
    (TAT, 'hin-fra_links.tsv.bz2',    TATOEBA + 'hin-fra_links.tsv.bz2'),
    (TAT, 'hin-por_links.tsv.bz2',    TATOEBA + 'hin-por_links.tsv.bz2'),
    (TAT, 'hin-jpn_links.tsv.bz2',    TATOEBA + 'hin-jpn_links.tsv.bz2'),
]

def fetch(folder, name, url):
    path = os.path.join(folder, name)
    if os.path.exists(path) and os.path.getsize(path) > 0:
        print('have ', name)
        return True
    os.makedirs(folder, exist_ok=True)
    try:
        req = urllib.request.Request(url, headers={'User-Agent': UA})
        with urllib.request.urlopen(req, timeout=600) as r, open(path + '.part', 'wb') as f:
            while True:
                chunk = r.read(1 << 20)
                if not chunk:
                    break
                f.write(chunk)
        os.replace(path + '.part', path)
        print('got  ', name, os.path.getsize(path), 'bytes')
        return True
    except Exception as e:
        print('FAILED', name, url, e)
        return False

def main():
    ok = all([fetch(*s) for s in SOURCES])
    sys.exit(0 if ok else 1)

if __name__ == '__main__':
    main()
