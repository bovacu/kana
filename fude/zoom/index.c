// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/index.h"

#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See index.h.
// ===========================================================================

#define FUDE_ZOOM_INDEX_DEPTH 32u   // far more than any tree gets (16^32 entries)

RDE_INTERNAL fude_zoom_index_node* fude_zoom_index_nodes(const fude_zoom_index* _ix) {
    return (fude_zoom_index_node*)_ix->nodes.memory;
}

void fude_zoom_index_init(fude_zoom_index* _ix) {
    memset(_ix, 0, sizeof(*_ix));
    _ix->nodes = rde_arr_new(sizeof(fude_zoom_index_node), rde_memory_allocator_get_default_std());
}

void fude_zoom_index_destroy(fude_zoom_index* _ix) {
    if(rde_arr_is_inited(&_ix->nodes)) {
        rde_arr_free(&_ix->nodes);
    }
    memset(_ix, 0, sizeof(*_ix));
}

void fude_zoom_index_clear(fude_zoom_index* _ix) {
    rde_arr_clear(&_ix->nodes);
    _ix->count = 0;
}

RDE_INTERNAL u32 fude_zoom_index_new_node(fude_zoom_index* _ix, b8 _leaf) {
    fude_zoom_index_node* _n = rde_arr_add(&_ix->nodes, NULL);
    memset(_n, 0, sizeof(*_n));
    _n->leaf = _leaf;
    _n->box  = fude_zoom_box_empty();
    return (u32)rde_arr_length(&_ix->nodes) - 1u;
}

RDE_INTERNAL void fude_zoom_index_refit(fude_zoom_index_node* _n) {
    _n->box = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _n->count; _i++) {
        _n->box = fude_zoom_box_union(_n->box, _n->ibox[_i]);
    }
}

// --- splitting ---------------------------------------------------------------------------

typedef struct {
    u32           item;
    fude_zoom_box box;
    f64           key;
} fude_zoom_index_entry;

RDE_INTERNAL int fude_zoom_index_by_key(const void* _a, const void* _b) {
    const f64 _ka = ((const fude_zoom_index_entry*)_a)->key;
    const f64 _kb = ((const fude_zoom_index_entry*)_b)->key;
    return _ka < _kb ? -1 : (_ka > _kb ? 1 : 0);
}

// Node _at is full and _item comes in: half its entries (and _item) move to a new
// node, its sibling. Returns the sibling.
RDE_INTERNAL u32 fude_zoom_index_split(fude_zoom_index* _ix, u32 _at, u32 _item, fude_zoom_box _box) {
    fude_zoom_index_entry _all[FUDE_ZOOM_INDEX_FAN + 1u];
    fude_zoom_index_node* _n = &fude_zoom_index_nodes(_ix)[_at];
    fude_zoom_box _span = _box;
    for(u32 _i = 0; _i < _n->count; _i++) {
        _all[_i] = (fude_zoom_index_entry){ _n->item[_i], _n->ibox[_i], 0.0 };
        _span    = fude_zoom_box_union(_span, _n->ibox[_i]);
    }
    _all[_n->count] = (fude_zoom_index_entry){ _item, _box, 0.0 };
    const u32 _total = _n->count + 1u;
    const b8  _wide  = _span.max_x - _span.min_x >= _span.max_y - _span.min_y;
    for(u32 _i = 0; _i < _total; _i++) {
        _all[_i].key = _wide ? _all[_i].box.min_x + _all[_i].box.max_x : _all[_i].box.min_y + _all[_i].box.max_y;
    }
    qsort(_all, _total, sizeof(_all[0]), fude_zoom_index_by_key);

    const b8  _leaf    = _n->leaf;
    const u32 _sibling = fude_zoom_index_new_node(_ix, _leaf);
    _n                 = &fude_zoom_index_nodes(_ix)[_at];   // the add may have moved the nodes
    fude_zoom_index_node* _s = &fude_zoom_index_nodes(_ix)[_sibling];
    const u32 _half = _total / 2u;
    _n->count = 0;
    for(u32 _i = 0; _i < _half; _i++) {
        _n->item[_n->count] = _all[_i].item;
        _n->ibox[_n->count] = _all[_i].box;
        _n->count++;
    }
    for(u32 _i = _half; _i < _total; _i++) {
        _s->item[_s->count] = _all[_i].item;
        _s->ibox[_s->count] = _all[_i].box;
        _s->count++;
    }
    fude_zoom_index_refit(_n);
    fude_zoom_index_refit(_s);
    return _sibling;
}

