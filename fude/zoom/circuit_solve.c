// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/circuit_parts.h"

// ===========================================================================
// See circuit.h. Circuits, the solve: each block's system shaped (sim/sparse.h) and solved (densely, when a pivot is
// too small to solve it sparse), Newton's method block by block, the parts where it would not settle marked, the rails'
// currents, the wires' currents along their tree, quiet blocks left as they were, and a solve at a time (fzc_at).
//
// The file is in three parts, in order: the solve (dense, shaped, by block, Newton's method, the scratch, the wires'
// currents); quiet blocks (a part's key, the blocks left quiet, those found still after a step); and fzc_at, which
// solves the circuit at a time and steps what is stepped.
//
// Part of the circuit: circuit_parts.h holds what the modules share and says which module holds what. Prefix fzc_ and
// FZC_. Main thread only.
// ===========================================================================

// The system solved (Gaussian elimination, the largest pivot each column). False: singular.
RDE_INTERNAL b8 fzc_solve(f64* _a, f64* _b, f64* _x, u32 _n, u32* _cols) {
    for(u32 _k = 0; _k < _n; _k++) {
        u32 _best = _k;
        f64 _big = rde_math_abs_f64(_a[_k * _n + _k]);
        for(u32 _i = _k + 1u; _i < _n; _i++) {
            if(rde_math_abs_f64(_a[_i * _n + _k]) > _big) {
                _big  = rde_math_abs_f64(_a[_i * _n + _k]);
                _best = _i;
            }
        }
        if(!(_big > 1e-30)) {
            return false;
        }
        if(_best != _k) {
            for(u32 _j = _k; _j < _n; _j++) {   // (left of _k: nothing read again)
                const f64 _t = _a[_k * _n + _j];
                _a[_k * _n + _j] = _a[_best * _n + _j];
                _a[_best * _n + _j] = _t;
            }
            const f64 _t = _b[_k];
            _b[_k] = _b[_best];
            _b[_best] = _t;
        }
        // (a circuit's rows are mostly zeros: only the pivot row's others taken from the rows under it)
        const f64* _row = &_a[_k * _n];
        u32 _m = 0;
        for(u32 _j = _k + 1u; _j < _n; _j++) {
            if(_row[_j] != 0.0) {
                _cols[_m++] = _j;
            }
        }
        const f64 _piv = _row[_k];
        for(u32 _i = _k + 1u; _i < _n; _i++) {
            f64* _under = &_a[_i * _n];
            const f64 _f = _under[_k] / _piv;
            if(_f == 0.0) {
                continue;
            }
            for(u32 _j = 0; _j < _m; _j++) {
                _under[_cols[_j]] -= _f * _row[_cols[_j]];
            }
            _b[_i] -= _f * _b[_k];
        }
    }
    for(u32 _k = _n; _k-- > 0u;) {
        f64 _s = _b[_k];
        for(u32 _j = _k + 1u; _j < _n; _j++) {
            _s -= _a[_k * _n + _j] * _x[_j];
        }
        _x[_k] = _s / _a[_k * _n + _k];
    }
    return true;
}

#define FZC_REFERENCE_G 1.0    // a block ground reaches nowhere: its reference's conductance to ground (siemens)
#define FZC_DENSE_MOST 400u   // a block solved densely when its sparse factoring cannot: so many unknowns at most
#define FZC_RESHAPES   8u     // a Newton solve's systems shaped again (a stamp writing somewhere new): so often at most

