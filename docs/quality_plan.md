# Code quality plan: getting every app to certifiable production code

Started 2026-10-12. The rules are in `docs/coding_rules.md`. This document tracks the work that brings the existing
code up to those rules, in the order it is done. Each item is ticked when it is done and verified: its tests pass,
and its screens match the before screenshots.

## Where the code starts (audit of 2026-10-12)

About 170,000 lines in `fude/` and `apps/`, plus 62,000 lines of tests. All 33 test suites pass.

| Rule | Found |
|---|---|
| C library | 7,048 calls in 199 files (131 functions), of which 3,922 are maths. RDE has no scalar maths at all. |
| Containers | About 64 hand-written ones: 7 hash tables, 3 binary heaps, 5 union-finds, 5 LRU caches, round-robin caches, 68 `qsort` calls, about 60 raw `malloc` calls. |
| Duplication | Six study-app shells that are near-identical (about 2,500 lines). Five copies of the chart screen and six of the word splitter in `fude/lang` (about 2,900 lines). About 20 copies of one hash function. Point-in-polygon written 9 times. 61 groups of same-bodied functions in all. |
| Layers | `zoom` includes `study` (ML Kit). `study` uses text ids only the apps define. App identity is hard-coded in `zoom` (`product.c`, log prefixes, "Sketching" in STL exports). The five folders of `drawing` include each other in a cycle. |
| Size | `zoom/page.c` is 25,918 lines (855 functions). `circuit.c` is 5,553 and `graph.c` 3,093. 14 functions exceed 300 lines. |
| Documentation | Every header has an overview. 85% of public functions have a comment, but only 37% of those returning pointers say who owns them, and threads are almost never described. `page.c` has no design note at all. The newer comments use a private shorthand. `architecture.md` predates the canvas apps. |
| Performance | Junction dots are recomputed every frame in every topic. The text caches thrash. Shapes are triangulated every frame. The index is double-inserted and never pruned. Mechanism trails are drawn one segment at a time. Handwriting reads selections at 2.2 ms per character. |

**Bugs the audit found** (fixed first, in phase 1):
- Shapes with more than 800 points are not drawn (`FUDE_ZOOM_CLIP_MAX`).
- User data is silently capped at 64: Library documents, turned pages, imported files, map areas, sheet exports
  and "My pieces".
- The word splitters' hash tables match on hash and length only, so one of two colliding words can never be found.
  The formula and rich-text caches trust the hash alone, so a collision draws the wrong layout.
- Two copies of the FNV offset basis in `render.c` are missing a digit.
- RDE: `rde_strtok` returns nothing, `rde_strncat` ignores its count, `rde_aligned_alloc`'s parameters are swapped
  between declaration and definition, and `rde_file_get_realpath` dereferences NULL. RDE's own crash handler is not
  async-signal-safe.

## Order of work, and why

1. **Safety net before any change.** Screenshots of every app's screens before the cleanup, compared after every step.
   A rules checker whose counts can only go down.
2. **RDE first**, because every later step needs its new functions: scalar maths, character classes, string and
   conversion functions, sort and search, condition variables, a heap, a disjoint set, and the crash handler.
3. **The study apps and their layers** (`drawing`, `study`, `lang`, the six apps), finished completely: structure,
   then RDE everywhere, then documentation. They are closest to release, and you can review them while the canvas is
   being done.
4. **The canvas** (`zoom`, `sim`, the canvas apps), in the same three passes. `page.c` is split before anything else
   there, because every other step is easier on small files.
5. **Performance**, measured before and after.
6. **Final architecture documents and the rules checker at zero.**

Changes to RDE are made in `~/RDE` with RDE unit tests, and are left for you to commit, as before. Every RDE change
stays source-compatible with your other RDE projects: new names are added, and existing signatures are not broken.

---

## Phase 0: safety net
- [x] 0.1 `tools/check/shots.sh`: captures every app's main screens with the look harness on the Mac (study apps:
      every screen, Settings, a written page; canvas apps: home, notebooks, examples, topics, panels). A second run
      compares pixels against the stored baseline.
- [x] 0.2 `tools/check/rules.py`: counts what `coding_rules.md` forbids, per file and in total. Its output is kept in
      `docs/quality_counts.txt` so progress can be seen.
- [x] 0.6 `tools/check/warnings.sh`: every compiler warning, which the RDE builder hides. 76 at first.
- [x] 0.3 `tools/check/build_all.sh`: builds every app for the Mac (and with `--android`, every release APK), so a
      broken app is noticed at once.
- [x] 0.4 Baseline: 33/33 suites pass (2026-10-12).
- [x] 0.5 One list of every app's sources (`tools/build/sources.sh`), read by the Mac build (`tools/build/mac.sh`, new)
      and the Android one, made by rule instead of hand-kept lists, so a new file reaches every build.

Notes from Phase 0:
- The canvas apps' screens also depended on where the real mouse was: the canvas draws its brush under the
  pointer. While a screenshot is taken, the look harness now owns the pointer (`fude_look_owns_pointer`). Every
  screen of every app now repeats byte for byte.
- `fude/zoom/page.c` and `fude/zoom/render.c` are compiled by no test suite: they are checked only through
  screenshots. Phase 3 adds suites for both (render: with counting doubles of the drawing calls).
- The first two captures of every screen were identical, except the study apps' viewer and practice screens, whose
  stroke-order animations follow real time. The app's own clock (`fude/drawing/base/clock.h`) fixes them: it follows
  the frames during a screenshot.
- The saved counts are in `docs/quality_counts.txt`: C library 7,037 calls and 555 headers, 32 layer violations, 17 app
  names in `fude`, 9 files and 56 functions over the size limits, and 363 undocumented public functions.

## Phase 1: RDE, and the bugs
- [x] 1.1 Printf-style functions get format checking (`-Wformat` on 840 call sites). (rde.h holds declarations only:
      every RDE function, these included, is implemented in the engine's source files. A call into the engine's
      library cannot be inlined into the app, so a hot loop pays a function call per maths or memory call; Phase 4
      measures whether that shows.)
- [x] 1.2 Scalar maths, f32 and f64: sqrt, trigonometry, exp/log, floor/ceil/round, fmod, hypot, pow, ldexp,
      min/max with `fmin`/`fmax` semantics (NaN ignored), clamp, is_finite/is_nan, NaN and infinity constants.
      `RDE_VEC*_LEN` stops needing `<math.h>`.
- [x] 1.3 Character classes, and string functions: strchr, memchr, strpbrk, strspn, strcspn, strnlen, strncasecmp.
      Number parsing with base and end pointer: `rde_strtol`, `rde_strtoul`, `rde_strtod`, `rde_strtof`. Also
      `rde_sscanf`.
- [x] 1.4 `rde_sort` and `rde_bsearch` for plain arrays, and standard comparators.
- [x] 1.5 Containers: `rde_heap` (a binary heap) and `rde_disjoint_set` (union-find, smallest index wins).
- [x] 1.6 Threads and the OS: condition variables, the module path (dladdr), the working folder, `rde_date_now_unix_seconds`,
      and writing formatted text to a file.
- [x] 1.7 Crash reports are an RDE facility (2026-10-13): `rde_crash_install` and `rde_crash_note` (`rde_crash.c`),
      fude's async-signal-safe handler moved into the engine as it was (write() alone, text made at start, the stack
      walked by frame pointers, the unwinder's tables on Android), installed over the engine's own handler, which
      still runs after it once the report is on disk. `fude/drawing/base/crash.c` keeps the app's side (where the
      report goes, its header, the running marker, what the last session left, the email) through RDE's files and
      dates: no C library is left in fude (rules.py: libc-header 0, libc-call 0). New with it: every `rde_thread`
      gets a stack of its own for the handler as it starts, so a stack overflow on a worker (the document drawer,
      the speech reader) is reported too; it was lost before. RDE's tests crash child processes for real: an abort
      on the main thread (the header, what, where, the last 24 notes, a note cut before a whole character) and a
      stack overflow on a worker. With the worker's stack taken away, that test fails (checked), and a child the
      report misses is given 20 s and then counted as a failure, so a regression cannot hang the run. The engine's
      own handler (calloc, popen) staying behind ours is the remaining step: replacing it changes what every RDE
      program's crash does, which is the engine owner's call.
