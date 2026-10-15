// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PAGE_PARTS
#define FUDE_ZOOM_PAGE_PARTS

#include "zoom/page.h"
#include "zoom/hand.h"
#include "drawing/app/app.h"
#include "drawing/app/documents.h"
#include "drawing/app/ui.h"
#include "drawing/app/page.h"
#include "drawing/ink/notes.h"
#include "drawing/widgets/notice.h"
#include "drawing/app/pagemenu.h"
#include "drawing/app/toolbar.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/base/draw.h"
#include "drawing/base/icons.h"
#include "drawing/widgets/kit.h"
#include "drawing/app/import.h"
#include "drawing/doc/pdf.h"
#include "zoom/smooth.h"
#include "zoom/fill.h"
#include "zoom/export.h"
#include "zoom/pdf.h"
#include "zoom/texcalc.h"
#include "zoom/mathread.h"
#include "zoom/graph.h"
#include "zoom/bucket.h"
#include "drawing/ink/mlkit.h"
#include "zoom/connect.h"
#include "zoom/stl.h"
#include "zoom/trim.h"
#include "zoom/plot.h"
#include "zoom/chart.h"
#include "zoom/rich.h"
#include "zoom/paper.h"
#include "zoom/words.h"
#include "zoom/listen.h"
#include "zoom/fetch.h"
#include "zoom/symbolart.h"
#include "zoom/props.h"
#include "zoom/examples.h"
#include "zoom/placer.h"
#include "zoom/logic.h"
#include "sim/body.h"
#include "zoom/cut.h"
#include "zoom/nest.h"
#include "drawing/app/session.h"
#include "drawing/app/look.h"

// ===========================================================================
// Page parts: the page driver's private interface. The page (page.h) is written as page.c and the page_*.c modules;
// this is what they share with each other: the functions one module calls in another, the state more than one uses,
// and the constants and types more than one needs. Included by page.c and page_*.c only, never anywhere else:
// everything outside the page uses page.h.
//
// Where it sits: fude/zoom, under page.h (which it includes, with every header the page uses). The modules include
// this header and nothing else.
// Its main types and their lifecycle: the page itself (fude_zoom_page, page.h), made by fude_zoom_page_init and freed
// by fude_zoom_page_destroy (page.c). One page at a time: fude_zoom_page_the points to it, for the hooks and the
// callbacks that are handed no page. Each shared variable is defined once, in the module its declaration names.
// Threads: the main thread only, as the page itself.
// Files: none of its own (the page's files are described in page.h and in the modules that write them).
// Design: page.c was one file of some 26 000 lines; it was split by what each part does, its code moved unchanged.
// A function one module calls in another is declared here (not RDE_INTERNAL); what one module alone uses stays
// RDE_INTERNAL in it. A table another module measures with sizeof is declared with its size.
//
// Which module holds what:
//   page.c              the coordinator: the one page, undo and redo, the page kind's hooks, init and destroy
//   page_file.c         the canvas's file, the instruments kept with it and their undo, a PDF's, a notebook's pages
//   page_pen.c          the pen (a stroke, its shape snapped), the Saw, Sculpt, Nudge, the eraser, paths as strokes
//   page_shapes.c       the Shapes tool, a path's nodes, stretching, containers, the Curve tool
//   page_pictures.c     pictures and Insert's choices, the Fill tool
//   page_export.c       Export, a notebook's cover, what is drawn off screen, the zoom video, Smoothing, settings
//   page_instruments.c  the instruments snapping onto and lying on the drawing; Instruments' and Arrange's choices
//   page_circuit.c      electronics: wires, the Circuit tool, the circuit drawn while it plays, Value, aligning parts
//   page_mech.c         Play, a hand on a mechanism, a speaker's sound, mechanisms drawn, bodies, a part's limits
//   page_topics.c       the Motion and Pages tools, the base tools, topics (the bar's tools for a canvas)
//   page_text.c         texts and sticky notes, writing on the page, typed text by hand, formulas, Offset, To curve
//   page_find.c         Find, Find through every canvas, handwriting read as text
//   page_diagrams.c     a diagram from Mermaid text, the diagram library, building plans, Kanban boards
//   page_measure.c      measuring and snapping, guides, the readout, a line's length, sheets, the size card, Repeat
//   page_input.c        a press, a move, a lift sent to the tool; fingers, a flick, the events
//   page_nav.c          getting around: flying the camera, Back, the marks, the navigation buttons
//   page_frame.c        each frame (fude_zoom_page_update)
//   page_render.c       drawing: the paper, the page (fude_zoom_page_render), the marks' previews, the instruments
//   page_settings.c     Settings' sections: Measurements, the core's Handwriting listed, Display (a developer's build
//                       only); the true size, the offset pad
//   page_present.c      the map, Insert's Named area, Present
//   page_strip.c        a notebook's pages strip, the canvases Find found the words in
//   page_audio.c        audio synced to ink, and its panel
//   page_speech.c       speech to text (transcripts, Live text)
//   page_pieces.c       my pieces, a piece laid as a stencil, a symbol's text
//   page_custom.c       Insert → Examples, custom parts, the Custom tab and the templates
//   page_edit.c         fillet, chamfer, extend, trim, Combine, constraints, the lasso's row, the page's menu
//   page_layers.c       layers and their panel
//   page_boards.c       boards, fitting parts into a board, holes, joints
//   page_ui.c           the page's own widgets (built, styled, updated, hit-tested)
//   page_look.c         the developer's look harness (fude_zoom_page_look_*)
// ===========================================================================

// --- the constants and types the modules share (as page.c had them) --------------------------------------------------

// Music paper's staves on a notebook's page; the marks' arrows' previews and how near a press takes one.
#define FUDE_ZOOM_PAGE_STAFF      2.0     // music paper's staves' lines apart on a notebook's page (millimetres: a staff 8 tall)
#define FUDE_ZOOM_PAGE_PREVIEW_W  104.0f  // an arrow's preview on the screen (points)...
#define FUDE_ZOOM_PAGE_PREVIEW_H  72.0f
#define FUDE_ZOOM_PAGE_PREVIEW_PX 2.0f    // ...and its picture's pixels a point
#define FUDE_ZOOM_PAGE_MARK_REACH 26.0f   // ...and how near a press counts as on one

// The offset pad's grip; which hand holds something (the pen, the mouse, a finger plus its id).
#define FUDE_ZOOM_PAGE_ANCHOR_GRIP_W 30.0f   // the offset pad's grip
#define FUDE_ZOOM_PAGE_HAND_PEN    1ull     // (which hand holds it: the pen, the mouse, a finger + its id)
#define FUDE_ZOOM_PAGE_HAND_MOUSE  2ull
#define FUDE_ZOOM_PAGE_HAND_FINGER 16ull

// The diagram library's tabs for diagrams and for circuits (Insert → Diagram shapes, Insert → Circuit parts): bits, the
// symbols' families' and (after them) the connectors' and Custom's (what a person kept: every panel's, its own of it).
#define FUDE_ZOOM_PAGE_RICH_REACH 8.0   // screen points round a note's task box or link a tap still takes it
#define FUDE_ZOOM_PAGE_LINK_SLOP  8.0f  // ...and how far the pen goes on one before it is a stroke, not a tap
#define FUDE_ZOOM_PAGE_LIBRARY_CUSTOM_BIT (1u << (FUDE_ZOOM_SYMBOL_FAMILIES + 1u))
#define FUDE_ZOOM_PAGE_LIBRARY_PLAIN ((1u << FUDE_ZOOM_SYMBOL_FAMILY_SHAPES) | (1u << FUDE_ZOOM_SYMBOL_FAMILY_BRACKETS) | (1u << FUDE_ZOOM_SYMBOL_FAMILY_STAMPS) | \
                                      (1u << FUDE_ZOOM_SYMBOL_FAMILY_CHARTS) | (1u << FUDE_ZOOM_SYMBOL_FAMILY_GEOMETRY) | (1u << FUDE_ZOOM_SYMBOL_FAMILY_SCIENCE))   // (every topic's)
#define FUDE_ZOOM_PAGE_LIBRARY_DIAGRAMS (((1u << FUDE_ZOOM_SYMBOL_FAMILY_ELECTRONICS) - 1u) | FUDE_ZOOM_PAGE_LIBRARY_PLAIN | (1u << FUDE_ZOOM_SYMBOL_FAMILIES) | \
                                         FUDE_ZOOM_PAGE_LIBRARY_CUSTOM_BIT)
#define FUDE_ZOOM_PAGE_LIBRARY_MECHANISMS ((1u << FUDE_ZOOM_SYMBOL_FAMILY_MECHANISMS) | FUDE_ZOOM_PAGE_LIBRARY_CUSTOM_BIT)
#define FUDE_ZOOM_PAGE_LIBRARY_PLANS ((1u << FUDE_ZOOM_SYMBOL_FAMILY_FLOORPLAN) | (1u << FUDE_ZOOM_SYMBOL_FAMILY_WIRING) | FUDE_ZOOM_PAGE_LIBRARY_CUSTOM_BIT)
#define FUDE_ZOOM_PAGE_LIBRARY_MATHS ((1u << FUDE_ZOOM_SYMBOL_FAMILY_MATHS) | (1u << FUDE_ZOOM_SYMBOL_FAMILY_BRACKETS) | (1u << FUDE_ZOOM_SYMBOL_FAMILY_GEOMETRY) | \
                                      (1u << FUDE_ZOOM_SYMBOL_FAMILY_CHARTS) | FUDE_ZOOM_PAGE_LIBRARY_CUSTOM_BIT)
#define FUDE_ZOOM_PAGE_LIBRARY_SEWING ((1u << FUDE_ZOOM_SYMBOL_FAMILY_SEWING) | FUDE_ZOOM_PAGE_LIBRARY_CUSTOM_BIT)
#define FUDE_ZOOM_PAGE_LIBRARY_CIRCUITS ((1u << FUDE_ZOOM_SYMBOL_FAMILY_ELECTRONICS) | (1u << FUDE_ZOOM_SYMBOL_FAMILY_LOGIC) | \
                                         (1u << FUDE_ZOOM_SYMBOL_FAMILY_CHIPS) | (1u << FUDE_ZOOM_SYMBOL_FAMILY_BOARDS) | FUDE_ZOOM_PAGE_LIBRARY_CUSTOM_BIT)

// A run of a pen line's pieces: its first piece's index and how many (fude_zoom_page_line_runs).
typedef struct { u32 first, count; } fude_zoom_run;

// The compass's follow, another name for fude_zoom_page_instruments_follow (every instrument follows so).
#define fude_zoom_page_compass_follow fude_zoom_page_instruments_follow

// Tags beside the pen (their letters), and how near a pin or a wire a press takes it.
#define FUDE_ZOOM_PAGE_TAG_PX     13.0f  // a tag's letters (screen points): beside the pen; the most Play's are
#define FUDE_ZOOM_PAGE_TAG_LEAST  6.0f   // ...the least Play's are (smaller: not drawn, as a part's pin names)
#define FUDE_ZOOM_PAGE_PIN_REACH  28.0   // screen points a pin is taken from (a wire's end, a tap on a board's pin): more than it looks
#define FUDE_ZOOM_PAGE_WIRE_REACH 10.0   // ...a wire, to join another to it

// An oscilloscope's knob dragged: a step for each this far the hand goes right or up (screen points; more for a big
// knob — its width), after it has gone this far at all (else, let go: a tap).
#define FUDE_ZOOM_PAGE_KNOB_STEP  16.0
#define FUDE_ZOOM_PAGE_KNOB_STILL 6.0f