// Orders two u64s, for rde_sort.
RDE_INTERNAL int fzc_u64_cmp(const any _a, const any _b) {
    const u64 _x = *(const u64*)_a, _y = *(const u64*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// Its blocks' systems shaped (sim/sparse.h): where each part's stamps write — as they would now, and wherever they
// wrote before (the joins kept) —, each block's in its own numbering (its slots from its first).
RDE_INTERNAL void fzc_shape(fzc_ctx* _x) {
    fude_zoom_circuit* _c = _x->c;
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts);
    _c->recording = true;
    for(u32 _i = 0; _i < _np; _i++) {
        fzc_stamp(_x, &_p[_i]);   // (its currents written too: put right when it is stamped for real)
    }
    _c->recording = false;
    _c->reshape   = false;
    // The joins, each once; then dealt to their blocks.
    u64* _j = (u64*)_c->joins.memory;
    u32 _nj = (u32)rde_arr_length(&_c->joins);
    rde_sort(_j, _nj, sizeof(u64), fzc_u64_cmp);
    u32 _kept = 0;
    for(u32 _k = 0; _k < _nj; _k++) {
        if(_kept == 0u || _j[_k] != _j[_kept - 1u]) {
            _j[_kept++] = _j[_k];
        }
    }
    rde_arr_resize(&_c->joins, _kept);
    _nj = _kept;
    const u32* _block = (const u32*)_c->node_block.memory;
    const u32* _slot  = (const u32*)_c->node_slot.memory;
    const u32* _first = (const u32*)_c->block_first.memory;
    const u32* _size  = (const u32*)_c->block_size.memory;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr TYPE(u32) _at_arr    = rde_arr_new(sizeof(u32), _heap);
    rde_arr TYPE(u64) _local_arr = rde_arr_new(sizeof(u64), _heap);
    rde_arr_resize(&_at_arr, _c->blocks + 1u);
    rde_arr_resize(&_local_arr, _nj);
    u32* _at    = (u32*)_at_arr.memory;   // (each block's first in _local: counted, then added up)
    u64* _local = (u64*)_local_arr.memory;
    for(u32 _k = 0; _k < _nj; _k++) {
        _at[_block[(u32)(_j[_k] >> 32)] + 1u]++;
    }
    for(u32 _b = 0; _b < _c->blocks; _b++) {
        _at[_b + 1u] += _at[_b];
    }
    for(u32 _k = 0; _k < _nj; _k++) {
        const u32 _r = (u32)(_j[_k] >> 32), _q = (u32)(_j[_k] & 0xFFFFFFFFull), _b = _block[_r];
        _local[_at[_b]++] = ((u64)(_slot[_r] - _first[_b]) << 32) | (_slot[_q] - _first[_b]);
    }
    fude_sim_sparse* _sys = (fude_sim_sparse*)_c->block_system.memory;
    u32 _from = 0;
    for(u32 _b = 0; _b < _c->blocks; _b++) {
        fude_sim_sparse_shape(&_sys[_b], _size[_b], &_local[_from], _at[_b] - _from);
        _from = _at[_b];
    }
    rde_arr_free(&_at_arr);
    rde_arr_free(&_local_arr);
}

// Block _b's system solved for _rhs into _sol (its slots): factored as it is shaped; when a pivot is too small for
// that (a gain ahead of what it drives, a source straight across another), densely with rows swapped — not too big.
RDE_INTERNAL b8 fzc_block_solve(fude_zoom_circuit* _c, u32 _b, f64* _rhs, f64* _sol) {
    fude_sim_sparse* _s = &((fude_sim_sparse*)_c->block_system.memory)[_b];
    if(fude_sim_sparse_factor(_s)) {
        fude_sim_sparse_solve(_s, _rhs, _sol);
        return true;
    }
    if(_s->n > FZC_DENSE_MOST) {
        return false;
    }
    rde_arr_resize(&_c->a, (usize)_s->n * _s->n);
    fude_sim_sparse_dense(_s, (f64*)_c->a.memory);
    return fzc_solve((f64*)_c->a.memory, _rhs, _sol, _s->n, (u32*)_c->cols.memory);
}

// Not solved: the parts where it would not settle marked (suspect) — in block _b, each on a node that moved at least
// half as much as the most in it did last time round (FUDE_ZOOM_NONE: every block not settled); _all: every part of it
// (it could not be solved at all: a source across another, a loop of them).
RDE_INTERNAL void fzc_suspect(fude_zoom_circuit* _c, u32 _b, b8 _all) {
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    const u32* _block = (const u32*)_c->node_block.memory;
    const u32* _slot  = (const u32*)_c->node_slot.memory;
    const u8*  _done  = (const u8*)_c->block_done.memory;
    const f64* _sol   = (const f64*)_c->x.memory;
    const f64* _v     = (const f64*)_c->v.memory;
    f64 _most = 0.0;
    for(u32 _k = 1; _k < _c->nodes; _k++) {
        const u32 _kb = _block[_k];
        if(_kb != FUDE_ZOOM_NONE && (_kb == _b || (_b == FUDE_ZOOM_NONE && _done[_kb] != 1u))) {
            _most = rde_math_max_f64(_most, rde_math_abs_f64(_sol[_slot[_k]] - _v[_k]));
        }
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        const u32 _pb = _p[_i].block;
        if(_pb == FUDE_ZOOM_NONE || !(_pb == _b || (_b == FUDE_ZOOM_NONE && _done[_pb] != 1u))) {
            continue;
        }
        for(u32 _k = 0; _k < _p[_i].part->pin_count && !_p[_i].suspect; _k++) {
            const u32 _node = _p[_i].node[_k];
            _p[_i].suspect = _all || (_node != FUDE_ZOOM_NONE && _node != 0u && _node < _c->nodes &&
                                      rde_math_abs_f64(_sol[_slot[_node]] - _v[_node]) >= 0.5 * _most && _most > 0.0);
        }
        _c->suspected = _c->suspected || _p[_i].suspect;
    }
}

// What each rail gives: what the rest on its node take (its node known: its own stamps say nothing of it). Each pin on
// a held node is its place in pin_amps (held_pins: node << 32 | place).
RDE_INTERNAL void fzc_held_currents(fude_zoom_circuit* _c) {
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    const u32* _held = (const u32*)_c->node_held.memory;
    const u64* _at   = (const u64*)_c->held_pins.memory;
    const f64* _amps = (const f64*)_c->pin_amps.memory;
    const u32  _n    = (u32)rde_arr_length(&_c->held_pins);
    for(u32 _k = 0; _k < _n; _k++) {
        _p[_held[(u32)(_at[_k] >> 32)]].pin_i[0] = 0.0;
    }
    for(u32 _k = 0; _k < _n; _k++) {
        _p[_held[(u32)(_at[_k] >> 32)]].pin_i[0] -= _amps[(u32)(_at[_k] & 0xFFFFFFFFull)];
    }
}

// Is part _q's every block left as it is (settled, or quiet)? Then it is not stamped.
RDE_INTERNAL b8 fzc_part_done(const fude_zoom_circuit* _c, const fude_zoom_circuit_part* _q, const u8* _done) {
    if(_q->block == FUDE_ZOOM_NONE || _done[_q->block] != 1u) {
        return false;
    }
    if(_q->spans) {
        const u32* _block = (const u32*)_c->node_block.memory;
        for(u32 _k = 0; _k < _q->part->pin_count; _k++) {
            const u32 _node = _q->node[_k];
            if(_node != FUDE_ZOOM_NONE && _node != 0u && _node < _c->nodes && _block[_node] != FUDE_ZOOM_NONE && _done[_block[_node]] != 1u) {
                return false;   // (a leak it stamps where it senses: in a block being solved)
            }
        }
    }
    return true;
}

// Each part's stamps, the system solved, again until it settles (Newton) — block by block, each settled left as it is
// (_only: that block alone; FUDE_ZOOM_NONE: all but the quiet, unless every one is woken). False: it did not (all of
// them: where, marked) — or its blocks are to be made again (reblock).
RDE_INTERNAL b8 fzc_newton_once(fzc_ctx* _x, u32 _only) {
    fude_zoom_circuit* _c = _x->c;
    const u32 _n = _x->n;
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts);
    if(_n == 0u || _c->blocks == 0u) {
        // (nothing unknown: each part's currents as the known volts say — a rail, an LED and ground)
        for(u32 _i = 0; _i < _np; _i++) {
            rde_memset(_p[_i].pin_i, 0, (usize)_p[_i].pin_room * sizeof(f64));
            if(_n > 0u) {
                fzc_stamp(_x, &_p[_i]);
            }
        }
        if(_n > 0u) {
            fzc_held_currents(_c);
        }
        return true;
    }
    if(_c->reshape) {
        fzc_shape(_x);
    }
    f64* _rhs = (f64*)_c->rhs.memory;   // (sized by fzc_room)
    f64* _sol = (f64*)_c->x.memory;
    const u32* _first = (const u32*)_c->block_first.memory;
    const u32* _size  = (const u32*)_c->block_size.memory;
    const u32* _block = (const u32*)_c->node_block.memory;
    const u32* _slot  = (const u32*)_c->node_slot.memory;
    fude_sim_sparse* _sys = (fude_sim_sparse*)_c->block_system.memory;
    f64* _v    = (f64*)_c->v.memory;
    u8*  _done = (u8*)_c->block_done.memory;    // 1: settled; 0, 2: being solved (2: nothing in it moved yet this time)
    f64* _most = (f64*)_c->block_most.memory;
    const u8* _quiet = (const u8*)_c->block_quiet.memory;
    for(u32 _b = 0; _b < _c->blocks; _b++) {
        _done[_b] = _only != FUDE_ZOOM_NONE ? (_b != _only ? 1u : 0u) : (!_c->wake && _quiet[_b] ? 1u : 0u);
    }
    if(_c->reblock) {
        return false;
    }
    u32 _reshapes = 0;
    b8 _known = false;   // the parts in no block stamped this solve
    for(u32 _it = 0; _it < 150u;) {
        // (a settled block's system, and its parts' currents, as they were)
        for(u32 _b = 0; _b < _c->blocks; _b++) {
            if(_done[_b] != 1u) {
                fude_sim_sparse_clear(&_sys[_b]);
                rde_memset(&_rhs[_first[_b]], 0, (usize)_size[_b] * sizeof(f64));
            }
        }
        // (a part in no block — its pins all known: ground, a rail's, a node only logic is on — writes into no system,
        // and its currents are the same at every iteration, from volts that do not move: stamped once a solve)
        for(u32 _i = 0; _i < _np; _i++) {
            if(fzc_part_done(_c, &_p[_i], _done) || (_known && _p[_i].block == FUDE_ZOOM_NONE)) {
                continue;
            }
            rde_memset(_p[_i].pin_i, 0, (usize)_p[_i].pin_room * sizeof(f64));
            fzc_stamp(_x, &_p[_i]);
        }
        _known = true;
        if(_c->reblock) {
            return false;   // (two blocks one: made again, fzc_newton)
        }
        if(_c->reshape) {
            // A stamp wrote where its block's shape had no place: shaped again with it, and solved from the start
            // (every part's currents written again).
            if(++_reshapes > FZC_RESHAPES) {
                return false;
            }
            fzc_shape(_x);
            if(_c->reblock) {
                return false;
            }
            for(u32 _b = 0; _b < _c->blocks; _b++) {
                _done[_b] = _only != FUDE_ZOOM_NONE ? (_b != _only ? 1u : 0u) : (!_c->wake && _quiet[_b] ? 1u : 0u);
            }
            continue;
        }
        const u32* _ref = (const u32*)_c->block_reference.memory;
        const u32* _node_block = (const u32*)_c->node_block.memory;
        const u32  _refs = (u32)rde_arr_length(&_c->block_reference);
        for(u32 _b = 0; _b < _c->blocks; _b++) {
            if(_done[_b] == 1u) {
                continue;
            }
            fude_sim_sparse_add_diagonal(&_sys[_b], FZC_GMIN);
            for(u32 _r = 0; _r < _refs; _r++) {
                if(_node_block[_ref[_r]] == _b) {
                    *fzc_entry(_c, _ref[_r], _ref[_r]) += FZC_REFERENCE_G;   // (its diagonal: always a place)
                }
            }
            if(!fzc_block_solve(_c, _b, &_rhs[_first[_b]], &_sol[_first[_b]])) {
                if(_only == FUDE_ZOOM_NONE) {
                    fzc_suspect(_c, _b, true);
                }
                return false;
            }
            _most[_b] = 0.0;
            _done[_b] = 2u;
        }
        // (each block's step: at most 2 V at its most — exponentials stay near —, settled when nothing in it moves)
        for(u32 _k = 1; _k < _c->nodes; _k++) {
            const u32 _b = _block[_k];
            if(_b != FUDE_ZOOM_NONE && _done[_b] != 1u) {
                _most[_b] = rde_math_max_f64(_most[_b], rde_math_abs_f64(_sol[_slot[_k]] - _v[_k]));
            }
        }
        for(u32 _k = 1; _k < _c->nodes; _k++) {
            const u32 _b = _block[_k];
            if(_b == FUDE_ZOOM_NONE || _done[_b] == 1u) {
                continue;
            }
            const f64 _damp = _most[_b] > 2.0 ? 2.0 / _most[_b] : 1.0;
            const f64 _d    = (_sol[_slot[_k]] - _v[_k]) * _damp;
            if(!(rde_math_abs_f64(_d) <= 1e-7 + 1e-6 * rde_math_abs_f64(_sol[_slot[_k]]))) {
                _done[_b] = 0u;
            }
            _v[_k] += _d;
        }
        b8 _all = true;
        for(u32 _b = 0; _b < _c->blocks; _b++) {
            _done[_b] = _done[_b] == 2u ? 1u : _done[_b];
            _all = _all && _done[_b] == 1u;
        }
        _it++;
        _c->solves = _it;
        if(_all) {
            fzc_held_currents(_c);
            return true;
        }
    }
    if(_only == FUDE_ZOOM_NONE) {
        fzc_suspect(_c, FUDE_ZOOM_NONE, false);
    }
    return false;
}

