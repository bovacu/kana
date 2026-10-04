# Hindi data

The character data for the Hindi app: every letter and sign it teaches, its
level, the words each one lists, and the example sentences. It is written in
the shared bake's PREPARED tables (`fude/study/chars/bake.h`), through
`tools/data/prepared.py` (shared with Thai).

## Running

From the repository root, after the strokes (`apps/hindi/tools/strokes/compose.py`,
which writes `data/raw/hi/devanagarivg.xml`):

```sh
python3 -m pip install msgpack                # once: wordfreq's file is msgpack
python3 apps/hindi/tools/data/fetch.py
python3 apps/hindi/tools/data/prepare.py      # about 4 s
./build/hindi/osx-silicon/debug/project/exe/runnable --bake
```

The English, Spanish, French and Portuguese Tatoeba sentences are Kana's
(`data/raw/tatoeba/`). The output is deterministic.

## Output

All in `data/raw/hi/prepared/` (git-ignored).

| File | What it holds |
| --- | --- |
| `chars.tsv` | The 90 letters and signs of `fude/lang/hi/lang.c`'s table at its levels (read from there), how common each is, and what each vowel sign and mark does (ours, en/es/pt/fr/ja). |
| `words.tsv` | English Wiktionary's Hindi lemmas: romanization (कमल kamal), meanings in English, and in French, Portuguese and Japanese where those Wiktionaries have the word, how common (wordfreq), common or not, and a sentence. |
| `lists.tsv` | Each letter's words, examples first; a letter with a dot (क़) also lists its base's (क). |
| `sentences.tsv` | Tatoeba's Hindi sentences used, with their English, Spanish, French and Portuguese translations. |

## Sources and licences

| Source | Used for | Licence |
| --- | --- | --- |
| English Wiktionary (kaikki.org) | Words, romanization, English, inflected forms | CC BY-SA 4.0 |
| Japanese, French, Portuguese Wiktionaries (kaikki.org) | Meanings | CC BY-SA 4.0 |
| wordfreq `small_hi` | How common words and letters are | CC BY-SA 4.0 |
| Tatoeba | Sentences | CC BY 2.0 FR |

## How it decides

- **Text** is compared in NFC (क़ as क and its dot), zero-width joiners left out.
- **Levels**: the letters', from `lang.c`. Hindi has no official word levels:
  a word wordfreq counts (zipf 3 and above) is common.
- **Readings**: Wiktionary's romanization, which drops the silent a (कमल kamal);
  its ṭ ḍ ṛ ṅ need the app's `NotoSansLatinExt-Regular.ttf`.
- **Sentences**: a word of text is a lemma, or one of its inflected forms
  (Wiktionary's form-of entries and inflection tables: लड़के → लड़का, गया → जाना).
  Lemmas spelt alike are told apart by the English translation.

## Known limits

- No Spanish meanings (the Spanish Wiktionary has almost no Hindi); French,
  Portuguese and Japanese ones only for a few hundred words. Wikidata's
  lexemes (CC0, about 1,500 senses each) would add some.
- wordfreq ships only its small Hindi list: rarer words are not common.
