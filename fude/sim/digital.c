// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "sim/sim.h"

// ===========================================================================
// See sim.h. This is the digital engine, the run of a flat circuit (fude_sim_run); sim.h's "the run" section says
// what each function does and why the delays are inertial.
//
// Its helpers are fsr_ ("fude sim run"). The file goes: the event heap (fsr_heapify, fsr_push, fsr_pop); nets, a net's
// value from its drivers (fsr_resolve), and the run's start and end; firing events (fsr_fire, fsr_start_quiet), running
// on (with a loop that never rests stopped: fsr_runaway) and settling; carrying a run over (fude_sim_run_keep); and
// what models call (fude_sim_in, fude_sim_out and the rest).
//
// Every table of the run is an rde_arr made at its full size at the start (fsr_table) and indexed straight through
// its memory (FSR_U8, FSR_U32, FSR_U64); only the events grow after that. Each net's readers and drivers are two flat
// lists, net n's from first[n] up to first[n + 1], so a change to a net walks only that net's readers.
// ===========================================================================

// Returns an array of _n zeroed items of _size bytes, on the std allocator.
RDE_INTERNAL rde_arr fsr_table(usize _size, u32 _n) {
    rde_arr _a = rde_arr_new(_size, rde_memory_allocator_get_default_std());
    rde_arr_resize(&_a, _n);
    return _a;
}

// A run's table read as an array of u8, u32 or u64 (its memory, cast).
#define FSR_U8(_arr)  ((u8*)(_arr).memory)
#define FSR_U32(_arr) ((u32*)(_arr).memory)
#define FSR_U64(_arr) ((u64*)(_arr).memory)

// --- the events: a heap, the soonest first (then the first made) --------------------------------------

// True when _a goes before _b: it is sooner, or at the same time and made first (a lower seq).
RDE_INTERNAL b8 fsr_before(const fude_sim_event* _a, const fude_sim_event* _b) {
    return _a->time < _b->time || (_a->time == _b->time && _a->seq < _b->seq);
}

// Adds an event to the heap, sifting it up to its place.
RDE_INTERNAL void fsr_push(fude_sim_run* _run, fude_sim_event _ev) {
    rde_arr_add(&_run->events, (any)&_ev);
    fude_sim_event* _h = (fude_sim_event*)_run->events.memory;
    u32 _i = (u32)rde_arr_length(&_run->events) - 1u;
    while(_i > 0u) {
        const u32 _up = (_i - 1u) / 2u;
        if(!fsr_before(&_h[_i], &_h[_up])) {
            break;
        }
        const fude_sim_event _t = _h[_i];
        _h[_i] = _h[_up];
        _h[_up] = _t;
        _i = _up;
    }
}

// Makes the run's events a heap again after they were changed in place (some dropped, some moved): each sifted up in
// turn, as if pushed one after another.
RDE_INTERNAL void fsr_heapify(fude_sim_run* _run) {
    fude_sim_event* _ev = (fude_sim_event*)_run->events.memory;
    const u32 _n = (u32)rde_arr_length(&_run->events);
    for(u32 _e = 1; _e < _n; _e++) {
        for(u32 _c = _e; _c > 0u && fsr_before(&_ev[_c], &_ev[(_c - 1u) / 2u]); _c = (_c - 1u) / 2u) {
            const fude_sim_event _t = _ev[_c];
            _ev[_c] = _ev[(_c - 1u) / 2u];
            _ev[(_c - 1u) / 2u] = _t;
        }
    }
}

// Takes the soonest event off the heap and returns it: the last one moves to the top and sifts down. The heap must
// not be empty (every caller checks).
RDE_INTERNAL fude_sim_event fsr_pop(fude_sim_run* _run) {
    fude_sim_event* _h = (fude_sim_event*)_run->events.memory;
    const u32 _n = (u32)rde_arr_length(&_run->events);
    const fude_sim_event _top = _h[0];
    _h[0] = _h[_n - 1u];
    rde_arr_resize(&_run->events, _n - 1u);
    u32 _i = 0;
    for(;;) {
        const u32 _l = 2u * _i + 1u, _r = _l + 1u;
        u32 _m = _i;
        if(_l < _n - 1u && fsr_before(&_h[_l], &_h[_m])) { _m = _l; }
        if(_r < _n - 1u && fsr_before(&_h[_r], &_h[_m])) { _m = _r; }
        if(_m == _i) {
            break;
        }
        const fude_sim_event _t = _h[_i];
        _h[_i] = _h[_m];
        _h[_m] = _t;
        _i = _m;
    }
    return _top;
}