// fzc_newton_once — its blocks made again first whenever a stamp is found to join two (every block solved then).
b8 fzc_newton(fzc_ctx* _x, u32 _only) {
    fude_zoom_circuit* _c = _x->c;
    for(u32 _again = 0;; _again++) {
        const b8 _ok = fzc_newton_once(_x, _only);
        if(!_c->reblock) {
            return _ok;
        }
        if(_again >= 8u) {
            return false;
        }
        _c->reblock = false;
        fzc_blocks(_c);
        fzc_room(_c, _x->n > 0u ? _x->n : 1u);
        _c->wake = true;
        _only    = FUDE_ZOOM_NONE;   // (its blocks numbered again: all of them)
    }
}

// The system's scratch sized for _n unknowns and its blocks (the dense matrix only when one is solved so: as it is).
void fzc_room(fude_zoom_circuit* _c, u32 _n) {
    rde_arr_resize(&_c->rhs, _n > 0u ? _n : 1u);
    rde_arr_resize(&_c->x, _n > 0u ? _n : 1u);
    rde_arr_resize(&_c->block_done, _c->blocks);
    rde_arr_resize(&_c->block_most, _c->blocks);
    rde_arr_resize(&_c->cols, _n > 0u ? _n : 1u);
}

// Returns the memory of scratch array _a, cleared and sized to _n elements, every one zero: as a new array sized so
// would be, with no allocation once it has been that big.
RDE_INTERNAL any fzc_zeroed(rde_arr* _a, u32 _n) {
    rde_arr_clear(_a);
    rde_arr_resize(_a, _n);
    return _a->memory;
}

