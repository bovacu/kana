// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/mechrun.h"
#include "zoom/circuit.h"

// ===========================================================================
// See mechrun.h.
//
// In order: the constants; the world's lifetime and its tracers' trails; making the world from the plan
// (fude_zoom_mech_world_start: the bodies, then the hinges and motors, slides, plungers, springs, ropes and gear
// meshes); what it hands out (fzw_publish); breaking and straining joints; the hand; what presses on its own; the step;
// then the questions coupling.h and the page ask. File-local names start with fzw_.
//
// The bodies' shapes are on collision layers (RDE's: a pair collides when each one's mask takes the other's layer; a
// mask of 0 takes every layer): 1, the parts of a linkage (links, plates, sliders, racks, gears, cranks, sprockets,
// ratchets' hubs, followers' rods, pawls' levers, plungers), which pass over one another as a linkage's parts do;
// 2, what is fixed (walls, fixed drawings); 4, what is free (weights, wheels, crates, drawings that move), which hits
// all of them; and 8, what bears on one another (cams, followers' rollers, ratchets' teeth, pawls' points).
// ===========================================================================

#define FZW_STEP      FUDE_ZOOM_MECH_STEP   // One step (seconds).
#define FZW_SUB_STEPS 8u      // Sub-steps a step: RDE's 4 by itself, twice, for gear trains light and heavy.
#define FZW_SPIN_UP   0.25    // Seconds a motor takes to come up to its speed.
// A pin's and a slide's stiffness (Hz): as stiff as RDE lets it be, since a machine's pins do not give.
#define FZW_STIFF     1.0e4f
#define FZW_PLASTIC   40.0    // The library's parts' material's strength (MPa: plastic, as a toy's or a printed one's).
#define FZW_GROUND    400.0   // The ground's (MPa: steel).
#define FZW_THICK     10.0    // A part's thickness (mm: sim/body.h's plate).
#define FZW_ROPE      2000.0  // A rope's strength (N: a 6 mm rope).
#define FZW_PIN_LEAST 3.0     // A pin's width (mm): six tenths of its thinner part's half width, so much at least,
#define FZW_PIN_MOST  12.0    // and at most.
#define FZW_SETTLE    0.1     // Seconds parts take to settle as it starts (drawn a little off): no load counted.
#define FZW_OVER      4u      // Steps past its strength in a row before it breaks (a 60th of a second),
#define FZW_SNAP      3.0     // or at once this many times past it.
#define FZW_APART     1.5     // A pin's parts this far apart (mm)
#define FZW_APART_FOR 0.3     // for this long (s): they cannot fit.
#define FZW_HELD_FOR  0.5     // A motor held still this long (s) while it drives: jammed.

// See the header. Every array goes on the std allocator; k starts at 1 until a plan sets it.
void fude_zoom_mech_world_init(fude_zoom_mech_world* _w) {
    rde_memset(_w, 0, sizeof(*_w));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_mech_plan_init(&_w->plan);
    _w->bodies     = rde_arr_new(sizeof(rde_physics_2d_body*), _heap);
    _w->inv_mass   = rde_arr_new(sizeof(f64), _heap);
    _w->moves      = rde_arr_new(sizeof(fude_zoom_sim), _heap);
    _w->ropes_were = rde_arr_new(sizeof(f64), _heap);
    _w->motors     = rde_arr_new(sizeof(fude_zoom_mech_motor), _heap);
    _w->shafts     = rde_arr_new(sizeof(fude_zoom_mech_shaft), _heap);
    _w->cranks     = rde_arr_new(sizeof(fude_zoom_mech_crank), _heap);
    _w->plungers   = rde_arr_new(sizeof(fude_zoom_mech_plunger), _heap);
    _w->turned     = rde_arr_new(sizeof(f64), _heap);
    _w->turned_was = rde_arr_new(sizeof(f64), _heap);
    _w->grab       = FUDE_ZOOM_NONE;
    _w->joints     = rde_arr_new(sizeof(fude_zoom_mech_joint), _heap);
    _w->events     = rde_arr_new(sizeof(fude_zoom_mech_event), _heap);
    _w->trails     = rde_arr_new(sizeof(rde_arr), _heap);
    _w->trail_last = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    _w->k          = 1.0;
}

// See the header. Destroys RDE's world (its bodies and joints with it) and empties every array, keeping their memory;
// the plan's copy is kept until the next start.
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
    rde_arr_clear(&_w->cranks);
    rde_arr_clear(&_w->plungers);
    rde_arr_clear(&_w->turned);
    rde_arr_clear(&_w->turned_was);
    _w->grab = FUDE_ZOOM_NONE;
    rde_arr_clear(&_w->joints);
    rde_arr_clear(&_w->events);
    rde_arr* _paths = (rde_arr*)_w->trails.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->trails); _i++) {
        rde_arr_clear(&_paths[_i]);   // (each tracer's: its memory kept for the next run)
    }
    _w->trail_points = 0;
    rde_arr_clear(&_w->trail_last);
    _w->time = 0.0;
    _w->left = 0.0;
}

// See the header.
void fude_zoom_mech_world_destroy(fude_zoom_mech_world* _w) {
    fude_zoom_mech_world_stop(_w);
    fude_zoom_mech_plan_destroy(&_w->plan);
    rde_arr_free(&_w->bodies);
    rde_arr_free(&_w->inv_mass);
    rde_arr_free(&_w->moves);
    rde_arr_free(&_w->ropes_were);
    rde_arr_free(&_w->motors);
    rde_arr_free(&_w->shafts);
    rde_arr_free(&_w->cranks);
    rde_arr_free(&_w->plungers);
    rde_arr_free(&_w->turned);
    rde_arr_free(&_w->turned_was);
    rde_arr_free(&_w->joints);
    rde_arr_free(&_w->events);
    rde_arr* _paths = (rde_arr*)_w->trails.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->trails); _i++) {
        rde_arr_free(&_paths[_i]);
    }
    rde_arr_free(&_w->trails);
    rde_arr_free(&_w->trail_last);
}

// See the header. Tracer _t's own array, as it is (none yet: no points).
const fude_zoom_v2* fude_zoom_mech_world_trail(const fude_zoom_mech_world* _w, u32 _t, u32* _count) {
    *_count = 0u;
    if(_t >= (u32)rde_arr_length(&_w->trails)) {
        return NULL;
    }
    const rde_arr* _path = &((const rde_arr*)_w->trails.memory)[_t];
    *_count = (u32)rde_arr_length(_path);
    return *_count > 0u ? (const fude_zoom_v2*)_path->memory : NULL;
}

// See the header. The points left out and the stretches cut as the renderer does a pen's stroke (render_stroke.c): a
// point under a fraction of a pixel from the last adds nothing seen; a segment is near when its box, grown by the
// stroke's half width, touches the screen's grown by 2 — that is, unless both its ends are past the same side by more
// than the half width (compared as such: no maths call for each of up to FUDE_ZOOM_MECH_TRAIL_MOST segments a frame).
// A stretch starts at a near segment's first point and ends at the last near one's end, so it begins and ends out of
// sight (its round caps unseen) unless the path does.
u32 fude_zoom_mech_trail_runs(const fude_zoom_v2* _path, u32 _count, fude_zoom_sim _to_screen, fude_zoom_v2 _half, f32 _radius, rde_arr* _points, rde_arr* _runs) {
    rde_arr_clear(_points);
    rde_arr_clear(_runs);
    if(_count < 2u) {
        return 0u;
    }
    rde_vec_2F* _p = (rde_vec_2F*)rde_arr_add_n(_points, _count);
    u32 _n = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        const fude_zoom_v2 _s = fude_zoom_sim_apply(_to_screen, _path[_i]);
        const rde_vec_2F _q = { (f32)_s.x, (f32)_s.y };
        if(_n > 0u && _i + 1u < _count) {
            const f32 _dx = _q.x - _p[_n - 1u].x, _dy = _q.y - _p[_n - 1u].y;
            if(_dx * _dx + _dy * _dy < FUDE_ZOOM_MECH_TRAIL_STEP * FUDE_ZOOM_MECH_TRAIL_STEP) {
                continue;
            }
        }
        _p[_n++] = _q;
    }
    rde_arr_resize(_points, _n);
    const f32 _hx = (f32)_half.x + 2.0f, _hy = (f32)_half.y + 2.0f;
    u32 _start = FUDE_ZOOM_NONE;
    for(u32 _i = 0; _i < _n; _i++) {
        const rde_vec_2F _a = _p[_i], _b = _p[_i + 1u < _n ? _i + 1u : _i];
        const b8 _near = _i + 1u < _n && !(_a.x - _radius > _hx && _b.x - _radius > _hx) && !(_a.x + _radius < -_hx && _b.x + _radius < -_hx) &&
                         !(_a.y - _radius > _hy && _b.y - _radius > _hy) && !(_a.y + _radius < -_hy && _b.y + _radius < -_hy);
        if(_near && _start == FUDE_ZOOM_NONE) {
            _start = _i;
        } else if(!_near && _start != FUDE_ZOOM_NONE) {
            const u32 _run[2] = { _start, _i - _start + 1u };
            rde_memcpy(rde_arr_add_n(_runs, 2u), _run, sizeof(_run));
            _start = FUDE_ZOOM_NONE;
        }
    }
    return (u32)rde_arr_length(_runs) / 2u;
}

