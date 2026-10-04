#!/usr/bin/env python3
# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# Hangul's data, prepared for the bake (study/chars/bake.h's PREPARED tables):
# data/raw/ko/prepared/{chars,words,lists,sentences}.tsv, and the strokes the
# bake reads, data/raw/ko/koreanvg.xml, from the sources fetch.py downloads.
# Run from the project root after fetch.py and the strokes' compose.py:
#
#   python3 -m pip install msgpack     # once (wordfreq's file)
#   python3 apps/hangul/tools/data/prepare.py
#
# Also writes fude/lang/ko/syllables.h, the syllables to learn (lang.c's
# Syllables group): the rest of the 11,172 are written, but listed nowhere.
#
# What each table holds, and from where:
#   koreanvg.xml  every jamo and syllable of hangulvg.xml (ours) and, from KanjiVG
#              (CC BY-SA 3.0, unchanged), the hanja to learn: the 1,800 education
#              hanja (Unihan's kKoreanEducationHanja) and every hanja in the origin
#              of a word krdict gives a level, where KanjiVG has it.
#   chars      every character of koreanvg.xml: its level (krdict's 초급 1, 중급 2,
#              고급 3: the easiest of the words it is in; for a hanja, the words
#              whose origin it is in, else 3 for an education hanja; the 40 basic
#              jamo 1), its radical (a hanja's, Unihan), how common it is (jamo
#              first; syllables by how often Tatoeba's and krdict's example
#              sentences have them, hanja by how common their words are); a
#              hanja's sounds, 음 (Unihan's kHangul, in hangul), its meaning words,
#              훈 (libhangul's hanja.txt, BSD-3: "하늘 천" → 하늘), its English
#              meaning (Unihan's kDefinition, else KANJIDIC2's), and its Spanish,
#              Portuguese and French ones where KANJIDIC2 has the character
#              (CC BY-SA 4.0).
#   words      krdict's words (NIKL 한국어기초사전, CC BY-SA 2.0 KR; none of its
#              sound or image links) of 1-5 syllables, all hangul, not particles,
#              endings or affixes: their reading in Revised Romanization (of
#              krdict's standard pronunciation, by our own code), their meaning in
#              English, Spanish, French and Japanese (krdict's; none in
#              Portuguese), how common (wordfreq, CC BY-SA 4.0), common when
#              krdict gives a level or frequent, and a Tatoeba sentence with them
#              in.
#   lists      each syllable's words (those with it in) and each hanja's (those
#              whose origin has it: 價 → 가격): the examples first (common, short,
#              the easiest level and the commonest first), then more to add.
#   sentences  the Tatoeba sentences kept (CC BY 2.0 FR: Korean, then English,
#              Spanish, French, Portuguese), each a word's.

import bz2, glob, gzip, html, json, os, re, unicodedata, zipfile
from collections import defaultdict

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..'))
RAW  = os.path.join(ROOT, 'data', 'raw')
KO   = os.path.join(RAW, 'ko')
OUT  = os.path.join(KO, 'prepared')
LANGS = ['es', 'pt', 'fr', 'ja']                 # the bake's other languages, in order (4 at most)

WORD_CHARS    = 5        # longer words are not kept (bake.h)
EXAMPLE_CHARS = 4
EXAMPLES      = 6
WORDS_ALL     = 20
MEANING_CHARS = 48       # glosses after the first while the meaning stays this short
MEANING_LONG  = 64       # a first gloss longer than this is cut at its first clause when it can be
MEANING_MOST  = 96       # ...and one longer than this, where a phrase ends
MEANING_BYTES = 127      # the bake keeps a word's meanings in 128 bytes (UTF-8, NUL included)
HUN_MOST      = 4        # a hanja's meaning words, at most
SENT_MIN, SENT_MAX, SENT_IDEAL = 6, 36, 16       # characters, spaces included

LEVELS   = {'초급': 1, '중급': 2, '고급': 3}
NOUNS    = {'명사', '의존 명사', '대명사', '수사'}            # 체언: RR keeps their ㅎ (묵호 Mukho)
VERBS    = {'동사', '형용사', '보조 동사', '보조 형용사'}      # lemmas ending in 다; text has them conjugated
GRAMMAR  = {'조사', '어미', '접사'}                          # not words to learn
LEVEL_FREQ = {1: 520, 2: 600, 3: 700}            # how common, for a word wordfreq can't tell: by its level
# How an ending starts, after a verb's stem in text (먹고, 먹어요, 가세요, 하겠다):
# a stem followed by anything else is another word (가방, not 가다).
ENDING_START = set('다고지게는니으은을음어아었았였여겠습기던도자네나냐거군구며면서세시셔셨십신실려러라야요오죠잖든')
# --- hangul ----------------------------------------------------------------------------

INITIALS = 'ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ'
VOWELS   = 'ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ'
FINALS   = ['', 'ㄱ', 'ㄲ', 'ㄳ', 'ㄴ', 'ㄵ', 'ㄶ', 'ㄷ', 'ㄹ', 'ㄺ', 'ㄻ', 'ㄼ', 'ㄽ', 'ㄾ', 'ㄿ', 'ㅀ', 'ㅁ', 'ㅂ', 'ㅄ', 'ㅅ', 'ㅆ', 'ㅇ', 'ㅈ', 'ㅊ', 'ㅋ', 'ㅌ', 'ㅍ', 'ㅎ']

# The 40 jamo of today's alphabet (14 consonants, 5 doubles, 10 vowels, 11
# compound vowels): level 1. The other 11, final clusters, are only parts.
BASIC_JAMO = set('ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ' 'ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ')
JAMO_ORDER = 'ㄱㄴㄷㄹㅁㅂㅅㅇㅈㅊㅋㅌㅍㅎ' 'ㄲㄸㅃㅆㅉ' 'ㅏㅑㅓㅕㅗㅛㅜㅠㅡㅣ' 'ㅐㅒㅔㅖㅘㅙㅚㅝㅞㅟㅢ'   # as taught (fude/lang/ko/chart.c)

def is_syllable(c):
    return '가' <= c <= '힣'

def is_hanja(c):
    o = ord(c)
    return 0x3400 <= o <= 0x9fff or 0x20000 <= o <= 0x3134f

