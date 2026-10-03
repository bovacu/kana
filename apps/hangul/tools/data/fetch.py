#!/usr/bin/env python3
# Hangul's raw data: every source its data bake reads, downloaded into data/raw/
# (git-ignored), each from where docs/chinese_korean_data.md says, under the
# licence it says. Run from the project root; a file already there is kept
# (delete it to fetch it again).
#
#   python3 apps/hangul/tools/data/fetch.py
#
# A generic User-Agent: nothing personal goes with the requests.
#
# Already in data/raw/ from Kana and Hanzi, and read from there: KanjiVG and
# KANJIDIC2 (Kana's fetch), Unihan (Hanzi's fetch.py), the English, Spanish,
# French and Portuguese Tatoeba sentences (Kana's). The jamo and syllable
# strokes, hangulvg.xml, are ours (apps/hangul/tools/strokes).

import os, sys, urllib.request, zipfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..'))
KO   = os.path.join(ROOT, 'data', 'raw', 'ko')
TAT  = os.path.join(ROOT, 'data', 'raw', 'tatoeba')
UA   = 'hangul-data/1.0'

TATOEBA = 'https://downloads.tatoeba.org/exports/per_language/kor/'
# krdict's own download page sends the file only to a request coming from it.
KRDICT_REFERER = 'https://krdict.korean.go.kr/download/downloadPopup'
SOURCES = [
    # Words: NIKL's Basic Korean Dictionary, 한국어기초사전 (CC BY-SA 2.0 KR), the
    # whole of it as JSON (seq 216 is the XML). Its sound and image links are
    # not ours to use and nothing reads them.
    (KO,  'krdict_json.zip',          'https://krdict.korean.go.kr/dicBatchDownload?seq=217'),
    # Hanja's Korean meaning words, 훈 ("하늘 천": 하늘), from libhangul (BSD-3-Clause,
    # in the file's own header; not the repository's LGPL).
    (KO,  'hanja.txt',                'https://raw.githubusercontent.com/libhangul/libhangul/main/data/hanja/hanja.txt'),
    # Word frequency (wordfreq's data, CC BY-SA 4.0). Korean has only the small list.
    (KO,  'small_ko.msgpack.gz',      'https://github.com/rspeer/wordfreq/raw/master/wordfreq/data/small_ko.msgpack.gz'),
    # Example sentences (Tatoeba, CC BY 2.0 FR): the Korean ones and their links;
    # the other languages' sentences are Kana's (data/raw/tatoeba).
    (TAT, 'kor_sentences.tsv.bz2',    TATOEBA + 'kor_sentences.tsv.bz2'),
    (TAT, 'kor-eng_links.tsv.bz2',    TATOEBA + 'kor-eng_links.tsv.bz2'),
    (TAT, 'kor-spa_links.tsv.bz2',    TATOEBA + 'kor-spa_links.tsv.bz2'),
    (TAT, 'kor-fra_links.tsv.bz2',    TATOEBA + 'kor-fra_links.tsv.bz2'),
    (TAT, 'kor-por_links.tsv.bz2',    TATOEBA + 'kor-por_links.tsv.bz2'),
    (TAT, 'kor-jpn_links.tsv.bz2',    TATOEBA + 'kor-jpn_links.tsv.bz2'),
    # Font (OFL 1.1), for the app.
    (KO,  'NotoSansKR-Regular.otf',   'https://github.com/notofonts/noto-cjk/raw/main/Sans/SubsetOTF/KR/NotoSansKR-Regular.otf'),
    (KO,  'LICENSE-NotoSansCJK.txt',  'https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/LICENSE'),
]

def fetch(folder, name, url):
    path = os.path.join(folder, name)
    if os.path.exists(path) and os.path.getsize(path) > 0:
        print('have ', name)
        return True
    os.makedirs(folder, exist_ok=True)
    try:
        headers = {'User-Agent': UA}
        if 'krdict.korean.go.kr' in url:
            headers['Referer'] = KRDICT_REFERER
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req, timeout=300) as r, open(path + '.part', 'wb') as f:
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

def unzip_krdict():
    # prepare.py reads the JSON files themselves (eleven, about 1 GB together).
    folder = os.path.join(KO, 'krdict')
    if os.path.isdir(folder) and any(n.endswith('.json') for n in os.listdir(folder)):
        print('have  krdict/')
        return True
    try:
        with zipfile.ZipFile(os.path.join(KO, 'krdict_json.zip')) as z:
            z.extractall(folder)
        print('unzipped krdict/')
        return True
    except Exception as e:
        print('FAILED to unzip krdict_json.zip', e)   # an HTML page instead of the zip: delete it, fetch again
        return False

def main():
    ok = all([fetch(*s) for s in SOURCES])
    ok = unzip_krdict() and ok
    sys.exit(0 if ok else 1)

if __name__ == '__main__':
    main()
