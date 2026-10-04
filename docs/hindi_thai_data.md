# Hindi and Thai: the data and its licences

Checked 2026-10-04 against primary sources (licence files, official terms
pages, download pages, the live data), for paid, closed-source apps on iPad,
Android and phones. The rules applied, as for Chinese and Korean:
- no NonCommercial or NoDerivatives licences;
- ShareAlike only where it covers the data and not the app's code;
- GPL, LGPL or AGPL data kept out of the app.

This is a licence reading, not legal advice. The full reports, with every
count, URL and quote: `docs/research/hindi_sources.md` and
`docs/research/thai_sources.md`. The downloaded licence texts, pages and
samples are in `data/raw/research/hi/`, `data/raw/research/th/` and
`data/raw/research/common/` (git-ignored).

**Verdict: both apps are viable.** Neither language has usable stroke data:
for both we draw the strokes ourselves, as we did for the Hangul jamo, from
well-attested school rules (the plan is below). Everything else has an open
source, with the same conditions as the other apps (attribution, and
publishing the converted ShareAlike data).

| | Hindi (Devanagari) | Thai |
|---|---|---|
| Stroke order | **None usable.** We draw about 90 shapes (vowels, consonants, the four alphabet conjuncts, vowel signs, signs, digits, the र forms) and build half-forms, conjuncts and syllables in code. The order follows the Central Hindi Directorate's 2024 standard chart, cross-checked against open Commons animations | **None usable.** We draw ~86 glyphs (44 consonants, vowel signs, tone marks, signs, digits); the Ministry of Education's school form and rules are clear. One small MIT dataset (75 finger-traced characters) as a cross-check |
| Words | English Wiktionary: 24,524 entries (36,225 senses), romanized (schwa-deleted) and with IPA; Wikidata (CC0) for es/pt/fr/ja on ~1,500 senses | English Wiktionary: 17,629 words with tone romanization and IPA; **Volubilis**: 103,230 headwords with tones, en/fr/es/pt; LEXiTRON (English, examples, classifiers) |
| Sentences | Tatoeba: 16,475 (13,301 with English), no audio | Tatoeba: 6,851 (4,819 with English, 1,871 with Japanese), **588 with CC BY audio** |
| Levels | None official: by frequency | None official: by frequency, with Volubilis's A0/A1/A2 tiers |
| Frequency | wordfreq (26,653 words) | PyThaiNLP's Thai National Corpus list (106,122 words, CC0); wordfreq has no Thai |
| Word splitting | Spaces (plus nukta and variant folding) | No spaces: the device's tokenizers, and our own longest-match over open word lists |
| Lessons | Wikivoyage phrasebook in en, es, fr, pt (no ja); FSI (public domain); UT Austin (CC BY) | Wikivoyage phrasebook in en, pt, fr (no es, ja); FSI Thai (public domain) |
| Google ML Kit | Handwriting (`hi`), photo text (Devanagari model), translation (`hi`) | Handwriting (`th`), **no photo-text model** (Apple's Vision on iOS reads Thai), translation (`th`) |
| Voices | iOS Lekha (`hi-IN`); Android Google TTS (Hindi) | iOS Kanya (`th-TH`); Android Google TTS (Thai) |
| Weak spots | Japanese phrasebook; es/pt/ja/fr meanings beyond ~1,500 senses; one contributor wrote 62% of the Tatoeba sentences | Portuguese and Japanese meanings; no Spanish or Japanese phrasebook; tones to compute for words outside the dictionaries |

---

## Hindi

### What the app uses

| Need | Source | Licence | Conditions |
|---|---|---|---|
| Stroke order and shapes | **Drawn by us** (plan below), following the **Central Hindi Directorate's standard**, "देवनागरी लिपि एवं हिंदी वर्तनी का मानकीकरण" (2024), §2.5 "हिंदी वर्णमाला लेखन विधि": every letter's build-up and direction. Main rules: body first, then the vertical bar (top to bottom), the head line (शिरोरेखा) last, left to right; dots after the head line; marks above the head line before it, drawn upward | Ours | The CHD chart is © Government of India (60 years): reference only, don't copy or trace |
| Stroke cross-check | Wikimedia Commons, Category:Devanagari stroke order: Opiaterein's 28 consonant animations, Saurmandal's 13 vowel diagrams (with stroke counts), JackPotte's 44 animations | CC BY 3.0; CC BY-SA 3.0 | Used as facts only; nothing traced |
| Letter data and rendering rules | **Unicode** 18.0 (128 Devanagari characters; chapter 12's half-form, RA and nukta rules) | Unicode License v3 | Ship the notice |
| Words, romanization, English | **English Wiktionary (via kaikki.org)**: 24,524 entries (36,225 senses) + 14,696 inflected forms; romanization on 99.8% (IAST-like, **schwa-deleted**: कमल kamal), IPA on 94.2% | CC BY-SA 4.0 | Attribution; publish the converted data |
| Words, es/pt/fr/ja | **Wikidata lexemes**: sense-to-item labels in es 1,486, fr 1,525, ja 1,466, pt 1,363 senses. Plus fr/ja/pt Wiktionary (1,279 / 494 / 485 words) and Wiktionary's translation tables. The rest: our own translation of the English, labelled (as for Chinese es/pt) | CC0; CC BY-SA 4.0 | Publish the translated glosses |
| Typed romanized input | **Dakshina** (Google): 30,000 words with romanizations as people type them; Aksharantar's mined rows | CC BY-SA 4.0; CC0 / CC BY (drop its "Existing" rows) | — |
| Sentences | **Tatoeba**, 16,475 Hindi. Direct: en 13,301; ja 278; fr 78; pt 51; es 49. Through English: fr 7,102, es 6,533, ja 5,883, pt 5,589 | CC BY 2.0 FR | As in Kana; label pivot translations |
| More sentences | **MASSIVE** (short requests in hi, es, pt-PT, ja, fr); FLORES-200 and NTREX-128 (advanced) | CC BY 4.0; CC BY-SA 4.0 | Credit Amazon / Meta / Microsoft |
| Frequency | **wordfreq**: `small_hi`, 26,653 words | CC BY-SA 4.0 | As in the other apps; fold nukta and nasal-spelling variants |
| Levels | **None official** (no JLPT/HSK/CEFR list for Hindi). By frequency and theme | — | — |
| Word splitting | Spaces; lemma lookup through Wiktionary's form entries; NFC, nukta folding, ZWJ/ZWNJ stripped from search keys | — | — |
| Romanization | Our own learner scheme (IAST-like, with schwa deletion), tested against Wiktionary's; optional plain ASCII (Hunterian-like) | Ours | Don't port Wiktionary's Lua (CC BY-SA) into the code |
| Lessons | **Wikivoyage Hindi phrasebook** in en (54.4 KB), es "Guía de hindi" (48.9 KB), fr (19.8 KB), pt (14.1 KB); **no Japanese**. FSI "An Active Introduction to Hindi" (1966). UT Austin's dialogues and glossaries. Wikibooks (39 en pages) | CC BY-SA 4.0; US public domain; CC BY 4.0 | Build with `tools/lectures/phrasebook.py` |
| Fonts | **Noto Sans Devanagari** (interface); **Playpen Sans Deva** (a handwriting family made for learners) or **Kalam** (handwritten) | OFL 1.1, no reserved names | Rename Martel or IBM Plex if ever subset |

### Conditions

- **Wiktionary, Dakshina, FLORES, NTREX:** CC BY-SA 4.0, as for JMdict:
  attribution, the converted data in its own file, published openly.
- **Wikidata:** CC0, no conditions (credit anyway).
- **Tatoeba, MASSIVE, UT Austin:** attribution (CC BY).
- **CHD chart, NCERT, Commons animations:** facts (the order) only; no image
  or shape copied.
- **Romanization code:** written by us; Wiktionary's module only as a test
  oracle through its output.

### Not usable

- Hindi WordNet / IndoWordNet: "The Hindi WordNet and its API are released
  under GNU GPL 3.0 and the lexicon under GNU FDL"; commercial use by
  contacting CFILT.
- IIT Bombay English–Hindi corpus (including its 66,474-entry administrative
  dictionary and wordnet linkage): CC BY-NC 4.0.
- Shabdkosh: no republication "as part of any commercial service".
- Shabdanjali and FreeDict eng-hin: GPL.
- PanLex: now CC BY-NC-SA 4.0.
- Aksharamukha (transliteration): AGPL.
- Samanantar sentences (CC0 claimed for the packaging, cc-by-nc-4.0 on its
  card; mined text); TED2020 (NC-ND).
- MSU "Basic Hindi" course: CC BY-NC 4.0.
- NCERT and other government textbooks, the Hindi Teaching Scheme and Central
  Hindi Directorate material: government copyright (60 years); cite only.
- HP Labs India's online Devanagari datasets (characters and words; UNIPEN
  pen data): "The dataset can only be used for research purpose ... It can not
  be used for any commercial purposes whatsoever." IAPR TC11 Devanagari:
  "strictly intended for scientiﬁc purpose only".
- Devanagari stroke projects on GitHub without a licence (padhaipal's
  generated traces and others).

### Gaps

- Stroke data: ours to draw (planned); no source for the order of vowel
  signs (notably ि) or digits: house rules, checked by a teacher.
- es/pt/fr/ja meanings beyond ~1,500 senses: through English, labelled, or
  ML Kit translation on the device.
- No Japanese phrasebook.
- No official graded list.
- Tatoeba: one contributor wrote 62% of the sentences; only 49–278 direct
  translations into es/pt/ja/fr; no audio.
- No usable recorded audio beyond Wiktionary's 802 words (Commons, per file)
  and UT Austin's glossaries; reading aloud uses the device's voices.

### To test, not documented by Google

- Whether the `hi` handwriting model reads a lone vowel sign (ि ु), virama,
  anusvara or a half-form written alone; it is trained on words.
- Translation between two non-English languages goes through English.

---

## Thai

### What the app uses

| Need | Source | Licence | Conditions |
|---|---|---|---|
| Stroke order and shapes | **Drawn by us** (plan below), following the Ministry of Education's school form (แบบตัวอักษรกระทรวงศึกษาธิการ, "หัวกลมตัวมน", 1977) and the school rules: start at the head loop (only ก and ธ have none), one stroke without lifting the pen (except ญ ฐ ศ ษ ส: two), the loop drawn once | Ours | The MOE chart is a government work (protected 50 years from publication): use it as a reference, do not trace it. Rules are methods, not protected (Copyright Act s.6) |
| Stroke cross-check | littlefrog1973/writing-practice: 44 consonants, 21 vowel/tone signs, 10 digits as finger-traced point lists | MIT | Ship the MIT notice if any of it is used |
| Character names, classes, sounds, tone rules | Unicode names (THAI CHARACTER KO KAI ...); facts from Wikipedia's "Thai script" tables and Wiktionary's Thai romanization page | Unicode License v3; facts | Our own table |
| Words, tones, English | **English Wiktionary (via kaikki.org)**: 17,629 words; tone romanization on 17,480 ("paa-sǎa"), IPA, RTGS, 6,206 romanized examples | CC BY-SA 4.0 | Attribution; publish the converted data |
| Words, tones, en/fr/es/pt | **Volubilis Mundo** v26.2 (July 2026): 103,230 headwords; tones on 102,022 rows; English 106,642, French 68,966, Spanish 13,282, Portuguese 4,497 | CC BY-SA 4.0 ("VOLUBILIS MULTILINGUAL THAI DICT. & DATABASE by Belisan is licensed under CC BY-SA 4.0") | As above |
| Extra English, Thai examples, classifiers | **LEXiTRON 2.0** (NECTEC): 33,060 Thai and 54,492 English headwords | NECTEC 2003 licence: redistribution "with or without modification" permitted, commercial use included | **Credit line in the app**: "This product is created by the adaptation of LEXiTRON developed by NECTEC (http://www.nectec.or.th/)."; don't call anything "LEXiTRON"; keep the licence and disclaimer |
| Japanese | Japanese Wiktionary: 1,876 Thai words with Japanese glosses; Tatoeba tha–jpn | CC BY-SA 4.0; CC BY 2.0 FR | — |
| Sentences | **Tatoeba**, 6,851 Thai. Direct: en 4,819; ja 1,871; es 83; fr 50; pt 11. Through English: fr 3,153, es 2,974, pt 2,891, ja 2,504. **588 have audio under CC BY 4.0** | CC BY 2.0 FR; audio CC BY 4.0 | As in Kana; credit each recording |
| More sentences | **MASSIVE** (short requests in th, es, pt-PT, ja, fr); scb-mt-en-th-2020 (1.0M en–th pairs, dialogues and SMS among them) | CC BY 4.0; CC BY-SA 4.0 (Common Voice part CC0) | Credit Amazon / SCB |
| Frequency | **PyThaiNLP `tnc_freq.txt`**: 106,122 words from the Thai National Corpus; `ttc_freq.txt` (textbooks) | CC0 | Counts from a non-open corpus: low–medium risk |
| Levels | By frequency, adjusted with **Volubilis's LEVEL**: A0 2,018 words, A1 2,984, A2 5,913 | CC BY-SA 4.0 | One author's grading |
| Word splitting | Apple's NLTokenizer / Android's ICU BreakIterator for free text; our longest-match over PyThaiNLP `words_th.txt` (62,107 words) + ICU `thaidict.txt` (26,383) + our headwords | CC0; Unicode License v3 | Ship the Unicode notice |
| Romanization | The dictionaries' tone romanization; RTGS as a second line; our own rules for the rest | RTGS: a Royal Gazette announcement (1999), not copyrighted (s.7) | Call it "tone romanization", not "Paiboon" |
| Lessons | Wikivoyage Thai phrasebook in en (45.5 KB), pt (47.2 KB), fr (17.0 KB); FSI Thai Basic Course (1970, 2 volumes, 847 pages) | CC BY-SA 4.0; US public domain | Build with `tools/lectures/phrasebook.py` |
| Fonts | **Noto Sans Thai Looped** or **Sarabun** (the government's TH Sarabun New) for the interface; **Mali** ("inspired by a 6th graders' handwriting") or Playpen Sans Thai for a handwritten look. Never loopless fonts (Noto Sans Thai, Kanit, Prompt) for teaching | OFL 1.1, no reserved names | — |

### Conditions

- **LEXiTRON:** the acknowledgement line above in the app's credits; its
  copyright notice, licence and disclaimer in Settings › Licences; no
  product named "LEXiTRON". NECTEC "reserves the right to update the license
  agreement at any time without prior notice": keep the 2003 text with the
  data.
- **Volubilis:** its licence is on the project's blog, not in the file; keep
  the dated copy (saved) and credit "Volubilis, by Belisan (Francis Bastien),
  CC BY-SA 4.0".
- **Tatoeba audio:** the contributor's name with each recording.
- **ICU word list:** the Unicode notice.

### Not usable

- Thai online-handwriting corpus (NECTEC, 2003–04): reported as
  non-commercial; its pages are unreachable.
- chokwritethai's per-consonant SVGs and thai-notes.com's animations: no
  licence (used as references only).
- Longdo's website content ("personal and non-commercial use only") and the
  third-party dictionaries it shows (Hope: GPL; NSTDA glossary: NC).
  Longdo's own downloadable community data has a permissive licence and could
  be used if the files can be obtained.
- PanLex (now CC BY-NC-SA), thai-language.com / thai2english, JTDic, Saikam.
- LibThai's dictionary (LGPL); deepcut/attacut models (trained on NECTEC's
  BEST corpus, reported NC); PyThaiNLP's Royal Society word list (no licence).
- Thai-MNIST (CC BY-ND), ALICE-THI (research only).
- FrequencyWords' Thai list (unsegmented and broken).
- OBEC basic word lists: agency copyright; reference only.

### Gaps

- Stroke data: ours to draw (planned).
- Japanese: 1,876 dictionary words; Portuguese: 4,497 Volubilis rows and 11
  direct Tatoeba pairs. The rest through English, labelled, or ML Kit
  translation on the device.
- No Spanish or Japanese phrasebook.
- No official graded list.
- No ML Kit photo-text model (iOS: Apple's Vision; Android: Tesseract's
  Apache-2.0 `tha` model, or no feature).
- Tones must be computed for words outside Wiktionary and Volubilis (rules
  are well defined; irregular words need an exceptions list).

### To test, not documented

- Whether the `th` handwriting model reads a lone vowel sign or tone mark
  (◌ั ◌่) written alone, and stacked marks.
- That Apple's Vision returns Thai on the oldest iOS we support
  (`supportedRecognitionLanguages()`).
- Android's ICU BreakIterator on a device.

---

## The stroke data: plan

Both scripts follow the Hangul pattern (`apps/hangul/tools/strokes/`): base
shapes drawn by hand in code as parametric centreline strokes, the composed
forms built by rule, written as KanjiVG-format XML that Kana's bake reads,
checked by a validator and contact sheets. Nothing is traced from a chart or
a font; fonts are only rendered beside the drawings to compare proportions.

Two things differ from Hangul:
- **Stroke types.** KanjiVG's `kvg:type` holds a CJK stroke (㇐ ㇑ ...). Most
  Devanagari and Thai strokes are loops and curves with no CJK equivalent, so
  most strokes will have no type, as Hangul's ㅇ has none. The bake and the
  matcher must accept typeless strokes everywhere, or we add our own type
  vocabulary (head loop CW/CCW, headline, stem, ...).
- **Loop direction matters.** A Thai head loop is drawn clockwise or
  counter-clockwise depending on the letter, and Devanagari loops have a
  fixed direction too. The data must store it (the path's own direction
  already does), and the matcher should check it.

### Thai

1. **What to draw** (about 86 glyphs, about 100 strokes):
   - 44 consonants (ฃ ฅ included: obsolete, but in the 44 and on the school
     chart); plus the footless ญ and ฐ used above ุ ู ฺ.
   - Vowel signs ะ ั า ำ ิ ี ึ ื ุ ู เ แ โ ใ ไ ๅ ็, ฤ ฦ (and ฤๅ ฦๅ as
     compositions).
   - Tone marks ่ ้ ๊ ๋; signs ์ ๆ ฯ ํ ฺ.
   - Digits ๐–๙.
   - Syllables composed in code: leading vowel, consonant, vowel above or
     below, tone mark above (and above the vowel when there is one), right-
     aligned to the consonant (shifted left over ป ฝ ฟ). Writing order is
     visual: leading vowel first, tone mark last.
2. **The form:** the MOE school form, หัวกลมตัวมน, with the four-part grid
   proportions from the Phichit area office's guide (head = 1 part; width =
   half the height; ข ฃ ช ซ half width; ฎ ฏ ฐ two parts below the line;
   ไ ใ โ up to three parts above; full list in `thai_sources.md` §1.4.8).
