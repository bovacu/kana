# Thai data

The character data for the Thai app: every letter it teaches, its level and
meaning, the words each one lists, and the example sentences. It is written in
the shared bake's PREPARED tables (`fude/study/chars/bake.h`), through
`tools/data/prepared.py` (shared with Hindi).

## Running

From the repository root, after the strokes (`apps/thai/tools/strokes/compose.py`,
which writes `data/raw/th/thaivg.xml`):

```sh
python3 apps/thai/tools/data/fetch.py
python3 apps/thai/tools/data/prepare.py       # about 6 s
./build/thai/osx-silicon/debug/project/exe/runnable --bake
```

The English, Spanish, French and Portuguese Tatoeba sentences are Kana's
(`data/raw/tatoeba/`). The output is deterministic.

## Output

All in `data/raw/th/prepared/` (git-ignored).

| File | What it holds |
| --- | --- |
| `chars.tsv` | The 83 letters of `fude/lang/th/lang.c`'s table at its levels (read from there), how common each is in the Thai National Corpus, and what it means (a consonant's key word, ก ko kai: chicken; a vowel's or sign's part), ours, in en/es/pt/fr/ja. |
| `words.tsv` | English Wiktionary's Thai words: tone romanization (ภาษา paa-sǎa), meanings in English (Wiktionary), Spanish, French, Portuguese (Volubilis, else those Wiktionaries) and Japanese (the Japanese Wiktionary), how common, common or not, and a sentence. |
| `lists.tsv` | Each letter's words, examples first. |
| `sentences.tsv` | Tatoeba's Thai sentences used, with their English, Spanish, French and Portuguese translations. |

## Sources and licences

| Source | Used for | Licence |
| --- | --- | --- |
| English Wiktionary (kaikki.org) | Words, tone romanization, English | CC BY-SA 4.0 |
| Japanese, French, Portuguese Wiktionaries (kaikki.org) | Meanings | CC BY-SA 4.0 |
| Volubilis Mundo v26.2, by Belisan | Levels A0–A2; Spanish, French, Portuguese meanings | CC BY-SA 4.0 (stated on its blog; `docs/research/thai_sources.md`) |
| PyThaiNLP `tnc_freq.txt` | How common words and letters are | CC0 |
| Tatoeba | Sentences | CC BY 2.0 FR |

## How it decides

- **Levels**: the letters', from `lang.c`. Words have Volubilis's A0, A1, A2 as
  their level; a word with one, or one the corpus counts above zipf 3.2, is
  common.
- **Readings**: Wiktionary's tone romanization (its "Paiboon" field; never
  named so in the app). Its letters ɛ ɔ ʉ ə and tones ǎ need the app's
  `NotoSansLatinExt-Regular.ttf` (Roboto has none of them).
- **Meanings**: Wiktionary's first two senses, the ones that say what the
  word's easiest Volubilis row says first (กับ "with", not its first
  etymology's "trap"); Volubilis's first two rows for es/fr/pt.
- **Sentences**: Thai has no spaces between words: a sentence is split into the
  fewest words of Wiktionary, Volubilis and the corpus that make it, never
  inside a consonant's cluster. Words spelt alike are told apart by the
  English translation, or the sentence goes to neither.

## Known limits

- Only Wiktionary's words (about 15,000): Volubilis's words not in Wiktionary
  (mostly compounds such as อาหารไทย, and spelling variants) are left out, as
  they would need its own romanization converted.
- About 1,450 words have a sentence: Tatoeba's Thai sentences cover few
  distinct words, and each sentence goes to one word.
- Portuguese and Japanese meanings are few (about 1,800 and 1,100 words);
  the rest show English.
