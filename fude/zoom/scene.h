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
// Scene: a canvas in memory: its frames, the objects in them, the camera, the undo history and the journal of changes.
// It knows nothing of the screen beyond the camera's numbers (render.h draws it).
//
// Where it sits: fude/zoom, over zoom.h, codec.h, index.h and shape.h. Almost every zoom module works on a scene: the
// page (page.h) owns the open canvas's, zfile.h loads and saves it, render.h draws it, and the tools (erase.h,
// select.h, fill.h, connect.h...) change it.
// Its main types and their lifecycle: a fude_zoom_scene is made with fude_zoom_scene_init and freed with
// fude_zoom_scene_destroy; it owns its frames, objects and payloads. Objects and frames are referred to by their index
// (a u32 slot), which never changes; a pointer from fude_zoom_scene_object or fude_zoom_scene_frame stays valid only
// until the next object or frame is added, because the tables may move.
// Threads: main thread only. Nothing here locks; a second scene (Find reading another canvas) is also used on the main
// thread.
// Files: the records and journal ops are written by fude_zoom_put_* and read by fude_zoom_get_* (their layout is in
// scene.c, above fude_zoom_put_frame); zfile.h puts them in the canvas's file.
// Design (docs/infinite_canvas_design.md §2, §3 and §6):
//   - FRAMES (zoom.h: the precision mechanism, never shown) nest. Each holds its objects, and a child frame is one of
//     them too (kind FRAME), so it has a place in its parent's order like a stroke: what is drawn LATER IS ON TOP, at
//     any depth. The root is the frame with no parent; zooming out past it makes a new one above it
//     (fude_zoom_camera_settle), so neither way has an end.
//   - OBJECTS live in one table for the whole canvas, in the order they were made. A frame lists its own, by order key
//     (z) for drawing and by arrival for the file's buckets. NOTHING IS DELETED BY AN EDIT (Kana's rule): erasing
//     marks objects dead, so the undo history only flips flags. An object becomes GARBAGE once it is dead and no undo
//     step can bring it back: its points are let go then, and the file leaves it out.
//   - THE HISTORY is Kana's log: actions[0 .. action_count) are applied, and the rest were undone and can be redone
//     until a new edit drops them. One gesture is one action (a stroke, an eraser sweep). The history is bounded by
//     the bytes it keeps alive, not by a number of steps.
//   - THE JOURNAL: every change is also written, as it happens, into `journal` (ops: zfile.h), which the file appends
//     every second or so, so every stroke is on disk within about a second. Changes that come FROM the file are not
//     journaled.
// ===========================================================================

// No object, frame or index.
#define FUDE_ZOOM_NONE         0xFFFFFFFFu

// A child frame's unit in its parent's units, and the camera's zoom range in a frame (in points per unit). Past the
// top the camera goes into a child (its z becomes 1/16); past the bottom it goes up to the parent (its z becomes 16).
// The gap between the two is the hysteresis, so zooming back and forth at a threshold does not flip frames.
#define FUDE_ZOOM_CHILD_SCALE  (1.0 / 1024.0)
#define FUDE_ZOOM_Z_MAX        64.0
#define FUDE_ZOOM_Z_MIN        (1.0 / 64.0)

// The paper's lattice (drawn 50 · 2^n units apart, at any zoom) runs on unbroken through frames: a new frame's origin
// is a whole multiple of this many of its own units (in its parent's units, this times its scale), the coarsest
// lattice the paper ever shows inside it.
#define FUDE_ZOOM_PAPER_ALIGN  6400.0

// The most points one piece of a stroke holds. A longer stroke is cut into pieces of one gesture
// (FUDE_ZOOM_FLAG_CONTINUES), which keeps their boxes tight and their saves small.
#define FUDE_ZOOM_STROKE_MAX   512u

// Order keys are spaced this far apart as objects arrive, so pieces cut from a stroke fit right after it (an erase
// keeps the place the stroke had in the order).
#define FUDE_ZOOM_Z_STEP       (1ull << 32)

// The most bytes the undo history keeps alive; past it, the oldest steps go.
#define FUDE_ZOOM_HISTORY_BUDGET (48u * 1024u * 1024u)

// What an object is. Each kind's payload (its blob) is described here.
typedef enum {
    // A stroke: its payload is its points, encoded by codec.h.
    FUDE_ZOOM_KIND_STROKE = 1,
    // A child frame, as an entry in its parent's order (no payload).
    FUDE_ZOOM_KIND_FRAME  = 2,
    FUDE_ZOOM_KIND_SHAPE  = 3,    // shape.h: its type in `channels`, its numbers (f64) in its payload
    FUDE_ZOOM_KIND_IMAGE  = 4,    // a picture: its payload is f64 half-width, f64 half-height (frame units, round
                                  // its translation, before its scale), then the picture's own bytes (JPEG or PNG)
    FUDE_ZOOM_KIND_FILL   = 5,    // a filled region (fill.h): its outline's points (codec.h, x and y), a closed
                                  // polygon filled even-odd in its colour, drawn just under the stroke it fills
    FUDE_ZOOM_KIND_MARK   = 6,    // a bookmark (page.h's Places): the view at its translation, its payload f64
                                  // zoom and u32 number. Never drawn; it is an object so that making and letting go
                                  // of one are undo steps, kept like any other
    FUDE_ZOOM_KIND_TEXT   = 7,    // a text box or a sticky note: its payload is f64 size (a line's letters, in frame
                                  // units), f64 width, f64 height (its box, from its translation at its top left,
                                  // down and right, before its scale), u32 style (FUDE_ZOOM_TEXT_), then its words
                                  // (UTF-8). Drawn in its colour; a note on its fill colour
    FUDE_ZOOM_KIND_LAYER  = 8     // a layer's name and state (never drawn, like a bookmark; the latest alive one for
                                  // an index wins): its payload is u16 index, u8 flags (FUDE_ZOOM_LAYER_), then its
                                  // name (UTF-8)
} FUDE_ZOOM_KIND_;

