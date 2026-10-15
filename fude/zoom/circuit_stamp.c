// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/circuit_parts.h"

// ===========================================================================
// See circuit.h. Circuits, each part stamped into its block's system for a step (nodal analysis: its unknowns the
// nodes' voltages alone): conductances, currents, sources (each a Norton equivalent, fzc_vsrc: no row of its own for
// its current), and non-linear elements made linear where they are now, as Newton's method takes them.
//
// The file is in three parts: a pin's node and voltage, the system's places and the elements stamped there (fzc_entry,
// fzc_g, fzc_i, fzc_vsrc, fzc_nonlinear); the non-linear devices' currents (a diode's, a transistor's, an op-amp's, a
// comparator's, a transformer's companion: fzc_*_fn, each with its constants), with a logic part's bridges and a
// contact; and each model's elements, in families, a function for each with a case for each model (fzc_stamp_load,
// _source, _semiconductor, _analog, _display, _digital), which fzc_stamp asks in turn.
//
// Part of the circuit: circuit_parts.h holds what the modules share and says which module holds what. Prefix fzc_ and
// FZC_. Main thread only.
// ===========================================================================

// A "pin" that is ground itself (node 0), for a source stamped from a pin to ground (a rail's): no part has this many
// pins. (Not FZC_GROUND, which is the ground symbol's look, FZC_LOOK_.)
#define FZC_GROUND_PIN 0xFFFFu

// --- solved -----------------------------------------------------------------------------------------

// Returns pin _pin's node (FZC_GROUND_PIN: ground, 0; past its pin_room or joined to nothing: FUDE_ZOOM_NONE).
u32 fzc_node(const fude_zoom_circuit_part* _p, u32 _pin) {
    return _pin == FZC_GROUND_PIN ? 0u : (_pin < _p->pin_room ? _p->node[_pin] : FUDE_ZOOM_NONE);
}

// Returns a node's voltage now (ground's, or none's, 0).
RDE_INTERNAL f64 fzc_volt(const fzc_ctx* _x, u32 _node) {
    return _node == FUDE_ZOOM_NONE || _node == 0u ? 0.0 : ((const f64*)_x->c->v.memory)[_node];
}

// Returns a pin's voltage now.
f64 fzc_pv(const fzc_ctx* _x, const fude_zoom_circuit_part* _p, u32 _pin) {
    return fzc_volt(_x, fzc_node(_p, _pin));
}

// Adds _i to the current out of the part at pin _pin (pin_i), when it is one of its pin_room (not ground's own).
RDE_INTERNAL void fzc_add_pin_i(fude_zoom_circuit_part* _p, u32 _pin, f64 _i) {
    if(_pin < _p->pin_room) {
        _p->pin_i[_pin] += _i;
    }
}

// The system's entry at node _row's row, node _col's column (a held node's, ground's: a place no one reads). Both in
// one block, as a part's pins but those that sense are; were they not, the two blocks joined from then on (reblock).
// While the systems are shaped (fzc_shape): each place a stamp writes noted, nothing written. After: a place its
// block's shape has not (a stamp that writes somewhere new — a chip's output turned on) noted too, and the system
// shaped again before it is solved.
f64* fzc_entry(fude_zoom_circuit* _c, u32 _row, u32 _col) {
    static f64 _nowhere;
    const u32* _block = (const u32*)_c->node_block.memory;
    const u32  _b     = _block[_row];
    if(_b == FUDE_ZOOM_NONE || _block[_col] == FUDE_ZOOM_NONE) {
        return &_nowhere;   // (a node a rail holds: known — what it does on the right-hand side, fzc_g)
    }
    if(_block[_col] != _b) {
        // (across two blocks: through a pin that senses, a stamp more than a leak — one block from now on)
        const u64 _pair = _row < _col ? ((u64)_row << 32) | _col : ((u64)_col << 32) | _row;
        rde_arr_add(&_c->couplings, (any)&_pair);
        _c->reblock = true;
        return &_nowhere;
    }
    fude_sim_sparse* _sys   = &((fude_sim_sparse*)_c->block_system.memory)[_b];
    const u32*       _slot  = (const u32*)_c->node_slot.memory;
    const u32        _first = ((const u32*)_c->block_first.memory)[_b];
    const u32 _e = _c->recording ? FUDE_SIM_SPARSE_NONE : fude_sim_sparse_find(_sys, _slot[_row] - _first, _slot[_col] - _first);
    if(_e == FUDE_SIM_SPARSE_NONE) {
        const u64 _join = ((u64)_row << 32) | _col;
        rde_arr_add(&_c->joins, (any)&_join);
        _c->reshape = _c->reshape || !_c->recording;
        return &_nowhere;
    }
    return &((f64*)_sys->val.memory)[_e];
}

// Is node _k known — ground, or a node a rail holds?
RDE_INTERNAL b8 fzc_known(const fude_zoom_circuit* _c, u32 _k) {
    return _k == 0u || ((const u32*)_c->node_held.memory)[_k] != FUDE_ZOOM_NONE;
}

// _value added to node _k's right-hand side (none: ground's, a held node's).
RDE_INTERNAL void fzc_rhs_add(fude_zoom_circuit* _c, u32 _k, f64 _value) {
    if(_k != 0u && ((const u32*)_c->node_block.memory)[_k] != FUDE_ZOOM_NONE) {
        ((f64*)_c->rhs.memory)[((const u32*)_c->node_slot.memory)[_k]] += _value;
    }
}

// A conductance _g between pins _a and _b.
RDE_INTERNAL void fzc_g(fzc_ctx* _x, fude_zoom_circuit_part* _p, u32 _pa, u32 _pb, f64 _g) {
    const u32 _a = fzc_node(_p, _pa), _b = fzc_node(_p, _pb);
    if(_a == FUDE_ZOOM_NONE || _b == FUDE_ZOOM_NONE) {
        return;
    }
    const f64 _i = _g * (fzc_volt(_x, _a) - fzc_volt(_x, _b));
    fzc_add_pin_i(_p, _pa, _i);
    fzc_add_pin_i(_p, _pb, -_i);
    fude_zoom_circuit* _c = _x->c;
    if(_a > 0u) { *fzc_entry(_c, _a, _a) += _g; }
    if(_b > 0u) { *fzc_entry(_c, _b, _b) += _g; }
    if(_a > 0u && _b > 0u) {
        *fzc_entry(_c, _a, _b) -= _g;
        *fzc_entry(_c, _b, _a) -= _g;
        // (a node a rail holds: known, as ground is — what it drives through _g, on the other's side)
        const b8 _ka = fzc_known(_c, _a), _kb = fzc_known(_c, _b);
        if(_kb && !_ka) { fzc_rhs_add(_c, _a, _g * fzc_volt(_x, _b)); }
        if(_ka && !_kb) { fzc_rhs_add(_c, _b, _g * fzc_volt(_x, _a)); }
    }
}

// A current _i from pin _a through the part to pin _b.
RDE_INTERNAL void fzc_i(fzc_ctx* _x, fude_zoom_circuit_part* _p, u32 _pa, u32 _pb, f64 _i) {
    const u32 _a = fzc_node(_p, _pa), _b = fzc_node(_p, _pb);
    if(_a == FUDE_ZOOM_NONE || _b == FUDE_ZOOM_NONE) {
        return;
    }
    fzc_add_pin_i(_p, _pa, _i);
    fzc_add_pin_i(_p, _pb, -_i);
    fzc_rhs_add(_x->c, _a, -_i);
    fzc_rhs_add(_x->c, _b, _i);
}

// A voltage source _v (pin _a over pin _b) behind _r.
RDE_INTERNAL void fzc_vsrc(fzc_ctx* _x, fude_zoom_circuit_part* _p, u32 _pa, u32 _pb, f64 _v, f64 _r) {
    fzc_g(_x, _p, _pa, _pb, 1.0 / _r);
    fzc_i(_x, _p, _pb, _pa, _v / _r);
}

