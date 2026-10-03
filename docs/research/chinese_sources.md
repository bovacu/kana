# Mandarin data sources: licence research (2026-10-03)

The full research behind `docs/chinese_korean_data.md`. It is checked against
primary sources: licence files, official pages and the live data. Counts were
measured on the live files unless marked. Anything not confirmed at the source
says "unverified". The downloaded licence texts, Make Me a Hanzi files, AnimCJK
dictionaries, the HSK 2025 syllabus PDF and the Tatoeba exports are kept in
`data/raw/research/zh/` and `data/raw/research/common/` (git-ignored).

The target: a paid, closed-source iPad app. ShareAlike is acceptable only on
the data. NonCommercial and NoDerivatives licences are excluded. GPL and LGPL
data is kept out of the app.

## 1. Stroke-order geometry

| Source | Covers | Licence | Paid closed-source app? | Verdict |
|---|---|---|---|---|
| **Make Me a Hanzi `graphics.txt`** (skishore) | 9,574 characters, simplified and traditional. SVG outline per stroke plus a median (centreline), in order, on a 1024 grid. All 3,088 HSK 2025 characters. 3,085 of the 3,088 Taiwan-form traditional counterparts (missing 汙 溼 譁). PRC stroke order | Arphic Public License 1999: github.com/skishore/makemeahanzi/blob/master/APL/english/ARPHICPL.TXT (the repo `COPYING` assigns it to graphics.txt) | Yes, with conditions (below) | **Primary source** |
| Make Me a Hanzi `dictionary.txt` | 9,574 entries: definition, pinyin, decomposition, radical, etymology (9,033), stroke-to-component matches | LGPL v3+ | Unclear, avoid | Keep out of the app |
| **hanzi-writer-data** (chanind) | 9,575 JSON files (npm 2.0.1): Make Me a Hanzi plus small fixes (cut-off strokes) | APL (its ARPHICPL.TXT) | Yes, with APL conditions | Alternative packaging |
| AnimCJK (parsimonhi) | `graphicsZhHans`: 8,014 characters. `graphicsZhHant`: 1,013 (907 HSK 3.0 levels 1–3 traditional), Taiwan-style order and glyphs | APL for "files prefixed by 'graphics' and SVG files"; LGPL for all other files (licenses/COPYING.txt) | Graphics: yes (APL). Dictionaries: no | ZhHant as Taiwan-order overrides |
| HanziVG (Connum) | 399 verified hanzi; inactive since 2023 | No repo licence | Unclear | Not viable |
| g0v zh-stroke-data / Taiwan MOE 筆順學習網 | Taiwan MOE order | MOE: 「…不得用於商業用途」; animations CC BY-NC-ND 3.0 TW | No | Excluded |
| CNS11643 全字庫 open data | `CNS_strokes_sequence.txt`: ~94,900 code points as stroke digits 1–5 (no geometry). TW-Kai and TW-Sung fonts | Data: Open Government Data License v1 (data.gov.tw/license). Fonts: OFL 1.1 | Yes; attribution 「數位發展部，CNS11643中文標準交換碼全字庫網站」 | To check and reorder strokes to Taiwan order |
| Wikimedia Commons Stroke Order Project | Raster only (1,061 PNG, 288 red PNG, 573 GIF) | Per file: in a sample of 200, 190 CC BY 3.0, 5 CC BY-SA 3.0, 4 GPL, 1 PD | Mixed | Not usable as geometry |
| Tomoe | Handwriting samples | LGPL 2.1+ (unverified upstream) | Unclear | Avoid |

### Arphic Public License: the decisive text

- **Converted data counts as "the Font":** "'Font' means the TrueType fonts …
  and the derivatives of those fonts created through any modification
  including modifying glyph, reordering glyph, converting format, changing font
  name, or adding/deleting some characters."
- **§1:** "You may copy and distribute verbatim copies of this Font in any
  medium, without restriction, provided that you retain this license file
  (ARPHICPL.TXT) unaltered in all copies."
- **§2, modification:**
  - "(a) You must insert a prominent notice in each modified file stating how
    and when you changed that file."
  - "(b) You must make such modifications Freely Available as a whole to all
    third parties under the terms of this License…"
  - "Freely Available" means "…not price. If you wish, you can charge for this
    service."
