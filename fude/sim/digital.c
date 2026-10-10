// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "sim/sim.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// An array of _n zeroed items of _size bytes.
RDE_INTERNAL rde_arr fsr_table(usize _size, u32 _n) {
    rde_arr _a = rde_arr_new(_size, rde_memory_allocator_get_default_std());
    rde_arr_resize(&_a, _n);
    return _a;
}

#define FSR_U8(_arr)  ((u8*)(_arr).memory)
#define FSR_U32(_arr) ((u32*)(_arr).memory)
#define FSR_U64(_arr) ((u64*)(_arr).memory)

// --- the events: a heap, the soonest first (then the first made) --------------------------------------

RDE_INTERNAL b8 fsr_before(const fude_sim_event* _a, const fude_sim_event* _b) {
    return _a->time < _b->time || (_a->time == _b->time && _a->seq < _b->seq);
}

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

RDE_INTERNAL b8 fsr_logic_port(const fude_sim_port* _p) {
    return _p->domain == FUDE_SIM_LOGIC;
}

RDE_INTERNAL void fsr_start_quiet(fude_sim_run* _run);

void fude_sim_run_init(fude_sim_run* _run, const fude_sim_flat* _flat) {
    memset(_run, 0, sizeof(*_run));
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
    memset(_run->drive.memory, FUDE_SIM_Z, _np);
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
    memset(_run, 0, sizeof(*_run));
}

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

// The starting outputs on their nets, none of their readers worked out (fude_sim_run_init's): each drive at time now
// applied as fsr_fire would, a wake left for its time.
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

void fude_sim_run_until(fude_sim_run* _run, f64 _t) {
    u32 _fired = 0;
    while(rde_arr_length(&_run->events) > 0u && ((const fude_sim_event*)_run->events.memory)[0].time <= _t) {
        const fude_sim_event _ev = fsr_pop(_run);
        _run->now = _ev.time;
        fsr_fire(_run, &_ev);
        if(++_fired > _run->settle_limit) {
            _run->oscillating = true;   // (it never rests: a loop of gates with no state)
            rde_arr_clear(&_run->events);
            break;
        }
    }
    _run->now = _t > _run->now ? _t : _run->now;
}

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

u8 fude_sim_run_net(const fude_sim_run* _run, u32 _net) {
    return _net < _run->flat->nets ? FSR_U8(_run->net_value)[_net] : FUDE_SIM_Z;
}

void fude_sim_run_set(fude_sim_run* _run, u32 _prim, u8 _value) {
    if(_prim >= (u32)rde_arr_length(&_run->flat->prims)) {
        return;
    }
    fude_sim_state(_run, _prim)[0] = (f64)_value;
    fude_sim_out(_run, _prim, 0u, _value, 0.0);
}

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

