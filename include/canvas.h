#ifndef KANA_CANVAS
#define KANA_CANVAS

#include "rde.h"

// ===========================================================================
// The infinite canvas: where the page sits under the screen, and the finger
// gestures that move it. The PEN writes; FINGERS move the page — one finger
// pans, two pinch-zoom around their midpoint (and pan with it). A quick TAP of
// two fingers is undo and of three is redo, as in most iPad drawing apps; one
// finger HELD still is a long press (the page's context menu).
//
// Two spaces:
//   SCREEN — the 2D camera's world: centre-origin, Y up, logical units. What
//            pen positions convert into and what the renderer draws in.
//   CANVAS — the page. Ink is STORED here, so it is independent of where the
//            view was when it was written — which is what scoring will want.
//
//   screen = canvas * zoom + offset
// ===========================================================================

#define KANA_CANVAS_ZOOM_MIN 0.2f
#define KANA_CANVAS_ZOOM_MAX 8.0f

// Background dot grid: spacing in canvas units, halved/doubled with zoom so the
// dots stay between these screen distances apart.
#define KANA_CANVAS_GRID_SPACING    50.0f
#define KANA_CANVAS_GRID_MIN_SCREEN 24.0f
#define KANA_CANVAS_GRID_MAX_SCREEN 96.0f
#define KANA_CANVAS_GRID_DOT        2.0f

// The PAPER (kana_page.background), a guide to write on: the dots above, ruled
// LINES across, practice SQUARES — each with its fainter dashed centre cross,
// like a practice sheet — or nothing. Lines and squares are in the theme's dot
// colour and part of the page: they pan and zoom with it, one size apart
// (Settings › Lines & squares, the same on every canvas); the lines fall on the
// squares' edges, so writing stays in place from one to the other.
#define KANA_CANVAS_SQUARE_CROSS    72.0f    // smaller on screen than this: no centre cross (and fewer dashes to draw)
// The page and dot colours are the theme's (theme.h).

// Fingers tracked at once. Only the first two move the page; the rest are just
// counted, so a three-finger tap works and a third finger doesn't drag the page.
#define KANA_CANVAS_MAX_FINGERS 5

// A multi-finger TAP: every finger up within this long of the first going down,
// and none moved further than the slop (screen units).
#define KANA_CANVAS_TAP_MAX_TIME 0.35
#define KANA_CANVAS_TAP_SLOP     16.0f

// One finger held this long without moving past the slop.
#define KANA_CANVAS_LONG_PRESS_TIME 0.5

typedef enum {
    KANA_CANVAS_TAP_NONE = 0,
    KANA_CANVAS_TAP_TWO,      // undo
    KANA_CANVAS_TAP_THREE     // redo
} KANA_CANVAS_TAP_;

RDE_STRUCT {
    rde_vec_2F offset;
    f32        zoom;
} kana_view;

RDE_STRUCT {
    b8         active;
    u64        finger_id;
    rde_vec_2F position;   // screen space
    rde_vec_2F start;      // where it went down, for the tap slop
} kana_canvas_finger;

// A page's paper. The values are what the file stores ('PAGE'): squares were
// the first, as 1.
typedef enum {
    KANA_PAPER_DOTS = 0,
    KANA_PAPER_SQUARES,
    KANA_PAPER_LINES,
    KANA_PAPER_NONE,
    KANA_PAPER_COUNT
} KANA_PAPER_;

// What the page shows under its ink — the page's own, saved with it (save.h
// 'PAGE'): a new canvas has the dots.
RDE_STRUCT {
    KANA_PAPER_ paper;
} kana_page;

// The sizes lines and squares can be (a setting).
typedef enum {
    KANA_PAPER_SMALL = 0,
    KANA_PAPER_MEDIUM,     // the default: a comfortable character at zoom 1
    KANA_PAPER_LARGE,
    KANA_PAPER_SIZE_COUNT
} KANA_PAPER_SIZE_;

// A size in canvas units: 110, 160, 240.
f32 kana_canvas_paper_units(KANA_PAPER_SIZE_ _size);

RDE_STRUCT {
    kana_view          view;
    kana_page          page;
    KANA_PAPER_SIZE_   paper_size;    // lines' and squares' size (a setting, not the page's)
    kana_canvas_finger fingers[KANA_CANVAS_MAX_FINGERS];

    // The finger gesture in progress, from the first finger down to the last up.
    f64                tap_start;
    u32                tap_fingers;   // most fingers down at once
    b8                 tap_spoiled;   // moved, held too long, or the pen came down
    kana_view          tap_view;      // the view when it began; a tap puts it back
    b8                 long_pressed;  // this gesture already fired its long press
} kana_canvas;

void       kana_canvas_init(kana_canvas* _canvas);
// Back to zoom 1, page origin at the screen centre.
void       kana_canvas_reset_view(kana_canvas* _canvas);

rde_vec_2F kana_canvas_from_screen(const kana_canvas* _canvas, rde_vec_2F _screen);
rde_vec_2F kana_canvas_to_screen(const kana_canvas* _canvas, rde_vec_2F _canvas_pos);

// Finger events, positions in SCREEN space. Lifting one of two hands the pan to
// the other without a jump. finger_up reports a multi-finger TAP when the last
// finger of one lifts; the caller acts on it (undo / redo). A tap's own small
// drift of the view is undone, so tapping never nudges the page.
void       kana_canvas_finger_down(kana_canvas* _canvas, u64 _finger_id, rde_vec_2F _screen);
void       kana_canvas_finger_moved(kana_canvas* _canvas, u64 _finger_id, rde_vec_2F _screen);
KANA_CANVAS_TAP_ kana_canvas_finger_up(kana_canvas* _canvas, u64 _finger_id);
// Drops every tracked finger (e.g. when the pen goes down mid-gesture).
void       kana_canvas_release_fingers(kana_canvas* _canvas);

// Once a frame (a held finger sends no events): true, once per gesture, when a
// single finger has been held still long enough; *_screen gets where. The view
// is put back as it was, and that finger stops moving the page.
b8         kana_canvas_long_press(kana_canvas* _canvas, rde_vec_2F* _screen);

// The page's paper: dots, lines, squares or nothing. Call inside a 2D drawing
// block, before the ink.
void       kana_canvas_draw_grid(const kana_canvas* _canvas, rde_vec_2I _window_size);

#endif
