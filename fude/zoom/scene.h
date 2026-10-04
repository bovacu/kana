// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SCENE
#define FUDE_ZOOM_SCENE

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/codec.h"
#include "zoom/index.h"
#include "zoom/shape.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// A canvas: its frames, the objects in them, the camera, and the undo history.
// Knows nothing of the screen beyond the camera's numbers (render.h draws it).
//
// FRAMES (zoom.h: precision, never shown) nest. Each holds its objects — a
// child frame is one of them too (kind FRAME), so it has a place in its
// parent's order like a stroke: what is drawn LATER IS ON TOP, at any depth.
// The root is the frame with no parent; zooming out past it makes a new one
// above it (fude_zoom_camera_settle), so neither way has an end.
//
// OBJECTS live in one table for the whole canvas, in the order they were made;
// a frame lists its own, by order key (z) for drawing and by arrival for the
// file's buckets. NOTHING IS DELETED BY AN EDIT (Kana's rule): an erase marks
// objects dead, so the undo history only flips flags. An object is GARBAGE once
// it is dead and no undo step can bring it back: its points are let go then,
// and the file leaves it out.
//
// THE HISTORY is Kana's log: actions[0 .. action_count) are applied, the rest
// were undone and can be redone until a new edit drops them. One gesture is one
// action (a stroke, an eraser sweep). It is bounded by the bytes it keeps
// alive, not by steps.
//
// THE JOURNAL. Every change is also written, as it happens, into `journal` (ops:
// zfile.h), which the file appends every second or so — every stroke is on disk
// within about a second. Changes that come FROM the file are not journaled.
// ===========================================================================

#define FUDE_ZOOM_NONE         0xFFFFFFFFu

// A child frame's unit in its parent's units, and the camera's zoom range in a
// frame (pt a unit). Past the top it goes into a child (z becomes 1/16), past
// the bottom to the parent (z becomes 16): the gap is the hysteresis.
#define FUDE_ZOOM_CHILD_SCALE  (1.0 / 1024.0)
#define FUDE_ZOOM_Z_MAX        64.0
#define FUDE_ZOOM_Z_MIN        (1.0 / 64.0)

// The paper's lattice (render: 50 · 2^n units, at any zoom) runs on unbroken
// through frames: a new frame's origin is a whole multiple of this many of its
// own units — in its parent, of this times its scale — the coarsest lattice the
// paper ever shows inside it.
#define FUDE_ZOOM_PAPER_ALIGN  6400.0

// A stroke's points a piece, at most: a longer one is cut into pieces of one
// gesture (FUDE_ZOOM_FLAG_CONTINUES), which keeps boxes tight and saves small.
#define FUDE_ZOOM_STROKE_MAX   512u

// Order keys are spaced this far apart as objects arrive, so pieces cut from a
// stroke fit right after it (an erase keeps the place a stroke had).
#define FUDE_ZOOM_Z_STEP       (1ull << 32)

// The undo history's bytes, at most: the oldest steps go past it.
#define FUDE_ZOOM_HISTORY_BUDGET (48u * 1024u * 1024u)

typedef enum {
    FUDE_ZOOM_KIND_STROKE = 1,
    FUDE_ZOOM_KIND_FRAME  = 2,
    FUDE_ZOOM_KIND_SHAPE  = 3,    // shape.h: its type in `channels`, its numbers in its payload
    FUDE_ZOOM_KIND_IMAGE  = 4,    // a picture: its payload f64 half-width, f64 half-height (frame
                                  // units, round its translation, before its scale), then the
                                  // picture's own bytes (JPEG or PNG)
    FUDE_ZOOM_KIND_FILL   = 5,    // a filled region (fill.h): its outline's points (the codec, x
                                  // and y), a closed polygon, even-odd, in its colour — drawn just
                                  // under the stroke it fills
    FUDE_ZOOM_KIND_MARK   = 6     // a bookmark (page.h's Places): the view at its translation, its
                                  // payload f64 zoom, u32 number — never drawn; an object so that
                                  // making and letting one go are undo steps, kept as any
} FUDE_ZOOM_KIND_;