- [x] 1.8 2D geometry in `rde_math` (f64): point-in-polygon, point-to-segment distance, polygon area.
- [x] 1.9 The RDE bugs listed above, and two more the new tests found: `rde_memory_allocator_get_default()` returned
      NULL without a pool, and `rde_file_free_read_text`/`_bytes` were declared but never written.
- [ ] 1.10 The app bugs listed above. Done: the 800-point shapes (the renderer's buffers now grow); the word splitters'
      hash collisions (2.2); the text widths' cache (2.5); the 64-item caps of the Library (the learner's documents:
      every one listed now) and of import (every file picked, on Android and everywhere); the page's 64 turned pages
      (2026-10-13: the turns are the canvas's own list, `fude_page.turns`, with a revision the session and the
      document watch instead of copying the page twice; a test turns 302 pages and reads them back). Left: `zoom`'s hash-keyed caches and the FNV constant
      (3.5).
      Found by the documentation pass (2.7) and fixed, 2026-10-13, each with a test where one can show it:
      - Saved work: an exam closed on its results before every answer was read was lost (now read by the matcher at
        once and kept); a file recovered from its .bak is put back as the main file (`fude_save_load`), so the exam
        log, which is only added to, no longer starts again without the recovered exams; the practice history reads
        through `fude_save_load` (it had no .bak at all); a damaged canvas list falls back to its .bak, then is
        rebuilt from the canvas files (it used to start empty and give new canvases the ids of old ones); a canvas
        file missing after a kill mid-compaction is read from its .bak (it opened empty); the reviews file is written
        once a sitting (100 writes for a 100-character exam before), and the vocabulary once when adding characters.
      - Crashes and overruns: a 32-point circuit wire wrote two numbers past its buffer (`FUDE_ZOOM_WIRE_NUMBERS`);
        Statistics and practice sheets with no character data (NULL); a PDF with no pages; a maths font's composite
        glyphs read past a damaged file; Find's excerpt room wrapped round with a small buffer; the UI's and filter
        bar's row limits are now checked at start instead of silently skipped.
      - Wrong behaviour: an undone erase after a document page turn came back unturned; Arabic and Thai scanned pages
        on iOS stayed "reading" for good; "Copied" never showed on the PDF text row; the Review count went stale
        after a search; the filter bar's toggles never followed their availability; the choice card kept old icons;
        mirrored walls, guides, angles and radii kept their orientation; circuit values ignored a minus sign; a tap on
        a part that cannot change still cut the circuit's steps; the scope had two different defaults; the
        transcript's prompt ran phrases together; a downloader cancel nobody called raced a new download (removed);
        a listener shut down with a piece waiting refused every later one; voice recording left the microphone on
        when out of memory; the bake miscounted examples when two were dropped (no shipped data changed: checked).
      - App identity: practice sheets and STL exports said "Kana" and "Sketching" in every app; they name the app.
      Found by the `.c` comment pass (2.7) and fixed, 2026-10-13:
      - Saved work: the exam log read through `fude_save_load` before each add, too (a damaged file no longer gives way
        to a new one holding only the newest exam); the marks and the reviews take a damaged chunk for a damaged file,
        so their backup is read instead of reading on without it; a practice square scribbled past 256 strokes is
        saved with its first 256, not saved whole and then dropped with its session every time it was read; switching
        canvases no longer resets the marker's colour and size and the eraser's size (`fude_ink_clear_page`, which
        keeps every setting in one place); vocabulary ids from a damaged file never start at 0.
      - Crashes and overruns: an exam prompt's word over about 95 bytes was written past its buffer; a document
        could start a second drawing job on a PDF, or close the PDF under one, after a page turn or a theme change
        (one job at a time now, a turn applied between jobs); the handwriting reader read past what it wrote for a
        piece of more than 64 strokes; a damaged character file could ask for any size of memory.
      - Data: two of Hanzi's shipped meanings were cut in the middle of a character (the bake cut bytes, now whole
        characters; rebaked: those two meanings, nothing else); the UTF-8 decoder swallowed the letters after a
        character cut short; a string argument's digits were turned into the language's ("Notes 2" in Arabic digits).
      - Wrong behaviour: Hindi, Thai and Arabic could not translate on iOS; another ML Kit model's download could mark
        handwriting ready or failed; an ML Kit reading that never answered blocked handwriting until restart (given up
        after 10 s now, its late answer dropped); a page that failed to draw stopped a document's reading; a PDF
        written on Apple's devices always reported success; a single C cleared the page, and every letter shortcut
        worked while typing in a field (undo and redo are Command or Control + Z now, and nothing acts while
        typing); importing several PDFs kept only the first, silently (each is a canvas now); the review counts' and
        the document bar's caches could miss a change (their numbers are compared whole); Practice's removed squares
        kept their old scores; the side panel hid folders more than 32 deep; the must-read card cut texts over
        4,096 bytes; the backup left out any folder called "outbox" at any depth; the reviews and the marked
        characters were looked at only to the first 4,096 or 16,384; notices faded by the engine's clock, so a
        screenshot mid-fade differed from run to run; the word card told users to turn ML Kit on when it was on.
      - Kana's name gone from shared code: the Objective-C classes and queues are Fude's; English in shared code
        ("%s backup", "Document", "Pictures") is translated. The strings tool now stops when a study app's language
        lacks a string, rather than building it with Japanese in its place (it did once, unseen until the screenshots).
      Found by the architecture write-up and fixed, 2026-10-13 (tests added):
      - Removing a canvas left its recordings, their transcripts, its pages and instruments, and its read text's
        backup on the device for good, and in every backup after. Every file of a canvas is `<id>.<something>` in the
        notes folder, and all of them go now, found by that name, so the drawing layer names none of a canvas app's
        files and a file a page kind adds later is never left behind.
      - A file damaged twice lost its first damaged copy (the second was renamed over it): the second is
        `<name>.2.bad` now, and on.
      - Every backup took in the downloaded speech model (60 MB): Your data's (read whole into memory on import) and
        the device's own (Google's keeps at most 25 MB of an app, so InfiNote's would have stopped). It lives in the
        platform's cache now (`fude_save_downloads_dir`: Android's cache, iOS's Library/Caches through RDE's new
        `rde_ios_get_cache_data_path`), which no backup takes in and the system may empty (it is then downloaded
        again); a model already on the device is moved there, not downloaded again. Your data's backup also left
        this device's crash report in (a restore would ask about it on another device): it is out, and an import
        leaves it where it is.
      Also fixed, 2026-10-13: a save is synced to the disk before its renames (a power cut just after them could
      leave the new name on a file whose bytes never reached the disk, the good copy already the .bak; the canvas's
      journal did this already); a document page whose picture could not be drawn was saved as read with no lines
      and never read again (it is tried again at the next opening now), and the read lines' file is read as every
      saved file is (its backup, a damaged one set aside); crash reports' notes say what the app was doing (the
      screen on top, a canvas switched, Your data's export or import, leaving and coming back); Draw, the core
      alone, is built by `build_all.sh` (it was built by no check, so the core could stop building on its own
      unseen); Thai's and Arabic's tutorials named the photo button as it is not ("Desde Fotos", "Das Fotos").
      The bug lists of the documentation passes, drawing and study, 2026-10-13 (checked: 25 suites with new tests,
      every study app and Draw built, every study screen as it was):
      - Limits on what a learner makes, gone: the vocabulary's lists (64: a 65th could not be made, and lists past
        64 in a file were dropped when it was read), the viewer's rows, an exam's source, Check's points, the word
        exam's undo, the side panel's About text, the notes index's 4,096, the page's 64 turns. Those kept have a
        comment saying why they are safe.
      - Input cut while saying all was well: pinyin over 16 letters, Korean syllables that did not fit, romaji and
        Korean over 255 bytes (they say they could not, now); a vocabulary search cut mid-character; Write in Text
        from a photo dropping lines past 16 KB.
      - Wrong behaviour: a tap on the selection threw away redo; the lām-alif counted as two letters by one part of
        the word exam and one box by the other; "Good" started at 75, not the 80 the header and the theme say; a
        damaged drawing in a kept exam left strokes behind; Search folded case in ASCII only (now full-width forms,
        accents and the ideographic space, as the PDF's own search); a one-page document's search repeated its
        matches; a failed add on the word card said the word was empty, a list that could not be made said nothing,
        a sheet saved on a computer did not say where; the side panel built its list in O(n²).
      - Code: four sets of statics into their screens' structs, shadowed names, a comparison that read a struct as
        two numbers, the languages' hand-counted tables counted by sizeof, pdf.h's contract made true (a worker may
        close a PDF it was drawing when the page let it go).
      Compiler warnings, 2026-10-13: 75 to 0 (`tools/check/warnings.sh`, which now checks Objective-C with ARC, as
      the builder compiles it, and third-party headers as system ones). Two were real bugs: the canvas's size card
      cut its area, perimeter and length to 7 characters (`FUDE_TEXTF` took the size of a pointer: every other use
      checked at compile time), and the canvas shell's demo jitter modified its seed twice in one expression
      (undefined behaviour; the same numbers now, one step a statement). Dead functions deleted, unused variables
      and parameters gone, every table initialised in full. Every write of an `rde_arr`'s length by hand (about 30,
      found by compiling against a copy of `rde.h` with the field renamed) goes through `rde_arr_resize`, `_clear` or
      `_remove`.
      Duplication removed, 2026-10-13: the exams', the word exams' and the demo's three hand-written random
      generators are RDE's `rde_random` (the demo keeps a fixed seed: still the same six weeks every run, but not
      the same as before, so Kana's store screenshots are made again with it); the exam and Statistics screens' two
      copies of the language's sets, and three screens' level-name buffers, are the catalog's
      (`fude_catalog_set_count`, `_set_group`, `_level_label`); the word exam's pass mark is the exams'. A
      vocabulary reading over 95 bytes is cut before a whole character (it was cut inside one); two lists' lengths
      are changed through RDE's array calls rather than written.
