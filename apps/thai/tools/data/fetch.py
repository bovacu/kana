#!/usr/bin/env python3
# Thai's raw data: every source its data bake reads, downloaded into data/raw/
# (git-ignored), each from where docs/hindi_thai_data.md says, under the
# licence it says. Run from the project root; a file already there is kept
# (delete it to fetch it again).
#
#   python3 apps/thai/tools/data/fetch.py
#
# A generic User-Agent: nothing personal goes with the requests.
#
# Already in data/raw/tatoeba/ from Kana, and read from there: the English,
# Spanish, French, Portuguese and Japanese Tatoeba sentences. The letters'
# strokes, thaivg.xml, are ours (apps/thai/tools/strokes).

import os, sys, urllib.parse, urllib.request

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..'))
TH   = os.path.join(ROOT, 'data', 'raw', 'th')
TAT  = os.path.join(ROOT, 'data', 'raw', 'tatoeba')
UA   = 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Safari/537.36'

def kaikki(edition, language):
    # A Wiktionary edition's Thai entries, as wiktextract's JSON lines (kaikki.org).
    q = urllib.parse.quote(language)
    return 'https://kaikki.org/%s/%s/kaikki.org-dictionary-%s.jsonl' % (edition, q, q)

TATOEBA = 'https://downloads.tatoeba.org/exports/per_language/tha/'
SOURCES = [
    # Words, tone romanization, IPA, English (English Wiktionary, CC BY-SA 4.0).
    # kaikki marks its per-language files deprecated: the raw dump and
    # wiktextract make the same file if this one goes.
    (TH,  'kaikki-en.jsonl',          kaikki('dictionary', 'Thai')),
    # Japanese, French and Portuguese meanings (those Wiktionaries, CC BY-SA 4.0).
    (TH,  'kaikki-ja.jsonl',          kaikki('jawiktionary', 'タイ語')),
    (TH,  'kaikki-fr.jsonl',          kaikki('frwiktionary', 'Thaï')),
    (TH,  'kaikki-pt.jsonl',          kaikki('ptwiktionary', 'Tailandês')),
    # Volubilis Mundo, by Belisan (Francis Bastien), CC BY-SA 4.0 (stated on the
    # project's blog, not in the file: docs/research/thai_sources.md keeps it):
    # tones, levels, and English, French, Spanish, Portuguese meanings.
    (TH,  'VOLUBILIS-Mundo.xlsx',     'https://www.dropbox.com/scl/fi/mdph2ayjh2r8ll3ia85ox/VOLUBILIS-Mundo.xlsx?rlkey=piceye7nv3ig7cfkw26fjyj5g&dl=1'),
    # Word frequency: PyThaiNLP's Thai National Corpus list (CC0).
    (TH,  'tnc_freq.txt',             'https://raw.githubusercontent.com/PyThaiNLP/pythainlp/dev/pythainlp/corpus/tnc_freq.txt'),
    # Example sentences (Tatoeba, CC BY 2.0 FR): the Thai ones and their links.
    (TAT, 'tha_sentences.tsv.bz2',    TATOEBA + 'tha_sentences.tsv.bz2'),
    (TAT, 'tha-eng_links.tsv.bz2',    TATOEBA + 'tha-eng_links.tsv.bz2'),
    (TAT, 'tha-spa_links.tsv.bz2',    TATOEBA + 'tha-spa_links.tsv.bz2'),
    (TAT, 'tha-fra_links.tsv.bz2',    TATOEBA + 'tha-fra_links.tsv.bz2'),
    (TAT, 'tha-por_links.tsv.bz2',    TATOEBA + 'tha-por_links.tsv.bz2'),
    (TAT, 'tha-jpn_links.tsv.bz2',    TATOEBA + 'tha-jpn_links.tsv.bz2'),
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
