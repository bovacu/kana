# Kana — TODO

In order. Each item says what it is, what it needs, and what is still open.
Tick an item when it has been seen working on the iPad, not when it compiles.

---

## 1. Check free writing — design §4, without ML Kit

*Built 2026-09-30 (check.h/.c), seen working on the iPad 2026-09-30.*

- [x] In the lasso menu, beside Cut/Copy/Duplicate/Delete: **Check**. It checks the
      selected strokes as one character.
- [x] Rank the selection with the draw-to-search matcher (`match.h`), which is
      offline, fast and already built.
- [x] Show the best few matches; tap the one you meant.
- [x] Score the selection against it with `score.h`: order, direction, shape, and
      the same feedback line as Practice.
- [x] From the result: **Practice** that character (the practice screen).

### 1b. Check sentences

*Built 2026-09-30 (segment.h/.c, check.h/.c), seen working on the iPad 2026-09-30
(with ML Kit reading, see below).*

- [x] A selection of several characters is READ: split into the characters it
      was written as, across or down, one line or several (`segment.h`).
  - Tested on synthetic sentences written from the reference strokes (jitter,
    tight and loose spacing, across and down): 69/69 read right; ~11 ms a
    sentence optimised, ~35 ms in a debug build (a long column down: ~260 ms).
  - Small kana by size and place (っ/つ), and by what may come before them
    (ょ only after an i-row kana); look-alikes across scripts (へ/ヘ, ー/一,
    カ/力, ロ/口…) by the script around them.
- [x] The Check screen: the whole writing with a box around each character, the
      reading under it with each character's score, and the one tapped looked
      at closely (square, candidates, feedback).
- [x] **I meant…**: a text typed in (romaji — lower case hiragana, UPPER
      katakana — or Japanese from the keyboard) reads the selection as that,
      and scores each character against what was meant.
- [x] Practice from Check practises the reading as a set; Stroke order steps
      through it.