// --- inserting ---------------------------------------------------------------------------

void fude_zoom_index_insert(fude_zoom_index* _ix, u32 _value, fude_zoom_box _box) {
    if(rde_arr_length(&_ix->nodes) == 0) {
        fude_zoom_index_new_node(_ix, true);
    }
    _ix->count++;

    // Down to a leaf, keeping the path: each node and the slot taken in it.
    u32 _path[FUDE_ZOOM_INDEX_DEPTH];
    u32 _slot[FUDE_ZOOM_INDEX_DEPTH];
    u32 _depth = 0;
    u32 _at    = 0;
    for(;;) {
        fude_zoom_index_node* _n = &fude_zoom_index_nodes(_ix)[_at];
        _path[_depth] = _at;
        if(_n->leaf || _depth + 1u >= FUDE_ZOOM_INDEX_DEPTH) {
            break;
        }
        u32 _best      = 0;
        f64 _best_grow = 0.0;
        f64 _best_area = 0.0;
        for(u32 _i = 0; _i < _n->count; _i++) {
            const f64 _area = fude_zoom_box_area(_n->ibox[_i]);
            const f64 _grow = fude_zoom_box_area(fude_zoom_box_union(_n->ibox[_i], _box)) - _area;
            if(_i == 0 || _grow < _best_grow || (_grow <= _best_grow && _area < _best_area)) {
                _best      = _i;
                _best_grow = _grow;
                _best_area = _area;
            }
        }
        _slot[_depth] = _best;
        _depth++;
        _at = _n->item[_best];
    }

    // Into the leaf; a full one splits, and the split goes up as far as it must.
    u32           _item = _value;
    fude_zoom_box _ibox = _box;
    for(i32 _d = (i32)_depth; _d >= 0; _d--) {
        fude_zoom_index_node* _n = &fude_zoom_index_nodes(_ix)[_path[_d]];
        if(_n->count < FUDE_ZOOM_INDEX_FAN) {
            _n->item[_n->count] = _item;
            _n->ibox[_n->count] = _ibox;
            _n->count++;
            _n->box = fude_zoom_box_union(_n->box, _ibox);
            // Nothing split above here: the parents' boxes only grow.
            for(i32 _u = _d - 1; _u >= 0; _u--) {
                fude_zoom_index_node* _p = &fude_zoom_index_nodes(_ix)[_path[_u]];
                _p->ibox[_slot[_u]] = fude_zoom_index_nodes(_ix)[_path[_u + 1]].box;
                _p->box = fude_zoom_box_union(_p->box, _p->ibox[_slot[_u]]);
            }
            return;
        }
        const u32 _sibling = fude_zoom_index_split(_ix, _path[_d], _item, _ibox);
        if(_d == 0) {
            // The root split: it moves down into a new node, and the root holds
            // the two halves. The root stays node 0.
            const u32 _moved = fude_zoom_index_new_node(_ix, false);
            fude_zoom_index_node* _nodes = fude_zoom_index_nodes(_ix);
            _nodes[_moved] = _nodes[0];
            fude_zoom_index_node* _root = &_nodes[0];
            memset(_root, 0, sizeof(*_root));
            _root->leaf    = false;
            _root->count   = 2u;
            _root->item[0] = _moved;
            _root->ibox[0] = _nodes[_moved].box;
            _root->item[1] = _sibling;
            _root->ibox[1] = _nodes[_sibling].box;
            fude_zoom_index_refit(_root);
            return;
        }
        // The parent's slot for the split node shrank to its half; the sibling
        // goes into the parent next.
        fude_zoom_index_node* _p = &fude_zoom_index_nodes(_ix)[_path[_d - 1]];
        _p->ibox[_slot[_d - 1]] = fude_zoom_index_nodes(_ix)[_path[_d]].box;
        fude_zoom_index_refit(_p);
        _item = _sibling;
        _ibox = fude_zoom_index_nodes(_ix)[_sibling].box;
    }
}

// --- building packed ---------------------------------------------------------------------

typedef struct {
    u32           item;
    fude_zoom_box box;
    f64           cx, cy;
} fude_zoom_index_pack;

RDE_INTERNAL int fude_zoom_index_by_x(const void* _a, const void* _b) {
    const f64 _ka = ((const fude_zoom_index_pack*)_a)->cx;
    const f64 _kb = ((const fude_zoom_index_pack*)_b)->cx;
    return _ka < _kb ? -1 : (_ka > _kb ? 1 : 0);
}

