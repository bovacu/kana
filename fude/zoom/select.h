// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SELECT
#define FUDE_ZOOM_SELECT

#include "rde.h"
#include "zoom/scene.h"
#include "zoom/render.h"

// ===========================================================================
// Select: the lasso's selection on the deep-zoom canvas, with its handles, its menu's commands and its clipboard.
//
// Where it sits: fude/zoom, over scene.h and render.h. The page drives it from the lasso tool (page_input.c) and
// the selection's menus (page_edit.c and the modules of their buttons); pieces (piece.c) use its clips.
// Its main types and their lifecycle: the page owns one fude_zoom_selection, made with fude_zoom_select_init and freed
// with fude_zoom_select_destroy. Clips (fude_zoom_clip) own their bytes: those in the selection's clipboard are freed
// with it, and an array of them made by fude_zoom_select_clips_of is freed with fude_zoom_select_clips_free.
// Threads: main thread only, like the scene.
// Design:
//   - A LOOP drawn round things selects them: a stroke when most of its points are inside, a shape when most of its
//     outline is, a picture or a text when most of its corners and middle are, and a frame (with everything drawn
//     deeper inside it) when its whole anchor box is inside. So a drawing moves with the detail drawn inside it at
//     any depth (the tree and the ladybug on its leaf: docs/infinite_canvas_design.md §2.6). A frame only partly
//     inside is gone into, and its own things and frames are taken by the same rule. Only what is at the camera's
//     depth or deeper can be taken (§2.5), and never what a hidden or locked layer holds.
//   - THE SELECTION is drawn with its box and handles: dragging inside the box moves it, a corner scales it
//     (uniformly, round the opposite corner), and the knob above it turns it (round its middle). While the pen is
//     down the selection is LIFTED: the selection draws it where it would be, not the canvas, so a drag costs nothing
//     however much is selected. Where the pen lifts, the canvas takes it back as one undo step (scene.h: a MOVED
//     action, with each object's place before and after; a stroke's points never change).
//   - THE CLIPBOARD keeps copies of things (their bytes and how they looked on screen), not references, so it
//     outlives the canvas: copy on one canvas, paste on another. Paste puts them where it is asked, the same size on
//     screen as when they were copied. Frames are not copied, only the things selected themselves.
// ===========================================================================

// Sizes on screen, in points.
#define FUDE_ZOOM_SELECT_HANDLE   9.0f    // a handle's radius
#define FUDE_ZOOM_SELECT_GRAB     22.0f   // how near a handle the pen must land to take it
#define FUDE_ZOOM_SELECT_KNOB     30.0f   // how far above the box's top the turning knob is
#define FUDE_ZOOM_SELECT_PAD      8.0f    // the box's margin round what is selected
#define FUDE_ZOOM_SELECT_DUPLICATE 24.0f  // how far off a duplicate lands

// What the pen is doing with the selection.
typedef enum {
    FUDE_ZOOM_GRAB_NONE = 0,
    FUDE_ZOOM_GRAB_LOOP,     // a loop being drawn
    FUDE_ZOOM_GRAB_MOVE,
    FUDE_ZOOM_GRAB_SCALE,
    FUDE_ZOOM_GRAB_ROTATE
} FUDE_ZOOM_GRAB_;

// One thing selected.
typedef struct {
    u32 object;   // the object's index: a thing, or a FRAME object (the frame taken whole)
} fude_zoom_pick;

// A copy of a thing on the clipboard: its record (how it looks), its bytes (a stroke's points, a shape's numbers, a
// text's words, a picture's file), and how it sat on screen when it was copied (from its own units to the screen).
typedef struct {
    fude_zoom_object look;
    rde_arr TYPE(u8) bytes;
    fude_zoom_sim    on_screen;
} fude_zoom_clip;

// fude_zoom_clip_data and fude_zoom_clip_size return a clip's bytes and how many there are.
static inline const u8* fude_zoom_clip_data(const fude_zoom_clip* _c) { return _c->bytes.memory; }
static inline u32       fude_zoom_clip_size(const fude_zoom_clip* _c) { return _c->bytes.count; }
// Makes the clip's bytes a new copy of _size bytes from _data, with room for just that; the clip owns them.
void                    fude_zoom_clip_put(fude_zoom_clip* _c, const u8* _data, u32 _size);

