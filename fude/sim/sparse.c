// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "sim/sparse.h"

// ===========================================================================
// See sparse.h.
//
// Internal names start fss_ (FSS_ for constants). fude_sim_sparse_shape does all the work on the shape once: the
// minimum-degree order, the fill, and the compressed rows. Factoring and solving then only walk the places it made.
// Both use `work` as their one scratch row, and leave it zero.
// ===========================================================================
#define FSS_PIVOT_SMALL 1e-14   // a pivot this much smaller than the largest of its row: too small

// See the header: every array made empty, on the std allocator.
void fude_sim_sparse_init(fude_sim_sparse* _s) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _s->n      = 0;
    _s->order  = rde_arr_new(sizeof(u32), _heap);
    _s->rank   = rde_arr_new(sizeof(u32), _heap);
    _s->row_at = rde_arr_new(sizeof(u32), _heap);
    _s->col    = rde_arr_new(sizeof(u32), _heap);
    _s->diag   = rde_arr_new(sizeof(u32), _heap);
    _s->val    = rde_arr_new(sizeof(f64), _heap);
    _s->lu     = rde_arr_new(sizeof(f64), _heap);
    _s->work   = rde_arr_new(sizeof(f64), _heap);
}

// See the header.
void fude_sim_sparse_free(fude_sim_sparse* _s) {
    rde_arr_free(&_s->order);
    rde_arr_free(&_s->rank);
    rde_arr_free(&_s->row_at);
    rde_arr_free(&_s->col);
    rde_arr_free(&_s->diag);
    rde_arr_free(&_s->val);
    rde_arr_free(&_s->lu);
    rde_arr_free(&_s->work);
}