// Keeps each tracer's point where it is now when it has moved a tenth of the plan's unit from where it was last kept,
// until the trail holds FUDE_ZOOM_MECH_TRAIL_MOST points.
RDE_INTERNAL void fzw_trace(fude_zoom_mech_world* _w) {
    const fude_zoom_mech_tracer* _t = (const fude_zoom_mech_tracer*)_w->plan.tracers.memory;
    const u32 _nt = (u32)rde_arr_length(&_w->plan.tracers);
    while((u32)rde_arr_length(&_w->trail_last) < _nt) {
        const fude_zoom_v2 _none = { RDE_NAN_F64, RDE_NAN_F64 };
        rde_arr_add(&_w->trail_last, (any)&_none);
    }
    while((u32)rde_arr_length(&_w->trails) < _nt) {   // (a tracer's array, the first time there is one)
        const rde_arr _path = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
        rde_arr_add(&_w->trails, (any)&_path);
    }
    fude_zoom_v2* _last = (fude_zoom_v2*)_w->trail_last.memory;
    rde_arr* _paths = (rde_arr*)_w->trails.memory;
    const f64 _apart = 0.1 * _w->plan.unit;
    for(u32 _i = 0; _i < _nt && _w->trail_points < FUDE_ZOOM_MECH_TRAIL_MOST; _i++) {
        const fude_zoom_v2 _p = fude_zoom_mech_world_point(_w, _t[_i].on, _t[_i].at);
        if(!rde_math_is_nan_f64(_last[_i].x) && rde_math_hypot_f64(_p.x - _last[_i].x, _p.y - _last[_i].y) < _apart) {
            continue;
        }
        rde_arr_add(&_paths[_i], (any)&_p);
        _w->trail_points++;
        _last[_i] = _p;
    }
}

// See the header. False also for an item that has no joint in the world.
b8 fude_zoom_mech_world_broken(const fude_zoom_mech_world* _w, u8 _kind, u32 _item) {
    const fude_zoom_mech_joint* _j = (const fude_zoom_mech_joint*)_w->joints.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->joints); _i++) {
        if(_j[_i].kind == _kind && _j[_i].item == _item) {
            return _j[_i].joint == NULL;
        }
    }
    return false;
}

// See the header.
b8 fude_zoom_mech_world_on(const fude_zoom_mech_world* _w) {
    return _w->world != NULL;
}

// A body's joints as the world is made: its hinge (a gear's: what a gear joint couples), which way its turn counts
// there (+1: it is the hinge's first body), and its slide.
typedef struct {
    rde_physics_2d_joint* hinge;
    f64                   sign;
    rde_physics_2d_joint* slide;
    u32                   hinge_at, slide_at;   // Their places in the world's joints.
} fzw_held;

// Returns a body's material's strength (MPa): the ground's and a plunger's, steel's; else the part's own, or the
// library's parts', plastic's.
RDE_INTERNAL f64 fzw_strength(const fude_zoom_mech_world* _w, u32 _i) {
    if(_i == FUDE_ZOOM_NONE || _i >= (u32)rde_arr_length(&_w->plan.bodies)) {
        return FZW_GROUND;
    }
    const fude_zoom_mech_body* _b = &((const fude_zoom_mech_body*)_w->plan.bodies.memory)[_i];
    if(_b->part->kind == FUDE_ZOOM_MECH_PLUNGER) {
        return FZW_GROUND;   // A solenoid's plunger: steel.
    }
    return _b->strength > 0.0 ? _b->strength : FZW_PLASTIC;
}