RDE_INTERNAL int fude_zoom_index_by_y(const void* _a, const void* _b) {
    const f64 _ka = ((const fude_zoom_index_pack*)_a)->cy;
    const f64 _kb = ((const fude_zoom_index_pack*)_b)->cy;
    return _ka < _kb ? -1 : (_ka > _kb ? 1 : 0);
}

// One level packed: _in's entries sorted into vertical slices by x, each slice
// by y, then cut into nodes of FAN — the next level's entries, into _out.
RDE_INTERNAL u32 fude_zoom_index_pack_level(fude_zoom_index* _ix, fude_zoom_index_pack* _in, u32 _count, b8 _leaf, fude_zoom_index_pack* _out) {
    const u32 _nodes  = (_count + FUDE_ZOOM_INDEX_FAN - 1u) / FUDE_ZOOM_INDEX_FAN;
    u32       _slices = 1u;
    while(_slices * _slices < _nodes) {
        _slices++;
    }
    const u32 _per_slice = _slices * FUDE_ZOOM_INDEX_FAN;
    qsort(_in, _count, sizeof(_in[0]), fude_zoom_index_by_x);
    u32 _made = 0;
    for(u32 _s = 0; _s < _count; _s += _per_slice) {
        const u32 _n = _count - _s < _per_slice ? _count - _s : _per_slice;
        qsort(&_in[_s], _n, sizeof(_in[0]), fude_zoom_index_by_y);
        for(u32 _i = 0; _i < _n; _i += FUDE_ZOOM_INDEX_FAN) {
            const u32 _node = fude_zoom_index_new_node(_ix, _leaf);
            fude_zoom_index_node* _nd = &fude_zoom_index_nodes(_ix)[_node];
            for(u32 _j = _i; _j < _n && _j < _i + FUDE_ZOOM_INDEX_FAN; _j++) {
                _nd->item[_nd->count] = _in[_s + _j].item;
                _nd->ibox[_nd->count] = _in[_s + _j].box;
                _nd->count++;
            }
            fude_zoom_index_refit(_nd);
            _out[_made++] = (fude_zoom_index_pack){ _node, _nd->box, (_nd->box.min_x + _nd->box.max_x) * 0.5, (_nd->box.min_y + _nd->box.max_y) * 0.5 };
        }
    }
    return _made;
}