// The selection, the loop being drawn, the drag under way and the clipboard.
typedef struct {
    rde_arr TYPE(fude_zoom_pick)  picks;
    rde_arr TYPE(rde_vec_2F)      loop;       // the loop being drawn, on screen
    u8                            grab;       // FUDE_ZOOM_GRAB_
    rde_vec_2F                    grab_at;    // where the pen took hold, on screen
    rde_vec_2F                    pivot;      // what a scale or a turn goes round, on screen
    fude_zoom_sim                 drag;       // the drag since the pen took hold, from the screen to the screen
    f64                           turn;       // a turn's angle so far (radians, anticlockwise; resting at each 15°)
    fude_zoom_box                 box;        // the selection's box on screen, as of the last update
    fude_zoom_v2                  half;       // half the screen's size: a guide counts only its part on screen (0: all)
    rde_arr TYPE(fude_zoom_place) before;     // each pick's place when the drag began
    rde_arr TYPE(u8)              lifted;     // one per object: whether the selection draws it while it is dragged
    // The clipboard, and the middle of the selection's box on screen when it was copied (a paste centres on it).
    rde_arr TYPE(fude_zoom_clip)  clip;
    fude_zoom_v2                  clip_center;
    // Scratch arrays, kept so they are not allocated again each time.
    rde_arr TYPE(u32)              found;
    rde_arr TYPE(fude_zoom_qpoint) q;
} fude_zoom_selection;

// fude_zoom_select_init makes an empty selection with an empty clipboard; fude_zoom_select_destroy frees it all.
void fude_zoom_select_init(fude_zoom_selection* _sel);
void fude_zoom_select_destroy(fude_zoom_selection* _sel);

// fude_zoom_select_clear lets go of everything selected (the clipboard is kept); fude_zoom_select_any returns whether
// anything is selected.
void fude_zoom_select_clear(fude_zoom_selection* _sel);
b8   fude_zoom_select_any(const fude_zoom_selection* _sel);
// Returns whether the pen is at the selection (a loop being drawn, or a drag); the menu waits until it is not.
b8   fude_zoom_select_busy(const fude_zoom_selection* _sel);
// Called once a frame: what has died since (by an undo or an erase) leaves the selection, and its box on screen is
// worked out again.
void fude_zoom_select_update(fude_zoom_selection* _sel, const fude_zoom_scene* _s);
// Writes the selection's box on screen (app space), padded, into _out. Returns false when nothing is selected.
b8   fude_zoom_select_box(const fude_zoom_selection* _sel, fude_zoom_box* _out);

// The pen went down with the lasso tool: on a handle or inside the box it starts a drag; anywhere else it starts a new
// loop, letting go of what was selected (fude_zoom_select_up takes what the loop holds from its frames).
void fude_zoom_select_down(fude_zoom_selection* _sel, fude_zoom_scene* _s, rde_vec_2F _screen);
// Returns what of the selection is under _screen: a corner (0 to 3: min-min, max-min, min-max, max-max), the turning
// knob (4) or inside its box (5). Returns -1 for none of them, or when nothing is selected.
i32  fude_zoom_select_handle_at(const fude_zoom_selection* _sel, rde_vec_2F _screen);
// The pen moved to _screen: the loop grows, or the drag follows it.
void fude_zoom_select_moved(fude_zoom_selection* _sel, rde_vec_2F _screen);
// While the selection is being turned, writes how far it has turned so far (in degrees, anticlockwise) and where to
// say so (on screen, over the knob). Returns false when it is not being turned.
b8   fude_zoom_select_turning(const fude_zoom_selection* _sel, f64* _degrees, rde_vec_2F* _at);
// The pen lifted: a loop selects what it holds, in the frames _r drew at the camera's depth and below; a drag puts
// what it moved down where it is, as one undo step, with the connectors and wires joined to it following.
void fude_zoom_select_up(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_renderer* _r);
// Lets go of what the pen began as if it never had (a second finger made it a pinch): a loop being drawn is dropped
// and takes nothing; a drag leaves the picks where they were, and nothing is done.
void fude_zoom_select_abort(fude_zoom_selection* _sel, const fude_zoom_scene* _s);
// Appends to _out (an rde_arr of fude_zoom_clip) copies of _objects as Copy makes them: their look, their bytes and how
// they are on the screen now, in the order they are drawn whatever order _objects has them in (so copies put down in
// it lie over each other as the things did). Strokes, fills, shapes, texts and pictures are copied; frames are left
// out. The properties on them (props.h) are copied too, after them, to go on their copies. The clips own their bytes:
// fude_zoom_select_clips_free frees them and empties the array (the array itself stays the caller's).
void fude_zoom_select_clips_of(const fude_zoom_scene* _s, const u32* _objects, u32 _n, rde_arr* _out);
void fude_zoom_select_clips_free(rde_arr* _clips);
// Puts copies of _clips down as Paste does with the clipboard's: each where its on_screen says, moved so that their
// point _center lands on _screen, into the camera's frame. The lasso holds them afterwards; one undo step.
void fude_zoom_select_paste_clips(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_clip* _clips, u32 _n, fude_zoom_v2 _center, rde_vec_2F _screen);
// A mirror line on screen: through at, at angle (radians).
typedef struct {
    fude_zoom_v2 at;
    f64          angle;
} fude_zoom_select_mirror;
// Puts copies down (Repeat's, in page_measure.c), into the camera's frame. With no _mirror, each of the _n clips is put
// down _m times, copy k moved by _moves[k] (from the screen to the screen). With _mirror, each is put down once,
// mirrored across its line: strokes, fills and shapes are turned over, while texts and pictures keep the right way
// round, their boxes where the reflection puts them. The _dead_n objects in _dead (originals, when it is a flip rather
// than a copy) are let go in the same undo step. The copies are added to what the lasso holds. A dimension, a circle's
// size or a connector is joined to the copies of what it was joined to, and an end whose thing was not copied to
// nothing: a copy never follows the originals. Returns how many objects were made (properties copied with their
// things included).
u32  fude_zoom_select_put(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_clip* _clips, u32 _n, const fude_zoom_sim* _moves, u32 _m,
                          const fude_zoom_select_mirror* _mirror, const u32* _dead, u32 _dead_n);
