# Korean data sources: licence research (2026-10-03)

The full research behind `docs/chinese_korean_data.md`. It is checked against
primary sources: official terms pages, LICENSE/README files and download
pages. Anything not confirmed at the source says "unverified". This is a
licence reading, not legal advice. The downloaded pages are kept in
`data/raw/research/ko/` (git-ignored):
- the krdict copyright and statistics pages;
- the NIKL FAQ;
- the TOPIK pages;
- the Copyright Act;
- the Commons diagram pages;
- the 편수자료 text.

## Shared licence points

### CC BY-SA 2.0 KR (NIKL's dictionaries)

Legal code: creativecommons.org/licenses/by-sa/2.0/kr/legalcode
- **ShareAlike stops at the data.** "그러나 편집저작물 및 데이터베이스를
  구성하는 저작물은 본 이용허락을 적용함에 있어서 2차적 저작물이 되지
  않습니다" (a work forming part of a compilation or database is not a
  derivative work under this licence).
- **§4:** "저작물을 포함하고 있는 편집저작물 또는 데이터베이스가 그 저작물과는
  별개로 본 이용허락의 조건에 제한받는 것은 아닙니다" (the compilation or
  database is not itself bound by the licence).
- **§3, format conversion is allowed:** "저작물을 각기 다른 매체 및 형식으로
  이용하기 위하여 기술적으로 수정하는 것도 포함됩니다".
- **Duties:** include the licence or its URI with every copy, and credit the
  author, title and URI.
- **DRM clause:** no distribution "기술적 보호조치를 이용하여 … 본 이용허락의
  조건을 위반하는 방식으로" (using technical protection measures in a way that
  breaks the licence's terms). CC BY-SA 4.0 and CC BY 2.0 FR have the same
  clause, so Kana already carries this risk. Mitigation: publish the
  converted data openly.

### KOGL (공공누리) types

Source: kogl.or.kr/info/license.do

| Type | Allows | Usable? |
|---|---|---|
| 1 | Attribution; commercial use and changes allowed | Yes, but see below |
| 2 | Non-commercial | No |
| 3 | No changes; "형식의 변경 … 도 금지" (even format changes are forbidden), which our conversion would breach | No |
| 4 | Non-commercial and no changes | No |

Type 1 forbids implying the agency endorses you, and ends automatically on a
breach.

### The KOGL catch for use abroad

From NIKL's FAQ at kcenter.korean.go.kr/kcenter/teaching/faq.do:
- "공공누리는 국민을 대상으로 하는 제도로, 국내법이 적용되는 곳에 한하여
  해당 조건으로 사용이 가능합니다. 그 외의 장소에서의 저작물 활용은 유형에
  상관없이 저작권자의 이용 허락이 필요합니다." (KOGL is for Korean nationals
  and applies only where Korean law applies; use elsewhere needs the rights
  holder's permission, whatever the type.)
- Permission is needed for "㉡ (국외 이용) 외국인 및 국외 기관에서 저작물을
  사용하는 경우" (use by foreigners or foreign institutions).
- The request form: 한국어교수학습샘터 › 교재 › 저작물 이용허락 신청.

This affects NIKL's KOGL files: the lists, the frequency studies and the
textbooks. It does not affect its CC-licensed dictionaries.

## 1. Hangul stroke order

| Source | Covers | Licence | Verdict |
|---|---|---|---|
| AnimCJK | `svgsKo`: 535 SVGs, all hanja; **0 Hangul** in ~16.9k SVGs | APL / LGPL | Not useful |
| KanjiVG, Make Me a Hanzi | No Hangul | — | Not useful |
| MagisterAdamus/hangeul-stroke-order | 35 of 40 jamo, one filled outline with arrows each (not per-stroke paths) | CC BY-SA 4.0; drawn with UnPen (a GPL-2 font) | Reference only |
| Wikimedia Commons, Category:Hangeul_stroke_order | 50 raster files: PNG steps for 28 jamo. Missing ㄲㄸㅃㅆㅉ and ㅘㅙㅚㅝㅞㅟㅢ | 46 public domain; a few CC BY-SA 3.0 / GFDL / CC BY 3.0 | **Best reference** |
| hayanhuman-code/hangulssugi, Hilokal/korean-handwriting, hangyul-ganada | Some per-stroke paths | No licence | Avoid |