// The wires' currents: each piece's, as the parts' pins push current into the places they touch, along a tree of the
// wires (a loop of wires: one of its pieces carries none). Its arrays are the circuit's scratch (wire_inject and the
// others, circuit.h), zeroed at each call as new ones were: called after every advance, it allocates nothing once
// they have grown.
void fzc_wire_currents(fude_zoom_circuit* _c) {
    const u32 _nv = _c->vertices, _nw = (u32)rde_arr_length(&_c->wires), _np = (u32)rde_arr_length(&_c->parts);
    fude_zoom_circuit_wire* _w = (fude_zoom_circuit_wire*)_c->wires.memory;
    if(_nv == 0u || _nw == 0u) {
        return;
    }
    const u32* _cv = (const u32*)_c->cut_vertex.memory;
    const u32* _pv = (const u32*)_c->pin_vertex.memory;
    f64* _inject = (f64*)fzc_zeroed(&_c->wire_inject, _nv);   // (sized once: they stay put)
    u32* _parent_edge = (u32*)fzc_zeroed(&_c->wire_parent, _nv);   // (wire × PIECES + piece; NONE: a root)
    u32* _order = (u32*)fzc_zeroed(&_c->wire_order, _nv);
    u8*  _seen = (u8*)fzc_zeroed(&_c->wire_seen, _nv);
    // (each vertex's pieces: a flat list)
    u32* _count = (u32*)fzc_zeroed(&_c->wire_count, _nv + 1u);
    for(u32 _i = 0; _i < _nw; _i++) {
        for(u32 _k = 0; _k < _w[_i].pieces; _k++) {
            const u32 _a = _cv[_i * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k], _b = _cv[_i * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k + 1u];
            if(_a != FUDE_ZOOM_NONE && _b != FUDE_ZOOM_NONE) {
                _count[_a + 1u]++;
                _count[_b + 1u]++;
            }
            _w[_i].current[_k] = 0.0;
        }
    }
    for(u32 _v = 0; _v < _nv; _v++) {
        _count[_v + 1u] += _count[_v];
    }
    u32* _adj = (u32*)fzc_zeroed(&_c->wire_adj, _count[_nv]);
    u32* _fill = (u32*)fzc_zeroed(&_c->wire_fill, _nv);
    rde_memcpy(_fill, _count, (usize)_nv * sizeof(u32));
    for(u32 _i = 0; _i < _nw; _i++) {
        for(u32 _k = 0; _k < _w[_i].pieces; _k++) {
            const u32 _a = _cv[_i * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k], _b = _cv[_i * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k + 1u];
            if(_a != FUDE_ZOOM_NONE && _b != FUDE_ZOOM_NONE) {
                _adj[_fill[_a]++] = _i * FUDE_ZOOM_WIRE_PIECES + _k;
                _adj[_fill[_b]++] = _i * FUDE_ZOOM_WIRE_PIECES + _k;
            }
        }
    }
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_c->parts.memory;
    for(u32 _i = 0; _i < _np; _i++) {
        for(u32 _k = 0; _k < _p[_i].part->pin_count; _k++) {
            const u32 _v = _pv[_p[_i].pin_at + _k];
            if(_v != FUDE_ZOOM_NONE) {
                _inject[_v] -= _p[_i].pin_i[_k];   // (out of the part, into where it touches)
            }
        }
    }
    // Breadth first from each untouched vertex; then from the leaves in, each piece carrying what is beyond it.
    u32 _n = 0;
    for(u32 _s = 0; _s < _nv; _s++) {
        if(_seen[_s]) {
            continue;
        }
        _seen[_s] = 1u;
        _parent_edge[_s] = FUDE_ZOOM_NONE;
        u32 _head = _n;
        _order[_n++] = _s;
        while(_head < _n) {
            const u32 _v = _order[_head++];
            for(u32 _e = _count[_v]; _e < _count[_v + 1u]; _e++) {
                const u32 _edge = _adj[_e], _wi = _edge / FUDE_ZOOM_WIRE_PIECES, _k = _edge % FUDE_ZOOM_WIRE_PIECES;
                const u32 _a = _cv[_wi * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k], _b = _cv[_wi * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k + 1u];
                const u32 _o = _a == _v ? _b : _a;
                if(!_seen[_o]) {
                    _seen[_o] = 1u;
                    _parent_edge[_o] = _edge;
                    _order[_n++] = _o;
                }
            }
        }
    }
    for(u32 _i = _n; _i-- > 0u;) {
        const u32 _v = _order[_i], _edge = _parent_edge[_v];
        if(_edge == FUDE_ZOOM_NONE) {
            continue;
        }
        const u32 _wi = _edge / FUDE_ZOOM_WIRE_PIECES, _k = _edge % FUDE_ZOOM_WIRE_PIECES;
        const u32 _a = _cv[_wi * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k], _b = _cv[_wi * (FUDE_ZOOM_WIRE_PIECES + 1u) + _k + 1u];
        const u32 _up = _a == _v ? _b : _a;
        // What comes into _v flows on to its parent along the piece: along it when _v is its start.
        _w[_wi].current[_k] = _a == _v ? _inject[_v] : -_inject[_v];
        _inject[_up] += _inject[_v];
    }
}

