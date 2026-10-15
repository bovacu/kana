// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/graph_parts.h"

// ===========================================================================
// See graph.h, which has how the layout works. Graphs, the layout as the graph keeps it: each edge's route
// (fude_zoom_graph_routes, outline to outline), the layered layout's last steps out of flow space (the bends, the
// labels, the subgraphs' boxes), a mind map's layout (its branches shared out either side of its root), and
// fude_zoom_graph_layout, which takes every step in turn.
//
// The file goes: the routes; the bends, the labels and the boxes; the work freed; the mind map's layout; then
// fude_zoom_graph_layout's steps (fude_zoom_graph_layout_*, in the order it takes them) and fude_zoom_graph_layout.
//
// Part of the graph: graph_parts.h holds what the modules share and says which module holds what. Internal names start
// fude_zoom_graph_ as the public ones do. It keeps no state of its own: any thread may use a graph no other thread is
// using.
// ===========================================================================

// --- routes ------------------------------------------------------------------------------------

// Where the line from _n's centre towards _to leaves its outline: a circle's
// (an ellipse, if it was not given square), a diamond's, else its box.
RDE_INTERNAL fude_zoom_v2 fude_zoom_graph_outline(const fude_zoom_graph_node* _n, fude_zoom_v2 _to) {
    const f64 _dx = _to.x - _n->x, _dy = _to.y - _n->y;
    const f64 _hw = fude_zoom_graph_sane(_n->w) * 0.5, _hh = fude_zoom_graph_sane(_n->h) * 0.5;
    if((_dx == 0.0 && _dy == 0.0) || (_hw <= 0.0 && _hh <= 0.0)) {
        return (fude_zoom_v2){ _n->x, _n->y };
    }
    f64 _t;
    const b8 _round = _n->shape == FUDE_ZOOM_NODE_CIRCLE || _n->shape == FUDE_ZOOM_NODE_DOUBLE_CIRCLE;
    if(_round && _hw > 0.0 && _hh > 0.0) {
        _t = 1.0 / rde_math_sqrt_f64((_dx / _hw) * (_dx / _hw) + (_dy / _hh) * (_dy / _hh));
    } else if(_n->shape == FUDE_ZOOM_NODE_DIAMOND && _hw > 0.0 && _hh > 0.0) {
        _t = 1.0 / (rde_math_abs_f64(_dx) / _hw + rde_math_abs_f64(_dy) / _hh);
    } else {
        _t = rde_math_min_f64(_dx != 0.0 ? _hw / rde_math_abs_f64(_dx) : 1e300, _dy != 0.0 ? _hh / rde_math_abs_f64(_dy) : 1e300);
    }
    return (fude_zoom_v2){ _n->x + _dx * _t, _n->y + _dy * _t };
}

// An edge's end at a subgraph (index + 1; 0: none) as a box-shaped node, if
// the subgraph has a box.
RDE_INTERNAL void fude_zoom_graph_end_box(const fude_zoom_graph* _g, u32 _sub, fude_zoom_graph_node* _end) {
    if(_sub == 0 || _sub > fude_zoom_graph_count(&_g->subgraphs)) {
        return;
    }
    const fude_zoom_graph_subgraph* _s = fude_zoom_graph_subgraph_at(_g, _sub - 1u);
    if(_s->w > 0.0 && _s->h > 0.0) {
        _end->x     = _s->x;
        _end->y     = _s->y;
        _end->w     = _s->w;
        _end->h     = _s->h;
        _end->shape = FUDE_ZOOM_NODE_RECT;
    }
}

// Returns whether _p is strictly inside node _n's box.
RDE_INTERNAL b8 fude_zoom_graph_inside(const fude_zoom_graph_node* _n, fude_zoom_v2 _p) {
    return rde_math_abs_f64(_p.x - _n->x) < _n->w * 0.5 && rde_math_abs_f64(_p.y - _n->y) < _n->h * 0.5;
}

