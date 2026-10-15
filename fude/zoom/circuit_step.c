// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/circuit_parts.h"

// ===========================================================================
// See circuit.h. Circuits, what the parts do as time goes on: what is stepped from the voltages (fzc_digital: a
// 555, a 74HC595, a driver, a relay, a fuse); after each step, what they show and remember (fzc_after), their limits
// (heated and burnt), the next step's size, and the moments a step must land on (fzc_breakpoint); a 555's threshold
// ahead, a servo, contacts' sparks, a thyristor, the speakers' sound, a solenoid's plunger, a stepper, an encoder, a
// display's glow, an LM3914 and a character LCD.
//
// The file is in order: what is stepped, then each part's own step after the solve (each with its comment), the
// breakpoints, the limits, the displays, and fzc_after, which calls them.
//
// Part of the circuit: circuit_parts.h holds what the modules share and says which module holds what. Prefix fzc_ and
// FZC_. Main thread only.
// ===========================================================================

// What is stepped (logic, a 555, a fuse) worked out from the voltages now. True: something changed (solved again).
b8 fzc_digital(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const fude_zoom_part* _part = _p->part;
    const f64 _was = _p->state[0], _was1 = _p->state[1];
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_TIMER555: {
        const f64 _ctrl = fzc_over(_x, _p, 4, 0);
        if(fzc_over(_x, _p, 3, 0) < 0.7) {
            _p->state[0] = 0.0;
        } else if(fzc_over(_x, _p, 1, 0) < _ctrl * 0.5) {
            _p->state[0] = 1.0;
        } else if(fzc_over(_x, _p, 5, 0) > _ctrl) {
            _p->state[0] = 0.0;
        }
        break;
    }
    case FUDE_ZOOM_MODEL_SHIFT595: {
        const f64 _half = 0.5 * fzc_over(_x, _p, 15, 7);
        const b8 _srclk = fzc_over(_x, _p, 10, 7) > _half, _rclk = fzc_over(_x, _p, 11, 7) > _half;
        u32 _reg = (u32)_p->state[0];
        if(fzc_over(_x, _p, 9, 7) < _half) {
            _reg = 0u;   // (cleared)
        } else if(_srclk && _p->state[2] < 0.5) {
            _reg = ((_reg << 1) | (fzc_over(_x, _p, 13, 7) > _half ? 1u : 0u)) & 0xFFu;
        }
        if(_rclk && _p->state[3] < 0.5) {
            _p->state[1] = (f64)_reg;
        }
        _p->state[0] = (f64)_reg;
        _p->state[2] = _srclk ? 1.0 : 0.0;
        _p->state[3] = _rclk ? 1.0 : 0.0;
        return _p->state[0] != _was || _p->state[1] != _was1;
    }
    case FUDE_ZOOM_MODEL_DRIVER293: {
        const f64 _half = 0.5 * rde_math_max_f64(fzc_over(_x, _p, 15, 3), 3.0);
        static const u32 _ch[4][3] = { { 1, 2, 0 }, { 6, 5, 0 }, { 9, 10, 8 }, { 14, 13, 8 } };
        u32 _bits = 0;
        for(u32 _k = 0; _k < 4u; _k++) {
            _bits |= fzc_over(_x, _p, _ch[_k][0], 3) > _half ? 1u << _k : 0u;
            _bits |= fzc_over(_x, _p, _ch[_k][2], 3) > _half ? 1u << (4u + _k) : 0u;
        }
        _p->state[0] = (f64)_bits;
        break;
    }
    case FUDE_ZOOM_MODEL_ULN2003: {
        u32 _bits = 0;
        for(u32 _k = 0; _k < 7u; _k++) {
            _bits |= fzc_over(_x, _p, _k, 7) > 1.5 ? 1u << _k : 0u;
        }
        _p->state[0] = (f64)_bits;
        break;
    }
    case FUDE_ZOOM_MODEL_RELAY:
        _p->state[0] = fzc_over(_x, _p, 2, 1) > 2.0 && fzc_over(_x, _p, 0, 1) > 3.0 ? 1.0 : 0.0;
        break;
    case FUDE_ZOOM_MODEL_FUSE:
        if(rde_math_abs_f64(_p->pin_i[0]) > _p->value[0] * 1.5) {
            _p->state[0] = 1.0;   // (blown, until the circuit starts again)
        }
        break;
    default:
        return false;
    }
    return _p->state[0] != _was;
}

