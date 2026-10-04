# The architecture

How the code is put together: the shared code (`fude/`, after 筆, the brush) and
the apps built from it (`apps/`). Kana is one; Draw, the start of the diagram
and drawing app, is another; a Mandarin and a Korean study app are meant to be
the next. Rewritten 2026-10-02, when the shared code was untied from Kana.
Every screen looked the same before and after (see the checking below).

---

## The folders

```
fude/drawing/            the drawing core: every app has it
  app/                   app (the screens' table, the pointer) · ui (the retained UI) ·
                         page (writing on the page) · session (saves, Your data) ·
                         look (a developer's flags) · screen.h · info.h · extension.h
  widgets/               kit row filterbar notice toolbar pagemenu docbar side draw scroll · icons.h
  ink/                   the page's strokes, the lasso, the view, the canvases
  doc/                   documents under a canvas's ink: pdf (Core Graphics; stand-ins
                         elsewhere; its text: pdf_kit.m, PDFKit) · doc (pages, textures, its
                         worker, search) · import (Files, Photos, the document camera:
                         import_ios.m) · library (the screen)
  base/                  files, words, colours: kfile save backup text theme utf8
  strings.py             its UI strings
fude/study/              a study app's: what is learned, and the screens to learn it
  app/                   study (fude_study, its hooks) · verbs · files · look · demo (a learner's six weeks)
  screens/               viewer browse practice album check exam stats scan vocabview wordexam translator
  widgets/               header wordcard pagetext
  models/                vocab review marks examlog history charnote select sheet
  handwriting/           match score segment recognize guide textink
  chars/                 the character data and its index: kanji catalog glyph
  services/              the device's: ML Kit, text in photos, translation, speech (each a .c and an _ios.m)
  strings.py             its UI strings
fude/lang/lang.h         what the study asks of the language an app teaches (one implementation per app)
fude/lang/wordsplit.h    a language's word splitter (its interface)
fude/lang/ja/            Japanese's own: lang (lang.c, lang_ios.m) strings.py romaji wordsplit bake chart
apps/kana/               Kana: kana.c (the shell) · src/ (kana_app, welcome, version.h,
                         text_ids.h) · assets/ · platform/ios/ · tools/ · docs/ · site/
apps/draw/               Draw: draw.c · src/text_ids.h · assets/ · tools/strings.py
fude/zoom/               the deep-zoom canvas (Sketching's): zoom (shared types, f64 transforms) ·
                         codec (strokes as exact integers, ~1.6 bytes a point) · index (R-tree) ·
                         scene (frames, objects, camera, undo, journal) · erase (three erasers) ·
                         shape (shapes as numbers, hold to snap) · smooth (smoothing, the rope) ·
                         nav (flying, the empty screen's marks, the depth) · fill (tap to fill, cuts) ·
                         export (SVG; PNG through the renderer) ·
                         select (the lasso) · zfile (the canvas's file: Kana's chunks as a log) ·
                         render · page · strings.py
apps/sketching/          Sketching (a working name): sketching.c · src/ (version.h, text_ids.h) · assets/ ·
                         platform/ (ios, android) · tools/strings.py
tests/                   a suite per module group (tests/README.md)
tools/                   strings/build.py (every app's strings) · icons/bearings.py · mlkit/
```

Each header is beside its source and is included by its layer's path
(`#include "drawing/widgets/row.h"`, `"study/app/study.h"`), so a file's
includes show which layers it uses. The include paths are `-I<project>/fude` and
the app's own `src/` (where its `text_ids.h` is).

## The layers

Each uses the ones under it, never the ones over it:

| Layer | May include | Is |
|---|---|---|
| `fude/drawing` | itself, and the app's `text_ids.h` | the page, the screens' table, the toolbar, the side panel, Settings, Your data |
| `fude/study` | the core, `fude/lang/lang.h` and `fude/lang/wordsplit.h` (the interfaces, not a language) | the character data, the study screens, their verbs, the word card, the page read as text |
| `fude/lang/<code>` | the core, the study | one language: what `lang.h` asks, its strings, its data bake, its own screens (see the last section) |
| `fude/zoom` | the core | the deep-zoom canvas: frames nested for precision, its objects, undo and file, and the page that draws on it (docs/infinite_canvas_design.md) |
| `apps/<app>` | all of them | its shell, what it is (`info.h`), what it adds (`extension.h`), its screens' order |