def split_syllable(c):
    n = ord(c) - 0xac00
    return [INITIALS[n // 588], VOWELS[n % 588 // 28], FINALS[n % 28]]

def join_syllable(l, v, t=''):
    return chr(0xac00 + (INITIALS.index(l) * 21 + VOWELS.index(v)) * 28 + FINALS.index(t))

# --- Revised Romanization -----------------------------------------------------------------
#
# 국어의 로마자 표기법 (문화체육관광부 고시 제2014-42호): the sounds romanized as the
# standard pronunciation says them, but tensing not written (압구정 Apgujeong),
# vowels as written (광희문 Gwanghuimun), a noun's ㄱ ㄷ ㅂ before ㅎ kept apart
# (묵호 Mukho). Lowercase, without the optional hyphens (중앙 jungang).

RR_INITIAL = dict(zip(INITIALS, ['g', 'kk', 'n', 'd', 'tt', 'r', 'm', 'b', 'pp', 's', 'ss', '', 'j', 'jj', 'ch', 'k', 't', 'p', 'h']))
RR_VOWEL   = dict(zip(VOWELS, ['a', 'ae', 'ya', 'yae', 'eo', 'e', 'yeo', 'ye', 'o', 'wa', 'wae', 'oe', 'yo', 'u', 'wo', 'we', 'wi', 'yu', 'eu', 'ui', 'i']))
RR_FINAL   = {'': '', 'ㄱ': 'k', 'ㄴ': 'n', 'ㄷ': 't', 'ㄹ': 'l', 'ㅁ': 'm', 'ㅂ': 'p', 'ㅇ': 'ng'}
# A final as said before a consonant or at the end: one of seven.
NEUTRAL  = {'ㄲ': 'ㄱ', 'ㅋ': 'ㄱ', 'ㄳ': 'ㄱ', 'ㄺ': 'ㄱ', 'ㅅ': 'ㄷ', 'ㅆ': 'ㄷ', 'ㅈ': 'ㄷ', 'ㅊ': 'ㄷ', 'ㅌ': 'ㄷ', 'ㅎ': 'ㄷ',
            'ㅍ': 'ㅂ', 'ㄿ': 'ㅂ', 'ㅄ': 'ㅂ', 'ㄵ': 'ㄴ', 'ㄶ': 'ㄴ', 'ㄻ': 'ㅁ', 'ㄼ': 'ㄹ', 'ㄽ': 'ㄹ', 'ㄾ': 'ㄹ', 'ㅀ': 'ㄹ'}
CLUSTER  = {'ㄳ': ('ㄱ', 'ㅅ'), 'ㄵ': ('ㄴ', 'ㅈ'), 'ㄶ': ('ㄴ', 'ㅎ'), 'ㄺ': ('ㄹ', 'ㄱ'), 'ㄻ': ('ㄹ', 'ㅁ'), 'ㄼ': ('ㄹ', 'ㅂ'),
            'ㄽ': ('ㄹ', 'ㅅ'), 'ㄾ': ('ㄹ', 'ㅌ'), 'ㄿ': ('ㄹ', 'ㅍ'), 'ㅀ': ('ㄹ', 'ㅎ'), 'ㅄ': ('ㅂ', 'ㅅ')}
ASPIRATE = {'ㄱ': 'ㅋ', 'ㄷ': 'ㅌ', 'ㅂ': 'ㅍ', 'ㅈ': 'ㅊ'}
TENSE    = {'ㄲ': 'ㄱ', 'ㄸ': 'ㄷ', 'ㅃ': 'ㅂ', 'ㅆ': 'ㅅ', 'ㅉ': 'ㅈ'}
NASAL    = {'ㄱ': 'ㅇ', 'ㄷ': 'ㄴ', 'ㅂ': 'ㅁ'}

def sounds(written, noun=False):
    # The standard pronunciation's main rules, for a word krdict gives none:
    # aspiration, palatalisation, liaison, the seven finals, nasalisation and
    # the ㄹ rules. Not the word-by-word ones (ㄴ insertion: 알약 [알략]; 신문로 [신문노]).
    s = [split_syllable(c) for c in written]
    for a, b in zip(s, s[1:]):
        if a[2] in ('ㅎ', 'ㄶ', 'ㅀ'):
            rest = {'ㅎ': '', 'ㄶ': 'ㄴ', 'ㅀ': 'ㄹ'}[a[2]]
            if b[0] in ASPIRATE:                     # 좋고 [조코], 많다 [만타]
                b[0], a[2] = ASPIRATE[b[0]], rest
            elif b[0] in ('ㅇ', 'ㅅ'):               # 좋아 [조아], 좋소 [조쏘]
                a[2] = rest
            elif b[0] == 'ㄴ':                       # 놓는 [논는]
                a[2] = rest or 'ㄴ'
        elif b[0] == 'ㅎ' and a[2] not in ('', 'ㄴ', 'ㄹ', 'ㅁ', 'ㅇ', 'ㄻ'):
            first, last = {'ㄳ': ('', 'ㄱ'), 'ㅄ': ('', 'ㅂ')}.get(a[2]) or CLUSTER.get(a[2], ('', a[2]))   # 값하다 [가파다]
            last = 'ㅈ' if last in ('ㅈ', 'ㅊ') else NEUTRAL.get(last, last)
            if noun and a[2] in ('ㄱ', 'ㄷ', 'ㅂ'):
                pass                                 # 묵호 Mukho: kept apart
            elif last in ASPIRATE:                   # 잡혀 [자펴], 굳히다 [구치다]
                b[0] = 'ㅊ' if last == 'ㄷ' and b[1] == 'ㅣ' else ASPIRATE[last]
                a[2] = first
        if b[0] == 'ㅇ' and b[1] == 'ㅣ' and a[2] in ('ㄷ', 'ㅌ', 'ㄾ'):   # 해돋이 [해도지], 같이 [가치]
            b[0] = 'ㅈ' if a[2] == 'ㄷ' else 'ㅊ'
            a[2] = 'ㄹ' if a[2] == 'ㄾ' else ''
        if b[0] == 'ㅇ' and a[2] not in ('', 'ㅇ'):  # 음악 [으막], 읽어 [일거]
            a[2], b[0] = CLUSTER.get(a[2], ('', a[2]))
            if b[0] == 'ㅎ':
                b[0] = 'ㅇ'
    for i, a in enumerate(s):
        nxt = s[i + 1][0] if i + 1 < len(s) else ''
        if a[2] == 'ㄺ' and nxt == 'ㄱ':
            a[2] = 'ㄹ'                              # 읽고 [일꼬]
        elif a[2] == 'ㄼ' and written[i] == '밟':
            a[2] = 'ㅂ'                              # 밟다 [밥따]
        a[2] = NEUTRAL.get(a[2], a[2])
    for a, b in zip(s, s[1:]):
        if b[0] == 'ㄹ':
            if a[2] == 'ㄴ':
                a[2] = 'ㄹ'                          # 신라 [실라]
            elif a[2] in ('ㅁ', 'ㅇ', 'ㄱ', 'ㄷ', 'ㅂ'):
                b[0] = 'ㄴ'                          # 종로 [종노], 백로 [뱅노]
        elif b[0] == 'ㄴ' and a[2] == 'ㄹ':
            b[0] = 'ㄹ'                              # 별내 [별래]
        if b[0] in ('ㄴ', 'ㅁ') and a[2] in NASAL:
            a[2] = NASAL[a[2]]                       # 백마 [뱅마]
    return s

def clean_pronunciation(p):
    # krdict's: long vowels marked (조ː타), sometimes two (되다/뒈다), a space.
    p = re.sub(r'[ː:\s\u00a0]', '', (p or '').split('/')[0])
    return p if p and all(is_syllable(c) for c in p) else ''

def romanize(written, pronunciation='', noun=False):
    w = [split_syllable(c) for c in written]
    said = [split_syllable(c) for c in clean_pronunciation(pronunciation)]
    if len(said) != len(w):
        said = sounds(written, noun)                 # none, or one krdict cut short (관리자 [괄리])
    for a, b in zip(said, said[1:]):
        if b[0] == 'ㅇ' and a[2] not in ('', 'ㅇ'):  # one written unlinked (화풀이하다 [화ː풀이하다])
            a[2], b[0] = CLUSTER.get(a[2], ('', a[2]))
    out = [list(x) for x in said]
    for i, (x, y) in enumerate(zip(said, w)):
        moved = CLUSTER.get(w[i - 1][2], ('', w[i - 1][2]))[1] if i > 0 and y[0] == 'ㅇ' else ''
        if x[0] in TENSE and (y[0] == TENSE[x[0]] or moved == TENSE[x[0]]):
            out[i][0] = TENSE[x[0]]                  # 학교 [학꾜] hakgyo; 없이 [업씨] eopsi
        out[i][1] = y[1]                             # 의사, 시계: as written
        nxt = said[i + 1] if i + 1 < len(said) else None
        if nxt and y[2] == 'ㅅ' and x[2] == '' and nxt[0] in TENSE:
            out[i][2] = 'ㄷ'                         # 샛별 [새뼐] saetbyeol
        if nxt and noun and y[2] in ('ㄱ', 'ㄷ', 'ㅂ') and w[i + 1][0] == 'ㅎ' and x[2] == '' and nxt[0] == ASPIRATE[y[2]]:
            out[i][2], out[i + 1][0] = y[2], 'ㅎ'       # 묵호 [무코] mukho
    rr, prev = [], ''
    for l, v, t in out:
        rr.append('l' if l == 'ㄹ' and prev == 'ㄹ' else RR_INITIAL[l])
        t = NEUTRAL.get(t, t)
        rr.append(RR_VOWEL[v] + RR_FINAL[t])
        prev = t
    return ''.join(rr)

def test_romanize():
    # The notice's own examples with their standard pronunciation (the notice's
    # where it gives one), then krdict's words.
    said = [('구미', '', 'gumi'), ('영동', '', 'yeongdong'), ('백암', '배감', 'baegam'), ('옥천', '', 'okcheon'), ('합덕', '합떡', 'hapdeok'),
            ('호법', '', 'hobeop'), ('월곶', '월곧', 'wolgot'), ('벚꽃', '벋꼳', 'beotkkot'), ('한밭', '한받', 'hanbat'), ('구리', '', 'guri'),
            ('설악', '서락', 'seorak'), ('칠곡', '', 'chilgok'), ('임실', '', 'imsil'), ('울릉', '', 'ulleung'), ('대관령', '대괄령', 'daegwallyeong'),
            ('백마', '뱅마', 'baengma'), ('신문로', '신문노', 'sinmunno'), ('종로', '종노', 'jongno'), ('왕십리', '왕심니', 'wangsimni'),
            ('별내', '별래', 'byeollae'), ('신라', '실라', 'silla'), ('학여울', '항녀울', 'hangnyeoul'), ('알약', '알략', 'allyak'),
            ('해돋이', '해도지', 'haedoji'), ('같이', '가치', 'gachi'), ('굳히다', '구치다', 'guchida'), ('좋고', '조코', 'joko'),
            ('놓다', '노타', 'nota'), ('잡혀', '자펴', 'japyeo'), ('낳지', '나치', 'nachi'), ('압구정', '압꾸정', 'apgujeong'),
            ('낙동강', '낙똥강', 'nakdonggang'), ('죽변', '죽뼌', 'jukbyeon'), ('낙성대', '낙썽대', 'nakseongdae'), ('합정', '합쩡', 'hapjeong'),
            ('팔당', '팔땅', 'paldang'), ('샛별', '새뼐', 'saetbyeol'), ('울산', '울싼', 'ulsan'), ('광희문', '광히문', 'gwanghuimun'),
            ('중앙', '', 'jungang'), ('반구대', '', 'bangudae'), ('세운', '', 'seun'), ('해운대', '', 'haeundae'), ('남산', '', 'namsan'),
            ('속리산', '송니산', 'songnisan'), ('금강', '', 'geumgang'), ('독도', '독또', 'dokdo'), ('경복궁', '경복꿍', 'gyeongbokgung'),
            ('무량수전', '', 'muryangsujeon'), ('연화교', '', 'yeonhwagyo'), ('극락전', '긍낙쩐', 'geungnakjeon'), ('안압지', '아납찌', 'anapji'),
            ('남한산성', '', 'namhansanseong'), ('화랑대', '', 'hwarangdae'), ('불국사', '불국싸', 'bulguksa'), ('현충사', '', 'hyeonchungsa'),
            ('독립문', '동님문', 'dongnimmun'), ('촉석루', '촉썽누', 'chokseongnu'), ('종묘', '', 'jongmyo'), ('다보탑', '', 'dabotap'),
            ('의정부', '', 'uijeongbu'),
            ('읽다', '익따', 'ikda'), ('학교', '학꾜', 'hakgyo'), ('먹다', '먹따', 'meokda'), ('사랑하다', '사랑하다', 'saranghada'),
            ('좋다', '조ː타', 'jota'), ('되다', '되다/뒈다', 'doeda'), ('가격', '가격', 'gagyeok'), ('없이', '업ː씨', 'eopsi'),
            ('값하다', '가파다', 'gapada'), ('짓밟다', '짇빱따', 'jitbapda'), ('있어', '이써', 'isseo')]
    nouns = [('묵호', '무코', 'mukho'), ('집현전', '지편전', 'jiphyeonjeon'), ('오죽헌', '오주컨', 'ojukheon')]
    for written, p, rr in said:
        assert romanize(written, p or written) == rr, (written, p, romanize(written, p or written), rr)
    for written, p, rr in nouns:
        assert romanize(written, p, noun=True) == rr, (written, p, romanize(written, p, noun=True), rr)
    assert romanize('관리자', '괄리') == 'gwallija' and romanize('화풀이하다', '화ː풀이하다') == 'hwapurihada'
    # Without the pronunciation: the same, but for the word-by-word ones.
    for written, _, rr in said + nouns:
        noun = (written, _, rr) in nouns
        if written not in ('신문로', '학여울', '알약'):
            assert romanize(written, '', noun) == rr, (written, romanize(written, '', noun), rr)

# --- sources -------------------------------------------------------------------------

def kanji_blocks(path):
    # Each <kanji> of a KanjiVG-form file, as it is, by code point; variants
    # (an id with '-') left out, as the bake does.
    out = {}
    text = open(path, encoding='utf-8').read()
    for m in re.finditer(r'<kanji id="kvg:kanji_([0-9a-f]+)">.*?</kanji>\n?', text, re.S):
        out.setdefault(chr(int(m.group(1), 16)), m.group(0).rstrip('\n') + '\n')
    return out

def read_unihan():
    info, education = defaultdict(dict), set()
    wanted = {'kHangul', 'kDefinition', 'kRSUnicode', 'kKoreanEducationHanja'}
    with zipfile.ZipFile(os.path.join(RAW, 'zh', 'Unihan.zip')) as z:
        for name in ('Unihan_Readings.txt', 'Unihan_IRGSources.txt', 'Unihan_OtherMappings.txt'):
            for line in z.read(name).decode('utf-8').splitlines():
                if line.startswith('#') or not line.strip():
                    continue
                cp, field, value = line.split('\t', 2)
                if field in wanted:
                    c = chr(int(cp[2:], 16))
                    info[c][field] = value
                    if field == 'kKoreanEducationHanja':
                        education.add(c)
    return info, education

def read_libhangul():
    # hanja.txt's single characters: "천:天:하늘 천" (a sound, the character, its
    # meaning words each followed by the sound). Each gloss by the sound it ends in.
    hun = defaultdict(lambda: defaultdict(list))
    with open(os.path.join(KO, 'hanja.txt'), encoding='utf-8') as f:
        for line in f:
            if line.startswith('#'):
                continue
            p = line.rstrip('\n').split(':')
            if len(p) < 3 or len(p[1]) != 1:
                continue
            for gloss in p[2].split(','):
                words = gloss.split()
                if len(words) >= 2 and len(words[-1]) == 1 and is_syllable(words[-1]):
                    meaning = ' '.join(words[:-1])
                    if meaning not in hun[p[1]][words[-1]]:
                        hun[p[1]][words[-1]].append(meaning)
    return hun

def read_kanjidic():
    # Meanings in English, Spanish, Portuguese and French by character (KANJIDIC2's
    # m_lang), and its Korean readings for the few Unihan has not.
    import xml.etree.ElementTree as ET
    out = {}
    for _, el in ET.iterparse(os.path.join(RAW, 'kanjidic2.xml')):
        if el.tag != 'character':
            continue
        lit = el.findtext('literal')
        ms = defaultdict(list)
        for m in el.iter('meaning'):
            lang = m.get('m_lang', 'en')
            if (lang == 'en' or lang in LANGS) and m.text:
                ms[lang].append(m.text)
        if lit and ms:
            out[lit] = {l: ', '.join(v) for l, v in ms.items()}
        korean = [r.text for r in el.iter('reading') if r.get('r_type') == 'korean_h' and r.text]
        if lit and korean:
            out.setdefault(lit, {})['ko'] = korean
        el.clear()
    return out

def read_wordfreq():
    # wordfreq's Korean is split into morphemes: 학교, 사랑, and verbs' stems (먹, 읽).
    import msgpack
    data = msgpack.unpackb(gzip.open(os.path.join(KO, 'small_ko.msgpack.gz')).read(), raw=False)
    zipf = {}
    for i, bucket in enumerate(data[1:], start=1):
        for w in bucket:
            zipf.setdefault(w, 9.0 - i / 100.0)
    return zipf

def as_list(x):
    return x if isinstance(x, list) else ([] if x is None else [x])

def feats(node):
    # krdict's JSON: a node's "feat" is one {att, val} or a list of them; a node
    # can itself be a list (a lemma with variants).
    out = defaultdict(list)
    for n in as_list(node):
        for f in as_list(n.get('feat') if isinstance(n, dict) else None):
            out[f.get('att')].append(html.unescape(f.get('val') or ''))
    return out

def read_krdict():
    entries, examples = [], []
    for path in sorted(glob.glob(os.path.join(KO, 'krdict', '*.json'))):
        with open(path, encoding='utf-8') as f:
            data = json.load(f)
        for e in as_list(data['LexicalResource']['Lexicon']['LexicalEntry']):
            ef = feats(e)
            forms = [feats(x) for x in as_list(e.get('WordForm'))]
            senses, n_examples = [], 0
            for s in as_list(e.get('Sense')):
                eq = {}
                for q in as_list(s.get('Equivalent')):
                    qf = feats(q)
                    lang = qf.get('language', [''])[0]
                    eq.setdefault(lang, (qf.get('lemma', [''])[0], qf.get('definition', [''])[0]))
                senses.append(eq)
                for x in as_list(s.get('SenseExample')):
                    examples.extend(feats(x).get('example', []))
                    n_examples += 1
            entries.append({
                'id': int(e.get('val') or 0),
                'written': feats(e['Lemma']).get('writtenForm', [''])[0].strip(),
                'homonym': int((ef.get('homonym_number') or ['0'])[0] or 0),
                'unit': (ef.get('lexicalUnit') or [''])[0],
                'pos': (ef.get('partOfSpeech') or [''])[0],
                'origin': unicodedata.normalize('NFC', (ef.get('origin') or [''])[0]),
                'level': LEVELS.get((ef.get('vocabularyLevel') or [''])[0], 0),
                'pron': next((x.get('pronunciation', [''])[0] for x in forms if x.get('type', [''])[0] == '발음'), ''),
                'forms': [x.get('writtenForm', [''])[0] for x in forms if x.get('type', [''])[0] == '활용'],
                'senses': senses,
                'examples': n_examples,
            })
    entries.sort(key=lambda e: (e['written'], e['homonym'], e['id']))
    return entries, examples

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
    kor = sentences(os.path.join(T, 'kor_sentences.tsv.bz2'))
    trans = {}
    for code, name in [('en', 'eng'), ('es', 'spa'), ('fr', 'fra'), ('pt', 'por')]:
        ln = links(os.path.join(T, 'kor-%s_links.tsv.bz2' % name))
        wanted = {t for s in kor for t in ln.get(s, [])}
        texts = sentences(os.path.join(T, '%s_sentences.tsv' % name), wanted)
        trans[code] = {s: next((texts[t] for t in ln.get(s, []) if t in texts), '') for s in kor}
    return kor, trans

# --- meanings ----------------------------------------------------------------------------

def clean(s):
    return re.sub(r'\s+', ' ', s.replace('\u200b', '').replace('\t', ' ').replace('\n', ' ')).strip()

def placeholder(s):
    # krdict's "(No equivalent expression)", "(No hay expresión equivalente)",
    # "(Pas d'expression équivalente)", "(対訳語無し)".
    return not s or bool(re.match(r'^[(（].*(equivalent|equivalente|équivalente|対訳).*[)）]$', s, re.I))

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
    # A long first gloss (Hanzi's rules): its notes in brackets left out, cut at
    # its first clause, and one still too long where a phrase ends.
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

def glosses_of(s, sep, reading='', origin=''):
    # A sense's glosses, without placeholders: krdict gives some words with no
    # English their romanization instead (보다 "boda", 갓 "gat"; not 신라 "Silla",
    # nor a loanword's own, 요가 "yoga").
    out = []
    for g in top_split(clean(s), sep):
        g = re.sub(r'^\((n|dét|adj|adv|v|a|pro)\.?\)\s*', '', g)       # French glosses' parts of speech: "(n.) national"
        if placeholder(g) or not g or (reading and g.islower() and re.sub(r'[^a-z]', '', g) == reading and g != origin.lower()):
            continue
        out.append(g)
    return out

def join_senses(senses):
    # The first sense's glosses while they fit, the second sense's too if they all
    # still fit; each gloss once; "; " between.
    out, seen = [], set()
    for n, glosses in enumerate(senses[:2]):
        glosses = [g for g in glosses if g.lower() not in seen]
        if not glosses:
            continue
        if not out:
            out = [shorten(glosses[0])]
            for g in glosses[1:]:
                if len('; '.join(out + [g])) > MEANING_CHARS:
                    break
                out.append(g)
        elif len('; '.join(out + glosses)) <= MEANING_CHARS:
            out += glosses
        seen |= {g.lower() for g in out}
    return fit_bytes('; '.join(out))

def meaning_of(senses, sep, reading='', origin=''):
    # sep: how krdict separates glosses, "; " in English, ", " in Spanish and French.
    return join_senses([glosses_of(s, sep, reading, origin) for s in senses[:2]])

PROPER = {'Korean', 'Korea', 'Chinese', 'Japanese', 'Western', 'English', 'Buddhist', 'Confucian', 'Christian', 'Catholic'}

def short_definition(d):
    # An English definition for a sense with no English word, cut to what it is:
    # its gloss when it starts with one ("kiln: A heated furnace..."), a bound
    # noun's or an auxiliary's point, else its head up to a clause.
    d = clean(d).rstrip('. ')
    m = re.match(r'^([^:()]{2,40}): [A-Z]', d)
    if m:
        return fit_bytes(m.group(1))
    while re.search(r'\([^()]*\)', d):
        d = re.sub(r'\s*\([^()]*\)', '', d).strip()      # notes: "(archaic)", "(a conjugated form of ...)"
    d = re.sub(r'^In [^,]{1,30}, ', '', d)                 # "In music, an instruction..."
    article = re.match(r'^(a|an|the|to) ', d, re.I)        # "A pronoun...", "To go..."
    if article:
        d = d[article.end():]
        if d.split(' ')[0] not in PROPER:
            d = d[:1].lower() + d[1:]
    d = re.sub(r'^bound noun that serves as a unit for counting (the number of )?', 'counter for ', d)
    d = re.sub(r'^bound noun (meaning|indicating|referring to|used to indicate|used to refer to|used to mean) (that )?', '', d)
    d = re.sub(r'^auxiliary (verb|adjective) used to ((indicate|express|mean|imply) (that )?)?', 'auxiliary: ', d)
    d = re.sub(r'^abbreviated word for ', 'short for ', d)
    d = re.sub(r'^bound noun used ', 'used ', d)
    if len(d) > MEANING_LONG:
        cut = [d.find(w) for w in (' that ', ' which ', ' who ', ' whose ', ' worn ', ' used ', ' made ', ' when ', ' where ', ' so as ', ' so that ')]
        cut = [c for c in cut if c >= 12]
        d = d[:min(cut)] if cut else d
    if len(d) > MEANING_LONG:
        cut = [m.start() for m in re.finditer(r', | by | with | of | in | for | and | but | than | from | to ', d) if 20 <= m.start() <= MEANING_LONG]
        d = d[:max(cut)] if cut else d[:d.rfind(' ', 0, MEANING_LONG)]
    return fit_bytes(d.rstrip(' ,;:'))

def japanese_of(lemma):
    # krdict's "かかく【価格】。ねだん【値段】。プライス" → "価格; 値段; プライス": each
    # word written as written (the first form in 【】), else in kana.
    out = []
    for item in re.split(r'[。．]', clean(lemma)):
        item = item.strip()
        if placeholder(item) or not item:
            continue
        m = re.search(r'【([^】]*)】', item)
        word = m.group(1).split('・')[0].strip() if m else item
        if word and word not in out:
            out.append(word)
    return '; '.join(out)

def japanese_meaning(lemmas):
    return join_senses([[g for g in japanese_of(l).split('; ') if g] for l in lemmas[:2]])

def char_meaning(definition):
    # Unihan's kDefinition, as KANJIDIC2's are: glosses joined by ", ", a few, no
    # notes on radicals, hexagrams and stars.
    out = []
    for sense in definition.split(';'):
        for g in top_split(sense, ','):
            g = clean(g)
            if not g or re.search(r'Kangxi radical|radical (number )?\d|hexagram|lunar mansion|determinative star|^\(?(Cant|J|K)\.?\)|^U\+|^variant of|^same as', g, re.I):
                continue
            if g not in out:
                out.append(g)
    s = ''
    for g in out:
        if not s:
            s = g
        elif len(s) + 2 + len(g) <= MEANING_CHARS:
            s += ', ' + g
        else:
            break
    return s

# --- the tables ----------------------------------------------------------------------

def dueum(r):
    # A reading as said at a word's start (두음 법칙): 락 → 낙, 리 → 이, 녀 → 여.
    l, v, t = split_syllable(r)
    if l == 'ㄹ':
        return join_syllable('ㅇ' if v in 'ㅑㅕㅖㅛㅠㅣ' else 'ㄴ', v, t)
    if l == 'ㄴ' and v in 'ㅑㅕㅛㅠㅣ':
        return join_syllable('ㅇ', v, t)
    return r

def hanja_sounds(c, unihan, hun, kd):
    # Unihan's kHangul ("낙:0 락:0E 악:0N 요:0N"): the readings KS X 1001 or the
    # education list give (the others are for names only), not those that are
    # another's form at a word's start (낙 for 락), the education one first.
    readings = []
    for x in unihan.get(c, {}).get('kHangul', '').split():
        r, _, flags = x.partition(':')
        if r and is_syllable(r):
            readings.append((r, flags))
    if any('0' in f or 'E' in f for _, f in readings):
        readings = [(r, f) for r, f in readings if '0' in f or 'E' in f]
    if not readings:
        readings = [(r, '') for r in hun.get(c, {})] or [(r, '') for r in kd.get(c, {}).get('ko', []) if is_syllable(r)]
    have = {r for r, _ in readings}
    readings = [(r, f) for r, f in readings if not any(r == dueum(o) and o != r for o in have)]
    readings.sort(key=lambda rf: 0 if 'E' in rf[1] else 1)
    sounds_ = [r for r, _ in readings]
    meanings = []
    for r in sounds_:
        for m in hun.get(c, {}).get(r, []):
            if m not in meanings:
                meanings.append(m)
    return sounds_, meanings[:HUN_MOST]

def zipf_of(e, zipf, endings, suffixes):
    # wordfreq counts morphemes: a verb's stem (먹 for 먹다), a 하다 verb's noun
    # (사랑 for 사랑하다, said a little less), never the lemma. It can't tell a
    # word from a particle, ending or suffix spelt alike (은 silver, 들 field): those
    # have none; a verb's stem spelt like one (가 for 가다), a share. Nor one
    # syllable's words apart (말 speech, 末 end; 할 割 and 하다's 할): only a
    # beginner's is taken to be the one counted.
    w = e['written']
    found = [zipf[w]] if w in zipf and w not in endings and w not in suffixes and (len(w) > 1 or e['level'] == 1) else []
    if e['pos'] in VERBS and w.endswith('다') and len(w) >= 2:
        stem = w[:-1]
        if stem in zipf:
            found.append(zipf[stem] - (1.0 if len(stem) == 1 else 0.0) - (1.0 if stem in endings else 0.0))
        noun = stem[:-1]
        if stem.endswith('하') and noun in zipf and noun not in endings and noun not in suffixes:
            found.append(zipf[noun] - (1.0 if len(noun) == 1 else 0.5))
        found += [zipf[f] for f in e['forms'] if f in zipf and f not in endings and len(f) >= 2]
    return max(found) if found else 0.0

def verb_forms(e):
    # How a verb or adjective starts a word in text (an eojeol): its stem and an
    # ending (먹고, 먹어요), its last syllable taking the ending's consonant (갑니다,
    # 간), or its vowel contracted (와, 봤어요, 했다); and krdict's conjugated forms
    # (들어, 도와, 몰라). (form, True): something must follow the form.
    w = e['written']
    stem = w[:-1]
    forms = {(stem, True)}
    l, v, t = split_syllable(stem[-1])
    if not t:
        for f in ('ㄴ', 'ㄹ', 'ㅂ', 'ㅁ'):
            forms.add((stem[:-1] + join_syllable(l, v, f), False))
        contracted = {'ㅗ': 'ㅘ', 'ㅜ': 'ㅝ', 'ㅣ': 'ㅕ', 'ㅡ': 'ㅓ', 'ㅚ': 'ㅙ'}.get(v, v)
        if stem[-1] == '하':
            contracted = 'ㅐ'
        if v in 'ㅏㅓㅐㅔㅕㅗㅜㅣㅡㅚ' or stem[-1] == '하':
            if contracted != v:
                forms.add((stem[:-1] + join_syllable(l, contracted), False))
            forms.add((stem[:-1] + join_syllable(l, contracted, 'ㅆ'), False))
    for f in e['forms']:
        if f and all(is_syllable(c) for c in f) and f != stem:
            forms.add((f, False))
    return forms

# Telling homonyms apart by a sentence's English translation: words too plain to
# go by, and the irregular forms of the commonest verbs.
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

def mentions(keywords, english):
    # Is one of a word's English glosses in a translation's words (eat: eating, ate)?
    for kw in keywords:
        for t in english:
            if t == kw or (len(kw) >= 3 and t.startswith(kw)) or (len(kw) >= 5 and t.startswith(kw[:-1])) or t in IRREGULAR.get(kw, ()):
                return True
    return False

def eojeol_start(token):
    # A word of text as Korean spaces it: its hangul from the start, after any
    # quote or bracket; one starting with a letter or digit, none.
    token = token.lstrip('"\'“‘(「『《[-–—…·')
    m = re.match(r'[가-힣]+', token)
    return m.group(0) if m else ''

def main():
    test_romanize()
    print('reading the sources...')
    hangul   = kanji_blocks(os.path.join(RAW, 'hangulvg.xml'))
    kanjivg  = kanji_blocks(os.path.join(RAW, 'kanjivg.xml'))
    unihan, education = read_unihan()
    hun      = read_libhangul()
    kd       = read_kanjidic()
    zipf     = read_wordfreq()
    krdict, examples = read_krdict()
    kor, trans = read_tatoeba()
    print(len(hangul), 'jamo and syllables,', len(krdict), 'krdict entries,', len(examples), 'krdict examples,', len(kor), 'Korean sentences')

    # Particles and endings as written ('-습니다' → 습니다), and suffixes ('-들'):
    # wordfreq's counts of those forms are mostly theirs, not a word's spelt alike.
    endings  = {e['written'].replace('-', '') for e in krdict if e['pos'] in ('조사', '어미')}
    suffixes = {e['written'].replace('-', '') for e in krdict if e['pos'] == '접사' and e['written'].startswith('-')}
    def hanja_in(e):
        return [c for c in e['origin'] if is_hanja(c)]
    single_words = [e for e in krdict if e['unit'] == '단어']

    # Words: krdict's, each written form, reading and meaning once.
    words, seen = [], set()
    for e in single_words:
        w = e['written']
        if e['pos'] in GRAMMAR or not w or len(w) > WORD_CHARS or not all(is_syllable(c) for c in w):
            continue
        senses = e['senses']
        reading = romanize(w, e['pron'], e['pos'] in NOUNS)
        english = meaning_of([s.get('영어', ('', ''))[0] for s in senses], '; ', reading, e['origin'])
        defined = not english and senses
        if defined:
            english = short_definition(senses[0].get('영어', ('', ''))[1])
        if not english:
            continue                     # a spelling krdict only points away from (곤색 → 감색)
        meanings = {'es': meaning_of([s.get('스페인어', ('', ''))[0] for s in senses], ', ', reading, e['origin']),
                    'fr': meaning_of([s.get('프랑스어', ('', ''))[0] for s in senses], ', ', reading, e['origin']),
                    'ja': japanese_meaning([s.get('일본어', ('', ''))[0] for s in senses]),
                    'pt': ''}
        if english[:1].islower():
            # krdict starts some Spanish and French glosses with a capital (Ir).
            for l in ('es', 'fr'):
                meanings[l] = '; '.join(g[0].lower() + g[1:] if g[:1].isupper() and g[1:2].islower() else g for g in meanings[l].split('; ') if g)
        key = (w, reading, english)
        if key in seen:
            continue
        seen.add(key)
        z = zipf_of(e, zipf, endings, suffixes)
        level = e['level']
        freq = max(0, min(0xFFFE, int(round((8.0 - z) * 100)) if z > 0 else LEVEL_FREQ.get(level, 1060)))
        words.append({'written': w, 'reading': reading, 'meaning': english, 'in': meanings, 'freq': freq, 'zipf': z,
                      'common': level > 0 or z >= 3.2, 'level': level, 'chars': len(w), 'pos': e['pos'],
                      'hanja': hanja_in(e), 'entry': e,
                      'keywords': set() if defined else {k for k in re.findall(r'[a-z]+', english.lower()) if k not in STOPWORDS}})
    print(len(words), 'words,', sum(1 for w in words if not w['entry']['pron']), 'romanized without krdict\'s pronunciation')

    # The hanja kept: the education ones and every one in a levelled word's origin,
    # where KanjiVG has its strokes.
    levelled_hanja = {c for e in single_words if e['level'] for c in hanja_in(e)}
    hanja = sorted(c for c in (education | levelled_hanja) if c in kanjivg and c not in hangul)
    chars = sorted(set(hangul) | set(hanja))

    # Levels: a syllable's, the easiest of the levelled words with it in (particles
    # and endings too: 습 in -습니다 is a beginner's); a hanja's, of those whose
    # origin has it, else 3 for an education hanja; the basic jamo 1.
    level = {}
    for e in single_words:
        if not e['level']:
            continue
        for c in set(e['written'].replace('-', '')):
            if is_syllable(c):
                level[c] = min(level.get(c, 9), e['level'])
        for c in set(hanja_in(e)):
            level[c] = min(level.get(c, 9), e['level'])
    for c in chars:
        if 'ㄱ' <= c <= 'ㅣ':
            level[c] = 1 if c in BASIC_JAMO else 0
        elif c in education and c not in level:
            level[c] = 3
    # How common each character is, rank 1 the commonest: the jamo first, then
    # syllables and hanja together — a syllable by how often the sentences have it
    # (per million syllables), a hanja by how often its words come up (wordfreq's
    # per million words; a word it can't tell, about where its level sits).
    counts = defaultdict(int)
    for t in list(kor.values()) + examples:
        for c in t:
            if is_syllable(c):
                counts[c] += 1
    total = sum(counts.values()) or 1
    usage = {c: n * 1e6 / total for c, n in counts.items()}
    for e in single_words:
        if e['written'] and all(is_syllable(c) for c in e['written']):
            z = zipf_of(e, zipf, endings, suffixes) or (8.0 - LEVEL_FREQ[e['level']] / 100.0 if e['level'] else 0.0)
            for c in hanja_in(e):
                usage[c] = usage.get(c, 0.0) + (10 ** (z - 3) if z > 0 else 0.0)
    # The 40 jamo in the order they are taught (the Hangul chart's: the fourteen
    # consonants, the doubled, the ten vowels, the eleven they make); the final
    # clusters, only parts, after every character in use.
    rank = {c: i + 1 for i, c in enumerate(JAMO_ORDER) if c in chars}
    ranked = sorted((c for c in chars if usage.get(c, 0) > 0 and c not in rank), key=lambda c: (-usage[c], c))
    rank.update({c: len(rank) + 1 + i for i, c in enumerate(ranked)})
    rank.update({c: len(rank) + 1 + i for i, c in enumerate(c for c in chars if 'ㄱ' <= c <= 'ㅣ' and c not in rank)})

    # Sentences: each word's best (it in it, a comfortable length, translated into
    # more languages), each sentence one word's — the easiest and commonest words
    # choose first. A word is found at the start of a word of text: a noun with
    # its particle after it (학교에), a verb conjugated (verb_forms); the longest
    # match wins (가게에: 가게, not 가다), and a word of text that is both a whole
    # noun and a verb's form (가요: songs, or goes) counts for neither. Words that
    # match alike (눈 eye, 눈 snow; 해 sun, 해 did) are told apart by the English
    # translation; failing that, of words spelt alike the easiest when only one is
    # (집 house, not 輯 edition), or of verbs as easy the one krdict makes most of
    # (있다), and else none (이: tooth, this or two).
    forms = defaultdict(list)            # form → (word, something must follow)
    for i, w in enumerate(words):
        if not w['common'] or w['pos'] in ('보조 동사', '보조 형용사'):
            continue                     # an auxiliary looks like its verb: a sentence can't show which
        if w['pos'] in VERBS and w['written'].endswith('다') and w['chars'] >= 2:
            for f, more in verb_forms(w['entry']):
                forms[f].append((i, more))
        else:
            forms[w['written']].append((i, False))
    longest = max(len(f) for f in forms)
    usable = {sid: t for sid, t in kor.items() if SENT_MIN <= len(clean(t)) <= SENT_MAX and trans['en'].get(sid) and not clean(t).startswith('#')}
    found = defaultdict(set)             # word → its sentences
    for sid, t in usable.items():
        for token in clean(t).split(' '):
            run = eojeol_start(token)
            matches = []
            for k in range(min(len(run), longest), 0, -1):
                for i, more in forms.get(run[:k], []):
                    if not more or (len(run) > k and run[k] in ENDING_START):
                        matches.append((k, i, more))
            if not matches:
                continue
            best = max(k for k, _, _ in matches)
            ids = {i for k, i, _ in matches if k == best}
            if best == len(run) and any(more for k, _, more in matches if k < best) and any(words[i]['pos'] in NOUNS for i in ids):
                continue
            if len(ids) > 1:
                english = re.findall(r'[a-z]+', trans['en'][sid].lower())
                told = {i for i in ids if mentions(words[i]['keywords'], english)}
                if told:
                    ids = told
                elif len({words[i]['written'] for i in ids}) == 1:
                    easiest = min(words[i]['level'] or 9 for i in ids)
                    tied = [i for i in ids if (words[i]['level'] or 9) == easiest]
                    if len(tied) == 1 or all(words[i]['pos'] in VERBS for i in tied):
                        ids = {min(tied, key=lambda i: (-len(words[i]['entry']['senses']), -words[i]['entry']['examples'], i))}
                    else:
                        ids = set()
                else:
                    ids = set()
            for i in ids:
                found[i].add(sid)
    def sentence_score(sid):
        t = clean(kor[sid])
        return abs(len(t) - SENT_IDEAL) - 3 * sum(1 for l in ('es', 'fr', 'pt') if trans[l].get(sid)) + (8 if re.search(r'[A-Za-z]', t) else 0)
    word_sentence, used = {}, set()
    for i in sorted(found, key=lambda i: (words[i]['level'] or 9, words[i]['freq'], i)):
        free = [sid for sid in found[i] if sid not in used]
        if free:
            best = min(free, key=lambda sid: (sentence_score(sid), sid))
            word_sentence[i] = best
            used.add(best)
    kept_sentences = sorted(set(word_sentence.values()))
    sentence_line = {sid: n for n, sid in enumerate(kept_sentences)}
    print(len(word_sentence), 'words with a sentence,', len(kept_sentences), 'sentences')

    # Each character's list: a syllable's words with it in, a hanja's words whose
    # origin has it.
    cands = defaultdict(list)
    for i, w in enumerate(words):
        for c in set(w['written']) | set(w['hanja']):
            cands[c].append(i)
    def score(i, c):
        w = words[i]
        s = (w['level'] if w['level'] else 9) * 100 + w['freq'] / 4.0
        s += 0 if w['common'] else 5000
        s += 40 * max(0, w['chars'] - 2)
        s -= 100 if w['written'] == c or w['entry']['origin'] == c else 0     # the character on its own first (눈, 山 산)
        s += 100 if w['pos'] in ('보조 동사', '보조 형용사', '품사 없음') else 0   # after the verb itself; contractions (누가) after words
        return s
    lists = {}
    for c, ids in cands.items():
        if c not in rank and c not in level and c not in hangul and c not in hanja:
            continue
        # Words spelt alike score alike: the one a sentence was found for first (its
        # translation said it: 가장 "most", not "head of a household"), then the one
        # krdict says more of (금 gold, not Friday).
        ids.sort(key=lambda i: (score(i, c), 0 if i in word_sentence else 1, -min(len(words[i]['entry']['senses']), 10), -words[i]['entry']['examples'], i))
        kept = []
        for i in ids:
            w = words[i]
            if len(kept) >= EXAMPLES:
                break
            if not w['common'] or w['chars'] > EXAMPLE_CHARS:
                continue
            if any(words[k]['written'] == w['written'] or (len(words[k]['written']) > 1 and words[k]['written'] in w['written']) for k in kept):
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
    lists = {c: v for c, v in lists.items() if c in hangul or c in hanja}

    # Words kept: every listed one and every common one (text is read with them).
    keep = sorted({i for _, ks in lists.values() for i in ks} | {i for i, w in enumerate(words) if w['common']})
    line_of = {i: n for n, i in enumerate(keep)}

    os.makedirs(OUT, exist_ok=True)
    readings = {}
    with open(os.path.join(OUT, 'chars.tsv'), 'w', encoding='utf-8') as f:
        f.write('# code point, grade, level, radical, frequency, 음 (sounds), 훈 (meaning words), English, ' + ', '.join(LANGS) + '\n')
        for c in chars:
            row_in = [''] * len(LANGS)
            sounds_, meanings, english, radical = [], [], '', '0'
            if c in hanja:
                u = unihan.get(c, {})
                sounds_, meanings = hanja_sounds(c, unihan, hun, kd)
                jp = kd.get(c, {})
                english = char_meaning(u.get('kDefinition', '')) or clean(jp.get('en', ''))
                row_in = [clean(jp.get(l, '')) if l != 'ja' else '' for l in LANGS]
                r = u.get('kRSUnicode', '').split(' ')[0].split('.')[0].rstrip("'")
                radical = r if r.isdigit() and int(r) < 256 else '0'
                readings[c] = (sounds_, meanings)
            row = ['%x' % ord(c), '0', str(level.get(c, 0) if level.get(c, 0) < 9 else 0), radical,
                   str(rank[c] if rank.get(c, 99999) < 65535 else 0), '、'.join(sounds_), '、'.join(meanings), english] + row_in
            f.write('\t'.join(clean(x) for x in row) + '\n')
    with open(os.path.join(OUT, 'words.tsv'), 'w', encoding='utf-8') as f:
        f.write('# written, reading, English, ' + ', '.join(LANGS) + ', freq, common, sentence\n')
        for i in keep:
            w = words[i]
            s = sentence_line[word_sentence[i]] if i in word_sentence else -1
            f.write('\t'.join([w['written'], w['reading'], clean(w['meaning'])] + [clean(w['in'][l]) for l in LANGS] +
                              [str(w['freq']), '1' if w['common'] else '0', str(s)]) + '\n')
    with open(os.path.join(OUT, 'lists.tsv'), 'w', encoding='utf-8') as f:
        f.write('# code point, examples, word lines\n')
        for c in sorted(lists):
            examples_n, ks = lists[c]
            f.write('\t'.join(['%x' % ord(c), str(examples_n)] + [str(line_of[k]) for k in ks]) + '\n')
    with open(os.path.join(OUT, 'sentences.tsv'), 'w', encoding='utf-8') as f:
        for sid in kept_sentences:
            f.write('\t'.join(clean(x) for x in [kor[sid], trans['en'].get(sid, ''), trans['es'].get(sid, ''), trans['fr'].get(sid, ''), trans['pt'].get(sid, '')]) + '\n')

    # The strokes: our jamo and syllables, KanjiVG's hanja, in code point order.
    with open(os.path.join(KO, 'koreanvg.xml'), 'w', encoding='utf-8') as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n<!--\n'
                'Hangul\'s strokes in KanjiVG\'s format, for the bake. Generated by\n'
                'apps/hangul/tools/data/prepare.py: do not edit.\n\n'
                'The jamo and syllables (U+3131..U+3163, U+AC00..U+D7A3) are our own\n'
                '(apps/hangul/tools/strokes; licence to be decided).\n\n'
                'The hanja are KanjiVG\'s (http://kanjivg.tagaini.net), unchanged,\n'
                'distributed under the conditions of the Creative Commons\n'
                'Attribution-Share Alike 3.0 Licence\n'
                '(http://creativecommons.org/licenses/by-sa/3.0/).\n'
                '-->\n'
                "<kanjivg xmlns:kvg='http://kanjivg.tagaini.net'>\n")
        for c in chars:
            f.write(hangul[c] if c in hangul else kanjivg[c])
        f.write('</kanjivg>\n')

    groups = {'jamo': [c for c in chars if 'ㄱ' <= c <= 'ㅣ'], 'syllables': [c for c in chars if is_syllable(c)], 'hanja': hanja}
    for name, cs in groups.items():
        by = defaultdict(int)
        for c in cs:
            by[level.get(c, 0) if level.get(c, 0) < 9 else 0] += 1
        print('%-9s %5d  levels: %s; ranked %d; listed %d' % (name, len(cs), ', '.join('%d: %d' % kv for kv in sorted(by.items())),
              sum(1 for c in cs if c in rank), sum(1 for c in cs if c in lists)))
    print('hanja:', sum(1 for c in hanja if c in education), 'of the', len(education), 'education ones;',
          sum(1 for c in education if c not in kanjivg), 'not in KanjiVG;', sum(1 for c in hanja if readings[c][1]), 'with 훈')
    # The syllables to learn: KS X 1001's 2,350 (the standard set: every one a
    # Korean writes), any a word of the dictionary has (찟), and the Hangul
    # chart's (땨 쌰 쬬). A bit each from 가; lang.c's group reads it.
    listed = set()
    for hi in range(0xB0, 0xC9):
        for lo in range(0xA1, 0xFF):
            try:
                ch = bytes([hi, lo]).decode('euc-kr')
            except UnicodeDecodeError:
                continue
            if len(ch) == 1 and is_syllable(ch):
                listed.add(ch)
    listed |= {c for w in words for c in w['written'] if is_syllable(c)}
    for i in (0, 2, 3, 5, 6, 7, 9, 11, 12, 14, 15, 16, 17, 18, 1, 4, 8, 10, 13):   # fude/lang/ko/chart.c's table
        for m in (0, 2, 4, 6, 8, 12, 13, 17, 18, 20):
            listed.add(chr(0xAC00 + (i * 21 + m) * 28))
    bits = bytearray((11172 + 7) // 8)
    for c in listed:
        n = ord(c) - 0xAC00
        bits[n >> 3] |= 1 << (n & 7)
    with open(os.path.join(ROOT, 'fude', 'lang', 'ko', 'syllables.h'), 'w', encoding='utf-8') as f:
        f.write('// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.\n\n'
                '// The syllables to learn (lang.c\'s Syllables group): KS X 1001\'s 2,350, any a\n'
                '// word of the dictionary has, and the Hangul chart\'s — %d; the rest of the\n'
                '// 11,172 are written, but listed nowhere. A bit each, from U+AC00 (bit n of\n'
                '// byte n / 8). Generated by apps/hangul/tools/data/prepare.py: do not edit.\n' % len(listed))
        f.write('static const u8 FUDE_KO_LISTED[%d] = {\n' % len(bits))
        for k in range(0, len(bits), 16):
            f.write('    ' + ', '.join('0x%02X' % b for b in bits[k:k + 16]) + ',\n')
        f.write('};\n')

    print('wrote', OUT, ':', len(chars), 'characters,', len(keep), 'words (', sum(1 for i in keep if words[i]['common']), 'common,',
          sum(1 for i in keep if i in word_sentence), 'with a sentence ),', len(lists), 'lists,', len(kept_sentences), 'sentences; and koreanvg.xml;', len(listed), 'syllables to learn (fude/lang/ko/syllables.h)')

if __name__ == '__main__':
    main()
