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
      With the flush's index rewrite skipped where the draws are already in order (2026-10-16, RDE: "RDE changes" below),
      the same tablet, the same canvas, three runs: 59.7, 59.8, 59.8 fps (the display's 60), a frame 16.0 ms, render
      9.9, 9.1, 9.1 ms (13.4 with meshes alone, 17.8 before either), 2 to 4 frames over 20 ms in 10 s.
      The flush's index rewrite, skipped where it changes nothing (2026-10-16; RDE, `rde_rendering_2d.c`). At the end
      of each 2D frame RDE sorted the draw commands and copied every shader's indices into a scratch and back in that
      order, even when each command already started where the one before it ended, which a canvas of strokes always
      does (one command, 1.8 million indices at the Mac's window size): two copies of 7 MB a frame that left every byte
      where it was. One walk over the commands now finds the shaders whose commands are in place and leaves them be;
      the others are rewritten as before, so the GPU is given the same bytes and the same draws. Measured on the Mac
      (M2, `measure` builds of before and after, each on its own engine library, run in turn in one sitting,
      `--perf=10`, three runs each): the gate drawn, render 3.29, 3.37, 3.32 ms a frame before, 3.00, 2.99, 2.92
      after; the same canvas opened again, 3.22, 3.33, 3.40 before, 2.92, 2.94, 2.95 after (11% less; update 0.07–0.10
      ms before, 0.13–0.16 after, as it rose with the meshes too). `sample` (6 s, three of each, in turn): the canvas's
      flush was 354–381 samples of the render's 765–824, 195–198 of them the rewrite's two copies; now 227–242 of
      703–733, the upload's two copies and little else. Those copies and the meshes' copy into the batch take a little
      longer than they did (the upload's 148–170 samples are 204–223, the page's drawing 406–438 is 472–493: the cache
      no longer holds what the rewrite left in it, it seems), so the render saves about half of what the rewrite cost.
      Pictures: every screen as it was. The tablet's measuring APK is built
      (`build/infiniworkshop-android-measure/game.apk`), not installed: to measure there.
- [x] 4.10 RDE's rendering, reviewed and fixed (2026-10-16–19). The owner asked for a review of
      RDE's 2D, 3D and UI/text rendering: what is conceptually wrong, what wastes work, what could batch. It found 21
      correctness bugs and eleven kinds of waste, all to be fixed. Every change is in "RDE changes" below; the kana
      side is here.
      - **Owner's decision on text:** no shaping cache inside the engine. An app that draws text by string keeps the
        shaped text and draws it with `rde_rendering_2d_draw_shaped_text_2`, as `rde_ui` labels already do.
        `fude/drawing/base/draw.c` keeps its shapes (`fude_draw_shape_of`: an `rde_lru` of 1,024 shapes plus a
        spare, keyed by the font and the text's bytes; nothing drawn this frame is evicted; all freed at
        `fude_draw_forget`). Every fude text draw goes through it (`fude_draw_text`, `fude_draw_text_placed` for the
        25 placed draws in fude/zoom). Its test in tests/draw: a text drawn twice is shaped once, any byte or font
        reshapes, a 1,000-text screen over 30 frames is shaped once.
      - **Tiles drawn in two layers:** `fude_draw_tiles_begin`/`_end` put a grid's text one layer above its
        backgrounds, so the 2D batch grouped each. Used by Browse, the chart, the Album, the vocabulary and the
        translator. Taken out on 2026-10-19: RDE's 2D batch now gathers draws that do not overlap on its own, and
        every screen draws the same bytes with the same draw calls without them (Browse: 13 a frame).
      - **Measured, Kana's Browse (Mac, `measure` builds, before and after in turn):**
        - render 0.59–0.60 ms → 0.36–0.37 ms (the chart 0.80 → 0.45);
        - texts shaped a frame: 74 → 0;
        - per frame: draws 178 → 107, pipeline binds 176 → 105, scissors 177 → 106.

        Most of what is left is one draw per Slug text run, which phase 2 merges.
      - **Measured, the gate canvas (InfiniWorkshop `measure`, `--gate=1000000 --perf=10`):**
        - kept stroke meshes now stay on the GPU, so the bytes uploaded a frame went from 9.70 MB to 0.176 MB (first
          open) and from 19.30 MB to 0.179 MB (reopened);
        - render 0.91–1.82 → 0.22 ms, and 1.69–2.75 → 0.28 ms.

        `fude/zoom/render_stroke.c`, `render.c` and `render.h` call the new `rde_rendering_2d_keep_stroke_mesh` family.
        The zoom suite's harness stands in for it, so it records kept and made meshes alike.
      - **Pictures:**
        - Rounded shapes' corners are now antialiased; straight sides are byte for byte the same.
        - Slug text is heavier where it is light on dark. Its coverage was applied twice.
        - Text is about 2% wider, because advances are no longer floored per glyph. Chips sized to their text grow 1–2
          px, and two paragraphs re-wrap (welcome-2's card gains a line; the Settings licence).
        - Arabic "بِـ" joins before the tatweel.
        - Punctuation kerns ("T.").

        The pictures as they were before any of this are kept for the comparison; the baseline is the owner's to
        refresh.
      - **Phase 2, Slug text batched (RDE).**
        - Each string's quads are placed on the CPU, so one uniform serves a whole range.
        - Consecutive runs merge into one draw, across fonts.
        - All Slug fonts share one glyph texture pair.
        - A draw moves ahead, past draws it does not overlap, to join one it merges with.

        Kana's Browse, per frame, old → new (debug, lldb, frames 120–130):

        | | Old | New |
        |---|---|---|
        | Draws | 107 | 11 |
        | Pipeline binds | 105 | 10 |
        | Scissors | 106 | 11 |
        | Sampler binds | 103 | 7 |
        | Command buffers | 2 | 1 |
        | Render (`measure`) | 0.94–0.99 ms | 0.51–0.54 ms |

        InfiNote's main screen goes from 23 draws to 8.
      - **Pixels from phase 2:**
        - Moving the placement from the GPU to the CPU shifts each vertex by float rounding (about 10⁻⁵ px).
        - Where a pixel's centre lies exactly on a glyph's straight edge, that flips it: 7,149 pixels over 124 of
          151 pictures, isolated, on glyph edges.
        - The notches the old renderer left at such points (the "Y" stem in Kana's library) are gone.
      - **Phase 2, render on demand (fude, all ten apps).** A still screen draws one frame a second instead of
        sixty.
        - One module decides: `fude/drawing/base/redraw.{h,c}`, its rule in `fude_redraw_end_frame_at`, called from
          each shell's late update.
        - A frame is drawn when the launch asks for continuous drawing (a debug launch with any flag, or
          `--continuous`; release builds take no flags).
        - A frame is drawn when something changed in the last 4 frames or 0.5 s: an event, `fude_redraw_changed`,
          `fude_redraw_request` from any thread, or RDE's UI moving (`rde_ui_get_wake_after`).
        - A frame is drawn when a timer asked for with `fude_redraw_after` is due. It draws one frame, with no grace
          period, so a caret's blink stays one frame.
        - Otherwise nothing is drawn, however long the screen stays still. A first version drew one frame a second
          (a heartbeat) as a safety net. You chose the natural flow instead: every change reports itself. Debug
          builds then check for a source that forgot to: once a second while still they draw a frame anyway, compare
          it with the last drawn one, and warn with the screen's name and before/after pictures if it differs.
        - Each source reports through one call: animations and fades, flights, flicks, Play, recordings, the PDF
          worker, ML Kit and other polled answers (every 0.1 s), timers (holds, autosave, the journal flush, the
          cover), file-dialog callbacks.

        `docs/architecture.md` ("Render on demand") lists them. The suite `tests/redraw` checks the rule, including
        20,000 random frames, and fails against five broken versions of it. Every screenshot is byte for byte as
        before.

        Measured on the Mac (debug builds, still screen, 20 s; the same binary with `--continuous`, then
        `--on-demand`):

        | Screen | CPU | Frames in 10 s |
        |---|---|---|
        | Kana's Browse | 17.6% → 0.1% | 593 → 10 |
        | Hanzi's Browse | 26.8% → 3.3% | 599 → 10 |
        | InfiNote's canvas | 20.6% → 0.1% | 600 → 10 |
        | Sketching's canvas | 13.1% → 0.1% | 600 → 10 |
        | InfiniWorkshop's canvas | 13.3% → 0.1% | 597 → 10 |
        | Draw's page | 10.2% → 0.1% | 594 → 10 |

        Those were measured with the heartbeat. Without it, Kana's measure build (release, no flags) draws 0 frames
        in 10 s on a still screen, at 0.1% CPU. Debug builds draw 10, the check's frames.
        - A paused Play is a still screen: the circuit's current dots and the warning rings run on Play's own clock.
        - Five sources that the heartbeat had hidden now report themselves:
          - Android's voice check after the app comes back;
          - the reviews' day labels at midnight;
          - Android's orientation-lock retry;
          - the autosave's retry;
          - the journal flush's retry.
        - Checked live through lldb: strokes, holds, flicks, flights, notices, the journal flush and the cover while
          asleep, the practice demo, stroke-order playback, PDF pieces and the reading-card countdown.
        - Not run: Android and iOS (surface recreation, permission dialogs and pickers were reasoned about and
          built, not run).
      - **Phase 3, after three second-opinion reviews (2026-10-19), kana's side:**
        - **Kept characters.** Browse drew its 364 glyph strokes from scratch every frame. Each character is now one
          mesh kept on the GPU (`fude/study/chars/glyph.c`; 512 kept, keyed by character, size and pixel size, all
          forgotten on trim). RDE's regrouping now moves kept meshes too. Before, each character sat between two
          captions, and the tablet switched GPU pipeline 299 times a frame (now 14).

          | Kana's Browse | Before | After |
          |---|---|---|
          | Galaxy Tab render | 5.6–5.7 ms | 2.8 ms |
          | Galaxy Tab bytes uploaded a frame | 624 KB | 62 KB |
          | Mac frame CPU (measure) | 1.9–2.6 ms | 1.3 ms |

          Five of Kana's nineteen screens differ by 1/255 in 2–34 soft-edge pixels: a moved mesh rounds in another
          order. Test: `tests/glyph`.
        - **Text fields.** Text and caret sat low in every field, in every app (your report). It was an old editor
          bug: the baseline was at the bottom of the line box. They are centred now, within 1 px.
        - **Tiles.** The two-layer tiles are gone (`fude_draw_tiles_*`): RDE's batch groups Browse's cells by itself.
          All 151 pictures are the same bytes, and draws are equal or fewer (Vocabulary 8 → 6).
        - **Render on demand, the last of it:**
          - The debug check for a forgotten source is in.
          - On Android, a still screen burned a whole core inside SDL's wait. The vendored SDL is fixed: the tablet
            now sits at about 6% in total, 0 frames.
          - The audio device stops after 3 s of silence, and SDL's accelerometer listener is off unless an app opens
            the sensor.
          - Frames are paced to Android's vsync (Choreographer). Frames over 20 ms on Browse went from 9/1/2 to 0–2
            per run, with one frame less latency.
        - **App names.** 171 comments in fude named an app; all are generic now. `rules.py`'s app-identity check reads
          comments and symbol names too, so the 0 holds: 233 hits on the tree before, 0 now.
          `FUDE_ZOOM_PRODUCT_SKETCHING` is `FUDE_ZOOM_PRODUCT_ALL`. The language bakes take their app paths and
          Hanzi's licence notice from the app (the shell def's `bake`), and the rebuilt data is byte-identical.
        - **Versions on the tablet:** Kana 0.5.10, study apps 0.1.8, canvas apps 0.2.2. The final install is still to
          do.
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
also run from the command line: `sh tests/headless/run.sh` (new; sh, not zsh, which does not split its flags).
503 cases pass, and the groups that need the engine running, `sh tests/engine/run.sh`, 51 (2026-10-17).

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
- The 2D flush's index rewrite (4.9, 2026-10-16): skipped for a shader whose draw commands, in drawing order, each
  start where the one before it ended (a frame drawn in the order it is drawn, as a canvas of strokes is). The rewrite
  copied such a shader's indices twice, into a scratch and back, only to leave every byte where it was. One walk over
  the commands decides; a shader with a command out of place is rewritten whole, as before, and the others are not
  touched. The sort and the rewrite moved out of `_rde_rendering_flush_all_commands_2d` into
  `_rde_rendering_2d_order_commands` (`rde_rendering_2d.c`; it says why and when it skips, as do the file's overview
  and `rde.c`'s), its scratch passed in; `rde_shader_buffer_slot`'s rewrite fields say what they hold while it
  decides. New for the test, your call whether it stays public: `rde_rendering_2d_order_draws` and
  `rde_rendering_2d_batch_draw` (rde.h), the same code run on draws the caller describes, without a GPU. Its test,
  `tests/unit_tests/test_2d_order.c`, runs the old sort and rewrite (kept in the test) on the same frames (in order,
  out of order, across layers, stencil masks, shader and texture switches, draws with buffers of their own, indices no
  draw records, 20,000 random frames) and checks the indices, first indices, order and draw calls byte for byte, that
  nothing is copied when a frame is in order, and only the shaders out of place when it is not.
- Comments made true: `rde_malloc`/`rde_calloc` (the engine's pool, through SDL, not the system heap) and
  `rde_physics_2d_world_create` (`_body_capacity` is Box2D's first reservation, not a cap).
- Not changed, for you to decide: `rde_memory_texture_gpu_upload` ignores `free_data`, so a texture's CPU copy can
  never be dropped after upload (the documents' tiles keep theirs); the builder applies no framework's module-map
  `link` lines (Google Mobile Ads 11 needs `-framework JavaScriptCore` by hand; 13 needs Xcode 16); `builder run`
  starts the app in the project's folder (its saves land there); assets' dot files (`.DS_Store`) are packed into APKs;
  on POSIX `bs_copy_file_if_exists` always reports success.

**Uncommitted, 2026-10-16–17: the rendering review (4.10), for you to review**

Every fix has a unit test that was checked to fail without it.

*Frame and 2D* (`rde_rendering.c`, `rde_rendering_2d.c`, `rde_shader.c`, `rde_svg.c`, `rde.c`, the 2D shaders):
- **Blend.** Alpha is stored as a + d(1−a); it was a·a + d(1−a). A transparent render texture is flagged
  premultiplied and drawn with a premultiplied shader. The gate canvas exported to PNG had 342,377 pixels with alpha
  below 255 on an opaque page; it now has none.
- **Draw order and stencil.**
  - `draw_static*` and particles close the open batch before they record.
  - Particles set a stencil mode.
  - Masked draws are no longer sorted after all unmasked ones.
  - Nested and sibling masks each test only themselves: the reference is counted by depth, and push/use/pop are
    documented in rde.h. A nested mask used to hit a fatal assert.
- **Clipping.**
  - The clip state is reset after an SVG draw.
  - The no-clip scissor is the whole viewport, its x/y included, so FIT with bars no longer cuts a strip.
  - `begin_clipping_rect` places the clip in the drawing's own size and nests as a stack; `end` restores the one
    before.
- **Pipeline rebuilds.** A pipeline rebuilt for MSAA or stencil keeps slot 1 per instance (particles had broken).
  Single-sample 2D pipelines are not rebuilt on an MSAA change.
- **Merging.** Untextured draws merge: no texture counts as an identity. The GPU texture is captured at record time,
  and extra samplers are compared. 40 shapes are now 1 draw.
- **Kept stroke meshes on the GPU** (new: `rde_rendering_2d_keep_stroke_mesh` and its family). They live in one GPU
  arena with 12-byte vertices and u16 indices, are uploaded once in the flush's copy pass, and are drawn with offsets
  and a small uniform. Kept is byte for byte what appended was.
- **The frame.**
  - One copy pass and one render pass per target, single-thread.
  - One transfer buffer per shader, and a frame's flushes share one buffer per frame at their own offsets.
  - Buffers shrink after a while unused.
  - The frame arena is cleared once, colours are unpacked per vertex, and samplers are bound only when they change.
  - The sort is skipped when the commands are already in order: 4M indices, 0.843 ms → 0.001 ms.
- **Fences on Metal.** A signalled fence is released only after the next submit; releasing it before SDL had cleaned
  its command buffer let the next submit reuse it, so command buffers stuck and pools grew while the GPU lagged.
- **Rounded rectangles** antialiased at the corners only. The sides snap to where the hard edge was, so they are byte
  for byte as before. Toolbar round buttons' false bars are gone (exact arc ends).
- **Translucent strokes:** a sharp joint is inked once. **Rotated quads** move as one in physical pixels.
- **Render on demand** (new, opt-in, off by default): `rde_engine_set_render_on_demand`,
  `rde_engine_frame_unchanged`, `rde_engine_wake_after` and `rde_engine_request_frame`.
- **The profiler's draw counter** counts every draw, Slug's included.

*3D* (`rde_rendering_3d.c` rebuilt, clustered, decals, GPU cull, compute, the mesh/skybox/BRDF shaders,
`mesh_3d_depth_alpha.frag` new):
- **Lights.** Point and spot lights live in a storage buffer that grows; there is no cap.
  `RDE_MAX_POINT_LIGHTS`/`_SPOT_LIGHTS` are now only starting capacities. Clustered lighting is the one path, and it
  samples shadows; `rde_rendering_3d_clustered_set_enabled`/`_is_enabled` are removed. The old fixed arrays wrote 96
  bytes past a struct on the stack.
- **Shadows.**
  - One depth-only atlas replaces the spot array and the 32×6 cube array.
  - A per-frame budget ranks lights by brightness × apparent size: 8 lights in 4096² on desktop, 4 in 2048² on
    mobile (`rde_lighting_set_shadow_budget`). A light outside the budget still lights the scene, unshadowed.
  - A point light takes 6 tiles, each a little over 90° so filtering stays inside it.
  - Casters are classified once per frame and culled per view, with a static-caster cache per tile.
  - Cascades: their count is a setting (`rde_lighting_set_shadow_cascade_count`, 1–4, default 3); the light view no
    longer moves with the camera, so texel snapping works; there is a shadow distance
    (`rde_directional_light.shadow_distance`).
  - No more trails: a light with no caster gets a cleared tile.
  - Skinned-only scenes get shadows, the spot cache is keyed on the view-projection, MASK/BLEND casters are
    alpha-tested, and casters draw both faces with slope bias.
- **Batching.** Opaque draws are sorted by (shader, mesh), with one mapped instance buffer, one copy pass and one
  scene render pass. State is bound only on change, and frame uniforms are pushed once. A test scene of 900
  interleaved meshes: 13.8 → 0.60 ms CPU, 1,483 → 29 draws, 718 → 5 render passes. 200 lights: 1.96 → 0.28 ms.
- **Pass order.** Opaque static → skinned → dynamic → decals → skybox → every BLEND mesh back to front → blended text
  → particles. One place decides CLEAR or LOAD and routes render textures and MSAA, so no pass draws on last frame.
- **Shading.**
  - Colour factors are applied after decoding the texture (they were squared).
  - Normals use the cofactor matrix (non-uniform scale).
  - Double-sided back faces flip their normal.
  - The skybox no longer applies pow(2.2) twice; LDR cubes are sRGB with mips.
  - The BRDF table uses k = α/2.
- **Culling and buffers.**
  - Dynamic batches no longer wrap u16 indices past 65,535 vertices.
  - GPU cull is no longer out of bounds past 42 groups.
  - The frustum, the cascades and the clusters all come from the projection actually used: in portrait the frustum
    covered 56% of the view (`rde_rendering_3d_get_camera_frustum`).
  - Bounds use Arvo's method, and skinned meshes are culled.
  - Buffers grow ×1.5, NPOT textures get mipmaps, and compute work is recorded in the frame's command buffer.

*Text and UI* (`rde_text_engine.c`, `rde_font.c`, `rde_slug.c`, `rde_ui.c` and the widgets, slug/text shaders):
- **Dangling `char_info`.** A shaped character pointed into the font's glyph map, which moves when it grows. It now
  carries copies.
- **Shaping.**
  - Each font run is shaped once (1,034 → 374 `hb_shape` calls for Browse's strings), and punctuation joins its
    neighbours, so it kerns.
  - FriBidi is skipped when nothing is right-to-left, and HarfBuzz buffers are reused.
  - 29.3 → 16.6 µs a string.
  - The engine keeps no shaping cache (your decision). Every `rde_ui` widget that draws text keeps its shaped text,
    the editor's placeholder included.
- **Correctness.**
  - Advances are kept in 26.6 and rounded once.
  - Slug's em comes from the exact scale.
  - The ellipsis cuts between whole characters.
  - Bidi is ordered per line after wrapping.
  - Slug's per-pass limits grow instead of dropping text.
  - COLRv1 foreground stops are resolved per draw.
  - Fallback fonts at another size are placed by their own scale.
  - rde.h's text docs now say what is true.
- **Atlas.** Packing adds only the new glyph, only dirty rectangles are uploaded, coverage is stored as R8, and Slug
  glyph rows are uploaded once per frame.
- **UI.**
  - A node's layer comes from what it overlaps, so later siblings draw over earlier ones and hit-testing agrees.
  - Masks use the per-mask stencil reference.
  - A label that fits pushes no scissor.
  - The clip stack's overflow is reported.
- **Two use-after-frees and a double free.** A font unloaded mid-frame was still in the atlas upload queue, and an
  editor destroyed on its own freed twice.

*Phase 2, what phase 1 left:*
- **`rde_render_texture_read_pixels` returns straight colour again.** It is un-premultiplied when the render texture
  is premultiplied. kana's three exports draw an opaque page first, so their PNGs are byte for byte the same.
- **A render texture on a BLEND or MASK 3D material** is un-premultiplied in the mesh shaders (a flag in the
  material's spare channel, so no new pipelines). World-space UI canvases are OPAQUE and unchanged.
- **3D atlas text billboards.**
  - Each glyph reads its own font's atlas: a fallback glyph used to read the primary font's.
  - They sample LINEAR, since a billboard is always scaled.
  - Text used only on billboards is uploaded; it never reached the GPU.
  - The by-string billboard function says it shapes every call.
- **Atlas glyphs have a 1-texel gutter** (`rde_font.c`). LINEAR sampling of magnified text drew lines from its
  neighbours. Unscaled text samples NEAREST and never reads the gutter.
- **The metrics overlay keeps each row's shaped text:** 3.0–3.3 ms → 0.17–0.21 ms a draw (debug). A NULL font no
  longer crashes it. New `rde_metrics_get_overlay_shape_count`, for the test.
- **Integer limits.**
  - `RDE_i8/i16/i32/i64_MIN` were positive literals, so an i16 never equalled `RDE_i16_MIN`. They are now typed,
    as −MAX − 1, and every MAX is cast to its type.
  - `RDE_u64_MAX` reads as unsigned in Lua; it read as −1.
  - The .rdef getters clamp before narrowing: an i8 of 200 read as −56, and every negative float read as 1.2e-38.
  - The Lua generator understands the new forms.
- **The engine architecture docs** (tools/doc_generator: 3D, 2D, clustered lighting, decals, text, UI, foundation)
  say what the code does now.
- **Slug text batched** (`rde_slug.c`, `rde_rendering_2d.c`).
  - Quads are placed on the CPU in the drawing's vertex space: position, em size, scale and rotation. So Slug now
    follows the 2D camera and FIT like every other draw. 3D Slug runs keep em-space quads, byte for byte as before.
  - A Slug command extends the previous one at record time, and contiguous entries with the same clip are one draw,
    across fonts.
  - One curve/band texture pair (`g_slug_store`) serves every Slug font. The oldest glyph of any font is evicted,
    `rde_font_trim` frees a font's glyphs, and the store shrinks back once nothing holds it up.
  - Every Slug upload is recorded in the frame's copy pass (no command buffer of its own), and the quad index buffer
    is static.
- **Gathering** (`_rde_rendering_2d_gather_commands`, after the sort). A command moves earlier to just after the
  nearest command it merges with, past commands of the same layer and clip it does not overlap. It never crosses a
  stencil mask or a command whose bounds are unknown. `rde_draw_command_2d` gains a 16-byte bounds field.
- **Slug honours rotation and scale.y**, so `rde_ui_label_set_rotation` works on Slug fonts. Its no-clip scissor
  includes the viewport's x/y.
- **Atlas text rotation was wrong.** Radians were passed where `rde_math_mat_rotate` takes degrees, so 90° turned the
  text 1.6°, and each glyph turned about a point of its own. It now turns about the text's position.
- **CPU arrays shrink.** At the end of a 600-frame window whose peak is under a quarter of the capacity, an array
  shrinks to twice the peak.
- **Retina snapping.** Unrotated sprites snap to physical pixels at a whole scale of 2 or more; 1x is the old
  rounding bit for bit.
- **Bugs the reviewers suspected, verified and fixed** (each test fails without its fix):
  - **A font atlas growing mid-frame** released its old texture while 2D commands recorded earlier in the frame
    still bound it, and that texture lacked the frame's new glyphs. It was made worse by this week's capture of
    textures at record time.
    - The old texture now gets its pending glyphs, is retired, and is released once its frame has been submitted
      (`rde_font.c`: `_rde_font_retire_gpu_texture`, `_rde_font_release_retired_textures`).
    - Also fixed: a font's very first text was recorded with no texture, so for one frame it sampled whatever was
      bound before (at HEAD too).
    - Test `test_font_atlas.c`.
  - **A shadow tile kept another light's depth** when its light left the budget for a few frames and came back.
    Rendering a tile now drops every other record overlapping it (`test_shadow_cache.c`).
  - **Decals applied alpha twice:** a premultiplied shader with a straight blend. The pass now blends premultiplied
    (`test_decals.c`).
  - **Decals and the clustered overlay assumed a full-window viewport.** With FIT bars a square decal came out
    1.79× wide, and the overlay lit the bars. Both now use the scene's viewport (`decals`, `clustered_overlay`).
  - **Threaded mode read live state.** The render thread read the live directional light, IBL and skybox. They are
    now copied into the frame's slot when the packet is built.
  - **Floating windows never ticked:** `_rde_ui_update` skipped `windows_root`, so their widgets got no
    transitions and no `on_update` (`test_ui_window.c`).
  - **Every Lua plugin failed to load** (at HEAD too). The generated cdef used `rde_memory_allocator` without
    declaring it.
    - The generator now declares any `rde_` type that is used but not declared, as an opaque struct, with a build
      warning.
    - `rde_model_load_params` and `rde_bounding_box` are exported with their fields.
    - Tests `test_lua_cdef.c` and `test_lua_plugins.c`.
  - **Stale comments made true:** `mesh_3d_pbr.frag`, the tonemap and IBL blocks, the UI's z-bands,
    `rde_text_engine.c`'s header, and `rde_shader.c`'s blend list.
- **Render on demand, the engine's part** (for kana's adoption, 4.10).
  - A window or display event always draws (`frame_forced` in `rde.c`), until a window has really drawn, so a
    swapchain not ready yet does not count. A minimised app does not spin.
  - Returning to the foreground wakes a sleeping engine and draws (`rde_event.c`). SDL never queues those events.
  - New `rde_ui_get_wake_after`: 0 when RDE's UI moved this frame, above 0 for a timer, below 0 for nothing.
    - Moving: transitions, scroll flings and spring-backs, toggles, toasts, spinners.
    - Timers: a focused editor's caret, the touch-hold selection, tooltip dwell.
  - Mobile edit-menu picks call `rde_engine_request_frame`.
  - Test: `test_render_on_demand.c` (an EXPOSED event during "unchanged" frames is drawn).
- **Not done:** threaded mode keeps one pass per drawing. Sharing a pass would hold drawings back and lose the
  overlap threaded mode is for. Nothing runs threaded today.

*Tests and the builder:*
- `tests/main.c` runs staged groups one at a time and confirms a leak over up to 8 more passes. SDL's pools grow to a
  high-water mark and stop.
- `tests/unit_test_util.h` passes a group's test names as the variadic part, so a group of one test compiles under
  clang's `-Werror`.
- Its `RDE_UNIT_TEST_EXPECT_*` macros parenthesise their arguments. Pasted bare, an argument bound into the
  comparison: `EXPECT_I32_EQ(a ? b : c, 61)` compared `a ? b : (c == 61)`, so `test_crash.c`'s length check passed
  for any length. It holds at 61 now. Found by the hub session.
- `rde_build.c`'s `compile_tests` adds `external/include` (the tests reach SDL's atomics), and `test_uuid.c` uses
  `rde_memset`.
- `sh tests/engine/run.sh` now builds and runs on the Mac: 51 cases pass. `sh tests/headless/run.sh` (sh, not zsh)
  passes 503.

*For you to decide:*
- **Rounded rectangles.** Sides snapped to the old raster (chosen), or purely geometric antialiasing, which would
  make off-grid sides soft.
- **A kept-mesh draw outside a 2D drawing:** it draws nothing and warns once (chosen), or it asserts.
- **3D into a render texture** is documented as unsupported. Render textures are RGBA8 and 3D renders in 16-bit HDR
  with no tonemap on that path.
- **Shadow casters** draw both faces with slope bias plus a texel-relative normal offset, which keeps closed meshes
  as before.
- **Text metrics changed meaning.** A line's height is now tallest + deepest over the whole line, and a trailing
  "\r\n" is one break.
- **Not done yet:** SSAO darkening only ambient light, which needs a depth prepass or a second target, and LOD levels
  in one buffer with multi-draw, which needs a mesh-format change.
- **World-space UI canvases are OPAQUE**, so their transparent parts show black, as before. As BLEND they would now
  composite correctly.
- **The atlas gutter** costs about one texel per glyph on each axis.
- **Slug pixels.** Byte-identical text would need the old em-to-clip rows carried per vertex (+32 bytes a vertex). The
  CPU placement was kept; its only cost is the isolated edge pixels above.
- **Rotation directions differ.** Text turns counter-clockwise for positive degrees (as the label docs say); sprites
  (`rde_rendering_2d_draw_texture_*`) turn clockwise.
- **Odd drawing sizes.** Slug keeps the exact half size as its centre, so its pixels stay put. The batch uses the
  floored half, so Slug text can sit 0.5 unit right of where atlas text would.
- **`rde_font_trim`'s return value** now means "the shared store is back at its start size", which can be false
  while another font still holds glyphs.
- **`rde_model_load_ex` takes its allocator in the middle**, so the Lua generator cannot strip it: Lua calls it as
  `(path, nil, params)`. Move the allocator last, as the engine's other functions have it, or teach
  `_rde_lua_strip_allocator` the middle.
- **The Lua plugin suite's own failures**, visible now that plugins load:
  - `t_io_path_escape` gets error code 8.
  - `t_alloc_strip` finds no depth format before any 3D frame.
  - `t_memory_bomb` and `t_str_arr_roundtrip` crash in libunwind while LuaJIT unwinds an error on macOS arm64.
- **A font gaining k pages in one frame** now uploads the whole atlas k times. Spare layers would avoid it, at up to
  twice the atlas's GPU memory.
- **Threaded mode is finished since** (see the 2026-10-18–19 block below), and still off by default.
- **Render on demand (kana), decided:**
  - no heartbeat, plus the debug check;
  - a paused Play is still;
  - on iOS the engine now sleeps in UIKit's run loop instead of SDL's 1 ms polling (built; to test on an iPad).

**Uncommitted, 2026-10-18–19: the rendering made natural, threaded mode finished, Lua fixed (for you to review)**

You asked for a pipeline that is correct, efficient, easy to follow, well structured and natural. Four reviews (frame
and 2D, 3D, text and UI, SIMD) listed where the code was unusual and why. Fixers then changed the cause, not the
symptom, each fix with a test that fails without it. The checklist, ticked as each item landed and was checked, is the
"RDE Rendering Pipeline Plan" doc (https://claude.ai/code/artifact/aed39bbb-5e10-4497-89ec-5f42a9c07500); your
decisions are in its last table.

*Frame, 2D and threaded mode:*
- **A pipeline cache.** Pipelines are keyed by shader, stencil mode and pass targets. 2D drawn into a render texture had
  used screen-format pipelines (175 Metal validation errors). It also replaces the in-place MSAA rebuilds, the stencil
  variants made mid-pass and SVG's rebuild hook.
- **One deferred-deletion queue on one frame clock.** Unloading something the frame still draws no longer frees it
  under the GPU, in single-thread mode too. It replaces three ad-hoc "release later" schemes and the GPU-idle waits in
  unloads; cubemaps, IBL, meshes, models and fonts go through it.
- **One upload service:** a per-frame staging arena recorded into the frame's one copy pass, replacing about 40 sites
  that each made their own command buffer.
- **One 2D coordinate space and one view-projection per drawing.** Clips are float rectangles in drawing space
  (`rde_rendering_2d_begin_clip` / `end_clip`), viewport regions are real (`rde_camera_set_viewport_region`), and a
  scissor can no longer reach past its target.
- **Premultiplied alpha throughout 2D.** `rde_rendering_clear_background_color` now clears what is drawn into: a render
  texture started transparent before.
- **Simpler 2D state.**
  - SVG is drawn as a texture.
  - The unused LDR MSAA target is gone (about 90 MB at 4x on an iPad Pro).
  - Clear/load is per-target state; the global window "cursor" is gone; flush points are explicit.
  - Particles, 2D and 3D, are copied into the frame when drawn.
- **Immutable frame packets with one executor,** run inline in single-thread mode and on the render thread in threaded
  mode. Single-thread and threaded give the same frame hash (`tests/engine/run.sh --compare`).
- **Threaded mode** (still OFF by default):
  - It presents from an offscreen target per window; the main thread acquires, blits and presents (your choice b).
  - Five thread-sanitizer races are fixed (allocation counters, peak windows, metrics counters, pool counters, Slug
    init). None are left in RDE code; the remaining reports are inside SDL's Metal backend.
  - A crash is fixed: the kept-mesh upload buffer was sized on one thread and written on the other.
  - Measured, threaded against single-thread: Kana Browse −7%, the gate canvas about equal, encode-heavy 3D large
    (200 lights 2.66 → 1.93 ms).
- **Your decisions:**
  - rounded rectangles with exact signed-distance coverage (one quad each);
  - one rotation convention: positive is clockwise on screen, for everything;
  - vsync from the config honoured, and `--vsync` accepted.
- **Speed:**
  - `_rde_2d_extra_samplers` returns early: it scanned 63 empty slots per draw, 34% of the gate canvas's CPU frame.
  - Stroke tessellation is bit-exact and about 20% faster, twice over: from scalar fixes, then from the maths pass.
  - sqrt, floor, ceil, round, trunc and fabs are compiler built-ins, bit-identical.
  - The 3D LOD pick went from 7.2 to 4.9 ns.
  - Bounds taken at record time were built, measured slower and taken back.
- **Found on the way:**
  - `rde_window_destroy` leaked each window's creation properties.
  - `rde_hash_map_foreach` read one slot past the end on its last step.

*Apple platforms always present with vsync* (`rde_rendering.c` `_rde_rendering_unsynced_present_allowed`, used by
all three present-mode choosers). Your Mac restarted twice on 2026-10-19. Both were kernel panics in the display
coprocessor: "DCP PANIC – program_swap: Async Swap request landing on unsupported platform". They came from presenting
without vsync: measuring runs with `--vsync=false`, and RDE gave every window after the first no vsync. No program
should be able to do that, so RDE refuses it there. Frame cost is measured on the CPU (`--perf`).

*3D:*
- **A view object** decides each pass's target, size, viewport and clear/load in one place. SSAO and the outline are
  right under FIT bars, and 3D into a render texture works, at its own aspect.
- **SSAO** comes from a depth prepass and darkens ambient light only (your choice).
- **Decals** are lit, in the forward shader (your choice).
- **One render-item list** with per-pass sort keys (your choice).
- **Bloom and the outline** are folded into the tonemap: one full-screen pass instead of three.
- **Shadows:**
  - cached static depth per tile, with only the moving casters drawn on top;
  - persistent tile placement with hysteresis;
  - a light re-added at a freed light's address no longer inherits its cache.
- **LOD** by projected screen error, with all LODs in one index buffer.
- **GPU culling** stays opt-in, on the main path's instance data (your choice); the double instance offset that dropped
  cubes is fixed.
- **3D Slug text is drawn like 2D's:** 8 billboards are 1 draw. Each entry's plane sits in a storage buffer;
  `rde_slug_vertex` is unchanged.
- **Colour:**
  - Colour maps are sRGB with mips made in linear light, data maps UNORM, and no pow(2.2) is left (a decode flag
    covers render textures used as maps).
  - 3D text and particles decode their sRGB colour too, so they match 2D before the tonemap.
- **Smaller changes:**
  - one lighting layout for Blinn and PBR;
  - shader includes (`engine/shaders/include/`);
  - the BRDF table baked once;
  - load-time GPU work through the upload service;
  - a default directional light;
  - the 4-wide SIMD frustum test (`rde_simd.h`: NEON, SSE2 or scalar; bit-identical, 4.6× faster).
- **The `RDE_RENDERIG_` typo** is renamed to `RDE_RENDERING_`, with the old names kept as marked deprecated aliases.

*Text and UI:*
- **Shaped text is a glyph run:** exact 26.6 pens, byte clusters, rows and row metrics. Shaping rasterises nothing,
  and Slug draws at the exact pen.
- **A font is a face plus sizes:** the face holds the bytes, FT_Face, hb_face and fallback chain; a size is an FT_Size
  and hb_font. Scaled labels keep their fallbacks.
- **One shared glyph atlas,** R8 and BGRA 2D arrays. It grows by GPU copy, uploads only new glyphs in the copy pass,
  and evicts pages. The k-uploads-per-k-pages problem is gone.
- **The UI is drawn in tree order,** one layer per band, with the batch's gathering grouping draws. Kana Browse: 11 →
  12 draws, byte-identical pictures.
- **Every UI band was drawn twice a frame:** a macro evaluated its argument twice.
- **Colour emoji** are on the premultiplied path (no dark fringe); one text shader per atlas format; Slug honours
  stencil masks; one font loader.
- **The metrics HUD** gained a "Text shapes" row, and `rde_metrics_get_overlay_shape_count` is gone.
- **The heatmap's y caption** reads upwards like the charts'.
- **Being finished as this is written:** the UI on the new clip API, and the Slug store handed over at frame close
  (the last render-thread waits).

*Vendored SDL, two local patches* (marked `RDE:`, listed in the memory notes): in `SDL_gpu_metal.m`
`METAL_BindGraphicsPipeline`, samplers, storage textures and vertex storage buffers are re-bound when a pipeline's
counts differ. On Metal they share binding slots. Without it, Slug text drawn after a sprite drew nothing: kana's
Settings and Licences dialogs lost most of their text.

*Lua and the builder:*
- **`rde_model_load_ex`** takes its allocator last, and Lua calls it as `(path, params)`. The Godot .tscn importer on
  your Windows machine must swap its last two arguments. `rde_rdef_try_parse` is `(path, out, allocator)` too (the hub
  session's).
- **The Lua plugin suite is green** (33 tests).
  - A path that didn't exist yet escaped the plugin's mount.
  - The sandbox loaded a second, unstarted copy of the engine.
  - macOS arm64's LuaJIT uses internal unwinding: external crashed in libunwind through JIT frames.
  - Every string and array Lua got from RDE leaked.
  - Several entry points called Lua outside a protected call.
- **Physics callbacks** never call Lua inside a step: events are queued and delivered after it. The contact filter isn't
  exported to Lua. Physics tells listeners of freed bodies through a hook (`_rde_physics_2d/3d_on_free`), so it knows
  nothing of Lua.
- **Path functions** go through the plugin's mounts.
- **Android's arm64 LuaJIT:** my old DUALNUM suspicion was wrong (the output is byte-identical); its row now follows
  LuaJIT's Makefile. A number-mode test is to run on the tablet.
- **The builder** recompiles itself again: the Android LuaJIT row outgrew its array.

*2026-10-19 (evening): three second-opinion reviews and a docs pass, all fixed* (each with a test that fails
without its fix):
- **Frame and threaded mode:**
  - ImGui is a pass of the frame.
  - The atlas unload goes through the deferred queue; a window destroyed from its own callback waits for the frame's
    end; MSAA changed mid-frame becomes a request for the next frame.
  - The pak's mip pyramid is bounds-checked.
  - The deletion queue drains before a render-on-demand sleep; the present is keyed by frame; fences are released by
    one thread.
  - Static meshes follow the 2D rotation and +y rule; one units-per-pixel function; premultiplied alpha applied once.
  - A resized render texture is cleared; a 2D bake into a render texture outside a frame gets its own submission.
  - Debug resolution works in both modes.
- **A rare threaded crash.** A texture made, uploaded and unloaded between frames was released while its upload sat in
  a frame already handed over (4.5% of threaded runs; 0 in over 800 after). Also fixed in the threaded paths:
  - a vsync-change race;
  - an audio callback race;
  - the text libraries reading the pool's counter unlocked;
  - a GPU-idle wait at 64 fences.

  The thread sanitizer now shows only SDL's own false positive. Mac crash reports name their functions (dladdr).
- **3D:** five multi-view and shadow-cache bugs (a mesh vanished from a second view; a returning light lost its
  shadow; a picture-in-picture view showed the first view's image; two views shared a shadow tile's depth; a lost
  invalidation). Also:
  - SIMD identical with FMA on x86;
  - each view's own projection and frustum (new `rde_rendering_3d_screen_to_world_ray`);
  - the decal atlas through the upload service;
  - GPU-compute objects deferred;
  - per-window post targets;
  - cluster grids kept across views.
- **Text and UI:**
  - an atlas grown in a frame with no drawing recursed until it crashed;
  - a fallback chain mixing Slug and atlas fonts corrupted both stores;
  - `rde_font_load_from_memory` returned the default font;
  - fallback cycles and leaks;
  - a shelf packer for the atlas;
  - lists rebinding only rows that change;
  - Slug read past a band row's end (a stroke could vanish);
  - the even-odd flag;
  - `rde_font_get_generation`;
  - text editors centred (the old low-text bug).

  The vendored SDL Metal patch now marks every binding dirty on any pipeline change, compute too.
- **Outside rendering:**
  - iOS purchases finish (they replayed every launch), and iOS ads are reachable (built with each app).
  - Parked results wake render on demand, and Cmd+Q / system quit ends the app (`RDE_EVENT_TYPE_SYSTEM_QUIT_REQUESTED`).
  - Plugins: leaks, a use-after-free and a sandbox hole fixed; reload works.
  - Localization skips a bad line instead of emptying the language.
  - Long sounds stream; the audio device rests after 3 s.
  - `builder check` wants three-part Android ids.
  - The network allocator hooks no longer crash.
  - Eight more architecture notes match the code.
- **Android:**
  - The vendored SDL's wait spun a whole core on a still screen. It is fixed (`SDL_events.c`, and `HAVE_SEM_TIMEDWAIT`
    in the Android build config), both marked `RDE:`.
  - Frames are paced to AChoreographer's vsync.
  - The accelerometer listener starts only when an app opens the sensor.
- **The disk:** on 2026-10-19 the agents' scratch copies filled it once (34 MB free). They were cleaned up, and
  backups now keep notes only.

*Still for you to decide or do:*
- the autocomplete plugin (it needs tree-sitter, and names a type nothing defines; the hub session plans a generic
  completion plugin);
- the iOS run-loop sleep, implemented but untested on an iPad;
- the Lua number-mode test on the tablet;
- filing the vendored SDL patches upstream;
- `rde_font_parameters` lost its atlas page fields, an API break nothing in kana used.

**Removed**
- `rde_file_free_read_text` and `rde_file_free_read_bytes`: declared but never implemented, and contradicting the
  read functions (whose caller frees the buffer).

**Known, not fixed (your call)**
- The builder (`rde_build.c`) links every `.o` in a project's `obj` folder (`find … -name "*.o"`), including those
  of source files that were removed or renamed, which are linked into the app as they were. `tools/build/mac.sh`
  works around it: it clears the folder when an app's list of sources changes. Android builds start from a clean
  folder anyway. The fix in the builder: link only the objects of the sources it was given, or delete the others
  before linking.
