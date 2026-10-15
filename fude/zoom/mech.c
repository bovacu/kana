// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/mech.h"
#include "zoom/symbol.h"
#include "zoom/shape.h"
#include "zoom/circuit.h"
#include "zoom/props.h"
#include "sim/body.h"

// ===========================================================================
// See mech.h.
//
// Laid out as: the catalogue, the parts drawn (fude_zoom_mech_draw and its small pen, fzm), the plan's storage, then
// building the plan: the bodies from the canvas's parts and drawings, then what joins them (hinges, springs, axles,
// keys, tracers, pins, ropes, slides, racks, gear meshes, belts, dampers, worms, pawls), then the racks' travel and the
// gears' starting phases. Every joint is found by places: a hole within half the plan's unit of another hole, or of a
// body's shape. Internal names start fzm_ (FZM_ for constants).
// ===========================================================================

// How hard a drive motor held still turns, unless its text says (N·m: a strong geared one's).
#define FZM_MOTOR_TORQUE 10.0

// The catalogue: each part's symbol id, kind, teeth, holes and starting text. (Gears: a module of 5 points as they
// come, their pitch circle 5 × teeth across, so any two mesh.)
RDE_INTERNAL const fude_zoom_mech_part FZM_PARTS[] = {
    { "link",        FUDE_ZOOM_MECH_LINK,   0, 2, { { -0.8f, 0.0f }, { 0.8f, 0.0f } }, "" },
    { "plate",       FUDE_ZOOM_MECH_PLATE,  0, 3, { { -0.7f, -0.62f }, { 0.7f, -0.62f }, { 0.0f, 0.62f } }, "" },
    { "gear 10T",    FUDE_ZOOM_MECH_GEAR,  10, 1, { { 0.0f, 0.0f } }, "" },
    { "gear 15T",    FUDE_ZOOM_MECH_GEAR,  15, 1, { { 0.0f, 0.0f } }, "" },
    { "gear 20T",    FUDE_ZOOM_MECH_GEAR,  20, 1, { { 0.0f, 0.0f } }, "" },
    { "gear 30T",    FUDE_ZOOM_MECH_GEAR,  30, 1, { { 0.0f, 0.0f } }, "" },
    { "gear 40T",    FUDE_ZOOM_MECH_GEAR,  40, 1, { { 0.0f, 0.0f } }, "" },
    { "gear 60T",    FUDE_ZOOM_MECH_GEAR,  60, 1, { { 0.0f, 0.0f } }, "" },
    { "fixed pivot", FUDE_ZOOM_MECH_PIVOT,  0, 1, { { 0.0f, 0.5f } }, "" },
    { "drive motor", FUDE_ZOOM_MECH_MOTOR,  0, 1, { { 0.0f, 0.0f } }, "30 rpm" },
    { "spring",      FUDE_ZOOM_MECH_SPRING, 0, 2, { { -0.9f, 0.0f }, { 0.9f, 0.0f } }, "" },
    { "weight",      FUDE_ZOOM_MECH_WEIGHT, 0, 1, { { 0.0f, 0.0f } }, "1 kg" },
    { "wheel",       FUDE_ZOOM_MECH_WHEEL,  0, 1, { { 0.0f, 0.0f } }, "" },
    { "wall",        FUDE_ZOOM_MECH_WALL,   0, 0, { { 0.0f, 0.0f } }, "" },
    { "pulley",      FUDE_ZOOM_MECH_PULLEY, 0, 1, { { 0.0f, 0.0f } }, "" },
    { "rope",        FUDE_ZOOM_MECH_ROPE,   0, 2, { { -0.95f, 0.0f }, { 0.95f, 0.0f } }, "" },
    { "crate",       FUDE_ZOOM_MECH_CRATE,  0, 1, { { 0.0f, 0.8f } }, "2 kg" },
    { "rail",        FUDE_ZOOM_MECH_RAIL,   0, 0, { { 0.0f, 0.0f } }, "" },
    { "slider",      FUDE_ZOOM_MECH_SLIDER, 0, 1, { { 0.0f, 0.0f } }, "" },
    { "rack",        FUDE_ZOOM_MECH_RACK,   0, 0, { { 0.0f, 0.0f } }, "" },
    { "pin",         FUDE_ZOOM_MECH_PIN,    0, 1, { { 0.0f, 0.0f } }, "" },
    { "drawn body",  FUDE_ZOOM_MECH_DRAWN,  0, 0, { { 0.0f, 0.0f } }, "" },   // (no symbol: a drawing's, props.h)
    { "motor shaft", FUDE_ZOOM_MECH_SHAFT,  0, 1, { { 0.0f, 0.0f } }, "" },   // (no symbol of its own: a circuit's motor's)
    { "tracer",      FUDE_ZOOM_MECH_TRACER, 0, 0, { { 0.0f, 0.0f } }, "" },   // (no hole: on whatever is under it)
    { "hand crank",  FUDE_ZOOM_MECH_CRANK,  0, 2, { { 0.0f, 0.0f }, { 0.7f, 0.0f } }, "" },   // (its axle, its handle)
    { "plunger",     FUDE_ZOOM_MECH_PLUNGER, 0, 1, { { (f32)FUDE_ZOOM_SOLENOID_TIP, 0.0f } }, "" },   // (no symbol of its own: a solenoid's)
    // (0.1.74) A belt's and a chain's ends' middles as a link's holes (however long); a cam's hole off its middle; a
    // follower's at its rod's back end; a pawl's at its pivot.
    { "belt",        FUDE_ZOOM_MECH_BELT,     0, 2, { { -0.8f, 0.0f }, { 0.8f, 0.0f } }, "" },
    { "chain",       FUDE_ZOOM_MECH_BELT,     0, 2, { { -0.8f, 0.0f }, { 0.8f, 0.0f } }, "" },
    { "sprocket",    FUDE_ZOOM_MECH_SPROCKET, 0, 1, { { 0.0f, 0.0f } }, "" },
    { "cam",         FUDE_ZOOM_MECH_CAM,      0, 1, { { (f32)(-FUDE_ZOOM_MECH_CAM_ECCENTRIC * 0.95), 0.0f } }, "" },
    { "follower",    FUDE_ZOOM_MECH_FOLLOWER, 0, 1, { { -0.85f, 0.0f } }, "" },
    { "ratchet",     FUDE_ZOOM_MECH_RATCHET,  0, 1, { { 0.0f, 0.0f } }, "" },
    { "pawl",        FUDE_ZOOM_MECH_PAWL,     0, 1, { { -0.7f, 0.0f } }, "" },
    { "damper",      FUDE_ZOOM_MECH_DAMPER,   0, 2, { { -0.9f, 0.0f }, { 0.9f, 0.0f } }, "1" },
    { "worm",        FUDE_ZOOM_MECH_WORM,     0, 0, { { 0.0f, 0.0f } }, "60 rpm" },
};
#define FZM_N ((u32)(sizeof(FZM_PARTS) / sizeof(FZM_PARTS[0])))   // how many parts the catalogue has

// See the header. A linear search of the catalogue by id.
const fude_zoom_mech_part* fude_zoom_mech_find(const c8* _id) {
    for(u32 _i = 0; _id != NULL && _i < FZM_N; _i++) {
        if(rde_strcmp(FZM_PARTS[_i].id, _id) == 0) {
            return &FZM_PARTS[_i];
        }
    }
    return NULL;
}

// See the header: the topics' one kind-to-part map (symbol.h: fude_zoom_symbol_topic_part), which finds each kind's
// part once by its id (fude_zoom_mech_find) and keeps it.
const fude_zoom_mech_part* fude_zoom_mech_of_kind(u32 _kind) {
    return (const fude_zoom_mech_part*)fude_zoom_symbol_topic_part(_kind, FUDE_ZOOM_SYMBOL_TOPIC_MECH);
}

// See the header. A symbol's kind is its shape's first number.
const fude_zoom_mech_part* fude_zoom_mech_of(const fude_zoom_scene* _s, u32 _object) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_SYMBOL) {
        return NULL;
    }
    f64 _n[1];
    const u32 _kind = fude_zoom_symbol_kind_of(_n, fude_zoom_scene_shape_numbers(_s, _object, _n, 1u));
    return _kind != FUDE_ZOOM_NONE ? fude_zoom_mech_of_kind(_kind) : NULL;   // (none, or a damaged file's: none)
}

// See the header. A gear of t teeth is t + 2 modules across its tips, t across its pitch circle.
f64 fude_zoom_mech_pitch(const fude_zoom_mech_part* _part, f64 _hw) {
    return _part != NULL && _part->teeth > 0u ? _hw * (f64)_part->teeth / ((f64)_part->teeth + 2.0) : _hw;
}

// The words for a way round (fude_zoom_mech_rpm), 1 clockwise and 2 counter-clockwise, in English, Spanish,
// Portuguese, French and Japanese. Those with a hyphen are here whole, so that "clockwise" is not read out of
// "anti-clockwise".
#define FZM_CW  1u
#define FZM_CCW 2u
RDE_INTERNAL const fude_zoom_word FZM_WAYS[] = {
    { "cw", FZM_CW }, { "clockwise", FZM_CW }, { "horario", FZM_CW }, { "hor\xC3\xA1rio", FZM_CW }, { "horaire", FZM_CW },
    { "\xE6\x99\x82\xE8\xA8\x88\xE5\x9B\x9E\xE3\x82\x8A", FZM_CW },                 // 時計回り
    { "\xE5\x8F\xB3\xE5\x9B\x9E\xE3\x82\x8A", FZM_CW },                             // 右回り
    { "ccw", FZM_CCW }, { "acw", FZM_CCW }, { "anticlockwise", FZM_CCW }, { "anti-clockwise", FZM_CCW }, { "counterclockwise", FZM_CCW },
    { "counter-clockwise", FZM_CCW }, { "antihorario", FZM_CCW }, { "anti-horario", FZM_CCW }, { "antihor\xC3\xA1rio", FZM_CCW },
    { "anti-hor\xC3\xA1rio", FZM_CCW }, { "antihoraire", FZM_CCW }, { "anti-horaire", FZM_CCW }, { "trigonom\xC3\xA9trique", FZM_CCW },
    { "\xE5\x8F\x8D\xE6\x99\x82\xE8\xA8\x88\xE5\x9B\x9E\xE3\x82\x8A", FZM_CCW },     // 反時計回り (longer than 時計回り in it)
    { "\xE5\xB7\xA6\xE5\x9B\x9E\xE3\x82\x8A", FZM_CCW },                             // 左回り
};

// See the header: the number with its sign, and the first word for a way round.
f64 fude_zoom_mech_rpm(const c8* _text, f64 _rpm) {
    f64 _v = 0.0;
    const f64 _speed = fude_zoom_circuit_value(_text, 0u, &_v) ? _v : _rpm;
    const u8  _way   = fude_zoom_circuit_word(_text, FZM_WAYS, (u32)(sizeof(FZM_WAYS) / sizeof(FZM_WAYS[0])), 0u);
    return _way == FZM_CW ? -_speed : _speed;
}

// --- drawn -------------------------------------------------------------------------------------------

// The pen a part is drawn with: where the points and parts go, the part's half sizes, how finely curves are cut, and
// the first point of the part being drawn.
typedef struct {
    rde_arr* points;
    rde_arr* parts;
    f64      hw, hh;
    u32      segments;
    u32      first;
} fzm;