Inside each layer the old rule holds. Models keep what is learned and written,
and know nothing of the screen; most are tested on their own. Widgets are
pieces of UI that know no screen. Screens fill the screen over the page and
declare their rows of buttons. The app owns everything.

`fude/drawing` builds alone: Draw is its shell and the core's 25 sources, no
more.

## The app, as each layer knows it

One struct, three views of it. Each layer's struct starts with the one under
it, so a hook handed the core's `fude_app*` finds the rest by a cast:

```c
typedef struct fude_app   { ... } fude_app;      // drawing/app/app.h: the page, the screens, the UI
typedef struct fude_study { fude_app app; ... }  // study/app/study.h: FUDE_STUDY(_app)
typedef struct kana_app   { fude_study study; fude_chart* chart; kana_welcome* welcome; }   // KANA_APP(_app)
```

- **`fude_app`** holds what the core needs: the window and font, the page (ink,
  canvas, lasso, canvases), the screens' table in stack order
  (`screens[FUDE_APP_SCREENS]`, `screen_count`), the UI, the pointer. Plus two
  pointers to what the app is and adds: `info` and `ext`.
- **`fude_study`** adds the character data and its catalog, the study screens,
  Select mode's ticks, and its own state: the page read as text, the word card,
  a practice sheet asked for, Settings' Handwriting widgets.