#define FUDE_ZOOM_FLAG_ALIVE     0x01u
#define FUDE_ZOOM_FLAG_CONTINUES 0x02u   // a piece of the stroke before it (same gesture)
#define FUDE_ZOOM_FLAG_FROM_PEN  0x04u
#define FUDE_ZOOM_FLAG_MARKER    0x08u   // the marker's: see-through, one width
#define FUDE_ZOOM_FLAG_PRESSURE  0x10u   // its width varies: the pressure channel is each point's share of the half-width
#define FUDE_ZOOM_FLAG_FILLED    0x20u   // a shape: filled with its colour...
#define FUDE_ZOOM_FLAG_FILL_OWN  0x40u   // ...or, with this too, with its fill colour (the Fill tool's)
#define FUDE_ZOOM_FLAG_GARBAGE   0x80u   // dead for good: its points let go (never saved)


typedef struct {
    fude_zoom_id  id;
    fude_zoom_id  gesture;    // the id of the gesture's first piece (itself, for one piece)
    u32           frame;      // the frame it is in (a slot of scene.frames)
    u32           child;      // FRAME: the frame it is (a slot); else FUDE_ZOOM_NONE
    u32           blob;       // its points' bytes (a handle; 0: none)
    u32           count;      // points
    u32           file_pos;   // its place in its frame's arrival order (the file's buckets)
    u32           history;    // undo steps that name it
    u64           z;          // order key: higher is drawn later
    fude_zoom_v2  t;          // translation: its first point, frame units
    fude_zoom_box box;        // what it covers, frame units (for a FRAME: its anchor)
    rde_color     color;
    rde_color     fill;       // a filled shape's fill when FUDE_ZOOM_FLAG_FILL_OWN (the theme's ink is 0 0 0 0)
    f32           radius;     // base half-width, frame units
    u16           layer;
    u8            kind;       // FUDE_ZOOM_KIND_
    u8            flags;      // FUDE_ZOOM_FLAG_
    u8            channels;   // FUDE_ZOOM_CHANNEL_ (codec.h)
    i8            q;          // quantum: 2^q frame units
    f64           rotation;   // radians, round its translation (a stroke's points stay as they were drawn)
    f64           scale;      // times its points' size, round its translation (1: as drawn)
} fude_zoom_object;

// Where an object is, apart from its shape: what moving, scaling and turning it
// change — a stroke's translation, rotation and scale; a frame's (its FRAME
// object's) origin, rotation and scale in its parent (zoom.h: its xform).
typedef struct {
    fude_zoom_v2 t;
    f64          rotation;
    f64          scale;
} fude_zoom_place;

typedef struct {
    fude_zoom_id      id;
    u32               parent;     // slot; FUDE_ZOOM_NONE: the root
    u32               object;     // its FRAME object in the parent (FUDE_ZOOM_NONE: the root)
    fude_zoom_xform   xf;         // into the parent
    fude_zoom_box     anchor;     // parent units: its content and the view it was made for
    rde_arr TYPE(u32) order;      // its objects by z
    rde_arr TYPE(u32) kids;       // its FRAME objects (the frames in it), as they came
    rde_arr TYPE(u32) arrival;    // its objects as they came (file_pos indexes this)
    rde_arr TYPE(u8)  dirty;      // a bucket each (FUDE_ZOOM_BUCKET objects): changed since the last checkpoint
    fude_zoom_index   index;
    u32               dead;       // dead objects in the index: past half, it is built again
    u32               stale;      // index entries an object moved away from (its new place is in too): queries take each object once
    u64               z_next;
    rde_arr TYPE(u64) offsets;    // where each bucket's latest copy is in the file (zfile.h)
    rde_arr TYPE(u32) sizes;      // ...and how big it is
    u32               visited;    // the camera's last time in it (scene.clock): the most recent wins
    b8                removed;    // made for a camera that left before anything was drawn
    b8                saved;      // the file has it (or the journal does): only frames drawn in are written
} fude_zoom_frame;

