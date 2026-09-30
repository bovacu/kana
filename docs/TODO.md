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
  fits: measured) and 2,048 slots (~30 MB of GPU memory). A glyph keeps its
  slot until the app quits, so a very long session could still run out; the
  engine would need to recycle slots for that.
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

- [ ] **Select** in Browse's row and the chart's: a tap ticks (or unticks) instead
      of opening; every cell shows a circle, a ticked one filled with a check
      and the cell tinted.
- [ ] The row while selecting: **All** (Browse: every filtered, sorted, searched
      result; the chart: the section in view), **None**, **Practice n** (the
      ticked as a set, in the order ticked), **Done** (taps open again; the ticks
      stay until None, or the app quits).
- [ ] One selection for both: kanji ticked in Browse and kana in the chart
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
- Open: exams in the album, and in the statistics (#8).

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
- Open: exams in the album; marks over time (when each became Known) is not
  kept — only the exams that made it so.

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

- [ ] A design pass: spacing, radii, a type scale, primary / secondary / toggle
      buttons, one panel style, across every screen. Icons (an icon font through
      Slug: sharp, theme-coloured, beside text in one label; Borja picks the
      set). Every UI string into a table, for translation.
      - Done, to check on the iPad (from the mock-up Borja approved): light
        surfaces and one accent per theme (theme.h: surface, surface_2,
        outline, accent, on_accent, tint); the kit's looks (plain, quiet,
        selected, primary, danger, chips) and Phosphor icons in buttons
        (toolbar_kit.h: kana_toolbar_icon); the bar icon-only with
        separators; every row an icon over its label; the side panel's rows
        with icons (字, あ, 試 as characters); outlined fields; the viewer's
        chips, 音 / 訓 / 部 written from their strokes, the words in a card;
        the exam's header, progress and prompt card; statistics' cards; the
        marks as icons (Studying an amber star, Known a green seal).
      - Still to do: the string table.

## 10. Other languages

- [ ] Meanings in other languages: KANJIDIC2 has Spanish for 2,505 kanji (all
      2,136 Jōyō), French 2,066, Portuguese 1,944; JMdict's full file has
      German, Russian, Dutch, French, Spanish, Hungarian, Swedish, Slovenian
      (coverage of the common words to measure; English where missing). The
      UI strings translated too. Spanish first.

## 11. Release polish

See `docs/app_store.md` for privacy, licences and permissions.


- [ ] A release (optimised, non-debug) build configuration, with performance
      measured in it.
- [ ] **The app icon**, and a launch screen.
- [ ] Check what ships in the bundle: no `.bak`/`.tmp`, only the assets needed.
- [ ] Steps towards TestFlight (a paid developer account is needed for it).
- [ ] Commit the data licence credits screen as done: Settings > About (in place).

## 12. Engine: window screenshots fail on the Mac (last)

- [ ] `rde_window_take_screenshot` and `rde_window_take_screenshot_bytes` die on
      macOS (Metal, single-threaded rendering) with FATAL ASSERT "Couldn't
      re-acquire the GPU swapchain texture after a screenshot ... 'Parameter
      'command_buffer' is invalid'" (`rde_window.c`, the re-acquire after the
      readback). Called from `on_update` and from `on_render`, same result: the
      readback seems to leave the frame's command buffer submitted, so
      re-acquiring on it is invalid. An ENGINE fix: other projects use the
      engine, so check them (and Windows, Android) after. When it works, a
      desktop `--shot=FILE` launch option lets screens be checked without the
      device (Kana has the other look options already: `--size=744x1133`,
      `--theme=`, `--side`, `--paper`, `--stats`, `--exam`, `--exam-start`).

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
