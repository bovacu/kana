// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PAGE
#define FUDE_ZOOM_PAGE

#include "rde.h"
#include "drawing/app/extension.h"
#include "drawing/ink/ink.h"
#include "zoom/scene.h"
#include "zoom/render.h"
#include "zoom/erase.h"
#include "zoom/zfile.h"
#include "zoom/select.h"
#include "zoom/nav.h"

// ===========================================================================
// The deep-zoom page: Sketching's, in place of the ink page (extension.h's
// page kind). The same hands as everywhere in fude:
//
//   PEN            draws (the toolbar's pen or marker), or erases (its Erase, or
//                  the pen's eraser end) — the eraser's mode: Erase pressed
//                  again, or E on a computer (partial, whole strokes, trim) —
//                  or, with the Lasso, loops things to select them, then drags
//                  them, scales them by a corner, turns them by the knob (select.h)
//   PEN HELD       at a stroke's end: it snaps to the shape it looks like (shape.h),
//                  which then follows the pen — bigger, smaller, turned — until it
//                  lifts; one undo brings the stroke back as drawn
//   PEN, SHAPES    with a shape chosen in the toolbar's Shapes (a line, a rectangle,
//                  an ellipse, a triangle; filled or not): dragged out from where
//                  it goes down to where it lifts
//   PICTURE        the toolbar's Picture: Photos or Files; what comes in is put in
//                  the middle of the screen, selected (with the Lasso) to be moved,
//                  scaled and turned at once
//   1 FINGER HELD  the page's menu (Paste, Select all), when fingers do not draw
//   1 FINGER       pans — or, with the toolbar's hand on, draws
//   2 FINGERS      pinch-zoom (and pan), as far in or out as anyone likes
//   2-FINGER TAP   undo;           3-FINGER TAP  redo
// On a computer: the left button draws, the right drags the page (a right
// click without dragging: the page's menu), the wheel zooms where the mouse is;
// Z undo, Y redo, C clear, R back to the start, E the eraser's mode, Backspace
// or Delete the selection, Escape lets it go.
//
// A stroke is captured by an ink of its own (ink.h, exactly Kana's pen: its
// pressure, its pens without pressure, its sampling) in the camera frame's
// units round where it began, and becomes the canvas's (scene.h) when the pen
// lifts. Brush colour, size and width come from the app's ink, which the
// toolbar and Settings set as everywhere.
//
// The canvas's file (zfile.h) is <save dir>/notes/<id>.zoom; its journal is
// appended about every second while there is anything new, and everything is
// checkpointed when the canvas is left or the app goes.
// ===========================================================================

struct fude_app;

#define FUDE_ZOOM_PAGE_FLUSH         1.0     // seconds between journal appends while drawing
#define FUDE_ZOOM_PAGE_FINGERS       5u
#define FUDE_ZOOM_PAGE_LONG_PRESS    0.5     // seconds a still finger is held for the page's menu
#define FUDE_ZOOM_PAGE_HOLD          0.55    // seconds the pen rests at a stroke's end before it snaps to a shape
#define FUDE_ZOOM_PAGE_HOLD_SLOP     3.0f    // ...moving less than this (screen units)

// The paper's spacing on screen, at least (and under twice that): the dots', and
// lines' and squares' by Settings' Lines & squares size.
#define FUDE_ZOOM_PAGE_DOTS          40.0
#define FUDE_ZOOM_PAGE_PAPER_SMALL   28.0
#define FUDE_ZOOM_PAGE_PAPER_MEDIUM  44.0
#define FUDE_ZOOM_PAGE_PAPER_LARGE   72.0

typedef struct {
    b8         active;
    u64        id;
    rde_vec_2F at;       // screen
    rde_vec_2F start;
} fude_zoom_finger;

#define FUDE_ZOOM_PAGE_BACK   16u    // views Back remembers
#define FUDE_ZOOM_PAGE_CRUMBS 8u     // the depth's levels shown (the top and the nearest above)
#define FUDE_ZOOM_PAGE_PLACES 8u     // bookmarks listed (the newest)

