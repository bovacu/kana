// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/mechrun.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FZW_STEP      FUDE_ZOOM_MECH_STEP
#define FZW_SUB_STEPS 8u      // (RDE's 4 by itself: twice, for gear trains light and heavy)
#define FZW_SPIN_UP   0.25    // seconds a motor takes to come up to its speed
#define FZW_STIFF     1.0e4f  // a pin's, a slide's stiffness (Hz: as stiff as RDE lets it be — a machine's pins do not give)
#define FZW_PLASTIC   40.0    // the library's parts' material's strength (MPa: plastic, as a toy's or a printed one's)
#define FZW_GROUND    400.0   // the ground's (steel)
#define FZW_THICK     10.0    // a part's thickness (mm: sim/body.h's plate)
#define FZW_ROPE      2000.0  // a rope's strength (N: a 6 mm rope)
#define FZW_PIN_LEAST 3.0     // a pin's width (mm): six tenths of its thinner part's half width, so much at least…
#define FZW_PIN_MOST  12.0    // …and at most
#define FZW_SETTLE    0.1     // seconds parts take to settle as it starts (drawn a little off): no load counted
#define FZW_OVER      4u      // steps past its strength in a row before it breaks (a 60th of a second)…
#define FZW_SNAP      3.0     // …or at once this far past it
#define FZW_APART     1.5     // a pin's parts this far apart (mm) for…
#define FZW_APART_FOR 0.3     // …this long (s): they cannot fit
#define FZW_HELD_FOR  0.5     // a motor held still this long (s) while it drives: jammed

void fude_zoom_mech_world_init(fude_zoom_mech_world* _w) {
    memset(_w, 0, sizeof(*_w));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_mech_plan_init(&_w->plan);
    _w->bodies     = rde_arr_new(sizeof(rde_physics_2d_body*), _heap);
    _w->inv_mass   = rde_arr_new(sizeof(f64), _heap);
    _w->moves      = rde_arr_new(sizeof(fude_zoom_sim), _heap);
    _w->ropes_were = rde_arr_new(sizeof(f64), _heap);
    _w->motors     = rde_arr_new(sizeof(fude_zoom_mech_motor), _heap);
    _w->shafts     = rde_arr_new(sizeof(fude_zoom_mech_shaft), _heap);
    _w->joints     = rde_arr_new(sizeof(fude_zoom_mech_joint), _heap);
    _w->events     = rde_arr_new(sizeof(fude_zoom_mech_event), _heap);
    _w->k          = 1.0;
}

void fude_zoom_mech_world_stop(fude_zoom_mech_world* _w) {
    if(_w->world != NULL) {
        rde_physics_2d_world_destroy(_w->world);
        _w->world = NULL;
    }
    _w->ground = NULL;
    rde_arr_clear(&_w->bodies);
    rde_arr_clear(&_w->inv_mass);
    rde_arr_clear(&_w->moves);
    rde_arr_clear(&_w->ropes_were);
    rde_arr_clear(&_w->motors);
    rde_arr_clear(&_w->shafts);
    rde_arr_clear(&_w->joints);
    rde_arr_clear(&_w->events);
    _w->time = 0.0;
    _w->left = 0.0;
}

void fude_zoom_mech_world_destroy(fude_zoom_mech_world* _w) {
    fude_zoom_mech_world_stop(_w);
    fude_zoom_mech_plan_destroy(&_w->plan);
    rde_arr_free(&_w->bodies);
    rde_arr_free(&_w->inv_mass);
    rde_arr_free(&_w->moves);
    rde_arr_free(&_w->ropes_were);
    rde_arr_free(&_w->motors);
    rde_arr_free(&_w->shafts);
    rde_arr_free(&_w->joints);
    rde_arr_free(&_w->events);
}

b8 fude_zoom_mech_world_broken(const fude_zoom_mech_world* _w, u8 _kind, u32 _item) {
    const fude_zoom_mech_joint* _j = (const fude_zoom_mech_joint*)_w->joints.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->joints); _i++) {
        if(_j[_i].kind == _kind && _j[_i].item == _item) {
            return _j[_i].joint == NULL;
        }
    }
    return false;
}

b8 fude_zoom_mech_world_on(const fude_zoom_mech_world* _w) {
    return _w->world != NULL;
}

// A plan point (home units) in body _i's own (before its turn), in world units.
// A body's joints as the world is made: its hinge (a gear's: what a gear joint couples), which way its turn counts
// there (+1: it is the hinge's first body), and its slide.
typedef struct {
    rde_physics_2d_joint* hinge;
    f64                   sign;
    rde_physics_2d_joint* slide;
    u32                   hinge_at, slide_at;   // (their places in the world's joints)
} fzw_held;

// A body's material's strength (MPa: the ground's, steel's; the library's parts', plastic's).
RDE_INTERNAL f64 fzw_strength(const fude_zoom_mech_world* _w, u32 _i) {
    if(_i == FUDE_ZOOM_NONE || _i >= (u32)rde_arr_length(&_w->plan.bodies)) {
        return FZW_GROUND;
    }
    const f64 _s = ((const fude_zoom_mech_body*)_w->plan.bodies.memory)[_i].strength;
    return _s > 0.0 ? _s : FZW_PLASTIC;
}

// A joint noted (its place in joints).
RDE_INTERNAL u32 fzw_note(fude_zoom_mech_world* _w, rde_physics_2d_joint* _j, u8 _kind, u32 _item, u32 _a, u32 _b, fude_zoom_v2 _at, f64 _strength) {
    fude_zoom_mech_joint _r;
    memset(&_r, 0, sizeof(_r));
    _r.joint    = _j;
    _r.kind     = _kind;
    _r.item     = _item;
    _r.a        = _a;
    _r.b        = _b;
    _r.at       = _at;
    _r.strength = _strength;
    _r.on[0]    = _r.on[1] = FUDE_ZOOM_NONE;
    rde_arr_add(&_w->joints, (any)&_r);
    return (u32)rde_arr_length(&_w->joints) - 1u;
}

