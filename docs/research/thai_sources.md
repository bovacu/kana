# Thai data sources: licence research (2026-10-04)

The full research behind `docs/hindi_thai_data.md`, for a paid, closed-source
app teaching Thai and its script. It is checked against primary sources:
licence files, official terms pages, download pages and the live data. Counts
were measured on the live files unless marked "quoted". Anything not confirmed
at the source says "unverified". This is a licence reading, not legal advice.

The downloaded pages, licence texts and samples are in
`data/raw/research/th/` (git-ignored; the stroke references in `th/stroke/`);
the Tatoeba exports, ML Kit pages, font licences and sentence-corpus READMEs
shared with Hindi are in `data/raw/research/common/` (`tatoeba2/`,
`fonts_hi_th/`, `sentences/`, `mlkit_*.html`).

The rules applied:
- no NonCommercial or NoDerivatives licences;
- ShareAlike only where it covers the data and not the app's code;
- GPL, LGPL or AGPL data kept out of the app.

## Shared licence points

### CC BY-SA 4.0 (Wiktionary, Volubilis, Wikivoyage, scb-mt-en-th, FLORES)

Handled as JMdict and CC-CEDICT are in the other apps: the converted data
ships as its own file with attribution, and is published in the public data
repository; the app's code stays closed. Wiktionary is dual-licensed CC BY-SA
4.0 and GFDL; we take it under CC BY-SA 4.0 only.

### The Thai Copyright Act B.E. 2537

From the consolidated text (Krisdika; saved as
`th/stroke/legal/copyright_act_2537_krisdika_via_dsi.pdf` and
`th/levels_thai_copyright_act_2537_s7_s14_s23_wikisource.txt`):
- **s.6, paragraph 2:** "การคุ้มครองลิขสิทธิ์ไม่คลุมถึงความคิด หรือขั้นตอน
  กรรมวิธีหรือระบบ หรือวิธีใช้หรือทำงาน หรือแนวความคิด หลักการ ..." (copyright
  does not cover ideas, procedures, processes or systems, methods of use or
  operation, concepts, principles ...). **Stroke-order rules and
  romanization systems are methods, not protected; the drawn charts are.**
- **s.7** (not copyrightable): "(๑) ข่าวประจำวัน ... (๒) รัฐธรรมนูญ และกฎหมาย
  (๓) ระเบียบ ข้อบังคับ ประกาศ คำสั่ง คำชี้แจง และหนังสือโต้ตอบของกระทรวง ทบวง
  กรม หรือหน่วยงานอื่นใดของรัฐ ... (๔) คำพิพากษา ... และรายงานของทางราชการ (๕)
  คำแปลและการรวบรวมสิ่งต่าง ๆ ตาม (๑) ถึง (๔)" (news of the day; laws;
  regulations, announcements, orders and letters of state agencies; judgments
  and official reports; their translations and compilations). Teaching charts,
  workbooks and word lists are not in this list.
- **s.14:** "กระทรวง ทบวง กรม หรือหน่วยงานอื่นใดของรัฐ ... ย่อมมีลิขสิทธิ์
  ในงานที่ได้สร้างสรรค์ขึ้นโดยการจ้างหรือตามคำสั่งหรือในความควบคุมของตน"
  (state agencies own the copyright in works made under their employment,
  order or control).
- **s.23:** such works are protected "ห้าสิบปีนับแต่ได้สร้างสรรค์งานนั้นขึ้น
  แต่ถ้าได้มีการโฆษณางานนั้น ... ห้าสิบปีนับแต่ได้มีการโฆษณาเป็นครั้งแรก" (50
  years from creation, or from first publication).

### US government works (FSI, DLI)

17 U.S.C. §105: "Copyright protection under this title is not available for
any work of the United States Government". Works by contractors or
non-employees are not automatically covered.

### Unicode

Unicode 18.0.0 data (UnicodeData.txt, ICU, CLDR) is under the Unicode License
v3: "...to use, copy, modify, merge, publish, distribute, and/or sell
copies..." provided the notice is included. Ship the notice, as in the other
apps.

## 1. Stroke order and shapes

**There is no authoritative, openly licensed, machine-readable Thai
stroke-order dataset, and no usable open pen-trajectory corpus.** We draw the
strokes ourselves, as for Hangul; the rules for doing so are clear and well
attested (§1.4).

### 1.1 Stroke data

