// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_LASSO
#define FUDE_LASSO

#include "rde.h"
#include "drawing/ink/ink.h"

// ===========================================================================
// The lasso: select strokes by drawing a loop around them, then move or delete
// them. The selection is only a list of stroke indices; the edits themselves are
// the ink's (fude_ink_move_*, fude_ink_erase_strokes), so they undo like any
// other edit.
//
// A stroke is selected when at least half its points fall inside the loop, so a
// sloppy loop that clips the end of a neighbour does not take it. A loop too
// small to be one is a TAP, and selects the single stroke under the pen.
//
// The selection FOLLOWS the ink rather than owning anything: fude_lasso_sync
// drops strokes that are no longer alive (an undo, an erase, Clear), and the box
// is read from the strokes' own bounds — so undoing a move carries the box back.
//
// A loop that takes no ink can still take the page's own TEXT — a PDF's, under
// a document canvas (doc.h): the page asks whether there is some in the loop's
// box (fude_lasso_looped) and, if so, selects that AREA (fude_lasso_select_area).
// It shows like a selection (its box, the menus: the app's text row), but holds
// no strokes, so nothing moves or is deleted.
//
// CLIPBOARD: copied strokes are held here (in the app, not the system
// pasteboard), positions relative to their centre, so a paste can drop them
// centred anywhere. Pasting and duplicating are the ink's add_strokes — one
// undo step each — and leave the new strokes selected, ready to drag.
// ===========================================================================

// Strokes copied out of the ink. Only point_count, colour and flags of each
// stroke are meaningful; first_point indexes `points` here.
RDE_STRUCT {
    rde_arr TYPE(fude_ink_stroke) strokes;
    rde_arr TYPE(fude_ink_point)  points;   // relative to the copied selection's centre
} fude_clip;

typedef enum {
    FUDE_LASSO_IDLE = 0,
    FUDE_LASSO_LOOPING,      // the pen is drawing the loop
    FUDE_LASSO_MOVING        // the pen is dragging the selection
} FUDE_LASSO_STATE_;

// Tolerances, in SCREEN units (the same feel at any zoom).
#define FUDE_LASSO_STEP        3.0f    // loop points closer than this are skipped
#define FUDE_LASSO_TAP_SIZE    8.0f    // a loop smaller than this box is a tap
#define FUDE_LASSO_PICK_RADIUS 10.0f   // a tap's reach
#define FUDE_LASSO_BOX_PAD     8.0f    // the drawn box, around the selected ink
#define FUDE_LASSO_GRAB_PAD    6.0f    // a press this far outside the drawn box still grabs it
#define FUDE_LASSO_DUPLICATE   20.0f   // how far down and right a duplicate lands

// Fraction of a stroke's points that must be inside the loop.
#define FUDE_LASSO_INSIDE      0.5f

RDE_STRUCT {
    FUDE_LASSO_STATE_        state;
    rde_arr TYPE(rde_vec_2F) loop;       // canvas units, while looping
    rde_arr TYPE(u32)        selected;   // stroke indices
    rde_vec_2F               grab;       // canvas point the move is measured from
    b8                       looped;     // the last loop took no ink: its box (for the page's text)
    rde_vec_2F               loop_min, loop_max;
    b8                       area_on;    // an area of the page's text is selected (no strokes)
    rde_vec_2F               area_min, area_max;

    fude_clip                clipboard;
    fude_clip                _duplicate; // scratch: Duplicate must not touch the clipboard

    rde_arr TYPE(rde_vec_2F) _scratch_positions;
} fude_lasso;

void fude_lasso_init(fude_lasso* _lasso);
void fude_lasso_destroy(fude_lasso* _lasso);

// The pen (or mouse) with the Lasso tool, positions in CANVAS units. _zoom turns
// the screen-unit tolerances above into canvas units. Down inside the selection
// box starts a move; anywhere else starts a new loop (and drops the selection).
void fude_lasso_pen_down(fude_lasso* _lasso, fude_ink* _ink, rde_vec_2F _canvas, f32 _zoom);
void fude_lasso_pen_moved(fude_lasso* _lasso, fude_ink* _ink, rde_vec_2F _canvas, f32 _zoom);
void fude_lasso_pen_up(fude_lasso* _lasso, fude_ink* _ink, f32 _zoom);

// A loop or a move is in progress.
b8   fude_lasso_busy(const fude_lasso* _lasso);
// Deselects, dropping an unfinished loop and finishing a move.
void fude_lasso_clear(fude_lasso* _lasso, fude_ink* _ink);
// Drops selected strokes that died. Call before reading the selection.
void fude_lasso_sync(fude_lasso* _lasso, const fude_ink* _ink);
u32  fude_lasso_count(const fude_lasso* _lasso);
// The selected ink's box, canvas units. False when nothing is selected.
b8   fude_lasso_bounds(const fude_lasso* _lasso, const fude_ink* _ink, rde_vec_2F* _min, rde_vec_2F* _max);
// What is selected, as a box: the ink's, else the text area's. False: nothing.
b8   fude_lasso_box(const fude_lasso* _lasso, const fude_ink* _ink, rde_vec_2F* _min, rde_vec_2F* _max);
// The last loop's box, when it took no ink (and was a loop, not a tap).
b8   fude_lasso_looped(const fude_lasso* _lasso, rde_vec_2F* _min, rde_vec_2F* _max);
// An area of the page's text selected (see the top); the selected area, when one is.
void fude_lasso_select_area(fude_lasso* _lasso, rde_vec_2F _min, rde_vec_2F _max);
b8   fude_lasso_area(const fude_lasso* _lasso, rde_vec_2F* _min, rde_vec_2F* _max);
// Erases the selection (one undoable edit) and deselects.
void fude_lasso_delete(fude_lasso* _lasso, fude_ink* _ink);

// Clipboard. Copy keeps the selection; Cut deletes it (one undo step); Duplicate
// adds a copy offset down-right and selects it, without touching the clipboard;
// Paste drops the clipboard centred at _canvas and selects what it added.
void fude_lasso_copy(fude_lasso* _lasso, const fude_ink* _ink);
void fude_lasso_cut(fude_lasso* _lasso, fude_ink* _ink);
void fude_lasso_duplicate(fude_lasso* _lasso, fude_ink* _ink, f32 _zoom);
void fude_lasso_paste(fude_lasso* _lasso, fude_ink* _ink, rde_vec_2F _canvas);
// The same with strokes from elsewhere (text written as ink, textink.h): _clip
// centred at _canvas, one undo step, left selected. The lasso's clipboard is untouched.
void fude_lasso_paste_clip(fude_lasso* _lasso, fude_ink* _ink, const fude_clip* _clip, rde_vec_2F _canvas);
b8   fude_lasso_can_paste(const fude_lasso* _lasso);
// Selects every stroke on the page.
void fude_lasso_select_all(fude_lasso* _lasso, fude_ink* _ink);

// The glow goes UNDER the ink; the loop and the box OVER it.
void fude_lasso_render_under(fude_lasso* _lasso, fude_ink* _ink, rde_vec_2F _offset, f32 _zoom);
void fude_lasso_render_over(fude_lasso* _lasso, const fude_ink* _ink, rde_vec_2F _offset, f32 _zoom);

#endif