#define FUDE_ZOOM_BUCKET 512u

typedef struct {
    u8* data;
    u32 size;
    u32 refs;
} fude_zoom_blob;

typedef struct {
    u32          frame;
    fude_zoom_v2 at;        // frame units
    f64          z;         // pt a unit
} fude_zoom_camera;


typedef enum {
    FUDE_ZOOM_ACTION_BORN = 1,    // targets[first .. +count) were made
    FUDE_ZOOM_ACTION_DIED,        // targets[first .. +count) were erased
    FUDE_ZOOM_ACTION_REPLACED,    // targets[first .. +count) erased, targets[born_first .. +born) made from them
    FUDE_ZOOM_ACTION_MOVED        // targets[moved_first .. +moved) moved: places[place_first ..] before and after
} FUDE_ZOOM_ACTION_;

typedef struct {
    u8            type;
    u32           frame;      // where it happened, for the camera
    fude_zoom_box box;        // ...that frame's units
    u32           first, count;
    u32           born_first, born;
    u32           moved_first, moved;
    u32           place_first;  // into scene.places: two each (before, after)
    u32           bytes;      // what it keeps alive (the history's budget)
} fude_zoom_action;

// id → slot, for what the file names by id.
typedef struct {
    fude_zoom_id* keys;
    u32*          values;
    u32           capacity;   // a power of two
    u32           count;
} fude_zoom_map;

typedef struct {
    rde_arr TYPE(fude_zoom_frame)  frames;
    rde_arr TYPE(fude_zoom_object) objects;
    rde_arr TYPE(fude_zoom_blob)   blobs;
    rde_arr TYPE(u32)              free_blobs;
    fude_zoom_map                  object_ids;
    fude_zoom_map                  frame_ids;
    u32                            root;
    u32                            home;       // the canvas's own frame (its first root; a new root above
                                               // leaves it so): its zoom 1 is ×1, and Reset view's
    u32                            device;     // ids are (device << 32) | counter: unique across devices
    u32                            counter;
    fude_zoom_camera               camera;
    u32                            clock;      // goes up on each frame change of the camera

    rde_arr TYPE(fude_zoom_action) actions;
    u32                            action_count;
    rde_arr TYPE(u32)              targets;
    rde_arr TYPE(fude_zoom_place)  places;     // the MOVED actions' befores and afters, in pairs
    u64                            history_bytes;
    u64                            history_budget;

    fude_bytes                     journal;
    b8                             replaying;  // changes come from the file: not journaled
    u32                            revision;   // bumped by every change worth saving
} fude_zoom_scene;

// --- the scene ---------------------------------------------------------------------------

// An empty canvas: one root frame, the camera on it at zoom 1.
void fude_zoom_scene_init(fude_zoom_scene* _s, u32 _device);
void fude_zoom_scene_destroy(fude_zoom_scene* _s);

fude_zoom_id fude_zoom_scene_new_id(fude_zoom_scene* _s);

fude_zoom_frame*  fude_zoom_scene_frame(const fude_zoom_scene* _s, u32 _slot);
fude_zoom_object* fude_zoom_scene_object(const fude_zoom_scene* _s, u32 _index);
u32               fude_zoom_scene_frame_count(const fude_zoom_scene* _s);
u32               fude_zoom_scene_object_count(const fude_zoom_scene* _s);
u32               fude_zoom_scene_find_object(const fude_zoom_scene* _s, fude_zoom_id _id);   // FUDE_ZOOM_NONE: none
u32               fude_zoom_scene_find_frame(const fude_zoom_scene* _s, fude_zoom_id _id);
// How many parents up the root is (the root: 0).
u32               fude_zoom_scene_depth(const fude_zoom_scene* _s, u32 _frame);
// Alive strokes and their points, the whole canvas (for the HUD and the gate).
u32               fude_zoom_scene_alive_strokes(const fude_zoom_scene* _s, u64* _points);