RDE_INTERNAL rde_vec_2F fzw_local(const fude_zoom_mech_world* _w, u32 _i, fude_zoom_v2 _p) {
    const fude_zoom_mech_body* _b = &((const fude_zoom_mech_body*)_w->plan.bodies.memory)[_i];
    // (as it was made: a gear turned its phase more, a rack moved its shift along itself)
    const f64 _a = _b->angle + _b->phase;
    const f64 _dx = _p.x - (_b->at.x + cos(_b->angle) * _b->shift), _dy = _p.y - (_b->at.y + sin(_b->angle) * _b->shift);
    return (rde_vec_2F){ (f32)((_dx * cos(_a) + _dy * sin(_a)) * _w->k), (f32)((-_dx * sin(_a) + _dy * cos(_a)) * _w->k) };
}

fude_zoom_sim fude_zoom_mech_world_move(const fude_zoom_mech_world* _w, u32 _b) {
    return _b < (u32)rde_arr_length(&_w->moves) ? ((const fude_zoom_sim*)_w->moves.memory)[_b] : fude_zoom_sim_identity();
}

fude_zoom_v2 fude_zoom_mech_world_point(const fude_zoom_mech_world* _w, u32 _b, fude_zoom_v2 _p) {
    return _b == FUDE_ZOOM_NONE ? _p : fude_zoom_sim_apply(fude_zoom_mech_world_move(_w, _b), _p);
}

// A drawn body's piece _piece (the plan's) as a polygon's corners (world units, about its body). How many.
RDE_INTERNAL u32 fzw_piece(const fude_zoom_mech_world* _w, u32 _piece, rde_vec_2F* _out) {
    const u32* _cnt = (const u32*)_w->plan.piece_counts.memory;
    const fude_zoom_v2* _pts = (const fude_zoom_v2*)_w->plan.piece_points.memory;
    u32 _first = 0;
    for(u32 _k = 0; _k < _piece; _k++) {
        _first += _cnt[_k];
    }
    const u32 _n = _cnt[_piece] < RDE_PHYSICS_2D_MAX_POLYGON_VERTS ? _cnt[_piece] : RDE_PHYSICS_2D_MAX_POLYGON_VERTS;
    for(u32 _i = 0; _i < _n; _i++) {
        _out[_i] = (rde_vec_2F){ (f32)(_pts[_first + _i].x * _w->k), (f32)(_pts[_first + _i].y * _w->k) };
    }
    return _n;
}

RDE_INTERNAL void fzw_publish(fude_zoom_mech_world* _w);