// Adds things carried along by the press just taken (a container's contents in page_shapes.c: an area's, a Kanban
// column's) to the selection while the pen is down, after its own picks: they are lifted, move with it and go into its
// undo step. Does nothing unless a drag is under way. fude_zoom_select_keep then cuts the selection back to its own
// picks.
void fude_zoom_select_carry(fude_zoom_selection* _sel, fude_zoom_scene* _s, const u32* _objects, u32 _n);
// Moves what the lasso holds as a drag would, without the pen (a size typed on page_measure.c's size card).
// fude_zoom_select_begin takes hold of it (things may be carried then); fude_zoom_select_end lets go of it moved by
// _drag (from the screen to the screen), as one undo step, with the connectors joined to it following.
void fude_zoom_select_begin(fude_zoom_selection* _sel, fude_zoom_scene* _s);
void fude_zoom_select_end(fude_zoom_selection* _sel, fude_zoom_scene* _s, fude_zoom_sim _drag);
// Cuts the selection back to its first _n picks (after the pen lifts, dropping what was carried).
void fude_zoom_select_keep(fude_zoom_selection* _sel, const fude_zoom_scene* _s, u32 _n);

// Draws the selection over the canvas: what is lifted (being dragged), a glow on what is selected, its box and
// handles, and the loop. Call it after the canvas is drawn.
void fude_zoom_select_render(fude_zoom_selection* _sel, fude_zoom_renderer* _r, const fude_zoom_scene* _s, fude_zoom_v2 _half);

// The page menus' commands follow. Each one that changes the canvas is one undo step.
// Deletes what is selected (with the wires its parts kept) and lets go of the selection.
void fude_zoom_select_delete(fude_zoom_selection* _sel, fude_zoom_scene* _s);
// How fude_zoom_select_arrange lines things up: on the selection's left, centre or right, its top, middle or bottom,
// or spread across or down it.
typedef enum {
    FUDE_ZOOM_ARRANGE_LEFT = 0, FUDE_ZOOM_ARRANGE_CENTRE, FUDE_ZOOM_ARRANGE_RIGHT,
    FUDE_ZOOM_ARRANGE_TOP, FUDE_ZOOM_ARRANGE_MIDDLE, FUDE_ZOOM_ARRANGE_BOTTOM,
    FUDE_ZOOM_ARRANGE_ACROSS, FUDE_ZOOM_ARRANGE_DOWN, FUDE_ZOOM_ARRANGE_COUNT
} FUDE_ZOOM_ARRANGE_;
// Arranges what is selected as the screen shows it: lines up their boxes on the selection's own edges or middles, or
// spreads them out with even gaps between them (which needs three at least). Their connectors follow; one undo step.
// Returns false when nothing moved (fewer than two selected, or a drag under way).
b8   fude_zoom_select_arrange(fude_zoom_selection* _sel, fude_zoom_scene* _s, FUDE_ZOOM_ARRANGE_ _how);
// Moves what the lasso holds onto layer _layer: copies are made where they are, the originals are let go, and
// connectors joined to them are joined to the copies. One undo step; the selection stays, on the copies. Returns how
// many things moved.
u32  fude_zoom_select_to_layer(fude_zoom_selection* _sel, fude_zoom_scene* _s, u16 _layer);
// fude_zoom_select_copy puts copies of what is selected on the clipboard, in the order they are drawn, replacing what
// it held. fude_zoom_select_paste puts the clipboard down centred on _screen. fude_zoom_select_duplicate puts copies
// down FUDE_ZOOM_SELECT_DUPLICATE points off, as a copy and a paste would, but leaves the clipboard as it was (as
// Duplicate does in other apps: what was copied can still be pasted after it). fude_zoom_select_all selects
// everything in the frames _r drew at the camera's depth and below. fude_zoom_select_can_paste returns whether the
// clipboard holds anything.
void fude_zoom_select_copy(fude_zoom_selection* _sel, const fude_zoom_scene* _s);
void fude_zoom_select_paste(fude_zoom_selection* _sel, fude_zoom_scene* _s, rde_vec_2F _screen);
void fude_zoom_select_duplicate(fude_zoom_selection* _sel, fude_zoom_scene* _s);
void fude_zoom_select_all(fude_zoom_selection* _sel, fude_zoom_scene* _s, const fude_zoom_renderer* _r);
b8   fude_zoom_select_can_paste(const fude_zoom_selection* _sel);

#endif