// Where Play's panel and its parts are on the screen (fude_zoom_page_play_lay).
typedef struct {
    fude_zoom_box panel;
    fude_zoom_box buttons[5];    // Pause / Go on, Back to the start, Stop, its speed, Step
    fude_zoom_box time;          // its time
    fude_zoom_box names[8];      // each variable's value (then each sensor's)
    fude_zoom_box sliders[8];    // ...and its track
} fude_zoom_page_play_layout;

// The text card's styles while it takes something other than a text (fude_zoom_page_text_card).
#define FUDE_ZOOM_PAGE_TEXT_MERMAID 0xFFu  // the text card's style while it takes a diagram's text
#define FUDE_ZOOM_PAGE_TEXT_FIND    0xFEu  // ...while it takes what to look for
#define FUDE_ZOOM_PAGE_TEXT_RENAME  0xFDu  // ...while it takes a layer's name
#define FUDE_ZOOM_PAGE_TEXT_BOARD   0xFCu  // ...while it takes a board's name
#define FUDE_ZOOM_PAGE_TEXT_PARTS   0xFBu  // ...while it takes a list of parts to fit into a board
#define FUDE_ZOOM_PAGE_TEXT_SYMBOL  0xFAu  // ...while it takes a diagram symbol's text
#define FUDE_ZOOM_PAGE_TEXT_AREA    0xF8u  // ...while it takes a new area's name (map.h)
#define FUDE_ZOOM_PAGE_TEXT_PIECE   0xF7u  // ...while it takes a new piece's name (piece.h)
#define FUDE_ZOOM_PAGE_TEXT_CUSTOM  0xF6u  // ...while it takes a kept piece's or custom part's new name (custom.h)

// A new text's, a sticky note's and a formula's letters on the screen (points).
#define FUDE_ZOOM_PAGE_TEXT_PX    18.0    // a new text's letters on screen (points)
#define FUDE_ZOOM_PAGE_STICKY_PX  16.0    // ...a note's
#define FUDE_ZOOM_PAGE_FORMULA_PX 24.0    // ...a formula's em

// A sticky note's width and a text's line height, in letters.
#define FUDE_ZOOM_PAGE_STICKY_W   11.0    // a note's width: so many letters' heights
#define FUDE_ZOOM_PAGE_TEXT_LINE  1.3     // a line's height: so many letters'

// The panel of canvases Find found the words in: its rows (points), how many shown at once.
#define FUDE_ZOOM_PAGE_EVERYWHERE_ROW    58.0    // a canvas's row in the panel (points)
#define FUDE_ZOOM_PAGE_EVERYWHERE_HEAD   40.0
#define FUDE_ZOOM_PAGE_EVERYWHERE_ROWS   4u      // shown at once (the rest scrolled to)

// How far a new dimension's line is drawn from what it measures.
#define FUDE_ZOOM_PAGE_DIMENSION_OFFSET 22.0   // a new dimension's line beside what it measures (screen points)

// The page's own widgets: margins, buttons' and chips' sizes, gaps, the Places and Layers panels (points).
#define FUDE_ZOOM_PAGE_UI_MARGIN 16.0f
#define FUDE_ZOOM_PAGE_UI_BACK_W 52.0f    // as the menu button beside it (side.c)
#define FUDE_ZOOM_PAGE_UI_BACK_H 44.0f
#define FUDE_ZOOM_PAGE_UI_CHIP_W 84.0f
#define FUDE_ZOOM_PAGE_UI_CHIP_H 36.0f
#define FUDE_ZOOM_PAGE_UI_GAP    6.0f
#define FUDE_ZOOM_PAGE_UI_PLACES_W 44.0f  // Places' button
#define FUDE_ZOOM_PAGE_UI_ROW_W  230.0f   // ...and its rows
#define FUDE_ZOOM_PAGE_UI_DROP_W 40.0f    // ...and each place's ×
#define FUDE_ZOOM_PAGE_LAYERS_W  (200.0f + 5.0f * FUDE_ZOOM_PAGE_UI_GAP + 204.0f)   // the layers' panel: a row, its eye, lock, up, down and trash

// A new symbol's letters on the screen.
#define FUDE_ZOOM_PAGE_SYMBOL_PX  15.0     // a new symbol's letters (screen points)

// Its tabs: the symbols' families, then the connectors' styles, then Custom (what a person kept: custom.h).
#define FUDE_ZOOM_PAGE_LIBRARY_CONNECTORS FUDE_ZOOM_SYMBOL_FAMILIES
#define FUDE_ZOOM_PAGE_LIBRARY_CUSTOM     (FUDE_ZOOM_SYMBOL_FAMILIES + 1u)
#define FUDE_ZOOM_PAGE_LIBRARY_TABS_N     (FUDE_ZOOM_SYMBOL_FAMILIES + 2u)

// Where the diagram library's panel and its parts are (fude_zoom_page_library_lay).
typedef struct {
    fude_zoom_box panel, close, grid;
    fude_zoom_box tabs[FUDE_ZOOM_PAGE_LIBRARY_TABS_N];
    u32           cols;
    f32           content;   // the grid's whole height (its rows)
} fude_zoom_page_library_layout;

// A notebook's pages strip: a page's picture's height and the strip's padding (points).
#define FUDE_ZOOM_PAGE_STRIP_H     150.0   // a page's picture in it, its height (points)
#define FUDE_ZOOM_PAGE_STRIP_PAD   12.0

// The strip's close button.
#define FUDE_ZOOM_PAGE_STRIP_CLOSE 48.0    // the strip's ×, at its right end

// Where the pages strip and its parts are (fude_zoom_page_strip_lay).
typedef struct {
    fude_zoom_box panel, inner, close;
    f64           tw, th;   // a page's picture
} fude_zoom_page_strip_layout;

// The speech model's size in megabytes, as the download asks.
#define FUDE_ZOOM_PAGE_LISTEN_MB        57u                         // the model's size, as said

// --- the state the modules share (each defined once, in the module named) --------------------------------------------

// The one page (the page kind's hooks are handed the app, not the page). Defined in page.c.
extern fude_zoom_page* fude_zoom_page_the;
// The toolbar's tables of choices are declared here without their sizes, on purpose: a size here would make a table a
// row short compile, its missing row zero-filled (a choice with no name that does nothing). Each is checked against
// its count where it is defined; the bar (page_topics.c) takes these counts.
#define FUDE_ZOOM_SHAPES_CHOICES_N    16u   // the Shapes tool's choices
#define FUDE_ZOOM_SMOOTHING_CHOICES_N 7u    // Smoothing's levels, then Sculpt and Nudge
#define FUDE_ZOOM_CIRCUIT_CHOICES_N   3u    // the circuit tool's choices
#define FUDE_ZOOM_PLAN_CHOICES_N      5u    // the Plan tool's: three walls, a room, the symbols
// The toolbar's Shapes: its choices. Defined in page_shapes.c.
extern const fude_extension_choice FUDE_ZOOM_SHAPES_CHOICES[];   // (FUDE_ZOOM_SHAPES_CHOICES_N of them: checked where it is defined)
// The toolbar's Insert: its choices (product.h numbers them: FUDE_ZOOM_INSERT_). Defined in page_pictures.c.
extern const fude_extension_choice FUDE_ZOOM_PICTURE_CHOICES[];   // (FUDE_ZOOM_INSERT_COUNT of them: checked where it is defined)
// The toolbar's Export: its choices (product.h numbers them: FUDE_ZOOM_OUT_). Defined in page_export.c.
extern const fude_extension_choice FUDE_ZOOM_EXPORT_CHOICES[];   // (FUDE_ZOOM_OUT_COUNT of them: checked where it is defined)
// The toolbar's Smoothing: its choices, in FUDE_ZOOM_SMOOTH_ order, then Sculpt and Nudge. Defined in page_export.c.
extern const fude_extension_choice FUDE_ZOOM_SMOOTHING_CHOICES[];   // (FUDE_ZOOM_SMOOTHING_CHOICES_N of them: checked where it is defined)
// The toolbar's Instruments: its choices (product.h numbers them: FUDE_ZOOM_TOOLKIT_). Defined in page_instruments.c.
extern const fude_extension_choice FUDE_ZOOM_INSTRUMENT_CHOICES[];   // (FUDE_ZOOM_TOOLKIT_COUNT of them: checked where it is defined)
// The toolbar's Arrange: its choices. Defined in page_instruments.c.
extern const fude_extension_choice FUDE_ZOOM_ARRANGE_CHOICES[];   // (FUDE_ZOOM_ARRANGE_COUNT of them: checked where it is defined)
// The Circuit tool's choices: Wire, Play, Parts. Defined in page_circuit.c.
extern const fude_extension_choice FUDE_ZOOM_CIRCUIT_CHOICES[];   // (FUDE_ZOOM_CIRCUIT_CHOICES_N of them: checked where it is defined)
// The topic the bar was last filled for. Defined in page_topics.c.
extern u8 fude_zoom_topic_now;
// Each symbol's name in the app's language, in symbol.c's order (FUDE_ZOOM_SYMBOL_KINDS of them). Defined in
// page_diagrams.c. Declared without its size on purpose: a size here would make a shorter table compile, its missing
// names zero-filled; without one, page_diagrams.c's check sees how many there really are.
extern const FUDE_TEXT_ FUDE_ZOOM_PAGE_SYMBOL_NAMES[];
// The Plan tool's choices: walls 10, 15 and 25 cm thick, a room, the plan symbols. Defined in page_diagrams.c.
extern const fude_extension_choice FUDE_ZOOM_PLAN_CHOICES[];   // (FUDE_ZOOM_PLAN_CHOICES_N of them: checked where it is defined)

// --- page.c: the coordinator: the page, undo and redo, the page kind's hooks -----------------------------------------

// The box the screen shows, in the camera's frame.
fude_zoom_box fude_zoom_page_view(const fude_zoom_page* _page);
// Undo: the latest step (an instrument's or the drawing's), the camera flown to it if off screen.
b8 fude_zoom_page_undo(fude_zoom_page* _page);
// Redo: the next step (an instrument's or the drawing's), the camera flown to it if off screen.
b8 fude_zoom_page_redo(fude_zoom_page* _page);
// The bar's bin: what the lasso holds (as Delete), else what Clear takes (scene.h's fude_zoom_scene_clear: not the
// places, the layers, or a hidden or locked layer's things); not while playing.
void fude_zoom_page_clear(fude_zoom_page* _page);
// Back to the start: the camera flown home, at zoom 1.
void fude_zoom_page_reset_view(fude_zoom_page* _page);
// The eraser's next mode, said in a notice.
void fude_zoom_page_next_eraser(fude_zoom_page* _page);
// Back and Escape: the first of these that is up goes (page.c has the order): Present, the map, the pages strip, Find's
// panel of every canvas, the audio panel, a text being written on the page (saved), the diagram library, My pieces;
// else a template opened from a canvas goes back to it. False when none was, so the app's own Back runs.
b8 fude_zoom_kind_back(fude_app* _app);

// --- page_file.c: the canvas's file, the instruments' file and undo, a PDF's and a notebook's pages ------------------

