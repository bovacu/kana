# Hangul stroke data

Stroke-order data for the Hangul app: the 51 compatibility jamo and all 11,172
precomposed syllables, as centreline strokes in writing order. It is written in
KanjiVG's XML format, so Kana's bake (`fude/lang/ja/bake.c`, `fude_bake_kanjivg`)
reads it unchanged.

No usable Hangul stroke data exists under a licence we can use (checked
2026-10-03), so this is our own: about forty jamo drawn by hand, in code, and
every syllable composed from them by rule.

## Files

| File | What it is |
| --- | --- |
| `jamo.py` | The jamo, drawn by hand as parametric centreline strokes, with the stroke order and its source noted per jamo. Stroke counts per jamo (`STROKES`), and which jamo are written as two others (doubles, final clusters, compound vowels). |
| `compose.py` | Composes the syllables from the jamo (the block layouts, the shape each consonant takes in each position) and writes the XML. The entry point. |
| `pen.py` | Stroke geometry: strokes as cubic Béziers, path data in KanjiVG's style, and the bake's path-reading rules in Python. |
| `validate.py` | Reads the XML back with the bake's rules and checks it (below). |
| `sheets.py` | Contact sheets (PNG) for looking at the result. Needs Pillow. |

## Regenerating

From the repository root:

```sh
python3 apps/hangul/tools/strokes/compose.py          # writes data/raw/hangulvg.xml (git-ignored)
python3 apps/hangul/tools/strokes/validate.py         # checks it; add --clearance for the spacing report
python3 apps/hangul/tools/strokes/sheets.py OUT_DIR --font NotoSansKR-Light.otf   # optional: PNG sheets
```

`compose.py` takes an optional output path. The output is deterministic (no
dates or random numbers), so two runs give the same file.

`validate.py` checks: the character set (exactly U+3131..U+3163 and
U+AC00..U+D7A3, in order); the ids (`kvg:kanji_0ac00`, `kvg:0ac00`,
`kvg:0ac00-gN`, `kvg:0ac00-sN` numbered 1, 2, 3 ... in document order); that
paths use only M/m (once, first), C/c and S/s and parse with the bake's rules;
that every coordinate, control points included, is within 0..109; that
`kvg:type` is a CJK Strokes character as `fude_bake_parse_type` reads it; that
each syllable has as many strokes as its jamo have (from the counts in
`jamo.STROKES`, independent of the drawings), each jamo group as many as its
jamo; that the parts the bake will collect include the syllable's jamo; and
that each stroke runs the way its type says. `--clearance` reports jamo that
come closer than 4.7 units (centreline to centreline) to a neighbour.

`sheets.py` draws the jamo and a sample of syllables with each stroke in its
own colour, a dot where it starts and its number; with `--font`, each sample
syllable beside the font's glyph (to compare proportions only; nothing is taken
from the font); with `--all`, every syllable. Write the sheets outside the
repository. Noto Sans KR is at github.com/notofonts/noto-cjk
(`Sans/SubsetOTF/KR/NotoSansKR-Light.otf`).

## Format

```xml
<kanji id="kvg:kanji_0ac01">
<g id="kvg:0ac01" kvg:element="각">
	<g id="kvg:0ac01-g1" kvg:element="ㄱ" kvg:position="initial">
		<path id="kvg:0ac01-s1" kvg:type="㇇" d="M..."/>
	</g>
	<g id="kvg:0ac01-g2" kvg:element="ㅏ" kvg:position="medial">
		<path id="kvg:0ac01-s2" kvg:type="㇑" d="M..."/>
		<path id="kvg:0ac01-s3" kvg:type="㇐" d="M..."/>
	</g>
	<g id="kvg:0ac01-g3" kvg:element="ㄱ" kvg:position="final">
		<path id="kvg:0ac01-s4" kvg:type="㇕" d="M..."/>
	</g>
</g>
</kanji>
```

- Coordinates are KanjiVG's 109 x 109 box, Y down. Paths are centrelines: an
  absolute `M`, then relative `c` cubics. Straight pieces are cubics with their
  control points on the line (the bake has no L, H, V, Q, A or Z).
- `kvg:element` holds Hangul **compatibility** jamo (U+3131..U+318E). The bake
  collects every nested group's element as the character's parts.
