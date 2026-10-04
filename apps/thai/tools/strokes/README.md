# Thai stroke data

Stroke-order data for the Thai app: its 83 letters (the 44 consonants, the 19
vowels, the 10 tone marks and signs, the 10 digits; `fude/lang/th/lang.c`'s
table), as centreline strokes in writing order. It is written in KanjiVG's XML
format, so Kana's bake (`fude/study/chars/bake.c`, `fude_bake_kanjivg`) reads
it unchanged.

No usable Thai stroke data exists, so this is our own, drawn by hand in code
like the Hangul app's jamo (`apps/hangul/tools/strokes/`).

The hand is the one Thai schools teach, the Ministry of Education's "round
head" letters (หัวกลม ตัวมน): a letter starts at its head, the little circle,
goes once round it and runs on to its end without lifting the pen. Which way
the head goes (clockwise or not) decides whether it sits inside the letter or
outside it, and the learner is scored on it.

## Files

| File | What it is |
| --- | --- |
| `letters.py` | The letters, drawn as parametric centreline strokes, one function each, with the stroke order and its source noted per letter. Stroke counts per letter (`STROKES`), and the groups. |
| `compose.py` | Writes the XML. The entry point. |
| `pen.py` | Stroke geometry: strokes as cubic Béziers that keep the pen's heading (Hermite pieces, arcs, the head loops), path data in KanjiVG's style, and the bake's path-reading rules in Python. |
| `validate.py` | Reads the XML back with the bake's rules and checks it (below). |
| `sheets.py` | Contact sheets (PNG) for looking at the result. Needs Pillow; fontTools too for the font glyphs beside them. |

## Regenerating

From the repository root:

```sh
python3 apps/thai/tools/strokes/compose.py          # writes data/raw/th/thaivg.xml (git-ignored)
python3 apps/thai/tools/strokes/validate.py         # checks it; add --corners for the corner report
python3 apps/thai/tools/strokes/sheets.py OUT_DIR --font apps/thai/assets/fonts/NotoSansThaiLooped-Regular.ttf
```

`compose.py` takes an optional output path. The output is deterministic (no
dates or random numbers), so two runs give the same file. The bake then reads
it from its default place (`--bake`, `--strokes=data/raw/th/thaivg.xml`).