// Notes a joint the world holds and watches, with its kind, its plan item, its bodies, where it is and its strength.
// Returns its place in the world's joints. A NULL joint is noted as broken.
RDE_INTERNAL u32 fzw_note(fude_zoom_mech_world* _w, rde_physics_2d_joint* _j, u8 _kind, u32 _item, u32 _a, u32 _b, fude_zoom_v2 _at, f64 _strength) {
    fude_zoom_mech_joint _r;
    rde_memset(&_r, 0, sizeof(_r));
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

// Returns a plan point (home units) in body _i's own frame as it was made (before any turn since), in world units.
RDE_INTERNAL rde_vec_2F fzw_local(const fude_zoom_mech_world* _w, u32 _i, fude_zoom_v2 _p) {
    const fude_zoom_mech_body* _b = &((const fude_zoom_mech_body*)_w->plan.bodies.memory)[_i];
    // As it was made: a gear turned its phase more, a rack moved its shift along itself.
    const f64 _a = _b->angle + _b->phase;
    const f64 _dx = _p.x - (_b->at.x + rde_math_cos_f64(_b->angle) * _b->shift), _dy = _p.y - (_b->at.y + rde_math_sin_f64(_b->angle) * _b->shift);
    return (rde_vec_2F){ (f32)((_dx * rde_math_cos_f64(_a) + _dy * rde_math_sin_f64(_a)) * _w->k), (f32)((-_dx * rde_math_sin_f64(_a) + _dy * rde_math_cos_f64(_a)) * _w->k) };
}

// See the header.
fude_zoom_sim fude_zoom_mech_world_move(const fude_zoom_mech_world* _w, u32 _b) {
    return _b < (u32)rde_arr_length(&_w->moves) ? ((const fude_zoom_sim*)_w->moves.memory)[_b] : fude_zoom_sim_identity();
}

// See the header.
fude_zoom_v2 fude_zoom_mech_world_point(const fude_zoom_mech_world* _w, u32 _b, fude_zoom_v2 _p) {
    return _b == FUDE_ZOOM_NONE ? _p : fude_zoom_sim_apply(fude_zoom_mech_world_move(_w, _b), _p);
}

// Writes a drawn body's piece _piece (the plan's) as a polygon's corners (world units, about its body) into _out, at
// most RDE_PHYSICS_2D_MAX_POLYGON_VERTS of them. Returns how many.
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

// Declared here for fude_zoom_mech_world_start; defined below.
RDE_INTERNAL void fzw_publish(fude_zoom_mech_world* _w);

// A body's shape as fude_zoom_mech_world_start makes it (fzw_start_body), for the steps that set it (fzw_shape_plain,
// fzw_shape_more): its first shape, its mass, its body's type, whether it has a body of its own, and the shapes added
// to it (more: the body's array of FUDE_ZOOM_MECH_RATCHET_TEETH, mores of them used). A step takes what it sets of it
// under the names the function had, and hands it back.
typedef struct {
    rde_physics_2d_shape_def  sh;
    f32                       mass;
    RDE_PHYSICS_2D_BODY_TYPE_ type;
    b8                        own;
    rde_physics_2d_shape_def* more;
    u32                       mores;
} fzw_shape;

// Sets, for fzw_start_body, the shape and the mass of body _i of _b when it is of one plain shape (its kind _kind; _hw
// and _hh its half sizes in world units, _m the smaller): a link, a plate, a gear, a hand crank, a weight, a wheel, a
// crate, a slider, a rack, a plunger, a sprocket, a cam. False: it is none of them (nothing set).
RDE_INTERNAL b8 fzw_shape_plain(const fude_zoom_mech_body* _b, u32 _i, u8 _kind, f32 _hw, f32 _hh, f32 _m, fzw_shape* _s) {
    rde_physics_2d_shape_def _sh = _s->sh;
    f32 _mass = _s->mass;
    switch(_kind) {
    case FUDE_ZOOM_MECH_LINK:
        _sh.type = RDE_PHYSICS_2D_SHAPE_CAPSULE;
        _sh.capsule.radius = _hh;
        _sh.capsule.half_height = rde_math_max_f32(_hw - _hh, 0.01f);
        _sh.offset_angle = (f32)(RDE_PI * 0.5);
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
    case FUDE_ZOOM_MECH_CRANK:
        // A disc, passing over what is pinned to it, as a gear does.
        _sh.type = RDE_PHYSICS_2D_SHAPE_CIRCLE;
        _sh.circle.radius = _m * 0.95f;
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
    case FUDE_ZOOM_MECH_SLIDER:
    case FUDE_ZOOM_MECH_RACK:
        // On its line: it passes over the links pinned to it and the gears on it, as the parts of a linkage do.
        _sh.type = RDE_PHYSICS_2D_SHAPE_BOX;
        _sh.box.half_extents = (rde_vec_2F){ _hw, _hh };
        _sh.layer = 1u; _sh.layer_mask = 2u | 4u;
        _mass = 2.0f * _hw * _hh;
        break;
    case FUDE_ZOOM_MECH_PLUNGER: {
        // Its rod, from inside its coil to its end (its solenoid's middle its own: it slides along it). It passes
        // over what is pinned to it, as a slider does, and pushes what is in its way.
        const f32 _from = -0.35f * _hw, _to = (f32)FUDE_ZOOM_SOLENOID_TIP * _hw + 0.3f * _hh;
        _sh.type = RDE_PHYSICS_2D_SHAPE_BOX;
        _sh.box.half_extents = (rde_vec_2F){ (_to - _from) * 0.5f, 0.3f * _hh };
        _sh.offset = (rde_vec_2F){ (_to + _from) * 0.5f, 0.0f };
        _sh.layer = 1u; _sh.layer_mask = 2u | 4u;
        _mass = 2.0f * _sh.box.half_extents.x * _sh.box.half_extents.y;
        break;
    }
    case FUDE_ZOOM_MECH_SPROCKET:
        // A disc, passing over what is pinned to it, as a gear does.
        _sh.type = RDE_PHYSICS_2D_SHAPE_CIRCLE;
        _sh.circle.radius = _m * 0.88f;
        _sh.layer = 1u; _sh.layer_mask = 4u;
        _mass = 3.0f * _m * _m;
        break;
    case FUDE_ZOOM_MECH_CAM:
        // Its disc about its middle, the body's own; it turns about its hole off it.
        _sh.type = RDE_PHYSICS_2D_SHAPE_CIRCLE;
        _sh.circle.radius = _m * 0.95f;
        _sh.layer = 8u; _sh.layer_mask = 8u;
        _mass = 3.0f * _m * _m;
        break;
    default:
        return false;
    }
    _s->sh = _sh;
    _s->mass = _mass;
    return true;
}

// Sets, for fzw_start_body, the shapes of body _i of _b of any other kind (as fzw_shape_plain; _w the world, _k its
// scale): a drawn body's first piece (fzw_piece), a follower's rod and roller, a ratchet's hub and teeth, a pawl's
// lever and point, a wall (static); anything else (a pivot, a motor, a spring, a rope, a pulley) no body of its own.
RDE_INTERNAL void fzw_shape_more(const fude_zoom_mech_world* _w, const fude_zoom_mech_body* _b, u32 _i, u8 _kind, f64 _k, f32 _hw, f32 _hh, f32 _m, fzw_shape* _s) {
    rde_physics_2d_shape_def _sh = _s->sh;
    f32 _mass = _s->mass;
    RDE_PHYSICS_2D_BODY_TYPE_ _type = _s->type;
    b8 _own = _s->own;
    rde_physics_2d_shape_def* _more = _s->more;
    u32 _mores = _s->mores;
    switch(_kind) {
    case FUDE_ZOOM_MECH_DRAWN:
        // Its first convex piece is its shape; the rest are added once it is made, its mass theirs together.
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
    case FUDE_ZOOM_MECH_FOLLOWER:
        // Its rod, passing over links as a slider does; its roller, bearing on cams.
        _sh.type = RDE_PHYSICS_2D_SHAPE_BOX;
        _sh.box.half_extents = (rde_vec_2F){ 0.785f * _hw, 0.3f * _hh };
        _sh.offset = (rde_vec_2F){ -0.165f * _hw, 0.0f };
        _sh.layer = 1u; _sh.layer_mask = 2u | 4u;
        _mass = 2.0f * _sh.box.half_extents.x * _sh.box.half_extents.y;
        _more[_mores] = _sh;
        _more[_mores].type = RDE_PHYSICS_2D_SHAPE_CIRCLE;
        _more[_mores].circle.radius = 0.6f * _hh;
        _more[_mores].offset = (rde_vec_2F){ 0.75f * _hw, 0.0f };
        _more[_mores].layer = 8u; _more[_mores].layer_mask = 8u;
        _more[_mores].friction = 0.05f;   // A roller: it rolls.
        _mores++;
        break;
    case FUDE_ZOOM_MECH_RATCHET: {
        // Its hub, passing over what is pinned to it; its teeth, each a triangle from its tip down its back.
        f64 _tip, _root;
        fude_zoom_mech_ratchet_radii(&_b[_i], &_tip, &_root);
        _sh.type = RDE_PHYSICS_2D_SHAPE_CIRCLE;
        _sh.circle.radius = (f32)(_root * _k);
        _sh.layer = 1u; _sh.layer_mask = 4u;
        _mass = 3.0f * _m * _m;
        const f64 _p = 2.0 * RDE_PI / (f64)FUDE_ZOOM_MECH_RATCHET_TEETH;
        for(u32 _t = 0; _t < FUDE_ZOOM_MECH_RATCHET_TEETH; _t++) {
            const f64 _a = _p * (f64)_t;
            rde_physics_2d_shape_def* _q = &_more[_mores++];
            *_q = _sh;
            _q->type = RDE_PHYSICS_2D_SHAPE_POLYGON;
            _q->polygon.count = 3u;
            _q->polygon.verts[0] = (rde_vec_2F){ (f32)(rde_math_cos_f64(_a) * _root * 0.9 * _k), (f32)(rde_math_sin_f64(_a) * _root * 0.9 * _k) };
            _q->polygon.verts[1] = (rde_vec_2F){ (f32)(rde_math_cos_f64(_a) * _tip * _k), (f32)(rde_math_sin_f64(_a) * _tip * _k) };
            _q->polygon.verts[2] = (rde_vec_2F){ (f32)(rde_math_cos_f64(_a + _p) * _root * _k), (f32)(rde_math_sin_f64(_a + _p) * _root * _k) };
            _q->layer = 8u; _q->layer_mask = 8u;
        }
        break;
    }
    case FUDE_ZOOM_MECH_PAWL:
        // Its lever, passing over links; its point, bearing on ratchets.
        _sh.type = RDE_PHYSICS_2D_SHAPE_BOX;
        _sh.box.half_extents = (rde_vec_2F){ 0.75f * _hw, 0.6f * _hh };
        _sh.offset = (rde_vec_2F){ -0.2f * _hw, 0.0f };
        _sh.layer = 1u; _sh.layer_mask = 2u | 4u;
        _mass = 2.0f * _sh.box.half_extents.x * _sh.box.half_extents.y;
        _more[_mores] = _sh;
        _more[_mores].type = RDE_PHYSICS_2D_SHAPE_POLYGON;
        _more[_mores].offset = (rde_vec_2F){ 0.0f, 0.0f };
        _more[_mores].polygon.count = 3u;
        _more[_mores].polygon.verts[0] = (rde_vec_2F){ 0.55f * _hw, -0.6f * _hh };
        _more[_mores].polygon.verts[1] = (rde_vec_2F){ 0.97f * _hw, 0.0f };
        _more[_mores].polygon.verts[2] = (rde_vec_2F){ 0.55f * _hw, 0.6f * _hh };
        _more[_mores].layer = 8u; _more[_mores].layer_mask = 8u;
        _mores++;
        break;
    case FUDE_ZOOM_MECH_WALL:
        _sh.type = RDE_PHYSICS_2D_SHAPE_BOX;
        _sh.box.half_extents = (rde_vec_2F){ _hw, _hh };
        _sh.layer = 2u; _sh.layer_mask = 0u;
        _type = RDE_PHYSICS_2D_BODY_TYPE_STATIC;
        _mass = 0.0f;
        break;
    default:
        _own = false;   // A pivot, a motor, a spring, a rope, a pulley: no body of its own.
        break;
    }
    _s->sh = _sh;
    _s->mass = _mass;
    _s->type = _type;
    _s->own = _own;
    _s->mores = _mores;
}

// Makes plan body _i of _b in the world (fude_zoom_mech_world_start's, _k its scale): its RDE body with its shapes,
// mass and layers (fzw_shape_plain, fzw_shape_more), or none; its inverse mass, and its move from where it was drawn
// (none yet), noted. It adds one to each of the world's bodies, inv_mass and moves, a NULL body when it has none.
RDE_INTERNAL void fzw_start_body(fude_zoom_mech_world* _w, const fude_zoom_mech_body* _b, u32 _i, f64 _k) {
    rde_physics_2d_body* _body = NULL;
    const u8 _kind = _b[_i].part->kind;
    rde_physics_2d_shape_def _sh;
    rde_memset(&_sh, 0, sizeof(_sh));
    _sh.friction = 0.6f;
    _sh.restitution = 0.2f;
    const f32 _hw = (f32)(_b[_i].hw * _k), _hh = (f32)(_b[_i].hh * _k), _m = rde_math_min_f32(_hw, _hh);
    // A rack moved its shift along itself: its teeth in its gear's gaps.
    const rde_vec_2F _at = { (f32)((_b[_i].at.x + rde_math_cos_f64(_b[_i].angle) * _b[_i].shift) * _k), (f32)((_b[_i].at.y + rde_math_sin_f64(_b[_i].angle) * _b[_i].shift) * _k) };
    f32 _mass = 1.0f;
    RDE_PHYSICS_2D_BODY_TYPE_ _type = RDE_PHYSICS_2D_BODY_TYPE_DYNAMIC;
    b8 _own = true;
    // More shapes on it: a ratchet's teeth, a follower's roller, a pawl's tip, on layer 8, the layer of the parts
    // that bear on each other (cams, followers, ratchets, pawls).
    rde_physics_2d_shape_def _more[FUDE_ZOOM_MECH_RATCHET_TEETH];
    u32 _mores = 0;
    fzw_shape _shape = { _sh, _mass, _type, _own, _more, _mores };
    if(!fzw_shape_plain(_b, _i, _kind, _hw, _hh, _m, &_shape)) {
        fzw_shape_more(_w, _b, _i, _kind, _k, _hw, _hh, _m, &_shape);
    }
    _sh    = _shape.sh;
    _mass  = _shape.mass;
    _type  = _shape.type;
    _own   = _shape.own;
    _mores = _shape.mores;
    if(_own) {
        // A gear turned its phase more: its teeth in the gaps it meshes with.
        _body = rde_physics_2d_body_create(_w->world, _type, _at, (f32)(_b[_i].angle + _b[_i].phase), &_sh, rde_math_max_f32(_mass, 0.01f));
        for(u32 _k = 1; _body != NULL && _kind == FUDE_ZOOM_MECH_DRAWN && _k < _b[_i].pieces; _k++) {   // (no body made: nothing to add to)
            _sh.polygon.count = fzw_piece(_w, _b[_i].piece + _k, _sh.polygon.verts);
            rde_physics_2d_body_add_shape(_body, &_sh);
        }
        for(u32 _k = 0; _body != NULL && _k < _mores; _k++) {
            rde_physics_2d_body_add_shape(_body, &_more[_k]);
        }
    }
    rde_arr_add(&_w->bodies, (any)&_body);
    const f64 _inv = _body != NULL && _type == RDE_PHYSICS_2D_BODY_TYPE_DYNAMIC ? 1.0 / (f64)rde_math_max_f32(_mass, 0.01f) : 0.0;
    rde_arr_add(&_w->inv_mass, (any)&_inv);
    const fude_zoom_sim _still = fude_zoom_sim_identity();
    rde_arr_add(&_w->moves, (any)&_still);
}

// Makes the plan's hinges in the world, for fude_zoom_mech_world_start (_b its bodies, _pb theirs in the world, _k its
// scale), each noted with its pin's strength, as said below; each body's own hinge noted in _held.
RDE_INTERNAL void fzw_start_hinges(fude_zoom_mech_world* _w, const fude_zoom_mech_body* _b, f64 _k, rde_physics_2d_body** _pb, fzw_held* _held) {
    // Hinges: each to the ground or between two bodies, stiff; a motor's, a hand crank's or a circuit motor's shaft's
    // made to drive as it says.
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
        // Its pin's strength: its weaker part's material in shear (3/5 of its strength) over a pin six tenths of its
        // thinner part's half width across, 3 to 12 mm.
        const f64 _ha  = _b[_h[_i].a].hh, _hb = _h[_i].b != FUDE_ZOOM_NONE ? _b[_h[_i].b].hh : _ha;
        const f64 _d   = rde_math_min_f64(rde_math_max_f64(0.6 * rde_math_min_f64(_ha, _hb), FZW_PIN_LEAST), FZW_PIN_MOST);
        const u32 _pin = fzw_note(_w, _j, FUDE_ZOOM_MECH_JOINT_PIN, _i, _h[_i].a, _h[_i].b, _h[_i].at,
                                  0.6 * rde_math_min_f64(fzw_strength(_w, _h[_i].a), fzw_strength(_w, _h[_i].b)) * 0.25 * RDE_PI * _d * _d);
        fude_zoom_mech_joint* _pr = &((fude_zoom_mech_joint*)_w->joints.memory)[_pin];
        _pr->la = fzw_local(_w, _h[_i].a, _h[_i].at);
        _pr->lb = _h[_i].b == FUDE_ZOOM_NONE ? (rde_vec_2F){ (f32)(_h[_i].at.x * _k), (f32)(_h[_i].at.y * _k) } : fzw_local(_w, _h[_i].b, _h[_i].at);
        if(_j != NULL && _h[_i].motor) {
            // The ground's way round: its part turned as the motor says; from rest, brought up to it as it runs.
            const f64 _nm = 1e-3 / _k;   // A world torque in N·m is this squared.
            rde_physics_2d_joint_enable_motor(_j, 0.0f, (f32)rde_math_min_f64((_h[_i].torque > 0.0 ? _h[_i].torque : 10.0) / (_nm * _nm), 1e9));
            const fude_zoom_mech_motor _mo = { _j, -_h[_i].speed, _pin, 0.0, 0.0, 0.0, 0.0, false };
            rde_arr_add(&_w->motors, (any)&_mo);
        }
        if(_j != NULL && _h[_i].crank) {
            // A hand crank's axle: free until a hand turns it, but for its friction (a motor holding it still, as
            // hard).
            const f64 _nm = 1e-3 / _k;
            rde_physics_2d_joint_enable_motor(_j, 0.0f, (f32)(_b[_h[_i].a].value / (_nm * _nm)));
            const fude_zoom_mech_crank _cr = { _j, _h[_i].a, 0.0, 0.0, false, 0.0 };
            rde_arr_add(&_w->cranks, (any)&_cr);
        }
        if(_j != NULL && _h[_i].shaft != FUDE_ZOOM_NONE && _h[_i].b == FUDE_ZOOM_NONE) {
            // A circuit's motor's: free until the circuit drives it.
            const fude_zoom_mech_shaft _sh = { _j, _h[_i].a, _b[_h[_i].shaft].object };
            rde_arr_add(&_w->shafts, (any)&_sh);
        }
        // A gear's own hinge: the one to the ground, its axle or its motor, before one to another part.
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
}

// Makes the plan's slides in the world, for fude_zoom_mech_world_start (_b its _nb bodies, _pb theirs in the world, _k
// its scale), each body's noted in _held; then the solenoids' plungers, as said below.
RDE_INTERNAL void fzw_start_slides(fude_zoom_mech_world* _w, const fude_zoom_mech_body* _b, u32 _nb, f64 _k, rde_physics_2d_body** _pb, fzw_held* _held) {
    // What slides: along its line, from the ground (its travel limited as the plan says).
    const fude_zoom_mech_slide* _sl = (const fude_zoom_mech_slide*)_w->plan.slides.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.slides); _i++) {
        rde_physics_2d_body* _body = _pb[_sl[_i].body];
        if(_body == NULL) {
            continue;
        }
        const fude_zoom_v2 _c = _b[_sl[_i].body].at;
        // Its limits counted from where it starts.
        _held[_sl[_i].body].slide = rde_physics_2d_joint_create_slider(_w->world, _w->ground, (rde_vec_2F){ (f32)(_sl[_i].at.x * _k), (f32)(_sl[_i].at.y * _k) },
                                                                     _body, fzw_local(_w, _sl[_i].body, _sl[_i].at),
                                                                     (rde_vec_2F){ (f32)_sl[_i].axis.x, (f32)_sl[_i].axis.y },
                                                                     (f32)((_sl[_i].lower - _b[_sl[_i].body].shift) * _k), (f32)((_sl[_i].upper - _b[_sl[_i].body].shift) * _k));
        rde_physics_2d_joint_set_stiffness(_held[_sl[_i].body].slide, FZW_STIFF, 2.0f);
        // Its strength: its material over the part's own size (fude_zoom_mech_body_size, 2 mm at least) and thickness,
        // half of it bearing — not the plan's unit, the smallest part's, which made every slide as weak as the
        // smallest thing elsewhere on the canvas. A solenoid's plunger in its coil and a cam's follower never give: 0.
        const b8 _plunger = _b[_sl[_i].body].part->kind == FUDE_ZOOM_MECH_PLUNGER || _b[_sl[_i].body].part->kind == FUDE_ZOOM_MECH_FOLLOWER;
        const f64 _size = rde_math_max_f64(fude_zoom_mech_body_size(&_b[_sl[_i].body]), 2.0);
        _held[_sl[_i].body].slide_at = fzw_note(_w, _held[_sl[_i].body].slide, FUDE_ZOOM_MECH_JOINT_SLIDE, _i, _sl[_i].body, FUDE_ZOOM_NONE, _c,
                                                _plunger ? 0.0 : 0.5 * fzw_strength(_w, _sl[_i].body) * _size * FZW_THICK);
    }
    // Solenoids' plungers: pulled as their coils say (coupling.h), along their slides.
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind == FUDE_ZOOM_MECH_PLUNGER && _pb[_i] != NULL) {
            const fude_zoom_mech_plunger _pl = { _i, _b[_i].object, { rde_math_cos_f64(_b[_i].angle), rde_math_sin_f64(_b[_i].angle) }, rde_physics_2d_body_get_position(_pb[_i]),
                                                 FUDE_ZOOM_SOLENOID_STROKE * _b[_i].hw, 0.0 };
            rde_arr_add(&_w->plungers, (any)&_pl);
        }
    }
}