// Layers (docs/research/infinite_canvas_research.md §2.11) belong to the whole document, not to a region: every
// object's `layer` is an index. A hidden layer's things are drawn nowhere and touched by no tool at any depth; a
// locked layer's are drawn but not touched. There are up to FUDE_ZOOM_LAYERS of them; layer 0 is there from the
// start, unnamed.
#define FUDE_ZOOM_LAYERS 16u
// A layer's flags, as its LAYER object keeps them.
typedef enum {
    FUDE_ZOOM_LAYER_HIDDEN = 1,
    FUDE_ZOOM_LAYER_LOCKED = 2,
    // Its place in the pile (which of its things are drawn over which of the others', within a frame), kept in bits
    // 2 to 6 as one more than its rank, the highest on top. 0 means it was never moved, so its index is its rank (a
    // file from before ranks has its layers in that order).
    FUDE_ZOOM_LAYER_RANK   = 0x7C
} FUDE_ZOOM_LAYER_;

// A text object's style.
typedef enum {
    FUDE_ZOOM_TEXT_PLAIN = 0,
    FUDE_ZOOM_TEXT_STICKY,
    FUDE_ZOOM_TEXT_FORMULA     // its words are LaTeX, set and drawn as maths (formula.h); its size is an em
} FUDE_ZOOM_TEXT_;

// An object's flags (fude_zoom_object.flags).
#define FUDE_ZOOM_FLAG_ALIVE     0x01u
#define FUDE_ZOOM_FLAG_CONTINUES 0x02u   // a piece of the stroke before it (the same gesture)
// Written with a pen (not a finger or the mouse).
#define FUDE_ZOOM_FLAG_FROM_PEN  0x04u
#define FUDE_ZOOM_FLAG_MARKER    0x08u   // the marker's: see-through, one width
#define FUDE_ZOOM_FLAG_PRESSURE  0x10u   // its width varies: pressure is each point's share of the half-width
#define FUDE_ZOOM_FLAG_FILLED    0x20u   // a shape filled with its colour...
#define FUDE_ZOOM_FLAG_FILL_OWN  0x40u   // ...or, with this flag too, with its fill colour (the Fill tool's)
// Dead for good: its points have been let go. Its record is still written (with no points) until the file is compacted,
// which leaves it out (zfile.h).
#define FUDE_ZOOM_FLAG_GARBAGE   0x80u


// One object of the canvas: a stroke, a frame's place in its parent, a shape, a picture, a fill, a bookmark, a text or
// a layer's record.
typedef struct {
    fude_zoom_id  id;
    fude_zoom_id  gesture;    // the id of the gesture's first piece (its own id, for a single piece)
    u32           frame;      // the frame it is in (a slot of scene.frames)
    u32           child;      // for a FRAME, the frame it is (a slot); otherwise FUDE_ZOOM_NONE
    u32           blob;       // its payload's bytes (a handle; 0: none)
    u32           count;      // points (a stroke, a fill), numbers (a shape) or payload bytes (a picture, a text)
    u32           file_pos;   // its place in its frame's arrival order (which sets its bucket in the file)
    u32           history;    // how many undo steps name it
    u64           z;          // order key: higher is drawn later
    fude_zoom_v2  t;          // translation: its first point, in frame units
    fude_zoom_box box;        // what it covers, in frame units (for a FRAME, its anchor)
    rde_color     color;
    rde_color     fill;       // a filled shape's fill with FUDE_ZOOM_FLAG_FILL_OWN (the theme's ink is 0 0 0 0)
    f32           radius;     // base half-width, in frame units
    u16           layer;
    u8            kind;       // FUDE_ZOOM_KIND_
    u8            flags;      // FUDE_ZOOM_FLAG_
    u8            channels;   // FUDE_ZOOM_CHANNEL_ (codec.h)
    i8            q;          // quantum: points are kept in steps of 2^q frame units (a shape: its line's style,
                              // shape.h's FUDE_ZOOM_LINE_)
    f64           rotation;   // radians, round its translation (a stroke's points stay as they were drawn)
    f64           scale;      // how much bigger than drawn, round its translation (1: as drawn)
} fude_zoom_object;

// Where an object is, apart from its shape: what moving, scaling and turning it change. For a stroke, its
// translation, rotation and scale; for a FRAME object, its frame's origin, rotation and scale in its parent (zoom.h:
// its xform).
typedef struct {
    fude_zoom_v2 t;
    f64          rotation;
    f64          scale;
} fude_zoom_place;

