// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_INDEX
#define FUDE_ZOOM_INDEX

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Index: a frame's spatial index, an R-tree over its objects' boxes in the frame's own units, so drawing, erasing and
// picking only look at what is near.
//
// Where it sits: fude/zoom, under scene.h. Each frame of a scene owns one (scene.c fills it and queries it).
// Its main types and their lifecycle: fude_zoom_index is a value its owner initialises with fude_zoom_index_init and
// frees with fude_zoom_index_destroy; its nodes are an rde_arr, node 0 being the root.
// Threads: main thread only (it belongs to a scene, which is the main thread's). Queries do not change the tree.
// Design:
//   - Objects are never removed from a frame (erasing only marks them dead, so that it can be undone: see scene.h), so
//     they are never removed from the tree either. The tree only grows, and the caller skips the dead objects among
//     what a query finds. A moved object goes in again at its new place, and its old entry stays (stale): the caller
//     goes by each object's own box and takes each object once. When the entries of garbage objects and the stale ones
//     pile up (scene.c's fude_zoom_scene_reindex), or a frame is loaded, the tree is built again from scratch, packed
//     (sorted into tiles: Sort-Tile-Recursive), which is also the fastest tree to query.
//   - A node holds up to FUDE_ZOOM_INDEX_FAN entries. An insert goes down the path whose box grows least, and a full
//     node splits along its longer side, at the middle of its entries sorted along that side.
// ===========================================================================

// The most entries one node holds.
#define FUDE_ZOOM_INDEX_FAN 16u

// One node of the tree: the box round its entries, and the entries themselves.
typedef struct {
    fude_zoom_box box;
    u32           count;
    b8            leaf;
    u32           item[FUDE_ZOOM_INDEX_FAN];   // in a leaf, the caller's values; otherwise the indices of child nodes
    fude_zoom_box ibox[FUDE_ZOOM_INDEX_FAN];
} fude_zoom_index_node;

typedef struct {
    rde_arr TYPE(fude_zoom_index_node) nodes;   // node 0 is the root, once there is one
    u32                                count;   // how many entries (values) the tree holds
} fude_zoom_index;

// fude_zoom_index_init makes an empty index; fude_zoom_index_destroy frees its memory (it is also safe on a zeroed
// index that was never initialised); fude_zoom_index_clear empties it but keeps its memory for reuse.
void fude_zoom_index_init(fude_zoom_index* _ix);
void fude_zoom_index_destroy(fude_zoom_index* _ix);
void fude_zoom_index_clear(fude_zoom_index* _ix);

// Adds one value with its box.
void fude_zoom_index_insert(fude_zoom_index* _ix, u32 _value, fude_zoom_box _box);
// Builds the tree packed from _count values and their boxes all at once, replacing whatever it held.
void fude_zoom_index_build(fude_zoom_index* _ix, const u32* _values, const fude_zoom_box* _boxes, u32 _count);

// Appends to _out (an rde_arr of u32) every value whose box overlaps _box. Nothing is cleared first.
void fude_zoom_index_query(const fude_zoom_index* _ix, fude_zoom_box _box, rde_arr* _out);

// A visitor for fude_zoom_index_nearest: it is handed a value, its box, its leaf's box (the box round its neighbours)
// and its squared distance, and returns false to stop the search.
typedef b8 (*fude_zoom_index_visit)(any _user, u32 _value, fude_zoom_box _box, fude_zoom_box _leaf, f64 _d2);
// Visits the values nearest _at first. A box's distance is from its nearest point, so it is 0 inside it. The search
// stops when _visit returns false, or once _budget nodes have been opened. It is best first: a heap holds what has not
// been opened yet, and values at the same distance come in whatever order the heap gives.
void fude_zoom_index_nearest(const fude_zoom_index* _ix, fude_zoom_v2 _at, fude_zoom_index_visit _visit, any _user, u32 _budget);

#endif
