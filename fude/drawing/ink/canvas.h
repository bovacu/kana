// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_CANVAS
#define FUDE_CANVAS

#include "rde.h"

// ===========================================================================
// Canvas: the infinite canvas's view (where the page sits under the screen), the finger gestures that move it, and
// the paper drawn under the ink.
//
// The PEN writes; FINGERS move the page. One finger pans; two pinch-zoom around their midpoint (and pan with it). A
// quick TAP of two fingers is undo and of three is redo, as in most iPad drawing apps; one finger HELD still is a long
// press (the page's context menu).
//
// Two spaces:
//   SCREEN   the 2D camera's world: the origin at the centre, Y up, logical units. Pen positions are converted into
//            it, and the renderer draws in it.
//   CANVAS   the page. Ink is STORED here, so it does not depend on where the view was when it was written, which is
//            what scoring needs.
//   screen = canvas * zoom + offset
//
// Where it sits: drawing/ink. The app shell's page (app/page.c) owns a fude_canvas, feeds it finger events and draws
// its paper under the ink; a document (doc.c) sets the reading frame and turns its pages.
// Its main types and their lifecycle: fude_canvas is a plain struct the caller owns, which fude_canvas_init sets up.
// Its fude_page is saved with the page; the list of its turned pages is the only memory it holds, freed by
// fude_page_reset (the session's, before another canvas loads, and page.h's fude_page_destroy as the app ends).
// Threads: main thread only.
// Files: none of its own. The view and the fude_page are saved in the page's file (pagefile.h: 'VIEW', 'PAGE', 'TURN').
// ===========================================================================

// The zoom's limits.
#define FUDE_CANVAS_ZOOM_MIN 0.2f
#define FUDE_CANVAS_ZOOM_MAX 8.0f

// The background dot grid: its spacing in canvas units, halved or doubled with the zoom so the dots stay between
// these screen distances apart, and each dot's size on screen.
#define FUDE_CANVAS_GRID_SPACING    50.0f
#define FUDE_CANVAS_GRID_MIN_SCREEN 24.0f
#define FUDE_CANVAS_GRID_MAX_SCREEN 96.0f
#define FUDE_CANVAS_GRID_DOT        2.0f

// The PAPER (fude_page.paper) is a guide to write on: the dots above, ruled LINES across, practice SQUARES (each with
// a fainter dashed centre cross, like a practice sheet), or nothing. Lines and squares are in the theme's dot colour
// and are part of the page: they pan and zoom with it. They come in one size (Settings › Lines & squares, the same
// on every canvas), and the lines fall on the squares' edges, so writing stays in place when switching between them.
#define FUDE_CANVAS_SQUARE_CROSS    72.0f    // squares smaller than this on screen get no centre cross (fewer dashes)
// The page and dot colours are the theme's (theme.h).

// Fingers tracked at once. Only the first two move the page; the rest are only counted, so a three-finger tap works
// and a third finger does not drag the page.
#define FUDE_CANVAS_MAX_FINGERS 5

// A multi-finger TAP: every finger up within this many seconds of the first going down, and none moved further than
// the slop (screen units).
#define FUDE_CANVAS_TAP_MAX_TIME 0.35
#define FUDE_CANVAS_TAP_SLOP     16.0f

// A long press: one finger held this many seconds without moving past the slop.
#define FUDE_CANVAS_LONG_PRESS_TIME 0.5

// What a multi-finger tap asks for.
typedef enum {
    FUDE_CANVAS_TAP_NONE = 0,
    FUDE_CANVAS_TAP_TWO,      // undo
    FUDE_CANVAS_TAP_THREE     // redo
} FUDE_CANVAS_TAP_;

// Where the page sits under the screen: screen = canvas * zoom + offset.
RDE_STRUCT {
    rde_vec_2F offset;
    f32        zoom;
} fude_view;

// One tracked finger.
RDE_STRUCT {
    // This slot holds a finger that is down.
    b8         active;
    u64        finger_id;
    rde_vec_2F position;   // screen space
    rde_vec_2F start;      // where it went down, for the tap slop
} fude_canvas_finger;