void fude_sim_run_keep(fude_sim_run* _run, const fude_sim_run* _was) {
    const fude_sim_flat* _f = _run->flat, *_w = _was->flat;
    const fude_sim_prim* _p = (const fude_sim_prim*)_f->prims.memory, *_q = (const fude_sim_prim*)_w->prims.memory;
    const u32 _np = (u32)rde_arr_length(&_f->prims), _nq = (u32)rde_arr_length(&_w->prims);
    f64* _state = (f64*)_run->state.memory;
    const f64* _old = (const f64*)_was->state.memory;
    // Each primitive found in the last by its path (most in the same order: looked for from where the last was): its
    // state, and what it drove — a latch of gates keeps what it holds in its nets, not in any state.
    u32 _at = 0;
    for(u32 _i = 0; _i < _np && _nq > 0u; _i++) {
        for(u32 _k = 0; _k < _nq; _k++) {
            const u32 _j = (_at + _k) % _nq;
            if(_q[_j].model != _p[_i].model || _q[_j].count != _p[_i].count || strcmp(_q[_j].path, _p[_i].path) != 0) {
                continue;
            }
            const u32 _n = fude_sim_model_states(_p[_i].model, _p[_i].params);
            if(_n > 0u && fude_sim_model_states(_q[_j].model, _q[_j].params) == _n) {
                memcpy(&_state[_p[_i].state], &_old[_q[_j].state], (usize)_n * sizeof(f64));
            }
            memcpy(&FSR_U8(_run->drive)[_p[_i].first], &FSR_U8(_was->drive)[_q[_j].first], _p[_i].count);
            _at = _j + 1u;
            break;
        }
    }
    // Each net as those drive it; then each primitive worked out from them (what is already so changes nothing).
    for(u32 _n = 0; _n < _f->nets; _n++) {
        FSR_U8(_run->net_value)[_n] = fsr_resolve(_run, _n);
    }
    // (the changes the fresh start asked for dropped; its wakes kept: a clock goes on ticking)
    fude_sim_event* _ev = (fude_sim_event*)_run->events.memory;
    u32 _kept = 0;
    for(u32 _e = 0; _e < (u32)rde_arr_length(&_run->events); _e++) {
        if(_ev[_e].port == FUDE_SIM_NONE) {
            _ev[_kept++] = _ev[_e];
        }
    }
    rde_arr_resize(&_run->events, _kept);
    for(u32 _e = 1; _e < _kept; _e++) {   // (made a heap again: each sifted up as if pushed in turn)
        for(u32 _c = _e; _c > 0u && fsr_before(&_ev[_c], &_ev[(_c - 1u) / 2u]); _c = (_c - 1u) / 2u) {
            const fude_sim_event _t = _ev[_c];
            _ev[_c] = _ev[(_c - 1u) / 2u];
            _ev[(_c - 1u) / 2u] = _t;
        }
    }
    for(u32 _i = 0; _i < _np; _i++) {
        if(_p[_i].model != NULL && _p[_i].model->logic != NULL) {
            _p[_i].model->logic(_run, _i);
        }
    }
    fude_sim_run_settle(_run, 1e-3);
}

// --- for models -------------------------------------------------------------------------------------

u8 fude_sim_in(const fude_sim_run* _run, u32 _prim, u32 _port) {
    const fude_sim_prim* _p = &((const fude_sim_prim*)_run->flat->prims.memory)[_prim];
    if(_port >= _p->count) {
        return FUDE_SIM_Z;
    }
    return fude_sim_run_net(_run, ((const u32*)_run->flat->port_nets.memory)[_p->first + _port]);
}

void fude_sim_out(fude_sim_run* _run, u32 _prim, u32 _port, u8 _value, f64 _delay) {
    const fude_sim_prim* _p = &((const fude_sim_prim*)_run->flat->prims.memory)[_prim];
    if(_port >= _p->count) {
        return;
    }
    const fude_sim_event _ev = { _run->now + (_delay > 0.0 ? _delay : 0.0), ++_run->seq, _p->first + _port, _prim, _value };
    FSR_U64(_run->pending)[_ev.port] = _ev.seq;
    fsr_push(_run, _ev);
}

void fude_sim_wake(fude_sim_run* _run, u32 _prim, f64 _delay) {
    const fude_sim_event _ev = { _run->now + (_delay > 0.0 ? _delay : 0.0), ++_run->seq, FUDE_SIM_NONE, _prim, 0u };
    fsr_push(_run, _ev);
}

f64* fude_sim_state(fude_sim_run* _run, u32 _prim) {
    return &((f64*)_run->state.memory)[((const fude_sim_prim*)_run->flat->prims.memory)[_prim].state];
}

f64 fude_sim_arg(const fude_sim_run* _run, u32 _prim, u32 _k) {
    return _k < FUDE_SIM_PARAMS ? ((const fude_sim_prim*)_run->flat->prims.memory)[_prim].params[_k] : 0.0;
}

u32 fude_sim_port_count(const fude_sim_run* _run, u32 _prim) {
    return ((const fude_sim_prim*)_run->flat->prims.memory)[_prim].count;
}
