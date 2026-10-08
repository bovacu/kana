// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/coupling.h"
#include <math.h>
#include <string.h>

#define FZK_PI 3.14159265358979323846
#define FZK_STEPS_MOST 200u   // the circuit's steps in one of the mechanism's, at most (short of them: slow motion)
#define FZK_SERVO_GAIN 25.0   // a servo's speed a radian off where it is going (a second's 25th to get there, at most its top)
#define FZK_SERVO_LEAST 3.5   // a servo's supply at least for it to work (circuit.c's)

void fude_zoom_coupling_init(fude_zoom_coupling* _k) {
    memset(_k, 0, sizeof(*_k));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _k->motors  = rde_arr_new(sizeof(fude_zoom_coupled_motor), _heap);
    _k->servos  = rde_arr_new(sizeof(fude_zoom_coupled_servo), _heap);
    _k->buttons = rde_arr_new(sizeof(fude_zoom_coupled_button), _heap);
}

void fude_zoom_coupling_destroy(fude_zoom_coupling* _k) {
    rde_arr_free(&_k->motors);
    rde_arr_free(&_k->servos);
    rde_arr_free(&_k->buttons);
}

RDE_INTERNAL fude_zoom_circuit_part* fzk_part(fude_zoom_circuit* _c, u32 _i) {
    return _i < (u32)rde_arr_length(&_c->parts) ? &((fude_zoom_circuit_part*)_c->parts.memory)[_i] : NULL;
}

void fude_zoom_coupling_clear(fude_zoom_coupling* _k, fude_zoom_circuit* _c) {
    for(u32 _i = 0; _c != NULL && _i < (u32)rde_arr_length(&_c->parts); _i++) {
        fude_zoom_circuit_part* _p = fzk_part(_c, _i);
        _p->shafted = false;
        _p->spin    = 0.0;
        _p->pushed  = false;
        if(_p->part->model == FUDE_ZOOM_MODEL_SERVO) {
            _p->state[7] = -1.0;   // (turning itself)
        }
    }
    rde_arr_clear(&_k->motors);
    rde_arr_clear(&_k->servos);
    rde_arr_clear(&_k->buttons);
    _k->left = 0.0;
}

u32 fude_zoom_coupling_build(fude_zoom_coupling* _k, fude_zoom_circuit* _c, const fude_zoom_mech_world* _w, const fude_zoom_scene* _s) {
    fude_zoom_coupling_clear(_k, _c);
    if(!fude_zoom_mech_world_on(_w)) {
        return 0u;
    }
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        fude_zoom_circuit_part* _p = fzk_part(_c, _i);
        if(_p->part->model == FUDE_ZOOM_MODEL_MOTOR) {
            const u32 _shaft = fude_zoom_mech_world_shaft(_w, _p->object);
            if(_shaft != FUDE_ZOOM_NONE) {
                const fude_zoom_coupled_motor _m = { _i, _shaft };
                rde_arr_add(&_k->motors, (any)&_m);
                _p->shafted = true;
                _p->spin    = fude_zoom_mech_world_shaft_spin(_w, _shaft);
            }
        } else if(_p->part->model == FUDE_ZOOM_MODEL_SERVO) {
            const u32 _shaft = fude_zoom_mech_world_shaft(_w, _p->object);
            if(_shaft != FUDE_ZOOM_NONE) {
                const fude_zoom_coupled_servo _sv = { _i, _shaft, _p->state[0] * FZK_PI / 180.0 - fude_zoom_mech_world_shaft_angle(_w, _shaft) };
                rde_arr_add(&_k->servos, (any)&_sv);
                _p->state[7] = 0.0;
            }
        } else if(_p->part->model == FUDE_ZOOM_MODEL_BUTTON && _p->object < fude_zoom_scene_object_count(_s)) {
            // (its middle in the home frame, where the world's parts are)
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _p->object);
            const fude_zoom_sim _up = fude_zoom_sim_compose(fude_zoom_scene_sim(_s, _o->frame, _home), fude_zoom_object_sim(_o));
            const fude_zoom_coupled_button _b = { _i, fude_zoom_sim_apply(_up, (fude_zoom_v2){ 0.0, 0.0 }) };
            rde_arr_add(&_k->buttons, (any)&_b);
        }
    }
    return (u32)(rde_arr_length(&_k->motors) + rde_arr_length(&_k->servos) + rde_arr_length(&_k->buttons));
}

// Each shafted motor's drive, as the circuit loads it now: toward the speed at which nothing flows through it, with the
// torque the current at the speed it has makes.
RDE_INTERNAL void fzk_drive(fude_zoom_coupling* _k, fude_zoom_circuit* _c, fude_zoom_mech_world* _w) {
    const fude_zoom_coupled_motor* _m = (const fude_zoom_coupled_motor*)_k->motors.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_k->motors); _i++) {
        fude_zoom_circuit_part* _p = fzk_part(_c, _m[_i].part);
        f64 _g = 0.0, _still = 0.0;
        if(_p == NULL || !fude_zoom_circuit_motor_load(_c, _m[_i].part, &_g, &_still) || !(_g > 0.0)) {
            fude_zoom_mech_world_shaft_drive(_w, _m[_i].shaft, 0.0, 0.0);   // (nothing through it: free)
            continue;
        }
        const f64 _ke    = fude_zoom_motor_k(_p);
        const f64 _rated = _p->value[0] > 0.0 ? _p->value[0] : 6.0;
        const f64 _spin  = fude_zoom_mech_world_shaft_spin(_w, _m[_i].shaft);
        const f64 _to    = _still / _ke;
        // (its torque: STALL at its rated volts held still — the current then, V / R —, as the current is)
        const f64 _per_amp = FUDE_ZOOM_COUPLING_STALL * FUDE_ZOOM_MOTOR_R / _rated;
        fude_zoom_mech_world_shaft_drive(_w, _m[_i].shaft, _to, _per_amp * _g * _ke * fabs(_to - _spin));
    }
}

