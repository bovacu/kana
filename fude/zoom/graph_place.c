// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/graph_parts.h"

// ===========================================================================
// See graph.h, which has how the layout works. Graphs, the layered layout's places along each rank
// (fude_zoom_graph_place): the gaps kept, each row's exact fit towards where its neighbours draw it (isotonic, under
// the rules "x of b at least x of a + gap"), the subgraphs' boxes made whole, long edges straightened, the pinned nodes
// kept clear of, and each rank's line along the flow.
//
// The file goes: the gaps, the fit and its scratch, the rules, a row nudged, the boxes drawn in and made whole, long
// edges straightened, the pinned nodes avoided, the ranks' lines, then fude_zoom_graph_place, which takes them in turn.
//
// Part of the graph: graph_parts.h holds what the modules share and says which module holds what. Internal names start
// fude_zoom_graph_ as the public ones do. It keeps no state of its own: any thread may use a graph no other thread is
// using.
// ===========================================================================

#define FUDE_ZOOM_GRAPH_NUDGES  8u           // placing passes, each down then up (and half as many inside the boxes)

// --- places ------------------------------------------------------------------------------------

// How much of node_gap a vertex keeps from its neighbours: a node all of it,
// an edge's bend half.
RDE_INTERNAL f64 fude_zoom_graph_room(const fude_zoom_graph_vertex* _a) {
    return _a->kind == FUDE_ZOOM_GRAPH_VERTEX_NODE || _a->kind == FUDE_ZOOM_GRAPH_VERTEX_LABEL ? 1.0 : 0.5;
}

// Returns the gap kept between two vertices side by side: node_gap times the larger of their rooms.
RDE_INTERNAL f64 fude_zoom_graph_gap(const fude_zoom_graph_work* _w, const fude_zoom_graph_vertex* _a, const fude_zoom_graph_vertex* _b) {
    return _w->sp.node_gap * rde_math_max_f64(fude_zoom_graph_room(_a), fude_zoom_graph_room(_b));
}

// How far apart the centres of _a and _b (next to it on its right) must be:
// their halves, the gap, and the padding of every box edge between them.
RDE_INTERNAL f64 fude_zoom_graph_sep(const fude_zoom_graph_work* _w, const fude_zoom_graph_vertex* _a, const fude_zoom_graph_vertex* _b) {
    const u32 _c   = fude_zoom_graph_common(_w, _a->sub, _b->sub);
    const f64 _out = (f64)(_w->depth[_a->sub] - _w->depth[_c]), _in = (f64)(_w->depth[_b->sub] - _w->depth[_c]);
    return _a->w * 0.5 + _a->loop + _b->w * 0.5 + fude_zoom_graph_gap(_w, _a, _b) + _out * _w->pad[1] + _in * _w->pad[0];
}

// The x that keep their order, each at least _sep[i] from the next, nearest
// (least squares, by _weight) to _want: pool adjacent violators, on the
// wants less the gaps' running sum (which makes the gaps a plain order).
RDE_INTERNAL void fude_zoom_graph_fit(f64* _x, const f64* _want, const f64* _weight, const f64* _sep, u32 _m, f64* _off, f64* _bv, f64* _bw, u32* _bstart) {
    u32 _nb = 0;
    for(u32 _i = 0; _i < _m; _i++) {
        _off[_i]    = _i == 0 ? 0.0 : _off[_i - 1u] + _sep[_i - 1u];
        _bv[_nb]    = _want[_i] - _off[_i];
        _bw[_nb]    = _weight[_i];
        _bstart[_nb] = _i;
        _nb++;
        while(_nb > 1u && _bv[_nb - 2u] > _bv[_nb - 1u]) {
            const f64 _tw = _bw[_nb - 2u] + _bw[_nb - 1u];
            _bv[_nb - 2u] = (_bv[_nb - 2u] * _bw[_nb - 2u] + _bv[_nb - 1u] * _bw[_nb - 1u]) / _tw;
            _bw[_nb - 2u] = _tw;
            _nb--;
        }
    }
    for(u32 _b = 0; _b < _nb; _b++) {
        const u32 _end = _b + 1u < _nb ? _bstart[_b + 1u] : _m;
        for(u32 _i = _bstart[_b]; _i < _end; _i++) {
            _x[_i] = _bv[_b] + _off[_i];
        }
    }
}

// The scratch arrays a row's fit uses, sized once for the widest row (near: for the most neighbours).
typedef struct {
    f64* want;
    f64* weight;
    f64* sep;
    f64* x;
    f64* off;
    f64* bv;
    f64* bw;
    u32* bstart;
    f64* near;     // a vertex's neighbours' x
} fude_zoom_graph_scratch;

