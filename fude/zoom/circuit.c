// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/circuit_parts.h"

// ===========================================================================
// See circuit.h.
//
// circuit.c is the coordinator of the circuit: a circuit set up and freed, worked out from the canvas, taken back to
// its start, and run on in time (solved for DC, stepped as finely as what happens needs, or in steps of its longest);
// what its parts hold beyond their state, and its nodes' voltages. Everything else is in the circuit_*.c modules, by
// what it does; circuit_parts.h is what they share and says which module holds what.
//
// Every module's own functions, types and constants are prefixed fzc_ and FZC_. What is solved is in SI units: volts,
// amperes, ohms, siemens, farads, henries, seconds. Main thread only, as the whole circuit.
// ===========================================================================

// See the header: every array on the std allocator, its longest step a millisecond, and its logic made (owned by it).
void fude_zoom_circuit_init(fude_zoom_circuit* _c) {
    rde_memset(_c, 0, sizeof(*_c));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _c->parts      = rde_arr_new(sizeof(fude_zoom_circuit_part), _heap);
    _c->wires      = rde_arr_new(sizeof(fude_zoom_circuit_wire), _heap);
    _c->cut_vertex = rde_arr_new(sizeof(u32), _heap);
    _c->pin_vertex = rde_arr_new(sizeof(u32), _heap);
    _c->pin_nodes  = rde_arr_new(sizeof(u32), _heap);
    _c->pin_amps   = rde_arr_new(sizeof(f64), _heap);
    _c->amps_keep  = rde_arr_new(sizeof(f64), _heap);
    _c->part_scratch = rde_arr_new(sizeof(f64), _heap);
    _c->wire_inject  = rde_arr_new(sizeof(f64), _heap);
    _c->wire_parent  = rde_arr_new(sizeof(u32), _heap);
    _c->wire_order   = rde_arr_new(sizeof(u32), _heap);
    _c->wire_seen    = rde_arr_new(sizeof(u8), _heap);
    _c->wire_count   = rde_arr_new(sizeof(u32), _heap);
    _c->wire_adj     = rde_arr_new(sizeof(u32), _heap);
    _c->wire_fill    = rde_arr_new(sizeof(u32), _heap);
    _c->a          = rde_arr_new(sizeof(f64), _heap);
    _c->rhs        = rde_arr_new(sizeof(f64), _heap);
    _c->x          = rde_arr_new(sizeof(f64), _heap);
    _c->v          = rde_arr_new(sizeof(f64), _heap);
    _c->v_keep     = rde_arr_new(sizeof(f64), _heap);
    _c->limits_of  = rde_arr_new(sizeof(u32), _heap);
    _c->events     = rde_arr_new(sizeof(fude_zoom_circuit_event), _heap);
    _c->node_block  = rde_arr_new(sizeof(u32), _heap);
    _c->node_slot   = rde_arr_new(sizeof(u32), _heap);
    _c->block_first = rde_arr_new(sizeof(u32), _heap);
    _c->block_size  = rde_arr_new(sizeof(u32), _heap);
    _c->block_system = rde_arr_new(sizeof(fude_sim_sparse), _heap);
    _c->block_reference = rde_arr_new(sizeof(u32), _heap);
    _c->joins       = rde_arr_new(sizeof(u64), _heap);
    _c->node_held   = rde_arr_new(sizeof(u32), _heap);
    _c->held_pins   = rde_arr_new(sizeof(u64), _heap);
    _c->couplings   = rde_arr_new(sizeof(u64), _heap);
    _c->block_still = rde_arr_new(sizeof(u8), _heap);
    _c->block_live  = rde_arr_new(sizeof(u8), _heap);
    _c->block_quiet = rde_arr_new(sizeof(u8), _heap);
    _c->v_step      = rde_arr_new(sizeof(f64), _heap);
    _c->part_key    = rde_arr_new(sizeof(f64), _heap);
    _c->key_at      = rde_arr_new(sizeof(u32), _heap);
    _c->block_done  = rde_arr_new(sizeof(u8), _heap);
    _c->block_most  = rde_arr_new(sizeof(f64), _heap);
    _c->cols        = rde_arr_new(sizeof(u32), _heap);
    _c->store       = rde_arr_new(sizeof(u8), _heap);
    _c->audio       = rde_arr_new(sizeof(f32), _heap);
    _c->step       = 1e-3;
    _c->logic      = (fude_zoom_logic*)_heap->calloc(_heap->allocator, 1, sizeof(fude_zoom_logic));
    fude_zoom_logic_init(_c->logic);
}

