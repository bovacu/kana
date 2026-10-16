# The page driver

The page driver is the deep-zoom canvas as a page of the app: it takes the pen, the fingers, the mouse and the keys,
hands them to the tool in hand, keeps everything that lives between frames, draws the canvas and what goes over it,
and says when the canvas is saved. It is `fude/zoom/page.c` and its 29 `page_<what>.c` modules. It is the top of the
canvas layer: the scene (`scene.h`), the renderer (`render.h`) and the tools' models (`select.h`, `erase.h`,
`instrument.h`, `circuit.h` and the rest) know nothing of it, and it brings them together.

This note says how it is put together and why. Each module's own overview has the details; `page_parts.h` lists which
module holds what. The scene and its file are in `scene_and_index.md`, the renderer in `rendering.md`.

---

## Where it sits

```
apps/<app>/<app>.c        a fude_zoom_shell_def: what the app is (about 50 lines, no logic)
        │  fude_zoom_shell_run(&def, argc, argv)
fude/zoom/shell.c         RDE's callbacks; owns the core's parts, the UI, the session, the page and the home screen
        │  FUDE_ZOOM_SHELL_EXTENSION (extension.h): the page kind, the bar's tools, menus, Settings, widgets, papers
fude/zoom/page.c          the coordinator: the one page, undo and redo, the page kind's hooks, init and destroy
fude/zoom/page_*.c        one module for each thing the page does (page_parts.h: which holds what)
        │
scene.h  render.h  select.h  erase.h  instrument.h  circuit.h  mech.h  pdfview.h ...   the models, without UI
```