// The path of canvas _canvas's .zoom file (beside its notes file) into _out.
void fude_zoom_page_path(u32 _canvas, c8* _out, usize _size);
// The instruments out (and the canvas's topic) written beside the canvas, when they changed.
void fude_zoom_page_tools_save(fude_zoom_page* _page);
// Once a frame, and before an undo or redo: instruments left changed since the last step make a step.
void fude_zoom_page_tools_history(fude_zoom_page* _page);
// Undo of an instrument step, when it is the latest (later than the drawing's). True: one was undone.
b8 fude_zoom_page_tools_undo(fude_zoom_page* _page);
// Redo of an instrument step, when it is the next (not later than the drawing's). True: one was redone.
b8 fude_zoom_page_tools_redo(fude_zoom_page* _page);
// The frames the instruments lie in (and, leaving, the camera's) kept by the file, so they open again.
void fude_zoom_page_keep_frames(fude_zoom_page* _page, b8 _camera);
// The canvas saved now (_leaving: with a checkpoint, the camera kept too). False: it could not be.
b8 fude_zoom_page_save_now(fude_zoom_page* _page, b8 _leaving);
// A push let go of without keeping it (a pinch after all, the canvas closing): the lines as they were, shown again.
void fude_zoom_page_nudge_cancel(fude_zoom_page* _page);
// The open canvas closed: what plays stopped, what is under way finished or let go, saved, freed, and what the page
// kept that names its frames or objects (caches, a flight, Back's list, the marks, Places' rows...) let go with it.
void fude_zoom_page_close(fude_zoom_page* _page);
// The view kept over the canvas's pages (_b, home units: a PDF's, a notebook's), never past them.
void fude_zoom_page_keep_view_over(fude_zoom_page* _page, fude_zoom_box _b, f64 _least);
// A notebook's pages written beside the canvas.
void fude_zoom_page_pages_save(fude_zoom_page* _page);
// The tag of a step that changed a notebook's page count by _delta (0: no tag).
u32 fude_zoom_page_pages_tag(i32 _delta);
// A step undone (_undo) or redone: its pages' change, if it had one, undone or done again — kept.
void fude_zoom_page_pages_step(fude_zoom_page* _page, u32 _tag, b8 _undo);
// A notebook's pages rearranged (one put in, taken out or moved), what is on them with them; one step.
b8 fude_zoom_page_pages_do(fude_zoom_page* _page, u8 _do, u32 _i, u32 _j);
// Is it a notebook (its pages what the view is kept over: not a PDF's)?
b8 fude_zoom_page_paged(const fude_zoom_page* _page);
// A notebook's first view: its first page's width across the screen, its top at the top (as a PDF's).
void fude_zoom_page_pages_first_view(fude_zoom_page* _page);
// A notebook's view kept over its pages, and pages added to one past the last written on.
void fude_zoom_page_pages_follow(fude_zoom_page* _page);
// Canvas _canvas opened: its file read, its instruments, pages, recordings and view as they were left.
void fude_zoom_page_open(fude_zoom_page* _page, u32 _canvas);

// --- page_pen.c: the pen, the Saw, Sculpt, Nudge, the eraser, paths as strokes ---------------------------------------

// The pen down to draw at _screen: a stroke begun in the frame it is drawn in.
void fude_zoom_page_draw_down(fude_zoom_page* _page, rde_vec_2F _screen, b8 _from_pen);
// The pen moved while drawing: the stroke goes on (or the snapped shape follows the pen).
void fude_zoom_page_draw_moved(fude_zoom_page* _page, rde_vec_2F _screen);
// The drawing frame's units → the screen, now.
fude_zoom_sim fude_zoom_page_draw_sim(const fude_zoom_page* _page);
// The pen has rested: the stroke so far, if it looks like a shape, becomes it.
void fude_zoom_page_try_snap(fude_zoom_page* _page);
// The snapped shape as the pen has it now (screen units): a line's end follows the pen, others grow and turn.
fude_zoom_shape_fit fude_zoom_page_fit_now(const fude_zoom_page* _page);
// A shape in screen units into frame _frame (seen through _to): its numbers in that frame's units.
u32 fude_zoom_page_add_fit(fude_zoom_page* _page, u32 _frame, fude_zoom_sim _to, const fude_zoom_shape_fit* _f, rde_color _color, f32 _radius, u8 _flags);
// A shape in screen units drawn over the page while it is placed: its line _radius points, filled if _filled.
void fude_zoom_page_draw_fit(fude_zoom_page* _page, const fude_zoom_shape_fit* _f, f32 _radius, rde_color _color, b8 _filled);
// The saw's kerf in millimetres (Settings', or the default).
f64 fude_zoom_page_kerf(const fude_zoom_page* _page);
// A line drawn with the Saw (_frame's units): closed, its inside cut out; open, a cut along it.
void fude_zoom_page_saw_line(fude_zoom_page* _page, u32 _frame, const fude_zoom_v2* _p, u32 _n, b8 _closed);
// A pen line's pieces (_pieces, _n) in the order drawn, as runs into _runs. How many runs.
u32 fude_zoom_page_line_runs(const fude_zoom_scene* _s, u32* _pieces, u32 _n, fude_zoom_run* _runs);
// A run's points, frame units, into _out (appended; the point pieces share where they join given once).
void fude_zoom_page_run_points(const fude_zoom_scene* _s, const u32* _pieces, fude_zoom_run _run, rde_arr* _out);
// Nudge: the pen down; the lines within its reach taken to be pushed.
void fude_zoom_page_nudge_down(fude_zoom_page* _page, rde_vec_2F _screen);
// The pen moved: what is within reach pushed along with it, most at its middle, nothing at its edge.
void fude_zoom_page_nudge_moved(fude_zoom_page* _page, rde_vec_2F _screen);
// Nudge: the pen lifted; the lines pushed kept where they are now (one undo step).
void fude_zoom_page_nudge_up(fude_zoom_page* _page);
// While nudging: the lines pushed, where they are now, and the pen's reach.
void fude_zoom_page_render_nudge(fude_zoom_page* _page);
// The pen lifted while drawing: the stroke (or the shape it snapped to) put into the canvas.
void fude_zoom_page_draw_up(fude_zoom_page* _page);
// A stroke that turned out to be a gesture (a second finger): never drawn.
void fude_zoom_page_draw_cancel(fude_zoom_page* _page);
// The eraser down at _screen, in its mode.
void fude_zoom_page_erase_down(fude_zoom_page* _page, rde_vec_2F _screen);
// The eraser moved to _screen: what it passed over rubbed out.
void fude_zoom_page_erase_moved(fude_zoom_page* _page, rde_vec_2F _screen);
// The eraser lifted: what it rubbed out made one undo step.
void fude_zoom_page_erase_up(fude_zoom_page* _page);
// A path (frame units) added as a stroke of even width, its pieces into _born.
void fude_zoom_page_add_path(fude_zoom_page* _page, u32 _frame, const fude_zoom_v2* _p, u32 _n, b8 _closed, rde_color _color, f32 _radius, rde_arr* _born);

// --- page_shapes.c: the Shapes tool, nodes, stretching, containers, the Curve tool -----------------------------------

// The shape being dragged out with the Shapes tool, from where the pen went down to where it is.
fude_zoom_shape_fit fude_zoom_page_drag_fit(const fude_zoom_page* _page);
// Home-frame units a unit of _frame is (the canvas's units: what a brush "on the page" is measured in).
f64 fude_zoom_page_home_units(const fude_zoom_page* _page, u32 _frame);
// The brush's half-width in _frame's units (_frame_to_screen: that frame's scale on the screen).
f32 fude_zoom_page_brush_radius(const fude_zoom_page* _page, u32 _frame, f64 _frame_to_screen);
// Point _on of the instrument's edge moved out to where the pen's line runs beside it.
rde_vec_2F fude_zoom_page_ruled_beside(const fude_zoom_page* _page, fude_zoom_v2 _on);
// Turned onto a crossing edge: the corner between the line beside each edge drawn first.
void fude_zoom_page_ruled_corner(fude_zoom_page* _page);
// The pen down with a shape chosen: the shape dragged out from _screen (snapped).
void fude_zoom_page_shape_down(fude_zoom_page* _page, rde_vec_2F _screen);
// The thing under _screen as a Lasso tap takes it (a guide first, else the topmost); FUDE_ZOOM_NONE: none.
u32 fude_zoom_page_under(fude_zoom_page* _page, rde_vec_2F _screen);
// A tap with the Lasso: the thing under it selected whole; on nothing, nothing.
void fude_zoom_page_tap_select(fude_zoom_page* _page, rde_vec_2F _screen);
// The pen lifted with a shape chosen: the shape dragged out put down (a tap: none).
void fude_zoom_page_shape_up(fude_zoom_page* _page);
// What the lasso holds that has a line of its own given a line style, hatch and pen width; one step.
void fude_zoom_page_relook_into(fude_zoom_page* _page, i32 _style, i32 _hatch, f64 _pen_mm, rde_arr* _dead_all, rde_arr* _born_all, fude_zoom_box* _box_all);
// The toolbar's Shapes: choice _index taken (a shape, the Curve, filled, a line style, a pen, a hatch).
b8 fude_zoom_shapes_choose(fude_app* _app, u32 _index);
// The toolbar's Shapes: is choice _index the one in use?
b8 fude_zoom_shapes_chosen(const fude_app* _app, u32 _index);
// The toolbar's Shapes: is a shape (or the Curve) in hand?
b8 fude_zoom_shapes_selected(const fude_app* _app);
// Path or polygon _object's numbers, how many nodes and where they start, and its units to the screen.
u32 fude_zoom_page_nodes(const fude_zoom_page* _page, u32 _object, f64* _n, u32* _count, u32* _first, fude_zoom_sim* _to_screen);
// A press on a side's handle: that side taken.
b8 fude_zoom_page_stretch_down(fude_zoom_page* _page, rde_vec_2F _screen);
// Millimetres one of a thing's own units.
f64 fude_zoom_page_object_mm(const fude_zoom_page* _page, u32 _object);
// A side held moved to _screen: the shape stretched that way (no smaller than it may be).
void fude_zoom_page_stretch_moved(fude_zoom_page* _page, rde_vec_2F _screen);
// A side let go: the shape made so, one undo step, the lasso on the new one.
void fude_zoom_page_stretch_up(fude_zoom_page* _page);
// A lone shape's side handles, or, while one is held, its outline as it will be.
void fude_zoom_page_render_stretch(fude_zoom_page* _page);
// Is _object a container (an area, a measured sheet, a symbol things are put in)?
b8 fude_zoom_page_container(const fude_zoom_scene* _s, u32 _object);
// What lies wholly inside container _c (not on a hidden or locked layer), appended to _out.
void fude_zoom_page_container_contents(const fude_zoom_scene* _s, u32 _c, rde_arr* _out);
// The lasso's press just taken: each container it holds carries what lies inside it.
void fude_zoom_page_carry(fude_zoom_page* _page);
// A press on a node of the lassoed path or polygon (or on its line, to add one). True: it was.
b8 fude_zoom_page_node_down(fude_zoom_page* _page, rde_vec_2F _screen);
// The node held moved to _screen (snapped).
void fude_zoom_page_node_moved(fude_zoom_page* _page, rde_vec_2F _screen);
// The node held let go where it is now; or a tap on the line made a node there.
void fude_zoom_page_node_up(fude_zoom_page* _page);
// The lassoed path's or polygon's nodes as handles; the one held where the pen is.
void fude_zoom_page_render_nodes(fude_zoom_page* _page);
// The curve put down as a PATH (its nodes kept, to move later), in the brush's colour; one step.
void fude_zoom_page_curve_commit(fude_zoom_page* _page, b8 _closed);
// The Curve tool's Type a point chip: the card (in the way the last was typed).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_type_chip(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// A tap with the Curve tool: a point added; on the last point, the curve finished; on the first, closed.
void fude_zoom_page_curve_tap(fude_zoom_page* _page, rde_vec_2F _screen);
// The curve so far, on to where the pen hovers; its points as small rings.
void fude_zoom_page_render_curve(fude_zoom_page* _page);

// --- page_pictures.c: pictures and Insert's choices, the Fill tool ---------------------------------------------------