// What is not linear: the currents into the part at pins _pins (_k of them) as _fn says from their voltages, and
// how they change (worked out a little either way), made linear where they are now.
typedef void (*fzc_fn)(const f64* _v, f64* _i, const any _user);

// Stamps a non-linear element on _k pins (8 at most): _fn's currents at the voltages now, and its Jacobian worked out
// by nudging each voltage by a ten-millionth (of it, or of a volt), as Newton's method takes it. A pin joined to
// nothing: nothing stamped.
RDE_INTERNAL void fzc_nonlinear(fzc_ctx* _x, fude_zoom_circuit_part* _p, const u32* _pins, u32 _k, fzc_fn _fn, const any _user) {
    f64 _v[8], _i0[8], _ih[8], _j[8][8];
    u32 _node[8];
    for(u32 _t = 0; _t < _k; _t++) {
        _node[_t] = fzc_node(_p, _pins[_t]);
        if(_node[_t] == FUDE_ZOOM_NONE) {
            return;
        }
        _v[_t] = fzc_volt(_x, _node[_t]);
    }
    _fn(_v, _i0, _user);
    for(u32 _u = 0; _u < _k; _u++) {
        const f64 _h = 1e-7 * rde_math_max_f64(1.0, rde_math_abs_f64(_v[_u]));
        f64 _vh[8];
        rde_memcpy(_vh, _v, sizeof(f64) * _k);
        _vh[_u] += _h;
        _fn(_vh, _ih, _user);
        for(u32 _t = 0; _t < _k; _t++) {
            _j[_t][_u] = (_ih[_t] - _i0[_t]) / _h;
        }
    }
    for(u32 _t = 0; _t < _k; _t++) {
        fzc_add_pin_i(_p, _pins[_t], _i0[_t]);
        if(fzc_known(_x->c, _node[_t])) {
            continue;
        }
        f64 _lin = _i0[_t];
        for(u32 _u = 0; _u < _k; _u++) {
            if(fzc_known(_x->c, _node[_u])) {
                continue;   // (ground, or held: at the volts it was made linear at — nothing of it to move)
            }
            _lin -= _j[_t][_u] * _v[_u];
            *fzc_entry(_x->c, _node[_t], _node[_u]) += _j[_t][_u];
        }
        fzc_rhs_add(_x->c, _node[_t], -_lin);
    }
}

// e^x, carried on straight past e^40 (Newton's steps stay finite).
RDE_INTERNAL f64 fzc_exp(f64 _x) {
    return _x > 40.0 ? rde_math_exp_f64(40.0) * (1.0 + _x - 40.0) : rde_math_exp_f64(_x);
}

// The diode (A, K), as said above; a zener's breakdown the other way when its zener is above 0.
void fzc_diode_fn(const f64* _v, f64* _i, const any _user) {
    const fzc_diode_k* _k = (const fzc_diode_k*)_user;
    const f64 _vd = _v[0] - _v[1];
    // (straightened past a kiloampere, not only past e^40: an LED's exponent counted from its forward voltage would
    // otherwise reach 10^16 A a volt past it — conductances no solve keeps its digits through)
    const f64 _x = (_vd - _k->shift) / _k->nvt, _top = rde_math_min_f64(40.0, rde_math_log_f64(1e3 / _k->is));
    const f64 _e = _x > _top ? rde_math_exp_f64(_top) * (1.0 + _x - _top) : rde_math_exp_f64(_x);
    f64 _id = _k->is * (_e - rde_math_exp_f64(-_k->shift / _k->nvt)) + 1e-12 * _vd;
    if(_k->zener > 0.0) {
        _id -= 1e-14 * (fzc_exp((-_vd - _k->zener) / FZC_VT) - 1.0);   // (its breakdown, the other way)
    }
    _i[0] = _id;
    _i[1] = -_id;
}

// An LED behind a resistor of its own (an LCD's backlight): the current through both, the LED's drop as it is at it
// (halved till it is: what goes through is never more than all the voltage over the resistor).
typedef struct { f64 vf, r; } fzc_led_r_k;

// The LED and its resistor (A, K), as said above: the current found by halving, 60 times at most.
RDE_INTERNAL void fzc_led_r_fn(const f64* _v, f64* _i, const any _user) {
    const fzc_led_r_k* _k = (const fzc_led_r_k*)_user;
    const fzc_diode_k _d = { 0.01, 2.0 * FZC_VT, 0.0, _k->vf };
    const f64 _vd = _v[0] - _v[1];
    f64 _lo = 0.0, _hi = rde_math_max_f64(_vd / _k->r, 0.0), _at[2] = { 0.0, 0.0 }, _led[2];
    for(u32 _n = 0; _n < 60u && _hi > 0.0; _n++) {
        const f64 _mid = 0.5 * (_lo + _hi);
        _at[0] = _vd - _mid * _k->r;
        fzc_diode_fn(_at, _led, &_d);
        if(_led[0] > _mid) {
            _lo = _mid;
        } else {
            _hi = _mid;
        }
    }
    const f64 _id = 0.5 * (_lo + _hi) + 1e-12 * _vd;
    _i[0] = _id;
    _i[1] = -_id;
}

// A slotted sensor or an optocoupler (A, K, C, E): its infrared LED's current (10 mA at 1.2 V), and — its light getting
// through — its phototransistor's, its CTR times it while C is over E (saturated: less, as V_CE nears 0), a dark
// current's leak else.
typedef struct { b8 clear; f64 ctr; } fzc_light_k;

// The sensor's LED and phototransistor (A, K, C, E), as said above.
RDE_INTERNAL void fzc_slot_fn(const f64* _v, f64* _i, const any _user) {
    const fzc_light_k* _k = (const fzc_light_k*)_user;
    const fzc_diode_k _d = { 0.01, 2.0 * FZC_VT, 0.0, 1.2 };
    f64 _led[2];
    fzc_diode_fn(_v, _led, &_d);
    const f64 _vce = _v[2] - _v[3], _il = _k->clear ? _k->ctr * rde_math_max_f64(_led[0], 0.0) : 0.0;
    // (straight the wrong way round, as steep as at 0: a solve from far off not thrown from one flat side to the other)
    const f64 _ic  = (_vce >= 0.0 ? _il * rde_math_tanh_f64(_vce / 0.15) : _il * _vce / 0.15) + 1e-9 * _vce;
    _i[0] = _led[0];
    _i[1] = _led[1];
    _i[2] = _ic;
    _i[3] = -_ic;
}

// An output that sinks a current (an LM3914's: *_user amperes), less as its pin nears what it sinks into.
RDE_INTERNAL void fzc_sink_fn(const f64* _v, f64* _i, const any _user) {
    const f64 _id = *(const f64*)_user * rde_math_tanh_f64(rde_math_max_f64(_v[0] - _v[1], 0.0) / 0.15) + 1e-9 * (_v[0] - _v[1]);
    _i[0] = _id;
    _i[1] = -_id;
}

// An LM3914's LED _k's pin (LED1 its pin 1; LED2–LED10 its pins 18 down to 10).
RDE_INTERNAL u32 fzc_3914_led(u32 _k) {
    return _k == 0u ? 0u : 18u - _k;
}

// Breaking down past _bv volts (an opening contact's arc, a transistor's avalanche): as much current as the voltage is
// past it — 1 S a volt: steep, and straight (not exponential), so a solve from far below gets there in its 2 V steps
// and stays. Its way: _v's.
RDE_INTERNAL f64 fzc_breakdown(f64 _v, f64 _bv) {
    return _v > _bv ? _v - _bv : (_v < -_bv ? _v + _bv : 0.0);
}

// A transistor's way (an NPN's or an N-channel's 1, else -1), and the voltage over it it breaks down at (its collector
// over its emitter, its drain over its source): a coil's current stopped with no diode for it goes through it there.
typedef struct { f64 s, bv; } fzc_transistor_k;