// --- nets ------------------------------------------------------------------------------------------

// A net's value from its drivers: those released (Z) leave it to the rest; 0 and 1 together, or an X, are X.
RDE_INTERNAL u8 fsr_resolve(const fude_sim_run* _run, u32 _net) {
    const u32* _first = FSR_U32(_run->drive_first);
    const u32* _ports = FSR_U32(_run->drive_ports);
    const u8*  _drive = FSR_U8(_run->drive);
    u8 _v = FUDE_SIM_Z;
    for(u32 _i = _first[_net]; _i < _first[_net + 1u]; _i++) {
        const u8 _d = _drive[_ports[_i]];
        if(_d == FUDE_SIM_Z) {
            continue;
        }
        if(_d == FUDE_SIM_X || (_v != FUDE_SIM_Z && _v != _d)) {
            return FUDE_SIM_X;
        }
        _v = _d;
    }
    return _v;
}

// True for a logic port: only those are put on the run's lists of readers and drivers.
RDE_INTERNAL b8 fsr_logic_port(const fude_sim_port* _p) {
    return _p->domain == FUDE_SIM_LOGIC;
}

// Used by fude_sim_run_init; it is defined after fsr_fire.
RDE_INTERNAL void fsr_start_quiet(fude_sim_run* _run);

// See sim.h. Sizes every table, lists each net's readers and drivers in two passes (counting, then filling the
// places the counts give), then starts the primitives.
void fude_sim_run_init(fude_sim_run* _run, const fude_sim_flat* _flat) {
    rde_memset(_run, 0, sizeof(*_run));
    _run->flat         = _flat;
    _run->events       = rde_arr_new(sizeof(fude_sim_event), rde_memory_allocator_get_default_std());
    _run->settle_limit = 200000u;
    const u32 _nets = _flat->nets, _np = (u32)rde_arr_length(&_flat->ports), _prims = (u32)rde_arr_length(&_flat->prims);
    const fude_sim_port* _ports = (const fude_sim_port*)_flat->ports.memory;
    const u32* _pn = (const u32*)_flat->port_nets.memory;
    const fude_sim_prim* _pr = (const fude_sim_prim*)_flat->prims.memory;
    _run->net_value   = fsr_table(sizeof(u8), _nets + 1u);
    _run->drive       = fsr_table(sizeof(u8), _np + 1u);
    _run->pending     = fsr_table(sizeof(u64), _np + 1u);
    _run->read_first  = fsr_table(sizeof(u32), _nets + 2u);
    _run->drive_first = fsr_table(sizeof(u32), _nets + 2u);
    _run->read_prims  = fsr_table(sizeof(u32), 1u);
    _run->drive_ports = fsr_table(sizeof(u32), 1u);
    _run->state       = fsr_table(sizeof(f64), _flat->state + 1u);
    // Each net's readers (a primitive once a net) and drivers, counted and then listed.
    rde_arr _seen = fsr_table(sizeof(u32), _nets + 1u), _rfill = fsr_table(sizeof(u32), _nets + 1u), _dfill = fsr_table(sizeof(u32), _nets + 1u);
    u32* _seen_net = FSR_U32(_seen);
    for(u32 _n = 0; _n <= _nets; _n++) {
        _seen_net[_n] = FUDE_SIM_NONE;
    }
    for(u32 _pass = 0; _pass < 2u; _pass++) {
        u32* _rf = FSR_U32(_run->read_first), *_df = FSR_U32(_run->drive_first);
        if(_pass == 1u) {
            for(u32 _n = 0; _n < _nets; _n++) {
                _rf[_n + 1u] += _rf[_n];
                _df[_n + 1u] += _df[_n];
            }
            rde_arr_resize(&_run->read_prims, _rf[_nets] + 1u);
            rde_arr_resize(&_run->drive_ports, _df[_nets] + 1u);
            for(u32 _n = 0; _n <= _nets; _n++) {
                _seen_net[_n] = FUDE_SIM_NONE;
            }
        }
        u32* _rp = FSR_U32(_run->read_prims), *_dp = FSR_U32(_run->drive_ports), *_rn = FSR_U32(_rfill), *_dn = FSR_U32(_dfill);
        for(u32 _p = 0; _p < _prims; _p++) {
            for(u32 _k = 0; _k < _pr[_p].count; _k++) {
                const u32 _port = _pr[_p].first + _k, _net = _pn[_port];
                if(!fsr_logic_port(&_ports[_port]) || _net >= _nets) {
                    continue;
                }
                const u8 _dir = _ports[_port].dir;
                if((_dir == FUDE_SIM_IN || _dir == FUDE_SIM_INOUT) && _seen_net[_net] != _p) {
                    _seen_net[_net] = _p;
                    if(_pass == 0u) { _rf[_net + 1u]++; } else { _rp[_rf[_net] + _rn[_net]++] = _p; }
                }
                if(_dir == FUDE_SIM_OUT || _dir == FUDE_SIM_INOUT) {
                    if(_pass == 0u) { _df[_net + 1u]++; } else { _dp[_df[_net] + _dn[_net]++] = _port; }
                }
            }
        }
    }
    rde_arr_free(&_seen);
    rde_arr_free(&_rfill);
    rde_arr_free(&_dfill);
    rde_memset(_run->drive.memory, FUDE_SIM_Z, _np);
    // (driven nets unknown until they are; the rest released)
    const u32* _df = FSR_U32(_run->drive_first);
    for(u32 _n = 0; _n < _nets; _n++) {
        FSR_U8(_run->net_value)[_n] = _df[_n + 1u] > _df[_n] ? FUDE_SIM_X : FUDE_SIM_Z;
    }
    // Each started (its outputs as it begins), those outputs put on their nets all at once — nothing worked out from
    // them one at a time: a flip-flop reading its set still unknown as its reset arrives would latch X for ever —, then
    // each worked out once from what it reads, then all of it settled.
    for(u32 _p = 0; _p < _prims; _p++) {
        if(_pr[_p].model != NULL && _pr[_p].model->start != NULL) {
            _pr[_p].model->start(_run, _p);
        }
    }
    fsr_start_quiet(_run);
    for(u32 _p = 0; _p < _prims; _p++) {
        if(_pr[_p].model != NULL && _pr[_p].model->logic != NULL) {
            _pr[_p].model->logic(_run, _p);
        }
    }
    fude_sim_run_settle(_run, 1e-3);
}

