# Hangul data

The character data for the Hangul app: every jamo, syllable and hanja it
teaches, their readings, meanings and levels, the words each one lists, and the
example sentences. It is written in the shared bake's PREPARED tables
(`fude/study/chars/bake.h`), so the bake reads it the way it reads Hanzi's.

## Files

| File | What it is |
| --- | --- |
| `fetch.py` | Downloads the sources into `data/raw/` (git-ignored), and unzips krdict. A file already there is kept. |
| `prepare.py` | Reads the sources and writes the tables and the strokes file. Also holds our Revised Romanization, with the notice's examples as tests. |

## Running

From the repository root, after the strokes (`apps/hangul/tools/strokes/compose.py`,
which writes `data/raw/hangulvg.xml`):

```sh
python3 -m pip install msgpack                # once: wordfreq's file is msgpack
python3 apps/hangul/tools/data/fetch.py
python3 apps/hangul/tools/data/prepare.py     # about 20 s
```

KanjiVG and KANJIDIC2 come from Kana's fetch and Unihan from Hanzi's
(`apps/hanzi/tools/data/fetch.py`); the English, Spanish, French and
Portuguese Tatoeba sentences are Kana's. The output is deterministic.

## Output

All in `data/raw/ko/` (git-ignored).

| File | What it holds |
| --- | --- |
| `koreanvg.xml` | The strokes the bake reads, in KanjiVG's format: the 51 jamo and 11,172 syllables of `hangulvg.xml`, and KanjiVG's hanja (unchanged) for the 1,800 education hanja and every hanja in the origin of a krdict word with a level. |
| `prepared/chars.tsv` | One row per character of `koreanvg.xml`: level, radical (hanja), frequency rank, the hanja's sounds (음, e.g. 樂 락、악、요) and meaning words (훈, 天 하늘), English meaning, and Spanish, Portuguese and French ones (hanja only). |
| `prepared/words.tsv` | krdict's words of 1–5 syllables (not particles, endings or affixes): reading in Revised Romanization, meanings in English, Spanish, French and Japanese, how common, common or not, and a sentence. |
| `prepared/lists.tsv` | Each syllable's words (those it is in) and each hanja's (those whose origin has it: 價 → 가격), examples first. Jamo have none. |
| `prepared/sentences.tsv` | Tatoeba's Korean sentences used, with their English, Spanish, French and Portuguese translations. |

The languages after English are `es`, `pt`, `fr`, `ja`, in that order (the
bake takes four). krdict has no Portuguese, so words have none.

## Sources and licences

| Source | Used for | Licence |
| --- | --- | --- |
| krdict, NIKL 한국어기초사전 (JSON download) | Words, pronunciations, meanings (en, es, fr, ja), levels, hanja origins, example sentences (syllable counts only) | CC BY-SA 2.0 KR. Credit "국립국어원 한국어기초사전, CC BY-SA 2.0 KR". Its sound and image links are never read. |
| Tatoeba | Sentences | CC BY 2.0 FR |
| Unihan (`Unihan.zip`) | The 1,800 education hanja, 음 (kHangul), English (kDefinition), radicals | Unicode License v3 |
| libhangul `data/hanja/hanja.txt` | 훈 | BSD-3-Clause (the file's own header; not the repository's LGPL) |
| KANJIDIC2 | Hanja meanings in es, pt, fr; English and 음 where Unihan has none | CC BY-SA 4.0 (EDRDG) |
| KanjiVG | Hanja strokes | CC BY-SA 3.0 (its notice is kept in `koreanvg.xml`) |
| wordfreq `small_ko` | How common words are | CC BY-SA 4.0 |
| Noto Sans KR | Downloaded for the app (not used here) | OFL 1.1 |
| Revised Romanization, 고시 제2014-42호 | Readings (our own code) | Government notice, not copyrighted |

## How it decides

- **Levels**: krdict's 초급 1, 중급 2, 고급 3. A syllable takes the easiest level
  of the levelled krdict words it is in (particles and endings count: 습 in
  -습니다 is a beginner's). A hanja takes the easiest of the words whose origin
  has it, else 3 if it is an education hanja. The 40 jamo of today's alphabet
  are 1; the 11 final clusters 0.
- **Readings**: Revised Romanization of krdict's standard pronunciation (the
  first one), lowercase, syllables run together: tensing is not written (읽다
  [익따] ikda, 학교 [학꾜] hakgyo), vowels are as written (ㅢ is ui), a noun's ㄱ
  ㄷ ㅂ before ㅎ stay apart (축하 chukha). Words krdict gives no pronunciation
  (about 5%) go through the main sound rules instead; on words that have one,
  those rules agree with it 99% of the time (the rest are word-specific ㄴ
  insertions such as 알약 [알략]).
- **Meanings**: the first sense's glosses while they fit 48 characters, the
  second sense's if they still fit, each once. krdict's placeholders ("(No
  equivalent expression)", or the romanization itself, 보다 "boda") are dropped;
  a sense with no English word gets a short form of its English definition
  ("counter for houses, buildings, etc"). Japanese: the written form in 【】,
  else the kana. Words' meanings are kept within the bake's 127 bytes.
- **How common**: wordfreq splits Korean into morphemes, so a verb is counted by
  its stem (먹 for 먹다) and a 하다 verb by its noun. A word spelt like a particle,
  ending or suffix, and a one-syllable word above beginner level, can't be told
  apart from what wordfreq counted: those take a value from their level.
- **Character ranks**: jamo 1–51, then syllables and hanja together: syllables
  by how often Tatoeba's and krdict's example sentences have them, hanja by how
  common their words are.
- **Sentences**: a word is looked for at the start of a word of text (a noun
  with its particle, a verb conjugated), the longest match winning. Homonyms are
  told apart by the English translation, or the sentence goes to none of them.
  The easiest and commonest words choose first, each sentence going to one word.

## Known limits

- **50 education hanja have no strokes**: krdict writes the Korean standard
  forms (內 靑 德 說 稅 步 每 溫 綠 ...), and KanjiVG has only the Japanese ones
  (内 青 徳 説 ...), which differ in shape. Words with them do not reach those
  characters' lists. Strokes drawn for the Korean forms would fix this.
- Hanja stroke order is KanjiVG's, the Japanese one; it may differ for a few.
- Hanja meanings in es, pt and fr only where KANJIDIC2 has that exact
  character: traditional forms (學 價 樂) mostly have none, and show English.
- Only about 10,000 Tatoeba sentences have an English translation and a usable
  length, so about 3,900 words have a sentence. Spanish, French and Portuguese
  translations are Tatoeba's direct links only (about 390, 390 and 90 of them).
- Homonyms spelt alike share a frequency; in lists, the one krdict says more
  about comes first (금 gold before 금 Friday), which is not always the commoner
  (가장 "head of a household" before 가장 "most").
- wordfreq's Korean list is the small one (words above about zipf 3).