b8 fude_zoom_mech_world_start(fude_zoom_mech_world* _w, const fude_zoom_mech_plan* _plan) {
    fude_zoom_mech_world_stop(_w);
    fude_zoom_mech_plan_copy(&_w->plan, _plan);
    const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_w->plan.bodies.memory;
    const u32 _nb = (u32)rde_arr_length(&_w->plan.bodies);
    if(_nb == 0u) {
        return false;
    }
    _w->k = 1.0 / fmax(4.0 * _w->plan.unit, 1e-300);
    const f64 _k = _w->k;
    const f64 _g = 9810.0 * _k;   // (gravity: 9.81 m/s² in the home frame's millimetres, in world units)
    _w->world = rde_physics_2d_world_create((rde_vec_2F){ 0.0f, (f32)-_g }, 256u, NULL);
    if(_w->world == NULL) {
        return false;
    }
    rde_physics_2d_world_set_sub_steps(_w->world, FZW_SUB_STEPS);
    rde_physics_2d_world_enable_sleeping(_w->world, false);   // (a machine at rest is not asleep: its circuit may start it)
    rde_physics_2d_shape_def _dot;
    memset(&_dot, 0, sizeof(_dot));
    _dot.type = RDE_PHYSICS_2D_SHAPE_CIRCLE;
    _dot.circle.radius = 0.01f;
    _dot.is_sensor = true;
    _w->ground = rde_physics_2d_body_create(_w->world, RDE_PHYSICS_2D_BODY_TYPE_STATIC, (rde_vec_2F){ 0.0f, 0.0f }, 0.0f, &_dot, 0.0f);
    for(u32 _i = 0; _i < _nb; _i++) {
        rde_physics_2d_body* _body = NULL;
        const u8 _kind = _b[_i].part->kind;
        rde_physics_2d_shape_def _sh;
        memset(&_sh, 0, sizeof(_sh));
        _sh.friction = 0.6f;
        _sh.restitution = 0.2f;
        const f32 _hw = (f32)(_b[_i].hw * _k), _hh = (f32)(_b[_i].hh * _k), _m = fminf(_hw, _hh);
        // (a rack moved its shift along itself: its teeth in its gear's gaps)
        const rde_vec_2F _at = { (f32)((_b[_i].at.x + cos(_b[_i].angle) * _b[_i].shift) * _k), (f32)((_b[_i].at.y + sin(_b[_i].angle) * _b[_i].shift) * _k) };
        f32 _mass = 1.0f;
        RDE_PHYSICS_2D_BODY_TYPE_ _type = RDE_PHYSICS_2D_BODY_TYPE_DYNAMIC;
        b8 _own = true;
        switch(_kind) {
        case FUDE_ZOOM_MECH_LINK:
            _sh.type = RDE_PHYSICS_2D_SHAPE_CAPSULE;
            _sh.capsule.radius = _hh;
            _sh.capsule.half_height = fmaxf(_hw - _hh, 0.01f);
            _sh.offset_angle = (f32)(1.5707963267948966);
            _sh.layer = 1u; _sh.layer_mask = 2u | 4u;
            _mass = 2.0f * _hw * _hh;
            break;
        case FUDE_ZOOM_MECH_PLATE:
            _sh.type = RDE_PHYSICS_2D_SHAPE_POLYGON;
            _sh.polygon.count = 3u;
            for(u32 _v = 0; _v < 3u; _v++) {
                _sh.polygon.verts[_v] = (rde_vec_2F){ _b[_i].part->holes[_v][0] * _hw, _b[_i].part->holes[_v][1] * _hh };
            }
            _sh.layer = 1u; _sh.layer_mask = 2u | 4u;
            _mass = 1.5f * _hw * _hh;
            break;
        case FUDE_ZOOM_MECH_GEAR:
            _sh.type = RDE_PHYSICS_2D_SHAPE_CIRCLE;
            _sh.circle.radius = (f32)(fude_zoom_mech_pitch(_b[_i].part, (f64)_m));
            _sh.layer = 1u; _sh.layer_mask = 4u;
            _mass = 3.0f * _m * _m;
            break;
        case FUDE_ZOOM_MECH_WEIGHT:
        case FUDE_ZOOM_MECH_WHEEL:
            _sh.type = RDE_PHYSICS_2D_SHAPE_CIRCLE;
            _sh.circle.radius = _m * 0.95f;
            _sh.layer = 4u; _sh.layer_mask = 1u | 2u | 4u;
            _mass = _kind == FUDE_ZOOM_MECH_WEIGHT ? (f32)_b[_i].value : 3.0f * _m * _m;
            break;
        case FUDE_ZOOM_MECH_CRATE:
            _sh.type = RDE_PHYSICS_2D_SHAPE_BOX;
            _sh.box.half_extents = (rde_vec_2F){ _hw, _hh };
            _sh.layer = 4u; _sh.layer_mask = 1u | 2u | 4u;
            _mass = (f32)_b[_i].value;
            break;
        case FUDE_ZOOM_MECH_DRAWN:
            // (its first convex piece its shape; the rest added once it is made, its mass theirs together)
            if(_b[_i].pieces == 0u) {
                _own = false;
                break;
            }
            _sh.type = RDE_PHYSICS_2D_SHAPE_POLYGON;
            _sh.polygon.count = fzw_piece(_w, _b[_i].piece, _sh.polygon.verts);
            _sh.friction    = (f32)_b[_i].friction;
            _sh.restitution = (f32)_b[_i].bounce;
            _sh.layer       = _b[_i].fixed ? 2u : 4u;
            _sh.layer_mask  = _b[_i].fixed ? 0u : (1u | 2u | 4u);
            _type = _b[_i].fixed ? RDE_PHYSICS_2D_BODY_TYPE_STATIC : RDE_PHYSICS_2D_BODY_TYPE_DYNAMIC;
            _mass = _b[_i].fixed ? 0.0f : (f32)_b[_i].mass;
            break;
        case FUDE_ZOOM_MECH_SLIDER:
        case FUDE_ZOOM_MECH_RACK:
            // (on its line: it passes over the links pinned to it and the gears on it, as the parts of a linkage do)
            _sh.type = RDE_PHYSICS_2D_SHAPE_BOX;
            _sh.box.half_extents = (rde_vec_2F){ _hw, _hh };
            _sh.layer = 1u; _sh.layer_mask = 2u | 4u;
            _mass = 2.0f * _hw * _hh;
            break;
        case FUDE_ZOOM_MECH_WALL:
            _sh.type = RDE_PHYSICS_2D_SHAPE_BOX;
            _sh.box.half_extents = (rde_vec_2F){ _hw, _hh };
            _sh.layer = 2u; _sh.layer_mask = 0u;
            _type = RDE_PHYSICS_2D_BODY_TYPE_STATIC;
            _mass = 0.0f;
            break;
        default:
            _own = false;   // (a pivot, a motor, a spring, a rope, a pulley: no body of its own)
            break;
        }
        if(_own) {
            // (a gear turned its phase more: its teeth in the gaps it meshes with)
            _body = rde_physics_2d_body_create(_w->world, _type, _at, (f32)(_b[_i].angle + _b[_i].phase), &_sh, fmaxf(_mass, 0.01f));
            for(u32 _k = 1; _kind == FUDE_ZOOM_MECH_DRAWN && _k < _b[_i].pieces; _k++) {
                _sh.polygon.count = fzw_piece(_w, _b[_i].piece + _k, _sh.polygon.verts);
                rde_physics_2d_body_add_shape(_body, &_sh);
            }
        }
        rde_arr_add(&_w->bodies, (any)&_body);
        const f64 _inv = _body != NULL && _type == RDE_PHYSICS_2D_BODY_TYPE_DYNAMIC ? 1.0 / (f64)fmaxf(_mass, 0.01f) : 0.0;
        rde_arr_add(&_w->inv_mass, (any)&_inv);
        const fude_zoom_sim _still = fude_zoom_sim_identity();
        rde_arr_add(&_w->moves, (any)&_still);
    }
    rde_physics_2d_body** _pb = (rde_physics_2d_body**)_w->bodies.memory;
    // Each body's joints (fzw_held).
    rde_arr TYPE(fzw_held) _held_arr = rde_arr_new(sizeof(fzw_held), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_held_arr, _nb);
    fzw_held* _held = (fzw_held*)_held_arr.memory;   // (sized once: it stays put)
    const fude_zoom_mech_hinge* _h = (const fude_zoom_mech_hinge*)_w->plan.hinges.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.hinges); _i++) {
        rde_physics_2d_body* _a = _pb[_h[_i].a];
        if(_a == NULL) {
            continue;
        }
        rde_physics_2d_joint* _j;
        if(_h[_i].b == FUDE_ZOOM_NONE) {
            _j = rde_physics_2d_joint_create_hinge(_w->world, _a, fzw_local(_w, _h[_i].a, _h[_i].at), _w->ground,
                                                   (rde_vec_2F){ (f32)(_h[_i].at.x * _k), (f32)(_h[_i].at.y * _k) });
        } else {
            rde_physics_2d_body* _bb = _pb[_h[_i].b];
            if(_bb == NULL) {
                continue;
            }
            _j = rde_physics_2d_joint_create_hinge(_w->world, _a, fzw_local(_w, _h[_i].a, _h[_i].at), _bb, fzw_local(_w, _h[_i].b, _h[_i].at));
        }
        rde_physics_2d_joint_set_stiffness(_j, FZW_STIFF, 2.0f);
        // (its pin's strength: its weaker part's material in shear — ⅗ of its strength — over a pin six tenths of its
        // thinner part's half width across, 3 to 12 mm)
        const f64 _ha  = _b[_h[_i].a].hh, _hb = _h[_i].b != FUDE_ZOOM_NONE ? _b[_h[_i].b].hh : _ha;
        const f64 _d   = fmin(fmax(0.6 * fmin(_ha, _hb), FZW_PIN_LEAST), FZW_PIN_MOST);
        const u32 _pin = fzw_note(_w, _j, FUDE_ZOOM_MECH_JOINT_PIN, _i, _h[_i].a, _h[_i].b, _h[_i].at,
                                  0.6 * fmin(fzw_strength(_w, _h[_i].a), fzw_strength(_w, _h[_i].b)) * 0.25 * 3.14159265358979 * _d * _d);
        fude_zoom_mech_joint* _pr = &((fude_zoom_mech_joint*)_w->joints.memory)[_pin];
        _pr->la = fzw_local(_w, _h[_i].a, _h[_i].at);
        _pr->lb = _h[_i].b == FUDE_ZOOM_NONE ? (rde_vec_2F){ (f32)(_h[_i].at.x * _k), (f32)(_h[_i].at.y * _k) } : fzw_local(_w, _h[_i].b, _h[_i].at);
        if(_j != NULL && _h[_i].motor) {
            // (the ground's way round: its part turned as the motor says; from rest, brought up to it as it runs)
            const f64 _nm = 1e-3 / _k;   // (a world torque in N·m: this squared)
            rde_physics_2d_joint_enable_motor(_j, 0.0f, (f32)fmin((_h[_i].torque > 0.0 ? _h[_i].torque : 10.0) / (_nm * _nm), 1e9));
            const fude_zoom_mech_motor _mo = { _j, -_h[_i].speed, _pin, 0.0, 0.0, 0.0, 0.0, false };
            rde_arr_add(&_w->motors, (any)&_mo);
        }
        if(_j != NULL && _h[_i].shaft != FUDE_ZOOM_NONE && _h[_i].b == FUDE_ZOOM_NONE) {
            // (a circuit's motor's: free until the circuit drives it)
            const fude_zoom_mech_shaft _sh = { _j, _h[_i].a, _b[_h[_i].shaft].object };
            rde_arr_add(&_w->shafts, (any)&_sh);
        }
        // (a gear's own: to the ground, its axle or its motor, before one to another part)
        const u32 _ends[2] = { _h[_i].a, _h[_i].b };
        for(u32 _e = 0; _e < 2u && _j != NULL; _e++) {
            const u32 _g = _ends[_e];
            if(_g != FUDE_ZOOM_NONE && (_held[_g].hinge == NULL || _h[_i].b == FUDE_ZOOM_NONE)) {
                _held[_g].hinge    = _j;
                _held[_g].sign     = _e == 0u ? 1.0 : -1.0;
                _held[_g].hinge_at = _pin;
            }
        }
    }
    // What slides: along its line, from the ground (its travel limited as the plan says).
    const fude_zoom_mech_slide* _sl = (const fude_zoom_mech_slide*)_w->plan.slides.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.slides); _i++) {
        rde_physics_2d_body* _body = _pb[_sl[_i].body];
        if(_body == NULL) {
            continue;
        }
        const fude_zoom_v2 _c = _b[_sl[_i].body].at;
        _held[_sl[_i].body].slide = rde_physics_2d_joint_create_slider(_w->world, _w->ground, (rde_vec_2F){ (f32)(_sl[_i].at.x * _k), (f32)(_sl[_i].at.y * _k) },
                                                                     _body, fzw_local(_w, _sl[_i].body, _sl[_i].at),
                                                                     (rde_vec_2F){ (f32)_sl[_i].axis.x, (f32)_sl[_i].axis.y },
                                                                     (f32)((_sl[_i].lower - _b[_sl[_i].body].shift) * _k), (f32)((_sl[_i].upper - _b[_sl[_i].body].shift) * _k));   // (from where it starts)
        rde_physics_2d_joint_set_stiffness(_held[_sl[_i].body].slide, FZW_STIFF, 2.0f);
        // (its strength: its material over the part's size and thickness, half of it bearing)
        _held[_sl[_i].body].slide_at = fzw_note(_w, _held[_sl[_i].body].slide, FUDE_ZOOM_MECH_JOINT_SLIDE, _i, _sl[_i].body, FUDE_ZOOM_NONE, _c,
                                                0.5 * fzw_strength(_w, _sl[_i].body) * fmax(_w->plan.unit, 2.0) * FZW_THICK);
    }
    // Springs: Box2D's, their stiffness its frequency (a stiffness of 1: about 2 Hz), a little damped.
    const fude_zoom_mech_spring* _sp = (const fude_zoom_mech_spring*)_w->plan.springs.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.springs); _i++) {
        rde_physics_2d_body* _a = _sp[_i].a != FUDE_ZOOM_NONE ? _pb[_sp[_i].a] : _w->ground;
        rde_physics_2d_body* _c = _sp[_i].b != FUDE_ZOOM_NONE ? _pb[_sp[_i].b] : _w->ground;
        if(_a == NULL || _c == NULL) {
            continue;
        }
        const rde_vec_2F _la = _sp[_i].a != FUDE_ZOOM_NONE ? fzw_local(_w, _sp[_i].a, _sp[_i].pa) : (rde_vec_2F){ (f32)(_sp[_i].pa.x * _k), (f32)(_sp[_i].pa.y * _k) };
        const rde_vec_2F _lb = _sp[_i].b != FUDE_ZOOM_NONE ? fzw_local(_w, _sp[_i].b, _sp[_i].pb) : (rde_vec_2F){ (f32)(_sp[_i].pb.x * _k), (f32)(_sp[_i].pb.y * _k) };
        rde_physics_2d_joint* _js = rde_physics_2d_joint_create_spring(_w->world, _a, _la, _c, _lb, (f32)(_sp[_i].length * _k),
                                                                       (f32)(2.0 * sqrt(fmax(_sp[_i].stiffness, 0.01))), 0.08f);
        // (it gives stretched half again past its length: 2½ times as long as drawn)
        const fude_zoom_v2 _mid = { (_sp[_i].pa.x + _sp[_i].pb.x) * 0.5, (_sp[_i].pa.y + _sp[_i].pb.y) * 0.5 };
        const u32 _sat = fzw_note(_w, _js, FUDE_ZOOM_MECH_JOINT_SPRING, _i, _sp[_i].a, _sp[_i].b, _mid, 1.5 * _sp[_i].length);
        ((fude_zoom_mech_joint*)_w->joints.memory)[_sat].rest = _sp[_i].length;
    }
    // Ropes: RDE's rope (never longer than drawn); two over one pulley, RDE's pulley (one side a fixed end: a rope as
    // long as what is left of it).
    const fude_zoom_mech_rope* _r = (const fude_zoom_mech_rope*)_w->plan.ropes.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.ropes); _i++) {
        const f64 _len = hypot(_r[_i].pb.x - _r[_i].pa.x, _r[_i].pb.y - _r[_i].pa.y);
        rde_arr_add(&_w->ropes_were, (any)&_len);
        #define FZW_END(_body_index, _p) ((_body_index) != FUDE_ZOOM_NONE ? fzw_local(_w, (_body_index), (_p)) : (rde_vec_2F){ (f32)((_p).x * _k), (f32)((_p).y * _k) })
        #define FZW_BODY(_body_index) ((_body_index) != FUDE_ZOOM_NONE ? _pb[(_body_index)] : _w->ground)
        if(_r[_i].pair == FUDE_ZOOM_NONE) {
            if((_r[_i].a == FUDE_ZOOM_NONE && _r[_i].b == FUDE_ZOOM_NONE) || FZW_BODY(_r[_i].a) == NULL || FZW_BODY(_r[_i].b) == NULL) {
                continue;
            }
            fzw_note(_w, rde_physics_2d_joint_create_rope(_w->world, FZW_BODY(_r[_i].a), FZW_END(_r[_i].a, _r[_i].pa), FZW_BODY(_r[_i].b), FZW_END(_r[_i].b, _r[_i].pb),
                                                         (f32)(_r[_i].length * _k)),
                     FUDE_ZOOM_MECH_JOINT_ROPE, _i, _r[_i].a, _r[_i].b, (fude_zoom_v2){ (_r[_i].pa.x + _r[_i].pb.x) * 0.5, (_r[_i].pa.y + _r[_i].pb.y) * 0.5 }, FZW_ROPE);
        } else if(_i < _r[_i].pair) {
            const fude_zoom_mech_rope* _q = &_r[_r[_i].pair];
            const rde_vec_2F _ga = { (f32)(_r[_i].pb.x * _k), (f32)(_r[_i].pb.y * _k) }, _gb = { (f32)(_q->pb.x * _k), (f32)(_q->pb.y * _k) };
            if(_r[_i].a != FUDE_ZOOM_NONE && _q->a != FUDE_ZOOM_NONE && _pb[_r[_i].a] != NULL && _pb[_q->a] != NULL) {
                fzw_note(_w, rde_physics_2d_joint_create_pulley(_w->world, _pb[_r[_i].a], fzw_local(_w, _r[_i].a, _r[_i].pa), _ga, _pb[_q->a], fzw_local(_w, _q->a, _q->pa),
                                                                 _gb, 1.0f),
                         FUDE_ZOOM_MECH_JOINT_ROPE, _i, _r[_i].a, _q->a, _r[_i].pb, FZW_ROPE);
            } else {
                // (one side tied off: the other a rope from its rim, as long as the rope less that side)
                const fude_zoom_mech_rope* _free = _r[_i].a != FUDE_ZOOM_NONE ? &_r[_i] : _q;
                const fude_zoom_mech_rope* _tied = _free == &_r[_i] ? _q : &_r[_i];
                if(_free->a != FUDE_ZOOM_NONE && _pb[_free->a] != NULL) {
                    const f64 _left = _r[_i].length + _q->length - hypot(_tied->pa.x - _tied->pb.x, _tied->pa.y - _tied->pb.y);
                    fzw_note(_w, rde_physics_2d_joint_create_rope(_w->world, _pb[_free->a], fzw_local(_w, _free->a, _free->pa), _w->ground,
                                                                 (rde_vec_2F){ (f32)(_free->pb.x * _k), (f32)(_free->pb.y * _k) }, (f32)(_left * _k)),
                             FUDE_ZOOM_MECH_JOINT_ROPE, _i, _free->a, FUDE_ZOOM_NONE, _free->pb, FZW_ROPE);
                }
            }
        }
        #undef FZW_END
        #undef FZW_BODY
    }
    // Gears meshed: RDE's gear joints on their hinges (a rack's: on its slide). Two gears: turn₁ + (teeth₂ / teeth₁)·turn₂
    // kept; a gear and a rack: as the plan's ratio says (its travel in world units).
    const fude_zoom_mech_mesh* _m = (const fude_zoom_mech_mesh*)_w->plan.meshes.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.meshes); _i++) {
        rde_physics_2d_joint* _ja = _held[_m[_i].a].hinge;
        rde_physics_2d_joint* _jb = _m[_i].rack ? _held[_m[_i].b].slide : _held[_m[_i].b].hinge;
        if(_ja == NULL || _jb == NULL || !(_m[_i].ratio != 0.0)) {
            continue;
        }
        const f64 _ratio = _m[_i].rack ? _held[_m[_i].a].sign * _m[_i].ratio / _k : _held[_m[_i].a].sign * _held[_m[_i].b].sign / _m[_i].ratio;
        rde_physics_2d_joint* _gear = rde_physics_2d_joint_create_gear(_w->world, _ja, _jb, (f32)_ratio);
        // (its teeth's strength, as Lewis has a gear's: its material, its face — the part's thickness —, its module — its
        // pitch circle's width a tooth —, a tooth's form, 0.3; on the pins or slide it rests on)
        const fude_zoom_mech_body* _ga = &_b[_m[_i].a];
        const f64 _rp  = fude_zoom_mech_pitch(_ga->part, fmin(_ga->hw, _ga->hh));
        const f64 _mod = _ga->part != NULL && _ga->part->teeth > 0u ? 2.0 * _rp / (f64)_ga->part->teeth : fmax(_w->plan.unit, 1.0);
        const u32 _gat = fzw_note(_w, _gear, FUDE_ZOOM_MECH_JOINT_GEAR, _i, _m[_i].a, _m[_i].b, _ga->at,
                                  fmin(fzw_strength(_w, _m[_i].a), fzw_strength(_w, _m[_i].b)) * FZW_THICK * _mod * 0.3);
        fude_zoom_mech_joint* _gr = &((fude_zoom_mech_joint*)_w->joints.memory)[_gat];
        _gr->on[0] = _held[_m[_i].a].hinge_at;
        _gr->on[1] = _m[_i].rack ? _held[_m[_i].b].slide_at : _held[_m[_i].b].hinge_at;
        _gr->rest  = _rp;   // (its pitch circle's radius: its torque to its teeth's force)
        if(_m[_i].rack && _gear != NULL) {
            // (a rack: held only while the gear is over its teeth — its slide's travel, world units —, meshing again on a tooth)
            const f64 _from = _b[_m[_i].b].shift;   // (its travel counted from where it starts)
            rde_physics_2d_joint_set_gear_range(_gear, (f32)((_m[_i].lower - _from) * _k), (f32)((_m[_i].upper - _from) * _k));
            rde_physics_2d_joint_set_gear_period(_gear, (f32)_m[_i].period);
        }
    }
    rde_arr_free(&_held_arr);
    _w->time = 0.0;
    _w->left = 0.0;
    fzw_publish(_w);   // (as it starts: its gears turned to mesh already)
    return true;
}