// The page kind's imported hook: the pictures brought in put in the middle of the screen, selected.
void fude_zoom_kind_imported(fude_app* _app, u8 _kind, const c8* const* _paths, u32 _count);
// The toolbar's Insert: choice _index taken (a picture, a text, a note, a board, the library...).
b8 fude_zoom_picture_choose(fude_app* _app, u32 _index);
// The toolbar's Insert: is choice _index the one in use (the text tools)?
b8 fude_zoom_picture_chosen(const fude_app* _app, u32 _index);
// The toolbar's Insert: is one of its tools in hand?
b8 fude_zoom_picture_selected(const fude_app* _app);
// The pen given to one of the app's own tools (Fill, a shape): what the pen does is that tool's.
void fude_zoom_page_pen_under(fude_zoom_page* _page);
// The pen drawing again: whatever one of the app's tools had it do put down.
void fude_zoom_page_pen_modes_off(fude_zoom_page* _page);
// The ink on the screen as segments with their half-widths, appended to _ink_out (what closes an area).
void fude_zoom_page_view_ink(fude_zoom_page* _page, rde_arr* _ink_out, b8 _walls);
// The Fill tool at _screen: the closed area under it filled with the brush's colour (one step).
void fude_zoom_page_fill_at(fude_zoom_page* _page, rde_vec_2F _screen);
// The toolbar's Fill: in hand or put down (the Shapes tool, and the bar's own tools, put it down too).
void fude_zoom_fill_press(fude_app* _app);
// The toolbar's Fill: is it in hand?
b8 fude_zoom_fill_selected(const fude_app* _app);

// --- page_export.c: export, the cover, off-screen drawing, the zoom video, Smoothing, the page's settings ------------

// The materials' names in the app's language into _out (the first, no material, as _none).
void fude_zoom_page_material_words(const c8** _out, const c8* _none);
// The toolbar's Export: choice _index taken (a format, the tiles, the cut list, the video...).
b8 fude_zoom_export_choose(fude_app* _app, u32 _index);
// Notebook page _i's paper drawn into a picture of it (its cover, the strip's), lines at true spacing.
void fude_zoom_page_draw_sheet(fude_zoom_page* _page, u32 _i);
// The PNG's view drawn the frame before: read back, encoded, written, shared.
void fude_zoom_page_export_finish(fude_zoom_page* _page);
// A zoom video under way let go (its file and its picture freed).
void fude_zoom_page_video_drop(fude_zoom_page* _page);
// The frame drawn last frame: read back, into the video; the last: the video finished, shared or saved.
void fude_zoom_page_video_step(fude_zoom_page* _page);
// The toolbar's Smoothing: is choice _index the one in use (a level, Sculpt, Nudge)?
b8 fude_zoom_smoothing_chosen(const fude_app* _app, u32 _index);
// The toolbar's Smoothing: never shown as in hand (it is a setting).
b8 fude_zoom_smoothing_selected(const fude_app* _app);
// Settings' units choice _choice taken: millimetres, centimetres, inches or feet and inches.
void fude_zoom_page_set_units(fude_zoom_page* _page, u8 _choice);

// --- page_instruments.c: the instruments snapping and lying on the drawing -------------------------------------------

// The instrument in hand just moved: one of its key points snapped onto what is drawn near it.
void fude_zoom_page_instrument_snap(fude_zoom_page* _page, f64 _radius);
// Instrument _slot taken as lying where it is now on the drawing (its pinned point, a real length).
void fude_zoom_page_instrument_stick(fude_zoom_page* _page, u32 _slot);
// Instrument _slot's pinned point on the screen as it lies now (its key it was snapped by, or its middle).
fude_zoom_v2 fude_zoom_page_instrument_pin(const fude_zoom_page* _page, u32 _slot);
// The edge the pen draws along held on the drawing (the one last drawn along).
void fude_zoom_page_pin_ruled(fude_zoom_page* _page);
// Once a frame: each instrument where the hand puts it, or where its pinned point is now.
void fude_zoom_page_instruments_follow(fude_zoom_page* _page);
// The pen down on a compass's radius: its numpad.
b8 fude_zoom_page_compass_label_down(fude_zoom_page* _page, rde_vec_2F _screen);
// The toolbar's Instruments: choice _index taken (another instrument out, each put away by its own ×; the Stencil's
// pieces; the Tape, Dimension, the Saw, Guides or Trim on or off). True keeps the panel open.
b8 fude_zoom_instruments_choose(fude_app* _app, u32 _index);
// The toolbar's Instruments: is choice _index in use?
b8 fude_zoom_instruments_chosen(const fude_app* _app, u32 _index);
// The tool lit while the pen measures or saws.
b8 fude_zoom_instruments_selected(const fude_app* _app);

// --- page_circuit.c: electronics: wires, the circuit drawn while it plays, Value, aligning ---------------------------

// The part's pin nearest _screen within _reach at any depth shown: its object, its pin, where it is (screen).
b8 fude_zoom_page_pin_at(fude_zoom_page* _page, rde_vec_2F _screen, f64 _reach, u32* _object, u32* _pin, fude_zoom_v2* _at);
// The wire tool down at _screen: a wire begun on a pin or a wire (else none).
void fude_zoom_page_wire_down(fude_zoom_page* _page, rde_vec_2F _screen);
// The wire tool lifted: the wire drawn to the pin or wire under the pen (one step).
void fude_zoom_page_wire_up(fude_zoom_page* _page);
// The Circuit tool: Wire, Play (what the lasso holds), Parts.
b8 fude_zoom_circuit_choose(fude_app* _app, u32 _index);
// The Circuit tool: is choice _index in use (Wire, or Play on a circuit)?
b8 fude_zoom_circuit_chosen(const fude_app* _app, u32 _index);
// The Circuit tool: is the wire tool in hand, or a circuit playing?
b8 fude_zoom_circuit_selected(const fude_app* _app);
// The circuit's part an object is (FUDE_ZOOM_NONE: none).
u32 fude_zoom_page_circuit_part(const fude_zoom_page* _page, u32 _object);
// Running, a press on a part worked by hand (a switch, a button, a pot, a pin) works it. True: it did.
b8 fude_zoom_page_circuit_press(fude_zoom_page* _page, rde_vec_2F _screen);
// A value and its unit with a metric prefix, as a meter says it ("4.98 V", "12.3 mA", "250 µA").
void fude_zoom_page_si(f64 _v, const c8* _unit, c8* _out, usize _size);
// The circuit over its schematic: wires' joins, and while it plays its parts' states, meters and tags.
void fude_zoom_page_render_circuit(fude_zoom_page* _page, fude_zoom_v2 _half);
// The lone thing the lasso holds, if it has a value Value asks for (FUDE_ZOOM_NONE: not).
u32 fude_zoom_page_value_lone(const fude_zoom_page* _page);
// An LED's colour chosen.
void fude_zoom_page_value_led(any _self, u32 _index);
// A logic probe's colour lit chosen: its light (props.h) made again — its text, its name, as it was.
void fude_zoom_page_value_probe(any _self, u32 _index);
// The lasso's row: Value — the card, its number and unit as the part's text says them now.
void fude_zoom_page_value_selection(fude_app* _app, any _self, u32 _arg);
// The drag just made (the selection's): its wires followed; a move lined up.
void fude_zoom_page_align(fude_zoom_page* _page);
// The lines it lines up along, across the screen, a ring where; the wires its dragged parts keep, routed to them.
void fude_zoom_page_render_align(fude_zoom_page* _page, fude_zoom_v2 _half);

// --- page_mech.c: Play, a hand on a mechanism, the speaker, mechanisms drawn, bodies, limits -------------------------

// Playing, a hand put on the mechanism: a crank turns with it, else the part under it follows it.
b8 fude_zoom_page_mech_hold(fude_zoom_page* _page, rde_vec_2F _screen);
// The hand on the mechanism moved: the part held follows it, the crank turns as far as it went round.
void fude_zoom_page_mech_hold_moved(fude_zoom_page* _page, rde_vec_2F _screen);
// The hand on the mechanism let go: the part and the crank free.
void fude_zoom_page_mech_release(fude_zoom_page* _page);
// An oscilloscope's knob held, the hand moved: turned a step for each step's length gone.
void fude_zoom_page_knob_moved(fude_zoom_page* _page, rde_vec_2F _screen);
// A part worked by hand let go (after it has been seen pressed), and any knob held.
void fude_zoom_page_circuit_release(fude_zoom_page* _page);
// What the lasso holds that plays (fude_zoom_page_play_kind's bits together; drawings made bodies a mechanism's), kept
// until what is held, the scene or the custom parts known change (play_what_key, play_what_parts).
u8 fude_zoom_page_play_what(fude_zoom_page* _page);
// The speaker's sound stream closed.
void fude_zoom_page_sound_off(fude_zoom_page* _page);
// Play stopped: what plays let go, the drawing as it was, the panel gone.
void fude_zoom_page_play_stop(fude_zoom_page* _page);
// Play pressed on a tool (the Circuit's, Motion's): what the lasso holds; nothing held — what plays paused or on.
b8 fude_zoom_page_play_selection_now(fude_zoom_page* _page);
// The lasso's row: Play.
void fude_zoom_page_play_selection(fude_app* _app, any _self, u32 _arg);
// Objects made again for others (a value, a move's wires): played if those were.
void fude_zoom_page_play_remade(fude_zoom_page* _page, const u32* _died, const u32* _born, u32 _n);
// Play's step for this frame (a tenth of a second at most); a circuit edited since built again.
void fude_zoom_page_play_step(fude_zoom_page* _page);
// Where the panel and its parts are (screen, centre origin, Y up): at the bottom, in the middle.
void fude_zoom_page_play_lay(const fude_zoom_page* _page, fude_zoom_page_play_layout* _l);
// Slider _i set from where the hand is on it.
void fude_zoom_page_play_slide(fude_zoom_page* _page, u32 _i, rde_vec_2F _at);
// A hand down on Play's panel: its button or slider taken. True: it was on the panel.
b8 fude_zoom_page_play_down(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand);
// The hand on Play's panel moved: a slider held follows it. True: it was the panel's.
b8 fude_zoom_page_play_moved(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand);
// The hand on Play's panel lifted: a button pressed, a slider let go. True: it was the panel's.
b8 fude_zoom_page_play_up(fude_zoom_page* _page, u64 _hand);
// The panel: its buttons (each its sign and its word), its time, its sliders.
void fude_zoom_page_render_play(fude_zoom_page* _page);
// The mechanism's tracers: the path each has drawn, in the accent's colour, its pen where it is.
void fude_zoom_page_render_trails(fude_zoom_page* _page, fude_zoom_v2 _half);
// While playing, how far each part that slides is moved now (one value an object); NULL: none.
const rde_arr* fude_zoom_page_moving_parts(fude_zoom_page* _page);
// The mechanism drawn as it plays: its parts where they are now, springs, belts, dampers.
void fude_zoom_page_render_mech(fude_zoom_page* _page, fude_zoom_v2 _half);
// The drawings the lasso holds that can be bodies (an outline of their own, not a part), into _out.
u32 fude_zoom_page_body_targets(const fude_zoom_page* _page, u32* _out, u32 _max);
// The lasso's row: Make body — the card, as the first drawing is now.
void fude_zoom_page_body_selection(fude_app* _app, any _self, u32 _arg);
// The part lassoed alone, when it has limits (FUDE_ZOOM_NONE: none).
u32 fude_zoom_page_limits_lone(const fude_zoom_page* _page);
// The lasso's row: Limits — the card, as the part's are now.
void fude_zoom_page_limits_selection(fude_app* _app, any _self, u32 _arg);
// In Mechanisms (not playing): each body's mark at its drawing's middle — a ⊕ moving, the ground's hatch fixed.
void fude_zoom_page_render_bodies(fude_zoom_page* _page);