// A page's paper. The values are what the file stores ('PAGE'); squares were the first added, as 1.
typedef enum {
    FUDE_PAPER_DOTS = 0,
    FUDE_PAPER_SQUARES,
    FUDE_PAPER_LINES,
    FUDE_PAPER_NONE,
    // An app's own papers beyond the core's four (extension.h's papers); an app without them draws them as dots.
    FUDE_PAPER_ISOMETRIC,     // lines three ways, 60° apart: triangles (drawing things in 3D)
    FUDE_PAPER_HEX,           // hexagons
    FUDE_PAPER_POLAR,         // rings round the middle and rays every 15°
    FUDE_PAPER_ENGINEERING,   // squares, every fifth line strong (graph paper)
    FUDE_PAPER_MUSIC,         // staves of five lines
    FUDE_PAPER_CORNELL,       // a page with a cue column, ruled notes and a summary at its foot (ruled lines elsewhere)
    FUDE_PAPER_PLANNER,       // a page with a title and a box to tick on each line (ruled lines elsewhere)
    FUDE_PAPER_COUNT
} FUDE_PAPER_;
#define FUDE_PAPER_CORE 4u   // the core's own: dots, squares, lines, none

// How many of a document's pages can be turned: the first 65,536, whose numbers (0 to 65,535) fit in the 16 bits the
// page's file keeps a turned page's number in (pagefile.h's 'TURN').
#define FUDE_PAGE_TURN_PAGES 65536u

// A document's page turned by the learner (doc.h): which page, and how far, in quarter turns clockwise on top of the
// turn the PDF gives it.
RDE_STRUCT {
    u16 page;
    u8  quarters;   // 1, 2 or 3
} fude_page_turn;

// What the page shows under its ink. It is the page's own and saved with it (pagefile.h's 'PAGE' and 'TURN'): a new
// canvas has the dots and no page turned. The turned pages are a list as long as they are many, on the std allocator:
// made by the first turn (or read from the page's file), freed by fude_page_reset. Whoever follows the turns (the
// session, a document) compares their revision rather than keeping a copy of the list.
RDE_STRUCT {
    FUDE_PAPER_                  paper;
    rde_arr TYPE(fude_page_turn) turns;      // each page turned, in no order; not made until one is
    u32                          revision;   // goes up whenever the turns change: a turn, a file read, a reset
} fude_page;

// Returns how many of the document's pages are turned.
u32  fude_page_turn_count(const fude_page* _p);
// Returns how many quarter turns page _page has on _p (0 when it is not turned).
u8   fude_page_turned(const fude_page* _p, u32 _page);
// Turns page _page a quarter clockwise more; a full turn takes it off the list, upright again. Returns false only when
// the page is past the ones that can be turned (FUDE_PAGE_TURN_PAGES).
b8   fude_page_turn_more(fude_page* _p, u32 _page);
// Puts the page back as a new canvas has it (the dots, no page turned), freeing the list of turns. It is also how the
// page is let go of: before another canvas's page loads, and as the app ends.
void fude_page_reset(fude_page* _p);

// The sizes lines and squares can be (a setting).
typedef enum {
    FUDE_PAPER_SMALL = 0,
    FUDE_PAPER_MEDIUM,     // the default: a comfortable character at zoom 1
    FUDE_PAPER_LARGE,
    FUDE_PAPER_SIZE_COUNT
} FUDE_PAPER_SIZE_;

// Returns a paper size in canvas units: 110, 160 or 240.
f32 fude_canvas_paper_units(FUDE_PAPER_SIZE_ _size);

// A document's reading frame (doc.h). While a canvas over a document is open, the view is a reader's: the pages'
// width fits the screen at 100% (the least zoom), they move across only when zoomed in and never past their first
// and last page, and a flick carries on, slowing down. The zoom shown is relative to the fitted one
// (fude_canvas_zoom_shown).
#define FUDE_CANVAS_READER_GUTTER 12.0f    // screen units each side of the pages at 100%
#define FUDE_CANVAS_READER_TOP    76.0f    // the first page's top, under the menu button (and the safe area)
#define FUDE_CANVAS_READER_BOTTOM 96.0f    // room under the last page
#define FUDE_CANVAS_FRICTION      3.5f     // how fast a flick slows down, per second
#define FUDE_CANVAS_COAST_MIN     20.0f    // a flick slower than this (screen units a second) stops