// A transistor's breakdown: a little past its voltage limit (its own, a real part's, a typical one's); none known,
// 60 V.
RDE_INTERNAL f64 fzc_transistor_bv(const fude_zoom_circuit_part* _p, f64 _past) {
    const f64 _most = _p->limits.most[FUDE_ZOOM_LIMIT_VOLTAGE];
    return _most > 0.0 ? _most * _past : 60.0;
}

// A bipolar transistor (B, C, E): Ebers–Moll's transport model (Is 1e-14 A, beta forward 100, reverse 1), with its
// avalanche past its breakdown, collector to emitter.
RDE_INTERNAL void fzc_bjt_fn(const f64* _v, f64* _i, const any _user) {
    // Pins B, C, E; an NPN's (a PNP's: everything the other way, its user says -1).
    const fzc_transistor_k* _t = (const fzc_transistor_k*)_user;
    const f64 _s = _t->s;
    const f64 _is = 1e-14, _bf = 100.0, _br = 1.0;
    const f64 _vbe = _s * (_v[0] - _v[2]), _vbc = _s * (_v[0] - _v[1]);
    const f64 _if = _is * (fzc_exp(_vbe / FZC_VT) - 1.0), _ir = _is * (fzc_exp(_vbc / FZC_VT) - 1.0);
    const f64 _vce = _s * (_v[1] - _v[2]);
    const f64 _ic = _if - _ir * (1.0 + _br) / _br + (_vce > _t->bv ? _vce - _t->bv : 0.0), _ib = _if / _bf + _ir / _br;
    _i[0] = _s * _ib + 1e-12 * (_v[0] - _v[2]);
    _i[1] = _s * _ic;
    _i[2] = -_s * (_ib + _ic) - 1e-12 * (_v[0] - _v[2]);
}

// A MOSFET (G, D, S): the square law (threshold 2 V, k 1 A/V², lambda 0.01), either way through, its gate drawing
// nothing, with its avalanche past its breakdown, drain to source.
RDE_INTERNAL void fzc_mos_fn(const f64* _v, f64* _i, const any _user) {
    // Pins G, D, S; an N-channel's (a P's: the other way).
    const fzc_transistor_k* _t = (const fzc_transistor_k*)_user;
    const f64 _s = _t->s;
    const f64 _vdss = _s * (_v[1] - _v[2]);
    const f64 _vth = 2.0, _k = 1.0, _lambda = 0.01;
    f64 _vgs = _s * (_v[0] - _v[2]), _vds = _s * (_v[1] - _v[2]);
    f64 _sign = 1.0;
    if(_vds < 0.0) {   // (the other way through: its drain and source as each other)
        _vgs = _s * (_v[0] - _v[1]);
        _vds = -_vds;
        _sign = -1.0;
    }
    const f64 _ov = _vgs - _vth;
    f64 _id = 0.0;
    if(_ov > 0.0) {
        _id = _vds < _ov ? _k * (_ov * _vds - 0.5 * _vds * _vds) * (1.0 + _lambda * _vds) : 0.5 * _k * _ov * _ov * (1.0 + _lambda * _vds);
    }
    _id = _s * (_sign * _id + (_vdss > _t->bv ? _vdss - _t->bv : 0.0)) + 1e-9 * (_v[1] - _v[2]);   // (its avalanche, drain to source)
    _i[0] = 0.0;
    _i[1] = _id;
    _i[2] = -_id;
}

// An op-amp's rails (volts): its positive and its negative supply.
typedef struct { f64 vp, vn; } fzc_opamp_k;

// An op-amp (IN−, IN+, OUT), as its first line says.
RDE_INTERNAL void fzc_opamp_fn(const f64* _v, f64* _i, const any _user) {
    // Pins IN−, IN+, OUT: its output a source behind 1 Ω, as high as the difference × 10⁵, between its rails.
    const fzc_opamp_k* _k = (const fzc_opamp_k*)_user;
    const f64 _mid = 0.5 * (_k->vp + _k->vn), _half = rde_math_max_f64(0.5 * (_k->vp - _k->vn) - 0.05, 0.01);
    const f64 _want = _mid + _half * rde_math_tanh_f64(1e5 * (_v[1] - _v[0]) / _half);
    const f64 _io = (_v[2] - _want) / 1.0;
    _i[0] = 1e-12 * _v[0];
    _i[1] = 1e-12 * _v[1];
    _i[2] = _io;
}

// A pin's voltage over another's.
f64 fzc_over(const fzc_ctx* _x, const fude_zoom_circuit_part* _p, u32 _a, u32 _b) {
    return fzc_pv(_x, _p, _a) - fzc_pv(_x, _p, _b);
}

// A pin's node joined to nothing else (its supply left open: the op-amp's own rails then).
RDE_INTERNAL b8 fzc_open(const fude_zoom_circuit_part* _p, u32 _pin) {
    return fzc_node(_p, _pin) == FUDE_ZOOM_NONE;
}

// A board's supply pins: its volts for a pin's name (0: not one); IOREF gives the board's logic volts, _logic.
f64 fzc_supply(const c8* _name, f64 _logic) {
    if(fzc_same_name(_name, "IOREF")) {
        return _logic > 0.0 ? _logic : 5.0;   // (its logic's volts, as the board's own pins are driven to: an Uno's 5 V)
    }
    if(fzc_same_name(_name, "5V") || fzc_same_name(_name, "VBUS") || fzc_same_name(_name, "VSYS")) {
        return 5.0;
    }
    if(fzc_same_name(_name, "3V3") || fzc_same_name(_name, "3.3V")) {
        return 3.3;
    }
    return 0.0;
}

// Returns the index of _part's pin named _name (in any case), or FUDE_ZOOM_NONE.
RDE_INTERNAL u32 fzc_pin_named(const fude_zoom_part* _part, const c8* _name) {
    for(u32 _k = 0; _k < _part->pin_count; _k++) {
        if(fzc_same_name(_part->pins[_k].name, _name)) {
            return _k;
        }
    }
    return FUDE_ZOOM_NONE;
}

// A logic part (logic.h): where its pins meet the analog circuit, its bridges — an output a source of its level
// (its supply's high over its ground, through its output's resistance; none while it is released, unknown, or
// unpowered), an input a little leak to its ground; its supply's own draw.
RDE_INTERNAL void fzc_logic_stamp(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const fude_zoom_logic* _l = _x->c->logic;
    const u32 _index = (u32)(_p - (fude_zoom_circuit_part*)_x->c->parts.memory);
    u32 _first;
    const u32 _n = fude_zoom_logic_bridges_of(_l, _index, &_first);
    const fude_zoom_bridge* _b = &((const fude_zoom_bridge*)_l->bridges.memory)[_first];
    u32 _supply = FUDE_ZOOM_NONE, _ground = FUDE_ZOOM_NONE;
    for(u32 _k = 0; _k < _n; _k++) {
        const u32 _gp = _b[_k].ground != FUDE_ZOOM_NONE ? _b[_k].ground : FZC_GROUND_PIN;
        const f64 _high = _b[_k].supply != FUDE_ZOOM_NONE ? fzc_over(_x, _p, _b[_k].supply, _gp) : FZC_LOGIC_V;
        if(_b[_k].way != FUDE_ZOOM_BRIDGE_IN && (_b[_k].drive == FUDE_SIM_0 || _b[_k].drive == FUDE_SIM_1) && _high > 1.0) {
            fzc_vsrc(_x, _p, _b[_k].pin, _gp, _b[_k].drive == FUDE_SIM_1 ? _high : 0.0, FZC_LOGIC_R);
        }
        if(_b[_k].way != FUDE_ZOOM_BRIDGE_OUT) {
            fzc_g(_x, _p, _b[_k].pin, _gp, _b[_k].supply != FUDE_ZOOM_NONE ? 1e-9 : 1e-6);
        }
        _supply = _b[_k].supply;
        _ground = _b[_k].ground;
    }
    if(_p->part->model == FUDE_ZOOM_MODEL_SIM && _n == 0u) {
        // (no bridge to say them: its own)
        for(u32 _k = 0; _k < _p->part->pin_count; _k++) {
            const c8* _name = _p->part->pins[_k].name;
            if(_name != NULL && rde_strcmp(_name, "VCC") == 0) { _supply = _k; }
            if(_name != NULL && rde_strcmp(_name, "GND") == 0) { _ground = _k; }
        }
    }
    if(_supply != FUDE_ZOOM_NONE && _ground != FUDE_ZOOM_NONE) {
        fzc_g(_x, _p, _supply, _ground, 1e-5);   // (what a chip draws)
    }
}

