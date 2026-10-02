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
  widgets/               kit row filterbar notice toolbar pagemenu side draw scroll · icons.h
  ink/                   the page's strokes, the lasso, the view, the canvases
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
fude/lang/ja/            Japanese's own: romaji wordsplit bake chart
apps/kana/               Kana: kana.c (the shell) · src/ (kana_app, welcome, version.h,
                         text_ids.h) · assets/ · platform/ios/ · tools/ · docs/ · site/
apps/draw/               Draw: draw.c · src/text_ids.h · assets/ · tools/strings.py
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
| `fude/study` | the core, `fude/lang/ja` (for now) | the character data, the study screens, their verbs, the word card, the page read as text |
| `fude/lang/ja` | the core, the study | what is Japanese (see the last section) |
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
app's), then the app's `tools/strings.py`. That tool reads the layers it is
built from, adds its own rows (`t`), and writes the app's `src/text_ids.h` and
`assets/text/strings.rdel` (`tools/strings/build.py` checks them first). The
ids are in that order: the core's, the study's, the app's.

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

**A study app** (Mandarin, Korean): copy Kana's shape.
- `apps/<app>/<app>.c`, the shell (Kana's, with its own screens and data).
- `src/<app>_app.{h,c}`: its struct starting with `fude_study`, its screens'
  order, its info, its side panel's entries, its extension
  (`FUDE_STUDY_EXTENSION` plus its own).
- A strings tool reading both layers, with its own words where the study's
  name Japanese (below).
- Its language folder (`fude/lang/<xx>/`) and its data bake.

## The language layer: what is Japanese

The core is language-neutral. The study layer still has Japanese in it, in the
places below. A Mandarin or Korean app keeps the layers and replaces these.

| What | Where | Japanese now | Mandarin | Korean |
|---|---|---|---|---|
| Character data | `lang/ja/bake.c` → `characters.kana` (format: `study/chars/kanji.h`) | KanjiVG strokes, KANJIDIC2, JMdict words, Tatoeba sentences, JLPT lists | Make Me a Hanzi (strokes), CC-CEDICT (words), Tatoeba, HSK lists | jamo strokes (a small set, drawable by hand), a Korean dictionary, Tatoeba, TOPIK lists |
| Readings | `study/chars/kanji.h` (on and kun), `study/screens/viewer.c` (音 訓 lines), `study/chars/catalog.c` (search by reading) | on / kun | pinyin with tones | Revised Romanization; hanja readings optional |
| Script tables | `lang/ja/romaji.c` (gojūon, romaji ↔ hiragana), `lang/ja/chart.c` (the kana chart screen) | kana | none (pinyin input instead) | jamo and syllable composition: a jamo chart screen |
| Levels and groups | `study/screens/browse.c` (filter chips), `study/screens/exam.c` (sources), `study/screens/stats.c` (groups) | JLPT N5–N1, hiragana, katakana, kanji | HSK 1–6, simplified / traditional | TOPIK, jamo, syllables |
| Words in text | `lang/ja/wordsplit.c` (longest match + conjugation), `study/models/vocab.c` (a character's first reading) | Japanese conjugation | longest match, no conjugation | particles and conjugation (different rules) |
| On the device | `study/services/`: `mlkit_ios.m` (ink `ja`), `textscan_ios.m` (Japanese text model), `translate` (`ja` and the reader's, either way: the page and photos into theirs, Into Japanese into Japanese), `speech_ios.m` (ja-JP voice) | `ja` | `zh-Hani-CN` / `zh-Hani-TW`, the Chinese text model, zh-CN | `ko`, the Korean text model, ko-KR |
| The script's font | the app's info (`script_font`) | Noto Sans JP | Noto Sans SC / TC | Noto Sans KR |
| Words in the UI | `fude/study/strings.py` (some name Japanese or Kana: ML Kit's states, Paste text's notice), the badges in `study/widgets/header.c`'s callers (試 語 音 訓 部 似 記) | | | |

Also still Kana-shaped, and fine as they are:
- The file formats' names: `.kana` files, the `KANA` header, `KANABKP`. They
  are formats, not the app.
- The save folder's default name, `kana`, on a device. Kana's users' saves are
  there; another app names its own.

**The next step, when the second study app starts.** Gather the Japanese parts
behind one interface, `lang.h`, with a Japanese implementation:
- the language codes and voice
- the reading kinds and their labels
- the levels
- the script chart (or none)
- the word splitter
- romanization to script
- the study strings that name the language

Do the language step only then: with one language, an interface is guesswork.
The table above is the list of what goes behind it.
