// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/graph_parts.h"

// ===========================================================================
// See graph.h, which has how the layout works. Graphs, the layered layout's first steps: its working state
// (fude_zoom_graph_work: its own vertices, in flow space, top to bottom), the subgraphs as a tree, back edges turned
// round (cycles broken), ranks, each long edge's chain of dummies, fillers where a subgraph has nothing of its own in a
// rank, each vertex's neighbours, and the order within each rank (barycentre sweeps, the fewest crossings kept).
//
// The file goes in the layout's order: the vertices and their helpers, the subgraphs as a tree, cycles and ranks,
// dummies and fillers, and the order within the ranks.
//
// Part of the graph: graph_parts.h holds what the modules share and says which module holds what. Internal names start
// fude_zoom_graph_ as the public ones do. It keeps no state of its own: any thread may use a graph no other thread is
// using.
// ===========================================================================

// --- laying out ------------------------------------------------------------------------------
//
// Worked out top to bottom ("flow space": x across the ranks, y along them)
// and turned at the end. The layout's own vertices are the nodes (first, at
// their own index), the dummies long edges get at each rank they cross, and
// fillers: a subgraph's stand-in in a rank between its first and last where
// nothing of it is, so its box keeps others out there too.

#define FUDE_ZOOM_GRAPH_SWEEPS  24u          // ordering sweeps, down and up by turns (the best kept)
#define FUDE_ZOOM_GRAPH_DUMMIES (1u << 18)   // dummies at most: a long edge past it is drawn straight

// rde_sort's order for fude_zoom_graph_sorted: by k1, then k2, then index, so it never depends on the sort's own order.
int fude_zoom_graph_by_keys(const any _a, const any _b) {
    const fude_zoom_graph_sorted* _x = (const fude_zoom_graph_sorted*)_a;
    const fude_zoom_graph_sorted* _y = (const fude_zoom_graph_sorted*)_b;
    if(_x->k1 != _y->k1) return _x->k1 < _y->k1 ? -1 : 1;
    if(_x->k2 != _y->k2) return _x->k2 < _y->k2 ? -1 : 1;
    return _x->i < _y->i ? -1 : (_x->i > _y->i ? 1 : 0);
}

// Returns the vertices as an array (good until one is added).
fude_zoom_graph_vertex* fude_zoom_graph_v(const fude_zoom_graph_work* _w) {
    return (fude_zoom_graph_vertex*)_w->verts.memory;
}

// Returns a size or a gap made safe: one that is not a finite number above 0 is 0.
f64 fude_zoom_graph_sane(f64 _s) {
    return rde_math_is_finite_f64(_s) && _s > 0.0 ? _s : 0.0;
}

// Turns a point from flow space to the graph's direction (fude_zoom_graph_unturn, below, turns it back).
fude_zoom_v2 fude_zoom_graph_turn(u8 _dir, f64 _x, f64 _y) {
    switch(_dir) {
        case FUDE_ZOOM_FLOW_BT: return (fude_zoom_v2){ _x, -_y };
        case FUDE_ZOOM_FLOW_LR: return (fude_zoom_v2){ _y, _x };
        case FUDE_ZOOM_FLOW_RL: return (fude_zoom_v2){ -_y, _x };
        default:                return (fude_zoom_v2){ _x, _y };
    }
}

// Turns a point from the graph's direction back into flow space.
fude_zoom_v2 fude_zoom_graph_unturn(u8 _dir, f64 _x, f64 _y) {
    switch(_dir) {
        case FUDE_ZOOM_FLOW_BT: return (fude_zoom_v2){ _x, -_y };
        case FUDE_ZOOM_FLOW_LR: return (fude_zoom_v2){ _y, _x };
        case FUDE_ZOOM_FLOW_RL: return (fude_zoom_v2){ _y, -_x };
        default:                return (fude_zoom_v2){ _x, _y };
    }
}

// Returns whether the graph flows across (LR, RL), so a node's width runs along the flow.
b8 fude_zoom_graph_sideways(u8 _dir) {
    return _dir == FUDE_ZOOM_FLOW_LR || _dir == FUDE_ZOOM_FLOW_RL;
}

// Adds a vertex of kind _kind in subgraph _sub at rank _rank, _wide across and _tall along (flow space), and returns
// its index.
u32 fude_zoom_graph_add_vertex(fude_zoom_graph_work* _w, u8 _kind, u32 _sub, i32 _rank, f64 _wide, f64 _tall) {
    fude_zoom_graph_vertex _v;
    rde_memset(&_v, 0, sizeof(_v));
    _v.node = FUDE_ZOOM_GRAPH_NONE;
    _v.kind = _kind;
    _v.sub  = _sub;
    _v.rank = _rank;
    _v.w    = _wide;
    _v.h    = _tall;
    rde_arr_add(&_w->verts, (any)&_v);
    return fude_zoom_graph_count(&_w->verts) - 1u;
}