// A clock's part of each period high (its pulse over its period; none given: half).
f64 fzc_duty(const fude_zoom_circuit_part* _p) {
    return _p->value[2] > 0.0 && _p->value[0] > 0.0 ? rde_math_min_f64(rde_math_max_f64(_p->value[2] * _p->value[0], 0.0), 1.0) : 0.5;
}

// Where in its period a clock is at _t (0 ..1): a whisker more, so that a step landing on an edge (its time a sum of
// steps, a rounding short of it) is past it — as the clock's level and its next edge both take it.
f64 fzc_clock_phase(const fude_zoom_circuit_part* _p, f64 _t) {
    return rde_math_fmod_f64(_t * _p->value[0] + 1e-9, 1.0);
}

// A comparator's output (OUT, IN−, IN+, GND): sinking to GND while IN− is over IN+ (16 mA at most, less as OUT nears
// GND; straight the wrong way round: a solve from far off not thrown between two flat sides), its inputs a leak.
// Whether a comparator's supply is enough for it to work.
typedef struct { b8 powered; } fzc_comparator_k;

// A comparator's output and inputs (OUT, IN−, IN+, GND), as said above.
RDE_INTERNAL void fzc_comparator_fn(const f64* _v, f64* _i, const any _user) {
    const fzc_comparator_k* _k = (const fzc_comparator_k*)_user;
    const f64 _on = _k->powered ? 0.5 * (1.0 + rde_math_tanh_f64((_v[1] - _v[2]) * 2000.0)) : 0.0, _vo = _v[0] - _v[3];
    const f64 _io = 0.016 * _on * (_vo >= 0.0 ? rde_math_tanh_f64(_vo / 0.15) : _vo / 0.15) + 1e-9 * _vo;
    _i[0] = _io;
    _i[1] = 1e-12 * (_v[1] - _v[3]);
    _i[2] = 1e-12 * (_v[2] - _v[3]);
    _i[3] = -_io - _i[1] - _i[2];
}

// An op-amp as a follower (IN+, OUT): its output its IN+, between its rails (50 mV in from each), behind 1 Ω.
RDE_INTERNAL void fzc_follower_fn(const f64* _v, f64* _i, const any _user) {
    const fzc_opamp_k* _k = (const fzc_opamp_k*)_user;
    const f64 _want = rde_math_min_f64(rde_math_max_f64(_v[0], _k->vn + 0.05), _k->vp - 0.05);
    _i[0] = 1e-12 * _v[0];
    _i[1] = (_v[1] - _want) / 1.0;
}

// A thyristor off: a leak, breaking over past *_user volts.
RDE_INTERNAL void fzc_leak_fn(const f64* _v, f64* _i, const any _user) {
    const f64 _d = _v[0] - _v[1], _x = 1e-9 * _d + fzc_breakdown(_d, *(const f64*)_user);
    _i[0] = _x;
    _i[1] = -_x;
}

// A transformer's windings (P1, P2, S1, S2): their currents (into P1 and out of P2; into S1, out of S2) as its
// companion says.
// A transformer's companion: G (row by row) and h, so that [i1; i2] = G·[v1; v2] + h.
typedef struct { f64 g[4], h[2]; } fzc_xfmr_k;

// A transformer's windings' currents, as said above.
RDE_INTERNAL void fzc_xfmr_fn(const f64* _v, f64* _i, const any _user) {
    const fzc_xfmr_k* _k = (const fzc_xfmr_k*)_user;
    const f64 _v1 = _v[0] - _v[1], _v2 = _v[2] - _v[3];
    const f64 _i1 = _k->g[0] * _v1 + _k->g[1] * _v2 + _k->h[0], _i2 = _k->g[2] * _v1 + _k->g[3] * _v2 + _k->h[1];
    _i[0] = _i1;
    _i[1] = -_i1;
    _i[2] = _i2;
    _i[3] = -_i2;
}

// A phototransistor (C, E): its light's current (*_user), less as C nears E — straight the wrong way round.
RDE_INTERNAL void fzc_phototransistor_fn(const f64* _v, f64* _i, const any _user) {
    const f64 _il = *(const f64*)_user, _vce = _v[0] - _v[1];
    const f64 _ic = _il * (_vce >= 0.0 ? rde_math_tanh_f64(_vce / 0.15) : _vce / 0.15) + 1e-9 * _vce;
    _i[0] = _ic;
    _i[1] = -_ic;
}

// An open contact's leak, breaking down past FZC_ARC.
RDE_INTERNAL void fzc_arc_fn(const f64* _v, f64* _i, const any _user) {
    RDE_UNUSED(_user);
    const f64 _d = _v[0] - _v[1], _arc = 1e-9 * _d + fzc_breakdown(_d, FZC_ARC);
    _i[0] = _arc;
    _i[1] = -_arc;
}

// Stamps the contact itself: _g when closed, an arc's leak when open.
RDE_INTERNAL void fzc_contact(fzc_ctx* _x, fude_zoom_circuit_part* _p, u32 _a, u32 _b, b8 _closed, f64 _g) {
    if(_closed) {
        fzc_g(_x, _p, _a, _b, _g);
        return;
    }
    const u32 _pins[2] = { _a, _b };
    fzc_nonlinear(_x, _p, _pins, 2u, fzc_arc_fn, NULL);
}

// Are both of a two-pin part's ends known (ground, held)? Then what is across it is set, whatever it does.
b8 fzc_pinned(const fude_zoom_circuit* _c, const fude_zoom_circuit_part* _p) {
    const u32 _a = fzc_node(_p, 0), _b = fzc_node(_p, 1);
    return _a != FUDE_ZOOM_NONE && _b != FUDE_ZOOM_NONE && fzc_known(_c, _a) && fzc_known(_c, _b);
}

