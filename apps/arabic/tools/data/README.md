# Arabic data

The character data for the Arabic app: every letter, joined form, vowel mark
and digit it teaches, its level and what it is, the words each one lists, and
the example sentences. It is written in the shared bake's PREPARED tables
(`fude/study/chars/bake.h`), through `tools/data/prepared.py` (shared with
Thai and Hindi).

## Running

From the repository root, after the strokes (`apps/arabic/tools/strokes/compose.py`,
which writes `data/raw/ar/arabicvg.xml`):

```sh
python3 -m pip install msgpack                # once: wordfreq's file is msgpack
python3 apps/arabic/tools/data/fetch.py       # about 540 MB, most of it Wiktionary's
python3 apps/arabic/tools/data/prepare.py     # about 20 s
./build/arabic/osx-silicon/debug/project/exe/runnable --bake
```

The English, Spanish, French and Portuguese Tatoeba sentences are Kana's
(`data/raw/tatoeba/`). The output is deterministic.

## Output

All in `data/raw/ar/prepared/` (git-ignored).

| File | What it holds |
| --- | --- |
| `chars.tsv` | The 137 characters of `fude/lang/ar/lang.c`'s table at its levels (read from there): how common each is, and what it is — a letter's sound (ث: th, as in think), a joined form's place, a mark's vowel — ours, in en/es/pt/fr/ja. |
| `words.tsv` | English Wiktionary's Arabic lemmas, written with their vowels (كِتَاب): romanization (kitāb), meanings in English, and in Spanish, French, Portuguese and Japanese where those Wiktionaries have the word, how common (wordfreq), common or not, and a sentence. |
| `lists.tsv` | A letter's words; a joined form's, the words where its letter takes that form; a vowel mark's, the words that write it. Examples first. |
| `sentences.tsv` | Tatoeba's Arabic sentences used, with their English, Spanish, French and Portuguese translations. |

## Sources and licences

| Source | Used for | Licence |
| --- | --- | --- |
| English Wiktionary (kaikki.org) | Words, vowelled forms, romanization, English, inflected forms | CC BY-SA 4.0 |
| Spanish, French, Portuguese, Japanese Wiktionaries (kaikki.org) | Meanings | CC BY-SA 4.0 |
| wordfreq `large_ar` | How common words and letters are | CC BY-SA 4.0 |
| Tatoeba | Sentences | CC BY 2.0 FR |

## How it decides

- **Bare text.** Words and text are compared without their vowel marks or
  tatweel, with أ إ آ ٱ as ا: most Arabic is written without vowels. A word of
  text is found with its little words (و ف, then ال ب ل ك) taken off its front,
  as itself or as one of a lemma's inflected forms.
- **Readings**: Wiktionary's romanization, with sh th dh kh gh for its š ṯ ḏ ḵ ḡ
  and ʾ for its ʔ, as the letters' names are written; its ā ḥ ṣ ʿ need the app's
  `NotoSansLatinExt-Regular.ttf`.
- **Meanings**: a sense's general gloss (Wiktionary nests sub-senses), without
  "verbal noun of …" before it. Another Wiktionary's meaning by the same
  vowelled word, else by the same bare word and part of speech, never a noun's
  for a verb; its letter entries and its "nom d'action" glosses left out.
- **Words spelt alike without their vowels** (قَلَم pen, قَلَمَ to cut) share
  wordfreq's count: the one the sentences' English translations say most is the
  main one and common; another is common only when they say it three times.
- **Joined forms** are computed as `lang.c` does (`fude_lang_form`): ل then ا is
  the ligature ﻻ.

## Known limits

- No levels: Arabic has no open graded word list; words go by frequency.
- Spanish, Portuguese and Japanese meanings for a few hundred words, French
  for about 2,400; the rest show English.
- Word splitting is longest-match over bare text: a word's suffixes
  (pronouns, plurals not in Wiktionary's tables) can hide it.