// --- page_topics.c: the bar's tools and topics -----------------------------------------------------------------------

// The Motion tool (the Mechanisms topic): Play (what the lasso holds), Parts.
b8 fude_zoom_motion_choose(fude_app* _app, u32 _index);
// The Pages tool: choice _i taken (a page size, Add a page, or the endless canvas again).
b8 fude_zoom_pages_choose(fude_app* _app, u32 _i);
// Topic: one chosen — the bar filled for it the frame after (never from inside its own panel's press).
b8 fude_zoom_topic_choose(fude_app* _app, u32 _at);
// The bar's tools filled for topic _topic: Topic first, then its own; the rest left out.
void fude_zoom_topic_fill(u8 _topic);

// --- page_text.c: texts and sticky notes, writing on the page, typed text written by hand ----------------------------

// The text tool at _screen: the text there opened to change, or a new one begun there.
void fude_zoom_page_text_at(fude_zoom_page* _page, rde_vec_2F _screen);
// The note under _screen with marks in it, and where _screen is on it. False: none there.
b8 fude_zoom_page_rich_under(fude_zoom_page* _page, rde_vec_2F _screen, u32* _object, const fude_zoom_rich** _rich, f32* _x, f32* _y, f64* _k,
                            c8* _words, usize _size);
// A tap at _screen on a note's task (ticked or unticked) or link (opened, unless _tasks). True: one.
b8 fude_zoom_page_rich_tap(fude_zoom_page* _page, rde_vec_2F _screen, b8 _tasks);
// Is there a note's link under _screen (a text's, a diagram shape's label's)?
b8 fude_zoom_page_link_at(fude_zoom_page* _page, rde_vec_2F _screen);
// Text _object (a text, a sticky note, a formula) on the text card to change — as the text tool's tap on it opens it.
void fude_zoom_page_text_edit(fude_zoom_page* _page, u32 _object);
// The one text the lasso holds (a text, a note, a formula): FUDE_ZOOM_NONE when none, or more than it.
u32 fude_zoom_page_lone_text(const fude_zoom_page* _page);
// The lasso's row's Edit text: the one text it holds on the card.
void fude_zoom_page_text_edit_selection(fude_app* _app, any _self, u32 _arg);
// What the card (or the page, written on it) says, saved: as its style asks (a text made or changed, one undo step).
void fude_zoom_page_text_save(fude_zoom_page* _page);
// The text card's Save: what it says saved.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_text_save(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// A tap off the text card: as Cancel (not the lift of the press that opened it).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_text_dismiss(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// The text card's Cancel: put away, nothing changed.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_text_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// A note's mark put in by its button (rich.h): the field's words made again with it, its selection kept on them.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_text_format(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// The material chips (naming a board): each in its wood, the one chosen ringed in the accent.
void fude_zoom_page_style_material_chips(fude_zoom_page* _page);
// A material chip tapped: the board's material chosen.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_material(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// The text card shown (laid out, the keyboard up) or put away; a text or a note is written on the page.
void fude_zoom_page_text_card(fude_zoom_page* _page, b8 _show);
// Done (a tap off it, Back, another tool): its words saved, as the card's Save.
void fude_zoom_page_inline_end(fude_zoom_page* _page);
// A press while a text is written on the page: in its words, the caret there; off it, it is saved.
b8 fude_zoom_page_inline_down(fude_zoom_page* _page, rde_vec_2F _screen);
// A drag in a text written on the page: its words selected so far. True: it was its.
b8 fude_zoom_page_inline_moved(fude_zoom_page* _page, rde_vec_2F _screen);
// The press in a text written on the page lifted. True: it was its.
b8 fude_zoom_page_inline_up(fude_zoom_page* _page);
// A text written on the page: its words, caret and selection drawn, its bar placed over it.
void fude_zoom_page_render_inline(fude_zoom_page* _page);
// The bar of a text written on the page: cut, copy, paste and Done.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_inline_tool(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// The typed texts selected (sticky notes are left as they are).
u32 fude_zoom_page_lassoed_texts(const fude_zoom_page* _page);
// The formulas selected (texts of LaTeX: formula.h).
u32 fude_zoom_page_lassoed_formulas(const fude_zoom_page* _page);
// The selection's Graph: its formulas drawn on a graph beside them, the lasso on it; one step.
void fude_zoom_page_graph_formulas(fude_zoom_page* _page);
// The selection's formulas written out by hand (handmath.h), in place of them.
void fude_zoom_page_formula_hand_selection(fude_app* _app, any _self, u32 _arg);
// The selection's handwriting read as a formula (mathread.h), a formula in its place.
void fude_zoom_page_formula_read_selection(fude_app* _app, any _self, u32 _arg);
// Are formulas offered (the lasso's row's Formula, Handwriting, Graph: only in an app with them)?
b8 fude_zoom_formula_present(void);
// The selection's Handwriting: each typed text written out as pen strokes where it was.
void fude_zoom_page_handwrite_selection(fude_zoom_page* _page);
// The numpad's OK on Offset: each selected thing's outline offset by the length typed, beside it.
void fude_zoom_page_offset_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _mm);
// The board the lasso holds alone (FUDE_ZOOM_NONE: none, or more than it).
u32 fude_zoom_page_lone_board(const fude_zoom_page* _page);
// The lasso's row: Offset (its length asked for); a board alone has its grain turned instead.
void fude_zoom_page_offset_selection(fude_app* _app, any _self, u32 _arg);
// Appends to _out (an rde_arr of u32) the strokes object _object is or holds: itself when it is a stroke, every stroke
// inside it, at any depth down to 64, when it is a frame (_depth: how deep this call is; 0 to begin). Dead strokes and
// those on a hidden or locked layer are left out. What To curve and Formula take from what the lasso holds.
void fude_zoom_page_take_strokes(const fude_zoom_scene* _s, u32 _object, rde_arr* _out, u32 _depth);

// --- page_find.c: Find, Find through every canvas, handwriting read as text ------------------------------------------

// Find _query: the next thing holding it framed (and the other canvases looked through too).
void fude_zoom_page_find(fude_zoom_page* _page, const c8* _query);
// The path <canvas>.<_ext> beside canvas _canvas's own file, into _out.
void fude_zoom_page_side_path(u32 _canvas, const c8* _ext, c8* _out, usize _size);
// The open canvas's words kept beside it (as it is left), with how big its file is.
void fude_zoom_page_words_keep(fude_zoom_page* _page);
// A few more canvases looked through (as long as a frame allows), those holding the words kept, most matches first.
void fude_zoom_page_everywhere_step(fude_zoom_page* _page);
// The bar's Find (a UI press): opens the Find card on the text card, the words last looked for in it, selected so
// typing replaces them, and starts reading the handwriting meanwhile; the card's own Find looks for them.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_find(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// Is handwriting reading (ML Kit) on this device, and switched on?
b8 fude_zoom_read_present(void);
// The canvas let go: its lines and what they say forgotten, and a line being read and a To text under way marked
// stale, their answers dropped as they come (the reader still has them to give: one at a time).
void fude_zoom_page_hand_forget(fude_zoom_page* _page);
// Once a frame: handwriting for Find read a line at a time, again when the canvas changed.
void fude_zoom_page_hand_update(fude_zoom_page* _page);
// To text: the selected strokes handed to the reader; the answer comes in a later frame.
void fude_zoom_page_read_selection(fude_app* _app, any _self, u32 _arg);
// To text's answer in: written where the handwriting was, as a text of its height; one step. Dropped when its canvas
// was left meanwhile, or a stroke it read was rubbed out or undone.
void fude_zoom_page_read_update(fude_zoom_page* _page);

// --- page_diagrams.c: a diagram from text, the diagram library, building plans, Kanban boards ------------------------

// The card for a diagram's text (Mermaid), empty but for a hint.
void fude_zoom_page_mermaid_card(fude_zoom_page* _page);
// The diagram library's tiles for each family made (page.h's library_kinds and library_first), once, at init; freed
// by fude_zoom_page_destroy.
void fude_zoom_page_library_make(fude_zoom_page* _page);
// A text's box on the screen at _px: as wide as its longest line, as tall as its lines.
void fude_zoom_page_measure_words(const fude_zoom_page* _page, const c8* _words, f32 _px, f32* _w, f32* _h);
// Mermaid's flowchart _text made the canvas's own shapes and arrows, laid out in view. False: none.
b8 fude_zoom_page_import_mermaid(fude_zoom_page* _page, const c8* _text);
// What was joined to things an edit replaced (_died[i] by _born[i]) joined to the replacements.
void fude_zoom_page_rebind(fude_zoom_page* _page, const u32* _died, const u32* _born, u32 _n, rde_arr* _died_out, rde_arr* _born_out);
// A room labelled where it is tapped: the area its walls close, its name and area in its middle.
void fude_zoom_page_room_at(fude_zoom_page* _page, rde_vec_2F _screen);
// The Plan tool: is choice _index in use (a wall's thickness, a room)?
b8 fude_zoom_plan_chosen(const fude_app* _app, u32 _index);
// The Plan tool: is a wall or a room in hand?
b8 fude_zoom_plan_selected(const fude_app* _app);
// The parts list (Export): every part on the canvas counted by kind and value, as CSV into _out.
b8 fude_zoom_page_parts_list(const fude_zoom_page* _page, fude_bytes* _out);
// The panel opened with the tabs _tabs (its tab the first of them, unless it is on one already).
void fude_zoom_page_library_open_tabs(fude_zoom_page* _page, u32 _tabs);
// A panel across _x0 to _x1, from _y0 to _y1 (screen points), moved clear of the toolbar.
void fude_zoom_page_clear_of_bar(const fude_zoom_page* _page, f64* _x0, f64* _x1, f64 _y0, f64 _y1, f64 _least);
// Where the diagram library's panel and its parts are (screen, centre origin, Y up).
void fude_zoom_page_library_lay(fude_zoom_page* _page, fude_zoom_page_library_layout* _l);
// Is point _p inside box _b?
b8 fude_zoom_page_box_holds(fude_zoom_box _b, rde_vec_2F _p);
// The tile under _p (its symbol's kind; FUDE_ZOOM_NONE: none), its box into *_box (may be NULL).
u32 fude_zoom_page_library_tile(fude_zoom_page* _page, const fude_zoom_page_library_layout* _l, rde_vec_2F _p, fude_zoom_box* _box);
// The panel, over the page (under the toolbar): its tabs, a tile a symbol of the family shown, its ×.
void fude_zoom_page_render_library(fude_zoom_page* _page);
// A hand down: on the panel, its own (a tap or a drag through the grid); off it, the panel goes.
b8 fude_zoom_page_library_down(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand);
// The hand on the library's panel moved: its grid scrolled. True: it was the panel's.
b8 fude_zoom_page_library_moved(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand);
// The hand on the library's panel lifted: a tile tapped put down, a tab chosen. True: the panel's.
b8 fude_zoom_page_library_up(fude_zoom_page* _page, u64 _hand);
// A template's columns (kanbanform.h), a line each.
void fude_zoom_page_kanban_preset(u32 _which, c8* _out, usize _size);
// Insert's Kanban board: its columns asked for on their card (kanbanform.h: a row each, the templates over them).
void fude_zoom_page_kanban_card(fude_zoom_page* _page);
// Kanban's card done: a column for each line, side by side in the middle of the view; one step.
void fude_zoom_page_kanban_commit(fude_zoom_page* _page, const c8* _text);
// Sticky notes let go in a Kanban column lined up under its header, in their order down it.
void fude_zoom_page_kanban_tidy(fude_zoom_page* _page);

// --- page_measure.c: measuring, snapping, guides, the readout, exact lengths, sheets, the size card, Repeat ----------