// A 555 as its capacitor heads for a threshold (charging, its output high: THRES for CTRL; discharging: TRIG for half
// of it): when it will reach it, on as it goes now — kept (state[7]: the time; 0, none) for the next step to land just
// past it, its output's edge where it is.
RDE_INTERNAL void fzc_555_ahead(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const f64 _ctrl = fzc_over(_x, _p, 4, 0);
    const f64 _thr  = fzc_over(_x, _p, 5, 0) - _ctrl, _trg = fzc_over(_x, _p, 1, 0) - 0.5 * _ctrl;
    const f64 _dt   = _x->t - _p->state[6];
    _p->state[7] = 0.0;
    if(_p->state[6] > 0.0 && _dt > 0.0) {
        const f64 _ds = (_thr - _p->state[4]) / _dt, _dg = (_trg - _p->state[5]) / _dt;
        if(_p->state[0] > 0.5 && _thr < 0.0 && _ds > 0.0) {
            _p->state[7] = _x->t + -_thr / _ds;
        } else if(_p->state[0] <= 0.5 && _trg > 0.0 && _dg < 0.0) {
            _p->state[7] = _x->t + _trg / -_dg;
        }
    }
    _p->state[4] = _thr;
    _p->state[5] = _trg;
    _p->state[6] = _x->t;
}

#define FZC_SERVO_HIGH   1.5    // a servo's signal high from (volts over its ground); its least supply: circuit.h's
                                // FUDE_ZOOM_SERVO_LEAST

// A servo a step on (GND, VCC, SIG; state: 0 its horn's angle (degrees), 1 where its pulses say to go (-1: none), 2 its
// signal high, 3 when its pulse began, 4 when its last one ended, 5 its signal's volts, 6 what it draws, 7 on a
// mechanism's shaft — how hard it strains (0..1) — or not, -1). Its pulses timed where its signal crosses 1.5 V — at
// the end of the step it crossed in: the steps land on a source's edges and a 555's thresholds, where a pulse jumps (a
// line across the step would put it early by most of the step before): 0.5 to 2.5 ms its angle, 0 to 180°; none for
// 60 ms, it lets go.
// Its horn turned toward it as fast as its supply lets it (its speed at 4.8 V) — on a shaft, coupling turns it. What it
// draws: 10 mA at rest, up to a third of its stall's turning, its stall's held back.
RDE_INTERNAL void fzc_servo(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const f64 _vcc = fzc_over(_x, _p, 1, 0), _sig = fzc_over(_x, _p, 2, 0), _was = _p->state[5];
    if(_was < FZC_SERVO_HIGH && _sig >= FZC_SERVO_HIGH) {
        _p->state[3] = _x->t;
        _p->state[2] = 1.0;
    } else if(_was >= FZC_SERVO_HIGH && _sig < FZC_SERVO_HIGH && _p->state[2] > 0.5) {
        const f64 _end = _x->t;
        const f64 _width = _end - _p->state[3];
        if(_width >= 0.4e-3 && _width <= 2.7e-3) {
            _p->state[1] = rde_math_min_f64(rde_math_max_f64((_width - 0.5e-3) / 2.0e-3 * 180.0, 0.0), 180.0);
            _p->state[4] = _end;
        }
        _p->state[2] = 0.0;
    }
    _p->state[5] = _sig;
    if(_p->state[4] >= 0.0 && _x->t - _p->state[4] > 0.06) {
        _p->state[1] = -1.0;
    }
    const b8  _on  = _vcc >= FUDE_ZOOM_SERVO_LEAST && _p->state[1] >= 0.0;
    const f64 _top = _p->value[1] * 180.0 / RDE_PI * rde_math_min_f64(_vcc / 4.8, 1.3);   // (degrees a second)
    f64 _pace = 0.0, _strain = _p->state[7] >= 0.0 ? _p->state[7] : 0.0;
    if(_on && _p->state[7] < 0.0) {
        const f64 _d = rde_math_min_f64(rde_math_max_f64(_p->state[1] - _p->state[0], -_top * _x->dt), _top * _x->dt);
        _p->state[0] += _d;
        _pace = _top > 0.0 ? rde_math_abs_f64(_d) / (_top * _x->dt) : 0.0;
    } else if(!_on) {
        _strain = 0.0;
    }
    _p->state[6] = _vcc >= FUDE_ZOOM_SERVO_LEAST ? 0.01 + _p->value[2] * rde_math_max_f64(_pace / 3.0, _strain) : 0.0;
    _p->shown = _p->state[0];
}