// See sim.h. A run whose flat is NULL was never started (or was destroyed already), and is left alone.
void fude_sim_run_destroy(fude_sim_run* _run) {
    if(_run->flat == NULL) {
        return;
    }
    rde_arr_free(&_run->net_value);
    rde_arr_free(&_run->drive);
    rde_arr_free(&_run->pending);
    rde_arr_free(&_run->read_first);
    rde_arr_free(&_run->read_prims);
    rde_arr_free(&_run->drive_first);
    rde_arr_free(&_run->drive_ports);
    rde_arr_free(&_run->state);
    rde_arr_free(&_run->events);
    rde_memset(_run, 0, sizeof(*_run));
}

// Fires one event. A wake calls its primitive's wake. A change of drive is dropped when a newer change to the same
// port was scheduled since (pending holds that one's seq): that is what makes the delays inertial. Otherwise the
// port's drive is set, its net worked out again, and only when the net's value changed is each of its readers'
// logic called.
RDE_INTERNAL void fsr_fire(fude_sim_run* _run, const fude_sim_event* _ev) {
    const fude_sim_prim* _pr = (const fude_sim_prim*)_run->flat->prims.memory;
    if(_ev->port == FUDE_SIM_NONE) {
        if(_pr[_ev->prim].model != NULL && _pr[_ev->prim].model->wake != NULL) {
            _pr[_ev->prim].model->wake(_run, _ev->prim);
        }
        return;
    }
    if(_ev->seq != FSR_U64(_run->pending)[_ev->port]) {
        return;   // (a newer change to it since: this one did not last its delay)
    }
    FSR_U8(_run->drive)[_ev->port] = _ev->value;
    const u32 _net = ((const u32*)_run->flat->port_nets.memory)[_ev->port];
    if(_net >= _run->flat->nets) {
        return;
    }
    const u8 _v = fsr_resolve(_run, _net);
    if(_v == FSR_U8(_run->net_value)[_net]) {
        return;
    }
    FSR_U8(_run->net_value)[_net] = _v;
    const u32* _first = FSR_U32(_run->read_first);
    for(u32 _i = _first[_net]; _i < _first[_net + 1u]; _i++) {
        const u32 _p = FSR_U32(_run->read_prims)[_i];
        if(_pr[_p].model != NULL && _pr[_p].model->logic != NULL) {
            _pr[_p].model->logic(_run, _p);
        }
    }
}