// Stamps a part that conducts or stores, as its value says: a resistor, a pot, a capacitor and an inductor (their
// companions for the step), an LDR, a thermistor, a lamp, a solenoid's coil, a stepper's coils, a transformer's
// windings, a speaker, a motor (behind its back-EMF when it turns a mechanism), a buzzer, a servo's draw, a fuse, a
// voltmeter, an ammeter. False: its model is none of them.
RDE_INTERNAL b8 fzc_stamp_load(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const fude_zoom_part* _part = _p->part;
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_RESISTOR:
        fzc_g(_x, _p, 0, 1, 1.0 / _p->value[0]);
        break;
    case FUDE_ZOOM_MODEL_POT: {
        const f64 _f = rde_math_min_f64(rde_math_max_f64(_p->state[0], 0.0), 1.0);
        fzc_g(_x, _p, 0, 2, 1.0 / rde_math_max_f64(_p->value[0] * _f, 1.0));
        fzc_g(_x, _p, 2, 1, 1.0 / rde_math_max_f64(_p->value[0] * (1.0 - _f), 1.0));
        break;
    }
    case FUDE_ZOOM_MODEL_CAPACITOR:
        // Its companion for the step (state[0] its voltage, state[1] its current, as last stepped): the trapezoidal
        // rule's — 2C/h, and the last current carried — or backward Euler's (C/h). Across two known nodes (a rail's and
        // ground: decoupling) backward Euler's always — its voltage is theirs, its current a spike as it changes; the
        // trapezoidal rule would carry that spike on, turned round each step, for ever.
        if(_x->tran) {
            const b8  _be = _x->be || fzc_pinned(_x->c, _p);
            const f64 _g  = (_be ? 1.0 : 2.0) * _p->value[0] / _x->dt;
            fzc_g(_x, _p, 0, 1, _g);
            fzc_i(_x, _p, 0, 1, -_g * _p->state[0] - (_be ? 0.0 : _p->state[1]));
        }
        break;
    case FUDE_ZOOM_MODEL_INDUCTOR:
        // (state[0] its current, state[1] its voltage: the trapezoidal rule's h/2L and the last voltage carried, or
        // h/L)
        if(_x->tran) {
            const f64 _g = _x->dt / ((_x->be ? 1.0 : 2.0) * _p->value[0]);
            fzc_g(_x, _p, 0, 1, _g);
            fzc_i(_x, _p, 0, 1, _p->state[0] + (_x->be ? 0.0 : _g * _p->state[1]));
        } else {
            fzc_g(_x, _p, 0, 1, 1e3);
        }
        break;
    case FUDE_ZOOM_MODEL_LDR:        fzc_g(_x, _p, 0, 1, 1.0 / fude_zoom_circuit_ldr_ohms(_p->state[0])); break;
    case FUDE_ZOOM_MODEL_THERMISTOR: fzc_g(_x, _p, 0, 1, 1.0 / fude_zoom_circuit_ntc_ohms(_p->value[0], _p->value[1], _p->state[0])); break;
    case FUDE_ZOOM_MODEL_LAMP:     fzc_g(_x, _p, 0, 1, _p->value[1] / (_p->value[0] * _p->value[0])); break;
    case FUDE_ZOOM_MODEL_SOLENOID:
        // Its coil, R and L in series (state[0] its current, state[1] its voltage): the trapezoidal rule's companion —
        // 1 / (R + 2L/h), and what the last step carries, (v + (2L/h − R)·i) of it — or backward Euler's (L/h); DC: R.
        if(_x->tran) {
            const f64 _r = _p->value[1], _l = (_x->be ? 1.0 : 2.0) * _p->value[2] / _x->dt, _g = 1.0 / (_r + _l);
            fzc_g(_x, _p, 0, 1, _g);
            fzc_i(_x, _p, 0, 1, _x->be ? _g * _l * _p->state[0] : _g * (_p->state[1] + (_l - _r) * _p->state[0]));
        } else {
            fzc_g(_x, _p, 0, 1, 1.0 / _p->value[1]);
        }
        break;
    case FUDE_ZOOM_MODEL_STEPPER:
        for(u32 _k = 0; _k < 4u; _k++) {
            fzc_g(_x, _p, 4, _k, 1.0 / FUDE_ZOOM_STEPPER_COIL);   // (COM to each coil's end)
        }
        break;
    case FUDE_ZOOM_MODEL_TRANSFORMER: {
        // Its windings coupled: [i1; i2] = G·[v1; v2] + h (its companion: R and the inductance matrix L — L1, its
        // secondary's L1/n², 0.995 coupled — the trapezoidal rule's, (R + 2L/h)⁻¹, or backward Euler's (R + L/h)⁻¹;
        // DC: R alone).
        const f64 _n = rde_math_max_f64(_p->value[0], 1e-6), _l1 = _p->value[1], _l2 = _l1 / (_n * _n), _mm = 0.995 * rde_math_sqrt_f64(_l1 * _l2);
        const f64 _r1 = _p->value[2], _r2 = _r1 / (_n * _n);
        fzc_xfmr_k _k;
        rde_memset(&_k, 0, sizeof(_k));
        if(_x->tran) {
            const f64 _f = (_x->be ? 1.0 : 2.0) / _x->dt;
            const f64 _a11 = _r1 + _f * _l1, _a12 = _f * _mm, _a22 = _r2 + _f * _l2, _det = _a11 * _a22 - _a12 * _a12;
            _k.g[0] = _a22 / _det; _k.g[1] = -_a12 / _det; _k.g[2] = -_a12 / _det; _k.g[3] = _a11 / _det;
            const f64 _i1 = _p->state[0], _i2 = _p->state[1];
            f64 _w1 = _f * (_l1 * _i1 + _mm * _i2), _w2 = _f * (_mm * _i1 + _l2 * _i2);   // (f·L·i)
            if(!_x->be) {
                _w1 += _p->state[2] - _r1 * _i1;
                _w2 += _p->state[3] - _r2 * _i2;
            }
            _k.h[0] = _k.g[0] * _w1 + _k.g[1] * _w2;
            _k.h[1] = _k.g[2] * _w1 + _k.g[3] * _w2;
        } else {
            _k.g[0] = 1.0 / _r1;
            _k.g[3] = 1.0 / _r2;
        }
        const u32 _pins[4] = { 0u, 1u, 2u, 3u };
        fzc_nonlinear(_x, _p, _pins, 4u, fzc_xfmr_fn, &_k);
        break;
    }
    case FUDE_ZOOM_MODEL_SPEAKER:  fzc_g(_x, _p, 0, 1, 1.0 / rde_math_max_f64(_p->value[0], 1e-3)); break;
    case FUDE_ZOOM_MODEL_MOTOR:
        // (its shaft turning something of a mechanism: its winding behind its back-EMF; else a load, as ever)
        if(_p->shafted) {
            fzc_vsrc(_x, _p, 0, 1, fude_zoom_motor_k(_p) * _p->spin, FUDE_ZOOM_MOTOR_R);
        } else {
            fzc_g(_x, _p, 0, 1, 1.0 / FUDE_ZOOM_MOTOR_R);
        }
        break;
    case FUDE_ZOOM_MODEL_BUZZER:   fzc_g(_x, _p, 0, 1, 1.0 / 100.0); break;
    case FUDE_ZOOM_MODEL_SERVO:
        // GND, VCC, SIG: its signal an input (1 MΩ); what it draws from its supply as it last stepped (state[6]).
        fzc_g(_x, _p, 2, 0, 1e-6);
        fzc_g(_x, _p, 1, 0, 1e-6);
        fzc_i(_x, _p, 1, 0, _p->state[6]);
        break;
    case FUDE_ZOOM_MODEL_FUSE:     fzc_g(_x, _p, 0, 1, _p->state[0] > 0.5 ? 1e-9 : 20.0); break;
    case FUDE_ZOOM_MODEL_VOLTMETER: fzc_g(_x, _p, 0, 1, 1e-7); break;
    case FUDE_ZOOM_MODEL_AMMETER:  fzc_g(_x, _p, 0, 1, 1e3); break;
    default:
        return false;
    }
    return true;
}