typedef struct fude_zoom_page {
    struct fude_app*   app;
    fude_zoom_scene    scene;
    fude_zoom_renderer renderer;
    fude_zoom_eraser   eraser;
    fude_zoom_file     file;
    fude_ink           capture;      // the stroke being drawn
    fude_zoom_selection selection;   // the lasso's
    u8                 tool_seen;    // the toolbar's tool last frame (another one lets the selection go)
    b8                 long_pressed; // this finger gesture already opened the menu
    u32                canvas;       // the canvas open (notes.h id; 0: none yet)
    b8                 open;
    u32                device;
    u8                 erase_mode;   // FUDE_ZOOM_ERASE_
    u32                erase_taps_seen;
    u32                tool_taps_seen;
    // The Shapes tool: the shape chosen (0: none — the bar's tool is in hand),
    // filled or not, and the one being dragged out (screen).
    u8                 shape_tool;   // FUDE_ZOOM_SHAPE_ (a triangle is a POLYGON)
    b8                 shape_filled;
    b8                 fill_tool;    // the Fill tool in hand: a tap fills what is closed under it (fill.h)
    b8                 shaping;
    rde_vec_2F         shape_from;

    // The pen (or mouse, or a writing finger) down.
    b8                 drawing;
    b8                 erasing;
    b8                 pen_on_ui;
    u32                draw_frame;
    fude_zoom_v2       origin;       // where the stroke began, that frame's units
    f64                draw_z;
    rde_vec_2F         erase_last;   // screen
    u64                writer;       // the finger drawing (with the hand on)
    b8                 finger_drawing;
    f64                stroke_began;
    // Hold to snap: when the pen last moved, and the shape it snapped to.
    f64                moved_at;
    rde_vec_2F         moved_from;   // screen
    b8                 snapped;
    fude_zoom_shape_fit fit;         // screen units, as recognized
    rde_vec_2F         snap_pen;     // where the pen was when it snapped
    rde_vec_2F         pen_now;      // and now
    // The brush's circle (its size, where it would draw): the pen hovering
    // (or the mouse), its eraser end or not.
    rde_vec_2F         hover_at;     // screen (eased: a pen leaving the glass reads unsteadily)
    b8                 hover_on;
    b8                 hover_eraser;
    f64                hover_quiet;  // not shown before this (the pen just lifted)
    rde_arr TYPE(fude_zoom_v2) snap_scratch;
    // Smoothing (smooth.h): the level for new strokes (FUDE_ZOOM_SMOOTH_, a
    // setting), and the stroke's points as the pen gave them (from origin, the
    // frame's units: the capture holds them smoothed), its first point still
    // unsettled, and the rope's tip.
    u8                 smooth_level;
    rde_arr TYPE(fude_zoom_v2) raw;
    rde_arr TYPE(fude_zoom_v2) smoothed;
    u32                smooth_from;
    fude_zoom_v2       rope_tip;

    // The fingers moving the page.
    fude_zoom_finger   fingers[FUDE_ZOOM_PAGE_FINGERS];
    f64                tap_start;
    u32                tap_fingers;
    b8                 tap_spoiled;

    // A computer's right-button drag (no drag: the page's menu).
    b8                 mouse_panning;
    rde_vec_2F         mouse_last;
    rde_vec_2F         mouse_down;
    b8                 mouse_dragged;

    f64                flushed_at;
    rde_arr TYPE(fude_zoom_visible) editable;

    // Getting around (nav.h): the flight under way; where Back goes (the views
    // flights left, newest last — their frames by id, which outlive slots); the
    // marks for an empty screen, and the camera they were found for.
    fude_zoom_flight   flight;
    struct { fude_zoom_id frame; fude_zoom_v2 at; f64 z; } back[FUDE_ZOOM_PAGE_BACK];
    u32                back_count;
    fude_zoom_nav_mark marks[FUDE_ZOOM_NAV_MARKS];
    u32                mark_count;
    fude_zoom_camera   marks_for;
    f64                marks_at;
    // ...and their widgets (the extension's own: ui.h): Back beside the menu,
    // the depth at the bottom left, its levels over it once tapped.
    rde_ui_button*     back_button;
    rde_ui_button*     depth_button;
    rde_ui_button*     crumbs[FUDE_ZOOM_PAGE_CRUMBS];
    fude_zoom_camera   crumb_views[FUDE_ZOOM_PAGE_CRUMBS];
    u32                crumb_count;
    b8                 crumbs_open;
    c8                 depth_said[32];
    rde_vec_2F         back_center, depth_center, crumb_size;
    b8                 shown_back, shown_depth, shown_crumbs;
    rde_vec_2F         laid_out;
    // Places (the bookmarks: scene.h): its button by the depth, and its list
    // over it once tapped — first Mark this view (or Unmark), then the places.
    rde_ui_button*     places_button;
    rde_ui_button*     place_rows[FUDE_ZOOM_PAGE_PLACES + 1u];
    rde_ui_button*     place_drops[FUDE_ZOOM_PAGE_PLACES];   // the × beside each place: let it go
    u32                place_marks[FUDE_ZOOM_PAGE_PLACES];   // each row's bookmark (scene.marks)
    u32                place_count;     // rows of places shown
    i32                place_here;      // the bookmark the view is at (-1: none): row 0 unmarks it
    b8                 places_open, shown_places, shown_place_rows;
    rde_vec_2F         places_center, place_size;

    // Export (export.h): a PNG under way — its file chosen (1), the view drawn
    // into the target (2: read back the frame after, the GPU done with it).
    u8                 export_stage;
    b8                 export_share;      // to the system's share sheet (a tablet's), not a file chosen
    c8                 export_path[1024];
    rde_render_texture* export_target;
} fude_zoom_page;