// Puts the starting outputs on their nets without calling any reader's logic (fude_sim_run_init's first step): each
// change of drive due now is applied as fsr_fire would apply it, and it stops at the first wake, or at the first event
// due later, which is left for its time.
RDE_INTERNAL void fsr_start_quiet(fude_sim_run* _run) {
    while(rde_arr_length(&_run->events) > 0u) {
        const fude_sim_event* _top = (const fude_sim_event*)_run->events.memory;
        if(_top->port == FUDE_SIM_NONE || _top->time > _run->now) {
            break;
        }
        const fude_sim_event _ev = fsr_pop(_run);
        if(_ev.seq != FSR_U64(_run->pending)[_ev.port]) {
            continue;
        }
        FSR_U8(_run->drive)[_ev.port] = _ev.value;
        const u32 _net = ((const u32*)_run->flat->port_nets.memory)[_ev.port];
        if(_net < _run->flat->nets) {
            FSR_U8(_run->net_value)[_net] = fsr_resolve(_run, _net);
        }
    }
}

// Past settle_limit in one call (a loop of gates with no state that never rests): its changes of drive waiting are
// dropped, which stops the loop where it is, and its wakes are kept, each due before _t moved on to _t (so time never
// goes back), so a clock or a timer of the same circuit goes on ticking. Dropping every event, as it once did, stopped
// every clock in the circuit for good along with the loop.
RDE_INTERNAL void fsr_runaway(fude_sim_run* _run, f64 _t) {
    _run->oscillating = true;
    fude_sim_event* _ev = (fude_sim_event*)_run->events.memory;
    u32 _kept = 0;
    for(u32 _e = 0; _e < (u32)rde_arr_length(&_run->events); _e++) {
        if(_ev[_e].port == FUDE_SIM_NONE) {
            _ev[_e].time = _ev[_e].time < _t ? _t : _ev[_e].time;
            _ev[_kept++] = _ev[_e];
        }
    }
    rde_arr_resize(&_run->events, _kept);
    fsr_heapify(_run);
}

// See sim.h. now ends at _t, or stays where it was if it was later already.
void fude_sim_run_until(fude_sim_run* _run, f64 _t) {
    u32 _fired = 0;
    while(rde_arr_length(&_run->events) > 0u && ((const fude_sim_event*)_run->events.memory)[0].time <= _t) {
        const fude_sim_event _ev = fsr_pop(_run);
        _run->now = _ev.time;
        fsr_fire(_run, &_ev);
        if(++_fired > _run->settle_limit) {
            fsr_runaway(_run, _t);   // (it never rests: the loop stopped, its clocks kept)
            break;
        }
    }
    _run->now = _t > _run->now ? _t : _run->now;
}

// See sim.h. Past settle_limit it only stops, every event left waiting: the next fude_sim_run_until fires them, and
// stops a loop that never rests there (fsr_runaway).
void fude_sim_run_settle(fude_sim_run* _run, f64 _horizon) {
    // (what follows now, as far as _horizon on; a clock's next tick is left for its time)
    u32 _fired = 0;
    const f64 _until = _run->now + _horizon;
    while(rde_arr_length(&_run->events) > 0u && ((const fude_sim_event*)_run->events.memory)[0].time <= _until) {
        const fude_sim_event* _top = (const fude_sim_event*)_run->events.memory;
        if(_top->port == FUDE_SIM_NONE && _top->time > _run->now) {
            break;   // (a wake later on: not part of settling)
        }
        const fude_sim_event _ev = fsr_pop(_run);
        _run->now = _ev.time > _run->now ? _ev.time : _run->now;
        fsr_fire(_run, &_ev);
        if(++_fired > _run->settle_limit) {
            _run->oscillating = true;
            break;
        }
    }
}

// See sim.h.
u8 fude_sim_run_net(const fude_sim_run* _run, u32 _net) {
    return _net < _run->flat->nets ? FSR_U8(_run->net_value)[_net] : FUDE_SIM_Z;
}

// See sim.h. The value is kept as the primitive's first number, where the input model keeps it, and its port 0 is
// driven to it with no delay. Any other primitive is refused: one that keeps no numbers has its `state` index at the
// next primitive's, whose first number would be written over, and its port 0 may be an input.
b8 fude_sim_run_set(fude_sim_run* _run, u32 _prim, u8 _value) {
    if(_prim >= (u32)rde_arr_length(&_run->flat->prims)) {
        return false;
    }
    const fude_sim_model* _m = ((const fude_sim_prim*)_run->flat->prims.memory)[_prim].model;
    if(_m == NULL || rde_strcmp(_m->id, "input") != 0) {
        return false;
    }
    fude_sim_state(_run, _prim)[0] = (f64)_value;
    fude_sim_out(_run, _prim, 0u, _value, 0.0);
    return true;
}