// A frame: its place in its parent, its objects and its index, and where its buckets are in the file.
typedef struct {
    fude_zoom_id      id;
    u32               parent;     // a slot; FUDE_ZOOM_NONE for the root
    u32               object;     // its FRAME object in the parent (FUDE_ZOOM_NONE for the root)
    fude_zoom_xform   xf;         // its place in the parent
    fude_zoom_box     anchor;     // in parent units: its content and the view it was made for
    rde_arr TYPE(u32) order;      // its objects by z
    rde_arr TYPE(u32) kids;       // its FRAME objects (the frames in it), in the order they came
    rde_arr TYPE(u32) arrival;    // its objects in the order they came (file_pos indexes this)
    rde_arr TYPE(u8)  dirty;      // one per bucket (FUDE_ZOOM_BUCKET objects): changed since the last checkpoint
    fude_zoom_index   index;
    // Index entries of garbage objects. Once they and the stale ones below are more than 64 and more than half the
    // frame's arrival order, the index is built again (scene.c's fude_zoom_scene_reindex).
    u32               dead;       // garbage objects' index entries
    // Index entries an object has moved away from (its new place is in the index too); queries take each object once.
    u32               stale;      // index entries moved away from
    // The order key the next object put on top gets.
    u64               z_next;
    rde_arr TYPE(u64) offsets;    // where each bucket's latest copy is in the file (zfile.h)
    rde_arr TYPE(u32) sizes;      // ...and how big it is
    u32               visited;    // when the camera was last in it (scene.clock): the most recent wins
    b8                removed;    // made for a camera that left before anything was drawn, or deleted
    b8                saved;      // the file (or the journal) has it: only frames something was drawn in are written
} fude_zoom_frame;

// How many objects a bucket of a frame holds in the file.
#define FUDE_ZOOM_BUCKET 512u

// An object's payload: its bytes, shared by copies (refs counts them).
typedef struct {
    rde_arr TYPE(u8) data;
    u32              refs;
    // Which payload this is, of all the scene has made (fude_zoom_scene.blob_serials): a freed slot is given out
    // again, so what was kept for a slot (render.c's pictures) is matched by slot AND serial, never by slot alone.
    u32              serial;
} fude_zoom_blob;

// Return a blob's bytes and how many there are.
static inline u8*  fude_zoom_blob_bytes(const fude_zoom_blob* _b) { return _b->data.memory; }
static inline u32  fude_zoom_blob_size(const fude_zoom_blob* _b)  { return _b->data.count; }

// The camera: a frame, a point in it, and a zoom.
typedef struct {
    u32          frame;
    fude_zoom_v2 at;        // in frame units: the point in the middle of the screen
    f64          z;         // points on screen per frame unit
} fude_zoom_camera;


// What an undo step did, by the parts of scene.targets it names.
typedef enum {
    FUDE_ZOOM_ACTION_BORN = 1,    // targets[first .. +count) were made
    FUDE_ZOOM_ACTION_DIED,        // targets[first .. +count) were erased
    FUDE_ZOOM_ACTION_REPLACED,    // targets[first .. +count) erased, targets[born_first .. +born) made from them
    FUDE_ZOOM_ACTION_MOVED        // targets[moved_first .. +moved) moved: places[place_first ..] before and after
} FUDE_ZOOM_ACTION_;

// One undo step.
typedef struct {
    u8            type;
    u32           frame;      // where it happened, for the camera to fly to
    fude_zoom_box box;        // ...in that frame's units
    u32           first, count;
    u32           born_first, born;
    u32           moved_first, moved;
    u32           place_first;  // into scene.places: two for each moved object (before, after)
    u32           bytes;      // what it keeps alive (counted against the history's budget)
    u32           tag;        // the app's own, kept with it (page_file.c: a notebook's pages added or taken out);
                              // 0: none
} fude_zoom_action;

// A canvas.
typedef struct {
    rde_arr TYPE(fude_zoom_frame)  frames;
    rde_arr TYPE(fude_zoom_object) objects;
    rde_arr TYPE(fude_zoom_blob)   blobs;        // the payloads' pool: a handle is its slot + 1
    rde_stack TYPE(u32)            free_blobs;   // the slots freed, the last freed given out first
    u32                            blob_serials; // payloads made so far: the last one's serial (fude_zoom_blob)
    rde_hash_map TYPE(fude_zoom_id, u32) object_ids;   // from an id to its slot, for what the file names by id
    rde_hash_map TYPE(fude_zoom_id, u32) frame_ids;
    u32                            root;
    u32                            home;       // the canvas's own frame (its first root, which stays home when a new
                                               // root is made above it): its zoom 1 is ×1, and Reset view goes there
    u32                            device;     // ids are (device << 32) | counter, so they are unique across devices
    u32                            counter;
    fude_zoom_camera               camera;
    u32                            clock;      // goes up each time the camera changes frame

    rde_arr TYPE(fude_zoom_action) actions;
    u32                            action_count;
    rde_arr TYPE(u32)              targets;
    rde_arr TYPE(fude_zoom_place)  places;     // the MOVED actions' befores and afters, in pairs
    u64                            history_bytes;
    u64                            history_budget;
    // Steps ever pushed (this session's count: a new one ends any redo), and steps dropped from the oldest end (a
    // step's place among all of them is history_dropped plus its index).
    u64                            history_pushes;    // steps ever pushed (see above)
    u64                            history_dropped;   // steps dropped from the oldest end

    fude_bytes                     journal;
    b8                             replaying;  // the changes come from the file, so they are not journaled
    // A file is being read (zfile.c sets it; fude_zoom_scene_loaded clears it): objects go into their frames' orders
    // as they come, unsorted, and into no index, and fude_zoom_scene_loaded sorts each order and builds each index
    // once, at the end. A read that fails clears it too (zfile.c), though that scene is only fit to be destroyed: its
    // orders are left unsorted and its indexes unbuilt.
    b8                             loading;
    u32                            revision;   // goes up with every change worth saving

    // Layers: the one new things go on (pieces an eraser cuts keep their own's), and those hidden and locked, a bit
    // each, worked out from the LAYER objects (fude_zoom_scene_layers_refresh).
    u16                            layer;
    u32                            layers_hidden;
    u32                            layers_locked;
    u8                             layer_rank[FUDE_ZOOM_LAYERS];   // each layer's place in the pile (0 the bottom)
    // The line style new shapes are drawn in (shape.h's FUDE_ZOOM_LINE_, kept in a shape's q, which a shape has no
    // other use for). Like the layer, the caller sets it round making a shape (to a remade shape's own style) and
    // puts it back to 0.
    i8                             style;
} fude_zoom_scene;