// Which neighbours a nudge draws a row towards: bits.
enum { FUDE_ZOOM_GRAPH_ABOVE = 1, FUDE_ZOOM_GRAPH_BELOW = 2 };

// Returns the x of a vertex or of a box's edge (variables nv + 2s and nv + 2s + 1, s the subgraph's index).
RDE_INTERNAL f64 fude_zoom_graph_at(const fude_zoom_graph_work* _w, u32 _var) {
    return _var < _w->nv ? fude_zoom_graph_v(_w)[_var].x : _w->wall[_var - _w->nv];
}

// Sets the x of a vertex or of a box's edge (as fude_zoom_graph_at reads it).
RDE_INTERNAL void fude_zoom_graph_set_at(fude_zoom_graph_work* _w, u32 _var, f64 _x) {
    if(_var < _w->nv) {
        fude_zoom_graph_v(_w)[_var].x = _x;
    } else {
        _w->wall[_var - _w->nv] = _x;
    }
}

// Adds the rule "x of _b at least x of _a + _gap".
RDE_INTERNAL void fude_zoom_graph_rule_add(rde_arr* _rules, u32 _a, u32 _b, f64 _gap) {
    const fude_zoom_graph_rule _rule = { _a, _b, _gap };
    rde_arr_add(_rules, (any)&_rule);
}

// The rules, "x of b at least x of a + gap", for each two things side by side
// in a row: the vertices, and the subgraph boxes' left and right edges, each
// one line through all its box's ranks. From a vertex to the next along a
// row: out of each box it is in and the other is not (each right edge its
// padding further), the gap, into each box the other is in (each left edge
// its padding further). _boxes false: the vertices alone, as far apart as
// all that adds up to. False if the rules go round in a circle (they should
// not, the siblings being in one order in every rank).
RDE_INTERNAL b8 fude_zoom_graph_rules(fude_zoom_graph_work* _w, b8 _boxes) {
    const u32 _nv = _w->nv, _ns = _boxes ? _w->ns : 0u;
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    _w->nvars = _nv + 2u * _ns;
    rde_arr_clear(&_w->rules);
    rde_arr _tables = fude_zoom_graph_tables_new();
    u32* _pa = (u32*)fude_zoom_graph_table(&_tables, _w->most_depth + 1u, sizeof(u32));
    u32* _pb = (u32*)fude_zoom_graph_table(&_tables, _w->most_depth + 1u, sizeof(u32));
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        u32 _a = FUDE_ZOOM_GRAPH_NONE, _na = 0;
        for(u32 _i = _w->row_first[_r]; _i <= _w->row_first[_r + 1u]; _i++) {
            const b8  _last = _i == _w->row_first[_r + 1u];
            const u32 _b    = _last ? FUDE_ZOOM_GRAPH_NONE : _w->row[_i];
            if(!_boxes) {
                if(_a != FUDE_ZOOM_GRAPH_NONE && !_last) {
                    fude_zoom_graph_rule_add(&_w->rules, _a, _b, fude_zoom_graph_sep(_w, &_v[_a], &_v[_b]));
                }
                _a = _b;
                continue;
            }
            const u32 _nb = _last ? 0u : fude_zoom_graph_path(_w, _v[_b].sub, _pb);
            u32 _lca = 0;
            while(_a != FUDE_ZOOM_GRAPH_NONE && _lca < _na && _lca < _nb && _pa[_lca] == _pb[_lca]) _lca++;
            u32 _cur = _a;
            f64 _off = _a != FUDE_ZOOM_GRAPH_NONE ? _v[_a].w * 0.5 + _v[_a].loop : 0.0;
            for(u32 _k = _na; _a != FUDE_ZOOM_GRAPH_NONE && _k > _lca; _k--) {
                const u32 _right = _nv + 2u * (_pa[_k - 1u] - 1u) + 1u;
                fude_zoom_graph_rule_add(&_w->rules, _cur, _right, _off + _w->pad[1]);
                _cur = _right;
                _off = 0.0;
            }
            if(_last) {
                break;
            }
            f64 _space = _a != FUDE_ZOOM_GRAPH_NONE ? fude_zoom_graph_gap(_w, &_v[_a], &_v[_b]) : 0.0;
            for(u32 _k = _lca; _k < _nb; _k++) {
                const u32 _left = _nv + 2u * (_pb[_k] - 1u);
                if(_cur != FUDE_ZOOM_GRAPH_NONE) {
                    fude_zoom_graph_rule_add(&_w->rules, _cur, _left, _off + _space);
                }
                _cur   = _left;
                _off   = 0.0;
                _space = _w->pad[0];
            }
            if(_cur != FUDE_ZOOM_GRAPH_NONE) {
                fude_zoom_graph_rule_add(&_w->rules, _cur, _b, _off + _space + _v[_b].w * 0.5);
            }
            _a  = _b;
            _na = _nb;
            rde_memcpy(_pa, _pb, (usize)_nb * sizeof(u32));
        }
    }
    fude_zoom_graph_tables_clear(&_tables);
    // Each variable's rules both ways, and an order every rule goes forward in
    // (Kahn's, by index on ties).
    const u32 _nr = fude_zoom_graph_count(&_w->rules), _nvar = _w->nvars;
    const fude_zoom_graph_rule* _rule = (const fude_zoom_graph_rule*)_w->rules.memory;
    fude_zoom_graph_tables_clear(&_w->rule_tables);   // (the last time's)
    _w->rin_first  = (u32*)fude_zoom_graph_table(&_w->rule_tables, _nvar + 1u, sizeof(u32));
    _w->rout_first = (u32*)fude_zoom_graph_table(&_w->rule_tables, _nvar + 1u, sizeof(u32));
    _w->rin        = (u32*)fude_zoom_graph_table(&_w->rule_tables, _nr, sizeof(u32));
    _w->rout       = (u32*)fude_zoom_graph_table(&_w->rule_tables, _nr, sizeof(u32));
    _w->topo       = (u32*)fude_zoom_graph_table(&_w->rule_tables, _nvar, sizeof(u32));
    u32* _fill_in  = (u32*)fude_zoom_graph_table(&_tables, _nvar + 1u, sizeof(u32));
    u32* _fill_out = (u32*)fude_zoom_graph_table(&_tables, _nvar + 1u, sizeof(u32));
    u32* _in       = (u32*)fude_zoom_graph_table(&_tables, _nvar, sizeof(u32));
    for(u32 _k = 0; _k < _nr; _k++) {
        _w->rout_first[_rule[_k].a + 1u]++;
        _w->rin_first[_rule[_k].b + 1u]++;
    }
    for(u32 _i = 0; _i < _nvar; _i++) {
        _in[_i] = _w->rin_first[_i + 1u];
        _w->rout_first[_i + 1u] += _w->rout_first[_i];
        _w->rin_first[_i + 1u]  += _w->rin_first[_i];
        _fill_out[_i] = _w->rout_first[_i];
        _fill_in[_i]  = _w->rin_first[_i];
    }
    for(u32 _k = 0; _k < _nr; _k++) {
        _w->rout[_fill_out[_rule[_k].a]++] = _k;
        _w->rin[_fill_in[_rule[_k].b]++]   = _k;
    }
    u32 _nt = 0, _at = 0;
    for(u32 _i = 0; _i < _nvar; _i++) {
        if(_in[_i] == 0) _w->topo[_nt++] = _i;
    }
    while(_at < _nt) {
        const u32 _x = _w->topo[_at++];
        for(u32 _k = _w->rout_first[_x]; _k < _w->rout_first[_x + 1u]; _k++) {
            if(--_in[_rule[_w->rout[_k]].b] == 0) _w->topo[_nt++] = _rule[_w->rout[_k]].b;
        }
    }
    fude_zoom_graph_tables_free(&_tables);
    return _nt == _nvar;
}