// See sim.h. The same resolution as fsr_resolve, skipping the drivers among _prim's ports; a _prim out of range skips
// nothing.
u8 fude_sim_run_net_without(const fude_sim_run* _run, u32 _net, u32 _prim) {
    if(_net >= _run->flat->nets) {
        return FUDE_SIM_Z;
    }
    const fude_sim_prim* _pr = (const fude_sim_prim*)_run->flat->prims.memory;
    const b8 _skip = _prim < (u32)rde_arr_length(&_run->flat->prims);
    const u32* _first = FSR_U32(_run->drive_first);
    const u32* _ports = FSR_U32(_run->drive_ports);
    const u8*  _drive = FSR_U8(_run->drive);
    u8 _v = FUDE_SIM_Z;
    for(u32 _i = _first[_net]; _i < _first[_net + 1u]; _i++) {
        const u32 _port = _ports[_i];
        if(_skip && _port >= _pr[_prim].first && _port < _pr[_prim].first + _pr[_prim].count) {
            continue;
        }
        const u8 _d = _drive[_port];
        if(_d == FUDE_SIM_Z) {
            continue;
        }
        if(_d == FUDE_SIM_X || (_v != FUDE_SIM_Z && _v != _d)) {
            return FUDE_SIM_X;
        }
        _v = _d;
    }
    return _v;
}

// See sim.h. A primitive's state is copied only when both runs keep the same count of numbers for it (a memory's
// count depends on its parameters). The run goes on from the last one's time, so whoever drives it to a time (the
// canvas, to its circuit's) does not see every edge since 0 fire at once. The events the fresh start scheduled are
// dropped, so the carried-over drives are not undone, except the wakes of a primitive that took no state over, moved
// on to that time (its fresh start was at 0). A primitive that took its state takes the wakes it was waiting for too:
// a clock keeps its phase, where its level carried with the fresh start's first edge would put it half a cycle off
// whenever its duty is not a half.
void fude_sim_run_keep(fude_sim_run* _run, const fude_sim_run* _was) {
    const fude_sim_flat* _f = _run->flat, *_w = _was->flat;
    const fude_sim_prim* _p = (const fude_sim_prim*)_f->prims.memory, *_q = (const fude_sim_prim*)_w->prims.memory;
    const u32 _np = (u32)rde_arr_length(&_f->prims), _nq = (u32)rde_arr_length(&_w->prims);
    f64* _state = (f64*)_run->state.memory;
    const f64* _old = (const f64*)_was->state.memory;
    // Which last primitive each took its state from, and the other way round (FUDE_SIM_NONE: none).
    rde_arr _from_arr = fsr_table(sizeof(u32), _np + 1u), _to_arr = fsr_table(sizeof(u32), _nq + 1u);
    u32* _from = FSR_U32(_from_arr), *_to = FSR_U32(_to_arr);
    for(u32 _i = 0; _i <= _np; _i++) {
        _from[_i] = FUDE_SIM_NONE;
    }
    for(u32 _j = 0; _j <= _nq; _j++) {
        _to[_j] = FUDE_SIM_NONE;
    }
    // Each primitive found in the last by its path (most in the same order: looked for from where the last was): its
    // state, and what it drove — a latch of gates keeps what it holds in its nets, not in any state.
    u32 _at = 0;
    for(u32 _i = 0; _i < _np && _nq > 0u; _i++) {
        for(u32 _k = 0; _k < _nq; _k++) {
            const u32 _j = (_at + _k) % _nq;
            if(_q[_j].model != _p[_i].model || _q[_j].count != _p[_i].count || rde_strcmp(_q[_j].path, _p[_i].path) != 0) {
                continue;
            }
            const u32 _n = fude_sim_model_states(_p[_i].model, _p[_i].params);
            if(fude_sim_model_states(_q[_j].model, _q[_j].params) == _n) {
                if(_n > 0u) {
                    rde_memcpy(&_state[_p[_i].state], &_old[_q[_j].state], (usize)_n * sizeof(f64));
                }
                _from[_i] = _j;
                _to[_j]   = _i;
            }
            rde_memcpy(&FSR_U8(_run->drive)[_p[_i].first], &FSR_U8(_was->drive)[_q[_j].first], _p[_i].count);
            _at = _j + 1u;
            break;
        }
    }
    // Each net as those drive it; then each primitive worked out from them (what is already so changes nothing).
    for(u32 _n = 0; _n < _f->nets; _n++) {
        FSR_U8(_run->net_value)[_n] = fsr_resolve(_run, _n);
    }
    // The events: the changes the fresh start asked for dropped; its wakes kept for a primitive that took no state,
    // moved on to the last run's time; then the last run's wakes of each primitive that took its state, moved over to
    // it, each given a seq after the fresh start's in the order of its old one, so those due together keep their order.
    fude_sim_event* _ev = (fude_sim_event*)_run->events.memory;
    u32 _kept = 0;
    for(u32 _e = 0; _e < (u32)rde_arr_length(&_run->events); _e++) {
        if(_ev[_e].port == FUDE_SIM_NONE && _ev[_e].prim < _np && _from[_ev[_e].prim] == FUDE_SIM_NONE) {
            _ev[_e].time += _was->now;
            _ev[_kept++] = _ev[_e];
        }
    }
    rde_arr_resize(&_run->events, _kept);
    const u64 _seq = _run->seq;
    const fude_sim_event* _old_ev = (const fude_sim_event*)_was->events.memory;
    for(u32 _e = 0; _e < (u32)rde_arr_length(&_was->events); _e++) {
        if(_old_ev[_e].port != FUDE_SIM_NONE || _old_ev[_e].prim >= _nq || _to[_old_ev[_e].prim] == FUDE_SIM_NONE) {
            continue;
        }
        fude_sim_event _wake = _old_ev[_e];
        _wake.prim = _to[_wake.prim];
        _wake.seq  = _seq + _wake.seq;
        rde_arr_add(&_run->events, (any)&_wake);
    }
    _run->seq = _seq + _was->seq;
    _run->now = _was->now;
    rde_arr_free(&_from_arr);
    rde_arr_free(&_to_arr);
    fsr_heapify(_run);
    for(u32 _i = 0; _i < _np; _i++) {
        if(_p[_i].model != NULL && _p[_i].model->logic != NULL) {
            _p[_i].model->logic(_run, _i);
        }
    }
    fude_sim_run_settle(_run, 1e-3);
}

