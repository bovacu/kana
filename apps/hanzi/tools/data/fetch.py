#!/usr/bin/env python3
# Hanzi's raw data: every source its data bake reads, downloaded into data/raw/
# (git-ignored), each from where docs/chinese_korean_data.md says, under the
# licence it says. Run from the project root; a file already there is kept
# (delete it to fetch it again).
#
#   python3 apps/hanzi/tools/data/fetch.py
#
# A generic User-Agent: nothing personal goes with the requests.

import os, sys, urllib.request

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..'))
ZH   = os.path.join(ROOT, 'data', 'raw', 'zh')
TAT  = os.path.join(ROOT, 'data', 'raw', 'tatoeba')
HSK  = os.path.join(ZH, 'hsk2025')
UA   = 'hanzi-data/1.0'

TATOEBA = 'https://downloads.tatoeba.org/exports/per_language/cmn/'
SOURCES = [
    # Stroke order: Make Me a Hanzi's medians and outlines (Arphic Public License),
    # as hanzi-writer-data packs them (with its fixes for cut-off strokes).
    (ZH, 'hanzi-writer-data-2.0.1.tgz', 'https://registry.npmjs.org/hanzi-writer-data/-/hanzi-writer-data-2.0.1.tgz'),
    (ZH, 'ARPHICPL.TXT',                'https://raw.githubusercontent.com/skishore/makemeahanzi/master/APL/english/ARPHICPL.TXT'),
    # Taiwan's stroke order for HSK 1-3's traditional forms (AnimCJK, Arphic).
    (ZH, 'graphicsZhHant.txt',          'https://raw.githubusercontent.com/parsimonhi/animCJK/master/graphicsZhHant.txt'),
    # Characters: pinyin, meanings, strokes, radicals, variants (Unicode licence).
    (ZH, 'Unihan.zip',                  'https://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip'),
    # Words (CC BY-SA 4.0).
    (ZH, 'cedict_1_0_ts_utf-8_mdbg.txt.gz', 'https://www.mdbg.net/chinese/export/cedict/cedict_1_0_ts_utf-8_mdbg.txt.gz'),
    # Simplified <-> traditional (Apache 2.0).
    (ZH, 'STCharacters.txt',            'https://raw.githubusercontent.com/BYVoid/OpenCC/master/data/dictionary/STCharacters.txt'),
    (ZH, 'TSCharacters.txt',            'https://raw.githubusercontent.com/BYVoid/OpenCC/master/data/dictionary/TSCharacters.txt'),
    (ZH, 'STPhrases.txt',               'https://raw.githubusercontent.com/BYVoid/OpenCC/master/data/dictionary/STPhrases.txt'),
    (ZH, 'TSPhrases.txt',               'https://raw.githubusercontent.com/BYVoid/OpenCC/master/data/dictionary/TSPhrases.txt'),
    (ZH, 'TWVariants.txt',              'https://raw.githubusercontent.com/BYVoid/OpenCC/master/data/dictionary/TWVariants.txt'),
    # Components (BabelStone: free for any use).
    (ZH, 'IDS.TXT',                     'https://www.babelstone.co.uk/CJK/IDS.TXT'),
    # Word frequency (wordfreq's data, CC BY-SA 4.0, with SUBTLEX-CH's permission).
    (ZH, 'large_zh.msgpack.gz',         'https://github.com/rspeer/wordfreq/raw/master/wordfreq/data/large_zh.msgpack.gz'),
    # Example sentences (Tatoeba, CC BY 2.0 FR): the Mandarin ones and their links;
    # the other languages' sentences are Kana's (data/raw/tatoeba).
    (TAT, 'cmn_sentences.tsv.bz2',      TATOEBA + 'cmn_sentences.tsv.bz2'),
    (TAT, 'cmn-eng_links.tsv.bz2',      TATOEBA + 'cmn-eng_links.tsv.bz2'),
    (TAT, 'cmn-spa_links.tsv.bz2',      TATOEBA + 'cmn-spa_links.tsv.bz2'),
    (TAT, 'cmn-fra_links.tsv.bz2',      TATOEBA + 'cmn-fra_links.tsv.bz2'),
    (TAT, 'cmn-por_links.tsv.bz2',      TATOEBA + 'cmn-por_links.tsv.bz2'),
    (TAT, 'cmn-jpn_links.tsv.bz2',      TATOEBA + 'cmn-jpn_links.tsv.bz2'),
    # HSK's 2025 syllabus (levels 1-6, 7-9): words, characters to read, characters to
    # write, as krmanik/HSK-3.0 lists them (CC BY-SA 4.0); the bake keeps only each
    # one's level, checked against the official syllabus.
    (HSK, 'words_1.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Words/HSK_Level_1_words.txt'),
    (HSK, 'hanzi_1.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Hanzi/HSK_Level_1_hanzi.txt'),
    (HSK, 'words_2.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Words/HSK_Level_2_words.txt'),
    (HSK, 'hanzi_2.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Hanzi/HSK_Level_2_hanzi.txt'),
    (HSK, 'words_3.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Words/HSK_Level_3_words.txt'),
    (HSK, 'hanzi_3.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Hanzi/HSK_Level_3_hanzi.txt'),
    (HSK, 'words_4.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Words/HSK_Level_4_words.txt'),
    (HSK, 'hanzi_4.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Hanzi/HSK_Level_4_hanzi.txt'),
    (HSK, 'words_5.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Words/HSK_Level_5_words.txt'),
    (HSK, 'hanzi_5.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Hanzi/HSK_Level_5_hanzi.txt'),
    (HSK, 'words_6.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Words/HSK_Level_6_words.txt'),
    (HSK, 'hanzi_6.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Hanzi/HSK_Level_6_hanzi.txt'),
    (HSK, 'words_7-9.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Words/HSK_Level_7-9_words.txt'),
    (HSK, 'hanzi_7-9.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Hanzi/HSK_Level_7-9_hanzi.txt'),
    (HSK, 'handwritten_1-2.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Handwritten/HSK_Level_1-2_handwritten.txt'),
    (HSK, 'handwritten_3.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Handwritten/HSK_Level_3_handwritten.txt'),
    (HSK, 'handwritten_4.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Handwritten/HSK_Level_4_handwritten.txt'),
    (HSK, 'handwritten_5.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Handwritten/HSK_Level_5_handwritten.txt'),
    (HSK, 'handwritten_6.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Handwritten/HSK_Level_6_handwritten.txt'),
    (HSK, 'handwritten_7-9.txt', 'https://raw.githubusercontent.com/krmanik/HSK-3.0/main/New%20HSK%20(2025)/HSK%20Handwritten/HSK_Level_7-9_handwritten.txt'),
    # Fonts (OFL 1.1).
    (ZH, 'NotoSansSC-Regular.otf',      'https://github.com/notofonts/noto-cjk/raw/main/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf'),
    (ZH, 'NotoSansTC-Regular.otf',      'https://github.com/notofonts/noto-cjk/raw/main/Sans/SubsetOTF/TC/NotoSansTC-Regular.otf'),
    (ZH, 'LICENSE-NotoSansCJK.txt',     'https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/LICENSE'),
]

def fetch(folder, name, url):
    path = os.path.join(folder, name)
    if os.path.exists(path) and os.path.getsize(path) > 0:
        print('have ', name)
        return True
    os.makedirs(folder, exist_ok=True)
    try:
        req = urllib.request.Request(url, headers={'User-Agent': UA})
        with urllib.request.urlopen(req, timeout=120) as r, open(path + '.part', 'wb') as f:
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