// Adds a point to the part being drawn.
RDE_INTERNAL void fzm_pt(fzm* _d, f64 _x, f64 _y) {
    const fude_zoom_v2 _p = { _x, _y };
    rde_arr_add(_d->points, (any)&_p);
}
// Begins a part: its points start here.
RDE_INTERNAL void fzm_begin(fzm* _d) {
    _d->first = (u32)rde_arr_length(_d->points);
}
// Ends the part with _flags (symbol.h's FUDE_ZOOM_SYMBOL_); a part of fewer than 2 points is dropped.
RDE_INTERNAL void fzm_end(fzm* _d, u8 _flags) {
    const u32 _n = (u32)rde_arr_length(_d->points) - _d->first;
    if(_n >= 2u) {
        const fude_zoom_symbol_part _p = { .first = _d->first, .count = _n, .flags = _flags };
        rde_arr_add(_d->parts, (any)&_p);
    } else {
        rde_arr_resize(_d->points, _d->first);
    }
}
// Draws a line as a part of its own.
RDE_INTERNAL void fzm_line(fzm* _d, f64 _x0, f64 _y0, f64 _x1, f64 _y1) {
    fzm_begin(_d);
    fzm_pt(_d, _x0, _y0);
    fzm_pt(_d, _x1, _y1);
    fzm_end(_d, 0u);
}
// Adds an arc's points, from angle _a0 to _a1, to the part being drawn: as finely as a whole circle of _segments
// (16 at least) would be, 3 segments at least.
RDE_INTERNAL void fzm_arc(fzm* _d, f64 _cx, f64 _cy, f64 _r, f64 _a0, f64 _a1) {
    u32 _k = (u32)rde_math_ceil_f64((f64)(_d->segments < 16u ? 16u : _d->segments) * rde_math_abs_f64(_a1 - _a0) / (2.0 * RDE_PI));
    _k = _k < 3u ? 3u : _k;
    for(u32 _i = 0; _i <= _k; _i++) {
        const f64 _a = _a0 + (_a1 - _a0) * (f64)_i / (f64)_k;
        fzm_pt(_d, _cx + rde_math_cos_f64(_a) * _r, _cy + rde_math_sin_f64(_a) * _r);
    }
}
// Draws a closed circle as a part of its own, with _flags.
RDE_INTERNAL void fzm_circle(fzm* _d, f64 _cx, f64 _cy, f64 _r, u8 _flags) {
    fzm_begin(_d);
    const u32 _k = _d->segments < 16u ? 16u : _d->segments;
    for(u32 _i = 0; _i < _k; _i++) {
        const f64 _a = 2.0 * RDE_PI * (f64)_i / (f64)_k;
        fzm_pt(_d, _cx + rde_math_cos_f64(_a) * _r, _cy + rde_math_sin_f64(_a) * _r);
    }
    fzm_end(_d, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}
// Draws the outline round circles _r round each of _c (a convex polygon's corners, counter-clockwise): a plate's, a
// link's, a belt's.
RDE_INTERNAL void fzm_round_hull(fzm* _d, const fude_zoom_v2* _c, u32 _n, f64 _r, u8 _flags) {
    fzm_begin(_d);
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_v2 _p = _c[(_i + _n - 1u) % _n], _q = _c[_i], _nx = _c[(_i + 1u) % _n];
        const f64 _a0 = rde_math_atan2_f64(_q.y - _p.y, _q.x - _p.x) - RDE_PI * 0.5, _a1 = rde_math_atan2_f64(_nx.y - _q.y, _nx.x - _q.x) - RDE_PI * 0.5;
        f64 _b1 = _a1;
        while(_b1 < _a0) {
            _b1 += 2.0 * RDE_PI;
        }
        fzm_arc(_d, _q.x, _q.y, _r, _a0, _b1);
    }
    fzm_end(_d, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}

// Draws, for fude_zoom_mech_draw, a part that turns: a gear, a drive motor, a wheel, a pulley, a hand crank, a
// sprocket, a cam, a ratchet, a worm. It takes the pen under the name the function had, and hands it back. False: _part
// is none of them (nothing drawn).
RDE_INTERNAL b8 fzm_draw_turning(fzm* _pen, const fude_zoom_mech_part* _part, f64 _w, f64 _h, f64 _m) {
    fzm _d = *_pen;
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED, S = FUDE_ZOOM_SYMBOL_SOLID;
    switch(_part->kind) {
    case FUDE_ZOOM_MECH_GEAR: {
        const u32 _t = _part->teeth;
        const f64 _ra = _m, _rp = fude_zoom_mech_pitch(_part, _m), _mod = 2.0 * _rp / (f64)_t, _rr = _rp - 1.25 * _mod;
        fzm_begin(&_d);
        for(u32 _k = 0; _k < _t; _k++) {
            const f64 _a = 2.0 * RDE_PI * (f64)_k / (f64)_t, _p = 2.0 * RDE_PI / (f64)_t;
            const f64 _at[4] = { -0.26, -0.13, 0.13, 0.26 };
            const f64 _r[4]  = { _rr, _ra, _ra, _rr };
            for(u32 _j = 0; _j < 4u; _j++) {
                fzm_pt(&_d, rde_math_cos_f64(_a + _at[_j] * _p) * _r[_j], rde_math_sin_f64(_a + _at[_j] * _p) * _r[_j]);
            }
        }
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        fzm_circle(&_d, 0.0, 0.0, _ra * 0.12, 0u);
        // (a line out from its middle: its turn seen)
        fzm_line(&_d, _ra * 0.12, 0.0, _rr * 0.75, 0.0);
        break;
    }
    case FUDE_ZOOM_MECH_MOTOR:
        fzm_circle(&_d, 0.0, 0.0, _m * 0.8, F);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.14, 0u);
        fzm_begin(&_d);
        fzm_arc(&_d, 0.0, 0.0, _m * 0.98, RDE_PI * 0.15, RDE_PI * 0.85);
        fzm_end(&_d, 0u);
        fzm_begin(&_d);   // (its arrow's head: it turns counter-clockwise)
        fzm_pt(&_d, rde_math_cos_f64(RDE_PI * 0.85) * _m * 0.98, rde_math_sin_f64(RDE_PI * 0.85) * _m * 0.98);
        fzm_pt(&_d, rde_math_cos_f64(RDE_PI * 0.78) * _m * 0.80, rde_math_sin_f64(RDE_PI * 0.78) * _m * 0.80 + 0.1 * _m);
        fzm_pt(&_d, rde_math_cos_f64(RDE_PI * 0.78) * _m * 1.12, rde_math_sin_f64(RDE_PI * 0.78) * _m * 1.12 + 0.1 * _m);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | S);
        break;
    case FUDE_ZOOM_MECH_WHEEL:
        fzm_circle(&_d, 0.0, 0.0, _m * 0.95, F);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.2, 0u);
        for(u32 _k = 0; _k < 4u; _k++) {
            const f64 _a = RDE_PI * 0.5 * (f64)_k;
            fzm_line(&_d, rde_math_cos_f64(_a) * _m * 0.2, rde_math_sin_f64(_a) * _m * 0.2, rde_math_cos_f64(_a) * _m * 0.95, rde_math_sin_f64(_a) * _m * 0.95);
        }
        break;
    case FUDE_ZOOM_MECH_PULLEY:
        fzm_circle(&_d, 0.0, 0.0, _m * 0.95, F);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.78, 0u);   // (its groove: where the rope runs)
        fzm_circle(&_d, 0.0, 0.0, _m * 0.14, 0u);
        for(u32 _k = 0; _k < 3u; _k++) {
            const f64 _a = RDE_PI * 2.0 / 3.0 * (f64)_k + RDE_PI * 0.5;
            fzm_line(&_d, rde_math_cos_f64(_a) * _m * 0.14, rde_math_sin_f64(_a) * _m * 0.14, rde_math_cos_f64(_a) * _m * 0.78, rde_math_sin_f64(_a) * _m * 0.78);
        }
        break;
    case FUDE_ZOOM_MECH_CRANK:
        // (a disc, its axle in its middle, its handle (a knob) toward its rim, an arm between)
        fzm_circle(&_d, 0.0, 0.0, _m * 0.95, F);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.16, 0u);
        fzm_line(&_d, _m * 0.16, 0.0, _m * 0.56, 0.0);
        fzm_circle(&_d, _w * 0.7, 0.0, _m * 0.14, 0u);
        fzm_circle(&_d, _w * 0.7, 0.0, _m * 0.05, S);
        break;
    case FUDE_ZOOM_MECH_SPROCKET: {
        // (sixteen round teeth, its hole, a spoke: its turn seen)
        const f64 _ro = _m * 0.97, _ri = _m * 0.8;
        fzm_begin(&_d);
        for(u32 _k = 0; _k < 16u; _k++) {
            const f64 _a = 2.0 * RDE_PI * (f64)_k / 16.0, _p = 2.0 * RDE_PI / 16.0;
            const f64 _at[4] = { -0.3, -0.12, 0.12, 0.3 }, _r[4] = { _ri, _ro, _ro, _ri };
            for(u32 _j = 0; _j < 4u; _j++) {
                fzm_pt(&_d, rde_math_cos_f64(_a + _at[_j] * _p) * _r[_j], rde_math_sin_f64(_a + _at[_j] * _p) * _r[_j]);
            }
        }
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.6, 0u);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.12, 0u);
        fzm_line(&_d, _m * 0.12, 0.0, _m * 0.6, 0.0);
        break;
    }
    case FUDE_ZOOM_MECH_CAM: {
        // (a disc, its hole off its middle, a line from it across: its turn seen)
        const f64 _r = _m * 0.95, _hx = (f64)_part->holes[0][0] * _w;
        fzm_circle(&_d, 0.0, 0.0, _r, F);
        fzm_circle(&_d, _hx, 0.0, _m * 0.12, 0u);
        fzm_line(&_d, _hx + _m * 0.12, 0.0, _r * 0.8, 0.0);
        break;
    }
    case FUDE_ZOOM_MECH_RATCHET: {
        // (sawteeth: each from its tip down its back to the next one's root, free counter-clockwise, caught clockwise;
        // its hole, a spoke)
        f64 _tip, _root;
        fude_zoom_mech_body _rb;
        rde_memset(&_rb, 0, sizeof(_rb));
        _rb.part = _part;
        _rb.hw   = _w;
        _rb.hh   = _h;
        fude_zoom_mech_ratchet_radii(&_rb, &_tip, &_root);
        fzm_begin(&_d);
        for(u32 _k = 0; _k < FUDE_ZOOM_MECH_RATCHET_TEETH; _k++) {
            const f64 _a = 2.0 * RDE_PI * (f64)_k / (f64)FUDE_ZOOM_MECH_RATCHET_TEETH;
            fzm_pt(&_d, rde_math_cos_f64(_a) * _tip, rde_math_sin_f64(_a) * _tip);
            fzm_pt(&_d, rde_math_cos_f64(_a + 2.0 * RDE_PI / (f64)FUDE_ZOOM_MECH_RATCHET_TEETH) * _root, rde_math_sin_f64(_a + 2.0 * RDE_PI / (f64)FUDE_ZOOM_MECH_RATCHET_TEETH) * _root);
        }
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.12, 0u);
        fzm_line(&_d, _m * 0.12, 0.0, _root * 0.8, 0.0);
        break;
    }
    case FUDE_ZOOM_MECH_WORM: {
        // (a screw on its shaft: its threads' slants a lead apart (what moves as it plays), short of its right end by
        // more than a lead, so that they stay on it however far round it is)
        fzm_line(&_d, -_w, 0.0, -0.85 * _w, 0.0);
        fzm_line(&_d, 0.85 * _w, 0.0, _w, 0.0);
        fzm_begin(&_d);
        fzm_pt(&_d, -0.85 * _w, -0.6 * _h); fzm_pt(&_d, 0.85 * _w, -0.6 * _h); fzm_pt(&_d, 0.85 * _w, 0.6 * _h); fzm_pt(&_d, -0.85 * _w, 0.6 * _h);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        const f64 _lead = FUDE_ZOOM_MECH_WORM_LEAD;
        for(f64 _x = -0.85 * _w + 0.2 * _lead; _x + 1.35 * _lead <= 0.85 * _w; _x += _lead) {
            fzm_begin(&_d);
            fzm_pt(&_d, _x, -0.6 * _h);
            fzm_pt(&_d, _x + 0.3 * _lead, 0.6 * _h);
            fzm_end(&_d, FUDE_ZOOM_SYMBOL_MOVES);
        }
        break;
    }
    default:
        return false;
    }
    *_pen = _d;
    return true;
}

// Draws, for fude_zoom_mech_draw, what joins parts or holds them: a link, a plate, a fixed pivot, a spring, a rope, a
// belt or a chain, a pawl, a damper. It takes the pen under the name the function had, and hands it back. False:
// _part is none of them (nothing drawn).
RDE_INTERNAL b8 fzm_draw_linkage(fzm* _pen, const fude_zoom_mech_part* _part, f64 _w, f64 _h, f64 _m) {
    fzm _d = *_pen;
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED, S = FUDE_ZOOM_SYMBOL_SOLID;
    switch(_part->kind) {
    case FUDE_ZOOM_MECH_LINK: {
        const fude_zoom_v2 _c[2] = { { -_w + _h, 0.0 }, { _w - _h, 0.0 } };
        fzm_round_hull(&_d, _c, 2u, _h, F);
        for(u32 _i = 0; _i < 2u; _i++) {
            fzm_circle(&_d, _c[_i].x, 0.0, _h * 0.38, 0u);
        }
        break;
    }
    case FUDE_ZOOM_MECH_PLATE: {
        fude_zoom_v2 _c[3];
        for(u32 _i = 0; _i < 3u; _i++) {
            _c[_i] = (fude_zoom_v2){ (f64)_part->holes[_i][0] * _w, (f64)_part->holes[_i][1] * _h };
        }
        fzm_round_hull(&_d, _c, 3u, _m * 0.3, F);
        for(u32 _i = 0; _i < 3u; _i++) {
            fzm_circle(&_d, _c[_i].x, _c[_i].y, _m * 0.12, 0u);
        }
        break;
    }
    case FUDE_ZOOM_MECH_PIVOT: {
        const f64 _y = (f64)_part->holes[0][1] * _h, _base = -0.3 * _h;
        fzm_begin(&_d);
        fzm_pt(&_d, 0.0, _y + _h * 0.2);
        fzm_pt(&_d, -0.6 * _w, _base);
        fzm_pt(&_d, 0.6 * _w, _base);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        fzm_circle(&_d, 0.0, _y, _m * 0.16, 0u);
        fzm_line(&_d, -0.9 * _w, _base, 0.9 * _w, _base);
        for(u32 _k = 0; _k < 5u; _k++) {
            const f64 _x = -0.8 * _w + 0.38 * _w * (f64)_k;
            fzm_line(&_d, _x, _base, _x - 0.25 * _w, -_h);
        }
        break;
    }
    case FUDE_ZOOM_MECH_SPRING: {
        const f64 _x0 = -0.9 * _w, _x1 = 0.9 * _w;
        fzm_begin(&_d);
        fzm_pt(&_d, _x0, 0.0);
        fzm_pt(&_d, -0.6 * _w, 0.0);
        for(u32 _k = 0; _k < 8u; _k++) {
            fzm_pt(&_d, -0.6 * _w + 1.2 * _w * ((f64)_k + 0.5) / 8.0, (_k % 2u == 0u ? 0.7 : -0.7) * _h);
        }
        fzm_pt(&_d, 0.6 * _w, 0.0);
        fzm_pt(&_d, _x1, 0.0);
        fzm_end(&_d, 0u);
        fzm_circle(&_d, _x0, 0.0, _h * 0.3, 0u);
        fzm_circle(&_d, _x1, 0.0, _h * 0.3, 0u);
        break;
    }
    case FUDE_ZOOM_MECH_ROPE: {
        const f64 _x0 = -0.95 * _w, _x1 = 0.95 * _w;
        fzm_line(&_d, _x0, 0.0, _x1, 0.0);
        // (twisted: a strand's marks along it)
        for(f64 _x = _x0 + _h; _x < _x1 - _h * 0.5; _x += _h * 1.2) {
            fzm_line(&_d, _x - _h * 0.3, -_h * 0.35, _x + _h * 0.3, _h * 0.35);
        }
        fzm_circle(&_d, _x0, 0.0, _h * 0.3, 0u);
        fzm_circle(&_d, _x1, 0.0, _h * 0.3, 0u);
        break;
    }
    case FUDE_ZOOM_MECH_BELT: {
        // (a loop round its two ends' middles (its holes), as round as it is high: a chain's dashed, its links)
        const fude_zoom_v2 _c[2] = { { -_w + _h, 0.0 }, { _w - _h, 0.0 } };
        fzm_round_hull(&_d, _c, 2u, _h * 0.9, rde_strcmp(_part->id, "chain") == 0 ? FUDE_ZOOM_SYMBOL_DASHED : 0u);
        for(u32 _i = 0; _i < 2u; _i++) {
            fzm_circle(&_d, _c[_i].x, 0.0, _h * 0.12, S);
        }
        break;
    }
    case FUDE_ZOOM_MECH_PAWL:
        // (a lever to a point, its pivot's hole at its back)
        fzm_begin(&_d);
        fzm_pt(&_d, -0.95 * _w, -0.6 * _h); fzm_pt(&_d, 0.55 * _w, -0.6 * _h); fzm_pt(&_d, 0.97 * _w, 0.0); fzm_pt(&_d, 0.55 * _w, 0.6 * _h); fzm_pt(&_d, -0.95 * _w, 0.6 * _h);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        fzm_circle(&_d, -0.7 * _w, 0.0, 0.32 * _h, 0u);
        break;
    case FUDE_ZOOM_MECH_DAMPER:
        // (a cylinder on one end, a rod from the other into it to its piston)
        fzm_line(&_d, -0.9 * _w, 0.0, -0.75 * _w, 0.0);
        fzm_begin(&_d);
        fzm_pt(&_d, -0.75 * _w, -0.7 * _h); fzm_pt(&_d, 0.15 * _w, -0.7 * _h); fzm_pt(&_d, 0.15 * _w, 0.7 * _h); fzm_pt(&_d, -0.75 * _w, 0.7 * _h);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        fzm_line(&_d, -0.15 * _w, -0.55 * _h, -0.15 * _w, 0.55 * _h);
        fzm_line(&_d, -0.15 * _w, 0.0, 0.9 * _w, 0.0);
        fzm_circle(&_d, -0.9 * _w, 0.0, _h * 0.3, 0u);
        fzm_circle(&_d, 0.9 * _w, 0.0, _h * 0.3, 0u);
        break;
    default:
        return false;
    }
    *_pen = _d;
    return true;
}

