# Arabic data sources: licence research (2026-10-04)

The full research behind `docs/arabic_data.md`, for a paid, closed-source
app teaching Modern Standard Arabic and its script in the Naskh school hand
(each letter in its four positional forms, the vowel marks, the digits).
It is checked against primary sources: licence files, official terms pages,
download pages and the live data. Counts were measured on the live files
unless marked "quoted". Anything not confirmed at the source says
"unverified". This is a licence reading, not legal advice.

The downloaded pages, licence texts and samples are in
`data/raw/research/ar/` (git-ignored): `dict/`, `morph/`, `sentences/`,
`freq/`, `levels/`, `roman/`, `stroke/`, `mlkit/`, `fonts/`, `lessons/`.
The Tatoeba English pivot links, FLORES/NTREX READMEs and the wordfreq README
shared with the other apps are in `data/raw/research/common/`.

The rules applied:
- no NonCommercial or NoDerivatives licences;
- ShareAlike only where it covers the data and not the app's code;
- GPL, LGPL or AGPL data kept out of the app.

## Shared licence points

### CC BY-SA 4.0 (Wiktionary, Wikivoyage, Wikibooks, FLORES, NTREX, FrequencyWords, wordfreq data)

Handled as JMdict and CC-CEDICT are in the other apps: the converted data
ships as its own file with attribution, and is published in the public data
repository; the app's code stays closed. Wiktionary: "The original texts of
Wiktionary entries are dual-licensed to the public under both the Creative
Commons Attribution-ShareAlike 4.0 ... and the GNU Free Documentation
License" (`dict/wiktionary_copyrights_page.wikitext`); we take it under
CC BY-SA 4.0 only. Arabic WordNet is CC BY-SA 3.0 (same handling).

### Government textbooks in Arab countries

In all four laws checked, the exclusion from protection covers laws,
judgments, administrative decisions and "official documents", not
textbooks; and ideas, procedures and methods are not protected. Ministry
primers and workbooks are protected collective works owned by the ministry
that directs them, for **50 years from first publication**. So: the
**stroke-order rules are methods and free to follow; the drawn models are
protected: reference only, never trace**. (Texts saved in `stroke/legal/`.)
- **Saudi Arabia**, Copyright Law, Royal Decree M/41 (1424 H), amended 1439:
  - Art. 4: "لا تشمل الحماية… ١ – الأنظمة والأحكام القضائية، وقرارات الهيئات
    الإدارية، والاتفاقيات الدولية، وسائر الوثائق الرسمية… ٣ – الأفكار،
    والإجراءات، وأساليب العمل، ومفاهيم العلوم الرياضية، والمبادئ، والحقائق
    المجردة" (SAIP English: "(3) Ideas, procedures, work methods, concepts of
    mathematical sciences, axioms, and abstract facts").
  - Art. 19 First (3): works whose author is a legal person, "خمسون سنة من
    تاريخ أول نشر للمصنَّف"; (5): collective works, 50 years from first
    publication. Art. 6(3): the person who "directs or organizes the
    creation of a collective work shall have the sole right to exercise the
    copyright".
- **Egypt**, Law 82/2002: Art. 141: "لا تشمل الحماية مجرد الأفكار والإجراءات
  وأساليب العمل… أولاً - الوثائق الرسمية… مثل نصوص القوانين، واللوائح،
  والقرارات، والاتفاقيات الدولية، والأحكام القضائية…"; Art. 162: collective
  works of a legal person, "خمسين سنة تبدأ من تاريخ نشرها أو إتاحتها للجمهور
  لأول مرة أيهما أبعد" (the Arabic, which governs, says the later date; WIPO's
  English says "whichever comes first").
- **UAE**, Federal Decree-Law 38/2021: Art. 3(1) excludes ideas, procedures
  and methods; Art. 3(2) "الوثائق الرسمية… مثل نصوص القوانين واللوائح
  والقرارات…"; Art. 20(3): collective works of a juristic person, "(50)
  خمسين سنة تبدأ من أول السنة الميلادية التالية للسنة التي تنشر فيها لأول
  مرة".
- **Jordan**, Law 22/1992 (consolidated 2014): Art. 7(أ) "القوانين والأنظمة
  والأحكام القضائية وقرارات الهيئات الادارية والاتفاقيات الدولية وسائر
  الوثائق الرسمية…", 7(د) "الأفكار والأساليب وطرق العمل…"; Art. 31(ب): 50
  years from 1 January after publication for works of a legal person. (The
  Arabic extract is line-reversed; not checked by eye.)

### US government works (FSI, DLI, Library of Congress)

17 U.S.C. §105: "Copyright protection under this title is not available for
any work of the United States Government"; only works by federal employees
in their duties. Contractors' and grantees' works are not automatically
covered.

### Unicode

Unicode 18.0.0 data (UnicodeData.txt, ArabicShaping.txt, CLDR, ICU) is under
the Unicode License v3; ship the notice, as in the other apps.

## 1. Stroke order and shapes

**There is no usable open, machine-readable Arabic stroke-order dataset
(nothing like KanjiVG), and no open pen-trajectory corpus with a
commercial-use licence.** We draw the strokes ourselves, as for Thai and
Hindi. The rules are well attested; the best per-letter source is the Saudi
Ministry of Education's Grade 1 primer, whose writing exercises show the four
forms of each letter with arrows and a start circle ([SA-G1] below).

### 1.1 Stroke data

