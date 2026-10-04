// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_ERASE
#define FUDE_ZOOM_ERASE

#include "rde.h"
#include "zoom/scene.h"

// ===========================================================================
// The erasers, three behind one button (research §2.5):
//
//   PARTIAL  (the default) rubs out exactly what the eraser's disc passes
//            over, at any zoom: each stroke it reaches becomes its outline —
//            FILLs in its colour and order, the loops it closes round left
//            open — and those are cut as any fill, the sweep's reach taken out.
//            Only a stroke that cannot be traced is cut through its line
//            instead (the pieces on either side strokes of their own, with
//            round ends; bits under a pixel long dropped).
//   STROKE   takes every stroke it touches, whole, with every piece of the
//            same gesture (Kana's eraser).
//   TRIM     takes the stretch of a touched stroke between the two crossings
//            with other strokes nearest the touch; with no crossing, all of it.
//
// A SHAPE (shape.h) or a FILL (fill.h) is rubbed out where the eraser passes
// (PARTIAL: a shape becomes its line, a stroke, and its fill, a FILL, which are
// cut as any; a fill gets the eraser's path cut out of it), or goes whole in
// the other modes — touched on its line, or inside when filled. A picture is never
// erased — what is written over a photo rubs out and the photo stays (as in
// GoodNotes or Notability); a picture goes through the selection's Delete.
//
// A sweep — pen down to pen up — is ONE undo step however much it takes: what
// it erased and the pieces it left. A piece cut again in the same sweep never
// reaches the history.
//
// Everything here is in one frame's units; the page maps the eraser into each
// frame it may edit (render.h's visible frames at the camera's depth and below).
// ===========================================================================

typedef enum {
    FUDE_ZOOM_ERASE_PARTIAL = 0,
    FUDE_ZOOM_ERASE_STROKE,
    FUDE_ZOOM_ERASE_TRIM,
    FUDE_ZOOM_ERASE_COUNT
} FUDE_ZOOM_ERASE_;

// A fill the sweep has been at (PARTIAL): it as it was before the sweep (its
// points, its place), the sweep's path over it (its units), and what stands
// for it now. The sweep's reach is one cut ring (fill.h's sweep), traced again
// as the path grows — never a ring a step, which piles up past drawing.
typedef struct {
    u32                            now;      // the fill standing for it now (made this sweep, or itself)
    rde_arr TYPE(fude_zoom_qpoint) points;   // as it was: its points, rings and all
    fude_zoom_place                place;
    i8                             q;
    u32                            frame;
    rde_color                      color;
    u8                             flags;    // what it keeps when cut (a marker's)
    u64                            z;
    u32                            ring;     // the ring this sweep's cut takes (one past its last)
    f64                            reach;    // the eraser's radius, its units
    rde_arr TYPE(fude_zoom_v2)     path;     // the sweep over it, its units
} fude_zoom_erase_fill;

typedef struct {
    u8                mode;    // FUDE_ZOOM_ERASE_
    b8                open;
    rde_arr TYPE(fude_zoom_erase_fill) fills;
    rde_arr TYPE(u32) died;    // objects that were there before the sweep
    rde_arr TYPE(u32) born;    // pieces it left that are still there
    // Scratch, kept between steps.
    rde_arr TYPE(u32)              found;
    rde_arr TYPE(fude_zoom_qpoint) points;
} fude_zoom_eraser;

void fude_zoom_eraser_init(fude_zoom_eraser* _e);
void fude_zoom_eraser_destroy(fude_zoom_eraser* _e);

void fude_zoom_erase_begin(fude_zoom_eraser* _e, FUDE_ZOOM_ERASE_ _mode);
// The eraser moved from _a to _b in frame _frame, radius _r, and bits shorter
// than _min_len dropped (all in that frame's units). How many strokes it took
// or cut.
u32  fude_zoom_erase_step(fude_zoom_scene* _s, fude_zoom_eraser* _e, u32 _frame, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _r, f64 _min_len);
// The sweep over: one undo step (none if it took nothing), recorded for frame
// _frame's box _box (the camera's, for flying back to it).
void fude_zoom_erase_end(fude_zoom_scene* _s, fude_zoom_eraser* _e, u32 _frame, fude_zoom_box _box);

#endif