`validate.py` checks: the character set (exactly the 83 letters, in code point
order, and the same set as `lang.c`'s table when that file is there); the ids
(`kvg:kanji_00e01`, one top group `kvg:00e01` whose `kvg:element` is the
letter, paths `kvg:00e01-sN` numbered 1, 2, 3 ... in document order); that
paths use only M/m (once, first), C/c and S/s and parse with the bake's rules,
at most 255 segments a stroke; that every coordinate, control points included,
is within 0..109; that no path has a `kvg:type`; that each letter has as many
strokes as `letters.STROKES` says it is written with (the counts as taught,
kept apart from the drawings); that no stroke or segment is degenerate.
`--corners` lists every sharp turn inside a stroke (ก's notch, the zigzags of
พ ผ, the turns at a foot ...), so that one that should not be there shows.

`sheets.py` draws every letter with each stroke in its own colour, a ring
where it starts with its number, an arrowhead where it ends, and faint lines at
the frame's heights; with `--font`, the font's glyph in grey behind it and a
side-by-side comparison (proportions only; nothing is taken from the font).
`--only` limits it to some letters, `--prefix` names the files. Write the
sheets outside the repository.

## Format

```xml
<kanji id="kvg:kanji_00e02">
<g id="kvg:00e02" kvg:element="ข">
	<path id="kvg:00e02-s1" d="M37,35c..."/>
</g>
</kanji>
```

- Coordinates are KanjiVG's 109 x 109 box, Y down. Paths are centrelines: an
  absolute `M`, then relative `c` cubics. Straight pieces are cubics with their
  control points on the line (the bake has no L, H, V, Q, A or Z).
- One frame for every letter, so they line up: bodies from y 30 to the
  baseline at 84, ascenders (ป ฝ ฟ) up to 12, descenders (ญ ฎ ฏ ฐ ฤ ฦ ๅ ๆ) down
  to 102. The leading vowels เ แ โ ใ ไ stand on the baseline, their tops as
  written (โ ใ ไ up to about 4). Marks are drawn alone, where they sit on a
  letter whose body fills the band (x about 33..76): those above it between
  y 3 and 25, centred over it; those below (ุ ู ฺ) between 89 and 105, on its
  back (right) line. า ะ ำ stand in the band; ำ's circle sits up and to the left,
  over where the letter before it would be.
- No `kvg:type`: Thai strokes are not CJK stroke types (the bake accepts its
  absence). No nested groups, so no parts: Thai letters are learnt whole.
- A head is a whole circle, tangent to the line it runs into, starting and
  ending where it joins it; the line leaves it smoothly. Loops (ม's foot, น's
  corner, ห's top, ฬ's tail, ฎ's curl) are drawn the same way.

## Stroke order sources

- **[M]** The Ministry of Education's model letters (แบบตัวอักษรไทย
  กระทรวงศึกษาธิการ, "หัวกลม ตัวมน"), as reproduced in a school's guide to
  teaching handwriting, *คู่มือการสอนคัดลายมือ* (Bankhai school,
  bky.ac.th, 11 pages): the model consonants; the vowels, tone marks and digits
  with each movement numbered (๑ ๒ ๓ ...); where each consonant's head sits
  (outside or inside, top or bottom: ง ช น บ ป พ ฟ ม ห ฬ outside at the top, ภ
  outside at the bottom, ผ ฝ inside at the top, ถ inside at the bottom, ค ฅ ศ อ
  ฮ inside, ฉ ด ต inside facing out, ข ฃ ช ซ double); the heads of ผ ฝ ค ฅ
  counter-clockwise, of น ม บ ป clockwise; the direction of each kind of line;
  "start at the head; do not lift the pen until the letter is finished".
- **[P]** ThaiPod101, *Thai Alphabet Made Easy* #1-#25 (thaipod101.com): a Thai
  teacher's instructions for every consonant, vowel, tone mark and digit: where
  it starts, which way its head goes, the path, where the pen is lifted (ฐ's
  foot; ศ ษ ส's last stroke).
- **[A]** ActiveThai, consonant worksheets (activethai.com, middle, high and
  low class): every consonant with a green start dot, arrows, a red end dot and
  stroke numbers.
- **[T]** thai-notes.com's per-letter notes (start, loop direction, pen
  lifts) and the littlefrog writing-practice data (MIT), as reported in
  `docs/research/thai_sources.md` section 1.4. They agree with [M] [P] [A] on
  the letters those show, and settle the signs and ๙ that those leave open.
- Noto Sans Thai (SIL OFL, `apps/thai/assets/fonts/`) was rendered beside the
  drawings to compare proportions; a looped face (Thonburi) was looked at for
  the heads' places. Nothing was traced or extracted from either.

The sources agree on almost every letter; the per-letter notes in
`letters.py` say which ones show it. Where they differ or are silent, see the
house choices.

## Stroke counts

One stroke for every consonant but ญ 2 (the curl under it), ฐ 2 (its foot), ศ 2
and ส 2 (the tail), ษ 2 (the inside). Vowels: ะ 2, ำ 2 (the circle, then า), ี 2,
ึ 2, ื 3, แ 2; the rest 1 (ฤ ฦ ๅ included). Marks: ๋ 2; the rest 1. Digits: ๙ 2
(its tail); the rest 1.

## House choices

1. **บ ป: one stroke**, as the school rule and [P] ("start at the head and then
   draw 3 straight lines") have it. [A] lifts the pen at the bottom left corner
   (two strokes); not followed. ษ is บ plus its inside: 2.
2. **ฎ ฏ: one stroke**, the right side running on below the line into the
   curl, as [A] and [P] draw them ("extend the line on the right below the
   head, and make a loop on the left side"). **ฐ's foot is a second stroke**
   ([A], [P]: "lift your pen ... it starts with a clockwise head"), drawn from
   its little head on the right, leftwards. ญ's curl is a second stroke ([A]).
   **ฤ ฦ: one stroke** each, ถ and ภ with the right side long ([P]).
3. **ช ซ: one stroke**, the right side running on into the tail ([A]); the
   tails of ศ ส and the inside of ษ are separate strokes ([A], [P]: "pick up your
   pen before the last stroke"). Each tail is drawn upward to the right.
4. **ล ส**: after the small inner curve the pen goes down the right side to the
   foot and back up it before the large curve over to the left, as [A]'s arrows
   show (down on the inside, up on the outside) and [P] says ("a small curve to
   the right, and a larger curve to the left"). The way back up runs 0.6 units
   right of the way down at the top, the same line at the foot.
5. **ก's notch** (หยัก) is a small zigzag at the top of the left side: right,
   then up to the beak on the left, then the round top. The same in every
   letter "with ก's shape" (ถ ภ ฎ ฏ ญ ณ ฌ ฤ ฦ).
6. **ห ฬ**: their loops are counter-clockwise ([A]): ห's sits on top of the
   right side and the pen comes down from its foot; ฬ's right side rises into
   its loop and leaves it in the tail.
7. **ม ฆ ฌ ฒ**: the loop at the foot of the stem is clockwise, on the stem's
   left (outside, like the head), and the pen leaves it across to the bottom
   right corner ([A], [P]).
8. **ะ ั**: each curl is a little head, counter-clockwise ([M]'s numbering),
   then out along its foot and up to the right. ะ's upper curl first.
9. **ิ ี ึ ื** ([M], [P]): from the right tip, the lower line to the left, then the
   arch back to the tip; then ี's line, ึ's circle (clockwise, [P]), ื's right line
   and then its left one, the lines downward.
10. **ำ**: the circle first, then า ([P]); the circle clockwise.
11. **้**: the head on the left, clockwise ([P]), over a hump to the right and
    down, the end turning right ([M]'s figure). Fonts disagree on this mark's
    shape (Noto draws a "2", looped faces a loop and a "v"); the stroke runs
    left to right from the head either way.
12. **๋**: the bar across first, then the line down ([M]'s numbering; the
    littlefrog data agrees; thai-notes writes the line first).
13. **๐**: from the top right, counter-clockwise ([T]).
14. **๙: two strokes** ([T], thai-notes: "lift your pen before adding the
    tail"): the head, round the left, over the top and straight down to the
    right; then the tail, from that line, a zigzag up to the upper right. [P]
    and [M] do not say whether the pen is lifted; drawn in one stroke the pen
    would have to go back up the straight line.
15. **ึ: two strokes**, the circle clockwise, as [P] ("then you add a tiny
    clockwise circle"), [M]'s numbering and the littlefrog data have it;
    thai-notes draws the circle on without lifting the pen, anticlockwise.
16. **The signs** ์ ฯ ๆ ฺ ํ, which [M] [P] [A] do not show, follow [T]: ์ its
    little head at the foot, clockwise, up from its left side and over to the
    right; ฯ the head at the top left, inside, counter-clockwise, down into the
    bowl, up to the right and down the right side; ๆ the head at the top left,
    clockwise, over to the right and down the long right side, the foot bending
    left; ฺ ํ small clockwise circles (as ำ's and ึ's). **๎** (yamakkan) is in no
    source: drawn like an ε from the top right (it has no head).
17. **Marks' places**: above, centred over the letter ([M]: "ิ starts from the
    letter's back line", "ื's back line in line with the letter's"); tone marks
    are centred too, as asked for this app (in print they sit over the letter's
    right side); below (ุ ู ฺ), on the letter's back line ([M]: "ู's back line in
    line with the letter's").

## Licence

The stroke data is our own work: drawn for this project, not derived from any
other stroke data or font outlines. Its licence is for the owner to decide
(candidates: CC BY-SA 4.0, like KanjiVG's CC BY-SA 3.0, or CC0). The generated
XML says "licence to be decided" until then.

## Known limits

- One hand throughout: the school's round-head letters, upright, uniform
  width. The modern headless letters of signs and advertising (ไม่มีหัว) are not
  drawn; a learner who writes them will be marked as off in shape.
- Shapes are parametric and drawn by eye against the references, consistent
  rather than calligraphic.
- A head is a whole circle that starts where it joins the letter; a learner
  who starts elsewhere on it will still be near the start.
- ล ส go back up their right side (as taught); the drawing shows it as one
  line.
