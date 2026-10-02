# Kana's tests

```sh
tests/run.sh               # every suite
tests/run.sh vocab exam    # just those
```

Each suite is a small C program: its `test.c`, the app's own sources it
tests, and the stand-ins in `support/` for what a test has no use for (a window,
a GPU, a font). It is built with clang, with AddressSanitizer and UBSan on, and
run against the real character data (`assets/data/characters.kana`, its first
argument). It prints `ALL PASSED` last, or how many checks failed and where
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
| `save` | the page and the settings files: round trip, older files, damaged ones |
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

## `support/`

| File | Stands in for |
|---|---|
| `engine.c` | RDE's files (through stdio), log, a clock that stands still, drawing that does nothing |
| `arr.c` | RDE's dynamic arrays and allocators, with the engine's own semantics and asserts |
| `rich.c` | text measuring and the UI's shapes (no font in a test) |
| `text.c` | RDE's localization, reading the real `assets/text/strings.rdel` |
| `app.c` | the app's verbs and a row's faces (a screen's buttons call them; nothing presses them here) |

Every function in `engine.c` is weak. A suite that needs its own version (a
clock it moves, a file that fails to open) defines it in its `test.c`, and that
one is used.

## A new suite

1. Make `tests/<name>/test.c`. Its `main` takes the character data's path as
   its first argument and prints `ALL PASSED` last.
2. Add a row to `SUITES` in `run.sh`: `name | its flags | its sources`, with
   paths from the project root.
3. Link only what it tests, plus the stand-ins it needs.