// --- frames ------------------------------------------------------------------------------

// A new frame in _parent at _xf, anchored on _anchor (parent units), drawn on
// top of what is there. Its slot.
u32  fude_zoom_scene_new_frame(fude_zoom_scene* _s, u32 _parent, fude_zoom_xform _xf, fude_zoom_box _anchor);
// A new root above the current one, which becomes its child at its origin.
u32  fude_zoom_scene_new_root(fude_zoom_scene* _s);
// From frame _from's units to frame _to's, through their nearest common frame.
fude_zoom_sim fude_zoom_scene_sim(const fude_zoom_scene* _s, u32 _from, u32 _to);
// Does the frame hold anything worth keeping (alive or undoable objects, frames)?
b8   fude_zoom_scene_frame_used(const fude_zoom_scene* _s, u32 _frame);
// A frame's anchor grown to hold _box (its own units) — content drawn past it.
void fude_zoom_scene_frame_reach(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box);

// --- objects -----------------------------------------------------------------------------

// A stroke in _frame from _count points (their first at 0, 0), alive, on top
// (_z 0) or at order key _z; _gesture 0: its own. Its index. Not an undo step
// by itself: the caller records the gesture (fude_zoom_history_push).
u32  fude_zoom_scene_add_stroke(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _t, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                                u8 _channels, rde_color _color, f32 _radius, u8 _flags, fude_zoom_id _gesture, u64 _z);
// A shape (shape.h) in _frame at _place: its type and its _count numbers.
// Its index. Not an undo step by itself.
u32  fude_zoom_scene_add_shape(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, u8 _type, const f64* _numbers, u32 _count,
                               rde_color _color, f32 _radius, u8 _flags, u64 _z);
// The same, filled with its own colour _fill (FUDE_ZOOM_FLAG_FILL_OWN: the Fill tool's).
u32  fude_zoom_scene_add_shape_fill(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, u8 _type, const f64* _numbers, u32 _count,
                                    rde_color _color, f32 _radius, u8 _flags, rde_color _fill, u64 _z);
// A filled region (fill.h) in _frame at _place: its outline's _count points
// (their first at 0, 0; the last joins the first) — each point's ring in its
// time (0: the outline; the eraser's cuts after it) — in _color, at order key
// _z (0: on top). Its index. Not an undo step by itself.
u32  fude_zoom_scene_add_fill(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                              rde_color _color, u64 _z);
// The same with flags kept (FUDE_ZOOM_FLAG_MARKER: drawn with the markers, under the ink).
u32  fude_zoom_scene_add_fill_flags(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                                    rde_color _color, u8 _flags, u64 _z);
// A shape's numbers into _out (at most _max). How many (0: not a shape, or damaged).
u32  fude_zoom_scene_shape_numbers(const fude_zoom_scene* _s, u32 _object, f64* _out, u32 _max);
// A shape's outline where it is, its frame's units, into _out (fude_zoom_v2).
void fude_zoom_scene_shape_outline(const fude_zoom_scene* _s, u32 _object, u32 _segments, rde_arr* _out, b8* _closed);

// A picture in _frame at _place, _hw by _hh each way of its middle (frame units),
// from its file's bytes (JPEG or PNG: _bytes, _size). Its index. Not an undo
// step by itself.
u32  fude_zoom_scene_add_image(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, f64 _hw, f64 _hh, const u8* _bytes, u32 _size, u64 _z);
// A picture's half-size and its file's bytes (in the scene's memory: valid
// until the object changes); any of them NULL when not wanted. False: not a
// picture.
b8   fude_zoom_scene_image(const fude_zoom_scene* _s, u32 _object, f64* _hw, f64* _hh, const u8** _bytes, u32* _size);
// A picture's corners where it is, its frame's units (bottom-left, bottom-right,
// top-right, top-left of the picture as it stands).
void fude_zoom_scene_image_corners(const fude_zoom_scene* _s, u32 _object, fude_zoom_v2 _out[4]);

