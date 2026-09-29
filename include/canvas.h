#ifndef KANA_CANVAS
#define KANA_CANVAS

#include "rde.h"

// ===========================================================================
// The infinite canvas: where the page sits under the screen, and the finger
// gestures that move it. The PEN writes; FINGERS move the page — one finger
// pans, two pinch-zoom around their midpoint (and pan with it).
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
#define KANA_CANVAS_GRID_COLOR      (rde_color){ 22, 22, 26, 255 }

#define KANA_CANVAS_MAX_FINGERS 2

RDE_STRUCT {
    rde_vec_2F offset;
    f32        zoom;
} kana_view;

RDE_STRUCT {
    b8         active;
    u64        finger_id;
    rde_vec_2F position;   // screen space
} kana_canvas_finger;

RDE_STRUCT {
    kana_view          view;
    kana_canvas_finger fingers[KANA_CANVAS_MAX_FINGERS];
} kana_canvas;

void       kana_canvas_init(kana_canvas* _canvas);
// Back to zoom 1, page origin at the screen centre.
void       kana_canvas_reset_view(kana_canvas* _canvas);

rde_vec_2F kana_canvas_from_screen(const kana_canvas* _canvas, rde_vec_2F _screen);
rde_vec_2F kana_canvas_to_screen(const kana_canvas* _canvas, rde_vec_2F _canvas_pos);

// Finger events, positions in SCREEN space. A third finger is ignored; lifting
// one of two hands the pan to the other without a jump.
void       kana_canvas_finger_down(kana_canvas* _canvas, u64 _finger_id, rde_vec_2F _screen);
void       kana_canvas_finger_moved(kana_canvas* _canvas, u64 _finger_id, rde_vec_2F _screen);
void       kana_canvas_finger_up(kana_canvas* _canvas, u64 _finger_id);
// Drops every tracked finger (e.g. when the pen goes down mid-gesture).
void       kana_canvas_release_fingers(kana_canvas* _canvas);

// The dot grid. Call inside a 2D drawing block, before the ink.
void       kana_canvas_draw_grid(const kana_canvas* _canvas, rde_vec_2I _window_size);

#endif
