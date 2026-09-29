#ifndef KANA_LASSO
#define KANA_LASSO

#include "rde.h"
#include "ink.h"

// ===========================================================================
// The lasso: select strokes by drawing a loop around them, then move or delete
// them. The selection is only a list of stroke indices; the edits themselves are
// the ink's (kana_ink_move_*, kana_ink_erase_strokes), so they undo like any
// other edit.
//
// A stroke is selected when at least half its points fall inside the loop, so a
// sloppy loop that clips the end of a neighbour does not take it. A loop too
// small to be one is a TAP, and selects the single stroke under the pen.
//
// The selection FOLLOWS the ink rather than owning anything: kana_lasso_sync
// drops strokes that are no longer alive (an undo, an erase, Clear), and the box
// is read from the strokes' own bounds — so undoing a move carries the box back.
//
// CLIPBOARD: copied strokes are held here (in the app, not the system
// pasteboard), positions relative to their centre, so a paste can drop them
// centred anywhere. Pasting and duplicating are the ink's add_strokes — one
// undo step each — and leave the new strokes selected, ready to drag.
// ===========================================================================

// Strokes copied out of the ink. Only point_count, colour and flags of each
// stroke are meaningful; first_point indexes `points` here.
RDE_STRUCT {
    rde_arr TYPE(kana_ink_stroke) strokes;
    rde_arr TYPE(kana_ink_point)  points;   // relative to the copied selection's centre
} kana_clip;

typedef enum {
    KANA_LASSO_IDLE = 0,
    KANA_LASSO_LOOPING,      // the pen is drawing the loop
    KANA_LASSO_MOVING        // the pen is dragging the selection
} KANA_LASSO_STATE_;

// Tolerances, in SCREEN units (the same feel at any zoom).
#define KANA_LASSO_STEP        3.0f    // loop points closer than this are skipped
#define KANA_LASSO_TAP_SIZE    8.0f    // a loop smaller than this box is a tap
#define KANA_LASSO_PICK_RADIUS 10.0f   // a tap's reach
#define KANA_LASSO_BOX_PAD     8.0f    // the drawn box, around the selected ink
#define KANA_LASSO_GRAB_PAD    6.0f    // a press this far outside the drawn box still grabs it
#define KANA_LASSO_DUPLICATE   20.0f   // how far down and right a duplicate lands

// Fraction of a stroke's points that must be inside the loop.
#define KANA_LASSO_INSIDE      0.5f

RDE_STRUCT {
    KANA_LASSO_STATE_        state;
    rde_arr TYPE(rde_vec_2F) loop;       // canvas units, while looping
    rde_arr TYPE(u32)        selected;   // stroke indices
    rde_vec_2F               grab;       // canvas point the move is measured from

    kana_clip                clipboard;
    kana_clip                _duplicate; // scratch: Duplicate must not touch the clipboard

    rde_arr TYPE(rde_vec_2F) _scratch_positions;
    rde_arr TYPE(f32)        _scratch_radii;
} kana_lasso;

void kana_lasso_init(kana_lasso* _lasso);
void kana_lasso_destroy(kana_lasso* _lasso);

// The pen (or mouse) with the Lasso tool, positions in CANVAS units. _zoom turns
// the screen-unit tolerances above into canvas units. Down inside the selection
// box starts a move; anywhere else starts a new loop (and drops the selection).
void kana_lasso_pen_down(kana_lasso* _lasso, kana_ink* _ink, rde_vec_2F _canvas, f32 _zoom);
void kana_lasso_pen_moved(kana_lasso* _lasso, kana_ink* _ink, rde_vec_2F _canvas, f32 _zoom);
void kana_lasso_pen_up(kana_lasso* _lasso, kana_ink* _ink, f32 _zoom);

// A loop or a move is in progress.
b8   kana_lasso_busy(const kana_lasso* _lasso);
// Deselects, dropping an unfinished loop and finishing a move.
void kana_lasso_clear(kana_lasso* _lasso, kana_ink* _ink);
// Drops selected strokes that died. Call before reading the selection.
void kana_lasso_sync(kana_lasso* _lasso, const kana_ink* _ink);
u32  kana_lasso_count(const kana_lasso* _lasso);
// The selected ink's box, canvas units. False when nothing is selected.
b8   kana_lasso_bounds(const kana_lasso* _lasso, const kana_ink* _ink, rde_vec_2F* _min, rde_vec_2F* _max);
// Erases the selection (one undoable edit) and deselects.
void kana_lasso_delete(kana_lasso* _lasso, kana_ink* _ink);

// Clipboard. Copy keeps the selection; Cut deletes it (one undo step); Duplicate
// adds a copy offset down-right and selects it, without touching the clipboard;
// Paste drops the clipboard centred at _canvas and selects what it added.
void kana_lasso_copy(kana_lasso* _lasso, const kana_ink* _ink);
void kana_lasso_cut(kana_lasso* _lasso, kana_ink* _ink);
void kana_lasso_duplicate(kana_lasso* _lasso, kana_ink* _ink, f32 _zoom);
void kana_lasso_paste(kana_lasso* _lasso, kana_ink* _ink, rde_vec_2F _canvas);
b8   kana_lasso_can_paste(const kana_lasso* _lasso);
// Selects every stroke on the page.
void kana_lasso_select_all(kana_lasso* _lasso, kana_ink* _ink);

// The glow goes UNDER the ink; the loop and the box OVER it.
void kana_lasso_render_under(kana_lasso* _lasso, kana_ink* _ink, rde_vec_2F _offset, f32 _zoom);
void kana_lasso_render_over(kana_lasso* _lasso, const kana_ink* _ink, rde_vec_2F _offset, f32 _zoom);

#endif