// Each body's move from where it was drawn (home units), a pulley's turn with the rope over it.
RDE_INTERNAL void fzw_publish(fude_zoom_mech_world* _w) {
    const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_w->plan.bodies.memory;
    rde_physics_2d_body** _pb = (rde_physics_2d_body**)_w->bodies.memory;
    fude_zoom_sim* _mv = (fude_zoom_sim*)_w->moves.memory;
    const u32 _nb = (u32)rde_arr_length(&_w->plan.bodies);
    for(u32 _i = 0; _i < _nb; _i++) {
        _mv[_i] = fude_zoom_sim_identity();
        if(_pb[_i] == NULL) {
            continue;
        }
        const rde_vec_2F _p = rde_physics_2d_body_get_position(_pb[_i]);
        const f64 _turn = (f64)rde_physics_2d_body_get_angle(_pb[_i]) - _b[_i].angle;
        const fude_zoom_v2 _now = { (f64)_p.x / _w->k, (f64)_p.y / _w->k };
        const f64 _c = cos(_turn), _s = sin(_turn);
        _mv[_i] = (fude_zoom_sim){ _c, _s, _now.x - (_c * _b[_i].at.x - _s * _b[_i].at.y), _now.y - (_s * _b[_i].at.x + _c * _b[_i].at.y) };
    }
    // (a pulley: turned as far as the rope down its first side has come off it, over its radius)
    const fude_zoom_mech_rope* _r = (const fude_zoom_mech_rope*)_w->plan.ropes.memory;
    const f64* _was = (const f64*)_w->ropes_were.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.ropes); _i++) {
        if(_r[_i].over == FUDE_ZOOM_NONE || _r[_i].pair == FUDE_ZOOM_NONE || _i > _r[_i].pair) {
            continue;
        }
        const fude_zoom_mech_body* _p = &_b[_r[_i].over];
        const f64 _rad = fude_zoom_mech_pulley_radius(_p);
        const fude_zoom_v2 _load = fude_zoom_mech_world_point(_w, _r[_i].a, _r[_i].pa);
        const f64 _l = hypot(_load.x - _r[_i].pb.x, _load.y - _r[_i].pb.y);
        if(!(_l > 0.0) || !(_rad > 0.0)) {
            continue;
        }
        const fude_zoom_v2 _arm = { _r[_i].pb.x - _p->at.x, _r[_i].pb.y - _p->at.y }, _u = { (_load.x - _r[_i].pb.x) / _l, (_load.y - _r[_i].pb.y) / _l };
        const f64 _way = _arm.x * _u.y - _arm.y * _u.x >= 0.0 ? 1.0 : -1.0;
        const f64 _turn = _way * (_l - _was[_i]) / _rad, _c = cos(_turn), _s = sin(_turn);
        _mv[_r[_i].over] = (fude_zoom_sim){ _c, _s, _p->at.x - (_c * _p->at.x - _s * _p->at.y), _p->at.y - (_s * _p->at.x + _c * _p->at.y) };
    }
}