// Where FUDE_ZOOM_LAYER_RANK starts in a layer's flags.
#define FUDE_ZOOM_LAYER_RANK_SHIFT 2u
// fude_zoom_layer_rank_of returns the rank a layer's flags keep (FUDE_ZOOM_LAYER_RANK), or -1 when it was never moved.
// fude_zoom_layer_with_rank returns the flags with rank _rank instead.
static inline i32 fude_zoom_layer_rank_of(u8 _flags) {
    const u32 _r = ((u32)_flags & FUDE_ZOOM_LAYER_RANK) >> FUDE_ZOOM_LAYER_RANK_SHIFT;
    return _r > 0u ? (i32)_r - 1 : -1;
}
static inline u8 fude_zoom_layer_with_rank(u8 _flags, u32 _rank) {
    return (u8)((_flags & ~FUDE_ZOOM_LAYER_RANK) | (((_rank + 1u) << FUDE_ZOOM_LAYER_RANK_SHIFT) & FUDE_ZOOM_LAYER_RANK));
}

// fude_zoom_scene_hides returns whether an object's layer is hidden (so it is neither drawn nor touched);
// fude_zoom_scene_touchable whether its layer is neither hidden nor locked. A frame is on no layer (the things in it
// are on theirs), so a layer never hides it.
static inline b8 fude_zoom_scene_hides(const fude_zoom_scene* _s, const fude_zoom_object* _o) {
    return _o->kind != FUDE_ZOOM_KIND_FRAME && _o->layer < 32u && ((_s->layers_hidden >> _o->layer) & 1u) != 0;
}
static inline b8 fude_zoom_scene_touchable(const fude_zoom_scene* _s, const fude_zoom_object* _o) {
    return _o->kind == FUDE_ZOOM_KIND_FRAME || _o->layer >= 32u || (((_s->layers_hidden | _s->layers_locked) >> _o->layer) & 1u) == 0;
}
// Returns the key that decides what is drawn over what within a frame: the object's layer's rank, then its order key.
// A frame inside it (its FRAME object) is on no layer, but its layer field is 0, so it takes layer 0's rank: all a
// frame holds is drawn at that one place among its parent's things, whatever layers it is on. A frame is drawn whole,
// in one place of its parent's order, so no one rank would be right for a frame holding several layers' things (only
// drawing each layer through every depth would be), and layer 0's keeps a canvas of one layer as it always was. The
// cost: something drawn deep on a layer ranked above layer 0 is drawn under its parent's things on such a layer where
// they cover it, even those drawn before it.
static inline u64 fude_zoom_scene_draw_key(const fude_zoom_scene* _s, const fude_zoom_object* _o) {
    const u64 _rank = _o->layer < FUDE_ZOOM_LAYERS ? (u64)_s->layer_rank[_o->layer] : 0u;
    return (_rank << 56) | (_o->z & 0x00FFFFFFFFFFFFFFull);
}

// --- the scene ---------------------------------------------------------------------------

// fude_zoom_scene_init makes an empty canvas: one root frame (also home), the camera on it at zoom 1. _device makes
// the ids this device gives unique among devices. fude_zoom_scene_destroy frees everything the scene holds.
void fude_zoom_scene_init(fude_zoom_scene* _s, u32 _device);
void fude_zoom_scene_destroy(fude_zoom_scene* _s);

// Returns a new id: (device << 32) | the next counter.
fude_zoom_id fude_zoom_scene_new_id(fude_zoom_scene* _s);

// Return a frame by its slot or an object by its index (not checked: they must exist), how many of each there are,
// and an object's index by its id (FUDE_ZOOM_NONE when there is none). A pointer stays valid until the next frame or
// object is added.
fude_zoom_frame*  fude_zoom_scene_frame(const fude_zoom_scene* _s, u32 _slot);
fude_zoom_object* fude_zoom_scene_object(const fude_zoom_scene* _s, u32 _index);
u32               fude_zoom_scene_frame_count(const fude_zoom_scene* _s);
u32               fude_zoom_scene_object_count(const fude_zoom_scene* _s);
u32               fude_zoom_scene_find_object(const fude_zoom_scene* _s, fude_zoom_id _id);   // FUDE_ZOOM_NONE: none

