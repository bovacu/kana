#!/usr/bin/env python3
# Arabic's raw data: every source its data bake reads, downloaded into data/raw/
# (git-ignored), each from where docs/arabic_data.md says, under the
# licence it says. Run from the project root; a file already there is kept
# (delete it to fetch it again).
#
#   python3 apps/arabic/tools/data/fetch.py
#
# A generic User-Agent: nothing personal goes with the requests.
#
# Already in data/raw/tatoeba/ from Kana, and read from there: the English,
# Spanish, French, Portuguese and Japanese Tatoeba sentences. The letters'
# strokes, arabicvg.xml, are ours (apps/arabic/tools/strokes).

import os, sys, urllib.parse, urllib.request

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..'))
AR   = os.path.join(ROOT, 'data', 'raw', 'ar')
TAT  = os.path.join(ROOT, 'data', 'raw', 'tatoeba')
UA   = 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Safari/537.36'

def kaikki(edition, language):
    # A Wiktionary edition's Arabic entries, as wiktextract's JSON lines (kaikki.org).
    q = urllib.parse.quote(language)
    return 'https://kaikki.org/%s/%s/kaikki.org-dictionary-%s.jsonl' % (edition, q, q)

TATOEBA = 'https://downloads.tatoeba.org/exports/per_language/ara/'
SOURCES = [
    # Words, their vowelled forms, romanization, English (English Wiktionary,
    # CC BY-SA 4.0), and the inflected forms that lead to them. kaikki marks its
    # per-language files deprecated: the raw dump and wiktextract make the same
    # file if this one goes.
    (AR,  'kaikki-en.jsonl',          kaikki('dictionary', 'Arabic')),
    # Spanish, Japanese, French and Portuguese meanings (those Wiktionaries, CC BY-SA 4.0).
    (AR,  'kaikki-es.jsonl',          kaikki('eswiktionary', 'Árabe')),
    (AR,  'kaikki-ja.jsonl',          kaikki('jawiktionary', 'アラビア語')),
    (AR,  'kaikki-fr.jsonl',          kaikki('frwiktionary', 'Arabe')),
    (AR,  'kaikki-pt.jsonl',          kaikki('ptwiktionary', 'Árabe')),
    # Word frequency (wordfreq's data, CC BY-SA 4.0): Arabic has the large list.
    (AR,  'large_ar.msgpack.gz',      'https://github.com/rspeer/wordfreq/raw/master/wordfreq/data/large_ar.msgpack.gz'),
    # Example sentences (Tatoeba, CC BY 2.0 FR): the Arabic ones and their links.
    (TAT, 'ara_sentences.tsv.bz2',    TATOEBA + 'ara_sentences.tsv.bz2'),
    (TAT, 'ara-eng_links.tsv.bz2',    TATOEBA + 'ara-eng_links.tsv.bz2'),
    (TAT, 'ara-spa_links.tsv.bz2',    TATOEBA + 'ara-spa_links.tsv.bz2'),
    (TAT, 'ara-fra_links.tsv.bz2',    TATOEBA + 'ara-fra_links.tsv.bz2'),
    (TAT, 'ara-por_links.tsv.bz2',    TATOEBA + 'ara-por_links.tsv.bz2'),
    (TAT, 'ara-jpn_links.tsv.bz2',    TATOEBA + 'ara-jpn_links.tsv.bz2'),
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