// A part's open contacts arcing (FUDE_ZOOM_ARC over one, all but): its spark's time; a spark after none for a fifth of
// a second, told (FUDE_ZOOM_CIRCUIT_ARC).
RDE_INTERNAL void fzc_sparks(fzc_ctx* _x, fude_zoom_circuit_part* _p, u32 _index) {
    u32 _pairs[4][2], _n = 0;
    const b8 _on = (_p->switch_on & 1u) != 0u;
    switch(_p->part->model) {
    case FUDE_ZOOM_MODEL_SWITCH:
    case FUDE_ZOOM_MODEL_BUTTON:
        if(!_on && !_p->pushed) { _pairs[_n][0] = 0u; _pairs[_n++][1] = 1u; }
        break;
    case FUDE_ZOOM_MODEL_SPDT:
    case FUDE_ZOOM_MODEL_DPDT:
        for(u32 _k = 0; _k < (_p->part->model == FUDE_ZOOM_MODEL_DPDT ? 2u : 1u); _k++) {
            _pairs[_n][0] = 3u * _k; _pairs[_n++][1] = 3u * _k + (_on ? 1u : 2u);   // (the side it is not on)
        }
        break;
    case FUDE_ZOOM_MODEL_RELAY:
        _pairs[_n][0] = 3u; _pairs[_n++][1] = _p->state[0] > 0.5 ? 5u : 4u;
        break;
    default:
        break;
    }
    f64 _worst = 0.0;
    for(u32 _k = 0; _k < _n; _k++) {
        if(_p->node[_pairs[_k][0]] != FUDE_ZOOM_NONE && _p->node[_pairs[_k][1]] != FUDE_ZOOM_NONE) {
            _worst = rde_math_max_f64(_worst, rde_math_abs_f64(fzc_over(_x, _p, _pairs[_k][0], _pairs[_k][1])));
        }
    }
    if(_worst < FZC_ARC * 0.99) {
        return;
    }
    if(!(_p->state[7] > 0.0) || _x->t - _p->state[7] > 0.2) {
        const fude_zoom_circuit_event _e = { _index, FUDE_ZOOM_CIRCUIT_ARC, 0u, _worst, FZC_ARC };
        rde_arr_add(&_x->c->events, (any)&_e);
    }
    _p->state[7] = _x->t;
}

// See the header: a contact's state[7]; 0 for any other part.
f64 fude_zoom_circuit_sparked(const fude_zoom_circuit_part* _p) {
    const u8 _m = _p->part->model;
    return _m == FUDE_ZOOM_MODEL_SWITCH || _m == FUDE_ZOOM_MODEL_BUTTON || _m == FUDE_ZOOM_MODEL_SPDT || _m == FUDE_ZOOM_MODEL_DPDT || _m == FUDE_ZOOM_MODEL_RELAY ?
           _p->state[7] : 0.0;
}

// A thyristor a step on: off, fired by its gate's current (either way, a TRIAC's) with its anode over its cathode (or
// either way); on, let go when what goes through it falls under its holding current. Either: a jump.
RDE_INTERNAL void fzc_thyristor(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const b8 _tri = _p->part->model == FUDE_ZOOM_MODEL_TRIAC;
    const f64 _ig = _p->pin_i[2], _it = _p->pin_i[0], _v = fzc_over(_x, _p, 0, 1);
    if(_p->state[0] < 0.5) {
        if((_tri ? rde_math_abs_f64(_ig) : _ig) > _p->value[0] && (_tri ? rde_math_abs_f64(_v) : _v) > 0.3 && !_p->burnt) {
            _p->state[0] = 1.0;
            fude_zoom_circuit_jump(_x->c);
        }
    } else if((_tri ? rde_math_abs_f64(_it) : _it) < _p->value[1]) {
        _p->state[0] = 0.0;
        fude_zoom_circuit_jump(_x->c);
    }
}

// See the header.
void fude_zoom_circuit_sound(fude_zoom_circuit* _c, f64 _rate) {
    _c->audio_rate = _rate > 0.0 ? _rate : 0.0;
    rde_arr_clear(&_c->audio);
    _c->audio_next = _c->time;
    _c->audio_dc   = 0.0;
}

#define FZC_SOUND_FULL 0.1      // a speaker's current heard at full loudness (A: what a 555 or a transistor drives one with)
#define FZC_SOUND_MOST 48000u   // samples kept unplayed at most (a second's: the page takes them each frame)

// The speakers' sound for the step just taken: their currents together, a sample each 1/audio_rate of the circuit's
// time up to now (as the step ends: what it reached), the slow average (under 20 Hz) taken out.
RDE_INTERNAL void fzc_sound(fzc_ctx* _x) {
    fude_zoom_circuit* _c = _x->c;
    if(!_x->tran || !(_c->audio_rate > 0.0)) {
        return;
    }
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_c->parts.memory;
    f64 _sum = 0.0;
    b8 _any = false;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        if(_p[_i].part->model == FUDE_ZOOM_MODEL_SPEAKER && !_p[_i].burnt) {
            _sum += _p[_i].pin_i[0];
            _any = true;
        }
    }
    if(!_any) {
        return;
    }
    const f64 _per = 1.0 / _c->audio_rate, _k = 2.0 * RDE_PI * 20.0 * _per;
    if(_c->audio_next < _x->t - _x->dt - _per) {
        _c->audio_next = _x->t - _x->dt;   // Nothing made for long ago: from this step's start.
    }
    while(_c->audio_next <= _x->t && (u32)rde_arr_length(&_c->audio) < FZC_SOUND_MOST) {
        const f64 _v = _sum / FZC_SOUND_FULL;
        _c->audio_dc += (_v - _c->audio_dc) * _k;
        const f32 _smp = (f32)rde_math_min_f64(rde_math_max_f64(_v - _c->audio_dc, -1.0), 1.0);
        rde_arr_add(&_c->audio, (any)&_smp);
        _c->audio_next += _per;
    }
}