**An app is a description.** `apps/infinote/infinote.c` (and `sketching.c`, `infiniworkshop.c`) fills a
`fude_zoom_shell_def`: its `fude_app_info`, its `product` (`product.h`'s `FUDE_ZOOM_PRODUCT_`), its home screen's
title and New button, whether New makes a notebook and of what page size, its welcome (or none), and whether it opens
on its home screen. Its `main` calls `fude_zoom_shell_run`. Nothing else in the app is code: what two apps do lives in
`fude` (`coding_rules.md` §1, §4).

**The shell is the program.** `shell.c` keeps everything as statics, made once in `fude_zoom_shell_init` and freed in
`fude_zoom_shell_end`, so there is one app a process. Its init order matters, each step needing the ones before:
the window's units, the language, `fude_zoom_product_set(shell->product)` (before the page is made, since the page
fills its bar from the product), the workshop's themes where the product has them (`fude_zoom_themes_use`), the core's
ink, canvas, lasso and notes, the app (`fude_app_start`: the save folder), the core's page input,
`fude_zoom_page_init`, the home screen (`fude_zoom_shell_place_home`: the app's only screen), the UI, then
`fude_session_load`, which opens the last canvas through the page kind.

**The core drives the page through the page kind.** `extension.h`'s `fude_page_kind` is how the drawing core's
toolbar, menus, side panel and session reach a page that is not its own ink page. `FUDE_ZOOM_PAGE_KIND` (page.c)
answers undo, redo, can undo, can redo, clear, reset view, open, revision, busy, save, the selection's box, the menu's
commands, paste, what the pickers brought (`fude_zoom_kind_imported`) and Back. Its hooks are handed the app, not the
page, so page.c keeps one pointer, `fude_zoom_page_the`, set by `fude_zoom_page_init` and cleared by
`fude_zoom_page_destroy`; the bar's choice callbacks reach the page the same way.

**The canvas asks the product what it offers, never which app it is.** `fude_zoom_product_get` returns the app's
`fude_zoom_product`: bit masks of its topics, tools, Insert entries, instruments and exports, and a few flags (the
project's lasso actions, the workshop's themes, recordings). Every product is a constant in `product.c`.

## The modules

| Module | Holds |
|---|---|
| `page.c` | the one page, undo and redo, the page kind, init, destroy, trim |
| `page_frame.c` | `fude_zoom_page_update`, each frame's steps |
| `page_input.c` | a press, a move, a lift handed to the tool; fingers, a flick; the events |
| `page_render.c` | `fude_zoom_page_render`: the paper, the canvas, everything over it |
| `page_topics.c` | the bar's tools: the base tools' table, the topics, the Motion and Pages tools |
| `page_file.c` | opening and closing a canvas, saving, the instruments' file and undo, a PDF's view, a notebook's pages |
| `page_ui.c` | the page's own widgets and cards: built, styled, updated, hit-tested |
| `page_look.c` | the developer's look harness (`fude_zoom_page_look_*`) |
| the other 22 | one feature each: the pen, shapes, text, find, measuring, circuits, mechanisms, diagrams, audio, ... |

page.c was one file of some 26,000 lines; it was split by what each part does, its code moved unchanged
(`page_parts.h`'s overview). A function one module calls in another is declared in `page_parts.h`; one used by its own
module alone stays `RDE_INTERNAL` there. A new feature goes in the module of its kind, or in a new module listed in
`page_parts.h`.

## The page and its life

**One struct.** `fude_zoom_page` (page.h) is everything the page keeps between frames: the scene, the renderer, the
maths fonts, the eraser, the file, the stroke being captured (a `fude_ink` of its own, so the pen samples exactly as
Kana's), the lasso's selection, the instruments, the circuit and the mechanism being played, a PDF's view, the
recordings, a notebook's pages, and each feature's flags, scratch arrays and widgets, grouped by what they serve. Most
fields belong to one module. Growing arrays are on the std allocator (`coding_rules.md` §3).

| When | What | Where |
|---|---|---|
| The app starts | the page zeroed and its parts made: a device number for ids (new each launch: `scene.h`), the renderer, audio, the maths font, the symbols' SVG art, the diagram library's tiles, every scratch array; the bar filled for General | `fude_zoom_page_init` |
| A canvas opens | the canvas before is closed; a fresh scene read from `<id>.zoom`; the selection let go; the instruments and the topic read from `<id>.tools`; the instruments' undo emptied; a notebook's pages read; a PDF's view opened; the recordings opened | `fude_zoom_page_open` (page_file.c) |
| Each frame | the update, then the drawing (below) | `fude_zoom_page_update`, `fude_zoom_page_render` |
| A canvas closes | what is under way finished or let go (Play stopped, a template kept, a push cancelled, a video dropped, a text being written saved); saved and checkpointed; its words kept for Find; recordings closed; the renderer's caches and the page's per-canvas caches emptied; the views and indices the page held into it let go (`fude_zoom_page_forget_views`); the PDF closed; the scene freed | `fude_zoom_page_close` (page_file.c) |
| Memory runs short | the renderer's caches, the strip's pictures, the arrows' previews, the map's picture, the notes' fonts' glyphs | `fude_zoom_page_trim` |
| The app ends | closed (saved), then everything freed | `fude_zoom_page_destroy` |

**What is the canvas's, and what is the page's.** Object indices and frame slots are a scene's own: a new scene gives
the same numbers to other things (and a new canvas made in the same launch even gives out the same ids). So anything
that holds one is the canvas's and must be emptied as it closes: the renderer's caches (`fude_zoom_render_trim`), the
junction dots and the attributes looked through (page_file.c resets their counters), the strip's pictures, the map.
`fude_zoom_page_forget_views` lets go of the rest: the flight under way, Back's list, the empty screen's marks, the
depth's levels, Places' rows, a tape left showing, a curve's points, the line's chip, what the page's menu opened on
(`context_target`), and the renderer's `visible` list, so the pen is mapped through nothing until the new canvas is
drawn. Without it a flight carried on, or a mark, level or place tapped, would put the camera in a frame the new scene
does not have. Back's list names frames by id, which outlive slots, but it goes too: ids are a canvas's own, and Back
would fly to whatever has the same id in the next one. What carries over from canvas to canvas is the page's: the
renderer itself, the maths fonts, the pen's settings, the eraser's mode, the units, the diagram library's tiles, and
the lasso's clipboard (its clips own their bytes, so copying on one canvas and pasting on another works).

## A frame

RDE calls the shell; the shell calls the page. The shell's update and render go through `drawing/app/look.h`'s
timers, so `--perf` measures them.

```
fude_zoom_shell_update                         fude_zoom_shell_render_frame
  fude_look_frame (a developer's looks)          fude_zoom_page_render_offscreen   (cover, map, strip, export,
  fude_ui_follow_language                                                           video, arrows' previews)
  fude_zoom_page_topic_follow (the bar)          clear, begin 2D
  ink frames begun (the app's and the page's)    fude_app_render (a screen on top), else fude_zoom_page_render
  fude_app_update (a screen on top),             fude_app_render_overlays
    else fude_zoom_page_update                   end 2D
  fude_zoom_look_frame                         then RDE's top overlay: fude_zoom_page_render_top, the notice
  fude_ui_update
  fude_session_update (the autosave)
```

**The update** (`fude_zoom_page_update`, page_frame.c) runs in a fixed order, each step a block or a
`fude_zoom_page_update_*` function: a flick coasting; the examples, a template, Settings' test canvas and the
library's held tile carried on;
the instruments kept on the drawing as it is now (before the pen meets them); the layers' bits read again when the
revision moved, and the lasso's length chip; what is under way (the pickers' files, handwriting being read, a PNG or a
video frame, a flight, the pen resting long enough to snap a shape); the bar's taps; the keys; another tool chosen
(the selection let go); a long press opening the menu; the mouse (on a computer); the camera settled
(`fude_zoom_camera_settle`, only while nothing is drawn, rubbed out, lassoed or flown); the selection; Play's step,
timed on its own; Back's key; what follows the view (the instruments' undo step, a PDF's pages, a notebook's pages,
the recordings, Find opened from another canvas); and last the journal flushed, after everything that may have changed
the canvas.

**Input.** Events come one at a time (`fude_zoom_page_event`, page_input.c); the keys, the mouse's moves and its
wheel are read once a frame in the update instead, as the drawing core's page reads its keys (the code gives no other
reason). No key acts while a text field has the keyboard, and no key clears the canvas. One press is
followed at a time: the pen's, the mouse's left button, or a finger's when fingers write. `fude_zoom_page_down` hands
it to the first thing that takes it, in this order: presenting; the diagram library's and Play's panels; a text being
written; the map's, My pieces', the strip's, Find's and audio's panels; Play (only parts worked by hand); the marks;
a guide pulled from a sheet's ruler; the compass's label; the instruments; the Fill, Curve and text tools; what the
lasso holds, whatever the tool; then the tool in hand (wire, room, trim, measure, Nudge, the eraser, a shape, the
lasso, a note's task box or link, else the pen). `fude_zoom_page_moved` and `fude_zoom_page_up` go to whatever took
the press. The order is the design: a panel over the canvas must win over the canvas under it, and what the lasso
holds must stay movable with any tool, so no one has to go back to the lasso to move it. Other fingers move the view:
one pans, two pan and pinch; a quick tap of two is undo and of three redo; one finger held still opens the page's
menu. A pen's eraser end rubs out whatever the tool.

**Drawing** (`fude_zoom_page_render`, page_render.c) goes bottom to top: the paper, a PDF's pages, the canvas
(`fude_zoom_render`, with what is playing: a mechanism's moving parts, graphs, a recording's faint ink), the mechanism
as it plays, its bodies and tracers, the alignment lines, the sheets' rulers stuck to the screen's edges; the stroke or
shape being drawn; the selection; the marks, the measure, guides, readouts, constraints, the circuit, nodes and a text
being written; the instruments, the offset pad and the brush; then the panels. Presenting leaves out what only helps
to draw. The renderer is told what it draws with each frame (its fonts, units, the lifted and faint objects), and the
pointers set for one draw are put back after it.

**Off-screen draws come first.** A cover, the map, a strip page, an export or a video frame is drawn with the same
renderer and scene, the camera swapped for the draw and put back. They run in `fude_zoom_page_render_offscreen`,
before the page's own draw, so that the renderer's `visible` list (the frames the last draw went into, which the pen
is mapped through and the tools edit: `fude_zoom_render_editable`) is always the screen's when input comes.

## Settings

The canvas's Settings sections are `page_settings.c`'s, handed to the core in `FUDE_ZOOM_PAGE_SECTIONS` (each one
four functions: build, layout, refresh, restyle): Measurements (the units, the true size's calibration, the printer's
check, the kerf, how parts are fitted to a board), the core's Handwriting, and Display. What they change is saved with
the settings (`page_export.c` gathers it). The rows are laid out as the core's are: a row's words at the left, its
buttons at the right; words too long for their room wrap, and the row is as tall as their lines.

**Display**, in every build, is for seeing how smoothly the device draws. Its first row shows the frame rate at the
top right (`fude_zoom_page_render_fps`; while it shows, where each frame's time goes is logged every 2 s). Its second,
"A test canvas", is only in a developer's or a measuring build (`FUDE_ZOOM_INKTEST_OFFERED`, `inktest.h`: `RDE_DEBUG`
or `FUDE_ALLOW_ARGS`, so the Mac's `measure` and the tablet's `android-measure`; never a release that ships). It has Make: a new canvas, named "Drawing test" in the app's language, filled with a million points of
the test ink (`inktest.h`), the very canvas the developer's `--gate=1000000` measures: rose curves of 400 points with
pressure and time, over four depths each 1024× inside the last, then the camera back at the top. The person turns the
frame rate on, pans and zooms.

- **It never touches the person's canvases.** Make adds a canvas of its own at the top level and opens it
  (`fude_zoom_inktest_canvas`: a new one every time, even beside one of the same name), closes Settings and shows a
  notice. The session opens the canvas at the end of that frame; the next frame's update fills it
  (`fude_zoom_page_inktest_follow`, beside the examples' and the templates' follows), and only if it holds nothing,
  so a file left behind under its id is never drawn over.
- **It is filled in one go, not over frames.** The ink is placed round the camera as the camera goes down through the
  depths and back, so the camera must be the fill's until it is done; spread over frames, a pan, a zoom or a stroke
  would land in the middle of it, and the canvas would not be the one `--gate` measures. The notice is drawn the frame
  before, so the person sees something happen; the fill takes 66 ms on an M2 Mac in a release build (half a second
  in a debug one); a tablet's time is not measured yet.
- `--inktest=N` (zoom/look.c) presses Make at frame N, and `--settings-end` (the core's) shows the row.

## Tools and topics

The bar shows the core's tools (undo, the pen, the eraser, the lasso, colour, paper), then the app's: up to
`FUDE_EXTENSION_TOOLS` (12) slots in `FUDE_ZOOM_TOOLS`, which the shell's extension points at. Slot 0 is always Topic;
the rest are the topic's.

```
FUDE_ZOOM_TOPICS[topic]          each topic's tools (indices into FUDE_ZOOM_BASE_TOOLS) and its Insert, Instruments
        │                        and Export lists (indices into each tool's own choice table)
        │  fude_zoom_product_keep: only what the app's product allows
FUDE_ZOOM_TOOLS[0..11]           Topic, then the topic's tools; Insert, Instruments and Export given the topic's
                                 lists, each choice's press remapped to the table's own index
```

`fude_zoom_topic_fill` builds it (page_topics.c). The tables are in `product.h`'s enum orders (`FUDE_ZOOM_TOOL_`,
`FUDE_ZOOM_INSERT_`, `FUDE_ZOOM_TOOLKIT_`, `FUDE_ZOOM_OUT_`, `FUDE_ZOOM_TOPIC_`), and a typedef with an array of size
−1 (page_topics.c's `fude_zoom_page_tables_as_product_h`) refuses to compile when one table's length does not match
its enum. A topic's list holds the table's indices, so a choice keeps its meaning whatever topic shows it;
`fude_zoom_topic_insert_choose` and its kin hand the press on with the table's own index. The check sees each table's
length. Every one of them is sized by its entries, never by a number, so a row left out does not compile, rather than
being read as a topic, a tool or a choice of zeros (no name, nothing done): the topics and the base tools are checked
in page_topics.c; each tool's choices are declared in `page_parts.h` without a size, beside the count the bar takes
(`FUDE_ZOOM_INSERT_COUNT`, `FUDE_ZOOM_SHAPES_CHOICES_N`…), and checked against it in the file that defines them.

**A choice sets the pen.** Most choices do not act at once; they set what the pen will do next (`shape_tool`,
`fill_tool`, `curve_tool`, `measure_tool`, `text_tool`, `wire_tool`, `room_tool`, `trim_tool`, the Saw, Nudge), and
`fude_zoom_page_down` routes the next press by those flags. Choosing one of the core's own tools counts in the
toolbar's `tool_taps`; the update sees the count move and puts the pen's modes down
(`fude_zoom_page_pen_modes_off`), so the pen draws again. Insert's cards and panels open at once instead.

**A topic changes the bar the frame after.** `fude_zoom_topic_choose` puts down what the last topic's tools left in
hand (the pen's modes, the Saw, the parts' panels, the instruments the new topic does not have), sets the page's
topic and saves it in `<id>.tools`. `fude_zoom_page_topic_follow`, which the shell calls every frame, sees the topic
differ from the one the bar was filled for, fills it again and rebuilds the UI at once (`fude_ui_rebuild_now`). It is
never done inside the press: the press is the Topic panel's own, whose tables would change under it. A canvas opened
from another app takes one of this app's topics (`fude_zoom_product_topic`).

## Undo

The drawing's undo is the scene's history (`scene_history.c`, described in `scene_and_index.md`): every gesture
changes the scene first, then records one step naming what it made, erased and moved
(`fude_zoom_history_push`, `_push_moved`, `_push_all`, `_push_tagged`). Undo and redo flip flags and put places back;
nothing is deleted by an edit. The page adds four things.

- **When.** `fude_zoom_page_undo` and `fude_zoom_page_redo` refuse while a stroke, an erase or the lasso is under
  way, and while something plays (with a notice: playing is only playing, nothing is edited). The kind's `busy` hook
  tells the session when a gesture is under way, and it does not save then.
- **The instruments' own steps.** The ruler, set squares and the rest lie on the drawing but are not in the scene.
  Their moves are undone in memory only (`fude_zoom_page_tools_history`, page_file.c): once a frame, instruments left
  changed with no hand on them make a step of before and after, kept with the drawing's place in its history
  (`history_dropped + action_count`). Undo takes whichever is later, redo whichever comes first, so the two histories
  interleave as they happened; a drawing step after an instrument's undo drops their redo, as the drawing's own redo
  is dropped. At most `FUDE_ZOOM_TOOLS_STEPS` (64) are kept.
- **Tags.** A step can carry the page's own number (`fude_zoom_action.tag`): a notebook's page added is a step with
  only its tag in it, and `fude_zoom_page_pages_step` takes the page out again when it is undone.
- **Going to the change.** Undo and redo fly the camera to the step's frame and box (`fude_zoom_page_show`) unless it is
  on the screen already and at least 4 points across, framed at about 60% of the screen. The step knows its frame and
  box for this alone.

Clear (the bar's bin) deletes the selection if there is one, else everything alive, at every depth, that a tool may
touch, as one step (`fude_zoom_page_clear`, through `scene.h`'s `fude_zoom_scene_clear`). It leaves the frames, the
places and the layers' records (their names, eyes, locks and order are not what is drawn), and whatever a hidden or
locked layer holds (`fude_zoom_scene_touchable`): a locked layer's things are not to be changed, and a hidden one's
would go unseen. A second Clear finds nothing and makes no step. While something plays the bin does nothing but say so.

## Saving

The page decides when; the file (`zfile.h`) decides how.

- **About every second** (`FUDE_ZOOM_PAGE_FLUSH`), last in the update, while there is anything in the journal and
  nothing is being drawn or rubbed out, `fude_zoom_page_save_now(page, false)` appends it to the file and syncs. So a
  kill loses at most about a second of finished gestures.
- **The session's autosave** (`fude_session_update`) calls the kind's `save` after a quiet spell; it flushes the same
  way. The kind's `revision` (the scene's) tells it there is something to save, and `busy` holds it off mid-gesture.
- **Leaving** (another canvas, the background, the end) saves with `_leaving` set: the journal flushed, then a
  checkpoint when anything was journaled or the camera moved since the last, because only a checkpoint keeps the
  camera, and the app should open where it was left.
- **Beside the file**: `<id>.tools` (the instruments and the topic) when they changed, `<id>.pages`, `<id>.words` (as
  the canvas closes), `<id>.cover.png`, the recordings. The frames an instrument lies in, and the camera's when
  leaving, are kept in the file first (`fude_zoom_page_keep_frames`), since an empty frame is not written and what
  names it would find nothing on reopening.

## Threads

The page runs on the main thread only. What is slow runs elsewhere and is polled once a frame: handwriting
recognition (ML Kit), speech to text (`listen.h`'s thread), a PDF's tiles (`pdfview.h`'s worker) and the audio
threads' ring buffers (`voice.h`). `pdf_and_threads.md` has them.

## Invariants

- One page. `fude_zoom_page_the` is valid between `fude_zoom_page_init` and `fude_zoom_page_destroy`; every hook and
  callback that can come with no canvas open checks `open`.
- Anything holding an object index or frame slot is the canvas's: reset it in `fude_zoom_page_open` or
  `fude_zoom_page_close` (what names a view or a place, in `fude_zoom_page_forget_views`). Frame ids are safer than
  slots but not unique across canvases.
- Every change goes through the scene's functions (so it is journaled) and is recorded as one history step per
  gesture. Nothing is edited while Play runs; only the parts worked by hand respond.
- `busy` must be true for as long as a gesture holds half-made state; the session must not save in the middle of it.
- Off-screen draws happen before the page's own draw, and put the camera back.
- The bar's tables stay in `product.h`'s orders; a topic stays one number in `<id>.tools`, so topics are only ever
  added at the end of `FUDE_ZOOM_TOPIC_`.
- `fude_zoom_page_down`'s order is deliberate; a new taker goes where it must win, and no earlier.

## How to change common things

**A new choice in Insert, Instruments or Export.** Add its value at the end of the enum in `product.h` and its entry
at the same place in the table (`FUDE_ZOOM_PICTURE_CHOICES` in page_pictures.c, `FUDE_ZOOM_INSTRUMENT_CHOICES` in
page_instruments.c, `FUDE_ZOOM_EXPORT_CHOICES` in page_export.c; `page_parts.h` declares Insert's and Export's with a
number for their size, so change that too, and Instruments' with `FUDE_ZOOM_INSTRUMENT_COUNT + 5u`). The size check
does not see a row left out, so count them. Handle its index in the table's choose function
(`fude_zoom_picture_choose` and its kin). Add its index to the lists of the topics that offer it (`FUDE_ZOOM_TOPICS`),
and its bit to each product that has it (`product.c`). Its words go in `fude/zoom/strings.py`, then
`python3 tools/strings/build.py all`.

**A new tool on the bar.** The same, for `FUDE_ZOOM_TOOL_` and `FUDE_ZOOM_BASE_TOOLS` (and page_topics.c's
`FUDE_ZOOM_BASE_` names). If the pen does something new with it in hand: a flag in `fude_zoom_page`, set by its choice
and cleared in `fude_zoom_page_pen_modes_off`; a case in `fude_zoom_page_down` at the place it must win, and in
`fude_zoom_page_moved` and `fude_zoom_page_up` if it follows the press; its drawing in page_render.c's
`fude_zoom_page_render_all`; and its gesture recorded as one history step. A topic shows at most 11 tools.

**A new topic.** A value at the end of `FUDE_ZOOM_TOPIC_` (product.h), its entry at the same place in
`FUDE_ZOOM_TOPICS` (its name, icon, tools and lists), its bit in the products that have it, and its name in
strings.py. If it has parts of its own in the diagram library, a `FUDE_ZOOM_PAGE_LIBRARY_` mask for its tabs
(`page_parts.h`).

**A new feature's state.** Fields in `fude_zoom_page` under a comment saying what they serve; arrays made in
`fude_zoom_page_init` and freed in `fude_zoom_page_destroy`; anything of the canvas's reset in open or close; its
cross-module functions in `page_parts.h`; a `fude_zoom_page_look_*` in page_look.c so the look harness can show it.

## Where the tests are

- `tests/zoom` (`bash tests/run.sh zoom`): the models the page drives, and the page's own parts that can run headless:
  `test_history_tags` (a step's tag kept, through the file), `test_pages` (a notebook's pages), `test_navigation`
  (flights, the empty screen's marks), `test_products` (each app's topics and choices), `test_clear` (what Clear
  takes and leaves), `test_select_abort`, `test_select_frames`, `test_inktest` (Display's test canvas: the gate's
  ink byte for byte, and no other canvas touched).
- The look harness: `fude_zoom_page_look_*` (page_look.c) and the launch flags in `zoom/look.h` and `COMMANDS.txt`
  put the page into a state for a screenshot; `zsh tools/check/shots.sh` compares them with a baseline.
- `--stress` with the kill test (COMMANDS.txt) draws without pause and kills the app at random moments, checking the
  canvas reopens whole.