// One step (motors coming up to speed as they start).
// Joint _i broken: what rests on it first (a mesh on its gear's pin), then it — let go; what drove it (a motor, a circuit's
// shaft) driving nothing.
RDE_INTERNAL void fzw_break(fude_zoom_mech_world* _w, u32 _i) {
    fude_zoom_mech_joint* _j = (fude_zoom_mech_joint*)_w->joints.memory;
    const u32 _n = (u32)rde_arr_length(&_w->joints);
    for(u32 _k = 0; _k < _n; _k++) {
        if(_k != _i && _j[_k].joint != NULL && (_j[_k].on[0] == _i || _j[_k].on[1] == _i)) {
            fzw_break(_w, _k);
        }
    }
    rde_physics_2d_joint* _gone = _j[_i].joint;
    if(_gone == NULL) {
        return;
    }
    rde_physics_2d_joint_destroy(_w->world, _gone);
    _j[_i].joint = NULL;
    _j[_i].load  = 0.0;
    fude_zoom_mech_motor* _mo = (fude_zoom_mech_motor*)_w->motors.memory;
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_w->motors); _k++) {
        _mo[_k].joint = _mo[_k].joint == _gone ? NULL : _mo[_k].joint;
    }
    fude_zoom_mech_shaft* _sh = (fude_zoom_mech_shaft*)_w->shafts.memory;
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_w->shafts); _k++) {
        _sh[_k].joint = _sh[_k].joint == _gone ? NULL : _sh[_k].joint;
    }
}