// --- for models -------------------------------------------------------------------------------------

// See sim.h.
u8 fude_sim_in(const fude_sim_run* _run, u32 _prim, u32 _port) {
    const fude_sim_prim* _p = &((const fude_sim_prim*)_run->flat->prims.memory)[_prim];
    if(_port >= _p->count) {
        return FUDE_SIM_Z;
    }
    return fude_sim_run_net(_run, ((const u32*)_run->flat->port_nets.memory)[_p->first + _port]);
}

// See sim.h. The event's seq becomes the port's pending one, which cancels any change still waiting for it. A
// negative delay is taken as 0.
void fude_sim_out(fude_sim_run* _run, u32 _prim, u32 _port, u8 _value, f64 _delay) {
    const fude_sim_prim* _p = &((const fude_sim_prim*)_run->flat->prims.memory)[_prim];
    if(_port >= _p->count) {
        return;
    }
    const fude_sim_event _ev = { _run->now + (_delay > 0.0 ? _delay : 0.0), ++_run->seq, _p->first + _port, _prim, _value };
    FSR_U64(_run->pending)[_ev.port] = _ev.seq;
    fsr_push(_run, _ev);
}

// See sim.h. A wake is an event with port FUDE_SIM_NONE; a negative delay is taken as 0.
void fude_sim_wake(fude_sim_run* _run, u32 _prim, f64 _delay) {
    const fude_sim_event _ev = { _run->now + (_delay > 0.0 ? _delay : 0.0), ++_run->seq, FUDE_SIM_NONE, _prim, 0u };
    fsr_push(_run, _ev);
}

// See sim.h.
f64* fude_sim_state(fude_sim_run* _run, u32 _prim) {
    return &((f64*)_run->state.memory)[((const fude_sim_prim*)_run->flat->prims.memory)[_prim].state];
}

// See sim.h.
f64 fude_sim_arg(const fude_sim_run* _run, u32 _prim, u32 _k) {
    return _k < FUDE_SIM_PARAMS ? ((const fude_sim_prim*)_run->flat->prims.memory)[_prim].params[_k] : 0.0;
}

// See sim.h.
u32 fude_sim_port_count(const fude_sim_run* _run, u32 _prim) {
    return ((const fude_sim_prim*)_run->flat->prims.memory)[_prim].count;
}