// The page's hooks for the extension (extension.h: .page_kind), and its tools
// for the toolbar (extension.h: .tools): Shapes, Picture and Smoothing, each
// with its choices, and Fill; its settings (extension.h: .settings_gather, .settings_apply).
extern const fude_page_kind      FUDE_ZOOM_PAGE_KIND;
extern const fude_extension_tool FUDE_ZOOM_TOOLS[5];   // Shapes, Picture, Smoothing, Fill, Export

// A picture's longer side, at most, as it comes in (pixels).
#define FUDE_ZOOM_PAGE_PICTURE_PX 4096u

void fude_zoom_page_settings_gather(const struct fude_app* _app, fude_settings* _settings);
void fude_zoom_page_settings_apply(struct fude_app* _app, const fude_settings* _settings);
// Its widgets over the page (extension.h: .ui_build, .ui_forget, .ui_update,
// .ui_restyle, .ui_hit): Back and the depth.
void fude_zoom_page_ui_build(struct fude_ui* _ui, rde_ui_node* _root);
void fude_zoom_page_ui_forget(struct fude_ui* _ui);
void fude_zoom_page_ui_update(struct fude_ui* _ui, b8 _full);
void fude_zoom_page_ui_restyle(struct fude_ui* _ui);
b8   fude_zoom_page_ui_hit(const struct fude_ui* _ui, rde_vec_2F _screen, rde_vec_2F _canvas);

void fude_zoom_page_init(fude_zoom_page* _page, struct fude_app* _app);
void fude_zoom_page_destroy(fude_zoom_page* _page);
// A pen, touch or mouse event while no screen is on top.
void fude_zoom_page_event(fude_zoom_page* _page, rde_event* _event);
// Once a frame while no screen is on top.
void fude_zoom_page_update(fude_zoom_page* _page);
// The page: its paper, the canvas, the stroke being drawn. Inside a 2D drawing block.
void fude_zoom_page_render(fude_zoom_page* _page, rde_window* _window);
// The OS short of memory: the decoded strokes let go.
void fude_zoom_page_trim(fude_zoom_page* _page);
// A developer's looks: the pen as if it went down (0), moved (1), lifted (2) at
// _screen — what the pen's events do.
void fude_zoom_page_look_pen(fude_zoom_page* _page, u8 _phase, rde_vec_2F _screen);
// ...and the getting around: flown home as Reset view does (0), the depth's
// levels opened (1), Back (2), the view marked (3), Places opened (4).
void fude_zoom_page_look_nav(fude_zoom_page* _page, u8 _what);
// ...and Export into _path: PNG (0) or SVG (1).
void fude_zoom_page_look_export(fude_zoom_page* _page, u8 _format, const c8* _path);
// A PNG export's view drawn off screen, when one is under way: in the frame's
// render, before the page's own drawing (the shell's).
void fude_zoom_page_render_offscreen(fude_zoom_page* _page, rde_window* _window, rde_camera* _camera);

#endif