// Where _screen snaps to: an end, a corner, a middle, a centre drawn near it (what was found is kept, to be shown).
rde_vec_2F fude_zoom_page_snapped(fude_zoom_page* _page, rde_vec_2F _screen);
// Where _screen snaps to within _radius (also square to lines, and a sheet's grid).
rde_vec_2F fude_zoom_page_snapped_within(fude_zoom_page* _page, rde_vec_2F _screen, f64 _radius);
// A frame's units to the screen, now.
fude_zoom_sim fude_zoom_page_frame_sim(const fude_zoom_page* _page, u32 _frame);
// The measuring tape down at _screen (snapped): it starts there.
void fude_zoom_page_measure_down(fude_zoom_page* _page, rde_vec_2F _screen);
// The measuring tape drawn to _screen (snapped).
void fude_zoom_page_measure_moved(fude_zoom_page* _page, rde_vec_2F _screen);
// The measuring tape lifted: the tape stays showing, or a dimension is drawn (one step).
void fude_zoom_page_measure_up(fude_zoom_page* _page);
// A small tag of one or two lines beside the pen (_at, screen): below it and to its right, kept on the screen.
void fude_zoom_page_draw_tag(fude_zoom_page* _page, rde_vec_2F _at, const c8* _first, const c8* _second, fude_zoom_v2 _half);
// A small tag beside the pen as fude_zoom_page_draw_tag draws it, its letters _px high.
void fude_zoom_page_draw_tag_px(fude_zoom_page* _page, rde_vec_2F _at, const c8* _first, const c8* _second, fude_zoom_v2 _half, f32 _px);
// The stuck ruler under _screen (its index in the renderer's), or -1.
i32 fude_zoom_page_stuck_at(const fude_zoom_page* _page, rde_vec_2F _screen);
// A press on a stuck ruler: a guide pulled out of it. True: it was on one.
b8 fude_zoom_page_guide_pull_down(fude_zoom_page* _page, rde_vec_2F _screen);
// The guide pulled out let go: put down on its sheet (let go on a ruler: none).
void fude_zoom_page_guide_pull_up(fude_zoom_page* _page);
// The guide being pulled: across the screen, its distance from the sheet's corner beside the pen.
void fude_zoom_page_render_guide_pull(fude_zoom_page* _page, fude_zoom_v2 _half);
// The pen's readout: where it is on the sheet, how long and at what angle what it draws is.
void fude_zoom_page_render_readout(fude_zoom_page* _page, fude_zoom_v2 _half);
// A LINE's or a DIMENSION's length in millimetres (its numbers in its frame's units, its scale, the canvas's).
f64 fude_zoom_page_line_mm(const fude_zoom_page* _page, u32 _object, f64* _dx, f64* _dy);
// The numpad's OK on a line's or a dimension's chip: made exactly that long, its ends' joins moved.
void fude_zoom_page_line_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _mm);
// A sheet's true size (mm) and its numbers as read.
b8 fude_zoom_page_sheet_of(const fude_zoom_page* _page, u32 _object, f64* _w_mm, f64* _h_mm, fude_zoom_sheet* _sheet);
// What a sheet's chip says: its size and scale ("420 × 297 mm · 1:50").
void fude_zoom_page_sheet_said(const fude_zoom_page* _page, f64 _w_mm, f64 _h_mm, f64 _scale, c8* _out, usize _size);
// Insert's Measured sheet (FUDE_ZOOM_NONE), or that sheet changed: its card.
void fude_zoom_page_sheet_start(fude_zoom_page* _page, u32 _sheet);
// Export's Sheets to scale: a PDF page for each sheet, the sheet's size at its scale, into _out.
b8 fude_zoom_page_sheets_pdf(fude_zoom_page* _page, fude_bytes* _out);
// The measured sheet under screen point _p (not _not), with its units to the screen and its numbers.
u32 fude_zoom_page_sheet_under(const fude_zoom_page* _page, fude_zoom_v2 _p, u32 _not, fude_zoom_sim* _all, fude_zoom_sheet* _sheet, f64* _mm);
// The size chip tapped: the size card, with what the lasso holds as it is now.
void fude_zoom_page_size_start(fude_zoom_page* _page);
// The lasso's Repeat: its card (a row of three to begin with).
void fude_zoom_page_repeat_start(fude_zoom_page* _page, u8 _mode);
// The selection's row: Repeat.
void fude_zoom_page_repeat_selection(fude_app* _app, any _self, u32 _arg);
// The size of what the lasso holds as its chip says it ("120 × 45 mm"), into _out.
b8 fude_zoom_page_size_said(fude_zoom_page* _page, c8* _out, usize _size);

// --- page_input.c: a press, a move, a lift; fingers, a flick, events -------------------------------------------------

// A press at _screen (the pen's, the mouse's, a finger drawing) handed to the tool in hand.
void fude_zoom_page_down(fude_zoom_page* _page, rde_vec_2F _screen, b8 _from_pen, b8 _eraser_end);
// The press moved to _screen, handed to what took it.
void fude_zoom_page_moved(fude_zoom_page* _page, rde_vec_2F _screen);
// The press lifted, handed to what took it.
void fude_zoom_page_up(fude_zoom_page* _page);
// How many fingers are on the screen.
u32 fude_zoom_page_finger_count(const fude_zoom_page* _page);
// A pan of _moved (screen units) just made by hand: how fast the view goes, for a flick.
void fude_zoom_page_pan_speed(fude_zoom_page* _page, fude_zoom_v2 _moved);
// Once a frame: a flick carrying on, slowing; anything else taking the view ends it.
void fude_zoom_page_coast(fude_zoom_page* _page);

// --- page_nav.c: getting around: flying, Back, the marks, the navigation widgets -------------------------------------

// The camera flown to _to; _remember: where it was goes on Back's list.
void fude_zoom_page_fly(fude_zoom_page* _page, fude_zoom_camera _to, b8 _remember);
// Back: to the view the last flight left.
void fude_zoom_page_go_back(fude_zoom_page* _page);
// A press on one of the marks (an empty screen's arrows and rings): flown to what it points at.
b8 fude_zoom_page_mark_press(fude_zoom_page* _page, rde_vec_2F _screen);
// The home button: the app's home screen (its canvases).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_home(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// The Back button: back to the view the last flight left.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_back(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// The depth tapped: its levels shown over it (or put away).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_depth(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// A crumb of the depth tapped: flown to that level.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_crumb(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// The map's button: its panel up or put away (a second press at once is the same tap: a tablet's comes twice).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_map(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// The Places button: its rows shown or put away.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_places(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// A place's ×: it let go, the list as it is now.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_place_drop(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// A row of Places: row 0 marks the view (or unmarks the place it is at); the others fly to theirs.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_place(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);

// --- page_render.c: drawing the page, the marks' previews ------------------------------------------------------------

// The page's own papers' lines (paper.h), made into _page->paper_lines (cleared first).
rde_arr* fude_zoom_page_paper_lines(fude_zoom_page* _page);
// The paper's lines _lines drawn onto the screen through _to, in ink _ink as strong as each says.
void fude_zoom_page_draw_paper_lines(const rde_arr* _lines, fude_zoom_sim _to, rde_color _ink);
// Where arrow _m's preview goes: inside it, toward the middle, all of it on the screen.
b8 fude_zoom_page_preview_at(const fude_zoom_page* _page, const fude_zoom_nav_mark* _m, fude_zoom_v2 _half, rde_vec_2F* _out);
// Once a frame, before the page is drawn: the next arrow's preview not drawn yet, drawn.
void fude_zoom_page_previews_draw(fude_zoom_page* _page, rde_window* _window, rde_camera* _camera);
// The pictures let go (the canvas closing; the app in the background).
void fude_zoom_page_previews_free(fude_zoom_page* _page);
// How many millimetres a screen point is now (the canvas's scale at the camera's zoom).
f64 fude_zoom_page_mm_per_point(const fude_zoom_page* _page);
// What fude_zoom_page_dots_along hands each point it finds (on the screen, centre origin, Y up), with its _user.
typedef void (*fude_zoom_page_dot_fn)(any _user, rde_vec_2F _at);
// The points along the line _p (_n points, its own units; _closed: on round from its last to its first) at lengths
// _start + i·_step for every whole i, those between lengths _from and _to along it and on the screen (_to_screen takes
// its units there; _half: half the screen), each handed to _dot, at most _most of them. What the current's dots and a
// belt's marks are drawn at, however far in the view is on them. Returns how many.
u32 fude_zoom_page_dots_along(const fude_zoom_v2* _p, u32 _n, b8 _closed, f64 _from, f64 _to, f64 _start, f64 _step, fude_zoom_sim _to_screen,
                              fude_zoom_v2 _half, u32 _most, fude_zoom_page_dot_fn _dot, any _user);

// --- page_settings.c: Settings' Measurements and Display, the offset pad ---------------------------------------------

// Calibrate: Settings closes, and the card's outline comes up on the page.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_calibrate(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// Calibration's arrows: the card drawn bigger or smaller, a big or a small step.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_calibrate_key(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// The bank card's outline at the size being tried, and what to do with it.
void fude_zoom_page_render_calibration(fude_zoom_page* _page);
// The true size: the view where a millimetre drawn is a millimetre on the glass, round the same middle.
fude_zoom_camera fude_zoom_page_true_view(const fude_zoom_page* _page);
// The printer's check: the first length measured on the printed page asked for.
void fude_zoom_page_print_check_ask(fude_zoom_page* _page);
// Settings' kerf: the saw's kerf asked for on the numpad.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_kerf(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// The instrument the pad moves still out (not put away, its slot not another's since).
b8 fude_zoom_page_anchor_alive(const fude_zoom_page* _page);
// The instrument put back where its snapped point plus the pad's offset says.
void fude_zoom_page_anchor_apply(fude_zoom_page* _page);
// The offset pad's step: a few points on the screen at this zoom, in a round length.
f64 fude_zoom_page_anchor_step(const fude_zoom_page* _page);
// "X +12 mm" (_axis 0) or "Y −5 mm" (1).
void fude_zoom_page_anchor_said(const fude_zoom_page* _page, u32 _axis, c8* _out, usize _size);
// The offset pad's buttons: the instrument moved a step, the offset typed, or the pad put away.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_anchor(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// The pad's grip: a hand on it drags the pad (where it is left, it stays).
b8 fude_zoom_page_grip_down(fude_zoom_page* _page, rde_vec_2F _screen, u64 _hand);
// The pad's grip dragged to _screen by hand _hand. True: it was held by that hand.
b8 fude_zoom_page_grip_moved(fude_zoom_page* _page, rde_vec_2F _screen, u64 _hand);
// The pad's grip let go by hand _hand. True: it was held by that hand.
b8 fude_zoom_page_grip_up(fude_zoom_page* _page, u64 _hand);
// The kerf as Settings says it, into _out.
void fude_zoom_page_kerf_said(const fude_zoom_page* _page, c8* _out, usize _size);
// The frame rate, at the top right in the theme's text (just its number): frames counted over half a second at a time.
void fude_zoom_page_render_fps(fude_zoom_page* _page);

// --- page_present.c: the map, areas, Present -------------------------------------------------------------------------

