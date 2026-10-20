# Kana's tests

```sh
tests/run.sh               # every suite
tests/run.sh vocab exam    # just those
```

Each suite is a small C program: its `test.c` and the app's own sources it tests, linked against **RDE's real
library** (the engine's Mac or Linux debug build), with test doubles in `support/` only for what a test cannot use
for real: a window, a GPU, a font, a moving clock. It is built with clang, with AddressSanitizer and UBSan on, and
run against the real character data (`apps/kana/assets/data/characters.kana`, its first argument). It prints
`ALL PASSED` last, or how many checks failed and where (`FAIL line N: the check`).

A suite that crashes, or that a sanitizer stops, prints its backtrace and ends at once with its own exit code
(`support/headless.c`), instead of waiting for macOS's crash reporter.

- **Needs:** macOS or Linux, clang, and RDE beside the project (`../RDE`) or
  where `RDE=` says. On Windows, run it from WSL.
- **RDE:** its header as it is (it tells the platform from the compiler's own defines, so it needs no config file),
  and its desktop debug library
  (`RDE/build/osx-silicon/debug/engine/lib/libRDE.dylib` on a Mac; `RDE_LIB=` to use another). `run.sh` builds it
  first if it is missing. The engine is never started: `support/headless.c` sets up its allocator (`rde_cook_init`)
  before `main`.
- **Where things go:** the builds, and the files the suites write (saves,
  sheets), go to `tests/build/`, one folder per suite (ignored by git).

## The suites

| Suite | What it checks |
|---|---|
| `kanji` | the character data: records, strokes, readings, meanings, words, sentences, look-alikes; a damaged file's counts refused |
| `catalog` | Browse's index: filters, sorts, search by meaning, reading and romaji; a damaged marks file's backup read |
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
| `review` | the reviews: the schedule, new ones a day, saved and read back, a damaged file's backup read |
| `vocab` | the vocabulary: words, lists (as many as are made: a hundred saved and read back), old files read, a list from characters |
| `wordexam` | word exams: a box a character, marked by the matcher, given characters, reviews |
| `wordsplit` | the words found in Japanese text, conjugated ones included; words spelt alike offered the commonest first |
| `zh` | Chinese (`fude/lang/zh`, built as Hanzi): pinyin typed with tone numbers or without, of any length, and what does not fit; readings compared; levels and groups |
| `ko` | Korean (`fude/lang/ko`, built as Hangul): romanization typed back into Hangul, a text past 255 bytes refused whole, what does not fit; Latin names, levels and groups; a syllable written by hand read without a rare character's cost |
| `scan` | Text from a photo: lines kept, rotation, translations, word taps |
| `sheet` | practice sheets as PDF: every page's structure checked |
| `text` | the strings: every language has every id; templates render |
| `draw` | the wrapped layouts and balanced widths draw.c keeps: each asked again, kept or made again, as first made; lines within their width, every word in them |
| `zoom` | the deep-zoom canvas: the codec exact, the index against brute force, 20 levels deep and back, the erasers, undo, moves, shapes and hold to snap, pictures, smoothing (drawn live the same as smoothed whole), flights (exact, in range, no frames made), the empty screen's marks, home, fills (triangulation areas, clipping, the file, the eraser cutting them, a figure 8's lobe), the close-up eraser (exact cuts, an "o" keeps its middle), instruments (docking, drawing along an edge, several of a kind, each compass its own radius, put away by its ×, a dozen at most, a ruler made longer by its tab, the circle and ellipse templates, the French curve, chained edges turning at their crossings), layers' ranks (what is drawn over what), texts, layers (hidden, locked, through the file) and moving things onto one, connectors following moves, driving (what meets a dimension's end follows), boards and the cut list, cutting boards (splits, slots, notches, holes, cuts snapped onto edges, hand-drawn loops, the eraser), snapping onto crossings, fitting parts onto a board (kerf, grain, what does not fit, the list of parts typed), paths (smooth through their nodes, corners) and Sculpt, the paint bucket (gaps, leaks, islands), offsets, the SVG, PDF and DXF exports (the SVG's opacities, texts' and notes' too), the lasso's copies in the order drawn and its moves followed in every frame, A4 templates with a printer's correction, the file reopened and cut at every byte, views far from 0 (a graph's ticks, dashes, papers, hatches, the lasso's box), a board cut into thousands of pieces, LaTeX nested past reason, numbers no type holds (attributes, ids, offsets, hex typed, a number line's), Make part's ports named alike, a circle's size following its circle, a dimension's and a connector's ids in halves (an older file's read), a connector pasted joined to the copies, Duplicate leaving the clipboard as it was, papers' rings joined exactly, a damaged font's glyph made of itself, a custom part's 70 pins, a piece renamed when its file cannot be written, the topics' one kind-to-part map (circuits, mechanisms, plans), the renderer's caches (shapes' and cut boards' triangles kept, covering what those made each frame cover; the Library's tiles from kept symbols; notes' layouts; pictures found by their payload; frames' order and the backdrop) |
| `units` | lengths and angles typed (fractions, feet-inches, arithmetic, variables) and written back, "≈" when rounded |
| `pdf` | the vector PDF writer: its structure, text, pictures (JPEG passed through, PNG with alpha) |
| `dxf` | the DXF R12 writer for cutting: layers, units, entities, the pre-flight |
| `graph` | Mermaid flowcharts read (node shapes, edges, subgraphs, errors) and laid out (stable layers) |
| `crash` | crash reports: a report left by the last session, the session marker, a real crash in a child process writing its report, a death with no report, the email it goes in |
| `spice` | the circuit simulator (`zoom/circuit.c` and its `circuit_*.c` modules) against ngspice on the same circuits and models |
| `mech` | mechanisms' physics: gears, racks, pulleys, ropes run as Play runs them, against exact ratios and kept energy |
| `sim` | the simulation core (`fude/sim`): definitions, nested custom parts (one of 70 ports kept as text and read back), flattening, the digital engine's gates, adders, flip-flops and buses, a loop that never rests stopped while its clock ticks on, thousands of nets and primitives found by name |
| `geometry` | plane geometry from RDE: its point in polygon, distance to a segment and area answer, bit for bit, as the copies the core and the canvas had did (corners, edges, flat edges, figure eights, segments of no length) |
| `docwork` | the PDF worker shared by the documents and the canvas's PDF view: a piece drawn and taken, turned and darkened, thrown away when stale, freed with its PDF when orphaned, failed, its tiles |
| `kit` | the form kit the canvas's cards are built on: its keypad types as the value and limits cards' own keypads did, key for key |
| `redraw` | render on demand's rule (`drawing/base/redraw.h`): the grace period after a change, a still screen never drawn again, timers kept until due, RDE's UI and other threads waking it, continuous runs; the developer's check (its reference, probes and pixel comparison); the scroller's, the canvas's and a notice's reports; and 20000 random frames against the rule |

## `support/`

| File | Is |
|---|---|
| `headless.c` | the engine set up without starting it (its allocator), and a crash ending the suite at once with its backtrace |
| `engine.c` | the doubles for what has no use in a test: a clock that stands still, drawing that does nothing, pictures that decode to nothing |
| `rich.c` | text measuring and the UI's shapes (no font in a test) |
| `text.c` | RDE's localization, reading Kana's real `apps/kana/assets/text/strings.rdel` (a suite built as another app reads that app's) |
| `app.c` | the study's verbs (`study/app/study.h`) and a row's faces (a screen's buttons call them; nothing presses them here) |
| `zoomdraw.c` | the canvas's drawing, for the suites that build the canvas's code without it: text widths for the instruments' numbers, the lasso's drawing as nothing (the zoom suite builds the renderer itself) |
| `physics.c` | RDE's 2D physics (its Box2D wrapper) built into a suite, with what of the engine it uses (`mech`) |
| `parts.h` | custom parts as a person builds them on the canvas (latches, registers, a memory, counters, an ALU...), shared by `sim` and `zoom` |

Every function in `engine.c` is weak. A suite that needs its own version (a
clock it moves, a file that fails to open) defines it in its `test.c`, and that
one is used — RDE's real one otherwise.

## A new suite

1. Make `tests/<name>/test.c`. Its `main` takes the character data's path as
   its first argument and prints `ALL PASSED` last.
2. Add a row to `SUITES` in `run.sh`: `name | its flags | its sources`, with
   paths from the project root (includes by layer: `drawing/...`, `study/...`).
   A suite is built as Kana: its strings (`apps/kana/src/text_ids.h`, its `strings.rdel`) and its character data.
   `@CANVAS` in its flags builds it with a canvas app's strings; `@APP=<app>` builds it as that study app (its
   strings and its character data), for a suite of another language (`fude/lang/<code>`: `zh`, `ko`).
3. Link only what it tests, plus the doubles it needs. (`run.sh` links `support/headless.c` first in every suite
   itself: start-up code runs in link order on macOS, and the engine's allocator must be ready before a suite's own.)