// Stamps a source or a contact: a DC, AC or current source, a battery, a clock, a rail; a switch, a push button, a
// changeover switch, an encoder's two contacts. False: its model is none of them.
RDE_INTERNAL b8 fzc_stamp_source(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const fude_zoom_part* _part = _p->part;
    const f64 _t = _x->t;
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_VSOURCE:  fzc_vsrc(_x, _p, 0, 1, _p->value[0], 1e-3); break;
    case FUDE_ZOOM_MODEL_BATTERY:  fzc_vsrc(_x, _p, 0, 1, _p->value[0], 0.5); break;
    case FUDE_ZOOM_MODEL_ACSOURCE: fzc_vsrc(_x, _p, 0, 1, _p->value[0] * rde_math_sin_f64(2.0 * RDE_PI * _p->value[1] * _t), 1e-3); break;
    case FUDE_ZOOM_MODEL_CLOCK:    fzc_vsrc(_x, _p, 0, 1, fzc_clock_phase(_p, _t) < fzc_duty(_p) ? _p->value[1] : 0.0, 1e-3); break;
    case FUDE_ZOOM_MODEL_ISOURCE:  fzc_i(_x, _p, 1, 0, _p->value[0]); break;
    case FUDE_ZOOM_MODEL_RAIL:     fzc_vsrc(_x, _p, 0, FZC_GROUND_PIN, _p->value[0], 1e-3); break;
    case FUDE_ZOOM_MODEL_SWITCH:
    case FUDE_ZOOM_MODEL_BUTTON:   fzc_contact(_x, _p, 0, 1, (_p->switch_on & 1u) || _p->pushed, 100.0); break;
    case FUDE_ZOOM_MODEL_SPDT:
    case FUDE_ZOOM_MODEL_DPDT: {
        // (its common to A, or — on — to B; a DPDT's second pole the same)
        const b8 _b = (_p->switch_on & 1u) != 0u;
        for(u32 _k = 0; _k < (_part->model == FUDE_ZOOM_MODEL_DPDT ? 2u : 1u); _k++) {
            fzc_contact(_x, _p, 3u * _k, 3u * _k + 1u, !_b, 100.0);
            fzc_contact(_x, _p, 3u * _k, 3u * _k + 2u, _b, 100.0);
        }
        break;
    }
    case FUDE_ZOOM_MODEL_ENCODER: {
        b8 _a, _b;
        fude_zoom_circuit_encoder_contacts(_p->state[0], &_a, &_b);
        fzc_contact(_x, _p, 0, 1, _a, 100.0);
        fzc_contact(_x, _p, 2, 1, _b, 100.0);
        break;
    }
    default:
        return false;
    }
    return true;
}

// Stamps a semiconductor, each a non-linear element: a diode, a zener, an LED, a transistor (bipolar or MOSFET), a
// slotted sensor or an optocoupler, a thyristor, a bridge rectifier, a photodiode, a phototransistor. False: its
// model is none of them.
RDE_INTERNAL b8 fzc_stamp_semiconductor(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const fude_zoom_part* _part = _p->part;
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_DIODE:
    case FUDE_ZOOM_MODEL_ZENER:
    case FUDE_ZOOM_MODEL_LED: {
        fzc_diode_k _k = { 1e-14, FZC_VT, 0.0, 0.0 };
        if(_part->model == FUDE_ZOOM_MODEL_LED) {
            _k.nvt   = 2.0 * FZC_VT;
            _k.is    = 0.01;   // (10 mA at its forward voltage)
            _k.shift = _p->value[0];
        } else if(_part->model == FUDE_ZOOM_MODEL_ZENER) {
            _k.zener = _p->value[0];
        }
        const u32 _pins[2] = { 0u, 1u };
        fzc_nonlinear(_x, _p, _pins, 2u, fzc_diode_fn, &_k);
        break;
    }
    case FUDE_ZOOM_MODEL_NPN:
    case FUDE_ZOOM_MODEL_PNP: {
        // (avalanche a quarter past its V_CEO)
        const fzc_transistor_k _k = { _part->model == FUDE_ZOOM_MODEL_NPN ? 1.0 : -1.0, fzc_transistor_bv(_p, 1.25) };
        const u32 _pins[3] = { 0u, 1u, 2u };
        fzc_nonlinear(_x, _p, _pins, 3u, fzc_bjt_fn, &_k);
        break;
    }
    case FUDE_ZOOM_MODEL_NMOS:
    case FUDE_ZOOM_MODEL_PMOS: {
        // (avalanche a tenth past its V_DSS)
        const fzc_transistor_k _k = { _part->model == FUDE_ZOOM_MODEL_NMOS ? 1.0 : -1.0, fzc_transistor_bv(_p, 1.1) };
        const u32 _pins[3] = { 0u, 1u, 2u };
        fzc_nonlinear(_x, _p, _pins, 3u, fzc_mos_fn, &_k);
        break;
    }
    case FUDE_ZOOM_MODEL_SLOT:
    case FUDE_ZOOM_MODEL_OPTO: {
        const fzc_light_k _k = { _part->model == FUDE_ZOOM_MODEL_OPTO || _p->state[0] < 0.5, _part->model == FUDE_ZOOM_MODEL_OPTO ? _p->value[0] : FUDE_ZOOM_SLOT_CTR };
        const u32 _pins[4] = { 0u, 1u, 2u, 3u };
        fzc_nonlinear(_x, _p, _pins, 4u, fzc_slot_fn, &_k);
        break;
    }
    case FUDE_ZOOM_MODEL_SCR:
    case FUDE_ZOOM_MODEL_TRIAC: {
        // Its gate a junction to its cathode (MT1: either way, a TRIAC's); on, a diode from anode to cathode (either
        // way); off, a leak breaking over past its voltage limit.
        const b8 _tri = _part->model == FUDE_ZOOM_MODEL_TRIAC;
        const fzc_diode_k _gk = { 1e-14, FZC_VT, 0.0, 0.0 }, _on = { 1e-10, 2.0 * FZC_VT, 0.0, 0.0 };
        const u32 _gate[2] = { 2u, 1u }, _back[2] = { 1u, 2u }, _ak[2] = { 0u, 1u }, _ka[2] = { 1u, 0u };
        fzc_nonlinear(_x, _p, _gate, 2u, fzc_diode_fn, &_gk);
        if(_tri) {
            fzc_nonlinear(_x, _p, _back, 2u, fzc_diode_fn, &_gk);
        }
        if(_p->state[0] > 0.5) {
            fzc_nonlinear(_x, _p, _ak, 2u, fzc_diode_fn, &_on);
            if(_tri) {
                fzc_nonlinear(_x, _p, _ka, 2u, fzc_diode_fn, &_on);
            }
        } else {
            const f64 _bv = fzc_transistor_bv(_p, 1.1);
            fzc_nonlinear(_x, _p, _ak, 2u, fzc_leak_fn, &_bv);
        }
        break;
    }
    case FUDE_ZOOM_MODEL_BRIDGE: {
        // (AC1, +, AC2, −: each AC pin into +, − into each; a real one's leak — 1 MΩ from each AC pin to − — so that
        // what feeds it, floating while all four are off at a crossing, has somewhere to be)
        const fzc_diode_k _d = { 1e-14, FZC_VT, 0.0, 0.0 };
        const u32 _dd[4][2] = { { 0u, 1u }, { 2u, 1u }, { 3u, 0u }, { 3u, 2u } };
        for(u32 _k = 0; _k < 4u; _k++) {
            fzc_nonlinear(_x, _p, _dd[_k], 2u, fzc_diode_fn, &_d);
        }
        fzc_g(_x, _p, 0, 3, 1e-6);
        fzc_g(_x, _p, 2, 3, 1e-6);
        break;
    }
    case FUDE_ZOOM_MODEL_PHOTODIODE: {
        const fzc_diode_k _d = { 1e-14, FZC_VT, 0.0, 0.0 };
        const u32 _pins[2] = { 0u, 1u };
        fzc_nonlinear(_x, _p, _pins, 2u, fzc_diode_fn, &_d);
        fzc_i(_x, _p, 1, 0, FUDE_ZOOM_PHOTODIODE_A_LUX * rde_math_max_f64(_p->state[0], 0.0));   // (its light's current, against it)
        break;
    }
    case FUDE_ZOOM_MODEL_PHOTOTRANSISTOR: {
        const f64 _il = FUDE_ZOOM_PHOTOTRANSISTOR_A_LUX * rde_math_max_f64(_p->state[0], 0.0);
        const u32 _pins[2] = { 0u, 1u };
        fzc_nonlinear(_x, _p, _pins, 2u, fzc_phototransistor_fn, &_il);
        break;
    }
    default:
        return false;
    }
    return true;
}