What NIKL says:
- On X, 2014-06-20: "한글 자모의 획순은 규정되어 있지 않습니다. 다만, 초등학교
  교과 과정에서 지도하는 자음의 획순은 제시하신 대로입니다." (Jamo stroke order
  is not regulated; the consonant order taught in primary school is as you
  showed.)
- 온라인가나다 (2025-08-05, qna_seq 319090): stroke counts are "어문 규범에서
  정하는 사항이 아니므로" (not set by the language norms); ㅈ is 2 or 3 strokes
  depending on its form.

Also citable:
- the primary-school curriculum;
- English Wikipedia's "Hangul" stroke-order section, citing Kim-Renaud 1997,
  p. 283: "strokes of letters are written from top to bottom, left to right";
- the public-domain Commons diagrams.

세종한국어 입문 is KOGL Type 4: cite it only. House decisions needed: the
forms of ㅈ, ㅊ and ㅎ, an alternative ㄱ, and three different ㅗ diagrams on
Commons.

## 2. Dictionary

| Source | Licence | OK? | Verdict |
|---|---|---|---|
| **한국어기초사전 krdict (NIKL)** | **CC BY-SA 2.0 KR** (krdict.korean.go.kr/kor/kboardPolicy/copyRightTermsInfo) | **Yes**; attribution "국립국어원 한국어기초사전, CC BY-SA 2.0 KR"; ShareAlike on the converted data; **audio and images excluded** | **Core** |
| 표준국어대사전 stdict | CC BY-SA 2.0 KR | Yes; **quoted examples from publications excluded** | Optional |
| 우리말샘 opendict | CC BY-SA 2.0 KR | Yes; same exclusion | Optional |
| kengdic (garfieldnate) | **MPL 2.0 / LGPL 2.0+** (choose MPL) | Yes; make the tsv with changes available as Source Code Form | Supplement |
| Wiktionary via kaikki.org | CC BY-SA 4.0 + GFDL | Yes | English supplement |

**krdict:**
- **Headwords:** 53,671 (51,555 words, 1,120 phrases, 996 grammar patterns) plus 2,884 idioms and proverbs; 73,636 senses (stats page, 2026-10-03).
- **Grades:** 초급 2,545, 중급 9,068, 고급 37,024, none 5,034.
- **Translations:** headword and definition in 11 languages: **en, ja, fr, es**, ar, mn, vi, th, id, ru, zh. **No Portuguese.**
- **Examples:** phrase, sentence and dialogue examples, Korean only.
- **Other fields:** hanja origin, semantic category, pronunciation.
- **Download:** a full Excel / XML / JSON download (63–84 MB zips, 2026-09-19) at krdict.korean.go.kr/download/downloadPopup.

**The other dictionaries:**
- **stdict:** 425,282 headwords, Korean-only definitions; XML download after login.
- **opendict:** 1,212,844 headwords (counted by sense), Korean-only definitions; download after login.
- **kengdic:** 133,764 rows, 117,509 with English; "quite dirty".
- **kaikki:** the English Wiktionary has 57,252 Korean words (2026-09-28). Other editions: fr 14,916 senses, ja 40,493, pt 1,279, es 563.

**The licence wording:**
- **krdict:**
  - "본 누리집에서 별도로 명시되지 않은 모든 자료는 '크리에이티브 커먼즈
    저작자표시-동일조건변경허락2.0 대한민국 라이선스'에 따라 배포"
    (everything not otherwise marked is distributed under CC BY-SA 2.0 KR).
  - "누구나 상업적 용도까지 포함하여 자유롭게 이용할 수 있으며" (anyone may
    use it freely, including commercially).
  - Media are excluded: "이미지, 동영상, 음악, 소리, 발음 등 … 개별적으로
    설정" (images, video, music, sound and pronunciation have their own terms).
- **stdict and opendict:** "텍스트 자료 중 출전이 있는 용례는 원저작자가 저작권을
  소유하고 있으므로 … 이용 허락을 받아야 합니다" (quoted examples belong to their
  original authors and need permission).

## 3. Example sentences

Tatoeba has **15,941 Korean sentences** (export of 2026-09-26, 233
contributors). The licence is CC BY 2.0 FR, with no CC0 sentences and no audio.