// rde_sort's order for u32s: smallest first.
RDE_INTERNAL int fss_u32_cmp(const any _a, const any _b) {
    const u32 _x = *(const u32*)_a, _y = *(const u32*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// Sorts a list of u32s and drops the repeats.
RDE_INTERNAL void fss_tidy(rde_arr* _list) {
    u32* _v = (u32*)_list->memory;
    const u32 _n = (u32)rde_arr_length(_list);
    if(_n < 2u) {
        return;
    }
    rde_sort(_v, _n, sizeof(u32), fss_u32_cmp);
    u32 _kept = 1u;
    for(u32 _i = 1; _i < _n; _i++) {
        if(_v[_i] != _v[_kept - 1u]) {
            _v[_kept++] = _v[_i];
        }
    }
    rde_arr_resize(_list, _kept);
}

// --- the unknowns ordered: minimum degree ------------------------------------------------------------
//
// A min-heap of (degree, unknown): a changed degree is pushed again, and an entry whose unknown is gone or whose
// degree is no longer its own is let go when it comes up (the same entry may be in it twice: the second is let go).

// See the header. Three steps: who is joined to whom; the unknowns eliminated in minimum-degree order, each one's
// others still left kept as its row's places after the diagonal and joined to each other (the fill); then the
// compressed rows, built from those lists by position. A list moves, without a copy, from _adj to _upper when its
// unknown is eliminated.
void fude_sim_sparse_shape(fude_sim_sparse* _s, u32 _n, const u64* _pairs, u32 _count) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _s->n = _n;
    // Who is joined to whom (each unknown's others, in order, each once).
    rde_arr TYPE(rde_arr) _adj_arr = rde_arr_new(sizeof(rde_arr), _heap);
    rde_arr_resize(&_adj_arr, _n);
    rde_arr* _adj = (rde_arr*)_adj_arr.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        _adj[_i] = rde_arr_new(sizeof(u32), _heap);
    }
    for(u32 _k = 0; _k < _count; _k++) {
        const u32 _i = (u32)(_pairs[_k] >> 32), _j = (u32)(_pairs[_k] & 0xFFFFFFFFull);
        if(_i != _j && _i < _n && _j < _n) {
            rde_arr_add(&_adj[_i], (any)&_j);
            rde_arr_add(&_adj[_j], (any)&_i);
        }
    }
    for(u32 _i = 0; _i < _n; _i++) {
        fss_tidy(&_adj[_i]);
    }
    // Eliminated one by one, the fewest joined first: its others (still there) are its row's places after the
    // diagonal, and are joined to each other from then on.
    rde_arr TYPE(rde_arr) _upper_arr = rde_arr_new(sizeof(rde_arr), _heap);
    rde_arr_resize(&_upper_arr, _n);
    rde_arr* _upper = (rde_arr*)_upper_arr.memory;
    rde_arr TYPE(u8)  _alive_arr = rde_arr_new(sizeof(u8), _heap);
    rde_heap          _queue     = rde_heap_new(sizeof(u64), rde_sort_compare_u64, _heap);   // degree << 32 | unknown
    rde_arr TYPE(u32) _merged    = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_alive_arr, _n);
    u8* _alive = (u8*)_alive_arr.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        _alive[_i] = 1u;
        const u64 _item = ((u64)rde_arr_length(&_adj[_i]) << 32) | _i;
        rde_heap_push(&_queue, (any)&_item);
    }
    rde_arr_resize(&_s->order, 0u);
    rde_arr_resize(&_s->rank, _n);
    u32* _rank = (u32*)_s->rank.memory;
    u64 _top;
    while(rde_heap_pop(&_queue, (any)&_top)) {
        const u32 _v = (u32)(_top & 0xFFFFFFFFull);
        if(!_alive[_v] || (u32)(_top >> 32) != (u32)rde_arr_length(&_adj[_v])) {
            continue;   // (gone, or its degree since changed: pushed again then)
        }
        _alive[_v] = 0u;
        _rank[_v] = (u32)rde_arr_length(&_s->order);
        rde_arr_add(&_s->order, (any)&_v);
        _upper[_v] = _adj[_v];   // (its others: all still there — the eliminated are taken out of every list)
        const u32* _nb = (const u32*)_upper[_v].memory;
        const u32  _nn = (u32)rde_arr_length(&_upper[_v]);
        for(u32 _k = 0; _k < _nn; _k++) {
            // Its other a: joined to the rest of them, _v taken out.
            const u32 _a = _nb[_k];
            const u32* _l = (const u32*)_adj[_a].memory;
            const u32  _ln = (u32)rde_arr_length(&_adj[_a]);
            rde_arr_clear(&_merged);
            u32 _x = 0, _y = 0;
            while(_x < _ln || _y < _nn) {
                u32 _next;
                if(_y >= _nn || (_x < _ln && _l[_x] < _nb[_y])) {
                    _next = _l[_x++];
                } else if(_x >= _ln || _nb[_y] < _l[_x]) {
                    _next = _nb[_y++];
                } else {
                    _next = _l[_x++];
                    _y++;
                }
                if(_next != _a && _next != _v) {
                    rde_arr_add(&_merged, (any)&_next);
                }
            }
            rde_arr_clear(&_adj[_a]);
            rde_arr_add_arr(&_adj[_a], &_merged);
            const u64 _item = ((u64)rde_arr_length(&_adj[_a]) << 32) | _a;
            rde_heap_push(&_queue, (any)&_item);
        }
        _adj[_v] = (rde_arr){ 0 };   // (now _upper's)
    }
    // Rows by position: before the diagonal, the earlier positions whose others included this one (in order, as they
    // come); after it, its own others' positions (in order).
    const u32* _order = (const u32*)_s->order.memory;
    rde_arr TYPE(u32) _lower_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_lower_arr, _n);
    u32* _lower = (u32*)_lower_arr.memory;   // (how many before each row's diagonal)
    for(u32 _p = 0; _p < _n; _p++) {
        rde_arr* _u = &_upper[_order[_p]];
        u32* _uv = (u32*)_u->memory;
        for(u32 _k = 0; _k < (u32)rde_arr_length(_u); _k++) {
            _uv[_k] = _rank[_uv[_k]];   // (unknowns → positions)
            _lower[_uv[_k]]++;
        }
        rde_sort(_uv, rde_arr_length(_u), sizeof(u32), fss_u32_cmp);
    }
    rde_arr_resize(&_s->row_at, _n + 1u);
    rde_arr_resize(&_s->diag, _n);
    u32* _row_at = (u32*)_s->row_at.memory;
    u32* _diag   = (u32*)_s->diag.memory;
    u32 _places = 0;
    for(u32 _p = 0; _p < _n; _p++) {
        _row_at[_p] = _places;
        _diag[_p]   = _places + _lower[_p];
        _places    += _lower[_p] + 1u + (u32)rde_arr_length(&_upper[_order[_p]]);
    }
    _row_at[_n] = _places;
    rde_arr_resize(&_s->col, _places);
    u32* _col = (u32*)_s->col.memory;
    for(u32 _p = 0; _p < _n; _p++) {
        _lower[_p] = _row_at[_p];   // (now: where the next one before the diagonal goes)
    }
    for(u32 _p = 0; _p < _n; _p++) {
        _col[_diag[_p]] = _p;
        const rde_arr* _u = &_upper[_order[_p]];
        const u32* _uv = (const u32*)_u->memory;
        for(u32 _k = 0; _k < (u32)rde_arr_length(_u); _k++) {
            _col[_diag[_p] + 1u + _k] = _uv[_k];
            _col[_lower[_uv[_k]]++]   = _p;   // (rows by position in turn: each row's earlier ones in order)
        }
    }
    rde_arr_resize(&_s->val, 0u);
    rde_arr_resize(&_s->val, _places);
    rde_arr_resize(&_s->lu, _places);
    rde_arr_resize(&_s->work, 0u);
    rde_arr_resize(&_s->work, _n);
    for(u32 _i = 0; _i < _n; _i++) {
        rde_arr_free(&_upper[_i]);
    }
    rde_arr_free(&_upper_arr);
    rde_arr_free(&_adj_arr);   // (every list now _upper's, freed above)
    rde_arr_free(&_alive_arr);
    rde_heap_free(&_queue);
    rde_arr_free(&_merged);
    rde_arr_free(&_lower_arr);
}