// Stamps an analog chip: an LM393's comparators, an LM358's op-amps, an op-amp, a 555, a regulator. False: its model
// is none of them.
RDE_INTERNAL b8 fzc_stamp_analog(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const fude_zoom_part* _part = _p->part;
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_COMPARATOR: {
        // Two comparators (OUT, IN−, IN+ each) on VCC and GND (1 mA): each output sinking to GND while its IN− is over
        // its IN+ (16 mA at most), let go else — open collector: a pull-up makes it high.
        // Unpowered (under 2 V), let go.
        const fzc_comparator_k _k = { fzc_over(_x, _p, 7, 3) > 2.0 };
        fzc_g(_x, _p, 7, 3, 1e-3 / 5.0);
        const u32 _a[4] = { 0u, 1u, 2u, 3u }, _b[4] = { 6u, 5u, 4u, 3u };
        fzc_nonlinear(_x, _p, _a, 4u, fzc_comparator_fn, &_k);
        fzc_nonlinear(_x, _p, _b, 4u, fzc_comparator_fn, &_k);
        break;
    }
    case FUDE_ZOOM_MODEL_OPAMP2: {
        // Two op-amps (IN−, IN+, OUT each) on VCC and GND: their outputs from GND up to 1.5 V under VCC (an LM358's).
        fzc_opamp_k _k = { fzc_pv(_x, _p, 7) - 1.5, fzc_pv(_x, _p, 3) };
        if(_k.vp <= _k.vn) {
            _k.vp = _k.vn + 0.1;
        }
        fzc_g(_x, _p, 7, 3, 1e-3 / 5.0);
        const u32 _a[3] = { 1u, 2u, 0u }, _b[3] = { 5u, 4u, 6u };
        for(u32 _h = 0; _h < 2u; _h++) {
            const u32* _q = _h == 0u ? _a : _b;
            if(_p->node[_q[0]] != FUDE_ZOOM_NONE && _p->node[_q[0]] == _p->node[_q[2]]) {
                // (a follower — its output straight into its IN−: as a buffer, its IN+ out between its rails; the open
                // loop's 10⁵ through itself throws a solve from rail to rail)
                const u32 _f[2] = { _q[1], _q[2] };
                fzc_nonlinear(_x, _p, _f, 2u, fzc_follower_fn, &_k);
            } else {
                fzc_nonlinear(_x, _p, _q, 3u, fzc_opamp_fn, &_k);
            }
        }
        break;
    }
    case FUDE_ZOOM_MODEL_OPAMP: {
        fzc_opamp_k _k = { fzc_open(_p, 3) ? 12.0 : fzc_pv(_x, _p, 3), fzc_open(_p, 4) ? -12.0 : fzc_pv(_x, _p, 4) };
        if(_k.vp <= _k.vn) {
            _k.vp = _k.vn + 0.1;
        }
        const u32 _pins[3] = { 0u, 1u, 2u };
        fzc_nonlinear(_x, _p, _pins, 3u, fzc_opamp_fn, &_k);
        break;
    }
    case FUDE_ZOOM_MODEL_TIMER555: {
        // GND 0, TRIG 1, OUT 2, RESET 3, CTRL 4, THRES 5, DISCH 6, VCC 7.
        fzc_g(_x, _p, 7, 4, 1.0 / 5000.0);
        fzc_g(_x, _p, 4, 0, 1.0 / 10000.0);
        const f64 _vcc = rde_math_max_f64(fzc_over(_x, _p, 7, 0), 0.0);
        fzc_vsrc(_x, _p, 2, 0, _p->state[0] > 0.5 ? rde_math_max_f64(_vcc - 1.5, 0.0) : 0.0, 10.0);
        fzc_g(_x, _p, 6, 0, _p->state[0] > 0.5 ? 1e-9 : 0.1);
        fzc_g(_x, _p, 7, 0, 1.0 / 1e5);   // (what it draws)
        for(u32 _k = 1; _k < 6u; _k += 2u) {
            fzc_g(_x, _p, _k, 0, 1e-9);
        }
        break;
    }
    case FUDE_ZOOM_MODEL_REGULATOR: {
        // IN 0, GND 1, OUT 2: as high as it is made for, while its input is over that by its dropout.
        const f64 _want = rde_math_max_f64(rde_math_min_f64((f64)_part->a, fzc_over(_x, _p, 0, 1) - (f64)_part->b), 0.0);
        fzc_vsrc(_x, _p, 2, 1, _want, 0.05);
        fzc_i(_x, _p, 0, 1, rde_math_max_f64(_p->state[1], 0.0));   // (what goes out comes in)
        fzc_g(_x, _p, 0, 1, 1e-4);
        break;
    }
    default:
        return false;
    }
    return true;
}

// Stamps a display (display.h): a panel's, a matrix's or a bar graph's LEDs, an LM3914, an oscilloscope's probes, a
// panel meter, a character LCD, a 7-segment digit. False: its model is none of them.
RDE_INTERNAL b8 fzc_stamp_display(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const fude_zoom_part* _part = _p->part;
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_SEG_PANEL:
    case FUDE_ZOOM_MODEL_LED_MATRIX:
    case FUDE_ZOOM_MODEL_BAR_GRAPH: {
        // An LED from each anode it has to its cathode (display.h), as bright a colour's forward voltage as its text
        // says.
        const fzc_diode_k _k = { 0.01, 2.0 * FZC_VT, 0.0, _p->value[0] };
        const u32 _n = fude_zoom_display_lights(_part);
        for(u32 _l = 0; _l < _n; _l++) {
            u32 _pins[2];
            if(fude_zoom_display_led(_part, _l, &_pins[0], &_pins[1])) {
                fzc_nonlinear(_x, _p, _pins, 2u, fzc_diode_fn, &_k);
            }
        }
        break;
    }
    case FUDE_ZOOM_MODEL_LM3914: {
        // LED1 0, V− 1, V+ 2, RLO 3, SIG 4, RHI 5, REF OUT 6, REF ADJ 7, MODE 8, LED10 9 … LED2 17: what it draws, its
        // reference (1.25 V, REF OUT over REF ADJ), its divider (ten 1 kΩ from RLO to RHI), its inputs; each output its
        // comparator lit (last step: state[0]'s bits) sinking ten times what REF OUT gives (state[1]).
        fzc_g(_x, _p, 2, 1, 1.0 / 2400.0);
        fzc_vsrc(_x, _p, 6, 7, 1.25, 0.2);
        fzc_g(_x, _p, 3, 5, 1.0 / 10000.0);
        fzc_g(_x, _p, 4, 1, 1e-9);
        fzc_g(_x, _p, 8, 1, 1e-9);
        const u32 _lit = (u32)_p->state[0];
        for(u32 _k = 0; _k < 10u; _k++) {
            const u32 _pins[2] = { fzc_3914_led(_k), 1u };
            if(_lit & (1u << _k)) {
                fzc_nonlinear(_x, _p, _pins, 2u, fzc_sink_fn, &_p->state[1]);
            } else {
                fzc_g(_x, _p, _pins[0], 1, 1e-9);
            }
        }
        break;
    }
    case FUDE_ZOOM_MODEL_SCOPE:
        // Its probes: 1 MΩ each to its ground.
        fzc_g(_x, _p, 0, 2, 1e-6);
        fzc_g(_x, _p, 1, 2, 1e-6);
        break;
    case FUDE_ZOOM_MODEL_PANEL_METER:
        // An ammeter's shunt (200 mV across it at its full scale), or a voltmeter's 10 MΩ.
        fzc_g(_x, _p, 0, 1, _p->value[1] > 0.5 ? _p->value[0] / 0.2 : 1e-7);
        break;
    case FUDE_ZOOM_MODEL_CHAR_LCD: {
        // VSS 0, VDD 1, V0 2, RS 3, RW 4, E 5, D0–D7 6–13, A 14, K 15: what it draws (1.5 mA), its inputs (a little
        // each), its backlight (its LEDs behind its own 100 Ω).
        fzc_g(_x, _p, 1, 0, 1.0 / 3300.0);
        for(u32 _k = 2; _k < 14u; _k++) {
            fzc_g(_x, _p, _k, 0, 1e-9);
        }
        const fzc_led_r_k _k = { _p->value[0], 100.0 };
        const u32 _pins[2] = { 14u, 15u };
        fzc_nonlinear(_x, _p, _pins, 2u, fzc_led_r_fn, &_k);
        break;
    }
    case FUDE_ZOOM_MODEL_SEVEN_SEG: {
        fzc_diode_k _k = { 0.01, 2.0 * FZC_VT, 0.0, 1.8 };
        for(u32 _s = 0; _s < 8u; _s++) {
            const u32 _pins[2] = { _s, 8u };
            fzc_nonlinear(_x, _p, _pins, 2u, fzc_diode_fn, &_k);
        }
        break;
    }
    default:
        return false;
    }
    return true;
}