// The map's panel opened: what is drawn measured, its picture to be drawn.
void fude_zoom_page_map_open(fude_zoom_page* _page);
// What the map shows, again when it has to (what is drawn, the view's place, its shape, the theme).
void fude_zoom_page_map_refresh(fude_zoom_page* _page);
// Off screen, once wanted: the canvas from above drawn into the map's picture.
void fude_zoom_page_map_draw(fude_zoom_page* _page, rde_window* _window, rde_camera* _camera);
// The map's panel over the page: its picture, the areas, the places' pins, the view.
void fude_zoom_page_render_map(fude_zoom_page* _page);
// A hand down: on the panel, its own (a tap, or a drag of the view); off it, the page's (the map stays).
b8 fude_zoom_page_map_down(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand);
// The hand on the map moved: the view dragged with it.
b8 fude_zoom_page_map_moved(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand);
// The hand on the map lifted: a tap on its ×, Present, a pin, an area's name, or the map (flown there).
b8 fude_zoom_page_map_up(fude_zoom_page* _page, u64 _hand);
// The map's picture let go (made again when it is next wanted).
void fude_zoom_page_map_free(fude_zoom_page* _page);
// Insert's Named area: its name asked for ("Area 3" to begin with, chosen: typing replaces it).
void fude_zoom_page_area_card(fude_zoom_page* _page);
// Insert's Named area made, named _text: round what the lasso holds, else the view; one step.
void fude_zoom_page_area_commit(fude_zoom_page* _page, const c8* _text);
// Step _i framed: an area's box filling most of the screen, a place as it was marked — flown there, slower than Back.
void fude_zoom_page_present_go(fude_zoom_page* _page, u32 _i);
// Present: the areas in turn (none: the places), nothing but the page on the screen.
void fude_zoom_page_present_start(fude_zoom_page* _page);
// Presenting stopped: the app's chrome back.
void fude_zoom_page_present_end(fude_zoom_page* _page);
// A tap while presenting: the pill's buttons (while it shows), else the right part of the screen on, the left back.
void fude_zoom_page_present_tap(fude_zoom_page* _page, rde_vec_2F _at);
// The pen (or the mouse) while presenting: a tap is a tap; a drag, the laser's trail.
void fude_zoom_page_present_pen_down(fude_zoom_page* _page, rde_vec_2F _at);
// The pen moved while presenting: the laser's trail.
void fude_zoom_page_present_pen_moved(fude_zoom_page* _page, rde_vec_2F _at);
// The pen lifted while presenting: a tap goes on or back.
void fude_zoom_page_present_pen_up(fude_zoom_page* _page);
// Over the page while presenting: the laser's trail (fading), the pill (a while after a touch).
void fude_zoom_page_render_present(fude_zoom_page* _page);
// The keys while presenting: on (right, down, space, Page Down), back (left, up, Page Up).
void fude_zoom_page_present_keys(fude_zoom_page* _page, rde_window* _window);

// --- page_strip.c: a notebook's pages strip, the canvases Find found -------------------------------------------------

// Where the pages strip and its parts are (screen, centre origin, Y up).
void fude_zoom_page_strip_lay(const fude_zoom_page* _page, fude_zoom_page_strip_layout* _l);
// The page the view is on (its middle's).
u32 fude_zoom_page_page_now(const fude_zoom_page* _page);
// Opened: scrolled to have the page you are on in its middle.
void fude_zoom_page_strip_open(fude_zoom_page* _page);
// The pages' chip: the strip up or put away (a second press at once is the same tap: a tablet's comes twice).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_pages(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// A picture wanted and not drawn (or drawn before a change): drawn, off screen — one a frame, those shown first.
void fude_zoom_page_strip_draw(fude_zoom_page* _page, rde_window* _window, rde_camera* _camera);
// The strip's pages' pictures let go.
void fude_zoom_page_strip_free(fude_zoom_page* _page);
// Page _i shown at place _slot (its own, or another while one is dragged past it): its picture's box, as scrolled.
fude_zoom_box fude_zoom_page_strip_slot(const fude_zoom_page* _page, const fude_zoom_page_strip_layout* _l, f64 _slot);
// The pages strip over the page: each page's picture and number, its chips, its ×.
void fude_zoom_page_render_strip(fude_zoom_page* _page);
// A hand down on the pages strip: a page, a chip or the strip taken. True: it was on the strip.
b8 fude_zoom_page_strip_down(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand);
// Dragged along: scrolled with the hand — or, a page held, that page carried.
b8 fude_zoom_page_strip_moved(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand);
// The hand on the strip lifted: a page carried put down, or a tap on a chip, the × or a page.
b8 fude_zoom_page_strip_up(fude_zoom_page* _page, u64 _hand);
// The panel of canvases Find found the words in, over the page (looking through a few more).
void fude_zoom_page_render_everywhere(fude_zoom_page* _page);
// A hand down on the found canvases' panel. True: it was on the panel.
b8 fude_zoom_page_everywhere_down(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand);
// The hand on the found canvases' panel moved: its rows scrolled. True: it was the panel's.
b8 fude_zoom_page_everywhere_moved(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand);
// Let go: a tap — its ×, or a canvas (opened, the words found in it once it is up).
b8 fude_zoom_page_everywhere_up(fude_zoom_page* _page, u64 _hand);

// --- page_audio.c: audio synced to ink and its panel -----------------------------------------------------------------

// Is it offered (product.h: a notes app's, not InfiniWorkshop's)?
b8 fude_zoom_page_audio_offered(void);
// The path of canvas _canvas's recording _number (beside the canvas), into _out.
void fude_zoom_page_audio_rec_path(u32 _canvas, u32 _number, c8* _out, usize _size);
// The canvas's list of recordings written beside it (none left: the file deleted).
void fude_zoom_page_audio_keep(fude_zoom_page* _page);
// "m:ss" (or "h:mm:ss") for _ms milliseconds.
void fude_zoom_page_audio_time(u64 _ms, c8* _out, usize _size);
// A canvas opened: its recordings read (one cut short by the app stopping as long as what reached its file).
void fude_zoom_page_audio_open(fude_zoom_page* _page);
// A canvas left: what was being recorded kept, what was playing stopped.
void fude_zoom_page_audio_close(fude_zoom_page* _page);
// The microphone's chip: a recording started (once the microphone may be used), or the one running stopped.
void fude_zoom_page_audio_toggle(fude_zoom_page* _page);
// The recording playing stopped, its faint ink let go.
void fude_zoom_page_audio_stop_playing(fude_zoom_page* _page);
// Recording _number in the player (at its start), playing if _play.
b8 fude_zoom_page_audio_load(fude_zoom_page* _page, u32 _number, b8 _play);
// With the panel up, is there ink a recording linked under _screen?
b8 fude_zoom_page_audio_linked_at(fude_zoom_page* _page, rde_vec_2F _screen);
// With the panel up, a tap on linked ink: its recording played from a moment before it was written.
b8 fude_zoom_page_audio_tap(fude_zoom_page* _page, rde_vec_2F _screen);
// Once a frame: the microphone read, what was made linked, the player kept fed, the faint ink worked out.
void fude_zoom_page_audio_update(fude_zoom_page* _page);
// The recordings' panel over the page: each recording's row, the player, its chips.
void fude_zoom_page_render_audio(fude_zoom_page* _page);
// A hand down on the recordings' panel. True: it was on the panel.
b8 fude_zoom_page_audio_down(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand);
// The hand on the recordings' panel moved: the player scrubbed, or the rows scrolled.
b8 fude_zoom_page_audio_moved(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand);
// The hand on the recordings' panel lifted: a tap on a row, a chip or the player done.
b8 fude_zoom_page_audio_up(fude_zoom_page* _page, u64 _hand);
// The microphone button: a recording started or stopped.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_mic(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// The recordings button: their panel shown or put away.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_listen(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);

// --- page_speech.c: speech to text -----------------------------------------------------------------------------------

// The path of the transcript of canvas _canvas's recording _number, into _out.
void fude_zoom_page_heard_path(u32 _canvas, u32 _number, c8* _out, usize _size);
// Recording _number's transcript (read from its file the first time); NULL when there is none and _make is false.
fude_zoom_page_heard* fude_zoom_page_heard_of(fude_zoom_page* _page, u32 _number, b8 _make);
// Every transcript let go (another canvas).
void fude_zoom_page_heard_forget(fude_zoom_page* _page);
// Live text's setting written (_save) or read.
void fude_zoom_page_listen_setting(fude_zoom_page* _page, b8 _save);
// The model downloaded (asked first, by the press that wanted it).
void fude_zoom_page_listen_download(fude_zoom_page* _page);
// Once a frame: the model's download followed, a piece read taken in, the next piece given to it.
void fude_zoom_page_listen_update(fude_zoom_page* _page);
// As text: recording _number's transcript as one text in view, in the pen's colour, linked to when it began.
void fude_zoom_page_listen_as_text(fude_zoom_page* _page, u32 _number);
// As handwriting: recording _number's transcript written out in view, each line linked to its time.
void fude_zoom_page_listen_as_hand(fude_zoom_page* _page, u32 _number);
// The live caption: while Live text reads the recording being made, its last words over the bottom row a while.
void fude_zoom_page_render_caption(fude_zoom_page* _page);

// --- page_pieces.c: my pieces, stencils, a symbol's text -------------------------------------------------------------

// The folder they are kept in (beside the canvases), created if missing.
const c8* fude_zoom_page_pieces_dir(void);
// My pieces, read from their folder the first time.
fude_zoom_pieces* fude_zoom_page_pieces(fude_zoom_page* _page);
// The pictures' lines, made again (the list changed).
void fude_zoom_page_pieces_stale(fude_zoom_page* _page);
// The lasso's row: Keep (its name asked for, then kept in My pieces).
void fude_zoom_page_keep_selection(fude_app* _app, any _self, u32 _arg);
// What selection _sel holds as a piece's things at their true size: copies into _clips, their box.
b8 fude_zoom_page_held_clips(fude_zoom_page* _page, const fude_zoom_selection* _sel, rde_arr* _clips, fude_zoom_box* _box);
// Keep's name _text given: what the lasso holds kept at its true size in My pieces.
void fude_zoom_page_piece_commit(fude_zoom_page* _page, const c8* _text);
// A piece put down: in the middle of the view at its true size, held by the lasso (to move it); one undo step.
void fude_zoom_page_put_piece(fude_zoom_page* _page, const fude_zoom_piece* _piece);
// Piece _i put back (put_piece's).
void fude_zoom_page_piece_put(fude_zoom_page* _page, u32 _i);
// A piece's longest lines made a stencil's (_st), its plate's sizes with them. False: no line.
b8 fude_zoom_page_stencil_fill(const fude_zoom_piece* _piece, fude_zoom_stencil* _st, f64* _side);
// Stencil _st's lines and plate set for the instrument in _slot.
void fude_zoom_page_stencil_set(fude_zoom_page* _page, u32 _slot, const fude_zoom_stencil* _st);
// Piece _i laid as a stencil in the middle of the view, at its true size.
void fude_zoom_page_piece_stencil(fude_zoom_page* _page, u32 _i);
// Piece _i's picture: its lines fitted into _box (made once, kept until the list changes).
void fude_zoom_page_piece_picture(fude_zoom_page* _page, u32 _i, fude_zoom_box _box, rde_color _ink);
// The panel, over the page: its name, its ×, a tile a piece — its picture, its name, its × (tapped twice).
void fude_zoom_page_render_pieces(fude_zoom_page* _page);
// A hand down: on the panel, its own (a tap, or a drag through the grid); off it, the panel goes.
b8 fude_zoom_page_pieces_down(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand);
// The hand on My pieces' panel moved: its grid scrolled. True: it was the panel's.
b8 fude_zoom_page_pieces_moved(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand);
// The hand on My pieces' panel lifted: a tile tapped put down or deleted. True: the panel's.
b8 fude_zoom_page_pieces_up(fude_zoom_page* _page, u64 _hand);
// Symbol _object's text on the text card, to change.
void fude_zoom_page_symbol_edit(fude_zoom_page* _page, u32 _object);
// A symbol of kind _kind put in the middle of the view, lassoed, its text asked for; one step.
void fude_zoom_page_symbol_put(fude_zoom_page* _page, u32 _kind);
// A symbol's new text _text saved: the symbol made again with it where it is; one undo step.
void fude_zoom_page_symbol_text_commit(fude_zoom_page* _page, const c8* _text);
// The symbol the lasso holds alone (FUDE_ZOOM_NONE: none, or more).
u32 fude_zoom_page_lone_symbol(const fude_zoom_page* _page);

