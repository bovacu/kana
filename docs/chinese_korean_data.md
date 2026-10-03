# Chinese and Korean: the data and its licences

Checked 2026-10-03 against primary sources (licence files, official terms
pages, download pages), for a paid, closed-source iPad app. The rules applied:
- no NonCommercial or NoDerivatives licences;
- ShareAlike only where it covers the data and not the app's code;
- GPL or LGPL data kept out of the app.

This is a licence reading, not legal advice. The full reports, with every
count, URL and quote: `docs/research/chinese_sources.md` and
`docs/research/korean_sources.md`. The downloaded licence texts, pages and
samples are kept in `data/raw/research/` (git-ignored).

The apps' names: **Hanzi** (Chinese; `apps/hanzi`, `com.rde.hanzi`) and
**Hangul** (Korean; `apps/hangul`, `com.rde.hangul`).

**Verdict: both apps are viable.**

**Status (2026-10-03): both apps are built** — Mac and iPad Simulator; the iPad
needs a provisioning profile for each (com.rde.hanzi, com.rde.hangul). Their
data tools are `apps/hanzi/tools/data/` and `apps/hangul/tools/data/` (README
there), their commands in COMMANDS.txt (HANZI, HANGUL). Still open:
- Hanzi's converted stroke data, under the Arphic licence (below): prepared in
  github.com/bovacu/hazi_custom_stroke_data (the repository beside this one), to
  push; the app's licence text and the strokes file's notice link to it.
- 50 of Korea's 1,800 education hanja are written in their Korean forms (內 靑
  德 說 ...), which KanjiVG does not draw (it has 内 青 徳): those need strokes
  of our own.
- Words spelt alike share one wordfreq count, and Korean's counts are by stem:
  a list can show the rarer of two (가지 "eggplant" before "kind").
- Hanja meanings in Spanish, Portuguese and French come from KANJIDIC2 by the
  exact character only; its variant references could fill the traditional
  forms (學 價 樂).

| | Chinese (Mandarin) | Korean |
|---|---|---|
| Stroke order | Make Me a Hanzi: 9,574 characters, Simplified and Traditional, covering all 3,088 HSK characters. Arphic licence: usable with conditions (below). | No usable data exists for Hangul. We draw the ~40 jamo ourselves and build the 11,172 syllables in code; hanja strokes come from KanjiVG, which Kana already has. |
| Words | CC-CEDICT, 125,166 entries (English) | NIKL Basic Korean Dictionary (krdict): 53,671 headwords, with meanings in English, Japanese, French and Spanish |
| Sentences | Tatoeba: 89,276 Mandarin sentences (66k with English) | Tatoeba: 15,941 Korean sentences (10.7k with English) |
| Levels | HSK 2025 syllabus, levels 1–9 | krdict's own beginner / intermediate / advanced tags |
| Lessons | Wikivoyage phrasebook in all five languages, plus Wikibooks | Wikivoyage phrasebook in all five languages, plus Wikibooks |
| Google ML Kit | Handwriting (`zh-Hani-CN` / `-TW` / `-HK`), photo text (one model reads both scripts), translation (`zh`) | Handwriting (`ko`), photo text (Korean model), translation (`ko`) |
| Weak spots | Spanish and Portuguese meanings | Portuguese; no translated example sentences graded by level |

---

## Chinese

### What the app uses