// The same, rotated and scaled round its translation (_place).
u32  fude_zoom_scene_add_stroke_at(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                                   u8 _channels, rde_color _color, f32 _radius, u8 _flags, fude_zoom_id _gesture, u64 _z);
void fude_zoom_scene_set_alive(fude_zoom_scene* _s, u32 _object, b8 _alive);
// Dead for good, now: an object made and unmade inside one gesture (a piece an
// eraser cut, cut again before it lifted) — no undo step will ever name it.
void fude_zoom_scene_discard(fude_zoom_scene* _s, u32 _object);
// Where an object is (a FRAME object: its frame's xform), and moving it there:
// its box, its frame's index, the journal follow.
fude_zoom_place fude_zoom_scene_place_of(const fude_zoom_scene* _s, u32 _object);
void            fude_zoom_scene_set_place(fude_zoom_scene* _s, u32 _object, fude_zoom_place _p);
// A place moved by _m (a similarity in the object's frame's units): m ∘ place.
fude_zoom_place fude_zoom_place_moved(fude_zoom_place _p, fude_zoom_sim _m);
// An object's own similarity: its points (frame units from its translation,
// unscaled) → its frame's units.
fude_zoom_sim   fude_zoom_object_sim(const fude_zoom_object* _o);
// A copy of an object into _frame at _place, alive, on top (its points shared,
// not copied). Its index. Not an undo step by itself.
u32             fude_zoom_scene_copy_stroke(fude_zoom_scene* _s, u32 _object, u32 _frame, fude_zoom_place _place);

// A stroke's points, decoded into _out (object->count of them). False: damaged.
b8   fude_zoom_scene_points(const fude_zoom_scene* _s, u32 _object, fude_zoom_qpoint* _out);
// A point of a stroke, frame units.
fude_zoom_v2 fude_zoom_scene_point_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p);
// A point's half-width, frame units — and as drawn, before the object's scale.
f32  fude_zoom_scene_radius_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p);
f32  fude_zoom_scene_local_radius_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p);
// A point as drawn, before the object's rotation and scale: units from its translation.
fude_zoom_v2 fude_zoom_scene_local_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p);
// The objects of _frame whose boxes overlap _box (its units), alive ones only,
// appended to _out (u32 indices).
void fude_zoom_scene_query(const fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, rde_arr* _out);

// A new frame's origin (parent units) for scale _scale: _at, on the paper's lattice.
fude_zoom_v2 fude_zoom_paper_snap(fude_zoom_v2 _at, f64 _scale);

// The quantum (2^q) for drawing at zoom _z: about 1/16 pt.
i8   fude_zoom_quantum_for(f64 _z);

// --- the camera --------------------------------------------------------------------------

// A screen point (app space, centre origin, Y up) in the camera frame's units.
fude_zoom_v2 fude_zoom_camera_to_frame(const fude_zoom_camera* _c, fude_zoom_v2 _screen);
fude_zoom_v2 fude_zoom_camera_to_screen(const fude_zoom_camera* _c, fude_zoom_v2 _frame_point);
// Zoom by _factor round screen point _screen; pan by _delta (screen units).
void fude_zoom_camera_zoom_at(fude_zoom_scene* _s, fude_zoom_v2 _screen, f64 _factor);
void fude_zoom_camera_pan(fude_zoom_scene* _s, fude_zoom_v2 _delta);
// The camera kept in range: into a child or up to the parent as the zoom says,
// handed to the frame under the view at its depth. _half: half the screen.
void fude_zoom_camera_settle(fude_zoom_scene* _s, fude_zoom_v2 _half);
// The frame new strokes go in, for a stroke starting with box _box (camera
// frame units): the camera's, unless something drawn LATER in a parent covers
// it — then a new frame on top, which the camera moves into.
u32  fude_zoom_camera_drawing_frame(fude_zoom_scene* _s, fude_zoom_box _box);
// The camera at frame _frame's point _at, zoom _z (the history flying to a change).
void fude_zoom_camera_look_at(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _at, f64 _z);
// The camera's frame → the screen (centre origin, Y up).
fude_zoom_sim fude_zoom_camera_sim(const fude_zoom_camera* _c);

