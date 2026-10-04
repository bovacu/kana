// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SELECT
#define FUDE_ZOOM_SELECT

#include "rde.h"
#include "zoom/scene.h"
#include "zoom/render.h"

// ===========================================================================
// The lasso's selection on the deep-zoom canvas.
//
// A LOOP drawn round things selects them: a stroke when most of it is inside;
// a frame (and everything drawn deeper inside it) when its whole anchor is
// inside — so a drawing moves with the detail drawn inside it at any depth
// (the tree and the ladybug on its leaf: design §2.6). A frame only partly
// inside is gone into, and its strokes and frames taken by the same rule.
// Only what is at the camera's depth or deeper can be taken (§2.5).
//
// THE SELECTION is drawn with its box and handles: inside the box drags it,
// a corner scales it (uniformly, round the opposite corner), the knob above it
// turns it (round its middle). While the pen is down it is LIFTED: drawn on
// top by the selection as it would be, not by the canvas, so a drag costs
// nothing however much is selected; the canvas takes it back where the pen
// lifts, as one undo step (scene.h: a MOVED action, each object's place before
// and after — a stroke's points never change).
//
// THE CLIPBOARD keeps copies of strokes (their points and how they looked on
// screen), not references, so it outlives the canvas: copy on one, paste on
// another. Paste puts them at the same size on screen where it is asked.
// Frames are not copied (only the strokes in the selection itself).
// ===========================================================================

#define FUDE_ZOOM_SELECT_HANDLE   9.0f    // a handle's radius on screen
#define FUDE_ZOOM_SELECT_GRAB     22.0f   // how near a handle the pen must land to take it
#define FUDE_ZOOM_SELECT_KNOB     30.0f   // the turning knob, above the box's top
#define FUDE_ZOOM_SELECT_PAD      8.0f    // the box round what is selected
#define FUDE_ZOOM_SELECT_DUPLICATE 24.0f  // a duplicate lands this far off (screen units)

typedef enum {
    FUDE_ZOOM_GRAB_NONE = 0,
    FUDE_ZOOM_GRAB_LOOP,     // a loop being drawn
    FUDE_ZOOM_GRAB_MOVE,
    FUDE_ZOOM_GRAB_SCALE,
    FUDE_ZOOM_GRAB_ROTATE
} FUDE_ZOOM_GRAB_;

typedef struct {
    u32 object;   // a stroke, or a FRAME object (the frame whole)
} fude_zoom_pick;

// A copy of a stroke on the clipboard: its record's look, its points' bytes,
// and its place as it was on screen (stroke → screen at copy time, from the
// selection's middle).
typedef struct {
    fude_zoom_object look;
    u8*              bytes;
    u32              size;
    fude_zoom_sim    on_screen;
} fude_zoom_clip;

typedef struct {
    rde_arr TYPE(fude_zoom_pick)  picks;
    rde_arr TYPE(rde_vec_2F)      loop;       // the loop being drawn, screen
    u8                            grab;       // FUDE_ZOOM_GRAB_
    rde_vec_2F                    grab_at;    // where the pen took hold (screen)
    rde_vec_2F                    pivot;      // what a scale or a turn goes round (screen)
    fude_zoom_sim                 drag;       // screen → screen, since the pen took hold
    fude_zoom_box                 box;        // the selection's box on screen, as of the last update
    rde_arr TYPE(fude_zoom_place) before;     // each pick's place when the drag began
    rde_arr TYPE(u8)              lifted;     // per object: drawn by the selection while dragged
    rde_arr TYPE(fude_zoom_clip)  clip;
    fude_zoom_v2                  clip_center;
    // Scratch.
    rde_arr TYPE(u32)              found;
    rde_arr TYPE(fude_zoom_qpoint) q;
} fude_zoom_selection;

void fude_zoom_select_init(fude_zoom_selection* _sel);
void fude_zoom_select_destroy(fude_zoom_selection* _sel);

void fude_zoom_select_clear(fude_zoom_selection* _sel);
b8   fude_zoom_select_any(const fude_zoom_selection* _sel);
// The pen at the selection (a loop being drawn or a drag): the menu waits.
b8   fude_zoom_select_busy(const fude_zoom_selection* _sel);
// Once a frame: what died (an undo, an erase) leaves it; its box on screen anew.
void fude_zoom_select_update(fude_zoom_selection* _sel, const fude_zoom_scene* _s);
// Its box on screen (app space), padded. False: nothing selected.
b8   fude_zoom_select_box(const fude_zoom_selection* _sel, fude_zoom_box* _out);

// The pen on the lasso tool: on a handle or inside the box, a drag; anywhere
// else a new loop (what was selected let go). _r: the last draw's frames.
void fude_zoom_select_down(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_renderer* _r, rde_vec_2F _screen);
void fude_zoom_select_moved(fude_zoom_selection* _sel, rde_vec_2F _screen);
void fude_zoom_select_up(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_renderer* _r);

// What the selection draws over the canvas: what is lifted (dragged), a glow
// on what is selected, its box and handles, the loop. After the canvas.
void fude_zoom_select_render(fude_zoom_selection* _sel, fude_zoom_renderer* _r, const fude_zoom_scene* _s, fude_zoom_v2 _half);

// The page menus' commands (one undo step each that changes the canvas).
void fude_zoom_select_delete(fude_zoom_selection* _sel, fude_zoom_scene* _s);
void fude_zoom_select_copy(fude_zoom_selection* _sel, const fude_zoom_scene* _s);
void fude_zoom_select_paste(fude_zoom_selection* _sel, fude_zoom_scene* _s, rde_vec_2F _screen);
void fude_zoom_select_duplicate(fude_zoom_selection* _sel, fude_zoom_scene* _s);
void fude_zoom_select_all(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_renderer* _r);
b8   fude_zoom_select_can_paste(const fude_zoom_selection* _sel);

#endif