// The least and most x _var may have, its rules' other ends where they are.
// _walls_only: only the box edges count (the row's vertices fit among
// themselves).
RDE_INTERNAL void fude_zoom_graph_room_of(const fude_zoom_graph_work* _w, u32 _var, b8 _walls_only, f64* _lo, f64* _hi) {
    const fude_zoom_graph_rule* _rule = (const fude_zoom_graph_rule*)_w->rules.memory;
    *_lo = -1e300;
    *_hi = 1e300;
    for(u32 _k = _w->rin_first[_var]; _k < _w->rin_first[_var + 1u]; _k++) {
        const fude_zoom_graph_rule* _q = &_rule[_w->rin[_k]];
        if(!_walls_only || _q->a >= _w->nv) *_lo = rde_math_max_f64(*_lo, fude_zoom_graph_at(_w, _q->a) + _q->gap);
    }
    for(u32 _k = _w->rout_first[_var]; _k < _w->rout_first[_var + 1u]; _k++) {
        const fude_zoom_graph_rule* _q = &_rule[_w->rout[_k]];
        if(!_walls_only || _q->b >= _w->nv) *_hi = rde_math_min_f64(*_hi, fude_zoom_graph_at(_w, _q->b) - _q->gap);
    }
}

// Whether a box edge stands between _a and _b, side by side in a row.
RDE_INTERNAL b8 fude_zoom_graph_walled(const fude_zoom_graph_work* _w, u32 _a, u32 _b) {
    const fude_zoom_graph_rule* _rule = (const fude_zoom_graph_rule*)_w->rules.memory;
    return !(_w->rout_first[_a + 1u] - _w->rout_first[_a] == 1u && _rule[_w->rout[_w->rout_first[_a]]].b == _b);
}

