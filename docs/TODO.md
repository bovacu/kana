# Kana — TODO

In order. Each item says what it is, what it needs, and what is still open.
Tick an item when it has been seen working on the iPad, not when it compiles.

---

## 1. Check free writing — design §4, without ML Kit

*Built 2026-09-30 (check.h/.c), waiting to be seen on the iPad.*

- [ ] In the lasso menu, beside Cut/Copy/Duplicate/Delete: **Check**. It checks the
      selected strokes as one character.
- [ ] Rank the selection with the draw-to-search matcher (`match.h`), which is
      offline, fast and already built.
- [ ] Show the best few matches; tap the one you meant.
- [ ] Score the selection against it with `score.h`: order, direction, shape, and
      the same feedback line as Practice.
- [ ] From the result: **Practice** that character (the practice screen).

### 1b. Check sentences

*Built 2026-09-30 (segment.h/.c, check.h/.c), waiting to be seen on the iPad.*

- [ ] A selection of several characters is READ: split into the characters it
      was written as, across or down, one line or several (`segment.h`).
  - Tested on synthetic sentences written from the reference strokes (jitter,
    tight and loose spacing, across and down): 69/69 read right; ~11 ms a
    sentence optimised, ~35 ms in a debug build (a long column down: ~260 ms).
  - Small kana by size and place (っ/つ), and by what may come before them
    (ょ only after an i-row kana); look-alikes across scripts (へ/ヘ, ー/一,
    カ/力, ロ/口…) by the script around them.
- [ ] The Check screen: the whole writing with a box around each character, the
      reading under it with each character's score, and the one tapped looked
      at closely (square, candidates, feedback).
- [ ] **I meant…**: a text typed in (romaji — lower case hiragana, UPPER
      katakana — or Japanese from the keyboard) reads the selection as that,
      and scores each character against what was meant.
- [ ] Practice from Check practises the reading as a set; Stroke order steps
      through it.
