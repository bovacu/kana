// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SCENE_PARTS
#define FUDE_ZOOM_SCENE_PARTS

#include "zoom/scene.h"
#include "zoom/shape.h"

// ===========================================================================
// Scene parts: the scene's private interface. The scene (scene.h) is written as scene.c and the scene_*.c
// modules; this is what they share with each other: the functions one module calls in another. Included by scene.c and
// scene_*.c only, never anywhere else: everything outside the scene uses scene.h.
//
// Where it sits: fude/zoom, under scene.h (which it includes, with every header the scene uses). The modules include
// this header and nothing else.
// Its main types and their lifecycle: the scene (fude_zoom_scene, scene.h), set up by fude_zoom_scene_init and freed by
// fude_zoom_scene_destroy (scene.c).
// Threads: the main thread only, as the scene itself.
// Files: none of its own (the records' layout is in scene_records.c, the file's in zfile.h).
// Design: scene.c was one file of some 2,100 lines; it was split by what each part does, its code moved unchanged.
// A function one module calls in another is declared here (not RDE_INTERNAL); what one module alone uses stays
// RDE_INTERNAL in it. Every name keeps scene.c's.
//
// Which module holds what:
//   scene.c          the tables and ids, the scene, frames, payloads, objects made, read and moved, the camera
//   scene_history.c  the history: actions kept and dropped, pushed, undone and redone
//   scene_records.c  the records written and read, a journal's operations applied, a scene loaded
// ===========================================================================

// --- scene.c: the tables, frames, payloads, objects ------------------------------------------------------------------

// The scene's tables as arrays; valid until the next element is added to that table.
fude_zoom_frame*  fude_zoom_frames(const fude_zoom_scene* _s);
fude_zoom_object* fude_zoom_objects(const fude_zoom_scene* _s);
fude_zoom_blob*   fude_zoom_blobs(const fude_zoom_scene* _s);
fude_zoom_action* fude_zoom_actions(const fude_zoom_scene* _s);
u32*              fude_zoom_targets(const fude_zoom_scene* _s);
fude_zoom_place*  fude_zoom_places(const fude_zoom_scene* _s);

// An array of u32 as a pointer.
u32*              fude_zoom_u32s(const rde_arr* _a);
// Returns an array's length as a u32.
u32 fude_zoom_len(const rde_arr* _a);
// Maps _id to _slot: a new id, or one that is there already (a FRAME object made again) moved to _slot.
void fude_zoom_ids_set(rde_hash_map* _m, fude_zoom_id _id, u32 _slot);
// Sets up an empty frame: its arrays on the std allocator, its index empty, no parent or FRAME object, an empty anchor,
// the identity place, and its first order key one step in.
void fude_zoom_frame_init(fude_zoom_frame* _f);
// Notes an id the file brought, so new ids never repeat it: this device's counter goes past it.
void fude_zoom_scene_saw_id(fude_zoom_scene* _s, fude_zoom_id _id);
// Starts a journal op: its kind, and a size filled in by fude_zoom_journal_op_end; *_at is where the size is.
void fude_zoom_journal_op_begin(fude_zoom_scene* _s, u8 _op, u32* _at);
// Ends a journal op begun at _at: its size written in.
void fude_zoom_journal_op_end(fude_zoom_scene* _s, u32 _at);
// Marks the bucket an object is in as changed (the next checkpoint writes it).
void fude_zoom_scene_touch(fude_zoom_scene* _s, const fude_zoom_object* _o);
// Sorts frame _frame's order once a file has loaded (scene.h, loading: its objects were appended as they came) into the
// order putting each in by its key, one at a time, gives.
void fude_zoom_order_settle(fude_zoom_scene* _s, u32 _frame);
// Builds frame _frame's index again, packed, from its objects that are not garbage; nothing in it is dead or stale
// then.
void fude_zoom_scene_index_build(fude_zoom_scene* _s, u32 _frame);
// Makes the FRAME object for frame _child in its parent, at key _z (0: on top): alive unless the frame is removed, its
// box the frame's anchor. Nothing for a root.
void fude_zoom_scene_frame_object(fude_zoom_scene* _s, u32 _child, u64 _z);
// Makes a payload of _size bytes copied from _data, its room just what it holds, with one reference. Returns its
// handle: its slot plus 1 (0 means none), reusing a freed slot first.
u32 fude_zoom_blob_new(fude_zoom_scene* _s, const u8* _data, u32 _size);
// Makes an object garbage when it is dead and no undo step names it: its points let go for good, its bucket marked
// changed (the file leaves it out), and its frame's dead entries counted (its index built again once they pile up).
// Frames are never collected.
void fude_zoom_scene_collect(fude_zoom_scene* _s, u32 _object);
// Puts an object record into its frame: the objects' table and the ids, the arrival order (its file_pos), and, unless
// it is garbage, the order, and the index with its box as it is when _boxed. An object whose box is not known yet
// (_boxed false) is put into the index by the caller once it is, so that it goes in once. _o->frame says which.
// Returns its index.
u32 fude_zoom_scene_put_in(fude_zoom_scene* _s, fude_zoom_object* _o, b8 _boxed);

// --- scene_history.c: the history ------------------------------------------------------------------------------------

// Drops the oldest _n actions (over the budget, or as the journal says): their objects forgotten, the targets and
// places moved to the front, and the rest renumbered. Journaled as an OP_DROP.
void fude_zoom_history_drop_oldest(fude_zoom_scene* _s, u32 _n);
// Appends an action as it is (its flags are already what it did), dropping what was undone first: its type from what
// it names (MOVED when anything moved), each object it names counted, and its bytes against the budget. Not
// journaled: the callers do that.
void fude_zoom_history_append(fude_zoom_scene* _s, u32 _frame, fude_zoom_box _box, const u32* _died, u32 _died_count, const u32* _born, u32 _born_count,
                              const u32* _moved, const fude_zoom_place* _before, const fude_zoom_place* _after, u32 _moved_count, u32 _tag);

#endif