// Makes the plan's springs in the world, for fude_zoom_mech_world_start (_pb its bodies in the world, _k its scale), as
// said below.
RDE_INTERNAL void fzw_start_springs(fude_zoom_mech_world* _w, f64 _k, rde_physics_2d_body** _pb) {
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
                                                                       (f32)(2.0 * rde_math_sqrt_f64(rde_math_max_f64(_sp[_i].stiffness, 0.01))), 0.08f);
        // It gives stretched half again past its length: 2½ times as long as drawn.
        const fude_zoom_v2 _mid = { (_sp[_i].pa.x + _sp[_i].pb.x) * 0.5, (_sp[_i].pa.y + _sp[_i].pb.y) * 0.5 };
        const u32 _sat = fzw_note(_w, _js, FUDE_ZOOM_MECH_JOINT_SPRING, _i, _sp[_i].a, _sp[_i].b, _mid, 1.5 * _sp[_i].length);
        ((fude_zoom_mech_joint*)_w->joints.memory)[_sat].rest = _sp[_i].length;
    }
}

// Makes the plan's ropes in the world, for fude_zoom_mech_world_start (_pb its bodies in the world, _k its scale), as
// said below.
RDE_INTERNAL void fzw_start_ropes(fude_zoom_mech_world* _w, f64 _k, rde_physics_2d_body** _pb) {
    // Ropes: RDE's rope (never longer than drawn); two over one pulley, RDE's pulley (one side a fixed end: a rope as
    // long as what is left of it).
    const fude_zoom_mech_rope* _r = (const fude_zoom_mech_rope*)_w->plan.ropes.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.ropes); _i++) {
        const f64 _len = rde_math_hypot_f64(_r[_i].pb.x - _r[_i].pa.x, _r[_i].pb.y - _r[_i].pa.y);
        rde_arr_add(&_w->ropes_were, (any)&_len);
        // A rope's end on its body, in the body's frame, or on the ground, in the world's; and its body or the ground.
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
                // One side tied off: the other a rope from its rim, as long as the rope less that side.
                const fude_zoom_mech_rope* _free = _r[_i].a != FUDE_ZOOM_NONE ? &_r[_i] : _q;
                const fude_zoom_mech_rope* _tied = _free == &_r[_i] ? _q : &_r[_i];
                if(_free->a != FUDE_ZOOM_NONE && _pb[_free->a] != NULL) {
                    const f64 _left = _r[_i].length + _q->length - rde_math_hypot_f64(_tied->pa.x - _tied->pb.x, _tied->pa.y - _tied->pb.y);
                    fzw_note(_w, rde_physics_2d_joint_create_rope(_w->world, _pb[_free->a], fzw_local(_w, _free->a, _free->pa), _w->ground,
                                                                 (rde_vec_2F){ (f32)(_free->pb.x * _k), (f32)(_free->pb.y * _k) }, (f32)(_left * _k)),
                             FUDE_ZOOM_MECH_JOINT_ROPE, _i, _free->a, FUDE_ZOOM_NONE, _free->pb, FZW_ROPE);
                }
            }
        }
        #undef FZW_END
        #undef FZW_BODY
    }
}