// Keep an object's id exactly in two of a shape's numbers (a wire's ends, a constraint's lines): fude_zoom_id_put
// writes its two halves into _n[0] and _n[1], each a whole number a double holds exactly, and fude_zoom_id_get reads
// them back (0 when either is not a number a u32 holds: negative, too big, not a number, as a damaged file's may be;
// casting such a one would be undefined). One double would lose the id's last bits once the device half is large.
static inline void fude_zoom_id_put(f64* _n, fude_zoom_id _id) {
    _n[0] = (f64)(u32)(_id >> 32);
    _n[1] = (f64)(u32)(_id & 0xFFFFFFFFu);
}
static inline fude_zoom_id fude_zoom_id_get(const f64* _n) {
    return _n[0] >= 0.0 && _n[0] < 4294967296.0 && _n[1] >= 0.0 && _n[1] < 4294967296.0 ? (((fude_zoom_id)(u32)_n[0]) << 32) | (fude_zoom_id)(u32)_n[1] : 0u;
}
// Returns a frame's slot by its id, or FUDE_ZOOM_NONE when there is none.
u32               fude_zoom_scene_find_frame(const fude_zoom_scene* _s, fude_zoom_id _id);
// Returns how many parents up the root is from _frame (0 for the root itself).
u32               fude_zoom_scene_depth(const fude_zoom_scene* _s, u32 _frame);
// Returns how many strokes are alive in the whole canvas, and writes how many points they have into _points (which may
// be NULL). For the HUD and the performance gate.
u32               fude_zoom_scene_alive_strokes(const fude_zoom_scene* _s, u64* _points);

// --- frames ------------------------------------------------------------------------------

// Makes a new frame in _parent at _xf, anchored on _anchor (in parent units), drawn on top of what is there. Returns
// its slot. Nothing is journaled until something is drawn in it.
u32  fude_zoom_scene_new_frame(fude_zoom_scene* _s, u32 _parent, fude_zoom_xform _xf, fude_zoom_box _anchor);
// Makes a new root above the current one, which becomes its child at its origin, at FUDE_ZOOM_CHILD_SCALE. Returns the
// new root's slot.
u32  fude_zoom_scene_new_root(fude_zoom_scene* _s);
// Returns the transform from frame _from's units to frame _to's, worked out through their nearest common frame.
fude_zoom_sim fude_zoom_scene_sim(const fude_zoom_scene* _s, u32 _from, u32 _to);
// Returns whether the frame holds anything worth keeping: an object alive, or one dead that an undo can bring back
// (an alive frame counts; a dead one does not).
b8   fude_zoom_scene_frame_used(const fude_zoom_scene* _s, u32 _frame);
// Returns whether what is in _frame shows at all: true unless it or any frame round it is removed, or the FRAME object
// of any of them is dead or on a hidden layer.
b8   fude_zoom_scene_frame_shown(const fude_zoom_scene* _s, u32 _frame);
// Grows a frame's anchor to hold _box (in the frame's own units), for content drawn past it; its parents' anchors grow
// to hold it in turn. The root has no anchor and is left alone.
void fude_zoom_scene_frame_reach(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box);

// --- objects -----------------------------------------------------------------------------

// Makes a stroke in _frame at _t from _count points (relative to _t, the first at 0, 0), alive, on top (_z 0) or at
// order key _z. _gesture is the id of the gesture's first piece (0 starts a gesture of its own). Returns its index. It
// is not an undo step by itself: the caller records the gesture (fude_zoom_history_push).
u32  fude_zoom_scene_add_stroke(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _t, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                                u8 _channels, rde_color _color, f32 _radius, u8 _flags, fude_zoom_id _gesture, u64 _z);
// Makes a shape (shape.h) of type _type with its _count numbers in _frame at _place, in the scene's current style.
// Returns its index. It is not an undo step by itself.
u32  fude_zoom_scene_add_shape(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, u8 _type, const f64* _numbers, u32 _count,
                               rde_color _color, f32 _radius, u8 _flags, u64 _z);
// The same, filled with its own colour _fill when _flags has FUDE_ZOOM_FLAG_FILL_OWN (the Fill tool's).
u32  fude_zoom_scene_add_shape_fill(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, u8 _type, const f64* _numbers, u32 _count,
                                    rde_color _color, f32 _radius, u8 _flags, rde_color _fill, u64 _z);
// Makes a filled region (fill.h) in _frame at _place from its outline's _count points (the first at 0, 0; the last
// joins the first). Each point's ring is in its time channel (0 for the outline; the eraser's cuts come after it). It
// is drawn in _color, at order key _z (0: on top). Returns its index. It is not an undo step by itself.
u32  fude_zoom_scene_add_fill(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                              rde_color _color, u64 _z);
// The same, keeping FUDE_ZOOM_FLAG_MARKER from _flags (a marker's fill, see-through); other flags are dropped.
u32  fude_zoom_scene_add_fill_flags(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                                    rde_color _color, u8 _flags, u64 _z);
// Copies a shape's numbers into _out (at most _max). Returns how many it copied (0 when it is not a shape, or has no
// numbers).
u32  fude_zoom_scene_shape_numbers(const fude_zoom_scene* _s, u32 _object, f64* _out, u32 _max);
// Copies all of a shape's numbers, however many, into _out (an rde_arr of f64, cleared first). Returns how many.
u32  fude_zoom_scene_shape_numbers_all(const fude_zoom_scene* _s, u32 _object, rde_arr* _out);
// Writes a shape's outline where it is, in its frame's units, into _out (an rde_arr of fude_zoom_v2, cleared first),
// its curves in _segments; *_closed says whether the outline closes.
void fude_zoom_scene_shape_outline(const fude_zoom_scene* _s, u32 _object, u32 _segments, rde_arr* _out, b8* _closed);
// Writes any drawn thing's outline in its frame into _out (an rde_arr of fude_zoom_v2, cleared first): a shape's own
// (its curves in _segments), a fill's outline ring, or a stroke's line. *_closed says whether it closes (a stroke
// closes when its ends are within a few of its widths). Returns false when it has none: a measure, a connector, a
// guide, an attribute, a text, a picture, or something dead.
b8   fude_zoom_scene_object_outline(const fude_zoom_scene* _s, u32 _object, u32 _segments, rde_arr* _out, b8* _closed);