// --- quiet blocks (fude_zoom_circuit's) ------------------------------------------------------------

// Which of a part's state its stamps read (bits): the rest is what it shows, or keeps for itself as it steps (a 555's
// watch on its thresholds, an oscilloscope's sweep), changing every step without changing what it stamps.
RDE_INTERNAL u32 fzc_state_read(u8 _model) {
    switch(_model) {
    case FUDE_ZOOM_MODEL_TIMER555:    return 0x01u;
    case FUDE_ZOOM_MODEL_SCOPE:
    case FUDE_ZOOM_MODEL_SPEAKER:
    case FUDE_ZOOM_MODEL_SEVEN_SEG:
    case FUDE_ZOOM_MODEL_SEG_PANEL:
    case FUDE_ZOOM_MODEL_LED_MATRIX:
    case FUDE_ZOOM_MODEL_BAR_GRAPH:
    case FUDE_ZOOM_MODEL_LED:
    case FUDE_ZOOM_MODEL_LAMP:
    case FUDE_ZOOM_MODEL_BUZZER:
    case FUDE_ZOOM_MODEL_VOLTMETER:
    case FUDE_ZOOM_MODEL_AMMETER:
    case FUDE_ZOOM_MODEL_PANEL_METER: return 0x00u;
    default:                          return 0xFFu;
    }
}