- [x] Japanese text shows in the UI font (Noto Sans JP as Roboto's fallback).
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

*Built 2026-09-30 (bake.c, kanji.h/.c 'WORD', viewer.h/.c), seen working on the
iPad 2026-09-30.*

- [x] **The source:** JMdict (JMdict_e, English), EDRDG's Japanese–English
      dictionary, CC BY-SA 4.0 like KANJIDIC2: credited in Settings › About and
      `assets/data/LICENSE-data.txt`.
- [x] **Same bake, one more input:** `--bake` also reads `data/raw/JMdict_e.xml`
      (COMMANDS.txt has the download), a line at a time (60 MB of XML).
  - Only common written forms (news1, ichi1, spec1, spec2), up to four
    characters, whose kanji all have strokes.
  - Up to six a kanji, best first: the kanji on its own (月 つき, 上 うえ);
    then by newspaper frequency, with everyday words (ichi1) about mid-list;
    words whose other kanji are harder than it, usually written in kana, or
    that are names (山形 "Yamagata") lower; one number word at most (一月,
    not all twelve months); no longer form of one kept (日本人 after 日本).
  - Each word: its written form, reading and first meaning (its first
    glosses, while short). A word is stored once however many kanji list it.
  - 8,818 words for 2,548 kanji: +0.45 MB (characters.kana 3.9 MB). The
    stale `characters.kana.bak` that was shipping (3.3 MB) is gone: the bake
    removes it.
- [x] **Viewer:** the words under the details (portrait), or in a column beside
      the character (landscape, which also keeps the character big): each a
      row with its written form (Noto Sans JP), reading and meaning, cut to
      the screen with "…". Portrait drops words (down to three) before the
      character gets smaller than 300.
- [x] **Tap a word:** practises its kanji as a set (kana in it left out).
- Fixed on the first look (2026-09-30): the columns overlapped (Noto's
  characters are 1.31× their size, not 1×: now measured through the font), and
  録 was missing — Slug's default budgets (128 curves a glyph) left out 28% of
  Noto's kanji, silently, everywhere Japanese text shows. The Japanese font now
  has 304 curves / 1,280 band texels a glyph (every kana and kanji in the font
  fits: measured) and 2,048 slots (~30 MB of GPU memory). The slots are no
  longer a limit over time (2026-10-01): once all are taken, RDE gives a new
  glyph the slot of the one drawn longest ago (unused for 3 frames at least),
  so 2,048 only bounds the distinct glyphs on one screen.
- Slug's glyph textures grow and shrink (2026-10-01, Borja): nothing about a
  Slug font is set any more — rde_font_parameters lost max_glyphs and the two
  per-glyph budgets. A font starts at 128 slots of 128 curve + 256 band texels;
  slots double when all are taken and none has gone ~10 s undrawn (up to 4096),
  and the slots widen when a bigger glyph comes (up to 2048 curves / 4096 band
  texels a glyph, past any real font: Noto's busiest kanji needs 289 / 1,220).
  rde_font_trim takes a font back to the start; Kana trims its four fonts on
  going to the background or a low-memory warning. Kana's fonts held ~46 MB of
  GPU memory from launch (Japanese 30, Latin 10, the two icon fonts 3 each);
  they now start at 1.5 MB and sit at ~3 MB after the usual screens (鬱's
  viewer takes the Japanese font to 1.7 MB). No glyph can be left out for
  being too big any more (録 once was). Checked on the Mac with every font
  started at 8 slots, so rows and widths grew together and were trimmed
  mid-run: Statistics, Settings, Browse and the viewer on 鬱 and 録
  pixel-identical to the normal shots (only the stroke animation's frame
  differs), also with RDE's threaded renderer. --trim-fonts (debug) trims just
  before the shot. On the iPad: installed and launching; going to the
  background and back is Borja's to see.
- Open: words for kana (あ: あさ, あめ…)? JMdict has them, but which ones to
  show is a different choice (words that start with it?).

## 3. Guided training mode

*Built 2026-09-30 (guide.h/.c, Practice's guided mode), seen working on the iPad
2026-09-30.*

Decided (Claude's proposal, not objected to): one stroke at a time; a wrong
stroke is taken back and written again; traced → start dots → from memory; only
the last step is scored and saved.

- [x] **Guided** in Practice's row (both: one character and a set): one big
      square instead of the squares, the squares' − / + off. It stays on across
      a set's characters.
- [x] **Step 1, trace:** the character faint; the stroke to write stronger,
      writing itself (red pen tip), a dot where it starts and an arrow beside
      its first part for its direction.
- [x] **Step 2, start dots:** the character fainter, only the dot; the stroke
      writes itself only after a miss.
- [x] **Each stroke checked as the pen lifts** (steps 1 and 2), in place
      against the model: a later stroke ("That is stroke 3. Stroke 2 comes
      first."), backwards, or not close enough. A wrong stroke flashes red and
      goes, the reason shows under the square, the stroke shows itself again.
      Undo takes back the last right one; Clear starts the step again.
- [x] **A step done:** a tick, and the next after a moment (or at once, with the
      pen).
- [x] **Step 3, from memory:** a blank square, nothing checked on the way; with
      all its strokes it is scored and saved like a Practice square (a session
      of one square, in the album), the model faint behind it and the score in
      the corner. Writing again starts another attempt. Score scores a short
      one; it does nothing in steps 1 and 2.
- Tested (guidetest): each mistake's message, Undo, the pause, steps 1-3, a
  wrong order in step 3 scored (not stopped), a set moving on.
- Open, to tune on the iPad: how close is close enough (10 units of 109
  tracing, 13 from the dot); the arrow's look; whether Guided should be
  remembered in the settings.

## 4. Practice squares on the canvas

*Built 2026-09-30 (canvas.h/.c, save.h 'PAGE', the toolbar's Squares, Settings ›
Practice squares), seen working on the iPad 2026-09-30.*

Decided (Claude's proposal): per canvas, saved in its file; part of the page
(they pan and zoom with it); squares with the dashed centre cross.

- [x] **Squares** in the floating toolbar (after Page/Screen): the page's
      practice squares on or off, lit when on. Each canvas has its own; a new
      one starts without.
- [x] A grid of squares over the page instead of the dots, lines only, in the
      theme's canvas colours (the edges as its dots, the dashed centre crosses
      fainter, between those and the page), one centred on the page's origin,
      where Reset puts the middle of the screen. Zoomed out past 72 points a
      square, only the lines.
- [x] **Settings › Practice squares:** Small / Medium / Large (110 / 160 / 240
      canvas units; Medium a comfortable character at zoom 1), the same on
      every canvas, saved in the settings. The Settings card grew a row; its
      About text now shrinks to fit a short (landscape) screen.
- [x] Saved with the page ('PAGE' chunk: a file from before it loads without
      squares, and an older build skips the chunk).
- Open: genkō yōshi's gaps between columns, for vertical writing.

## 5. Background modes

*Built 2026-09-30 (canvas.h/.c KANA_PAPER_, the toolbar's Paper panel), seen
working on the iPad 2026-09-30.*

- [x] The page's paper: dots, lines, squares or nothing — **Paper** in the
      floating toolbar opens a panel of the four (like Color's palette; one of
      the two open at a time), per canvas, saved with the page ('PAGE' u8: a
      canvas saved with squares keeps them).
- [x] Lines fall on the squares' edges (writing stays in place switching), in
      the theme's dot colour. One size for both: Settings › Lines & squares.

## 6. Selecting characters

*Built 2026-09-30 (select.h/.c, Browse's and the chart's Select), waiting to be
seen on the iPad.*

- [x] **Select** in Browse's row and the chart's: a tap ticks (or unticks) instead
      of opening; every cell shows a circle, a ticked one filled with a check
      and the cell tinted.
- [x] The row while selecting: **All** (Browse: every filtered, sorted, searched
      result; the chart: the section in view), **None**, **Practice n** (the
      ticked as a set, in the order ticked), **Done** (taps open again; the ticks
      stay until None, or the app quits).
- [x] One selection for both: kanji ticked in Browse and kana in the chart
      practise together.
- Later: exams and study marks from the same selection.

## 7. Study marks and exams

*Built 2026-09-30 (marks.h/.c, examlog.h/.c, exam.h/.c; the viewer's Study, Select
mode's Study and Exam, Browse's Studying / Known filters, the side panel's Exams),
seen working on the iPad 2026-09-30.*

Decided with Borja, 2026-09-30:
- Marks: **Studying** and **Known**, per character (by code point: they survive
  a re-bake), saved in `marks.kana`.
- Exams: pick a source → a preview of everything, ticked (untick to leave out)
  → each character once, no help → results, saved.
- The prompt, kanji: meaning, readings, and an example word with the kanji
  blanked (□生 がくせい "student"). Kana: the romaji ("ka", in hiragana).
- Right: recognition (ML Kit, the matcher behind it) has it among its first 3
  candidates. Points: how well it was written (Practice's score) when right, 0
  when not. Passed with 80% right.

- [x] **Marks:** the viewer's **Study** cycles Study → Studying → Known → none;
      Select mode's **Study** marks the ticked Studying (all already are: none);
      Browse's **Studying** and **Known** filters.
- [x] **Exams** (side panel), the setup: What (Studying, Known, N5..N1,
      Hiragana, Katakana — with their counts; Selection when opened from Select
      mode), How many (10, 20, 50, All — an exam asks 100 at most), shuffled.
- [x] Select mode's **Exam**: the ticked, straight to the preview.
- [x] The preview: every one ticked, tap to leave one out; All / None; Start n.
- [x] Writing: "3 of 20", the prompt, a blank square (the pen only); Undo,
      Clear, Next (Finish on the last), Quit. Answers are read while the next
      is written (ML Kit one at a time; the matcher alone without it).
- [x] Results: "17 of 20 right - 78 points  Passed", each answer's writing
      with what was asked in its corner, ✓ and points or ✗ and what it read
      as; tap one: the viewer, walking the exam. Retry wrong (straight to
      writing), Practice wrong (a set).
- [x] Kept (`exams.kana`: each exam's results and the writing); marks follow:
      right in 3 exams in a row → Known; a Known one wrong → Studying.
- Tested (examtest): marks and the log round trip; the sources; an exam with
  one written as another character (wrong, read as it, 0 points); Known after
  three, back to Studying after a miss; Retry wrong; a hiragana exam.
- [x] Marks shown: a badge in the top-right corner of Browse's and the chart's
      cells (top-left in Select mode, the tick has the right) and of the
      viewer's square — Studying a circle, Known a square, for now (icons with
      the restyle, #9).
- [x] The hiragana and katakana sources are the gojūon and its voiced forms,
      71 each: no small kana (ぁ っ ゃ ゎ ゕ), no old ones (ゐ ゑ ゔ ヷ..ヺ).
- Answers are not read while the viewer or Practice is over the results; they
  go on when back (Borja: fine).
- [x] Exams in the album (2026-10-01, to see on the iPad): the album's **Exams**
      (by the sorts) lists every exam, newest first — what it was of, when,
      passed or not, right / points, each character asked in green or red; a
      tap opens its results again, the writing read back from `exams.kana`
      (Retry wrong and Practice wrong work as after any exam). A character's
      page shows its exam answers among its sessions, by date: right or wrong,
      which exam, the writing (a tap replays it at a steady pace: exams keep no
      timing) and its points or what it read as.
- Tested (examtest, albumtest): the writing read back per exam and per
  character; a kept exam reopened as it was, nothing kept twice, Retry wrong
  from it; the exams view and its tap; the page's order by date and a replay.

## 8. Statistics

*Built 2026-09-30 (stats.h/.c, the side panel's Statistics), seen working on the
iPad 2026-09-30.*

Everything the app keeps, worked out when the screen opens (every practice
history file read once, the exam log, the marks); local days. Cards, one column,
or two on a wide screen, scrolling:

- [x] **Overview:** practice sessions, characters practised, squares written,
      writing time (the pen on the page), days active, day streak (up to today,
      or yesterday: today may still come), best streak, exams taken and passed,
      exam accuracy, studying, known. "since <the first day>" by the title.
- [x] **Activity:** a calendar of the last 26 weeks, Monday first — squares
      practised and exam answers each day, in four strengths.
- [x] **Practice scores:** the last 30 days against the 30 before, all time; the
      average score each week, as a line over lines at 0, 50 and 80.
- [x] **Exams:** taken, passed, answers right, points; the last 12 as bars
      (the share right, against the 80% pass line) and dots (points).
- [x] **Levels:** hiragana and katakana (the 71 each exams use), N5..N1: known,
      studying, practised, of all.
- [x] **Mistakes:** the share of squares with each — stroke order, direction,
      missing and extra strokes, a poor shape (under 60) — all time and the
      last 30 days.
- [x] **Characters:** the 10 weakest (by their latest session) and the 10 most
      improved (first session to latest); a tap: the viewer, walking the list.
- [x] **When you practise:** squares by hour of the day and by weekday.
- Tested (statstest): a planted history (three characters over 40 days,
  mistakes of each kind, two exams, marks): every number, the streaks, the
  calendar, the lists; the screen in portrait and landscape, scrolled, a tap.
- [x] **Known over time** (2026-10-01, to see on the iPad): every change of
      mark is kept now (`marks.kana`'s 'MHIS'; an older file starts its history
      with the marks it has); a card with Known and Studying at the end of each
      of the 26 weeks as stacked bars, and how Known moved in the last 30 days.
- Tested (examtest, statstest): the changes kept in order and through a
  reload, an old file seeded; planted changes over 100 days give each week's
  counts.

## 8b. The learner's own words

*Built 2026-09-30 (userwords.h/.c, the bake's longer word lists, the viewer's
Add, the toolbar's word form), seen working on the iPad 2026-09-30.*

Decided with Borja, 2026-09-30: both — pick from a longer JMdict list per kanji
(20 words: the 6 examples, then more), and type your own; the list scrolls, Add
by the Words title.

- [x] The bake keeps up to 20 words a kanji: the examples as before (common,
      up to 4 characters, the same 10,461), then the rest in rank order —
      common ones, then uncommon ones (up to 5 characters). 51,705 words for
      5,108 kanji; characters.kana 6.1 MB (+2.2 MB). 'WORD' lists say how many
      of their words are examples.
- [x] The viewer's Words: the learner's words first (a bar at their left), then
      the examples; as many rows as fit, scrolling; **+ Add** at the right of
      the title.
- [x] **Add**: the character small, "n of yours", and every further word, a
      tick each (tap: added, or taken off); words typed in listed first,
      ticked. Its row: **Done**, **Type your own**.
- [x] **Type your own**: a form high on the screen (the keyboard under it) —
      the word (must have the kanji), its reading (kana, or romaji made
      hiragana), its meaning (optional); Return goes to the next field.
- [x] Kept in `words.kana` as text (a re-bake cannot lose them). Exams blank
      the learner's words first; Statistics counts them.

## 9. UI restyle, icons, the string table

- [x] A design pass: spacing, radii, a type scale, primary / secondary / toggle
      buttons, one panel style, across every screen. Icons (an icon font through
      Slug: sharp, theme-coloured, beside text in one label; Borja picks the
      set). Every UI string into a table, for translation.
      - Done, seen working on the iPad 2026-10-01 (from the mock-up Borja approved): light
        surfaces and one accent per theme (theme.h: surface, surface_2,
        outline, accent, on_accent, tint); the kit's looks (plain, quiet,
        selected, primary, danger, chips) and Phosphor icons in buttons
        (toolbar_kit.h: kana_toolbar_icon); the bar icon-only with
        separators; every row an icon over its label; the side panel's rows
        with icons (字, あ, 試 as characters); outlined fields; the viewer's
        chips, 音 / 訓 / 部 written from their strokes, the words in a card;
        the exam's header, progress and prompt card; statistics' cards; the
        marks as icons (Studying an amber star, Known a green seal). Cards
        (Settings, Licences, the note card, the word form) restyled; fields in
        rounded boxes with themed text and caret (engine: the text editor's
        new set_text_color / set_caret_color); icons centred for their
        bearings (a label is off by a glyph's whole left bearing).
      - The string table: done, seen working 2026-10-01. Every UI string is an
        id (src/base/text_ids.h, KANA_TEXT_*; made by tools/text/strings.py) in assets/text/strings.rdel (RDE's
        localization: one block per language, {0} and plural placeholders);
        text.h caches them and renders templates through RDE.

## 10. Other languages

- [x] Meanings in other languages: KANJIDIC2 has Spanish for 2,505 kanji (all
      2,136 Jōyō), French 2,066, Portuguese 1,944; JMdict's full file has
      German, Russian, Dutch, French, Spanish, Hungarian, Swedish, Slovenian
      (coverage of the common words to measure; English where missing). The
      UI strings translated too. Spanish first.
      - Done, seen working 2026-10-01: English, Spanish, Portuguese (Brazil),
        Japanese and French — the UI in all five (strings.rdel), chosen in
        Settings › Language (a flag each; the device's language at first, then
        the choice, saved; the UI is rebuilt live). Meanings: the bake reads
        the full JMdict — Spanish words 17,610 of 51,705, French 12,796, no
        Portuguese; kanji from KANJIDIC2 (es 2,505, pt 1,944, fr 2,066). English
        wherever a language has none (and Japanese is English throughout).
        Browse searches the meaning in the language and in English, accents
        folded ("arbol" finds 木).
      - Translations to have a native speaker read before release.
- [ ] **The meanings JMdict and KANJIDIC2 lack, translated by AI** (Borja: yes,
      both phases; to run overnight). Still English-only after the bake:
      words — es 34,095 (1,738 of them examples), pt 51,705 (8,818), fr 38,909
      (1,647); kanji — es 3,908, pt 4,469 (282 JLPT), fr 4,347 (219 JLPT).
      Phase A: every kanji meaning and the example words (~25,000 items).
      Phase B: the rest of the words (~112,000). Translated with the Japanese
      word and reading as context, in checked batches, into
      data/translations/<lang>/{words,kanji}.tsv (keyed by written+reading,
      and by character); the bake prefers EDRDG's, then these (flagged as AI),
      then English. CC BY-SA 4.0 like their sources: a line in
      LICENSE-data.txt and the About credits saying they are machine-translated.

## 10b. Text in and out of the canvas

*Built 2026-10-01 (textink.h/.c; the selection's Copy as text, the context
menu's Paste text), to see on the iPad.*

- [x] **Copy as text** (the lasso's selection menu): the selection read as Check
      reads it — ML Kit's best reading, Kana's own (segment.h) without it — and
      put on the system clipboard; the page says what was copied (up to 16
      characters; more: how many).
- [x] **Paste text** (the page's long-press menu, when the clipboard has text):
      the text written in each character's own strokes with the brush — 72
      points a character at the zoom it is pasted at, lines wrapping at 80% of
      the screen, line breaks kept — where the menu was opened, selected to drag.
      What the data does not have is left out and counted (the data has kana,
      kanji, 、。 and the full-width Latin letters).
- Tested (textinktest): written and read back the same — one line, two lines,
  small and large; spaces, wrapping, centring, an emoji left out.
- [x] **Text from a photo** (2026-10-01, to see on the iPad; Borja chose ML Kit
      for both platforms): the page's long-press menu → a screen with Back,
      Camera, Photos, Hold / Write n. CAMERA is live (RDE's camera, 1280x720,
      its permission asked through RDE the first time): every line ML Kit finds
      is boxed over the picture, read again and again — the newest frame each
      time the last read is done — and Hold keeps the frame and its lines (the
      camera stops; it also stops when the app goes to the background). PHOTOS:
      the system photo picker (no permission: only that photo reaches the app).
      Either is read by ML Kit Text Recognition's Japanese model — in the app,
      1.9 MB, no download — and shown with every line boxed, all kept at first;
      a tap leaves one out. Write puts the kept lines on the page, in the order
      read, as Paste text does. RDE gained rde_engine_enable_subsystem (Borja: the
      camera is not among what the engine starts — RDE_SUBSYSTEM_CAMERA, SENSOR,
      GAMEPAD turned on and off by the app; Kana turns the camera on only while
      it is live), rde_device_camera_get_rotation (SDL's
      per-frame turn, which RDE dropped), rde_memory_texture_get_texture, and
      rde_device_camera_update_texture refusing a texture of the wrong size (it
      wrote past it). With ML
      Kit switched off in Settings it says so. tools/mlkit/setup.py now fetches
      the text recognition pods too (MLKitTextRecognitionJapanese 6.0.0, its
      Common, MLKitVision, MLImage). textscan.h is the platform's part (iOS:
      textscan_ios.m; Android to come, with the same ML Kit recognizer; the
      desktop: not available), scan.h the screen, the same everywhere.
- Seen working on the iPad (2026-10-01), after three fixes: the picture was
  upside down (RDE draws a texture's first row at the bottom; the camera's rows
  come top first — mirrored back — and RDE turns CLOCKWISE for a positive angle,
  checked with --scan-demo-turn / --scan-demo-top-first); the camera gave ONE
  frame a second (SDL reads a frame rate of 0 as "the slowest": RDE now asks for
  30); a read's preparation (copy, upright redraw) moved off the frame loop. On
  the iPad mini 6, release, --scan-live --perf=10: the app 59.8 fps, the camera
  33 frames a second shown, 17 read a second at 33 ms a read.
- The floating bar's camera (2026-10-01, Borja): a button between the paper and
  rotate (its own hairline) opens Text from a photo with the camera already
  live; what is written goes to the middle of the page as it is on screen. Left
  out of the bar where text cannot be read (kana_textscan_available: the
  desktop, Android until its side is written). Seen on the Mac (no camera, the
  bar as before); on the iPad: installed, Borja's to try.
- [x] **Translate with Google** (2026-10-01; Borja chose ML Kit's on-device
      translator for both platforms over Apple's Translation + ML Kit on
      Android): Japanese into the app's language (English when the app is in
      Japanese). translate.h is the platform's part (iOS: translate_ios.m;
      Android: to come, the same ML Kit API; the desktop: not available, but
      debug builds can pretend). Two places use it:
      - Text from a photo: a "Translate with Google" toggle in its row; with a
        held picture (a photo, or Hold), a panel under it lists every line with
        its translation, Google's badge on top, four lines asked at a time;
        left-out lines fade, a tap on a row leaves its line out, a drag scrolls.
      - A lasso selection's menu: "Translate with Google" reads the selection as
        Copy as text does (textink.h), translates what was read, and shows both
        in a card on the selection's other side from its menu, until the
        selection changes or goes; the pen does not draw through it.
      The models are not in the app: Japanese's and the target's (about 30 MB
      each) download the first time (Wi-Fi or mobile data, as the handwriting
      model), and the screens say so. Google's terms: the button reads
      "Translate with Google", the "powered by Google Translate" badge
      (assets/translate/, colour on light themes, white on dark) is by every
      result, and Google's disclaimer heads Settings › Licences › ML Kit
      (tools/mlkit/setup.py writes it). Off with ML Kit's Settings switch.
      tools/mlkit/setup.py now fetches MLKitTranslate 8.0.0 and
      MLKitNaturalLanguage 10.0.0 (and GTMStringEncoding). The .ipa grew 41 →
      55 MB, the executable 95 → 139 MB before stripping.
- Seen working on the iPad (2026-10-01, Borja: "working perfectly"), both
  places. --translate-probe=es on the device: the Japanese and Spanish models
  downloaded in 5.8 s; 今日は日本語を勉強します。→ "Yo estudiaré japonés hoy.",
  駅はどこですか？→ "¿Dónde está la estación?", ~200 ms a sentence. (An iOS
  app cannot quit itself: the probe first stopped RDE's loop to "quit", which
  froze the app on screen — now it just carries on.)
- Tested (scantest, translate.h pretending): nothing asked while off; four at a
  time, in order; the answers to their lines, a failed one; the panel's layout,
  the badge, the clip; a tap on a row; off again. Looked at on the Mac:
  --scan-demo=PNG --scan-demo-translate (light, Night, --scroll), and
  --paste-text=駅はどこですか --translate-selection (Kana's own reader read it
  right).
- Tested (scantest): taps on lines (vertical ones too), what Write writes,
  a photo with no text, one that cannot be shown; live with a stubbed camera:
  the first frame telling the size, frames turned a quarter (upright 1080x1920),
  Hold, the background, Back — the camera closed each time. The screen seen on the Mac
  with --scan-demo=PNG (a debug look flag that feeds it lines). On the iPad:
  installed and launching; the camera, the picker and ML Kit's reading are
  Borja's to try.
- The app grew: the executable 47 → 95 MB, the .ipa 24 → 41 MB (ML Kit's text
  recognition engine). Stripping it (below) matters more now: 95 → 68 MB.

## 10c. Missing features (Borja, 2026-10-01: "all of them, except the daily reminder")

Asked what Kana lacked; Borja chose everything proposed but a daily reminder
("apps that constantly remind to open them piss me off"), and no sync (it needs
a server): export / import instead. Built in this order:

- [x] **Your data** (backup.h; Settings › Your data): says Kana is offline (the
      writing never leaves the device; ML Kit downloads its models once), that the
      platform's own backup has it (iOS: iCloud Backup; Android: Google's Auto
      Backup, on by default — RDE's manifest does not turn it off; the desktop:
      Kana's folder), and Export / Import. Export: everything in the save folder
      but the .bak/.tmp/.bad copies and diagnostics, in one .kanabackup (checksummed)
      — shared on a tablet or phone (the system sheet: Files, iCloud Drive, a
      message), saved where chosen on the desktop. Import: picked (RDE's open
      dialog — on iOS a document picker RDE gained: SDL has no iOS dialog), checked
      whole, a question ("from <date>, n pages"), then Replace: what was there goes
      to before-import/, the backup's files are written (all or nothing: a failure
      puts everything back), and Kana reads everything again in place. A "restore
      from iCloud" button is not possible (iCloud Backup is the whole device's, out
      of an app's reach; an app's own iCloud needs the paid account) — Export to
      iCloud Drive + Import does the same. Tested (backuptest): what goes in, damage,
      the round trip, a failed import changing nothing, paths that would leave the
      folder refused; on the Mac: export, change, import, Replace — the page as
      exported. RDE: rde_ios_pick_documents; the Windows crawl's missing separator
      in subfolders fixed (not built here: Borja's to build on Windows).
- [x] **Apple Pencil's double tap** (RDE: RDE_EVENT_TYPE_PEN_DOUBLE_TAP,
      rde_pen_listen_double_tap — UIPencilInteraction; the user's own setting comes
      with it, RDE_PEN_TAP_ACTION_): the eraser and back, the tool before, or the
      colour palette — as set in the iPad's Apple Pencil settings; nothing when off.
      Built with the iOS 17.4 SDK here: the 17.5 callback is compiled in only with
      a newer one (iOS keeps calling the older one). Borja's to try.
- [x] **Read aloud** (speech.h; iOS: AVSpeechSynthesizer): a speaker before each
      word's reading in the viewer (a tap on the reading says it — the kana, so the
      pronunciation is exact), the 音 / 訓 lines (the readings, without the
      okurigana marks), each row of Text from a photo's translations, and the lasso
      translation card. Borja: "low quality" — the basic voice iOS gives when asked
      for ja-JP. Now the best Japanese voice installed is picked each time (Premium,
      then Enhanced; never a Personal Voice), and while only the basic one is there
      a notice says, once a run, where the better ones are (Settings › Accessibility
      › Spoken Content › Voices — free; Siri's voices are not open to apps).
      Android: to come (TextToSpeech).
- [x] **The welcome / Tutorial** (welcome.h): four pages the first time Kana opens
      (no settings yet) — what Kana is (offline), the pen and the fingers, the long
      press and the lasso, the menu — Skip / Next / Start writing; the bar and the
      menu hide under it. Again from the side panel's Tutorial (Borja: above
      Settings and the version). --welcome[=PAGE] for looks.
- [x] **Reviews** (review.h): spaced repetition of the Studying and Known
      characters — 1 day, 3 days, then the step times the ease (2.5, up with clean
      answers, down with shaky ones, down more when wrong); wrong: tomorrow, the
      steps start over; right early: nothing changes. At most 10 new a day, 50 a
      sitting. Every exam's answers move the schedules; the side panel's "Reviews · n"
      opens an exam of what is due, straight to writing (its own source, never a
      setup chip; the log and the album name it Reviews). reviews.kana (in
      backups). Tested (reviewtest: the steps, early, wrong, the day's new ones,
      the order, kept) — it caught a wrong answer restarting at two days.
      Borja found "Reviews · 1" opening an empty exam (0 of 0): the exam never read
      the list it was given for a review, and the Exam opened after one kept that
      empty list. Fixed; examtest covers both.
- [x] **Rate Kana** (Borja: not invasive — a button in the side panel, over
      Tutorial, on a tablet or phone only): RDE gained rde_mobile_open_review_page —
      iOS: the App Store's write-review page for KANA_APP_STORE_ID (version.h; empty
      until App Store Connect gives the app its Apple ID: until then the system's
      rating sheet, which may not appear), Android: the Play listing.
- [x] **Words from a photo / a translation** (wordsplit.h): the words of a line
      (Kana's dictionary: longest match, verbs and adjectives by their endings,
      alike-spelt words equally common all listed), each with its reading and
      meaning, under the translation in Text from a photo and in the lasso's
      card; a tap adds it to the learner's words (or takes it off). The bake keeps
      every common word now, numbered, with how common it is ('WFRQ': 2,653 more
      words). Tested (wordsplittest, on the real data).
- [x] **Example sentences** (Tatoeba, CC BY 2.0 FR; kanji.h 'SENT'): one per word,
      ~1.6 MB, chosen by length and how many of the languages translate it,
      translated into the app's language (English where Tatoeba has none). Which
      word a sentence has comes from Tatoeba's word index (jpn_indices.csv, with
      readings: 行 ぎょう is not the 行 of 行って); sentences not indexed are
      searched as text, never for a one-kanji word. 16,415 words have one. The
      viewer shows one under the words — the learner's words' first, › for the
      next word's, a tap reads it aloud — when there is room (on a tall screen two
      word rows give way to it; the character stays ≥ 300). Credited in Credits
      and LICENSE-data.txt. While at it, the bake ranks a word's rarer spelling
      below another word written alike (本 ほん before 本 もと, which is usually
      元): 16 kanji's lists changed, 本's for the better. Tested (wordsplittest's
      sentence checks).
- [x] **Practice sheets** (sheet.h): a PDF to print — per character its meaning,
      readings, JLPT and strokes, its stroke order a box per stroke (the new one
      dark, a red dot where it starts), then rows of ten boxes with the dashed
      cross: the model (numbered up to six strokes), three faded to write over,
      the rest empty. One character fills an A4 page (12 rows); more share
      pages, six at most; up to 200 characters. Japanese is drawn from the
      KanjiVG curves (no font in the file), Latin text in Helvetica; the credit
      (KanjiVG, CC BY-SA 3.0) at every page's foot. From the viewer's new Sheet
      button (the character) and Select mode's (the ticked, in order): shared on
      a tablet (the share sheet prints), a save dialog on the desktop.
      --sheet=FILE for looks. Tested (sheettest: 1, 3, 7, 200+ characters, en/ja/fr,
      every cross-reference and stream length checked; rendered with sips).

## 10e. Vocabulary (Borja, 2026-10-01: "target that feature development in that direction")

- [x] **The vocabulary** (vocab.h, replaces "My words", words.kana read over):
      words with reading and meaning, the learner's named LISTS (a word in any,
      or none). A kanji's own words are the vocabulary's with it in. vocabtest.
- [x] **The word card** (wordcard.h): the one place a word is saved, changed,
      filed in lists or removed — from the viewer's rows (a bookmark), a
      sentence's words, a photo's and a translation's words, the lasso (Save
      word: what was written or pasted read; one dictionary word fills the card,
      as the dictionary writes it — 食べました is 食べる; more, its words offered),
      or typed in. A new word comes with the last word's lists ticked. Its list
      form names, renames and deletes lists; its note form, character notes.
- [x] **Readings for the example sentence**: its words as chips with their
      readings (wordsplit.h, now with なさい), a tap: the word card.
- [x] **The Vocabulary screen** (vocabview.h, the side panel's 語 Vocabulary):
      lists as chips, words newest first with their review state; Back, + Word,
      Review n, Exam, Sheet, Practice (the words' characters).
- [x] **Saved lists of characters**: Select mode's Save list makes a list,
      each character a word (its first reading, its meanings).
- [x] **Word exams** (wordexam.h): a word written whole, a box a character (a
      letter, ー, 々 given); by meaning and reading, or by ear; Retry the wrong.
      Every answer moves the word's review (keys above the code points, review.h;
      words have their own 10-new-a-day). wordexamtest.
- [x] **Look-alikes** (kanji.h 'LOOK', baked with the matcher: 未 末, 土 士, 間 問,
      シ ン, わ れ; 1,371 characters), 似 in the viewer, the learner's exam mix-ups
      first in red; a tap visits one (Prev comes back). Beside 部 when it fits.
- [x] **Character notes** (charnote.h): Note in the viewer's header, the note
      under the details (記).
- [x] Fixed on the way: Reviews opened an empty exam (its list never read);
      exams gave reviews their quality as 0..100 instead of 0..1 (every right
      answer made the ease grow).
- [ ] On the iPad: the word card (typing, lists), Save word on handwriting,
      word exams by ear, the screens' layouts.

## 10f. To the App Store (2026-10-02; details in docs/app_store.md)

- [ ] macOS and Xcode updated (this Mac: macOS 14.4, Xcode 15.3 — uploads need a
      current SDK); the engine and Kana rebuilt with it and tried on the iPad.
- [x] Writing with a finger: the toolbar's hand (mobile only), saved in the
      settings. One finger writes (a short wait tells it from a two-finger pan;
      a long press is still the menu), two move the page. On for a new install
      (no Pencil used yet); the first Pencil stroke turns it off, and the hand
      turns it back on. No palm rejection while it is on. [ ] On the iPad.
- [x] The camera's prompt in the five languages; the app declares them
      (CFBundleLocalizations, platform/ios/localized/*.lproj): the store lists them.
- [x] Privacy policy and support pages, five languages: tools/store/site.py →
      site/; CONTACT rde.apps.support@gmail.com. [ ] site/ on GitHub Pages, the
      two URLs in App Store Connect.
- [x] The store texts in five languages, the age-rating answers, the review
      notes: docs/store/listing.md.
- [x] The screenshot framer: tools/store/screenshots.py (iPad shots → 13-inch,
      captioned, per language). [ ] The eight shots taken on the iPad.
- [ ] The App Store Connect record; its Apple ID into KANA_APP_STORE_ID. Age
      13+ (Apple's nearest to the 12+ chosen); price suggested USD 9.99 once,
      with the Small Business Program.

## 10g. The architecture (Borja, 2026-10-02: "this will help on future Chinese + Korean versions")

The layers, the screen interface, the widgets, the language layer: docs/architecture.md.

- [x] Screens behind one interface (screen.h) in one order (app.h): kana.c's three
      if-chains (input, update, render) became one table. Each screen declares its
      rows of buttons as data (row.h) with faces for what they show now; toolbar.c's
      ~90 callbacks and ten "what is shown" caches went into the screens or the row.
- [x] Widgets out of toolbar.c (3,725 lines → 697, the floating bar alone): kit.c,
      row.c, pagemenu.c, filterbar.c (generic: Browse declares its chips), ui.c
      (the canvas, the fonts, rebuilding); header.c and kit's modal from code that
      was repeated across screens.
- [x] kana.c (2,566 lines → 384) split into app.c, page.c (writing, the hand),
      session.c (saves, files out), look.c (launch flags, --perf, probes).
- [x] Pure logic out of screens: the kana tables and romaji (romaji.c, from the
      chart and the catalog), a vocabulary list from characters (vocab.c, from the
      toolbar), the reviews due (app.c, from the side panel).
- [x] Fixed on the way: after a language change the Vocabulary screen and word
      exams had no row of buttons (the rebuild forgot them); a notice raised on a
      screen was never shown there; with the welcome over Practice, the pointer
      went to Practice; Translate with Google's chosen icon stayed Regular.
- [x] Checked: 39 screens shot before and after: of the 30 that don't animate
      or pick at random, 29 identical, 1 changed on purpose (Translate's icon); 13 pressed sequences (--press); 24 test suites; iOS release builds.
- [x] Folders by layer (Borja: "the correct hierarchy of directories"): src/app,
      screens, widgets, ink, handwriting, study, chars, base, services, lang/ja;
      each header beside its source, included by folder ("widgets/row.h"), one
      include path (-I<project>/src). The four build lists in COMMANDS.txt follow.
- [x] The tests in the repository: tests/run.sh, the 24 suites, one set of engine
      stand-ins (tests/support/; 17 copies before). The UI strings' source
      (tools/text/strings.py) and the icons' bearings (tools/icons/bearings.py)
      too — both lived only in a scratch folder.
- [ ] Windows and Android built from the new lists (delete build/ first, once:
      the builder links every object left in obj/).
- [ ] On the iPad: every screen's row, the side panel, a language change.
- [ ] When the second app starts: the language layer (lang.h) — docs/architecture.md.

## 10h. The shared code, untied from Kana (Borja, 2026-10-02: "extract all the common things ... for a Mandarin Chinese App, and a Korean App"; "extract the canvas free drawing, side panel and toolbar widgets to make the infinite diagram + drawing app")

One repository; the shared code is fude/ (筆, kana_ → fude_), the apps apps/.
docs/architecture.md has how it fits together and how to start a new app.

- [x] fude/drawing builds alone: nothing in it includes the study layer, Japanese
      or Kana. What it asks of an app is two structs: info.h (name, version, store
      id, backup extension, script font, credits, Licences) and extension.h (its
      toolbar tools, its rows of the page's menus, its side panel section and
      Settings sections, the tutorial, hooks for its widgets, what it draws over
      the page, its files and settings, its first launch).
- [x] fude/study/app: fude_study (starts with fude_app), the verbs
      (fude_study_view...), Select mode's row, the camera tool, Settings'
      Handwriting, the study files and practice sheets, its launch flags;
      FUDE_STUDY_EXTENSION for a study app's extension. The page's reading (Copy as
      text, Translate and its card, Save word, Check, Paste text, Text from a
      photo) left pagemenu.c for study/widgets/pagetext.c; the word card's state
      left the UI for the study.
- [x] Kana's own in apps/kana/src/kana_app.c: kana_app (the study, the chart, the
      welcome), the screens' order (KANA_SCREEN_), the side panel's Study section,
      its info, its flags (--kana, --welcome).
- [x] The strings in layers: fude/drawing/strings.py, fude/study/strings.py, the
      app's tools/strings.py (tools/strings/build.py writes them); the core's
      words name no app, Kana keeps its own for six (o()).
- [x] apps/draw: Draw, the drawing core alone (25 sources and a shell), on the
      Mac: the page, the toolbar, the canvases, Settings, Your data. Its own save
      folder (fude_save_set_folder); Kana's stays kana/.
- [x] Checked after every step: the Mac build, the 24 suites, 39 screens and 13
      pressed sequences against the shots before (37 that don't vary: identical
      every time), the very first launch, the iOS release build.
- [ ] On the iPad: the camera on the toolbar, Copy as text and Translate on a
      selection, Paste text and Text from a photo on a long press, Rate, Your data.
- [ ] The engine's iOS debug library is older than rde_pen_listen_double_tap and
      rde_mobile_open_review_page: an iOS debug build needs `./builder --engine
      --ios --debug` first (the release library has them).
- [ ] Draw on a device: its own bundle id, icon, Info.plist; its build lists.
- [x] The app's name in one place (src/version.h: KANA_NAME), the rest from it: the
      window's title, the backup's name and extension, the save folder on a device
      (the id, info.h), and the strings' {APP} (filled in by the strings tool).
- [x] The iOS Simulator (Borja: "lets add the new flag"): RDE's builder takes
      --ios_simulator (its own build folders, ad hoc signing, no profile) and rde.h
      defines RDE_PLATFORM_IOS_SIMULATOR; ML Kit's code is a device's only (the
      stand-ins in the Simulator); RDE's SDL3 lets the Simulator's GPU through its
      Metal check. Kana runs on the iPad Pro 12.9-inch Simulator at 2048 x 2732, and
      --stay runs a look without quitting, for its screenshots (COMMANDS.txt).
- [ ] The store's screenshots from the Simulator: data to show (the test saves are
      thin), and the shots that need a hand or ML Kit (2, 3, 6: listing.md).

## 11. Release polish

See `docs/app_store.md` for privacy, licences and permissions.


- [x] A release (optimised, non-debug) build configuration, with performance
      measured in it (2026-10-01): the builder's --release (-O3), the engine
      built --release for iOS. On the iPad mini 6, launched on each screen
      with --perf=8 (COMMANDS.txt): 59.9 fps everywhere, Kana's render 1.5 ms
      (Album) to 5.4 ms (Kana chart) a frame, update under 0.6 ms; two frames
      over 20 ms in all, as Statistics first draws.
- [ ] Optional: strip the release executable (47 MB → 35 MB, a smaller
      download) — the builder would also need to keep a dSYM for crash reports.
- [ ] **The app icon**, and a launch screen. Done, to see on the iPad: Borja's
      sketch made into the "Hinomaru" icon (platform/ios/AppIcon.svg: 日本語 in
      KanjiVG's strokes, white, before a red sun on indigo, over an open book);
      the launch screen the icon, centred, on Paper's page colour, Night's in
      dark mode (Borja: it should follow the system); an old launch screen stays
      cached until the iPad restarts; Info.plist: iPad
      only, every orientation, full screen, ITSAppUsesNonExemptEncryption false.
      The builder's new --ios_assets / --ios_launch_storyboard / --ios_plist.
- [x] Nothing developer-only in a release (2026-10-01, Borja): the diagnostics
      HUD always starts hidden and is no longer a setting (its Settings row is
      gone, a saved "on" is ignored); H (HUD) and M (raw samples) work in debug
      builds only; a release build ignores every launch argument — main hands
      RDE only the program's name — unless built with -DKANA_ALLOW_ARGS (for
      --perf). Checked on the iPad: a release launched with --stats --perf=3
      opened as usual and measured nothing.
- [x] The first theme follows the device (2026-10-01, Borja): with no settings
      saved yet, Kana starts in Night when the device is in dark mode and Paper
      otherwise — as the launch screen does — and saves the settings at once;
      from then on the theme is the learner's (Settings › Theme).
- [x] Check what ships in the bundle (2026-10-01): no `.bak`/`.tmp`; the builder
      no longer copies Finder's `.DS_Store` into the iOS assets, nor the
      template's Icon.png / Default.png when the project brings its own icon
      and launch screen; the Phosphor fonts cut down to Kana's 82 icons (~940 KB
      less; COMMANDS.txt says how to cut them again). What remains: the
      character data (7.4 MB), Noto Sans JP (4.5 MB, any Japanese the learner
      types), the licences (ML Kit's notices are 1.5 MB).
- [ ] Steps towards TestFlight (a paid developer account is needed for it).
- [ ] Commit the data licence credits screen as done: Settings > About (in place).

## 12. Engine: window screenshots fail on the Mac (last)

- [x] Fixed 2026-10-01 (RDE: rde_rendering.c, rde_window.c, rde.c). Two faults:
      the deferred screenshot ran AFTER the frame's submit and then re-acquired
      the swapchain on the submitted (NULLed) command buffer — the FATAL
      "'command_buffer' is invalid"; and it read the swapchain, which SDL GPU
      documents as write-only (Metal refuses: "Copy From Texture Validation").
      Now the frame a screenshot is asked for is drawn into a readable texture
      (as debug resolution does), downloaded, blitted to the swapchain so it
      still shows, submitted with a fence and handed over — single-threaded and
      threaded rendering both. A shot asked for mid-frame waits for the next
      frame. Debug resolution on keeps the old read-after path. Checked on macOS
      under Metal validation, both modes, from on_update, on_render and the
      bytes callback; to check on Windows and Android (other projects use it).
- [x] Kana's desktop `--shot=FILE` is back, with `--scroll=PX`, `--album-exams`,
      `--album-page=HEX` and `--kept-exam=N`: Claude now checks screens itself.

---

## Done (for the record)

The canvas toolbar minimizes: a double tap on its grip folds it to the grip
(still movable) and opens it again; saved in the settings.

Ink with Apple Pencil at full rate. Undo and redo. Saving. Lasso with cut, copy,
paste and duplicate. Themes. The data bake (KanjiVG, KANJIDIC2, JLPT). The
viewer. Browse with filter, sort, search, draw-to-search and search by parts. The
kana chart. Practice with scoring, tuned on real attempts. Practice sets. The
album. The side panel with nested folders of canvases and drag and drop.
Settings. The zoom indicator.