// --- subgraphs as a tree -----------------------------------------------------------------------

// The parents made safe (a loop or a bad index cut at the top) and depths.
void fude_zoom_graph_tree(fude_zoom_graph_work* _w) {
    const u32 _ns = _w->ns;
    _w->parent = (u32*)fude_zoom_graph_table(&_w->tables, _ns + 1u, sizeof(u32));
    _w->depth  = (u32*)fude_zoom_graph_table(&_w->tables, _ns + 1u, sizeof(u32));
    for(u32 _s = 1; _s <= _ns; _s++) {
        const u32 _p = fude_zoom_graph_subgraph_at(_w->g, _s - 1u)->parent;
        _w->parent[_s] = _p <= _ns && _p != _s ? _p : 0u;
    }
    for(u32 _s = 1; _s <= _ns; _s++) {
        u32 _t = _s, _steps = 0;
        while(_t != 0 && _steps <= _ns) {
            _t = _w->parent[_t];
            _steps++;
        }
        if(_t != 0) {
            _w->parent[_s] = 0;
        }
    }
    for(u32 _s = 1; _s <= _ns; _s++) {
        u32 _d = 0;
        for(u32 _t = _s; _t != 0; _t = _w->parent[_t]) _d++;
        _w->depth[_s]  = _d;
        _w->most_depth = _d > _w->most_depth ? _d : _w->most_depth;
    }
}

// Returns the innermost subgraph holding both _a and _b (index + 1; 0 the top).
u32 fude_zoom_graph_common(const fude_zoom_graph_work* _w, u32 _a, u32 _b) {
    while(_w->depth[_a] > _w->depth[_b]) _a = _w->parent[_a];
    while(_w->depth[_b] > _w->depth[_a]) _b = _w->parent[_b];
    while(_a != _b) {
        _a = _w->parent[_a];
        _b = _w->parent[_b];
    }
    return _a;
}

// The subgraph just inside _s on the way down to _sub (0: _sub is _s).
RDE_INTERNAL u32 fude_zoom_graph_child_of(const fude_zoom_graph_work* _w, u32 _sub, u32 _s) {
    if(_sub == _s) {
        return 0;
    }
    while(_sub != 0 && _w->parent[_sub] != _s) _sub = _w->parent[_sub];
    return _sub;
}

// _sub's subgraphs outermost first into _path (most_depth long). How many.
u32 fude_zoom_graph_path(const fude_zoom_graph_work* _w, u32 _sub, u32* _path) {
    const u32 _n = _w->depth[_sub];
    for(u32 _k = _n; _k > 0; _k--) {
        _path[_k - 1u] = _sub;
        _sub = _w->parent[_sub];
    }
    return _n;
}

// --- cycles and ranks ----------------------------------------------------------------------------