// --- page_custom.c: examples, custom parts, the Custom tab and templates ---------------------------------------------

// The name an example (or an example's word) is drawn with, in the app's language (NULL: none).
const c8* fude_zoom_page_example_title(u32 _example);
// Example _e drawn on the canvas open now, the view out to all of it, the topic its own.
void fude_zoom_page_examples_draw(fude_zoom_page* _page, u32 _e);
// Which of them chosen: those, in order, each a canvas, in the folder of examples (made at the top, if not there).
void fude_zoom_page_examples_chosen(any _self, u32 _which);
// Insert → Examples: the card asking which of them (all, electronics', mechanisms', both).
void fude_zoom_page_examples_card(fude_zoom_page* _page);
// Once a frame: the one waiting drawn once its canvas is open, then the next made.
void fude_zoom_page_examples_follow(fude_zoom_page* _page);
// My parts learnt (once a folder: a person's other saves, theirs).
void fude_zoom_page_parts_load(void);
// Does the lasso hold a logic part (what a part is made of)?
b8 fude_zoom_page_lasso_logic(const fude_zoom_page* _page);
// The lasso's row: Make part.
void fude_zoom_page_part_selection(fude_app* _app, any _self, u32 _arg);
// A Custom tile's item as a kind and a key (FUDE_ZOOM_CUSTOM_KEY bytes).
b8 fude_zoom_page_custom_of(fude_zoom_page* _page, u32 _item, u8* _kind, c8* _key);
// The library's Custom tiles listed again (my parts, then my pieces). How many.
u32 fude_zoom_page_custom_items(fude_zoom_page* _page);
// A tile's ⋯ (its card): its top right corner.
fude_zoom_box fude_zoom_page_custom_more(fude_zoom_box _tile);
// A Custom tile: its picture (a piece's lines, a part's block), its name under it, its ⋯.
void fude_zoom_page_custom_tile(fude_zoom_page* _page, u32 _item, fude_zoom_box _tile);
// A Custom tile tapped: put down in the middle of the view (a piece as it was kept, a part's block), lassoed.
void fude_zoom_page_custom_tap(fude_zoom_page* _page, u32 _item);
// A Custom tile's card's choice _which: Edit or Inside, Rename, Delete.
void fude_zoom_page_custom_chosen(any _self, u32 _which);
// A Custom tile's card (its ⋯ tapped, or it held): its name, Edit or Inside, Rename, Delete.
void fude_zoom_page_custom_card(fude_zoom_page* _page, u32 _item);
// Once a frame: a hand held still on a Custom tile — its card (what that hand does next, the panel's no more).
void fude_zoom_page_library_hold(fude_zoom_page* _page);
// A Custom item's new name _text: the piece's or the part's, and its template canvas's.
void fude_zoom_page_custom_rename(fude_zoom_page* _page, const c8* _text);
// Custom item _key of kind _kind deleted: the piece, or the part, with its template canvas.
void fude_zoom_page_custom_delete(fude_zoom_page* _page, u8 _kind, const c8* _key);
// The canvas open now: a template (whose, from the map), or not — then Back is the app's again.
void fude_zoom_page_template_opened(fude_zoom_page* _page, b8 _say);
// Once a frame: the template being made filled once its canvas is open.
void fude_zoom_page_template_follow(fude_zoom_page* _page);
// A template kept as it is now (its canvas closing): as its piece, or its part made again.
void fude_zoom_page_template_keep(fude_zoom_page* _page);
// A template left (its canvas closing): kept, a canvas no more.
void fude_zoom_page_template_leave(fude_zoom_page* _page);
// The custom part the lasso holds alone (NULL: none).
const fude_sim_def* fude_zoom_page_lone_custom(const fude_zoom_page* _page);
// The lasso's row: Inside — a custom part's template (what it is made of: to see, to change).
void fude_zoom_page_inside_selection(fude_app* _app, any _self, u32 _arg);
// The page's menu: Inside — on a custom part (the one it opened on, or the one the lasso holds alone).
const fude_sim_def* fude_zoom_page_context_custom(const fude_zoom_page* _page);
// The page's menu: Inside (the custom part's template opened).
void fude_zoom_page_context_inside(fude_app* _app, any _self, u32 _arg);

// --- page_edit.c: fillet, chamfer, trim, Combine, constraints, the lasso's row, the page's menu ----------------------

// A key for what the lasso holds as the scene is now: every object held, their count and the scene's revision,
// hashed. What the lasso's row's faces work out each frame from the whole scene (Combine's, Play's) is kept under it
// and worked out again only when it changes.
u64 fude_zoom_page_held_key(const fude_zoom_page* _page);
// The corner of the two lassoed lines rounded _mm round, both cut back to the arc; one step.
void fude_zoom_page_fillet(fude_zoom_page* _page, f64 _mm);
// The corner the two lassoed lines make cut straight _mm back along each (0: carried on or cut back to meet).
void fude_zoom_page_chamfer(fude_zoom_page* _page, f64 _mm);
// Extend: the lone line's ends carried on to the first thing each meets (an end that meets nothing stays).
void fude_zoom_page_extend(fude_zoom_page* _page);
// Trim: the piece of the line under the pen between the crossings either side of it cut away (no crossings: the line).
void fude_zoom_page_trim_at(fude_zoom_page* _page, rde_vec_2F _screen);
// The closed shapes lassoed combined by _op (joined, cut out of the lowest, or their overlap); one step.
void fude_zoom_page_combine(fude_zoom_page* _page, u8 _op);
// The lasso's row: Combine (its card: Union, Subtract, Intersect).
void fude_zoom_page_combine_selection(fude_app* _app, any _self, u32 _arg);
// The constraints of what the lasso holds, marked: ∥, ⊥ or = by each of their lines' middles.
void fude_zoom_page_render_constraints(fude_zoom_page* _page, fude_zoom_v2 _half);
// The lasso's row's To curve button, which does what fits what is held (Fillet, Extend, Fit parts...).
void fude_zoom_page_curve_selection(fude_app* _app, any _self, u32 _arg);
// A PDF's own text inside box _box (home units), UTF-8 into _out. Its length (0: none there).
usize fude_zoom_page_pdf_text(fude_zoom_page* _page, fude_zoom_box _box, c8* _out, usize _size);
// The selection's row: Copy text (the PDF's under the loop, to the clipboard).
void fude_zoom_page_copy_pdf_text(fude_zoom_page* _page);
// The page's menu opened at _screen, for what is under it (or what the lasso holds).
void fude_zoom_page_context_open(fude_zoom_page* _page, rde_vec_2F _screen);
// The page's menu: Delete (what it opened on, or what the lasso holds); one step.
void fude_zoom_page_context_delete(fude_app* _app, any _self, u32 _arg);

// --- page_layers.c: layers -------------------------------------------------------------------------------------------

// The canvas's layers found (their objects, names and flags), how many things each holds into _counts.
void fude_zoom_page_find_layers(fude_zoom_page* _page, u32* _counts);
// The rows' words and looks, as things are now.
void fude_zoom_page_show_layers(fude_zoom_page* _page);
// A layer's flags or name changed: a new LAYER object for it in place of its last (one undo step).
void fude_zoom_page_layer_set(fude_zoom_page* _page, u32 _row, u8 _flags, const c8* _name);
// A layer's row tapped: drawn on from now (tapped again: its name to change).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_layer(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// A layer's eye: hidden or shown.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_layer_eye(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// A layer's lock: locked or unlocked.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_layer_lock(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// Layer row _row moved _by places in the list (-1: up), every layer given its place anew; one step.
void fude_zoom_page_layer_move(fude_zoom_page* _page, u32 _row, i32 _by);
// A layer's up arrow: moved over the one above it.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_layer_up(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// A layer's down arrow: moved under the one below it.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_layer_down(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// A layer's trash: it and everything on it let go, at every depth (one undo step brings them back).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_layer_drop(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// New layer: the next index, named, drawn on from now.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_layer_add(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);
// The lasso's To layer: the panel opened for the layer to move it to.
void fude_zoom_page_to_layer_selection(fude_app* _app, any _self, u32 _arg);
// The toolbar's Layers: the panel shown or put away.
void fude_zoom_layers_press(fude_app* _app);
// The toolbar's Layers: is its panel up?
b8 fude_zoom_layers_selected(const fude_app* _app);

// --- page_boards.c: boards, fitting parts, holes and joints ----------------------------------------------------------

// A length in the unit shown, to type over ("600", "23.625").
void fude_zoom_page_length_text(const fude_zoom_page* _page, f64 _mm, c8* _out, usize _size);
// Board _object's sizes in millimetres, its look (grain, material) and its name. False: not a board.
b8 fude_zoom_page_board_of(const fude_zoom_page* _page, u32 _object, f64* _mm, u8* _look, c8* _name, usize _size);
// Whether _object is a board that has been cut (sawn, notched, rubbed out: shape.h's fude_zoom_board_is_cut). Its
// outline is its own then, so its sizes are not typed again on the chip: that would make it a plain rectangle and lose
// its cuts and holes. False for anything else, a plain board included.
b8 fude_zoom_page_board_cut(const fude_zoom_page* _page, u32 _object);
// A board's size _which typed (_mm): kept, the next asked for, its name last.
void fude_zoom_page_board_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _mm, u32 _which);
// A new board (FUDE_ZOOM_NONE), or that board changed: its sizes asked one by one, then its name. Nothing for a cut
// board (fude_zoom_page_board_cut).
void fude_zoom_page_board_start(fude_zoom_page* _page, u32 _board);
// The board put down (in the middle of the view, at its real size) or changed where it is: one undo step.
void fude_zoom_page_board_commit(fude_zoom_page* _page, const c8* _name);
// The parts typed (_text) fitted into board _stock (nest.h).
void fude_zoom_page_fit_list(fude_zoom_page* _page, u32 _stock, const c8* _text);
// The lassoed boards (alive, cut or not): how many, and the largest by area (the stock) into *_largest (FUDE_ZOOM_NONE:
// none).
u32 fude_zoom_page_lassoed_boards(const fude_zoom_page* _page, u32* _largest);
// The selection's Fit parts: one board, its parts asked for; several, fitted into the largest.
void fude_zoom_page_fit_selection(fude_zoom_page* _page);
// The row's length typed: holes 32 mm apart over it, across the middle of the view.
void fude_zoom_page_holes_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _mm);
// The joint put down: its outline at its real size in the middle of the view; one step.
void fude_zoom_page_joint_put(fude_zoom_page* _page);
// Insert's Finger joint or Dovetail (_kind): its sizes asked one by one.
void fude_zoom_page_joint_start(fude_zoom_page* _page, u8 _kind);
// Insert's row of holes: its length asked for on the numpad.
void fude_zoom_page_holes_start(fude_zoom_page* _page);

// --- page_ui.c: the page's own widgets -------------------------------------------------------------------------------

// The line's chip tapped: the card for what it shows (a size, a board, a sheet, a line's length).
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_line_chip(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);

// --- page_look.c: the look harness -----------------------------------------------------------------------------------

// A part _w × _h points (0: as it comes) put _x, _y screen points from the view's middle, turned _turn degrees.
u32 fude_zoom_page_look_sized(fude_zoom_page* _page, u32 _frame, const c8* _id, f64 _x, f64 _y, f64 _turn, f64 _w, f64 _h, const c8* _text, rde_arr* _born);

#endif