// Draws, for fude_zoom_mech_draw, a body or what one slides on: a weight, a crate, a rail, a slider, a rack, a pin, a
// tracer, a cam's follower. It takes the pen under the name the function had, and hands it back. False: _part is none
// of them (nothing drawn).
RDE_INTERNAL b8 fzm_draw_body(fzm* _pen, const fude_zoom_mech_part* _part, f64 _w, f64 _h, f64 _m) {
    fzm _d = *_pen;
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED, S = FUDE_ZOOM_SYMBOL_SOLID;
    switch(_part->kind) {
    case FUDE_ZOOM_MECH_WEIGHT:
        fzm_circle(&_d, 0.0, 0.0, _m * 0.95, F);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.14, 0u);
        break;
    case FUDE_ZOOM_MECH_CRATE: {
        const f64 _top = _h * 0.7;   // (its hook above it)
        fzm_begin(&_d);
        fzm_pt(&_d, -_w, -_h); fzm_pt(&_d, _w, -_h); fzm_pt(&_d, _w, _top); fzm_pt(&_d, -_w, _top);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        const f64 _b = rde_math_min_f64(_w, _h) * 0.16;
        fzm_begin(&_d);
        fzm_pt(&_d, -_w + _b, -_h + _b); fzm_pt(&_d, _w - _b, -_h + _b); fzm_pt(&_d, _w - _b, _top - _b); fzm_pt(&_d, -_w + _b, _top - _b);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED);
        fzm_line(&_d, -_w + _b, -_h + _b, _w - _b, _top - _b);
        fzm_line(&_d, -_w + _b, _top - _b, _w - _b, -_h + _b);
        fzm_begin(&_d);
        fzm_arc(&_d, 0.0, _top + (_h - _top) * 0.5, (_h - _top) * 0.45, -RDE_PI * 0.5, RDE_PI * 1.5);
        fzm_end(&_d, 0u);
        break;
    }
    case FUDE_ZOOM_MECH_RAIL:
        // (two lines, its ends stopped: a guide)
        fzm_begin(&_d);
        fzm_pt(&_d, -_w, -_h); fzm_pt(&_d, _w, -_h); fzm_pt(&_d, _w, _h); fzm_pt(&_d, -_w, _h);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        fzm_line(&_d, -_w, 0.0, _w, 0.0);
        fzm_line(&_d, -_w + _h * 0.6, -_h, -_w + _h * 0.6, _h);
        fzm_line(&_d, _w - _h * 0.6, -_h, _w - _h * 0.6, _h);
        break;
    case FUDE_ZOOM_MECH_SLIDER:
        fzm_begin(&_d);
        fzm_pt(&_d, -_w, -_h); fzm_pt(&_d, _w, -_h); fzm_pt(&_d, _w, _h); fzm_pt(&_d, -_w, _h);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        fzm_line(&_d, -_w, -_h * 0.55, _w, -_h * 0.55);
        fzm_line(&_d, -_w, _h * 0.55, _w, _h * 0.55);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.22, 0u);
        break;
    case FUDE_ZOOM_MECH_RACK: {
        // (a bar, teeth along its top: a third of its half height their module (the library's, 5 points, as its
        // gears'), π modules apart, their pitch line its)
        const f64 _pl = fude_zoom_mech_rack_pitch_line(_h), _mod = fude_zoom_mech_rack_module(_h);
        const f64 _p = RDE_PI * _mod, _base = _pl - 1.25 * _mod, _tip = _pl + _mod;
        fzm_begin(&_d);
        fzm_pt(&_d, -_w, -_h);
        fzm_pt(&_d, _w, -_h);
        fzm_pt(&_d, _w, _base);
        for(f64 _x = _w - _p * 0.5; _x > -_w + _p * 0.25; _x -= _p) {
            fzm_pt(&_d, rde_math_min_f64(_x + _p * 0.26, _w), _base);
            fzm_pt(&_d, _x + _p * 0.13, _tip);
            fzm_pt(&_d, _x - _p * 0.13, _tip);
            fzm_pt(&_d, rde_math_max_f64(_x - _p * 0.26, -_w), _base);
        }
        fzm_pt(&_d, -_w, _base);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        break;
    }
    case FUDE_ZOOM_MECH_PIN:
        fzm_circle(&_d, 0.0, 0.0, _m * 0.9, F);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.3, S);
        break;
    case FUDE_ZOOM_MECH_TRACER:
        // (a pen's nib on a ring, its point at its middle)
        fzm_circle(&_d, 0.0, 0.0, _m * 0.85, 0u);
        fzm_line(&_d, -_m * 0.85, 0.0, -_m * 0.35, 0.0);
        fzm_line(&_d, _m * 0.35, 0.0, _m * 0.85, 0.0);
        fzm_line(&_d, 0.0, -_m * 0.85, 0.0, -_m * 0.35);
        fzm_line(&_d, 0.0, _m * 0.35, 0.0, _m * 0.85);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.18, S);
        break;
    case FUDE_ZOOM_MECH_FOLLOWER: {
        // (its rod and its roller (what moves as it plays: render.h's moved), the hole at its back end; its guide over
        // it, fixed, hatched)
        const u8 M = FUDE_ZOOM_SYMBOL_MOVES;
        fzm_begin(&_d);
        fzm_pt(&_d, -0.95 * _w, -0.3 * _h); fzm_pt(&_d, 0.62 * _w, -0.3 * _h); fzm_pt(&_d, 0.62 * _w, 0.3 * _h); fzm_pt(&_d, -0.95 * _w, 0.3 * _h);
        fzm_end(&_d, (u8)(FUDE_ZOOM_SYMBOL_CLOSED | F | M));
        fzm_circle(&_d, 0.75 * _w, 0.0, 0.6 * _h, (u8)(F | M));
        fzm_circle(&_d, 0.75 * _w, 0.0, 0.12 * _h, (u8)(S | M));
        fzm_circle(&_d, -0.85 * _w, 0.0, 0.18 * _h, M);
        for(u32 _k = 0; _k < 2u; _k++) {
            const f64 _sg = _k == 0u ? 1.0 : -1.0;
            fzm_begin(&_d);
            fzm_pt(&_d, -0.2 * _w, _sg * 0.38 * _h); fzm_pt(&_d, 0.2 * _w, _sg * 0.38 * _h); fzm_pt(&_d, 0.2 * _w, _sg * 0.95 * _h); fzm_pt(&_d, -0.2 * _w, _sg * 0.95 * _h);
            fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
            fzm_line(&_d, -0.12 * _w, _sg * 0.95 * _h, -0.2 * _w, _sg * 0.6 * _h);
            fzm_line(&_d, 0.08 * _w, _sg * 0.95 * _h, -0.08 * _w, _sg * 0.45 * _h);
        }
        break;
    }
    default:
        return false;
    }
    *_pen = _d;
    return true;
}

// See the header. One case for each kind, in its family's function (fzm_draw_turning, fzm_draw_linkage, fzm_draw_body):
// its outline first (filled), then its holes and marks; a wall, and anything unknown, is a hatched box.
u32 fude_zoom_mech_draw(const fude_zoom_mech_part* _part, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts) {
    rde_arr_clear(_points);
    rde_arr_clear(_parts);
    if(_part == NULL) {
        return 0;
    }
    fzm _d = { _points, _parts, rde_math_abs_f64(_hw), rde_math_abs_f64(_hh), _segments < 16u ? 16u : (_segments > 128u ? 128u : _segments), 0u };
    const f64 _w = _d.hw, _h = _d.hh, _m = rde_math_min_f64(_w, _h);
    const u8 F = FUDE_ZOOM_SYMBOL_FILLED;
    // Each kind is its family's; a wall, and anything unknown, is drawn here.
    if(!fzm_draw_turning(&_d, _part, _w, _h, _m) && !fzm_draw_linkage(&_d, _part, _w, _h, _m) &&
       !fzm_draw_body(&_d, _part, _w, _h, _m)) {
        fzm_begin(&_d);
        fzm_pt(&_d, -_w, -_h); fzm_pt(&_d, _w, -_h); fzm_pt(&_d, _w, _h); fzm_pt(&_d, -_w, _h);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        for(f64 _x = -_w + _h; _x < _w + _h; _x += 2.0 * _h) {   // (hatched)
            fzm_line(&_d, rde_math_max_f64(_x - _h, -_w), _x - _h < -_w ? -_h + (-_w - (_x - _h)) : -_h, rde_math_min_f64(_x + _h, _w), _x + _h > _w ? _h - (_x + _h - _w) : _h);
        }
    }
    return (u32)rde_arr_length(_parts);
}

// --- the plan ----------------------------------------------------------------------------------------

// See the header. The unit starts at 1 (home units).
void fude_zoom_mech_plan_init(fude_zoom_mech_plan* _p) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _p->bodies  = rde_arr_new(sizeof(fude_zoom_mech_body), _heap);
    _p->hinges  = rde_arr_new(sizeof(fude_zoom_mech_hinge), _heap);
    _p->springs = rde_arr_new(sizeof(fude_zoom_mech_spring), _heap);
    _p->meshes  = rde_arr_new(sizeof(fude_zoom_mech_mesh), _heap);
    _p->ropes   = rde_arr_new(sizeof(fude_zoom_mech_rope), _heap);
    _p->slides  = rde_arr_new(sizeof(fude_zoom_mech_slide), _heap);
    _p->tracers = rde_arr_new(sizeof(fude_zoom_mech_tracer), _heap);
    _p->belts   = rde_arr_new(sizeof(fude_zoom_mech_belt), _heap);
    _p->dampers = rde_arr_new(sizeof(fude_zoom_mech_damper), _heap);
    _p->worms   = rde_arr_new(sizeof(fude_zoom_mech_worm), _heap);
    _p->pawls   = rde_arr_new(sizeof(fude_zoom_mech_pawl), _heap);
    _p->piece_points = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    _p->piece_counts = rde_arr_new(sizeof(u32), _heap);
    _p->unit    = 1.0;
}

// See the header.
void fude_zoom_mech_plan_destroy(fude_zoom_mech_plan* _p) {
    rde_arr_free(&_p->bodies);
    rde_arr_free(&_p->hinges);
    rde_arr_free(&_p->springs);
    rde_arr_free(&_p->meshes);
    rde_arr_free(&_p->ropes);
    rde_arr_free(&_p->slides);
    rde_arr_free(&_p->tracers);
    rde_arr_free(&_p->belts);
    rde_arr_free(&_p->dampers);
    rde_arr_free(&_p->worms);
    rde_arr_free(&_p->pawls);
    rde_arr_free(&_p->piece_points);
    rde_arr_free(&_p->piece_counts);
}

// Makes array _to a copy of _from's _size-byte elements.
RDE_INTERNAL void fzm_arr_copy(rde_arr* _to, const rde_arr* _from, usize _size) {
    rde_arr_clear(_to);
    const u32 _n = (u32)rde_arr_length(_from);
    if(_n > 0u) {
        rde_memcpy(rde_arr_add_n(_to, _n), _from->memory, (usize)_n * _size);
    }
}