// Stamps what logic drives: a logic part's bridges (logic.h), a 74HC595, an L293D, a ULN2003, a relay module, a
// board's pins. False: its model is none of them.
RDE_INTERNAL b8 fzc_stamp_digital(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const fude_zoom_part* _part = _p->part;
    const f64 _t = _x->t;
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_GATE:
    case FUDE_ZOOM_MODEL_DFF:
    case FUDE_ZOOM_MODEL_TFF:
    case FUDE_ZOOM_MODEL_JKFF:
    case FUDE_ZOOM_MODEL_SRLATCH:
    case FUDE_ZOOM_MODEL_LOGIC_IN:
    case FUDE_ZOOM_MODEL_LOGIC_OUT:
    case FUDE_ZOOM_MODEL_SIM:
        fzc_logic_stamp(_x, _p);
        break;
    case FUDE_ZOOM_MODEL_SHIFT595: {
        // QA 14, QB–QH 0–6, GND 7, QH' 8, SRCLR 9, SRCLK 10, RCLK 11, OE 12, SER 13, VCC 15.
        const f64 _pow = fzc_over(_x, _p, 15, 7);
        fzc_g(_x, _p, 15, 7, 1e-5);
        const u32 _latch = (u32)_p->state[1], _reg = (u32)_p->state[0];
        const b8 _on = _pow > 1.0 && fzc_over(_x, _p, 12, 7) < _pow * 0.5;
        static const u32 _q[8] = { 14, 0, 1, 2, 3, 4, 5, 6 };
        for(u32 _k = 0; _k < 8u; _k++) {
            if(_on) {
                fzc_vsrc(_x, _p, _q[_k], 7, (_latch >> _k) & 1u ? _pow : 0.0, FZC_LOGIC_R);
            }
        }
        if(_pow > 1.0) {
            fzc_vsrc(_x, _p, 8, 7, (_reg >> 7) & 1u ? _pow : 0.0, FZC_LOGIC_R);
        }
        for(u32 _k = 9; _k <= 13u; _k++) {
            fzc_g(_x, _p, _k, 7, 1e-9);
        }
        break;
    }
    case FUDE_ZOOM_MODEL_DRIVER293: {
        // EN1,2 0, 1A 1, 1Y 2, GND 3, 2Y 5, 2A 6, VCC2 7, EN3,4 8, 3A 9, 3Y 10, 4Y 13, 4A 14, VCC1 15.
        const f64 _vm = fzc_over(_x, _p, 7, 3);
        static const u32 _ch[4][3] = { { 1, 2, 0 }, { 6, 5, 0 }, { 9, 10, 8 }, { 14, 13, 8 } };
        const u32 _bits = (u32)_p->state[0];
        for(u32 _k = 0; _k < 4u; _k++) {
            if((_bits >> (4u + _k)) & 1u) {   // (enabled)
                fzc_vsrc(_x, _p, _ch[_k][1], 3, (_bits >> _k) & 1u ? rde_math_max_f64(_vm - 1.4, 0.0) : 0.0, 1.0);
            }
            fzc_g(_x, _p, _ch[_k][0], 3, 1e-9);
        }
        fzc_g(_x, _p, 15, 3, 1e-4);
        break;
    }
    case FUDE_ZOOM_MODEL_ULN2003: {
        // (each output sinking to GND while its input is high; its clamp diode from it to COM — COM on the coils'
        // supply: a coil's current carried on when its output lets go)
        const u32 _bits = (u32)_p->state[0];
        const fzc_diode_k _clamp = { 1e-12, 1.5 * FZC_VT, 0.0, 0.0 };
        for(u32 _k = 0; _k < 7u; _k++) {
            fzc_g(_x, _p, 15u - _k, 7, (_bits >> _k) & 1u ? 1.0 / 2.0 : 1e-9);
            fzc_g(_x, _p, _k, 7, 1.0 / 2700.0);   // (its input's resistor)
            const u32 _pins[2] = { 15u - _k, 8u };
            fzc_nonlinear(_x, _p, _pins, 2u, fzc_diode_fn, &_clamp);
        }
        break;
    }
    case FUDE_ZOOM_MODEL_RELAY: {
        // VCC 0, GND 1, IN 2, COM 3, NO 4, NC 5.
        const b8 _on = _p->state[0] > 0.5;
        fzc_g(_x, _p, 0, 1, _on ? 1.0 / 70.0 : 1e-6);
        fzc_g(_x, _p, 2, 1, 1e-5);
        fzc_contact(_x, _p, 3, 4, _on, 20.0);
        fzc_contact(_x, _p, 3, 5, !_on, 20.0);
        break;
    }
    case FUDE_ZOOM_MODEL_BOARD: {
        const u32 _gnd = fzc_pin_named(_part, "GND");
        if(_gnd == FUDE_ZOOM_NONE) {
            break;
        }
        for(u32 _k = 0; _k < _part->pin_count && _k < FZC_PIN_BITS; _k++) {   // (a board's pins: fewer than 64)
            const f64 _sv = fzc_supply(_part->pins[_k].name, (f64)_part->a);
            if(_sv > 0.0) {
                fzc_vsrc(_x, _p, _k, _gnd, _sv, 0.02);
                continue;
            }
            const u64 _bit = 1ull << _k;
            if(_p->drive & _bit) {
                const b8 _high = (_p->drive_blink & _bit) ? rde_math_fmod_f64(_t, 1.0) < 0.5 : (_p->drive_high & _bit) != 0u;
                fzc_vsrc(_x, _p, _k, _gnd, _high ? (f64)_part->a : 0.0, FZC_LOGIC_R);
            }
        }
        break;
    }
    default:
        return false;
    }
    return true;
}

// Stamps part _p into its block's system for the step: what each kind is, as conductances, currents, sources and
// non-linear elements, each family of models in its own function above (each case says its pins in order). A burnt
// part is open but for an arc.
void fzc_stamp(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const fude_zoom_part* _part = _p->part;
    if(_p->burnt) {
        // (burnt: open, all but a leak — flashing over past FUDE_ZOOM_ARC between any two of its pins, a chip's from
        // its first to each other: a coil's current through it when it went goes on somewhere)
        const u32 _np = _part->pin_count < 8u ? _part->pin_count : 8u;
        for(u32 _a = 0; _a < (_np <= 4u ? _np : 1u); _a++) {
            for(u32 _b = _a + 1u; _b < _np; _b++) {
                fzc_contact(_x, _p, _a, _b, false, 0.0);
            }
        }
        return;
    }
    // Each model is its family's (fzc_stamp_*); one in none of them (a module, a microcontroller) stamps nothing.
    if(!fzc_stamp_load(_x, _p) && !fzc_stamp_source(_x, _p) && !fzc_stamp_semiconductor(_x, _p) &&
       !fzc_stamp_analog(_x, _p) && !fzc_stamp_display(_x, _p)) {
        fzc_stamp_digital(_x, _p);
    }
}
