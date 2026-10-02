#ifndef FUDE_PAGE
#define FUDE_PAGE

#include "rde.h"
#include "drawing/doc/doc.h"

// ===========================================================================
// The page, under every screen: writing on it and moving it.
//
//   PEN            writes (or erases, with the Erase tool or the pen's eraser end;
//                  with the Lasso tool, loops strokes to select, then drags them)
//   1 FINGER       pans the page — or, with the toolbar's hand on, writes
//   2 FINGERS      pinch-zoom (and pan)
//   2-FINGER TAP   undo;           3-FINGER TAP  redo
//   1-FINGER HOLD  the page's context menu (pagemenu.h)
// Keys, for the desktop: C clear, Z undo, Y redo, R reset view, B brush scale,
// Backspace/Delete deletes the selection, Esc deselects; right-click is the
// context menu, and the left button writes. A developer's build: M raw samples,
// H the HUD.
//
// Presses on the UI (fude_ui_hit) are the UI's: nothing under them writes.
// ===========================================================================

struct fude_app;

#define FUDE_PAGE_FINGER_POINTS 128u

typedef struct fude_page_input {
    struct fude_app* app;
    b8         pen_on_ui;      // the pen went down on the UI: nothing it does until it lifts writes, erases or pans
    b8         erasing;        // the pen (or mouse) is down with the Erase tool: its path erases
    b8         touch_seen;     // a finger has touched (the HUD tells "no input at all" from "touch only")
    b8         show_samples;   // a developer's: the raw pen samples (M)
    b8         show_hud;       // a developer's: the diagnostics (H)

    // Writing with a finger (the toolbar's hand): one finger writes as the pen
    // does; two or more move the page, as fingers always do. Which it is shows a
    // moment later: a finger down is PENDING — its points kept — until it moves
    // or rests a moment with a little movement: then it writes, from where it
    // landed. A second finger landing first makes them the page's gesture (pinch,
    // pan, the two- and three-finger taps). Held still, it is the long press. A
    // finger landing while one writes is a resting hand: nothing. A short touch
    // is a dot.
    u8         finger_state;   // FUDE_PAGE_FINGER_
    u64        finger_id;      // the finger pending, writing or ignored
    rde_vec_2F finger_points[FUDE_PAGE_FINGER_POINTS];
    u32        finger_count;
    f64        finger_since;
    u32        finger_gesture; // fingers handed to the page's gesture, still down

    // The zoom, shown a moment at the bottom right whenever it changes.
    f32        zoom_seen;      // < 0: not seen yet (no toast for the loaded zoom)
    f64        zoom_shown_at;

    // The open canvas's document, under its ink (doc.h).
    fude_doc   doc;
} fude_page_input;

void fude_page_init(fude_page_input* _page, struct fude_app* _app);
// A pen, touch or mouse event while no screen is on top.
void fude_page_event(fude_page_input* _page, rde_event* _event);
// Once a frame while no screen is on top: the keys, the desktop's drag, a
// finger's moment to decide, the long press.
void fude_page_update(fude_page_input* _page);
// The page: its paper, the ink, the lasso's selection; the zoom's toast.
void fude_page_render(fude_page_input* _page, rde_window* _window);
// What the pen (or a finger) was doing, finished: a stroke ended, an erase, a
// lasso (going to the background, another canvas, a screen over the page).
void fude_page_let_go(fude_page_input* _page);
// A pen came down, anywhere: the pen writes from now on (the hand goes off).
void fude_page_pen_came(fude_page_input* _page);
// The zoom shown is the zoom now (another canvas loaded: no toast for it).
void fude_page_zoom_seen(fude_page_input* _page);
// After a lasso loop: one that took no ink, round a document's text, selects
// that area (lasso.h) for the app's text row.
void fude_page_lasso_text(fude_page_input* _page);

#endif
