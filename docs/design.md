# Project Kana

A tablet app for learning to **write** Japanese by hand — hiragana, katakana and kanji —
with a stylus, where the app can tell you not just *what* you wrote but *how* you wrote it.

Android and iOS tablets. Pencil/stylus is the primary input, not an afterthought.

---

## Why this exists

Every Japanese app on the market tests **recognition**: can you read this, what does it
mean, which reading is right. Almost none test **production** — and the part of writing
that is genuinely hard to self-teach is stroke order and stroke direction, because you
cannot see your own mistake in the finished character. A 木 drawn in the wrong order looks
exactly like a 木 drawn correctly.

So the thing this app must do, and that a generic handwriting recogniser cannot, is say:

> *"Stroke 3 was drawn bottom-to-top. It should go top-to-bottom."*

That sentence is the product. Everything below serves it.

---

## The four sections

1. **Register** — every kana and kanji, with stroke count, stroke order, radicals and
   components. Browsable and searchable, including search by component.
2. **Reference** — a character in detail: readings (on/kun), meanings, example vocabulary,
   JLPT level, animated stroke order.
3. **Practice** — pick a set of characters, write them, get scored on shape **and order**.
   This is the core.
4. **Free writing** — write whatever you like; select any part of it and have it checked.

---

## THE CENTRAL DECISION: two techniques, not one

Sections 1–3 and section 4 are **different problems**, and conflating them is the trap.

### Practice (§3) needs no machine learning at all

In the practice section **the app already knows which character the user is meant to
draw**. That is not a recognition problem. It is a comparison against a known reference,
and the reference — every stroke, in order, with direction — is available for free from
KanjiVG.

A classifier is strictly worse here. The best it can say is "that looks like 木". It cannot
say which stroke was wrong, because stroke identity is not something it models. Worse, a
good classifier is *robust to* exactly the errors we want to catch: it will happily
recognise a character drawn in a scrambled order, because a human reader would too.

So: **deterministic stroke matching, no model, no training data, no inference cost,
explainable output.**

### Free writing (§4) is the only place recognition is needed

Here we genuinely do not know what the user intended, so something must classify. That is a
solved commodity — see ML Kit below — and not worth building.

### Consequence

**No model gets trained for this project.** That removes the entire handwriting-dataset
problem, which is the expensive, licence-encumbered part (see "Rejected data" below).

---

## Data

### Adopted

| Source | Gives us | Licence |
|---|---|---|
| **KanjiVG** | Per-character SVG, one path per stroke, **in stroke order, with direction**. Kana + ~11,000 kanji. Radical/component grouping. | CC BY-SA 3.0 |
| **KANJIDIC2** | Readings, meanings, stroke count, grade, frequency, JLPT | CC BY-SA 4.0 (EDRDG) |
| **JMdict** | ~200k vocabulary entries, for characters in context | CC BY-SA 4.0 (EDRDG) |
| **KRADFILE / RADKFILE** | Radical decomposition, for search-by-component | CC BY-SA 4.0 (EDRDG) |

KanjiVG is the backbone: it serves the register (§1), the stroke-order animation (§2) **and**
the scoring reference (§3). One dataset, three sections, no training.

### Licence obligations — not optional

Both KanjiVG and the EDRDG files are **share-alike**. Concretely:

- An **attribution screen is required**, naming KanjiVG (© Ulrich Apel) and EDRDG, with
  links to their licences.
- Anything *derived from the data* inherits the terms. The processed database we ship is a
  derivative. Our own application code is not — these are not GPL — but the boundary is
  worth getting right rather than assumed.
- **If this is ever sold, get the share-alike terms reviewed properly.** Nothing here is
  legal advice.

### Rejected data, and why

- **ETL Character Database (AIST)** — the classic Japanese handwriting corpus.
  **Non-commercial use only**, and redistribution is prohibited. It is also *offline* data
  (images), so it carries no stroke order — it cannot express the one thing this app is for.
  Rejected on all three counts.
- **Kuzushiji-MNIST / Kuzushiji-Kanji** — permissive, well-packaged, and **the wrong
  script**: historical cursive, not modern handwriting. Teaching from it would teach the
  wrong shapes.
- **Nakayosi / Kuchibue (TUAT)** — genuine online pen trajectories with order and timing,
  which is the right *shape* of data. Research licence, by request. Only relevant if the
  no-training decision is ever revisited.
- **CASIA-OLHWDB** — online Chinese handwriting. Large kanji overlap, academic licence.
  Same note as above.

If a model ever does become necessary, the first move is **synthetic data generated from
KanjiVG** — perturb the reference strokes — not a licensed corpus. It sidesteps the
non-commercial problem entirely and the labels are perfect by construction.

---

## Recognition, for free writing only

**Google ML Kit Digital Ink Recognition.** On-device, fully offline, Japanese supported,
Android *and* iOS, free. It takes **stroke objects**, not bitmaps — which is exactly what
the pen layer already produces, so no rasterisation step is needed.

Open question: integrating it from an RDE app. On Android it is an `.aar` dependency, which
the builder already resolves from Google Maven and extracts. On iOS it is a framework, and
how that reaches an RDE iOS build is **not yet established**. See Open Questions.

---

## The pen

### Do not use PencilKit — and this is not a compromise

The obvious iOS move is PencilKit: Apple's own ink, beautiful, low-latency, free.

It is the wrong choice here, for a reason specific to this app: **we need the raw stroke
points anyway.** Scoring is per-stroke comparison against a reference, so the app must own
an ordered list of strokes, each an ordered list of points. PencilKit hands back a
`PKDrawing` — a *picture*, from which strokes would have to be recovered. Building our own
ink is not a downgrade from PencilKit; it is a requirement that PencilKit happens not to
meet.