// See the header: its current over its rated current, squared, times 0.6 out to 1.4 in (its plunger's state[2]).
f64 fude_zoom_circuit_solenoid_pull(const fude_zoom_circuit_part* _p) {
    const f64 _rated = _p->value[0] / rde_math_max_f64(_p->value[1], 1e-9);
    const f64 _k = _p->state[0] / rde_math_max_f64(_rated, 1e-12);
    return _k * _k * (0.6 + 0.8 * rde_math_min_f64(rde_math_max_f64(_p->state[2], 0.0), 1.0));
}

// A solenoid's plunger alone, a step on: in (15 ms all the way) while its pull beats its spring, out (30 ms) while it
// does not.
RDE_INTERNAL void fzc_plunger(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const f64 _was = _p->state[2];
    _p->state[2] = fude_zoom_circuit_solenoid_pull(_p) > FUDE_ZOOM_SOLENOID_SPRING ? rde_math_min_f64(_was + _x->dt / 0.015, 1.0) : rde_math_max_f64(_was - _x->dt / 0.03, 0.0);
}

// See the header: its four coils' currents (state 3 to 6) added as vectors a quarter turn apart; its strength against
// one coil's full current.
f64 fude_zoom_circuit_stepper_field(const fude_zoom_circuit_part* _p, f64* _strength) {
    f64 _x = 0.0, _y = 0.0;
    for(u32 _k = 0; _k < 4u; _k++) {
        _x += _p->state[3u + _k] * rde_math_cos_f64(0.5 * RDE_PI * (f64)_k);
        _y += _p->state[3u + _k] * rde_math_sin_f64(0.5 * RDE_PI * (f64)_k);
    }
    *_strength = rde_math_hypot_f64(_x, _y) / (5.0 / FUDE_ZOOM_STEPPER_COIL);
    return rde_math_atan2_f64(_y, _x);
}

// A stepper a step on: its coils' currents (into each from COM: state 3–6); alone, its rotor toward its field the
// nearest way there (none: the field too weak, or straight behind it), as fast as it can. Its shaft's angle and its
// steps from its rotor's (on a mechanism's shaft: coupling's, its shaft's).
RDE_INTERNAL void fzc_stepper(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    for(u32 _k = 0; _k < 4u; _k++) {
        _p->state[3u + _k] = _p->burnt ? 0.0 : fzc_over(_x, _p, 4, _k) / FUDE_ZOOM_STEPPER_COIL;
    }
    if(_p->state[7] < 0.0) {
        f64 _strength;
        const f64 _to = fude_zoom_circuit_stepper_field(_p, &_strength);
        const f64 _off = rde_math_remainder_f64(_to - _p->state[0], 2.0 * RDE_PI);
        if(_strength > 0.3 && rde_math_abs_f64(_off) < RDE_PI - 1e-3) {
            const f64 _most = FUDE_ZOOM_STEPPER_RATE * 0.5 * RDE_PI * _x->dt;
            _p->state[0] += rde_math_min_f64(rde_math_max_f64(_off, -_most), _most);
        }
    }
    _p->state[1] = _p->state[0] * 4.0 / rde_math_max_f64(_p->value[0], 1.0) * 180.0 / RDE_PI;
    _p->state[2] = _p->state[0] / (0.5 * RDE_PI);
    _p->shown    = _p->state[1];
}

// See the header: within each detent, A closed over its middle half, B over its second half (a quarter out of step).
void fude_zoom_circuit_encoder_contacts(f64 _detents, b8* _a, b8* _b) {
    const f64 _f = _detents - rde_math_floor_f64(_detents);
    *_a = _f >= 0.25 && _f < 0.75;
    *_b = _f >= 0.5;
}

// An encoder a step on: alone, toward where taps send it (a detent in 40 ms); its contacts changed: a jump.
RDE_INTERNAL void fzc_encoder(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    b8 _a0, _b0, _a1, _b1;
    fude_zoom_circuit_encoder_contacts(_p->state[6], &_a0, &_b0);
    if(_p->state[7] < 0.0) {
        const f64 _most = _x->dt / 0.04;
        _p->state[0] += rde_math_min_f64(rde_math_max_f64(_p->state[1] - _p->state[0], -_most), _most);
    }
    fude_zoom_circuit_encoder_contacts(_p->state[0], &_a1, &_b1);
    if(_a0 != _a1 || _b0 != _b1) {
        fude_zoom_circuit_jump(_x->c);
    }
    _p->state[6] = _p->state[0];   // Where its contacts were last stepped.
    _p->shown    = _p->state[0];
}