// See the header. _to must be set up (fude_zoom_mech_plan_init).
void fude_zoom_mech_plan_copy(fude_zoom_mech_plan* _to, const fude_zoom_mech_plan* _from) {
    fzm_arr_copy(&_to->bodies, &_from->bodies, sizeof(fude_zoom_mech_body));
    fzm_arr_copy(&_to->hinges, &_from->hinges, sizeof(fude_zoom_mech_hinge));
    fzm_arr_copy(&_to->springs, &_from->springs, sizeof(fude_zoom_mech_spring));
    fzm_arr_copy(&_to->meshes, &_from->meshes, sizeof(fude_zoom_mech_mesh));
    fzm_arr_copy(&_to->ropes, &_from->ropes, sizeof(fude_zoom_mech_rope));
    fzm_arr_copy(&_to->slides, &_from->slides, sizeof(fude_zoom_mech_slide));
    fzm_arr_copy(&_to->tracers, &_from->tracers, sizeof(fude_zoom_mech_tracer));
    fzm_arr_copy(&_to->belts, &_from->belts, sizeof(fude_zoom_mech_belt));
    fzm_arr_copy(&_to->dampers, &_from->dampers, sizeof(fude_zoom_mech_damper));
    fzm_arr_copy(&_to->worms, &_from->worms, sizeof(fude_zoom_mech_worm));
    fzm_arr_copy(&_to->pawls, &_from->pawls, sizeof(fude_zoom_mech_pawl));
    fzm_arr_copy(&_to->piece_points, &_from->piece_points, sizeof(fude_zoom_v2));
    fzm_arr_copy(&_to->piece_counts, &_from->piece_counts, sizeof(u32));
    _to->unit = _from->unit;
}

// See the header. The radii are fractions of the smaller half size, as each part is drawn.
f64 fude_zoom_mech_belt_radius(const fude_zoom_mech_body* _b) {
    const f64 _m = rde_math_min_f64(_b->hw, _b->hh);
    switch(_b->part->kind) {
    case FUDE_ZOOM_MECH_GEAR:     return fude_zoom_mech_pitch(_b->part, _m);
    case FUDE_ZOOM_MECH_SPROCKET: return 0.88 * _m;
    case FUDE_ZOOM_MECH_CRANK:
    case FUDE_ZOOM_MECH_WHEEL:    return 0.95 * _m;
    default:                      return 0.0;
    }
}

// See the header. As the ratchet is drawn: 0.95 and 0.72 of its smaller half size.
void fude_zoom_mech_ratchet_radii(const fude_zoom_mech_body* _b, f64* _tip, f64* _root) {
    const f64 _m = rde_math_min_f64(_b->hw, _b->hh);
    *_tip  = 0.95 * _m;
    *_root = 0.72 * _m;
}

// See the header.
f64 fude_zoom_mech_body_size(const fude_zoom_mech_body* _b) {
    const u8 _k = _b->part->kind;
    const b8 _long = _k == FUDE_ZOOM_MECH_LINK || _k == FUDE_ZOOM_MECH_ROPE || _k == FUDE_ZOOM_MECH_RAIL || _k == FUDE_ZOOM_MECH_RACK || _k == FUDE_ZOOM_MECH_BELT;
    return _long ? rde_math_max_f64(_b->hh, _b->hw * 0.08) : rde_math_min_f64(_b->hw, _b->hh) * 0.5;
}

// See the header. Its groove, as drawn: 0.78 of its smaller half size.
f64 fude_zoom_mech_pulley_radius(const fude_zoom_mech_body* _b) {
    return rde_math_min_f64(_b->hw, _b->hh) * 0.78;
}

// See the header.
f64 fude_zoom_mech_rack_module(f64 _hh) {
    return _hh / 3.0;   // (as big as the rack is drawn: placed at any zoom, as the gears placed with it)
}

// See the header.
f64 fude_zoom_mech_rack_pitch_line(f64 _hh) {
    return _hh - 2.0 * fude_zoom_mech_rack_module(_hh);   // (its teeth's tips a module above it)
}

// Returns whether _q is on a drawn body's pieces, or within _tol of their edges (defined with the drawn bodies, below).
RDE_INTERNAL b8 fzm_on_drawn(const fude_zoom_mech_plan* _p, const fude_zoom_mech_body* _b, fude_zoom_v2 _q, f64 _tol);

// Returns whether _q (home units) is on body _b: its shape as drawn, _tol about it. A round part is its circle, a
// follower its rod and roller (not its guide), a plunger its rod (not its coil), a drawn body its pieces, anything
// else its box.
RDE_INTERNAL b8 fzm_on(const fude_zoom_mech_plan* _p, const fude_zoom_mech_body* _b, fude_zoom_v2 _q, f64 _tol) {
    if(_b->part->kind == FUDE_ZOOM_MECH_DRAWN) {
        return fzm_on_drawn(_p, _b, _q, _tol);
    }
    const f64 _dx = _q.x - _b->at.x, _dy = _q.y - _b->at.y;
    const f64 _lx = _dx * rde_math_cos_f64(_b->angle) + _dy * rde_math_sin_f64(_b->angle), _ly = -_dx * rde_math_sin_f64(_b->angle) + _dy * rde_math_cos_f64(_b->angle);
    switch(_b->part->kind) {
    case FUDE_ZOOM_MECH_WEIGHT:
    case FUDE_ZOOM_MECH_WHEEL:
    case FUDE_ZOOM_MECH_GEAR:
    case FUDE_ZOOM_MECH_SPROCKET:
    case FUDE_ZOOM_MECH_CAM:
    case FUDE_ZOOM_MECH_RATCHET:
        return rde_math_hypot_f64(_lx, _ly) <= rde_math_min_f64(_b->hw, _b->hh) + _tol;
    case FUDE_ZOOM_MECH_FOLLOWER:
        // (its rod and roller: not its guide)
        return _lx >= -0.95 * _b->hw - _tol && _lx <= 0.95 * _b->hw + _tol && rde_math_abs_f64(_ly) <= 0.6 * _b->hh + _tol;
    case FUDE_ZOOM_MECH_LINK:
        return rde_math_abs_f64(_ly) <= _b->hh + _tol && rde_math_abs_f64(_lx) <= _b->hw + _tol;
    case FUDE_ZOOM_MECH_PLUNGER:
        // (its rod, out of its coil: not the coil)
        return _lx >= 0.2 * _b->hw - _tol && _lx <= FUDE_ZOOM_SOLENOID_TIP * _b->hw + 0.3 * _b->hh + _tol && rde_math_abs_f64(_ly) <= 0.3 * _b->hh + _tol;
    default:
        return rde_math_abs_f64(_lx) <= _b->hw + _tol && rde_math_abs_f64(_ly) <= _b->hh + _tol;
    }
}

// Returns body _b's hole _h where it is (home units).
RDE_INTERNAL fude_zoom_v2 fzm_hole(const fude_zoom_mech_body* _b, u32 _h) {
    f64 _x = (f64)_b->part->holes[_h][0] * _b->hw, _y = (f64)_b->part->holes[_h][1] * _b->hh;
    if(_b->part->kind == FUDE_ZOOM_MECH_LINK || _b->part->kind == FUDE_ZOOM_MECH_BELT) {
        _x = (_h == 0u ? -1.0 : 1.0) * rde_math_max_f64(_b->hw - _b->hh, 0.0);   // (a link's holes at its round ends' middles, however long; a belt's)
    }
    return (fude_zoom_v2){ _b->at.x + _x * rde_math_cos_f64(_b->angle) - _y * rde_math_sin_f64(_b->angle), _b->at.y + _x * rde_math_sin_f64(_b->angle) + _y * rde_math_cos_f64(_b->angle) };
}

// Returns whether a part of kind _kind holds nothing itself, its ends being held on what they are on: a spring, a
// rope, a belt, a damper.
RDE_INTERNAL b8 fzm_not_held(u8 _kind) {
    return _kind == FUDE_ZOOM_MECH_SPRING || _kind == FUDE_ZOOM_MECH_ROPE || _kind == FUDE_ZOOM_MECH_BELT || _kind == FUDE_ZOOM_MECH_DAMPER;
}

// See its declaration above: inside any of the body's convex pieces (as many corners of each as a body's piece takes),
// or within _tol of one's edges.
RDE_INTERNAL b8 fzm_on_drawn(const fude_zoom_mech_plan* _p, const fude_zoom_mech_body* _b, fude_zoom_v2 _q, f64 _tol) {
    const fude_zoom_v2* _pts = (const fude_zoom_v2*)_p->piece_points.memory;
    const u32* _cnt = (const u32*)_p->piece_counts.memory;
    u32 _first = 0;
    for(u32 _k = 0; _k < _b->piece; _k++) {
        _first += _cnt[_k];
    }
    const fude_sim_v2 _l = { _q.x - _b->at.x, _q.y - _b->at.y };
    for(u32 _k = _b->piece; _k < _b->piece + _b->pieces; _k++) {
        fude_sim_v2 _c[FUDE_SIM_BODY_CORNERS];
        const u32 _n = _cnt[_k] < FUDE_SIM_BODY_CORNERS ? _cnt[_k] : FUDE_SIM_BODY_CORNERS;
        for(u32 _i = 0; _i < _n; _i++) {
            _c[_i] = (fude_sim_v2){ _pts[_first + _i].x, _pts[_first + _i].y };
        }
        if(fude_sim_polygon_inside(_c, _n, _l)) {
            return true;
        }
        // (or within _tol of its edges)
        for(u32 _i = 0, _j = _n - 1u; _i < _n; _j = _i++) {
            if(rde_math_point_segment_distance_2d_f64(_l, _c[_j], _c[_i]) <= _tol) {
                return true;
            }
        }
        _first += _cnt[_k];
    }
    return false;
}

#define FZM_PIECES (RDE_PHYSICS_2D_MAX_COMPOUND_SHAPES + 1u)   // a body's pieces, at most: its first shape and those added

