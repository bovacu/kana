// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/circuit_parts.h"

// ===========================================================================
// See circuit.h. Circuits, what Play asks of the circuit as it plays: an oscilloscope's knob turned, a shafted
// motor's load (for coupling.h), how big Play's tags are, and a tap on a part (a switch, a sensor's light, an encoder,
// a board's pin) with what the part now says.
//
// Part of the circuit: circuit_parts.h holds what the modules share and says which module holds what. Prefix fzc_ and
// FZC_. Main thread only.
// ===========================================================================

// See the header. A knob at its end turns no further, but what it says is still written.
b8 fude_zoom_circuit_scope_turn(fude_zoom_circuit* _c, u32 _part, u32 _knob, i32 _way, c8* _say, usize _size) {
    if(_part >= (u32)rde_arr_length(&_c->parts) || _knob >= FUDE_ZOOM_SCOPE_KNOBS) {
        return false;
    }
    fude_zoom_circuit_part* _q = &((fude_zoom_circuit_part*)_c->parts.memory)[_part];
    if(_q->part->model != FUDE_ZOOM_MODEL_SCOPE) {
        return false;
    }
    fude_zoom_scope _s = fude_zoom_display_scope_of(_q);
    if(fude_zoom_scope_turn(&_s, _knob, _way < 0 ? -1 : 1)) {
        _q->value[0] = _s.time_div;
        _q->value[1] = _s.volt_div;
        _q->value[3] = _s.volt_div2;
        _q->value[4] = _s.pos1;
        _q->value[5] = _s.pos2;
        if(_knob == FUDE_ZOOM_SCOPE_KNOB_TIME) {
            // A sweep of another length: waiting for its trigger again; the steps as fine as its samples.
            if(_q->state[1] > -1.5) {
                _q->state[1] = -1.0;
                _q->state[5] = _c->time;
            }
            fzc_step_most(_c);
        }
    }
    c8 _v[32];
    fude_zoom_scope_knob_say(&_s, _knob, _v, sizeof(_v));
    rde_snprintf(_say, _size, "%s %s", fude_zoom_scope_knob_name(_knob), _v);
    return true;
}

// See the header: its rated volts over its rated speed in radians a second.
f64 fude_zoom_motor_k(const fude_zoom_circuit_part* _motor) {
    const f64 _volts = _motor->value[0] > 0.0 ? _motor->value[0] : 6.0, _rpm = _motor->value[1] > 0.0 ? _motor->value[1] : 60.0;
    return _volts / (_rpm * 2.0 * RDE_PI / 60.0);
}

// See the header. Worked out by solving the motor's own block again with its back-EMF a little higher (2%, 0.05 V at
// least), then putting every voltage and current back as it was.
b8 fude_zoom_circuit_motor_load(fude_zoom_circuit* _c, u32 _part, f64* _conductance, f64* _still) {
    *_conductance = 0.0;
    *_still = 0.0;
    if(_part >= (u32)rde_arr_length(&_c->parts) || !_c->ok) {
        return false;
    }
    fude_zoom_circuit_part* _p = &((fude_zoom_circuit_part*)_c->parts.memory)[_part];
    if(_p->part->model != FUDE_ZOOM_MODEL_MOTOR || !_p->shafted) {
        return false;
    }
    const f64 _k = fude_zoom_motor_k(_p), _e = _k * _p->spin, _i0 = _p->pin_i[0];
    *_still = _e;
    const u32 _n = _c->nodes > 0u ? _c->nodes - 1u : 0u;
    if(_n == 0u || fzc_node(_p, 0) == FUDE_ZOOM_NONE || fzc_node(_p, 1) == FUDE_ZOOM_NONE) {
        return true;   // Joined to nothing: nothing through it whatever it turns.
    }
    // Solved again with its back-EMF a little more (as the last step was: its state as it is), what it changes through
    // it; then all as it was.
    // (every part's pins' currents are the circuit's pin_amps: kept whole, and put back whole)
    const usize _amps = rde_arr_length(&_c->pin_amps);
    rde_arr_resize(&_c->amps_keep, _amps);
    rde_memcpy(_c->amps_keep.memory, _c->pin_amps.memory, _amps * sizeof(f64));
    rde_arr_resize(&_c->v_keep, _c->nodes);
    rde_memcpy(_c->v_keep.memory, _c->v.memory, (usize)_c->nodes * sizeof(f64));
    const u32 _solves = _c->solves;
    const f64 _de = rde_math_max_f64(0.02 * rde_math_abs_f64(_e), 0.05);
    const f64 _spin = _p->spin;
    _p->spin = _spin + _de / _k;
    fzc_room(_c, _n);
    fzc_ctx _x = { _c, _n, true, _c->step_last > 0.0 ? _c->step_last : _c->step, _c->time, _c->be_last };
    // Only its own block: the others are as they were.
    const u32 _own = fzc_node(_p, 0) != 0u ? fzc_node(_p, 0) : fzc_node(_p, 1);
    const u32 _blk = _own != 0u && _own < _c->nodes ? ((const u32*)_c->node_block.memory)[_own] : FUDE_ZOOM_NONE;
    const b8 _ok = _blk != FUDE_ZOOM_NONE && fzc_newton(&_x, _blk);
    const f64 _i1 = _p->pin_i[0];
    _p->spin = _spin;
    rde_memcpy(_c->v.memory, _c->v_keep.memory, (usize)_c->nodes * sizeof(f64));
    rde_memcpy(_c->pin_amps.memory, _c->amps_keep.memory, _amps * sizeof(f64));
    _c->solves = _solves;
    if(!_ok) {
        return false;
    }
    // More back-EMF, less through it: a conductance; never more than its winding's own.
    const f64 _g = rde_math_min_f64(rde_math_max_f64((_i0 - _i1) / _de, 0.0), 1.0 / FUDE_ZOOM_MOTOR_R);
    *_conductance = _g;
    *_still = _g > 1e-12 ? _e + _i0 / _g : _e;
    return true;
}