// How long until something is about to happen (seconds; a longer step would miss its moment): a clock's next edge, a
// 555's threshold (just past it), an AC source's fortieth of a period.
f64 fzc_breakpoint(const fude_zoom_circuit* _c) {
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_c->parts.memory;
    const f64 _t = _c->time;
    f64 _h = 1e300;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        const fude_zoom_circuit_part* _q = &_p[_i];
        switch(_q->part->model) {
        case FUDE_ZOOM_MODEL_CLOCK:
            if(_q->value[0] > 0.0) {
                const f64 _f = _q->value[0], _d = fzc_duty(_q);
                const f64 _ph = fzc_clock_phase(_q, _t);
                _h = rde_math_min_f64(_h, ((_ph < _d ? _d : 1.0) - _ph + 1e-9) / _f);   // (landing just past it)
            }
            break;
        case FUDE_ZOOM_MODEL_ACSOURCE:
            if(_q->value[1] > 0.0) {
                _h = rde_math_min_f64(_h, 1.0 / (40.0 * _q->value[1]));
            }
            break;
        case FUDE_ZOOM_MODEL_TIMER555:
            if(_q->state[7] > _t) {
                const f64 _to = _q->state[7] - _t;
                _h = rde_math_min_f64(_h, _to + rde_math_max_f64(1e-3 * _to, 2.0 * FZC_STEP_LEAST));
            }
            break;
        default:
            break;
        }
    }
    return _h;
}

#define FZC_OVER_TELL 0.02   // a source past its current so long (seconds) before it is said (not a capacitor's charging)

// Declared ahead: a display's LEDs measured (defined below).
RDE_INTERNAL void fzc_display_measure(fzc_ctx* _x, fude_zoom_circuit_part* _p);

// Each part against its limits (limits.h), a step of time on: measured, heated past them; burnt, or a source past its
// current, said (an event).
RDE_INTERNAL void fzc_limits(fzc_ctx* _x) {
    fude_zoom_circuit* _c = _x->c;
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        fude_zoom_circuit_part* _q = &_p[_i];
        if(fude_zoom_limits_kinds(_q->part) == 0u) {
            continue;
        }
        if(_q->burnt) {
            rde_memset(_q->measure, 0, sizeof(_q->measure));   // Open: nothing in it now.
            _q->stress = 0.0;
            continue;
        }
        // Its pins' volts and currents, in the circuit's scratch for one part (fzc_blocks sizes it for every part).
        const u32 _pins = _q->part->pin_count;
        if(rde_arr_length(&_c->part_scratch) < 2u * (usize)_pins + 2u) {
            rde_arr_resize(&_c->part_scratch, 2u * (usize)_pins + 2u);
        }
        f64* _volts = (f64*)_c->part_scratch.memory;
        f64* _amps  = _volts + _pins;
        for(u32 _k = 0; _k < _pins; _k++) {
            // (a pin joined to nothing: NaN, no voltage across it to the limits, where 0 V would be one)
            _volts[_k] = _q->node[_k] != FUDE_ZOOM_NONE ? fzc_pv(_x, _q, _k) : RDE_NAN_F64;
            _amps[_k]  = _q->node[_k] != FUDE_ZOOM_NONE ? _q->pin_i[_k] : 0.0;
        }
        fude_zoom_limits_measure(_q->part, _volts, _amps, _pins, _q->measure);
        if(fude_zoom_display_lights(_q->part) > 0u) {
            fzc_display_measure(_x, _q);
        }
        const u8 _m = _q->part->model;
        if((_m == FUDE_ZOOM_MODEL_BATTERY || _m == FUDE_ZOOM_MODEL_VSOURCE || _m == FUDE_ZOOM_MODEL_ACSOURCE) && _q->node[0] != FUDE_ZOOM_NONE &&
           _q->node[0] == _q->node[1]) {
            // A wire straight across it (its two pins one node): a dead short — as much as its own resistance lets go.
            _q->measure[FUDE_ZOOM_LIMIT_CURRENT] = rde_math_abs_f64(_q->value[0]) / (_m == FUDE_ZOOM_MODEL_BATTERY ? 0.5 : 1e-3);
            _q->heat = FZC_OVER_TELL;
        }
        const b8 _burns = fude_zoom_limits_step(_q->part, &_q->limits, _q->measure, _x->dt, &_q->stress, &_q->worst, &_q->heat);
        if(_q->stress > 1.0) {
            _q->worst_seen = rde_math_max_f64(_q->worst_seen, _q->measure[_q->worst]);
        }
        const fude_zoom_circuit_event _e = { _i, 0u, _q->worst, _q->measure[_q->worst], _q->limits.most[_q->worst] };
        if(_burns) {
            _q->burnt = true;
            fude_zoom_circuit_event _b = _e;
            _b.kind    = FUDE_ZOOM_CIRCUIT_BURNT;
            _b.measure = rde_math_max_f64(_q->worst_seen, _e.measure);
            rde_arr_add(&_c->events, (any)&_b);
        } else if(_q->stress > 1.0 && !(fude_zoom_limits_tau(_q->part) > 0.0) && _q->heat >= FZC_OVER_TELL && !_q->told) {
            _q->told = true;
            fude_zoom_circuit_event _o = _e;
            _o.kind = FUDE_ZOOM_CIRCUIT_OVER;
            rde_arr_add(&_c->events, (any)&_o);
        } else if(_q->stress <= 1.0) {
            _q->told = false;
        }
    }
}