- [ ] Japanese text shows in the UI font (Noto Sans JP as Roboto's fallback).
- **Real writing** (the first sentence copied off the iPad, ありがとございます,
  2026-09-30): at first read as 6 characters (neighbours merged — real strokes
  match KanjiVG at 15-20 a stroke, not 2-5 like the synthetic ones). With the
  gaps along the line weighed, scripts kept per word and kanji frequency:
  **split 9/9, read 7/9** (い → じ, す → 丁: the writer's forms, far from
  KanjiVG's, are not among the candidates). "I meant" splits it 9/9.
- **Three more real sentences** (all kana, kanji, katakana; kept in
  `data/samples/` with their truth): the kana one merged neighbours again (the
  gap floor was too high). Now, over all four (51 characters): **split 51/51,
  read 46/51** — missed: す (as 丁) and い in ありがとう, ね (the writer joins
  its first two strokes), セ (as ヒ), ン (written with ソ's second stroke: the
  reading is right to say ソ). Dakuten are read apart from their base (で, ジ);
  a small kana must fit what follows (あつい, not あっい); particles sit between
  katakana words; missing strokes cost; out-of-use kana (ヺ) cost.
  A 68-stroke sentence reads in ~0.4 s in a debug build.
  A fifth, smaller and quicker (ありがとございます again): split 9/9, read
  6/9 — its が has 4 strokes (か's third left out) and reads as ド, pulling り
  to リ. Over all five: **split 60/60, read 52/60**; "I meant" splits 60/60.
- Tried 2026-09-30, measured on `data/samples/` (baseline 52/60):
  - **The writer's own hand as templates** (good Practice attempts as extra
    references): 51-53/60 whatever the threshold — the characters that need
    help score low against KanjiVG, so they never qualify, and a practised
    character pulls look-alikes that are not (ち ↔ す). Built, measured,
    **removed**.
  - **A word list**, upper bound (a list holding exactly the samples' words):
    55/60 at best — the misses are mostly characters the matcher does not
    even propose, which no word list can choose. Still worth having for #2,
    not as the fix for reading.
- Open, to make reading better:
  - Samples from OTHER writers (the App Store will have many): the same five
    sentences written by a few people, kept in `data/samples/`.
  - **ML Kit trial — done 2026-09-30, branch `ml-kit`:** Google ML Kit Digital
    Ink Recognition (iOS) read all five samples exactly: **60/60** (Kana's own
    reader 52/60), ~35-65 ms a sentence (the first ~380 ms). Check asks it
    first and reads the selection AS its answer (our split, our scores); Kana's
    reader stays for the desktop, before the model is downloaded, and as the
    fallback. Built in without CocoaPods (`tools/mlkit/setup.py`), with builder
    options `--ios_framework`, `--ios_bundle` and the iOS `-L`. IPA 25 MB (was
    ~5). **Kept (Borja, 2026-09-30).** Done since: every SDK's privacy
    manifest ships in a bundle of its own; Settings › Handwriting (ML Kit on
    or off — off, nothing is downloaded or sent — and the model's state, with
    Download / Retry); About credits ML Kit and the fonts; Settings › Licences
    shows every licence in full (ML Kit's 25,000-line notices scroll).
    No permission prompt is needed. What is left for the App Store (the
    privacy answers, the policy, export compliance) is in `docs/app_store.md`.
  - **Google ML Kit Digital Ink Recognition**, only if the above is not
    enough: iOS spike first (CocoaPods frameworks linked into an RDE build,
    ~20 MB Japanese model downloaded once, no desktop version). It returns
    text only, so it would replace the free reading, feeding "I meant" (our
    split) and our scoring; our reader stays for the PC and as the fallback.
- Open: fixing a split by hand (join / split) if "I meant" is not enough; the
  matcher is rough-then-exact now (~2.5× faster) — Browse's draw-to-search
  uses it too.

## 2. Example vocabulary — the rest of design §2 (Reference)

- [ ] **Needs a new source:** JMdict, EDRDG's Japanese–English dictionary. It is
      the same group and the same licence (CC BY-SA 4.0) as KANJIDIC2, so the
      credits in Settings > About and `assets/data/LICENSE-data.txt` grow by one
      entry.
- [ ] **Same bake, one more input:** `--bake` also reads `data/raw/JMdict_e.xml`
      (a download, documented in COMMANDS.txt).
  - It keeps only common words (JMdict's priority marks: news1, ichi1, spec1…),
    a few per kanji.
  - Each word stores its written form, reading, first meaning, and the kanji it
    contains.
  - Estimate: well under 1 MB added to the shipped data.
- [ ] **Viewer:** 3–6 example words under the meaning, each with its reading
      (drawn from strokes, like the readings) and its meaning.
- [ ] **Tap a word:** practise its kanji as a set.

## 3. Guided training mode — explore first

To design properly before building. The first idea:
- the model faint in the square;
- a dot where the next stroke starts, and an arrow for its direction;
- each stroke checked as the pen lifts, so "backwards" or "wrong stroke" shows at
  once, not only at Score.

Questions to settle then:
- Does it show one stroke at a time, or the whole character?
- Does it let a wrong stroke stand, or ask again?
- How does it progress: traced → half-faded → blank?
- Is it scored and saved to the album like Practice?

## 4. Practice squares on the canvas

- [ ] An optional grid of practice squares over the infinite canvas, half-faded,
      as a guide for writing (like genkō yōshi paper). It is not a page to fill:
      only lines to write between.
- Open:
  - Where the switch lives (per canvas, saved in its file; or global in
    Settings).
  - The square size, and whether it follows the zoom.
  - Squares only, or also the centre cross.

## 5. Release polish

See `docs/app_store.md` for privacy, licences and permissions.


- [ ] A release (optimised, non-debug) build configuration, with performance
      measured in it.
- [ ] **The app icon**, and a launch screen.
- [ ] Check what ships in the bundle: no `.bak`/`.tmp`, only the assets needed.
- [ ] Steps towards TestFlight (a paid developer account is needed for it).
- [ ] Commit the data licence credits screen as done: Settings > About (in place).

---

## Done (for the record)

Ink with Apple Pencil at full rate. Undo and redo. Saving. Lasso with cut, copy,
paste and duplicate. Themes. The data bake (KanjiVG, KANJIDIC2, JLPT). The
viewer. Browse with filter, sort, search, draw-to-search and search by parts. The
kana chart. Practice with scoring, tuned on real attempts. Practice sets. The
album. The side panel with nested folders of canvases and drag and drop.
Settings. The zoom indicator.