// Rank _r's row drawn towards the median of each one's neighbours (above,
// below or both), keeping order and gaps: an exact fit for each run of it
// between box edges (_walls; else the whole row as one), and kept between the
// edges at its ends, which stay where they are.
RDE_INTERNAL void fude_zoom_graph_nudge(fude_zoom_graph_work* _w, u32 _r, u32 _from, b8 _walls, fude_zoom_graph_scratch* _s) {
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    const u32* _row = &_w->row[_w->row_first[_r]];
    const u32  _m   = _w->row_first[_r + 1u] - _w->row_first[_r];
    for(u32 _i = 0; _i < _m; _i++) {
        const u32 _a = _row[_i];
        u32 _n = 0;
        if(_from & FUDE_ZOOM_GRAPH_ABOVE) {
            for(u32 _k = _w->up_first[_a]; _k < _w->up_first[_a + 1u]; _k++) _s->near[_n++] = _v[_w->up[_k]].x;
        }
        if(_from & FUDE_ZOOM_GRAPH_BELOW) {
            for(u32 _k = _w->down_first[_a]; _k < _w->down_first[_a + 1u]; _k++) _s->near[_n++] = _v[_w->down[_k]].x;
        }
        for(u32 _k = 1; _k < _n; _k++) {
            const f64 _t = _s->near[_k];
            u32 _j = _k;
            for(; _j > 0 && _s->near[_j - 1u] > _t; _j--) _s->near[_j] = _s->near[_j - 1u];
            _s->near[_j] = _t;
        }
        if(_n == 0) {
            _s->want[_i]   = _v[_a].x;
            _s->weight[_i] = 0.05;
        } else {
            _s->want[_i]   = (_n & 1u) ? _s->near[_n / 2u] : 0.5 * (_s->near[_n / 2u - 1u] + _s->near[_n / 2u]);
            _s->weight[_i] = _v[_a].kind == FUDE_ZOOM_GRAPH_VERTEX_NODE ? 1.0 : 2.0;   // straighter long edges
        }
        if(_i + 1u < _m) {
            _s->sep[_i] = fude_zoom_graph_sep(_w, &_v[_a], &_v[_row[_i + 1u]]);
        }
    }
    for(u32 _a = 0; _a < _m;) {
        u32 _b = _a + 1u;
        while(_b < _m && !(_walls && fude_zoom_graph_walled(_w, _row[_b - 1u], _row[_b]))) _b++;
        const u32 _len = _b - _a;
        fude_zoom_graph_fit(_s->x, _s->want + _a, _s->weight + _a, _s->sep + _a, _len, _s->off, _s->bv, _s->bw, _s->bstart);
        f64 _lo = -1e300, _hi = 1e300, _ignore = 0.0;
        if(_walls) {
            fude_zoom_graph_room_of(_w, _row[_a], true, &_lo, &_ignore);
            fude_zoom_graph_room_of(_w, _row[_b - 1u], true, &_ignore, &_hi);
            _hi -= _s->off[_len - 1u];
        }
        if(_lo <= _hi) {
            // A uniform bound on an isotonic fit: clamping it is the fit with the bound.
            for(u32 _i = 0; _i < _len; _i++) {
                _v[_row[_a + _i]].x = rde_math_min_f64(rde_math_max_f64(_s->x[_i] - _s->off[_i], _lo), _hi) + _s->off[_i];
            }
        }
        _a = _b;
    }
}

// The box edges drawn in to what they hold (the deepest boxes first, so an
// outer one follows the inner): only ever inwards, so every rule still holds.
RDE_INTERNAL void fude_zoom_graph_hug(fude_zoom_graph_work* _w, const u32* _deepest) {
    const fude_zoom_graph_rule* _rule = (const fude_zoom_graph_rule*)_w->rules.memory;
    if(_w->nvars == _w->nv) {
        return;
    }
    for(u32 _k = 0; _k < _w->ns; _k++) {
        const u32 _left = _w->nv + 2u * (_deepest[_k] - 1u), _right = _left + 1u;
        if(_w->rout_first[_left + 1u] > _w->rout_first[_left]) {
            f64 _x = 1e300;
            for(u32 _j = _w->rout_first[_left]; _j < _w->rout_first[_left + 1u]; _j++) {
                _x = rde_math_min_f64(_x, fude_zoom_graph_at(_w, _rule[_w->rout[_j]].b) - _rule[_w->rout[_j]].gap);
            }
            _w->wall[_left - _w->nv] = _x;
        }
        if(_w->rin_first[_right + 1u] > _w->rin_first[_right]) {
            f64 _x = -1e300;
            for(u32 _j = _w->rin_first[_right]; _j < _w->rin_first[_right + 1u]; _j++) {
                _x = rde_math_max_f64(_x, fude_zoom_graph_at(_w, _rule[_w->rin[_j]].a) + _rule[_w->rin[_j]].gap);
            }
            _w->wall[_right - _w->nv] = _x;
        }
    }
}