- [x] 1.11 The tests link the real RDE library, not hand-written copies of `rde_arr` and the allocators.

## Phase 2: study apps and their layers
- [x] 2.1 One shared study-app shell (2026-10-12). `fude/study/app/shell.c` is what RDE calls and what a study app
      owns (the page, the character data, every study screen, their order: `FUDE_STUDY_SCREEN_`); an app is its
      description (`fude_study_shell_def`: info, extension with its side panel and lectures) and a `main()` that hands
      it over. Six shells, six `src/<app>_app.c` and six `_app.h` (3,932 lines) became the shell and six app files of
      about 80 lines (1,009 lines in all). The chart, the welcome and the Library are the study's (`fude_study`), the
      welcome's two hooks are in `FUDE_STUDY_EXTENSION`, the apps' launch flags are the study's (`--kana` is
      `--chart` now, for every language), the bake's two calls are `lang.h`'s (the six `lang/<code>/bake.h` are
      gone), logs carry the app's name from its description, and `-DFUDE_ALLOW_ARGS` replaces each app's own.
      Still to do: the canvas apps' shell (3.3), and the RDE glue both shells copy, into `fude/drawing/app`.
- [x] 2.2 `fude/lang`: one chart screen and one dictionary word splitter in `fude/study` (on `rde_hash_map`). Each
      language keeps only its data and its true differences. One bake wrapper, and default `lang.h` implementations.
      - [x] The chart (2026-10-12). One screen, `study/screens/chart.c`, draws any language's chart from a description
        the language gives (`lang.h`: `fude_lang_chart`). The description holds its sections, its rows (a table, or a
        function where they are computed: Korean's syllable table), how a narrow screen fits them (cells shrink, rows
        wrap, or letters flow on), and whether gaps keep their place, a section mirrors right to left, or a block is
        listed down its columns (the gojūon). Five copies of about 380 lines each (with headers, 2,400 lines) became
        one screen and five data files (903 lines in all). Every chart is as it was, except Kana's two, which differ
        by at most 1 of 255 in a few anti-aliased pixels (positions computed in another order). All languages now fit a
        long Latin name to its cell, as Thai, Hindi and Arabic did.
      - [x] The word splitter (2026-10-12). One splitter, `study/chars/wordsplit.c`, finds a line's words in the
        dictionary for every language; a language says how (`lang.h`: `fude_lang_words`): which letters a word is
        looked up for, what a character is compared as (Arabic: without its vowel marks), where a word can end (Thai,
        Hindi: never inside a syllable), how many words spelt alike to offer (Korean: one), or a matcher of its own
        for a language that inflects (Japanese's conjugations, Korean's verb stems). The index is an `rde_hash_map`
        keyed by the whole written form, so words that only share a hash are no longer taken for one another (the
        old tables matched on hash and length: Chinese 賬簿 could never be found). Six copies of the index and the
        loop (1,762 lines, with the old header 1,797) became one splitter and six rule files (755 lines). `lang/wordsplit.h` is gone: a
        language's one interface is `lang.h`. Checked against the old splitters, rebuilt and run side by side over
        each language's whole dictionary and its example sentences (695,000 lines): every line is split as before,
        but for the five Chinese lines with 賬簿. Speed with the release engine: a line takes 1 to 3 µs (Japanese 2.3×
        faster than before, the others up to 1.6× slower); the index is built once, in 1 to 8 ms.
        Also from this: `fude_utf8_is_han` (seven copies of the Han ranges), `fude_utf8_count`, `fude_utf8_put`
        returns its length, and `fude/lang/ko/hangul.h` (four copies of the syllable arithmetic).
      - [x] Default `lang.h` implementations: considered, and kept explicit. What several languages share is the
        plain case in a line (`fude_lang_rtl` is false, `fude_lang_letter` is the letter itself); a table of defaults
        would hide which languages differ, and each `lang.c` shows everything its language does.
      - [x] One bake wrapper (2026-10-12). Chinese, Korean, Thai, Hindi and Arabic each wrote the same run (read the
        strokes and the prepared tables, write, add the look-alikes); each is now a description
        (`study/chars/bake.h`: `fude_bake_prepared_def`) that `fude_bake_run_prepared` runs. The `--bake` check and
        the phone's stand-ins are written once (`study/chars/bake.c`), not seven times. Japanese keeps its own readers.
        Checked: Hindi rebaked through the shared run is byte for byte the file the app ships.
- [x] 2.3 Strings (2026-10-12). The 17 welcome and lecture rows every study app repeated are the study's
      (`fude/study/strings.py`), each language's translation of them in the study's `strings_<code>.py`; where an
      app words one differently it says so with `o()` and why (Kana is feminine in Spanish and French; Thai and
      Arabic scan pages, which ML Kit cannot read). `python3 tools/strings/build.py <app>|all` makes every app's
      strings from one table of what each is built from (`APPS`), so a layer's change is one command for every app;
      an app's `tools/strings.py` holds only its own rows (the canvas apps and Draw have none), and a language says
      what it is as a fourth language itself (`language()`). Checked: every app's ids and texts, in every language,
      are what they were.
- [x] 2.4 Layers inside `drawing` (2026-10-12): base < ink < doc < widgets < app, with no include going up (31 did).
      - `draw` and `icons` are `base`'s: they draw with RDE and the theme, and the ink draws with them.
      - `save` was three things at three levels: the save folder stays in `base`; a page's file is `ink/pagefile`; the
        settings' file is `app/settings`. The legacy-colour rule is the theme's (`fude_theme_saved_color`), and so is
        `fude_theme_shade` (it was the kit's).
      - The app's own UI pieces, filed in `widgets` but each including the app, are `app`'s: the toolbar, the side
        panel, the page menu, the document bar, the crash and must-read cards; so are the Library screen and import
        (from `doc`), which make canvases in the app.
      - `doc` no longer knows the app: each frame the page hands it what it shows (`fude_doc_place`: the file under
        the open canvas, the canvas, the window, the reading hooks), and what was the app's — a canvas's document's
        path, new canvases over one, the lectures opened — is `app/documents`.
      Left: `zoom` includes `study` for ML Kit (3.4).
- [ ] 2.5 Shared helpers in `drawing/base`:
      - one hash: RDE's (`rde_hash_*`, wyhash) for every in-memory key of `drawing` and `study` (done 2026-10-12:
        the formatted texts, the page text's selection); the backup's checksum stays FNV-1a, its file format's. The
        measured widths' cache is an `rde_hash_map` keyed by font and whole text (it matched on hash and length:
        two texts could share a width). The wrapped-lines and balanced-width caches matched on RDE's hash and
        the length only: each is an `rde_lru` keyed by the whole text, its font and sizes now (3.5, 2026-10-14). `zoom`'s keys go with 3.5;
      - UTF-8 count (`fude_utf8_count`) and cut (`fude_utf8_copy`, `fude_utf8_set`: three copies in the study's
        models and the word card) — done; `zoom`'s eleven hand-written cuts go with 3.6;
      - the Han ranges (done: `fude_utf8_is_han`); the bake's own UTF-8 decoder (`fude_bake_decode_utf8`);
      - little-endian u64/f64 in `kfile` (done: they were the canvas's `scene.c`'s; five files wrote and read the two
        halves by hand, and the practice history had its own pair);
      - one loader for every saved file (done 2026-10-12: `save.h`, `fude_save_load`): the main file, else its
        backup, also when the main one is damaged — before, only a page's file fell back; the marks, exams,
        reviews, words, notes and settings set a damaged file aside and started empty, the day before still in the
        backup. Seven copies of the choosing and checking became one; each kind keeps only its reader;
      - the save subfolder (done: `fude_save_subdir`, three copies);
      - a picked file's name and type (done: `fude_import_name`, `fude_import_is_pdf`, two copies; a PDF is found in
        any case now, `.Pdf` too);
      - the kit's field text (done: `fude_kit_field_text`, six copies: the word card's and five canvas forms').
      Every copy is removed.