// Orders two f64s, for rde_sort.
RDE_INTERNAL int fzc_f64_order(const any _a, const any _b) {
    const f64 _x = *(const f64*)_a, _y = *(const f64*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// See the header: half the median size, at most _most; under _least, 0. _sizes is sorted in place, the sizes not above
// 0 or not finite left out.
f64 fude_zoom_circuit_tag_px(f64* _sizes, u32 _count, f64 _most, f64 _least) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        if(_sizes[_i] > 0.0 && rde_math_is_finite_f64(_sizes[_i])) {
            _sizes[_n++] = _sizes[_i];
        }
    }
    if(_n == 0u) {
        return 0.0;
    }
    rde_sort(_sizes, _n, sizeof(f64), fzc_f64_order);
    const f64 _px = rde_math_min_f64(0.5 * _sizes[_n / 2u], _most);
    return _px >= _least ? _px : 0.0;
}

// What a tap changes on _p (at its pin _pin, for a board), with what it now says in _say, in no language (a level, a
// contact's or a pin's name, a number with a symbol for its unit; the word that goes with it is
// fude_zoom_circuit_tap_word's); false when a tap changes nothing on it.
RDE_INTERNAL b8 fzc_tap_change(fude_zoom_circuit_part* _p, i32 _pin, c8* _say, usize _size) {
    switch(_p->part->model) {
    case FUDE_ZOOM_MODEL_SWITCH:
        _p->switch_on ^= 1u;   // (on or off: its word alone)
        return true;
    case FUDE_ZOOM_MODEL_LOGIC_IN:
        _p->switch_on ^= 1u;
        rde_snprintf(_say, _size, "%s", (_p->switch_on & 1u) ? "1" : "0");
        return true;
    case FUDE_ZOOM_MODEL_BUTTON:
        _p->switch_on ^= 1u;
        return true;
    case FUDE_ZOOM_MODEL_SPDT:
    case FUDE_ZOOM_MODEL_DPDT:
        _p->switch_on ^= 1u;
        rde_snprintf(_say, _size, "%s", (_p->switch_on & 1u) ? "B" : "A");
        return true;
    case FUDE_ZOOM_MODEL_LDR:
    case FUDE_ZOOM_MODEL_PHOTODIODE:
    case FUDE_ZOOM_MODEL_PHOTOTRANSISTOR: {
        // Dark, a room, sunlight, round again: its lux (the word "lux" fude_zoom_circuit_tap_word's).
        _p->state[0] = _p->state[0] < 10.0 ? 100.0 : (_p->state[0] < 1000.0 ? 10000.0 : 1.0);
        rde_snprintf(_say, _size, "%.0f", _p->state[0]);
        return true;
    }
    case FUDE_ZOOM_MODEL_THERMISTOR: {
        // Freezing, a room, hot, boiling, round again.
        _p->state[0] = _p->state[0] < 12.0 ? 25.0 : (_p->state[0] < 40.0 ? 60.0 : (_p->state[0] < 80.0 ? 100.0 : 0.0));
        rde_snprintf(_say, _size, "%.0f \xC2\xB0" "C", _p->state[0]);
        return true;
    }
    case FUDE_ZOOM_MODEL_ENCODER:
        // A detent on, clockwise.
        _p->state[1] = rde_math_round_f64(_p->state[1]) + 1.0;
        rde_snprintf(_say, _size, "%.0f", _p->state[1]);
        return true;
    case FUDE_ZOOM_MODEL_SLOT:
        _p->state[0] = _p->state[0] < 0.5 ? 1.0 : 0.0;
        return true;
    case FUDE_ZOOM_MODEL_POT: {
        const f64 _f = _p->state[0] + 0.25;
        _p->state[0] = _f > 1.0001 ? 0.0 : _f;
        rde_snprintf(_say, _size, "%.0f%%", _p->state[0] * 100.0);
        return true;
    }
    case FUDE_ZOOM_MODEL_BOARD: {
        if(_pin < 0 || (u32)_pin >= _p->part->pin_count || _pin >= (i32)FZC_PIN_BITS || fzc_supply(_p->part->pins[_pin].name, (f64)_p->part->a) > 0.0 ||
           fzc_same_name(_p->part->pins[_pin].name, "GND")) {
            return false;
        }
        // Its pin through input, high, low, blinking: its name (how it is driven now, fude_zoom_circuit_tap_word's).
        const u64 _bit = 1ull << (u32)_pin;
        if(!(_p->drive & _bit)) {
            _p->drive |= _bit; _p->drive_high |= _bit; _p->drive_blink &= ~_bit;
        } else if((_p->drive_high & _bit) && !(_p->drive_blink & _bit)) {
            _p->drive_high &= ~_bit;
        } else if(!(_p->drive_blink & _bit)) {
            _p->drive_blink |= _bit;
        } else {
            _p->drive &= ~_bit; _p->drive_high &= ~_bit; _p->drive_blink &= ~_bit;
        }
        rde_snprintf(_say, _size, "%s", _p->part->pins[_pin].name);
        return true;
    }
    default:
        return false;
    }
}