// See the header: a binary search along the row, whose columns are in order.
u32 fude_sim_sparse_find(const fude_sim_sparse* _s, u32 _i, u32 _j) {
    if(_i >= _s->n || _j >= _s->n) {
        return FUDE_SIM_SPARSE_NONE;
    }
    const u32* _rank = (const u32*)_s->rank.memory;
    const u32* _col  = (const u32*)_s->col.memory;
    const u32  _p = _rank[_i], _q = _rank[_j];
    u32 _lo = ((const u32*)_s->row_at.memory)[_p], _hi = ((const u32*)_s->row_at.memory)[_p + 1u];
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(_col[_mid] < _q) {
            _lo = _mid + 1u;
        } else {
            _hi = _mid;
        }
    }
    return _lo < ((const u32*)_s->row_at.memory)[_p + 1u] && _col[_lo] == _q ? _lo : FUDE_SIM_SPARSE_NONE;
}

// See the header.
void fude_sim_sparse_clear(fude_sim_sparse* _s) {
    rde_memset(_s->val.memory, 0, rde_arr_length(&_s->val) * sizeof(f64));
}

// See the header.
void fude_sim_sparse_add_diagonal(fude_sim_sparse* _s, f64 _g) {
    f64* _val = (f64*)_s->val.memory;
    const u32* _diag = (const u32*)_s->diag.memory;
    for(u32 _p = 0; _p < _s->n; _p++) {
        _val[_diag[_p]] += _g;
    }
}