// Each joint against its strength, a step on: what pulls it (a world force in newtons: its masses kilograms, its lengths
// k a millimetre; a mesh's: its torque over its pitch circle's radius; a spring's: its stretch past its length) — past
// its strength, broken; a pin's parts apart, for how long; each motor held still while it drives, for how long. What
// happened: an event.
RDE_INTERNAL void fzw_strain(fude_zoom_mech_world* _w) {
    const f64 _to_n = 1e-3 / _w->k;
    fude_zoom_mech_joint* _j = (fude_zoom_mech_joint*)_w->joints.memory;
    const u32 _n = (u32)rde_arr_length(&_w->joints);
    for(u32 _i = 0; _i < _n; _i++) {
        fude_zoom_mech_joint* _r = &_j[_i];
        if(_r->joint == NULL) {
            continue;
        }
        switch(_r->kind) {
        case FUDE_ZOOM_MECH_JOINT_GEAR:
            _r->force = fabs((f64)rde_physics_2d_joint_get_torque(_r->joint)) * _to_n * _to_n / fmax(_r->rest * 1e-3, 1e-9);
            break;
        case FUDE_ZOOM_MECH_JOINT_SPRING:
            _r->force = fmax((f64)rde_physics_2d_joint_get_coordinate(_r->joint) / _w->k - _r->rest, 0.0);
            break;
        default:
            _r->force = (f64)rde_physics_2d_joint_get_force(_r->joint) * _to_n;
            break;
        }
        _r->load = _r->strength > 0.0 ? _r->force / _r->strength : 0.0;
        _r->over = _r->load > 1.0 && _w->time >= FZW_SETTLE ? _r->over + 1u : 0u;
        if(_r->over >= FZW_OVER || (_r->over > 0u && _r->load > FZW_SNAP)) {
            const fude_zoom_mech_event _e = { FUDE_ZOOM_MECH_BROKE, _i, _r->force, _r->strength };
            rde_arr_add(&_w->events, (any)&_e);
            fzw_break(_w, _i);
            _j = (fude_zoom_mech_joint*)_w->joints.memory;
            continue;
        }
        if(_r->kind == FUDE_ZOOM_MECH_JOINT_PIN) {
            const f64 _sep = (f64)rde_physics_2d_joint_get_separation(_r->joint) / _w->k;
            if(_sep > FZW_APART) {
                if(_r->apart >= 0.0) {
                    _r->apart += FZW_STEP;
                    if(_r->apart >= FZW_APART_FOR) {
                        const fude_zoom_mech_event _e = { FUDE_ZOOM_MECH_MISFIT, _i, _sep, FZW_APART };
                        rde_arr_add(&_w->events, (any)&_e);
                        _r->apart = -1.0;   // (said: until they fit again)
                    }
                }
            } else if(_sep < 0.5 * FZW_APART) {
                _r->apart = 0.0;
            }
        }
    }
    // Motors: how far each has turned (its angle's steps added up: RDE's angle within a half turn either way), and each
    // half second, once up to speed, how far on it got against how far it drives — a tenth of it: jammed (rocking against
    // what holds it back is not turning).
    fude_zoom_mech_motor* _mo = (fude_zoom_mech_motor*)_w->motors.memory;
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_w->motors); _k++) {
        if(_mo[_k].joint == NULL) {
            continue;
        }
        const f64 _now = (f64)rde_physics_2d_joint_get_coordinate(_mo[_k].joint);
        f64 _d = _now - _mo[_k].was;
        _d -= 2.0 * 3.14159265358979323846 * floor((_d + 3.14159265358979323846) / (2.0 * 3.14159265358979323846));
        _mo[_k].was     = _now;
        _mo[_k].turned += _d;
        if(_w->time < FZW_SPIN_UP || !(fabs(_mo[_k].speed) > 0.2)) {
            _mo[_k].from = _mo[_k].turned;
            continue;
        }
        _mo[_k].held += FZW_STEP;
        if(_mo[_k].held >= FZW_HELD_FOR) {
            const f64 _got = fabs(_mo[_k].turned - _mo[_k].from), _want = fabs(_mo[_k].speed) * _mo[_k].held;
            if(_got < 0.1 * _want && !_mo[_k].jammed) {
                const fude_zoom_mech_event _e = { FUDE_ZOOM_MECH_JAMMED, _mo[_k].pin, _got / _mo[_k].held, fabs(_mo[_k].speed) };
                rde_arr_add(&_w->events, (any)&_e);
                _mo[_k].jammed = true;
            } else if(_got > 0.5 * _want) {
                _mo[_k].jammed = false;   // (turning again)
            }
            _mo[_k].from = _mo[_k].turned;
            _mo[_k].held = 0.0;
        }
    }
}

