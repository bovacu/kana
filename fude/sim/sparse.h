// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SIM_SPARSE_H
#define FUDE_SIM_SPARSE_H

#include "rde.h"

// ===========================================================================
// Sparse system: a square matrix whose entries' places are known before their values (a circuit's nodes, and which
// of them its parts join), solved again and again as the values change (each time step, each Newton iteration).
//
// Where it sits: `fude/sim`, for the analog engine. zoom/circuit_solve.c keeps one for each independent block of a
// circuit's unknowns, and tests/sim tests it on its own.
// Its main type, fude_sim_sparse, is the caller's struct: set up by fude_sim_sparse_init and released by
// fude_sim_sparse_free. Its SHAPE is worked out once (fude_sim_sparse_shape); then, each time, its entries are
// cleared, written into their places in `val` (fude_sim_sparse_find gives each place), factored and solved.
// Threads: called on the app's main thread. It has no state of its own, so separate systems could be used on
// different threads, but one system must not be used from two threads at once.
//
// Design:
// - The unknowns are put in the order that makes factoring fill in least: minimum degree, which takes first the
//   unknown joined to the fewest others; the ones it was joined to are then joined to each other (the fill).
// - Each row keeps its places in order, in compressed rows: L's before the diagonal, U's after it. The shape is
//   structurally symmetric: a place at (i, j) means there is one at (j, i) too.
// - It factors without swapping rows (L has a unit diagonal below it; U is on and above the diagonal), so the shape
//   never changes between factorings. A pivot too small is reported instead, and the caller solves that system
//   another way (zoom/circuit_solve.c copies it out with fude_sim_sparse_dense and solves it with rows swapped).
//
// Indices given to and returned by the functions are the caller's (0 to n - 1); the order is the system's own.
// ===========================================================================

// "No place": the shape has no entry there.
#define FUDE_SIM_SPARSE_NONE 0xFFFFFFFFu

// The system. `n` is how many unknowns it has. A place is an index into col, val and lu.
typedef struct {
    u32 n;
    rde_arr TYPE(u32) order;    // the unknown at each position (the caller's index)
    rde_arr TYPE(u32) rank;     // each unknown's position
    rde_arr TYPE(u32) row_at;   // n + 1 of them: each row's first place (rows by position)
    rde_arr TYPE(u32) col;      // each place's column (a position), in order along its row
    rde_arr TYPE(u32) diag;     // each row's diagonal place
    rde_arr TYPE(f64) val;      // the matrix: each place's entry, which the caller writes
    rde_arr TYPE(f64) lu;       // its factors, place by place
    rde_arr TYPE(f64) work;     // n numbers of scratch (a row spread out, the solution by position), zero between uses
} fude_sim_sparse;

// Sets up an empty system, and frees what one holds.
void fude_sim_sparse_init(fude_sim_sparse* _s);
void fude_sim_sparse_free(fude_sim_sparse* _s);
// Shapes the system for _n unknowns with entries at _pairs, each written as i << 32 | j (either way round, repeats
// allowed; pairs out of range are ignored, and every diagonal entry is there anyway). It orders the unknowns, works
// out the fill, and sets every entry to zero. It may be called again to shape the system anew.
void fude_sim_sparse_shape(fude_sim_sparse* _s, u32 _n, const u64* _pairs, u32 _count);
// Returns the place (an index into val) of the entry at row _i, column _j (the caller's indices), or
// FUDE_SIM_SPARSE_NONE when the shape has no entry there.
u32  fude_sim_sparse_find(const fude_sim_sparse* _s, u32 _i, u32 _j);
// Sets every entry to zero.
void fude_sim_sparse_clear(fude_sim_sparse* _s);
// Adds _g to every diagonal entry.
void fude_sim_sparse_add_diagonal(fude_sim_sparse* _s, f64 _g);
// Factors the entries. Returns false when a pivot is too small beside the rest of its row (the matrix is singular, or
// it needs rows swapped); the factors cannot be used to solve then.
b8   fude_sim_sparse_factor(fude_sim_sparse* _s);
// Solves _x = A⁻¹ _b with the last factors, in the caller's indices. _x may be the same array as _b.
void fude_sim_sparse_solve(fude_sim_sparse* _s, const f64* _b, f64* _x);
// Writes the entries into _out as a dense n × n matrix, row by row, in the caller's indices.
void fude_sim_sparse_dense(const fude_sim_sparse* _s, f64* _out);
// Returns how many places the factors have, the fill included: a measure of the work and memory a solve takes.
u32  fude_sim_sparse_places(const fude_sim_sparse* _s);

#endif