// Makes a picture in _frame at _place, reaching _hw and _hh each way from its middle (frame units), from its file's
// _size bytes at _bytes (JPEG or PNG; they are copied). Returns its index. It is not an undo step by itself.
u32  fude_zoom_scene_add_image(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, f64 _hw, f64 _hh, const u8* _bytes, u32 _size, u64 _z);
// Reads a picture's half-size and its file's bytes; pass NULL for any not wanted. The bytes are the scene's: they stay
// valid while the object exists and is not garbage. Returns false when it is not a picture.
b8   fude_zoom_scene_image(const fude_zoom_scene* _s, u32 _object, f64* _hw, f64* _hh, const u8** _bytes, u32* _size);
// Returns the serial of the payload with handle _handle (fude_zoom_blob: which payload a slot holds now), 0 for
// handle 0. What is kept for a payload elsewhere (render.c's pictures) matches it with the handle.
u32 fude_zoom_blob_serial(const fude_zoom_scene* _s, u32 _handle);
// Writes a picture's corners where it is, in its frame's units: bottom left, bottom right, top right, top left of the
// picture as it stands.
void fude_zoom_scene_image_corners(const fude_zoom_scene* _s, u32 _object, fude_zoom_v2 _out[4]);
// Makes a text (kind TEXT) in _frame at _place: _size is its letters' height, _w by _h its box (the page lays it out
// and measures it), _style a FUDE_ZOOM_TEXT_, _fill a sticky note's paper, and _text its _len bytes of words. Returns
// its index. It is not an undo step by itself.
u32  fude_zoom_scene_add_text(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, f64 _size, f64 _w, f64 _h, u8 _style,
                              rde_color _color, rde_color _fill, const c8* _text, u32 _len, u64 _z);
// Reads a text's numbers and its words (_len bytes, not NUL-terminated, valid while the object is). Pass NULL for any
// not wanted. Returns false when it is not a text.
b8   fude_zoom_scene_text(const fude_zoom_scene* _s, u32 _object, f64* _size, f64* _w, f64* _h, u8* _style, const c8** _text, u32* _len);
// Writes a text's box's corners in its frame: its top left first, then round anticlockwise (bottom left, bottom
// right, top right).
void fude_zoom_scene_text_corners(const fude_zoom_scene* _s, u32 _object, fude_zoom_v2 _out[4]);
// fude_zoom_scene_add_layer records a layer's state as an object (kind LAYER, in _frame, which is the home frame): its
// index, flags and name (up to 64 bytes are kept). It returns the object's index. fude_zoom_scene_layer_of reads them
// back (each output may be NULL; the name is cut to _size and NUL-terminated), returning false when it is not one.
u32  fude_zoom_scene_add_layer(fude_zoom_scene* _s, u32 _frame, u16 _index, u8 _flags, const c8* _name);
b8   fude_zoom_scene_layer_of(const fude_zoom_scene* _s, u32 _object, u16* _index, u8* _flags, c8* _name, usize _size);
// Works out the layers' hidden and locked bits and their ranks again from the LAYER objects.
void fude_zoom_scene_layers_refresh(fude_zoom_scene* _s);

// Like fude_zoom_scene_add_stroke, but placed at _place: rotated and scaled round its translation.
u32  fude_zoom_scene_add_stroke_at(fude_zoom_scene* _s, u32 _frame, fude_zoom_place _place, i8 _q, const fude_zoom_qpoint* _points, u32 _count,
                                   u8 _channels, rde_color _color, f32 _radius, u8 _flags, fude_zoom_id _gesture, u64 _z);