- **`kana_app`** adds what only Kana has: the kana chart (Japanese's) and the
  welcome.

The verbs, from one study screen to another, are the study's
(`fude_study_view`, `fude_study_practice`, `fude_study_exam`, `fude_study_sheet`,
`fude_study_review`...). They take the `fude_app*` every press and update is
handed.

## What the core asks of an app

**What it is: `info.h`.** Its name and version (the side panel's version line,
About), its store id (Rate), its backup's file extension (Your data), its script
font (the language's characters, behind Roboto: Kana's is Noto Sans JP), its
credits, and its Licences documents.

**What it adds: `extension.h`.** Every field may be left out; Draw leaves them
all out.

| Field | The core's place for it | Kana's |
|---|---|---|
| `tools` | the toolbar, after Paper | the camera (Text from a photo) |
| `selection_row`, `context_row` | the page's menus (the core's: Cut, Copy, Duplicate, Delete; Paste, Select all) | those with Copy as text, Translate, Save word, Check; Paste text, Text from a photo |
| `menu_update`, `selection_faces`, `context_faces` | the menus, each frame and at a long press | the reading under way ("Reading…"), what can be pasted |
| `nav` | the side panel's top section | Study: Kanji, Kana, Album, Reviews · n, Vocabulary, Exams, Statistics |
| `sections` | Settings, between Paper and About | Handwriting (Google ML Kit) |
| `tutorial` | the side panel's Tutorial button (none without it) | the welcome |
| `ui_build/forget/update/restyle/hit` | its own widgets on the UI's canvas | the word card |
| `page_render`, `page_press` | over the page's ink | Translate's card by the selection, its speaker, its words |
| `session_open`, `settings_gather/apply`, `first_launch` | the session | the study files; ML Kit on or off; the welcome |
| `text_row` | the page's menu over a PDF's text the lasso took (no ink) | Copy as text, Translate, Save word |
| `page_kind` | the page itself: the toolbar's Undo, Redo, Clear and Reset view, and the session's opening and saving of a canvas | none (Sketching's: the deep-zoom page, `zoom/page.h`) |
| `library`, `library_count` | the Library screen's books (`doc.h`'s `fude_doc_book`: a PDF and its cover in the assets, its title, what it is, its credit) | the lectures: First Year Japanese I (CC BY 4.0, Kana's edition) |

The study fills most of it: a study app's extension starts with
`FUDE_STUDY_EXTENSION` and adds its own.

```c
const fude_extension KANA_EXTENSION = {
    FUDE_STUDY_EXTENSION,
    .nav = &KANA_NAV, .tutorial = kana_tutorial, .first_launch = kana_first_launch,
};
```

The page's menus are put together from both layers' buttons. The core's are
`FUDE_PAGEMENU_BUTTON_CUT`, `..._COPY`... A row finds a button by what it does
(`fude_row_def_find`), so "Copied" lands on whichever of the row's buttons
copied.

## A screen

A screen module keeps its state, draws itself and takes the pointer. At the end
of its `.c`, it fills a `fude_screen` (`FUDE_<NAME>_SCREEN`). Use the adapters
in `screen.h` for the usual function shapes.

```c
const fude_screen FUDE_STATS_SCREEN = {
    .name = "stats", .input = FUDE_SCREEN_INPUT_POINT,
    .is_open = ..., .close = ...,            // Escape and Back
    .update = fude_stats_screen_update,      // its frame, and what it hands on
    .render = ..., .pointer_down/moved/up = ...,
    .rows = FUDE_STATS_BUTTON_ROWS, .row_count = 1u, .row = ...,   // its rows of buttons
    .faces = ...,                            // how each button shows now (counts, greyed out, chosen)
    .field_hint = FUDE_TEXT_COUNT,           // no field of its own
};
```

- **Its rows are data.** Each button is `{ text, icon, press, arg, look,
  counted, present }`. `press` gets the app and the screen. When a button only
  calls a function of the screen, use `FUDE_ROW_CALL(name, type, fn)`. Each
  frame, `faces` says how the buttons show now: "Start 12", greyed out, chosen.
  The widget changes only what differs, so a screen keeps no "what is shown"
  state of its own.
- **What it hands on**, such as a character tapped or a word to practise, it
  either keeps for its update to pass to a verb, or does from a button with a
  verb. It never opens another screen directly.
- **Extras, declared too:**
  - `bar`: a filter bar across its top (Browse's)
  - `field_*`: a text field at the top right (Check's "I meant…")
  - `in_view`: what is in view, in order (what its Practice and Select's All take)
  - `selects`: its taps can tick (Browse, the chart); Select mode ends when no
    such screen is open
  - `resume`: what to do when it is on top again (the viewer replays)
  - `overlay`: drawn over what is under it (the welcome)

**Adding a screen:**
1. Write the module in its layer's `screens/` (or the app's `src/`).
2. Fill its `fude_screen`.
3. Give it a place in the app's order (Kana: `KANA_SCREEN_` in `kana_app.h`).
4. Add it to the app's table (Kana: `fude_screens_place` in `kana.c`).
5. Add its `.c` to the four source lists in `COMMANDS.txt`.

The UI builds its rows, the app routes to it, and its row shows when it is on
top. Nothing else changes.

**The stack.** The app's order is the stack's, top first. The first open
screen is on top: it alone gets the pointer, the frame, Escape and its row.
Under every screen is the page. When a screen closes, the one under it comes
back on top and its `resume` runs.

**Input.** One pointer at a time for the screen on top (pen, finger or desktop
mouse), in `fude_app_screen_event`.
- A writing screen (`FUDE_SCREEN_INPUT_WRITE`, Practice) takes a finger only
  with the hand on, and a pen takes over from it.
- Presses on the UI (`fude_ui_hit`) are the UI's.
- The page has its own input, with the finger-writing state machine
  (`page.c`).

**Right to left.** When the UI is right to left (its language's
`@direction`, or `--rtl`), `fude_app` draws every screen mirrored and maps its
pointer back the same way (`draw.h`: `fude_draw_mirror_begin`). A screen lays
out left to right as ever: boxes, cards and lines land mirrored, and text and
icons land at the mirrored place without being mirrored themselves.
- **Drawing straight with the engine** in a screen: put x through `fude_draw_x`
  (a centre) or `fude_draw_at` (a point), and a clip's centre too.
- **What must keep its own left and right** (a character's strokes,
  handwriting, a photo, a tick, a run of Latin) goes between
  `fude_draw_keep_begin` and `_end`: drawn as it is, its box placed mirrored.
  Pass `true` for a box written or tapped in, so a press there maps the same.
  Characters (`glyph.c`), the guide and ticks already do it.
- **Content laid out in its own script's direction** (the word test's boxes,
  the Arabic chart's rows) checks `fude_lang_rtl() != fude_draw_is_rtl()`,
  so the mirror does not undo it.
- **An icon inside a label's text** ("+ Add"): `fude_draw_icon_label`, which
  keeps it at the start of the line in either direction.

## Documents

A canvas can have a PDF under its ink (`fude/drawing/doc/`). Its note says which
(`fude_note.document`, a byte in the index's record that older builds skip):
`FUDE_NOTE_DOCUMENT_OWN`, the canvas's own `notes/<id>.pdf` (imported from
Files, or pictures and scans made into one); 2 and up, a book of the app's
library, by its id (never reused), read from the assets in place. Removing the
canvas removes its own PDF.

The page holds the open canvas's document (`fude_page_input.doc`): it follows
`notes.open`, lays the pages out down the canvas from its origin (each 1000
units wide, room around them for notes), and draws them under the ink from
textures — each page whole, then, once the view rests, what is on screen again at
the screen's pixels. One piece is drawn at a time, on a detached RDE thread; a
document let go mid-piece leaves the job to the worker to free. A dark theme
draws the pages in its own colours, so ink in the theme's colour shows on them.

While one is open the canvas reads like a PDF viewer (`canvas.h`'s reader
frame): the pages' width fits the screen at 100%, the least zoom; they move
across only when zoomed in, never past the first and last page, and a flick
carries on, slowing. The document bar (`docbar.c`) searches the PDF's own text
(PDFKit, a few milliseconds a frame) and goes to a page; the lasso, taking no
ink, takes the text in its loop's box (`lasso.h`: an area) for the app's text
row. The marker (`ink.h`: a stroke flag) is see-through and drawn under the
other ink; it is never read as writing.

A page with no text of its own (a scan, a photo) is read once, in the
background, by the app's picture reader (`extension.h`'s `picture_read` and
`picture_lines`: Kana's is ML Kit's Japanese text recognition), and its lines
are kept beside the PDF (`<id>.lines`), so the lasso and Search find them as
they find a PDF's own text. Share as PDF (`fude_doc_export`) writes the pages
with the ink over them and the read lines as invisible text. A page can be
turned a quarter at a time (the document bar's Turn page): the canvas keeps the
turns with its page (`canvas.h`'s `fude_page`, saved as 'TURN'), the PDF is
never changed, the ink on the page turns with it and the ink on the pages after
it moves with them, and a page read from its picture is read again.

PDFs are drawn by Core Graphics (`pdf.c`, plain C: iOS and macOS); the Mac links
it with the builder's `--osx_framework`. Android (PdfRenderer) and Windows have
stand-ins for now. The Library (`library.c`) is a core screen: an app that has
books gives them in its extension and puts the screen in its table (Kana:
`KANA_SCREEN_LIBRARY`, the side panel's Lectures).

## The UI, and rebuilding it

All retained widgets live on one canvas (`ui.c`) and are built from the app's
state. In stack order, bottom first: the toolbar, the page's menus, the
screens' rows, bars and fields, the side panel and Settings, then the app's own
widgets (Kana's word card, over everything). When the language changes,
`fude_ui_follow_language` destroys the canvas and builds everything again,
the app's own included (`ui_build`). The only state kept across a rebuild is
what the widgets do not own: the toolbar's tool and place, the side panel
being open, and the study's reading and card (in `fude_study`, not on the
canvas).

## The strings

Every text the app shows is in its layers' strings files and its own:
`fude/drawing/strings.py` (the core's), `fude/study/strings.py` (a study
app's), `fude/lang/<code>/strings.py` (its language's: the study's ids that
name the language, in its words, and the ids its `lang.c` returns), then the
app's `tools/strings.py`. That tool reads the layers it is built from, adds
its own rows (`t`), and writes the app's `src/text_ids.h` and
`assets/text/strings.rdel` (`tools/strings/build.py` checks them first). The
ids are in that order: the core's, the study's, the language's, the app's.

The core's words name no app ("Rate the app", "This app works offline…"). An
app gives a lower layer's string its own words with `o`, keeping its id and
place. Kana does that for six: About's version line, Rate, and four of Your
data's.

## Checking a change without a device

A developer's build reads launch flags. The core's are in `look.h`, the
study's in `study/app/look.c`, Kana's (`--kana`, `--welcome`) in
`kana_app.c`. `COMMANDS.txt` says what each does.

- `--size=744x1133` and a screen flag (`--browse`, `--viewer=6728`, `--exam`,
  `--vocab`...) open a screen in a given state.
- `--shot=FILE` writes a screenshot and quits.
- `--press=I,J,...` presses the buttons of the screen on top in turn, as taps
  would.
- `--swipe=DX` drags a finger across it.
- `--language=N` switches the language mid-run.

`tests/run.sh` builds and runs the test suites (tests/README.md).

Untying the core from Kana was checked this way. 39 screens and 13 pressed
sequences were shot and compared pixel by pixel with the shots taken before. 15
of the 52 animate or pick at random. The other 37 came out identical after every
step. The very first launch (no saves) was shot too: the welcome still opens.
Draw was shot on its own: the page, the side panel, Settings, Licences, Your
data, the paper panel, and another language. (The move into layers before it
was checked the same way. Of its 30 screens that do not vary, 29 came out
identical. One changed on purpose: Translate with Google's chosen icon now
fills, as every other toggle's does.)

## Making a new app

**A drawing app** (Draw is one): its shell (`apps/draw/draw.c`: what RDE calls,
what it owns), its `fude_app_info`, its strings tool (the core's layer, and its
own words where it wants them), its assets (Roboto, Phosphor, the flags, a
config), and `fude_save_set_folder` with its own name. It builds from the
core's sources alone. What it adds later goes through `extension.h`:
- its tools on the bar
- what it draws over the page, and presses there (diagram shapes, connectors)
- its rows of the page's menus
- its side panel section
- its own screens in its table

**A study app** (Hanzi for Chinese, Hangul for Korean, Thai, Hindi, Arabic): copy Kana's shape.
- `apps/<app>/<app>.c`, the shell (Kana's, with its own screens and data).
- `src/<app>_app.{h,c}`: its struct starting with `fude_study`, its screens'
  order, its info, its side panel's entries, its extension
  (`FUDE_STUDY_EXTENSION` plus its own).
- A strings tool reading the core's, the study's and its language's layers,
  then its own.
- Its language (`fude/lang/<code>/`, below) in its sources, and its data bake.

## The language layer

The study layer teaches whichever language its app compiles: it asks
`fude/lang/lang.h`, and each app compiles one implementation, in
`fude/lang/<code>/` (Kana: `fude/lang/ja`; Hanzi: `fude/lang/zh`; Hangul:
`fude/lang/ko`; Thai: `fude/lang/th`; Hindi: `fude/lang/hi`; Arabic: `fude/lang/ar`). The binding is at link time: a
study app lists `fude/lang/<code>/lang.c` (and, on iOS, `lang_ios.m`) with its
sources, and nothing else changes.

What a language gives (`lang.h` has each, with Japanese's answer):

| What | Asked by | Japanese |
|---|---|---|
| Codes: translation, the handwriting model, the voice | `services/` (`translate`, `mlkit_ios.m`, `speech_ios.m`), the screens that translate | `ja`, `ja`, `ja-JP` |
| The text recognizer's options (iOS) | `services/textscan_ios.m` | `lang_ios.m`: `MLKJapaneseTextRecognizerOptions` (the only file that links the Japanese model) |
| Whether ML Kit can read its printed text (`fude_lang_text_readable`) | Text from a photo, offered only then | yes (Thai: no; ML Kit has no Thai model) |
| Which characters combine with the one before (`fude_lang_combining`): a vowel sign or mark, practised over a dotted circle, taken with its letter in a word test's box and written over it by Paste text | `chars/glyph.c`, `screens/wordexam.c`, `handwriting/textink.c` | none (Thai's ั ิ ่, Hindi's ि ् ं, Arabic's harakat) |
| The writing direction (`fude_lang_rtl`), the form a letter takes between its neighbours (`fude_lang_form`) and the letter a form is of (`fude_lang_letter`) | the word test (boxes from the right, each letter's form), Paste text (joined forms meeting at their joins), exams and recognition (a form is right when its letter is read) | left to right, each letter itself (Arabic: right to left, its joined forms, ﻻ) |
| Groups: the kinds of character, each a script's letters (read as themselves, with a Latin name) and/or a set learnt whole (an exam source, a statistics row, of its core characters) | the catalog, Browse's chips, exams, statistics, the viewer, sheets | hiragana and katakana (script, set), kanji |
| Levels: how many, the byte each is stored as, short and long names | the catalog, Browse, exams, statistics, the viewer, practice, sheets | JLPT N5..N1 as 5..1, "N5" / "JLPT N5" |
| Readings: up to two kinds, their names, how Latin letters read, how they are compared, which kind a one-character word is read by, whether words are read in Latin letters | the catalog's search and sorts, the word card, Check, Vocabulary's search, vocab | on and kun; romaji to hiragana; katakana as hiragana; kun then on; no (Korean: yes, 학교 hakgyo) |
| Badges: the characters headers and labels wear | the viewer, exams, Vocabulary, word exams | 音 訓 部 試 語 似 記 |
| The word splitter (`fude/lang/wordsplit.h`) | Text from a photo, page text, the word card, the viewer | longest match plus conjugation (`wordsplit.c`) |
| Strings (`strings.py`) | the study's ids that name the language (Text from a photo's hints, Into Japanese, ML Kit's model, the voice, the word form's reading, the exam intro, the sheet's credit) and the ids `lang.c` returns | `fude/lang/ja/strings.py` |
| The character data (`bake.c`, the format in `study/chars/kanji.h`): its level byte and two reading lists are the language's to fill | Kana's `--bake` | KanjiVG, KANJIDIC2, JMdict, Tatoeba, the JLPT lists |
| Its own screens, which its app puts in its table | the app | the kana chart (`chart.c`; Korean's: the Hangul chart; Thai's and Hindi's: their alphabets) |

Numbers that are saved keep their meaning across languages because they are
counted from the language's own lists: a filter (`catalog.h`) is all, each
group, each level, studying, known; an exam source (`exam.h`, kept in the exam
log) is studying, known, each level, each set, selection, review. For Japanese
they are the numbers Kana always had.

**Still Japanese-shaped in the study layer** (fine for Kana; to look at when a
language needs it):
- The handwriting reader of its own (used when ML Kit is off): `segment.c`'s
  rules for where characters split, and `match.c`'s marks of written text
  (、。ー々・！？). ML Kit, on by default, reads every language.
- The catalog's default order goes by school grade (`fude_catalog_grade_key`:
  Japan's Kyouiku grades); another language leaves the grade at 0 and the
  order falls to frequency.
- Browse's Parts toggle is labelled 部.
- The file formats' names: `.kana` files, the `KANA` header, `KANABKP`. They
  are formats, not the app.
- The save folder's default name, `kana`, on a device. Kana's users' saves are
  there; another app names its own.

**A new language** (`docs/chinese_korean_data.md` has what Chinese and Korean
use, `docs/hindi_thai_data.md` what Hindi and Thai use):
1. `fude/lang/<code>/lang.c` answering `lang.h`, and `lang_ios.m` with its
   text recognizer's options (and its ML Kit text framework in the iOS build).
2. `wordsplit.c` answering `fude/lang/wordsplit.h`.
3. `strings.py`: every id `fude/lang/ja/strings.py` has, in the language's
   words, plus its own group, reading and prompt names.
4. `bake.c`: its data, in `study/chars/kanji.h`'s format — read from the
   PREPARED tables (`study/chars/bake.h`) its tools write; Thai's and Hindi's
   tools share `tools/data/prepared.py`. Romanization with letters Roboto
   lacks needs the app's `latin_font` (`drawing/app/info.h`).
5. The app (`docs/architecture.md`, "Making a new app"): its strings tool
   reads the core's, the study's, the language's, then its own.