// A display's LEDs as the eye sees them (display.h): each one's current against a fully bright one's, held a moment —
// stepping, eased toward it over FUDE_ZOOM_DISPLAY_GLOW; solved for a moment, as it is.
RDE_INTERNAL void fzc_glow(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    f32* _glow = (f32*)fude_zoom_circuit_store(_x->c, _p);
    if(_glow == NULL) {
        return;
    }
    const fzc_diode_k _k = { 0.01, 2.0 * FZC_VT, 0.0, _p->value[0] };
    const f64 _ease = _x->tran ? 1.0 - rde_math_exp_f64(-_x->dt / FUDE_ZOOM_DISPLAY_GLOW) : 1.0;
    const u32 _n = fude_zoom_display_lights(_p->part);
    for(u32 _l = 0; _l < _n; _l++) {
        u32 _a, _b;
        f64 _see = 0.0;
        if(!_p->burnt && fude_zoom_display_led(_p->part, _l, &_a, &_b) && _p->node[_a] != FUDE_ZOOM_NONE && _p->node[_b] != FUDE_ZOOM_NONE) {
            const f64 _v[2] = { fzc_pv(_x, _p, _a), fzc_pv(_x, _p, _b) };
            f64 _i[2];
            fzc_diode_fn(_v, _i, &_k);
            _see = rde_math_min_f64(rde_math_max_f64(_i[0] / FUDE_ZOOM_DISPLAY_FULL, 0.0), 1.5);
        }
        _glow[_l] += (f32)((_see - (f64)_glow[_l]) * _ease);
    }
}

// A display's limits measured LED by LED (a panel's segment pin carries a digit's LED each: what each takes is its own
// current, and how far it is the wrong way round), over fude_zoom_limits_measure's: that one has only the pins'
// currents (a bound for an LED that shares its pins), and reads a pin joined to nothing as 0 V; this one has each
// LED's own current from its model, and leaves out an LED with a pin joined to nothing.
RDE_INTERNAL void fzc_display_measure(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    const fzc_diode_k _k = { 0.01, 2.0 * FZC_VT, 0.0, _p->value[0] };
    f64 _most = 0.0, _back = 0.0;
    const u32 _n = fude_zoom_display_lights(_p->part);
    for(u32 _l = 0; _l < _n; _l++) {
        u32 _a, _b;
        if(fude_zoom_display_led(_p->part, _l, &_a, &_b) && _p->node[_a] != FUDE_ZOOM_NONE && _p->node[_b] != FUDE_ZOOM_NONE) {
            const f64 _v[2] = { fzc_pv(_x, _p, _a), fzc_pv(_x, _p, _b) };
            f64 _i[2];
            fzc_diode_fn(_v, _i, &_k);
            _most = rde_math_max_f64(_most, _i[0]);
            _back = rde_math_max_f64(_back, _v[1] - _v[0]);
        }
    }
    _p->measure[FUDE_ZOOM_LIMIT_CURRENT] = _most;
    _p->measure[FUDE_ZOOM_LIMIT_REVERSE] = _back;
}

// An LM3914 after a step: its outputs' current (ten times what REF OUT gives: 1 to 30 mA); which its comparators light
// — each of its divider's ten steps SIG is over, from RLO up to RHI (a bar: all of them, MODE on V+; a dot: the highest
// alone) — its supply 3 V at least. What changes jumps.
RDE_INTERNAL void fzc_3914(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    _p->state[1] = rde_math_min_f64(rde_math_max_f64(-10.0 * _p->pin_i[6], 1e-3), 0.03);
    const f64 _supply = fzc_over(_x, _p, 2, 1);
    u32 _lit = 0u;
    if(_supply >= 3.0 && !_p->burnt) {
        const f64 _lo = fzc_over(_x, _p, 3, 1), _hi = fzc_over(_x, _p, 5, 1), _sig = fzc_over(_x, _p, 4, 1);
        for(u32 _k = 0; _k < 10u; _k++) {
            if(_sig > _lo + (_hi - _lo) * (f64)(_k + 1u) / 10.0) {
                _lit |= 1u << _k;
            }
        }
        const b8 _bar = _p->node[8] != FUDE_ZOOM_NONE && fzc_over(_x, _p, 8, 1) > _supply - 0.6;
        if(!_bar && _lit != 0u) {
            u32 _top = 0;
            while((_lit >> (_top + 1u)) != 0u) {
                _top++;
            }
            _lit = 1u << _top;
        }
    }
    if((f64)_lit != _p->state[0]) {
        _p->state[0] = (f64)_lit;
        fude_zoom_circuit_jump(_x->c);
    }
}

