// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_LASSO
#define FUDE_LASSO

#include "rde.h"
#include "drawing/ink/ink.h"

// ===========================================================================
// Lasso: selecting strokes by drawing a loop around them, then moving, deleting, copying or duplicating them.
//
// Where it sits: drawing/ink, over ink.h. The app shell's page (app/page.c) drives it with the Lasso tool, calls the
// clipboard from its menus, and over a document (doc.h) asks for the loop's box to select the PDF's own text. The
// study's page text (pagetext.c) pastes text written as ink through it.
// Its main types and their lifecycle: fude_lasso is the caller's struct; fude_lasso_init gives it its arrays (on the
// standard heap) and fude_lasso_destroy frees them, the clipboard included. A fude_clip handed to
// fude_lasso_paste_clip stays the caller's.
// Threads: main thread only.
// Design:
//   The selection is only a list of stroke indices. The edits themselves are the ink's (fude_ink_move_*,
//   fude_ink_erase_strokes), so they undo like any other edit.
//   A stroke is selected when at least half its points fall inside the loop, so a sloppy loop that clips the end of
//   a neighbour does not take it. A loop too small to be one is a TAP, and selects the single stroke under the pen.
//   The selection FOLLOWS the ink rather than owning anything: fude_lasso_sync drops strokes that are no longer alive
//   (after an undo, an erase, Clear), and the box is read from the strokes' own bounds, so undoing a move carries the
//   box back.
//   A loop that takes no ink can still take the page's own TEXT, a PDF's under a document canvas (doc.h). The page
//   asks whether there is text in the loop's box (fude_lasso_looped) and, if so, selects that AREA
//   (fude_lasso_select_area). It shows like a selection (its box, and the menus with the app's text row), but holds no
//   strokes, so nothing moves or is deleted.
//   CLIPBOARD: copied strokes are held here, in the app rather than the system pasteboard, with positions relative to
//   their centre, so a paste can drop them centred anywhere. Pasting and duplicating are the ink's add_strokes (one
//   undo step each), and they leave the new strokes selected, ready to drag.
// ===========================================================================

// Strokes copied out of the ink. Only each stroke's point_count, colour and flags mean anything; its first_point
// indexes this clip's own points.
RDE_STRUCT {
    rde_arr TYPE(fude_ink_stroke) strokes;
    rde_arr TYPE(fude_ink_point)  points;   // relative to the copied selection's centre
} fude_clip;

// What the pen is doing with the lasso.
typedef enum {
    FUDE_LASSO_IDLE = 0,
    FUDE_LASSO_LOOPING,      // the pen is drawing the loop
    FUDE_LASSO_MOVING        // the pen is dragging the selection
} FUDE_LASSO_STATE_;

// Tolerances, in SCREEN units, so they feel the same at any zoom.
#define FUDE_LASSO_STEP        3.0f    // loop points closer than this are skipped
#define FUDE_LASSO_TAP_SIZE    8.0f    // a loop smaller than this box is a tap
#define FUDE_LASSO_PICK_RADIUS 10.0f   // a tap's reach
#define FUDE_LASSO_BOX_PAD     8.0f    // the drawn box, around the selected ink
#define FUDE_LASSO_GRAB_PAD    6.0f    // a press this far outside the drawn box still grabs it
#define FUDE_LASSO_DUPLICATE   20.0f   // how far down and right a duplicate lands

// The fraction of a stroke's points that must be inside the loop.
#define FUDE_LASSO_INSIDE      0.5f

// A lasso: the loop being drawn, the selection, and the clipboard.
RDE_STRUCT {
    FUDE_LASSO_STATE_        state;
    rde_arr TYPE(rde_vec_2F) loop;       // canvas units, while looping
    rde_arr TYPE(u32)        selected;   // stroke indices
    rde_vec_2F               grab;       // the canvas point the move is measured from
    b8                       looped;     // the last loop took no ink: its box is kept, for the page's text
    rde_vec_2F               loop_min, loop_max;
    b8                       area_on;    // an area of the page's text is selected (no strokes)
    rde_vec_2F               area_min, area_max;

    fude_clip                clipboard;
    fude_clip                _duplicate; // scratch space, because Duplicate must not touch the clipboard

    rde_arr TYPE(rde_vec_2F) _scratch_positions;
} fude_lasso;

// fude_lasso_init sets up an empty lasso; fude_lasso_destroy frees its arrays, the clipboard included.
void fude_lasso_init(fude_lasso* _lasso);
void fude_lasso_destroy(fude_lasso* _lasso);

