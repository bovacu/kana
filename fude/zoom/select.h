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
    f64                           turn;       // a turn's angle so far (radians, counter-clockwise; resting a moment at each 15°)
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
// Being turned: by how much so far (degrees, counter-clockwise), and where to
// say so (screen: over the knob). False: not being turned.
b8   fude_zoom_select_turning(const fude_zoom_selection* _sel, f64* _degrees, rde_vec_2F* _at);
void fude_zoom_select_up(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_renderer* _r);
// Things carried along by the press just taken (a container's contents, page.c: an area's, a Kanban column's): with
// the selection while the pen is down — lifted, moved with it, in its undo step — after its own picks; and the
// selection back to its first _n picks once the pen has lifted (fude_zoom_select_keep).
// Copies of things (_objects: strokes, fills, shapes, texts, pictures; frames left out), as Copy makes them — their
// look, their bytes, how they are on the screen now — appended to _out (fude_zoom_clip; their bytes freed by
// fude_zoom_select_clips_free).
void fude_zoom_select_clips_of(const fude_zoom_scene* _s, const u32* _objects, u32 _n, rde_arr* _out);
void fude_zoom_select_clips_free(rde_arr* _clips);
// Copies put down as Paste puts the clipboard's (each as its on_screen says, the point _center of theirs moved to
// _screen), into the camera's frame, held by the lasso after; one undo step.
void fude_zoom_select_paste_clips(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_clip* _clips, u32 _n, fude_zoom_v2 _center, rde_vec_2F _screen);
// Copies put down (Repeat's: page.c), each of the _n clips _m times — copy k moved by _moves[k] (screen → screen) — or
// once, mirrored across the line through _mirror->at at _mirror->angle (screen, radians): strokes, fills and shapes
// turned over, text and pictures kept the right way round (their boxes where the reflection puts them). Into the
// camera's frame; _dead (originals: a flip, not a copy) let go in the same undo step; the lasso on the copies, added
// to what it holds. How many made.
typedef struct {
    fude_zoom_v2 at;
    f64          angle;
} fude_zoom_select_mirror;
u32  fude_zoom_select_put(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_clip* _clips, u32 _n, const fude_zoom_sim* _moves, u32 _m,
                          const fude_zoom_select_mirror* _mirror, const u32* _dead, u32 _dead_n);
void fude_zoom_select_carry(fude_zoom_selection* _sel, fude_zoom_scene* _s, const u32* _objects, u32 _n);
// What the lasso holds moved as a drag of it would move it, without the pen (a size typed: page.c's size card):
// taken hold of (things may be carried then), then let go moved by _drag (screen → screen) — one undo step, the
// connectors joined to it following.
void fude_zoom_select_begin(fude_zoom_selection* _sel, fude_zoom_scene* _s);
void fude_zoom_select_end(fude_zoom_selection* _sel, fude_zoom_scene* _s, fude_zoom_sim _drag);
void fude_zoom_select_keep(fude_zoom_selection* _sel, const fude_zoom_scene* _s, u32 _n);

// What the selection draws over the canvas: what is lifted (dragged), a glow
// on what is selected, its box and handles, the loop. After the canvas.
void fude_zoom_select_render(fude_zoom_selection* _sel, fude_zoom_renderer* _r, const fude_zoom_scene* _s, fude_zoom_v2 _half);

// The page menus' commands (one undo step each that changes the canvas).
void fude_zoom_select_delete(fude_zoom_selection* _sel, fude_zoom_scene* _s);
// Arranged (FUDE_ZOOM_ARRANGE_): lined up by their boxes on the selection's
// own edges or middles, or spread out with even gaps between them (three at
// least), as the screen has them; their connectors following; one undo step.
typedef enum {
    FUDE_ZOOM_ARRANGE_LEFT = 0, FUDE_ZOOM_ARRANGE_CENTRE, FUDE_ZOOM_ARRANGE_RIGHT,
    FUDE_ZOOM_ARRANGE_TOP, FUDE_ZOOM_ARRANGE_MIDDLE, FUDE_ZOOM_ARRANGE_BOTTOM,
    FUDE_ZOOM_ARRANGE_ACROSS, FUDE_ZOOM_ARRANGE_DOWN, FUDE_ZOOM_ARRANGE_COUNT
} FUDE_ZOOM_ARRANGE_;
b8   fude_zoom_select_arrange(fude_zoom_selection* _sel, fude_zoom_scene* _s, FUDE_ZOOM_ARRANGE_ _how);
// What the lasso holds moved onto layer _layer (copies where they are, the
// originals let go, connectors joined to them joined again): one undo step,
// the selection kept (on the copies). How many things moved.
u32  fude_zoom_select_to_layer(fude_zoom_selection* _sel, fude_zoom_scene* _s, u16 _layer);
void fude_zoom_select_copy(fude_zoom_selection* _sel, const fude_zoom_scene* _s);
void fude_zoom_select_paste(fude_zoom_selection* _sel, fude_zoom_scene* _s, rde_vec_2F _screen);
void fude_zoom_select_duplicate(fude_zoom_selection* _sel, fude_zoom_scene* _s);
void fude_zoom_select_all(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_renderer* _r);
b8   fude_zoom_select_can_paste(const fude_zoom_selection* _sel);

#endif