// Adds a drawing made a body (props.h) to the plan: its outline in the home frame, simplified, cut into convex pieces
// about its centre (sim/body.h), its mass from its area and its material (or as given); an open line: fixed thin
// pieces along it (the ground: a ramp, a track). The plan's unit shrinks to suit it.
RDE_INTERNAL void fzm_drawn(fude_zoom_mech_plan* _p, const fude_zoom_scene* _s, u32 _target, const fude_zoom_body_props* _bp, u32 _home) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    b8 _closed = false;
    if(!fude_zoom_scene_object_outline(_s, _target, 64u, &_pts, &_closed)) {
        rde_arr_free(&_pts);
        return;
    }
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _target);
    const fude_zoom_sim _up = fude_zoom_scene_sim(_s, _o->frame, _home);
    const u32 _n = (u32)rde_arr_length(&_pts);
    rde_arr _in_arr = rde_arr_new(sizeof(fude_sim_v2), _heap), _out_arr = rde_arr_new(sizeof(fude_sim_v2), _heap);
    rde_arr_resize(&_in_arr, _n);
    rde_arr_resize(&_out_arr, _n);
    fude_sim_v2* _in = (fude_sim_v2*)_in_arr.memory, *_out = (fude_sim_v2*)_out_arr.memory;   // (sized once: they stay put)
    f64 _x0 = 1e300, _y0 = 1e300, _x1 = -1e300, _y1 = -1e300;
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_v2 _h = fude_zoom_sim_apply(_up, ((const fude_zoom_v2*)_pts.memory)[_i]);
        _in[_i] = (fude_sim_v2){ _h.x, _h.y };
        _x0 = rde_math_min_f64(_x0, _h.x); _x1 = rde_math_max_f64(_x1, _h.x); _y0 = rde_math_min_f64(_y0, _h.y); _y1 = rde_math_max_f64(_y1, _h.y);
    }
    rde_arr_free(&_pts);
    const f64 _size = rde_math_max_f64(_x1 - _x0, _y1 - _y0);
    const fude_sim_material* _mat = fude_sim_material_at(_bp->material);
    _mat = _mat != NULL ? _mat : fude_sim_material_at(0u);
    fude_zoom_mech_body _b;
    rde_memset(&_b, 0, sizeof(_b));
    _b.object   = _target;
    _b.part     = fude_zoom_mech_find("drawn body");
    _b.fixed    = _bp->fixed || !_closed;
    _b.friction = _bp->friction >= 0.0 ? _bp->friction : _mat->friction;
    _b.bounce   = _bp->bounce >= 0.0 ? _bp->bounce : _mat->bounce;
    _b.strength = _mat->strength;
    _b.piece    = (u32)rde_arr_length(&_p->piece_counts);
    rde_arr _pp = rde_arr_new(sizeof(fude_sim_v2), _heap), _pc = rde_arr_new(sizeof(u32), _heap);
    fude_sim_v2 _centre = { (_x0 + _x1) * 0.5, (_y0 + _y1) * 0.5 };
    f64 _area = 0.0;
    if(_closed && _size > 0.0) {
        // (its shape kept to half a percent of its size, coarser only as far as the physics needs: FZM_PIECES pieces at
        // most, so a jagged drawing keeps its corners while they cut into that many; 120 corners at most, for the
        // cutting's time)
        f64 _tol = _size * 0.005;
        u32 _m = 0, _pieces = 0;
        for(u32 _try = 0; _try < 16u; _try++, _tol *= 1.6) {
            _m = fude_sim_outline_simplify(_in, _n, _tol, _out, _n);
            if(_m > 120u) {
                continue;
            }
            rde_arr_clear(&_pp);
            rde_arr_clear(&_pc);
            _pieces = _m >= 3u ? fude_sim_convex_pieces(_out, _m, FUDE_SIM_BODY_CORNERS, &_pp, &_pc) : 0u;
            if(_pieces > 0u && _pieces <= FZM_PIECES) {
                break;
            }
        }
        if(_m >= 3u && _pieces > 0u && _pieces <= FZM_PIECES) {
            _area   = fude_sim_polygon_area(_out, _m);
            _centre = fude_sim_polygon_centroid(_out, _m);
        } else {
            // (it crosses itself, or would not cut: its box)
            rde_arr_clear(&_pp);
            rde_arr_clear(&_pc);
            const fude_sim_v2 _box[4] = { { _x0, _y0 }, { _x1, _y0 }, { _x1, _y1 }, { _x0, _y1 } };
            const u32 _four = 4u;
            for(u32 _i = 0; _i < 4u; _i++) {
                rde_arr_add(&_pp, (any)&_box[_i]);
            }
            rde_arr_add(&_pc, (any)&_four);
            _area = (_x1 - _x0) * (_y1 - _y0);
        }
    } else if(_size > 0.0) {
        // An open line: a thin piece along each of its (simplified) stretches, as thick as it is drawn (or a little).
        f64 _tol = _size * 0.005;
        u32 _m = 0;
        for(u32 _try = 0; _try < 12u; _try++, _tol *= 1.6) {
            _m = fude_sim_polyline_simplify(_in, _n, _tol, _out, _n);
            if(_m <= 34u) {
                break;
            }
        }
        const f64 _half = rde_math_max_f64((f64)_o->radius * _o->scale * fude_zoom_sim_scale(_up), _size * 0.004);
        for(u32 _i = 0; _i + 1u < _m && _i < FZM_PIECES; _i++) {
            const f64 _dx = _out[_i + 1u].x - _out[_i].x, _dy = _out[_i + 1u].y - _out[_i].y, _l = rde_math_hypot_f64(_dx, _dy);
            if(!(_l > 0.0)) {
                continue;
            }
            const f64 _nx = -_dy / _l * _half, _ny = _dx / _l * _half;
            const fude_sim_v2 _q[4] = { { _out[_i].x - _nx, _out[_i].y - _ny }, { _out[_i + 1u].x - _nx, _out[_i + 1u].y - _ny },
                                        { _out[_i + 1u].x + _nx, _out[_i + 1u].y + _ny }, { _out[_i].x + _nx, _out[_i].y + _ny } };
            const u32 _four = 4u;
            for(u32 _k = 0; _k < 4u; _k++) {
                rde_arr_add(&_pp, (any)&_q[_k]);
            }
            rde_arr_add(&_pc, (any)&_four);
        }
    }
    const u32 _np = (u32)rde_arr_length(&_pc);
    if(_np > 0u) {
        const fude_sim_v2* _q = (const fude_sim_v2*)_pp.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_pp); _i++) {
            const fude_zoom_v2 _l = { _q[_i].x - _centre.x, _q[_i].y - _centre.y };
            rde_arr_add(&_p->piece_points, (any)&_l);
        }
        rde_memcpy(rde_arr_add_n(&_p->piece_counts, _np), _pc.memory, (usize)_np * sizeof(u32));
        _b.pieces = _np;
        _b.at     = (fude_zoom_v2){ _centre.x, _centre.y };
        _b.angle  = 0.0;
        _b.hw     = (_x1 - _x0) * 0.5;
        _b.hh     = (_y1 - _y0) * 0.5;
        // (kg: its density, its area in square metres (the home frame's units are millimetres), 10 mm thick)
        _b.mass   = _bp->mass > 0.0 ? _bp->mass : rde_math_max_f64(_mat->density * rde_math_abs_f64(_area) * 1e-6 * FUDE_SIM_BODY_THICKNESS, 1e-4);
        _b.value  = _b.mass;
        rde_arr_add(&_p->bodies, (any)&_b);
        const f64 _unit = rde_math_max_f64(rde_math_min_f64(_b.hw, _b.hh) * 0.5, _size * 0.02);
        _p->unit = _p->unit > 0.0 ? rde_math_min_f64(_p->unit, _unit) : _unit;
    }
    rde_arr_free(&_pp);
    rde_arr_free(&_pc);
    rde_arr_free(&_in_arr);
    rde_arr_free(&_out_arr);
}

// See the header. The teeth are counted as fude_zoom_mech_draw lays them (100000 at most).
u32 fude_zoom_mech_rack_teeth(const fude_zoom_mech_body* _rack, f64* _first, f64* _pitch, f64* _line) {
    // (as fude_zoom_mech_draw draws them: in its own units, a 5-point module's, their pitch line its)
    const f64 _k = _rack->scale > 0.0 ? _rack->scale : 1.0;
    const f64 _w = _rack->hw / _k, _h = _rack->hh / _k, _mod = fude_zoom_mech_rack_module(_h), _p = RDE_PI * _mod;
    u32 _n = 0;
    for(f64 _x = _w - _p * 0.5; _x > -_w + _p * 0.25 && _n < 100000u; _x -= _p) {
        _n++;
    }
    *_first = (_w - _p * 0.5) * _k;
    *_pitch = _p * _k;
    *_line  = fude_zoom_mech_rack_pitch_line(_h) * _k;
    return _n;
}

// See the header.
f64 fude_zoom_mech_tooth_at(const fude_zoom_mech_body* _gear, f64 _toward) {
    const f64 _x = (_toward - (_gear->angle + _gear->phase)) * (f64)_gear->part->teeth / (2.0 * RDE_PI);
    return _x - rde_math_floor_f64(_x);
}

// Returns the phase that puts _want (a fraction of a tooth) of gear _gear's teeth toward world angle _toward, by the
// least turn: within half a tooth either way.
RDE_INTERNAL f64 fzm_phase_for(const fude_zoom_mech_body* _gear, f64 _toward, f64 _want) {
    const f64 _tooth = 2.0 * RDE_PI / (f64)_gear->part->teeth;
    const f64 _phase = _toward - _gear->angle - _want * _tooth;
    return _phase - _tooth * rde_math_round_f64(_phase / _tooth);
}

// Sets racks meshed with gears to slide as far as some gear is over their teeth (each mesh's own travel: there it lets
// go); never less than where they are (0 stays within the travel).
RDE_INTERNAL void fzm_rack_travel(fude_zoom_mech_plan* _p) {
    const fude_zoom_mech_mesh* _m = (const fude_zoom_mech_mesh*)_p->meshes.memory;
    fude_zoom_mech_slide* _sl = (fude_zoom_mech_slide*)_p->slides.memory;
    for(u32 _s = 0; _s < (u32)rde_arr_length(&_p->slides); _s++) {
        f64 _lower = 0.0, _upper = 0.0;
        b8 _meshed = false;
        for(u32 _k = 0; _k < (u32)rde_arr_length(&_p->meshes); _k++) {
            if(_m[_k].rack && _m[_k].b == _sl[_s].body) {
                _lower  = rde_math_min_f64(_lower, _m[_k].lower);
                _upper  = rde_math_max_f64(_upper, _m[_k].upper);
                _meshed = true;
            }
        }
        if(_meshed) {
            _sl[_s].lower = _lower;
            _sl[_s].upper = _upper;
        }
    }
}

// Teeth in gaps: each gear meshed is turned (its phase) so its teeth meet the gaps of what it meshes with, along the
// gears' meshes (a train from its first); then each rack is slid (its shift) to its gear, a rack moving rather than
// the gear. A gear something else is on (a link pinned to it, a spring's or a rope's end) is not turned: those meshing
// with it are, to it.
RDE_INTERNAL void fzm_phases(fude_zoom_mech_plan* _p) {
    fude_zoom_mech_body* _b = (fude_zoom_mech_body*)_p->bodies.memory;
    const fude_zoom_mech_mesh* _m = (const fude_zoom_mech_mesh*)_p->meshes.memory;
    const u32 _nb = (u32)rde_arr_length(&_p->bodies), _nm = (u32)rde_arr_length(&_p->meshes);
    if(_nm == 0u) {
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _set_arr = rde_arr_new(sizeof(u8), _heap), _queue = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_set_arr, _nb);
    u8* _set = (u8*)_set_arr.memory;   // (1: its phase decided)
    #define FZM_HELD(_i) do { if((_i) != FUDE_ZOOM_NONE && (_i) < _nb) { _set[(_i)] = 1u; } } while(0)
    const fude_zoom_mech_hinge* _h = (const fude_zoom_mech_hinge*)_p->hinges.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_p->hinges); _i++) {
        if(_h[_i].b != FUDE_ZOOM_NONE) {
            FZM_HELD(_h[_i].a);
            FZM_HELD(_h[_i].b);
        }
    }
    const fude_zoom_mech_spring* _sp = (const fude_zoom_mech_spring*)_p->springs.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_p->springs); _i++) {
        FZM_HELD(_sp[_i].a);
        FZM_HELD(_sp[_i].b);
    }
    const fude_zoom_mech_rope* _r = (const fude_zoom_mech_rope*)_p->ropes.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_p->ropes); _i++) {
        FZM_HELD(_r[_i].a);
        FZM_HELD(_r[_i].b);
    }
    #undef FZM_HELD
    // Along the trains: from each gear decided (pinned to something), those it meshes with; a train with none decided,
    // from its first.
    for(u32 _pass = 0; _pass < 2u; _pass++) {
        for(u32 _k = 0; _k < _nm; _k++) {
            if(_m[_k].rack || _m[_k].belt) {
                continue;
            }
            const u32 _start = _set[_m[_k].a] ? _m[_k].a : (_set[_m[_k].b] ? _m[_k].b : (_pass == 1u ? _m[_k].a : FUDE_ZOOM_NONE));
            if(_start == FUDE_ZOOM_NONE) {
                continue;
            }
            _set[_start] = 1u;
            rde_arr_clear(&_queue);
            rde_arr_add(&_queue, (any)&_start);
            for(u32 _head = 0; _head < (u32)rde_arr_length(&_queue); _head++) {
                const u32 _v = ((const u32*)_queue.memory)[_head];
                for(u32 _e = 0; _e < _nm; _e++) {
                    const u32 _o = _m[_e].rack || _m[_e].belt ? FUDE_ZOOM_NONE : (_m[_e].a == _v ? _m[_e].b : (_m[_e].b == _v ? _m[_e].a : FUDE_ZOOM_NONE));
                    if(_o == FUDE_ZOOM_NONE || _set[_o]) {
                        continue;
                    }
                    // (where they touch: a tooth of one, a gap of the other — their places there add to
                    // a half, as they turn)
                    const f64 _toward = rde_math_atan2_f64(_b[_o].at.y - _b[_v].at.y, _b[_o].at.x - _b[_v].at.x);
                    const f64 _want = 0.5 - fude_zoom_mech_tooth_at(&_b[_v], _toward);
                    _b[_o].phase = fzm_phase_for(&_b[_o], _toward + RDE_PI, _want - rde_math_floor_f64(_want));
                    _set[_o] = 1u;
                    rde_arr_add(&_queue, (any)&_o);
                }
            }
        }
    }
    // Racks, to their gears (a rack slides: it is moved, the gear left as its train has it): where they touch, a tooth
    // of the gear's in a gap of the rack's. The gear over the rack: its tooth's place less the rack's kept as they go;
    // under it, the two added.
    for(u32 _k = 0; _k < _nm; _k++) {
        if(!_m[_k].rack || _set[_m[_k].b]) {
            continue;
        }
        const fude_zoom_mech_body* _g = &_b[_m[_k].a];
        fude_zoom_mech_body* _rk = &_b[_m[_k].b];
        const fude_zoom_v2 _u = { rde_math_cos_f64(_rk->angle), rde_math_sin_f64(_rk->angle) }, _n = { -_u.y, _u.x };
        const f64 _dx = _g->at.x - _rk->at.x, _dy = _g->at.y - _rk->at.y;
        const f64 _along = _dx * _u.x + _dy * _u.y, _up = _dx * _n.x + _dy * _n.y;
        f64 _first, _pitch, _line;
        fude_zoom_mech_rack_teeth(_rk, &_first, &_pitch, &_line);
        const f64 _pg = fude_zoom_mech_tooth_at(_g, _up >= 0.0 ? rde_math_atan2_f64(-_n.y, -_n.x) : rde_math_atan2_f64(_n.y, _n.x));
        const f64 _q = _up >= 0.0 ? _pg - 0.5 : 0.5 - _pg;   // (the rack's place there it needs)
        // (moved d along itself, the gear's place on it goes back d: (along − d − first) / pitch ≡ q)
        const f64 _d = _along - _first - _q * _pitch;
        _rk->shift = _d - _pitch * rde_math_round_f64(_d / _pitch);
        _set[_m[_k].b] = 1u;
    }
    rde_arr_free(&_set_arr);
    rde_arr_free(&_queue);
}

// Returns the turning part whose hole is within _tol of _q (a gear, a sprocket, a crank, a wheel on its axle: one a
// belt goes round), not _not; FUDE_ZOOM_NONE: none.
RDE_INTERNAL u32 fzm_belt_on(const fude_zoom_mech_plan* _p, fude_zoom_v2 _q, f64 _tol, u32 _not) {
    // (the topmost there, the last drawn: a sprocket on a gear's middle, not the gear under it)
    const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_p->bodies.memory;
    for(u32 _j = (u32)rde_arr_length(&_p->bodies); _j-- > 0u;) {
        if(_j != _not && fude_zoom_mech_belt_radius(&_b[_j]) > 0.0) {
            const fude_zoom_v2 _c = fzm_hole(&_b[_j], 0u);
            if(rde_math_hypot_f64(_c.x - _q.x, _c.y - _q.y) <= _tol) {
                return _j;
            }
        }
    }
    return FUDE_ZOOM_NONE;
}

