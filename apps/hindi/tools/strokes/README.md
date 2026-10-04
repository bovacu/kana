# Devanagari stroke data

Stroke-order data for the Hindi app: its 90 letters (the vowels, the
consonants and their nukta forms, the vowel signs and marks, the dandas and
the digits, as listed in `fude/lang/hi/lang.c`), as centreline strokes in
writing order. It is written in KanjiVG's XML format, so the study's bake
(`fude/study/chars/bake.c`, `fude_bake_kanjivg`, called from
`fude/lang/hi/bake.c`) reads it unchanged.

No usable Devanagari stroke data exists under a licence we can use (checked
2026-10-04; see `docs/research/hindi_sources.md` §1), so this is our own: every
letter drawn by hand, in code, as the Hangul app's jamo were.

## Files

| File | What it is |
| --- | --- |
| `letters.py` | The letters, drawn as parametric centreline strokes (smooth splines through a few points each), with each letter's stroke plan (count and kind of stroke) and the source of its order. Vowel signs are drawn in place around an imaginary consonant. |
| `compose.py` | Writes the XML. The entry point. |
| `pen.py` | Stroke geometry: strokes as cubic Béziers, centripetal Catmull-Rom splines, path data in KanjiVG's style, and the bake's path-reading rules in Python. |
| `validate.py` | Reads the XML back with the bake's rules and checks it (below). |
| `sheets.py` | Contact sheets (PNG) for looking at the result. Needs Pillow. |

## Regenerating

From the repository root:

```sh
python3 apps/hindi/tools/strokes/compose.py          # writes data/raw/hi/devanagarivg.xml (git-ignored)
python3 apps/hindi/tools/strokes/validate.py         # checks it; add --corners for the corner report
python3 apps/hindi/tools/strokes/sheets.py OUT_DIR --font apps/hindi/assets/fonts/NotoSansDevanagari-Regular.ttf
```

`compose.py` takes an optional output path. The output is deterministic (no
dates or random numbers): two runs give the same file.

`validate.py` checks: the character set (exactly the app's 90 letters, copied
from `lang.c` into the validator, in code point order, and the same set drawn
in `letters.py`); the ids (`kvg:kanji_00915`, `kvg:00915`, `kvg:00958-gN`,
`kvg:00915-sN` numbered 1, 2, 3 ... in document order); that paths use only
M/m (once, first), C/c and S/s and parse with the bake's rules; that every
coordinate, control points included, is within 0..109; that there is no
`kvg:type`; that each letter has as many strokes as its plan declares (the
count as taught, written independently of the drawings); that each stroke is
what the plan says it is (a headline runs left to right along y 28, a bar runs
top to bottom and upright, a dot is short, nothing else is under 4 units);
that the headline is the letter's last stroke but for a dot or a mark above it
(ङ's dot, a nukta, the candra of ऍ ऑ); and that a nukta form holds its base
letter and the nukta as groups, which the bake collects as its parts.
`--corners` lists every joint inside a stroke that turns by more than 35
degrees: the strokes are smooth splines, so each of these is a real corner of
the letter (the waist of the "3" in अ उ ॐ ३, the turns of च छ थ य ऋ १).

`sheets.py` draws every character with each stroke in its own colour, a dot
where it starts and its number (`strokes_N.png`), all of them in black at the
app's stroke width (`plain.png`), each beside the font's glyph
(`compare_N.png`; a vowel sign on a grey प) and drawn thin over the font's
glyph (`overlay_N.png`). The font is for comparing proportions only; nothing
is taken from it. Write the sheets outside the repository.

## Format

```xml
<kanji id="kvg:kanji_00915">
<g id="kvg:00915" kvg:element="क">
	<path id="kvg:00915-s1" d="M..."/>
	...
</g>
</kanji>
<kanji id="kvg:kanji_00958">
<g id="kvg:00958" kvg:element="क़">
	<g id="kvg:00958-g1" kvg:element="क">
		<path id="kvg:00958-s1" d="M..."/>
		...
	</g>
	<g id="kvg:00958-g2" kvg:element="़">
		<path id="kvg:00958-s5" d="M..."/>
	</g>
</g>
</kanji>
```

- Coordinates are KanjiVG's 109 x 109 box, Y down. Paths are centrelines: an
  absolute `M`, then relative `c` cubics (the bake has no L, H, V, Q, A or Z).
- The letters stand on the five lines Devanagari is taught on (CHD 2016
  §2.6.1): marks above up to about y 6, the headline at y 28, the baseline at
  y 84, signs below down to about y 104. Letters keep their own width (narrow
  ones narrower) and are centred; they are drawn about 10 % narrower than the
  printed font, as handwriting is.
- The vowel signs are drawn alone but in place, around an imaginary consonant
  in the middle of the box (body x 32..77, its bar at x 75): ा ी ो ौ ॉ to its
  right (bar at x 90), ि's bar to its left (x 21) with the hook over it, े ै ॅ
  ं ँ above it, ु ू ृ ् ़ below it, ः beside it.
- No `kvg:type`: Devanagari strokes are not CJK stroke types (the bake reads a
  missing one as none). A dot is a short tick (4 units), as KanjiVG draws ㇔.
- A nukta form (क़ ख़ ग़ ज़ ड़ ढ़ फ़ य़, ऩ ऱ ऴ) holds its base letter and ़ as two
  groups, so its parts are the base letter and the nukta. Nothing else has
  groups.
- Every stroke runs the way it is written: bars top to bottom, the headline
  left to right, everything else from its taught start (below).

## Stroke order sources

- **[C]** Central Hindi Directorate (केंद्रीय हिंदी निदेशालय, Ministry of
  Education), "देवनागरी लिपि एवं हिंदी वर्तनी का मानकीकरण", revised edition 2024,
  §2.5 "हिंदी वर्णमाला लेखन विधि", book pages 15-18: every vowel and consonant
  built up stage by stage, then the finished letter with direction arrows; and
  the 2016 edition (five lines, the knot of ग न म). The Government of India's
  standard; followed wherever it shows the letter. Copyrighted: used for the
  order and direction only (facts); nothing was traced. Saved under
  `data/raw/research/hi/` (see `docs/research/hindi_sources.md` §1.3-1.4).
- **[S]** Wikimedia Commons, Category:Devanagari stroke order (SVG):
  Saurmandal's numbered diagrams "Devanagari अ stroke order.svg" ... (CC BY-SA
  3.0): the vowels and झ, with stroke counts.