// --- bookmarks ---------------------------------------------------------------------------

// A bookmark (a MARK object) of the view at _frame's point _at, zoom _z,
// numbered _number. Its index. Not an undo step by itself.
u32  fude_zoom_scene_add_mark(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _at, f64 _z, u32 _number);
// A bookmark's zoom and number. False: not a bookmark.
b8   fude_zoom_scene_mark_of(const fude_zoom_scene* _s, u32 _object, f64* _z, u32* _number);

// --- the history -------------------------------------------------------------------------

// One gesture done: what it made (_born) and what it erased (_died) — their
// flags already set by the caller. Drops whatever was undone.
void fude_zoom_history_push(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _died, u32 _died_count, const u32* _born, u32 _born_count);
b8   fude_zoom_history_undo(fude_zoom_scene* _s);
b8   fude_zoom_history_redo(fude_zoom_scene* _s);
// Things moved (scaled, turned): from _before to _after, already where _after says.
void fude_zoom_history_push_moved(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _objects, const fude_zoom_place* _before, const fude_zoom_place* _after, u32 _count);
b8   fude_zoom_history_can_undo(const fude_zoom_scene* _s);
b8   fude_zoom_history_can_redo(const fude_zoom_scene* _s);
// The action undo or redo would step over next (NULL: none): where to look.
const fude_zoom_action* fude_zoom_history_next_undo(const fude_zoom_scene* _s);
const fude_zoom_action* fude_zoom_history_next_redo(const fude_zoom_scene* _s);

// --- the journal (zfile.h reads and writes it) -----------------------------------------------

typedef enum {
    FUDE_ZOOM_OP_FRAME = 1,   // a frame's record, new or changed
    FUDE_ZOOM_OP_OBJECT,      // an object's record and its points
    FUDE_ZOOM_OP_ALIVE,       // u64 id, u8 alive
    FUDE_ZOOM_OP_PUSH,        // an action pushed (the undone ones dropped first)
    FUDE_ZOOM_OP_COUNT,       // u32 action_count (an undo or redo)
    FUDE_ZOOM_OP_DROP,        // u32 n: the oldest n actions dropped
    FUDE_ZOOM_OP_ROOT,        // u64 the root frame's id (a new root)
    FUDE_ZOOM_OP_PLACE,       // u64 a stroke's id, f64 tx ty rotation scale (a frame's place goes as its record)
} FUDE_ZOOM_OP_;

// The records as the file has them (and the journal's ops).
void fude_zoom_put_frame(const fude_zoom_scene* _s, fude_bytes* _b, u32 _frame);
void fude_zoom_put_object(const fude_zoom_scene* _s, fude_bytes* _b, u32 _object);
void fude_zoom_put_action(const fude_zoom_scene* _s, fude_bytes* _b, u32 _action);
// One record read and applied: a frame (new or replacing the one with its id),
// an object, an action appended. False: the bytes are not one.
b8   fude_zoom_get_frame(fude_zoom_scene* _s, fude_reader* _r);
b8   fude_zoom_get_object(fude_zoom_scene* _s, fude_reader* _r);
b8   fude_zoom_get_action(fude_zoom_scene* _s, fude_reader* _r);
// The ops of a journal chunk, applied in order. False at the first that is not one.
b8   fude_zoom_apply_ops(fude_zoom_scene* _s, fude_reader* _r);
// After loading: each frame's index packed, garbage let go, counts recomputed.
void fude_zoom_scene_loaded(fude_zoom_scene* _s);

void fude_put_u64(fude_bytes* _b, u64 _v);
void fude_put_f64(fude_bytes* _b, f64 _v);
u64  fude_get_u64(fude_reader* _r);
f64  fude_get_f64(fude_reader* _r);

#endif