// Returns whether bodies _a and _b (FUDE_ZOOM_NONE: the ground) are joined at _q already, by hinges there (within
// _tol), through others on the same pin. (Three parts on one pin: two hinges, not three; more fight each other.) At
// most 32 bodies on one pin are followed.
RDE_INTERNAL b8 fzm_pinned_there(const fude_zoom_mech_plan* _p, u32 _a, u32 _b, fude_zoom_v2 _q, f64 _tol) {
    if(_a == _b) {
        return true;
    }
    const fude_zoom_mech_hinge* _h = (const fude_zoom_mech_hinge*)_p->hinges.memory;
    const u32 _n = (u32)rde_arr_length(&_p->hinges);
    u32 _reach[32];   // (what _a reaches there: a pin holds few)
    u32 _count = 1;
    _reach[0] = _a;
    for(u32 _grew = 1; _grew;) {
        _grew = 0;
        for(u32 _k = 0; _k < _n; _k++) {
            if(rde_math_hypot_f64(_h[_k].at.x - _q.x, _h[_k].at.y - _q.y) > _tol) {
                continue;
            }
            b8 _has_a = false, _has_b = false;
            for(u32 _r = 0; _r < _count; _r++) {
                _has_a = _has_a || _reach[_r] == _h[_k].a;
                _has_b = _has_b || _reach[_r] == _h[_k].b;
            }
            if(_has_a != _has_b && _count < 32u) {
                _reach[_count++] = _has_a ? _h[_k].b : _h[_k].a;
                _grew = 1;
            }
        }
    }
    for(u32 _r = 0; _r < _count; _r++) {
        if(_reach[_r] == _b) {
            return true;
        }
    }
    return false;
}

// Returns whether _part, its hole on _on's middle, is keyed to turning part _on, on one shaft, as a camshaft's cams are
// on its sprocket: a cam on a gear, a sprocket or a hand crank; a sprocket on a gear.
RDE_INTERNAL b8 fzm_keyed(const fude_zoom_mech_body* _part, const fude_zoom_mech_body* _on) {
    const u8 _k = _on->part->kind;
    return (_part->part->kind == FUDE_ZOOM_MECH_CAM && (_k == FUDE_ZOOM_MECH_GEAR || _k == FUDE_ZOOM_MECH_SPROCKET || _k == FUDE_ZOOM_MECH_CRANK)) ||
           (_part->part->kind == FUDE_ZOOM_MECH_SPROCKET && _k == FUDE_ZOOM_MECH_GEAR);
}

// Belts and chains: each round the two turning parts its ends' middles are on, meshed as gears are, the one turned as
// far along its rim as the other (−a's radius over b's: the same way; crossed, +). A belt on fewer than two is left
// out.
RDE_INTERNAL void fzm_belts(fude_zoom_mech_plan* _p, f64 _tol) {
    const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_p->bodies.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_p->bodies); _i++) {
        if(_b[_i].part->kind != FUDE_ZOOM_MECH_BELT) {
            continue;
        }
        const f64 _reach = rde_math_max_f64(_tol, _b[_i].hh);   // (its end's middle within its own roundness of the wheel's)
        const u32 _a = fzm_belt_on(_p, fzm_hole(&_b[_i], 0u), _reach, FUDE_ZOOM_NONE);
        const u32 _c = _a != FUDE_ZOOM_NONE ? fzm_belt_on(_p, fzm_hole(&_b[_i], 1u), _reach, _a) : FUDE_ZOOM_NONE;
        if(_c == FUDE_ZOOM_NONE) {
            continue;
        }
        const b8 _crossed = _b[_i].value > 0.5;
        const fude_zoom_mech_belt _bt = { _i, _a, _c, fude_zoom_mech_belt_radius(&_b[_a]), fude_zoom_mech_belt_radius(&_b[_c]), _crossed,
                                          rde_strcmp(_b[_i].part->id, "chain") == 0 };
        rde_arr_add(&_p->belts, (any)&_bt);
        const fude_zoom_mech_mesh _m = { _a, _c, (_crossed ? 1.0 : -1.0) * _bt.ra / _bt.rb, 0.0, 0.0, 0.0, false, true };
        rde_arr_add(&_p->meshes, (any)&_m);
        _b = (const fude_zoom_mech_body*)_p->bodies.memory;
    }
}

// Dampers: their ends on what they are on (a body that moves; else the ground there), as springs' are. One on nothing
// that moves at either end is left out.
RDE_INTERNAL void fzm_dampers(fude_zoom_mech_plan* _p, f64 _tol) {
    const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_p->bodies.memory;
    const u32 _nb = (u32)rde_arr_length(&_p->bodies);
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind != FUDE_ZOOM_MECH_DAMPER) {
            continue;
        }
        fude_zoom_mech_damper _d = { _i, FUDE_ZOOM_NONE, FUDE_ZOOM_NONE, fzm_hole(&_b[_i], 0u), fzm_hole(&_b[_i], 1u), _b[_i].value };
        for(u32 _e = 0; _e < 2u; _e++) {
            const fude_zoom_v2 _at = _e == 0u ? _d.pa : _d.pb;
            for(u32 _j = 0; _j < _nb; _j++) {
                if(_j == _i || fzm_not_held(_b[_j].part->kind) || _b[_j].fixed) {
                    continue;
                }
                for(u32 _h = 0; _h < _b[_j].part->hole_count; _h++) {
                    const fude_zoom_v2 _q = fzm_hole(&_b[_j], _h);
                    if(rde_math_hypot_f64(_q.x - _at.x, _q.y - _at.y) <= _tol) {
                        if(_e == 0u) { _d.a = _j; } else { _d.b = _j; }
                    }
                }
            }
        }
        if(_d.a != FUDE_ZOOM_NONE || _d.b != FUDE_ZOOM_NONE) {
            rde_arr_add(&_p->dampers, (any)&_d);
        }
    }
}

#define FZM_WORM_TORQUE 200.0   // a worm's hold on its gear (N·m: it does not give; a worm is not turned back)

// Worms: a gear whose pitch circle is on a worm's pitch line (0.45 of its half height from its axis), along it: its
// axle (a hinge to the ground of its own, not a motor's or a shaft's) driven a tooth a turn of the worm, over it
// counter-clockwise as the worm turns on, under it clockwise.
RDE_INTERNAL void fzm_worms(fude_zoom_mech_plan* _p) {
    const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_p->bodies.memory;
    const u32 _nb = (u32)rde_arr_length(&_p->bodies);
    for(u32 _w = 0; _w < _nb; _w++) {
        if(_b[_w].part->kind != FUDE_ZOOM_MECH_WORM) {
            continue;
        }
        const fude_zoom_v2 _u = { rde_math_cos_f64(_b[_w].angle), rde_math_sin_f64(_b[_w].angle) }, _n = { -_u.y, _u.x };
        for(u32 _g = 0; _g < _nb; _g++) {
            if(_b[_g].part->kind != FUDE_ZOOM_MECH_GEAR) {
                continue;
            }
            const f64 _r = fude_zoom_mech_pitch(_b[_g].part, rde_math_min_f64(_b[_g].hw, _b[_g].hh)), _mod = 2.0 * _r / (f64)_b[_g].part->teeth;
            const f64 _dx = _b[_g].at.x - _b[_w].at.x, _dy = _b[_g].at.y - _b[_w].at.y;
            const f64 _along = _dx * _u.x + _dy * _u.y, _up = _dx * _n.x + _dy * _n.y;
            if(rde_math_abs_f64(rde_math_abs_f64(_up) - (0.45 * _b[_w].hh + _r)) > 0.6 * _mod + 0.15 * _b[_w].hh || rde_math_abs_f64(_along) > _b[_w].hw) {
                continue;
            }
            fude_zoom_mech_hinge* _h = (fude_zoom_mech_hinge*)_p->hinges.memory;
            for(u32 _k = 0; _k < (u32)rde_arr_length(&_p->hinges); _k++) {
                if(_h[_k].a == _g && _h[_k].b == FUDE_ZOOM_NONE && !_h[_k].motor && _h[_k].shaft == FUDE_ZOOM_NONE) {
                    const f64 _side = _up >= 0.0 ? 1.0 : -1.0;
                    _h[_k].motor  = true;
                    _h[_k].speed  = _side * 2.0 * RDE_PI * _b[_w].value / (f64)_b[_g].part->teeth;
                    _h[_k].torque = FZM_WORM_TORQUE;
                    const fude_zoom_mech_worm _wm = { _w, _g, _k, _side, _b[_w].value };
                    rde_arr_add(&_p->worms, (any)&_wm);
                    break;
                }
            }
        }
    }
}

// Pawls: each pressed onto the ratchet its tip is at (between its teeth's roots and tips); a pawl on none is listed
// with no ratchet. When its tip is on several, the last is taken.
RDE_INTERNAL void fzm_pawls(fude_zoom_mech_plan* _p, f64 _tol) {
    const fude_zoom_mech_body* _b = (const fude_zoom_mech_body*)_p->bodies.memory;
    const u32 _nb = (u32)rde_arr_length(&_p->bodies);
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind != FUDE_ZOOM_MECH_PAWL) {
            continue;
        }
        const fude_zoom_v2 _tip = { _b[_i].at.x + rde_math_cos_f64(_b[_i].angle) * 0.97 * _b[_i].hw, _b[_i].at.y + rde_math_sin_f64(_b[_i].angle) * 0.97 * _b[_i].hw };
        fude_zoom_mech_pawl _pw = { _i, FUDE_ZOOM_NONE };
        for(u32 _j = 0; _j < _nb; _j++) {
            if(_b[_j].part->kind != FUDE_ZOOM_MECH_RATCHET) {
                continue;
            }
            f64 _rt, _rr;
            fude_zoom_mech_ratchet_radii(&_b[_j], &_rt, &_rr);
            const f64 _d = rde_math_hypot_f64(_tip.x - _b[_j].at.x, _tip.y - _b[_j].at.y);
            if(_d >= _rr - _tol && _d <= _rt + _tol) {
                _pw.ratchet = _j;
            }
        }
        rde_arr_add(&_p->pawls, (any)&_pw);
    }
}

// The plan's bodies from the canvas's parts, for fude_zoom_mech_plan_build: each mechanism part in scope (_scope, NULL
// for all of them) alive and shown, or a circuit's part that becomes one (a motor's shaft, a solenoid's plunger),
// placed in the home frame _home of _s and its text read for its value; the plan's unit the smallest of their sizes.
RDE_INTERNAL void fzm_bodies(fude_zoom_mech_plan* _p, const fude_zoom_scene* _s, const u8* _scope, u32 _home) {
    rde_arr _n = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std());
    c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if((_scope != NULL && !_scope[_i]) || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || fude_zoom_scene_hides(_s, _o) || !fude_zoom_scene_frame_shown(_s, _o->frame)) {
            continue;
        }
        const fude_zoom_mech_part* _part = fude_zoom_mech_of(_s, _i);
        if(_part == NULL) {
            // (a circuit's motor, servo or stepper: its shaft, what is pinned on it turned as the circuit drives it; an
            // encoder's or a pot's, read as it turns; a solenoid's plunger)
            const fude_zoom_part* _ep = fude_zoom_part_of(_s, _i);
            const u8 _em = _ep != NULL ? _ep->model : FUDE_ZOOM_MODEL_NONE;
            _part = _em == FUDE_ZOOM_MODEL_MOTOR || _em == FUDE_ZOOM_MODEL_SERVO || _em == FUDE_ZOOM_MODEL_STEPPER || _em == FUDE_ZOOM_MODEL_ENCODER ||
                    _em == FUDE_ZOOM_MODEL_POT ? fude_zoom_mech_find("motor shaft") :
                    (_em == FUDE_ZOOM_MODEL_SOLENOID ? fude_zoom_mech_find("plunger") : NULL);
        }
        if(_part == NULL) {
            continue;
        }
        const u32 _count = fude_zoom_scene_shape_numbers_all(_s, _i, &_n);
        const f64* _num = (const f64*)_n.memory;
        if(_count < 3u) {
            continue;
        }
        const fude_zoom_sim _up = fude_zoom_sim_compose(fude_zoom_scene_sim(_s, _o->frame, _home), fude_zoom_object_sim(_o));
        const f64 _k = fude_zoom_sim_scale(_up);
        fude_zoom_mech_body _b;
        rde_memset(&_b, 0, sizeof(_b));
        _b.object = _i;
        _b.part   = _part;
        _b.at     = fude_zoom_sim_apply(_up, (fude_zoom_v2){ 0.0, 0.0 });
        _b.angle  = rde_math_atan2_f64(_up.b, _up.a);
        _b.hw     = _num[1] * _k;
        _b.hh     = _num[2] * _k;
        _b.scale  = _k;
        _b.fixed  = _part->kind == FUDE_ZOOM_MECH_PIVOT || _part->kind == FUDE_ZOOM_MECH_MOTOR || _part->kind == FUDE_ZOOM_MECH_WALL ||
                    _part->kind == FUDE_ZOOM_MECH_PULLEY || _part->kind == FUDE_ZOOM_MECH_RAIL || _part->kind == FUDE_ZOOM_MECH_SHAFT ||
                    _part->kind == FUDE_ZOOM_MECH_WORM;
        fude_zoom_symbol_text(_num, _count, _text, sizeof(_text));
        f64 _v = 0.0;
        const b8 _has = fude_zoom_circuit_value(_text, 0u, &_v);
        if(_part->kind == FUDE_ZOOM_MECH_MOTOR) {
            f64 _nm = 0.0;
            _b.value  = fude_zoom_mech_rpm(_text, 30.0) / 60.0;   // (rpm, with its way: turns a second, below 0 clockwise)
            _b.torque = fude_zoom_circuit_value(_text, 1u, &_nm) && _nm > 0.0 ? _nm : FZM_MOTOR_TORQUE;
        } else if(_part->kind == FUDE_ZOOM_MECH_WEIGHT || _part->kind == FUDE_ZOOM_MECH_CRATE) {
            // (kilograms: "2 kg" read as 2000 grams, "500 g" as 500, "2" as 2 kg)
            _b.value = _has && _v > 0.0 ? (rde_strchr(_text, 'g') != NULL ? _v / 1000.0 : _v) : (_part->kind == FUDE_ZOOM_MECH_CRATE ? 2.0 : 1.0);
        } else if(_part->kind == FUDE_ZOOM_MECH_SPRING || _part->kind == FUDE_ZOOM_MECH_DAMPER) {
            _b.value = _has && _v > 0.0 ? _v : 1.0;
        } else if(_part->kind == FUDE_ZOOM_MECH_WORM) {
            // (turns a second; a minus on its number, or clockwise said: the other way)
            _b.value = fude_zoom_mech_rpm(_text, 60.0) / 60.0;
        } else if(_part->kind == FUDE_ZOOM_MECH_BELT) {
            _b.value = rde_strstr(_text, "cross") != NULL ? 1.0 : 0.0;   // (crossed)
        } else if(_part->kind == FUDE_ZOOM_MECH_CRANK) {
            _b.value = _has && _v >= 0.0 ? _v : FUDE_ZOOM_MECH_CRANK_FRICTION;   // (its axle's friction, N·m)
        }
        rde_arr_add(&_p->bodies, (any)&_b);
        if(_part->kind == FUDE_ZOOM_MECH_SHAFT || _part->kind == FUDE_ZOOM_MECH_PLUNGER || _part->kind == FUDE_ZOOM_MECH_TRACER) {
            continue;   // (a circuit's part, a tracer's pen: not the mechanism's size, no body of its own to weigh)
        }
        const f64 _size = fude_zoom_mech_body_size(&_b);
        _p->unit = _p->unit > 0.0 ? rde_math_min_f64(_p->unit, _size) : _size;
    }
    rde_arr_free(&_n);
}