// A character LCD after a step (display.h): its backlight as bright as its current; with power (2.7 V at least), its
// controller on (as it comes, when it was off), its contrast (VDD over V0: from 3 V, fully at 4.2 V); E fallen while RW
// is low, its data pins read — a byte, or half of one — and RS: a command or a character. Its levels half its supply.
RDE_INTERNAL void fzc_lcd(fzc_ctx* _x, fude_zoom_circuit_part* _p) {
    fude_zoom_lcd* _l = (fude_zoom_lcd*)fude_zoom_circuit_store(_x->c, _p);
    if(_l == NULL) {
        return;
    }
    _p->shown = rde_math_min_f64(rde_math_max_f64(_p->pin_i[14] / 0.015, 0.0), 1.5);
    const f64 _vdd = fzc_over(_x, _p, 1, 0);
    if(_p->burnt || _vdd < 2.7 || _p->node[0] == FUDE_ZOOM_NONE || _p->node[1] == FUDE_ZOOM_NONE) {
        _l->powered  = 0u;
        _p->state[0] = 0.0;
        return;
    }
    if(!_l->powered) {
        fude_zoom_lcd_power_on(_l);
    }
    _p->state[0] = rde_math_min_f64(rde_math_max_f64((_vdd - fzc_over(_x, _p, 2, 0) - 3.0) / 1.2, 0.0), 1.0);
    const f64 _half = 0.5 * _vdd;
    const b8 _e = fzc_over(_x, _p, 5, 0) > _half;
    if(_l->e && !_e && fzc_over(_x, _p, 4, 0) < _half) {
        u8 _pins = 0u;
        for(u32 _b = 0; _b < 8u; _b++) {
            if(fzc_over(_x, _p, 6u + _b, 0) > _half) {
                _pins |= (u8)(1u << _b);
            }
        }
        fude_zoom_lcd_strobe(_l, fzc_over(_x, _p, 3, 0) > _half, _pins);
    }
    _l->e = _e ? 1u : 0u;
}

