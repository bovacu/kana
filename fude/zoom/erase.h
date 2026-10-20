// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_ERASE
#define FUDE_ZOOM_ERASE

#include "rde.h"
#include "zoom/scene.h"

// ===========================================================================
// Erase: the canvas's three erasers, behind one button (docs/research/infinite_canvas_research.md §2.5).
//
//   PARTIAL  (the default) rubs out exactly what the eraser's disc passes over, at any zoom. Each stroke it reaches
//            becomes its outline (FILLs in its colour and its place in the order, with the loops it closes round left
//            open), and those are cut like any fill, the sweep's reach taken out of them. Only a stroke whose outline
//            cannot be traced is cut through its line instead: the pieces on either side become strokes of their
//            own, with round ends, and bits shorter than about a pixel are dropped.
//   STROKE   takes every stroke it touches whole, with every other piece of the same gesture (the ink page's eraser).
//   TRIM     takes the stretch of a touched stroke between the two crossings with other strokes nearest the touch;
//            with no crossing, all of it.
//
// A SHAPE (shape.h) or a FILL (fill.h) is rubbed out where the eraser passes in PARTIAL mode: a shape becomes its line
// (a stroke) and its fill (a FILL), which are cut like any others, and a fill has the eraser's path cut out of it. A
// board loses the wood rubbed out of it (cut.h). Dimensions, connectors, a diagram's symbols, sheets, guides and wires
// always go whole. In the other modes a shape or fill goes whole when it is touched on its line, or inside when it is
// filled. A picture is never erased: what is written over a photo rubs out and the photo stays (as in GoodNotes or
// Notability); a picture goes through the selection's Delete. Hidden and locked layers are left alone.
//
// Where it sits: fude/zoom, over scene.h. The page (page_pen.c) drives it: it maps the eraser into each frame it may
// edit (render.h's visible frames at the camera's depth and below), so everything here is in one frame's units.
// Its main types and their lifecycle: the page owns one fude_zoom_eraser, made with fude_zoom_eraser_init and freed
// with fude_zoom_eraser_destroy. A sweep runs fude_zoom_erase_begin, fude_zoom_erase_step for each move, then
// fude_zoom_erase_end.
// Threads: main thread only, like the scene.
// Design: a sweep (pen down to pen up) is ONE undo step however much it takes: what it erased and the pieces it left.
// A piece cut again in the same sweep never reaches the history.
// ===========================================================================

// The eraser's modes, as above.
typedef enum {
    FUDE_ZOOM_ERASE_PARTIAL = 0,
    FUDE_ZOOM_ERASE_STROKE,
    FUDE_ZOOM_ERASE_TRIM,
    FUDE_ZOOM_ERASE_COUNT
} FUDE_ZOOM_ERASE_;

// A fill the sweep has been at (PARTIAL mode): the fill as it was before the sweep (its points and place), the sweep's
// path over it (in its units), and what stands for it now. The sweep's reach is one cut ring (fill.h's sweep), traced
// again as the path grows, never a new ring for each step, which would pile up until it could not be drawn.
typedef struct {
    u32                            now;      // the fill that stands for it now (one made this sweep, or itself)
    rde_arr TYPE(fude_zoom_qpoint) points;   // its points as it was, rings and all
    fude_zoom_place                place;
    i8                             q;
    u32                            frame;
    rde_color                      color;
    u8                             flags;    // the flags it keeps when cut (a marker's, for one)
    u64                            z;
    u32                            ring;     // the ring this sweep's cut takes (one past its last ring)
    f64                            reach;    // the eraser's radius, in its units
    rde_arr TYPE(fude_zoom_v2)     path;     // the sweep over it, in its units
} fude_zoom_erase_fill;

// The eraser: its mode, and what the sweep under way has done so far.
typedef struct {
    u8                mode;    // FUDE_ZOOM_ERASE_
    b8                open;
    rde_arr TYPE(fude_zoom_erase_fill) fills;
    rde_arr TYPE(u32) died;    // objects that were there before the sweep and are gone now
    rde_arr TYPE(u32) born;    // pieces the sweep left that are still there
    // Scratch arrays, kept between steps so they are not allocated again each time.
    rde_arr TYPE(u32)              found;
    rde_arr TYPE(fude_zoom_qpoint) points;
} fude_zoom_eraser;

// fude_zoom_eraser_init makes an eraser ready for sweeps; fude_zoom_eraser_destroy frees what it holds.
void fude_zoom_eraser_init(fude_zoom_eraser* _e);
void fude_zoom_eraser_destroy(fude_zoom_eraser* _e);

// Starts a sweep (the pen going down) in mode _mode.
void fude_zoom_erase_begin(fude_zoom_eraser* _e, FUDE_ZOOM_ERASE_ _mode);
// The eraser moved from _a to _b in frame _frame with radius _r; bits shorter than _min_len are dropped. All of these
// are in that frame's units. Returns how many objects it took or cut (0 when no sweep is open).
u32  fude_zoom_erase_step(fude_zoom_scene* _s, fude_zoom_eraser* _e, u32 _frame, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _r, f64 _min_len);
// Ends the sweep: it becomes one undo step (none when it took nothing), recorded for frame _frame's box _box (the
// camera's view, so undo can fly back to it). A fill rubbed out all over is dropped, and wires left without their
// parts go too.
void fude_zoom_erase_end(fude_zoom_scene* _s, fude_zoom_eraser* _e, u32 _frame, fude_zoom_box _box);

#endif