// The plan's hinges and springs among its _nb bodies _b, for fude_zoom_mech_plan_build (_tol: how near counts as on):
// holes over holes hinged (to the ground at a fixed part's, a motor's driving), but where one pin through them all
// joins them already; a cam or a sprocket on a turning part's middle keyed to it instead, noted in _keyed for the
// meshes (its handle taken under the name the function had, and handed back); a spring's ends to what they are on.
RDE_INTERNAL void fzm_hinges(fude_zoom_mech_plan* _p, const fude_zoom_mech_body* _b, u32 _nb, f64 _tol, rde_arr* _keyed) {
    rde_arr _keys = *_keyed;
    // Hinges: holes over holes (a fixed pivot's or a motor's: the ground there). Springs: their ends
    // to what they are on.
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind == FUDE_ZOOM_MECH_ROPE || _b[_i].part->kind == FUDE_ZOOM_MECH_PIN || _b[_i].part->kind == FUDE_ZOOM_MECH_BELT ||
           _b[_i].part->kind == FUDE_ZOOM_MECH_DAMPER) {
            continue;   // (below)
        }
        if(_b[_i].part->kind == FUDE_ZOOM_MECH_SPRING) {
            fude_zoom_mech_spring _sp = { _i, FUDE_ZOOM_NONE, FUDE_ZOOM_NONE, fzm_hole(&_b[_i], 0u), fzm_hole(&_b[_i], 1u), 0.0, _b[_i].value };
            _sp.length = rde_math_hypot_f64(_sp.pb.x - _sp.pa.x, _sp.pb.y - _sp.pa.y);
            for(u32 _e = 0; _e < 2u; _e++) {
                const fude_zoom_v2 _at = _e == 0u ? _sp.pa : _sp.pb;
                for(u32 _j = 0; _j < _nb; _j++) {
                    if(_j == _i || fzm_not_held(_b[_j].part->kind)) {
                        continue;
                    }
                    for(u32 _h = 0; _h < _b[_j].part->hole_count; _h++) {
                        const fude_zoom_v2 _q = fzm_hole(&_b[_j], _h);
                        if(rde_math_hypot_f64(_q.x - _at.x, _q.y - _at.y) <= _tol) {
                            if(_e == 0u) { _sp.a = _b[_j].fixed ? FUDE_ZOOM_NONE : _j; } else { _sp.b = _b[_j].fixed ? FUDE_ZOOM_NONE : _j; }
                        }
                    }
                }
            }
            if(_sp.a != FUDE_ZOOM_NONE || _sp.b != FUDE_ZOOM_NONE) {
                rde_arr_add(&_p->springs, (any)&_sp);
            }
            continue;
        }
        for(u32 _h = 0; _h < _b[_i].part->hole_count; _h++) {
            const fude_zoom_v2 _at = fzm_hole(&_b[_i], _h);
            for(u32 _j = _i + 1u; _j < _nb; _j++) {
                if(fzm_not_held(_b[_j].part->kind) || _b[_j].part->kind == FUDE_ZOOM_MECH_PIN || (_b[_i].fixed && _b[_j].fixed)) {
                    continue;
                }
                for(u32 _g = 0; _g < _b[_j].part->hole_count; _g++) {
                    const fude_zoom_v2 _q = fzm_hole(&_b[_j], _g);
                    if(rde_math_hypot_f64(_q.x - _at.x, _q.y - _at.y) > _tol) {
                        continue;
                    }
                    const b8 _cam_i = _g == 0u && fzm_keyed(&_b[_i], &_b[_j]), _cam_j = _h == 0u && fzm_keyed(&_b[_j], &_b[_i]);
                    if(_cam_i || _cam_j) {
                        // (a cam or a sprocket on a turning part's middle: keyed to it, on an axle of its own there,
                        // turned with it: below)
                        const fude_zoom_mech_mesh _key = { _cam_i ? _j : _i, _cam_i ? _i : _j, -1.0, 0.0, 0.0, 0.0, false, true };
                        rde_arr_add(&_keys, (any)&_key);
                        continue;
                    }
                    fude_zoom_mech_hinge _hg = { _i, _j, { (_at.x + _q.x) * 0.5, (_at.y + _q.y) * 0.5 }, false, 0.0, 0.0, FUDE_ZOOM_NONE, false };
                    const u32 _fixed = _b[_i].fixed ? _i : (_b[_j].fixed ? _j : FUDE_ZOOM_NONE);
                    if((_fixed == FUDE_ZOOM_NONE || _b[_fixed].part->kind == FUDE_ZOOM_MECH_PIVOT) &&
                       fzm_pinned_there(_p, _fixed == _i ? FUDE_ZOOM_NONE : _i, _fixed == _j ? FUDE_ZOOM_NONE : _j, _hg.at, _tol)) {
                        continue;   // (one pin through them all: these two joined there already, through the
                                    // others on it; a motor's or a circuit's shaft's own hinge kept: what drives it,
                                    // what reads it)
                    }
                    if(_fixed != FUDE_ZOOM_NONE) {
                        _hg.a = _fixed == _i ? _j : _i;
                        _hg.b = FUDE_ZOOM_NONE;
                        _hg.at = fzm_hole(&_b[_fixed], _fixed == _i ? _h : _g);   // (on the ground where the pivot is)
                        if(_b[_fixed].part->kind == FUDE_ZOOM_MECH_MOTOR) {
                            _hg.motor  = true;
                            _hg.speed  = 2.0 * RDE_PI * _b[_fixed].value;
                            _hg.torque = _b[_fixed].torque;
                        } else if(_b[_fixed].part->kind == FUDE_ZOOM_MECH_SHAFT) {
                            _hg.shaft = _fixed;
                        }
                    }
                    rde_arr_add(&_p->hinges, (any)&_hg);
                }
            }
        }
    }
    *_keyed = _keys;
}

// The plan's axles of their own among its _nb bodies _b, for fude_zoom_mech_plan_build: a hand crank's, and one for
// each gear, sprocket, cam, ratchet or pawl that nothing holds.
RDE_INTERNAL void fzm_axles(fude_zoom_mech_plan* _p, const fude_zoom_mech_body* _b, u32 _nb) {
    // A hand crank: on an axle of its own at its middle, turned by hand.
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind == FUDE_ZOOM_MECH_CRANK) {
            const fude_zoom_mech_hinge _axle = { _i, FUDE_ZOOM_NONE, _b[_i].at, false, 0.0, 0.0, FUDE_ZOOM_NONE, true };
            rde_arr_add(&_p->hinges, (any)&_axle);
        }
    }
    // A gear nothing holds (or a sprocket, a cam, a ratchet, a pawl): on an axle of its own where its hole is (they
    // turn, they do not fall).
    for(u32 _i = 0; _i < _nb; _i++) {
        const u8 _k = _b[_i].part->kind;
        if(_k != FUDE_ZOOM_MECH_GEAR && _k != FUDE_ZOOM_MECH_SPROCKET && _k != FUDE_ZOOM_MECH_CAM && _k != FUDE_ZOOM_MECH_RATCHET && _k != FUDE_ZOOM_MECH_PAWL) {
            continue;
        }
        b8 _held = false;
        const fude_zoom_mech_hinge* _h = (const fude_zoom_mech_hinge*)_p->hinges.memory;
        for(u32 _k = 0; _k < (u32)rde_arr_length(&_p->hinges); _k++) {
            _held = _held || _h[_k].a == _i || _h[_k].b == _i;
        }
        if(!_held) {
            const fude_zoom_mech_hinge _axle = { _i, FUDE_ZOOM_NONE, fzm_hole(&_b[_i], 0u), false, 0.0, 0.0, FUDE_ZOOM_NONE, false };
            rde_arr_add(&_p->hinges, (any)&_axle);
        }
    }
}

// The plan's tracers among its _nb bodies _b, for fude_zoom_mech_plan_build: each on the topmost body that moves under
// its point (within half of _tol), else on nothing.
RDE_INTERNAL void fzm_tracers(fude_zoom_mech_plan* _p, const fude_zoom_mech_body* _b, u32 _nb, f64 _tol) {
    // Tracers: each on the topmost thing that moves under its point (the last drawn), else on nothing.
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind != FUDE_ZOOM_MECH_TRACER) {
            continue;
        }
        fude_zoom_mech_tracer _t = { _i, FUDE_ZOOM_NONE, _b[_i].at };
        for(u32 _j = 0; _j < _nb; _j++) {
            const u8 _k = _b[_j].part->kind;
            if(_j != _i && !_b[_j].fixed && !fzm_not_held(_k) && _k != FUDE_ZOOM_MECH_PIN &&
               _k != FUDE_ZOOM_MECH_TRACER && _k != FUDE_ZOOM_MECH_PULLEY && fzm_on(_p, &_b[_j], _t.at, _tol * 0.5)) {
                _t.on = _j;
            }
        }
        rde_arr_add(&_p->tracers, (any)&_t);
    }
}

// The plan's pins among its _nb bodies _b, for fude_zoom_mech_plan_build: the moving bodies under each (within half of
// _tol) hinged together there, and to the ground when one is alone or a fixed part is under it too.
RDE_INTERNAL void fzm_pins(fude_zoom_mech_plan* _p, const fude_zoom_mech_body* _b, u32 _nb, f64 _tol) {
    // Pins: the bodies under each hinged together there; one alone, or a fixed thing under it too: to the ground.
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind != FUDE_ZOOM_MECH_PIN) {
            continue;
        }
        const fude_zoom_v2 _q = _b[_i].at;
        // (each moving body under it hinged to the first, as it is found: however many there are, none kept)
        u32 _first = FUDE_ZOOM_NONE, _m = 0;
        b8 _ground = false;
        for(u32 _j = 0; _j < _nb; _j++) {
            const u8 _k = _b[_j].part->kind;
            if(_j == _i || fzm_not_held(_k) || _k == FUDE_ZOOM_MECH_PIN || _k == FUDE_ZOOM_MECH_TRACER ||
               !fzm_on(_p, &_b[_j], _q, _tol * 0.5)) {
                continue;
            }
            if(_b[_j].fixed) {
                _ground = true;
            } else if(_m++ == 0u) {
                _first = _j;
            } else {
                const fude_zoom_mech_hinge _hg = { _first, _j, _q, false, 0.0, 0.0, FUDE_ZOOM_NONE, false };
                rde_arr_add(&_p->hinges, (any)&_hg);
            }
        }
        if(_m > 0u && (_ground || _m == 1u)) {
            const fude_zoom_mech_hinge _hg = { _first, FUDE_ZOOM_NONE, _q, false, 0.0, 0.0, FUDE_ZOOM_NONE, false };
            rde_arr_add(&_p->hinges, (any)&_hg);
        }
    }
}