- `kvg:position` on a syllable's jamo is its role: `initial`, `medial` or
  `final`. A double consonant (ㄲ), final cluster (ㄳ) or compound vowel (ㅘ)
  holds its two jamo as nested groups, `left` and `right`, as KanjiVG nests a
  kanji's components. So the parts of 꽉 are ㄲ, ㄱ, ㅘ, ㅗ, ㅏ (the bake drops
  repeats; the final ㄱ is the same part as ㄲ's ㄱ).
- `kvg:type` is set where a CJK stroke fits: ㇐ horizontal, ㇑ vertical (also the
  short top strokes of ㅊ ㅎ and the short strokes of ㅗ ㅛ ㅜ ㅠ), ㇒ left-falling,
  ㇏ right-falling, ㇕ across-then-down (upright ㄱ, ㄹ's first stroke, ㅁ's second),
  ㇗ down-then-across (ㄴ, and the last stroke of ㄷ ㄹ ㅌ), ㇇ across-then-sweep-left
  (ㄱ before a vertical vowel, ㅈ's first stroke). ㅇ (and ㅎ's circle) have none:
  no CJK stroke type is a circle (KanjiVG never uses ㇣).
- Every stroke goes the way it is written: horizontals left to right, verticals
  down, ㇒ from top right to bottom left, ㇏ from top left to bottom right, ㅇ from
  the top, counter-clockwise, all the way round.

## Stroke order sources