// Subgraph boxes made whole, each edge one line through all its ranks, so a
// box holds its members and no one else: from where everything is (each box
// round what it holds), pushed right as little as the rules need and pushed
// left as little, the two met halfway, which keeps every rule.
RDE_INTERNAL void fude_zoom_graph_legalise(fude_zoom_graph_work* _w) {
    const u32 _nv = _w->nv, _nvar = _w->nvars;
    const fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    const fude_zoom_graph_rule* _rule = (const fude_zoom_graph_rule*)_w->rules.memory;
    rde_arr _tables = fude_zoom_graph_tables_new();
    f64* _right = (f64*)fude_zoom_graph_table(&_tables, _nvar, sizeof(f64));
    f64* _left  = (f64*)fude_zoom_graph_table(&_tables, _nvar, sizeof(f64));
    for(u32 _i = 0; _i < _nv; _i++) {
        _right[_i] = _v[_i].x;
    }
    for(u32 _i = _nv; _i < _nvar; _i++) {
        _right[_i] = ((_i - _nv) & 1u) ? -1e300 : 1e300;
    }
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        for(u32 _i = _w->row_first[_r]; _i < _w->row_first[_r + 1u]; _i++) {
            const fude_zoom_graph_vertex* _x = &_v[_w->row[_i]];
            for(u32 _s = _x->sub; _s != 0; _s = _w->parent[_s]) {
                const u32 _l = _nv + 2u * (_s - 1u);
                _right[_l]      = rde_math_min_f64(_right[_l], _x->x - _x->w * 0.5 - _w->pad[0]);
                _right[_l + 1u] = rde_math_max_f64(_right[_l + 1u], _x->x + _x->w * 0.5 + _x->loop + _w->pad[1]);
            }
        }
    }
    for(u32 _i = _nv; _i < _nvar; _i += 2u) {
        if(_right[_i] > _right[_i + 1u]) {
            _right[_i] = _right[_i + 1u] = 0.0;
        }
    }
    rde_memcpy(_left, _right, (usize)_nvar * sizeof(f64));
    for(u32 _k = 0; _k < _nvar; _k++) {
        const u32 _x = _w->topo[_k];
        for(u32 _j = _w->rout_first[_x]; _j < _w->rout_first[_x + 1u]; _j++) {
            const fude_zoom_graph_rule* _q = &_rule[_w->rout[_j]];
            _right[_q->b] = rde_math_max_f64(_right[_q->b], _right[_x] + _q->gap);
        }
    }
    for(u32 _k = _nvar; _k > 0; _k--) {
        const u32 _x = _w->topo[_k - 1u];
        for(u32 _j = _w->rout_first[_x]; _j < _w->rout_first[_x + 1u]; _j++) {
            const fude_zoom_graph_rule* _q = &_rule[_w->rout[_j]];
            _left[_x] = rde_math_min_f64(_left[_x], _left[_q->b] - _q->gap);
        }
    }
    for(u32 _i = 0; _i < _nvar; _i++) {
        if(_i >= _nv || _w->row_at[_i] != FUDE_ZOOM_GRAPH_NONE) {
            fude_zoom_graph_set_at(_w, _i, 0.5 * (_right[_i] + _left[_i]));
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

// Long edges made straight where their rows let them: each edge's dummies in
// runs, as long as the rows they cross have a place in common, each run on one
// line across (where its middle one is, or as near as it can be).
RDE_INTERNAL void fude_zoom_graph_straighten(fude_zoom_graph_work* _w) {
    fude_zoom_graph_vertex* _v     = fude_zoom_graph_v(_w);
    const u32*              _chain = (const u32*)_w->chain.memory;
    rde_arr                 _tables = fude_zoom_graph_tables_new();
    f64*                    _xs    = (f64*)fude_zoom_graph_table(&_tables, _w->ranks + 1u, sizeof(f64));
    for(u32 _e = 0; _e < _w->ne; _e++) {
        const u32  _n = _w->chain_count[_e];
        const u32* _c = &_chain[_w->chain_first[_e]];
        for(u32 _k = 1; _k + 1u < _n;) {
            f64 _lo = -1e300, _hi = 1e300;
            u32 _end = _k;
            for(; _end + 1u < _n; _end++) {
                f64 _a, _b;
                fude_zoom_graph_room_of(_w, _c[_end], false, &_a, &_b);
                if(rde_math_max_f64(_lo, _a) > rde_math_min_f64(_hi, _b)) {
                    break;
                }
                _lo = rde_math_max_f64(_lo, _a);
                _hi = rde_math_min_f64(_hi, _b);
                u32 _j = _end - _k;
                for(; _j > 0 && _xs[_j - 1u] > _v[_c[_end]].x; _j--) _xs[_j] = _xs[_j - 1u];
                _xs[_j] = _v[_c[_end]].x;
            }
            if(_end - _k >= 2u) {
                const f64 _at = rde_math_min_f64(rde_math_max_f64(_xs[(_end - _k - 1u) / 2u], _lo), _hi);
                for(u32 _j = _k; _j < _end; _j++) {
                    _v[_c[_j]].x = _at;
                }
            }
            _k = _end > _k ? _end : _k + 1u;
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

// The pinned nodes kept clear of: along each row, left to right, a vertex
// that would touch one goes past it, to whichever side is nearer (left only
// if there is room), and the rest of the row after it.
RDE_INTERNAL void fude_zoom_graph_avoid(fude_zoom_graph_work* _w) {
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    rde_arr _tables = fude_zoom_graph_tables_new();
    fude_zoom_graph_sorted* _obs = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _w->n, sizeof(fude_zoom_graph_sorted));
    const f64 _g = _w->sp.node_gap;
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        const f64 _top = _w->rank_y[_r] - _w->rank_h[_r] * 0.5, _bottom = _w->rank_y[_r] + _w->rank_h[_r] * 0.5;
        u32 _no = 0;
        for(u32 _p = 0; _p < _w->n; _p++) {
            if(_v[_p].pinned && _v[_p].y - _v[_p].h * 0.5 < _bottom && _v[_p].y + _v[_p].h * 0.5 > _top) {
                _obs[_no++] = (fude_zoom_graph_sorted){ _v[_p].x - _v[_p].w * 0.5, _v[_p].x + _v[_p].w * 0.5 + _v[_p].loop, _p };
            }
        }
        if(_no == 0) {
            continue;
        }
        rde_sort(_obs, _no, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
        u32 _prev = FUDE_ZOOM_GRAPH_NONE;
        for(u32 _i = _w->row_first[_r]; _i < _w->row_first[_r + 1u]; _i++) {
            fude_zoom_graph_vertex* _x = &_v[_w->row[_i]];
            const f64 _lo = _prev != FUDE_ZOOM_GRAPH_NONE ? _v[_prev].x + fude_zoom_graph_sep(_w, &_v[_prev], _x) : -1e300;
            f64 _at = rde_math_max_f64(_x->x, _lo);
            for(u32 _guard = 0; _guard <= _no; _guard++) {
                u32 _hit = FUDE_ZOOM_GRAPH_NONE;
                for(u32 _o = 0; _o < _no && _hit == FUDE_ZOOM_GRAPH_NONE; _o++) {
                    if(_at - _x->w * 0.5 - _g < _obs[_o].k2 && _at + _x->w * 0.5 + _x->loop + _g > _obs[_o].k1) _hit = _o;
                }
                if(_hit == FUDE_ZOOM_GRAPH_NONE) {
                    break;
                }
                const f64 _l = _obs[_hit].k1 - _g - _x->w * 0.5 - _x->loop, _rr = _obs[_hit].k2 + _g + _x->w * 0.5;
                b8 _left_free = _l >= _lo && _at - _l <= _rr - _at;
                for(u32 _o = 0; _o < _no && _left_free; _o++) {
                    _left_free = !(_l - _x->w * 0.5 - _g < _obs[_o].k2 && _l + _x->w * 0.5 + _x->loop + _g > _obs[_o].k1);
                }
                _at = _left_free ? _l : _rr;
            }
            _x->x = _at;
            _prev = _w->row[_i];
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

// Each rank's line along the flow: as tall as its tallest (pinned ones count,
// as the ranks line up on them), with room between for the padding (and
// title) of the boxes that end above and begin below.
RDE_INTERNAL void fude_zoom_graph_rank_lines(fude_zoom_graph_work* _w) {
    const u32 _ns = _w->ns, _nv = _w->nv;
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    _w->rank_y = (f64*)fude_zoom_graph_table(&_w->tables, _w->ranks, sizeof(f64));
    _w->rank_h = (f64*)fude_zoom_graph_table(&_w->tables, _w->ranks, sizeof(f64));
    rde_arr _tables = fude_zoom_graph_tables_new();
    f64* _below = (f64*)fude_zoom_graph_table(&_tables, _w->ranks, sizeof(f64));
    f64* _above = (f64*)fude_zoom_graph_table(&_tables, _w->ranks, sizeof(f64));
    for(u32 _i = 0; _i < _nv; _i++) {
        _w->rank_h[_v[_i].rank] = rde_math_max_f64(_w->rank_h[_v[_i].rank], _v[_i].h);
    }
    if(_ns > 0) {
        // How far each box reaches past its first and last ranks: its own pad
        // and that of the boxes inside it ending there too.
        f64* _top    = (f64*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(f64));
        f64* _bottom = (f64*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(f64));
        for(u32 _d = _w->most_depth; _d > 0; _d--) {
            for(u32 _s = 1; _s <= _ns; _s++) {
                if(_w->depth[_s] != _d || _w->smin[_s] > _w->smax[_s]) {
                    continue;
                }
                _top[_s]    += _w->pad[2];
                _bottom[_s] += _w->pad[3];
                _above[_w->smin[_s]] = rde_math_max_f64(_above[_w->smin[_s]], _top[_s]);
                _below[_w->smax[_s]] = rde_math_max_f64(_below[_w->smax[_s]], _bottom[_s]);
                const u32 _p = _w->parent[_s];
                if(_p != 0 && _w->smin[_p] == _w->smin[_s]) _top[_p] = rde_math_max_f64(_top[_p], _top[_s]);
                if(_p != 0 && _w->smax[_p] == _w->smax[_s]) _bottom[_p] = rde_math_max_f64(_bottom[_p], _bottom[_s]);
            }
        }
    }
    const f64 _base = _w->labels ? _w->sp.rank_gap * 0.5 : _w->sp.rank_gap;
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        if(_r == 0) {
            _w->rank_y[0] = _w->rank_h[0] * 0.5;
            continue;
        }
        const f64 _boxes = _below[_r - 1u] + _above[_r];
        const f64 _gap   = _boxes > 0.0 ? rde_math_max_f64(_base, _boxes + _base * 0.5) : _base;
        _w->rank_y[_r] = _w->rank_y[_r - 1u] + _w->rank_h[_r - 1u] * 0.5 + _gap + _w->rank_h[_r] * 0.5;
    }
    fude_zoom_graph_tables_free(&_tables);
}

// Along each rank: packed, drawn towards their neighbours, the boxes made
// whole, drawn towards their neighbours again inside them, long edges
// straightened, and the pinned nodes kept clear of.
void fude_zoom_graph_place(fude_zoom_graph_work* _w) {
    const u32 _nv = _w->nv;
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    // The rows: each rank without its pinned nodes.
    _w->row_first = (u32*)fude_zoom_graph_table(&_w->tables, _w->ranks + 1u, sizeof(u32));
    _w->row       = (u32*)fude_zoom_graph_table(&_w->tables, _nv, sizeof(u32));
    _w->row_at    = (u32*)fude_zoom_graph_table(&_w->tables, _nv, sizeof(u32));
    u32 _nrow = 0, _widest = 0;
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        _w->row_first[_r] = _nrow;
        for(u32 _i = _w->layer_first[_r]; _i < _w->layer_first[_r + 1u]; _i++) {
            _w->row_at[_w->layer[_i]] = _v[_w->layer[_i]].pinned ? FUDE_ZOOM_GRAPH_NONE : _nrow;
            if(!_v[_w->layer[_i]].pinned) _w->row[_nrow++] = _w->layer[_i];
        }
        _widest = _nrow - _w->row_first[_r] > _widest ? _nrow - _w->row_first[_r] : _widest;
    }
    _w->row_first[_w->ranks] = _nrow;
    // Lines along the flow, moved to where the pinned nodes have their ranks.
    fude_zoom_graph_rank_lines(_w);
    rde_arr _tables = fude_zoom_graph_tables_new();
    f64 _centre = 0.0;
    if(_w->any_pinned) {
        fude_zoom_graph_sorted* _off = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _w->n, sizeof(fude_zoom_graph_sorted));
        u32 _np = 0;
        for(u32 _i = 0; _i < _w->n; _i++) {
            if(_v[_i].pinned) {
                _off[_np++] = (fude_zoom_graph_sorted){ _v[_i].y - _w->rank_y[_v[_i].rank], 0.0, _i };
                _centre    += _v[_i].x;
            }
        }
        rde_sort(_off, _np, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
        const f64 _shift = _off[(_np - 1u) / 2u].k1;
        for(u32 _r = 0; _r < _w->ranks; _r++) {
            _w->rank_y[_r] += _shift;
        }
        _centre /= (f64)_np;
        fude_zoom_graph_tables_clear(&_tables);
    }
    for(u32 _i = 0; _i < _nv; _i++) {
        if(!_v[_i].pinned) _v[_i].y = _w->rank_y[_v[_i].rank];
    }
    // Packed, each row centred.
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        const u32 _a = _w->row_first[_r], _b = _w->row_first[_r + 1u];
        if(_a == _b) {
            continue;
        }
        f64 _x = 0.0;
        for(u32 _i = _a; _i < _b; _i++) {
            _x += _i > _a ? fude_zoom_graph_sep(_w, &_v[_w->row[_i - 1u]], &_v[_w->row[_i]]) : 0.0;
            _v[_w->row[_i]].x = _x;
        }
        const fude_zoom_graph_vertex* _first = &_v[_w->row[_a]];
        const fude_zoom_graph_vertex* _last  = &_v[_w->row[_b - 1u]];
        const f64 _mid = 0.5 * (_first->x - _first->w * 0.5 + _last->x + _last->w * 0.5 + _last->loop);
        for(u32 _i = _a; _i < _b; _i++) {
            _v[_w->row[_i]].x += _centre - _mid;
        }
    }
    fude_zoom_graph_scratch _s;
    _s.want   = (f64*)fude_zoom_graph_table(&_tables, _widest, sizeof(f64));
    _s.weight = (f64*)fude_zoom_graph_table(&_tables, _widest, sizeof(f64));
    _s.sep    = (f64*)fude_zoom_graph_table(&_tables, _widest, sizeof(f64));
    _s.x      = (f64*)fude_zoom_graph_table(&_tables, _widest, sizeof(f64));
    _s.off    = (f64*)fude_zoom_graph_table(&_tables, _widest, sizeof(f64));
    _s.bv     = (f64*)fude_zoom_graph_table(&_tables, _widest, sizeof(f64));
    _s.bw     = (f64*)fude_zoom_graph_table(&_tables, _widest, sizeof(f64));
    _s.bstart = (u32*)fude_zoom_graph_table(&_tables, _widest, sizeof(u32));
    _s.near   = (f64*)fude_zoom_graph_table(&_tables, _w->most_degree + 1u, sizeof(f64));
    for(u32 _pass = 0; _pass < FUDE_ZOOM_GRAPH_NUDGES; _pass++) {
        for(u32 _r = 1; _r < _w->ranks; _r++) fude_zoom_graph_nudge(_w, _r, FUDE_ZOOM_GRAPH_ABOVE, false, &_s);
        for(u32 _r = _w->ranks; _r-- > 0;) fude_zoom_graph_nudge(_w, _r, FUDE_ZOOM_GRAPH_BELOW, false, &_s);
    }
    // The rules, with the boxes if they hold together (else without), the
    // boxes made whole, and a few more passes inside them.
    _w->rules = rde_arr_new(sizeof(fude_zoom_graph_rule), rde_memory_allocator_get_default_std());
    _w->wall  = (f64*)fude_zoom_graph_table(&_w->tables, 2u * _w->ns, sizeof(f64));
    const b8 _boxes = _w->ns > 0 && fude_zoom_graph_rules(_w, true);
    if(!_boxes) {
        fude_zoom_graph_rules(_w, false);
    } else {
        fude_zoom_graph_legalise(_w);
    }
    u32* _deepest = (u32*)fude_zoom_graph_table(&_tables, _w->ns, sizeof(u32));
    for(u32 _d = _w->most_depth, _k = 0; _d > 0; _d--) {
        for(u32 _t = 1; _t <= _w->ns; _t++) {
            if(_w->depth[_t] == _d) _deepest[_k++] = _t;
        }
    }
    for(u32 _pass = 0; _pass <= FUDE_ZOOM_GRAPH_NUDGES / 2u; _pass++) {
        const b8 _both = _pass == FUDE_ZOOM_GRAPH_NUDGES / 2u;
        for(u32 _r = _both ? 0u : 1u; _r < _w->ranks; _r++) {
            fude_zoom_graph_nudge(_w, _r, _both ? FUDE_ZOOM_GRAPH_ABOVE | FUDE_ZOOM_GRAPH_BELOW : FUDE_ZOOM_GRAPH_ABOVE, true, &_s);
        }
        fude_zoom_graph_hug(_w, _deepest);
        for(u32 _r = _both ? 0u : _w->ranks; !_both && _r-- > 0;) fude_zoom_graph_nudge(_w, _r, FUDE_ZOOM_GRAPH_BELOW, true, &_s);
        fude_zoom_graph_hug(_w, _deepest);
    }
    fude_zoom_graph_tables_free(&_tables);
    fude_zoom_graph_straighten(_w);
    if(_w->any_pinned) {
        fude_zoom_graph_avoid(_w);
    }
}