- **[O]** The same category (GIF): Opiaterein's animations "Deva-क-order.gif"
  ... "Deva-ह-order.gif" (CC BY 3.0): 27 consonants.
- **[J]** The same category: JackPotte's animations "Devanagari a अ.gif" ...
  (CC BY-SA 3.0): vowels and consonants, an independent second set.
- **[T]** The rules all of these follow: the body first, from the top; then
  the bar, top to bottom; the headline last, left to right; a mark on the
  headline starts at the headline and goes up and out; a sign under a letter
  starts where it joins it; a dot or candra after the finished letter.
- **[H]** House choice, by [T]'s rules, where no source shows the character:
  the vowel signs alone, ऌ ळ ॐ ऽ ।॥, the digits.
- Noto Sans Devanagari (SIL OFL, `apps/hindi/assets/fonts/`) was rendered
  beside and under the drawings to compare proportions. Nothing was traced or
  extracted from it.

Only the order and direction of strokes were taken from [C] [S] [O] [J]; the
shapes are ours. Each letter's citation is in `letters.py`. By group:

| Group | Sources | Agreement |
| --- | --- | --- |
| Vowels अ ... औ | [C] p.15, [S] (all 11, with counts), [J] | [S]'s counts are ours. Disagreements below. |
| ऌ ऍ ऑ | [H] (ऌ); [C] ए/आ + candra after the letter [T] | — |
| Consonants क ... ह | [C] pp.16-18, [O] (27), [J] (32) | Two or three sources for every consonant but ळ. |
| Nukta forms | [C] ड़ ढ़ (letter, headline, dot); the others by analogy [T] | — |
| Vowel signs and marks | [T] after [C]'s letters; [H] | No source shows the signs alone. |
| Digits | [H] (forms as [C] p.9) | No source shows their order. |

## Stroke counts

