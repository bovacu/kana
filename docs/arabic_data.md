# Arabic: the data and its licences

Checked 2026-10-04 against primary sources (licence files, official terms
pages, download pages, the live data), for a paid, closed-source app on
iPad, Android and phones that teaches Modern Standard Arabic in the Naskh
school hand: each letter in its isolated, initial, medial and final forms,
the vowel marks (harakat) and the digits, with words, meanings,
romanizations and sentences. The rules applied, as for the other apps:
- no NonCommercial or NoDerivatives licences;
- ShareAlike only where it covers the data and not the app's code;
- GPL, LGPL or AGPL data kept out of the app.

This is a licence reading, not legal advice. The full report, with every
count, URL and quote: `docs/research/arabic_sources.md`. The downloaded
licence texts, pages and samples are in `data/raw/research/ar/`
(git-ignored).

**Verdict: the app is viable.** As for Thai and Hindi, there is no usable
stroke data: we draw the strokes ourselves from well-attested school rules
(the Saudi Ministry of Education's Grade 1 primer shows each letter's four
forms with arrows; a published handwriting guide gives the rest; "body
first, then the dots, then the marks" is universal); the plan is below. The
dictionary side is strong: English Wiktionary gives vocalized headwords,
romanizations, roots and 1.45 million inflected forms, all CC BY-SA. The new
weak spot is the vowel marks: almost no open sentence is vocalized, and
beginners need them, so sentences need a vocalizing step and a human check
(Gaps).

| | Arabic (MSA, Naskh) |
|---|---|
| Stroke order | **None usable.** We draw ~60 letter bodies (17 shape families in their positional forms), the dots and marks, ~11 harakat and 10 digits, and build the ~120 letter forms and the words in code. The order follows the Saudi Grade 1 primer's arrows (لغتي, 2025) and *Mastering Arabic Script* |
| Words | English Wiktionary: 27,249 entries (47,005 senses; plus 50,886 inflected-form entries), **26,972 with vowel marks**, 26,672 romanized, 1.45 million inflected forms; roots on 11,309. French Wiktionary: 10,701 words |
| Sentences | Tatoeba: 68,570 (46,686 with English), **almost none vocalized** (91 fully), no usable audio; Common Voice: 59,160 CC0 sentences, 15,955 half-vocalized or more |
| Levels | None usable (Kelly's CEFR list is NC): by frequency |
| Frequency | wordfreq `large_ar` (620,701 words; surface forms, clitics attached) |
| Word lookup | Our clitic rules over Wiktionary's inflection index: 99.5% of the top 1,000 tokens and 96.6% of the top 10,000 found |
| Lessons | Wikivoyage phrasebook in en (MSA, usable), es and fr (thin), pt (empty), no ja; FSI "Classical Arabic – The Writing System" (1952) and "Modern Written Arabic" (public domain); LibreTexts CC BY courses |
| Google ML Kit | Handwriting (`ar`), **no photo-text model** (Apple's Vision reads Arabic from iOS 18, probably), translation (`ar`) |
| Voices | iOS/macOS Majed (`ar-001`), reads harakat, guesses unvocalized words; Android Google TTS (Arabic) |
| Weak spots | Vowel marks on sentences; es/pt/ja meanings; no graded list; no recorded audio |

---

## What the app uses

| Need | Source | Licence | Conditions |
|---|---|---|---|
| Stroke order and shapes | **Drawn by us** (plan below), following the **Saudi Ministry of Education's Grade 1 primer** لغتي (Part 2, 1447/2025: each letter's four forms on a baseline with arrows and a start circle) and Wightwick & Gaafar, *Mastering Arabic Script* (2005). Main rules: right to left; the joined body of each connected part in one stroke; then the dots, right to left; then the detached parts (the vertical of ط ظ, the top bar of medial ك, hamza, madda); harakat last; loops on the line turn clockwise from their left | Ours | The primer is a protected government work (Saudi law: 50 years from publication): reference only, never trace. The rules are "methods", unprotected (Art. 4(3)) |
| Free reference | FSI, "Classical Arabic – The Writing System" (Rice, 1952): every letter in its four forms, handwriting notes | US public domain | — |
| Teaching order | **Shape groups in hijāʾī order**, as *Alif Baa* does: ا ب ت ث, ج ح خ, د ذ, ر ز, س ش, ص ض, ط ظ, ع غ, ف ق, ك ل م ن ه و ي, with the six non-connectors (ا د ذ ر ز و) flagged; each group reuses one body. (The Saudi and Egyptian Grade 1 primers use a word-building order instead: Saudi م ب ل د ن ر first, Egypt ا ب م ح ج د خ ت) | Facts | — |
| Letter data, joining | **Unicode** 18.0: ArabicShaping.txt gives each letter's joining type (22 dual-joining with four forms; ا د ذ ر ز و and آ أ إ ؤ ة right-joining with two; ء none) | Unicode License v3 | Ship the notice |
| Words, vowel marks, romanization, English | **English Wiktionary (via kaikki.org)**: 27,249 entries, 47,005 senses; the vocalized headword on 26,972 (كِتَاب, كَتَبَ), romanization on 26,672 (kitāb), IPA on 17,800; inflection tables with 1,449,518 vocalized forms (288,069 distinct unvocalized) | CC BY-SA 4.0 | Attribution; publish the converted data. The per-language file (521 MB) is marked deprecated: fetch it now |
| Roots | **Wiktionary**: a root on 11,309 entries (2,816 roots; verbs 65%), the verb form (I–X), derivation links ("verbal noun of"); the 1,495 root pages via the API. **Arabic WordNet v2**: 14,683 lemma–root rows as a cross-check | CC BY-SA 4.0; CC BY-SA 3.0 | As above |
| Words, French | **French Wiktionary**: 10,701 words (596 of the 1,000 commonest) | CC BY-SA 4.0 | — |
| Words, es/pt/ja | Japanese 951, Portuguese 1,148, Spanish 659 Wiktionary words (~200 of the commonest 1,000 each); Wikidata's ~1,000 item-linked senses; the rest our own translation of the English, labelled | CC BY-SA 4.0; CC0 | Publish the translated glosses |
| Finding words in text | **Our own clitic rules** (و ف ب ل ك س أ ال; the pronoun suffixes; ة→ت, ى→ا; hamza folding) over Wiktionary's inflection index; the Snowball (BSD) or ISRI (Apache 2.0, NLTK) stemmer as a search fallback | Ours; BSD-3 / Apache 2.0 | Ship the stemmer's notice if used |
| Sentences | **Tatoeba**, 68,570 Arabic. Direct: en 46,686; es 3,081; fr 2,956; ja 1,210; pt 300. Through English: fr 18,044, es 14,715, ja 12,381, pt 12,371 | CC BY 2.0 FR | As in Kana; label pivot translations; vowel marks to add (below) |
| Vocalized sentences | **Common Voice** Arabic sentence list: 59,160, 15,955 at least half vocalized (no translations) | CC0 | Filter Qur'anic and religious text |
| More sentences | FLORES-200 `arb_Arab` and NTREX-128 `arb` (advanced, MSA, in es/pt/ja/fr) | CC BY-SA 4.0 | Credit Meta / Microsoft |
| Frequency | **wordfreq** `large_ar` (620,701 surface words): sum each lemma's forms after clitic stripping and hamza folding; FrequencyWords (OpenSubtitles) as a check | CC BY-SA 4.0 | As in the other apps |
| Levels | **None usable**: by frequency, with hand-made first lessons | — | — |
| Romanization | Wiktionary's (Hans Wehr-based: ʔ ʕ ḵ ḡ ṯ ḏ š ḥ ṣ ḍ ṭ ẓ ā ī ū), shown in a learner scheme close to ALA-LC by a fixed table (ʾ ʿ kh gh th dh sh), as `fude/lang/ar/lang.c`'s letter names already are | Ours; ALA-LC table is a US government work | Don't port Wiktionary's Lua module |
| Lessons | **Wikivoyage "Arabic phrasebook"** (en, 29.3 KB, MSA; vowel marks to add), es "Guía de árabe" (30.6 KB, machine-translated look), fr (23.2 KB); pt an empty template; **no ja**. **FSI "Classical Arabic – The Writing System"** (Rice, 1952) and "Modern Written Arabic" Vol. 1 (1969). LibreTexts "Introduction to Arabic" I–II and "Arabic Level One/Three/Four" | CC BY-SA 4.0; US public domain; CC BY 4.0 | Build with `tools/lectures/phrasebook.py`; check each LibreTexts book's licensing page |
| Fonts | **Noto Naskh Arabic** (printed model forms, vocalized text), **Noto Sans Arabic** (interface); Scheherazade New for large vocalized text; Aref Ruqaa to show Ruqʿah | OFL 1.1; Scheherazade reserves "Scheherazade" and "SIL" | Rename Scheherazade or IBM Plex if subset |

## Conditions

- **Wiktionary (all editions), FLORES, NTREX, wordfreq, FrequencyWords:**
  CC BY-SA 4.0, as for JMdict: attribution, the converted data in its own
  file, published openly. **Arabic WordNet:** CC BY-SA 3.0, the same.
- **Tatoeba:** CC BY 2.0 FR, attribution. **Common Voice sentences,
  Wikidata, tico-19:** CC0.
- **Stemmers, if shipped:** Snowball's BSD notice or NLTK's Apache 2.0
  notice.
- **ML Kit translation:** Google's attribution rules.
- **Saudi and Egyptian primers, *Mastering Arabic Script*:** facts (the
  order) only; no image or shape copied.

## MSA, not a dialect

The app teaches **Modern Standard Arabic**: the written standard of every
Arab country, the language of the school primers, books, press and signs,
and the only variety with a fixed spelling. The letters and the handwriting
are the same for the dialects. The sources fit: English Wiktionary's
"Arabic" is MSA (Classical and Qur'anic senses labelled, dialects separate);
Tatoeba's "ara" is Standard Arabic (dialects have their own codes); FLORES
`arb_Arab`, NTREX and the English Wikivoyage phrasebook ("deals with Modern
Standard Arabic") are MSA. wordfreq mixes in dialect words (Twitter,
subtitles), and **MASSIVE's ar-SA is largely Saudi/Gulf colloquial** (it is
left out). The Maghreb writes Western digits (0–9); the Arab East, which
the app follows, ٠–٩.

## Not usable

- Buckwalter Arabic Morphological Analyzer 1.0 (38,600 lemmas with English
  glosses): "License(s): GNU General Public License v2". Its successors
  BAMA 2.0 and SAMA 3.1: LDC licences.
- Arramooz, Qutrub, Qalsadi, Tashaphyne, Mishkal, Tashkeela2, arabic-roots,
  arabic-affixes, pyarabic (linuxscout): GPL ("This program is licensed
  under the GPL License").
- CAMeL Tools' MSA morphology databases: "GPL v2" (`calima-msa-r13`) and
  "LDC" (`msa-s31`); the code itself is MIT.
- Farasa: "made public for research purpose only. For non-research use,
  please contact us."
- FreeDict ara-eng / eng-ara (Arabeyes): "GNU General Public License ver.
  2.0 and any later version".
- PanLex: "Commercial use ... is permitted only by obtaining written
  permission".
- **Kelly project's Arabic CEFR list** (8,893 lemmas, A1–C2): "The lists are
  available under the CC BY-NC-SA 2.0 license."
- SAMER Readability Lexicon: "solely for your internal research and
  evaluation purposes ... no rights to ... further distribute".
- Tatoeba's 483 Arabic recordings: no licence given.
- Tanzil (Qur'an translations): "for non-commercial purposes only".
- KFGQPC fonts: "may not be reproduced, modified without the express
  written approval".
- LibreTexts "Elementary Arabic (Mohamed and Issa)" (labelled CC BY, but its
  MSU source says "Attribution-NonCommercial 4.0"), "Arabic Level Two"
  (CC BY-NC-SA), PDX "From MSA to CA" (NC), UT Austin's Tadriis (CC BY-NC)
  and Aswaat Arabiyya (TV clips).
- Wiktionary's "Frequency lists/Arabic5000": pasted in one edit with no
  source; provenance unclear.
- Hans Wehr's dictionary: commercial (its romanization *system* is free).
- MASSIVE `ar-SA` (16,521 utterances): CC BY 4.0, but largely Saudi/Gulf
  colloquial ("حط منبه بعد ساعتين من الحين"): not MSA; by hand at most.
- Online handwriting data: ADAB (CC BY on IEEE DataPort but "freely
  available for non-commercial research" per its authors, and behind an
  account; adult cursive town names), Online-KHATT (research; host gone),
  AltecOnDB (on request, no licence), Hijja (children's letters, offline,
  no licence), HMBD, Muharaf and Mendeley's numerals (NC).
- Arabic stroke and tracing projects on GitHub without a licence
  (moustafa-mk/arabisch-lernen-ios, akzize, Mohamed0khaled ...).

## Gaps

- Stroke data: ours to draw (planned). No source gives the digits'
  strokes, and Part 1 of the Saudi primer (18 of the letters) was not
  obtained: house rules, checked by a teacher.
- **Vowel marks on sentences.** Tatoeba has 91 fully vocalized Arabic
  sentences out of 68,570; the Wikivoyage phrasebook has 63 harakat in
  29 KB. Learners need vocalized text. Options, in order: vocalize each word
  from Wiktionary's vocalized forms where the word has only one reading;
  Common Voice's 15,955 vocalized CC0 sentences; a build-time vocalizer with
  a permissive licence (CATT, Apache-2.0 code; its weights and training
  data still to check; Mishkal is GPL), then a human check of every
  sentence shipped.
- es/pt/ja meanings: French is good (10,701 words), the others cover ~200 of
  the commonest 1,000 words; the rest through English, labelled, or ML Kit
  translation.
- No Japanese or Portuguese phrasebook; the Spanish one looks
  machine-translated.
- No open graded list.
- No ML Kit photo-text model (iOS 18+: Apple's Vision, probably; Android:
  Tesseract `ara`, Apache 2.0, or no feature).
- No reusable recorded audio (837 Wiktionary entries have Commons audio,
  per-file licences); the device voices read aloud.

## To test, not documented

- ML Kit `ar` handwriting: a lone letter in each positional form; dots,
  hamza and harakat drawn as separate strokes; a letter without its dots.
- Apple's Vision `supportedRecognitionLanguages()` on the oldest iOS we
  support (Arabic expected from iOS 18).
- The voice on iOS: Majed (`ar-001`) present? On macOS 14.4,
  `AVSpeechSynthesisVoice(language: "ar-SA")` returns Majed, so the code
  `fude/lang/ar/lang.c` uses works there; check iOS. Google TTS on Android:
  does it follow harakat?
- Playpen Sans Arabic with its shuffler off (`calt`/`rclt`) on CoreText.

## The stroke data: plan

The Thai and Hindi pattern (`apps/thai/tools/strokes/`): shapes drawn by
hand in code as parametric centreline strokes, the composed forms built by
rule, written as KanjiVG-format XML that Kana's bake reads
(`data/raw/ar/arabicvg.xml`, where `fude/lang/ar/bake.c` looks), checked by
a validator and contact sheets. Nothing is traced from a primer or a font;
Noto Naskh Arabic is only rendered beside the drawings to compare
proportions.

What is new for Arabic:
- **Four forms per letter**, each its own drawing, keyed by its Unicode
  presentation form (U+FE80–U+FEFC) as `fude/lang/ar/lang.c` and `chart.c`
  already do; the isolated form by the base letter.
- **Strokes that run left to right** inside a right-to-left script (the head
  of ج ح خ, the top bar of medial ك, the bottom of final ـه), and **strokes
  that go up and back down the same path** (final ـك, medial and final ل,
  the teeth of ـثـ): the matcher must accept a path that doubles back.
- **The baseline matters**: ر ز و ن ي ج ح خ ع غ م ل ق س ش ص ض go below it.
  The practice screen shows the primer's four lines.
- **Loop rotation** stored per stroke, as for Thai.

1. **What to draw:**
   - Bodies, by family and form (about 60): ا (isolated, final); the tooth
     of ب (4 forms; also ت ث, and the initial and medial ن ي ئ); ج ح خ (4);
     د ذ (2); ر ز (2); س ش (4); ص ض (4); ط ظ (4); ع غ (4); ف (4) and the bowl
     of ق (isolated, final); ك (4); ل (4); م (4); the bowl of ن (isolated,
     final); ه (4; also ة); و (2; also ؤ); ى ي (isolated, final); لا
     (isolated, final).
   - Detached parts, placed by rule: one, two and three dots above or below
     (ث ش: one above two); hamza above, below and alone (ء); madda; the
     vertical of ط ظ; the top bar of medial ك and the small mark of isolated
     and final ك.
   - Harakat: fatha, kasra, damma, sukun, shadda, the three tanwin, dagger
     alif.
   - Digits ٠–٩ (U+0660–0669, the Arab-East forms; not ۴ ۵ ۶).
   - Composed in code: the 100 forms of the 28 letters (22 × 4 + 6 × 2) and
     19 more for ء آ أ إ ؤ ئ ة ى لا; then words: each letter's form from its
     joining type and its neighbours; the bodies of each connected part
     merged into one stroke along the baseline; then its dots, right to
     left; then the detached marks; harakat after the word.
2. **The form:** the primer's school Naskh: print-like, separate dots (small
   rhombi), the teeth of س ش drawn, the four-line guide. Not Ruqʿah (taught
   later in school; shown at most as "how adults write").
3. **The order** (`arabic_sources.md` §1.4): body first (one stroke per
   connected part); dots right to left; detached parts; harakat last, shadda
   before its vowel. Per letter: start points and directions from the primer
   (ع ك خ ض ث ه seen with arrows) and *Mastering Arabic Script* (ط ظ ص ض م ك
   ل).
4. **House decisions** (sources disagree or are silent): initial ك in one
   stroke, top bar first (the primer; recommended) or the bar added last; the
   dot order in ت ث ش ق ي; the vertical of ط ظ after the loop (recommended);
   the rotation of the loops of ف ق و and of sukun; لا in two strokes or one;
   medial ه's shape and path; the direction of ك's small mark; every digit.
   Then how lenient the matcher is: joined dots (a dash for two, a cap for
   three) accepted; dots and harakat after each connected part or after the
   word; small loops either way.
5. **Checks:** a validator like Thai's (stroke counts against the per-letter
   table, start side, rotation, forms that meet at the baseline join
   cleanly); contact sheets beside Noto Naskh Arabic; an Arab Grade 1 teacher
   (Gulf or Egyptian) to review the animations and settle the house
   decisions before release.

## What we commit to publishing

As with the other apps, the converted ShareAlike data goes in a public
repository (which also answers the licences' DRM clauses):
- the Wiktionary extracts (English and the French, Spanish, Portuguese and
  Japanese editions), including the inflection index and roots;
- Arabic WordNet roots, if used;
- FLORES/NTREX (if used);
- the machine-translated glosses;
- the PDFs built from Wikivoyage and Wikibooks.

Our own stroke data's licence is the owner's choice (as for Hangul: CC BY-SA
4.0 or CC0). The app's code stays closed.

## Next

1. The stroke data (plan above): the letter bodies, then the forms, dots
   and marks, the harakat and the digits, with a validator and contact
   sheets; an Arab primary teacher to review the animations.
2. Fetch the kaikki files now (deprecated) and the Wiktionary root pages.
3. Decide how sentences get their vowel marks (Gaps) before the data build.
4. The data build (`apps/arabic/tools/data/`), then the app.