// See the header. Leaves the struct zeroed.
void fude_zoom_circuit_destroy(fude_zoom_circuit* _c) {
    rde_arr_free(&_c->parts);
    rde_arr_free(&_c->wires);
    rde_arr_free(&_c->cut_vertex);
    rde_arr_free(&_c->pin_vertex);
    rde_arr_free(&_c->pin_nodes);
    rde_arr_free(&_c->pin_amps);
    rde_arr_free(&_c->amps_keep);
    rde_arr_free(&_c->part_scratch);
    rde_arr_free(&_c->wire_inject);
    rde_arr_free(&_c->wire_parent);
    rde_arr_free(&_c->wire_order);
    rde_arr_free(&_c->wire_seen);
    rde_arr_free(&_c->wire_count);
    rde_arr_free(&_c->wire_adj);
    rde_arr_free(&_c->wire_fill);
    rde_arr_free(&_c->a);
    rde_arr_free(&_c->rhs);
    rde_arr_free(&_c->x);
    rde_arr_free(&_c->v);
    rde_arr_free(&_c->v_keep);
    rde_arr_free(&_c->limits_of);
    rde_arr_free(&_c->events);
    rde_arr_free(&_c->node_block);
    rde_arr_free(&_c->node_slot);
    rde_arr_free(&_c->block_first);
    rde_arr_free(&_c->block_size);
    fzc_systems_free(_c);
    rde_arr_free(&_c->block_system);
    rde_arr_free(&_c->block_reference);
    rde_arr_free(&_c->joins);
    rde_arr_free(&_c->node_held);
    rde_arr_free(&_c->held_pins);
    rde_arr_free(&_c->couplings);
    rde_arr_free(&_c->block_still);
    rde_arr_free(&_c->block_live);
    rde_arr_free(&_c->block_quiet);
    rde_arr_free(&_c->v_step);
    rde_arr_free(&_c->part_key);
    rde_arr_free(&_c->key_at);
    rde_arr_free(&_c->block_done);
    rde_arr_free(&_c->block_most);
    rde_arr_free(&_c->cols);
    rde_arr_free(&_c->store);
    rde_arr_free(&_c->audio);
    if(_c->logic != NULL) {
        fude_zoom_logic_destroy(_c->logic);
        rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
        _heap->free(_heap->allocator, _c->logic);
    }
    rde_memset(_c, 0, sizeof(*_c));
}

// See the header: fude_zoom_circuit_build_in with everything in scope.
u32 fude_zoom_circuit_build(fude_zoom_circuit* _c, const fude_zoom_scene* _s) {
    return fude_zoom_circuit_build_in(_c, _s, NULL);
}

// See the header: the parts let go (they are read from their texts again at the next build, none of them kept: so
// nothing of them need be cleared first), the voltages, time, sound and logic cleared.
void fude_zoom_circuit_reset(fude_zoom_circuit* _c) {
    rde_arr_clear(&_c->parts);   // Its states read from the parts' texts again at the next build,
    fude_zoom_logic_forget(_c->logic);   // nor what its logic held carried over.
    rde_memset(_c->v.memory, 0, rde_arr_length(&_c->v) * sizeof(f64));
    _c->time     = 0.0;
    _c->step_now = 0.0;
    _c->be_next  = true;
    _c->ok   = false;
    rde_arr_clear(&_c->audio);
    _c->audio_next = 0.0;
    _c->audio_dc   = 0.0;
}

// See the header, then its wires' currents.
b8 fude_zoom_circuit_dc(fude_zoom_circuit* _c) {
    const b8 _ok = fzc_at(_c, false, _c->step);
    if(_ok) {
        fzc_wire_currents(_c);
    }
    return _ok;
}

// See the header.
b8 fude_zoom_circuit_run(fude_zoom_circuit* _c, f64 _dt, u32 _max_steps) {
    return fude_zoom_circuit_advance(_c, _dt, _max_steps, true);
}

