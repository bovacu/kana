# The architecture

How the code is put together: nine apps (and Draw, the drawing core alone, built to check it), built from one shared
library, `fude/` (after 筆, the brush), on the engine RDE (`~/RDE`). This document is the map. Each module's header
has the details (its overview says where it sits, who owns what, which threads call it, the files it writes, and why
it is built as it is), and `docs/coding_rules.md` has the rules every file follows. The parts that need more than a
header are explained in the design notes in `docs/design/` (its `README.md` lists them); this document points to each
where it applies. `docs/infinite_canvas_design.md` is the canvas's history, decision by decision: read it for why,
and the code (and the notes) for what is true now.

---

## The apps

| App | What it is | Built from | Its shell |
|---|---|---|---|
| Kana | Japanese: kana and kanji, written by hand | `drawing` + `study` + `lang/ja` | `study/app/shell.c` (`apps/kana/kana.c` describes it) |
| Hanzi | Chinese characters | `drawing` + `study` + `lang/zh` | the same shell |
| Hangul | Korean | `drawing` + `study` + `lang/ko` | the same shell |
| Thai | the Thai script | `drawing` + `study` + `lang/th` | the same shell |
| Hindi | Devanagari | `drawing` + `study` + `lang/hi` | the same shell |
| Arabic | the Arabic script (right to left) | `drawing` + `study` + `lang/ar` | the same shell |
| Sketching | the deep-zoom canvas with everything in it (the developer's build) | `drawing` + `zoom` + `sim` | `zoom/shell.c` (`apps/sketching/sketching.c` describes it) |
| InfiNote | the canvas as a notebook: writing, PDFs, maths, recordings | the same as Sketching | the same shell (`apps/infinote/infinote.c`) |
| InfiniWorkshop | the canvas for projects: circuits, mechanisms, plans, wood, sewing | the same as Sketching | the same shell (`apps/infiniworkshop/infiniworkshop.c`) |
| Draw | the drawing core alone, to check that it builds and runs on its own | `drawing` | `apps/draw/draw.c` |

A study app is a description handed to the study shell (`study/app/shell.h`: its info, its extension, its data
bake's config), and a canvas app one handed to the canvas shell (`zoom/shell.h`: its info, its product, its home
screen's words, whether New makes a notebook, its welcome, whether it opens on its home screen). What a canvas app
offers is its product's row (`zoom/product.h`), and the canvas asks the row what is offered, never which app it is.
`docs/product_split.md` says why the canvas became three apps.

## The folders

```
fude/drawing/            the drawing core: every app has it
  base/                  files, saves, words, colours and drawing with them, no UI: kfile (the KANA file format)
                         save (where files go, how they are read back) backup (Your data's one file) text (the
                         strings) theme utf8 draw clock crash android (the Java bridge) · icons.h
  ink/                   the page's strokes (ink), the lasso, the canvas (the view and its pages), the canvases'
                         list (notes), a page's file (pagefile), ML Kit's handwriting reader (mlkit, mlkit_ios.m,
                         mlkit_android.c)
  doc/                   documents under a canvas's ink: pdf (each platform's PDF library: pdf.c and pdf_kit.m on
                         Apple's, pdf_android.c) · worker (the PDF worker: a page's pieces drawn on a thread, for
                         doc and zoom's pdfview) · doc (pages, their textures, search)
  widgets/               UI pieces that know no app: kit (buttons, labels, fields, and a form: a card asking for
                         something) row (a screen's buttons) filterbar notice
  app/                   the app shell: app (the screens' table, the pointer) · ui (the retained UI) and its parts:
                         toolbar side (the side panel, Settings; side_list its canvases' list; side_parts.h what
                         the two share) pagemenu docbar crashcard readcard handwriting (Settings' Handwriting: ML
                         Kit's switch and its model) · library
                         import (Files, Photos, the document camera; import_ios.m, import_android.c) · page (writing
                         on the page) · session (opening and saving) · settings · documents · look (a developer's
                         launch flags) · screen.h info.h extension.h
  strings.py             its UI strings (strings_<code>.py: them in a study app's own language)
fude/study/              what the six study apps share
  app/                   shell (the shell itself) · study (fude_study, the hooks) · verbs (one screen asking for
                         another) · files (opening the study's files) · look · demo (six weeks of a learner, for
                         screenshots) · welcome
  chars/                 the character data (kanji: the file, read in place) · catalog (filters, orders, search) ·
                         glyph (a character drawn from its strokes) · wordsplit (a line's words) · bake (making the
                         data file)
  handwriting/           match (the app's own reader) score recognize (ML Kit, else the matcher) segment guide
                         textink (typed text written out by hand)
  models/                what is learned, without UI: marks examlog history review vocab charnote select sheet
  screens/               viewer browse chart practice album check exam wordexam stats scan vocabview translator
  widgets/               header wordcard pagetext
  services/              the device's services, each a .c with an _ios.m and an _android.c: textscan (text in
                         photos) translate speech
  strings.py             its UI strings (and strings_<code>.py)
fude/lang/lang.h         what the study asks of the language an app teaches
fude/lang/<code>/        one language: ja zh ko th hi ar (lang.c, lang_ios.m, strings.py and strings_<code>.py
                         (none for ja: Japanese is a column of every strings.py), wordsplit.c, bake.c, chart.c
                         where it has a chart of letters (all but zh), and what the language needs of its own: ja's
                         romaji, ko's hangul and syllables.h, zh's traditional.h)
fude/sim/                the simulation under the canvas (sim.h): def (parts' definitions), elab (a custom part
                         flattened), digital (the event engine), logic (the gates' models), chips, sparse (the
                         circuit solver's linear algebra), body (rigid bodies' mass and shape)
fude/zoom/               the deep-zoom canvas: zoom (shared types, f64 transforms), scene (frames, objects, the
                         camera, undo, the journal), index (an R-tree per frame), render, zfile (the canvas's file),
                         page and its modules (the page driver), the tools (select, erase, shape, fill, smooth,
                         instrument, snap...), the topics (circuit, logic, mech, plan, graph, formula...), the
                         exports (export, pdf, dxf, stl, video), media (audio, voice, listen, transcript), the shell,
                         product, look, inktest, home · strings.py (the canvas apps' strings). "The canvas layer"
                         has the map
fude/android/java/       the Java every app needs on Android (com.rde.fude: FudeAndroid (the context, the pickers'
                         fragment, the speech model's download), FudeInk, FudeImport, FudePdf, FudePdfBox,
                         FudeSpeech, FudeVideo)
fude/android/java_study/ the study apps' only (FudeText: text in photos; FudeTranslate)
apps/<app>/              <app>.c (the description, or Draw's shell) · <app>.rdeproj (its build: "The builds") ·
                         src/ (version.h, the generated text_ids.h) · assets/ · platform/ios/ and
                         platform/android/deps.lock (all but Draw) · tools/ (a study app's strings.py and
                         strings_<code>.py, its data/ and strokes/ tools; Kana's lectures/ and store/) · site/ and
                         store/ (the released apps: their web pages, their store listing) · docs/ (Kana's)
tests/                   a suite per group of modules (tests/README.md; support/: the doubles they link)
tools/                   build/app.sh (an app's build, from its project file) · check/ (build_all, shots,
                         warnings, rules.py, pngdiff.py) · strings/build.py · icons/ ·
                         mlkit/ · whisper/ (whisper.cpp's libraries) · data/ (prepared.py: what Thai's, Hindi's and
                         Arabic's data tools share; each language's own are in its app's tools/data/) · licences/
                         (build.py: every app's Libraries document) · store/ (site.py: the apps' web pages) ·
                         hershey/ · lectures/
```

Each header is beside its source and included by its layer's path (`#include "drawing/widgets/row.h"`,
`"study/app/study.h"`), so a file's includes show which layers it uses. The include paths are `-I<project>/fude` and
the app's own `src/` (where its generated `text_ids.h` is). Each app's project file takes its layers' folders by
pattern, so a new file in a layer is built without editing any list. A platform file is named after its
platform and built only there: `*_ios.m` on iOS, `*_apple.m` and `pdf_kit.m` on iOS and the Mac, `*_android.c`
(and `drawing/base/android.c`, the Java bridge) on Android. The Java in `fude/android/` is not a C layer: C reaches it
only from a `*_android.c` through `base/android.h`, and every app's project file builds `java/` into it (its
`[android] java`), the study apps' `java_study/` too.

## The layers

Each uses the ones under it, never the ones over it. `python3 tools/check/rules.py` checks it (its `layer` count).

| Layer | May include | Is |
|---|---|---|
| `fude/drawing/base` | RDE, the app's `text_ids.h` | files, saves, strings, colours, drawing primitives: no UI |
| `fude/drawing/ink` | `base` | strokes, the lasso, the canvas, the canvases' list, a page's file, the handwriting reader |
| `fude/drawing/doc` | `base`, `ink` | PDF documents under a canvas's ink |
| `fude/drawing/widgets` | `base`, `ink`, `doc` (it uses only `base` today) | UI pieces that know no app |
| `fude/drawing/app` | all of `drawing` | the app shell: the page, the screens' table, the toolbar, the side panel, Settings, Your data |
| `fude/study` | `drawing`, `fude/lang/lang.h` (the interface, not a language) | the character data, the study screens, their verbs, the word card |
| `fude/lang/<code>` | `drawing`, `study` | one language: what `lang.h` asks, its strings, its data bake, its chart of letters (drawn by the study's chart screen) |
| `fude/sim` | RDE | the simulation: logic, the circuit solver's algebra, bodies |
| `fude/zoom` | `drawing`, `sim` | the deep-zoom canvas and everything drawn on it ("The canvas layer") |
| `fude/android` | (Java) | what each platform service needs on Android, reached from C only through `drawing/base/android.h` |
| `apps/<app>` | all of them | what it is (`info.h`) and what it adds (`extension.h`): a study app's side panel entries and lectures, a canvas app's product, home screen words and welcome |

Nothing in `fude/` names an app: an app's name, its id and what it offers come from its description, and rules.py's
`app-identity` count checks it (every app's own symbols, such as `THAI_VERSION`, and its name as a log prefix; spelt
out in a string, only the canvas apps' names, since the study apps' are also their languages' names). Inside each layer: models keep what is learned and written
and know nothing of the screen (most are tested on their own); widgets know no screen; screens fill the screen over
the page and declare their rows of buttons; the app owns everything.

## The app, as each layer knows it

One struct, two views of it. The study's struct starts with the core's, so a
hook handed the core's `fude_app*` finds the rest by a cast:

```c
typedef struct fude_app   { ... } fude_app;      // drawing/app/app.h: the page, the screens, the UI
typedef struct fude_study { fude_app app; ... }  // study/app/study.h: FUDE_STUDY(_app)
```

- **`fude_app`** holds what the core needs: the window and font, the page (ink,
  canvas, lasso, canvases), the screens' table in stack order
  (`screens[FUDE_APP_SCREENS]`, `screen_count`), the UI, the pointer. Plus two
  pointers to what the app is and adds: `info` and `ext`.
- **`fude_study`** adds the character data and its catalog, the study screens
  (the language's chart, the welcome and the Library among them), Select mode's
  ticks, and its own state: the page read as text, the word card, a practice
  sheet asked for, and the reviews due as the side panel counts them. (Settings'
  Handwriting is the core's: `drawing/app/handwriting.h`, in `fude_ui`.)
- **A canvas app** has no struct of its own: the canvas shell keeps a plain
  `fude_app` and the deep-zoom page (`fude_zoom_page`) beside it, as statics of
  `zoom/shell.c`.

Every study app runs on one shell, `study/app/shell.c`: what RDE calls, the
objects it owns, the screens' order (`FUDE_STUDY_SCREEN_`). An app is a
description of itself (`fude_study_shell_def`: its `fude_app_info`, its
extension with its side panel's entries and its lectures) and a `main()` that
hands it to `fude_study_shell_run`: `apps/kana/kana.c`, about 80 lines.

The verbs, from one study screen to another, are the study's
(`fude_study_view`, `fude_study_practice`, `fude_study_exam`, `fude_study_sheet`,
`fude_study_review`...). They take the `fude_app*` every press and update is
handed.

## How an app runs

An app is a `main()` that calls RDE's `rde_run` with its callbacks: init, event, fixed update, update, late update,
render, crash and end. RDE calls init once, then each frame every pending event, the fixed update (zero or more
times, at a fixed step), the update, the late update and the render, and end when it stops. Every app leaves the
fixed update empty (nothing here runs at a fixed step); its late update, after RDE's UI has moved, only decides
whether the frame is drawn (`fude_redraw_frame_end`: "Render on demand", below); and its crash callback only logs:
the crash report is `crash.h`'s. Everything they do runs on the main thread.

A study app, with `apps/kana/kana.c`, `fude/study/app/shell.c`, `fude/drawing/app/app.c` and
`fude/drawing/app/session.c` open:

1. **`main()`** hands `KANA_SHELL` (its `fude_app_info`, its extension, the bake's config) to `fude_study_shell_run`.
   A release keeps only the program's name of its arguments, for the app and for RDE, which reads arguments as config
   overrides. Then `rde_run`, with `./assets/config.rdef`.
2. **Init** (`fude_shell_init`), each step needing the ones before (`--bake` runs the data bake instead and stops):
   - the window's units (`fude_app_window`: dp on Android), the UI's language (the device's until the saved one is
     read), the camera, the top overlay (the notice) and the launch flags;
   - the page's models (ink, canvas, lasso, notes), the character data (`fude_kanji_load`; without it the screens
     that need it stay empty) and every screen;
   - the `fude_study`, with its `fude_app`, and `fude_app_start`: Android's Java, the save folder from the app's id,
     the crash handlers, the window's title;
   - the screens' table (`fude_shell_place_screens`), the study's own state (`fude_study_init`: the page read as
     text, the word card), the page, and the UI (`fude_ui_init`: the fonts, then every widget);
   - `fude_look_start`, `fude_session_load` (the canvases, the study's files, the settings, the open page),
     `fude_look_loaded`, and ML Kit's model for the language taught (`fude_mlkit_set_model`, then
     `fude_mlkit_prepare`: downloaded the first time, on iOS and Android, when ML Kit is on).
3. **Each event** (`fude_shell_event`). Going to the background, or memory running short, trims the fonts' glyph
   memory. Leaving (will or did enter the background, terminating) saves at once (`fude_session_save_on_exit`) and
   pauses Text from a photo's camera. Then the pointer goes to the screen on top (`fude_app_screen_event`), or to the
   page (`fude_page_event`) when none is open.
4. **Each update** (`fude_shell_update`, timed by `look.c` for `--perf`): the look's frame; the UI built again if the
   language changed, and the data's meanings with it; the study's frame (`fude_study_update`: Select mode ends with
   no ticking screen, a better voice is offered, the voice card shows once); then the screens' frame,
   `fude_app_update`. It applies the interface's size, locks a tablet to portrait (unless the app turns,
   `info.turns`), handles Back (Android's, on its release) and Escape by closing the topmost thing open, resumes a
   screen that has come back on top, and runs the top screen's update. With no screen open the page has the frame
   (`fude_page_update`: an import, the document, the canvas, the keys). Then the UI (`fude_ui_update`), the session
   (`fude_session_update`: a canvas chosen in the side panel, Your data's requests, the autosave) and a practice
   sheet asked for.
5. **Each render** (`fude_shell_render_frame`): the theme's page colour (set every frame: the engine resets it), the
   first open screen that is not an overlay (`fude_app_render`, mirrored when the UI is right to left), or else the
   page and the HUD, then the open overlays (the welcome, the read card). RDE then draws the UI's canvas (the
   toolbar, the rows, the side panel), and last the top overlay: the notice.
6. **The end** (`fude_shell_end`, a computer's quit): it saves, then frees everything in about the reverse order it
   was made. A tablet usually kills the app in the background instead, which is why the session saves on leaving and
   a second after each change.

**A canvas app** runs the same steps with the deep-zoom page in place of the ink page. Read it with
`apps/infinote/infinote.c`, `fude/zoom/shell.c`, `fude/zoom/page.c` and `fude/zoom/page_frame.c` open
(`docs/design/page_driver.md` follows the page further):

1. **`main()`** hands its `fude_zoom_shell_def` to `fude_zoom_shell_run`: its `fude_app_info`, its product
   (`product.h`), its home screen's title and New button, whether New makes a notebook (and of which pages), its
   welcome (or none, and then no Tutorial), and whether it opens on its home screen. A release keeps only the
   program's name of its arguments, as a study app does.
2. **Init** (`fude_zoom_shell_init`), each step needing the ones before:
   - the window's units; no taught language (`fude_text_drop_taught_language`: a canvas app has no fourth UI
     language) and the device's; the product (`fude_zoom_product_set`), and the workshop's themes when it has them;
     the camera, the top overlay and the launch flags;
   - the core's ink, canvas, lasso and notes, and the app with the shell's extension (`FUDE_ZOOM_SHELL_EXTENSION`,
     its Tutorial only when the app has a welcome); `fude_app_start`;
   - the core's page, then the deep-zoom page (`fude_zoom_page_init`: its renderer, the maths font, the symbols' SVG
     art, the diagram library's tiles, the circuit and the mechanism's worlds), then the home screen, the app's one
     screen (`app.screens[0]`), and the UI;
   - `fude_look_start`, `fude_session_load` (the last canvas opened on the deep-zoom page through the page kind:
     `fude_zoom_page_open` reads `notes/<id>.zoom` and its side files), `fude_look_loaded`; the handwriting model set
     to the language the app is shown in (`fude_mlkit_set_model(fude_zoom_ink_model())`: fetched the first time
     handwriting is read, not at launch); the home screen opened, with its welcome over it once, when the app opens
     there and was launched with no flags; and the canvas's own looks (`fude_zoom_look_start`).
3. **Each event** (`fude_zoom_shell_event`). The background or short memory trims the fonts' glyphs and the page's
   caches (`fude_zoom_page_trim`); leaving pauses a recording (kept as it is: the app may not come back) and saves at
   once. Then the event goes to the home screen when it is open, or to the page (`fude_zoom_page_event`; the hands'
   state machines are `page_input.c`'s).
4. **Each update** (`fude_zoom_shell_update`): the look's frame; the UI built again if the language changed; the
   bar's tools following the topic chosen (`fude_zoom_page_topic_follow`); a cover the page wrote again forgotten by
   the home screen; then the home screen's frame when it is open (`fude_app_update`), else the page's
   (`fude_zoom_page_update`, `page_frame.c`). The page's frame runs in a fixed order: a flick, an example or template
   being filled, the instruments, the layers, what is under way (the pickers, handwriting being read, an export, a
   flight, a pen resting at a stroke's end), the bar, the keys, a long press, the mouse, the camera settled into the
   right frame, the selection, Play's step, a PDF's and a notebook's pages, the recordings, and last the journal
   flushed to disk. Then the canvas's looks, the UI and the session.
5. **Each render** (`fude_zoom_shell_render_frame`): first what is drawn off screen
   (`fude_zoom_page_render_offscreen`: a cover, the map, a strip's page, a video's frame, an arrow's preview, a PNG
   export); then the theme's page colour, and the home screen or the page (`fude_zoom_page_render`, `page_render.c`:
   the paper, a PDF's pages, the canvas through the renderer, what plays, the stroke being drawn, the selection, the
   measures and guides, the instruments, the page's panels); then the overlays. RDE draws the UI's canvas, and last
   the top overlay (`fude_zoom_shell_render_top`): the Formula card's preview and the notice. The update and the
   render are both timed, for the frame rate's readout (`fude_zoom_page_perf_frame`).
6. **The end** (`fude_zoom_shell_end`): it saves, then frees the home screen, the page, the UI and the core's
   models.

Draw (`apps/draw/draw.c`) is the shortest: its shell is its own file, with the core alone, no screens and no
extension.

**When the session saves** (`session.c`). Each frame it compares what is on screen (the ink's revision, the view,
the page under the ink, the settings gathered from the app, the app's own page's revision) with what it saw last
frame and with what it last wrote. It writes what differs:
- a second after the last change (`FUDE_SESSION_AUTOSAVE_DELAY`), never while a stroke is open or the lasso drags,
  so a file never holds half a stroke; a failed save is tried again after another quiet second;
- on leaving the app and at the end: whatever differs, and the app's own page whole;
- before another canvas opens, and before Your data exports or replaces.

The study's files are not the session's: each model writes its own as the change happens (the marks, words and
notes on every change, the reviews after a sitting, an exam or a practice session appended as it ends). Leaving
also marks the session as ended cleanly for the crash reports (`fude_crash_session(false)`); each frame marks it
running again.

**When a canvas saves** (`page_file.c`, `zfile.h`). A canvas's file is a log, never overwritten in place: each change
goes into the scene's journal as it is made, and the file takes it from there.
- About every second (`FUDE_ZOOM_PAGE_FLUSH`) while there is anything new and nothing is being drawn or erased, the
  page's frame ends by appending the journal (`fude_zoom_page_save_now` through `fude_zoom_file_flush`: a `JRNL`
  chunk with its CRC, synced). A checkpoint (the changed buckets, the frames, the history, the index, the footer)
  follows once 256 KB of journal has piled up (`FUDE_ZOOM_FILE_CHECKPOINT_BYTES`), and a compaction (the whole file
  written again through `.tmp` and `.bak`) once the bytes no index points at outnumber the live ones and pass 64 KB.
  So a kill loses at most about a second of drawing, and opening replays every whole journal after the last
  checkpoint.
- The session sees the page kind's revision (the scene's) move and, a quiet second later, saves through the kind
  (`fude_zoom_kind_save`), never while a stroke, an erase, a shape or the lasso is under way (`fude_zoom_kind_busy`).
- Leaving the canvas (`fude_zoom_page_close`, as another opens) or the app (`fude_session_save_on_exit`) flushes,
  then checkpoints if anything was journaled since the last or the camera moved (only a checkpoint keeps where the
  camera is, and a canvas opens where it was left); a template being edited is kept first. Closing also writes the
  canvas's words for Find and closes a recording.

The instruments' `<id>.tools` is written with the canvas when they changed; the other side files by their owners as
they change (`audio.h`, `pages.h`, `transcript.h`); the cover once nothing has changed for 4 seconds
(`FUDE_ZOOM_PAGE_COVER_WAIT`). `docs/design/scene_and_index.md` has the file's chunks and how it is read back.

**The launch flags** (`look.c`). A developer's build reads them; a release reads none, unless it is built with
`FUDE_ALLOW_ARGS` to be measured on a device (Draw's shell reads them in a debug build only). `fude_look_args`
reads them before anything is built, since some change what is built; `fude_look_start` does what needs no saves,
`fude_look_loaded` what needs them (a document, a theme shown but never saved); `fude_look_frame` runs a shot's
sequence, each flag at its frame, the shot at frame 45 or later (`fude_look_shot_at`: later with a document or
presses, or where `--shot-at` says), then quits five frames after (`--stay`: no shot, no quitting). A study app's
own flags are in `study/app/look.c`, the canvas apps' in `zoom/look.c` (and the page's in `zoom/page_look.c`).

## Render on demand

A still screen is not drawn again every vsync. A frame nothing changed in is left as it is (the window keeps showing
its last picture: nothing is shaped, tessellated, uploaded or presented), and the app sleeps until an event, a timer or
another thread wakes it. `drawing/base/redraw.h` decides, on RDE's render on demand (`rde_engine_set_render_on_demand`,
`rde_engine_frame_unchanged`, `rde_engine_wake_after`, `rde_engine_request_frame`: RDE's
`05_rendering_foundation.md`, "Render on demand"). Measured on the Mac in debug builds, which still draw a still
screen once a second (the check, below), Kana's Browse went from 17.6% of a CPU to 0.1% and InfiNote's canvas from
20.6% to 0.1% (`docs/quality_plan.md` 4.10).

**The rule** (`fude_redraw_end_frame_at`), applied once a frame by `fude_redraw_frame_end` from each shell's late
update (`study/app/shell.c: fude_shell_on_late_update`, `zoom/shell.c: fude_zoom_shell_on_late_update`, Draw's
`on_late_update`): late, so RDE's UI has moved and can say so. A frame is drawn when
1. every frame is: a launch with flags (`look.c: fude_look_args`, through `fude_redraw_set_continuous`). The looks
   count frames, and `--perf`, `--gate` and the screenshots must measure and look exactly as before; `--on-demand`
   keeps render on demand with other flags. A release takes no flags, so it always draws on demand;
2. something changed in this frame or the 3 before it, or within the last half second (`FUDE_REDRAW_GRACE_FRAMES`,
   `FUDE_REDRAW_GRACE_SECONDS`): an event, a report below, RDE's UI moving (`rde_ui_get_wake_after` is 0), or the
   app's start (`fude_redraw_start`, first thing in each shell's init). The grace period draws what settles a frame
   or two after a change without reporting it (a layout resolved the frame after, a picture decoded a frame later, a
   short fade). A count of frames alone is too short for a fade at 120 Hz, and a time alone too few frames after a
   slow one (a file opened in one 600 ms frame);
3. a timer is due: one asked for with `fude_redraw_after`, or the UI's next change (a focused field's caret). It draws
   its one frame with no grace period after it, or a blinking caret would draw every frame. A source drawn from the
   clock cannot tell in its update that its picture changed, so the frame it wakes is drawn for it.

Otherwise `fude_redraw_frame_end` calls `rde_engine_frame_unchanged`, and `rde_engine_wake_after` with the soonest
timer kept, or the check's next probe if sooner (a timer is kept as a time on the engine's clock until it is due, 2 ms
early at most, `FUDE_REDRAW_EARLY`, since RDE sleeps in whole milliseconds); with neither, RDE sleeps until an event or
a request. It also sets RDE's render on demand each frame, off while every frame is drawn, so a launch with flags runs
exactly as without it. RDE itself always draws after a window event that changes what the window shows, and after a
return to the foreground.