// The reading frame's state.
RDE_STRUCT {
    b8         on;
    f32        width;     // the pages' width, canvas units (centred on x = 0)
    f32        bottom;    // canvas y of the last page's bottom (the first's top is 0)
    f32        fit;       // the zoom that shows the pages at 100%
    rde_vec_2I window;    // the screen it was fitted to
    rde_vec_4I safe;      // ...and that screen's safe area (left, top, right, bottom)
} fude_canvas_reader;

// A canvas's view and the finger gesture moving it.
RDE_STRUCT {
    fude_view          view;
    fude_page          page;
    fude_canvas_reader reader;
    rde_vec_2F         velocity;      // the page's speed, screen units a second: a pan's, then its coast's
    // A flick is carrying the page on.
    b8                 coasting;
    f64                moved_at;      // when the last pan moved it
    f64                ticked_at;     // when fude_canvas_update last ran
    FUDE_PAPER_SIZE_   paper_size;    // lines' and squares' size (a setting, not the page's)
    fude_canvas_finger fingers[FUDE_CANVAS_MAX_FINGERS];

    // The finger gesture in progress, from the first finger down to the last up.
    f64                tap_start;
    u32                tap_fingers;   // the most fingers down at once
    b8                 tap_spoiled;   // a finger moved, or was held too long, or the pen came down: not a tap
    fude_view          tap_view;      // the view when it began; a tap puts it back
    b8                 long_pressed;  // this gesture already fired its long press
} fude_canvas;

// Sets up a canvas: zoom 1 with the page's origin at the screen's centre, the dots, no page turned, medium paper.
void       fude_canvas_init(fude_canvas* _canvas);
// Puts the view back to zoom 1 with the page's origin at the screen's centre (when reading, the first page's top at
// 100%), and stops any flick.
void       fude_canvas_reset_view(fude_canvas* _canvas);

// Turns the reading frame (above) on, for pages _width wide whose last page's bottom is at canvas y _bottom, or off.
// doc.c calls it every frame the page has a document.
void       fude_canvas_set_reader(fude_canvas* _canvas, b8 _on, f32 _width, f32 _bottom);
// Returns the zoom that shows pages _width wide at 100% on a _window with _safe insets.
f32        fude_canvas_reader_fit(f32 _width, rde_vec_2I _window, rde_vec_4I _safe);
// Called once a frame: carries a flick on and, when reading, keeps the view on the pages (fitted again when the screen
// turns, with the same part of them in view).
void       fude_canvas_update(fude_canvas* _canvas, rde_vec_2I _window, rde_vec_4I _safe);
// Returns the zoom as the zoom pill shows it: when reading, relative to 100%; otherwise the zoom itself.
f32        fude_canvas_zoom_shown(const fude_canvas* _canvas);

// Convert a point from screen space to canvas space, and back.
rde_vec_2F fude_canvas_from_screen(const fude_canvas* _canvas, rde_vec_2F _screen);
rde_vec_2F fude_canvas_to_screen(const fude_canvas* _canvas, rde_vec_2F _canvas_pos);

// Finger events, positions in SCREEN space. Lifting one of two fingers hands the pan to the other without a jump.
// fude_canvas_finger_up reports a multi-finger TAP when the last finger of one lifts, and the caller acts on it (undo
// or redo). A tap's own small drift of the view is undone, so tapping never nudges the page.
void       fude_canvas_finger_down(fude_canvas* _canvas, u64 _finger_id, rde_vec_2F _screen);
void       fude_canvas_finger_moved(fude_canvas* _canvas, u64 _finger_id, rde_vec_2F _screen);
FUDE_CANVAS_TAP_ fude_canvas_finger_up(fude_canvas* _canvas, u64 _finger_id);
// Drops every tracked finger, for example when the pen goes down mid-gesture; the gesture is then not a tap.
void       fude_canvas_release_fingers(fude_canvas* _canvas);

// Called once a frame, since a finger held still sends no events. Returns true, once per gesture, when a single
// finger has been held still long enough, with *_screen set to where it is. The view is put back as it was, and the
// finger is dropped: it no longer moves the page.
b8         fude_canvas_long_press(fude_canvas* _canvas, rde_vec_2F* _screen);

// Draws the page's paper (dots, lines, squares or nothing) over a screen of _window_size. Call inside a 2D drawing
// block, before the ink.
void       fude_canvas_draw_grid(const fude_canvas* _canvas, rde_vec_2I _window_size);

#endif