// Back edges turned round: those a depth-first walk (the nodes in order, each one's edges in order) finds going back
// up its own path. Self-loops and edges with no ends are left as they are.
void fude_zoom_graph_break_cycles(fude_zoom_graph_work* _w) {
    const u32 _n = _w->n, _ne = _w->ne;
    rde_arr _tables = fude_zoom_graph_tables_new();
    u32* _first = (u32*)fude_zoom_graph_table(&_tables, _n + 1u, sizeof(u32));
    u32* _list  = (u32*)fude_zoom_graph_table(&_tables, _ne, sizeof(u32));
    u32* _fill  = (u32*)fude_zoom_graph_table(&_tables, _n + 1u, sizeof(u32));
    u8*  _state = (u8*)fude_zoom_graph_table(&_tables, _n, sizeof(u8));
    u32* _stack = (u32*)fude_zoom_graph_table(&_tables, _n, sizeof(u32));
    u32* _it    = (u32*)fude_zoom_graph_table(&_tables, _n, sizeof(u32));
    for(u32 _e = 0; _e < _ne; _e++) {
        if(_w->tail[_e] != FUDE_ZOOM_GRAPH_NONE && _w->tail[_e] != _w->head[_e]) {
            _first[_w->tail[_e] + 1u]++;
        }
    }
    for(u32 _i = 0; _i < _n; _i++) {
        _first[_i + 1u] += _first[_i];
        _fill[_i]        = _first[_i];
    }
    for(u32 _e = 0; _e < _ne; _e++) {
        if(_w->tail[_e] != FUDE_ZOOM_GRAPH_NONE && _w->tail[_e] != _w->head[_e]) {
            _list[_fill[_w->tail[_e]]++] = _e;
        }
    }
    for(u32 _root = 0; _root < _n; _root++) {
        if(_state[_root] != 0) {
            continue;
        }
        u32 _sp = 0;
        _stack[_sp++] = _root;
        _state[_root] = 1;
        _it[_root]    = _first[_root];
        while(_sp > 0) {
            const u32 _v = _stack[_sp - 1u];
            if(_it[_v] < _first[_v + 1u]) {
                const u32 _e = _list[_it[_v]++];
                const u32 _t = _w->head[_e];
                if(_state[_t] == 1) {
                    _w->reversed[_e] = true;
                } else if(_state[_t] == 0) {
                    _state[_t]    = 1;
                    _it[_t]       = _first[_t];
                    _stack[_sp++] = _t;
                }
            } else {
                _state[_v] = 2;
                _sp--;
            }
        }
    }
    for(u32 _e = 0; _e < _ne; _e++) {
        if(_w->reversed[_e]) {
            const u32 _t = _w->tail[_e];
            _w->tail[_e] = _w->head[_e];
            _w->head[_e] = _t;
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

// Ranks by longest path from the sources, then each node that feeds more edges than it is fed pulled down as far as
// its edges let it (shorter edges in all: a source joins what it feeds instead of waiting at the top). The ranks are
// then counted from 0.
void fude_zoom_graph_rank(fude_zoom_graph_work* _w) {
    const u32 _n = _w->n, _ne = _w->ne;
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    rde_arr _tables = fude_zoom_graph_tables_new();
    u32* _first = (u32*)fude_zoom_graph_table(&_tables, _n + 1u, sizeof(u32));
    u32* _list  = (u32*)fude_zoom_graph_table(&_tables, _ne, sizeof(u32));
    u32* _fill  = (u32*)fude_zoom_graph_table(&_tables, _n + 1u, sizeof(u32));
    u32* _in    = (u32*)fude_zoom_graph_table(&_tables, _n, sizeof(u32));
    u32* _ins   = (u32*)fude_zoom_graph_table(&_tables, _n, sizeof(u32));
    u32* _topo  = (u32*)fude_zoom_graph_table(&_tables, _n, sizeof(u32));
    u8*  _done  = (u8*)fude_zoom_graph_table(&_tables, _n, sizeof(u8));
    for(u32 _e = 0; _e < _ne; _e++) {
        if(_w->tail[_e] != FUDE_ZOOM_GRAPH_NONE && _w->tail[_e] != _w->head[_e]) {
            _first[_w->tail[_e] + 1u]++;
            _in[_w->head[_e]]++;
        }
    }
    for(u32 _i = 0; _i < _n; _i++) {
        _first[_i + 1u] += _first[_i];
        _fill[_i]        = _first[_i];
        _ins[_i]         = _in[_i];
    }
    for(u32 _e = 0; _e < _ne; _e++) {
        if(_w->tail[_e] != FUDE_ZOOM_GRAPH_NONE && _w->tail[_e] != _w->head[_e]) {
            _list[_fill[_w->tail[_e]]++] = _e;
        }
    }
    u32 _nt = 0, _at = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        _v[_i].rank = 0;
        if(_in[_i] == 0) {
            _topo[_nt++] = _i;
            _done[_i]    = 1;
        }
    }
    while(_at < _nt) {
        const u32 _t = _topo[_at++];
        for(u32 _k = _first[_t]; _k < _first[_t + 1u]; _k++) {
            const u32 _e = _list[_k], _h = _w->head[_e];
            const i32 _r = _v[_t].rank + (i32)_w->minlen[_e];
            _v[_h].rank = _r > _v[_h].rank ? _r : _v[_h].rank;
            if(--_in[_h] == 0 && !_done[_h]) {
                _topo[_nt++] = _h;
                _done[_h]    = 1;
            }
        }
    }
    for(u32 _k = _nt; _k > 0; _k--) {
        const u32 _t = _topo[_k - 1u];
        const u32 _outs = _first[_t + 1u] - _first[_t];
        if(_outs <= _ins[_t]) {
            continue;
        }
        i32 _hi = 0x7FFFFFFF;
        for(u32 _j = _first[_t]; _j < _first[_t + 1u]; _j++) {
            const i32 _r = _v[_w->head[_list[_j]]].rank - (i32)_w->minlen[_list[_j]];
            _hi = _r < _hi ? _r : _hi;
        }
        _v[_t].rank = _hi > _v[_t].rank ? _hi : _v[_t].rank;
    }
    i32 _low = 0x7FFFFFFF, _high = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        _low = _v[_i].rank < _low ? _v[_i].rank : _low;
    }
    for(u32 _i = 0; _i < _n; _i++) {
        _v[_i].rank -= _low;
        _high = _v[_i].rank > _high ? _v[_i].rank : _high;
    }
    _w->ranks = (u32)_high + 1u;
    fude_zoom_graph_tables_free(&_tables);
}

// --- dummies and fillers -------------------------------------------------------------------------

// Makes each edge's chain: its tail, a dummy at each rank it crosses (the middle one holding its label when labels
// have ranks of their own), and its head. A dummy is in its ends' innermost common subgraph. Once
// FUDE_ZOOM_GRAPH_DUMMIES are made, a longer edge gets no chain.
void fude_zoom_graph_chains(fude_zoom_graph_work* _w) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _w->chain        = rde_arr_new(sizeof(u32), _heap);
    _w->chain_first  = (u32*)fude_zoom_graph_table(&_w->tables, _w->ne, sizeof(u32));
    _w->chain_count  = (u32*)fude_zoom_graph_table(&_w->tables, _w->ne, sizeof(u32));
    _w->label_vertex = (u32*)fude_zoom_graph_table(&_w->tables, _w->ne, sizeof(u32));
    const b8 _side = fude_zoom_graph_sideways(_w->g->direction);
    u32 _dummies = 0;
    for(u32 _e = 0; _e < _w->ne; _e++) {
        _w->label_vertex[_e] = FUDE_ZOOM_GRAPH_NONE;
        const u32 _t = _w->tail[_e], _h = _w->head[_e];
        if(_t == FUDE_ZOOM_GRAPH_NONE || _t == _h) {
            continue;
        }
        const i32 _rt = fude_zoom_graph_v(_w)[_t].rank, _rh = fude_zoom_graph_v(_w)[_h].rank;
        const u32 _span = (u32)(_rh - _rt);
        if(_span > 1u && _dummies + _span - 1u > FUDE_ZOOM_GRAPH_DUMMIES) {
            continue;
        }
        const u32 _sub = fude_zoom_graph_common(_w, fude_zoom_graph_v(_w)[_t].sub, fude_zoom_graph_v(_w)[_h].sub);
        const fude_zoom_graph_edge* _edge = fude_zoom_graph_edge_at(_w->g, _e);
        const f64 _lw = fude_zoom_graph_sane(_edge->label_w), _lh = fude_zoom_graph_sane(_edge->label_h);
        const b8 _labelled = _w->labels && (_lw > 0.0 || _lh > 0.0);
        _w->chain_first[_e] = fude_zoom_graph_count(&_w->chain);
        _w->chain_count[_e] = _span + 1u;
        rde_arr_add(&_w->chain, (any)&_t);
        for(u32 _k = 1; _k < _span; _k++) {
            const b8  _here = _labelled && _k == _span / 2u;
            const u32 _d    = fude_zoom_graph_add_vertex(_w, _here ? FUDE_ZOOM_GRAPH_VERTEX_LABEL : FUDE_ZOOM_GRAPH_VERTEX_DUMMY, _sub, _rt + (i32)_k,
                                                         _here ? (_side ? _lh : _lw) : 0.0, _here ? (_side ? _lw : _lh) : 0.0);
            if(_here) {
                _w->label_vertex[_e] = _d;
            }
            rde_arr_add(&_w->chain, (any)&_d);
        }
        rde_arr_add(&_w->chain, (any)&_h);
        _dummies += _span - 1u;
    }
}