| Source | What it contains | Licence (quoted) | Verdict | Confidence |
|---|---|---|---|---|
| **littlefrog1973/writing-practice** (GitHub, created 2026-08-16) | `data/strokes/thai_consonants.json` (44), `thai_vowels.json` (21: ะ ั า ำ ิ ี ึ ื ุ ู เ แ โ ใ ไ ่ ้ ๊ ๋ ็ ์), `thai_numerals.json` (10). Each stroke is an ordered point list in a 0–1 box (y down), 20–425 points per stroke. One adult traced each character with a finger over Sarabun at 30% opacity, starting at the loop. Missing ฤ ฦ ๅ ํ ฺ ๆ ฯ | LICENSE: "MIT License / Copyright (c) 2026 littlefrog / Permission is hereby granted, free of charge ... to deal in the Software without restriction"; README: "The recorded stroke data in `data/strokes/` is original work, authored by hand on a touch screen, and is MIT along with the code." | **Usable** (ship the MIT notice), but as a **cross-check or seed only**: wobbly finger strokes, traced over a typeface rather than the school form, one author, several choices that disagree with other sources (§1.4.7) | Licence high; data medium |
| jpnantcom/chokwritethai (GitHub, 2026-01-25) | One SVG per consonant (44), centreline cubic Béziers in writing order | No licence file; GitHub reports none: all rights reserved | **Avoid** (used only as a third opinion on counts and directions) | High |
| thesisbytes/hito | Tracing engine; README: "Thai — 44 consonants: engine works; no stroke data yet" | No licence | Avoid; no data | High |
| bee-san/thaiwrite | Android trainer using ML Kit; no stroke data | No licence | Not relevant | High |
| thai-notes.com (reading lessons) | Per-character stroke animations (data inside the page's script) and written notes for 41 consonants, all vowels and tone marks, ๆ ฯ ์ ็ and the digits | No licence or copyright statement: all rights reserved | **Reference only**; its notes are among the most useful sources ([TN] below) | High |
| Wikimedia Commons | Has stroke-order categories for CJK, kana, Hangeul, Devanagari and Asomtavruli; **none for Thai** (Category:Thai script holds charts, no stroke diagrams) | — | Nothing to use | High |
| Tracing apps on Google Play ("ฝึกเขียน ก.ไก่–ฮ.นกฮูก", "Write It! Thai" ...) | Proprietary | — | Avoid | — |

### 1.2 Handwriting datasets

| Dataset | Content | Licence | Verdict |
|---|---|---|---|
| NECTEC Thai handwritten character corpus (Sae-Tang & Methasate, IEEE ISCIT 2004) | **Online** and offline isolated, touching and cursive characters; >44,000 images; WACOM tablet, 63 writers (secondary figures) | Primary download pages unreachable (thailang.nectec.or.th refused the connection; aiforthai.in.th is script-only). nlpforthai.com lists NECTEC "Thai OCR" as "CC BY-SA-NC 3.0". Unverified for this corpus | **Avoid** (NC as reported); adult handwriting, not the school norm anyway |
| ALICE-THI (Groningen) | 78 classes, 150 writers, 200 dpi images | "The dataset downloaded for research use only." | Avoid |
| Thai-MNIST (nextwaverr) | 28×28 images | CC BY-ND 4.0 | Avoid (ND) |
| kittinan/thai-handwriting-number | Thai digit images | MIT | Images only; not useful for order |
| iapp/thai_handwriting_dataset (Hugging Face) | 13,550 line images (9.12 GB), merged from NECTEC's BEST 2019 and another set | Card: apache-2.0; upstream terms unverified | Not relevant (offline lines) |
| KVIS Thai OCR (Mendeley), Burapha-TH (1,072 writers) | Offline character images | CC BY 4.0 (Burapha: per the paper; the download index shows none) | Images only |

No open Thai online-handwriting (pen-trajectory) corpus with a commercial-use
licence exists.

### 1.3 References for the correct order

The school model is the **Ministry of Education letterform,
"ตัวอักษรแบบกระทรวงศึกษาธิการ"**, adapted in 1977 from the letters of
ขุนสัมฤทธิ์วรรณการ: "กรมวิชาการกระทรวงศึกษาธิการ ได้ดัดแปลงรูปแบบตัวอักษรของ
ขุนสัมฤทธิ์วรรณการ เพื่อทำเป็นแบบฝึกหัดคัดลายมือ ... ตั้งแต่ปี พ.ศ. ๒๕๒๐"
[PCT]. Its style is **"หัวกลมตัวมน"** (round head, rounded body), not
"หัวกลมตัวเหลี่ยม", which the same sources list as a different style:
"รูปแบบของตัวอักษรไทยมีหลายรูปแบบ เช่น หัวกลมตัวมน หัวกลมตัวเหลี่ยม หัวเหลี่ยม
ตัวเหลี่ยม ซึ่งแบบตัวอักษรที่ใช้กันทั่วไป คือ หัวกลมตัวมน
หรือตัวอักษรแบบกระทรวงศึกษาธิการ" [DLTV-a].

| Code | Reference | What it gives | Copyright | Verdict |
|---|---|---|---|---|
| — | MOE letterform chart, reproduced in several of the documents below | The model letters: 44 consonants, vowels (with ฤ ฤๅ ฦ ฦๅ), tone marks, digits | Government work (s.14); 50 years from first publication (s.23): if first published in 1977, to about 2027 (date unverified) | Reference for shapes; **do not copy or trace** |
| [BKY] | "คู่มือการสอนคัดลายมือ", ละเอียด สดคมขำ (2020; school site bky.ac.th; `refs/bky_khukhmue_kadlaimue.pdf`) | The rules: "เริ่มที่หัวพยัญชนะก่อน และไม่วนซ้ำ"; "โดยไม่ยกดินสอ / ปากกา จนกว่าจะจบตัวอักษร"; line types incl. "วงกลมทวนเข็มนาฬิกา หัวอักษร ผ ฝ ค ฅ / วงกลมตามเข็มนาฬิกา หัวอักษร น ม บ ป ฯลฯ"; vowel alignment | Teacher's work, copyrighted | Reference |
| [PCT] | "เขียนได้ลายมือสวย แนวทางคัดลายมือตามแบบตัวอักษรของกระทรวงศึกษาธิการ", สพป.พิจิตร เขต ๑, via TruePlookpanya (`refs/trueplookpanya_kadlaimue_moe.pdf`) | The proportions: 4 parts per line; the 10 basic strokes children practise (incl. CW and CCW circles); sizes (§1.4.8) | Government area-office work (s.14); "© 2026 TruePlookpanya All rights reserved" | Reference; the proportions are facts |
| [DLTV-a], [DLTV-b] | DLTV (satellite distance-education foundation): P.5 knowledge sheet (download 155681); P.4 slides 25–29 (155630) and sheet (155632) | "การเขียนพยัญชนะไทยทุกตัว ต้องเริ่มเขียนจากหัวก่อน ยกเว้น ก และ ธ ซึ่งไม่มีหัว" [a]; "เริ่มคัดตัวอักษรจากหัวไปหางเสมอโดยไม่ยกดินสอ" [b]; **arrow diagrams with numbered starts** for ก ค ด ฃ and ฐ (strokes 1 and 2) | Private foundation, copyrighted | Reference; the closest thing to an official per-letter source |
| [TPP] | TruePlookpanya articles 33350 "พยัญชนะไทย", 34223 "การเขียนตัวอักษรไทย" | Head classes; the MOE form "ถือเป็นแบบตัวอักษรอย่างเป็นทางการ"; "ต้องเริ่มเขียนหัวก่อน ยกเว้นตัว ก และ ธ" | © TruePlookpanya | Reference |
| [TN] | thai-notes.com | Per-character start point, loop direction, pen lifts | No licence | Reference |
| [WP] | Thai Wikipedia, ก and ถ; ฐ | The writing path of ก and ถ; ฐ written without its foot (เชิง) above ุ ู ฺ | CC BY-SA 4.0 (facts only used) | Reference |
| [DLI] | DLI Thai SOLT I, Module 1 Lesson 1 (2004, yojik mirror) | "The basic rule is to write from left to right starting with the loop and trying to complete the letter in one stroke. Only two consonants are written without a loop: ก and ธ." | US DLIFLC work, probably public domain (unverified) | Reference |
| [LF] | littlefrog data (§1.1), measured | Start, direction and rotation per stroke | MIT | Cross-check |
| — | Unicode ch. 16.1 (Thai) | Encoding only (leading vowels "typed and stored before the base consonant") | — | Not a stroke source |
| — | Royal Society (ราชบัณฑิตยสภา) | No handwriting standard found (it rules spelling and the dictionary; the letterform standard is the MOE's); its legacy site refused connections | — | None found |
| — | OBEC "คู่มือการสอนอ่านเขียน โดยการแจกลูกสะกดคำ" | HTTP 403 | Government work | Unverified |

### 1.4 The rules

#### 1.4.1 General rules (all sources agree)

1. **Start at the head (หัว)**, the small loop. "เริ่มที่หัวพยัญชนะก่อน"
   [BKY]; "...ยกเว้น ก และ ธ ซึ่งไม่มีหัว" [DLTV-a, TPP, DLI, TN]. **Only ก and
   ธ have no head.** ร has one: the small loop at its foot, drawn
   counter-clockwise.
   - ก starts at the bottom left, rises, forms the beak on the left, arches
     over and comes down the right stem [WP, TN, DLTV arrow].
   - ธ starts at the top left with a downstroke [TN].
2. **Do not lift the pen** until the letter ends ("จนกว่าจะจบตัวอักษร" [BKY];
   "จากหัวไปหางเสมอโดยไม่ยกดินสอ" [DLTV-b]). The exceptions are separate
   parts: the foot (เชิง) of ญ and ฐ, the tail of ศ and ส, the inner part (ไส้)
   of ษ.
3. **Draw the loop once** ("ไม่วนซ้ำ" [BKY]).
4. **The head loop is one part** of the four-part line [PCT].
5. **In a syllable, write in visual order, left to right**: a leading vowel
   (เ แ โ ใ ไ) before its consonant; above and below vowels after the
   consonant; the vowel before the tone mark (กิ่ว = ก, ิ, ่, ว); ั between
   the two consonants [TN].
6. **Upper and lower marks align with the consonant's back (right) edge**:
   "สระ ิ เริ่มจากเส้นแนวหลังพยัญชนะ ... สระ ื ขีดหลังต้องตรงกับเส้นแนวหลังพยัญชนะ ...
   ขีดหลังสระอูตรงกับเส้นแนวหลังพยัญชนะ" [BKY]; over ป ฝ ฟ they shift left
   [PCT, TN].

#### 1.4.2 Loop rotation

Our synthesis, which every per-letter datum found agrees with (the DLTV
arrows, BKY, TN, LF, and chokwritethai by eye): **the loop turns so that the
pen leaves it smoothly, without a cusp, along the line that follows.**

| Line leaving the loop goes | Loop on the left of that line | Loop on the right of that line |
|---|---|---|
| Down | **Clockwise** (บ ป น ม พ ฟ ห ฬ ง ด ต ฉ) | **Counter-clockwise** (ผ ฝ ค ฅ ศ อ ฮ) |
| Up | **Counter-clockwise** (ภ ฎ ฏ ร ว) | **Clockwise** (ถ ล ส จ; ข ฃ ช ซ, whose loop rises into the notched head) |

The labels หัวเข้า (head in) and หัวออก (head out) do **not** decide the
rotation, and Thai sources use them in two conflicting ways:

| Class | Convention A [BKY] | Convention B [TPP 33350, DLTV-b slides] |
|---|---|---|
| หัวเข้า | ผ ฝ (top); ถ (bottom); ค ฅ ศ อ ฮ (inside) | ง ฌ ญ ฒ ณ ด ต ถ ผ ฝ ย ล ว ส อ ฮ |
| หัวออก | ง ช น บ ป พ ฟ ม ห ฬ (top); ภ (bottom); ฉ ด ต (inside) | ค ฅ จ ฉ ฎ ฏ ฐ ท น บ ป พ ฟ ภ ม ร ศ ษ ห ฬ |
| Double head | ข ฃ ช ซ | ข ช |
| Notched head (หัวหยัก) | ฃ ซ ฆ ฑ | ฃ ซ ฆ ฑ |
| No head | ก ธ | ก ธ |

They disagree on ค ฅ ศ ด ต ง, yet both agree with the actual rotations (ค
counter-clockwise, ด clockwise). **Store the rotation per letter.**

#### 1.4.3 Consonants

Rotation is of the first (head) loop, as seen on paper.

| Letter | Strokes | Start, path, loop | Sources |
|---|---|---|---|
| ก | 1 | No head. Bottom left → up → beak on the left → arch → down the right stem | WP, TN, DLTV arrow, LF |
| ข | 1 | Loop at the left, CW, from its lower left → up into the double head → down → base → up the right stem. Half width | LF; DLTV arrow for ฃ; PCT |
| ฃ | 1 | As ข with a notched head; CW | DLTV arrow, LF |
| ค | 1 | Inner loop, CCW → down to bottom left → up the outer left → arch → down the right stem | BKY, DLTV arrow, LF |
| ฅ | 1 | As ค with a notch at the top; CCW | BKY, LF |
| ฆ | 1 | Top-left loop CW; notched head | TN, LF |
| ง | 1 | Loop at top right, CW → down → diagonal up to the left | BKY, TN, LF |
| จ | 1 | Inner lower loop CW → down right → up the right → arch → hook at top left | LF, BKY |
| ฉ | 1 | "Both loops are written clockwise" | TN, LF |
| ช | 1 | As ข with a long tail; CW | LF |
| ซ | 1 | Notched head; "Start by drawing the loop clockwise" | TN, LF |
| ฌ | 1 | Bottom-left loop CW; wide | TN, LF |
| ญ | **2** | (1) Body, bottom-left loop CW. (2) Foot: from its left loop, CCW, rightwards, ending upward. **No foot above ุ ู ฺ** | TN, LF, WP |
| ฎ | 1 | Loop at the foot of the left stem, CCW → up → beak → arch → down → the under-part in the same stroke (2 parts below the line) | TN, LF, PCT |
| ฏ | 1 | As ฎ with a notch; CCW | TN, LF |
| ฐ | **2** | (1) Body: inner loop → down → round → top flag ending top right. (2) Foot: from its **right** loop, CW, leftwards, ending upward. **No foot above ุ ู ฺ** | DLTV arrows (strokes 1 and 2), TN, LF, WP; body loop disputed (§1.4.7) |
| ฑ | 1 | Notched head, CW | TN, LF |
| ฒ | 1 | Loop CW | TN, LF |
| ณ | 1 | Loop CW, then a second loop at bottom right, finishing up the right stem | LF (start bottom left); TN says "top left loop" |
| ด | 1 | Inner loop CW → down left → up the outer left → arch → down | DLTV arrow, LF |
| ต | 1 | "Drawn exactly the same as ด, apart from the notch at the top" | TN, LF |
| ถ | 1 | Loop inside at the foot of the left stem, CW → up → beak → arch → down | TN, WP, LF |
| ท | 1 | Top-left loop CW; hump | LF |
| ธ | 1 | No head. Top left, down → base → up → top flag | TN, LF |
| น | 1 | Top-left loop CW → down → across → second loop CW → up | TN, LF |
| บ | 1 | Loop CW → down → base → up | BKY |
| ป | 1 | As บ with a tall right stem | BKY, TN |
| ผ | 1 | Head at top, inside, CCW; zigzag base | BKY, LF |
| ฝ | 1 | As ผ with a tall stem | BKY, LF |
| พ | 1 | Loop on the left CW; full-height notch | BKY head class + §1.4.2 |
| ฟ | 1 | As พ with a tall stem | LF |
| ภ | 1 | Loop outside at the foot of the left stem, CCW | TN, LF |
| ม | 1 | Top-left loop CW | TN, LF |
| ย | 1 | Loop CCW | TN, LF |
| ร | 1 | Loop at the foot, CCW → up the right → top left → beak → top flag to the right | TN, LF |
| ล | 1 | Bottom-left loop CW → up → arch → down | LF |
| ว | 1 | Loop at the foot of the right stem, CCW → up → over → ends top left | LF |
| ศ | **2** | (1) Loop CCW. (2) Separate tail; direction disputed | TN, LF |
| ษ | **2** | (1) A บ, loop CW. (2) The inner part, from its loop, CCW | TN, LF |
| ส | **2** | (1) Bottom-left loop CW → up → arch → down. (2) Separate tail; direction disputed | TN, LF |
| ห | 1 | "The first loop is written clockwise, the second, anticlockwise" | TN, LF |
| ฬ | 1 | Loop CW; curled tail | TN, LF |
| อ | 1 | "Start with the loop, anticlockwise, and continue in a single stroke" | TN, LF |
| ฮ | 1 | As อ with a curled tail; CCW | LF |

**39 consonants are one stroke; ญ ฐ ศ ษ ส are two** (TN, LF and
chokwritethai agree; DLTV confirms ฐ). ฎ ฏ are one stroke.

#### 1.4.4 Vowel signs, tone marks, other signs

| Sign | Strokes | Notes | Sources |
|---|---|---|---|
| ะ | 2 | Upper part first; each part from its loop | TN, LF |
| ั | 1 | Loop CCW | TN, LF |
| า | 1 | Small hook at the left → over → down | TN, LF |
| ำ | 2 | The circle first, then า. Circle rotation disputed (TN CW, LF CCW) | TN, LF |
| ิ | 1 | Starts at the **right**, on the consonant's back edge; base right to left, then the arc (LF starts left) | BKY, TN vs LF |
| ี | 2 | ิ, then a short downward stroke touching its end ("ลากขีดลงแตะปลายสระ") | PCT, TN, LF |
| ึ | 1 or 2 | ิ, then "without lifting the pen, draw the loop anticlockwise" (TN: 1); LF draws the loop separately | TN vs LF |
| ื | 3 | ิ, then two short downward strokes, right one first, the right one on the back edge | TN, LF, BKY |
| ุ | 1 | Loop first, then down; right-aligned; ≤ 3 parts below the line | TN, LF, PCT |
| ู | 1 | Loop CW → down → base → up the right | TN, LF |
| เ | 1 | Loop at the foot, CW → up | TN, LF |
| แ | 2 | Left เ, then right เ | TN, LF |
| โ | 1 | Loop CW → up → top flag | TN, LF |
| ใ | 1 | Loop CW → up → curl back at the top | TN, LF |
| ไ | 1 | Loop CW → up → zigzag top; ใ ไ โ rise ≤ 3 parts above the line | TN, LF, PCT |
| ฤ, ฦ, ๅ | 1 each | Not found in any source. By construction: ฤ as ถ (loop CW) with the right stem below the line; ฦ as ภ (CCW) with the stem below; ๅ as า descending below the line | Inferred; unverified |
| ็ | 1 | Loop CW | TN, LF |
| ่ | 1 | Top to bottom | TN, LF |
| ้ ๊ | 1 | Loop CW | TN, LF |
| ๋ | 2 | Order disputed: TN vertical (down) first, then horizontal; LF horizontal first | TN vs LF |
| ์ | 1 | Loop CW | TN, LF |
| ๆ | 1 | Loop CW; "In handwriting the line bends to the left" | TN |
| ฯ | 1 | Loop CCW | TN |
| ํ | 1 | Small circle; rotation unverified (as ำ) | — |
| ฺ | 1 | A dot | — |

#### 1.4.5 Digits

All one stroke except possibly ๙.

| Digit | Notes | Sources |
|---|---|---|
| ๐ | "Start at the top right, drawing the character anticlockwise" | TN, LF |
| ๑ ๒ ๓ | From the loop, CW | TN, LF |
| ๔ | Loop CCW | TN, LF |
| ๕ | As ๔ with a loop in the tail | TN, LF |
| ๖ | Loop CCW | TN, LF |
| ๗ ๘ | Loop CW | TN, LF |
| ๙ | Loop CW; TN: "Lift your pen before adding the tail" (2 strokes); LF: 1 | TN vs LF |

#### 1.4.6 Looped school form, not modern fonts

Teach the looped MOE หัวกลมตัวมน form; loopless fonts (Kanit, Prompt, Noto
Sans Thai) are for reading only. The school form differs from typefaces: ข is
narrow [TN, PCT]; ฃ ซ ฆ ฑ have notched heads; the handwritten ๆ bends left
[TN]; ฐ and ญ drop their foot above ุ ู ฺ [WP]; ศ varies across fonts.

#### 1.4.7 Where the sources disagree (house decisions)

1. The tail of ศ and ส: TN draws it downward; LF and chokwritethai upward
   (from where it meets the body to the top right). No MOE arrow diagram
   found.
2. The body loop of ฐ: TN counter-clockwise; the DLTV arrow, LF,
   chokwritethai and §1.4.2 clockwise. **Recommend clockwise.**
3. ิ: BKY and TN start at the right; LF at the left (and inconsistently, its
   ึ ื start at the right). **Recommend right.**
4. The circle of ำ and ํ: TN clockwise, LF counter-clockwise.
5. The order of ๋'s two strokes.
6. ึ as one stroke (TN) or two (LF).
7. ๙ as two strokes (TN) or one (LF).
8. The start of ณ: TN "top left loop"; LF and chokwritethai bottom left.
   **Recommend bottom left.**
9. The หัวเข้า / หัวออก labels (§1.4.2).

#### 1.4.8 Proportions [PCT]

- Each writing line is divided into 4 parts; the head loop is 1 part.
- Letter width is half the height; ข ฃ ช ซ are half the width of the others;
  ฌ ญ ฒ ณ: the front part half the height wide, the rear part half the front.
- ข ช have a twisted, notched head ("หัวขมวดหยัก"); ฅ ฆ ซ ฑ a notched or curved
  head.
- Stems from the head are vertical, except ค ฅ จ ฐ ฒ ด ต ล ศ ส, which curve
  and slant ("เป็นเส้นโค้งเฉียง").
- The tails of ป ฝ ฟ are straight, at most 3 parts; other tails curve up, at
  most 3 parts.
- ฎ ฏ ฐ go 2 parts below the line, as wide as the rear part; ญ's foot sits in
  the first part below; ษ's inner part in part 2.
- ไ ใ โ rise at most 3 parts above the line; ุ ู go at most 3 parts below;
  upper marks sit in parts 2–3, right-aligned (shifted left over ป ฝ ฟ).

## 2. Character metadata

### Unicode

UnicodeData.txt (18.0.0) has 87 Thai code points (U+0E00–U+0E7F; extract in
`unicode_thai.txt`). U+0E01–U+0E2E are 46 letters: the 44 consonants plus ฤ
(RU) and ฦ (LU). The Unicode names are the traditional letter names:
"0E01;THAI CHARACTER KO KAI" ... "0E2E;THAI CHARACTER HO NOKHUK". Unicode
stores Thai in **visual order**: a leading vowel (เ แ โ ใ ไ) comes before the
consonant it follows in speech. Chapter 16 also fixes the order of stacked
marks (SARA AM after tone marks: "the nikhahit that is part of the sara am
should be displayed below those tone marks").

### Names, classes and sounds

The facts (not copyrightable; build our own table and cite the sources):
- **Names**: each consonant has an acrophonic name, the letter plus a word
  starting with it: ก ไก่ (ko kai, "chicken") ... ฮ นกฮูก (ho nok-huk, "owl").
  Sources: Unicode names; en Wikipedia "Thai script" table (letter, Thai name,
  RTGS name, meaning, initial and final sound in RTGS and IPA, class; CC BY-SA,
  saved as `roman_wikipedia_thai_script_consonant_and_tone_tables.wikitext`);
  Wiktionary's "Thai romanization" page and `Module:th-pron`.
- **Classes**: mid 9 (ก จ ฎ ฏ ด ต บ ป อ); high 11 (ข ฃ ฉ ฐ ถ ผ ฝ ศ ษ ส ห); low
  24 (the rest).
- **Tone rules** (the same sources):
  - A syllable is *live* if it ends in a long vowel or a sonorant final (m, n,
    ng, y, w), *dead* if it ends in a short vowel or a stop (p, t, k).
  - No tone mark: mid class live → mid, dead → low; high class live → rising,
    dead → low; low class live → mid, dead with a short vowel → high, dead with
    a long vowel → falling.
  - ่ (mai ek): mid/high → low; low → falling. ้ (mai tho): mid/high → falling;
    low → high. ๊ and ๋ are written on mid-class letters only: high and rising.
  - A leading ห makes a following low-class sonorant high-class (หมา, ไหน);
    อ before ย does the same in four words (อย่า อยู่ อย่าง อยาก).
- The hard parts for software: syllable boundaries, unwritten vowels (คน kon,
  สบาย sa-baai), linking vowels in Pali/Sanskrit loans (ราชการ), silent letters
  under ์, true and false clusters, and irregular words (เขา, น้ำ, ก็).
  **Wiktionary (17,480 words) and Volubilis (102,022 rows) give the tones of
  each word**, so the app computes tones only for words outside both.

### Romanization

| Scheme | What | Status | Verdict |
|---|---|---|---|
| **RTGS** (Royal Thai General System, Royal Institute) | Official transcription; **no tones, no vowel length** | Prime Minister's Office announcement, Royal Gazette vol. 116, 37 ง, p. 11 (1999-05-11): an announcement of a state agency (s.7(3)) and a system (s.6) | Free; for place names and as a secondary line |
| ISO 11940 | Letter-by-letter transliteration | Standard text sold; scheme free to implement. ICU/CLDR's Thai-Latin implements it ("This set of rules follows ISO 11940 ... The transcription is fairly ugly"; output like s̄wạs̄dī) | Not for learners |
| **Paiboon / Paiboon+** (Benjawan Poomsan Becker's books) | Tone-marked learner romanization (paa-sǎa) | A system: not protected by copyright (s.6; also generally). The name's trademark status is unverified | Use the style; **don't label it "Paiboon"** in the UI (call it "tone romanization") |
| **Wiktionary's scheme** | Paiboon-style with tones, plus RTGS and IPA, generated by `Module:th-pron` from a hand-entered phonemic respelling | Lua code CC BY-SA | **Use its output** (in the kaikki data); read the module for the rules but re-implement rather than copy it |
| Volubilis THAIPHON | Its own tone notation before each syllable (- mid, _ low, \ falling, ¯ high, / rising: อาหาร → "-ā/hān") | CC BY-SA 4.0 | Map to our display scheme |
| FSI romanization | Tone-marked (châat, càak) | Public domain (§10) | Reference |

Libraries: PyThaiNLP's `royin` engine (rule-based RTGS, Apache 2.0, small
enough to port); `thai2rom` models (Apache 2.0); `thaig2p` (IPA, trained on
Wiktionary: treat as CC BY-SA); tltk (syllables and tones, but its GitHub
LICENSE is LGPL 3.0 while PyPI says BSD-3: conflicting, build-time use at
most).

## 3. Dictionary

| Source | Covers | Licence | OK? | Verdict |
|---|---|---|---|---|
| **English Wiktionary via kaikki.org** | See below | CC BY-SA 4.0 + GFDL | Yes | **Core** (tones, IPA, English, examples) |
| **Volubilis "Mundo"** (Belisan / Francis Bastien), v26.2 "Monsoon edition", July 2026 | See below | Blog sidebar: "VOLUBILIS MULTILINGUAL THAI DICT. & DATABASE by Belisan is licensed under CC BY-SA 4.0" (linked to the legal code); the xlsx has no licence text | Yes | **Core** (tones, en/fr/es/pt) |
| **LEXiTRON 2.0** (NECTEC) | Quoted "Thai→English 53,000 words, English→Thai 83,000"; measured telex.csv 40,854 rows / 33,060 Thai headwords, etlex.csv 83,232 rows / 54,492 English headwords. Fields: Thai entry, English, category, synonyms, **Thai example sentence**, antonyms, definition, **classifier**. No romanization, no tones | 2003 licence in the zip (the portal says "License not specified"): "Redistribution and use in source and binary form, with or without modification, are permitted provided that all of the following conditions are met"; (c) products must include "This product is created by the adaptation of LEXiTRON developed by NECTEC (http://www.nectec.or.th/)."; (d) not named "LEXiTRON". The Thai text defines the user as one who uses it "...การนำไปใช้ประโยชน์ในเชิงพาณิชย์" (including commercial use). Risk: "NECTEC reserves the right to update the license agreement at any time without prior notice." | **Yes** (no NC clause) | Supplement: English glosses, Thai examples, classifiers |
| Japanese Wiktionary (kaikki, タイ語, dump 2026-10-01) | **1,876 words / 2,600 senses** with Japanese glosses (กิน → 食べる); IPA on 716 | CC BY-SA 4.0 + GFDL | Yes | **Japanese** (the only sizeable open Thai→Japanese source) |
| French Wiktionary (kaikki, Thaï) | 640 words / 658 senses (326 proper names) | Same | Yes | Small |
| Portuguese Wiktionary (kaikki, Tailandês) | 581 words (404 nouns) | Same | Yes | Small |
| Spanish Wiktionary | Not on kaikki; Categoría:Tailandés 186 pages | Same | Yes | Negligible |
| Longdo community data (en-th, ja-th, de-th, fr-th downloads) | Size unverified (live files behind Cloudflare; the fr-th file had 744 lines in a 2023 archive copy) | Archived license.txt (2024-04-15): "Permission to use, copy, modify and distribute this database for any purpose and without fee or royalty is hereby granted"; must include "This product is created by the adaptation of Longdo Dictionary by Metamedia Technology (http://dict.longdo.com/)."; not named "Longdo" | Yes, if obtainable | Possible ja-th supplement |
| Longdo website content (and the third-party dictionaries it shows) | — | Terms: "made available for your personal and non-commercial use only"; Hope EN-TH from KDictThai (GPL); NSTDA glossary CC BY-NC 3.0 TH; Nontri, Saikam: no licence | No | **Avoid** |
| Thai WordNet (NICT; PyThaiNLP `wordnet_th.db`, OMW) | 73,350 synsets, 82,504 lemmas (OMW copy), linked to Princeton synsets; Thai-only meanings | "Permission to use, copy, modify and distribute this software and database ... for any purpose and without fee or royalty is hereby granted", with the notice on all copies | Yes | Secondary (synset links) |
| PyThaiNLP `thai_dict` (from Thai Wiktionary) | 19,480 words, Thai definitions | CC BY-SA 4.0 | Yes | Not needed |
| Wikidata Thai lexemes | 29 lexemes | CC0 | Yes | Negligible |
| PanLex | — | **Now CC BY-NC-SA 4.0**: "Commercial use ... is permitted only by obtaining written permission" | No | **Avoid** |
| thai-language.com / thai2english.com | — | "Copyright © 2026 thai-language.com ... rights reserved"; thai2english returned 403 (terms unverified) | No | **Avoid** |
| JTDic (JA–TH), Saikam (JA–TH), FreeDict | JTDic freeware "©2004 - 2016", no open licence; Saikam no licence; FreeDict has no Thai | — | No | Avoid |

**English Wiktionary (kaikki.org/dictionary/Thai/, extracted 2026-09-28 from
the dump of 2026-09-02):**
- 21,020 entries, **17,629 distinct words** (the page says the same).
- **Tone romanization on 17,480 words**, IPA on 17,486, audio on 176 (Commons,
  each file under its own licence); 6,206 example sentences, themselves
  romanized ("ภาษาไทย" → "paa-sǎa tai").
- Senses (quoted): noun 15,349, verb 5,991, adjective 2,383, proper name
  1,986, classifier 147, particle 89.
- Sample, ภาษา: `{"raw_tags":["Paiboon"],"roman":"paa-sǎa"}`,
  `{"raw_tags":["Royal Institute"],"roman":"pha-sa"}`,
  `{"ipa":"/pʰaː˧.saː˩˩˦/"}`, plus the respelling "พา-สา".
- Glosses English only. The per-language JSONL (48.1 MB) is marked
  "DEPRECATED, will be removed in the near future": fetch it now.

**Volubilis Mundo** (`VOLUBILIS-Mundo.xlsx`, 11,961,003 bytes, through the
blog's Dropbox link; the SourceForge pages were behind Cloudflare):
- 114,577 rows, **103,230 distinct Thai headwords**; tone-marked phonetic
  (THAIPHON) on 102,022 rows (spot-checked อาหาร, แอปเปิล, อดีตตำรวจ: right).
- Meanings: **English 106,642; French 68,966; Spanish 13,282; Portuguese
  4,497**; also Chinese, Lao, Italian, German. **No Japanese.**
- Fields: THAIROM, EASYTHAI, THAIPHON, THA, ENG, FRA, TYPE, USAGE, SCIENT, DOM,
  CLASSIF, SYN, **LEVEL**, NOTE, SPA, ITA, POR, DEU and others.

**Routes to each UI language:**
- English: Wiktionary + Volubilis + LEXiTRON.
- French: Volubilis (68,966 rows) + fr.wiktionary.
- Spanish: Volubilis (13,282 rows); the rest through English.
- Portuguese: Volubilis (4,497 rows) + pt.wiktionary (581); the rest through
  English: the weakest.
- Japanese: ja.wiktionary (1,876 words) + Tatoeba tha–jpn (1,871 sentences) +
  Longdo ja-th if the file can be obtained; the rest through English.

## 4. Example sentences

### Tatoeba

The export of 2026-10-03 (`downloads.tatoeba.org/exports/per_language/tha/`;
kept in `data/raw/research/common/tatoeba2/`). "These files are released
under CC BY 2.0 FR."

- **Thai sentences: 6,851**; median 21 characters, short and everyday
  ("เธอไม่ได้เป็นเด็กแล้วนะ", "ทอมเป็นนักพับกระดาษ"). Contributors are spread
  out (the top three wrote 2,766, 1,835 and 1,282). Most were added in
  2019–2024.
- 6,708 have a translation in at least one of our five languages; 145 none.
- **No CC0 sentences.**
- **Audio: 588 sentences have audio under CC BY 4.0** (two contributors: 390
  and 198 recordings). Another 133 have audio with no licence, which may not
  be reused. The CC BY 4.0 audio is usable with attribution (the
  contributor's name, per recording). It is the only reusable recorded audio
  found for either language.

| Translation | Direct | Through English | Either |
|---|---|---|---|
| English | **4,819** (4,899 links) | — | 4,819 |
| Japanese | **1,871** | 2,504 | **4,365** |
| French | 50 | 3,153 | 3,187 |
| Spanish | 83 | 2,974 | 3,015 |
| Portuguese | **11** | 2,891 | 2,892 |

Japanese is unusually well served: over a quarter of the Thai sentences are
linked directly to Japanese.

### Other sentence sources

| Source | What | Licence (quoted) | Verdict |
|---|---|---|---|
| **scb-mt-en-th-2020** (SCB / VISTEC) | 1,001,752 en–th pairs in 12 subsets: task_master_1 222,733 (dialogues, professional translators), generated_review_* (reviews), nus_sms 43,750, mozilla_common_voice 33,797, msr_paraphrase 10,371, assorted_government, thai_websites, paracrawl, wikipedia, apdf | "published the dataset to the public under Attribution-ShareAlike 4.0 International license (CC BY-SA 4.0) except the English-Thai sentences pairs from Mozilla Common Voice that will be under CC0" | **Usable**: the dialogue and SMS subsets are everyday Thai (English only); pick and check |
| **MASSIVE** (Amazon) | Voice-assistant requests localised into 51 locales incl. **th-TH, es-ES, pt-PT, ja-JP, fr-FR**; ~16.5k per locale (paper) | NOTICE.md: "The MASSIVE dataset is licensed under CC BY 4.0" | **Usable**: short sentences in all five UI languages (localised, not word for word) |
| FLORES-200 | ~2,000 public sentences (of 3,001), professional, tha_Thai + spa/por/jpn/fra | "FLORES-200: CC-BY-SA 4.0" | Usable; advanced |
| NTREX-128 | 1,997 news sentences, tha + spa, por-BR, jpn, fra | "released under the CC BY-SA 4.0 license" | Usable; advanced |
| ALT (NICT) | ~20,106 Wikinews sentences, th + ja + en (+10) | Parallel corpus "Creative Commons Attribution 4.0 International (CC BY 4.0)" (treebanks NC) | Text usable; advanced |
| LEXiTRON examples | Thai-only example sentences in telex.csv | LEXiTRON licence (§3) | Untranslated examples |
| OPUS en–th (2026-10-04) | OpenSubtitles 24.8M, CCAligned 10.7M, QED 264k, TED2020 160k, wikimedia 65,759, tldr-pages 2,072, Tatoeba 1,195 | OpenSubtitles/CCAligned not OPUS's to license; QED/TED2020 NC or NC-ND | Don't bundle |

## 5. Word frequency

| Source | What | Licence | Verdict |
|---|---|---|---|
| wordfreq 3.1.1 | **Thai is not supported.** The library warns: "The language 'th' is in the 'Thai' script, which we don't have a tokenizer for. The results will be bad." No Thai list | — | No |
| **PyThaiNLP `tnc_freq.txt`** | 106,122 words with counts from the **Thai National Corpus**, 33.5 million tokens, segmented | `corpus_license.md`: "The following word lists are created by the PyThaiNLP project and released under Creative Commons Zero 1.0 Universal Public Domain Dedication License", listing `tnc_freq.txt` | **Use** (main ranking). The TNC itself (Chulalongkorn) is not open; the list is counts compiled by PyThaiNLP. Low–medium risk |
| **PyThaiNLP `ttc_freq.txt`** | 19,493 words from the **Thai Textbook Corpus**, 3.0 million tokens | Same CC0 statement | Second opinion (school register) |
| PyThaiNLP Phupha (Zenodo 10.5281/zenodo.18490474, 2026-02-05) | 62,264 strings counted in Common Crawl (July 2025) with the Infini-gram mini API: **substring counts**, so short strings are inflated (the top rows are single letters: น 1.1 billion) | Description: "Dataset license: Creative Commons Zero 1.0 Universal Public Domain Dedication License (CC0)"; the record's licence field says CC BY 4.0 | Usable (attribute to be safe); poor for short words |
| FrequencyWords 2018 `th_50k.txt` (Hermit Dave) | 50,000 rows from OpenSubtitles: **unsegmented phrases, broken encoding and English words** (row 1 is "เธ", a broken byte sequence; "อะไรนะ" counted as one word) | "CC-by-sa-4.0 for content" | **Not usable** |

## 6. Levels

**There is no open, JLPT/HSK-style graded Thai syllabus or word list.**

| Source | What | Licence | Verdict |
|---|---|---|---|
| CU-TFL (Chulalongkorn) | Five levels per skill (Chula Novice → Distinguished); no public word list | — | Level names only |
| Thai Competency Test (MOE / OBEC) | Six levels; no word list found (the 2024 announcement page now returns 404) | — | Level names only |
| CEFR-aligned Thai | None found | — | — |
| OBEC บัญชีคำพื้นฐาน (basic word lists for Thai primary pupils) | Per teacher-site copies: P.1 600, P.2 780, P.3 1,200, P.4 1,400, P.5 1,600, P.6 1,800 words; the primary OBEC document was not found | A word list is not a s.7 item (not a regulation or announcement); more likely an agency-owned work (s.14, 50 years) | Reference only |
| **Volubilis LEVEL column** | A0 = 2,152 rows / 2,018 words (exactly its "Basic" edition); A1 = 3,135 / 2,984 (Basic + A1 = the "Classic" count); A2 = 6,177 / 5,913 (adds up to "Jumbo"). Codes U, S, X, M, I undocumented | CC BY-SA 4.0 | **Usable**: the only open graded list; one author's grading |

**Grade by frequency** (TNC, TTC) adjusted with the Volubilis A0/A1/A2
tiers.

## 7. Word segmentation

Thai is written without spaces between words, so the app's word splitter
(`wordsplit.h`) needs a dictionary.

| Option | Licence | In a closed app? |
|---|---|---|
| **iOS / macOS: NLTokenizer(.word), CFStringTokenizer** | Platform API | **Yes**, nothing to bundle. Apple's docs don't name Thai, but a test on macOS (Darwin 23.4) gave `ผม\|ชอบ\|กิน\|ข้าวผัด\|กับ\|น้ำปลา\|ที่\|ร้าน\|อาหารไทย` from both. CFStringTokenizer gives no Latin transcription for Thai; ICU's "ToLatin" gives ISO 11940 (`s̄wạs̄dī`) |
| **Android: `android.icu.text.BreakIterator`** (API 24+) | ICU user guide: "ICU provides dictionary support for word boundaries in Chinese, Japanese, Thai, Lao, Khmer and Burmese. Use of the dictionaries is automatic"; AOSP's ICU includes `thaidict.txt` | **Yes**; not tested on a device |
| **ICU `thaidict.txt`** (26,383 words) | Header: "Copyright (C) 2016 and later: Unicode, Inc. and others. License & terms of use: http://www.unicode.org/copyright.html ... Copyright (c) 2006-2015 International Business Machines Corporation, Apple Inc., and others." ICU's LICENSE has no separate Thai section (only cjdict, Lao, Burmese), so the Unicode License v3 applies | **Yes**, with the notice |
| **PyThaiNLP `words_th.txt`** (62,107 words) + the newmm algorithm | Word list CC0 (above; some words were copied from Royal Society lists, which carry little copyright as bare lists); code Apache 2.0; the Rust port nlpO3 Apache 2.0 | **Yes**: the best base for a bundled longest-match splitter |
| `orst_words_th.txt` (38,160 Royal Society words, in PyThaiNLP) | **No licence listed** in corpus_license.md | Avoid |
| LibThai `tdict` (25,471 lines) | No separate data licence, so the package's: "GNU Lesser General Public License ... version 2.1" | **Avoid** (LGPL data) |
| deepcut, attacut | Code MIT, but trained on NECTEC's BEST corpus, reported as CC BY-NC-SA 3.0 TH (secondary; the NECTEC page was unreachable) | Avoid bundling the models |

Recommendation: Apple's tokenizer and ICU for free text (photos, pasted
text), and our own longest-match splitter over `words_th.txt` + ICU's list +
our dictionary's headwords, so that the splits agree with the dictionary.

## 8. Google ML Kit and the device's voices

From Google's pages (fetched 2026-10-04; copies in `data/raw/research/common/`):

- **Digital Ink Recognition** ("Last updated 2024-07-10"): "Thai, Thai script.
  `th` `th-x-gesture`". The model downloads at runtime.
- **Text Recognition v2: Thai is not supported.** The supported list has
  Latin, Chinese, Devanagari, Japanese and Korean scripts only; Thai is not in
  the Supported, Experimental or Mapped lists. So "text from a photo" needs
  another engine:
  - iOS: Apple's **Live Text lists Thai** (feature-availability page, iOS and
    iPadOS 27), so Apple's Vision text recogniser should read Thai on the
    device. Check `supportedRecognitionLanguages()` on the oldest iOS we
    support.
  - Android: no Google on-device option. Tesseract's `tha.traineddata`
    (tessdata_fast, 1.07 MB, Apache 2.0) is the open fallback; or turn the
    feature off on Android.
- **Translation** ("Last updated 2026-09-28"): `th` Thai. Through English for
  other pairs; Google's attribution rules apply.
- **To test:** whether the ink model returns a lone vowel sign or tone mark
  written by itself (◌ั ◌่), and how it handles the order of above/below marks.

**Voices:**
- iOS/iPadOS: "Accessibility: VoiceOver, Live Speech, Read & Speak" lists
  "Thai (Thailand)" (iOS and iPadOS 27). The voice is **Kanya** (`th-TH`;
  `say -v '?'` on macOS 14.4 lists "Kanya th_TH").
- Android: TalkBack's page lists "Thai (Thailand)" among "The Google
  Text-to-Speech languages".
- Plus the 588 CC BY 4.0 Tatoeba recordings (§4).
- A TTS voice reads tones correctly only if it segments and reads the word
  correctly; spot-check the voices on tone minimal pairs.

## 9. Fonts

All SIL OFL 1.1 (google/fonts; copies in
`data/raw/research/common/fonts_hi_th/`). No Reserved Font Name unless noted.

Thai has two printed styles: **looped** (หัว, the traditional form taught in
school) and **loopless** (modern headings and ads: Kanit, Prompt). The app
must show looped forms.

| Font | Style | Use | Notes |
|---|---|---|---|
| **Noto Sans Thai Looped** | Looped | Interface | "the more traditional, looped variant of the Southeast Asian Thai script" |
| Noto Sans Thai | Loopless | Not for teaching | "the more modern, loopless variant ... mainly suitable for headlines, packaging and advertising" |
| Noto Serif Thai | Modulated (serif) | Dictionary headwords | — |
| **Sarabun** (Cadson Demak) | Looped | Interface, dictionary | "It is the 'TH Sarabun New' font, made available under the Open Font License" — the Thai government's official document font |
| **Mali** (Cadson Demak) | Looped, handwritten | Model letters / handwritten look | "inspired by a 6th graders' handwriting" |
| **Itim** | Looped, handwritten | Handwritten look | "informal looped + semiserif" |
| **Playpen Sans Thai** (TypeTogether, added 2025) | Handwriting | Children's look | Same education-research family as Playpen Sans Deva |
| K2D, Krub, Niramit, Chakra Petch (looped); Kodchasan ("inspired by teenage handwriting"); Charm (flat-pen hand); Bai Jamjuree, Thasadith | Mostly looped; check each | Alternatives | Cadson Demak's OFL families, several redesigned from the government's "national fonts" |
| Sriracha, Kanit, Prompt | Loopless | Avoid for teaching | — |
| IBM Plex Sans Thai / Thai Looped | — | — | Reserved Font Name "Plex": rename if subset |

## 10. Lessons

| Source | What | Licence | Verdict |
|---|---|---|---|
| **Wikivoyage Thai phrasebook** | en "Thai phrasebook" 45,507 bytes (RTGS with doubled long vowels, mostly no tones); pt "Guia de conversação tailandês" 47,176; fr "Guide linguistique thaï" 17,031; **es and ja: none** | "Text is available under the Creative Commons Attribution-ShareAlike License" | **Yes, three languages**; Spanish and Japanese must be ours |
| Wikibooks "Thai" (en) | Main page + 7 subpages, 42,377 bytes; no es/pt/fr/ja books | CC BY-SA 4.0 | Too thin to matter |
| **FSI Thai Basic Course** (Warren G. Yates & Absorn Tryon, 1970) | Vol. 1 426 pages, vol. 2 421 pages (scans), 38 lessons plus a programmed phonology introduction; FSI tone romanization (châat, càak) rather than Thai script; full archive with audio 392.7 MB (fsi-languages.yojik.eu/archives/FSI-Thai.zip, not downloaded) | US Government work (§105) | **Usable** (re-check the audio before reuse) |
| DLI Thai | DLI-Thai-Complete.zip, 367 MB (not downloaded) | Likely public domain; per item unverified | Possible |
| Peace Corps Thai | Three regional dictionaries (1986, ERIC ED401728–30, Thai authors) and a 13-lesson PDF | §105 covers only federal employees: unverified | Use with care |
| Open Textbook Library | No Thai titles | — | — |
| AUA materials | Commercial | — | No |

## Best combination

1. **Strokes:** our own centreline strokes in KanjiVG-format XML, as for
   Hangul: 44 consonants, the footless ญ and ฐ, the vowel signs, tone marks
   and other signs, ฤ ฦ, and the digits (about 86 glyphs, about 100 strokes),
   following the MOE หัวกลมตัวมน form, the proportions of [PCT] and the rules
   of §1.4, with the loop rotation stored per stroke. littlefrog's MIT data as
   an automatic cross-check (counts, start, rotation). Syllables composed in
   code (leading vowel, consonant, above/below vowel, tone mark).
2. **Dictionary:** English Wiktionary (tones, IPA, English, examples) and
   Volubilis Mundo (tones, en/fr/es/pt, levels), one CC BY-SA data file;
   LEXiTRON for extra English, Thai examples and classifiers (credit line);
   ja.wiktionary for Japanese; the rest of Japanese, Spanish and Portuguese
   through English, labelled.
3. **Sentences:** Tatoeba (with its 588 CC BY 4.0 recordings), MASSIVE for
   short sentences in all five languages, scb-mt-en-th dialogues for more
   English pairs.
4. **Frequency:** PyThaiNLP's TNC list (CC0), TTC as a second opinion.
5. **Levels:** by frequency, adjusted with Volubilis A0/A1/A2.
6. **Segmentation:** Apple's tokenizer / ICU for free text; our longest-match
   splitter over `words_th.txt` (CC0) + ICU's list (Unicode notice) + our
   headwords.
7. **Romanization:** the dictionaries' tone romanization (Wiktionary's, with
   Volubilis's THAIPHON mapped to it), IPA, RTGS as a second line; our own
   rules (from RTGS and the tone rules) for words outside both.
8. **Lessons:** the Wikivoyage phrasebook (en, pt, fr), FSI Thai (public
   domain), our own es and ja material.
9. **ML Kit:** handwriting `th`, translation `th`; photo text through Apple's
   Vision on iOS (Thai is in Live Text), none or Tesseract on Android.
10. **Fonts:** Noto Sans Thai Looped or Sarabun for the interface; Mali or
    Playpen Sans Thai for a handwritten look.
11. **Publish** the converted CC BY-SA data (Wiktionary, Volubilis,
    scb-mt-en-th if used), as for Kana.

## Gaps

- No open Thai stroke data of quality: ours to draw (high confidence).
- No open graded syllabus; Volubilis's levels are one author's.
- Japanese meanings: 1,876 words; Portuguese: 4,497 Volubilis rows and 11
  direct Tatoeba pairs; no Spanish or Japanese Wikivoyage phrasebook.
- Tones for words outside Wiktionary and Volubilis must be computed.
- No ML Kit photo-text model for Thai.
- No open pen-trajectory corpus to test the matcher against real children's
  handwriting.

## Risks (with confidence)

1. **Our own Thai strokes vs the school norm:** the rules are well attested
   (high), but nine points disagree between sources (§1.4.7); have a Thai
   primary teacher review the animations. Medium.
2. **Tracing the MOE chart** would copy a government work protected until
   about 2027 (first-publication date unverified): draw from the rules and
   proportions instead. High.
3. **LEXiTRON's licence** allows commercial use (high), but NECTEC "reserves
   the right to update the license agreement at any time without prior
   notice"; keep the 2003 text with the data. Medium.
4. **Volubilis's licence** is stated on the blog, not in the file (medium–high);
   keep a dated copy of the page (saved).
5. **TNC frequency provenance:** CC0 by PyThaiNLP, counts from a non-open
   corpus. Low–medium.
6. **Anti-DRM clauses vs FairPlay** (CC BY-SA, CC BY): as for Kana; publish
   the data. Medium.
7. **ML Kit:** support for `th` handwriting is documented (high); its handling
   of lone marks is untested.
8. **Fonts:** rename IBM Plex subsets ("Plex" reserved); the others have no
   reserved names (high).
9. **Tatoeba audio:** CC BY 4.0 for 588 sentences (high); credit each
   recording.