// Appends edge _e's line to _points (fude_zoom_graph_routes' line for one edge) and returns how many points; 0 when an
// end is not a node.
RDE_INTERNAL u32 fude_zoom_graph_route(const fude_zoom_graph* _g, u32 _e, rde_arr* _points) {
    const fude_zoom_graph_edge* _edge = fude_zoom_graph_edge_at(_g, _e);
    const u32 _n = fude_zoom_graph_count(&_g->nodes);
    if(_edge->from >= _n || _edge->to >= _n) {
        return 0;
    }
    fude_zoom_graph_node _a = *fude_zoom_graph_node_at(_g, _edge->from);
    fude_zoom_graph_node _b = *fude_zoom_graph_node_at(_g, _edge->to);
    fude_zoom_graph_end_box(_g, _edge->from_subgraph, &_a);
    fude_zoom_graph_end_box(_g, _edge->to_subgraph, &_b);
    // The bends, less those inside a box the edge ends at.
    const fude_zoom_v2* _bends = (const fude_zoom_v2*)_g->bends.memory + _edge->bend_first;
    u32 _first = 0, _last = (u64)_edge->bend_first + _edge->bend_count <= fude_zoom_graph_count(&_g->bends) ? _edge->bend_count : 0u;
    while(_edge->from_subgraph != 0 && _first < _last && fude_zoom_graph_inside(&_a, _bends[_first])) _first++;
    while(_edge->to_subgraph != 0 && _last > _first && fude_zoom_graph_inside(&_b, _bends[_last - 1u])) _last--;
    const fude_zoom_v2 _start = fude_zoom_graph_outline(&_a, _last > _first ? _bends[_first] : (fude_zoom_v2){ _b.x, _b.y });
    const fude_zoom_v2 _end   = fude_zoom_graph_outline(&_b, _last > _first ? _bends[_last - 1u] : (fude_zoom_v2){ _a.x, _a.y });
    rde_arr_add(_points, (any)&_start);
    for(u32 _k = _first; _k < _last; _k++) {
        rde_arr_add(_points, (any)&_bends[_k]);
    }
    rde_arr_add(_points, (any)&_end);
    return _last - _first + 2u;
}

// See the header.
void fude_zoom_graph_routes(const fude_zoom_graph* _g, rde_arr* _points, rde_arr* _starts) {
    rde_arr_clear(_points);
    rde_arr_clear(_starts);
    const u32 _ne = fude_zoom_graph_count(&_g->edges);
    for(u32 _e = 0; _e <= _ne; _e++) {
        const u32 _at = fude_zoom_graph_count(_points);
        rde_arr_add(_starts, (any)&_at);
        if(_e < _ne) {
            fude_zoom_graph_route(_g, _e, _points);
        }
    }
}

// --- the layout --------------------------------------------------------------------------------