- **Aggregation, which keeps the app's code out:** "If identifiable sections of
  that work are not derived from the Font, and can be reasonably considered
  independent and separate works in themselves, then this License and its
  terms, do not apply to those sections when you distribute them as separate
  works. Therefore, mere aggregation of another work not based on the Font with
  the Font on a volume of a storage or distribution medium does not bring the
  other work under the scope of this License."
- **§5:** "You may not impose any further restrictions on the recipients'
  exercise of the rights granted herein."

To comply:
1. Keep the strokes in their own bundled file.
2. Put a header in it saying how and when it changed.
3. Ship ARPHICPL.TXT unaltered and show it in Licences.
4. Publish the converted file under the APL. Two apps already do:
   jwsanders76/juzigenius-data (9,531 characters) and
   NINSTASS/chinese-app-stroke-data.
5. State that the stroke data stays under the APL, outside the app's terms.

§2(c), about interactive programs, does not apply to data.

## 2. Character data

| Source | Covers | Licence | OK? | Verdict |
|---|---|---|---|---|
| **Unihan** (Unicode 18.0, Aug 2026) | See below | Unicode License v3 (unicode.org/license.txt): the notice must appear "with all copies … or … in associated Documentation" | Yes | **Use** |
| **OpenCC** | STCharacters 4,013, TSCharacters 4,149, STPhrases 49,239, TWPhrases 819, TWVariants 41 | Apache 2.0 (no NOTICE file) | Yes | **Use** |
| **BabelStone IDS** (Andrew West) | 97,680 decompositions (Unicode 16.0, 2025-06-27) | Waiver: "anyone is free to make use of the IDS data … for personal or commercial purposes without asking permission or providing attribution". It is partly "based on IDS data provided by Kawabata Taichi" (whose ids-db.el is GPLv3) | Yes; low residual risk | **Use** for components |
| CHISE IDS | Large IDS set | GPL v2+ | No | Excluded |
| CJKVI-IDS | IDS | Derived from CHISE; "All other data are distributed uner GPLv2" | No | Excluded |
| Make Me a Hanzi dictionary.txt; AnimCJK dictionaries | Decomposition, etymology hints | LGPL 3+: §4 "do not restrict modification … and reverse engineering", §4(d) relinking, §4(e) "Provide Installation Information" | Unclear | Avoid; write our own hints |
| CNS11643 properties | Pinyin, strokes, radicals, components, stroke sequences (traditional-centred) | OGDL v1, "compatible with the Creative Commons Attribution License 4.0 International" | Yes | Taiwan components |
| Commons "Chinese characters decomposition" | Decomposition table | CC BY-SA 4.0 | Yes | Optional |
| Jun Da character frequency | Frequency | "Copyright. 1998-2026. Jun Da." (no licence) | Unclear | Avoid |
| SUBTLEX-CH | Frequency | "freely available for research purposes" | Unclear | Only through wordfreq |
| **wordfreq** (zh) | Word frequency, frozen about 2021 | Data CC BY-SA 4.0. SUBTLEX: "obtained permission … to be used for any purpose" | Yes | **Use**; credit SUBTLEX |
| KANJIDIC2 (in Kana) | Meanings for HSK characters (direct or via traditional/Japanese form): en 3,040, es 1,784, pt 1,528, fr 1,588 of 3,088 | CC BY-SA 4.0 | Yes | Partial es/pt/fr; Japanese senses |

Unihan in detail:
- **Counts:** kMandarin 44,355; kDefinition 23,310; kTotalStrokes and kRSUnicode 102,999; kSimplifiedVariant 7,291; kTraditionalVariant 6,840.
- **HSK coverage:** all 3,088 HSK 2025 characters have pinyin, a definition, strokes and a radical.
- **Two-value readings:** for kMandarin, "When there are two values, then the first is preferred for zh-Hans (CN) and the second … for zh-Hant (TW)".
- **No frequency:** kFrequency is gone.

## 3. Dictionary words