- [x] 2.6 C library to RDE, done for every layer at once (2026-10-12), since RDE's replacements behave
      identically and every screen and suite checks the result:
      - 6,747 calls renamed by a script that touches only calls, not comments or strings;
      - 505 now-unused includes removed;
      - the rest converted by hand: dates (to `rde_date_*`), files (to `rde_file_*` and the new `kfile` helpers),
        allocation (as `coding_rules.md` §3 says), `strtok`/`strcat`/`strncpy`, and the speech reader's threads.
      What is left: the crash handler (RDE port, 1.7), and the bake loaders and word splitters (rebuilt in 2.2).
      Totals: C library calls 7,037 → 117, headers 555 → 43, with every screen and every suite unchanged.
      Still to do: the `draw.c` caches become RDE maps (2.5).
- [ ] 2.7 Documentation of every module in these layers to `coding_rules.md` §5: ownership, threads, decisions, and
      comments in plain English.
      Under way (2026-10-12): every header of `drawing`, `study` and `lang` rewritten to §5 (an overview of each
      module; a comment over every declaration; plain English instead of shorthand), checked by stripping the
      comments and comparing the code with what it was. Found while documenting `lang`, and fixed: Thai's
      `fude_th_level` was dead code; Chinese's group prompt answered for groups it does not have; Japanese named
      levels past N1 ("N6"); the docs said only Thai lacks a text recognition model (Arabic too). `.c` files next.
      Headers done (2026-10-13): all 165 headers of `drawing`, `study`, `lang`, `zoom` and `sim` (rules.py's
      `undocumented` is 0), each checked by stripping the comments and comparing the code with what it was. The
      wrong comments found were corrected (threads, formats, units, limits, functions that do not exist), and the
      bugs found in the code were fixed: see 1.10. Left: the `.c` files' comments, still largely in shorthand.
- [x] 2.10 Japanese knowledge out of the shared study layer, into `lang/ja` behind `lang.h`. Done (2026-10-13): Check's
      "capitals are katakana" is `fude_lang_reading_capital`; the word exam's kana ranges are the language's own
      groups (`fude_lang_group`); the `japanese` fields of the sentences, the vocabulary and the translator are
      `taught`. Then (2026-10-14; the scan and check suites to show nothing changed): `study/handwriting/segment.c`'s
      reading model for free writing is the language's, `lang.h`'s `fude_lang_writing_def`, as the word splitters'
      rules are (`fude_lang_words_def`): the look-alikes across kana and kanji and how the neighbours choose, the costs
      of switching script, the small kana, the voicing marks and their size, 、。・, the kana out of use and which
      characters say where a line is are `lang/ja/lang.c`'s; the other languages have none of these rules. Checked: the scan and check suites pass with their expectations untouched, so Kana reads exactly as before.