// The pen (or mouse) with the Lasso tool, positions in CANVAS units. _zoom turns the screen-unit tolerances above
// into canvas units. A press inside the selection's box (or just outside it) starts a move; anywhere else starts a
// new loop and drops the selection. On pen up, a loop selects what it took, and a move becomes one undoable edit.
void fude_lasso_pen_down(fude_lasso* _lasso, fude_ink* _ink, rde_vec_2F _canvas, f32 _zoom);
void fude_lasso_pen_moved(fude_lasso* _lasso, fude_ink* _ink, rde_vec_2F _canvas, f32 _zoom);
void fude_lasso_pen_up(fude_lasso* _lasso, fude_ink* _ink, f32 _zoom);

// Returns whether a loop or a move is in progress.
b8   fude_lasso_busy(const fude_lasso* _lasso);
// Deselects: drops an unfinished loop, finishes a move, and forgets a selected text area.
void fude_lasso_clear(fude_lasso* _lasso, fude_ink* _ink);
// fude_lasso_sync drops selected strokes that are no longer alive; call it before reading the selection.
// fude_lasso_count returns how many strokes are selected.
void fude_lasso_sync(fude_lasso* _lasso, const fude_ink* _ink);
u32  fude_lasso_count(const fude_lasso* _lasso);
// Writes the selected ink's box (canvas units) into *_min and *_max. Returns false when nothing is selected.
b8   fude_lasso_bounds(const fude_lasso* _lasso, const fude_ink* _ink, rde_vec_2F* _min, rde_vec_2F* _max);
// Writes what is selected as a box: the ink's, else the text area's. Returns false when nothing is selected.
b8   fude_lasso_box(const fude_lasso* _lasso, const fude_ink* _ink, rde_vec_2F* _min, rde_vec_2F* _max);
// Writes the last loop's box when it took no ink (and was a loop, not a tap). Returns false otherwise.
b8   fude_lasso_looped(const fude_lasso* _lasso, rde_vec_2F* _min, rde_vec_2F* _max);
// fude_lasso_select_area selects an area of the page's text (see the top), dropping any selected strokes.
// fude_lasso_area writes the selected area's box, and returns false when no area is selected (or strokes are, or a
// loop or move is in progress).
void fude_lasso_select_area(fude_lasso* _lasso, rde_vec_2F _min, rde_vec_2F _max);
b8   fude_lasso_area(const fude_lasso* _lasso, rde_vec_2F* _min, rde_vec_2F* _max);
// Erases the selected strokes as one undoable edit, and deselects.
void fude_lasso_delete(fude_lasso* _lasso, fude_ink* _ink);

// The clipboard. Copy keeps the selection; Cut deletes it (one undo step); Duplicate adds a copy offset down and to
// the right and selects it, without touching the clipboard; Paste drops the clipboard centred at _canvas and selects
// what it added. With nothing selected, Copy and Cut leave the clipboard as it was.
void fude_lasso_copy(fude_lasso* _lasso, const fude_ink* _ink);
void fude_lasso_cut(fude_lasso* _lasso, fude_ink* _ink);
void fude_lasso_duplicate(fude_lasso* _lasso, fude_ink* _ink, f32 _zoom);
void fude_lasso_paste(fude_lasso* _lasso, fude_ink* _ink, rde_vec_2F _canvas);
// Pastes strokes from elsewhere (text written as ink, textink.h): _clip is dropped centred at _canvas as one undo
// step and left selected, and the lasso's clipboard is untouched. fude_lasso_can_paste returns whether the
// clipboard holds anything.
void fude_lasso_paste_clip(fude_lasso* _lasso, fude_ink* _ink, const fude_clip* _clip, rde_vec_2F _canvas);
b8   fude_lasso_can_paste(const fude_lasso* _lasso);
// Selects every alive stroke on the page.
void fude_lasso_select_all(fude_lasso* _lasso, fude_ink* _ink);

// Draw the selection through the canvas view (screen = canvas * _zoom + _offset), inside a 2D drawing block. The
// glow goes UNDER the ink (fude_lasso_render_under, which also drops dead strokes from the selection); the loop and
// the box go OVER it (fude_lasso_render_over).
void fude_lasso_render_under(fude_lasso* _lasso, fude_ink* _ink, rde_vec_2F _offset, f32 _zoom);
void fude_lasso_render_over(fude_lasso* _lasso, const fude_ink* _ink, rde_vec_2F _offset, f32 _zoom);

#endif
