// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_INDEX
#define FUDE_ZOOM_INDEX

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// A frame's spatial index: an R-tree over its objects' boxes, in its units.
//
// Objects are never removed from a frame (an erase only marks them dead, so it
// can be undone: scene.h), so neither are they from the tree — it only grows,
// and the caller skips the dead in what a query finds. When the dead pile up,
// or a frame is loaded, it is built again from scratch, packed (sorted into
// tiles: STR), which is also the fastest tree to query.
//
// Up to FUDE_ZOOM_INDEX_FAN entries a node; an insert goes down the path that
// grows least, and a full node splits along its longer side, at the middle of
// its entries sorted there.
// ===========================================================================

#define FUDE_ZOOM_INDEX_FAN 16u

typedef struct {
    fude_zoom_box box;
    u32           count;
    b8            leaf;
    u32           item[FUDE_ZOOM_INDEX_FAN];   // a leaf's: the caller's values; else child nodes
    fude_zoom_box ibox[FUDE_ZOOM_INDEX_FAN];
} fude_zoom_index_node;

typedef struct {
    rde_arr TYPE(fude_zoom_index_node) nodes;   // [0] is the root (once there is one)
    u32                                count;   // entries
} fude_zoom_index;

void fude_zoom_index_init(fude_zoom_index* _ix);
void fude_zoom_index_destroy(fude_zoom_index* _ix);
void fude_zoom_index_clear(fude_zoom_index* _ix);

void fude_zoom_index_insert(fude_zoom_index* _ix, u32 _value, fude_zoom_box _box);
// Everything at once, packed: replaces what was there.
void fude_zoom_index_build(fude_zoom_index* _ix, const u32* _values, const fude_zoom_box* _boxes, u32 _count);

// Every value whose box overlaps _box, appended to _out (an rde_arr of u32).
void fude_zoom_index_query(const fude_zoom_index* _ix, fude_zoom_box _box, rde_arr* _out);

// The values nearest _at first (a box's distance: from its nearest point, 0
// inside it), each handed to _visit with its box, its leaf's box (its
// neighbours') and the squared distance, until _visit says enough (false) or
// _budget nodes have been opened. Best first: a heap of what is still unopened.
typedef b8 (*fude_zoom_index_visit)(any _user, u32 _value, fude_zoom_box _box, fude_zoom_box _leaf, f64 _d2);
void fude_zoom_index_nearest(const fude_zoom_index* _ix, fude_zoom_v2 _at, fude_zoom_index_visit _visit, any _user, u32 _budget);

#endif