3. **The order** (`thai_sources.md` §1.4, sources: DLTV, the BKY teaching
   manual, TruePlookpanya, the Phichit guide, thai-notes.com, the DLI course):
   - start at the head loop; ก starts at its bottom left, ธ at its top left;
   - one continuous stroke; second strokes only for the foot of ญ ฐ, the tail
     of ศ ส and the inner part of ษ;
   - the loop turns so the pen leaves it smoothly along the next line (the
     per-letter rotation is tabulated); store it per stroke.
4. **House decisions** (sources disagree): the direction of the tail of ศ
   and ส; the order of ๋'s two strokes; ึ in one stroke or two; ๙ in one or
   two; the circle of ำ and ํ; recommended: ฐ's body loop clockwise, ิ started
   at the right, ณ started at the bottom left. Then how lenient the matcher is
   (either rotation for tiny circles; a pen lift inside a one-stroke letter).
5. **Checks:** littlefrog's MIT data (counts, start quadrant, rotation per
   glyph) compared automatically; a Thai primary-school teacher to review the
   animations before release.

### Hindi

1. **What to draw:**
   - Vowels अ आ इ ई उ ऊ ऋ ए ऐ ओ औ (11), with अं अः and ऑ composed (अ/आ +
     sign).
   - Consonants क–ह (33), the four conjuncts the alphabet includes, क्ष त्र ज्ञ
     श्र (each its own drawing), ड़ ढ़; ळ optional (new in CHD 2024, Marathi
     rather than Hindi).
   - Signs: vowel signs ा ि ी ु ू ृ े ै ो ौ ॉ (and ॅ); virama ्, anusvara ं,
     chandrabindu ँ, visarga ः, nukta ़; avagraha ऽ; danda । ॥.
   - Digits ०–९ (Hindi forms of ५ ८ ९).
   - Half-forms, built by rule: the letters with a vertical bar (ख ग घ च ज झ
     ञ ण त थ ध न प ब भ म य ल व श ष स) drop it; क and फ drop half the hook;
     ङ छ ट ठ ड ढ ह take the virama instead (CHD §3.1). Special shapes drawn
     by hand: the three र forms (repha र्क, subscript क्र, the ट्र/ड्र caret),
     द्य द्व, and the few common stacked ligatures (द्ध, ट्ट, ड्ड, ह्म, ह्य).
   - Composed in code: the nukta letters (base + dot), consonant + vowel sign
     (the बारहखड़ी, with the special shapes रु रू and the attached ृ of हृ दृ), and
     conjuncts (half-form + full letter under one head line).
