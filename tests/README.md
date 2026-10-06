# Kana's tests

```sh
tests/run.sh               # every suite
tests/run.sh vocab exam    # just those
```

Each suite is a small C program: its `test.c`, the app's own sources it
tests, and the stand-ins in `support/` for what a test has no use for (a window,
a GPU, a font). It is built with clang, with AddressSanitizer and UBSan on, and
run against the real character data (`apps/kana/assets/data/characters.kana`,
its first argument). It prints `ALL PASSED` last, or how many checks failed and where
(`FAIL line N: the check`).

- **Needs:** macOS or Linux, clang, and RDE beside the project (`../RDE`) or
  where `RDE=` says. On Windows, run it from WSL.
- **RDE's header:** used as it is, with a desktop `rde_config.h` that `run.sh`
  writes. The engine's own config is whichever platform it was built for last.
- **Where things go:** the builds, and the files the suites write (saves,
  sheets), go to `tests/build/`, one folder per suite (ignored by git).

## The suites

| Suite | What it checks |
|---|---|
| `kanji` | the character data: records, strokes, readings, meanings, words, sentences, look-alikes |
| `catalog` | Browse's index: filters, sorts, search by meaning, reading and romaji |
| `parts` | Browse's search by parts, and Select mode's ticks |
| `match` | the shape matcher: a character drawn as a hand would, found among the candidates |
| `score` | a written character's score: shape, order, direction, missing and extra strokes |
| `check` | Check: a selection read character by character, "I meant…" |
| `textink` | text written in the characters' own strokes, then read back |
| `lasso` | the lasso: select, drag, copy, paste, delete, undo |
| `save` | the page and the settings files: round trip, older files, damaged ones (Sketching's units, true size, printer and saw kerf included) |
| `notes` | the canvases and folders: add, move, remove, saved and read back |
| `backup` | Your data: export, inspect, restore |
| `history` | Practice's history: sessions saved, read back, summed up |
| `set` | Practice over a set: Next, Finish, the summary, Weakest again |
| `guide` | guided Practice: its three steps |
| `exam` | exams: setup, preview, writing, grading, results, reviews |
| `album` | the album: its sorts, a character's page, kept exams |
| `stats` | Statistics: what is computed, and a character tapped |
| `review` | the reviews: the schedule, new ones a day, saved and read back |
| `vocab` | the vocabulary: words, lists, old files read, a list from characters |
| `wordexam` | word exams: a box a character, marked by the matcher, given characters, reviews |
| `wordsplit` | the words found in Japanese text, conjugated ones included |
| `scan` | Text from a photo: lines kept, rotation, translations, word taps |
| `sheet` | practice sheets as PDF: every page's structure checked |
| `text` | the strings: every language has every id; templates render |
| `zoom` | the deep-zoom canvas: the codec exact, the index against brute force, 20 levels deep and back, the erasers, undo, moves, shapes and hold to snap, pictures, smoothing (drawn live the same as smoothed whole), flights (exact, in range, no frames made), the empty screen's marks, home, fills (triangulation areas, clipping, the file, the eraser cutting them, a figure 8's lobe), the close-up eraser (exact cuts, an "o" keeps its middle), instruments (docking, drawing along an edge, several of a kind, each compass its own radius, put away by its ×, a dozen at most, a ruler made longer by its tab, the circle and ellipse templates, the French curve, chained edges turning at their crossings), layers' ranks (what is drawn over what), texts, layers (hidden, locked, through the file) and moving things onto one, connectors following moves, driving (what meets a dimension's end follows), boards and the cut list, cutting boards (splits, slots, notches, holes, cuts snapped onto edges, hand-drawn loops, the eraser), snapping onto crossings, fitting parts onto a board (kerf, grain, what does not fit), paths (smooth through their nodes, corners) and Sculpt, the paint bucket (gaps, leaks, islands), offsets, the SVG, PDF and DXF exports, A4 templates with a printer's correction, the file reopened and cut at every byte |
| `units` | lengths and angles typed (fractions, feet-inches, arithmetic, variables) and written back, "≈" when rounded |
| `pdf` | the vector PDF writer: its structure, text, pictures (JPEG passed through, PNG with alpha) |
| `dxf` | the DXF R12 writer for cutting: layers, units, entities, the pre-flight |
| `graph` | Mermaid flowcharts read (node shapes, edges, subgraphs, errors) and laid out (stable layers) |

## `support/`

| File | Stands in for |
|---|---|
| `engine.c` | RDE's files (through stdio), log, a clock that stands still, drawing that does nothing |
| `arr.c` | RDE's dynamic arrays and allocators, with the engine's own semantics and asserts |
| `rich.c` | text measuring and the UI's shapes (no font in a test) |
| `text.c` | RDE's localization, reading Kana's real `apps/kana/assets/text/strings.rdel` |
| `app.c` | the study's verbs (`study/app/study.h`) and a row's faces (a screen's buttons call them; nothing presses them here) |

Every function in `engine.c` is weak. A suite that needs its own version (a
clock it moves, a file that fails to open) defines it in its `test.c`, and that
one is used.

## A new suite

1. Make `tests/<name>/test.c`. Its `main` takes the character data's path as
   its first argument and prints `ALL PASSED` last.
2. Add a row to `SUITES` in `run.sh`: `name | its flags | its sources`, with
   paths from the project root (includes by layer: `drawing/...`, `study/...`).
3. Link only what it tests, plus the stand-ins it needs.