// Part _i's key into _out: what its stamps read — its numbers, the state they use, how it is turned, pressed and
// switched, burnt, its pins' volts (but where it senses).
RDE_INTERNAL void fzc_key(const fude_zoom_circuit* _c, u32 _i, f64* _out) {
    const fude_zoom_circuit_part* _p = &((const fude_zoom_circuit_part*)_c->parts.memory)[_i];
    const u32  _read = fzc_state_read(_p->part->model);
    const f64* _v    = (const f64*)_c->v.memory;
    for(u32 _k = 0; _k < 8u; _k++) {
        _out[_k]      = _p->value[_k];
        _out[8u + _k] = (_read >> _k & 1u) ? _p->state[_k] : 0.0;
    }
    _out[16] = _p->spin;
    _out[17] = _p->pushed ? 1.0 : 0.0;
    _out[18] = (f64)_p->switch_on;
    _out[19] = _p->burnt ? 1.0 : 0.0;
    for(u32 _k = 0; _k < _p->part->pin_count; _k++) {
        const u32 _node = _p->node[_k];
        _out[FZC_KEY_FIXED + _k] = _node != FUDE_ZOOM_NONE && _node < _c->nodes && !fzc_senses(_p, _k) ? _v[_node] : 0.0;
    }
}

// The blocks left quiet this step: still, not live, each of their parts as it was when they were last solved (its key
// the same, bit for bit). The rails' volts again; the volts as it begins (to see what moves).
RDE_INTERNAL void fzc_quiet_begin(fude_zoom_circuit* _c, b8 _tran) {
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_c->parts.memory;
    const u32* _held = (const u32*)_c->node_held.memory;
    f64* _v = (f64*)_c->v.memory;
    for(u32 _k = 1; _k < _c->nodes && _k < (u32)rde_arr_length(&_c->node_held); _k++) {
        _v[_k] = _held[_k] != FUDE_ZOOM_NONE ? _p[_held[_k]].value[0] : _v[_k];
    }
    rde_arr_resize(&_c->v_step, _c->nodes > 0u ? _c->nodes : 1u);
    rde_memcpy(_c->v_step.memory, _c->v.memory, (usize)_c->nodes * sizeof(f64));
    u8* _quiet = (u8*)_c->block_quiet.memory;
    const u8* _still = (const u8*)_c->block_still.memory;
    const u8* _live  = (const u8*)_c->block_live.memory;
    _c->wake    = !_tran;
    _c->skipped = 0;
    for(u32 _b = 0; _b < _c->blocks; _b++) {
        _quiet[_b] = _tran && _still[_b] && !_live[_b] ? 1u : 0u;
    }
    const u32* _key_at = (const u32*)_c->key_at.memory;
    const f64* _keys   = (const f64*)_c->part_key.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        const u32 _b = _p[_i].block;
        if(_b == FUDE_ZOOM_NONE || !_quiet[_b]) {
            continue;
        }
        if(rde_arr_length(&_c->part_scratch) < (usize)FZC_KEY_FIXED + _p[_i].part->pin_count) {
            rde_arr_resize(&_c->part_scratch, (usize)FZC_KEY_FIXED + _p[_i].part->pin_count);   // (fzc_blocks sizes it)
        }
        f64* _now = (f64*)_c->part_scratch.memory;
        fzc_key(_c, _i, _now);
        if(rde_memcmp(_now, &_keys[_key_at[_i]], (usize)(_key_at[_i + 1u] - _key_at[_i]) * sizeof(f64)) != 0) {
            _quiet[_b] = 0u;
        }
    }
    for(u32 _b = 0; _b < _c->blocks; _b++) {
        _c->skipped += _quiet[_b];
    }
}