And once the ink is ours, it is **the same implementation on both platforms**, which is what
we wanted anyway.

### RDE already has what this needs

Verified in `rde.h`: the engine surfaces SDL3's pen API in full.

```
RDE_EVENT_TYPE_PEN_BEGIN / PEN_END
RDE_EVENT_TYPE_PEN_PROXIMITY_IN / PROXIMITY_OUT
RDE_EVENT_TYPE_PEN_DOWN / PEN_UP / PEN_MOVED
RDE_EVENT_TYPE_PEN_BUTTON_DOWN / BUTTON_UP
RDE_EVENT_TYPE_PEN_AXIS
```

with `rde_event_pen` carrying `position`, `pressure` (0..1), `tilt` (degrees), `eraser`,
`button` and `pen_id`. Pressure and tilt arrive as `PEN_AXIS` updates rather than on every
move — the axis events have to be folded into the current pen state, not read off a move
event.

`eraser` is worth designing for from the start: flipping the Pencil to erase is a gesture
users expect, and it is free here.

### What actually decides whether it feels good: latency

Below roughly **30 ms** the ink feels attached to the nib. Above it, the line swims behind
the pen and the app feels cheap no matter how good the rest is. This is the single biggest
technical risk in the project and it should be measured on a real tablet **before** anything
else is built.

Three levers, in order of impact:

1. **Do not drop input points.** Pen hardware samples far faster than the display refreshes.
   Every sample between frames must be consumed, or curves become polygons at speed.
2. **Prediction.** Both platforms can extrapolate a few milliseconds ahead. This is how
   commercial apps appear to beat the display pipeline.
3. **Low-latency presentation.** Front-buffered rendering on Android; on iOS the
   equivalent is minimising the frames between touch and present.

The Android low-latency renderer lives in `androidx.graphics.lowlatency`, and **the builder
supports androidx** — it resolves from Google Maven and unpacks full `.aar` trees without
Gradle. So that path is open if measurement says it is needed.

### Rendering the ink

A stroke is a point list; drawing it well means a variable-width ribbon, not a polyline.
Width from pressure, smoothed — raw pressure is noisy and produces a lumpy stroke. RDE's 2D
renderer already draws filled shapes with mitred joins, which is the primitive this needs.

---

## Scoring

Given the user's strokes and the KanjiVG reference for the same character:

**Normalise.** Scale and translate both to a common box. The user's character being smaller
or off-centre is not an error.

**Resample.** Each stroke to a fixed N points, evenly spaced by arc length, so comparison is
independent of how fast it was drawn or how many samples the hardware produced.

**Compare, stroke by stroke,** and score each dimension separately — because each maps to a
different sentence of feedback:

| Dimension | Test | Feedback it enables |
|---|---|---|
| **Count** | user strokes vs reference strokes | "That was 4 strokes; 木 has 4" |
| **Order** | user stroke *i* nearest to reference stroke *i* | "Strokes 2 and 3 were swapped" |
| **Direction** | start→end vector agreement | "Stroke 3 was drawn upwards" |
| **Shape** | DTW or mean point distance after normalisation | "Stroke 1 is too short" |
| **Position** | stroke centroid vs reference | "The radical is too far right" |

Order and direction are the pedagogically valuable ones and the ones no classifier gives.
Shape should be the *loosest* — handwriting varies, and an app that rejects legitimate
personal style will not be used twice.

**Thresholds must be tuned against real handwriting**, not chosen analytically. That means
capturing a corpus of my own attempts, good and deliberately bad, early.

---

## Open questions

Ordered by how much they would hurt if answered badly, late.

1. **Does SDL3 deliver pen events on iOS with an Apple Pencil?** RDE exposes the API; that
   the events actually arrive on an iPad is **unverified**. If they do not, the shared-ink
   plan needs a native iOS input path. *Settle this first — it is cheap to test and
   everything else depends on it.*
2. **Measured pen-to-ink latency on a real tablet, per platform.** Decides whether the
   low-latency renderer work is needed at all.
3. **ML Kit on iOS from an RDE build.** Android is understood; iOS is not. §4 depends on it,
   and §4 is the least important section — so this can wait, and can be cut.
4. **How strict is "correct"?** Needs real data and taste, not a formula.
5. **Where does the processed data live?** KanjiVG is ~11,000 SVG files; parsing them at
   runtime is wrong. Wanted: a baked binary of stroke polylines, built offline, small enough
   to ship in the APK/IPA.

---

## Milestones

Each ends in something that can be held in the hand and judged.

1. **Ink spike.** One screen, pen down/move/up, ink on the glass, latency measured on both
   an Android tablet and an iPad. *Answers open questions 1 and 2, and is a go/no-go for the
   whole approach.*
2. **Data bake.** KanjiVG + KANJIDIC2 → one binary asset. Draw a character's strokes in
   order as an animation. *Proves the data pipeline and gives §2 almost for free.*
3. **Scoring.** Write a character, get per-stroke feedback against the reference. Tune
   thresholds against real attempts. *This is the product; everything before it is
   scaffolding.*
4. **Practice sets.** Choose characters, drill them, track what is weak.
5. **Register and reference screens.** Mostly UI over data that already exists by then.
6. **Free writing + ML Kit.** Last, and droppable.

---

## What this is not

- Not a reading/vocabulary SRS. There are good ones already, and competing with them
  dilutes the one thing this does that they do not.
- Not a dictionary, beyond what supports writing practice.
- Not a model-training project. See the central decision.
