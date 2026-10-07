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
    FUDE_ZOOM_KIND_MARK   = 6,    // a bookmark (page.h's Places): the view at its translation, its
                                  // payload f64 zoom, u32 number — never drawn; an object so that
                                  // making and letting one go are undo steps, kept as any
    FUDE_ZOOM_KIND_TEXT   = 7,    // a text box or a sticky note: its payload f64 size (a line's
                                  // letters, frame units), f64 width, f64 height (its box, from its
                                  // translation at its top left, down and right, before its scale),
                                  // u32 style (FUDE_ZOOM_TEXT_), then its words (UTF-8); in its
                                  // colour, a note on its fill colour
    FUDE_ZOOM_KIND_LAYER  = 8     // a layer's name and state (as a bookmark, never drawn; the latest
                                  // alive one for an index wins): its payload u16 index, u8 flags
                                  // (FUDE_ZOOM_LAYER_), then its name (UTF-8)
} FUDE_ZOOM_KIND_;

// Layers (research §2.11) are the document's, not a region's: every object's
// `layer` is an index, and a hidden layer's things are drawn nowhere and
// touched by no tool at any depth, a locked one's drawn but not touched. Up to
// FUDE_ZOOM_LAYERS of them; layer 0 is there from the start, unnamed.
#define FUDE_ZOOM_LAYERS 16u
typedef enum {
    FUDE_ZOOM_LAYER_HIDDEN = 1,
    FUDE_ZOOM_LAYER_LOCKED = 2,
    // Its place in the pile (what of it is drawn over what of the others, in a
    // frame), in bits 2 to 6 as one more than its rank, the highest on top; 0:
    // never moved — its index (a file from before has its layers in that order).
    FUDE_ZOOM_LAYER_RANK   = 0x7C
} FUDE_ZOOM_LAYER_;

typedef enum {
    FUDE_ZOOM_TEXT_PLAIN = 0,
    FUDE_ZOOM_TEXT_STICKY
} FUDE_ZOOM_TEXT_;

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
    i8            q;          // quantum: 2^q frame units (a shape: its line's style, shape.h's FUDE_ZOOM_LINE_)
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
    rde_arr TYPE(u8) data;
    u32              refs;
} fude_zoom_blob;

// A blob's bytes, how many.
static inline u8*  fude_zoom_blob_bytes(const fude_zoom_blob* _b) { return _b->data.memory; }
static inline u32  fude_zoom_blob_size(const fude_zoom_blob* _b)  { return _b->data.count; }

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

typedef struct {
    rde_arr TYPE(fude_zoom_frame)  frames;
    rde_arr TYPE(fude_zoom_object) objects;
    rde_arr TYPE(fude_zoom_blob)   blobs;
    rde_arr TYPE(u32)              free_blobs;
    rde_hash_map TYPE(fude_zoom_id, u32) object_ids;   // id → slot, for what the file names by id
    rde_hash_map TYPE(fude_zoom_id, u32) frame_ids;
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
    u64                            history_pushes;    // steps ever pushed (this session's count: a new one ends any redo)
    u64                            history_dropped;   // ...and dropped from the oldest end (a step's place in all of them: this + its index)

    fude_bytes                     journal;
    b8                             replaying;  // changes come from the file: not journaled
    u32                            revision;   // bumped by every change worth saving

    // Layers: the one new things go on (pieces an eraser cuts keep their
    // own's), and those hidden and locked — a bit each, from the LAYER objects
    // (fude_zoom_scene_layers_refresh).
    u16                            layer;
    u32                            layers_hidden;
    u32                            layers_locked;
    u8                             layer_rank[FUDE_ZOOM_LAYERS];   // each layer's place in the pile (0 the bottom)
    // The line style new shapes are drawn in (shape.h's FUDE_ZOOM_LINE_: kept in a shape's q, which a shape has no
    // other use for), as the layer: set round making one (a shape made again: its own).
    i8                             style;
} fude_zoom_scene;

#define FUDE_ZOOM_LAYER_RANK_SHIFT 2u
// A layer's rank as its flags keep it (FUDE_ZOOM_LAYER_RANK; -1: never moved) and the flags with another.
static inline i32 fude_zoom_layer_rank_of(u8 _flags) {
    const u32 _r = ((u32)_flags & FUDE_ZOOM_LAYER_RANK) >> FUDE_ZOOM_LAYER_RANK_SHIFT;
    return _r > 0u ? (i32)_r - 1 : -1;
}
static inline u8 fude_zoom_layer_with_rank(u8 _flags, u32 _rank) {
    return (u8)((_flags & ~FUDE_ZOOM_LAYER_RANK) | (((_rank + 1u) << FUDE_ZOOM_LAYER_RANK_SHIFT) & FUDE_ZOOM_LAYER_RANK));
}

// Hidden: not drawn, not touched. Touchable: neither hidden nor locked. A
// frame is on no layer (what is in it is on theirs): never hidden by one.
static inline b8 fude_zoom_scene_hides(const fude_zoom_scene* _s, const fude_zoom_object* _o) {
    return _o->kind != FUDE_ZOOM_KIND_FRAME && _o->layer < 32u && ((_s->layers_hidden >> _o->layer) & 1u) != 0;
}
static inline b8 fude_zoom_scene_touchable(const fude_zoom_scene* _s, const fude_zoom_object* _o) {
    return _o->kind == FUDE_ZOOM_KIND_FRAME || _o->layer >= 32u || (((_s->layers_hidden | _s->layers_locked) >> _o->layer) & 1u) == 0;
}
// What draws it over what in a frame: its layer's rank, then when it was put there.
static inline u64 fude_zoom_scene_draw_key(const fude_zoom_scene* _s, const fude_zoom_object* _o) {
    const u64 _rank = _o->layer < FUDE_ZOOM_LAYERS ? (u64)_s->layer_rank[_o->layer] : 0u;
    return (_rank << 56) | (_o->z & 0x00FFFFFFFFFFFFFFull);
}

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