// Was block _b solved this step?
RDE_INTERNAL b8 fzc_solved(const fude_zoom_circuit* _c, u32 _b) {
    return _b != FUDE_ZOOM_NONE && (_c->wake || !((const u8*)_c->block_quiet.memory)[_b]);
}

// After the step: each block solved still if nothing in it moved, its capacitors' currents and inductors' volts next
// to none (a millivolt, a milliamp a second), nothing live in it — its parts' keys taken. (Not stepped in time, or not
// solved: none still.)
RDE_INTERNAL void fzc_quiet_end(fzc_ctx* _x, b8 _ok) {
    fude_zoom_circuit* _c = _x->c;
    u8* _still = (u8*)_c->block_still.memory;
    if(!_x->tran || !_ok) {
        rde_memset(_still, 0, _c->blocks);
        return;
    }
    const u8*  _live  = (const u8*)_c->block_live.memory;
    const u32* _block = (const u32*)_c->node_block.memory;
    const f64* _v     = (const f64*)_c->v.memory;
    const f64* _was   = (const f64*)_c->v_step.memory;
    for(u32 _b = 0; _b < _c->blocks; _b++) {
        _still[_b] = fzc_solved(_c, _b) ? (_live[_b] ? 0u : 1u) : _still[_b];
    }
    for(u32 _k = 1; _k < _c->nodes; _k++) {
        const u32 _b = _block[_k];
        if(fzc_solved(_c, _b) && !(rde_math_abs_f64(_v[_k] - _was[_k]) <= 1e-7 + 1e-6 * rde_math_abs_f64(_v[_k]))) {
            _still[_b] = 0u;
        }
    }
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    const u32* _key_at = (const u32*)_c->key_at.memory;
    f64* _keys = (f64*)_c->part_key.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        const u32 _b = _p[_i].block;
        if(!fzc_solved(_c, _b)) {
            continue;
        }
        switch(_p[_i].part->model) {
        case FUDE_ZOOM_MODEL_CAPACITOR:
            _still[_b] = rde_math_abs_f64(_p[_i].pin_i[0]) > 1e-3 * _p[_i].value[0] ? 0u : _still[_b];
            break;
        case FUDE_ZOOM_MODEL_INDUCTOR:
            _still[_b] = rde_math_abs_f64(fzc_over(_x, &_p[_i], 0, 1)) > 1e-3 * _p[_i].value[0] ? 0u : _still[_b];
            break;
        case FUDE_ZOOM_MODEL_SOLENOID:
        case FUDE_ZOOM_MODEL_TRANSFORMER:
            _still[_b] = 0u;   // (their windings: watched every step)
            break;
        default: break;
        }
        fzc_key(_c, _i, &_keys[_key_at[_i]]);
    }
}