| Source | What | Licence (quoted) | Verdict | Confidence |
|---|---|---|---|---|
| GitHub searches (24 queries: "arabic stroke order", "arabic tracing svg", "arabic letter tracing json", "hanzi-writer arabic", "arabic kanjivg", "huruf tracing", "حروف تتبع", "naskh stroke" ...) | **Nothing comparable to KanjiVG or AnimCJK**; most queries return 0–3 repositories | — | Ours to draw | High |
| [MM] moustafa-mk/arabisch-lernen-ios, `Resources/Curriculum.json` | 28 letters, "strokePaths" of 1–4 polylines with 2–11 points each in a 0–1 box; dots as 2-point strokes. Counts: ب 2, ت 3, ث 4, ط 2 (vertical after the loop), ك 2, و 2, ي 3. Crude | No LICENSE: all rights reserved | **Avoid** (a weak third opinion on counts at most) | High |
| [TY] thiernoyunus/arabic-calligraphy (`letters.js`) | 38 entries (28 letters, ء, لا, 7 words): numbered start points and text tips only ("ط Oval base, then the tall vertical stem"; "لا Lam first, then alif crossing or joining"); no paths | "MIT License / Copyright (c) 2026" | Usable licence, almost no data: a cross-check of tips | High |
| akzize/ArabicLetterTracingGame (7 letters as points); Mohamed0khaled/arabic-letter-tracing (ج's four forms as SVG); hadress92/huroofi and other tracing apps | Fragments or nothing | No licence (mostly) | Avoid | High |
| kourgeorge/arabic-icr | Letters cut from ADAB | No licence; derived from ADAB | Avoid | High |
| Tegaki-style extraction from a Naskh font | A heuristic order from glyph outlines, not the taught order | — | A rough first pass at most | Medium |
| **Wikimedia Commons** | **No Arabic stroke-order category.** Per-letter categories hold static images ("Arabic character Ba.gif", CC BY-SA 4.0); Michelet's "X - ruq'a.png" series (CC0) is font-rendered, without order | Per file | Nothing to use | High |
| Arabic Wikibooks "ويكي الأطفال:الحروف الأبجدية/طريقة الكتابة" | A positional-forms table only | CC BY-SA | Facts only | High |
| Hugging Face (searches "arabic handwriting", "arabic online", "inkml", "khatt", "hijja", "ahcd") | Only offline image sets | Various | No online data | High |

### 1.2 Online handwriting (pen-trajectory) datasets

| Dataset | Content | Licence (quoted) | Verdict |
|---|---|---|---|
| ADAB (REGIM Sfax + IfN Braunschweig) | "The text written is from 937 Tunisian town/village names… An InkML file including trajectory information"; ~33,000 words, 166 writers (secondary); IEEE DataPort "ADAB DATABASE.zip (Size: 13.78 MB)" | Conflicting: DataPort's terms say content is "made available subject to the terms of the Creative Commons Attribution (CC-BY) License", but downloading needs an account ("LOGIN TO ACCESS DATASET FILES"), and the literature calls ADAB "freely available for non-commercial research" | **Not used** (conflict; account needed; adult cursive town names, no use for teaching letters) |
| Online-KHATT (KFUPM) | 10,040 lines, 623 writers | "will be made freely available to researchers at http://onlinekhatt.ideas2serve.net/" (host did not resolve) | Avoid |
| AltecOnDB | Sentences, ~1,000 writers | "can be downloaded by requesting a copy from ALTEC"; no licence published | Avoid |
| OHASD, LMCA, CHAW | — | No public download found | Unverified; avoid |
| Hijja / Hijja2 (KSU) | **Offline** images: "47,434 characters written by 591 participants" aged 7–12, by positional form | No LICENSE (GitHub `license: null`): all rights reserved | Avoid |
| AHCD; HMBD; Muharaf; Mendeley "Handwritten Arabic Numerals" | Offline images | AHCD unverified (captcha); the others CC BY-NC-SA 4.0 / CC BY-NC 3.0 (search snippets) | Avoid |

From the literature (fact only), AltecOnDB: "These dots and strokes are
called delayed strokes since they are usually drawn last in a handwritten
word-part/word."

### 1.3 References for the correct order

| Code | Reference | What it gives | Copyright | Verdict |
|---|---|---|---|---|
| **[SA-G1]** | **Saudi MoE, لغتي, Grade 1, Part 2**, "طبعة 1447 – 2025", National Center for Curricula, 237 pp. (`stroke/refs/curriculum/`, via a mirror; the official iEN links returned HTTP 500) | Exercise "أكتبُ الحرفَ وأُكملُ الناقصَ، ثمّ أكتبُ الكلمةَ": **the four forms on a dotted baseline with red arrows and a start circle** (no numbers; dots get no arrows). Read for ض ع ك خ ه ث (checked by eye for ك). Dots printed as **separate rhombi**. p. 10 "أتعلَّمُ فنَّ الخطِّ": "هُناكَ أربعةَ أسطرٍ يُكتبُ بينها خطُّ النَّسخِ" (Naskh is written between four lines) | "ح المركز الوطني للمناهج ، ١٤٤٧هـ"; protected (collective work, 50 years) | **The closest thing to an official per-letter source.** Reference only, do not trace. Part 1 (the other 18 letters) not obtained |
| [EG-G1] | Egypt MoE, اللغة العربية Grade 1, term 1, 2025/26 | The letter order (§1.5) | Protected (Law 82/2002) | Reference |
| [FSI-W] | FSI, "Classical Arabic – The Writing System" (Frank A. Rice, 1952; `lessons/fsi_pc/`) | Every letter in its four forms ("1. Connected to the following letter only. 2. Connected to the preceding and following letters. 3. Connected to the preceding letter only. 4. Standing alone."), handwriting notes ("In handwriting the three upward strokes [of س] are usually omitted and the letter is a horizontal straight line"); no stroke arrows | **US public domain** (§105; the author was an FSI instructor) | Reference; the only free-to-copy one |
| [MAS] | J. Wightwick & M. Gaafar, *Mastering Arabic Script: A Guide to Handwriting* (Palgrave Macmillan, 2005; consultant: Academy of Arabic Script, Cairo). Read through a transcript: **check against a copy** | "Complete the main letter shape first and then add any 'dots'"; "Double and treble dots above and below letters become dashes and 'caps'"; "Complete the main shape of the word. Add the letter dots from right to left. Add the vowel and doubling signs from right to left." Per letter: ط ظ "loop at the bottom first in a clockwise direction, and then add the vertical stroke downwards"; ص ض "start the loop on the left, in the centre of the letter"; م "start the letter shape in the middle, forming a tight circle in a clockwise direction"; ك "write the lower part of the letter, but leave the upper slanted stroke as an addition"; ل "take your pen to the top of the medial and final lam first, before returning down the same path"; hamza "after completing the alif" | © Palgrave | Reference: the best English rule set |
| [KG] | Egyptian kindergarten worksheet "شيتات مراجعة الحروف", quoted on arabeduguide.com (2026-06) | Spoken pen paths: ب "نزل بشرطة مائلة من أعلى اليمين / نمد الشرطة الأفقية على السطر ثم نرتفع لأعلى بميل بسيط / ثم نضع نقطة أسفل الطبق"; ش "طبق صغير.. كمان طبق صغير ثم طبق كبير.. ثلاث نقاط فوق الحرف"; ك "نرسم (ا) نازل على السطر.. نعمل منه طبق ماشي.. نحط فيه كاف صغيرة"; ي "نرسم اتنين صغيرة.. ثم نرسم طبق تحت السطر طالع لحد السطر" | © | Reference |
| [BAG] | Hashem al-Baghdadi, كراسة قواعد الخط العربي (1961); al-Baghdadi died 30 April 1973 | Calligraphic (reed-pen) construction in dot measures; not obtained | Life + 50: expired at the end of 2023 under the SA, EG, AE and JO rules (Iraq unverified); reprints may add new matter | Not needed (calligraphy, not the school ball-point hand) |
| [UNI] | Unicode 18.0 core spec, ch. 9 | Joining types; "lam-alef ligatures are considered obligatory"; "the lam is the right part of the ligature, and the alef is the left part"; digit forms (Tables 9-1, 9-2) | Unicode License v3 | Facts |
| [WP] | Wikipedia (en, ar) | "six letters (و ,ز ,ر ,ذ ,د ,ا) can only be linked to their preceding letter"; shadda: "the fatḥah is written above the shaddah… kasrah… is written between the consonant and the shaddah"; Ruqʿah's two dots one dot wide ("ويقاس عرض النقطتين بنقطة") | CC BY-SA 4.0 | Facts only |
| [MM], [TY] | §1.1 data | Counts, tips | None / MIT | Weak cross-check |
| [ALT] | AltecOnDB paper (arXiv 1412.7626; `stroke/datasets/`) | "delayed strokes ... usually drawn last" | arXiv | Fact |

### 1.4 The rules

#### 1.4.1 General rules

1. **Right to left**, letters and words. Digits inside a number run left to
   right (the most significant on the left).
2. **The body (rasm) first, in one continuous stroke.** Every letter joined
   to the next is written without lifting the pen: [SA-G1]'s arrows run
   through the connectors, with the start circle on the right connector of
   medial and final forms. The pen lifts only after a non-connector (ا د ذ ر
   ز و) or at the end of the word.
3. **Then the dots, right to left** [MAS, ALT, KG]. The school model writes
   separate dots [SA-G1 ث]; everyday handwriting joins two dots into a dash
   and three into a cap ("^") [MAS]; in Ruqʿah the two dots are one stroke
   [WP-ar].
4. **Then the other detached parts**: the vertical of ط ظ, after the loop
   [MAS, MM, TY]; the top bar of medial ك, a separate stroke drawn left to
   right and upward [SA-G1]; the small mark inside isolated and final ك, last
   [KG]; hamza "after completing the alif" [MAS]; madda.
5. **Harakat last**: "Add the vowel and doubling signs from right to left",
   after the dots [MAS]; shadda before the vowel that sits on it [WP].
   Teach the marks after the whole word; accept them after each word part
   (adults vary [ALT]).
6. **Loop rotation** (synthesis; every per-letter datum agrees): a loop on
   the baseline whose stroke leaves leftwards turns **clockwise from its left
   side**: ص ض [SA-G1, MAS], ط ظ [MAS], م [MAS], the medial and final heads of
   ع غ [SA-G1], isolated ه [SA-G1]; ف ق و inferred. Exceptions: the head of
   isolated and initial ع غ (and ء) turns **counter-clockwise from the top
   right** [SA-G1]; final ـه goes up the stem, then counter-clockwise
   [SA-G1]. The C-bowl of ج ح خ ع غ is a counter-clockwise arc; the U-bowl of
   ن س ش ص ض ق ل ي, drawn right to left, a clockwise one. **Store the rotation
   per stroke** (as for Thai).
7. **Some movements run left to right**: the head (حاجب) of ج ح خ [SA-G1, KG];
   the top bar of medial ك [SA-G1]; the bottom of final ـه [SA-G1].
8. **Up and back down the same path** in connected forms: final ـك [SA-G1],
   medial and final ل [MAS], the tooth of medial and final ـثـ ـث [SA-G1].

#### 1.4.2 Letters (isolated form unless stated)

Stroke counts include dots and marks (in brackets: with joined dots).

| Letter | Strokes | Start, path | Sources | Conf. |
|---|---|---|---|---|
| ا | 1 | Top → down to the line | MAS, KG, MM, TY | High |
| ب | 2 | Top of the right tooth, just above the line → down → left along the line → small rise at the left tip; dot below | SA-G1 (ث's body), KG, MM | High |
| ت | 3 (2) | ب body; two dots above, right then left | SA-G1, MAS | High; dot order medium |
| ث | 4 (2) | ب body; three dots (one above two), separate in the school model | SA-G1 | High; dot order: house |
| ج | 2 | Top left of the head → head **left to right** → sharp turn back down-left → bowl below the line (counter-clockwise), ending up at the right; dot in the bowl | SA-G1 (خ), KG, MAS | High |
| ح | 1 | As ج | SA-G1 | High |
| خ | 2 | As ج; dot above the head | SA-G1, KG | High |
| د | 1 | Top tip → down-right to the corner on the line → left along the line | MM; no primer arrow seen | Medium |
| ذ | 2 | د + dot | — | Medium |
| ر | 1 | Small head at or above the line → curve down-left below the line | KG, MAS, MM | High |
| ز | 2 | ر + dot | — | High |
| س | 1 | Top of the right tooth → three teeth right to left → deep U-bowl below the line, up at the left | KG, MAS, MM | High |
| ش | 4 (2) | س + three dots | KG | High |
| ص | 1 | Left side of the loop, mid-height → loop clockwise (up, over to the right, down, back left along the line) → tooth → U-bowl | SA-G1 (ض), MAS | High |
| ض | 2 | ص + dot over the loop | SA-G1 | High |
| ط | 2 | Loop as ص, ending along the line; then the vertical, top → down at the loop's left | MAS, MM, TY | Medium-high |
| ظ | 3 | ط + dot right of the vertical | — | Medium-high |
| ع | 1 | Head from the top right, counter-clockwise; at the waist into the bowl below the line (counter-clockwise), ending at the right | SA-G1, MAS | High |
| غ | 2 | ع + dot | — | High |
| ف | 2 | Head loop clockwise from its left (base junction) → base leftwards, rising at the end; dot above | Inferred from SA-G1/MAS loops | Medium (house) |
| ق | 3 (2) | Loop as ف → deep U-bowl; two dots | Inferred | Medium (house) |
| ك | 2 | Stem top → down → left along the line, rising at the end; then the small inner mark | SA-G1, KG | High (body); mark medium |
| ل | 1 | Top → down → U-bowl below the line, up at the left | MAS, MM | Medium-high |
| م | 1 | Head: tight clockwise loop started "in the middle" → tail straight down below the line | MAS | Medium-high |
| ن | 2 | Right tip above the line → deep U-bowl → up at the left; dot centred above | MAS, MM | High |
| ه | 1 | Isolated (round): top → clockwise all round. Initial هـ: top → down the right side → left along the line → up into the inner eye. Medial ـهـ (figure-8): connector → lower loop → upper loop clockwise → out along the line (medium). Final ـه: connector → up the stem → down over the left (counter-clockwise) → bottom left to right | SA-G1 (all four forms) | High; medial medium |
| و | 1 | Head loop, then the tail down-left below the line; loop clockwise from the junction proposed | Inferred; MAS; MM | Medium (house) |
| ي | 3 (2) | Small head from the top right → U-bowl below the line, rising back to the line; two dots below, right then left | KG, WP-ar | Medium-high |
| ء | 1 | Top right → small counter-clockwise arc → short slanted dash | KG | Medium |
| أ إ | 2 | ا, then hamza above or below | MAS | High |
| آ | 2 | ا, then madda, a wave right to left (direction inferred) | MAS (shape) | Medium |
| ؤ | 2 | و, then hamza | — | High |
| ئ | 2 | Dotless ى (or the tooth when joined), then hamza; never dots | UNI | High |
| ة | 3 (2) | Isolated ه or final ـه body, then two dots above | SA-G1 | High |
| ى | 1 | ي without dots | — | High |
| لا | 2 | Lām first, top → down to the base; then alif from the top left down to meet it | TY, UNI | Medium (house; some write one stroke) |

**Positional forms seen in [SA-G1]:**
- ع: initial = the head counter-clockwise from the top right, then left
  along the line; medial and final = from the right connector up into the
  head, clockwise over the top, then along the line (medial) or into the
  tail (final).
- ك: initial = **one stroke**: top bar right to left, diagonal down-right,
  then left along the line; medial = from the connector up the diagonal,
  then the top bar as a **separate stroke, left to right, upward**; final =
  connector → up the stem → back down → base and rise; isolated = stem top
  → down → base → rise.
- خ: initial = head left to right, then the line; medial = connector → head
  left to right → line.
- ض: initial = loop clockwise from the left, line leftwards, then the closing
  tooth; medial and final = connector → loop → tooth / bowl.
- ث: initial = down from the tooth, then left along the line; medial and
  final = connector → up the tooth → down → line.

#### 1.4.3 Harakat and marks

| Mark | Order and direction | Source; confidence |
|---|---|---|
| All harakat | After the word's body and dots, right to left | MAS; high |
| fatha ـَ, kasra ـِ | Short oblique dash, upper right → lower left; above / below | Common practice; medium |
| damma ـُ | A small و: loop, then tail | MAS; medium |
| sukun ـْ | Small circle (rotation: house) | MAS; medium |
| shadda ـّ | A small "w", right to left, **before** its vowel; fatha above it, kasra between the letter and the shadda (or below the letter) | WP, MAS; high |
| tanwin ـً ـٌ ـٍ | The doubled marks, right to left / upper first; fathatan on the letter before the final alif ("يجب أن يكتب التنوين على الحرف الذي يسبق الألف الأخيرة") | WP-ar; medium |
| madda | Wave above the alif, right to left | Medium |
| hamza above/below | After the seat letter | MAS; high |
| dagger alif ـٰ | Short vertical, top → down | Medium |

#### 1.4.4 Digits

**No source found gives the strokes of any digit.** Teach the Arabic-Indic
digits U+0660–0669 (٠١٢٣٤٥٦٧٨٩), not the Persian/Urdu forms ۴ ۵ ۶ ۷: "There
are distinct glyph forms for Eastern Arabic-Indic digits for the digits
four, five, six, and seven" [UNI]. The Maghreb uses 0–9 [UNI]. House
proposals, all one stroke, for a teacher to confirm: ٠ a dot or small
rhombus; ١ top → down; ٢ ٣ from the right tip of the top tooth (or teeth)
leftwards, then down the stem; ٤ from the top right, two leftward bulges
down; ٥ a closed loop from the top, clockwise; ٦ top bar left to right, then
down; ٧ top left → down to the point → up to the top right; ٨ bottom left →
up to the apex → down to the right; ٩ head loop, then the stem down.

#### 1.4.5 Joining and positional forms

- Non-connectors (join only on the right): ا د ذ ر ز و (Unicode also lists
  ة as right-joining); ء never joins; ة and ى are word-final only.
- The connector is a short baseline stretch inside the one continuous body
  stroke. Kashida (U+0640 TATWEEL) is a stretched connector: not for
  beginners.
- لا is obligatory in Naskh [UNI].

#### 1.4.6 Naskh vs Ruqʿah; school form vs print

- **Saudi Grade 1 teaches Naskh**, on four lines [SA-G1 p. 10]. High.
- When Ruqʿah starts (grade 3, 4 or later) is **unverified** in every
  country checked (only a secondary product title, "لغتي والخط - خط الرقعة
  للسنة الرابعة").
- The school model is a print-like Naskh with **separate rhombic dots**
  [SA-G1]; everyday handwriting joins dots into dashes and caps and flattens
  the teeth of س ش [MAS, FSI-W]. Teach the school form; let the matcher
  accept joined dots.

#### 1.4.7 Where the sources disagree (house decisions)

1. Initial ك: one stroke with the top bar first, right to left [SA-G1], or
   the top stroke added after the shape [MAS]. Medial: both agree it is
   separate (SA: left to right, upward). **Recommend SA-G1.**
2. Dot order in ت ث ش ق ي; whether to teach the dash and cap (accept both).
3. The vertical of ط ظ: after the loop [MAS, MM, TY]; no primer arrow seen.
   **Recommend after.**
4. Loop rotation of ف ق و (inferred clockwise); sukun's and ه's eye.
5. لا in two strokes (lām, then alif) or one.
6. Marks after each word part or after the word; harakat after the word
   (recommended).
7. The direction of ك's inner mark.
8. Every digit (§1.4.4).
9. Medial ه: the figure-8 ـهـ of [SA-G1] or the "ـﮩـ" form, and its path.

### 1.5 Teaching order

**Saudi لغتي, Grade 1** (high confidence; Part 2's contents and its review
of Part 1, p. 6 "مُراجعةُ الحُروفِ الَّتي سَبَقَتْ دِراسَتُها في الجزء الأول"):

| Unit | Theme | Letters |
|---|---|---|
| 1 | أسرتي | م ب ل د ن ر |
| 2 | مدرستي | ص ف س ق ت ح |
| 3 | مدينتي | أ ط ز و ج ش |
| 4 | صحتي وسلامتي | ض ع ك خ ي ذ |
| 5 | ألعابي وهواياتي | هـ ث غ ظ |

**Egypt, Grade 1, term 1, 2025/26** (medium-high): unit 1 ا ب م ح ج د خ ت;
unit 2 ل س ن ر ف ك ق; unit 3 ي ع ش و هـ ذ ظ; unit 4 ز ط ص ض ث غ.

Both primers use a **word-building, frequency-led order**, not the hijāʾī
(ا ب ت ث ...) or abjadī (أبجد هوز) order, nor shape groups. UAE and Jordan:
unverified.

**For foreign learners:** *Alif Baa* (3rd ed., Georgetown UP 2021; the
publisher's contents) teaches hijāʾī order in shape families: unit 2 ا ب ت ث
and و ي as long vowels; 3 ج ح خ, sukūn, consonant و ي; 4 ء, numerals, د ذ ر
ز; 5 shadda, س ش ص ض; 6 ة ط ظ ع غ; 7 ف ق ك ل لا; 8 م ن هـ آ. FSI's 1952
booklet [FSI-W] starts with the non-connectors (ا د ذ ر ز و) and the vowel
signs, then the joining letters one by one.

**Shape groups** (one body, different dots): ب ت ث (and ن ي in initial and
medial forms) / ج ح خ / د ذ / ر ز / س ش / ص ض / ط ظ / ع غ / ف ق.

**Recommendation:** shape groups in hijāʾī order (as *Alif Baa*) for our
learners, with the non-connectors flagged: each group reuses one body
stroke, which suits both the learner and our stroke data.

## 2. Character metadata and joining

UnicodeData.txt (18.0.0) has 256 assigned code points in the Arabic block
U+0600–U+06FF (extract in `roman/unicode_arabic_block_UnicodeData.txt`).
ArabicShaping-18.0.0.txt (dated 2026-08-18; `roman/unicode_ArabicShaping.txt`)
gives each letter's joining type:
- **Dual-joining (D)**, four forms: ب ت ث ج ح خ س ش ص ض ط ظ ع غ ف ق ك ل م ن
  ه ي (22 of the 28), and ئ.
- **Right-joining (R)**, joined only to the letter before, two forms:
  **ا د ذ ر ز و** (the six "non-connectors"), and آ أ إ ؤ ة.
- **Non-joining (U)**: ء.
- ى is D in Unicode (it joins medially in other languages) but final-only in
  Arabic.

Unicode encodes one character per letter; the font's shaping chooses the
positional form (the presentation forms U+FB50–U+FEFF are for legacy
display only). So our stroke data must hold the four forms of each letter
itself, and the app must know each letter's joining type (from this file).
Letter names (alif, bāʾ, tāʾ ...) and sounds are facts; build our own table.

## 3. Dictionary

| Source | Covers | Licence (quoted) | OK? | Verdict |
|---|---|---|---|---|
| **English Wiktionary via kaikki.org** | See below | Wiktionary dual licence (above) | Yes (CC BY-SA 4.0) | **Core**: vocalized headwords, romanization, English, roots, inflection tables |
| **French Wiktionary** (kaikki, "Arabe", dump 2026-10-01, file 12.2 MB) | 12,804 entries, 11,703 distinct words; **10,701 non-name words with French glosses** (27,265 senses); a vocalized form on 11,390 entries; no romanization field (transcriptions sit inside the examples: "نُورُ الشَّمْسِ (nūru aš-šamsi)"); 7,818 distinct words once vowel marks are stripped (some page titles are vocalized) | Same | Yes | **French** (the only sizeable non-English edition) |
| Japanese Wiktionary (kaikki, アラビア語, dump 2026-10-01) | 1,316 entries, **951 non-name words** with Japanese glosses (1,754 senses); transliteration on 471 | Same | Yes | Japanese supplement |
| Portuguese Wiktionary (kaikki, Árabe, dump 2026-09-01) | 1,363 entries, **1,148 non-name words** (1,634 senses), vocalized on 325 | Same | Yes | Portuguese supplement |
| Spanish Wiktionary (kaikki, Árabe, dump 2026-10-01) | 731 entries, **659 non-name words** (856 senses), romanization on 527 | Same | Yes | Spanish supplement |
| **Wikidata lexemes** (Arabic, Q13955; query service, 2026-10-04) | **85,921 lexemes** but only **16,380 senses**, and 1,821,165 forms (bot-imported inflections). Sense glosses by language: ar 14,270, en 1,900, fr 145, es 3, pt 1, **ja 0**. 1,081 senses (915 lexemes) link to an item (P5137), whose labels could give es/pt/ja meanings (the per-language label count could not be run: the query service was "Aggressively rate-limiting to 1 req / min" during an outage) | "All structured data from the main, Property, Lexeme, and EntitySchema namespaces is available under the Creative Commons CC0 License" | Yes, no conditions | Small: at most ~1,000 senses (Hindi had ~1,500) |
| **Arabic WordNet v2** (Abouenour, Bouzoubaa, Rosso 2015; OMW `wns/arb`, `wn-data-arb.tab`) | 37,335 lemma–synset rows: **9,916 synsets, 17,785 distinct lemmas (17,699 vocalized)**; 14,683 root rows; 2,948 broken plurals; linked to Princeton WordNet 3.0 | LICENSE: "The Arabic WordNet data in the distribution is subject to the Creative Commons Attribution-ShareAlike license, Version 3.0" | Yes | Roots (cross-check); synset links to the es/fr/pt/ja wordnets (each under its own licence) |
| Buckwalter Arabic Morphological Analyzer 1.0 (LDC2002L49) | "prefixes (299 entries), suffixes (618 entries), and stems (82,158 entries representing 38,600 lemmas)", English glosses | LDC: "License(s): GNU General Public License v2"; "Organizations interested in licensing the lexicon and/or morphological analyzer for commercial use should contact: QAMUS LLC" | **No** (GPL data) | Avoid |
| BAMA 2.0 (LDC2004L02); SAMA 3.1 (LDC2010L01) | Successors | "BAMA Agreement"; "LDC Standard Arabic Morphological Analyzer (SAMA) Version 3.1 Agreement" | No | Avoid |
| Arramooz (Taha Zerrouki, `linuxscout/arramooz`) | Vocalized nouns, verbs and stop words with roots | LICENSE: "GNU GENERAL PUBLIC LICENSE Version 2"; README: "License [GPL]" | **No** | Avoid |
| Qutrub (verb conjugator), Qalsadi (analyzer), Tashaphyne (stemmer), Mishkal (vocalizer), arabic-roots, arabic-affixes, Tashkeela2 (vocalized corpus), Alyahmor, Ghalatawi, pyarabic (all `linuxscout`) | Conjugation, analysis, roots, affixes, vocalized text | Qutrub README: "This program is licensed under the GPL License"; Qalsadi README: "License [GPL]"; Tashaphyne LICENSE: "GNU GENERAL PUBLIC LICENSE Version 3"; GitHub reports GPL-2.0/GPL-3.0 for the others (`morph/linuxscout_repos.json`) | **No** | Avoid; read for the linguistics only |
| "Ghalib" | No Arabic data project of that name found (GitHub search: 0 results); perhaps Ghalatawi (above) | — | — | — |
| Ayaspell (Hunspell Arabic dictionary) | Word list + affix rules | COPYING: "GPL 2.0/LGPL 2.1/MPL 1.1 tri-license" | Possible under MPL 1.1 (file-level copyleft) | Not needed (the platforms have Arabic spell-checkers) |
| FreeDict ara-eng (52,996 headwords) and eng-ara (87,424), from Arabeyes' Wordlist | Word lists | TEI header: "Available under the terms of the GNU General Public License ver. 2.0 and any later version" | **No** | Avoid |
| PanLex | Translations | "Commercial use of these materials is permitted only by obtaining written permission from PanLex" (CC BY-NC-SA 4.0) | No | Avoid |
| SAMER Readability Lexicon (NYU Abu Dhabi) | Lemmas with readability levels | "A license to use and copy this software, data and its documentation solely for your internal research and evaluation purposes ... 2) no rights to sublicense or further distribute this software are granted; 3) no rights to modify this software are granted" | No | Avoid |
| Hans Wehr, *A Dictionary of Modern Written Arabic* (ed. J. Milton Cowan) | The standard learner's dictionary; Wiktionary's headwords and romanization follow its conventions | Commercial, copyrighted (Wehr died 1981) | No | Reference only; a romanization *system* is not protected |
| Lane (1863–93), Hava (1899), Steingass (1884) | Classical Arabic | Public domain; digitisations not checked | — | Archaic; not needed |
| Arabic Wiktionary | Arabic-language definitions | CC BY-SA 4.0 | — | Not on kaikki; Arabic–Arabic, not needed |

**English Wiktionary** (kaikki.org/dictionary/Arabic/, "extracted on
2026-09-28 from the enwiktionary dump dated 2026-09-02"):
- Quoted on the page: "All word forms (59484 distinct words)"; senses by
  part of speech: verb 56,391, noun 31,407, adjective 8,965, proper name
  2,944 (the verb count includes inflected-form entries).
- The per-language JSONL is 521,119,592 bytes ("497.0MB") and **marked
  "DEPRECATED, will be removed in the near future"**. Measured on the whole
  file, fetched gzip-compressed (50,729,267 bytes on the wire; kept in the
  scratchpad, a 63-entry sample in `dict/kaikki_en_arabic_sample.jsonl`):
  - **78,135 entries**: 27,249 real entries (at least one sense that is not
    "form of"), 50,886 only inflected or alternative forms.
  - **27,249 lemma entries, 47,005 senses**; 20,544 distinct headwords
    (18,589 without proper names). By part of speech: noun 12,728; verb
    7,233; adjective 3,770; proper name 2,368; adverb 368; phrase 134;
    preposition 121; interjection 93; conjunction 89; numeral 54; particle
    51; pronoun 50.
  - **A vocalized headword on 26,972 of 27,249** (the `canonical` form:
    كتاب → كِتَاب; verbs with their final vowel, كَتَبَ). The entry guidelines:
    "Verbs are shown with full ʔiʕrāb ... Triptote and diptote nouns,
    adjectives, and participles ... are normally shown without the ʔiʕrāb"
    ("generally follows Hans Wehr").
  - **Romanization on 26,672** (`kitāb`); IPA on 17,800; audio on 837
    (Commons files, each under its own licence).
  - **Inflection tables**: 1,449,518 vocalized inflected forms with
    romanization (كِتَابٌ kitābun, الْكِتَابُ al-kitābu, كُتُب kutub ...),
    288,069 distinct once unvocalized; 7,140 verbs have a full conjugation
    table.
  - 7,657 example sentences (5,715 romanized), many of them Qur'anic.
  - Glosses in English only. Dialect pronunciations appear as extra IPA lines
    (Egypt, Levant, Gulf, Morocco ...).
- Wiktionary's own category sizes (API, 2026-10-04; `dict/wiktionary_en_arabic_categoryinfo.json`):
  "Arabic lemmas" 23,189 pages; "Arabic non-lemma forms" 157,887; "Arabic
  terms by root" 3,070 root categories with 14,762 page memberships in all;
  "Arabic roots" 1,495 root pages; "Arabic terms with IPA pronunciation"
  16,920; "with audio pronunciation" 731.
- If the per-language file goes: the raw wiktextract dump ("compressed .gz
  (2.8GB)") filtered to Arabic gives the same data (wiktextract is MIT).

**Routes to French, Spanish, Portuguese and Japanese meanings**, in order:
1. The native Wiktionary entries: **French 10,701 words**; Portuguese 1,148;
   Japanese 951; Spanish 659. Against the commonest words (wordfreq tokens
   that are English-Wiktionary headwords): of the top 1,000, **fr 596, ja
   214, pt 196, es 180**; of the top 5,000, fr 1,808, pt 485, ja 482, es 421.
2. Wikidata's sense-to-item labels (CC0): ~1,000 senses at most.
3. Arabic WordNet synsets → the Spanish, French, Portuguese and Japanese
   wordnets in OMW (each wordnet's own licence; sense-level, noisy).
4. Wiktionary translation tables (an English entry's table lists Arabic
   beside es/pt/fr/ja): needs the full English extract; not measured.
5. Our own translation of the English glosses, labelled as such (as for
   Chinese es/pt), or ML Kit translation on the device.

## 4. Roots and morphology

Arabic vocabulary is taught by root (ك ت ب → كَتَبَ kataba "to write", كِتَاب
kitāb "book", كَاتِب kātib "writer", مَكْتَب maktab "office", مَكْتَبَة maktaba
"library").

| Source | What | Licence | Verdict |
|---|---|---|---|
| **English Wiktionary (kaikki)** | A root on **11,309 of the 27,249 lemma entries** (the sense category "Arabic terms belonging to the root ك ت ب", or the `ar-root` / `ar-rootbox` / `etymon` templates): verbs 4,727 of 7,233, nouns 4,727 of 12,728, adjectives 1,471 of 3,770; 2,816 distinct roots. Verbs carry their form in the head template (`ar-verb`: I, II, IV, V ...); nouns link to their verb ("verbal noun of كَتَبَ (kataba) (form I)", "active participle of") | CC BY-SA 4.0 | **Use**: root, verb form, derivation links |
| Wiktionary root categories and root pages (API) | 3,070 roots, 14,762 memberships; the 1,495 root pages (title "ك ت ب", lists of derived terms) are **not** in kaikki's file | CC BY-SA 4.0 | Fetch from the API or the dump to fill roots kaikki dropped |
| **Arabic WordNet v2** | 14,683 lemma–root rows | CC BY-SA 3.0 | Cross-check |
| Arramooz; `linuxscout/arabic-roots` ("All roots: 7504 / 3 letters roots: 5674 / 4 letters roots: 1830"); Qutrub | Roots, patterns, conjugation | GPL | **Avoid** |

The rest can be computed: a regular derived word strips to its radicals by
its pattern (مَكْتَبَة on maf3ala → ك ت ب); weak and doubled roots (و ي ء, مدّ)
need the dictionary. Patterns are rules, not data.

## 5. Finding a dictionary word in running text (clitics)

Arabic attaches short words to the next word: the proclitics و wa- "and",
ف fa- "so", ب bi- "with, in", ل li- "for", ك ka- "like", س sa- (future, on
verbs), أ ʔa- (question), and the article ال al- (ل + ال is written لل); and
the pronoun suffixes ـي ـني ـك ـه ـها ـنا ـكم ـكن ـهم ـهن ـكما ـهما. Before a
suffix, ة is written ت and final ى becomes ا. Running text is unvocalized,
and hamza is often dropped (أ إ آ written ا; ى for final ي).

| Option | Licence | In a closed app? |
|---|---|---|
| **Our own rules** (the clitic lists above, longest first, each candidate checked against the dictionary) over **Wiktionary's inflection index** (288,069 unvocalized table forms + 42,142 form entries, each pointing to its lemma; folding أ إ آ → ا, ى → ي, ة → ه as a fallback) | Rules ours; data CC BY-SA 4.0 | **Yes**. Tested on wordfreq's commonest tokens (`morph/clitic_coverage_test.py.txt`): of the top 1,000, **995 resolve** (458 headwords, 420 table forms, 24 after folding, 93 after clitic stripping); of the top 10,000, **9,657 (96.6%)**; of the top 20,000, 18,791 (94.0%). The misses are mostly names and loanwords (تويتر, جورج, جوجل) and compounds written solid (عبدالله). A match is a candidate, not a proof: ambiguous forms need ranking |
| ISRI stemmer (Taghva et al. 2005; NLTK `nltk/stem/isri.py`) | NLTK: Apache License 2.0 ("Copyright (C) 2001-2026 NLTK Project") | **Yes** (reimplement or port; keep the notice). Root-oriented and coarse: fallback for search only |
| Snowball Arabic stemmer (`algorithms/arabic.sbl`, 558 lines; Assem Chelli, Abdelkrim Aries) | BSD-3-Clause: "Redistribution and use in source and binary forms, with or without modification, are permitted provided that ..." | **Yes** (with the notice). Light stemmer: fallback for search |
| CAMeL Tools (NYU Abu Dhabi) | Code: "MIT License / Copyright 2018-2026 New York University Abu Dhabi". Data (`morph/camel_tools_data_catalogue-1.6.json`): `morphology-db-msa-r13` "GPL v2", `morphology-db-msa-s31` "LDC", `disambig-mle-calima-msa-r13` "GPL v2"; Gulf/Levantine DBs "CC BY 4.0"; BERT disambiguators "MIT" (hundreds of MB) | Code yes; **the MSA data no** (GPL/LDC). Offline checks at most |
| Farasa (QCRI) | "QCRI FARASA package for processing Arabic text is being made public for research purpose only. For non-research use, please contact us." | **No** |
| Qalsadi, Tashaphyne | GPL | No |
| Apple NaturalLanguage | Tested on macOS 14.4: `NLTagger.availableTagSchemes(for: .word, language: .arabic)` = Language, Script, TokenType only: **no lemma, no part of speech** (`morph/apple_nltagger_arabic_test_output.txt`) | Word tokens only |
| ICU / Android BreakIterator | Arabic is space-separated; the tokenizers split words, not clitics | Tokens only |

## 6. Example sentences

### Tatoeba

The export of 2026-10-03 (`downloads.tatoeba.org/exports/per_language/ara/`;
saved in `sentences/`). The downloads page: "These files are released under
CC BY 2.0 FR. A part of our sentences are also available under CC0 1.0."

- **Arabic (ara) sentences: 68,570**; median 20 characters. 554
  contributors; the top four wrote 20,020 (29%), 8,980, 6,060 and 5,950.
  Added mostly in 2010 (7,493), 2012, 2018 (7,907) and 2023 (21,905).
- **Vowel marks: 14,054 have at least one, but 9,967 of those carry only
  shadda and/or tanwin fatha (شكرًا). Only 280 are at least half vocalized
  and 91 fully** ("أَخْرَجَ جُوْن مِفْتَاحًا مِنْ جَيْبِهِ."). A learner app
  needs vocalized text: see Gaps.
- Tatoeba keeps dialects apart (arz Egyptian, apc/ajp Levantine, ary
  Moroccan, arq Algerian, acm Iraqi ...); "ara" is meant as Standard Arabic.
  Tags show some classical text ("Quran" 6, "from the Bible" 17, "proverb"
  21).
- CC0: 2 sentences.
- **Audio: 483 sentences, all by one contributor, with an empty licence
  field: not reusable** ("The license covering an audio file is chosen by the
  contributor").

| Translation | Direct | Through English | Either |
|---|---|---|---|
| English | **46,686** (48,653 links) | — | 46,686 |
| French | 2,956 | 18,044 | **19,735** |
| Spanish | 3,081 | 14,715 | **15,806** |
| Japanese | 1,210 | 12,381 | **12,817** |
| Portuguese | 300 | 12,371 | **12,453** |

49,302 sentences have a direct translation in at least one of the five.
(The pivot uses the eng–spa/fra/por/jpn link files fetched on 2026-10-04,
in `common/tatoeba2/`; script in `sentences/tatoeba_count.py.txt`.)

### Other sentence sources

| Source | What | Licence (quoted) | Verdict |
|---|---|---|---|
| **Common Voice Arabic sentences** (`server/data/ar/sentence-collector.txt`, 3.9 MB) | **59,160 sentences**; 25,482 carry vowel marks, **15,955 are at least half vocalized**; median 26 characters. Mixed: everyday sentences, proverbs, Qur'anic verses ("وَسَخَّرَ لَكُمُ الشَّمْسَ وَالْقَمَرَ ..."), some apparently from Tatoeba ("أفسد سامي الحفل.") | README: the sentence text is "released under a CC0 public domain Creative Commons license" (repository code MPL 2.0) | **Usable** for vocalized sentences (no translations); filter religious text; spot-check provenance. The CC0 recordings need a sign-up to download (not done) |
| **MASSIVE** (Amazon), `ar-SA.jsonl` | **16,521 utterances** (train 11,514, dev 2,033, test 2,974); voice-assistant requests, localized: **largely Saudi/Gulf colloquial** ("صحيني خمسة الفجر", "حط منبه بعد ساعتين من الحين"; at least 3,060 contain obvious colloquial words: ابي، وين، حط، اللي، الحين ...) mixed with MSA and English names; 182 carry vowel marks | NOTICE: "The MASSIVE dataset is licensed under CC BY 4.0" | Licence fine, **register wrong for MSA**: pick by hand at most |
| FLORES-200 | ~2,000 public sentences, `arb_Arab` (MSA) + spa/por/jpn/fra; dialects separate (arz, apc, ajp, ary, acm, aeb, ars) | "FLORES-200: CC-BY-SA 4.0" | Usable; advanced (news, Wikipedia) |
| NTREX-128 | 1,997 news sentences, `arb` + spa, por, jpn, fra | "released under the CC BY-SA 4.0 license" | Usable; advanced |
| tico-19 (OPUS) | COVID-19 translation memories, ar–en 3,071 | "Creative Commons CC0 license" | Usable; narrow |
| UN Parallel Corpus (OPUS; ar–en 20.0M) | UN documents | Disclaimer; "the user must acknowledge the United Nations as the source of the information" | Legally fine; wrong register |
| Tanzil (OPUS) | Qur'an translations | "The translations provided at this page are for non-commercial purposes only." | **No** |
| BAREC Corpus v1.0 (CAMeL Lab) | 69,441 MSA sentences (54,845 + 7,310 + 7,286) with 19 readability levels | Card: "license: cc-by-sa-4.0"; but the sentences come from books and magazines (example "Source: Majed", a children's magazine, 1983) that the card cannot license | Reference only |
| OPUS OpenSubtitles v2024 (ar–en 87.9M), CCMatrix/NLLB (49.7M), CCAligned (13.1M), TED2020 (407,595), QED (500,898) | — | Not the packagers' to license, or NC/ND | Don't bundle |

## 7. Word frequency

| Source | What | Licence | Verdict |
|---|---|---|---|
| **wordfreq** (`rspeer/wordfreq`, `wordfreq/data/`) | **Arabic has the large list**: `large_ar.msgpack.gz` (3,063,585 bytes, **620,701 words**, 586,435 in Arabic script) and `small_ar.msgpack.gz` (259,359 bytes, 56,642 words); 57,357 words at zipf ≥ 3, 11,342 at zipf ≥ 4. README: "Arabic ar 5 Yes │ Yes Yes Yes - Yes Yes - -" (Wikipedia, subtitles, news, web, Twitter) | README: "freely redistributable under the Apache license ... and it includes data files that may be redistributed under a Creative Commons Attribution-ShareAlike 4.0 license" | **Use** (as in the other apps). It counts **surface tokens**: clitics stay attached (zipf: كتاب 5.38, الكتاب 5.47, والكتاب 4.09, بالكتاب 3.73) and hamza spellings are separate (إن 6.27, ان 6.31); "In Arabic and Hebrew, it additionally normalizes ligatures and removes combining marks". So a lemma's frequency = the sum over its forms after clitic stripping and folding (§5). Mixed register (Twitter, subtitles). The project is sunset (no updates) |
| FrequencyWords 2018 `ar_50k.txt` (Hermit Dave) | 50,000 rows from OpenSubtitles (row 1 "،", then لا من في أن هذا), `ar_full.txt` 42.4 MB | "MIT License for code. CC-by-sa-4.0 for content." | Second opinion (spoken register) |
| Leipzig Corpora Collection (Arabic) | Word lists | Site behind an anti-bot challenge (Anubis) on 2026-10-04; Wiktionary labels it "(CC BY-4.0)": **unverified** | Not used |
| Leeds Internet-corpus lists (Sharoff) | Arabic frequency lists | corpus.leeds.ac.uk timed out; Wiktionary says "(CC BY-2.5)": unverified | Not used |
| Wiktionary "Frequency lists/Arabic5000" | 2,043 numbered entries with glosses | Pasted in one 120 KB edit on 2025-07-12 by one user, no source given, cut off mid-entry ("evid…") | **Avoid** (provenance unclear) |
| Wiktionary Qur'an frequency appendices | Qur'anic vocabulary | CC BY-SA | Wrong register |
| Buckwalter & Parkinson, *A Frequency Dictionary of Arabic* (Routledge, 2011) | 5,000 lemmas | Commercial | No |

## 8. Levels

**There is no open, commercially usable graded Arabic vocabulary list.**

| Source | What | Licence | Verdict |
|---|---|---|---|
| **Kelly project, Arabic list** (`ar_m3.xls`, University of Leeds; ssharoff.github.io/kelly) | **8,893 lemmas with CEFR levels**: A1 775, A2 1,246, B1 1,553, B2 1,703, C1 1,440, C2 2,176 (measured; the file was not kept) | "The lists are available under the CC BY-NC-SA 2.0 license." (Only the Swedish Kelly list is CC BY 4.0, at Språkbanken.) | **No** (NC) |
| SAMER Readability Lexicon | Lemmas in 5 readability levels | Internal research only (§3) | No |
| BAREC | Sentence levels (19) | §6 | Reference only |
| ACTFL / ILR | Proficiency descriptors; no word list | — | Level names only |
| Textbook syllabi (Al-Kitaab, Mastering Arabic ...), national curricula for non-native speakers | Commercial or no public list | — | No |

**Grade by frequency** (wordfreq lemma sums, FrequencyWords as a check),
with hand-made first lessons (greetings, numbers, family, classroom words),
as in the other apps.

## 9. Romanization

| Scheme | What | Status | Verdict |
|---|---|---|---|
| **Wiktionary's** (in the kaikki data) | "based on the system found in Hans Wehr *A Dictionary of Modern Written Arabic*, 4th edition", with changes: "Hamzas are always written ʔ regardless of which letter they sit on"; ʕ for ع; **ḵ** for خ and **ḡ** for غ; ṯ ḏ š; ḥ ṣ ḍ ṭ ẓ; ā ī ū; -iyy, -uww, -ay, -aw; ة -a, or -at in the construct state; "Assimilation and elision of the definite article *is* shown" (aš-šams); clitics hyphenated (wa-l-kitāb); "Capitalisation of proper nouns or beginnings of sentences is dispreferred" | Made by `Module:ar-translit` (Lua, CC BY-SA; saved) from the vocalized spelling, with manual overrides for loans | **Use its output**; don't port the module |
| ALA-LC (Library of Congress, 2012 table; `roman/alalc_arabic_romanization.pdf`, 10 pages) | b t **th** j ḥ **kh** d **dh** r z s **sh** ṣ ḍ ṭ ẓ ‘ (ayn) **gh** f q k l m n h w y; ā ī ū; aw, ay | US government work (§105) | A good learner basis (digraphs, few special letters) |
| DIN 31635 (1982) | One letter per consonant: ʾ ṯ ǧ ḥ ḫ ḏ š ṣ ḍ ṭ ẓ ʿ ġ | The standard's text is sold; a scheme is a system, free to implement | Scholarly (Europe); ǧ ḫ ġ are odd for learners |
| ISO 233 (1984) / ISO 233-2 (1993) | Letter-by-letter transliteration | As DIN | Not for learners. ICU's "Arabic-Latin" gives this style (tested on macOS: كتاب → "ktạb"; كِتَابٌ → "kitābuⁿ") |
| Buckwalter | One ASCII character per Arabic letter (ktAb; $ = ش, E = ع) | Free scheme | Internal keys only |
| "Arabic chat" (3 = ع, 7 = ح, 5 = خ, 2 = ء) | Informal typing | — | Accept as typed search input at most |

The non-ASCII letters in Wiktionary's romanizations (measured on all
romanizations, commonest first): ā ʔ ʕ ḥ ī ū š ṭ ṣ ḵ ḡ ḍ ṯ ḏ ẓ, plus ō ē in
loans. **Recommendation:** show a learner scheme close to ALA-LC and DIN,
converted from Wiktionary's by a fixed table (ʔ → ʾ, ʕ → ʿ, ḵ → kh, ḡ → gh,
ṯ → th, ḏ → dh, š → sh; ḥ ṣ ḍ ṭ ẓ ā ī ū kept), keeping the article's
assimilation and the hyphens; check that the Latin font has ʾ ʿ (U+02BE,
U+02BF) and the dotted letters.

## 10. Google ML Kit and the device's voices

From Google's and Apple's pages (fetched 2026-10-04; copies in `mlkit/`):

- **Digital Ink Recognition** (developers.google.com/ml-kit/vision/digital-ink-recognition/base-models,
  "Last updated 2024-07-10 UTC"): "Arabic, Arabic script. | `ar` |
  `ar-x-gesture`". Other Arabic-script models: `fa`, `ur`, `ur-PK`. The
  model downloads at runtime.
- **Text Recognition v2: Arabic is not supported** (…/text-recognition/v2/languages,
  "Last updated 2024-07-10 UTC"). The Supported list (37 rows) has only the
  Latin, Devanagari, Chinese, Japanese and Korean scripts; the Experimental
  list (16) only Latin and Devanagari; the Mapped list (122) maps to the
  Latin, Cyrillic or Chinese models. "Arabic", "Persian", "Urdu" and
  "Hebrew" appear 0 times on the v2 pages. So "text from a photo" needs
  another engine:
  - **iOS: Apple's Vision.** Apple does not publish Vision's language list.
    On macOS 14.4.1 (the iOS 17 generation), `supportedRecognitionLanguages()`
    for revision 3 "accurate" returns "en-US, fr-FR, it-IT, de-DE, es-ES,
    pt-BR, zh-Hans, zh-Hant, yue-Hans, yue-Hant, ko-KR, ja-JP, ru-RU, uk-UA,
    th-TH, vi-VT": **no Arabic** (`mlkit/mac_vision_tts_probe_20261004.txt`).
    A third-party test on iOS 18 (KKday tech blog) lists `"ar-SA",
    "ars-SA"` as well: **Arabic OCR probably from iOS 18** (unverified by
    Apple; check on the oldest iOS we support). Apple's iOS 27
    feature-availability page does **not** list Arabic under "Live Text",
    but does under "On-device text detection in Magnifier" and "Visual Look
    Up" ("Arabic (Saudi Arabia)").
  - **Android: Tesseract** `tessdata_fast/ara.traineddata` (1,432,056 bytes;
    `tessdata_best/ara` 12,603,724); the repository's LICENSE is "Apache
    License Version 2.0, January 2004". Whether it reads harakat: to test.
- **Translation** (…/translation-language-support, "Last updated 2026-09-28
  UTC"): "`ar` | Arabic". Through English for other pairs; Google's
  attribution rules apply.
- **Apple Pencil**: the iOS 27 page lists "Arabic" for "Scribble",
  "Searchable Handwriting", "Copy Handwriting as Text" (system text input,
  not a stroke-grading API).
- **To test with the ink model**: what `ar` returns for a lone letter in its
  isolated vs initial/medial/final form; whether dots, hamza and harakat
  drawn as separate strokes come back; a letter drawn without its dots (ٮ).

**Voices:**
- **iOS/iPadOS**: Apple's "Accessibility: VoiceOver, Live Speech, Read &
  Speak" list (iOS and iPadOS 27) **does not include Arabic** (it lists
  Farsi and Hebrew); Arabic is listed for Dictation ("Arabic (Saudi
  Arabia)", "Arabic (United Arab Emirates)"), Siri, Translate, QuickType
  ("Arabic (Modern Standard)", "Arabic (Najdi)") and the system language.
  The voice exists anyway: on macOS 14.4.1 `AVSpeechSynthesisVoice` returns
  "Majed ar-001 com.apple.voice.compact.ar-001.Maged"; `say -v '?'` shows
  "Majed ar_001 # مرحبًا! اسمي ماجد.". The locale is **`ar-001`**, not
  `ar-SA`; but `AVSpeechSynthesisVoice(language:)` with "ar-SA", "ar",
  "ar-EG" or "ar-AE" also returns Majed on macOS 14.4
  (`mlkit/mac_avspeech_language_lookup_20261004.txt`). On iOS: check
  `speechVoices()` on a device (unverified).
- **Android**: TalkBack's page: "The Google Text-to-Speech languages that
  TalkBack currently supports are: Albanian, Arabic, ...".
- **Harakat (measured with Majed, `say -o`)**: كَتَبَ and كُتِبَ give different
  audio; عَلِمَ (0.604 s) and عُلِمَ (0.557 s) differ. So Majed **reads the
  vowel marks**. Bare علم gives audio byte-identical to عِلْم, and bare مدرسة
  to مَدْرَسَة: for unvocalized text it **guesses one reading**. The app must
  send fully vocalized text to the voice. Google TTS: to test.
- No reusable recorded audio was found: Tatoeba's 483 Arabic recordings have
  no licence; Wiktionary has 837 entries with Commons audio (per-file
  licences); Common Voice's CC0 Arabic clips need a sign-up.

## 11. Fonts

All SIL OFL 1.1 from google/fonts (`ofl/<family>/OFL.txt`; METADATA.pb
`license: "OFL"`); copies in `fonts/<family>/`. Google Fonts lists 57
families with an Arabic subset. A Reserved Font Name only stops a modified
or subset copy from using **that** name: several reserve only the name of
their Latin part (EURM10, "Josefin Sans", "Source", "RevReading Lexend");
SIL's and IBM's reserve the family name, so those must be renamed if
subset.

| Font | Style | Use | Copyright / RFN (quoted from OFL.txt) | Notes |
|---|---|---|---|---|
| **Noto Naskh Arabic** | Naskh, modulated | **Model letters and vocalized lesson text (first choice)** | "Copyright 2022 The Noto Project Authors (https://github.com/notofonts/arabic)"; no RFN | "a modulated ("serif") Naskh design ... 1,598 glyphs"; variable weight; marks stacked separately |
| **Scheherazade New** (SIL) | Traditional Naskh | Alternative for large vocalized text | "Copyright (c) 1994-2026, SIL Global (https://www.sil.org/), with Reserved Font Names "Scheherazade" and "SIL"." | "a traditional naskh typeface, supporting all Arabic script characters in Unicode 14.0"; generous mark spacing; **rename if subset** |
| **Amiri** | Classical Naskh (Bulaq press revival) | Literary text, not first letters | "Copyright 2010-2022 The Amiri Project Authors (https://github.com/aliftype/amiri)."; no RFN | "for typesetting books"; 6,710 glyphs, many calligraphic ligatures |
| Markazi Text | Naskh-based, moderate contrast | Body text, dictionary | "Copyright 2017 The Markazi Text Project Authors (https://github.com/BornaIz/markazitext)"; no RFN | — |
| Alyamama (added 2026-02-18) | "classic Naskh design with sharp, simple strokes" | Headings, body | "Copyright 2025 The Alyamama Project Authors (https://github.com/Mestaratype/AlYamama)"; no RFN | New |
| Lateef (SIL) | Naskh, South-Asian | No | "... with Reserved Font Names "Lateef" and "SIL"." | "appropriate style for use in Sindhi and other languages of the South Asian region" |
| Harmattan, Alkalami, Ruwudu (SIL) | West-African styles | No | RFNs incl. "SIL" | — |
| **Noto Sans Arabic** | Sans | **Interface** | "Copyright 2022 The Noto Project Authors ..."; no RFN | "an unmodulated ("sans serif") design ... 1,642 glyphs" |
| Readex Pro | Sans for readability | Interface | "... with Reserved Font Name "RevReading Lexend"." (the family name is free) | Lexend methodology |
| Tajawal, Almarai, Vazirmatn, Alexandria | Sans | Interface alternatives | No RFN | Vazirmatn leans Persian |
| IBM Plex Sans Arabic | Sans | — | "Copyright © 2017 IBM Corp. with Reserved Font Name "Plex"" | Rename if subset |
| Cairo | Kufi-based display | Headings | No RFN | **No U+25CC dotted circle** |
| Noto Kufi Arabic, Reem Kufi, Qahiri, Fustat, Kufam | Kufic | Decoration | Reem Kufi RFN "Josefin Sans"; others none | Not for teaching |
| El Messiri, Baloo Bhaijaan 2, Lemonada | Display | Children's headings | No RFN | — |
| **Playpen Sans Arabic** (TypeTogether, added 2025-05-12) | Handwriting | Children's look, not a fixed model | "Copyright 2023 The Playpen Sans Project Authors (https://github.com/TypeTogether/Playpen-Sans)"; no RFN | Built-in "shuffler" of alternates and "Ruqʿah alternates": turn off `calt`/`rclt` for stable shapes (to test) |
| **Aref Ruqaa** (+ Ink) | **Ruqʿah** | To show the everyday adult hand | "... with Reserved Font Name EURM10." (the Latin part) | "the classical Ruqaa calligraphic style" |
| Noto Nastaliq Urdu, Gulzar | Nastaliq | Never for MSA | No RFN | Urdu style |
| KFGQPC fonts (Uthmanic Hafs etc.) | Qur'anic | **No** | "This Font is the property of King Fahd Glorious Quran Printing Complex, and may not be reproduced, modified without the express written approval of King Fahd Glorious Quran Printing Complex." (as reproduced by ScanCode LicenseDB) | Not open |

- **No OFL font is made for the school Naskh hand** (searched). Playpen Sans
  Arabic is the only learner-oriented family, and it is a casual hand with
  Ruqʿah alternates.
- **Harakat and digits** (fontTools + HarfBuzz on 13 TTFs;
  `fonts/arabic_font_cmap_check_20261004.txt`): all map every basic letter,
  U+064B–0652, U+0653–0655 and U+0670; all except Cairo have U+25CC (to show
  a lone haraka on a dotted circle); open tanwin U+08F0–08F2 in Amiri,
  Scheherazade, Lateef, Harmattan, the Noto fonts and IBM Plex. All 13 map
  both the Arabic-Indic digits U+0660–0669 and the Persian ones
  U+06F0–06F9, with different outlines for ٤٥٦ and ۴۵۶; no font swaps them by
  language. **The app must store U+0660–0669 for Arabic.**
- **Recommendation:** the model letters come from our own stroke data;
  **Noto Naskh Arabic** to show printed forms and vocalized text
  (Scheherazade New as the large-text alternative, renamed if subset);
  **Noto Sans Arabic** for the interface; optionally Aref Ruqaa to show
  Ruqʿah and Playpen Sans Arabic for a children's look. Avoid Kufi,
  Nastaliq, Lateef, Harmattan and KFGQPC fonts for teaching.

## 12. Lessons

| Source | What | Licence | Verdict |
|---|---|---|---|
| **Wikivoyage Arabic phrasebook** | en "Arabic phrasebook" **29,336 bytes**: "The following phrasebook deals with Modern Standard Arabic"; status "usable"; 1,763 Arabic letters but only 63 harakat (mostly unvocalized), ad-hoc romanization. es "Guía de árabe" 30,625 ("esquemático"; looks machine-translated from en). fr "Guide linguistique arabe" 23,236 ("esquisse"). pt "Guia de conversação árabe" 14,593 (mostly an empty template). **ja: none** (the 会話集 index links アラビア語会話集 to the English page) | "Text is available under the Creative Commons Attribution-ShareAlike License; additional terms may apply." | **en usable** (we add the vowel marks); es/fr thin; pt and ja ours |
| Wikivoyage dialect phrasebooks (en) | Egyptian 47,558; Tunisian 38,776; Jordanian 28,572; Chadian 21,876; Lebanese 20,183 (romanized only); Algerian 13,767; Moroccan 7,949 | CC BY-SA | Optional dialect notes only |
| Wikibooks | en "Arabic" 130 pages, 391,767 bytes; ja 25 pages, 327,566 (アラビア語便覧, アラビア語図鑑); fr "Arabe" 8 pages, 41,674; pt 14 pages, 10,783; es 1 page, 6,581. en Wikiversity 7 pages | CC BY-SA 4.0 | Supplement |
| **FSI "Classical Arabic – The Writing System"** (Frank A. Rice, "Instructor, Foreign Service Institute", Department of State, 1952; 63 pages) | The alphabet, non-connectors, short and long vowels, nunation, sukūn, shadda, letter-by-letter lessons, hamza, the article | US Government work (§105): the author was an FSI instructor | **Usable**; a reference for the teaching order |
| **FSI "Modern Written Arabic" Vol. 1** (1969; 434 pages; "prepared by the Staff of the Foreign Service Institute Arabic Language and Area School in Beirut, Lebanon. Edited by Harlie L. Smith, Jr.") | MSA reading course; vols. 2–3 not downloaded | §105 (whether Beirut local staff were federal employees: unverified) | Usable (old) |
| FSI dialect courses | Saudi (Urban Hijazi) Basic Course (Omar, 1975, 305 pp.; "with the support of the Office of Education"); Levantine pronunciation (Snow, 1971, 108 pp.); "From Eastern to Western Arabic" (1974); Levantine–Egyptian comparative (1976) | §105 likely; authorship unverified | Dialect: reference only |
| "DLI Arabic General Intermediate" (yojik) = Michigan's "Modern Standard Arabic: Intermediate Level" (Abboud, McCarus et al., Ann Arbor, 1971) | Grammar, drills, readings | Copyright page: "Copyright is claimed until November 19, 1981. Thereafter all portions of this work covered by this copyright will be in the public domain. This work was developed under a contract with/or grant from the U.S. Office of Education" | **Usable for the authors' grammar and drills**; the reading passages were "extracted ... from a large number of books and journals": exclude |
| DLI MSA courses (yojik: Basic 593.7 MB, MSA SOLT 789.1 MB; archive.org "MODERN STANDARD ARABIC BASIC COURSE" 7.1 GB scan) | Courses | Likely public domain (DLI faculty are federal employees); per item unverified; may embed third-party media | Possible; not downloaded (over 200 MB) |
| Peace Corps Arabic | Dialect only: Moroccan (1986, 2011), Jordan pre-departure (24 pp.), Tunisian, Hassaniya | §105 only for Peace Corps employees; local teachers likely: unverified | Dialect; use with care |
| **LibreTexts "Introduction to Arabic"**, "Introduction to Arabic II", "Arabic Level One / Three / Four" (Kassas et al., ASCCC OERI) | Beginner courses | Each: "shared under a CC BY 4.0 license" | **Usable** after checking each book's "Detailed Licensing" page and images |
| LibreTexts "Arabic Level Two" | Course | "shared under a CC BY-NC-SA 4.0 license" | No |
| LibreTexts "Elementary Arabic (Mohamed and Issa)" | Labelled CC BY 4.0, but its source (openbooks.lib.msu.edu/elemarabicll) says "licensed under a Creative Commons Attribution-NonCommercial 4.0 International License" | NC at the source | **No** |
| "From MSA to CA" (Lina Gomaa, PDX Pressbooks) | Colloquial transition | "Creative Commons Attribution-NonCommercial 4.0" | No |
| UT Austin COERLL: Tadriis; Aswaat Arabiyya | — | "License: CC-BY-NC"; Aswaat: TV clips "selected from television stations throughout the Arab world" | No |
| Open Textbook Library, OER Commons | — | Not checked (search page unparseable) | Unverified |

## 13. MSA and the dialects

The app should teach **Modern Standard Arabic** (الفصحى): the written
standard of every Arab country, the language of school primers, books, the
press and signs, and the only variety with a fixed spelling. The dialects
(Egyptian, Levantine, Gulf, Iraqi, Maghrebi ...) differ in vocabulary and
sounds and have no standard spelling. The letters and handwriting are the
same for all of them. What each source covers:
- English Wiktionary "Arabic": MSA, with Classical senses and Qur'anic
  quotations labelled; dialect pronunciations as extra IPA lines; dialects
  are separate languages (Egyptian Arabic, etc.).
- Tatoeba "ara": meant as Standard Arabic (dialects have their own codes).
- wordfreq: mixed (Twitter, subtitles); FrequencyWords: subtitles.
- **MASSIVE ar-SA: largely Saudi/Gulf colloquial** (measured, §6).
- FLORES arb_Arab, NTREX arb: MSA.
- Common Voice ar: MSA plus Classical and Qur'anic text.
- Wikivoyage: the English Arabic phrasebook says "The following phrasebook
  deals with Modern Standard Arabic"; the dialect phrasebooks are separate
  pages (§12).
- ML Kit `ar`, Apple's Majed voice: Arabic-script/MSA, no dialect choice.
- The Maghreb uses Western digits (0–9) and a somewhat different handwriting
  tradition; the Arab East uses ٠–٩ and the Naskh/Ruqʿah school hands.

## Best combination

1. **Strokes:** our own centreline strokes in KanjiVG-format XML, as for
   Thai and Hindi: the letter bodies in their positional forms, dots, hamza,
   madda and the ك/ط marks as separate strokes, the harakat and the digits,
   following §1.4 ([SA-G1] where it shows the letter, [MAS] otherwise), with
   the loop rotation stored per stroke; words composed in code (bodies of
   each connected part, then dots, then marks, then harakat).
2. **Dictionary:** English Wiktionary (vocalized headwords, romanization,
   English, roots, inflection tables); French Wiktionary for French; the
   small es/pt/ja editions, Wikidata and our labelled translations for the
   rest. Arabic WordNet's roots as a cross-check.
3. **Word lookup:** our clitic rules over Wiktionary's inflection index;
   Snowball or ISRI as a search fallback.
4. **Sentences:** Tatoeba (vowel marks added and checked), Common Voice's
   vocalized CC0 sentences, FLORES/NTREX for advanced levels.
5. **Frequency:** wordfreq `large_ar`, summed per lemma; FrequencyWords as a
   check.
6. **Levels:** by frequency (no open list).
7. **Romanization:** Wiktionary's, shown in an ALA-LC-like learner scheme.
8. **Lessons:** the English Wikivoyage phrasebook (vocalized by us), FSI's
   writing-system booklet and Modern Written Arabic (public domain),
   LibreTexts' CC BY courses; our own es, pt and ja material.
9. **ML Kit:** handwriting `ar`, translation `ar`; photo text through Apple's
   Vision on iOS 18+, Tesseract or nothing on Android.
10. **Fonts:** Noto Naskh Arabic (model forms, vocalized text), Noto Sans
    Arabic (interface).
11. **Publish** the converted CC BY-SA data, as for Kana.

## Gaps

- No open Arabic stroke data: ours to draw (high confidence). No source for
  the digits' strokes; nine house decisions (§1.4.7).
- **Vowel marks on sentences**: Tatoeba has 91 fully vocalized sentences of
  68,570. Options: vocalize word by word from Wiktionary where a word has one
  reading; Common Voice's 15,955 vocalized CC0 sentences; a build-time
  vocalizer with a permissive licence (CATT: Apache-2.0 code, weights and
  training data to check; Mishkal and CAMeL's MSA database are GPL), then a
  human check.
- es/pt/ja meanings: ~200 of the commonest 1,000 words each (French: 596).
- No open graded list (Kelly is NC).
- No ML Kit photo-text model.
- No reusable recorded audio.
- No Japanese or Portuguese phrasebook; the Spanish one looks
  machine-translated.

## Risks (with confidence)

1. **Our strokes vs the school norm:** the main rules are well attested
   (high), but Part 1 of the Saudi primer (18 letters) was not obtained, so
   ط ف ق و ل م ن د ي ء لا rest on [MAS] and inference; have an Arab Grade 1
   teacher review the animations. Medium.
2. **Tracing a ministry primer** would copy a protected work (50 years from
   publication): draw from the rules. High.
3. **Kaikki's per-language file is deprecated**: fetch now; the raw dump is
   the fallback. High.
4. **Clitic resolution is ambiguous** for some forms (ولد "boy" vs و+لد);
   rank candidates by frequency and prefer the whole word. Medium.
5. **Unvocalized text read aloud**: the voices guess the vowels (measured on
   Majed); always send vocalized text. High.
6. **Anti-DRM clauses vs FairPlay** (CC BY-SA, CC BY): as for Kana; publish
   the data. Medium.
7. **Common Voice sentences**: CC0 as contributed, but some look copied
   (Tatoeba-like, Qur'anic); spot-check. Low–medium.
8. **Fonts:** rename Scheherazade New or IBM Plex Sans Arabic if subset
   (reserved names). High.
9. **ML Kit `ar`**: documented (high); its handling of lone positional forms,
   dots and harakat is untested.
