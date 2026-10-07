// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/mechrun.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FZW_STEP      FUDE_ZOOM_MECH_STEP
#define FZW_SUB_STEPS 8u      // (RDE's 4 by itself: twice, for gear trains light and heavy)
#define FZW_SPIN_UP   0.25    // seconds a motor takes to come up to its speed
#define FZW_STIFF     1.0e4f  // a pin's, a slide's stiffness (Hz: as stiff as RDE lets it be — a machine's pins do not give)

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
} fzw_held;

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
        if(_j != NULL && _h[_i].motor) {
            // (the ground's way round: its part turned as the motor says; from rest, brought up to it as it runs)
            rde_physics_2d_joint_enable_motor(_j, 0.0f, 1e9f);
            const fude_zoom_mech_motor _mo = { _j, -_h[_i].speed };
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
                _held[_g].hinge = _j;
                _held[_g].sign  = _e == 0u ? 1.0 : -1.0;
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
        RDE_UNUSED(_c);
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
        rde_physics_2d_joint_create_spring(_w->world, _a, _la, _c, _lb, (f32)(_sp[_i].length * _k), (f32)(2.0 * sqrt(fmax(_sp[_i].stiffness, 0.01))), 0.08f);
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
            rde_physics_2d_joint_create_rope(_w->world, FZW_BODY(_r[_i].a), FZW_END(_r[_i].a, _r[_i].pa), FZW_BODY(_r[_i].b), FZW_END(_r[_i].b, _r[_i].pb), (f32)(_r[_i].length * _k));
        } else if(_i < _r[_i].pair) {
            const fude_zoom_mech_rope* _q = &_r[_r[_i].pair];
            const rde_vec_2F _ga = { (f32)(_r[_i].pb.x * _k), (f32)(_r[_i].pb.y * _k) }, _gb = { (f32)(_q->pb.x * _k), (f32)(_q->pb.y * _k) };
            if(_r[_i].a != FUDE_ZOOM_NONE && _q->a != FUDE_ZOOM_NONE && _pb[_r[_i].a] != NULL && _pb[_q->a] != NULL) {
                rde_physics_2d_joint_create_pulley(_w->world, _pb[_r[_i].a], fzw_local(_w, _r[_i].a, _r[_i].pa), _ga, _pb[_q->a], fzw_local(_w, _q->a, _q->pa), _gb, 1.0f);
            } else {
                // (one side tied off: the other a rope from its rim, as long as the rope less that side)
                const fude_zoom_mech_rope* _free = _r[_i].a != FUDE_ZOOM_NONE ? &_r[_i] : _q;
                const fude_zoom_mech_rope* _tied = _free == &_r[_i] ? _q : &_r[_i];
                if(_free->a != FUDE_ZOOM_NONE && _pb[_free->a] != NULL) {
                    const f64 _left = _r[_i].length + _q->length - hypot(_tied->pa.x - _tied->pb.x, _tied->pa.y - _tied->pb.y);
                    rde_physics_2d_joint_create_rope(_w->world, _pb[_free->a], fzw_local(_w, _free->a, _free->pa), _w->ground,
                                                     (rde_vec_2F){ (f32)(_free->pb.x * _k), (f32)(_free->pb.y * _k) }, (f32)(_left * _k));
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
RDE_INTERNAL void fzw_tick(fude_zoom_mech_world* _w) {
    if(_w->time < FZW_SPIN_UP + FZW_STEP) {
        // (motors coming up to speed: smoothly, from rest)
        const f64 _u = fmin(fmax((_w->time + FZW_STEP) / FZW_SPIN_UP, 0.0), 1.0), _ramp = _u * _u * (3.0 - 2.0 * _u);
        const fude_zoom_mech_motor* _mo = (const fude_zoom_mech_motor*)_w->motors.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->motors); _i++) {
            rde_physics_2d_joint_set_motor_speed(_mo[_i].joint, (f32)(_mo[_i].speed * _ramp));
        }
    }
    rde_physics_2d_world_step(_w->world, (f32)FZW_STEP);   // (hinges, motors, slides, springs, ropes, pulleys, gears: RDE's)
    _w->time += FZW_STEP;
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

void fude_zoom_mech_world_shaft_drive(fude_zoom_mech_world* _w, u32 _shaft, f64 _spin, f64 _torque) {
    if(_w->world == NULL || _shaft >= (u32)rde_arr_length(&_w->shafts)) {
        return;
    }
    rde_physics_2d_joint* _j = ((fude_zoom_mech_shaft*)_w->shafts.memory)[_shaft].joint;
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