// Makes an object alive or dead (journaled); deleting a FRAME object removes its frame. Garbage stays dead.
void fude_zoom_scene_set_alive(fude_zoom_scene* _s, u32 _object, b8 _alive);
// Makes an object dead for good, now: for one made and unmade inside one gesture (a piece an eraser cut, cut again
// before the pen lifted), which no undo step will ever name.
void fude_zoom_scene_discard(fude_zoom_scene* _s, u32 _object);
// Clear (the toolbar's bin, with nothing lassoed): makes dead everything alive, at every depth, that a tool may touch,
// and adds each one's index to _died (an rde_arr of u32) for the caller's one undo step. It leaves the frames, the
// bookmarks and the layers' records (the canvas's places and its layers' names, eyes, locks and order are not what is
// drawn), and whatever a hidden or locked layer holds (fude_zoom_scene_touchable: a locked layer's things are not to
// be changed, and a hidden one's are not seen go). Returns how many it made dead.
u32  fude_zoom_scene_clear(fude_zoom_scene* _s, rde_arr* _died);
// fude_zoom_scene_place_of returns where an object is (for a FRAME object, its frame's xform).
// fude_zoom_scene_set_place moves it there, and its box, its frame's index and the journal follow. Neither is an undo
// step.
fude_zoom_place fude_zoom_scene_place_of(const fude_zoom_scene* _s, u32 _object);
void            fude_zoom_scene_set_place(fude_zoom_scene* _s, u32 _object, fude_zoom_place _p);
// Returns place _p moved by _m (a similarity in the object's frame's units): m ∘ place.
fude_zoom_place fude_zoom_place_moved(fude_zoom_place _p, fude_zoom_sim _m);
// Returns an object's own similarity: from its points (frame units from its translation, before its scale) to its
// frame's units.
fude_zoom_sim   fude_zoom_object_sim(const fude_zoom_object* _o);
// Makes a copy of an object (any kind but a frame) in _frame at _place, alive, on top, on the original's layer. Its
// payload is shared, not copied. Returns its index, or FUDE_ZOOM_NONE for a frame or an object with no payload. It is
// not an undo step by itself.
u32             fude_zoom_scene_copy_stroke(fude_zoom_scene* _s, u32 _object, u32 _frame, fude_zoom_place _place);
// Makes a copy of an object where it is, right after it in the order, on _layer: a gesture of its own (_gesture 0) or
// a later piece of _gesture (the id of the copy of its first piece). Returns its index, or FUDE_ZOOM_NONE as above. It
// is not an undo step by itself: the caller lets the original go in the same step.
u32             fude_zoom_scene_copy_to_layer(fude_zoom_scene* _s, u32 _object, u16 _layer, fude_zoom_id _gesture);

// Decodes a stroke's or a fill's points into _out, which has room for the object's count. Returns false when there is
// no payload or it is damaged.
b8   fude_zoom_scene_points(const fude_zoom_scene* _s, u32 _object, fude_zoom_qpoint* _out);
// Returns a point of a stroke in its frame's units.
fude_zoom_v2 fude_zoom_scene_point_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p);
// fude_zoom_scene_radius_at returns a point's half-width in frame units; fude_zoom_scene_local_radius_at returns it as
// drawn, before the object's scale.
f32  fude_zoom_scene_radius_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p);
f32  fude_zoom_scene_local_radius_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p);
// Returns a point as drawn, before the object's rotation and scale: in units from its translation.
fude_zoom_v2 fude_zoom_scene_local_at(const fude_zoom_object* _o, const fude_zoom_qpoint* _p);
// Appends to _out (an rde_arr of u32 indices) the alive objects of _frame whose boxes overlap _box (in its units),
// each once.
void fude_zoom_scene_query(const fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, rde_arr* _out);

// Returns _at (in parent units) moved onto the paper's lattice for a frame of scale _scale: a new frame's origin.
fude_zoom_v2 fude_zoom_paper_snap(fude_zoom_v2 _at, f64 _scale);

// Returns the quantum q (points are kept in steps of 2^q units) for drawing at zoom _z: about 1/16 of a point.
i8   fude_zoom_quantum_for(f64 _z);

// --- the camera --------------------------------------------------------------------------

// fude_zoom_camera_to_frame turns a screen point (app space, origin at the centre, Y up) into the camera frame's units;
// fude_zoom_camera_to_screen does the opposite.
fude_zoom_v2 fude_zoom_camera_to_frame(const fude_zoom_camera* _c, fude_zoom_v2 _screen);
fude_zoom_v2 fude_zoom_camera_to_screen(const fude_zoom_camera* _c, fude_zoom_v2 _frame_point);
// fude_zoom_camera_zoom_at zooms by _factor round screen point _screen, which stays over the same place;
// fude_zoom_camera_pan pans by _delta (in screen units). Call fude_zoom_camera_settle afterwards.
void fude_zoom_camera_zoom_at(fude_zoom_scene* _s, fude_zoom_v2 _screen, f64 _factor);
void fude_zoom_camera_pan(fude_zoom_scene* _s, fude_zoom_v2 _delta);
// Keeps the camera in range: it goes into a child (made if none holds the view) or up to the parent as its zoom says,
// and is handed to the frame under the view at its depth when panned out of its own. _half is half the screen's size.
// A frame made for the camera and left with nothing drawn in it is dropped.
void fude_zoom_camera_settle(fude_zoom_scene* _s, fude_zoom_v2 _half);
// Zooms by _factor round the middle of the screen as a pinch would: in steps of at most 2× each way, each settled, so
// each settle sees the zoom pass one frame's threshold at most, and a zoom of any size goes down (or up) through every
// frame between. _half is half the screen's size. For the test ink (inktest.h) and the developer's looks (look.c).
void fude_zoom_camera_zoom_by(fude_zoom_scene* _s, f64 _factor, fude_zoom_v2 _half);
// Returns the frame new strokes go in, for a stroke starting with box _box (in camera frame units): the camera's frame,
// unless something drawn LATER in a parent covers it (after it in the draw order, and not on a layer ranked above,
// which a new frame would still be under). Then a new frame is made on top (docs/infinite_canvas_design.md §2.4,
// "later is on top"), and the camera moves into it.
u32  fude_zoom_camera_drawing_frame(fude_zoom_scene* _s, fude_zoom_box _box);
// Puts the camera at frame _frame's point _at, zoom _z (used when flying to a change).
void fude_zoom_camera_look_at(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _at, f64 _z);
// Returns the transform from the camera's frame to the screen (origin at the centre, Y up).
fude_zoom_sim fude_zoom_camera_sim(const fude_zoom_camera* _c);