**No heartbeat.** Nothing else draws: a still screen is never drawn again, however long it waits. The first version drew
one frame a second whatever happened, so that a source that forgot to report showed at most a second late; the owner
dropped it (2026-10-18). It hid the very bug it was there for, and it drew a frame a second on every still screen
(Hanzi's Browse spent 3.3% of a CPU on it in a debug build). Without it a forgotten source leaves the screen stale until
the next touch, a visible bug, and a developer's build names it (the check, below).

**Who reports.** Everything that changes the screen with no event reports itself where it lives, with one call:
- `fude_redraw_changed`: it moved this frame, or moves every frame while it runs (from its update or its render: a
  render's report counts for the next frame);
- `fude_redraw_after(seconds)`: it changes on its own then (the soonest asked for wins);
- `fude_redraw_after(FUDE_REDRAW_POLL)`: it waits on an answer it polls, which comes with no event (a platform's,
  read over JNI or from the main queue): looked at again 10 times a second while it waits;
- `fude_redraw_request`: another thread has something new (any thread: its flag is under a mutex, and it wakes RDE
  with `rde_engine_request_frame`).

The table names them changed, after (after 0: the next frame), poll and request.

| Where | What changes with no event | Call |
|---|---|---|
| the shells' events: `study/app/shell.c: fude_shell_event`, `zoom/shell.c: fude_zoom_shell_event`, Draw's `on_event` | whatever an event does | changed |
| the shells' updates: `fude_shell_update`, `fude_zoom_shell_update`, Draw's `draw_update` | Apple Pencil's double tap, listened for each frame for the first 120 (iOS) | after 0 |
| `study/app/shell.c: fude_hud_render` | the study HUD's numbers, every frame it shows | changed |
| RDE's UI, read by `fude_redraw_frame_end` (`rde_ui_get_wake_after`) | its colour blends, scroll areas' flings and springs, toggles, toasts, spinners; a focused field's caret, a held finger, a tooltip | changed; timer |
| `drawing/ink/canvas.c: fude_canvas_update`, `fude_canvas_long_press` | a safe area arriving after launch; the page's flick; a finger held still | changed; after |
| `drawing/widgets/kit.h: fude_kit_scroller_update` (RDE's `rde_ui_scroller`) | a list's fling | changed |
| `drawing/widgets/notice.c: fude_notice_show`, `fude_notice_render` | a notice shown (often by a job's answer); held, then fading | changed; after, then changed |
| `drawing/app/page.c: fude_page_finger_poll`, `fude_page_render_zoom` | a finger's wait before it writes, then its long press; the zoom pill, held then fading | after; after, then changed |
| `drawing/app/pagemenu.c: fude_pagemenu_row_faces` | "Copied" on a button, going back | after |
| `drawing/app/readcard.c: fude_readcard_render` | the reading card's countdown, each whole second | after |
| `drawing/app/session.c: fude_session_autosave` | the autosave, a quiet second after a change (and again after a failed one) | after |
| `drawing/app/ui.c: fude_ui_update`, `side.c: fude_side_update`, `docbar.c: fude_docbar_update` | a safe area arriving a frame or two after launch; the side panel's app button following a platform's state, and its list scrolling under a drag held at its edge | changed |
| `drawing/app/handwriting.c: fude_handwriting_refresh` | ML Kit's model downloading; its state shown | poll; changed |
| `drawing/app/import.c: fude_import_update` | Android's picked files | poll |
| `drawing/app/app.c: fude_app_update` | Android's orientation lock, until it holds | poll |
| `drawing/app/import.c: fude_import_arrived`, `fude_import_on_files`; `session.c: fude_session_on_export_path`, `fude_session_on_import_path`; `docbar.c: fude_docbar_on_export_path`; `study/app/files.c: fude_study_on_sheet_path`; `zoom/page_export.c: fude_zoom_page_on_export_path` | a picker's files, a file dialog's answer: they may come while the app sleeps | request |
| `drawing/doc/doc.c: fude_doc_update`, `fude_doc_render_pill` | a search gathering its matches; pages probed for text, a few a frame; a page's reading; the rest before the sharp pieces; the page pill, held then fading | changed; after 0; poll; after; after, then changed |
| `drawing/doc/worker.c: fude_doc_worker_work` | a PDF's piece drawn, on the worker's thread | request |
| `study/chars/glyph.c: fude_glyph_box` | stroke-order playback, until done | changed |
| `study/handwriting/guide.c: fude_guide_update`, `fude_guide_render_steps`, `fude_guide_render_over` | guided practice: the next step after its tick; the stroke writing itself, a wrong stroke fading, the tick fading in | after; changed |
| `study/handwriting/recognize.c: fude_recognize_pump` | ML Kit's reading, polled until it comes | poll; changed |
| `study/handwriting/textink.c: fude_textink_update`, `study/screens/browse.c: fude_browse_update` | ML Kit busy: asked again | poll |
| `study/models/review.c: fude_reviews_today` | local midnight, where the reviews' day shows | after |
| `study/screens/album.c: fude_album_date` | a replay, until done | changed |
| `study/screens/check.c: fude_check_update` | the reading, once "Reading…" has been drawn | after 0 |
| `study/screens/exam.c: fude_exam_grade`, `wordexam.c: fude_wordexam_grade` | answers graded one a frame, ML Kit polled | changed |
| `study/screens/practice.c: fude_practice_render` | the demo writing again after its rest | after |
| `study/screens/scan.c: fude_scan_update`, `fude_scan_on_camera` | Text from a photo's live camera (about 30 frames a second); a photo's reading; the camera permission's answer | changed; poll; request |
| `study/services/translate.c: fude_translate_state`, `fude_translate_take` | a translation model downloading; an answer waited for, and its arrival | poll; changed |
| `study/services/speech_android.c: fude_speech_state` | Android's voice check, asked again when the app comes back (`fude_speech_came_back`, from `fude_shell_event`) | poll |
| `zoom/page_frame.c: fude_zoom_page_update_under_way`, `fude_zoom_page_update_views` | a flight; a pen resting at a stroke's end (a shape); a canvas opened from Find, its words found a few frames on | changed; after; changed |
| `zoom/page_frame.c: fude_zoom_page_update_flush`, `fude_zoom_page_update` | the journal's flush, and after any change the instruments' file (`fude_redraw_last_change`); one finger held still (the page's menu) | after |
| `zoom/page_input.c: fude_zoom_page_coast` | a flick | changed |
| `zoom/page_mech.c: fude_zoom_page_play_step` | Play running or taking a Step. Paused, nothing moves: the current's dots and the warning rings run on Play's time, not the clock | changed |
| `zoom/page_audio.c: fude_zoom_page_audio_update`, `fude_zoom_page_audio_permitted` | a recording made or played; the microphone permission's answer | changed; request |
| `zoom/page_speech.c: fude_zoom_page_listen_update`, `fude_zoom_page_render_caption` | the speech model's download and a piece being read; the live caption going | poll; after |
| `zoom/listen.c: fzls_main` | a piece transcribed, on its thread | request |
| `zoom/page_find.c: fude_zoom_page_hand_update`, `fude_zoom_page_read_update` | ML Kit reading handwriting for Find, and its answer; the lines found again a second after a change | poll; changed; after |
| `zoom/page_strip.c: fude_zoom_page_strip_draw`, `fude_zoom_page_render_strip`, `fude_zoom_page_render_everywhere` | the strip's pages drawn one a frame; a page held to lift it, then scrolling under it; Find in every canvas, a few canvases a frame | changed; after; changed |
| `zoom/page_render.c: fude_zoom_page_previews_draw`, `fude_zoom_page_render_marks`, `fude_zoom_page_render_brush` | the arrows' previews, one a frame; the empty screen's marks, found once the camera rests; the brush's circle back after the pen lifts | changed; after |
| `zoom/page_present.c: fude_zoom_page_map_refresh`, `fude_zoom_page_render_present` | the map looked at again and followed, its picture drawn the next frame; the laser's trail fading; the presenting pill, held then fading | after; changed; after, then changed |
| `zoom/page_export.c: fude_zoom_page_render_offscreen`, `fude_zoom_page_cover_draw` | an export's frames; the cover after its quiet wait, read back the frame after | changed; after |
| `zoom/page_settings.c: fude_zoom_page_render_fps`, `fude_zoom_page_inktest_follow` | the frame rate shown (every frame, so it counts them); the test canvas opening | changed |
| `zoom/page_custom.c: fude_zoom_page_examples_follow`, `fude_zoom_page_template_follow`, `fude_zoom_page_library_hold` | an example or a template filled once its canvas is open; a hold on one of My parts in the library | changed; after |
| `zoom/page_text.c: fude_zoom_page_render_inline` | the caret's blink; the view moved up for the keyboard | after; changed |
| `zoom/page_ui.c: fude_zoom_page_ui_update_line_chip`, `page_pieces.c: fude_zoom_page_render_pieces`, `instrument_draw.c: fude_zoom_instruments_render` | a line's length chip going; a piece's × going grey; an instrument's angle going after a turn | after |
| `zoom/home.c: fude_zoom_home_cover_of`, `fude_zoom_home_update` | covers loaded a few a frame; a card held | changed; after |
| `zoom/pdfview.c: fude_zoom_pdfview_update` | a PDF's rest before its sharp pieces; a search's matches | after; changed |

A new animation, timer, poll or worker reports the same way and gets its row here, or the screen is stale until the
next touch. A simulation's animation runs on Play's time (`page.h`: `play_time`), so it stands still while Play is
paused (`docs/design/rendering.md`, "When it draws").

**The check** (`FUDE_REDRAW_CHECK`: a developer's build, `RDE_DEBUG`; never a release or a measuring build, and not
while every frame is drawn). While the rule leaves frames undrawn, the first of them is drawn anyway and its picture
kept, as what the screen shows (the reference); then one frame every second (`FUDE_REDRAW_CHECK_SECONDS`) is drawn and
compared with it, pixel for pixel (`fude_redraw_check_step` plans it, `fude_redraw_compare` compares; the picture is
RDE's `rde_window_take_screenshot_bytes` of the whole window, drawn into a texture and read back). A difference means
the screen changed and nothing reported it. It is logged as a warning ("redraw check: the screen changed and nothing
reported it"), with what shows (`fude_app_shown`: the screen on top, or the page), how many pixels changed in what box,
and the seconds since the last change reported; the box is written out as it was and as it is
(`redraw_check_before.png`, `redraw_check_after.png`, in the working folder). The new picture is kept either way, so a
change is told once, and a source that keeps moving is told every second. What it costs: a frame drawn and read back
every second on a still screen (the read back stalls the GPU for that frame, and in threaded mode the main thread waits
for the render thread to draw it), a copy of the window's pixels (4 bytes each) on the std allocator, and a comparison
that skips rows that are the same. Pixels, because they are what a person sees, whatever drew them, and nothing else RDE
offers tells two pictures apart; a hash of a frame's 2D commands, asked of RDE, would do it without the read back.

The suite `tests/redraw` checks the rule (the grace period, a still screen never drawn again, timers kept until due,
RDE's UI and other threads waking it, continuous runs, 20,000 random frames against the rule), the check (its
reference, probes and comparison), and the scroller's, the canvas's and a notice's reports.

## What the core asks of an app

**What it is: `info.h`.** Its name and version (the window's title, the side
panel's version line, About), its id (its save folder and its backup's
extension, never changed once it has users), the app whose backups it opens
too (InfiNote: Sketching's), its store id (Rate), its script font (the
language's characters, behind Roboto: Kana's is Noto Sans JP), its Latin font
(the letters Roboto lacks, for Thai's, Hindi's and Arabic's romanization), its credits,
its Licences documents (`FUDE_APP_LICENCES_OF`, counted by `sizeof`), its FAQ
page, and whether the screen turns to landscape.

**What it adds: `extension.h`.** Every field may be left out; Draw leaves them
all out. A table's count goes beside it (`tool_count`, `section_count`,
`paper_count`, `library_count`). The right column is Kana's, as every study
app's but for its side panel and lectures; a canvas app's is the canvas
shell's (`FUDE_ZOOM_SHELL_EXTENSION`, "The canvas layer").

| Field | The core's place for it | Kana's |
|---|---|---|
| `tools` | the toolbar, after Paper (a tool marked `by_paper`, just before it): 12 at most, each with 24 choices at most | the camera (Text from a photo), where the device can read text |
| `selection_row`, `context_row` | the page's menus (the core's: Cut, Copy, Duplicate, Delete; Paste, Select all) | those with Copy as text, Translate, Save word, Check; Paste text, Text from a photo |
| `menu_update`, `selection_faces`, `context_faces` | the menus, each frame and at a long press | the reading under way ("Reading…"), what can be pasted |
| `nav` | the side panel's top section | Study: Kanji, Kana, Album, Reviews · n, Vocabulary, Lectures, Exams, Statistics |
| `sections` | Settings, between Paper and About | Handwriting (Google ML Kit: the core's `FUDE_HANDWRITING_SECTION`, listed) |
| `tutorial` | the side panel's Tutorial button (none without it) | the welcome |
| `side_button` | the side panel, above Tutorial, while it says so | Set up the voice, while the device has no voice for the language |
| `ui_build/forget/update/restyle/hit` | its own widgets on the UI's canvas | the word card |
| `page_render`, `page_press` | over the page's ink | Translate's card by the selection, its speaker, its words |
| `session_open`, `settings_gather/apply`, `first_launch` | the session | the study files; none (ML Kit's switch is the core's own setting, `fude_settings.mlkit`); the welcome |
| `text_row` | the page's menu over a PDF's text the lasso took (no ink) | Copy as text, Translate, Save word |
| `page_kind` | the page itself: the toolbar's Undo, Redo, Clear and Reset view, the session's opening and saving of a canvas, its menus' commands, Back | none (every canvas app's: the deep-zoom page, `zoom/page.h`, its hooks in `zoom/page.c`) |
| `library`, `library_count` | the Library screen's books (`doc.h`'s `fude_doc_book`: a PDF and its cover in the assets, its title, what it is, its credit) | the lectures: First Year Japanese I (CC BY 4.0) and Wikivoyage's phrasebook in English, Spanish, French and Portuguese (CC BY-SA 4.0; the Japanese UI shows the English one), each in Kana's edition |
| `picture_read`, `picture_lines` | a document's pages with no text of their own, read once each (`doc.h`) | ML Kit's Japanese text recognition |
| `papers`, `paper_offered`, `brush_on_screen` | the Paper panel, after the core's four; the brush always sized on screen | none (every canvas app's papers: engineering, isometric, hexagons...; no app sets `brush_on_screen` today) |

The study fills most of it: a study app's extension starts with
`FUDE_STUDY_EXTENSION` (`study/app/study.h`), which brings the tutorial and
the first launch too, and adds its own.

```c
static const fude_extension KANA_EXTENSION = {
    FUDE_STUDY_EXTENSION,
    .nav = &KANA_NAV, .library = KANA_LIBRARY, .library_count = (u32)(sizeof(KANA_LIBRARY) / sizeof(KANA_LIBRARY[0])),
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
const fude_screen FUDE_STATS_SCREEN = {                // study/screens/stats.c
    .name = "stats", .input = FUDE_SCREEN_INPUT_POINT,
    .is_open = fude_stats_screen_is_open, .close = fude_stats_screen_close,   // Escape and Back
    .update = fude_stats_screen_update,                // its frame, and what it hands on
    .render = fude_stats_screen_render,
    .pointer_down = fude_stats_screen_down, .pointer_moved = fude_stats_screen_moved, .pointer_up = fude_stats_screen_up,
    .rows = FUDE_STATS_BUTTON_ROWS, .row_count = 1u, .row = fude_stats_screen_row,   // its rows of buttons
    .field_hint = FUDE_TEXT_COUNT,                     // no field of its own
};
```

- **Its rows are data.** Each button is `{ text, icon, press, arg, look,
  counted, present }`. `press` gets the app, the screen and the button's `arg`.
  When a button only calls a function of the screen, use
  `FUDE_ROW_CALL(name, type, fn)`. Each frame, the screen's `faces` (when it has
  one; Statistics has none) says how the buttons show now: "Start 12", greyed
  out, chosen, hidden. The widget changes only what differs, so a screen keeps
  no "what is shown" state of its own.
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
  - `overlay`: drawn over what is under it (the welcome, the read card)

**Adding a screen** (a study screen; the canvas apps have one screen, the home
screen, placed by `zoom/shell.c`'s `fude_zoom_shell_place_home`):
1. Write the module in its layer: a study screen in `fude/study/screens/`, a
   screen every app could have in `fude/drawing/app/` (as `library.c` and
   `readcard.c` are). Apps hold no screens: an app's `.c` is only its
   description, and its project file builds no other file of the app.
2. Fill its `fude_screen`.
3. Give it a place in the screens' order (`FUDE_STUDY_SCREEN_` in
   `study/app/shell.h`). The table holds `FUDE_APP_SCREENS` (16) and the study
   uses 15, so a second new screen means raising it.
4. In `study/app/shell.c`: a static for it, its init in `fude_shell_init` and its
   destroy in `fude_shell_end`, and its line in `fude_shell_place_screens`.
5. A pointer to it in `fude_study` (`study/app/study.h`) when a verb opens it,
   and a case in `fude_study_open` when the side panel does (with an entry in
   the app's `nav`).
6. Nothing to add to a build: every app's project file takes every file of a
   layer's folders.

The UI builds its rows, the app routes to it, and its row shows when it is on
top.

**The stack.** The app's order is the stack's, top first. The first open
screen is on top: it alone gets the pointer, the frame, Escape and its row.
Under every screen is the page. When a screen closes, the one under it comes
back on top and its `resume` runs.

**Input.** One pointer at a time for the screen on top (pen, finger or desktop
mouse), in `fude_app_screen_event`.
- A writing screen (`FUDE_SCREEN_INPUT_WRITE`, Practice) takes a finger only
  with the hand on, and a pen takes over from it.
- A finger the system cancels (a hand resting on the glass as the pen comes
  down, an edge gesture) is let go, and the screen's `pointer_cancel` ends
  what it was doing without a tap. Until it is let go no other press is taken.
- Presses on the UI (`fude_ui_hit`) are the UI's.
- The page has its own input, with the finger-writing state machine
  (`drawing/app/page.c`; the canvas apps' page has its own, `zoom/page_input.c`).
  Fingers the system cancels let go of the page as a pen coming down does: a
  finger's stroke ends, the page's gesture is dropped, and the next finger never
  pinches against one that is gone.

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
- **Content laid out in its own script's direction** is laid out so the mirror
  does not undo it: the word test's boxes check
  `fude_lang_rtl() != fude_draw_is_rtl()`, and a chart section marked
  `mirrored` (`lang.h`; the Arabic chart's) is reversed by the chart screen.
- **An icon that points** (an arrow, Back) is swapped for its pair
  (`fude_draw_icon_dir`).
- **An icon inside a label's text** ("+ Add"): `fude_draw_icon_label`, which
  keeps it at the start of the line in either direction.

**Drawing text.** A screen draws its text through `drawing/base/draw.h`: `fude_draw_text`
and the wrapped and whole texts built on it, or `fude_draw_text_placed` where the code
places a text itself (the canvas does). Never the engine's `rde_rendering_2d_draw_text_2`:
it shapes the text again on every call, and shaping is most of what drawing a text costs.
draw.c shapes each text once and keeps the shape, RDE's glyph runs, drawing it by the
engine's own call, so the pixels are the same ("Caches": text shapes). A grid drawn
tile by tile (a box, its text, the next box) needs nothing of its own: RDE's 2D batch
gathers draws that do not overlap (RDE's `06_rendering_2d.md`, "Layers": within 256
commands, never past a draw it cannot bound), so the boxes are drawn in one run and the
texts in another. Browse is 13 draw calls a frame either way; the tiles that layered
them by hand (`fude_draw_tiles_begin`/`_end`) were taken out on 2026-10-19, every
screen's picture the same bytes without them.

## Documents

A canvas can have a PDF under its ink (`fude/drawing/doc/`). Its note says which
(`fude_note.document`, a byte in the index's record that older builds skip):
`FUDE_NOTE_DOCUMENT_OWN`, the canvas's own `notes/<id>.pdf` (imported from
Files, or pictures and scans made into one); 2 and up, a book of the app's
library, by its id (never reused), read from the assets in place (on Android
copied once into the platform's cache folder, `cache/pdf/`, since PdfRenderer
wants a file it can seek in: `FudePdf.fromAssets`). Removing the canvas removes
its own PDF and its read text. This is the study apps' (and Draw's) document; a
canvas app's PDF is the canvas's own, under its frames (`zoom/pdfview.c`), on the
same PDF worker. `docs/design/pdf_and_threads.md` has both.

The page holds the open canvas's document (`fude_page_input.doc`): it follows
`notes.open`, lays the pages out down the canvas from its origin (each 1000
units wide, room around them for notes), and draws them under the ink from
textures — each page whole, then, once the view rests, what is on screen again at
the screen's pixels. The pieces are drawn on a worker thread, one at a time
("Threads"). A dark theme draws the pages in its own colours, so ink in the
theme's colour shows on them; a theme changed draws them again. A PDF that does
not open, or has no pages, is refused (`fude_doc.failed`): the canvas shows its
ink alone, and the PDF is not tried again while the canvas is open.

While one is open the canvas reads like a PDF viewer (`canvas.h`'s reader
frame): the pages' width fits the screen at 100%, the least zoom; they move
across only when zoomed in, never past the first and last page, and a flick
carries on, slowing. The document bar (`docbar.c`) searches the PDF's own text
(PDFKit a few milliseconds a frame; PDFBox on a thread of its own on Android) and
goes to a page; the lasso, taking no ink, takes the text in its loop's box
(`lasso.h`: an area) for the app's text row. The marker (`ink.h`: a stroke flag)
is see-through and drawn under the other ink; it is never read as writing.

A page with no text of its own (a scan, a photo) is read once, in the
background, by the app's picture reader (`extension.h`'s `picture_read` and
`picture_lines`: Kana's is ML Kit's Japanese text recognition), and its lines
are kept beside the PDF (`<id>.lines`), so the lasso and Search find them as
they find a PDF's own text. A page whose picture could not be drawn
(`FUDE_DOC_TEXT_FAILED`) is not asked again while the document is open, and is
not saved as read, so the next opening tries it again. Share as PDF
(`fude_doc_export`) writes the pages with the ink over them and the read lines
as invisible text. A page can be
turned a quarter at a time (the document bar's Turn page): the canvas keeps the
turns with its page (`canvas.h`'s `fude_page`, saved as 'TURN'), the PDF is
never changed, the ink on the page turns with it and the ink on the pages after
it moves with them, and a page read from its picture is read again.

Each platform draws PDFs with its own library (`pdf.h`). On Apple's, Core
Graphics draws them and writes new ones (`pdf.c`, plain C) and PDFKit reads
their text and searches it (`pdf_kit.m`); the Mac links both (the project files'
`[macos] frameworks`). On Android, PdfRenderer draws them and
makes a PDF of pictures (`FudePdf.java`), and PDFBox (PdfBox-Android,
`FudePdfBox.java`) reads their text, searches and writes Share as PDF;
`pdf_android.c` reaches both. Windows has none yet: `fude_pdf_available` is
false and nothing opens.

Documents come in through `import.c` (Files, Photos, the document camera): each
PDF becomes a canvas of its own, and the pictures picked together become one
more, made into a PDF a page each. An app with its own page takes them itself
(`page_kind`'s `imported`: a canvas app puts pictures on its canvas, and opens a
PDF as a canvas of its own over it, `fude_zoom_kind_imported`). The Library
(`library.c`) is a core screen: an app that has books gives them in its
extension and puts the screen in its table (a study app:
`FUDE_STUDY_SCREEN_LIBRARY`, the side panel's Lectures).

## The UI, and rebuilding it

All retained widgets live on one canvas (`ui.c`) and are built from the app's
state. In stack order, bottom first: the toolbar, the page's menus, the
document bar, the screens' rows, bars and fields, the side panel and Settings,
then the app's own widgets (Kana's word card; the canvas's panels), and the
crash card over everything. When the language changes,
`fude_ui_follow_language` destroys the canvas and builds everything again,
the app's own included (`ui_build`); `fude_ui_rebuild_now` does the same at
once, when a canvas's topic changes what its bar holds
(`page_topics.c`). The only state kept across a rebuild is
what the widgets do not own: the toolbar's tool, place and folding, the side
panel, Settings and the FAQ being open, and the study's reading and card (in
`fude_study`, not on the canvas).

## The strings

Every text the app shows is in its layers' strings files and its own:
`fude/drawing/strings.py` (the core's); for a study app, `fude/study/strings.py`
and its language's `fude/lang/<code>/strings.py` (the study's ids that name the
language, in its words, and the ids its `lang.c` returns); for a canvas app,
`fude/zoom/strings.py`; then the app's own `tools/strings.py`, when it has any.
A row gives a string in English, Spanish, Portuguese (Brazil), Japanese and
French. A study app's fourth language is the one it teaches: Japanese for Kana;
for the others, their `strings_<code>.py` beside each of those files. A canvas
app has no fourth. `python3 tools/strings/build.py <app>` (or `all`) reads the
layers the app is built from (its `APPS` table), then the app's rows (`t`; `o`
for a layer's string in the app's own words), checks them, and writes the app's
`src/text_ids.h` and `assets/text/strings.rdel`. The ids are in that order: the
core's, the study's (or the canvas's), the language's, the app's.

The core's words name no app: where a string needs the app's name it says
`{APP}` ("Rate {APP}", "{APP} works offline…"), which build.py fills from the
app's `src/version.h` (`<APP>_NAME`). A layer or an app gives a lower layer's
string its own words with `o`, keeping its id and place: Kana does that for its
welcome's second line (Kana is feminine in Spanish and French), the study for
three of ML Kit's and Your data's lines, and Japanese for four of ML Kit's.

## Saving and loading

**Where.** `fude_save_dir` (`base/save.c`): on iOS the app's Application Support
(private to it, in the device's backups), on Android its internal storage, each in
a folder named by `fude_save_set_folder`, the app's id (`fude_app_start` sets it
before anything is read or written); on a computer, `./saves/` under the working
directory. A kind with many files has a folder of its own (`fude_save_subdir`:
`notes/`, `practice/`, `pieces/`; and the canvas's `parts/`), and Your data has
two (`outbox/`, what is shared from a tablet; `before-import/`, what an import
replaced).

**The format** (`base/kfile.h`). Most files the apps write (every model, page,
index and setting) are a 12-byte header, "KANA", a version and a kind, then
chunks: a tag, a size, the payload. Numbers are
little-endian, written byte by byte; floats as their bits; 64-bit numbers as two
halves, the low one first. A file is built in memory (`fude_bytes`, `fude_put_*`,
`fude_chunk_begin`/`_end`) and read through a `fude_reader`, which reads zeros past
the end and turns `ok` false for good, so a parser reads a whole record and checks
once. `fude_next_chunk` hands over a reader of one chunk's payload only. Readers
skip tags they do not know, records carry their size, and new fields go at the
end, so old and new builds read each other's files. Each owner documents its
kind's layout in its header. The others are what their contents are: a
recording's raw ADPCM blocks (`.rec<n>`), a cover's PNG, a custom part's text
(`.part`, `templates.txt`), the one byte of `listen.kana`, PDFs, exports, the
crash report, and the backup's own `KANABKP` format (`backup.h`).

**Writing** (`fude_bytes_write_and_free`). The bytes go to `<name>.tmp`, the
current file is renamed `<name>.bak`, and the `.tmp` is renamed into place. A kill
at any moment leaves the old file or the new one, never half of one; between the
two renames there is no main file, and the load then reads the `.bak`.
`fude_file_write` writes a whole file with no `.tmp` or `.bak` (a practice sheet, a
backup's file restored), and deletes a partial one.

**Reading** (`fude_save_load(path, reader, user)`). It reads the main file; when
that is missing, or its reader returns false, it reads `<name>.bak`, the save
before. A file that does not read is first renamed `<name>.bad` and set aside, so
that a bug in a reader cannot destroy a page by autosaving an empty one over it. A
`.bak` that read is copied back as the main file (through a `.tmp`), so the next
save builds on what was recovered: a file only ever added to (the exam log, a
practice history) would otherwise start anew, and the save after would turn the
recovered one into the `.bak` and lose it. The reader runs twice in the worst case,
so it must start from nothing each time; the settings' and the page's parse into a
copy and change nothing until the whole file has read.

| Result | Means | What the callers do |
|---|---|---|
| `FUDE_LOAD_OK` | the main file read | use it |
| `FUDE_LOAD_RECOVERED` | the main file was missing or damaged; the `.bak` was read and put back | the same (the page's HUD says so); the exam log reads its memory again from the file before it appends |
| `FUDE_LOAD_MISSING` | neither file is there: nothing saved yet | start empty; with no settings, the app's first launch (the welcome, the device's theme) and the settings written at once; with no notes' index, the old `page.kana` becomes the first canvas |
| `FUDE_LOAD_CORRUPT` | there were files and none read; each was set aside | start empty; the settings keep their defaults, with no first launch; the notes' index is rebuilt from the canvases' files; an appended file starts anew |

The study's models (`fude_marks_open`...) take what read and ignore the result:
what is in memory then is what was saved, or nothing.

| File (in the save folder) | Written by | Read by | When it is damaged |
|---|---|---|---|
| `settings.kana` | `app/settings.c`, from the session | `fude_load_settings`, at launch | the `.bak`; neither: the defaults |
| `notes/index.kana` | `ink/notes.c`, on every change | `fude_notes_load`, at launch | the `.bak`; neither: rebuilt from the files in `notes/`, each canvas back at the top under a new name |
| `notes/<id>.kana`, a canvas's page | `ink/pagefile.c`, from the session | `fude_load_document`, as the canvas opens | the `.bak`; neither: an empty page |
| `notes/<id>.pdf`, a canvas's own document | `app/documents.c`, copied in at import | `pdf.h`, as the canvas opens | not opened: the ink alone |
| `notes/<id>.lines`, a document's read text | `doc/doc.c`, after each page is read | `fude_doc_lines_load`, through `fude_save_load` | its `.bak`; neither: the pages are read again and the file written anew |
| `notes/<id>.zoom`, a canvas app's canvas | `zoom/zfile.c`: changes appended as a journal, checkpoints, compaction ("When a canvas saves") | `fude_zoom_file_open`, as the canvas opens; `fude_zoom_file_peek` for Find's words | the last good checkpoint and every good journal after it; a torn end is cut off by compacting at once; a file that does not parse is kept as `.bad` and the `.bak` read |
| beside a canvas app's canvas: `.audio`, `.rec<n>` and `.rec<n>.text` (recordings, transcripts), `.words`, `.pages`, `.tools`, `.cover.png` | the deep-zoom page (`page_audio.c`, `page_speech.c`, `page_find.c`, `page_file.c`, `page_export.c`) | the page as the canvas opens; `.words`, `.pages` and the cover also the home screen (`homelist.c`, `home.c`) | each owner's header says: `zoom/audio.h`, `transcript.h`, `words.h`, `pages.h`; a missing cover is drawn again as the canvas opens |
| `pieces/` (`index.kana`, `<id>.piece`) and `parts/` (`<id>.part`, `templates.txt`), a canvas app's My pieces and My parts | `zoom/piece.c`; `page_custom.c` | the pieces the first time they are wanted (`fude_zoom_pieces_load`); the parts as the UI is built (`fude_zoom_page_parts_load`) | `piece.h`; a part that does not read is logged and left out, the others are learnt |
| `listen.kana` | `page_speech.c`: one byte, speech to text on or off | the same | missing or damaged: off |
| `marks.kana`, `words.kana`, `charnotes.kana` | the study's models, whole, on every change | `study/app/files.c`, at launch | the `.bak`; neither: empty |
| `reviews.kana` | `study/models/review.c`, after each sitting | the same | the same |
| `exams.kana` | `study/models/examlog.c`: the file read, an exam appended | the same; an exam's writing read back when the album asks | the `.bak`; neither: a new file from the next exam |
| `practice/<hex code point>.kana` | `study/models/history.c`: the file read, a session appended | `history.c`'s readers (the album, Statistics, Practice) | the `.bak`: at most the last session lost |
| Your data's backup, `<App> backup <date>.<id>backup` | `base/backup.c`, into `outbox/` to share (a tablet) or where chosen (a computer) | `fude_backup_inspect` (whole, its checksum right), then `fude_backup_restore` | refused before anything changes; a restore that fails puts back what was there (`before-import/`) |
| `crash_report.txt`, `crash_running` | RDE's crash handler (`rde_crash_install`, given the path and header by `base/crash.c`), and on Android a Java exception's by `FudeAndroid`; the session marks `crash_running` | the crash card, at launch | — |
| What is shared: Share as PDF, a practice sheet, a canvas's export (PNG, SVG, PDF, DXF, CSV, MP4, STL), Your data's backup | `docbar.c`, `study/app/files.c`, `zoom/page_export.c`, `base/backup.c`: into `outbox/` to share (a tablet), or where chosen (a computer) | the person, elsewhere | — |

A developer's runs add `perf.txt` (`--perf`), `translate.txt` and the study's
samples (`mlkit_samples.txt`); the canvas apps download the speech model into the
downloads folder, outside the save folder: the platform's cache, which no backup
takes in (`save.h`: `fude_save_downloads_dir`; `zoom/listen.h`). Your data's backup
holds every file of the save folder except the `.bak`, `.tmp` and `.bad` copies,
hidden files, `before-import/`, `outbox/`, the crash report's two files and the
diagnostics (`backup.h`). A file damaged twice keeps both copies: `<name>.bad`, then
`<name>.2.bad`, and so on up to `FUDE_FILE_ASIDE_MAX` (100) copies, past which the
last is replaced (`kfile.h`).

## Threads

Everything runs on the main thread, inside `rde_run`'s callbacks: the app, its
screens and models, the UI, the session and every save. Work leaves it only where
it would stall a frame, or where a platform answers on a thread of its own; the
files read or written off it are a PDF (by the worker), the speech model (read by
`listen.c`'s thread, written by the download's), a picked file (copied by the
picker's) and the crash report.

| What | Where it runs | How the result comes back |
|---|---|---|
| A document's pages (`doc/doc.c`), through the PDF worker (`doc/worker.c`) | a detached RDE thread for each piece, one at a time | `done`, under the one mutex every worker shares (`fude_doc_worker_mutex`), polled each frame (`fude_doc_worker_take`) |
| A canvas app's PDF view (`zoom/pdfview.c`) | the same worker | the same way |
| Handwriting (`ink/mlkit_ios.m`, `mlkit_android.c`, `FudeInk.java`) | ML Kit: on iOS it answers on the main queue; on Android on Android's UI thread | polled once a frame (`fude_mlkit_poll`); Android's answer, its number and the give-up under `FudeInk.LOCK`, read through JNI |
| Text in photos and a document's pictures, translation (`study/services/textscan*`, `translate*`, `FudeText.java`, `FudeTranslate.java`) | iOS: pictures turned upright on serial queues of their own, ML Kit answering on the main queue; Android: Android's UI thread (and a Java thread reading a picked photo) | polled each frame; Android's answers kept behind a lock |
| Speech (`speech_android.c`, `FudeSpeech.java`) | Android's TextToSpeech, made and answering on Android's UI thread | its state in a volatile field |
| The pickers: Android's (`FudeImport.java`), iOS's Photos (`import_ios.m`) | Android: a Java thread copies what was picked; iOS: the system's queues copy each picture (under `@synchronized`) | Android: `paths`, then `kind`, both volatile, polled by `fude_import_update`; iOS: handed over on the main queue when all are copied |
| Android's PDF search, and the opening and closing of PDFBox's document (`FudePdfBox.java`) | a thread of its own, "FudePdfFind" (`FINDER`), which opens and closes the document too (`Text.open`, `Text.close`), so a big PDF's seconds of opening never hold a frame | the matches so far read without waiting, under a lock of their own; the opening's end in a volatile field, asked by `fude_pdf_text_prepare` |
| Recording and playing (`zoom/voice.c`), a circuit's speaker (`page_mech.c`) | RDE's audio threads, filling and draining ring buffers of a second of sound; the speaker's samples pushed into an RDE audio stream | pumped once a frame by the page |
| Transcription (`zoom/listen.c`, whisper.cpp) | a thread of its own (`rde_thread_run`), one piece at a time | the piece handed in and marked done under a mutex, taken by `fude_listen_take`; its progress in volatile numbers read without the lock, each written whole by one thread; the guesses they are weighed by (`fzls_encode_s`, `fzls_share`) set as a piece ends and read, both under the mutex |
| The speech model's download (`zoom/fetch.h`) | the platform's own thread (Android's `FudeFetch`, NSURLSession's queue) | its state, a volatile value, polled |
| A zoom video (`zoom/video.h`) | the main thread, with the system's encoder on its own threads | the main thread waits for the encoder: up to 2 seconds a frame and 20 to finish on Apple's |
| A crash (`base/crash.c`) | RDE's signal handler, on the thread that crashed, on a stack of its own; on Android a Java exception's on the crashing Java thread (`FudeAndroid`) | writes only (open, write, close), with text made at start |

**The PDF worker** (the document worker, `doc/worker.c`), which a document (`doc.c`) and a canvas
app's PDF view (`zoom/pdfview.c`) each keep one of (`fude_doc_worker`). The
caller asks for a piece (`fude_doc_worker_start`: the PDF, the page, the part, its
size); the worker fills a `fude_doc_piece` with them and the theme's colours,
starts a thread for it and keeps it as its `job`. There is one piece at a time
(`fude_doc_worker_busy`), so a PDF is only ever drawn on one thread (`pdf.h` asks
it). The thread draws, flips and darkens into the piece's own pixels (a picture to
read is left as drawn), then sets `done` under the workers' one mutex, which guards
only `done` and `orphaned`. Each frame the caller takes it (`fude_doc_worker_take`): a
page or a sharp piece becomes a texture (`fude_doc_tile_take`), a picture to read
goes to the app's reader, a piece that failed is dropped.
- **A theme changed** (`fude_doc_worker_colours`) marks the running piece `stale`.
  It stays the worker's, so no second one starts on the same PDF, and
  `fude_doc_worker_take` throws it away.
- **A page turned** (`doc.c`) marks the running piece `stale` too
  (`fude_doc_worker_forget`: it draws the page the old way round), waits while it
  runs (a PDF must not be drawn while it turns), and no new piece starts
  meanwhile, so one piece after another cannot put the turn off.
- **A PDF written out** (`fude_doc_export`, `fude_zoom_pdfview_export`) draws the
  PDF's pages on the main thread, so it first waits for the running piece
  (`fude_doc_worker_wait`, on a condition variable the thread broadcasts as it
  sets `done`): one piece's drawing at most. The piece stays the worker's, taken
  as ever, and no new one starts until the export is through.
- **The PDF closing** (another canvas, the app's end, another PDF in the view) is
  the only caller of `fude_doc_worker_drop`. A finished piece is freed there; one
  still drawing is marked `orphaned`, and its thread frees it and closes the PDF
  itself, so the PDF is never closed under it; the caller forgets the PDF.
- **A reading no longer wanted** (its document closed, its page turned) is
  flagged `reader_drain`: `fude_doc_read_take` takes its answer when it comes and
  drops it.

`docs/design/pdf_and_threads.md` follows a piece from start to texture, and every
other hand-off in this table.

**ML Kit's answers.** Each reading has a number (`fude_mlkit_generation` on iOS,
`FudeInk.generation` on Android). A reading with no answer after 10 seconds
(`FUDE_MLKIT_GIVE_UP`, `GIVE_UP_NS`) is taken as an empty one, so the app falls
back to its own reader and the next reading can start; should the answer still
come, its number is no longer the current one and it is dropped. ML Kit always
answers, but without this one lost answer would leave handwriting unread until the
app restarts.

**The rule for new code.** Nothing the main thread owns is touched from another
thread. A thread is handed a job of its own, filled before it starts, and reports
back through one flag under a mutex (or the platform's main queue, or a Java field
behind a lock or volatile), which the main thread polls once a frame. A still screen
runs no frames ("Render on demand"), so the thread also wakes the app as it sets its
flag (`fude_redraw_request`, safe from any thread), or the poller asks to look again
while it waits (`fude_redraw_after(FUDE_REDRAW_POLL)`, when no code of ours runs on
the thread that answers). Nothing on the main thread waits for it, but for these:
`fude_listen_shutdown` waits for a
piece being read, as the app goes or before the model is downloaded again; a zoom
video waits for Apple's encoder (it is made a frame at a time, not in real time,
and each frame waits until the writer is ready for it, `video_apple.m`); a PDF
written out waits for the one piece the worker is drawing
(`fude_doc_worker_wait`); and on Android a question about a PDF page's text
(`FudePdfBox`'s `pageHasText`, `textIn`, and the writer's calls, under its
`LOCK`) can wait for the one page a search is reading. A result nobody
wants any more is dropped by a flag or a number (orphaned, stale, a generation),
never freed under the thread still using it. The header's Threads line says all of
it.

## Caches

Nothing is made every frame when it could be kept (`coding_rules.md` §8). Each
cache knows it is stale by a key: what it was made from, kept beside it and
compared when it is used. Two kinds of key cover almost all of them:
- **Revisions**, counters that move on every change: each study model has one
  (`fude_marks_revision`, `fude_reviews_revision`, `fude_vocab_revision`,
  `fude_examlog_revision`, `fude_history_revision`, `fude_charnotes_revision`),
  the ink, the notes and a page's turns have theirs (`fude_ink.revision`,
  `fude_notes.revision`, `fude_page.revision`), the language has
  `fude_text_revision`, a canvas `fude_zoom_scene.revision`, and the custom parts
  `fude_zoom_logic_revision`. A cache made at one revision is made again at
  another.
- **The whole of what it is made from**, as bytes, in an `rde_lru` (RDE's: any
  bytes as a key, compared whole; a fixed number of slots, which index the
  caller's own array of values; the one used longest ago makes room). The key
  is the bytes themselves, not a hash of them, so a cache never answers for the
  wrong thing (the one exception is Find's handwriting lines, below, whose 64-bit
  hash is accepted and documented). Where a bound would throw out what the
  current frame still draws, the canvas's caches keep a spare slot and make what
  does not fit each time instead.

A cache bounded in bytes (decoded strokes, pictures) keeps a "drawn at" stamp
on each entry and, only when a new one would pass the budget, lets go of the
oldest; an `rde_lru` would need a slot for every entry the budget could hold.

**The drawing core's and the study's**

| Cache | Holds (and its bound) | Made again when | Saves |
|---|---|---|---|
| The strings' renders (`base/text.c`) | every plain string, rendered once a language (with English stand-ins where it has none); 256 filled-in templates (`FUDE_TEXT_RENDERS`), in an `rde_lru` keyed by the id and every argument, whole | the language changes (all dropped); past 256, the one used longest ago makes room | RDE rendering the same strings every frame |
| Text widths (`base/draw.c`: `fude_draw_exact_width`) | 1,536 texts' widths per unit of size (`FUDE_DRAW_EXACT_KEPT`), in an `rde_lru` keyed by the font, the size it was loaded at and the text (a key of up to 1,024 bytes: a longer text is measured each time; a text with RDE's `[` markup is estimated instead) | past 1,536, the one used longest ago makes room; all when the UI trims or unloads the fonts (`fude_draw_forget`: a font loaded later at a freed one's address would be taken for it) | the engine laying a text out every time it is measured: every label, every frame |
| Text shapes (`base/draw.c`: `fude_draw_shape_of`) | every text drawn through draw.c (`fude_draw_text` and so its icons, wrapped and whole texts; the canvas's `fude_draw_text_placed`): its shape (`rde_text_engine_shape_text`: RDE's glyph runs, each glyph an `rde_text_engine_shaped_char` with its font, its glyph's record and its exact pen, and no picture of it, so a glyph's picture let go and made again leaves the shape good), drawn by the call the engine's `rde_rendering_2d_draw_text_2` makes with the shape it makes, so the pixels are the same. 1,024 (`FUDE_DRAW_SHAPES`) and a spare, in an `rde_lru` keyed by the font's pointer and every byte of the text (the engine shapes at the size the font was loaded at; the size drawn at only scales the shape) | past 1,024, the one drawn longest ago, never one this frame drew (`fude_draw_frame`, counted by the look harness each frame; more texts on screen than that: the rest shaped into the spare each frame); all when the UI trims or unloads the fonts (`fude_draw_forget`) | shaping every text every frame, as `rde_rendering_2d_draw_text_2` does: Browse shaped 74 texts a frame, 1 now (the engine's text field); its render 0.60 → 0.37 ms, Chart's 0.80 → 0.45 ms (a Mac, the measure build, 2026-10-16) |
| Text layouts (`base/draw.c`) | 64 texts' wrapped lines and 32 balanced widths, each in an `rde_lru` keyed by the font and its size, the size drawn at, the width and the whole text (`fude_draw_layout_key`) | the one used longest ago makes room; all at `fude_draw_forget` | measuring every place a line may break, every frame |
| A text's whole size (`base/draw.c`: `fude_draw_whole_size`) | 32 texts drawn whole on several lines: the size they fit at and their lines, in an `rde_lru` keyed as the layouts are, and the lines allowed | the one used longest ago makes room; all at `fude_draw_forget` | wrapping a text again and again only to know how big it is |
| The UI's widgets (`app/ui.c`) | every retained widget | the language changes, or a canvas's topic changes its bar: "The UI, and rebuilding it" | building them every frame |
| What a label says (`docbar.c`, `side.c`, `toolbar.h`, `row.h`, `filterbar.h`, `handwriting.h`, `study.h`) | the numbers it was made from: the document bar's `_page_said` (the page in the middle, the pages), `_count_said` (the mode, the pages while one is asked for, the matches, the one shown, done and searching) and `_mode_laid`; the side panel's `_nav_counts`, its canvases' list (`_notes_revision`, `_notes_open`), `_data_shown` and `_app_button_shown`; the toolbar's `_*_shown`; the filter bar's `_chosen_shown`, `_on_shown`, `_available_shown`; Handwriting's `_shown`; a row's faces | a number differs; `_count_said` is reset as the mode changes or a search starts | shaping text every frame |
| The reviews due (`study/app/study.c`, `fude_study_reviews_count`) | the side panel's "Reviews · n" | the marks' revision, the reviews' revision or today's day number changes (`_reviews_for`, compared whole: a sum could match another day's) | going through every marked character each frame the panel is open |
| The study screens' lists (`browse.c`, `viewer.c`, `vocabview.c`, `album.c`) | Browse's list, the viewer's word rows, sentences and look-alikes and its columns' widths, the Vocabulary's list, the album's sessions and exams | the revision each was made at differs (`_marks_seen`, `rows_words`, `sentences_for` and `sentences_words`, `similar_log`, `_listed_at`, `loaded_revision`, `exams_revision`); Browse's when it is marked `dirty` (a filter set, a stroke on its pad, the language, opening); the viewer's rows also when the language changes (`rows_for`), its widths when its rows, the language or the font do (`widths_*`) | reading, sorting and measuring every frame |
| Statistics' numbers (`stats.c`: `fude_stats_compute`) | every number the screen shows, worked out from every history file | the screen opens | reading every practice file every frame |
| The vocabulary's map (`study/models/vocab.c`: `fude_vocab_by_text`) | each word's id by its written form and reading | the words change wholesale (edited, read, cleared: `fude_vocab_words_changed`); a word added is added to it | a search of every word each time a word is looked for |
| The catalog (`study/chars/catalog.c`) | every character's readings and meanings, copied as search keys | the meanings' language changes (`fude_browse_language_changed` makes it again) | reading them out of the character data at every search |
| Whole characters (`study/chars/glyph.c`: `fude_glyph_kept_lru`, "Kept characters" below) | each character drawn whole (`fude_glyph_character`, and the ghost under one writing itself): the mesh of all its strokes on the GPU (RDE's `rde_rendering_2d_keep_stroke_mesh`), placed from its box's top-left; 512 (`FUDE_GLYPH_KEPT`), in an `rde_lru` keyed by the character data's pointer, the code point, the size on screen and a target pixel's size, whole | another size, a display of another density or a zoom makes another; past 512, the one drawn longest ago goes, never one drawn this frame (the rest are made each time they are drawn, as before); all at a trim and before the data is unloaded (`fude_glyph_forget`) | tessellating every character every frame and sending its triangles up again |
| The word split's index (`study/chars/wordsplit.c`) | every word's written form, folded, in a hash map: 2 to 4 MB, made in a few ms | another character database is split (it is of one, known by its address) | searching the dictionary for every word of a line |
| The matcher's prepared strokes (`study/handwriting/match.c`) | every character resampled and fitted the first time it is compared (about 80,000 strokes, 10 MB, on the std allocator, as everything that grows with the data is: the engine's pool keeps what it grows by until the app ends, `coding_rules.md` §3), and the records sorted by stroke count (`by_count`), so a ranking walks only the counts it compares | another database (`fude_match_layout`, known by its records' address and count); `fude_match_release` at the end | about 80 ms (a debug build) every ranking; the counts, with a floor under the rough cost, make a ranking 1.8 times faster optimised (`docs/quality_plan.md` 4.7) |
| A document's textures (`doc/doc.c`; a canvas app's PDF view, `zoom/pdfview.c`, the same) | each page whole, drawn at about its size on screen (`fude_doc_whole_size`: 1,100 pixels wide at most, no side past `FUDE_DOC_SHARP_MAX`: the GPU's largest texture, `rde_texture_get_max_size()`, up to 16,384), and what is on screen at the screen's pixels. Bounded by bytes (`fude_doc_wholes`): the pages on screen always, the others nearest the view first while all fit `FUDE_DOC_KEEP_BYTES` (48 MB; each byte held twice, on the GPU and in the memory texture's copy on the std allocator, which RDE's upload does not let go) | the theme's colours change (all dropped, the running piece stale); a page turned (all dropped, laid out again, the running piece stale); a sharp piece off screen, or zoomed over 2% away; a whole page no longer kept, or zoomed until its picture is 15% too coarse or 2.3 times too fine. A page is drawn only when it is kept, so a still view draws each once and the worker rests | drawing PDF pages every frame |
| A document's read lines (`<id>.lines`) | the text read in its pictures | a page turned (its lines dropped, the page read again) | reading a page's picture again |
| The Library's covers (`app/library.c`) | each book's cover texture, loaded the first time it is drawn (a missing one is tried once) | never; freed with the Library | loading a PNG every frame |
| On Android (`FudePdfBox.java`, `FudePdf.java`, `FudeTranslate.java`) | PDFBox's text of a document's pages (about 400,000 characters, `CHARS_KEPT`) and the folded characters search compares (`FOLDED`, up to 20,000); asset PDFs copied to `cache/pdf/`; one ML Kit translator a language pair | the text: past its bound, the page asked for longest ago goes; the others are kept | PDFBox extracting again; copying a book every time it opens; making a translator for every sentence |
| The fonts' glyphs (RDE's fonts, `ui.c`) | every glyph drawn, in textures that grow | the app goes to the background or memory runs short (`fude_ui_trim_fonts`); each glyph loads again when next drawn | loading a glyph every time it is drawn |

**The canvas's** (`fude/zoom`; `docs/design/rendering.md` has the renderer's,
`docs/design/scene_and_index.md` the index's). Most of the renderer's were
reworked on 2026-10-15 (`docs/quality_plan.md` 4.2 to 4.8); the numbers in the
last column are that work's, measured in the suites.

| Cache | Holds (and its bound) | Made again when | Saves |
|---|---|---|---|
| Decoded strokes (`render.c`: `fude_zoom_render_decoded`) | each visible stroke's and fill's points as f32 x, y and radius in its own units, a fill's triangles and cut edge with them; found by object (`slot_of`); 64 MB (`FUDE_ZOOM_RENDER_CACHE`), counted in bytes | past the budget, the least recently drawn go (never one this draw used); all of them as the canvas closes or memory runs short (`fude_zoom_render_trim`). An object's points never change (an edit makes a new object: `scene.h`), so nothing else makes one stale | decoding every visible stroke (`codec.h`) every frame |
| Strokes' meshes (`render_stroke.c`, kept in each decoded entry) | each stroke's tessellated triangles (RDE's `rde_stroke_mesh`) in screen units round where its translation lands, keyed by the turn and scale to the screen, the half-widths' scale and a pixel's size; 64 MB (`FUDE_ZOOM_RENDER_MESHES`), counted as the arrays' capacity. The second time a kept mesh is drawn as it is, a copy of it on the GPU too (`mesh_gpu`: RDE's `rde_rendering_2d_keep_stroke_mesh`), drawn from there after; bounded by the meshes', since it lives only beside one | a zoom, a turned or scaled stroke or another pixel size makes one again; past the budget the one drawn longest ago goes (never one this draw used); with its decoded entry and with a trim. Its GPU copy goes with it, and whenever it is made again. A pan, a still view, a theme or a faint stroke keeps both (the colour is given as it is drawn) | tessellating every visible stroke every frame (about 55% of a stroke-heavy canvas's render); with the copies, uploading every kept mesh every frame (4.10, the gate canvas still, the measure build: 9.7 MB a frame → 0.18 MB, its render 0.91–1.82 → 0.22 ms) |
| Pictures on the GPU (`render.c`: `fude_zoom_render_picture`) | a texture for each picture's bytes, shared by its copies; found through `picture_of`, a map keyed by the payload's handle and serial (a freed payload slot is given out again with a new serial); 256 MB of texture (`FUDE_ZOOM_RENDER_TEXTURES`, 4 bytes a pixel). A picture that does not decode is kept as none, not tried again | past the budget, the least recently drawn; all at a trim | decoding a JPEG or PNG and uploading it every frame |
| Kept geometry (`render.c`: `geo_lru`, `geos`; made by `render_symbol.c` and `render_shape.c`) | a symbol's parts at its kind, size and detail, and their triangles; a shape's outline at its detail; a cut board's rings; a shape's or board's triangles once a fill needs them. Keyed by `fude_zoom_render_geo_key`, compared whole: a symbol's kind, detail, half sizes and custom part; a shape's or board's type, payload (handle and serial: an edit makes a new payload), detail and how many of its numbers. 2,048 entries (`FUDE_ZOOM_RENDER_GEOS`) holding at most 2^21 points and triangle corners (`FUDE_ZOOM_RENDER_GEO_POINTS`), and one spare | past either bound, the one used longest ago, never one this draw used: what does not fit then is made into the spare, drawn from there and made again next time, while the rest stay kept; all at a trim | cutting curves and triangulating every symbol, shape and board every frame (every Library tile's symbol: 11.94 → 0.095 ms a frame; 500 filled shapes: 3.67 → 0.28 ms) |
| Notes' layouts (`render_text.c`: `rich_lru`, `fude_zoom_render_rich_aligned`) | a text's lines and marks laid out (only a text with marks is), keyed by its four fonts, the size they are loaded at, its letters' size, its width, its alignment and then its words; a plain text's wrapped lines kept in its slot too, with the size and width on the screen they were found at (`fude_zoom_render_plain_lines`); 256 (`FUDE_ZOOM_RENDER_RICH`) and a spare | past 256, the one drawn longest ago, never one this draw used (more notes on screen than that: the rest laid out into the spare each draw); a plain text's lines also at each step of a zoom; all at a trim | measuring every word of every note every frame (300 notes over 50 frames: 69.7 → 21.6 ms); wrapping every plain text every frame once more than draw.c's 64 layouts were on screen (300 plain texts: 44 laid out a still draw, not 300) |
| Graphs' lines (`render.c`: `plot_lru`, `fude_zoom_render_plot_made`) | a graph's lines, parts and labels at its size, keyed by its kind, a 0x1F byte, its text and its half sizes; 24 (`FUDE_ZOOM_RENDER_PLOTS`) and a spare | past 24, the one drawn longest ago, never one this draw used (more graphs on screen than that: the rest made into the spare each draw); all at a trim; a graph playing is made each frame | sampling an equation's curve every frame (40 graphs: 16 made a draw, not 40) |
| Formulas (`formula.c`) | each formula set by tex.h (`sets`: by its style and words) and each glyph's triangles at one of 9 levels of detail (`meshes`: by glyph and level); 512 formulas (`FZF_SETS_MOST`), 6,000 glyph cuts (`FZF_MESHES_MOST`) | past either bound, all of that kind go at once and are made again as they are drawn; the font loaded again | setting LaTeX and triangulating glyphs every frame |
| A canvas PDF's fonts (`zoom/pdf.c`: `fude_zoom_pdf_fonts`) | the app's font files read for a PDF being written, and the glyphs each document used | let go when no PDF made with them is left | reading the font files again for every PDF |
| The index (`index.h`, `scene.c`) | an R-tree for each frame over its objects' boxes; dead objects stay in it (an undo can bring them back) and a moved object's old entry stays, marked stale | built once, packed, as a file loads (`fude_zoom_scene_loaded`); built again when a frame's dead and stale entries pass 64 and half its objects (`fude_zoom_scene_reindex`, checked as an object moves or becomes garbage, `fude_zoom_scene_collect`) | looking at every object of a frame to draw, erase or pick; the rebuild clears the stale entries a query must otherwise sort out |
| Where wires join (`page_circuit.c`: `fude_zoom_page_junctions_kept`) | the junction dots on the screen and their radius | the scene's revision, its hidden layers, the frames drawn and where (the editable list) or the screen's size differs; the canvas closes. Not looked for at all while the canvas has had no wire (`junctions_wired`) | every wire's loose end against every segment on the screen, every frame |
| The canvas's attributes (`page.h`: `attributes`) | the PROPS shapes (`props.h`), alive or not, gathered once each as objects are made (`attributes_looked`) | the canvas closes | a walk of every object each frame for the probes' lights and the bodies' marks |
| The audio links (`audio.h`: `link_at`) | where each linked object's link is in `links`, by the object's id | made again when the links are read or a recording is removed (which closes up the array); added to with each link | a search of every link (10,000 added: 63.2 → 4.3 ms) |
| Handwriting for Find (`page.h`: `hand_lines`, `hand_said`) | the canvas's lines of writing, and what each says once read, by its key: a 64-bit hash of its strokes' ids (`handfind.h`; a collision, one in 2^64, is accepted) | the lines found again, a second apart at most, when the scene's revision moved; a reading kept while its line's strokes are the same; all dropped as another canvas opens | ML Kit reading every line again |
| Play's circuit and what plays (`page_mech.c`; `page.h`: `play_seen`, `play_what_key`, `play_what_parts`) | the circuit built from the drawing while it plays; which of what the lasso holds would play | the circuit: the canvas's revision moves while it plays; what plays: what the lasso holds, or the custom parts (`fude_zoom_logic_revision`), change | building the circuit every step; working out what plays every frame |
| Pictures the page draws off screen: the home screen's covers (`home.h`: `cover_lru`, 48, by canvas), a notebook strip's pages (`page.h`: `strip_lru`, 24, by page), the arrows' previews (8), the map | each one's texture | the strip's page at a new revision; a preview when the revision changes, its slot given to another arrow once no arrow points at what it shows; the map when what is drawn changes (`map_contents_rev`); a cover when the page writes it again (`cover_stamp`); covers and the strip past their bounds, the one shown longest ago; all at a trim (`fude_zoom_page_trim`) | drawing the canvas again for each small picture, every frame |

**Kept characters.** A whole character is drawn from one mesh of all its strokes
kept on the GPU (`glyph.h`, Design), made the first time and then only moved and
coloured by each draw, as a canvas keeps its strokes' meshes. Its key is all its
shape follows from: the character data, the character, its size on screen (which
sets the pen and how finely its curves are flattened) and a target pixel's size
(its soft edge is one pixel wide). A part of a character (the viewer's stroke
being drawn, a character writing itself) is still made each frame, since it
changes every frame. The kept meshes are drawn by a pipeline of their own, so
RDE's gathering (`rde_rendering_2d.c`) puts them together past what they do not
overlap, as it does Slug text: Browse's characters and captions are two changes
of pipeline, not two a cell. Its pixels differ from drawing by stroke only by
rounding: a mesh placed by a translation adds its coordinates in another order,
so a few soft-edge pixels move by 1 of 255 (5 of Kana's 19 screenshots: 2 to 34
pixels each). Kana's Browse, before → after (2026-10-19, `--browse --perf=10`):
on a Galaxy Tab S6 Lite (android-measure, three runs each), render 5.6–5.7 →
2.8 ms a frame, 624 → 62 KB sent to the GPU a frame, 13 → 109 draw calls but 13
→ 14 changes of pipeline, no frame over 20 ms either way (the loop is paced to
vsync); on the Mac (measure, 744x1133), 474 → 48 KB a frame, the frame's CPU
1.9–2.6 → 1.3 ms.

Not caches, whatever their names: a `fude_glyph` (the screens' "glyph cache")
holds scratch arrays (the kept characters are glyph.c's, shared by every screen).
A still study screen is not drawn at all ("Render on demand"). The
notice keeps its text alone; its lines come from draw.c's layouts each frame it
shows. The session's last written and last seen copies are how it knows what to
save ("How an app runs"). The renderer's and the page's scratch arrays (`found`,
`items`, the junctions' and the trail's) are kept so a frame allocates nothing,
but hold nothing from one frame to the next.

## The builds

Each app is built by RDE's builder from its project file, `apps/<app>/<app>.rdeproj` (the format, every key and the
builder's commands: `~/RDE/docs/docs/project_files.md`), through one script:

```
zsh tools/build/app.sh <app> [command] [profile] [builder options]      # kana, kana install android, kana run sim...
```

The project file is the whole build but for four things (below): the sources, include folders and flags, the assets,
the Mac's frameworks, Android's package, permissions, Java and libraries, iOS's package, Info.plist, asset catalog,
launch screen and frameworks, and the profiles. Every app has the same profiles (Draw, not released, the first three):

| Profile | What it is | Where it goes |
|---|---|---|
| `debug` (the default) | the Mac's developer build, which reads the look flags | `build/<app>/osx-silicon/debug/project/exe/runnable` |
| `release` | the Mac's release, which takes no launch arguments | `build/<app>/osx-silicon/release/project/exe/` |
| `measure` | a release that takes them (`FUDE_ALLOW_ARGS`), to measure one with `--perf` or `--gate`; never shipped | `build/<app>-measure/` |
| `android` | the release APK (64-bit ARM, API 37) | `build/<app>-android/game.apk` |
| `bundle` | the same, with Google Play's App Bundle | `build/<app>-android/game.aab` |
| `ios` | the iPad's release, signed, with ML Kit | `build/<app>-ios/<App>.ipa` |
| `sim` | the Simulator's, debug (the store's screenshots use the look flags), without ML Kit | `build/<app>-simulator/<App>.ipa` |

Profiles that would overwrite each other's package (the builder writes the APK and the IPA at the top of the output
folder) have folders of their own, and the measuring release has one so that it can never be taken for the shipped one.

**Why each line is there.** The ten files are alike line for line, so one read explains them all:
- `sources` is the app's description (`<app>.c`) and every `.c` and `.m` of its layers' folders, by pattern
  (`../../fude/study/*/*.c`): a file added to a layer is built by every app that has the layer, with no list to edit.
  A pattern that matches nothing stops the builder, so each names only what is there (`fude/sim/` has no `.m`).
- `exclude` keeps each platform's files to it. A platform file is named after its platform (`*_android.c` and
  `base/android.c`, JNI; `*_ios.m`; `*_apple.m` and `pdf_kit.m`, the Mac's and iOS's): `[project]` takes Android's
  out, `[macos]` takes `*_ios.m` out too, `[windows]` every `.m`, and `[android]` sets the list anew (`=`) to every
  `.m`, which brings Android's own files back.
- `flags`: `-Wall -Wextra`, and `-Werror=implicit-function-declaration`, because C would guess the type of a function
  nothing declares. The builder shows the warnings of every file it compiles.
- `[android]` names the app's `platform/android/deps.lock` and lists no `dependencies`: the lock has PDFBox's lines
  added by hand (without the BouncyCastle it would pull in), and the builder resolves a lock again, dropping them, when
  its `dependencies` do not match what the lock was made from. A version is changed by resolving by hand ("A dependency
  on Android", below).
- `[ios] permissions` names what the app asks for through `rde_mobile_request_permission`: the camera for Text from a
  photo (Kana, Hanzi, Hangul, Hindi), the microphone for the recordings (Sketching, InfiNote). RDE's iOS code for a
  permission is built only when it is named, because Apple refuses an upload whose code reaches a privacy API without
  that API's purpose string; the builder checks that the plist has the string of each one named. Thai, Arabic and
  InfiniWorkshop ask for nothing through RDE (their document scanner asks for the camera itself) and have no line.
- iOS's system frameworks are `link_flags` (`-framework PhotosUI`...), since the builder's `[ios] frameworks` takes
  framework folders. ML Kit's are such folders, in `[profile ios]`: a device's only, as ML Kit has no Simulator build
  (the app uses its stand-ins there).
- The canvas apps that record (Sketching, InfiNote) link whisper.cpp on the Mac and Android: `define.FUDE_WHISPER`, its
  headers with `-isystem` (their own warnings are not ours), its libraries, and on Android its shared ones packed
  beside the app's (`libs`: one per ARM generation of the CPU's code, as `tools/whisper/build.sh` makes them).
- InfiNote's and InfiniWorkshop's `assets` are `build/<app>-stage/assets/`: Sketching's assets with theirs over them.

**What `tools/build/app.sh` adds**, each because a project file cannot say it:
- The version, read from the app's `src/version.h` and passed as `--version` (Android's versionName and versionCode,
  iOS's bundle versions). That file is the one place it is written: the C code shows it, and the builder cannot hand a
  project file's version to the C code, so a version there would be written twice.
- The staged assets, put together before each build of an app whose `assets` names them.
- whisper.cpp's libraries, built by `tools/whisper/build.sh` when an app that links them finds them missing.
- iOS signing, which is the developer's, not the app's: `FUDE_IOS_PROFILE` (the app's `.mobileprovision`) and, when the
  keychain holds more than one Apple Development identity, `FUDE_APPLE_TEAM`.
- Android Studio's JDK, which the APKs have always been built with.

**The engine** has no step of its own: a build checks the engine library it links and rebuilds it first, for its
platform and configuration, when it is missing or older than the engine's sources (an engine change, a shader
included).

**Android's signing key** is `apps/<app>/android_signing/com.rde.<app>.keystore`, where the builder keeps it (the
folder ignores itself in git: the key is a secret). The first build with the project file copied it there from
`build/<app>-android/`; copies are kept in `~/kana-android-keys/`. An APK signed with another key cannot update the
installed app, nor Play's.

**What is repeated.** The format has no way for one file to include another, so what every app shares is written in
each: the patterns and exclusions, the flags, Android's and iOS's sections, the profiles. A change to one of them is
made in all ten files (`build_all.sh` builds every one, so a file left behind shows).

## Checking a change without a device

Every check runs on the Mac; none needs a device.

| Check | Command | What it says |
|---|---|---|
| Every app builds | `zsh tools/check/build_all.sh` | a line for each build, `ok` or `FAILED` with its first errors (`build/check/mac-<app>.log`, and `android-<app>.log`, `sim-<app>.log`) |
| No warnings | `zsh tools/check/warnings.sh` | every compiler warning in the apps' code, also of the files a build did not recompile |
| The screens | `zsh tools/check/shots.sh --baseline` before the change, `zsh tools/check/shots.sh` after | each picture DIFFERENT, NEW or MISSING |
| The tests | `perl -e 'alarm N; exec @ARGV' bash tests/run.sh` | each suite's `ALL PASSED`, or where it failed |
| The rules | `python3 tools/check/rules.py` | the counts of what `coding_rules.md` forbids |

- **`build_all.sh`** builds the ten apps for the Mac, debug (Draw, the core alone,
  among them, so the core is seen to build on its own), each through
  `tools/build/app.sh` and its project file. With `--android` it also builds the
  nine released apps' release APKs (not Draw's), with `--sim` the nine for the iOS
  Simulator. There is no engine step: each build rebuilds the engine for its own
  platform when that is older than the engine's sources. It exits 1 when one failed.
- **`warnings.sh`** checks every source file once, for the Mac, with the command
  the app's debug build compiles it with (`builder compdb` writes them into
  `apps/<app>/compile_commands.json`), and `-fsyntax-only`: a build prints the
  warnings of the files it compiles, and an incremental one compiles few.
- **`shots.sh`** captures the screens listed in `tools/check/shots.txt` (who:
  `study`, `canvas` or one app; the picture's name; its launch flags), for every
  app or those named, which must be built for the Mac first. An app's first run,
  from no saves, is shot (the welcome) and its saves kept; every other capture
  starts from a copy of them, a shot's clock counts frames (`clock.h`), and a launch
  with flags draws every frame ("Render on demand"), so the same code draws the same
  pixels. Pictures are compared byte for byte with
  `build/shots/baseline/`. `python3 tools/check/pngdiff.py before.png after.png
  --out diff.png` shows where two differ.
- **`tests/run.sh`** builds and runs the 39 suites (`tests/README.md`), each with
  clang under AddressSanitizer and UBSan, linked to RDE's real desktop library and
  run against Kana's character data (the `zh` and `ko` suites are built as Hanzi
  and Hangul, `@APP=`, and run on theirs); `tests/run.sh vocab exam` runs those
  alone. The canvas's suites are `zoom` (the canvas's models, built with a canvas
  app's strings: `@CANVAS`), `spice` (circuits against ngspice's answers), `mech`
  (mechanisms in RDE's physics, Box2D's sources built in: `@BOX2D`), `sim`,
  `graph`, `units`, `pdf`, `dxf`, `geometry`, `docwork` and `kit`; none links the
  page driver (`page*.c`), which the shots check instead. The `perl … alarm` gives
  the run N seconds at most (macOS has no `timeout`), so a suite that hangs cannot
  hold it.
- **`rules.py`** prints eight counts and the worst files of each: `libc-header`
  and `libc-call` (the C library or POSIX), `void-pointer` (`void*`, where RDE
  writes `any`), `layer` (an include against the layer table), `app-identity` (an
  app's name in `fude/`), `file-size` (over 2,000 lines), `function-size` (over
  150), `undocumented` (a public function with no comment). The totals are in
  `docs/quality_counts.txt` and only go down: `--ratchet` fails when one went up,
  `--save` writes them after a change brought them down, `--all` lists every
  finding. Every count has been 0 since 2026-10-14 (`docs/quality_plan.md` 5.4),
  so any finding is new.

A developer's build reads launch flags, so a screen can be opened in a given
state, shot and compared: the core's in `look.h`, a study app's (`--chart`,
`--welcome` and the rest) in `study/app/look.c`, the canvas apps' in
`zoom/look.c`. `COMMANDS.txt` says what each does.

- `--size=744x1133` and a screen flag (`--browse`, `--viewer=6728`, `--exam`,
  `--vocab`...) open a screen in a given state.
- `--shot=FILE` writes a screenshot and quits; `--shot-at=N` takes it at frame N.
- `--press=I,J,...` presses the buttons of the screen on top in turn, as taps
  would.
- `--swipe=DX` drags a finger across it.
- `--language=N` switches the language mid-run, and `--theme=N` shows a theme
  without saving it.
- `--perf=SECONDS` measures the frames for that long and appends a summary to
  `perf.txt` in the save folder.
- On the canvas: `--example=E@F` draws example E (its number in `examples.h`)
  at frame F, `--library=N` opens the diagram library, and `--gate=POINTS`
  makes a canvas of that many points over four depths, the canvas the
  performance work is measured on (with `--perf`). It is the test ink
  (`zoom/inktest.h`): in a developer's or a measuring build (the Mac's
  `measure`, the tablet's `android-measure`), never a release that ships,
  Settings' Display makes a canvas of it with a million points, the same
  strokes as `--gate=1000000`, to see on the device what was measured
  (`--inktest=N` presses its Make at frame N).

## How to add things

A screen, an app and a language have their own recipes: "Adding a screen" (under
[A screen](#a-screen)), [Making a new app](#making-a-new-app) and "A new language"
(under [The language layer](#the-language-layer)). The canvas's (a tool, a topic,
a symbol, a circuit part, a mechanism part, an example) are at the end of this
section.

**A string**
1. Add a row to the lowest layer that uses it: `fude/drawing/strings.py`,
   `fude/study/strings.py`, `fude/lang/<code>/strings.py`, `fude/zoom/strings.py`,
   or the app's `tools/strings.py`. A row is `t('ID', English, Spanish, Portuguese
   (Brazil), Japanese, French)`; `P(one, other)` makes a count's two forms.
2. For the study apps that teach another language, add `x('ID', text)` to the
   `strings_<code>.py` beside that file: a row of `fude/drawing/` or `fude/study/`
   needs it in all five (zh, ko, th, hi, ar), a language's or an app's in its own.
   The canvas apps have no fourth language, so `fude/zoom/strings.py` has no
   `strings_<code>.py`.
3. Write the typographic ’, never `'`: an ASCII apostrophe quotes the next
   character in RDE, and build.py refuses it. A text too long for its place is
   reworded or wrapped, never cut with an ellipsis.
4. `python3 tools/strings/build.py <app>`, or `all` after a layer's change. It
   checks that every language has every id with the same placeholders and writes
   `src/text_ids.h` and `assets/text/strings.rdel`, which are never edited by hand.
   A string missing in a study app's language stops it; `--allow-untranslated`
   keeps Japanese as that app's fourth language while a translation is drafted, and
   is never shipped.
5. Use it with `fude_text(FUDE_TEXT_<ID>)`, or `FUDE_TEXTF(out, FUDE_TEXT_<ID>,
   FUDE_TN(n), FUDE_TS(s))` for a template. `tests/run.sh text` checks Kana's.

**A setting**
1. Add the field at the end of `fude_settings` (`app/settings.h`), and to the
   field list in its overview.
2. In `settings.c`, write it last in `fude_save_settings`, read it last in
   `fude_settings_read` (an older file ends before it: keep the value it came in
   with, or give old files their own default), and compare it in
   `fude_settings_equal`, or a change to it alone is never saved.
3. Gather and apply it: a core one in `session.c`'s `fude_session_gather` and
   `fude_session_apply` (clamped to what the tool allows); an app's in its
   extension's `settings_gather` and `settings_apply` (only the canvas sets them:
   `zoom/page_export.c`'s `fude_zoom_page_settings_gather` and `_apply`).
4. Its control goes in Settings (`side.c`), or in a section of the app's own
   (`extension.h`: `sections`).
5. `tests/save` checks the settings file field by field: add the new one to its
   round trip; it also reads an older file and a damaged one.

The session then saves it a second after it changes.

**A file the app saves**
1. Give it one owner, and describe its kind, chunks and records in the owner's
   header (the Files line).
2. Write it with `fude_bytes_new`, `fude_put_header`, chunks and
   `fude_bytes_write_and_free`, in the save folder (`fude_save_dir`, or
   `fude_save_subdir` for a kind with many files).
3. Read it only through `fude_save_load`, with a reader that starts from nothing,
   skips unknown chunks, reads only the fields a record has, and returns false
   when the header does not read. Decide what each `FUDE_LOAD_` result means for it.
4. Decide when it is written: as the change happens (the study's models), or
   through the session for what is on screen.
5. It goes into Your data's backup by itself; a file that must not travel goes in
   `backup.c`'s list of what is left out. A file that goes with a canvas is named
   `<id>.<something>` in the notes folder (`fude_notes_dir`), and
   `fude_notes_delete_files` deletes it with the canvas, found by its name.
6. Test it in `tests/save`'s way: a round trip; the `.bak` after a second save; a
   truncated main file read as RECOVERED with its `.bad` kept; the main missing,
   read from the `.bak` and put back; garbage in both read as CORRUPT with nothing
   changed; a "newer" file with an unknown chunk and longer records read.

**A test suite**
1. `tests/<name>/test.c`, whose `main` takes the character data's path and prints
   `ALL PASSED` last (`tests/README.md`).
2. A row in `tests/run.sh`'s `SUITES`: its name, its flags, its sources from the
   project root. Link only what it tests and the doubles it needs
   (`tests/support/`); `run.sh` links `support/headless.c` first by itself. A
   suite is built as Kana; `@APP=<app>` in its flags builds it as another study app
   (a language's suite), `@CANVAS` with a canvas app's strings; `@RDE` in its flags
   is RDE's folder, and `@BOX2D` in its sources Box2D's (the physics' suites).
3. A row in `tests/README.md`'s table, then `tests/run.sh <name>`.

**A toolbar tool or a menu button** (`app/extension.h`)
- A tool: a `fude_extension_tool` in the extension's `tools` (12 at most,
  `tool_count`): its name, its icon (`icons.h`), its press, when it shows
  (`available`), whether it sits before Paper (`by_paper`), and, for a panel of
  choices beside the bar, its `choices` (24 at most, `choice_count`) with `chosen`
  and `selected`. The study's camera, `FUDE_STUDY_CAMERA`, is one. (A canvas tool
  is not added here: see "A canvas tool" below.)
- A button of the page's menus: a `fude_row_button` in the extension's
  `selection_row`, `context_row` or `text_row` (`FUDE_ROW_DEF`), with the core's
  `FUDE_PAGEMENU_BUTTON_*` where the app wants them; `selection_faces` and
  `context_faces` say how each shows, and `menu_update` does the work each frame.
  The study apps' rows are `study/widgets/pagetext.c`'s; the canvas's are
  `FUDE_ZOOM_SELECTION_ROW` and `FUDE_ZOOM_CONTEXT_ROW` (`page_edit.c`).
- A screen's button: its row (`rows`, `faces`: "A screen").
- Its name and label are strings (above).

**A library book**
1. The PDF and a PNG of its first page in the app's assets (`assets/lectures/`;
   `tools/lectures/` makes them).
2. A `fude_doc_book` in the app's library table (Kana's `KANA_LIBRARY` in
   `apps/kana/kana.c`): an id of 2 or more never used before (a canvas keeps it),
   the file, its title and about strings, its credit, its cover, its language (or
   NULL to show it always).
3. Its licence in the app's Licences (`info.h`; Kana's
   `assets/lectures/LICENSE-lectures.txt`).

The Library lists it, and opening it makes a canvas over it
(`fude_documents_open_book`).

**A dependency on Android**
1. Resolve the app's lock again with every root in one command
   (`~/RDE/builder --android_dep_cache=build/android_deps
   --android_dep_resolve=<group:artifact:version>...
   --android_deps_out=apps/<app>/platform/android/deps.lock`; `COMMANDS.txt`,
   THE LIBRARIES, has each app's roots), then add back the PDFBox lines at its
   end by hand. (The project files name the lock and list no `dependencies`, so
   the builder never resolves it on its own: "The builds".)
2. Call it from Java in `fude/android/java/` (every app) or
   `fude/android/java_study/` (the study apps), reached from C by a `*_android.c`
   through `base/android.h`.
3. `python3 tools/licences/build.py`: it reads every `deps.lock`, and a library of
   a group it does not know stops it until it is classified.
4. `zsh tools/build/app.sh <app> android`, or `build_all.sh --android`.

**A framework on iOS**
1. Add it to the `[ios]` section's `link_flags` (`-framework Name`) of every app
   that uses it, and to `[macos] frameworks` when the Mac uses it too.
2. Call it only from a `*_ios.m` (or a `*_apple.m`, for both).
3. ML Kit's pods are `third_party/mlkit/`, set up by `python3 tools/mlkit/setup.py`,
   which runs the licences tool at its end.

### On the canvas

Four rules run through every recipe here. **Numbers that are saved are
appended, never inserted or reordered**: a symbol's kind (its place in
`symbol.c`'s `FUDE_ZOOM_SYMBOLS`), a topic (`FUDE_ZOOM_TOPIC_`, kept in a
canvas's `<id>.tools`), and the entries of Insert, Instruments and Export, which
the topics' lists name by number. **A table a row short does not compile, where a
check guards it**: `page_topics.c`'s `FUDE_ZOOM_TOPICS`, `examples.c`'s
`FZX_BUILDERS`, `page_custom.c`'s `FUDE_ZOOM_PAGE_EXAMPLE_NAMES`, and
`page_diagrams.c`'s `FUDE_ZOOM_PAGE_LIBRARY_TABS` and
`FUDE_ZOOM_PAGE_SYMBOL_NAMES` are sized by their rows, and a typedef of size −1
compares each with its count (`FUDE_ZOOM_TOPIC_COUNT`, `FUDE_ZOOM_EXAMPLE_COUNT`,
`FUDE_ZOOM_PAGE_LIBRARY_TABS_N`, and `symbol.h`'s `FUDE_ZOOM_SYMBOL_KINDS`, 439,
which `symbol.c`'s `FUDE_ZOOM_SYMBOLS` is checked against too). `page_parts.h`
declares the symbols' names without a size on purpose: a size there would make a
short table compile, zero-filled. So a new symbol raises `FUDE_ZOOM_SYMBOL_KINDS`
by hand, with its row in both tables. The same holds for the bar's tables: the
base tools (`FUDE_ZOOM_BASE_TOOLS`, checked in `page_topics.c`), every tool's
choices (Shapes, Insert, Smoothing, Arrange, Instruments, Export, the circuit's,
the Plan's: `page_parts.h` declares them without sizes and names each one's
count, which the bar takes and the file defining the table checks), the products
(`product.c`'s `FZPD_PRODUCTS`) and the library's order of tabs: a row left out
does not compile. **A part is joined to its symbol by its id
string** alone: the symbol's id, the circuit's or the mechanism's catalogue
row's, and a chip's text must be the same. The page's
modules are named in `page_parts.h`'s map; the details are in
`docs/design/page_driver.md`, `circuits.md` and `mechanisms.md`.

**A canvas tool** (a tool of the bar, as Shapes, Fill or Arrange are)
1. Its model, without UI, in `fude/zoom/<tool>.c` and `.h` (as `fill.c`,
   `bucket.c`). The apps' builds take it by itself; `tests/run.sh` lists every
   source by name, so add it to the `zoom` row (and to `spice` and `mech`, which
   link most of the canvas too, when what they link calls it).
2. Its state in `fude_zoom_page` (`page.h`, as `fill_tool`), its press, choices
   and handlers in the `page_<what>.c` of its kind, declared in `page_parts.h`.
   As a pen mode: let go of it in `page_pictures.c`'s
   `fude_zoom_page_pen_modes_off` and in `page_frame.c`'s check for another bar
   tool tapped; take the pen in `page_input.c`'s `fude_zoom_page_down`, `_moved`
   and `_up` (their checks run in order: where it goes decides what wins); draw
   what it shows in `page_render.c`.
3. On the bar: append its value to `product.h`'s `FUDE_ZOOM_TOOL_`, and its
   `fude_extension_tool` to `page_topics.c`'s `FUDE_ZOOM_BASE_TOOLS` (and the
   private `FUDE_ZOOM_BASE_` enum: the typedef `fude_zoom_page_tables_as_product_h`
   fails to compile when the table's rows and the enum disagree).
4. In the topics that show it: their `tools` lists in `FUDE_ZOOM_TOPICS`. The
   bar holds 12 tools (`FUDE_EXTENSION_TOOLS`) and Topic is one of them, so a
   topic shows 11 at most.
5. In the products that offer it: Sketching's and Workshop's take every tool
   (`FZPD_ALL`); Notes' lists its own (`FZPD_BIT`, `product.c`).
6. Its icon (`drawing/base/icons.h`) and its name (`fude/zoom/strings.py`, then
   `python3 tools/strings/build.py all`).
7. A look flag to shoot it (`zoom/look.c`: listed in its header, read in
   `look_read_tools`, acted on in `look_frame_tools`, usually through a
   `fude_zoom_page_look_*` of `page_look.c`; `--demo-fill` is one to copy), and
   its shot in `tools/check/shots.txt`.
8. Tests of its model in `tests/zoom` (which links no `page*.c`).
   `test_products` checks what each product offers, by count.

A new choice inside Insert, Instruments or Export is a row appended to
`FUDE_ZOOM_PICTURE_CHOICES` (and its `extern` size in `page_parts.h`),
`FUDE_ZOOM_INSTRUMENT_CHOICES` or `FUDE_ZOOM_EXPORT_CHOICES`, with the matching
value appended to `product.h`'s enum and its number in the topics' lists.
Instruments' extra choices (Tape to Trim) are numbered after the instruments'
kinds (`FUDE_ZOOM_INSTRUMENT_COUNT + k`), so a new kind of instrument moves them.

**A topic** (what a canvas is for: Electronics, Sewing...)
1. Append `FUDE_ZOOM_TOPIC_<X>` before `FUDE_ZOOM_TOPIC_COUNT` (`product.h`).
   It is saved in `<id>.tools` (`page_file.c`'s `fude_zoom_page_tools_save`).
2. Its row in `page_topics.c`'s `FUDE_ZOOM_TOPICS`: its name, its icon, its
   tools (`FUDE_ZOOM_BASE_*`) and its lists of Insert's, Instruments' and
   Export's entries.
3. What it adds: library tabs (a `FUDE_ZOOM_PAGE_LIBRARY_<X>` mask in
   `page_parts.h`, opened by `fude_zoom_page_library_open_tabs`); a tool of its
   own with a run mode (as Circuit's `FUDE_ZOOM_CIRCUIT_CHOICES` in
   `page_circuit.c`, Motion's `FUDE_ZOOM_MOTION_CHOICES` in `page_topics.c`,
   Plan's `FUDE_ZOOM_PLAN_CHOICES` in `page_diagrams.c`); anything the page does
   differently in it, asked of `fude_zoom_topic_now` or the page's `topic`.
4. Offer it: Sketching takes every topic; add its `FZPD_BIT` to Notes' or
   Workshop's row. The Topic menu (`fude_zoom_topic_fill`) lists the app's
   topics, 24 at most.
5. Its strings, and `test_products` (Notes' and Workshop's topics are checked).

**A symbol** (a shape of the diagram library)
1. Append its row to the end of `symbol.c`'s `FUDE_ZOOM_SYMBOLS`:
   `FZS(id, name, FAMILY, w, h, PLACE, boxes, hull, fn)`. Its place is its kind,
   the number a canvas keeps (`symbol.h`'s Files), so no row is ever moved or
   removed; its id must be new (`fude_zoom_symbol_find` finds it by id).
2. Its draw function, `fzs_<what>(fude_zoom_sym*)`, in `symbol_diagrams.c` (the
   diagram families) or `symbol_shapes.c` (the others), declared in
   `symbol_parts.h`; its first part is its outline. A topic's part draws through
   `symbol.c`'s `fzs_circuit`, `fzs_mech` or `fzs_plan`, by its id.
3. Its name: the `FZS` row's name is not compiled. What shows is
   `page_diagrams.c`'s `FUDE_ZOOM_PAGE_SYMBOL_NAMES`, in the table's order: add
   its row and the string (`ZOOM_SYM_...`), and raise `symbol.h`'s
   `FUDE_ZOOM_SYMBOL_KINDS` (439 today), which both tables are checked against as
   they compile.
4. The library shows it by itself (`fude_zoom_page_library_make` makes the tiles
   family by family, in the table's order), and the renderer keeps its geometry
   by itself (by kind, size and detail). A new family needs its value in
   `symbol.h`, its tab (`FUDE_ZOOM_PAGE_LIBRARY_TABS` and its order), its bit in
   the masks of `page_parts.h`, and, when a topic owns it, a case in
   `fude_zoom_symbol_topic`.
5. Its art, if it is drawn from an SVG: `apps/sketching/assets/parts/` and its
   `parts.txt` (`symbolart.c`); the other canvas apps stage Sketching's assets.
6. `tests/zoom`: `test_symbols` (its points within its box), `test_render_tiles`,
   `test_kind_parts`.

**A circuit part**
1. Its symbol (above), in the Electronics, Logic, Chips or Boards family, drawn
   by `fzs_circuit`.
2. Its row in `circuit_catalogue.c`'s `FZC_PARTS` (`FZ`, `FZ_SIM` or
   `FZ_SIZED`), with the symbol's id, and its pins in the order its model reads
   them. Nothing more joins them: a symbol's kind finds its family's topic
   (`fude_zoom_symbol_topic`), `symbol.c`'s one table finds the part by id
   (`fude_zoom_symbol_topic_part`, `fude_zoom_part_find`), and `test_kind_parts`
   checks that every circuit family's symbol has a part.
3. A part that behaves as no other does is a new model: append
   `FUDE_ZOOM_MODEL_<X>` (`circuit.h`) and its look (`circuit_parts.h`), then
   give it its case in each step of the circuit (`circuit_parts.h`'s map says
   which module holds which): its value read (`circuit_read.c`,
   `fude_zoom_circuit_value` for "4.7k"), its stamps (`circuit_stamp.c`), its
   state (`circuit_solve.c`), what follows a step (`circuit_step.c`), its drawing
   (`circuit_draw.c`), a tap on it while it plays (`circuit_play.c`), its limits
   (`limits.c`), and the page's Value card and devices (`page_circuit.c`). Follow
   an existing model through them: `FUDE_ZOOM_MODEL_SPEAKER` touches each.
4. A logic part made of others is a chip: an `FZ_SIM` row and its text in
   `fude/sim/chips.c`'s `FSC_CHIPS` (its "id" line the same id). A new logic
   primitive is a model in `fude/sim/logic.c`'s `FSL_MODELS`, and the canvas's
   side of it in `zoom/logic.c`.
5. Tests: `tests/spice` (against ngspice's answers), `tests/zoom` (its limits,
   the examples), `tests/sim`.

**A mechanism part**
1. Its symbol, in the Mechanisms family, drawn by `fzs_mech`.
2. Its row in `mech.c`'s `FZM_PARTS` (its id, its `FUDE_ZOOM_MECH_` kind, its
   teeth, its holes, its value), with the symbol's id.
3. A new kind: append it to `mech.h`'s `FUDE_ZOOM_MECH_`, then draw it
   (`fude_zoom_mech_draw`), make its body (`fzm_bodies`), find its joints by
   where it lies in `fude_zoom_mech_plan_build` (the `fzm_hinges`, `_axles`,
   `_meshes`, `_belts`... steps; a new array of the plan is cleared there and
   made, freed and copied with the plan), and give it its shape in the physics
   (`mechrun.c`'s `fzw_shape_plain`). `sim/body.c` is only the drawn outline made
   a body; it has no list of parts.
4. Tests: `tests/mech` (the physics against exact ratios), `test_kind_parts`.

**An example** (Insert → Examples)
1. Its value in `examples.h`'s `FUDE_ZOOM_EXAMPLE_`. Its group is its place
   (`fude_zoom_example_group`: before `FUDE_ZOOM_EXAMPLE_CRANK` electronics,
   before `FUDE_ZOOM_EXAMPLE_MOTOR_GEARS` mechanisms, then both), so one added
   in the middle renumbers every one after it (see 6).
2. Its builder, `fzx_<name>(fzx*)`, in `examples_electronics.c`,
   `_mechanisms.c` or `_both.c`, declared in `examples_parts.h`, drawing as a
   hand would with `placer.h` and the `fzx_*` helpers (`fzx_part`, `fzx_wire`,
   `fzx_route`, `fzx_text`, `fzx_belt`...).
3. Its builder in `examples.c`'s `FZX_BUILDERS` (in the enum's order, checked
   against `FUDE_ZOOM_EXAMPLE_COUNT` as it compiles), its folder
   (`fude_zoom_example_set`; its group's big projects unless named) and, if it
   breaks something on purpose, `fude_zoom_example_goes_wrong`.
4. Its name in `page_custom.c`'s `FUDE_ZOOM_PAGE_EXAMPLE_NAMES` and its strings
   (`ZOOM_EX_...`).
5. Tests run every example: `tests/zoom` (`test_examples_electronics`: each
   works as it says, every wire joined at both ends; every one run three seconds
   within its parts' limits, unless it goes wrong on purpose;
   `test_example_sets`) and `tests/mech` (`test_examples`: it builds, its world
   starts, it has a circuit exactly when its group is both).
6. Its number is used by `--example=E@F` (`zoom/look.c`), by
   `tools/check/shots.txt` and in `docs/quality_plan.md`'s measurements: check
   them when numbers move. A person's example canvases are found by their title,
   not their number (`fude_zoom_page_example_canvas`), so a number may move but a
   title must not change.

## Making a new app

Every app has `apps/<app>/src/version.h`: its name (`<APP>_NAME`, which
`tools/strings/build.py` reads for `{APP}` in the strings, and from which its id
comes: the save folder and the backup's extension, which must not change once it
has users), its version (the one place it is written: `tools/build/app.sh` gives
it to the builder) and its store id. Every app has its project file,
`apps/<app>/<app>.rdeproj`: a copy of its kind's ("The builds"), with its name,
its folders and its package names changed. Every app also has a line in
`tools/strings/build.py`'s `APPS` (the layers its strings come from); a released
one also in `build_all.sh`, `warnings.sh` and `shots.sh` (their `APPS`),
`tools/licences/build.py` and `tools/store/site.py`, with its `platform/ios/` and
`platform/android/deps.lock`.

**A drawing app** (Draw is one): its shell (`apps/draw/draw.c`: what RDE calls,
what it owns), its `fude_app_info` (its name, or an id: the save folder and the
backup's extension come from it, through `fude_app_start`), its line in
`tools/strings/build.py`'s `APPS` (the core's layer; its own words, where it
wants them, in its `tools/strings.py`), and its assets (Roboto, Phosphor, the
flags, a config, and Noto Sans JP's names for the language list). It builds
from the core's sources alone: its project file, a copy of Draw's
(`apps/draw/draw.rdeproj`), takes only `fude/drawing/`.
What it adds later goes through `extension.h`:
- its tools on the bar
- what it draws over the page, and presses there (diagram shapes, connectors)
- its rows of the page's menus
- its side panel section

and its own screens go in the app's table (`app.h`'s `screens`, `screen.h`).

**A study app** (Hanzi for Chinese, Hangul for Korean, Thai, Hindi, Arabic): copy Kana's shape.
- `apps/<app>/<app>.c`: what it is for the study shell (`study/app/shell.h`) —
  its info, its lectures, its side panel's entries, its extension
  (`FUDE_STUDY_EXTENSION` plus its own) — and a `main()` handing them over.
- Its project file, a copy of Kana's: its name, `fude/lang/<code>/` in its
  `sources` (every file of its language), its package names, and in `[profile
  ios]` its language's text recognition model (the `MLKitTextRecognition...`
  framework and `ocr/<code>`; neither when ML Kit has none, as Thai's). Its line
  in `tools/strings/build.py`'s `APPS` (the core's, the study's and its
  language's layers); and its own strings in `tools/strings.py`, with its
  `strings_<code>.py`.
- Its language (`fude/lang/<code>/`, below), and its data bake.
- `shots.sh`'s `CP` (the character its viewer is shot at; it also tells the
  study apps from the canvas apps there), and `tools/mlkit/setup.py`'s `OCR`
  table.

**A canvas app** (InfiNote, InfiniWorkshop): copy InfiNote's shape.
- `apps/<app>/<app>.c`: its licences, its `fude_app_info`, its
  `fude_zoom_shell_def` (its product, its home screen's words, its welcome) and
  a `main()` handing it to `fude_zoom_shell_run`. About 50 lines.
- What it offers is a row of `product.h`: a new product is a new
  `FUDE_ZOOM_PRODUCT_` value and a row in `product.c`'s `FZPD_PRODUCTS` (the
  canvas never asks which app it is, only what its row says).
- Its project file, a copy of InfiNote's (an app that records: whisper.cpp for
  speech to text, and the microphone on Android) or of InfiniWorkshop's (one that
  does not). Its `assets` are `build/<app>-stage/assets/`, where
  `tools/build/app.sh` puts Sketching's with its own over them.
- Its line in `tools/strings/build.py`'s `APPS` (the core's and the canvas's
  layers).

## The language layer

The study layer teaches whichever language its app compiles: it asks
`fude/lang/lang.h`, and each app compiles one implementation, in
`fude/lang/<code>/` (Kana: `fude/lang/ja`; Hanzi: `fude/lang/zh`; Hangul:
`fude/lang/ko`; Thai: `fude/lang/th`; Hindi: `fude/lang/hi`; Arabic:
`fude/lang/ar`). The binding is at link time: a study app's project file takes
every file of `fude/lang/<code>/` (its `lang_ios.m` on iOS only), and nothing
else changes.

What a language gives (`lang.h` has each, with Japanese's answer):

| What | Asked by | Japanese |
|---|---|---|
| Codes: translation, the handwriting model, the voice | `services/` (`translate`, `speech_ios.m`), the study shell (hands the model to `drawing/ink/mlkit.h`), the screens that translate | `ja`, `ja`, `ja-JP` |
| The text recognizer's options (iOS) | `services/textscan_ios.m` | `lang_ios.m`: `MLKJapaneseTextRecognizerOptions` (the only file that links the Japanese model) |
| Whether ML Kit can read its printed text (`fude_lang_text_readable`) | Text from a photo, offered only then | yes (Thai and Arabic: no; ML Kit has no model for either) |
| Which characters combine with the one before (`fude_lang_combining`): a vowel sign or mark, practised over a dotted circle, taken with its letter in a word test's box and written over it by Paste text | `chars/glyph.c`, `screens/wordexam.c`, `handwriting/textink.c` | none (Thai's ั ิ ่, Hindi's ि ् ं, Arabic's harakat) |
| The writing direction (`fude_lang_rtl`), the form a letter takes between its neighbours (`fude_lang_form`) and the letter a form is of (`fude_lang_letter`) | the word test (boxes from the right, each letter's form), Paste text (joined forms meeting at their joins), exams and recognition (a form is right when its letter is read) | left to right, each letter itself (Arabic: right to left, its joined forms, ﻻ) |
| Groups: the kinds of character, each a script's letters (read as themselves, with a Latin name) and/or a set learnt whole (an exam source, a statistics row, of its core characters) | the catalog, Browse's chips, exams, statistics, the viewer, sheets; `fude_lang_is_letter` (`lang.h`, built from them for every language): the study's own reader, sheets | hiragana and katakana (script, set), kanji |
| Levels: how many, the byte each is stored as, short and long names | the catalog, Browse, exams, statistics, the viewer, practice, sheets | JLPT N5..N1 as 5..1, "N5" / "JLPT N5" |
| Readings: up to two kinds, their names, how Latin letters read, how they are compared, which kind a one-character word is read by, whether words are read in Latin letters | the catalog's search and sorts, the word card, Check, Vocabulary's search, vocab | on and kun; romaji to hiragana; katakana as hiragana; kun then on; no (Korean: yes, 학교 hakgyo) |
| Badges: the characters headers and labels wear | the viewer, exams, Vocabulary, word exams, the translator | 音 訓 部 試 語 似 記 訳 |
| How a line's words are found (`fude_lang_words`, in `wordsplit.c`): the letters a word is looked up for, what a character is compared as, where a word can end — or a matcher of its own | the word splitter (`study/chars/wordsplit.h`): Text from a photo, page text, the word card, the viewer, the translator | a matcher of its own: the longest stretch with a kanji, as written or with a conjugation undone |
| How free writing is read (`fude_lang_writing`, in `lang.c`, giving a `fude_lang_writing_def`): what switching script costs, the look-alikes across scripts and how the neighbours choose, letters at two sizes, marks that change a letter, punctuation with a cell of its own, letters out of use, which characters say where a line is | the study's own handwriting reader (`study/handwriting/segment.c`), used when ML Kit is off or has no reading: Check, and the selection's Copy as text, Translate and Save word | kana and kanji (ロ 口, や ゃ, か が, 、。・, ゐ); the other five give an empty one |
| The fourth UI language (`fude_lang_ui_language`): its `RDE_LANGUAGE_`, its name in itself and its flag (`assets/flags/<cc>.png`) | the study shell (the UI's fourth language), the translator | `RDE_LANGUAGE_JA_JP`, 日本語, jp |
| Strings (`strings.py`) | the study's ids that name the language (Text from a photo's hints, Into Japanese, ML Kit's model, the voice, the word form's reading, the exam intro, the sheet's credit) and the ids `lang.c` returns | `fude/lang/ja/strings.py` |
| The character data (`bake.c`, the format in `study/chars/kanji.h`): its level byte and two reading lists are the language's to fill | Kana's `--bake` | KanjiVG, KANJIDIC2, JMdict, Tatoeba, the JLPT lists |
| Whether a word is read as written (`fude_lang_reads_as_written`) | the word card: a word typed without a reading takes its written form as its reading | yes for a word all in kana (コーヒー) |
| Its chart of letters (`fude_lang_chart`, in `chart.c`; NULL for Chinese, which has none), its sections marked `mirrored` where they are laid out right to left already | the study's one chart screen (`study/screens/chart.c`, `FUDE_STUDY_SCREEN_CHART`), offered in the side panel when there is a chart (`fude_study_nav_has_chart`) | the kana chart (Korean: Hangul; Thai, Hindi, Arabic: their letters, Arabic's with each letter's joined forms) |

Numbers that are saved keep their meaning across languages because they are
counted from the language's own lists: a filter (`catalog.h`) is all, each
group, each level, studying, known; an exam source (`exam.h`, kept in the exam
log) is studying, known, each level, each set, selection, review. For Japanese
they are the numbers Kana always had.

**Still Japanese-shaped in the study layer** (fine for Kana; to look at when a
language needs it):
- The handwriting reader of its own (used when ML Kit is off or has no reading): `match.c`'s
  marks of written text (、。ー々・！？), and `segment.c`'s cells, KanjiVG's box.
  The language's own rules for reading free writing are behind `lang.h`
  (`fude_lang_writing`). ML Kit, on by default, reads every language.
- The catalog's default order goes by school grade (`fude_catalog_grade_key`:
  Japan's Kyouiku grades); another language leaves the grade at 0 and the
  order falls to frequency.
- Browse's Parts toggle wears 部 (it is hidden for Thai, Hindi and Arabic).
- The file formats' names: `.kana` files, the `KANA` header, `KANABKP`. They
  are formats, not the app. (The save folder is each app's own, from its id:
  Kana's users' saves are in `kana/` because that is Kana's.)

**A new language** (`docs/chinese_korean_data.md` has what Chinese and Korean
use, `docs/hindi_thai_data.md` what Hindi and Thai use):
1. `fude/lang/<code>/lang.c` answering `lang.h`, and `lang_ios.m` with its
   text recognizer's options. Its text recognition in the builds:
   `tools/mlkit/setup.py`'s `OCR` table (or `NO_OCR` when ML Kit has no model
   for it) and the model in its app's `[profile ios]`, and on Android
   `FudeText.java`'s options and the `text-recognition-<lang>` root in the app's
   `deps.lock`.
2. `wordsplit.c`: `fude_lang_words`, how its words are found (`lang.h`).
3. `chart.c` with its `fude_lang_chart`, or `lang.c` returning NULL for it, as
   Chinese does.
4. `strings.py`: every id `fude/lang/ja/strings.py` has, in the language's
   words, plus its own group, reading and prompt names. Its fourth-language
   `strings_<code>.py` beside every layer's strings file (`fude/drawing/`,
   `fude/study/`, `fude/lang/<code>/`, the app's `tools/`), or build.py stops.
   Its flag in `assets/flags/`, and an `RDE_LANGUAGE_` value for it in RDE.
5. `bake.c`: its data, in `study/chars/kanji.h`'s format — read from the
   PREPARED tables (`study/chars/bake.h`) its tools write; Thai's, Hindi's and
   Arabic's tools share `tools/data/prepared.py`. Romanization with letters
   Roboto lacks needs the app's `latin_font` (`drawing/app/info.h`).
6. The app ("Making a new app"): its strings are the core's, the study's, the
   language's, then its own (`tools/strings/build.py`). The language's
   `strings.py` says what it is as a fourth language (`language()`: its tag, its
   name, and its locale lines: `@plural`, and `@direction = rtl`, `@digits`,
   `@decimal` and `@group` where it needs them, as Arabic's does).

## The canvas layer

`fude/zoom` is the deep-zoom canvas, and `fude/sim` the simulation under it. The three canvas apps share one shell
(`zoom/shell.c`), each handing it its description (`apps/<app>/<app>.c`, about 50 lines): what each offers is its
product (`zoom/product.h`), and the canvas asks it what is offered, never which app it is. How the canvas came to be
is `docs/infinite_canvas_design.md` (the history, decision by decision), the split into apps `docs/product_split.md`,
and the simulation's plan `docs/simulation_architecture.md`. How it works now is in the design notes,
`docs/design/` (`README.md` lists them): the page driver (`page_driver.md`), drawing and its caches (`rendering.md`),
the scene, its index and its file (`scene_and_index.md`), circuits (`circuits.md`), mechanisms (`mechanisms.md`), and
PDFs and the threads (`pdf_and_threads.md`).

**How it joins the core.** The canvas is a page kind (`extension.h`'s `page_kind`, `FUDE_ZOOM_PAGE_KIND`): the core's
toolbar, menus, side panel, Settings and session drive it as they drive the ink page, and the session opens and saves
a canvas through it. Its tools for the bar, its rows of the page's menus, its Settings sections (Measurements, the
core's Handwriting, and Display: the frame rate in every build, and in a developer's or a measuring build a test
canvas of a million points to watch it on: `docs/design/page_driver.md`, "Settings") and its own widgets come through the same extension
(`zoom/shell.c`'s `FUDE_ZOOM_SHELL_EXTENSION`, the same in every canvas app). The core's ink, canvas and ink page are
still made: the ink holds the brush the toolbar and Settings set, while the ink page sits idle.

**What an app offers** (`product.h`). A product is a constant row of bit masks (`fude_zoom_product`, the three rows in
`product.c`): its topics (`FUDE_ZOOM_TOPIC_`, the Topic menu's), the bar's tools (`FUDE_ZOOM_TOOL_`), Insert's
entries (`FUDE_ZOOM_INSERT_`), the instruments (`FUDE_ZOOM_TOOLKIT_`) and the exports (`FUDE_ZOOM_OUT_`), with a few
switches (`hobby`: a part's limits, Make part, Make body; `workshop`: the workshop's themes; `audio`: recording with
the ink) and what it calls its General and Maths topics. A topic lists what it shows; the bar shows those of them the
product allows (`fude_zoom_product_keep`). A canvas saved in a topic this app lacks opens in the app's first
(`fude_zoom_product_topic`), so every canvas app opens every canvas. The shell sets the product once, before the page
is made (`fude_zoom_product_set`), and nothing in `fude/zoom` asks which app it is (`rules.py`'s `app-identity`
count checks it).

**The page driver** is `page.c` (the coordinator: the one page, undo and redo, the page kind's hooks, init and
destroy) and 29 `page_<what>.c` modules, one for each thing the page does: the file, the pen, shapes, text, find,
measuring, circuits, mechanisms, diagrams, audio, speech, settings, the frame, input, drawing, the look harness and
the rest. `page.h` is what the rest of the app sees; `page_parts.h` is what the modules share, and its overview lists
which module holds what. A new feature of the page goes in the module of its kind, or a new one listed there. Only
one page exists at a time: the hooks, which are handed the app and not the page, reach it through
`fude_zoom_page_the`. `docs/design/page_driver.md` has the page's state and its life.

| Part | Modules | Is |
|---|---|---|
| The canvas in memory | `zoom` (ids, points (RDE's `rde_vec_2D`, so the plane's geometry, a point in a polygon, the distance to a segment, an area, is `rde_math`'s), boxes, f64 transforms), `scene` (frames, objects, the camera; `scene_history` undo and redo, `scene_records` the journal's records, sharing `scene_parts.h`), `index` (an R-tree for each frame), `codec` (a stroke's points packed, about 1.6 bytes a point) | nested frames, so any depth keeps full precision: each frame has its own units. Nothing is deleted by an edit: erasing marks objects dead, so undo only flips flags (`scene.h`) |
| Its files | `zfile` (`<id>.zoom`: the journal appended about every second, checkpointed, compacted now and then; read back to the last whole step), `pages` (a notebook's pages), `words` (`<id>.words`, so Find looks through every canvas), `homelist` (what the home screen lists) | `zfile.h` has the format; every other side file is `<id>.<something>` beside it |
| Drawing it | `render` (each frame, with its caches keyed by what they were made from; its modules `render_stroke`, `_shape`, `_text`, `_symbol`, sharing `render_parts.h`), `paper`, `themes` (the workshop's colours), `rich` (a note's marks), `tex` and `mathfont` (formulas typeset), `symbol` (with `symbol_diagrams` and `symbol_shapes`, sharing `symbol_parts.h`) and `symbolart` (the diagram library; a symbol drawn from an SVG in `assets/parts`), `chart`, `plot`, `graph` (Mermaid flowcharts: `graph_read`, `_order`, `_place`, `_layout`, sharing `graph_parts.h`) | |
| The tools | `select` (the lasso's selection), `erase`, `shape`, `smooth`, `snap`, `fill`, `bucket`, `instrument` (with `instrument_hands`, `_pen`, `_draw`, sharing `instrument_parts.h`), `connect`, `trim`, `cut`, `nest`, `sheet`, `piece`, `placer` (drawing by code: the examples' hand), `units`, `nav`, `map` | each tool's model, without UI; the page drives them |
| The topics | `circuit` (the coordinator, and its modules `circuit_catalogue`, `_read`, `_draw`, `_wires`, `_build`, `_blocks`, `_stamp`, `_solve`, `_step`, `_play`, sharing `circuit_parts.h`; with `display`, `logic`, `limits`, `coupling`), `props` (what a drawn thing is to a simulation, kept as an attribute object), `mech` and `mechrun` (mechanisms, run as rigid bodies in RDE's 2D physics), `plan`, `formula`, `calc` and `texcalc`, `mathread`, `handmath`, `handfind`, `hand` (with `hand_glyphs.h`, `hand_maths.h`), `examples` (by the list's groups: `examples_electronics`, `_mechanisms`, `_both`, sharing `examples_parts.h`), `custom` | what each topic adds; `sim` simulates circuits (`sim.h`, `sparse.h`) and gives bodies their shapes (`body.h`); a drawn symbol's part (a circuit's, a mechanism's, a plan's) is found through one table of `symbol.c`'s (`fude_zoom_symbol_topic_part`), which each topic's `_of_kind` reads |
| The cards | `bodyform`, `choiceform`, `kanbanform`, `limitsform`, `partsform`, `pointform`, `repeatform`, `sheetform`, `sizeform`, `valueform`, `numpad` | small cards that ask for a value; the forms are the kit's form (`kit.h`: `fude_kit_form`: the card, its title, Cancel, a tap off it, laying it out again, fields, chips, a keypad), each keeping only what it asks |
| Out of the app | `export` (the walk) and `export_formats` (SVG, PDF, DXF, tiles, the print check, the cut list), `pdf` (a PDF writer in plain C), `dxf`, `stl`, `video` (H.264 through the system's encoder: `video_apple.m`, `video_android.c`) | |
| Sound | `audio` and `adpcm` (a canvas's recordings), `voice` (recording and playing), `listen` (speech to text, whisper.cpp), `transcript`, `fetch` (the speech model's download: `fetch_apple.m`, `fetch_android.c`) | InfiNote's and Sketching's; InfiniWorkshop records nothing (`product.h`'s `audio`) |
| The app around it | `shell` (the canvas apps' shell), `product` (what each app offers), `look` (the developer's flags; the page's own harness is `page_look.c`), `inktest` (the test ink: `--gate`'s canvas, and Settings' test canvas), `home` (the home screen of canvases, notebooks or projects), `pdfview` (a canvas over a PDF) | |

**The simulation** (`fude/sim`) is pure C with no canvas and no UI; `tests/sim` tests it on its own.

| File | Holds |
|---|---|
| `sim.h` | the interface: definitions (a part's ports, each in a domain, its parameters and its model; or a composite of parts), the library that owns them, elaboration and the digital run |
| `def.c` | definitions and the library; a definition's text written and read back (`fude_sim_def_write`, `fude_sim_def_read`: what a custom part's `.part` file holds) |
| `elab.c` | a composite flattened into primitives and numbered nets (`fude_sim_elaborate`), its nets joined through RDE's `rde_disjoint_set` so the numbering does not depend on the order of the joins |
| `digital.c` | the event engine (`fude_sim_run_init`, `_until`, `_settle`): values 0, 1, Z and X, inertial delays, an event heap |
| `logic.c` | the logic family's models: gates, flip-flops, the latch, memory |
| `chips.c` | the built-in 74HC and CD4000 chips, each a custom part's text (`fude_sim_chips_add`) |
| `sparse.h`, `sparse.c` | the analog solver's sparse matrix: its order (minimum degree) and fill worked out once, then factored and solved each step |
| `body.h`, `body.c` | a drawn outline made into a rigid body: simplified, cut into convex pieces, its area, centre and inertia; the materials |

The analog circuit itself is built and stepped in `zoom/circuit_build.c` to `circuit_step.c` on `sparse.h`, and
mechanisms run in RDE's 2D physics (`zoom/mechrun.c`): `docs/design/circuits.md` and `docs/design/mechanisms.md`.

**Threads.** The canvas runs on the main thread like everything else ("Threads", above), with these exceptions:
RDE's audio threads fill and drain `voice.c`'s ring buffers, pumped once a frame, and drain the stream a circuit's
speaker is heard through (`page_mech.c`: `fude_zoom_page_sound_on`, pushed from the main thread); `listen.c` reads one
piece of a recording at a time on a thread of its own; `fetch` downloads on the platform's thread, its state polled;
`pdfview.c` draws a PDF's tiles on the PDF worker `doc.c` uses (`drawing/doc/worker.h`), one at a time; handwriting
is read by ML Kit (`drawing/ink/mlkit.h`), polled. A zoom video is made on the main thread, which may wait for the
system's encoder (`video.h`: up to 2 seconds a frame on Apple's). `docs/design/pdf_and_threads.md` has the hand-offs.

**Files.** A canvas is `notes/<id>.zoom`, with `<id>.tools` (the instruments as they lie), `<id>.pages` (a notebook's
pages), `<id>.words`, `<id>.cover.png` (its home screen card), `<id>.audio`, `<id>.rec<n>` (recordings) and
`<id>.rec<n>.text` (transcripts) beside it; removing the canvas removes every `<id>.*` in the notes folder
(`notes.c`: `fude_notes_delete_files`, which finds them by name, so a side file added later is never left behind).
My pieces are in `pieces/` (`piece.h`) and My parts in `parts/` (`<id>.part` each, and `templates.txt`: `custom.h`).
The speech model is in the downloads folder (`save.h`), outside the save folder.