Vowels: अ4 आ5 इ2 ई3 उ2 ऊ3 ऋ4 ऌ3 ऍ4 ए3 ऐ4 ऑ6 ओ6 औ7.
Consonants: क4 ख4 ग3 घ3 ङ3 च3 छ2 ज4 झ4 ञ4 ट2 ठ2 ड2 ढ2 ण3 त3 थ3 द2 ध3 न3 ऩ4 प3
फ4 ब4 भ3 म3 य3 र2 ऱ3 ल4 ळ2 ऴ3 व3 श3 ष4 स4 ह3; क़5 ख़5 ग़4 ज़5 ड़3 ढ़3 फ़5 य़4.
Signs: ँ2 ं1 ः2 ़1 ऽ1 ा1 ि2 ी2 ु1 ू1 ृ1 ॅ1 े1 ै2 ॉ2 ो2 ौ3 ्1 ॐ4 ।1 ॥2.
Digits: one stroke each. 248 strokes in all.

## Where the sources disagree, and other choices

1. **[C] is followed where the sources differ**: ऋ (the bar before the right
   curl: [C] [J]; [S] has the curl first); ऊ (the hook before the headline:
   [C] [S]; [J] after); ष (the bar before the diagonal: [C] [J]; [O] the
   diagonal first); ङ (the dot after the headline: [C]; [O] before); उ (from
   the top left of the small lobe: [C] [J]; [S] from the headline).
2. **The marks of ऐ ओ औ (and े ै ो ौ) go up from the headline** ([C]'s arrows,
   [S]); [J] draws them downwards onto it. Of two marks (औ ै ौ), the lower,
   left one first, then the upper ([S]).
3. **ज: the bowl from its right arm**, under the tongue: down, round to the
   left and up to its point ([C]'s arrow). [O] and [J] start it at the top
   left. A learner taught the other way will be marked off in direction here.
4. **The headline covers only the bar's part in अ आ ओ औ ऑ थ ध भ** ([C], and
   the printed forms), so ध differs from घ and भ from म. Everywhere else it
   spans the letter, श included ([C]).
5. **भ's loop runs clockwise** (up its left side, over, down into the stem), as
   [J] draws it and as the printed form's loop ends; [C]'s arrow runs it the
   other way, on a slightly different form.
6. **ह: three strokes**: the body (stem, top, down the left, the belly round
   to its inward end), then the tail from the left side, then the headline
   ([C]'s stages, [O]). [J] draws another form in one body stroke.
7. **छ: one body stroke**: the little c, the bowl, up the right side, over and
   down inside ([O] [J]; [C] starts it at the top left too).
8. **ख: four strokes**: the र-part with the long sweep to the bar's foot, then
   the loop from its top right ([C]'s stages, [O] [J]); ल: the C, then the
   stroke up to the bar ([C] [O]; [J] draws the bar first). ग's knot is at the
   end of a stem drawn downwards ([C] [J]; [O] starts at the knot).
9. **Vowel signs alone ([H])**: ा a bar; ि ी the bar, then the hook from its
   top; ु ू ृ from where they join the consonant (ु as उ's big lobe); े ै as
   the marks above; ् a short stroke down to the right; ं ़ a dot; ँ the
   candra, then the dot; ः the upper dot, then the lower; ॅ ॉ the candra left
   to right (ॉ after its bar). Whether ि is written before or after its
   consonant in a word is not settled by any source (`hindi_sources.md`).
10. **ऌ ळ ॐ ऽ ([H])**: ऌ as ल's C, then the right part with its hook; ळ in one
    stroke from the headline round both loops; ॐ the 3, the right lobe, the
    candra, the dot; ऽ one stroke from the top right.
11. **Digits ([H])**: one stroke each, from the top; ० anticlockwise from the
    top; loops (१ ९) run up their left side first; ४ from the top left, through
    the crossing, round its loop and up to the right.

## Licence

The stroke data is our own work: drawn for this project, not derived from any
other stroke data or font outlines. Its licence is for the owner to decide
(as for the Hangul data: CC BY-SA 4.0 or CC0 are the candidates). The
generated XML says "licence to be decided" until then.

## Known limits

- One style throughout: a clean, upright school hand, uniform stroke width.
- Shapes follow the printed letters' proportions (Noto Sans Devanagari, the
  app's font), narrowed a little; where a printed form and the school chart
  differ in topology (भ's loop), the printed form is drawn.
- The order of the vowel signs, the digits and ऌ ळ ॐ ऽ has no source; see the
  house choices. ज's bowl and the direction of the marks of े ै ो ौ follow
  [C] against two other sources.
- Dots are short ticks; a learner's tap or tiny circle should be judged
  leniently.