// See the header. Row by row: the row is spread out into `work`, each earlier row it has a place in is taken away in
// order (its multiplier left at that place, L's entry), and the row is gathered back into `lu`. It stops at a pivot
// that is not finite, not bigger than FSS_PIVOT_SMALL times the largest entry from the diagonal on, or not bigger
// than 1e-300; `work` is zero again by then.
b8 fude_sim_sparse_factor(fude_sim_sparse* _s) {
    const u32  _n      = _s->n;
    const u32* _row_at = (const u32*)_s->row_at.memory;
    const u32* _col    = (const u32*)_s->col.memory;
    const u32* _diag   = (const u32*)_s->diag.memory;
    f64* _lu = (f64*)_s->lu.memory;
    f64* _w  = (f64*)_s->work.memory;   // (zero between rows)
    rde_memcpy(_lu, _s->val.memory, rde_arr_length(&_s->val) * sizeof(f64));
    for(u32 _p = 0; _p < _n; _p++) {
        const u32 _from = _row_at[_p], _to = _row_at[_p + 1u], _d = _diag[_p];
        for(u32 _e = _from; _e < _to; _e++) {
            _w[_col[_e]] = _lu[_e];
        }
        // Each earlier row this one has a place in, in order: its multiple taken away (every place that touches is this
        // row's too — the fill says so).
        for(u32 _e = _from; _e < _d; _e++) {
            const u32 _k = _col[_e];
            const f64 _l = _w[_k] / _lu[_diag[_k]];
            _w[_k] = _l;
            if(_l == 0.0) {
                continue;
            }
            for(u32 _f = _diag[_k] + 1u; _f < _row_at[_k + 1u]; _f++) {
                _w[_col[_f]] -= _l * _lu[_f];
            }
        }
        f64 _big = 0.0;
        for(u32 _e = _d; _e < _to; _e++) {
            _big = rde_math_max_f64(_big, rde_math_abs_f64(_w[_col[_e]]));
        }
        const f64 _pivot = _w[_p];
        for(u32 _e = _from; _e < _to; _e++) {
            _lu[_e] = _w[_col[_e]];
            _w[_col[_e]] = 0.0;
        }
        if(!rde_math_is_finite_f64(_pivot) || !(rde_math_abs_f64(_pivot) > FSS_PIVOT_SMALL * _big) || !(rde_math_abs_f64(_pivot) > 1e-300)) {
            return false;
        }
    }
    return true;
}

// See the header: forward through L (its diagonal 1), back through U, in positions in `work`, then each answer put at
// the caller's index; `work` is zeroed as it is read, for the next factoring.
void fude_sim_sparse_solve(fude_sim_sparse* _s, const f64* _b, f64* _x) {
    const u32  _n      = _s->n;
    const u32* _row_at = (const u32*)_s->row_at.memory;
    const u32* _col    = (const u32*)_s->col.memory;
    const u32* _diag   = (const u32*)_s->diag.memory;
    const u32* _order  = (const u32*)_s->order.memory;
    const f64* _lu = (const f64*)_s->lu.memory;
    f64* _y = (f64*)_s->work.memory;
    for(u32 _p = 0; _p < _n; _p++) {
        f64 _sum = _b[_order[_p]];
        for(u32 _e = _row_at[_p]; _e < _diag[_p]; _e++) {
            _sum -= _lu[_e] * _y[_col[_e]];
        }
        _y[_p] = _sum;
    }
    for(u32 _p = _n; _p-- > 0u;) {
        f64 _sum = _y[_p];
        for(u32 _e = _diag[_p] + 1u; _e < _row_at[_p + 1u]; _e++) {
            _sum -= _lu[_e] * _y[_col[_e]];
        }
        _y[_p] = _sum / _lu[_diag[_p]];
    }
    for(u32 _p = 0; _p < _n; _p++) {
        _x[_order[_p]] = _y[_p];
        _y[_p] = 0.0;   // (zero again: the factoring's scratch)
    }
}

// See the header. _out holds n × n numbers.
void fude_sim_sparse_dense(const fude_sim_sparse* _s, f64* _out) {
    const u32  _n      = _s->n;
    const u32* _row_at = (const u32*)_s->row_at.memory;
    const u32* _col    = (const u32*)_s->col.memory;
    const u32* _order  = (const u32*)_s->order.memory;
    const f64* _val    = (const f64*)_s->val.memory;
    rde_memset(_out, 0, (usize)_n * _n * sizeof(f64));
    for(u32 _p = 0; _p < _n; _p++) {
        for(u32 _e = _row_at[_p]; _e < _row_at[_p + 1u]; _e++) {
            _out[(usize)_order[_p] * _n + _order[_col[_e]]] = _val[_e];
        }
    }
}

// See the header.
u32 fude_sim_sparse_places(const fude_sim_sparse* _s) {
    return (u32)rde_arr_length(&_s->col);
}