RDE_INTERNAL void fzw_tick(fude_zoom_mech_world* _w) {
    if(_w->time < FZW_SPIN_UP + FZW_STEP) {
        // (motors coming up to speed: smoothly, from rest)
        const f64 _u = fmin(fmax((_w->time + FZW_STEP) / FZW_SPIN_UP, 0.0), 1.0), _ramp = _u * _u * (3.0 - 2.0 * _u);
        const fude_zoom_mech_motor* _mo = (const fude_zoom_mech_motor*)_w->motors.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->motors); _i++) {
            if(_mo[_i].joint != NULL) {
                rde_physics_2d_joint_set_motor_speed(_mo[_i].joint, (f32)(_mo[_i].speed * _ramp));
            }
        }
    }
    rde_physics_2d_world_step(_w->world, (f32)FZW_STEP);   // (hinges, motors, slides, springs, ropes, pulleys, gears: RDE's)
    _w->time += FZW_STEP;
    fzw_strain(_w);
}

void fude_zoom_mech_world_step(fude_zoom_mech_world* _w, f64 _dt) {
    if(_w->world == NULL) {
        return;
    }
    _w->left += fmin(fmax(_dt, 0.0), 0.1);
    while(_w->left >= FZW_STEP) {
        _w->left -= FZW_STEP;
        fzw_tick(_w);
    }
    fzw_publish(_w);
}