- **[C]** Wikimedia Commons, [Category:Hangeul stroke
  order](https://commons.wikimedia.org/wiki/Category:Hangeul_stroke_order): the
  public-domain diagrams only (46 of the category's 50 files; the four
  CC-licensed ones were not used). The per-letter PNGs ("ㄱ (giyeok) stroke
  order.png" ... "ㅣ (i) stroke order.png", three for ㅗ), the GIF set ("Giuk
  stroke order.gif" ... "Hiut stroke order.gif") and "Korean vowel
  strokes.gif". Used for order and direction only; no shape was traced. They
  draw printed (명조) letterforms.
- **[K]** The conventional order taught in Korean primary schools and
  handwriting workbooks: broadly top to bottom, then left to right, an
  enclosing ㄴ last. NIKL (국립국어원) says the stroke order of the jamo is not
  regulated; this school order is the one learners meet. It agrees with [C]
  letter by letter; where [C] shows the printed form of a letter rather than
  the handwritten one (ㅈ ㅊ ㅎ), see the house choices.
- Noto Sans KR (SIL OFL) was rendered beside the drawings to compare
  proportions. Nothing was traced or extracted from it.

Not used (licence): hayanhuman-code/hangulssugi, Hilokal/korean-handwriting,
hangyul-ganada, the 세종한국어 textbooks (KOGL Type 4).

## Stroke counts

Consonants: ㄱ1 ㄴ1 ㄷ2 ㄹ3 ㅁ3 ㅂ4 ㅅ2 ㅇ1 ㅈ2 ㅊ3 ㅋ2 ㅌ3 ㅍ4 ㅎ3; ㄲ ㄸ ㅃ ㅆ ㅉ are two of
the base (2 4 8 4 4). Vowels: ㅏ2 ㅐ3 ㅑ3 ㅒ4 ㅓ2 ㅔ3 ㅕ3 ㅖ4 ㅗ2 ㅛ3 ㅜ2 ㅠ3 ㅡ1 ㅣ1;
compounds are their parts in order (ㅘ4 ㅙ5 ㅚ3 ㅝ4 ㅞ5 ㅟ3 ㅢ2). Final clusters are
their parts in order (ㄳ3 ㄵ3 ㄶ4 ㄺ4 ㄻ6 ㄼ7 ㄽ5 ㄾ6 ㄿ7 ㅀ6 ㅄ6).

## House choices

Where practice varies, this is what the data does, and why.

1. **ㅈ: two strokes**, the handwritten form taught in school: ㇇ (a short bar,
   then turning into a long sweep down to the left, like ス) and ㇏ from the
   middle of the sweep. The printed form is three strokes (ㅡ, ㇒ from the
   middle of the bar, ㇏), as in [C] "ㅈ (jieut) stroke order.png" and "Jigut
   stroke order.gif"; it is not used.
2. **ㅊ: three strokes**: a short top stroke, then ㅈ's two. The top stroke is a
   short **vertical, written downward**, standing on the middle of the bar, as
   Noto Sans KR and gothic (고딕) charts draw it. Printed (명조) forms draw it as
   a short horizontal ([C] "ㅊ (chieut) stroke order.png", "Chiut stroke
   order.gif", which also use the 3-stroke ㅈ and so count 4).
3. **ㅎ: three strokes**: the same short downward top stroke, the bar, then the
   circle from the top, counter-clockwise. [C] "ㅎ (hieut) stroke order.png" and
   "Hiut stroke order.gif" draw the top stroke as a short horizontal (the
   printed form); the order is the same.
4. **ㄱ has three shapes**:
   - before a vertical vowel (가 거 개 기 ...; also with a final, 각): across, then
     a long sweep to the lower left (㇇);
   - above a horizontal vowel or top-left of a compound vowel (고 구 그 과 귀 긔):
     across, then down, the end easing slightly left (㇕);
   - as a final and standalone: a square corner (㇕).
   In ㄲ before a vertical vowel, the first ㄱ's sweep is short and gentle, clear
   of the second. ㅋ follows ㄱ in every position.
5. **ㅗ and ㅛ: the short stroke(s) first**, down onto the bar, then the bar
   ([C] all three "ㅗ (o) stroke order" PNGs, "Korean vowel strokes.gif"; [K]).
   ㅜ and ㅠ: the bar first, then the short stroke(s) down from it.
6. **ㅅ**: ㇒ from the top, then ㇏ starting partway down the ㇒ (the handwritten
   form; gothic fonts split both legs from one stem). Beside a vertical vowel
   the ㇏ is shorter and starts lower; above a vowel or as a final the legs are
   near-symmetric. ㅈ and ㅊ follow the same rule.
7. **ㅇ: one closed stroke** from the top, counter-clockwise; no stroke type.
8. **ㅍ**: the two inner strokes are vertical (printed forms slant them inward).
9. **ㅐ ㅒ ㅔ ㅖ**: the inner vertical is shorter than the outer one at both ends;
   the short stroke of ㅐ ㅒ runs between the two verticals and touches both.
10. **Compound vowels**: the bar of ㅗ/ㅜ/ㅡ stops short of the long vertical
    (some printed diagrams run it into the vertical). In ㅝ ㅞ the short stroke of
    ㅓ/ㅔ sits under the bar, right of ㅜ's stroke.
11. **Positional shapes**: beside a vertical vowel, the foot of ㄴ ㄷ ㄹ ㅌ lifts a
    little at its end, as in handwriting; elsewhere it is flat. Finals are wide
    and low. Doubles and clusters put two narrower letters side by side; the
    left one's last stroke stays short of the right one. Round and boxed
    letters (ㅇ ㅁ ㅂ ...) are drawn a little smaller than open ones, so they look
    the same size.
12. **Horizontals rise slightly to the right** (3 %), as Korean handwriting is
    taught; done as a vertical shear of each finished character, so joints stay
    joined and verticals stay vertical.

## Licence

The stroke data is our own work: drawn for this project, not derived from any
other stroke data or font outlines. Its licence is for the owner to decide;
candidates are **CC BY-SA 4.0** (like KanjiVG's CC BY-SA 3.0, others can build on
it and must share alike) or **CC0** (no conditions). The generated XML says
"licence to be decided" until then.

## Known limits

- One style throughout (a clean, upright 정자체-like hand); no calligraphic
  thick-thin or brush endings. Stroke width is the app's, uniform.
- Shapes are parametric, so they are consistent rather than individually
  tuned. The densest syllables (a double initial with ㅙ or ㅞ and a cluster
  final, e.g. 쀏 뾃 뛟, up to 20 strokes) give the initial only about 20 units
  of height: clear when large, crowded when small. Rare syllables were checked
  by machine (validate.py, --clearance) and by sampling, not one by one.
- The short top stroke of ㅊ ㅎ is vertical. A learner who writes the printed
  short horizontal will be marked as off in shape and direction.
- ㅇ is a closed loop that starts and ends at the top; a learner who starts
  elsewhere, or goes clockwise, may match less well.