// The plan's ropes among its _nb bodies _b, for fude_zoom_mech_plan_build: each end on a body, else off a pulley's rim,
// else held where it is (_tol: how near counts as on); then each two over one pulley paired.
RDE_INTERNAL void fzm_ropes(fude_zoom_mech_plan* _p, const fude_zoom_mech_body* _b, u32 _nb, f64 _tol) {
    // Ropes: each end on a body (anywhere on it), else off a pulley's rim, else held still where it is; two down one
    // pulley's sides one rope over it.
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind != FUDE_ZOOM_MECH_ROPE) {
            continue;
        }
        fude_zoom_mech_rope _r = { _i, FUDE_ZOOM_NONE, FUDE_ZOOM_NONE, fzm_hole(&_b[_i], 0u), fzm_hole(&_b[_i], 1u), 0.0, FUDE_ZOOM_NONE, FUDE_ZOOM_NONE };
        _r.length = rde_math_hypot_f64(_r.pb.x - _r.pa.x, _r.pb.y - _r.pa.y);
        u32 _over[2] = { FUDE_ZOOM_NONE, FUDE_ZOOM_NONE };
        for(u32 _e = 0; _e < 2u; _e++) {
            const fude_zoom_v2 _q = _e == 0u ? _r.pa : _r.pb;
            u32 _held = FUDE_ZOOM_NONE;
            for(u32 _j = 0; _j < _nb && _held == FUDE_ZOOM_NONE; _j++) {
                const u8 _k = _b[_j].part->kind;
                if(_j != _i && !_b[_j].fixed && !fzm_not_held(_k) && _k != FUDE_ZOOM_MECH_TRACER &&
                   fzm_on(_p, &_b[_j], _q, _tol * 0.5)) {
                    _held = _j;
                }
            }
            for(u32 _j = 0; _j < _nb && _held == FUDE_ZOOM_NONE && _over[_e] == FUDE_ZOOM_NONE; _j++) {
                if(_b[_j].part->kind == FUDE_ZOOM_MECH_PULLEY &&
                   rde_math_hypot_f64(_q.x - _b[_j].at.x, _q.y - _b[_j].at.y) <= rde_math_min_f64(_b[_j].hw, _b[_j].hh) + _tol) {
                    _over[_e] = _j;
                }
            }
            if(_e == 0u) { _r.a = _held; } else { _r.b = _held; }
        }
        // (the end off a pulley its b end)
        if(_over[0] != FUDE_ZOOM_NONE && _over[1] == FUDE_ZOOM_NONE) {
            const u32 _t = _r.a; _r.a = _r.b; _r.b = _t;
            const fude_zoom_v2 _tp = _r.pa; _r.pa = _r.pb; _r.pb = _tp;
            _over[1] = _over[0];
            _over[0] = FUDE_ZOOM_NONE;
        }
        _r.over = _r.b == FUDE_ZOOM_NONE ? _over[1] : FUDE_ZOOM_NONE;
        rde_arr_add(&_p->ropes, (any)&_r);   // (held at both ends too: drawn, nothing to keep)
    }
    {
        fude_zoom_mech_rope* _r = (fude_zoom_mech_rope*)_p->ropes.memory;
        const u32 _nr = (u32)rde_arr_length(&_p->ropes);
        for(u32 _i = 0; _i < _nr; _i++) {
            for(u32 _j = _i + 1u; _j < _nr && _r[_i].over != FUDE_ZOOM_NONE && _r[_i].pair == FUDE_ZOOM_NONE; _j++) {
                if(_r[_j].over == _r[_i].over && _r[_j].pair == FUDE_ZOOM_NONE) {
                    _r[_i].pair = _j;
                    _r[_j].pair = _i;
                }
            }
        }
    }
}

// The plan's slides among its _nb bodies _b, for fude_zoom_mech_plan_build: a solenoid's plunger's, a follower's and
// a rack's along themselves, and a slider's along the rail it is on.
RDE_INTERNAL void fzm_slides(fude_zoom_mech_plan* _p, const fude_zoom_mech_body* _b, u32 _nb) {
    // Sliders on rails: along the rail's line, between its ends. Racks: along their own length (as far as half of it
    // each way). A solenoid's plunger: along it, in as far as its stroke.
    for(u32 _i = 0; _i < _nb; _i++) {
        const u8 _k = _b[_i].part->kind;
        if(_k == FUDE_ZOOM_MECH_PLUNGER) {
            const fude_zoom_mech_slide _sl = { _i, _b[_i].at, { rde_math_cos_f64(_b[_i].angle), rde_math_sin_f64(_b[_i].angle) }, -FUDE_ZOOM_SOLENOID_STROKE * _b[_i].hw, 0.0 };
            rde_arr_add(&_p->slides, (any)&_sl);
            continue;
        }
        if(_k == FUDE_ZOOM_MECH_FOLLOWER) {
            // (a follower: along itself in its guide, its stroke either way)
            const f64 _st = FUDE_ZOOM_MECH_FOLLOWER_STROKE * _b[_i].hw;
            const fude_zoom_mech_slide _sl = { _i, _b[_i].at, { rde_math_cos_f64(_b[_i].angle), rde_math_sin_f64(_b[_i].angle) }, -_st, _st };
            rde_arr_add(&_p->slides, (any)&_sl);
            continue;
        }
        if(_k == FUDE_ZOOM_MECH_RACK) {
            const fude_zoom_mech_slide _sl = { _i, _b[_i].at, { rde_math_cos_f64(_b[_i].angle), rde_math_sin_f64(_b[_i].angle) }, -_b[_i].hw * 0.5, _b[_i].hw * 0.5 };
            rde_arr_add(&_p->slides, (any)&_sl);
            continue;
        }
        if(_k != FUDE_ZOOM_MECH_SLIDER) {
            continue;
        }
        for(u32 _j = 0; _j < _nb; _j++) {
            if(_b[_j].part->kind != FUDE_ZOOM_MECH_RAIL) {
                continue;
            }
            const fude_zoom_v2 _u = { rde_math_cos_f64(_b[_j].angle), rde_math_sin_f64(_b[_j].angle) };
            const f64 _dx = _b[_i].at.x - _b[_j].at.x, _dy = _b[_i].at.y - _b[_j].at.y;
            const f64 _along = _dx * _u.x + _dy * _u.y, _across = -_dx * _u.y + _dy * _u.x;
            if(rde_math_abs_f64(_across) <= _b[_j].hh + _b[_i].hh * 0.5 && rde_math_abs_f64(_along) <= _b[_j].hw) {
                const f64 _room = rde_math_max_f64(_b[_j].hw - _b[_i].hw, 0.0);
                const fude_zoom_mech_slide _sl = { _i, { _b[_j].at.x + _u.x * _along, _b[_j].at.y + _u.y * _along }, _u, -_room - _along, _room - _along };
                rde_arr_add(&_p->slides, (any)&_sl);
                break;
            }
        }
    }
}

// The plan's gear meshes among its _nb bodies _b, for fude_zoom_mech_plan_build: a gear on a rack, then each two gears
// whose pitch circles touch.
RDE_INTERNAL void fzm_meshes(fude_zoom_mech_plan* _p, const fude_zoom_mech_body* _b, u32 _nb) {
    // A gear on a rack (its pitch circle on the rack's pitch line, over it): meshed — its turn and the rack's travel
    // kept together (a's turn + ratio · b's travel).
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind != FUDE_ZOOM_MECH_GEAR) {
            continue;
        }
        const f64 _r = fude_zoom_mech_pitch(_b[_i].part, rde_math_min_f64(_b[_i].hw, _b[_i].hh));
        for(u32 _j = 0; _j < _nb; _j++) {
            if(_b[_j].part->kind != FUDE_ZOOM_MECH_RACK) {
                continue;
            }
            const fude_zoom_v2 _u = { rde_math_cos_f64(_b[_j].angle), rde_math_sin_f64(_b[_j].angle) }, _n = { -_u.y, _u.x };
            const f64 _dx = _b[_i].at.x - _b[_j].at.x, _dy = _b[_i].at.y - _b[_j].at.y;
            const f64 _along = _dx * _u.x + _dy * _u.y, _up = _dx * _n.x + _dy * _n.y;
            f64 _first, _pitch, _line;
            const u32 _teeth = fude_zoom_mech_rack_teeth(&_b[_j], &_first, &_pitch, &_line);
            const f64 _mod = 2.0 * _r / (f64)_b[_i].part->teeth;
            const b8 _above = rde_math_abs_f64(_up - (_line + _r)) <= 0.6 * _mod, _below = rde_math_abs_f64(_up + (_line + _r)) <= 0.6 * _mod;
            if((_above || _below) && rde_math_abs_f64(_along) <= _b[_j].hw) {
                // (the rack goes as the gear's rim where they touch: over it, turning anticlockwise, the rack goes on,
                // its travel r a radian: a's turn − b's travel / r kept; under it, the other way. As far as it goes
                // while the gear is on its teeth: the gear's middle along it from a tooth past its last one to a tooth
                // past its first; run off them, the gear's teeth clear of the rack's)
                const f64 _side = _above ? 1.0 : -1.0;
                const f64 _last = _first - _pitch * (f64)(_teeth > 0u ? _teeth - 1u : 0u);
                const fude_zoom_mech_mesh _m = { _i, _j, -_side / _r, _along - (_first + _pitch), _along - (_last - _pitch), 2.0 * RDE_PI / (f64)_b[_i].part->teeth, true, false };
                rde_arr_add(&_p->meshes, (any)&_m);
            }
        }
    }
    // Gears whose pitch circles touch: meshed.
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind != FUDE_ZOOM_MECH_GEAR) {
            continue;
        }
        const f64 _ri = fude_zoom_mech_pitch(_b[_i].part, rde_math_min_f64(_b[_i].hw, _b[_i].hh));
        for(u32 _j = _i + 1u; _j < _nb; _j++) {
            if(_b[_j].part->kind != FUDE_ZOOM_MECH_GEAR) {
                continue;
            }
            const f64 _rj = fude_zoom_mech_pitch(_b[_j].part, rde_math_min_f64(_b[_j].hw, _b[_j].hh));
            const f64 _d = rde_math_hypot_f64(_b[_j].at.x - _b[_i].at.x, _b[_j].at.y - _b[_i].at.y);
            const f64 _mod = 2.0 * _ri / (f64)_b[_i].part->teeth;
            if(rde_math_abs_f64(_d - (_ri + _rj)) <= 0.6 * _mod) {
                const fude_zoom_mech_mesh _m = { _i, _j, (f64)_b[_i].part->teeth / (f64)_b[_j].part->teeth, 0.0, 0.0, 2.0 * RDE_PI / (f64)_b[_i].part->teeth, false, false };
                rde_arr_add(&_p->meshes, (any)&_m);
            }
        }
    }
}

// See the header. First the bodies: each mechanism part (or circuit part that becomes one) alive and shown, its text
// read for its value, then each drawing made a body; the plan's unit is the smallest part's size, and joints are found
// within half of it. Then the joints, in the order the steps below say. Most steps look at every pair of bodies (holes
// over holes, a pin's, a rope's ends, tracers, dampers, worms, pawls): O(n²), but once a Play, over the parts drawn —
// a few dozen. Measured (an optimised build): the 55 bodies of Everything at once take 0.1 ms, the Gearbox's 12 under
// 0.01 ms, so a spatial lookup would buy nothing that shows.
u32 fude_zoom_mech_plan_build(fude_zoom_mech_plan* _p, const fude_zoom_scene* _s, const u8* _scope) {
    rde_arr_clear(&_p->bodies);
    rde_arr_clear(&_p->hinges);
    rde_arr_clear(&_p->springs);
    rde_arr_clear(&_p->meshes);
    rde_arr_clear(&_p->ropes);
    rde_arr_clear(&_p->slides);
    rde_arr_clear(&_p->tracers);
    rde_arr_clear(&_p->belts);
    rde_arr_clear(&_p->dampers);
    rde_arr_clear(&_p->worms);
    rde_arr_clear(&_p->pawls);
    rde_arr_clear(&_p->piece_points);
    rde_arr_clear(&_p->piece_counts);
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    _p->unit = 0.0;
    fzm_bodies(_p, _s, _scope, _home);
    // Drawings made bodies (props.h): theirs, or their properties', in the scope.
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        if(fude_zoom_props_kind(_s, _i) != FUDE_ZOOM_PROPS_BODY) {
            continue;
        }
        const u32 _t = fude_zoom_props_target(_s, _i);
        if(_t == FUDE_ZOOM_NONE) {
            continue;
        }
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _t);
        if((_scope != NULL && !_scope[_t] && !_scope[_i]) || fude_zoom_scene_hides(_s, _o) || !fude_zoom_scene_frame_shown(_s, _o->frame)) {
            continue;
        }
        fude_zoom_body_props _bp;
        if(fude_zoom_props_body(_s, _i, &_bp)) {
            fzm_drawn(_p, _s, _t, &_bp, _home);
        }
    }
    if(!(_p->unit > 0.0)) {
        _p->unit = 1.0;
    }
    fude_zoom_mech_body* _b = (fude_zoom_mech_body*)_p->bodies.memory;
    const u32 _nb = (u32)rde_arr_length(&_p->bodies);
    const f64 _tol = 0.5 * _p->unit;
    rde_arr _keys = rde_arr_new(sizeof(fude_zoom_mech_mesh), rde_memory_allocator_get_default_std());
    fzm_hinges(_p, _b, _nb, _tol, &_keys);
    fzm_axles(_p, _b, _nb);
    // Cams and sprockets keyed (as a belt is, a coupling with no teeth: turned as far as what they are on, its axle's
    // own or its motor's, the same way).
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_keys); _i++) {
        rde_arr_add(&_p->meshes, (any)&((const fude_zoom_mech_mesh*)_keys.memory)[_i]);
    }
    rde_arr_free(&_keys);
    fzm_tracers(_p, _b, _nb, _tol);
    fzm_pins(_p, _b, _nb, _tol);
    fzm_ropes(_p, _b, _nb, _tol);
    fzm_slides(_p, _b, _nb);
    fzm_meshes(_p, _b, _nb);
    fzm_belts(_p, _tol);
    fzm_dampers(_p, _tol);
    fzm_worms(_p);
    fzm_pawls(_p, _tol);
    fzm_rack_travel(_p);
    fzm_phases(_p);
    return _nb;
}