// --- bookmarks ---------------------------------------------------------------------------

// Makes a bookmark (a MARK object) of the view at _frame's point _at, zoom _z, numbered _number. Returns its index. It
// is not an undo step by itself.
u32  fude_zoom_scene_add_mark(fude_zoom_scene* _s, u32 _frame, fude_zoom_v2 _at, f64 _z, u32 _number);
// Reads a bookmark's zoom and number (either may be NULL). Returns false when it is not a bookmark.
b8   fude_zoom_scene_mark_of(const fude_zoom_scene* _s, u32 _object, f64* _z, u32* _number);

// Makes the file keep frame _frame (and the frames it is in) though nothing is drawn in it, because something names
// it: the camera is there, or an instrument lies there. It is journaled now if it was not yet.
void fude_zoom_scene_keep_frame(fude_zoom_scene* _s, u32 _frame);

// --- the history -------------------------------------------------------------------------

// Records one gesture done as an undo step: what it made (_born) and what it erased (_died), their flags already set
// by the caller. _frame and _box (in its units) say where it happened, for undo to fly to. It drops whatever was
// undone, and the oldest steps when over the budget. A gesture that did nothing records nothing.
void fude_zoom_history_push(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _died, u32 _died_count, const u32* _born, u32 _born_count);
// Undo or redo one step. Each returns false when there is nothing to undo or redo.
b8   fude_zoom_history_undo(fude_zoom_scene* _s);
b8   fude_zoom_history_redo(fude_zoom_scene* _s);
// Like fude_zoom_history_push, with moves too, all in one step: _died let go, _born made, and the _moved objects
// moved from _before to _after (already where _after says). One undo undoes all of it.
void fude_zoom_history_push_all(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _died, u32 _died_count, const u32* _born, u32 _born_count,
                                const u32* _moved, const fude_zoom_place* _before, const fude_zoom_place* _after, u32 _moved_count);
// The same, with the app's own _tag kept with the step (fude_zoom_action.tag), so that when it is undone or redone the
// app can do its part. A step with nothing in it but its tag is kept too.
void fude_zoom_history_push_tagged(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _died, u32 _died_count, const u32* _born, u32 _born_count,
                                   const u32* _moved, const fude_zoom_place* _before, const fude_zoom_place* _after, u32 _moved_count, u32 _tag);
// Records things moved (or scaled, or turned) from _before to _after as one step; they are already where _after says.
void fude_zoom_history_push_moved(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _objects, const fude_zoom_place* _before, const fude_zoom_place* _after, u32 _count);
// Return whether there is a step to undo or redo.
b8   fude_zoom_history_can_undo(const fude_zoom_scene* _s);
b8   fude_zoom_history_can_redo(const fude_zoom_scene* _s);
// Return the action undo or redo would step over next (NULL: none), so the page knows where to look. The pointer
// stays valid until the history changes.
const fude_zoom_action* fude_zoom_history_next_undo(const fude_zoom_scene* _s);
const fude_zoom_action* fude_zoom_history_next_redo(const fude_zoom_scene* _s);

// --- the journal (zfile.h reads and writes it) -----------------------------------------------

// The journal's ops: each is u8 op, u32 size, then its body.
typedef enum {
    FUDE_ZOOM_OP_FRAME = 1,   // a frame's record, new or changed
    FUDE_ZOOM_OP_OBJECT,      // an object's record and its points
    FUDE_ZOOM_OP_ALIVE,       // u64 id, u8 alive
    FUDE_ZOOM_OP_PUSH,        // an action pushed (the undone ones dropped first)
    FUDE_ZOOM_OP_COUNT,       // u32 action_count (an undo or redo)
    FUDE_ZOOM_OP_DROP,        // u32 n: the oldest n actions dropped
    FUDE_ZOOM_OP_ROOT,        // u64 the root frame's id (a new root)
    FUDE_ZOOM_OP_PLACE,       // u64 an object's id, f64 tx ty rotation scale (a frame's place goes as its record)
} FUDE_ZOOM_OP_;

// Append the records as the file has them (and as the journal's ops carry them) to _b.
void fude_zoom_put_frame(const fude_zoom_scene* _s, fude_bytes* _b, u32 _frame);
void fude_zoom_put_object(const fude_zoom_scene* _s, fude_bytes* _b, u32 _object);
void fude_zoom_put_action(const fude_zoom_scene* _s, fude_bytes* _b, u32 _action);
// Read one record and apply it: a frame (new, or replacing the one with its id), an object, or an action appended.
// Each returns false when the bytes are not one.
b8   fude_zoom_get_frame(fude_zoom_scene* _s, fude_reader* _r);
b8   fude_zoom_get_object(fude_zoom_scene* _s, fude_reader* _r);
b8   fude_zoom_get_action(fude_zoom_scene* _s, fude_reader* _r);
// Applies a journal chunk's ops in order, skipping kinds it does not know (a newer build's). Returns false at the first
// op that cannot be read or applied.
b8   fude_zoom_apply_ops(fude_zoom_scene* _s, fude_reader* _r);
// Called after loading: what no undo step can bring back becomes garbage, each frame's order is sorted (the file put
// its objects in as they came: loading) and its index built packed, the journal is emptied, and home falls back to the
// root for an older file.
void fude_zoom_scene_loaded(fude_zoom_scene* _s);


#endif