// Makes the plan's gear meshes in the world, for fude_zoom_mech_world_start (_b its bodies, _k its scale), on the
// hinges and slides noted in _held, as said below.
RDE_INTERNAL void fzw_start_meshes(fude_zoom_mech_world* _w, const fude_zoom_mech_body* _b, f64 _k, const fzw_held* _held) {
    // Gears meshed: RDE's gear joints on their hinges (a rack's: on its slide). Two gears: turn₁ + (teeth₂ / teeth₁) ·
    // turn₂ kept; a gear and a rack: as the plan's ratio says (its travel in world units).
    const fude_zoom_mech_mesh* _m = (const fude_zoom_mech_mesh*)_w->plan.meshes.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.meshes); _i++) {
        rde_physics_2d_joint* _ja = _held[_m[_i].a].hinge;
        rde_physics_2d_joint* _jb = _m[_i].rack ? _held[_m[_i].b].slide : _held[_m[_i].b].hinge;
        if(_ja == NULL || _jb == NULL || !(_m[_i].ratio != 0.0)) {
            continue;
        }
        const f64 _ratio = _m[_i].rack ? _held[_m[_i].a].sign * _m[_i].ratio / _k : _held[_m[_i].a].sign * _held[_m[_i].b].sign / _m[_i].ratio;
        rde_physics_2d_joint* _gear = rde_physics_2d_joint_create_gear(_w->world, _ja, _jb, (f32)_ratio);
        // Its teeth's strength, as Lewis has a gear's: its material, its face (the part's thickness), its module (its
        // pitch circle's width a tooth), a tooth's form, 0.3. It rests on the pins or slide of its two parts.
        const fude_zoom_mech_body* _ga = &_b[_m[_i].a];
        const f64 _rp  = fude_zoom_mech_pitch(_ga->part, rde_math_min_f64(_ga->hw, _ga->hh));
        const f64 _mod = _ga->part != NULL && _ga->part->teeth > 0u ? 2.0 * _rp / (f64)_ga->part->teeth : rde_math_max_f64(_w->plan.unit, 1.0);
        const u32 _gat = fzw_note(_w, _gear, FUDE_ZOOM_MECH_JOINT_GEAR, _i, _m[_i].a, _m[_i].b, _ga->at,
                                  rde_math_min_f64(fzw_strength(_w, _m[_i].a), fzw_strength(_w, _m[_i].b)) * FZW_THICK * _mod * 0.3);
        fude_zoom_mech_joint* _gr = &((fude_zoom_mech_joint*)_w->joints.memory)[_gat];
        _gr->on[0] = _held[_m[_i].a].hinge_at;
        _gr->on[1] = _m[_i].rack ? _held[_m[_i].b].slide_at : _held[_m[_i].b].hinge_at;
        _gr->rest  = _rp;   // Its pitch circle's radius: its torque to its teeth's force.
        if(_m[_i].rack && _gear != NULL) {
            // A rack: held only while the gear is over its teeth (its slide's travel, world units), meshing again on a
            // tooth.
            const f64 _from = _b[_m[_i].b].shift;   // Its travel counted from where it starts.
            rde_physics_2d_joint_set_gear_range(_gear, (f32)((_m[_i].lower - _from) * _k), (f32)((_m[_i].upper - _from) * _k));
            rde_physics_2d_joint_set_gear_period(_gear, (f32)_m[_i].period);
        }
    }
}

// See the header. The world is scaled so the parts are about 1 world unit across (k = 1 / (4 · the plan's unit)),
// under 9.81 m/s² of gravity, with sleeping off. Each plan body becomes an RDE body with its shapes, mass and layers
// (above); then its joints are made in order, each noted with its strength (fzw_note). Its steps: each body
// (fzw_start_body), then the hinges, the slides and the plungers, the springs, the ropes and the gear meshes
// (fzw_start_*).
b8 fude_zoom_mech_world_start(fude_zoom_mech_world* _w, const fude_zoom_mech_plan* _plan) {
    fude_zoom_mech_world_stop(_w);
    fude_zoom_mech_plan_copy(&_w->plan, _plan);
    const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_w->plan.bodies.memory;
    const u32 _nb = (u32)rde_arr_length(&_w->plan.bodies);
    if(_nb == 0u) {
        return false;
    }
    _w->k = 1.0 / rde_math_max_f64(4.0 * _w->plan.unit, 1e-300);
    const f64 _k = _w->k;
    const f64 _g = 9810.0 * _k;   // Gravity: 9.81 m/s² in the home frame's millimetres, in world units.
    // (256: the bodies Box2D reserves room for at first, not a cap; a bigger mechanism's arrays grow)
    _w->world = rde_physics_2d_world_create((rde_vec_2F){ 0.0f, (f32)-_g }, 256u, NULL);
    if(_w->world == NULL) {
        return false;
    }
    rde_physics_2d_world_set_sub_steps(_w->world, FZW_SUB_STEPS);
    // A machine at rest is not asleep: its circuit may start it.
    rde_physics_2d_world_enable_sleeping(_w->world, false);
    // The ground: a static body with a tiny sensor for its shape, for joints to the world to hang from.
    rde_physics_2d_shape_def _dot;
    rde_memset(&_dot, 0, sizeof(_dot));
    _dot.type = RDE_PHYSICS_2D_SHAPE_CIRCLE;
    _dot.circle.radius = 0.01f;
    _dot.is_sensor = true;
    _w->ground = rde_physics_2d_body_create(_w->world, RDE_PHYSICS_2D_BODY_TYPE_STATIC, (rde_vec_2F){ 0.0f, 0.0f }, 0.0f, &_dot, 0.0f);
    // The bodies: each plan part's shape, mass and layers. Pivots, motors, springs, ropes and pulleys have no body.
    for(u32 _i = 0; _i < _nb; _i++) {
        fzw_start_body(_w, _b, _i, _k);
    }
    rde_physics_2d_body** _pb = (rde_physics_2d_body**)_w->bodies.memory;
    // Each body's joints (fzw_held).
    rde_arr TYPE(fzw_held) _held_arr = rde_arr_new(sizeof(fzw_held), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_held_arr, _nb);
    fzw_held* _held = (fzw_held*)_held_arr.memory;   // Sized once: it stays put.
    fzw_start_hinges(_w, _b, _k, _pb, _held);
    fzw_start_slides(_w, _b, _nb, _k, _pb, _held);
    fzw_start_springs(_w, _k, _pb);
    fzw_start_ropes(_w, _k, _pb);
    fzw_start_meshes(_w, _b, _k, _held);
    rde_arr_free(&_held_arr);
    _w->time = 0.0;
    _w->left = 0.0;
    // Each body's turn counted from as it starts.
    for(u32 _i = 0; _i < _nb; _i++) {
        const f64 _zero = 0.0, _was = _pb[_i] != NULL ? (f64)rde_physics_2d_body_get_angle(_pb[_i]) : 0.0;
        rde_arr_add(&_w->turned, (any)&_zero);
        rde_arr_add(&_w->turned_was, (any)&_was);
    }
    fzw_publish(_w);   // As it starts: its gears turned to mesh already.
    fzw_trace(_w);     // Its tracers' first points: where they start.
    return true;
}

// Hands out what the world is now: each body's move from where it was drawn (home units), each body's turn counted
// (every whole turn), and a pulley's turn with the rope over it.
RDE_INTERNAL void fzw_publish(fude_zoom_mech_world* _w) {
    const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_w->plan.bodies.memory;
    rde_physics_2d_body** _pb = (rde_physics_2d_body**)_w->bodies.memory;
    fude_zoom_sim* _mv = (fude_zoom_sim*)_w->moves.memory;
    const u32 _nb = (u32)rde_arr_length(&_w->plan.bodies);
    f64* _tu = (f64*)_w->turned.memory;
    f64* _tw = (f64*)_w->turned_was.memory;
    const b8 _counting = (u32)rde_arr_length(&_w->turned) == _nb;
    for(u32 _i = 0; _i < _nb; _i++) {
        _mv[_i] = fude_zoom_sim_identity();
        if(_pb[_i] == NULL) {
            continue;
        }
        if(_counting) {
            // Its turn, every one counted: what the world's angle went round since it was last read.
            const f64 _an = (f64)rde_physics_2d_body_get_angle(_pb[_i]);
            _tu[_i] += rde_math_remainder_f64(_an - _tw[_i], 2.0 * RDE_PI);
            _tw[_i]  = _an;
        }
        const rde_vec_2F _p = rde_physics_2d_body_get_position(_pb[_i]);
        const f64 _turn = (f64)rde_physics_2d_body_get_angle(_pb[_i]) - _b[_i].angle;
        const fude_zoom_v2 _now = { (f64)_p.x / _w->k, (f64)_p.y / _w->k };
        const f64 _c = rde_math_cos_f64(_turn), _s = rde_math_sin_f64(_turn);
        _mv[_i] = (fude_zoom_sim){ _c, _s, _now.x - (_c * _b[_i].at.x - _s * _b[_i].at.y), _now.y - (_s * _b[_i].at.x + _c * _b[_i].at.y) };
    }
    // A pulley: turned as far as the rope down its first side has come off it, over its radius.
    const fude_zoom_mech_rope* _r = (const fude_zoom_mech_rope*)_w->plan.ropes.memory;
    const f64* _was = (const f64*)_w->ropes_were.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.ropes); _i++) {
        if(_r[_i].over == FUDE_ZOOM_NONE || _r[_i].pair == FUDE_ZOOM_NONE || _i > _r[_i].pair) {
            continue;
        }
        const fude_zoom_mech_body* _p = &_b[_r[_i].over];
        const f64 _rad = fude_zoom_mech_pulley_radius(_p);
        const fude_zoom_v2 _load = fude_zoom_mech_world_point(_w, _r[_i].a, _r[_i].pa);
        const f64 _l = rde_math_hypot_f64(_load.x - _r[_i].pb.x, _load.y - _r[_i].pb.y);
        if(!(_l > 0.0) || !(_rad > 0.0)) {
            continue;
        }
        const fude_zoom_v2 _arm = { _r[_i].pb.x - _p->at.x, _r[_i].pb.y - _p->at.y }, _u = { (_load.x - _r[_i].pb.x) / _l, (_load.y - _r[_i].pb.y) / _l };
        const f64 _way = _arm.x * _u.y - _arm.y * _u.x >= 0.0 ? 1.0 : -1.0;
        const f64 _turn = _way * (_l - _was[_i]) / _rad, _c = rde_math_cos_f64(_turn), _s = rde_math_sin_f64(_turn);
        _mv[_r[_i].over] = (fude_zoom_sim){ _c, _s, _p->at.x - (_c * _p->at.x - _s * _p->at.y), _p->at.y - (_s * _p->at.x + _c * _p->at.y) };
    }
}