// Returns a (subgraph, rank) pair as one key, for a set of them (never 0).
RDE_INTERNAL u64 fude_zoom_graph_pair(u32 _s, i32 _r) {
    return (((u64)_s << 32) | (u64)(u32)_r) + 1u;
}

// Returns whether the set holds key _k.
RDE_INTERNAL b8 fude_zoom_graph_set_has(const rde_hash_set* _set, u64 _k) {
    return rde_hash_set_contains(_set, (any)&_k);
}

// Adds key _k to the set.
RDE_INTERNAL void fude_zoom_graph_set_add(rde_hash_set* _set, u64 _k) {
    rde_hash_set_add(_set, (any)&_k);
}

// Each subgraph's ranks, and a filler wherever it has none between its first
// and last (the deepest first: one filler stands for its outer ones too).
// Pinned nodes do not count: they are not in the ranks' rows.
void fude_zoom_graph_fillers(fude_zoom_graph_work* _w) {
    const u32 _ns = _w->ns;
    _w->smin = (i32*)fude_zoom_graph_table(&_w->tables, _ns + 1u, sizeof(i32));
    _w->smax = (i32*)fude_zoom_graph_table(&_w->tables, _ns + 1u, sizeof(i32));
    for(u32 _s = 0; _s <= _ns; _s++) {
        _w->smin[_s] = 0x7FFFFFFF;
        _w->smax[_s] = -1;
    }
    if(_ns == 0) {
        return;
    }
    rde_hash_set TYPE(u64) _set = rde_hash_set_new(sizeof(u64), rde_hash_map_fn_u64_hash, rde_hash_map_fn_u64_cmp, NULL, rde_memory_allocator_get_default_std());
    const u32 _nv = fude_zoom_graph_count(&_w->verts);
    for(u32 _i = 0; _i < _nv; _i++) {
        const fude_zoom_graph_vertex* _v = &fude_zoom_graph_v(_w)[_i];
        if(_v->pinned) {
            continue;
        }
        for(u32 _s = _v->sub; _s != 0; _s = _w->parent[_s]) {
            fude_zoom_graph_set_add(&_set, fude_zoom_graph_pair(_s, _v->rank));
            _w->smin[_s] = _v->rank < _w->smin[_s] ? _v->rank : _w->smin[_s];
            _w->smax[_s] = _v->rank > _w->smax[_s] ? _v->rank : _w->smax[_s];
        }
    }
    rde_arr _tables = fude_zoom_graph_tables_new();
    fude_zoom_graph_sorted* _deep = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _ns, sizeof(fude_zoom_graph_sorted));
    for(u32 _s = 1; _s <= _ns; _s++) {
        _deep[_s - 1u] = (fude_zoom_graph_sorted){ -(f64)_w->depth[_s], 0.0, _s };
    }
    rde_sort(_deep, _ns, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
    for(u32 _k = 0; _k < _ns; _k++) {
        const u32 _s = _deep[_k].i;
        for(i32 _r = _w->smin[_s]; _r <= _w->smax[_s]; _r++) {
            if(fude_zoom_graph_set_has(&_set, fude_zoom_graph_pair(_s, _r))) {
                continue;
            }
            fude_zoom_graph_add_vertex(_w, FUDE_ZOOM_GRAPH_VERTEX_FILLER, _s, _r, 0.0, 0.0);
            for(u32 _t = _s; _t != 0; _t = _w->parent[_t]) {
                fude_zoom_graph_set_add(&_set, fude_zoom_graph_pair(_t, _r));
            }
        }
    }
    fude_zoom_graph_tables_free(&_tables);
    rde_hash_set_free(&_set);
}