// Each servo on a shaft driven toward where its pulses say, as fast as its supply lets it, with its stall torque at most
// — how hard it strains then (off where it should be, and not getting there) for what it draws; no pulses or no supply:
// free.
RDE_INTERNAL void fzk_servos(fude_zoom_coupling* _k, fude_zoom_circuit* _c, fude_zoom_mech_world* _w) {
    const fude_zoom_coupled_servo* _sv = (const fude_zoom_coupled_servo*)_k->servos.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_k->servos); _i++) {
        fude_zoom_circuit_part* _p = fzk_part(_c, _sv[_i].part);
        if(_p == NULL) {
            continue;
        }
        const f64 _vcc = fude_zoom_circuit_volts(_c, _p->node[1]) - fude_zoom_circuit_volts(_c, _p->node[0]);
        if(_p->burnt || _p->state[1] < 0.0 || _vcc < FZK_SERVO_LEAST) {
            fude_zoom_mech_world_shaft_drive(_w, _sv[_i].shaft, 0.0, 0.0);
            _p->state[7] = 0.0;
            continue;
        }
        const f64 _top   = _p->value[1] * fmin(_vcc / 4.8, 1.3);
        const f64 _theta = fude_zoom_mech_world_shaft_angle(_w, _sv[_i].shaft) + _sv[_i].offset;
        const f64 _off   = _p->state[1] * FZK_PI / 180.0 - _theta;
        fude_zoom_mech_world_shaft_drive(_w, _sv[_i].shaft, fmin(fmax(_off * FZK_SERVO_GAIN, -_top), _top), _p->value[0]);
        const f64 _going = fabs(fude_zoom_mech_world_shaft_spin(_w, _sv[_i].shaft));
        _p->state[7] = fmin(fabs(_off) / (10.0 * FZK_PI / 180.0), 1.0) * (1.0 - fmin(_going / fmax(_top, 1e-9), 1.0));
    }
}

b8 fude_zoom_coupling_step(fude_zoom_coupling* _k, fude_zoom_circuit* _c, fude_zoom_mech_world* _w, f64 _dt) {
    if(!fude_zoom_mech_world_on(_w)) {
        return true;
    }
    _k->left += fmin(fmax(_dt, 0.0), 0.1);
    b8 _ok = true, _ran = false;
    while(_k->left >= FUDE_ZOOM_MECH_STEP && _ok) {
        _k->left -= FUDE_ZOOM_MECH_STEP;
        // The mechanism into the circuit: its shafts' speeds, its buttons pressed.
        const fude_zoom_coupled_motor* _m = (const fude_zoom_coupled_motor*)_k->motors.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_k->motors); _i++) {
            fude_zoom_circuit_part* _p = fzk_part(_c, _m[_i].part);
            if(_p != NULL) {
                _p->shafted = true;
                _p->spin    = fude_zoom_mech_world_shaft_spin(_w, _m[_i].shaft);
            }
        }
        const fude_zoom_coupled_servo* _sv = (const fude_zoom_coupled_servo*)_k->servos.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_k->servos); _i++) {
            fude_zoom_circuit_part* _p = fzk_part(_c, _sv[_i].part);
            if(_p != NULL) {
                _p->state[0] = (fude_zoom_mech_world_shaft_angle(_w, _sv[_i].shaft) + _sv[_i].offset) * 180.0 / FZK_PI;   // (its horn: where its part is)
                _p->shown    = _p->state[0];
            }
        }
        const fude_zoom_coupled_button* _b = (const fude_zoom_coupled_button*)_k->buttons.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_k->buttons); _i++) {
            fude_zoom_circuit_part* _p = fzk_part(_c, _b[_i].part);
            if(_p != NULL) {
                const b8 _was = _p->pushed;
                _p->pushed = fude_zoom_mech_world_covers(_w, _b[_i].at);
                if(_p->pushed != _was) {
                    fude_zoom_circuit_jump(_c);
                }
            }
        }
        // The circuit as long, in its own steps (as fine as what happens in it needs).
        if(rde_arr_length(&_c->parts) > 0u) {
            _ok  = fude_zoom_circuit_advance(_c, FUDE_ZOOM_MECH_STEP, FZK_STEPS_MOST, false);
            _ran = true;
        }
        // The circuit into the mechanism: its motors' drives; then the mechanism a step on.
        if(_ok) {
            fzk_drive(_k, _c, _w);
            fzk_servos(_k, _c, _w);
        }
        fude_zoom_mech_world_tick(_w);
    }
    if(_ok && _ran) {
        fude_zoom_circuit_steps(_c, 0u, true);   // (its wires' currents, once: what is drawn)
    }
    return _ok;
}