void fude_zoom_mech_world_tick(fude_zoom_mech_world* _w) {
    if(_w->world == NULL) {
        return;
    }
    fzw_tick(_w);
    fzw_publish(_w);
}

u32 fude_zoom_mech_world_shaft(const fude_zoom_mech_world* _w, u32 _object) {
    const fude_zoom_mech_shaft* _sh = (const fude_zoom_mech_shaft*)_w->shafts.memory;
    for(u32 _i = 0; _w->world != NULL && _i < (u32)rde_arr_length(&_w->shafts); _i++) {
        if(_sh[_i].object == _object) {
            return _i;
        }
    }
    return FUDE_ZOOM_NONE;
}

f64 fude_zoom_mech_world_shaft_spin(const fude_zoom_mech_world* _w, u32 _shaft) {
    if(_w->world == NULL || _shaft >= (u32)rde_arr_length(&_w->shafts)) {
        return 0.0;
    }
    const fude_zoom_mech_shaft* _sh = &((const fude_zoom_mech_shaft*)_w->shafts.memory)[_shaft];
    rde_physics_2d_body* const* _pb = (rde_physics_2d_body* const*)_w->bodies.memory;
    return _pb[_sh->body] != NULL ? (f64)rde_physics_2d_body_get_angular_velocity(_pb[_sh->body]) : 0.0;
}

f64 fude_zoom_mech_world_shaft_angle(const fude_zoom_mech_world* _w, u32 _shaft) {
    if(_w->world == NULL || _shaft >= (u32)rde_arr_length(&_w->shafts)) {
        return 0.0;
    }
    const fude_zoom_mech_shaft* _sh = &((const fude_zoom_mech_shaft*)_w->shafts.memory)[_shaft];
    rde_physics_2d_body* const* _pb = (rde_physics_2d_body* const*)_w->bodies.memory;
    return _pb[_sh->body] != NULL ? (f64)rde_physics_2d_body_get_angle(_pb[_sh->body]) : 0.0;
}

void fude_zoom_mech_world_shaft_drive(fude_zoom_mech_world* _w, u32 _shaft, f64 _spin, f64 _torque) {
    if(_w->world == NULL || _shaft >= (u32)rde_arr_length(&_w->shafts)) {
        return;
    }
    rde_physics_2d_joint* _j = ((fude_zoom_mech_shaft*)_w->shafts.memory)[_shaft].joint;
    if(_j == NULL) {
        return;   // (its pin broken: it drives nothing)
    }
    if(!(_torque > 0.0)) {
        rde_physics_2d_joint_disable_motor(_j);
        return;
    }
    // (the ground's way round, as a drive motor's: the ground's turn against its part's)
    rde_physics_2d_joint_enable_motor(_j, (f32)-_spin, (f32)fmin(_torque * fude_zoom_mech_world_torque_unit(_w), 1e9));
}

f64 fude_zoom_mech_world_torque_unit(const fude_zoom_mech_world* _w) {
    return 9810.0 * _w->k;   // (its gravity, in world units: a unit of mass at a unit of length)
}

b8 fude_zoom_mech_world_covers(const fude_zoom_mech_world* _w, fude_zoom_v2 _at) {
    if(_w->world == NULL) {
        return false;
    }
    rde_physics_2d_overlap_result _hit[16];
    const u32 _n = rde_physics_2d_world_point_query(_w->world, (rde_vec_2F){ (f32)(_at.x * _w->k), (f32)(_at.y * _w->k) }, _hit, 16u);
    rde_physics_2d_body* const* _pb = (rde_physics_2d_body* const*)_w->bodies.memory;
    const f64* _inv = (const f64*)_w->inv_mass.memory;
    for(u32 _h = 0; _h < _n && _h < 16u; _h++) {
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->bodies); _i++) {
            if(_pb[_i] == _hit[_h].body && _inv[_i] > 0.0) {
                return true;   // (one of its parts that move: not the ground, a wall, a fixed drawing)
            }
        }
    }
    return false;
}