// Each vertex's neighbours above and below, from the chains (edges in order).
void fude_zoom_graph_neighbours(fude_zoom_graph_work* _w) {
    const u32 _nv = fude_zoom_graph_count(&_w->verts);
    const u32* _chain = (const u32*)_w->chain.memory;
    _w->up_first   = (u32*)fude_zoom_graph_table(&_w->tables, _nv + 1u, sizeof(u32));
    _w->down_first = (u32*)fude_zoom_graph_table(&_w->tables, _nv + 1u, sizeof(u32));
    u32 _segments = 0;
    for(u32 _e = 0; _e < _w->ne; _e++) {
        for(u32 _k = 0; _k + 1u < _w->chain_count[_e]; _k++) {
            _w->down_first[_chain[_w->chain_first[_e] + _k] + 1u]++;
            _w->up_first[_chain[_w->chain_first[_e] + _k + 1u] + 1u]++;
            _segments++;
        }
    }
    rde_arr _tables = fude_zoom_graph_tables_new();
    u32* _fill_up   = (u32*)fude_zoom_graph_table(&_tables, _nv + 1u, sizeof(u32));
    u32* _fill_down = (u32*)fude_zoom_graph_table(&_tables, _nv + 1u, sizeof(u32));
    for(u32 _i = 0; _i < _nv; _i++) {
        const u32 _du = _w->up_first[_i + 1u], _dd = _w->down_first[_i + 1u];
        _w->most_degree      = _du + _dd > _w->most_degree ? _du + _dd : _w->most_degree;
        _w->up_first[_i + 1u]   += _w->up_first[_i];
        _w->down_first[_i + 1u] += _w->down_first[_i];
        _fill_up[_i]   = _w->up_first[_i];
        _fill_down[_i] = _w->down_first[_i];
    }
    _w->up   = (u32*)fude_zoom_graph_table(&_w->tables, _segments, sizeof(u32));
    _w->down = (u32*)fude_zoom_graph_table(&_w->tables, _segments, sizeof(u32));
    for(u32 _e = 0; _e < _w->ne; _e++) {
        for(u32 _k = 0; _k + 1u < _w->chain_count[_e]; _k++) {
            const u32 _a = _chain[_w->chain_first[_e] + _k], _b = _chain[_w->chain_first[_e] + _k + 1u];
            _w->down[_fill_down[_a]++] = _b;
            _w->up[_fill_up[_b]++]     = _a;
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

// --- order within the ranks ----------------------------------------------------------------------

// Siblings' order, the same in every rank so their boxes never cross: by
// where their members were before when all of them have placed ones, else by
// where their members are now (across their ranks, on average).
RDE_INTERNAL void fude_zoom_graph_siblings(fude_zoom_graph_work* _w) {
    const u32 _ns = _w->ns, _nv = fude_zoom_graph_count(&_w->verts);
    if(_ns == 0) {
        return;
    }
    rde_arr _tables = fude_zoom_graph_tables_new();
    f64* _now   = (f64*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(f64));
    f64* _then  = (f64*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(f64));
    u32* _count = (u32*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(u32));
    u32* _fixed = (u32*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(u32));
    fude_zoom_graph_sorted* _kids = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _ns, sizeof(fude_zoom_graph_sorted));
    const fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    for(u32 _i = 0; _i < _nv; _i++) {
        const u32 _size = _w->layer_first[_v[_i].rank + 1] - _w->layer_first[_v[_i].rank];
        const f64 _at   = ((f64)_v[_i].order + 0.5) / (f64)_size;
        for(u32 _s = _v[_i].sub; _s != 0; _s = _w->parent[_s]) {
            _now[_s] += _at;
            _count[_s]++;
            if(_v[_i].kind == FUDE_ZOOM_GRAPH_VERTEX_NODE && _v[_i].fixed) {
                _then[_s] += _v[_i].prev;
                _fixed[_s]++;
            }
        }
    }
    for(u32 _p = 0; _p <= _ns; _p++) {
        u32 _nk = 0;
        b8  _all_fixed = true;
        for(u32 _s = 1; _s <= _ns; _s++) {
            if(_w->parent[_s] == _p) {
                _all_fixed = _all_fixed && _fixed[_s] > 0;
                _kids[_nk++].i = _s;
            }
        }
        for(u32 _k = 0; _k < _nk; _k++) {
            const u32 _s = _kids[_k].i;
            _kids[_k].k1 = _all_fixed ? _then[_s] / (f64)_fixed[_s] : (_count[_s] > 0 ? _now[_s] / (f64)_count[_s] : 0.0);
            _kids[_k].k2 = 0.0;
        }
        rde_sort(_kids, _nk, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
        for(u32 _k = 0; _k < _nk; _k++) {
            _w->sib[_kids[_k].i] = _k;
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

// One thing to order in a level of a rank: a vertex, or a subgraph's members together.
typedef struct {
    u32 group;     // the subgraph it stands for (0: a vertex)
    u32 vertex;
    u32 first;     // a subgraph's members in the gathered list
    u32 count;
    f64 key, tie;
    b8  fixed;
    f64 prev;
} fude_zoom_graph_item;

// _list (_m vertices of one rank, all inside subgraph _s) put in order in
// place: by key, each subgraph's members together, the subgraphs in their
// siblings' order and the fixed vertices of this level in theirs (each kept
// to the slots its kind had, so the rest still goes by key).
RDE_INTERNAL void fude_zoom_graph_arrange_level(fude_zoom_graph_work* _w, u32 _s, u32* _list, u32 _m) {
    if(_m == 0) {
        return;
    }
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    rde_arr _tables = fude_zoom_graph_tables_new();
    fude_zoom_graph_sorted* _by    = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _m, sizeof(fude_zoom_graph_sorted));
    fude_zoom_graph_sorted* _keyed = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _m, sizeof(fude_zoom_graph_sorted));
    fude_zoom_graph_sorted* _kind  = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _m, sizeof(fude_zoom_graph_sorted));
    fude_zoom_graph_item*   _items = (fude_zoom_graph_item*)fude_zoom_graph_table(&_tables, _m, sizeof(fude_zoom_graph_item));
    u32*                    _slots = (u32*)fude_zoom_graph_table(&_tables, _m, sizeof(u32));
    u32*                    _seq   = (u32*)fude_zoom_graph_table(&_tables, _m, sizeof(u32));
    for(u32 _i = 0; _i < _m; _i++) {
        _v[_list[_i]].group = fude_zoom_graph_child_of(_w, _v[_list[_i]].sub, _s);
        _by[_i] = (fude_zoom_graph_sorted){ (f64)_v[_list[_i]].group, (f64)_v[_list[_i]].order, _list[_i] };
    }
    rde_sort(_by, _m, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
    u32 _ni = 0;
    for(u32 _j = 0; _j < _m;) {
        const fude_zoom_graph_vertex* _x = &_v[_by[_j].i];
        fude_zoom_graph_item* _it = &_items[_ni++];
        rde_memset(_it, 0, sizeof(*_it));
        _it->first = _j;
        if(_x->group == 0) {
            _it->vertex = _by[_j].i;
            _it->count  = 1u;
            _it->key    = _x->key;
            _it->tie    = (f64)_x->order;
            _it->fixed  = _x->fixed;
            _it->prev   = _x->prev;
            _j++;
            continue;
        }
        _it->group  = _x->group;
        _it->vertex = FUDE_ZOOM_GRAPH_NONE;
        _it->tie    = (f64)_x->order;
        while(_j < _m && _v[_by[_j].i].group == _it->group) {
            _it->key += _v[_by[_j].i].key;
            _it->tie  = rde_math_min_f64(_it->tie, (f64)_v[_by[_j].i].order);
            _it->count++;
            _j++;
        }
        _it->key /= (f64)_it->count;
    }
    for(u32 _k = 0; _k < _ni; _k++) {
        _keyed[_k] = (fude_zoom_graph_sorted){ _items[_k].key, _items[_k].tie, _k };
    }
    rde_sort(_keyed, _ni, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
    for(u32 _k = 0; _k < _ni; _k++) {
        _seq[_k] = _keyed[_k].i;
    }
    // The subgraphs into their slots in their siblings' order, then the fixed
    // vertices into theirs in the order they had.
    for(u32 _pass = 0; _pass < 2u; _pass++) {
        u32 _nk = 0;
        for(u32 _k = 0; _k < _ni; _k++) {
            const fude_zoom_graph_item* _it = &_items[_seq[_k]];
            if(_pass == 0 ? _it->group != 0 : (_it->group == 0 && _it->fixed)) {
                _slots[_nk] = _k;
                _kind[_nk]  = (fude_zoom_graph_sorted){ _pass == 0 ? (f64)_w->sib[_it->group] : _it->prev, _pass == 0 ? 0.0 : (f64)_it->vertex, _seq[_k] };
                _nk++;
            }
        }
        rde_sort(_kind, _nk, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
        for(u32 _k = 0; _k < _nk; _k++) {
            _seq[_slots[_k]] = _kind[_k].i;
        }
    }
    u32 _at = 0;
    for(u32 _k = 0; _k < _ni; _k++) {
        const fude_zoom_graph_item* _it = &_items[_seq[_k]];
        for(u32 _j = 0; _j < _it->count; _j++) {
            _list[_at + _j] = _by[_it->first + _j].i;
        }
        if(_it->group != 0) {
            fude_zoom_graph_arrange_level(_w, _it->group, &_list[_at], _it->count);
        }
        _at += _it->count;
    }
    fude_zoom_graph_tables_free(&_tables);
}

// Puts rank _r in order by its vertices' keys (fude_zoom_graph_arrange_level from the top) and numbers them.
RDE_INTERNAL void fude_zoom_graph_arrange(fude_zoom_graph_work* _w, u32 _r) {
    u32* _list = &_w->layer[_w->layer_first[_r]];
    const u32 _m = _w->layer_first[_r + 1u] - _w->layer_first[_r];
    fude_zoom_graph_arrange_level(_w, 0, _list, _m);
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    for(u32 _i = 0; _i < _m; _i++) {
        _v[_list[_i]].order = _i;
    }
}

// Returns the edge crossings between each rank and the next, all told: for each rank, its edges down taken in order
// and counted against those before them with a Fenwick tree over the rank below.
RDE_INTERNAL u64 fude_zoom_graph_crossings(fude_zoom_graph_work* _w) {
    const fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    u32 _widest = 0;
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        const u32 _m = _w->layer_first[_r + 1u] - _w->layer_first[_r];
        _widest = _m > _widest ? _m : _widest;
    }
    rde_arr _tables = fude_zoom_graph_tables_new();
    u32* _tree = (u32*)fude_zoom_graph_table(&_tables, _widest + 1u, sizeof(u32));
    u32* _at   = (u32*)fude_zoom_graph_table(&_tables, _w->most_degree + 1u, sizeof(u32));
    u64  _total = 0;
    for(u32 _r = 0; _r + 1u < _w->ranks; _r++) {
        const u32 _below = _w->layer_first[_r + 2u] - _w->layer_first[_r + 1u];
        rde_memset(_tree, 0, (_below + 1u) * sizeof(u32));
        u32 _inserted = 0;
        for(u32 _i = _w->layer_first[_r]; _i < _w->layer_first[_r + 1u]; _i++) {
            const u32 _u = _w->layer[_i];
            u32 _n = 0;
            for(u32 _k = _w->down_first[_u]; _k < _w->down_first[_u + 1u]; _k++) {
                // Sorted as they go in (a vertex has few).
                u32 _p = _v[_w->down[_k]].order, _j = _n++;
                while(_j > 0 && _at[_j - 1u] > _p) {
                    _at[_j] = _at[_j - 1u];
                    _j--;
                }
                _at[_j] = _p;
            }
            for(u32 _k = 0; _k < _n; _k++) {
                u32 _not_after = 0;
                for(u32 _q = _at[_k] + 1u; _q > 0; _q -= _q & (0u - _q)) _not_after += _tree[_q];
                _total += _inserted - _not_after;
            }
            for(u32 _k = 0; _k < _n; _k++) {
                for(u32 _q = _at[_k] + 1u; _q <= _below; _q += _q & (0u - _q)) _tree[_q]++;
                _inserted++;
            }
        }
    }
    fude_zoom_graph_tables_free(&_tables);
    return _total;
}

// The ranks' order: first as a walk down from each node in turn found them,
// then barycentre sweeps, down and up by turns, each rank by where its
// neighbours are in the rank just done, the one with fewest crossings kept.
void fude_zoom_graph_order(fude_zoom_graph_work* _w) {
    const u32 _nv = fude_zoom_graph_count(&_w->verts);
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    // The walk.
    rde_arr _tables = fude_zoom_graph_tables_new();
    u32* _stack = (u32*)fude_zoom_graph_table(&_tables, _nv, sizeof(u32));
    u32* _it    = (u32*)fude_zoom_graph_table(&_tables, _nv, sizeof(u32));
    u8*  _seen  = (u8*)fude_zoom_graph_table(&_tables, _nv, sizeof(u8));
    u32  _disc  = 0;
    for(u32 _root = 0; _root < _nv; _root++) {
        if(_seen[_root]) {
            continue;
        }
        u32 _sp = 0;
        _stack[_sp++] = _root;
        _seen[_root]  = 1;
        _v[_root].disc = _disc++;
        _it[_root]    = _w->down_first[_root];
        while(_sp > 0) {
            const u32 _x = _stack[_sp - 1u];
            if(_it[_x] < _w->down_first[_x + 1u]) {
                const u32 _y = _w->down[_it[_x]++];
                if(!_seen[_y]) {
                    _seen[_y]      = 1;
                    _v[_y].disc    = _disc++;
                    _it[_y]        = _w->down_first[_y];
                    _stack[_sp++]  = _y;
                }
            } else {
                _sp--;
            }
        }
    }
    fude_zoom_graph_tables_clear(&_tables);
    // The ranks, each in the walk's order.
    _w->layer_first = (u32*)fude_zoom_graph_table(&_w->tables, _w->ranks + 1u, sizeof(u32));
    _w->layer       = (u32*)fude_zoom_graph_table(&_w->tables, _nv, sizeof(u32));
    fude_zoom_graph_sorted* _all = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _nv, sizeof(fude_zoom_graph_sorted));
    for(u32 _i = 0; _i < _nv; _i++) {
        _all[_i] = (fude_zoom_graph_sorted){ (f64)_v[_i].rank, (f64)_v[_i].disc, _i };
        _w->layer_first[_v[_i].rank + 1]++;
    }
    rde_sort(_all, _nv, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        _w->layer_first[_r + 1u] += _w->layer_first[_r];
    }
    for(u32 _i = 0; _i < _nv; _i++) {
        _w->layer[_i] = _all[_i].i;
        _v[_all[_i].i].order = _i - _w->layer_first[_v[_all[_i].i].rank];
    }
    fude_zoom_graph_tables_clear(&_tables);
    fude_zoom_graph_siblings(_w);
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        for(u32 _i = _w->layer_first[_r]; _i < _w->layer_first[_r + 1u]; _i++) {
            _v[_w->layer[_i]].key = (f64)_v[_w->layer[_i]].order;
        }
        fude_zoom_graph_arrange(_w, _r);
    }
    u32* _best       = (u32*)fude_zoom_graph_table(&_tables, _nv, sizeof(u32));
    u64  _best_cross = fude_zoom_graph_crossings(_w);
    rde_memcpy(_best, _w->layer, (usize)_nv * sizeof(u32));
    for(u32 _sweep = 0; _sweep < FUDE_ZOOM_GRAPH_SWEEPS && _best_cross > 0; _sweep++) {
        const b8 _down = (_sweep & 1u) == 0;
        fude_zoom_graph_siblings(_w);
        for(u32 _k = 0; _k < _w->ranks; _k++) {
            const u32 _r = _down ? _k : _w->ranks - 1u - _k;
            for(u32 _i = _w->layer_first[_r]; _i < _w->layer_first[_r + 1u]; _i++) {
                fude_zoom_graph_vertex* _x = &_v[_w->layer[_i]];
                const u32 _from = _down ? _w->up_first[_w->layer[_i]] : _w->down_first[_w->layer[_i]];
                const u32 _to   = _down ? _w->up_first[_w->layer[_i] + 1u] : _w->down_first[_w->layer[_i] + 1u];
                if(_k == 0 || _from == _to) {
                    _x->key = (f64)_x->order;
                    continue;
                }
                f64 _sum = 0.0;
                for(u32 _j = _from; _j < _to; _j++) {
                    _sum += (f64)_v[_down ? _w->up[_j] : _w->down[_j]].order;
                }
                _x->key = _sum / (f64)(_to - _from);
            }
            fude_zoom_graph_arrange(_w, _r);
        }
        const u64 _cross = fude_zoom_graph_crossings(_w);
        if(_cross < _best_cross) {
            _best_cross = _cross;
            rde_memcpy(_best, _w->layer, (usize)_nv * sizeof(u32));
        }
    }
    rde_memcpy(_w->layer, _best, (usize)_nv * sizeof(u32));
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        for(u32 _i = _w->layer_first[_r]; _i < _w->layer_first[_r + 1u]; _i++) {
            _v[_w->layer[_i]].order = _i - _w->layer_first[_r];
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}