// Joint _i broken: what rests on it first (a mesh on its gear's pin), then it, let go; what drove it (a motor, a
// circuit's shaft, a hand crank) drives nothing, its copy of the joint cleared, since the joint is freed here and each
// of them checks for NULL before it uses one (a crank whose axle broke: held or let go, it turns nothing).
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
    fude_zoom_mech_crank* _cr = (fude_zoom_mech_crank*)_w->cranks.memory;
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_w->cranks); _k++) {
        _cr[_k].joint = _cr[_k].joint == _gone ? NULL : _cr[_k].joint;
    }
}

// Each joint against its strength, a step on. What pulls it: a world force in newtons (its masses kilograms, its
// lengths k a millimetre); a mesh's, its torque over its pitch circle's radius; a spring's, its stretch past its
// length. Past its strength for FZW_OVER steps (or FZW_SNAP times past it at once), once settled, it breaks. A pin's
// parts apart, for how long; each motor held still while it drives, for how long. What happened goes in the events.
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
            _r->force = rde_math_abs_f64((f64)rde_physics_2d_joint_get_torque(_r->joint)) * _to_n * _to_n / rde_math_max_f64(_r->rest * 1e-3, 1e-9);
            break;
        case FUDE_ZOOM_MECH_JOINT_SPRING:
            _r->force = rde_math_max_f64((f64)rde_physics_2d_joint_get_coordinate(_r->joint) / _w->k - _r->rest, 0.0);
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
                        _r->apart = -1.0;   // Said: until they fit again.
                    }
                }
            } else if(_sep < 0.5 * FZW_APART) {
                _r->apart = 0.0;
            }
        }
    }
    // Motors: how far each has turned (its angle's steps added up: RDE's angle within a half turn either way), and each
    // half second, once up to speed, how far on it got against how far it drives: under a tenth of it, jammed (rocking
    // against what holds it back is not turning); over half, turning again.
    fude_zoom_mech_motor* _mo = (fude_zoom_mech_motor*)_w->motors.memory;
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_w->motors); _k++) {
        if(_mo[_k].joint == NULL) {
            continue;
        }
        const f64 _now = (f64)rde_physics_2d_joint_get_coordinate(_mo[_k].joint);
        f64 _d = _now - _mo[_k].was;
        _d -= 2.0 * RDE_PI * rde_math_floor_f64((_d + RDE_PI) / (2.0 * RDE_PI));
        _mo[_k].was     = _now;
        _mo[_k].turned += _d;
        if(_w->time < FZW_SPIN_UP || !(rde_math_abs_f64(_mo[_k].speed) > 0.2)) {
            _mo[_k].from = _mo[_k].turned;
            continue;
        }
        _mo[_k].held += FZW_STEP;
        if(_mo[_k].held >= FZW_HELD_FOR) {
            const f64 _got = rde_math_abs_f64(_mo[_k].turned - _mo[_k].from), _want = rde_math_abs_f64(_mo[_k].speed) * _mo[_k].held;
            if(_got < 0.1 * _want && !_mo[_k].jammed) {
                const fude_zoom_mech_event _e = { FUDE_ZOOM_MECH_JAMMED, _mo[_k].pin, _got / _mo[_k].held, rde_math_abs_f64(_mo[_k].speed) };
                rde_arr_add(&_w->events, (any)&_e);
                _mo[_k].jammed = true;
            } else if(_got > 0.5 * _want) {
                _mo[_k].jammed = false;   // Turning again.
            }
            _mo[_k].from = _mo[_k].turned;
            _mo[_k].held = 0.0;
        }
    }
}

// By hand, before a step: each crank's turn counted (its angle's steps added up), a held one driven toward where the
// hand would have it (as fast as it is off, so a 30th of a second to get there, at most 60 radians a second, and as
// hard as a hand turns at most); a part held pulled toward where it is dragged (a spring its mass makes
// FUDE_ZOOM_MECH_HAND_HZ, critically damped, at most 30 times its weight).
RDE_INTERNAL void fzw_hand(fude_zoom_mech_world* _w) {
    rde_physics_2d_body* const* _pb = (rde_physics_2d_body* const*)_w->bodies.memory;
    fude_zoom_mech_crank* _cr = (fude_zoom_mech_crank*)_w->cranks.memory;
    const f64 _nm = 1e-3 / _w->k;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->cranks); _i++) {
        if(_pb[_cr[_i].body] == NULL || _cr[_i].joint == NULL) {
            continue;
        }
        const f64 _now = (f64)rde_physics_2d_body_get_angle(_pb[_cr[_i].body]);
        f64 _d = _now - _cr[_i].was;
        _d -= 2.0 * RDE_PI * rde_math_floor_f64((_d + RDE_PI) / (2.0 * RDE_PI));
        _cr[_i].turned += _d;
        _cr[_i].was = _now;
        if(_cr[_i].held) {
            const f64 _spin = rde_math_min_f64(rde_math_max_f64(30.0 * (_cr[_i].target - _cr[_i].turned), -60.0), 60.0);
            rde_physics_2d_joint_enable_motor(_cr[_i].joint, (f32)-_spin, (f32)rde_math_min_f64(FUDE_ZOOM_MECH_HAND_TORQUE / (_nm * _nm), 1e9));
        }
    }
    if(_w->grab != FUDE_ZOOM_NONE && _pb[_w->grab] != NULL) {
        rde_physics_2d_body* _body = _pb[_w->grab];
        const rde_vec_2F _p = rde_physics_2d_body_get_position(_body), _v = rde_physics_2d_body_get_velocity(_body);
        const f32 _a = rde_physics_2d_body_get_angle(_body), _om = rde_physics_2d_body_get_angular_velocity(_body);
        const f32 _c = rde_math_cos_f32(_a), _s = rde_math_sin_f32(_a);
        const rde_vec_2F _r = { _c * _w->grab_on.x - _s * _w->grab_on.y, _s * _w->grab_on.x + _c * _w->grab_on.y };
        const rde_vec_2F _at = { _p.x + _r.x, _p.y + _r.y }, _va = { _v.x - _om * _r.y, _v.y + _om * _r.x };
        const f64 _m = (f64)rde_physics_2d_body_get_mass(_body), _w0 = 2.0 * RDE_PI * FUDE_ZOOM_MECH_HAND_HZ;
        f64 _fx = _m * (_w0 * _w0 * (_w->grab_to.x * _w->k - (f64)_at.x) - 2.0 * _w0 * (f64)_va.x);
        f64 _fy = _m * (_w0 * _w0 * (_w->grab_to.y * _w->k - (f64)_at.y) - 2.0 * _w0 * (f64)_va.y);
        const f64 _most = 30.0 * _m * 9810.0 * _w->k, _f = rde_math_hypot_f64(_fx, _fy);
        if(_f > _most) {
            _fx *= _most / _f;
            _fy *= _most / _f;
        }
        rde_physics_2d_body_apply_force_at_point(_body, (rde_vec_2F){ (f32)_fx, (f32)_fy }, _at);
    }
}

// See the header. Within 1.15 times the crank's smaller half size of its middle, as drawn.
u32 fude_zoom_mech_world_crank_at(const fude_zoom_mech_world* _w, fude_zoom_v2 _at) {
    const fude_zoom_mech_crank* _cr = (const fude_zoom_mech_crank*)_w->cranks.memory;
    const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_w->plan.bodies.memory;
    for(u32 _i = 0; _w->world != NULL && _i < (u32)rde_arr_length(&_w->cranks); _i++) {
        const fude_zoom_mech_body* _c = &_b[_cr[_i].body];
        if(rde_math_hypot_f64(_at.x - _c->at.x, _at.y - _c->at.y) <= 1.15 * rde_math_min_f64(_c->hw, _c->hh)) {
            return _i;
        }
    }
    return FUDE_ZOOM_NONE;
}

// See the header.
void fude_zoom_mech_world_crank_hold(fude_zoom_mech_world* _w, u32 _c, f64 _angle) {
    if(_c < (u32)rde_arr_length(&_w->cranks)) {
        fude_zoom_mech_crank* _cr = &((fude_zoom_mech_crank*)_w->cranks.memory)[_c];
        _cr->held   = true;
        _cr->target = _angle;
    }
}