| Need | Source | Licence | Conditions |
|---|---|---|---|
| Stroke order and shapes | **Make Me a Hanzi `graphics.txt`** (or its cleaned copy, hanzi-writer-data). Has both the outline and the centreline of each stroke, in mainland (PRC) order. | Arphic Public License | See "The Arphic conditions" below |
| Taiwan stroke order | AnimCJK `graphicsZhHant` for 1,013 characters (all HSK 1–3 traditional), plus Taiwan's CNS11643 stroke sequences to flag the rest | Arphic; Taiwan Open Government Data License v1 | Attribution: 數位發展部，CNS11643中文標準交換碼全字庫網站 |
| Pinyin, short meanings, strokes, radicals, Simplified ↔ Traditional | **Unihan** (Unicode 18.0): all 3,088 HSK characters complete | Unicode License v3 | Ship the notice |
| Script conversion, Taiwan forms | **OpenCC** tables | Apache 2.0 | Ship the licence |
| Character components | **BabelStone IDS**: 97,680 decompositions | The maintainer waives his rights ("for personal or commercial purposes without asking permission") | Partly based on GPL data; low risk |
| Words, English | **CC-CEDICT**, 125,166 entries | CC BY-SA 4.0 | Same as JMdict in Kana: attribution, and publish the converted data |
| Words, French | **CFDICT**, about 85,000 entries | CC BY-SA 3.0 | **Credit must also appear on the App Store page** |
| Words, Japanese | Japanese Wiktionary (via kaikki.org), 59,491 senses | CC BY-SA 4.0 | As above |
| Words, Spanish and Portuguese | No open dictionary exists. KANJIDIC2 character meanings (es 58%, pt 50% of HSK characters), plus our own machine translation of CC-CEDICT glosses, labelled as such | CC BY-SA 4.0 | Publish the translated glosses |
| Sentences | **Tatoeba**, 89,276 Mandarin. Translations: en 66,272; fr 19,294; ja 14,516; es 10,743; pt 1,400. More through English | CC BY 2.0 FR | As in Kana |
| Levels | **HSK 2025 syllabus** (released 2025-11, used in exams from 2026-07): 11,000 words, 3,088 characters to read, 1,200 to write, levels 1–9. Store only a level tag per word, checked against the official PDF | No licence stated | Low to moderate risk (below) |
| Word frequency | wordfreq (includes SUBTLEX-CH, with the authors' permission for any use) | CC BY-SA 4.0 | Credit SUBTLEX |
| Lessons | Wikivoyage Chinese phrasebook in en, es, fr, ja and pt (the Portuguese one is not linked from the English page). Wikibooks "Chinese (Mandarin)": 139 pages. FSI "Standard Chinese" (US government, likely public domain; check title pages) | CC BY-SA 4.0; public domain | Build with `tools/lectures/phrasebook.py` |
| Fonts | Noto Sans SC/TC (interface); LXGW WenKai GB/TC and TW-Kai (model characters) | OFL 1.1 | **Rename LXGW if subset** (reserved names) |

### The Arphic conditions (stroke data)

The Arphic licence counts converted data as part of "the Font". Its
modification clause asks for two things:
- "a prominent notice in each modified file stating how and when you changed
  that file";
- modifications "Freely Available ... to all third parties", where "Freely
  Available" refers to freedom, "not price".

Its aggregation clause keeps the app out:
> mere aggregation of another work not based on the Font with the Font on a
> volume of a storage or distribution medium does not bring the other work
> under the scope of this License.

So the app must:
1. Keep the strokes in their own bundled file, not merged with other data.
2. Put a header in that file saying how and when it was changed.
3. Ship `ARPHICPL.TXT` unaltered and show it in Settings › Licences.
4. Publish the converted stroke file under the Arphic licence (a public
   GitHub repository). Other apps already do this:
   jwsanders76/juzigenius-data and NINSTASS/chinese-app-stroke-data.
5. Say that the stroke data stays under its licence, outside the app's terms.

### Not usable

- Make Me a Hanzi `dictionary.txt` and AnimCJK's dictionaries: LGPL 3.
- CHISE IDS and CJKVI-IDS: GPL.
- Taiwan MOE stroke order and dictionaries: NonCommercial and NoDerivatives.
- Chinese Grammar Wiki: NonCommercial.
- Every university Mandarin course found (Open Textbook Library and others): NonCommercial.
- TOCFL lists (Taiwan's test): all rights reserved.
- Jun Da's frequency list: all rights reserved.
- Tatoeba audio: not reusable.

### Gaps
- No large open Spanish or Portuguese dictionary.
- Taiwan stroke order beyond about 1,000 characters.
- No recorded audio; reading aloud uses the iPad's voices.
- No openly licensed TOCFL lists.
- Etymology hints must be our own (the existing ones are LGPL).

### To test, not documented by Google
- Which script the handwriting models return: CN presumably Simplified, TW and HK Traditional.
- Translation returns `zh` in one script only. Convert it with OpenCC for Traditional mode.
- Translation between two non-English languages goes through English.

---

## Korean

### What the app uses

| Need | Source | Licence | Conditions |
|---|---|---|---|
| Jamo stroke order | **Drawn by us.** No machine-readable Hangul stroke data exists under a usable licence. NIKL confirms jamo stroke order is not regulated; schools teach top to bottom, left to right. Reference: the public-domain diagrams in Wikimedia Commons' "Hangeul stroke order" category | Ours | House choices needed for ㅈ ㅊ ㅎ, ㄱ and ㅗ |
| Syllables | Composed in code: jamo placed by layout (vowel position × final consonant), all 11,172 | Ours | — |
| Dictionary | **krdict** (NIKL Basic Korean Dictionary): 53,671 headwords, 73,636 senses. Translations in 11 languages including **en, ja, fr, es** (no Portuguese). Hanja origin, pronunciation, Korean-only examples. Full XML/JSON download | **CC BY-SA 2.0 KR**: "누구나 상업적 용도까지 포함하여 자유롭게 이용할 수 있으며" (anyone may use it freely, including commercially) | Attribution "국립국어원 한국어기초사전, CC BY-SA 2.0 KR"; publish the converted data. **Its audio and images are excluded** |
| Extra English | English Wiktionary (via kaikki.org), 57,252 Korean words | CC BY-SA 4.0 | As above |
| Sentences | **Tatoeba**, 15,941 Korean. Direct translations: en 10,727; ja 2,154; fr 2,005; es 1,015; pt 202. Through English: ja 6,329, fr 7,214, es 5,564, pt 4,749 (to be checked) | CC BY 2.0 FR | As in Kana |
| Levels | **krdict's grades**: 초급 (beginner) 2,545, 중급 (intermediate) 9,068, 고급 (advanced) 37,024 | CC BY-SA 2.0 KR | As above |
| Better levels (optional) | NIKL 2017 list: 10,635 words in levels 1–6. NIKL 2003 learner list: 5,965 words, graded A–C | KOGL Type 1, **but NIKL requires permission for use abroad** (below) | Ask NIKL |
| Frequency | Now: FrequencyWords `ko_50k` or wordfreq. With NIKL permission: NIKL 2002/2005 studies, which include **syllable and jamo statistics** | CC BY-SA 4.0; KOGL Type 1 | — |
| Hanja | **Unihan**: the official 1,800 education hanja (kKoreanEducationHanja) with readings and English meanings. **libhangul `hanja.txt`**: Korean meaning words (훈음, e.g. 하늘 천) for 1,795 of the 1,800. **KANJIDIC2**: es/fr/pt meanings. **KanjiVG**: strokes for all 6,293 characters with Korean readings | Unicode; BSD-3 (file header); CC BY-SA 4.0; CC BY-SA 3.0 | Credits; Korean stroke order may differ from Japanese for a few characters |
| Romanization | Revised Romanization, 고시 2014-42. Government notices are not copyrighted (Copyright Act art. 7) | — | Our own code, plus a small exceptions list |
| Lessons | Wikivoyage Korean phrasebook in en, es, fr, ja and pt. Wikibooks Korean (en 75 pages). NIKL textbooks (세종한국어 증보판 1–8) were moved to KOGL Type 1 in Feb 2026, **but need the same NIKL permission** | CC BY-SA 4.0; KOGL Type 1 | — |
| Fonts | Noto Sans KR (interface); Nanum Pen Script, Gaegu and others (handwritten look) | OFL 1.1 | **Rename Nanum if subset** |

### KOGL and use abroad (a NIKL catch)

Korea's public licence (KOGL, 공공누리) Type 1 allows commercial use, but
NIKL's FAQ says:

> 공공누리는 국민을 대상으로 하는 제도로, 국내법이 적용되는 곳에 한하여 해당
> 조건으로 사용이 가능합니다. 그 외의 장소에서의 저작물 활용은 유형에 상관없이
> 저작권자의 이용 허락이 필요합니다.

In English: KOGL is meant for Korean nationals and applies only under Korean
law; use elsewhere needs the rights holder's permission, whatever the type.

This affects NIKL's lists, frequency studies and textbooks. It does **not**
affect krdict and NIKL's other dictionaries, which use Creative Commons. The
app does not need any of the KOGL sources. They would make it better (six
levels, jamo statistics, a real course), so a permission request to NIKL is
worth sending, like the letters in `apps/kana/docs/lectures_permission.md`.
The request form is under 한국어교수학습샘터 › 교재 › 저작물 이용허락 신청.

### Not usable
- KOGL Types 2, 3 and 4 (including the 세종학당 / King Sejong Institute materials and the older NIKL curriculum lists).
- TOPIK past papers: commercial use needs NIIED's permission.
- The official TOPIK list: all rights reserved.
- AI Hub, 모두의 말뭉치 and the Sejong corpora: research-only, Korean nationals only.
- Every Korean course found on the Open Textbook Library and LibreTexts: NonCommercial.
- krdict's audio.
- Hangul stroke projects on GitHub with no licence.

### Gaps
- Hangul stroke data: ours to make (planned anyway).
- Portuguese: no dictionary translations, and only 202 Portuguese-linked sentences.
- No example sentences that are both translated and graded by level.
- No open TOPIK list.
- No usable audio.

---

## Both apps: what we commit to publishing

As with Kana's data, the converted ShareAlike data goes in a public repository,
which also covers the licences' DRM clauses:
- CC-CEDICT and CFDICT;
- krdict;
- the Wiktionary extracts;
- the machine-translated glosses;
- the Arphic stroke file;
- the PDFs built from Wikivoyage and Wikibooks.

The app's code stays closed.

## Next

1. **Step 0** (`docs/architecture.md`, "The language layer"): Kana's Japanese
   parts behind `lang.h`, with Kana's output identical.
2. **Chinese**: the data build first, then the app.
3. **Korean**: the jamo strokes and syllable composition first, then the data
   build, then the app.
4. Send NIKL a permission request for the Korean extras.