- [x] 2.8 Logs (2026-10-12): a line says which part of the code speaks (`vocab:`, `save:`, `canvas:`, `look:` for
      the developer's harness), never an app's name ("kana:" showed in Hanzi's logs, "sketching:" in InfiNote's) nor
      the library's ("fude:", 20 lines that said nothing). The shells' own lines carry the app's name from its
      description.

- [x] 2.9 Builds (2026-10-12): a call to an undeclared function is an error (`-Werror=implicit-function-declaration`
      in the Mac's, iOS's and Android's builds): C would guess its type, and RDE's builder shows warnings only when a
      build fails. Every app builds for iOS from the shared source lists (`tools/build/ios.sh`). The tests too
      (2026-10-15): every suite includes what it calls, so `tests/run.sh` no longer passes
      `-Wno-implicit-function-declaration` and a missing include fails a suite's build as it fails an app's.

## Phase 3: the canvas apps and their layers
- [x] 3.1 `page.c` split by domain into modules of a sensible size (2026-10-13). `page.c` (25,878 lines) is now the
      coordinator (457 lines: the page kind's hooks, init and destroy, undo and redo) and 29 `page_<domain>.c` modules
      of 237 to 1,565 lines (input, frame, render, file, pen, shapes, text, find, measure, circuit, mech, diagrams,
      audio, speech, settings, the look harness...), with `page_parts.h` holding what they share and a map of which
      module holds what. Checked: every one of the 852 functions, 169 defines and 51 tables appears once with its text
      unchanged; 840 of 850 functions compile to the same machine code (the other 10 reach the page through the GOT);
      the three canvas apps build with the same warnings as before; every suite passes; every screen is as it was.
      Left: 11 functions over 150 lines (`render_circuit` 465, `ui_update` 448), part of 3.2.
      Done after it, in the new modules: the canvas's keys are the drawing core's (Command or Control + Z, Shift for
      redo, no letter clears the canvas, none acts while a text field has the keyboard: Backspace while writing a
      text deleted the selection); its perf log names the code, not the app; the Mermaid hint's words are translated;
      Settings' frame rate is a developer's build's only; and Settings' Handwriting (Google ML Kit's switch) is the
      drawing core's (`drawing/app/handwriting.c`), listed by the study apps and now the canvas apps too, its switch
      saved with the core's settings (each layer says in its own words what off means), and To text hidden while it
      is off. The canvas sets its handwriting model (the language it is shown in) at launch, so Settings' Download
      gets the right one.
- [x] 3.2 `circuit.c` split (build, draw, solve), and every function over 300 lines broken up (2026-10-14).
      `circuit.c` (6,049 lines) is a 199-line coordinator and ten modules of 234 to 993 lines (catalogue, read,
      draw, wires, build, blocks, stamp, solve, step, play), sharing `circuit_parts.h` with its map of which module
      holds what. Checked by a script: every function, define, table and type moved once with its text unchanged
      (it caught each of a set of deliberately broken copies). Then its five longest functions (536, 568, 434, 244
      and 195 lines) were broken into named steps, the same statements in the same order: the longest circuit
      function is 131 lines now.
- [x] 3.3 The apps own their description. `zoom` asks for capabilities (audio, topics, welcome), not product
      identity. Each canvas app gets its own small shell, and the developer harness moves to `fude/zoom/look.c`.
      Done (2026-10-13): `zoom` no longer asks which app it is (rules.py's `app-identity` is 0): what General and
      Maths are called and whether sound is recorded are each app's description (`product.h`), and
      `fude_zoom_product_which` is gone. The practice sheets and STL files name the app they come from.
      Done (2026-10-14): the canvas shell is the library's (`zoom/shell.c`, taking a `fude_zoom_shell_def`, as the
      study's does), each canvas app is its own small file (`apps/<app>/<app>.c`, about 50 lines: its licences,
      its info, its description and a `main()`; `SKETCHING_PRODUCT` is gone), and the developer's flags are
      `zoom/look.c` (its two longest functions split into readers and per-frame steps, in their old order). Every
      app's licences are a table with a count taken by `sizeof` (`FUDE_APP_LICENCES_OF`), so a document added
      cannot leave the count behind. Every screen of the three apps is as it was.
- [x] 3.4 The ML Kit ink bridge moves to `drawing`, so `zoom` no longer includes `study` (2026-10-13). It is
      `drawing/ink/mlkit.*` now (it reads ink), and the app tells it which model to use (`fude_mlkit_set_model`):
      the study shell the language taught (`fude_lang_ink_model`), the canvas the language it is shown in. The
      canvas's stand-in definition of `fude_lang_ink_model` is gone, and so are the canvas builds' extra source
      lines. rules.py's `layer` is 0.
- [x] 3.5 Containers: the heaps, union-finds, LRU caches, pools, hash tables, laser trail and linear-scan sets
      become RDE containers or one shared helper each. Every cache compares its key.
      Done (2026-10-12): every cache compares its key — the renderer's graphs, text layouts and symbols, and the
      formulas (now keyed by the formula itself); the handwriting lines keep a 64-bit hash of their sorted strokes
      as their key, a 1-in-2^64 chance accepted and documented. The Mermaid reader's id table is an `rde_hash_map`.
      The mistyped FNV constant is gone with the hand-written hashes (RDE's everywhere but the backup's format).
      The R-tree's nearest search and the circuit solver's ordering use `rde_heap` (the same sift rules: the same
      order of visits); the handwriting search's lines use `rde_disjoint_set` (the same rule: the smaller names a
      group). DXF's open-contour count uses `rde_disjoint_set` (only how many groups have a loose end is returned,
      so which piece names a group does not matter), and the Japanese bake's word table is an `rde_hash_map` (the
      bake's output byte for byte as before, 2026-10-13). Left: the circuit's three union-finds (with 3.2, whose
      functions they live in), logic's (its groups are named by the last joined, not the smallest: checked against
      the tests and pictures when changed), the LRU caches, pools, the laser trail and the linear-scan sets.
      Done (2026-10-14, waiting for the suites and the screens): the circuit's three union-finds (the nets and the
      places in `circuit_build.c`, the blocks' in `circuit_blocks.c`) and `sim/elab.c`'s are `rde_disjoint_set`: the
      same rule (the smallest names a group) and the same path halving, so every node, vertex, block and flat net is
      numbered as before. Logic's drawing (`logic.c`, `fzlg_groups`) keeps its own names over the disjoint set's
      (a group is named by the net it was last joined into): the names decide the order its wires are placed in, so
      they matter, and a test pins that order. RDE has an `rde_lru` now (any bytes as a key, compared whole; slots the
      caller's values are indexed by): the graphs' lines, the home screen's covers, the notebook strip's pictures, the
      strings' filled-in templates (their key unambiguous now: a string holding the old separator was read as two
      arguments), and `draw.c`'s wrapped layouts and balanced widths (which matched on a hash and the length) use it.
      The laser's trail and the translator's demo queue are `rde_ring_buffer`s, the payloads' free slots an
      `rde_stack`. Linear scans in hot paths: `fude_zoom_symbol_find` is a map; the handwriting's readings are a map
      by line key; the vocabulary finds a word by its texts through a map kept in step; the lasso's "already picked"
      is a hash set (no longer O(n²)); a mechanism's trail is an array per tracer. Tests: `zoom`, `sim`, `scan`,
      `text`, `vocab`, a new `draw` suite, RDE's `lru` and `stack`. RDE's library must be rebuilt for the suites.
      Left: the decoded strokes' and the pictures' caches (budgets in bytes over an object-indexed table: no scan
      when found, an LRU by stamp when over budget), `wordsplit.c`'s chain of words spelt alike (an index-linked list),
      the fixed slot pools by design (fingers, instruments: their slot order is observable), and the scans run per
      edit rather than per frame (the bug log's "Left by the containers work").
- [x] 3.6 Shared helpers:
      - geometry from RDE (point-in-polygon ×9, segment distance, area);
      - one PDF tile worker shared by `drawing/doc` and `zoom/pdfview`;
      - one form kit for `zoom/*form.c`;
      - one kind-to-part map for circuits, mechanisms and plans.
      Done (2026-10-14), to be checked (builds, suites, screens):
      - Geometry: the canvas's points (`fude_zoom_v2`) and the bodies' (`fude_sim_v2`) are RDE's `rde_vec_2D`, and every
        copy calls `rde_math`'s: point in polygon (lasso, selection, render's cover, symbol art, eraser, instruments,
        fills, bodies), distance to a segment (eraser, fills, shapes, bucket, render, circuit's wires, junctions and
        leads, page's wires, guides, lines and nodes, snapping, mechanisms, ink), area (fills). Every expression is the
        copy's own, so the answers are bit for bit theirs (the `geometry` suite: the copies kept as references, checked on
        random shapes, grids, corners and edges); `fude_zoom_cut_area` (summed from the first edge: a flat loop's sign rounds otherwise) is the canvas's one
        shoelace (STL's rings and the clip's winding too); the bodies' area (about the first corner) and the measure's
        (an open ring's) are their own.
      - `drawing/doc/worker.{h,c}`: one PDF worker, a piece at a time, stale and orphaned as before (`docwork` suite).
      - `kit.h`'s form (`fude_kit_form`): the card, title, Cancel, a tap off it, laying out again, labels, fields, lists,
        chips, a keypad's typing; the ten cards keep what they ask (`kit` suite: the keypads type as before).
      - `symbol.c`'s one table (`fude_zoom_symbol_topic_part`) for the three topics' `_of_kind`, one draw function a
        topic for its 194 symbols, and `fude_zoom_plan_opening` for a room's doors and windows (`zoom` suite).
- [x] 3.7 C library to RDE in `zoom`, `sim` and the canvas apps: done with 2.6, for every layer at once (0 calls left there).
      The data bake too (2026-10-13): RDE's arrays and map and `fude_file_read` instead of malloc, fopen and a
      hand-made hash table; its UTF-8 decoder and kanji test are `utf8.h`'s. Every language's data baked before and
      after, byte for byte the same. What is left of the C library is the crash handler's (1.7).
- [x] 3.8 Documentation, as 2.7 (2026-10-13). Every `.c` of `zoom` and `sim` (120 files, 86,000 lines): a file block
      (how it is laid out, its prefix, its decisions), a comment over every function, constant and table, step
      comments in long functions, stale and misplaced comments fixed (many "page.c" ones after the split). Checked
      file by file: with comments stripped, the code is what it was. The readers found about 100 things wrong, all
      fixed the same day in four lists, each with a test where a suite can reach it:
      - Memory and data: the canvas's file lost everything appended after one failed write (disk full); custom
        parts of more than 64 ports or 8 parameters read past their arrays; a PDF label of about 490 characters
        wrote past its buffer; a damaged maths font could read past its bytes; a damaged transcript aborted the app;
        a broken crank's joint was used after it was freed; To text could delete strokes in another canvas, read at
        the old canvas's places; the PDF view could start a second drawer on the same PDF after a theme change; a
        failed cut of a fill erased it whole; renaming a template could lose it; a clock carried over a rebuild
        during Play fired every edge since 0 at once.
      - Hangs: a graph's ticks, a dashed line, the papers and a hatch at far-out numbers; the board cutter's table
        when it filled; formulas nested very deep (stack overflow), in both the reader and the typesetter; a port
        name's uniquing.
      - Behaviour: a switch named "SW1" or "contact" started closed, "infrared" read as red, "ccw" turned
        clockwise, "AC 20V" read amperes; gear trains driven from the second gear; the style of a board lost when
        cut, sawn or erased, and a cut board edited back into a rectangle; "2x Side 600 x 300" read as 300 parts;
        copies pasted in a different order on top; connectors left behind in deeper frames; a dimension came loose
        when its length was typed; SVG exports ignored transparency; English printed into PDFs and circuit
        notices; a full notebook said a page was added; deleting a wire was O(wires × objects × rounds).
      - Code: dead functions and fields removed, every module's prefix its own, one clip, one servo constant.
      Then (2026-10-14; checked: 35 suites, every app built, every screen as it was but for the date, 0 warnings):
      - The canvas's PDFs embed the app's own fonts, cut down to the characters used (about 20 KB a page), as
        Type 0 fonts with their Unicode map: Greek, Cyrillic, every Latin letter Roboto has, the Japanese the app's
        font has and its icons print as on screen, and stay selectable; before, anything outside Windows Latin-1
        printed "?". Bold and italic are Roboto's own faces.
      - The canvas's leftovers: the papers', the lasso's and the diagrams' loops counted by index (a far view hung
        them); a custom part's joins past 64 lost once saved; a damaged font's self-referring glyph hung the reader;
        To text's box kept on the canvas, not the screen; a dimension's links stored as ids (old files read); a
        re-pinned instrument no longer an undo step; one prefix a module, one clip, one file size helper.
      - The drawing and study leftovers: fude_ink_begin's unused eraser argument and the theme's eraser colour gone;
        the strings' test renderer nests braces as RDE's does; the Chinese and Korean code tested at last (two new
        suites, an app chooser in tests/run.sh); the viewer's widths cached; the Vocabulary's list chips scroll; text
        read from photos no longer cut at 256 lines; the notes' new names and repairs no longer quadratic; the zoom
        pill on the right side for right-to-left; every drawing and study function under 150 lines.

## Phase 4: performance (measured with `--perf` and `--gate`, numbers recorded here)

**How it is measured (2026-10-15).** (Since 5.5 the measuring build is each app's `measure` profile:
`zsh tools/build/app.sh <app> measure`.) Two ways. First, in the suites: each change's test runs the old code (kept in
the test as the reference) and the new on the same input, in one process, and checks they give the same answer;
those times are the ones to trust (sanitizer builds: the real ones are faster, the ratios hold). Second, whole apps:
Mac release builds with `-DFUDE_ALLOW_ARGS` (an M2 Mac mini, 60 Hz), each scenario from the same fresh saves, its
`--perf` line kept (`update` and `render` are the CPU's milliseconds a frame; every frame holds 60 fps on this Mac).
Whole-app numbers moved with the machine's state between morning and evening: Kana's screens, which no change but
the width cache touches, rendered about 1.5 times slower in the evening, and an A/B of that cache (the old and the new
built side by side, run in turn) showed no difference (Browse 1.56–1.70 ms, the exam 3.21–3.36 ms, either way). So
whole-app numbers are compared only within one sitting; the morning baseline is kept for the record.

| Run (whole app, morning baseline) | update avg / worst (ms) | render avg / worst (ms) |
|---|---|---|
| InfiniWorkshop, `--gate=1000000` (2,500 strokes, 4 depths), still | 0.12 / 0.36 | 3.01 / 22.52 |
| the same canvas opened again; its load | 0.11 / 0.31 | 2.84 / 21.17; loaded in 9.2 ms |
| Everything at once (`--example=63`), still | 0.16 / 7.83 | 1.18 / 10.21 |
| Everything at once, its circuits and mechanisms running | 1.15 / 12.18 | 1.25 / 8.77 |
| The diagram library open (`--library=0`) | 0.13 / 3.92 | 1.27 / 13.17 |
| Kana: Browse, chart, Album, Statistics, exam | 0.12–0.25 / 4.2–5.3 | 0.34–2.16 / 1.5–4.0 |
| Hanzi: Browse, chart, Album, Statistics, exam | 0.12–0.30 / 4.7–6.2 | 0.15–2.65 / 1.1–4.9 |

After the work, the gate canvas opens in 3.3–3.5 ms (9.2 before, the same file). Where a frame's time goes now
(macOS `sample`, the gate canvas still): about 65% of the render is drawing its strokes, most of it RDE tessellating
them (`rde_rendering_2d_draw_stroke`, about 55% of the render); handing the frame to the GPU is about 20%; the
frame's own bookkeeping (the query, the sort, the items) is a few per cent.

| Change, measured in its suite (old → new) | Time |
|---|---|
| Every Library tile's symbol, a frame | 11.94 → 0.095 ms |
| 500 filled shapes, a frame | 3.67 → 0.28 ms |
| 300 notes laid out over 50 frames | 69.7 → 21.6 ms |
| A canvas of 48,000 objects, 3,001 frames, 3,165 chunks opened (-O2) | 134 → 68 ms |
| A connector-heavy drag: 4,000 things, 2,132 moved | 106.0 → 14.8 ms |
| 10,000 audio links added | 63.2 → 4.3 ms |
| Wire currents, 2,000 steps (Everything at once) | 37.8 → 20.6 ms |
| Handwriting: every character ranked (-O2) | 4,322 → 2,342 ms |
| Handwriting: sentences read (日本語, 今日は, 水を飲む＋ねこ, 漢字; -O2) | 37.8, 2.7, 68.1, 39.8 → 29.6, 1.6, 38.3, 18.7 ms |

- [x] 4.1 Junction dots: skipped when there are no wires, and cached by revision and view (earlier). Its scratch
      (and the Wire tool's pin search) now kept in the page instead of four arrays made a call.
- [x] 4.2 Text caches that do not thrash (2026-10-15): the notes' layouts (`render_text.c`) and draw.c's exact widths
      are each an `rde_lru` keyed by every byte that decides them (fonts, their size, the size drawn at, the width,
      the alignment, the text), where the one used longest ago makes room instead of all being forgotten at once.
      Two keys lacked a part (the notes' fonts, the widths' font size) and could have given a stale answer: both in now.
- [x] 4.3 Shapes' and cut boards' outlines and triangles kept (2026-10-15), in one `rde_lru` with the symbols'
      geometry (`render.c`), keyed by the payload and its serial (an edit makes a new payload; a reused slot a new
      serial), drawn by moving the kept triangles as fills are; a shape far bigger than the screen is still clipped.
- [x] 4.4 Frame rendering (2026-10-15): no allocation a frame (`render.c`'s items, one stack for frames inside
      frames); every object in the index once (its box worked out first: shapes, texts and pictures were put in at
      {0,0,0,0} and again); dead and stale entries cleared when they pile up (`fude_zoom_scene_collect`), so queries
      are no longer sorted after every erase; `camera_child_at` walks the frame's children only; the cover test reads
      the decoded strokes.
- [x] 4.5 Circuits and mechanisms (2026-10-15): a tracer's trail drawn as one stroke per run near the screen, its path
      read in place (it was copied a frame and drawn as up to 40,000 separate lines: it now looks as every line on the
      canvas does, smooth, round-jointed, the same width and colour); belts and wire currents with kept scratch;
      probes and bodies drawn from a list of the canvas's attributes instead of a walk of every object. The stamps
      were measured and left: the order they are stamped in changes from one iteration to the next (settled blocks
      are skipped), so a replay would be wrong, and the search they would save is under 10% of the solve.
- [x] 4.6 Library panel: each tile's symbol drawn from the renderer's kept geometry (2026-10-15); symbols found
      through a map, `hand_update` and select-all were done earlier.
- [x] 4.7 Handwriting reading (2026-10-15): the references bucketed by stroke count and walked in their own order (the
      same candidates, costs and order, checked on 8,924 rankings), and a floor under the rough cost, with no square
      root, that turns away about 80% of candidates before their cost is worked out (its proof that it never turns away
      one that would be kept is in `match.c`). It is 1.8 times faster optimised; an unoptimised debug build is 2–3
      times slower than before (the floor's arithmetic costs more there than the square roots it saves): kept, since
      only optimised builds ship and an `#if` for debug would test other code than the apps run.
- [x] 4.8 Loading (2026-10-15): chunks found by a binary search on their offsets (always increasing: the file is read
      forward), objects appended to their orders and sorted once, the index built once, and the file's CRC worked
      eight bytes a step; a re-saved canvas is byte for byte what it was.
- [x] 4.9 Stroke meshes, measured (2026-10-15): tessellating strokes is about 55% of a stroke-heavy canvas's
      render. A mesh kept per stroke or per bucket would save it only while the view is still or panning
      (the fringe is a screen pixel wide, so a zoom makes it again), and needs a new RDE call (a stroke tessellated
      into a mesh, drawn with `rde_rendering_2d_draw_static_transformed`). The Mac draws the gate canvas in about 4 ms
      of 16.6; whether it pays depends on the tablet: measure the gate there first (COMMANDS.txt, "Measuring a build
      on the device"), and build it if its frames run over.
      Measured on the Galaxy Tab S6 Lite (2026-10-15; `android-measure`, a separate app with its own saves, started
      with `adb shell am start ... --esa args --gate=1000000,--perf=10`): drawing the gate, 31.5 fps, render 20.2 ms a
      frame; the same canvas opened again (loaded in 15.4 ms), 44.2 fps, render 17.8 ms avg, 23.2 ms worst, every
      frame over 20 ms. It does not hold 60 fps: the mesh cache is built (below).
      Built (2026-10-15; `docs/design/rendering.md`, "Strokes' meshes"). RDE splits `rde_rendering_2d_draw_stroke`
      into `rde_rendering_2d_tessellate_stroke` (a stroke into an `rde_stroke_mesh`: positions, and each vertex's share
      of the colour's alpha) and `rde_rendering_2d_draw_stroke_mesh` (a mesh appended to the 2D colour batch, moved and
      coloured: no draw call or GPU buffer per stroke, rather than `rde_rendering_2d_draw_static_transformed`'s one
      each); `rde_rendering_2d_draw_stroke` is the two in one, so the tessellation is written once. Its unit test
      checks the mesh against the old `rde_rendering_2d_draw_stroke`, kept in the test, on 3,000 random strokes: the
      same triangles, positions bit for bit, the same alphas. The renderer keeps each stroke's mesh with its decoded
      points (`render_stroke.c`), made in screen units round where its translation lands and moved there as it is
      drawn, keyed by the turn and scale to the screen, the half-widths' scale and a pixel's size, 64 MB at most
      (`FUDE_ZOOM_RENDER_MESHES`). A still or panning view, a moved stroke, a theme, a faint stroke or a glow makes
      none; a zoom makes each again, over its own arrays (no worse than before). Glows and the runs of a stroke far
      bigger than the screen are made each draw. The zoom suite's `test_render_kept_strokes` checks a kept mesh draws
      what one made in the draw does, bit for bit, and the old way's triangles within 3.1·10⁻⁵ of a pixel.
      Measured. The suite (sanitizers on), the gate's screen of 625 flowers (222,864 points: 941,477 vertices and
      4,159,854 indices a frame; meshes 26.7 MB): a frame tessellating every stroke 39.7 ms; from kept meshes 13.2 ms
      still, 13.2 ms panning; zooming 38.2 ms. The Mac (M2, `measure` builds of before and after run in turn in one
      sitting, the gate canvas opened again, `--perf=10`, three runs each): render 4.73, 4.70, 4.68 ms a frame before,
      3.63, 3.70, 3.76 after (21% less; worst 5.6–5.9 before, 5.1–6.8 after). `sample`: the strokes were 758 of the
      render's 1,179 samples (64%), now 339 of 812, nearly all of it copying the meshes into the batch (about 30 MB a
      frame); handing the frame to the GPU (four copies of the batch, two of them the index rewrite that sorts draw
      commands) is now the larger part, 385 samples. Pictures: every screen in `tools/check/shots.sh` is as it was but
      for the apps' version (0.5.4 to 0.5.5, not this change); 13 pictures full of strokes, before and after (deep
      flowers zoomed in, out, panned and at their edge, Night, the gate, smoothing, erasers, lasso, fill, snap, lines):
      6 the same, 7 with 2 to 17 pixels 1 of 255 apart. To measure on the tablet.
      On the Galaxy Tab S6 Lite after it (the same `android-measure` app, the gate canvas opened again, `--perf=10`,
      steady runs): 54.6 and 54.7 fps (44.2 before), a frame 17.6 ms (21.9), render 13.4 ms (17.8), 5 and 14 frames
      over 20 ms in 10 s (every one before). What is left is copying the meshes into the batch and the flush's index
      rewrite (RDE): the next step towards 60 on that tablet, if wanted.
- Also (2026-10-15): a hardening the review asked for — a failed read no longer leaves a scene skipping its index
      (`zfile.c`), and the constraints' passes check each line again (`connect.c`).

## Phase 5: the whole picture
- [x] 5.1 `docs/architecture.md` rewritten (2026-10-15). It covers:
      - every layer (`sim` and `android` included) and the module map;
      - how a study app and a canvas app run, from start through each frame to save;
      - saving and loading, the threads, and every cache with its limit and invalidation;
      - how to add a screen, an app, a language, a canvas tool, a topic, a symbol, a part or an example;
      - how the apps are built ("The builds": each app's project file, below).
      Every name it uses was checked against the code by a script (336 identifiers, every path).
- [x] 5.2 Design notes (2026-10-15), in `docs/design/` (its README is the index): `page_driver.md`, `rendering.md`
      (the renderer and every cache), `scene_and_index.md` (the scene, the R-tree's life, loading, the file as a log),
      `circuits.md`, `mechanisms.md` and `pdf_and_threads.md` (PDF in and out, and every thread in the apps).
      `infinite_canvas_design.md` stays as the history log; the current design lives in the notes. Writing them found
      about forty bugs and wrong comments, all fixed or decided (docs/bug_log.md, "What the design notes found").
- [x] 5.5 The apps built from project files (2026-10-15): RDE's builder was reworked to read an `.rdeproj` (an
      INI-like file holding the whole build, `~/RDE/docs/docs/project_files.md`). Each app has
      `apps/<app>/<app>.rdeproj` (profiles: debug, release, measure, android, bundle, ios, sim), and
      `tools/build/app.sh` does only what a project file cannot: the version from `version.h` (its one place), the
      canvas apps' staged assets, whisper.cpp when missing, iOS signing from the environment. The old scripts
      (`mac.sh`, `ios.sh`, `sources.sh`, `tools/android/build.sh`) are gone. Checked against them: the same sources for
      all 37 app and platform pairs (`builder show`), the APKs' manifests, files and signatures the same, the screens
      byte for byte the same. RDE's macOS/iOS test plan was run with Kana in Datebook's place
      (`~/RDE/docs/mac_ios_test_plan.md`: its results box by box); six fixes to the builder came out of it, for the
      owner to review.
- [x] 5.3 Stale documents brought up to date (2026-10-15): `tests/README.md` (already current: its 39 suites),
      `product_split.md` (what each app offers now, the code's division as it is, what was built since the plan),
      `simulation_architecture.md` (its status checked against the code, step by step), `apps/kana/docs/app_store.md`
      (pointing to `store/listing.md` and `docs/store_privacy_answers.md` instead of repeating them), and Kana's old
      `apps/kana/docs/TODO.md`, now a record: its open items carried into `docs/release_checklist.md` §2.
- [x] 5.4 `tools/check/rules.py` at zero (2026-10-14): every count is 0 — no C library, no void*, no layer crossed,
      no app named in shared code, no file over 2,000 lines, no function over 150, nothing undocumented. The last
      long files were split by domain (graph, symbol, render, instrument, scene, examples, export, after page and
      circuit), each with a private `<module>_parts.h` mapping which module holds what, and every long function was
      broken into named steps; scripts checked that every moved function kept its text and that each broken
      function, its steps put back, reads as it did, line for line and in order. Every screen is as it was.

---

## RDE changes, for you to review and commit (`~/RDE`)

Every change keeps existing code compiling. Each new function has a unit test, written in RDE's own format and
listed in the test app's `unit_test_scene.c`. All of them, with the containers', strings' and hashes' existing groups,
also run from the command line: `zsh tests/headless/run.sh` (new). 383 cases pass (2026-10-15).

**Added**
- `RDE_PRINTF_FORMAT`, which marks printf-style declarations for format checking.
- New `rde_memchr` and `rde_strchr`: SDL's, or the compiler's built-in where SDL has none (memchr), as RDE's other
  C library functions are.
- `rde_strnlen`, `rde_strpbrk`, `rde_strspn`, `rde_strcspn`, `rde_strcasecmp`, `rde_strncasecmp`, `rde_strncat_n`,
  `rde_sscanf`.
- `rde_strtol`, `rde_strtoul`, `rde_strtod`, `rde_strtof`, with base and end pointer.
- `rde_char_is_*` and `rde_char_to_lower`/`_upper`, for ASCII.
- Format checking on `rde_snprintf` and `rde_vsprintf`.
- Scalar maths, `rde_math_<fn>_f64` and `_f32` (`rde_math.c`: SDL's maths where SDL has the function, the
  compiler's built-in where it has none; the unit tests check every one against the C library bit for bit): sqrt, cbrt, hypot, pow, exp, exp2, log, log2, log10,
  ldexp, the trigonometric and hyperbolic functions, atan2, floor, ceil, round, trunc, round_to_i64, fmod, abs,
  min/max (`fmin`/`fmax` semantics), copysign, is_finite, is_nan and is_inf. Also `RDE_NAN_F64`/`_F32` and
  `RDE_INFINITY_F64`/`_F32`.
- `RDE_VEC*_LEN` no longer needs the caller to include `<math.h>`.
- `rde_sort` and `rde_bsearch` for plain arrays, and the `rde_sort_compare_*` orders: u32, i32, u64, i64, f32, f64
  (NaN last) and C strings.
- `rde_heap`, a priority queue.
- `rde_disjoint_set`, a union-find in which each group is named by its smallest element, so results are deterministic.
- `rde_cond_*`: condition variables.
- `rde_file_write_fmt`, `rde_file_get_working_dir`, `rde_file_get_module_path` (dladdr),
  `rde_date_now_unix_seconds` and `rde_date_local_components` (`localtime` in one call).
- `rde_math_remainder_*` (IEEE remainder) and `rde_math_round_even_to_i64_*` (`lrint`).
- `rde_memcpy`, `rde_memmove`, `rde_memcmp`, `rde_memchr` and `rde_bsearch` take `const any` for what they only
  read (`any` is a macro for `void*`, so that is a pointer to const data): a const buffer no longer warns.
- 2D geometry in f32 and `_f64`: `rde_math_point_in_polygon_2d`, `rde_math_polygon_signed_area_2d`,
  `rde_math_segment_closest_t_2d`, `rde_math_point_segment_distance_2d` and `_distance_sq_2d`.
- The C-library wrappers' and the containers' parameters are written `any`/`const any` throughout, never `void*`
  (kana's own code too: 544 places; `tools/check/rules.py` counts any that come back).
- `rde_crash_install` and `rde_crash_note` (`rde_crash.c`, 1.7), `rde_lru` (any bytes as a key, its slots made up
  front) and `rde_stack_is_inited` (3.5), and `rde_ios_get_cache_data_path` (a folder no backup takes in).
- `rde_hash_map_reserve`: grows a map once to the size it would reach for a number of entries, so filling it
  rehashes nothing (a dictionary of 85,000 words rehashed 11 times; its index now builds in half the time).

**Fixed**
- The Android manifest template (`external/templates/android/app/src/main/AndroidManifest.xml.in`): the camera
  feature is `android:required="false"`, so an app that uses the camera when there is one still installs on a
  tablet without one (Play hides it from those otherwise).
- Four format strings whose arguments did not match on Android (`u64` is `unsigned long` there, `%llu` wants
  `unsigned long long`; a `u32` given to `%zu`), found by the format checking above once the engine was built for
  Android: `rde_profiler.c` (the frame number and session id, cast) and `rde_error.c` (a stack frame's index, cast).
- `rde_thread_run`/`rde_thread_wait`: the thread freed its own handle when its function returned, so waiting on a
  thread that had already finished read freed memory. RDE's own `rde_thread_foreach` and file watcher did this.
  The handle now has two holders, the thread and its starter, and whichever lets go last frees it.
  `rde_thread_end`, which freed the handle of a possibly running thread, is now `rde_thread_detach`.
- `rde_date_to_local` used today's offset for any date, so a time from summer showed an hour off in winter. It now
  uses the offset in force at that instant, as `localtime` does.
- `rde_strtok` returned nothing; it now returns the token.
- `rde_aligned_alloc`'s declaration had its parameters swapped against its definition. The physics code, its only
  caller, used the definition's order, so the declaration now matches it.
- `rde_file_get_realpath` on POSIX:
  - it dereferenced NULL for a path that does not exist;
  - it leaked realpath's buffer;
  - it returned only the folder.
  It now returns the full path, as on Windows and as documented. The plugin VFS's "path does not exist yet" case
  used to crash here.
- `rde_memory_allocator_get_default()` returned NULL whenever the pool was not set up: before the engine starts, in
  `rde_cook_init` tools, or with the pool opted out. Memory freed through it then crashed. It now falls back to the
  std allocator, as a NULL allocator argument does.
- `RDE_UNIT_TEST_EXPECT_F32_EQ`/`_N_EQ` compared without the absolute value, so any result below the expected one
  passed.
- The docs of `rde_malloc`, `rde_calloc` and `rde_realloc` said they return NULL on failure; they assert.
- The base64 test called `memcmp`/`strlen` without including their header.
- The hash map's pool leak check is skipped when there is no pool (headless).
- The hash map's typed key functions (`rde_hash_map_fn_u64_hash`/`_cmp` and the others) read the key through a typed
  pointer, but a map stores its entries packed (state, key, value), so a u64 key sits at a 4-byte offset: undefined
  behaviour, caught by the sanitizer in a map of struct keys, and a fault on CPUs that trap unaligned loads. They now
  copy the key out (one unaligned load). `rde_hash_map_hash_fn`'s documentation says a custom function must too.

- `rde_file_read_full_file_bytes` (2026-10-15) read a byte at a time, took `ftell`'s -1 (a size it could not know) as
  a size, and handed back a short read as the whole file. It now reads in as few `fread` calls as the stream gives,
  and returns NULL (`rde_failed()` says why, its buffer freed) when the size cannot be known or the read falls short.
  Its test reads a 3 MB file back byte for byte, and a directory, read as a file, gives no bytes.
- The UTF-8 decoder (`_rde_text_engine_decode_utf8`, which the string hash `rde_hash_map_fn_str_hash`, the fonts and
  the bidi pass use) took the bytes after a leading byte without looking at them (2026-10-15). A name ending in a
  byte such as Latin-1's é (0xE9, which announces a three-byte character) had its terminator and what followed taken
  as the character's: the hash read past the string's end, and the same key in two buffers hashed differently, so a
  map of string keys lost it (the canvas's simulation found a net of that name twice). It now takes a leading byte
  as a character's only when the continuation bytes it announces are there, looked at one by one and never past a
  terminator; otherwise the byte reads as an invalid one already did (codepoint 0, one byte). Valid text decodes as
  before. Its test: such keys, in buffers that differ past the terminator, hash alike and are found.

**Uncommitted, 2026-10-15 (afternoon): for you to review**
- The builder on the Mac, found by running `docs/mac_ios_test_plan.md` (each also in its results there):
  `bs.h`'s `bs_snprintf` puts the format inside `...` (a call with nothing after the format left a trailing comma,
  which macOS's `snprintf` macro refuses: the self-recompile's 8 errors); `_rde_project_find_apple_identities` marks its
  unused size (`-Werror`); a macOS icon is made at 512 (sips will not write a 1024 `.icns`); a Mac package's zip and
  Info.plist use the version the build used (a `--version` was ignored there); a missing compiled shader stops with an
  error (it crashed in `fseek(NULL)`); a relative `-L` in `link_flags` is made absolute against the project file (the
  Android link runs in another folder).
- Strokes' meshes (4.9): `rde_rendering_2d_draw_stroke` split into `rde_rendering_2d_tessellate_stroke` (a stroke
  into an `rde_stroke_mesh`: positions and each vertex's share of the colour's alpha) and
  `rde_rendering_2d_draw_stroke_mesh` (a mesh appended to the 2D colour batch, moved and coloured), with
  `rde_stroke_mesh_new`/`_free` and `rde_rendering_2d_get_pixel_size`; `rde_rendering_2d_draw_stroke` is the two in
  one, its output bit for bit as before (`tests/unit_tests/test_stroke_mesh.c`: 3,000 random strokes against the old
  code, kept in the test). Android's `RDEActivity` passes the starting intent's "args" extra to `main()`
  (`getArguments`), so a build that reads arguments can be given them through `adb shell am start ... --esa args`.
- Comments made true: `rde_malloc`/`rde_calloc` (the engine's pool, through SDL, not the system heap) and
  `rde_physics_2d_world_create` (`_body_capacity` is Box2D's first reservation, not a cap).
- Not changed, for you to decide: `rde_memory_texture_gpu_upload` ignores `free_data`, so a texture's CPU copy can
  never be dropped after upload (the documents' tiles keep theirs); the builder applies no framework's module-map
  `link` lines (Google Mobile Ads 11 needs `-framework JavaScriptCore` by hand; 13 needs Xcode 16); `builder run`
  starts the app in the project's folder (its saves land there); assets' dot files (`.DS_Store`) are packed into APKs;
  on POSIX `bs_copy_file_if_exists` always reports success.

**Removed**
- `rde_file_free_read_text` and `rde_file_free_read_bytes`: declared but never implemented, and contradicting the
  read functions (whose caller frees the buffer).

**Known, not fixed (your call)**
- The builder (`rde_build.c`) links every `.o` in a project's `obj` folder (`find … -name "*.o"`), including those
  of source files that were removed or renamed, which are linked into the app as they were. `tools/build/mac.sh`
  works around it: it clears the folder when an app's list of sources changes. Android builds start from a clean
  folder anyway. The fix in the builder: link only the objects of the sources it was given, or delete the others
  before linking.