// An object's id kept in a shape's numbers exactly (a wire's ends', a constraint's lines'): its two halves, each a
// whole number a double holds as it is (an id as one number loses its last bits once a device's half is large).
static inline void fude_zoom_id_put(f64* _n, fude_zoom_id _id) {
    _n[0] = (f64)(u32)(_id >> 32);
    _n[1] = (f64)(u32)(_id & 0xFFFFFFFFu);
}
static inline fude_zoom_id fude_zoom_id_get(const f64* _n) {
    return _n[0] >= 0.0 && _n[1] >= 0.0 ? (((fude_zoom_id)(u32)_n[0]) << 32) | (fude_zoom_id)(u32)_n[1] : 0u;
}
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
// Whether what is in _frame shows at all: neither it nor any frame round it
// removed, nor any of their FRAME objects on a hidden layer.
b8   fude_zoom_scene_frame_shown(const fude_zoom_scene* _s, u32 _frame);
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
// The same with flags kept (FUDE_ZOOM_FLAG_MARKER: a marker's, see-through).
u32  fude_zoom_scene_add_fill_flags(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                                    rde_color _color, u8 _flags, u64 _z);
// A shape's numbers into _out (at most _max). How many (0: not a shape, or damaged).
u32  fude_zoom_scene_shape_numbers(const fude_zoom_scene* _s, u32 _object, f64* _out, u32 _max);
// All a shape's numbers, however many, into _out (f64s; cleared first). How many.
u32  fude_zoom_scene_shape_numbers_all(const fude_zoom_scene* _s, u32 _object, rde_arr* _out);
// A shape's outline where it is, its frame's units, into _out (fude_zoom_v2).
void fude_zoom_scene_shape_outline(const fude_zoom_scene* _s, u32 _object, u32 _segments, rde_arr* _out, b8* _closed);
// Any drawn thing's outline in its frame (fude_zoom_v2s into _out): a shape's (its own, its curves in _segments), a
// fill's (its outline ring), a stroke's line — closed when it closes (a stroke: its ends within a few of its widths).
// False: it has none (a measure, a connector, a guide, an attribute, a text, a picture; or it is gone).
b8   fude_zoom_scene_object_outline(const fude_zoom_scene* _s, u32 _object, u32 _segments, rde_arr* _out, b8* _closed);

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
// A text (kind TEXT): _size its letters' height, _w by _h its box (the page
// lays it out and measures it), _style FUDE_ZOOM_TEXT_, _fill a note's paper.
u32  fude_zoom_scene_add_text(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, f64 _size, f64 _w, f64 _h, u8 _style,
                              rde_color _color, rde_color _fill, const c8* _text, u32 _len, u64 _z);
// ...its numbers and its words (not NUL-ended: _len bytes). False: not a text.
b8   fude_zoom_scene_text(const fude_zoom_scene* _s, u32 _object, f64* _size, f64* _w, f64* _h, u8* _style, const c8** _text, u32* _len);
// ...and its box's corners in its frame (its top left first, then round counter-clockwise: bottom left...).
void fude_zoom_scene_text_corners(const fude_zoom_scene* _s, u32 _object, fude_zoom_v2 _out[4]);
// A layer's state as an object (kind LAYER, in _frame: the home frame's), its index, flags and name.
u32  fude_zoom_scene_add_layer(fude_zoom_scene* _s, u32 _frame, u16 _index, u8 _flags, const c8* _name);
b8   fude_zoom_scene_layer_of(const fude_zoom_scene* _s, u32 _object, u16* _index, u8* _flags, c8* _name, usize _size);
// The hidden and locked bits worked out again from the layers' objects.
void fude_zoom_scene_layers_refresh(fude_zoom_scene* _s);

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
// A copy of an object where it is, in its order, on _layer: its own gesture
// (_gesture 0) or a later piece of _gesture (the copy of its first piece's id).
// Not an undo step by itself (the caller lets the original go, in one step).
u32             fude_zoom_scene_copy_to_layer(fude_zoom_scene* _s, u32 _object, u16 _layer, fude_zoom_id _gesture);

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

// The file to keep frame _frame (and those it is in) though nothing is drawn in it — the
// camera is there, an instrument lies there: they are named by it (journaled now, if not yet).
void fude_zoom_scene_keep_frame(fude_zoom_scene* _s, u32 _frame);

// --- the history -------------------------------------------------------------------------

// One gesture done: what it made (_born) and what it erased (_died) — their
// flags already set by the caller. Drops whatever was undone.
void fude_zoom_history_push(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _died, u32 _died_count, const u32* _born, u32 _born_count);
b8   fude_zoom_history_undo(fude_zoom_scene* _s);
b8   fude_zoom_history_redo(fude_zoom_scene* _s);
// ...both in one step: _died let go, _born made, _moved moved (one undo undoes all of it).
void fude_zoom_history_push_all(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _died, u32 _died_count, const u32* _born, u32 _born_count,
                                const u32* _moved, const fude_zoom_place* _before, const fude_zoom_place* _after, u32 _moved_count);
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