| Source | Covers | Licence | OK? | Verdict |
|---|---|---|---|---|
| **CC-CEDICT** | 125,166 entries (2026-10-01), English, simplified, traditional, pinyin | CC BY-SA 4.0 (file header) | Yes | **English core** |
| **CFDICT** (French) | 240,487 translations, ~85,000 entries (count unverified) | CC BY-SA 3.0: 「…sous la réserve de mentionner l'endroit où vous avez obtenu les données (…dans votre application ET sur les pages de présentation de cette dernière)…」 | Yes; **credit on the store page too** | **French** |
| CxDICT / CFDICT-Next | French with LLM fill, English, HSK3-level Chinese explanations | Data CC BY-SA 4.0 (the code is NC, not needed) | Yes (data only) | Optional |
| Wiktionary via kaikki.org | en: 307,224 Chinese word forms. fr: 36,773 senses. **ja: 59,491 senses**. pt: 3,721. es: none | CC BY-SA 4.0 / GFDL | Yes | **Japanese**; extra French |
| Spanish / Portuguese | None found that is open and commercial-OK | — | — | **Gap** |
| moedict (Taiwan MOE) | Chinese-only | CC BY-ND 3.0 TW | No | Excluded |

## 4. Example sentences

Tatoeba: "These files are released under CC BY 2.0 FR."
- **Mandarin sentences:** 89,276 on the stats page; 89,102 in the 2026-09-26 export. About 49k were written in simplified and 40k in traditional; Tatoeba gives automatic transcriptions and pinyin.
- **CC0:** 1 sentence.
- **Direct links to translations:**
  - English: 66,272 sentences / 78,222 links
  - French: 19,294
  - Japanese: 14,516
  - Spanish: 10,743
  - Portuguese: 1,400
  - More are reachable through English.
- **Audio:** 5,826 sentences have it, but none is usable. 5,742 have an empty licence ("you may not reuse the audio outside the Tatoeba project") and 84 are NC.

Elsewhere: Wiktionary usage examples are CC BY-SA (count unverified). Chinese Grammar Wiki is CC BY-NC-SA: no.

## 5. Level lists

**The HSK 2025 syllabus** (official PDF:
hsk.cn-bj.ufileos.com/3.0/新版HSK考试大纲1219.pdf; 「中外语言交流合作中心 发布
2025-11 发布 2026-07 实施」, watermark 汉考国际):
- **11,000 numbered words:** L1 1–300, L2 –500, L3 –1,000, L4 –2,000, L5 –3,600, L6 –5,400, L7–9 –11,000.
- **3,088 recognition characters:** 246 / 125 / 284 / 441 / 431 / 413 / 1,148.
- **1,200 handwriting characters:** L1–2 100, L3–L6 150 each, L7–9 500.
- **It differs from the 2021 lists:** level 1 is now 300 words, not 500.

| Source | Status | OK? | Verdict |
|---|---|---|---|
| HSK 2.0 official lists (Hanban) | No open licence | Unclear | Legacy |
| GF 0025-2021 (MOE + 国家语委): 11,092 words, 3,000 characters, 1,200 handwriting | 「作为国家语委语言文字规范」. PRC Copyright Law art. 5 excludes 「国家机关的决议、决定、命令和其他具有立法、行政、司法性质的文件」 (arguable here) | Probably yes | Facts usable |
| elkmovie/hsk30, ivankra/hsk30 (2021 lists) | MIT for the compilation | Yes | Clean 2021 data |
| 2025 syllabus PDF (CLEC) | No copyright notice; CLEC is not a legislative body | Unclear (low–moderate risk) | Store only a level tag per word, checked against the PDF; credit the source |
| krmanik/HSK-3.0 (2025) | Labelled CC BY-SA 4.0 by its compiler | Same caveat | Cross-check |
| TOCFL 華語八千詞; NAER 三等七級 | "All rights reserved" | No / unclear | Skip or ask |

## 6. Lessons (PDFs)

| Source | Covers | Licence | OK? |
|---|---|---|---|
| **Wikivoyage phrasebooks** | See below | CC BY-SA 4.0 | **Yes, all five** |
| **Wikibooks "Chinese (Mandarin)"** | en 139 subpages; fr root page; ja and es tiny | CC BY-SA 4.0 | Yes (curate the English) |
| FSI "Standard Chinese: A Modular Approach" | 22 PDFs, 2,626 pages, plus audio (1970s–80s) | US Government work, likely public domain | Probably (check title pages) |
| Open Textbook Library: Elementary Mandarin, Elementary Chinese I, Mastering Mandarin Sounds | — | NC | No |
| Taiwan OCAC 全球華文網 | Course books | Non-profit only (unverified) | No |