// The bends (turned out of flow space), and where each label goes.
RDE_INTERNAL void fude_zoom_graph_bends(fude_zoom_graph_work* _w) {
    fude_zoom_graph* _g = _w->g;
    const u8 _dir = _g->direction;
    const fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    const u32* _chain = (const u32*)_w->chain.memory;
    rde_arr_clear(&_g->bends);
    // Edges side by side between the same two nodes, with no bend to keep them apart: each after the first bent aside
    // at its middle, by turns.
    rde_arr _tables = fude_zoom_graph_tables_new();
    fude_zoom_graph_sorted* _pairs = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _w->ne, sizeof(fude_zoom_graph_sorted));
    u32* _aside = (u32*)fude_zoom_graph_table(&_tables, _w->ne, sizeof(u32));
    u32  _np = 0;
    for(u32 _e = 0; _e < _w->ne; _e++) {
        if(_w->tail[_e] != FUDE_ZOOM_GRAPH_NONE && _w->tail[_e] != _w->head[_e] && _w->chain_count[_e] <= 2u) {
            _pairs[_np++] = (fude_zoom_graph_sorted){ (f64)_w->tail[_e], (f64)_w->head[_e], _e };
        }
    }
    rde_sort(_pairs, _np, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
    for(u32 _k = 1; _k < _np; _k++) {
        if(_pairs[_k].k1 == _pairs[_k - 1u].k1 && _pairs[_k].k2 == _pairs[_k - 1u].k2) {
            _aside[_pairs[_k].i] = _aside[_pairs[_k - 1u].i] + 1u;
        }
    }
    for(u32 _e = 0; _e < _w->ne; _e++) {
        fude_zoom_graph_edge* _edge = fude_zoom_graph_edge_at(_g, _e);
        _edge->bend_first = fude_zoom_graph_count(&_g->bends);
        _edge->bend_count = 0;
        const u32 _t = _w->tail[_e];
        if(_t == FUDE_ZOOM_GRAPH_NONE) {
            continue;
        }
        if(_t == _w->head[_e]) {
            // A loop out of its right side and back.
            const fude_zoom_graph_vertex* _x = &_v[_t];
            const f64 _out = _x->x + _x->w * 0.5 + _x->loop;
            const fude_zoom_v2 _two[2] = { fude_zoom_graph_turn(_dir, _out, _x->y - _x->h * 0.25), fude_zoom_graph_turn(_dir, _out, _x->y + _x->h * 0.25) };
            rde_arr_add(&_g->bends, (any)&_two[0]);
            rde_arr_add(&_g->bends, (any)&_two[1]);
            _edge->bend_count = 2u;
            continue;
        }
        const u32 _n = _w->chain_count[_e];
        for(u32 _k = 1; _k + 1u < _n; _k++) {
            const u32 _d = _chain[_w->chain_first[_e] + (_w->reversed[_e] ? _n - 1u - _k : _k)];
            const fude_zoom_v2 _p = fude_zoom_graph_turn(_dir, _v[_d].x, _v[_d].y);
            rde_arr_add(&_g->bends, (any)&_p);
            _edge->bend_count++;
        }
        if(_aside[_e] > 0) {
            const fude_zoom_graph_vertex *_a = &_v[_t], *_b = &_v[_w->head[_e]];
            const f64 _by = (f64)((_aside[_e] + 1u) / 2u) * _w->sp.node_gap * 0.5 * ((_aside[_e] & 1u) ? 1.0 : -1.0);
            const fude_zoom_v2 _p = fude_zoom_graph_turn(_dir, 0.5 * (_a->x + _b->x) + _by, 0.5 * (_a->y + _b->y));
            rde_arr_add(&_g->bends, (any)&_p);
            _edge->bend_count = 1u;
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

// Each label: on its own vertex if it has one; a loop's beside the loop; else
// halfway along the line as drawn.
RDE_INTERNAL void fude_zoom_graph_labels(fude_zoom_graph_work* _w) {
    fude_zoom_graph* _g = _w->g;
    const fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    rde_arr _points = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    for(u32 _e = 0; _e < _w->ne; _e++) {
        fude_zoom_graph_edge* _edge = fude_zoom_graph_edge_at(_g, _e);
        if(_w->label_vertex[_e] != FUDE_ZOOM_GRAPH_NONE) {
            const fude_zoom_graph_vertex* _x = &_v[_w->label_vertex[_e]];
            const fude_zoom_v2 _p = fude_zoom_graph_turn(_g->direction, _x->x, _x->y);
            _edge->label_x = _p.x + _w->shift_x;
            _edge->label_y = _p.y + _w->shift_y;
            continue;
        }
        if(_w->tail[_e] != FUDE_ZOOM_GRAPH_NONE && _w->tail[_e] == _w->head[_e]) {
            const fude_zoom_graph_vertex* _x = &_v[_w->tail[_e]];
            const fude_zoom_v2 _p = fude_zoom_graph_turn(_g->direction, _x->x + _x->w * 0.5 + _x->loop, _x->y);
            _edge->label_x = _p.x + _w->shift_x;
            _edge->label_y = _p.y + _w->shift_y;
            continue;
        }
        rde_arr_clear(&_points);
        const u32 _n = fude_zoom_graph_route(_g, _e, &_points);
        const fude_zoom_v2* _p = (const fude_zoom_v2*)_points.memory;
        f64 _length = 0.0;
        for(u32 _k = 1; _k < _n; _k++) {
            _length += rde_math_hypot_f64(_p[_k].x - _p[_k - 1u].x, _p[_k].y - _p[_k - 1u].y);
        }
        f64 _left = _length * 0.5;
        _edge->label_x = _n > 0 ? _p[0].x : 0.0;
        _edge->label_y = _n > 0 ? _p[0].y : 0.0;
        for(u32 _k = 1; _k < _n; _k++) {
            const f64 _d = rde_math_hypot_f64(_p[_k].x - _p[_k - 1u].x, _p[_k].y - _p[_k - 1u].y);
            if(_d >= _left && _d > 0.0) {
                _edge->label_x = _p[_k - 1u].x + (_p[_k].x - _p[_k - 1u].x) * _left / _d;
                _edge->label_y = _p[_k - 1u].y + (_p[_k].y - _p[_k - 1u].y) * _left / _d;
                break;
            }
            _left -= _d;
        }
    }
    rde_arr_free(&_points);
}

// Each subgraph's box: round its nodes (pinned ones too) and the boxes inside
// it, padded, then turned.
RDE_INTERNAL void fude_zoom_graph_boxes(fude_zoom_graph_work* _w) {
    const u32 _ns = _w->ns;
    if(_ns == 0) {
        return;
    }
    const fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    rde_arr _tables = fude_zoom_graph_tables_new();
    fude_zoom_box* _box = (fude_zoom_box*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(fude_zoom_box));
    for(u32 _s = 0; _s <= _ns; _s++) {
        _box[_s] = (fude_zoom_box){ 1e300, 1e300, -1e300, -1e300 };
    }
    for(u32 _i = 0; _i < _w->n; _i++) {
        const u32 _s = _v[_i].sub;
        if(_s != 0) {
            _box[_s].min_x = rde_math_min_f64(_box[_s].min_x, _v[_i].x - _v[_i].w * 0.5);
            _box[_s].max_x = rde_math_max_f64(_box[_s].max_x, _v[_i].x + _v[_i].w * 0.5 + _v[_i].loop);
            _box[_s].min_y = rde_math_min_f64(_box[_s].min_y, _v[_i].y - _v[_i].h * 0.5);
            _box[_s].max_y = rde_math_max_f64(_box[_s].max_y, _v[_i].y + _v[_i].h * 0.5);
        }
    }
    for(u32 _d = _w->most_depth; _d > 0; _d--) {
        for(u32 _s = 1; _s <= _ns; _s++) {
            fude_zoom_graph_subgraph* _sg = fude_zoom_graph_subgraph_at(_w->g, _s - 1u);
            if(_w->depth[_s] != _d) {
                continue;
            }
            if(_box[_s].min_x > _box[_s].max_x) {
                _sg->x = _sg->y = _sg->w = _sg->h = 0.0;
                continue;
            }
            _box[_s].min_x -= _w->pad[0];
            _box[_s].max_x += _w->pad[1];
            _box[_s].min_y -= _w->pad[2];
            _box[_s].max_y += _w->pad[3];
            const u32 _p = _w->parent[_s];
            if(_p != 0) {
                _box[_p].min_x = rde_math_min_f64(_box[_p].min_x, _box[_s].min_x);
                _box[_p].max_x = rde_math_max_f64(_box[_p].max_x, _box[_s].max_x);
                _box[_p].min_y = rde_math_min_f64(_box[_p].min_y, _box[_s].min_y);
                _box[_p].max_y = rde_math_max_f64(_box[_p].max_y, _box[_s].max_y);
            }
            const fude_zoom_v2 _a = fude_zoom_graph_turn(_w->g->direction, _box[_s].min_x, _box[_s].min_y);
            const fude_zoom_v2 _b = fude_zoom_graph_turn(_w->g->direction, _box[_s].max_x, _box[_s].max_y);
            _sg->x = 0.5 * (_a.x + _b.x);
            _sg->y = 0.5 * (_a.y + _b.y);
            _sg->w = rde_math_abs_f64(_b.x - _a.x);
            _sg->h = rde_math_abs_f64(_b.y - _a.y);
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

// Frees everything the layout's work holds.
RDE_INTERNAL void fude_zoom_graph_work_free(fude_zoom_graph_work* _w) {
    fude_zoom_graph_tables_free(&_w->tables);
    fude_zoom_graph_tables_free(&_w->rule_tables);
    if(rde_arr_is_inited(&_w->verts)) rde_arr_free(&_w->verts);
    if(rde_arr_is_inited(&_w->chain)) rde_arr_free(&_w->chain);
    if(rde_arr_is_inited(&_w->rules)) rde_arr_free(&_w->rules);
}

// --- a mind map's layout ---------------------------------------------------------------------------

// A mind map's layout: its tree and the gaps it keeps.
typedef struct {
    fude_zoom_graph* g;
    const u32*       first;   // each node's children: kids[first[n] .. first[n + 1])
    const u32*       kids;
    f64*             span;    // how tall each node's branch is
    f64              level, gap;   // between a node and its children; between siblings' branches
} fude_zoom_graph_mind;

// Returns, and keeps in span, how tall _n's branch is: its own height, or its children's branches stacked, whichever
// more. FUDE_ZOOM_GRAPH_MIND_DEPTH levels at most are followed.
RDE_INTERNAL f64 fude_zoom_graph_mind_span(fude_zoom_graph_mind* _m, u32 _n, u32 _depth) {
    const fude_zoom_graph_node* _node = fude_zoom_graph_node_at(_m->g, _n);
    f64 _sum = 0.0;
    for(u32 _k = _m->first[_n]; _k < _m->first[_n + 1u] && _depth < FUDE_ZOOM_GRAPH_MIND_DEPTH; _k++) {
        _sum += fude_zoom_graph_mind_span(_m, _m->kids[_k], _depth + 1u) + (_k > _m->first[_n] ? _m->gap : 0.0);
    }
    _m->span[_n] = rde_math_max_f64(_node->h, _sum);
    return _m->span[_n];
}

// _n's children (those in _which, _count of them) stacked down its side _side (1 right, -1 left), centred on it, and
// theirs after them.
RDE_INTERNAL void fude_zoom_graph_mind_place(fude_zoom_graph_mind* _m, u32 _n, const u32* _which, u32 _count, f64 _side, u32 _depth) {
    const fude_zoom_graph_node* _p = fude_zoom_graph_node_at(_m->g, _n);
    f64 _total = 0.0;
    for(u32 _i = 0; _i < _count; _i++) {
        _total += _m->span[_which[_i]] + (_i > 0u ? _m->gap : 0.0);
    }
    f64 _y = _p->y - _total * 0.5;
    for(u32 _i = 0; _i < _count; _i++) {
        fude_zoom_graph_node* _c = fude_zoom_graph_node_at(_m->g, _which[_i]);
        _c->x      = _p->x + _side * (_p->w * 0.5 + _m->level + _c->w * 0.5);
        _c->y      = _y + _m->span[_which[_i]] * 0.5;
        _c->placed = true;
        _y += _m->span[_which[_i]] + _m->gap;
        if(_depth < FUDE_ZOOM_GRAPH_MIND_DEPTH) {
            fude_zoom_graph_mind_place(_m, _which[_i], &_m->kids[_m->first[_which[_i]]], _m->first[_which[_i] + 1u] - _m->first[_which[_i]], _side, _depth + 1u);
        }
    }
}

// Lays a mind map out (see fude_zoom_graph_layout in the header): _level between a node and its children, _gap between
// siblings' branches. Each edge gets three bends along a curve, and its label at the middle between its ends.
RDE_INTERNAL void fude_zoom_graph_layout_mind(fude_zoom_graph* _g, f64 _level, f64 _gap) {
    const u32 _n = fude_zoom_graph_count(&_g->nodes), _ne = fude_zoom_graph_count(&_g->edges);
    rde_arr_clear(&_g->bends);
    if(_n == 0u) {
        return;
    }
    rde_arr _tables = fude_zoom_graph_tables_new();
    u32* _first = (u32*)fude_zoom_graph_table(&_tables, _n + 1u, sizeof(u32));
    u32* _kids  = (u32*)fude_zoom_graph_table(&_tables, _ne + 1u, sizeof(u32));
    f64* _span  = (f64*)fude_zoom_graph_table(&_tables, _n, sizeof(f64));
    b8*  _child = (b8*)fude_zoom_graph_table(&_tables, _n, sizeof(b8));
    // Each node's children, in the edges' order.
    for(u32 _e = 0; _e < _ne; _e++) {
        const fude_zoom_graph_edge* _ed = fude_zoom_graph_edge_at(_g, _e);
        if(_ed->from < _n && _ed->to < _n) {
            _first[_ed->from + 1u]++;
            _child[_ed->to] = true;
        }
    }
    for(u32 _i = 0; _i < _n; _i++) {
        _first[_i + 1u] += _first[_i];
    }
    {
        u32* _at = (u32*)fude_zoom_graph_table(&_tables, _n, sizeof(u32));
        for(u32 _e = 0; _e < _ne; _e++) {
            const fude_zoom_graph_edge* _ed = fude_zoom_graph_edge_at(_g, _e);
            if(_ed->from < _n && _ed->to < _n) {
                _kids[_first[_ed->from] + _at[_ed->from]++] = _ed->to;
            }
        }
    }
    fude_zoom_graph_mind _m = { _g, _first, _kids, _span, fude_zoom_graph_sane(_level), fude_zoom_graph_sane(_gap) };
    // Its root (the first node no edge comes to) where it is; its branches shared out.
    u32 _root = 0;
    while(_root < _n && _child[_root]) {
        _root++;
    }
    _root = _root < _n ? _root : 0u;
    fude_zoom_graph_node_at(_g, _root)->placed = true;
    fude_zoom_graph_mind_span(&_m, _root, 0u);
    const u32 _count = _first[_root + 1u] - _first[_root];
    u32* _right = (u32*)fude_zoom_graph_table(&_tables, _count + 1u, sizeof(u32));
    u32* _left  = (u32*)fude_zoom_graph_table(&_tables, _count + 1u, sizeof(u32));
    u32 _nr = 0, _nl = 0;
    f64 _sr = 0.0, _sl = 0.0;
    for(u32 _k = _first[_root]; _k < _first[_root + 1u]; _k++) {
        const u32 _c = _kids[_k];
        if(_sr <= _sl) {
            _right[_nr++] = _c;
            _sr += _span[_c] + _m.gap;
        } else {
            _left[_nl++] = _c;
            _sl += _span[_c] + _m.gap;
        }
    }
    fude_zoom_graph_mind_place(&_m, _root, _right, _nr, 1.0, 0u);
    fude_zoom_graph_mind_place(&_m, _root, _left, _nl, -1.0, 0u);
    // Each edge a curve out of its parent's side into its child's: three bends along it.
    for(u32 _e = 0; _e < _ne; _e++) {
        fude_zoom_graph_edge* _ed = fude_zoom_graph_edge_at(_g, _e);
        _ed->bend_first = fude_zoom_graph_count(&_g->bends);
        _ed->bend_count = 0u;
        _ed->label_x = _ed->label_y = 0.0;
        if(_ed->from >= _n || _ed->to >= _n) {
            continue;
        }
        const fude_zoom_graph_node* _a = fude_zoom_graph_node_at(_g, _ed->from);
        const fude_zoom_graph_node* _b = fude_zoom_graph_node_at(_g, _ed->to);
        const f64 _side = _b->x >= _a->x ? 1.0 : -1.0;
        const fude_zoom_v2 _p0 = { _a->x + _side * _a->w * 0.5, _a->y }, _p3 = { _b->x - _side * _b->w * 0.5, _b->y };
        const f64 _pull = rde_math_abs_f64(_p3.x - _p0.x) * 0.5;
        const fude_zoom_v2 _p1 = { _p0.x + _side * _pull, _p0.y }, _p2 = { _p3.x - _side * _pull, _p3.y };
        for(u32 _i = 1; _i <= 3u; _i++) {
            const f64 _t = (f64)_i / 4.0, _u = 1.0 - _t;
            const fude_zoom_v2 _q = { _u * _u * _u * _p0.x + 3.0 * _u * _u * _t * _p1.x + 3.0 * _u * _t * _t * _p2.x + _t * _t * _t * _p3.x,
                                      _u * _u * _u * _p0.y + 3.0 * _u * _u * _t * _p1.y + 3.0 * _u * _t * _t * _p2.y + _t * _t * _t * _p3.y };
            rde_arr_add(&_g->bends, (any)&_q);
            _ed->bend_count++;
        }
        _ed->label_x = (_p0.x + _p3.x) * 0.5;
        _ed->label_y = (_p0.y + _p3.y) * 0.5;
    }
    fude_zoom_graph_tables_free(&_tables);
}

// The layout's start: the boxes' padding in flow space (the title on the page's top side), the work's tables, the
// subgraphs as a tree, and their places among their siblings.
RDE_INTERNAL void fude_zoom_graph_layout_start(fude_zoom_graph_work* _w, u8 _dir) {
    // The box's padding in flow space: the title goes on the page's top side.
    for(u32 _k = 0; _k < 4u; _k++) {
        _w->pad[_k] = _w->sp.subgraph_pad;
    }
    _w->pad[_dir == FUDE_ZOOM_FLOW_TB ? 2 : (_dir == FUDE_ZOOM_FLOW_BT ? 3 : 0)] += _w->sp.subgraph_title;
    _w->tables      = fude_zoom_graph_tables_new();
    _w->rule_tables = fude_zoom_graph_tables_new();
    fude_zoom_graph_tree(_w);
    _w->sib = (u32*)fude_zoom_graph_table(&_w->tables, _w->ns + 1u, sizeof(u32));
}

// The nodes made vertices (first, at their own index), where they were (_dir: the graph's direction; _side: it
// flows across): a pinned one where it is. Returns where each node was before (a table of the work's, freed with
// it), for the shift at the end.
RDE_INTERNAL fude_zoom_v2* fude_zoom_graph_layout_nodes(fude_zoom_graph_work* _w, u8 _dir, b8 _side) {
    fude_zoom_graph* _g = _w->g;
    // The nodes as vertices (first, at their own index), where they were.
    _w->verts = rde_arr_new(sizeof(fude_zoom_graph_vertex), rde_memory_allocator_get_default_std());
    fude_zoom_v2* _before = (fude_zoom_v2*)fude_zoom_graph_table(&_w->tables, _w->n, sizeof(fude_zoom_v2));   // (freed with the work)
    for(u32 _i = 0; _i < _w->n; _i++) {
        const fude_zoom_graph_node* _node = fude_zoom_graph_node_at(_g, _i);
        const f64 _nw = fude_zoom_graph_sane(_node->w), _nh = fude_zoom_graph_sane(_node->h);
        const u32 _v  = fude_zoom_graph_add_vertex(_w, FUDE_ZOOM_GRAPH_VERTEX_NODE, _node->subgraph <= _w->ns ? _node->subgraph : 0u, 0, _side ? _nh : _nw, _side ? _nw : _nh);
        fude_zoom_graph_vertex* _x = &fude_zoom_graph_v(_w)[_v];
        const b8 _where = rde_math_is_finite_f64(_node->x) && rde_math_is_finite_f64(_node->y);
        const fude_zoom_v2 _f = fude_zoom_graph_unturn(_dir, _node->x, _node->y);
        _x->node   = _i;
        _x->pinned = _node->pinned && _where;
        _x->fixed  = (_node->placed || _node->pinned) && _where;
        _x->prev   = _f.x;
        _before[_i] = (fude_zoom_v2){ _node->x, _node->y };
        if(_x->pinned) {
            _x->x = _f.x;
            _x->y = _f.y;
            _w->any_pinned = true;
        }
    }
    return _before;
}

// The edges: their ends and lengths (all doubled when any label keeps a rank of its own), and a self-loop's room.
RDE_INTERNAL void fude_zoom_graph_layout_edges(fude_zoom_graph_work* _w) {
    fude_zoom_graph* _g = _w->g;
    // The edges: ends, lengths, and whether labels keep ranks of their own.
    _w->tail     = (u32*)fude_zoom_graph_table(&_w->tables, _w->ne, sizeof(u32));
    _w->head     = (u32*)fude_zoom_graph_table(&_w->tables, _w->ne, sizeof(u32));
    _w->minlen   = (u32*)fude_zoom_graph_table(&_w->tables, _w->ne, sizeof(u32));
    _w->reversed = (b8*)fude_zoom_graph_table(&_w->tables, _w->ne, sizeof(b8));
    for(u32 _e = 0; _e < _w->ne; _e++) {
        const fude_zoom_graph_edge* _edge = fude_zoom_graph_edge_at(_g, _e);
        _w->labels = _w->labels || (_edge->from < _w->n && _edge->to < _w->n && _edge->from != _edge->to &&
                                  (fude_zoom_graph_sane(_edge->label_w) > 0.0 || fude_zoom_graph_sane(_edge->label_h) > 0.0));
    }
    for(u32 _e = 0; _e < _w->ne; _e++) {
        const fude_zoom_graph_edge* _edge = fude_zoom_graph_edge_at(_g, _e);
        const b8 _ok  = _edge->from < _w->n && _edge->to < _w->n;
        const u32 _len = _edge->min_length == 0 ? 1u : (_edge->min_length < FUDE_ZOOM_GRAPH_MAX_LENGTH ? _edge->min_length : FUDE_ZOOM_GRAPH_MAX_LENGTH);
        _w->tail[_e]   = _ok ? _edge->from : FUDE_ZOOM_GRAPH_NONE;
        _w->head[_e]   = _ok ? _edge->to : FUDE_ZOOM_GRAPH_NONE;
        _w->minlen[_e] = _len * (_w->labels ? 2u : 1u);
        if(_ok && _edge->from == _edge->to) {
            fude_zoom_graph_vertex* _x = &fude_zoom_graph_v(_w)[_edge->from];
            _x->loop = rde_math_max_f64(_w->sp.node_gap * 0.5, _x->h * 0.25);
        }
    }
}

// The shift to where it all goes, unless a node is pinned (it stays): where the placed nodes were (_before), on
// average, or else with its corner at 0, 0; nodes, bends and boxes alike. _v: the vertices, as placed.
RDE_INTERNAL void fude_zoom_graph_layout_shift(fude_zoom_graph_work* _w, const fude_zoom_graph_vertex* _v, const fude_zoom_v2* _before) {
    fude_zoom_graph* _g = _w->g;
    // Where it all goes: pinned, it stays; else where the placed nodes were,
    // on average; else with its corner at 0, 0.
    if(!_w->any_pinned) {
        f64 _dx = 0.0, _dy = 0.0;
        u32 _placed = 0;
        for(u32 _i = 0; _i < _w->n; _i++) {
            if(_v[_i].fixed) {
                const fude_zoom_graph_node* _node = fude_zoom_graph_node_at(_g, _i);
                _dx += _before[_i].x - _node->x;
                _dy += _before[_i].y - _node->y;
                _placed++;
            }
        }
        if(_placed > 0) {
            _dx /= (f64)_placed;
            _dy /= (f64)_placed;
        } else {
            f64 _lx = 1e300, _ly = 1e300;
            for(u32 _i = 0; _i < _w->n; _i++) {
                const fude_zoom_graph_node* _node = fude_zoom_graph_node_at(_g, _i);
                _lx = rde_math_min_f64(_lx, _node->x - fude_zoom_graph_sane(_node->w) * 0.5);
                _ly = rde_math_min_f64(_ly, _node->y - fude_zoom_graph_sane(_node->h) * 0.5);
            }
            for(u32 _s = 0; _s < _w->ns; _s++) {
                const fude_zoom_graph_subgraph* _sg = fude_zoom_graph_subgraph_at(_g, _s);
                if(_sg->w > 0.0 || _sg->h > 0.0) {
                    _lx = rde_math_min_f64(_lx, _sg->x - _sg->w * 0.5);
                    _ly = rde_math_min_f64(_ly, _sg->y - _sg->h * 0.5);
                }
            }
            _dx = -_lx;
            _dy = -_ly;
        }
        _w->shift_x = _dx;
        _w->shift_y = _dy;
        for(u32 _i = 0; _i < _w->n; _i++) {
            fude_zoom_graph_node_at(_g, _i)->x += _dx;
            fude_zoom_graph_node_at(_g, _i)->y += _dy;
        }
        fude_zoom_v2* _bends = (fude_zoom_v2*)_g->bends.memory;
        for(u32 _k = 0; _k < fude_zoom_graph_count(&_g->bends); _k++) {
            _bends[_k].x += _dx;
            _bends[_k].y += _dy;
        }
        for(u32 _s = 0; _s < _w->ns; _s++) {
            fude_zoom_graph_subgraph* _sg = fude_zoom_graph_subgraph_at(_g, _s);
            if(_sg->w > 0.0 || _sg->h > 0.0) {
                _sg->x += _dx;
                _sg->y += _dy;
            }
        }
    }
}

// See the header. The steps: the nodes made vertices (pinned ones where they are), the edges' ends and lengths (all
// doubled when any label keeps a rank of its own), cycles broken, ranks, chains, fillers, neighbours, the order in
// each rank, the places, then out of flow space: the bends, the boxes, the shift to where it goes, and the labels.
// Its own steps are the fude_zoom_graph_layout_* above, each with its comment.
void fude_zoom_graph_layout(fude_zoom_graph* _g, fude_zoom_graph_spacing _sp) {
    if(_g->direction == FUDE_ZOOM_FLOW_MIND) {
        fude_zoom_graph_layout_mind(_g, _sp.rank_gap, _sp.node_gap);
        return;
    }
    fude_zoom_graph_work _w;
    rde_memset(&_w, 0, sizeof(_w));
    _w.g  = _g;
    _w.sp = (fude_zoom_graph_spacing){ fude_zoom_graph_sane(_sp.rank_gap), fude_zoom_graph_sane(_sp.node_gap),
                                       fude_zoom_graph_sane(_sp.subgraph_pad), fude_zoom_graph_sane(_sp.subgraph_title) };
    _w.n  = fude_zoom_graph_count(&_g->nodes);
    _w.ne = fude_zoom_graph_count(&_g->edges);
    _w.ns = fude_zoom_graph_count(&_g->subgraphs);
    const u8 _dir  = _g->direction <= FUDE_ZOOM_FLOW_RL ? _g->direction : FUDE_ZOOM_FLOW_TB;
    _g->direction  = _dir;
    const b8 _side = fude_zoom_graph_sideways(_dir);
    for(u32 _s = 0; _s < _w.ns; _s++) {
        fude_zoom_graph_subgraph* _sg = fude_zoom_graph_subgraph_at(_g, _s);
        _sg->x = _sg->y = _sg->w = _sg->h = 0.0;
    }
    if(_w.n == 0) {
        rde_arr_clear(&_g->bends);
        for(u32 _e = 0; _e < _w.ne; _e++) {
            fude_zoom_graph_edge_at(_g, _e)->bend_count = 0;
        }
        return;
    }
    fude_zoom_graph_layout_start(&_w, _dir);
    fude_zoom_v2* _before = fude_zoom_graph_layout_nodes(&_w, _dir, _side);
    fude_zoom_graph_layout_edges(&_w);
    fude_zoom_graph_break_cycles(&_w);
    fude_zoom_graph_rank(&_w);
    fude_zoom_graph_chains(&_w);
    fude_zoom_graph_fillers(&_w);
    _w.nv = fude_zoom_graph_count(&_w.verts);
    fude_zoom_graph_neighbours(&_w);
    fude_zoom_graph_order(&_w);
    fude_zoom_graph_place(&_w);
    // Out of flow space.
    const fude_zoom_graph_vertex* _v = fude_zoom_graph_v(&_w);
    for(u32 _i = 0; _i < _w.n; _i++) {
        if(!_v[_i].pinned) {
            fude_zoom_graph_node* _node = fude_zoom_graph_node_at(_g, _i);
            const fude_zoom_v2 _p = fude_zoom_graph_turn(_dir, _v[_i].x, _v[_i].y);
            _node->x = _p.x;
            _node->y = _p.y;
        }
    }
    fude_zoom_graph_bends(&_w);
    fude_zoom_graph_boxes(&_w);
    fude_zoom_graph_layout_shift(&_w, _v, _before);
    fude_zoom_graph_labels(&_w);
    for(u32 _i = 0; _i < _w.n; _i++) {
        fude_zoom_graph_node_at(_g, _i)->placed = true;
    }
    fude_zoom_graph_work_free(&_w);
}