// See the header. Each step is its planned size held to the longest step and the next breakpoint, at least
// FZC_STEP_LEAST; the last lands on the end. The size planned after a step is what its changes say, or an eighth of
// it when it jumped.
b8 fude_zoom_circuit_advance(fude_zoom_circuit* _c, f64 _span, u32 _max_steps, b8 _wires) {
    const f64 _most = _c->step > 0.0 ? _c->step : 1e-3;
    const f64 _end  = _c->time + rde_math_max_f64(_span, 0.0);
    b8 _ok = true;
    for(u32 _k = 0; _ok && _k < _max_steps && _c->time < _end - 0.5 * FZC_STEP_LEAST; _k++) {
        const f64 _planned = rde_math_max_f64(rde_math_min_f64(rde_math_min_f64(_c->step_now > 0.0 ? _c->step_now : FZC_STEP_FIRST, _most), fzc_breakpoint(_c)), FZC_STEP_LEAST);
        // The last one landing on the end: never a sliver left over.
        const f64 _h = _c->time + _planned > _end - FZC_STEP_LEAST ? _end - _c->time : _planned;
        _ok = fzc_at(_c, true, _h);
        // The next: as this one's changes say — its worst change at most what a step should make (grown at most four
        // times, cut at most to a quarter); as it was when this one was cut short to land somewhere, unless it changed
        // too much.
        const f64 _r     = _c->step_ratio;
        const f64 _scale = _r > 1e-12 ? rde_math_min_f64(rde_math_max_f64(0.9 / rde_math_sqrt_f64(_r), 0.25), FZC_STEP_GROW) : FZC_STEP_GROW;
        const f64 _base  = _h < _planned && _scale >= 1.0 ? rde_math_max_f64(_planned, _c->step_now) : _h;
        _c->step_now = rde_math_min_f64(rde_math_max_f64(_base * (_h < _planned && _scale >= 1.0 ? 1.0 : _scale), FZC_STEP_LEAST), _most);
        // A jump within it (fzc_at: the logic, what is stepped, a part's own step after it): the next an eighth of it,
        // as a jump between steps cuts it (fude_zoom_circuit_jump). Not again on the step right after a jump (taken by
        // backward Euler, be_last): with an edge at every step (logic oscillating faster than any step) the steps would
        // only shrink to the least, and show nothing more.
        if(_ok && _c->be_next && !_c->be_last) {
            _c->step_now = rde_math_max_f64(_h / 8.0, FZC_STEP_FIRST);
        }
    }
    if(_ok && _wires) {
        fzc_wire_currents(_c);
    }
    return _ok;
}

// See the header: every step the same, its longest.
b8 fude_zoom_circuit_steps(fude_zoom_circuit* _c, u32 _steps, b8 _wires) {
    const f64 _step = _c->step > 0.0 ? _c->step : 1e-3;
    b8 _ok = true;
    for(u32 _k = 0; _k < _steps && _ok; _k++) {
        _ok = fzc_at(_c, true, _step);
    }
    if(_ok && _wires) {
        fzc_wire_currents(_c);
    }
    return _ok;
}

// See the header: the step cut to an eighth (FZC_STEP_FIRST at least).
void fude_zoom_circuit_jump(fude_zoom_circuit* _c) {
    _c->be_next  = true;
    _c->step_now = _c->step_now > 0.0 ? rde_math_max_f64(_c->step_now / 8.0, FZC_STEP_FIRST) : 0.0;
}

// See the header.
u8* fude_zoom_circuit_store(fude_zoom_circuit* _c, const fude_zoom_circuit_part* _p) {
    return _p != NULL && _p->store_size > 0u && (usize)_p->store_at + _p->store_size <= rde_arr_length(&_c->store) ? (u8*)_c->store.memory + _p->store_at : NULL;
}

// See the header.
const u8* fude_zoom_circuit_store_of(const fude_zoom_circuit* _c, const fude_zoom_circuit_part* _p) {
    return _p != NULL && _p->store_size > 0u && (usize)_p->store_at + _p->store_size <= rde_arr_length(&_c->store) ? (const u8*)_c->store.memory + _p->store_at : NULL;
}

// See the header.
f64 fude_zoom_circuit_volts(const fude_zoom_circuit* _c, u32 _node) {
    return _node != FUDE_ZOOM_NONE && _node != 0u && _node < _c->nodes && _node < (u32)rde_arr_length(&_c->v) ? ((const f64*)_c->v.memory)[_node] : 0.0;
}