// See the header. Its axle's motor goes back to holding it still, as hard as its friction.
void fude_zoom_mech_world_crank_let_go(fude_zoom_mech_world* _w, u32 _c) {
    if(_c < (u32)rde_arr_length(&_w->cranks)) {
        fude_zoom_mech_crank* _cr = &((fude_zoom_mech_crank*)_w->cranks.memory)[_c];
        _cr->held = false;
        if(_cr->joint != NULL) {
            const f64 _nm = 1e-3 / _w->k;
            rde_physics_2d_joint_enable_motor(_cr->joint, 0.0f, (f32)(((const fude_zoom_mech_body*)_w->plan.bodies.memory)[_cr->body].value / (_nm * _nm)));
        }
    }
}

// See the header. 0 for no such crank.
f64 fude_zoom_mech_world_crank_angle(const fude_zoom_mech_world* _w, u32 _c) {
    return _c < (u32)rde_arr_length(&_w->cranks) ? ((const fude_zoom_mech_crank*)_w->cranks.memory)[_c].turned : 0.0;
}

// See the header. (0, 0) for no such crank.
fude_zoom_v2 fude_zoom_mech_world_crank_middle(const fude_zoom_mech_world* _w, u32 _c) {
    if(_c >= (u32)rde_arr_length(&_w->cranks)) {
        return (fude_zoom_v2){ 0.0, 0.0 };
    }
    return ((const fude_zoom_mech_body*)_w->plan.bodies.memory)[((const fude_zoom_mech_crank*)_w->cranks.memory)[_c].body].at;
}

// See the header. Of the first 16 shapes at the point, the first whose body moves (has a mass of its own) is taken.
b8 fude_zoom_mech_world_grab(fude_zoom_mech_world* _w, fude_zoom_v2 _at) {
    _w->grab = FUDE_ZOOM_NONE;
    if(_w->world == NULL) {
        return false;
    }
    rde_physics_2d_overlap_result _hit[16];
    const rde_vec_2F _q = { (f32)(_at.x * _w->k), (f32)(_at.y * _w->k) };
    const u32 _n = rde_physics_2d_world_point_query(_w->world, _q, _hit, 16u);
    rde_physics_2d_body* const* _pb = (rde_physics_2d_body* const*)_w->bodies.memory;
    const f64* _inv = (const f64*)_w->inv_mass.memory;
    for(u32 _h = 0; _h < _n && _h < 16u && _w->grab == FUDE_ZOOM_NONE; _h++) {
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->bodies); _i++) {
            if(_pb[_i] == _hit[_h].body && _inv[_i] > 0.0) {
                // Where on it: its own frame as it is now.
                const rde_vec_2F _p = rde_physics_2d_body_get_position(_pb[_i]);
                const f32 _a = rde_physics_2d_body_get_angle(_pb[_i]), _c = rde_math_cos_f32(_a), _s = rde_math_sin_f32(_a);
                const rde_vec_2F _d = { _q.x - _p.x, _q.y - _p.y };
                _w->grab    = _i;
                _w->grab_on = (rde_vec_2F){ _c * _d.x + _s * _d.y, -_s * _d.x + _c * _d.y };
                _w->grab_to = _at;
                break;
            }
        }
    }
    return _w->grab != FUDE_ZOOM_NONE;
}

// See the header.
void fude_zoom_mech_world_drag(fude_zoom_mech_world* _w, fude_zoom_v2 _to) {
    _w->grab_to = _to;
}

// See the header.
void fude_zoom_mech_world_let_go(fude_zoom_mech_world* _w) {
    _w->grab = FUDE_ZOOM_NONE;
}

// See the header.
b8 fude_zoom_mech_world_grabbed(const fude_zoom_mech_world* _w, fude_zoom_v2* _at) {
    rde_physics_2d_body* const* _pb = (rde_physics_2d_body* const*)_w->bodies.memory;
    if(_w->grab == FUDE_ZOOM_NONE || _w->world == NULL || _pb[_w->grab] == NULL) {
        return false;
    }
    const rde_vec_2F _p = rde_physics_2d_body_get_position(_pb[_w->grab]);
    const f32 _a = rde_physics_2d_body_get_angle(_pb[_w->grab]), _c = rde_math_cos_f32(_a), _s = rde_math_sin_f32(_a);
    *_at = (fude_zoom_v2){ ((f64)_p.x + (f64)(_c * _w->grab_on.x - _s * _w->grab_on.y)) / _w->k, ((f64)_p.y + (f64)(_s * _w->grab_on.x + _c * _w->grab_on.y)) / _w->k };
    return true;
}

// What presses on its own: a follower pushes along itself at FZW_FOLLOWER_PUSH times its weight, a pawl's point onto
// its ratchet at FZW_PAWL_PUSH times its weight, and a damper resists at a rate (per second) of its strength times
// FZW_DAMPING.
#define FZW_FOLLOWER_PUSH 3.0
#define FZW_PAWL_PUSH     2.0
#define FZW_DAMPING       10.0

// What presses on its own, each step: a follower along itself onto what is in front of it (three times its weight), a
// pawl's point onto its ratchet (twice its weight, toward its middle); and dampers: each as hard against its ends going
// apart or together as they go, its strength times their mass ten times a second, never more than stops half of it in
// a step (so it never throws them back).
RDE_INTERNAL void fzw_springs_of(fude_zoom_mech_world* _w) {
    const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_w->plan.bodies.memory;
    rde_physics_2d_body** _pb = (rde_physics_2d_body**)_w->bodies.memory;
    const f64* _inv = (const f64*)_w->inv_mass.memory;
    const f64 _g = fude_zoom_mech_world_torque_unit(_w);   // A unit of mass's weight.
    const fude_zoom_mech_slide* _sl = (const fude_zoom_mech_slide*)_w->plan.slides.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.slides); _i++) {
        const u32 _bd = _sl[_i].body;
        if(_b[_bd].part->kind != FUDE_ZOOM_MECH_FOLLOWER || _pb[_bd] == NULL) {
            continue;
        }
        const f64 _f = FZW_FOLLOWER_PUSH * (f64)rde_physics_2d_body_get_mass(_pb[_bd]) * _g;
        rde_physics_2d_body_apply_force(_pb[_bd], (rde_vec_2F){ (f32)(_sl[_i].axis.x * _f), (f32)(_sl[_i].axis.y * _f) });
    }
    const fude_zoom_mech_pawl* _pw = (const fude_zoom_mech_pawl*)_w->plan.pawls.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.pawls); _i++) {
        rde_physics_2d_body* _p = _pb[_pw[_i].body];
        if(_pw[_i].ratchet == FUDE_ZOOM_NONE || _p == NULL || _pb[_pw[_i].ratchet] == NULL) {
            continue;
        }
        const rde_vec_2F _o = rde_physics_2d_body_get_position(_p), _c = rde_physics_2d_body_get_position(_pb[_pw[_i].ratchet]);
        const f32 _a = rde_physics_2d_body_get_angle(_p), _r = (f32)(0.97 * _b[_pw[_i].body].hw * _w->k);
        const rde_vec_2F _tip = { _o.x + rde_math_cos_f32(_a) * _r, _o.y + rde_math_sin_f32(_a) * _r };
        const f64 _dx = (f64)(_c.x - _tip.x), _dy = (f64)(_c.y - _tip.y), _d = rde_math_hypot_f64(_dx, _dy);
        if(_d > 0.0) {
            const f64 _f = FZW_PAWL_PUSH * (f64)rde_physics_2d_body_get_mass(_p) * _g / _d;
            rde_physics_2d_body_apply_force_at_point(_p, (rde_vec_2F){ (f32)(_dx * _f), (f32)(_dy * _f) }, _tip);
        }
    }
    const fude_zoom_mech_damper* _dm = (const fude_zoom_mech_damper*)_w->plan.dampers.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plan.dampers); _i++) {
        const u32 _e[2] = { _dm[_i].a, _dm[_i].b };
        rde_vec_2F _at[2], _v[2];
        f64 _iw = 0.0;
        for(u32 _k = 0; _k < 2u; _k++) {
            const fude_zoom_v2 _q = fude_zoom_mech_world_point(_w, _e[_k], _k == 0u ? _dm[_i].pa : _dm[_i].pb);
            _at[_k] = (rde_vec_2F){ (f32)(_q.x * _w->k), (f32)(_q.y * _w->k) };
            _v[_k]  = (rde_vec_2F){ 0.0f, 0.0f };
            if(_e[_k] != FUDE_ZOOM_NONE && _pb[_e[_k]] != NULL) {
                const rde_vec_2F _o = rde_physics_2d_body_get_position(_pb[_e[_k]]), _lv = rde_physics_2d_body_get_velocity(_pb[_e[_k]]);
                const f32 _om = rde_physics_2d_body_get_angular_velocity(_pb[_e[_k]]);
                _v[_k] = (rde_vec_2F){ _lv.x - _om * (_at[_k].y - _o.y), _lv.y + _om * (_at[_k].x - _o.x) };
                _iw += _inv[_e[_k]];
            }
        }
        const f64 _dx = (f64)(_at[1].x - _at[0].x), _dy = (f64)(_at[1].y - _at[0].y), _len = rde_math_hypot_f64(_dx, _dy);
        if(!(_len > 0.0) || !(_iw > 0.0)) {
            continue;
        }
        const f64 _ux = _dx / _len, _uy = _dy / _len;
        const f64 _apart = (f64)(_v[1].x - _v[0].x) * _ux + (f64)(_v[1].y - _v[0].y) * _uy;
        // Their mass together: 1 / the sum of their 1 / masses.
        const f64 _c = rde_math_min_f64(_dm[_i].strength * FZW_DAMPING, 0.5 / FZW_STEP) / _iw;
        const f64 _f = _c * _apart;   // On b back toward a; on a toward b.
        for(u32 _k = 0; _k < 2u; _k++) {
            if(_e[_k] != FUDE_ZOOM_NONE && _pb[_e[_k]] != NULL) {
                const f64 _sg = _k == 0u ? 1.0 : -1.0;
                rde_physics_2d_body_apply_force_at_point(_pb[_e[_k]], (rde_vec_2F){ (f32)(_sg * _f * _ux), (f32)(_sg * _f * _uy) }, _at[_k]);
            }
        }
    }
}

