# Arabic stroke data

Stroke-order data for the Arabic app: its 137 characters
(`fude/lang/ar/lang.c`'s table): the 36 letters as they stand alone, at their
own code points (U+0621..U+064A); their 81 joined forms (final, initial,
medial) at Unicode's Arabic Presentation Forms-B code points (U+FE82..U+FEF4);
lam-alif, isolated and final (U+FEFB, U+FEFC); the 8 harakat (U+064B..U+0652);
the 10 Arabic-Indic digits (U+0660..U+0669). Each is a set of centreline
strokes in writing order, written in KanjiVG's XML format, so Kana's bake
(`fude/study/chars/bake.c`, `fude_bake_kanjivg`) reads it unchanged.

No usable open Arabic stroke data exists (`docs/research/arabic_sources.md`,
section 1.1), so this is our own, drawn by hand in code like the Thai letters
(`apps/thai/tools/strokes/`) and the Hangul jamo.

The hand is the Naskh that Arab primary schools teach, as the Saudi Ministry
of Education's primers model it: print-like, written with a pencil. A letter's
body is one continuous stroke, written right to left from where the primer
starts it; then come the dots (each on its own, right to left), the hamza,
the madda, ك's small mark, the upright of ط ظ and the alif of لا.

## Files

| File | What it is |
| --- | --- |
| `letters.py` | The letters, drawn as parametric centreline strokes: one body function per family (ب ت ث ن ي share ب's body and the tooth, ج ح خ one body, س ش, ص ض, ط ظ, ع غ, ف ق ...), dots and marks added per letter, every form derived from it; the source of each letter's order noted with it. The stroke counts as taught (`STROKES`, kept apart from the drawings), the forms table, the frame. |
| `compose.py` | Writes the XML. The entry point. |
| `pen.py` | Stroke geometry: strokes as cubic Béziers that keep the pen's heading (Hermite pieces, splines through points, arcs, ticks for dots), path data in KanjiVG's style, and the bake's path-reading rules in Python. |
| `validate.py` | Reads the XML back with the bake's rules and checks it (below). Standard library only, like `compose.py`. |
| `sheets.py` | Contact sheets (PNG) for looking at the result. Needs Pillow, and fontTools for the font's glyphs beside them. |

## Regenerating

From the repository root:

```sh
python3 apps/arabic/tools/strokes/compose.py          # writes data/raw/ar/arabicvg.xml (git-ignored)
python3 apps/arabic/tools/strokes/validate.py         # checks it; add --corners for the corner report
python3 apps/arabic/tools/strokes/sheets.py OUT_DIR --font NotoNaskhArabic[wght].ttf
```

`compose.py` takes an optional output path. The output is deterministic (no
dates or random numbers): two runs give the same file.

`validate.py` checks: the character set (exactly the 137, in code point order,
listed in `validate.py` itself, and the same set as `lang.c`'s table when that
file is there); the ids (`kvg:kanji_0fe8b`, one top group `kvg:0fe8b` whose
`kvg:element` is the character, paths `kvg:0fe8b-sN` numbered 1, 2, 3 ... in
document order); that each path is one absolute `M` followed by relative `c`
segments only, parses with the bake's rules and has at most 255 segments; that
every coordinate, control points included, is within 0..109; that no path has
a `kvg:type`; that each character has as many strokes as `letters.STROKES`
says it is written with; that no stroke or segment is degenerate; **the
joins**: a final or medial form's first stroke (its body) starts within 2
units of (98, 68) and runs left along the baseline from there, an initial or
medial form's body ends within 2 units of (11, 68) arriving along the
baseline, a side that does not join has no end there, and every letter form's
body meets the baseline; **the harakat**: a mark above lies wholly above the
app's dotted circle (centre (54.5, 56), radius 19, `fude/study/chars/glyph.c`)
and within its width, a mark below wholly below it. `--corners` lists every
sharp turn inside a stroke (a tooth's tip, the top of a hairpin, ح's head), so
that one that should not be there shows.

`sheets.py` draws every character, one letter a row in its four forms, each
stroke in its own colour, a ring where it starts with its number, white
chevrons along it and an arrowhead where it ends; faint lines at the ascender,
a tooth's tip, the baseline and the descender; the join points (green where
the form joins); the harakat's dotted circle; with `--font`, Noto Naskh
Arabic's glyph in grey behind it and in a side-by-side comparison
(proportions only; nothing is taken from the font). `--only U+FE8B,U+0628`
limits it to some characters, `--prefix` names the files. Write the sheets
outside the repository. The font (SIL OFL) is not in the repository: download
it from Google Fonts (`ofl/notonaskharabic`), with its `OFL.txt`.

## Format and frame

```xml
<kanji id="kvg:kanji_0fe91">
<g id="kvg:0fe91" kvg:element="ﺑ">
	<path id="kvg:0fe91-s1" d="M50.6,57c..."/>
	<path id="kvg:0fe91-s2" d="M48.87,75.87c..."/>
</g>
</kanji>
```

- KanjiVG's 109 x 109 box, Y down. Centrelines: an absolute `M`, then relative
  `c` cubics (straight pieces are cubics with their control points on the
  line). No `kvg:type`, no nested groups.
- **One frame for every character**, so forms can be set side by side into
  words: the **baseline at y 68** in every form of every letter; ascenders
  (ا ل ك ط ظ لا) up to 14; a tooth's tip at 57 (س ش's teeth 54.5); descenders
  (ر ز و ن ل ق س ش ص ض ح ج خ ع غ م ي) down to about 86..94, the dots under ي
  to 97. Dots above sit above the body (about y 38..47), dots below under it
  (about 77; ي's under its bowl, 95).
- **Joins on the baseline**: a form that joins the letter before (final,
  medial) starts its body stroke at the right connection **(98, 68)** and runs
  left along the line into the letter; a form that joins the letter after
  (initial, medial) ends its body stroke with the joining tail running left
  along the line to the left connection **(11, 68)**. Isolated letters are
  centred on x 54.5; joined forms keep their body near the centre (a final's
  a little left of it, an initial's a little right) and run their connecting
  strokes out to x 98 and 11, so a medial form shows long connectors, as a
  chart with tatweel does.
- Isolated forms connect to nothing; the non-connectors (ا د ذ ر ز و ة ى and
  their hamza and madda forms) have an isolated and a final form only; ء has
  only its isolated form.
- **Harakat** are drawn where they sit on an imagined letter, the dotted
  circle the app draws about (54.5, 56), radius 19: fatha, damma, sukun,
  shadda, fathatan, dammatan above it (y 19..36), kasra and kasratan below it
  (y 79..92), centred on x 54.5.
- **Digits** stand on the baseline, y 24..68 (٠ is a dot at mid-height, ٥ a
  loop above the line), each centred and drawn on its own (a number is
  written with its digits left to right; each digit's strokes as here).

## How the forms are built

The forms of a letter share its parts (`letters.py`), and follow two rules
that the primers' arrows bear out:

- **a final form is the isolated letter with the connector in front**: the
  pen comes along the line from (98, 68) and goes on as the isolated letter
  does; where the isolated letter starts at the top (ا ل ك د ر, the tooth of
  ب ن), the connector **goes up to that top and back down** [M] [G] [K] [W]
  (final ـا is the exception: the pen goes up the alif and ends at its top
  [M] [K]);
- **a medial form is the initial with the connector in front**, in the same
  way (up into the tooth of ـبـ ـسـ, up ـلـ's upright and back down, up the
  diagonal of ـكـ and back down);
- loops that sit on the line (ص ض ط ظ ف ق م و) are closed by the line: the
  connector of a medial or final form runs under the loop to its left side,
  the loop is drawn from there as in the isolated (or initial) letter, and
  the pen comes back along the line [G] (ض) [K], or goes on down into the
  tail or bowl (ـو ـق). Where the pen goes back over a line it has
  drawn (the top of an upright, a loop's foot) the second pass runs about a
  unit beside the first.

## Stroke order sources

- **[M]** Saudi Ministry of Education, *لغتي الجميلة*, Grade 4 (both terms),
  the handwriting lessons ("الرسم الكتابي: رسم الحرف ... بخط النسخ"), as
  reproduced page by page on sahl.io (the Ministry's iEN digital lessons):
  "ألاحظ طريقة رسم الحرف تبعًا لاتجاه الأسهم", each letter in its forms with
  arrows (some movements numbered ١ ٢ ٣ ٤), and the steps it is made of ("رسم
  الحرف (ل) يتكون من خطوتين: ألف، كأس تنزل تحت السطر"; "(لا) يتكون من ثلاث
  خطوات: لام ناقصة الكأس، وصلة، ألف مائلة"). Pages seen for ا ء ب ت ث د ذ ر ز
  و ط ظ ف ك ل لا م ه ة ج ح خ ـهـ ع غ س ش ق ي. Protected (a Ministry collective
  work): read for the order only, never traced.
- **[G]** Saudi Ministry of Education, *لغتي*, Grade 1, Part 2 (1447/2025),
  the model letters with red arrows and a start circle, four forms each, as
  collected for `docs/research/arabic_sources.md` ([SA-G1]): ك (all four
  forms), ع, ض, خ, ه (all four forms), ث. Same status.
- **[K]** Mamoun Sakkal, "Arabic Alphabet Chart (In Naskh Style)"
  (sakkal.com, 2007/2016): every letter in its four forms with arrows and a dot
  where each pen stroke starts; "follow the direction of the arrows writing in
  a clockwise direction in general". A calligrapher's (reed pen) chart: used
  for directions and where the primers are silent, not for its pen lifts.
- **[W]** J. Wightwick and M. Gaafar, *Mastering Arabic Script: a guide to
  handwriting* (Palgrave, 2005), as quoted in the research notes: body first,
  then the dots and marks, right to left; م "a tight circle in a clockwise
  direction"; ط ظ "loop ... first ... then add the vertical stroke
  downwards"; medial and final ل up and back down the same path; the hamza
  after the alif.
- **[S]** Saudi curriculum exercises on the Naskh dots: "ترسم النقاط في خط
  النسخ منفصلة" (true); "في خط النسخ عند كتابة نقاط حرف الثاء ... نفصل كل النقاط
  الثلاث". In Naskh each dot is written on its own (in Ruqʿah they join).
- **[D]** belarabyapps.com, "تعليم كتابة الأرقام العربية" (number-tracing
  worksheets): ١ top to bottom; ٢ and ٣ start at the top right, go left over
  the top, then down the stem.
- **[H]** House choice (below): no source shows it.
- Noto Naskh Arabic (SIL OFL) was rendered beside the drawings to compare
  proportions; nothing was traced or extracted from it.

The sources agree wherever more than one shows a letter, except on initial ك
(below). `docs/research/arabic_sources.md` (section 1.4) reaches the same
per-letter table independently.

## Stroke counts

Dots count one stroke each. Letters: ب 2, ت 3, ث 4, ن 2, ي 3, ى 1, ئ 2, ة 3,
ج 2, ح 1, خ 2, د 1, ذ 2, ر 1, ز 2, س 1, ش 4, ص 1, ض 2, ط 2, ظ 3, ع 1, غ 2, ف 2,
ق 3, ك 2, ل 1, م 1, ه 1, و 1, ؤ 2, ا 1, أ إ آ 2, ء 1, لا 2, in every form, but
**initial ك 1** (its bar is part of the body [G]). Harakat: fathatan, dammatan,
kasratan 2; the rest 1. Digits: 1 each. In all, 253 strokes.

## House choices

1. **Dots are separate strokes**, written right to left [S] [W] (the school
   model; everyday handwriting joins two dots into a dash and three into a
   cap, which a matcher may want to accept). A dot is a short tick, 3.2
   long, from upper left to lower right (the reed pen's rhombus as the
   chart's "Dot" arrow draws it [K]); ٠ the same, 5.5 long.
2. **Three dots** (ث ش): the two side by side, right then left, then the one
   above them (ت's dots, then one more). No source gives the order; reading
   order (right, top, left) is the other candidate.
3. **Initial ك is one stroke, the bar first**: from the bar's upper right end
   down to the elbow, down the diagonal to the right, then left along the line
   [G] [M] [K]. [W] adds the bar after the word instead; the primer is
   followed. **Medial ـكـ**: from the connector up the diagonal and back down,
   then the line; the bar is a second stroke, from the elbow **up to the
   right** [G]. **ك's small mark** (isolated, final) is written last [M] [G];
   it is drawn as a small hamza, from its top right [H].
4. **ط ظ**: the loop first, then the upright top down [M] (numbered ١ ٢) [W].
5. **ص ض ط ظ**: the loop starts on its left side, half way up, and goes
   clockwise [G] [W]; ص is one stroke (loop, back along the line, the small
   tooth, the bowl), as the school writes it; the chart's pen lift before the
   tooth [K] is not followed.
6. **ف**: the head starts at its lower left (the primer's start dot) and goes
   clockwise; the line under it closes it [M]. **ق و** (isolated): their heads
   start at the foot on the right and run left along the underside first, so
   the head closes before the bowl or tail [K]; the primers' arrows (up the
   left side, down the right) agree. ٩'s head the same.
7. **م**: isolated, from the head's left over the top, down and back along the
   line, clockwise, then the tail [M] [W]; **initial مـ counter-clockwise**:
   from the head's lower left right along the line, up, over the top to the
   left, down (the primer's arrows) [M]. Final follows the isolated, medial
   the initial (the rules above) [H].
8. **ه's four forms** as the primers draw them [M] [G]: isolated round, from
   the top clockwise; initial هـ from the top down the right side, along the
   line, up the left side to the top and down the middle (the two eyes); medial
   ـهـ the figure eight, the lower loop first (clockwise), then up into the
   upper loop and over its tip clockwise [G]; final ـه up the stem, down over
   the left, the bottom left to right [M] [G].
9. **ع غ**: isolated and initial, the crescent from its top right tip,
   counter-clockwise [M] [G]; medial and final, the closed head drawn from
   the connector up its left side, over the top to the right, down
   (clockwise) [G]; the medial head is the closed triangle of the school
   model ("مطموس").
10. **ج ح خ**: the head (حاجب) from its left tip, left to right, in every form
    [M] [G]. Medial and final: the connector goes up into the head's right end
    and along under it to the left tip, then the head is drawn as usual; the
    primers show only the connector and the head's direction [H].
11. **ي ى**: isolated and final from the top right, an arc down to the left,
    right, then the bowl [M]; the final's connector first rises into the
    arc's top (the primer's arrow runs over the top from the right) [M].
12. **لا**: two strokes, the lam (no cup) with its foot running on to the left
    as the joint, then the slanted alif from the top left down to meet it [M]
    ("لام ناقصة الكأس، وصلة، ألف مائلة"). Final ـلا: from the connector up the
    lam and back down first.
13. **Hamza** (ء and the small one): from the top right, the small crescent
    counter-clockwise, right, then the slanted dash down to the lower left
    [M] ("نقطة البداية، نزول بميل، تداخل، نزول"). Over أ آ (and their finals)
    the alif is shortened to y 22 to make room, as print does.
14. **Madda**: a short wave, right to left [H].
15. **Harakat** [H], from common practice as the research notes give it:
    fatha and kasra a short stroke from upper right to lower left; damma a
    small و (head clockwise from its foot, then the tail); sukun a small circle
    from the top, clockwise (as ه); shadda a small "w" from its right tip;
    fathatan and kasratan the doubled stroke, the upper first; **dammatan two
    dammas side by side, right then left** (print often joins them into one
    damma with a curl).
16. **Digits**: ١ ٢ ٣ as [D]; the rest [H], all one stroke: ٠ a dot; ٤ from
    the top right, two bulges to the left, the foot to the right; ٥ a closed
    loop from the top, clockwise; ٦ the top from left to right, then down; ٧
    from the top left down to the point and up to the top right; ٨ from the
    bottom left up to the apex and down to the right; ٩ the head (as و's),
    then the stem down.

## What to check with a teacher

- The three-dot order (2), and whether to teach separate dots or the dash
  and cap.
- Initial ك as one stroke with the bar first, and medial ـكـ's bar drawn
  upward afterwards (3); ك's small mark (its shape and direction).
- Initial مـ counter-clockwise against the isolated م clockwise (7), and the
  medial and final م that follow them.
- Where ق و start their heads (6).
- The medial and final ج ح خ path into the head (10), and the final ي's
  connector rising into the arc (11).
- Medial ـهـ (8) and the medial and final ع heads (9).
- The madda, the harakat (especially dammatan and sukun), and every digit but
  ١ ٢ ٣ (14-16).
- The joined forms' shapes beside the isolated ones: the primers model them
  in words, at their own proportions; here every form has one size and its
  connectors run to the box's join points.

## Licence

The stroke data is our own work: drawn for this project, not derived from any
other stroke data, primer drawing or font outline. Its licence is for the owner
to decide (candidates: CC BY-SA 4.0, like KanjiVG's CC BY-SA 3.0, or CC0). The
generated XML says "licence to be decided" until then.

## Known limits

- One hand: the school Naskh, upright, uniform width, every form at one size.
  Ruqʿah (taught later, and the everyday hand) is not drawn; a learner who
  writes it, or joins dots into dashes, will be marked as off in shape.
- Shapes are parametric and drawn by eye against the references and the font,
  consistent rather than calligraphic; joined forms have long connectors so
  that their join points line up.
- Where the pen goes back over its own line (ـا ـل ـك ـلا's uprights, a
  medial loop's foot, ـد ـر's tips), the drawing shows one line.
- The joined forms are shown alone; lam-alif is the only ligature drawn (the
  calligraphic ligatures of ج ح خ م ي with the letter before are not).
