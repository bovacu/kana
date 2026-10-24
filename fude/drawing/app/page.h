// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_PAGE
#define FUDE_PAGE

#include "rde.h"
#include "drawing/doc/doc.h"

// ===========================================================================
// Page: the page under every screen, and the input that writes on it and moves it.
//
//   PEN            writes (or erases, with the Erase tool or the pen's eraser end; with the Lasso tool, loops strokes
//                  to select them, then drags them)
//   1 FINGER       pans the page, or, with the toolbar's hand on, writes
//   2 FINGERS      pinch-zoom (and pan)
//   2-FINGER TAP   undo;           3-FINGER TAP  redo
//   1-FINGER HOLD  the page's context menu (pagemenu.h)
// Keys, for the desktop: C clear, Z undo, Y redo, R reset view, B brush scale, Backspace or Delete deletes the
// selection, Esc deselects; right-click is the context menu, and the left button writes. In a developer's build:
// M shows the raw samples, H the HUD.
//
// Presses on the UI (fude_ui_hit) are the UI's: nothing under them writes. Fingers the system takes away (a cancel: a
// palm the pen rejects, an edge gesture) let go of what they were doing, as when the pen comes down, and are no tap.
//
// Where it sits: drawing/app. The shell owns one fude_page_input (fude_app.page) and calls it while no screen is on
// top; it holds the open canvas's document (doc.h) and draws the ink (ink.h), the lasso (lasso.h) and the paper.
// Each frame it also runs the import (import.h).
//
// Its main types and their lifecycle: the shell makes the fude_page_input (a static) with fude_page_init and keeps it
// for the app's life.
//
// Threads: main thread only.
// ===========================================================================

struct fude_app;

#define FUDE_PAGE_FINGER_POINTS 128u   // The most points kept from a finger while it is pending.

// The page's input state. The fields are page.c's, except doc, which the document bar and the app's text row use.
typedef struct fude_page_input {
    struct fude_app* app;
    // The pen went down on the UI: nothing it does until it lifts writes, erases or pans.
    b8         pen_on_ui;
    b8         erasing;        // The pen (or mouse) is down with the Erase tool: its path erases.
    // A finger has touched, so the HUD can tell "no input at all" from "touch only".
    b8         touch_seen;
    b8         show_samples;   // A developer's view: the raw pen samples (M).
    b8         show_hud;       // A developer's view: the diagnostics (H).

    // Writing with a finger (the toolbar's hand): one finger writes as the pen does, and two or more move the page, as
    // fingers always do. Which it is shows a moment later. A finger down is PENDING, with its points kept, until it
    // moves, or rests a moment with a little movement: then it writes, from where it landed. A second finger landing
    // first makes them the page's gesture (pinch, pan, the two- and three-finger taps). A finger held still is the long
    // press. A finger landing while one writes is a resting hand, and does nothing. A short touch is a dot.
    u8         finger_state;   // FUDE_PAGE_FINGER_ (page.c)
    u64        finger_id;      // The finger that is pending, writing or ignored.
    rde_vec_2F finger_points[FUDE_PAGE_FINGER_POINTS];
    u32        finger_count;
    f64        finger_since;
    u32        finger_gesture; // Fingers handed to the page's gesture that are still down.

    // The zoom, shown for a moment at the bottom right whenever it changes.
    f32        zoom_seen;      // Below 0 until it is first seen, so the zoom a canvas loads with shows no toast.
    f64        zoom_shown_at;

    // The open canvas's document, under its ink (doc.h).
    fude_doc   doc;
} fude_page_input;

// Sets up the page's input for _app, with its document empty. fude_page_destroy, as the app ends, closes its document
// (its PDF, and the drawing worker's job let go) and frees what it holds, and the canvas's list of turned pages.
void fude_page_init(fude_page_input* _page, struct fude_app* _app);
void fude_page_destroy(fude_page_input* _page);
// Handles a pen, touch or mouse event while no screen is on top.
void fude_page_event(fude_page_input* _page, rde_event* _event);
// Called once a frame while no screen is on top: runs the import and the document, the keys, the desktop's drag, a
// writing finger's moment to decide, and the long press.
void fude_page_update(fude_page_input* _page);
// Draws the page: its paper, the document, the ink, the lasso's selection, the zoom's toast, the document's page pill,
// and what the app draws over the page (extension.h: page_render).
void fude_page_render(fude_page_input* _page, rde_window* _window);
// Finishes what the pen (or a finger) was doing: a stroke, an erase, a lasso. Called when going to the background,
// opening another canvas, or opening a screen over the page.
void fude_page_let_go(fude_page_input* _page);
// Called when a pen comes down anywhere: the pen writes from now on, so the hand goes off.
void fude_page_pen_came(fude_page_input* _page);
// Takes the zoom now as the zoom shown, so a canvas just loaded shows no toast for its zoom.
void fude_page_zoom_seen(fude_page_input* _page);
// After a lasso loop: a loop that took no ink, drawn round a document's text, selects that area (lasso.h) for the
// app's text row. Only an app with a text row (extension.h) gets this.
void fude_page_lasso_text(fude_page_input* _page);

#endif