2. **The form:** the modern Hindi forms in the CHD chart (अ as "३" plus bar,
   झ, ण as U plus bar, Hindi ल श; knots in ग न म), not the Marathi/Nepali
   variants. Proportions on the CHD five-line grid (upper-sign line, head line,
   middle line, base line, lower-sign line), which is also the practice
   screen's guide.
3. **The order:** CHD §2.5 for every letter it shows (per-letter table in
   `hindi_sources.md` §1.4.2); Saurmandal's counts for the vowels; CHD in
   each of the six disagreements (ष ऋ ङ ऊ, the ऐ/ओ/औ mark, उ's start).
   - Body, top to bottom, in as few strokes as the chart's stages allow; the
     vertical bar top to bottom after the body to its left; the head line
     last, left to right (partial over थ ध भ and the right part of अ आ ओ औ);
     dots after it; marks above it before it, drawn upward.
   - The head line is a stroke of the letter in letter mode, and one stroke
     across the whole syllable or word in word mode.
4. **House decisions:** the order of the vowel signs, especially whether ि
   comes before or after its consonant (ask a teacher); whether ु ू ृ come
   before or after the head line; digit order; where the chart's stages leave
   stroke boundaries open; how lenient the matcher is about when the head
   line is drawn (real writers vary).
5. **Checks:** a validator like Hangul's; the Commons animations compared by
   eye on contact sheets; one or two Hindi primary teachers to review the
   animations before release.

---

## Both apps: what we commit to publishing

As with the other apps, the converted ShareAlike data goes in a public
repository (which also answers the licences' DRM clauses):
- the Wiktionary extracts (Hindi and Thai, all editions used);
- Volubilis;
- Dakshina (if used for romanized input), FLORES/NTREX and scb-mt-en-th (if
  used);
- the machine-translated glosses;
- the PDFs built from Wikivoyage and Wikibooks.

Our own stroke data's licence is the owner's choice (as for Hangul: CC BY-SA
4.0 or CC0). The app's code stays closed.

## Next

1. The stroke data, Thai first (smaller, single strokes, clear rules), then
   Hindi (base letters, then half-forms, conjuncts and the vowel-sign
   placements), each with its validator and contact sheets.
2. A Thai teacher and a Hindi teacher to review the stroke animations.
3. Fetch the kaikki per-language files now (marked deprecated) and the
   Volubilis xlsx; keep dated copies of the Volubilis and LEXiTRON licences.
4. The data builds (`tools/data/`), then the apps.