// After a step: what the parts show (an LED's brightness, a meter's reading) and remember (a capacitor's voltage),
// their limits (stepping), and the next step's size from how far each capacitor and inductor changed. Long: one case
// for each model that keeps or shows something.
void fzc_after(fzc_ctx* _x) {
    fude_zoom_circuit* _c = _x->c;
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    if(_x->tran) {
        fzc_limits(_x);
        _c->step_ratio = 0.0;
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        fude_zoom_circuit_part* _q = &_p[_i];
        switch(_q->part->model) {
        case FUDE_ZOOM_MODEL_CAPACITOR: {
            // (the next step's size: how far its curve bent this step — its current's change over the step, in volts —
            // against what a step should bend it by; and how far it went, against a far looser most. A curve is
            // followed exactly however long the step while it does not bend — an RC's in a few steps a time constant —
            // and a 555's thresholds are landed on all the same)
            const f64 _now = fzc_over(_x, _q, 0, 1);
            if(_x->tran && !fzc_pinned(_c, _q)) {
                const f64 _bent = rde_math_abs_f64(_q->pin_i[0] - _q->state[1]) * _x->dt / rde_math_max_f64(_q->value[0], 1e-15);
                _c->step_ratio = rde_math_max_f64(_c->step_ratio, _bent / (FZC_STEP_DV + 0.02 * rde_math_abs_f64(_now)));
                _c->step_ratio = rde_math_max_f64(_c->step_ratio, rde_math_abs_f64(_now - _q->state[0]) / (FZC_STEP_DV_MOST + 0.1 * rde_math_abs_f64(_now)));
            }
            _q->state[0] = _now;
            _q->state[1] = _x->tran ? _q->pin_i[0] : 0.0;   // (its current: none, solved for a moment)
            break;
        }
        case FUDE_ZOOM_MODEL_INDUCTOR:
            if(_x->tran) {
                _c->step_ratio = rde_math_max_f64(_c->step_ratio, rde_math_abs_f64(_q->pin_i[0] - _q->state[0]) / (FZC_STEP_DI + 0.02 * rde_math_abs_f64(_q->pin_i[0])));
                _q->state[0] = _q->pin_i[0];
                _q->state[1] = fzc_over(_x, _q, 0, 1);
            }
            break;
        case FUDE_ZOOM_MODEL_TIMER555:
            if(_x->tran) {
                fzc_555_ahead(_x, _q);
            }
            break;
        case FUDE_ZOOM_MODEL_SERVO:
            if(_x->tran) {
                fzc_servo(_x, _q);
            }
            break;
        case FUDE_ZOOM_MODEL_SWITCH:
        case FUDE_ZOOM_MODEL_BUTTON:
        case FUDE_ZOOM_MODEL_SPDT:
        case FUDE_ZOOM_MODEL_DPDT:
        case FUDE_ZOOM_MODEL_RELAY:
            if(_x->tran) {
                fzc_sparks(_x, _q, _i);
            }
            _q->shown = _q->part->model == FUDE_ZOOM_MODEL_RELAY ? _q->state[0] : _q->shown;
            break;
        case FUDE_ZOOM_MODEL_SOLENOID:
            if(_x->tran) {
                _c->step_ratio = rde_math_max_f64(_c->step_ratio, rde_math_abs_f64(_q->pin_i[0] - _q->state[0]) / (FZC_STEP_DI + 0.02 * rde_math_abs_f64(_q->pin_i[0])));
                _q->state[0] = _q->pin_i[0];
                _q->state[1] = fzc_over(_x, _q, 0, 1);
                if(_q->state[3] < 0.5) {
                    fzc_plunger(_x, _q);
                }
            }
            _q->shown = _q->state[2];
            break;
        case FUDE_ZOOM_MODEL_STEPPER:
            if(_x->tran) {
                fzc_stepper(_x, _q);
            }
            break;
        case FUDE_ZOOM_MODEL_ENCODER:
            if(_x->tran) {
                fzc_encoder(_x, _q);
            }
            break;
        case FUDE_ZOOM_MODEL_SLOT:
        case FUDE_ZOOM_MODEL_OPTO:      _q->shown = _q->pin_i[2]; break;
        case FUDE_ZOOM_MODEL_SCR:
        case FUDE_ZOOM_MODEL_TRIAC:
            if(_x->tran) {
                fzc_thyristor(_x, _q);
            }
            _q->shown = _q->state[0];
            break;
        case FUDE_ZOOM_MODEL_TRANSFORMER:
            if(_x->tran) {
                for(u32 _w = 0; _w < 2u; _w++) {
                    const f64 _now = _q->pin_i[2u * _w];
                    _c->step_ratio = rde_math_max_f64(_c->step_ratio, rde_math_abs_f64(_now - _q->state[_w]) / (FZC_STEP_DI + 0.02 * rde_math_abs_f64(_now)));
                    _q->state[_w]      = _now;
                    _q->state[2u + _w] = fzc_over(_x, _q, 2u * _w, 2u * _w + 1u);
                }
            }
            break;
        case FUDE_ZOOM_MODEL_SPEAKER:
            // (how loud: its current's swing, held a moment)
            _q->state[0] = rde_math_max_f64(rde_math_abs_f64(_q->pin_i[0]), _q->state[0] * rde_math_exp_f64(-_x->dt / 0.15));
            _q->shown    = _q->state[0];
            break;
        case FUDE_ZOOM_MODEL_LED:       _q->shown = rde_math_min_f64(rde_math_max_f64(_q->pin_i[0] / 0.01, 0.0), 1.5); break;
        case FUDE_ZOOM_MODEL_SEG_PANEL:
        case FUDE_ZOOM_MODEL_LED_MATRIX:
        case FUDE_ZOOM_MODEL_BAR_GRAPH:   fzc_glow(_x, _q); break;
        case FUDE_ZOOM_MODEL_LM3914:      fzc_3914(_x, _q); break;
        case FUDE_ZOOM_MODEL_PANEL_METER: _q->shown = _q->value[1] > 0.5 ? _q->pin_i[0] : fzc_over(_x, _q, 0, 1); break;
        case FUDE_ZOOM_MODEL_CHAR_LCD:    fzc_lcd(_x, _q); break;
        case FUDE_ZOOM_MODEL_SCOPE: {
            // (sampled as it steps: its sweep, its trigger — display.h)
            f32* _smp = (f32*)fude_zoom_circuit_store(_c, _q);
            if(_smp != NULL && (_x->tran || _q->state[1] < -1.5)) {
                const fude_zoom_scope _sc = fude_zoom_display_scope_of(_q);
                fude_zoom_scope_step(&_sc, _q->state, _smp, _x->t, fzc_over(_x, _q, 0, 2), fzc_over(_x, _q, 1, 2), _q->state[1] < -1.5);
            }
            break;
        }
        case FUDE_ZOOM_MODEL_SEVEN_SEG:
            for(u32 _s = 0; _s < 8u; _s++) {
                _q->state[_s] = rde_math_min_f64(rde_math_max_f64(fzc_over(_x, _q, _s, 8) - 1.5, 0.0) / 0.3, 1.0);
            }
            break;
        case FUDE_ZOOM_MODEL_LAMP:      _q->shown = rde_math_abs_f64(fzc_over(_x, _q, 0, 1) * _q->pin_i[0]) / rde_math_max_f64(_q->value[1], 1e-9); break;
        case FUDE_ZOOM_MODEL_MOTOR:     _q->shown += _q->shafted ? _q->spin * _x->dt : _q->pin_i[0] * _x->dt * 60.0; break;
        case FUDE_ZOOM_MODEL_BUZZER:    _q->shown = rde_math_abs_f64(_q->pin_i[0]) > 2e-3 ? 1.0 : 0.0; break;
        case FUDE_ZOOM_MODEL_VOLTMETER: _q->shown = fzc_over(_x, _q, 0, 1); break;
        case FUDE_ZOOM_MODEL_AMMETER:   _q->shown = _q->pin_i[0]; break;
        case FUDE_ZOOM_MODEL_LOGIC_OUT: _q->shown = fude_zoom_logic_probe(_c->logic, _i) == FUDE_SIM_1 ? 1.0 : 0.0; break;
        case FUDE_ZOOM_MODEL_REGULATOR: _q->state[1] = -_q->pin_i[2]; break;
        default: break;
        }
    }
    fzc_sound(_x);
}