// One step: motors brought up to speed as they start (a smooth ramp over FZW_SPIN_UP), the hand, the plungers' pulls,
// what presses on its own, RDE's step (hinges, motors, slides, springs, ropes, pulleys, gears), then each joint's
// strain.
RDE_INTERNAL void fzw_tick(fude_zoom_mech_world* _w) {
    if(_w->time < FZW_SPIN_UP + FZW_STEP) {
        // Motors coming up to speed: smoothly, from rest.
        const f64 _u = rde_math_min_f64(rde_math_max_f64((_w->time + FZW_STEP) / FZW_SPIN_UP, 0.0), 1.0), _ramp = _u * _u * (3.0 - 2.0 * _u);
        const fude_zoom_mech_motor* _mo = (const fude_zoom_mech_motor*)_w->motors.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->motors); _i++) {
            if(_mo[_i].joint != NULL) {
                rde_physics_2d_joint_set_motor_speed(_mo[_i].joint, (f32)(_mo[_i].speed * _ramp));
            }
        }
    }
    fzw_hand(_w);
    // Solenoids' plungers: pulled as hard as their coils pull, along their way in.
    const fude_zoom_mech_plunger* _pl = (const fude_zoom_mech_plunger*)_w->plungers.memory;
    rde_physics_2d_body** _pb = (rde_physics_2d_body**)_w->bodies.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_w->plungers); _i++) {
        const f64 _f = -_pl[_i].pull * fude_zoom_mech_world_torque_unit(_w);
        rde_physics_2d_body_apply_force(_pb[_pl[_i].body], (rde_vec_2F){ (f32)(_pl[_i].axis.x * _f), (f32)(_pl[_i].axis.y * _f) });
    }
    fzw_springs_of(_w);
    rde_physics_2d_world_step(_w->world, (f32)FZW_STEP);   // Every joint is RDE's to solve.
    _w->time += FZW_STEP;
    fzw_strain(_w);
}

// See the header. The time owed is kept below a step; what it hands out is brought up to date once, after the steps.
void fude_zoom_mech_world_step(fude_zoom_mech_world* _w, f64 _dt) {
    if(_w->world == NULL) {
        return;
    }
    _w->left += rde_math_min_f64(rde_math_max_f64(_dt, 0.0), 0.1);
    while(_w->left >= FZW_STEP) {
        _w->left -= FZW_STEP;
        fzw_tick(_w);
    }
    fzw_publish(_w);
    fzw_trace(_w);
}

// See the header.
void fude_zoom_mech_world_tick(fude_zoom_mech_world* _w) {
    if(_w->world == NULL) {
        return;
    }
    fzw_tick(_w);
    fzw_publish(_w);
    fzw_trace(_w);
}

// See the header.
u32 fude_zoom_mech_world_shaft(const fude_zoom_mech_world* _w, u32 _object) {
    const fude_zoom_mech_shaft* _sh = (const fude_zoom_mech_shaft*)_w->shafts.memory;
    for(u32 _i = 0; _w->world != NULL && _i < (u32)rde_arr_length(&_w->shafts); _i++) {
        if(_sh[_i].object == _object) {
            return _i;
        }
    }
    return FUDE_ZOOM_NONE;
}

// See the header. 0 when what is on it has no body.
f64 fude_zoom_mech_world_shaft_spin(const fude_zoom_mech_world* _w, u32 _shaft) {
    if(_w->world == NULL || _shaft >= (u32)rde_arr_length(&_w->shafts)) {
        return 0.0;
    }
    const fude_zoom_mech_shaft* _sh = &((const fude_zoom_mech_shaft*)_w->shafts.memory)[_shaft];
    rde_physics_2d_body* const* _pb = (rde_physics_2d_body* const*)_w->bodies.memory;
    return _pb[_sh->body] != NULL ? (f64)rde_physics_2d_body_get_angular_velocity(_pb[_sh->body]) : 0.0;
}

// See the header. 0 when what is on it has no body.
f64 fude_zoom_mech_world_shaft_angle(const fude_zoom_mech_world* _w, u32 _shaft) {
    if(_w->world == NULL || _shaft >= (u32)rde_arr_length(&_w->shafts)) {
        return 0.0;
    }
    const fude_zoom_mech_shaft* _sh = &((const fude_zoom_mech_shaft*)_w->shafts.memory)[_shaft];
    rde_physics_2d_body* const* _pb = (rde_physics_2d_body* const*)_w->bodies.memory;
    return _pb[_sh->body] != NULL ? (f64)rde_physics_2d_body_get_angle(_pb[_sh->body]) : 0.0;
}

// See the header. Nothing for a shaft whose pin has broken.
void fude_zoom_mech_world_shaft_drive(fude_zoom_mech_world* _w, u32 _shaft, f64 _spin, f64 _torque) {
    if(_w->world == NULL || _shaft >= (u32)rde_arr_length(&_w->shafts)) {
        return;
    }
    rde_physics_2d_joint* _j = ((fude_zoom_mech_shaft*)_w->shafts.memory)[_shaft].joint;
    if(_j == NULL) {
        return;   // Its pin broken: it drives nothing.
    }
    if(!(_torque > 0.0)) {
        rde_physics_2d_joint_disable_motor(_j);
        return;
    }
    // The ground's way round, as a drive motor's: the ground's turn against its part's.
    rde_physics_2d_joint_enable_motor(_j, (f32)-_spin, (f32)rde_math_min_f64(_torque * fude_zoom_mech_world_torque_unit(_w), 1e9));
}

// See the header.
f64 fude_zoom_mech_world_torque_unit(const fude_zoom_mech_world* _w) {
    return 9810.0 * _w->k;   // Its gravity in world units: a unit of mass's weight at a unit of length.
}

// See the header. 0 for no such body.
f64 fude_zoom_mech_world_turned(const fude_zoom_mech_world* _w, u32 _body) {
    return _body < (u32)rde_arr_length(&_w->turned) ? ((const f64*)_w->turned.memory)[_body] : 0.0;
}

// See the header. Its move along its slide's axis, from where it was drawn.
f64 fude_zoom_mech_world_slid(const fude_zoom_mech_world* _w, u32 _body) {
    const fude_zoom_mech_slide* _sl = (const fude_zoom_mech_slide*)_w->plan.slides.memory;
    for(u32 _i = 0; _w->world != NULL && _i < (u32)rde_arr_length(&_w->plan.slides); _i++) {
        if(_sl[_i].body == _body) {
            const fude_zoom_v2 _at = ((const fude_zoom_mech_body*)_w->plan.bodies.memory)[_body].at;
            const fude_zoom_v2 _now = fude_zoom_sim_apply(fude_zoom_mech_world_move(_w, _body), _at);
            return (_now.x - _at.x) * _sl[_i].axis.x + (_now.y - _at.y) * _sl[_i].axis.y;
        }
    }
    return 0.0;
}

// See the header.
u32 fude_zoom_mech_world_plunger(const fude_zoom_mech_world* _w, u32 _object) {
    const fude_zoom_mech_plunger* _pl = (const fude_zoom_mech_plunger*)_w->plungers.memory;
    for(u32 _i = 0; _w->world != NULL && _i < (u32)rde_arr_length(&_w->plungers); _i++) {
        if(_pl[_i].object == _object) {
            return _i;
        }
    }
    return FUDE_ZOOM_NONE;
}

// See the header. Nothing for no such plunger.
void fude_zoom_mech_world_plunger_pull(fude_zoom_mech_world* _w, u32 _plunger, f64 _pull) {
    if(_plunger < (u32)rde_arr_length(&_w->plungers)) {
        ((fude_zoom_mech_plunger*)_w->plungers.memory)[_plunger].pull = _pull;
    }
}

// See the header. Its move out along its axis from where it started, over its stroke, kept between 0 and 1.
f64 fude_zoom_mech_world_plunger_in(const fude_zoom_mech_world* _w, u32 _plunger) {
    if(_w->world == NULL || _plunger >= (u32)rde_arr_length(&_w->plungers)) {
        return 0.0;
    }
    const fude_zoom_mech_plunger* _pl = &((const fude_zoom_mech_plunger*)_w->plungers.memory)[_plunger];
    const rde_vec_2F _now = rde_physics_2d_body_get_position(((rde_physics_2d_body* const*)_w->bodies.memory)[_pl->body]);
    const f64 _out = ((f64)(_now.x - _pl->start.x) * _pl->axis.x + (f64)(_now.y - _pl->start.y) * _pl->axis.y) / _w->k;
    return rde_math_min_f64(rde_math_max_f64(-_out / rde_math_max_f64(_pl->stroke, 1e-12), 0.0), 1.0);
}

// See the header. Of the first 16 shapes at the point, any whose body moves.
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
                return true;   // One of its parts that move: not the ground, a wall, a fixed drawing.
            }
        }
    }
    return false;
}