void fude_zoom_index_build(fude_zoom_index* _ix, const u32* _values, const fude_zoom_box* _boxes, u32 _count) {
    fude_zoom_index_clear(_ix);
    if(_count == 0) {
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_index_pack* _a = _heap->malloc(_heap->allocator, (usize)_count * sizeof(fude_zoom_index_pack));
    fude_zoom_index_pack* _b = _heap->malloc(_heap->allocator, (usize)_count * sizeof(fude_zoom_index_pack));
    for(u32 _i = 0; _i < _count; _i++) {
        _a[_i] = (fude_zoom_index_pack){ _values[_i], _boxes[_i], (_boxes[_i].min_x + _boxes[_i].max_x) * 0.5, (_boxes[_i].min_y + _boxes[_i].max_y) * 0.5 };
    }
    // Node 0 is kept for the root: the levels are built after it, and the top
    // level's single node is copied into it.
    fude_zoom_index_new_node(_ix, true);
    u32 _n    = _count;
    b8  _leaf = true;
    for(;;) {
        _n = fude_zoom_index_pack_level(_ix, _a, _n, _leaf, _b);
        _leaf = false;
        fude_zoom_index_pack* _t = _a;
        _a = _b;
        _b = _t;
        if(_n == 1u) {
            break;
        }
    }
    fude_zoom_index_node* _nodes = fude_zoom_index_nodes(_ix);
    _nodes[0] = _nodes[_a[0].item];
    // The old copy stays unused in the array: one node, nothing points at it.
    _ix->count = _count;
    _heap->free(_heap->allocator, _a);
    _heap->free(_heap->allocator, _b);
}

// --- querying ----------------------------------------------------------------------------

void fude_zoom_index_query(const fude_zoom_index* _ix, fude_zoom_box _box, rde_arr* _out) {
    if(rde_arr_length(&_ix->nodes) == 0 || _ix->count == 0) {
        return;
    }
    const fude_zoom_index_node* _nodes = fude_zoom_index_nodes(_ix);
    u32 _stack[FUDE_ZOOM_INDEX_DEPTH * FUDE_ZOOM_INDEX_FAN];
    u32 _top = 0;
    _stack[_top++] = 0;
    while(_top > 0) {
        const fude_zoom_index_node* _n = &_nodes[_stack[--_top]];
        for(u32 _i = 0; _i < _n->count; _i++) {
            if(!fude_zoom_box_overlaps(_n->ibox[_i], _box)) {
                continue;
            }
            if(_n->leaf) {
                rde_arr_add(_out, &_n->item[_i]);
            } else if(_top < sizeof(_stack) / sizeof(_stack[0])) {
                _stack[_top++] = _n->item[_i];
            }
        }
    }
}

// --- nearest first -----------------------------------------------------------------------------

typedef struct {
    f64 d2;
    u32 node;
    i32 entry;   // a leaf's entry; -1: the node itself, unopened
} fude_zoom_index_near;

RDE_INTERNAL f64 fude_zoom_index_d2(fude_zoom_box _b, fude_zoom_v2 _p) {
    const f64 _dx = _p.x < _b.min_x ? _b.min_x - _p.x : (_p.x > _b.max_x ? _p.x - _b.max_x : 0.0);
    const f64 _dy = _p.y < _b.min_y ? _b.min_y - _p.y : (_p.y > _b.max_y ? _p.y - _b.max_y : 0.0);
    return _dx * _dx + _dy * _dy;
}

RDE_INTERNAL void fude_zoom_index_heap_push(rde_arr* _h, fude_zoom_index_near _e) {
    rde_arr_add(_h, (any)&_e);
    fude_zoom_index_near* _a = (fude_zoom_index_near*)_h->memory;
    u32 _i = (u32)rde_arr_length(_h) - 1u;
    while(_i > 0u) {
        const u32 _up = (_i - 1u) / 2u;
        if(_a[_up].d2 <= _a[_i].d2) {
            break;
        }
        const fude_zoom_index_near _t = _a[_up];
        _a[_up] = _a[_i];
        _a[_i]  = _t;
        _i      = _up;
    }
}

RDE_INTERNAL fude_zoom_index_near fude_zoom_index_heap_pop(rde_arr* _h) {
    fude_zoom_index_near* _a   = (fude_zoom_index_near*)_h->memory;
    const fude_zoom_index_near _top = _a[0];
    const u32 _n = (u32)rde_arr_length(_h) - 1u;
    _a[0] = _a[_n];
    _h->count--;
    u32 _i = 0;
    for(;;) {
        const u32 _l = 2u * _i + 1u, _r = _l + 1u;
        u32 _m = _i;
        if(_l < _n && _a[_l].d2 < _a[_m].d2) { _m = _l; }
        if(_r < _n && _a[_r].d2 < _a[_m].d2) { _m = _r; }
        if(_m == _i) {
            break;
        }
        const fude_zoom_index_near _t = _a[_m];
        _a[_m] = _a[_i];
        _a[_i] = _t;
        _i     = _m;
    }
    return _top;
}

void fude_zoom_index_nearest(const fude_zoom_index* _ix, fude_zoom_v2 _at, fude_zoom_index_visit _visit, any _user, u32 _budget) {
    if(rde_arr_length(&_ix->nodes) == 0 || _ix->count == 0 || _visit == NULL) {
        return;
    }
    const fude_zoom_index_node* _nodes = fude_zoom_index_nodes(_ix);
    rde_arr _heap = rde_arr_new(sizeof(fude_zoom_index_near), rde_memory_allocator_get_default_std());
    fude_zoom_index_heap_push(&_heap, (fude_zoom_index_near){ fude_zoom_index_d2(_nodes[0].box, _at), 0u, -1 });
    u32 _opened = 0;
    while(rde_arr_length(&_heap) > 0) {
        const fude_zoom_index_near _e = fude_zoom_index_heap_pop(&_heap);
        const fude_zoom_index_node* _n = &_nodes[_e.node];
        if(_e.entry >= 0) {
            if(!_visit(_user, _n->item[_e.entry], _n->ibox[_e.entry], _n->box, _e.d2)) {
                break;
            }
            continue;
        }
        if(++_opened > _budget) {
            break;
        }
        for(u32 _i = 0; _i < _n->count; _i++) {
            const f64 _d2 = fude_zoom_index_d2(_n->ibox[_i], _at);
            fude_zoom_index_heap_push(&_heap, _n->leaf ? (fude_zoom_index_near){ _d2, _e.node, (i32)_i } : (fude_zoom_index_near){ _d2, _n->item[_i], -1 });
        }
    }
    rde_arr_free(&_heap);
}