| Translation | Direct | Through English (to check) |
|---|---|---|
| English | **10,727** | — |
| Japanese | 2,154 | 6,329 |
| French | 2,005 | 7,214 |
| Spanish | 1,015 | 5,564 |
| Portuguese | **202** | 4,749 |

Tatoeba's terms §6.5: "We are not generally opposed to using our content for
commercial purposes. However, this choice depends primarily on contributors…"

Other sources:
- **krdict:** Korean-only examples, graded by the word's level.
- **OPUS:**
  - WikiMatrix, wikimedia and tldr give en–ko pairs: 306,901 / 196,062 / 67,276. Licences CC BY-SA 4.0 / CC BY-SA 4.0 / CC BY 4.0. Noisy, not learner-grade.
  - ParaCrawl, NLLB, CCMatrix and OpenSubtitles: OPUS says "We do not own any of the text". Don't bundle.
- **Excluded:** TED2020, QED, AI Hub, 모두의 말뭉치 and the KAIST corpus. They are NC-ND, research-only, or for Korean nationals only.

## 4. Level lists

| Source | Covers | Licence | Verdict |
|---|---|---|---|
| **krdict grades** | 초급 2,545 / 중급 9,068 / 고급 37,024 | CC BY-SA 2.0 KR | **Safest** |
| NIKL 한국어 학습용 어휘 목록 (2003) | 5,965 words: A 982 / B 2,111 / C 2,872; frequency rank, part of speech, hanja (xls/txt; korean.go.kr etc_seq 70/71) | KOGL Type 1; attribution "본 저작물은 국립국어원에서 제1유형으로 개방한 '…'을 이용하였습니다" | After NIKL permission |
| NIKL 2017 국제 통용 한국어 표준 교육과정 적용 연구 (4단계) xlsx (report_seq 932) | **10,635 words, levels 1–6** (735 / 1,100 / 1,655 / 2,200 / 2,365 / 2,580), plus a 초/중/고 column and 336 grammar items | KOGL Type 1 | **Best 6-level list**, after permission |
| 2010/2011/2016 curriculum lists; 2015 어휘 내용 개발 | — | KOGL Type 4 | No |
| Official TOPIK list (2015; 초급/중급, about 1,963 / 4,029) | — | "Copyright 2014 NIIED. All Rights Reserved" | Avoid, or ask NIIED |
| TOPIK past papers | — | "영리 목적 이용은 국립국제교육원의 이용 허락을 받아야 합니다" (commercial use needs NIIED's permission) | No |

GitHub merges (julienshim, dyoo) and Wiktionary's "Korean 5800" add nothing
beyond the NIKL originals.

## 5. Frequency

| Source | Covers | Licence | Verdict |
|---|---|---|---|
| NIKL 현대 국어 사용 빈도 조사 (2002) | 58,437 words by genre (cp949 txt) | KOGL Type 1 | After permission |
| NIKL 현대 국어 사용 빈도 조사 2 (2005) | Words plus **syllable and jamo statistics** | KOGL Type 1 | Very useful, after permission |
| Hermit Dave FrequencyWords `ko_50k` (OpenSubtitles) | 50k words | "MIT License for code. CC-by-sa-4.0 for content." | Fallback now |
| wordfreq (ko) | Word frequency | CC BY-SA 4.0 | Fallback |
| Leipzig kor corpora | Corpora and word lists | Downloads CC BY (online services NC); verified via a 2026-01-03 Wayback snapshot | Optional (medium) |
| 모두의 말뭉치 / 21세기 세종계획 | — | Research only: "결과물에 … 원문 일부 또는 전부가 포함되어서는 안 되며" (outputs may not contain any of the source text); "제3자에게 양도 … 할 수 없습니다" (no transfer to third parties) | No |

## 6. Hanja

| Source | Covers | Licence | Verdict |
|---|---|---|---|
| **Unihan** (18.0) | kHangul readings for 8,526 characters. **kKoreanEducationHanja = exactly 1,800** (2007). kDefinition (English) for 1,799 of them. No 훈 | Unicode License v3 | The 1,800 list and readings |
| **libhangul `data/hanja/hanja.txt`** | **훈음** ("천:天:하늘 천"): 7,374 characters, 1,795 of the 1,800 (the 5 misses are variant code points); ~275k hanja-word rows | **BSD-3-Clause in the file header** (the repo's COPYING is LGPL 2.1; the `freq-hanja*.txt` files have no header, so avoid them) | 훈음 (the origin of the 훈 strings is undocumented) |
| **KANJIDIC2** (in Kana) | 6,293 characters with `korean_h` readings (by Charles Muller); English for all; es 2,502, fr 2,064, pt 1,942 | CC BY-SA 4.0 | es/fr/pt meanings |
| **KanjiVG** (in Kana) | Strokes for all 6,293 | CC BY-SA 3.0 | **Hanja strokes solved**; Korean order may differ for a few (unverified) |
| ko.wiktionary 부록:한문 교육용 기초 한자 1800 | All 1,800 with 훈음 | CC BY-SA 4.0 + GFDL | Cross-check |
| 교육부 편수자료 PDF | — | "저작권자 교육부 … 무단 복제를 금함" (copyright Ministry of Education; no unauthorised copying) | Unclear; take the list from Unihan |
| suminb/hanja, myungcheol/hanja, 한국어문회 grade lists, e-hanja | — | No licence or unreachable | Avoid |

## 7. Lessons (PDFs)

| Source | Covers | Licence | OK? |
|---|---|---|---|
| Open Textbook Library (5 Korean titles), LibreTexts (2) | — | CC BY-NC / BY-NC-SA | No |
| OER Commons "Hangul" | Embeds third-party video | CC BY 4.0 label, but not its own material | No |
| 세종학당 (nuri.iksi.or.kr), NIKL printed textbooks (개정판 세종한국어) | — | KOGL Type 4: "상업적 이용과 변형 등 2차적 저작을 금지" (commercial use and adaptations forbidden) | No |
| NIKL textbooks moved to **Type 1 in Feb 2026**: 세종한국어(증보판) 1–8, 초급 한국어 (English edition) and others (kcenter) | Full textbooks | KOGL Type 1. FAQ: "유료 판매 목적으로 사용해도 되나요? 상업적 활용 여부에 관계없이 이용할 수 있습니다" (yes, commercial or not). **Needs permission for use abroad; photos and illustrations separate**. Copies on korean.go.kr still show Type 4 | Only with permission (medium) |
| **Wikivoyage Korean phrasebook** | en 52 KB; es "Guía de coreano" 52 KB; fr "Guide linguistique coréen" 47 KB; ja "朝鮮語会話集" 41 KB; pt "Guia de conversação coreano" 55 KB | CC BY-SA 4.0 | **Yes, all five** |
| Wikibooks Korean | en 75 pages (~272 KB); fr 41; ja 19; es 22 | CC BY-SA 4.0 | Yes (uneven) |

## 8. Fonts

| Font | Licence | Notes |
|---|---|---|
| Noto Sans KR / Noto Serif KR | OFL 1.1 (Noto Sans KR reserves "Source") | Interface |
| Nanum Pen Script, Nanum Brush Script, Nanum Gothic, Nanum Myeongjo | OFL 1.1, reserved "Nanum…" | **Rename if subset**; handwritten look |
| Gaegu, Hi Melody, Gamja Flower, Yeon Sung, Poor Story, East Sea Dokdo, Single Day, Dokdo, Cute Font, Gowun Dodum, Gowun Batang | OFL 1.1 | OK |
| KERIS 학교안심 | Stated as OFL (embedding allowed) | OK (check each zip) |
| KoPub | Embedding "별도의 승인이 필요합니다" (needs separate approval) | Avoid |
| 문체부 쓰기 정체 | "유료 판매 금지 / 무단 배포 금지" (no paid sale; no unauthorised redistribution) | Avoid |
| 미래엔 | Commercial use excluded | No |

None of the usable fonts is a certified textbook typeface (교과서체), so the
model letterforms come from our own jamo data.

## 9. Google ML Kit

- **Digital Ink:** `ko` ("Korean, Korean script") and `ko-x-gesture`; the model downloads at runtime.
- **Text Recognition v2:** "한국어 | Korean | ko | Kore; supported in v2". Pod `GoogleMLKit/TextRecognitionKorean` (`KoreanTextRecognizerOptions`), statically linked, about 38 MB.
- **Translate:** `ko` (Swift `TranslateLanguage.korean`); the packs download at runtime.
- **Terms:** free, and commercial use is not prohibited. The usage metrics must be disclosed ("You are responsible for informing users"). Translation needs Google's attribution and branding.

## 10. Romanization

The rules:
- **Source:** 국어의 로마자 표기법 (Revised Romanization), 문화체육관광부 고시
  제2014-42호 (2014-12-05), at
  korean.go.kr/kornorms/regltn/regltnView.do?regltn_code=0004.
- **Not copyrighted:** Copyright Act art. 7 excludes "2. 국가 또는 지방자치단체의
  고시ㆍ공고ㆍ훈령 …" (state notices).

Libraries:
- MIT, usable as reference: Tyriar/hangul-romanization, gerosyab/koroman, creekpld/romanize (Swift).
- osori/korean-romanizer is GPL-3: avoid.
- iOS `.toLatin` is not Revised Romanization ("hangug").

Rules that need word-level information:
- pronunciation that varies by word (신라 → Silla, but 신문로 → Sinmunno);
- ㄴ/ㄹ insertion;
- ㅎ kept in nouns ("체언에서 … 'ㅎ'을 밝혀 적는다": 묵호 → Mukho);
- tensing is not marked (simpler);
- hyphens and proper names.

A small exceptions dictionary of our own covers these.

## Best combination

1. **Jamo and syllables:**
   - Our own per-stroke paths for the 40 jamo, in curriculum order checked against the public-domain Commons diagrams.
   - All 11,172 syllables composed in code.
   - Handwriting read by ML Kit `ko`.
   - Fonts: Noto Sans KR or Gowun Dodum to display; Nanum Pen Script or Gaegu for a handwritten look.
2. **Dictionary:**
   - krdict at the core: levels, en/es/fr/ja translations, hanja origin, Korean examples.
   - English Wiktionary as a supplement.
   - Portuguese: fall back to English, or translate on the device with ML Kit at runtime, so nothing is redistributed.
3. **Sentences:** Tatoeba, plus krdict's Korean examples as an untranslated, level-graded set.
4. **Levels:** krdict now; the NIKL 2003 and 2017 lists after permission.
5. **Frequency:** NIKL 2002/2005 (with jamo statistics) after permission; FrequencyWords until then.
6. **Hanja:** Unihan (the 1,800, readings, English), libhangul (훈음), KANJIDIC2 (es/fr/pt) and KanjiVG (strokes).
7. **Lessons:** the Wikivoyage phrasebook in all five languages, plus Wikibooks; the NIKL Type 1 textbooks only with permission.
8. **Romanization:** our own implementation of 고시 2014-42.
9. **Publish the converted ShareAlike data openly**, as for Kana.

## Gaps

- No open per-stroke Hangul geometry: ours to author (high confidence).
- Portuguese: no dictionary translations, and 202 direct sentence pairs.
- No example sentences that are both translated and graded by level.
- No open TOPIK list.
- No usable audio.
- Hanja es/fr/pt meanings cover only 2.0–2.5k characters.

## Risks (with confidence)

1. **KOGL use abroad needs NIKL's permission.** The quote is verified (high). Its scope over the lists and frequency files is medium. Mitigation: file NIKL's 이용허락 신청, or ship without them.
2. **Anti-DRM clauses vs FairPlay:** the same exposure Kana has; publish the data. Medium.
3. **ShareAlike reaching the code:** low risk; CC BY-SA 2.0 KR excludes compilations and databases (high).
4. **krdict's current zip:** the 2019 XML had 10 languages; 11 today is likely but unverified.
5. **krdict media and quoted stdict/opendict examples:** excluded (high).
6. **NIKL textbooks' Type 1 status:** medium.
7. **libhangul hanja.txt:** BSD-3 is high confidence; the 훈 strings' origin is medium.
8. **Hanja stroke order (Japanese KanjiVG) for Korean:** unverified for a minority.
9. **Fonts:** rename Nanum and Source-derived subsets (high).
10. **ML Kit:** support is high confidence; it needs a privacy disclosure.
11. **Tatoeba:** quality varies; contributors may restrict commercial use (§6.5); per-sentence attribution.
