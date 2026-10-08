// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SIM_SPARSE_H
#define FUDE_SIM_SPARSE_H

#include "rde.h"

// ===========================================================================
// A SPARSE SYSTEM: a square matrix whose entries' places are known before
// their values (a circuit's nodes, and which of them its parts join), solved
// again and again as the values change (a step, a Newton iteration).
//
// Its SHAPE is worked out once: the unknowns ordered so that factoring it
// fills in little (minimum degree: the one with the fewest others joined to
// it first, those it was joined to then joined to each other — the fill),
// and each row's places kept in order (compressed rows, L before the
// diagonal, U after). Structurally symmetric: a place at (i, j) is one at
// (j, i) too. Then, each time: its entries written into their places
// (val), factored (lu: L unit below the diagonal, U on and above it,
// without row swaps — a pivot too small says so, and the caller solves it
// another way), and solved.
//
// Indices are the caller's (0 .. n-1); the order is the system's own.
// ===========================================================================

#define FUDE_SIM_SPARSE_NONE 0xFFFFFFFFu

typedef struct {
    u32 n;
    rde_arr TYPE(u32) order;    // each position's unknown (the caller's index)
    rde_arr TYPE(u32) rank;     // each unknown's position
    rde_arr TYPE(u32) row_at;   // n + 1: each row's first place (rows by position)
    rde_arr TYPE(u32) col;      // each place's column (a position), in order along its row
    rde_arr TYPE(u32) diag;     // each row's diagonal place
    rde_arr TYPE(f64) val;      // the matrix: each place's entry
    rde_arr TYPE(f64) lu;       // its factors, place by place
    rde_arr TYPE(f64) work;     // n (scratch: one row spread out, the solution by position)
} fude_sim_sparse;

void fude_sim_sparse_init(fude_sim_sparse* _s);
void fude_sim_sparse_free(fude_sim_sparse* _s);
// Shaped for _n unknowns with entries at _pairs (each i << 32 | j, either way round, repeats allowed; every diagonal
// one is there anyway): ordered, its fill worked out, its entries zero.
void fude_sim_sparse_shape(fude_sim_sparse* _s, u32 _n, const u64* _pairs, u32 _count);
// The place of the entry at row _i, column _j (the caller's indices). FUDE_SIM_SPARSE_NONE: the shape has none there.
u32  fude_sim_sparse_find(const fude_sim_sparse* _s, u32 _i, u32 _j);
// Every entry zero.
void fude_sim_sparse_clear(fude_sim_sparse* _s);
// _g added to each diagonal entry.
void fude_sim_sparse_add_diagonal(fude_sim_sparse* _s, f64 _g);
// The entries factored. False: a pivot too small beside the rest of its row (singular, or it needs rows swapped).
b8   fude_sim_sparse_factor(fude_sim_sparse* _s);
// _x = A⁻¹ _b, as last factored (the caller's indices; _x may be _b).
void fude_sim_sparse_solve(fude_sim_sparse* _s, const f64* _b, f64* _x);
// The entries as a dense n × n matrix, row-major (the caller's indices) into _out.
void fude_sim_sparse_dense(const fude_sim_sparse* _s, f64* _out);
// Places in the factors (the fill included): how much work and room a solve takes.
u32  fude_sim_sparse_places(const fude_sim_sparse* _s);

#endif