// Solves the circuit at its time plus _dt (_tran: a step of time; else DC, as it stands), the logic settled within
// it, what is stepped worked out, and the quiet blocks left as they were. Returns false when it could not be solved;
// time moves on only when it was.
b8 fzc_at(fude_zoom_circuit* _c, b8 _tran, f64 _dt) {
    const u32 _n = _c->nodes > 0u ? _c->nodes - 1u : 0u;
    fzc_room(_c, _n > 0u ? _n : 1u);
    fzc_ctx _x = { _c, _n, _tran, _dt, _c->time + (_tran ? _dt : 0.0), _c->be_next };
    b8 _jumped = false;
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts);
    fzc_quiet_begin(_c, _tran);
    b8 _ok = fzc_newton(&_x, FUDE_ZOOM_NONE);
    // Logic settles within the step: solved again while it changes (16 times at most) — what is stepped worked out
    // from the voltages, the logic's bridges read and what they drive again. (A part stepped: its own block woken; the
    // logic: every block.)
    for(u32 _settle = 0; _ok && _settle < 16u; _settle++) {
        const b8 _logic = fude_zoom_logic_step(_c->logic, _c, _x.t);
        b8 _changed = _logic;
        for(u32 _i = 0; _i < _np; _i++) {
            if(fzc_digital(&_x, &_p[_i])) {
                _changed = true;
                if(_p[_i].block != FUDE_ZOOM_NONE) {
                    ((u8*)_c->block_quiet.memory)[_p[_i].block] = 0u;
                }
            }
        }
        if(!_changed) {
            break;
        }
        _c->wake = _c->wake || _logic;
        _jumped = true;
        _ok = fzc_newton(&_x, FUDE_ZOOM_NONE);
    }
    if(_ok && _c->suspected) {
        for(u32 _i = 0; _i < _np; _i++) {
            _p[_i].suspect = false;   // Solved again: nothing to look at.
        }
        _c->suspected = false;
    }
    if(_ok) {
        // A node only logic is on: its level's volts, as the analog circuit does not know it (it is in no block:
        // circuit_blocks.c's fzc_outside); 0 V released or unknown, as when nothing but GMIN held it.
        f64* _v = (f64*)_c->v.memory;
        for(u32 _node = 1; _node < _c->nodes; _node++) {
            const u8 _level = fude_zoom_logic_node(_c->logic, _node);
            if(_level != 0xFFu) {
                _v[_node] = _level == FUDE_SIM_1 ? FZC_LOGIC_V : 0.0;
            }
        }
        fzc_quiet_end(&_x, true);
        if(_tran) {
            _c->be_next = false;   // (taken by this step: only a jump in it asks again — a part's own, in fzc_after)
        }
        fzc_after(&_x);
        if(_tran) {
            _c->time     += _dt;
            _c->step_last = _dt;
            _c->be_last   = _x.be;
            // A jump anywhere in the step — the logic or what is stepped settling within it, or a part's own step
            // after it (a thyristor fired, an encoder's contact, an LM3914's outputs: fude_zoom_circuit_jump) —: the
            // next step backward Euler (and short: fude_zoom_circuit_advance).
            _c->be_next = _c->be_next || _jumped;
        }
    }
    if(!_ok) {
        fzc_quiet_end(&_x, false);
    }
    _c->wake = false;
    _c->ok = _ok;
    return _ok;
}