// See the header: from the part's state as a tap leaves it.
u8 fude_zoom_circuit_tap_word(const fude_zoom_circuit_part* _p, i32 _pin) {
    switch(_p->part->model) {
    case FUDE_ZOOM_MODEL_SWITCH:
        return (_p->switch_on & 1u) ? FUDE_ZOOM_TAP_ON : FUDE_ZOOM_TAP_OFF;
    case FUDE_ZOOM_MODEL_LDR:
    case FUDE_ZOOM_MODEL_PHOTODIODE:
    case FUDE_ZOOM_MODEL_PHOTOTRANSISTOR:
        return FUDE_ZOOM_TAP_LUX;
    case FUDE_ZOOM_MODEL_BOARD: {
        if(_pin < 0 || _pin >= (i32)FZC_PIN_BITS) {
            return FUDE_ZOOM_TAP_PLAIN;
        }
        const u64 _bit = 1ull << (u32)_pin;
        return !(_p->drive & _bit) ? FUDE_ZOOM_TAP_INPUT : ((_p->drive_blink & _bit) ? FUDE_ZOOM_TAP_BLINK : ((_p->drive_high & _bit) ? FUDE_ZOOM_TAP_HIGH : FUDE_ZOOM_TAP_LOW));
    }
    default:
        return FUDE_ZOOM_TAP_PLAIN;
    }
}

// See the header: fzc_tap_change, and a jump when it changed something.
b8 fude_zoom_circuit_tap(fude_zoom_circuit* _c, u32 _part, i32 _pin, c8* _say, usize _size) {
    _say[0] = 0;
    if(_part >= (u32)rde_arr_length(&_c->parts) || !fzc_tap_change(&((fude_zoom_circuit_part*)_c->parts.memory)[_part], _pin, _say, _size)) {
        return false;   // Nothing changed: the steps go on as they were.
    }
    fude_zoom_circuit_jump(_c);   // What it does now jumps: the steps after it short, the first backward Euler.
    return true;
}