The Wikivoyage phrasebooks:
- English "Chinese phrasebook", 76.8 KB.
- Spanish "Guía de chino", 16.3 KB.
- French "Guide linguistique mandarin", 27.8 KB. ("…chinois" is a 366-byte stub.)
- Japanese 「中国語会話集」, 45.3 KB.
- Portuguese "Guia de conversação mandarim padrão", 79.1 KB. It is not linked from the English page.

## 7. Fonts

| Font | Licence | Notes |
|---|---|---|
| Noto Sans SC / TC | OFL 1.1, no reserved names | Interface |
| LXGW WenKai / GB / TC | OFL 1.1, Reserved Font Names 「霞鹜, 霞鶩, 落霞孤鹜, 落霞孤鶩, LXGW」 | **Rename if subset**. GB has mainland standard forms; TC has traditional |
| TW-Kai (全字庫正楷體) | OFL 1.1 (「免費地使用、研究、複製、連結、嵌入、修改、散布與販售字型軟體」) | Taiwan-standard Kai |
| MOE 標準楷書 | CC BY-ND 3.0 TW | Excluded |

## 8. Google ML Kit

- **Digital Ink Recognition:**
  - Tags: `zh-Hani`, `zh-Hani-CN`, `zh-Hani-HK`, `zh-Hani-TW`, each also with `-x-gesture`.
  - They are described by region, not script. The output script is not documented: expect Simplified from CN and Traditional from TW/HK, and test it.
  - The language packs download at runtime.
- **Text Recognition v2:** "中文 Chinese zh — Hans/Hant; supported in v2". One model reads both scripts.
- **Translate:**
  - Code `zh` only, output script undocumented (reportedly Simplified, unverified): convert with OpenCC for Traditional.
  - "ML Kit's translation models are trained to translate to and from English. When you translate between non-English languages, English is used as an intermediate translation".
  - Google's attribution requirements apply.

## Best combination

- **Strokes:**
  - Make Me a Hanzi graphics (or hanzi-writer-data), with the APL compliance steps above.
  - Taiwan mode: AnimCJK ZhHant overrides, plus CNS11643 sequences to flag the rest.
- **Characters:** Unihan, OpenCC, BabelStone IDS and CNS components. Write our own etymology hints.
- **Meanings:**
  - en: CC-CEDICT and Unihan.
  - fr: CFDICT (plus CxDICT and French Wiktionary).
  - ja: Japanese Wiktionary (plus KANJIDIC2 readings as a hint).
  - es/pt: KANJIDIC2, plus our own labelled machine translation of CC-CEDICT glosses.
- **Sentences:** Tatoeba, pivoting through English.
- **Levels:** HSK 2025 tags; the 2021 and 2.0 levels as legacy tags.
- **Lessons:** Wikivoyage in five languages, Wikibooks (en) and FSI excerpts.
- **Fonts:** Noto Sans SC/TC for the interface; LXGW WenKai GB and TW-Kai for model characters.
- **ML Kit:** `zh-Hani-CN` / `zh-Hani-TW`, Text Recognition v2 Chinese, Translate `zh` plus OpenCC.

## Gaps

- An open Spanish or Portuguese dictionary.
- Taiwan stroke geometry beyond ~1,013 characters.
- Non-APL stroke geometry at scale.
- Commercial-OK courses beyond Wikibooks.
- Usable audio.
- Mandarin–Portuguese sentence pairs (1,400).
- Open TOCFL lists.
- Non-LGPL etymology.
- A clearly commercial character-frequency list, other than wordfreq or our own counts.

## Risks (with confidence)

1. **APL compliance as above is sufficient.** High that the obligations are as stated; medium-high that this is enough.
2. **APL §5 vs App Store terms** (the same wording that underlies the GPL/App Store conflict). Mitigation: publish the data separately and add an EULA carve-out. Low–medium risk; medium confidence.
3. **LGPL/GPL data on iOS:** keep it out. Medium-high.
4. **HSK 2025 list copyright:** level tags only. Low–moderate risk; low-medium confidence.
5. **BabelStone provenance:** low risk.
6. **ML Kit output script undocumented:** test it. High.
7. **Machine-translated es/pt and Japanese-sense KANJIDIC2 meanings:** they will have errors. High.
8. **Compliance details:**
   - CFDICT credit on the store page.
   - Rename LXGW when subset.
   - Publish the CC BY-SA data.

   High.
9. **HSK churn:** build on the 2025 lists. High.
