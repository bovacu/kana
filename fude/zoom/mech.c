// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/mech.h"
#include "zoom/symbol.h"
#include "zoom/shape.h"
#include "zoom/circuit.h"
#include "zoom/props.h"
#include "sim/body.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FZM_PI 3.14159265358979323846

// (Gears: a module of 5 points as they come — their pitch circle 5 × teeth across —, so any two mesh.)
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
};
#define FZM_N ((u32)(sizeof(FZM_PARTS) / sizeof(FZM_PARTS[0])))

const fude_zoom_mech_part* fude_zoom_mech_find(const c8* _id) {
    for(u32 _i = 0; _id != NULL && _i < FZM_N; _i++) {
        if(strcmp(FZM_PARTS[_i].id, _id) == 0) {
            return &FZM_PARTS[_i];
        }
    }
    return NULL;
}

const fude_zoom_mech_part* fude_zoom_mech_of_kind(u32 _kind) {
    static const fude_zoom_mech_part* _cache[1024];
    static u8 _known[1024];
    const fude_zoom_symbol_info* _info;
    if(_kind >= 1024u) {
        _info = fude_zoom_symbol_info_of(_kind);
        return _info != NULL ? fude_zoom_mech_find(_info->id) : NULL;
    }
    if(!_known[_kind]) {
        _info = fude_zoom_symbol_info_of(_kind);
        _cache[_kind] = _info != NULL ? fude_zoom_mech_find(_info->id) : NULL;
        _known[_kind] = 1u;
    }
    return _cache[_kind];
}

const fude_zoom_mech_part* fude_zoom_mech_of(const fude_zoom_scene* _s, u32 _object) {
    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
    if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind != FUDE_ZOOM_KIND_SHAPE || _o->channels != FUDE_ZOOM_SHAPE_SYMBOL) {
        return NULL;
    }
    f64 _n[1];
    if(fude_zoom_scene_shape_numbers(_s, _object, _n, 1u) < 1u || !(_n[0] >= 0.0)) {
        return NULL;
    }
    return fude_zoom_mech_of_kind((u32)_n[0]);
}

f64 fude_zoom_mech_pitch(const fude_zoom_mech_part* _part, f64 _hw) {
    return _part != NULL && _part->teeth > 0u ? _hw * (f64)_part->teeth / ((f64)_part->teeth + 2.0) : _hw;
}

// --- drawn -------------------------------------------------------------------------------------------

typedef struct {
    rde_arr* points;
    rde_arr* parts;
    f64      hw, hh;
    u32      segments;
    u32      first;
} fzm;

RDE_INTERNAL void fzm_pt(fzm* _d, f64 _x, f64 _y) {
    const fude_zoom_v2 _p = { _x, _y };
    rde_arr_add(_d->points, (any)&_p);
}
RDE_INTERNAL void fzm_begin(fzm* _d) {
    _d->first = (u32)rde_arr_length(_d->points);
}
RDE_INTERNAL void fzm_end(fzm* _d, u8 _flags) {
    const u32 _n = (u32)rde_arr_length(_d->points) - _d->first;
    if(_n >= 2u) {
        const fude_zoom_symbol_part _p = { _d->first, _n, _flags };
        rde_arr_add(_d->parts, (any)&_p);
    } else {
        _d->points->count -= _n;
    }
}
RDE_INTERNAL void fzm_line(fzm* _d, f64 _x0, f64 _y0, f64 _x1, f64 _y1) {
    fzm_begin(_d);
    fzm_pt(_d, _x0, _y0);
    fzm_pt(_d, _x1, _y1);
    fzm_end(_d, 0u);
}
RDE_INTERNAL void fzm_arc(fzm* _d, f64 _cx, f64 _cy, f64 _r, f64 _a0, f64 _a1) {
    u32 _k = (u32)ceil((f64)(_d->segments < 16u ? 16u : _d->segments) * fabs(_a1 - _a0) / (2.0 * FZM_PI));
    _k = _k < 3u ? 3u : _k;
    for(u32 _i = 0; _i <= _k; _i++) {
        const f64 _a = _a0 + (_a1 - _a0) * (f64)_i / (f64)_k;
        fzm_pt(_d, _cx + cos(_a) * _r, _cy + sin(_a) * _r);
    }
}
RDE_INTERNAL void fzm_circle(fzm* _d, f64 _cx, f64 _cy, f64 _r, u8 _flags) {
    fzm_begin(_d);
    const u32 _k = _d->segments < 16u ? 16u : _d->segments;
    for(u32 _i = 0; _i < _k; _i++) {
        const f64 _a = 2.0 * FZM_PI * (f64)_i / (f64)_k;
        fzm_pt(_d, _cx + cos(_a) * _r, _cy + sin(_a) * _r);
    }
    fzm_end(_d, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}
// The outline round circles _r round each of _c (a convex polygon's corners, counter-clockwise): a plate's.
RDE_INTERNAL void fzm_round_hull(fzm* _d, const fude_zoom_v2* _c, u32 _n, f64 _r, u8 _flags) {
    fzm_begin(_d);
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_v2 _p = _c[(_i + _n - 1u) % _n], _q = _c[_i], _nx = _c[(_i + 1u) % _n];
        const f64 _a0 = atan2(_q.y - _p.y, _q.x - _p.x) - FZM_PI * 0.5, _a1 = atan2(_nx.y - _q.y, _nx.x - _q.x) - FZM_PI * 0.5;
        f64 _b1 = _a1;
        while(_b1 < _a0) {
            _b1 += 2.0 * FZM_PI;
        }
        fzm_arc(_d, _q.x, _q.y, _r, _a0, _b1);
    }
    fzm_end(_d, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}

u32 fude_zoom_mech_draw(const fude_zoom_mech_part* _part, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts) {
    rde_arr_clear(_points);
    rde_arr_clear(_parts);
    if(_part == NULL) {
        return 0;
    }
    fzm _d = { _points, _parts, fabs(_hw), fabs(_hh), _segments < 16u ? 16u : (_segments > 128u ? 128u : _segments), 0u };
    const f64 _w = _d.hw, _h = _d.hh, _m = fmin(_w, _h);
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
    case FUDE_ZOOM_MECH_GEAR: {
        const u32 _t = _part->teeth;
        const f64 _ra = _m, _rp = fude_zoom_mech_pitch(_part, _m), _mod = 2.0 * _rp / (f64)_t, _rr = _rp - 1.25 * _mod;
        fzm_begin(&_d);
        for(u32 _k = 0; _k < _t; _k++) {
            const f64 _a = 2.0 * FZM_PI * (f64)_k / (f64)_t, _p = 2.0 * FZM_PI / (f64)_t;
            const f64 _at[4] = { -0.26, -0.13, 0.13, 0.26 };
            const f64 _r[4]  = { _rr, _ra, _ra, _rr };
            for(u32 _j = 0; _j < 4u; _j++) {
                fzm_pt(&_d, cos(_a + _at[_j] * _p) * _r[_j], sin(_a + _at[_j] * _p) * _r[_j]);
            }
        }
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        fzm_circle(&_d, 0.0, 0.0, _ra * 0.12, 0u);
        // (a line out from its middle: its turn seen)
        fzm_line(&_d, _ra * 0.12, 0.0, _rr * 0.75, 0.0);
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
    case FUDE_ZOOM_MECH_MOTOR:
        fzm_circle(&_d, 0.0, 0.0, _m * 0.8, F);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.14, 0u);
        fzm_begin(&_d);
        fzm_arc(&_d, 0.0, 0.0, _m * 0.98, FZM_PI * 0.15, FZM_PI * 0.85);
        fzm_end(&_d, 0u);
        fzm_begin(&_d);   // (its arrow's head: it turns counter-clockwise)
        fzm_pt(&_d, cos(FZM_PI * 0.85) * _m * 0.98, sin(FZM_PI * 0.85) * _m * 0.98);
        fzm_pt(&_d, cos(FZM_PI * 0.78) * _m * 0.80, sin(FZM_PI * 0.78) * _m * 0.80 + 0.1 * _m);
        fzm_pt(&_d, cos(FZM_PI * 0.78) * _m * 1.12, sin(FZM_PI * 0.78) * _m * 1.12 + 0.1 * _m);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | S);
        break;
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
    case FUDE_ZOOM_MECH_WEIGHT:
        fzm_circle(&_d, 0.0, 0.0, _m * 0.95, F);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.14, 0u);
        break;
    case FUDE_ZOOM_MECH_WHEEL:
        fzm_circle(&_d, 0.0, 0.0, _m * 0.95, F);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.2, 0u);
        for(u32 _k = 0; _k < 4u; _k++) {
            const f64 _a = FZM_PI * 0.5 * (f64)_k;
            fzm_line(&_d, cos(_a) * _m * 0.2, sin(_a) * _m * 0.2, cos(_a) * _m * 0.95, sin(_a) * _m * 0.95);
        }
        break;
    case FUDE_ZOOM_MECH_PULLEY:
        fzm_circle(&_d, 0.0, 0.0, _m * 0.95, F);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.78, 0u);   // (its groove: where the rope runs)
        fzm_circle(&_d, 0.0, 0.0, _m * 0.14, 0u);
        for(u32 _k = 0; _k < 3u; _k++) {
            const f64 _a = FZM_PI * 2.0 / 3.0 * (f64)_k + FZM_PI * 0.5;
            fzm_line(&_d, cos(_a) * _m * 0.14, sin(_a) * _m * 0.14, cos(_a) * _m * 0.78, sin(_a) * _m * 0.78);
        }
        break;
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
    case FUDE_ZOOM_MECH_CRATE: {
        const f64 _top = _h * 0.7;   // (its hook above it)
        fzm_begin(&_d);
        fzm_pt(&_d, -_w, -_h); fzm_pt(&_d, _w, -_h); fzm_pt(&_d, _w, _top); fzm_pt(&_d, -_w, _top);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        const f64 _b = fmin(_w, _h) * 0.16;
        fzm_begin(&_d);
        fzm_pt(&_d, -_w + _b, -_h + _b); fzm_pt(&_d, _w - _b, -_h + _b); fzm_pt(&_d, _w - _b, _top - _b); fzm_pt(&_d, -_w + _b, _top - _b);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED);
        fzm_line(&_d, -_w + _b, -_h + _b, _w - _b, _top - _b);
        fzm_line(&_d, -_w + _b, _top - _b, _w - _b, -_h + _b);
        fzm_begin(&_d);
        fzm_arc(&_d, 0.0, _top + (_h - _top) * 0.5, (_h - _top) * 0.45, -FZM_PI * 0.5, FZM_PI * 1.5);
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
        // (a bar, teeth along its top as a 5-point module's: 5π apart, their pitch line its)
        const f64 _pl = fude_zoom_mech_rack_pitch_line(_h), _mod = fmin(5.0, _h * 0.5);
        const f64 _p = FZM_PI * _mod, _base = _pl - 1.25 * _mod, _tip = _pl + _mod;
        fzm_begin(&_d);
        fzm_pt(&_d, -_w, -_h);
        fzm_pt(&_d, _w, -_h);
        fzm_pt(&_d, _w, _base);
        for(f64 _x = _w - _p * 0.5; _x > -_w + _p * 0.25; _x -= _p) {
            fzm_pt(&_d, fmin(_x + _p * 0.26, _w), _base);
            fzm_pt(&_d, _x + _p * 0.13, _tip);
            fzm_pt(&_d, _x - _p * 0.13, _tip);
            fzm_pt(&_d, fmax(_x - _p * 0.26, -_w), _base);
        }
        fzm_pt(&_d, -_w, _base);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        break;
    }
    case FUDE_ZOOM_MECH_PIN:
        fzm_circle(&_d, 0.0, 0.0, _m * 0.9, F);
        fzm_circle(&_d, 0.0, 0.0, _m * 0.3, S);
        break;
    case FUDE_ZOOM_MECH_WALL:
    default:
        fzm_begin(&_d);
        fzm_pt(&_d, -_w, -_h); fzm_pt(&_d, _w, -_h); fzm_pt(&_d, _w, _h); fzm_pt(&_d, -_w, _h);
        fzm_end(&_d, FUDE_ZOOM_SYMBOL_CLOSED | F);
        for(f64 _x = -_w + _h; _x < _w + _h; _x += 2.0 * _h) {   // (hatched)
            fzm_line(&_d, fmax(_x - _h, -_w), _x - _h < -_w ? -_h + (-_w - (_x - _h)) : -_h, fmin(_x + _h, _w), _x + _h > _w ? _h - (_x + _h - _w) : _h);
        }
        break;
    }
    return (u32)rde_arr_length(_parts);
}

// --- the plan ----------------------------------------------------------------------------------------

void fude_zoom_mech_plan_init(fude_zoom_mech_plan* _p) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _p->bodies  = rde_arr_new(sizeof(fude_zoom_mech_body), _heap);
    _p->hinges  = rde_arr_new(sizeof(fude_zoom_mech_hinge), _heap);
    _p->springs = rde_arr_new(sizeof(fude_zoom_mech_spring), _heap);
    _p->meshes  = rde_arr_new(sizeof(fude_zoom_mech_mesh), _heap);
    _p->ropes   = rde_arr_new(sizeof(fude_zoom_mech_rope), _heap);
    _p->slides  = rde_arr_new(sizeof(fude_zoom_mech_slide), _heap);
    _p->piece_points = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    _p->piece_counts = rde_arr_new(sizeof(u32), _heap);
    _p->unit    = 1.0;
}

void fude_zoom_mech_plan_destroy(fude_zoom_mech_plan* _p) {
    rde_arr_free(&_p->bodies);
    rde_arr_free(&_p->hinges);
    rde_arr_free(&_p->springs);
    rde_arr_free(&_p->meshes);
    rde_arr_free(&_p->ropes);
    rde_arr_free(&_p->slides);
    rde_arr_free(&_p->piece_points);
    rde_arr_free(&_p->piece_counts);
}

RDE_INTERNAL void fzm_arr_copy(rde_arr* _to, const rde_arr* _from, usize _size) {
    rde_arr_clear(_to);
    const u32 _n = (u32)rde_arr_length(_from);
    if(_n > 0u) {
        memcpy(rde_arr_add_n(_to, _n), _from->memory, (usize)_n * _size);
    }
}

void fude_zoom_mech_plan_copy(fude_zoom_mech_plan* _to, const fude_zoom_mech_plan* _from) {
    fzm_arr_copy(&_to->bodies, &_from->bodies, sizeof(fude_zoom_mech_body));
    fzm_arr_copy(&_to->hinges, &_from->hinges, sizeof(fude_zoom_mech_hinge));
    fzm_arr_copy(&_to->springs, &_from->springs, sizeof(fude_zoom_mech_spring));
    fzm_arr_copy(&_to->meshes, &_from->meshes, sizeof(fude_zoom_mech_mesh));
    fzm_arr_copy(&_to->ropes, &_from->ropes, sizeof(fude_zoom_mech_rope));
    fzm_arr_copy(&_to->slides, &_from->slides, sizeof(fude_zoom_mech_slide));
    fzm_arr_copy(&_to->piece_points, &_from->piece_points, sizeof(fude_zoom_v2));
    fzm_arr_copy(&_to->piece_counts, &_from->piece_counts, sizeof(u32));
    _to->unit = _from->unit;
}

f64 fude_zoom_mech_pulley_radius(const fude_zoom_mech_body* _b) {
    return fmin(_b->hw, _b->hh) * 0.78;
}

f64 fude_zoom_mech_rack_pitch_line(f64 _hh) {
    return _hh - 2.0 * fmin(5.0, _hh * 0.5);   // (its teeth a module above it at most: their tips at its top)
}

// Is _q on body _b (its shape as drawn, _tol about it)?
RDE_INTERNAL b8 fzm_on_drawn(const fude_zoom_mech_plan* _p, const fude_zoom_mech_body* _b, fude_zoom_v2 _q, f64 _tol);

// (a drawn body's shape is its pieces, in the plan)
RDE_INTERNAL b8 fzm_on(const fude_zoom_mech_plan* _p, const fude_zoom_mech_body* _b, fude_zoom_v2 _q, f64 _tol) {
    if(_b->part->kind == FUDE_ZOOM_MECH_DRAWN) {
        return fzm_on_drawn(_p, _b, _q, _tol);
    }
    const f64 _dx = _q.x - _b->at.x, _dy = _q.y - _b->at.y;
    const f64 _lx = _dx * cos(_b->angle) + _dy * sin(_b->angle), _ly = -_dx * sin(_b->angle) + _dy * cos(_b->angle);
    switch(_b->part->kind) {
    case FUDE_ZOOM_MECH_WEIGHT:
    case FUDE_ZOOM_MECH_WHEEL:
    case FUDE_ZOOM_MECH_GEAR:
        return hypot(_lx, _ly) <= fmin(_b->hw, _b->hh) + _tol;
    case FUDE_ZOOM_MECH_LINK:
        return fabs(_ly) <= _b->hh + _tol && fabs(_lx) <= _b->hw + _tol;
    default:
        return fabs(_lx) <= _b->hw + _tol && fabs(_ly) <= _b->hh + _tol;
    }
}

void fude_zoom_mech_rope_keep(fude_zoom_mech_pull* _t, u32 _n, f64 _stretch) {
    f64 _w = 0.0, _out = 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        _t[_i].dv = _t[_i].dp = (fude_zoom_v2){ 0.0, 0.0 };
        _w   += _t[_i].w;
        _out += _t[_i].v.x * _t[_i].u.x + _t[_i].v.y * _t[_i].u.y;   // (how fast it gets longer)
    }
    if(!(_w > 0.0) || _stretch < -1e-9) {
        return;   // (held at both ends, or slack)
    }
    const f64 _impulse = _out > 0.0 ? _out / _w : 0.0;     // (per its ends' inverse masses: theirs shared out)
    const f64 _back    = _stretch > 0.0 ? _stretch / _w : 0.0;
    for(u32 _i = 0; _i < _n; _i++) {
        _t[_i].dv = (fude_zoom_v2){ -_t[_i].w * _impulse * _t[_i].u.x, -_t[_i].w * _impulse * _t[_i].u.y };
        _t[_i].dp = (fude_zoom_v2){ -_t[_i].w * _back * _t[_i].u.x, -_t[_i].w * _back * _t[_i].u.y };
    }
}

// Body _b's hole _h where it is (home units).
RDE_INTERNAL fude_zoom_v2 fzm_hole(const fude_zoom_mech_body* _b, u32 _h) {
    f64 _x = (f64)_b->part->holes[_h][0] * _b->hw, _y = (f64)_b->part->holes[_h][1] * _b->hh;
    if(_b->part->kind == FUDE_ZOOM_MECH_LINK) {
        _x = (_h == 0u ? -1.0 : 1.0) * fmax(_b->hw - _b->hh, 0.0);   // (a link's holes at its round ends' middles, however long)
    }
    return (fude_zoom_v2){ _b->at.x + _x * cos(_b->angle) - _y * sin(_b->angle), _b->at.y + _x * sin(_b->angle) + _y * cos(_b->angle) };
}

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
            const f64 _dx = _c[_i].x - _c[_j].x, _dy = _c[_i].y - _c[_j].y, _ll = _dx * _dx + _dy * _dy;
            const f64 _u = _ll > 0.0 ? fmax(0.0, fmin(1.0, ((_l.x - _c[_j].x) * _dx + (_l.y - _c[_j].y) * _dy) / _ll)) : 0.0;
            if(hypot(_c[_j].x + _dx * _u - _l.x, _c[_j].y + _dy * _u - _l.y) <= _tol) {
                return true;
            }
        }
        _first += _cnt[_k];
    }
    return false;
}

// A drawing made a body (props.h) into the plan: its outline in the home frame, simplified, cut into convex pieces
// about its centre (sim/body.h), its mass from its area and its material (or as given); an open line: fixed thin
// pieces along it (the ground: a ramp, a track).
#define FZM_PIECES (RDE_PHYSICS_2D_MAX_COMPOUND_SHAPES + 1u)   // a body's pieces, at most: its first shape and those added

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
        _x0 = fmin(_x0, _h.x); _x1 = fmax(_x1, _h.x); _y0 = fmin(_y0, _h.y); _y1 = fmax(_y1, _h.y);
    }
    rde_arr_free(&_pts);
    const f64 _size = fmax(_x1 - _x0, _y1 - _y0);
    const fude_sim_material* _mat = fude_sim_material_at(_bp->material);
    _mat = _mat != NULL ? _mat : fude_sim_material_at(0u);
    fude_zoom_mech_body _b;
    memset(&_b, 0, sizeof(_b));
    _b.object   = _target;
    _b.part     = fude_zoom_mech_find("drawn body");
    _b.fixed    = _bp->fixed || !_closed;
    _b.friction = _bp->friction >= 0.0 ? _bp->friction : _mat->friction;
    _b.bounce   = _bp->bounce >= 0.0 ? _bp->bounce : _mat->bounce;
    _b.piece    = (u32)rde_arr_length(&_p->piece_counts);
    rde_arr _pp = rde_arr_new(sizeof(fude_sim_v2), _heap), _pc = rde_arr_new(sizeof(u32), _heap);
    fude_sim_v2 _centre = { (_x0 + _x1) * 0.5, (_y0 + _y1) * 0.5 };
    f64 _area = 0.0;
    if(_closed && _size > 0.0) {
        // (its shape kept to half a percent of its size, coarser only as far as the physics needs: FZM_PIECES pieces at
        // most — a jagged drawing keeps its corners while they cut into that many; 120 corners at most, for the
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
        const f64 _half = fmax((f64)_o->radius * _o->scale * fude_zoom_sim_scale(_up), _size * 0.004);
        for(u32 _i = 0; _i + 1u < _m && _i < FZM_PIECES; _i++) {
            const f64 _dx = _out[_i + 1u].x - _out[_i].x, _dy = _out[_i + 1u].y - _out[_i].y, _l = hypot(_dx, _dy);
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
        memcpy(rde_arr_add_n(&_p->piece_counts, _np), _pc.memory, (usize)_np * sizeof(u32));
        _b.pieces = _np;
        _b.at     = (fude_zoom_v2){ _centre.x, _centre.y };
        _b.angle  = 0.0;
        _b.hw     = (_x1 - _x0) * 0.5;
        _b.hh     = (_y1 - _y0) * 0.5;
        // (kg: its density, its area in square metres — the home frame's units millimetres —, 10 mm thick)
        _b.mass   = _bp->mass > 0.0 ? _bp->mass : fmax(_mat->density * fabs(_area) * 1e-6 * FUDE_SIM_BODY_THICKNESS, 1e-4);
        _b.value  = _b.mass;
        rde_arr_add(&_p->bodies, (any)&_b);
        const f64 _unit = fmax(fmin(_b.hw, _b.hh) * 0.5, _size * 0.02);
        _p->unit = _p->unit > 0.0 ? fmin(_p->unit, _unit) : _unit;
    }
    rde_arr_free(&_pp);
    rde_arr_free(&_pc);
    rde_arr_free(&_in_arr);
    rde_arr_free(&_out_arr);
}

u32 fude_zoom_mech_plan_build(fude_zoom_mech_plan* _p, const fude_zoom_scene* _s, const u8* _scope) {
    rde_arr_clear(&_p->bodies);
    rde_arr_clear(&_p->hinges);
    rde_arr_clear(&_p->springs);
    rde_arr_clear(&_p->meshes);
    rde_arr_clear(&_p->ropes);
    rde_arr_clear(&_p->slides);
    rde_arr_clear(&_p->piece_points);
    rde_arr_clear(&_p->piece_counts);
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    _p->unit = 0.0;
    rde_arr _n = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std());
    c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if((_scope != NULL && !_scope[_i]) || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || fude_zoom_scene_hides(_s, _o) || !fude_zoom_scene_frame_shown(_s, _o->frame)) {
            continue;
        }
        const fude_zoom_mech_part* _part = fude_zoom_mech_of(_s, _i);
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
        memset(&_b, 0, sizeof(_b));
        _b.object = _i;
        _b.part   = _part;
        _b.at     = fude_zoom_sim_apply(_up, (fude_zoom_v2){ 0.0, 0.0 });
        _b.angle  = atan2(_up.b, _up.a);
        _b.hw     = _num[1] * _k;
        _b.hh     = _num[2] * _k;
        _b.fixed  = _part->kind == FUDE_ZOOM_MECH_PIVOT || _part->kind == FUDE_ZOOM_MECH_MOTOR || _part->kind == FUDE_ZOOM_MECH_WALL ||
                    _part->kind == FUDE_ZOOM_MECH_PULLEY || _part->kind == FUDE_ZOOM_MECH_RAIL;
        fude_zoom_symbol_text(_num, _count, _text, sizeof(_text));
        f64 _v = 0.0;
        const b8 _has = fude_zoom_circuit_value(_text, 0u, &_v);
        if(_part->kind == FUDE_ZOOM_MECH_MOTOR) {
            _b.value = (_has ? _v : 30.0) / 60.0;   // (rpm: turns a second)
            if(strstr(_text, "-") != NULL || strstr(_text, "cw") != NULL || strstr(_text, "CW") != NULL) {
                _b.value = -_b.value;   // (clockwise)
            }
        } else if(_part->kind == FUDE_ZOOM_MECH_WEIGHT || _part->kind == FUDE_ZOOM_MECH_CRATE) {
            // (kilograms: "2 kg" read as 2000 grams, "500 g" as 500, "2" as 2 kg)
            _b.value = _has && _v > 0.0 ? (strchr(_text, 'g') != NULL ? _v / 1000.0 : _v) : (_part->kind == FUDE_ZOOM_MECH_CRATE ? 2.0 : 1.0);
        } else if(_part->kind == FUDE_ZOOM_MECH_SPRING) {
            _b.value = _has && _v > 0.0 ? _v : 1.0;
        }
        rde_arr_add(&_p->bodies, (any)&_b);
        const f64 _size = _part->kind == FUDE_ZOOM_MECH_LINK || _part->kind == FUDE_ZOOM_MECH_ROPE || _part->kind == FUDE_ZOOM_MECH_RAIL ||
                          _part->kind == FUDE_ZOOM_MECH_RACK ? fmax(_b.hh, _b.hw * 0.08) : fmin(_b.hw, _b.hh) * 0.5;
        _p->unit = _p->unit > 0.0 ? fmin(_p->unit, _size) : _size;
    }
    rde_arr_free(&_n);
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
    // Hinges: holes over holes (a fixed pivot's or a motor's: the ground there). Springs: their ends to what they are on.
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind == FUDE_ZOOM_MECH_ROPE || _b[_i].part->kind == FUDE_ZOOM_MECH_PIN) {
            continue;   // (below)
        }
        if(_b[_i].part->kind == FUDE_ZOOM_MECH_SPRING) {
            fude_zoom_mech_spring _sp = { FUDE_ZOOM_NONE, FUDE_ZOOM_NONE, fzm_hole(&_b[_i], 0u), fzm_hole(&_b[_i], 1u), 0.0, _b[_i].value };
            _sp.length = hypot(_sp.pb.x - _sp.pa.x, _sp.pb.y - _sp.pa.y);
            for(u32 _e = 0; _e < 2u; _e++) {
                const fude_zoom_v2 _at = _e == 0u ? _sp.pa : _sp.pb;
                for(u32 _j = 0; _j < _nb; _j++) {
                    if(_j == _i || _b[_j].part->kind == FUDE_ZOOM_MECH_SPRING || _b[_j].part->kind == FUDE_ZOOM_MECH_ROPE) {
                        continue;
                    }
                    for(u32 _h = 0; _h < _b[_j].part->hole_count; _h++) {
                        const fude_zoom_v2 _q = fzm_hole(&_b[_j], _h);
                        if(hypot(_q.x - _at.x, _q.y - _at.y) <= _tol) {
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
                if(_b[_j].part->kind == FUDE_ZOOM_MECH_SPRING || _b[_j].part->kind == FUDE_ZOOM_MECH_ROPE || _b[_j].part->kind == FUDE_ZOOM_MECH_PIN ||
                   (_b[_i].fixed && _b[_j].fixed)) {
                    continue;
                }
                for(u32 _g = 0; _g < _b[_j].part->hole_count; _g++) {
                    const fude_zoom_v2 _q = fzm_hole(&_b[_j], _g);
                    if(hypot(_q.x - _at.x, _q.y - _at.y) > _tol) {
                        continue;
                    }
                    fude_zoom_mech_hinge _hg = { _i, _j, { (_at.x + _q.x) * 0.5, (_at.y + _q.y) * 0.5 }, false, 0.0 };
                    const u32 _fixed = _b[_i].fixed ? _i : (_b[_j].fixed ? _j : FUDE_ZOOM_NONE);
                    if(_fixed != FUDE_ZOOM_NONE) {
                        _hg.a = _fixed == _i ? _j : _i;
                        _hg.b = FUDE_ZOOM_NONE;
                        _hg.at = fzm_hole(&_b[_fixed], _fixed == _i ? _h : _g);   // (on the ground where the pivot is)
                        if(_b[_fixed].part->kind == FUDE_ZOOM_MECH_MOTOR) {
                            _hg.motor = true;
                            _hg.speed = 2.0 * FZM_PI * _b[_fixed].value;
                        }
                    }
                    rde_arr_add(&_p->hinges, (any)&_hg);
                }
            }
        }
    }
    // A gear nothing holds: on an axle of its own where it is (gears turn, they do not fall).
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind != FUDE_ZOOM_MECH_GEAR) {
            continue;
        }
        b8 _held = false;
        const fude_zoom_mech_hinge* _h = (const fude_zoom_mech_hinge*)_p->hinges.memory;
        for(u32 _k = 0; _k < (u32)rde_arr_length(&_p->hinges); _k++) {
            _held = _held || _h[_k].a == _i || _h[_k].b == _i;
        }
        if(!_held) {
            const fude_zoom_mech_hinge _axle = { _i, FUDE_ZOOM_NONE, _b[_i].at, false, 0.0 };
            rde_arr_add(&_p->hinges, (any)&_axle);
        }
    }
    // Pins: the bodies under each hinged together there; one alone, or a fixed thing under it too: to the ground.
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind != FUDE_ZOOM_MECH_PIN) {
            continue;
        }
        const fude_zoom_v2 _q = _b[_i].at;
        u32 _moving[16];
        u32 _m = 0;
        b8 _ground = false;
        for(u32 _j = 0; _j < _nb; _j++) {
            const u8 _k = _b[_j].part->kind;
            if(_j == _i || _k == FUDE_ZOOM_MECH_SPRING || _k == FUDE_ZOOM_MECH_ROPE || _k == FUDE_ZOOM_MECH_PIN || !fzm_on(_p, &_b[_j], _q, _tol * 0.5)) {
                continue;
            }
            if(_b[_j].fixed) {
                _ground = true;
            } else if(_m < 16u) {
                _moving[_m++] = _j;
            }
        }
        for(u32 _k = 1; _k < _m; _k++) {
            const fude_zoom_mech_hinge _hg = { _moving[0], _moving[_k], _q, false, 0.0 };
            rde_arr_add(&_p->hinges, (any)&_hg);
        }
        if(_m > 0u && (_ground || _m == 1u)) {
            const fude_zoom_mech_hinge _hg = { _moving[0], FUDE_ZOOM_NONE, _q, false, 0.0 };
            rde_arr_add(&_p->hinges, (any)&_hg);
        }
    }
    // Ropes: each end on a body (anywhere on it), else off a pulley's rim, else held still where it is; two down one
    // pulley's sides one rope over it.
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind != FUDE_ZOOM_MECH_ROPE) {
            continue;
        }
        fude_zoom_mech_rope _r = { _i, FUDE_ZOOM_NONE, FUDE_ZOOM_NONE, fzm_hole(&_b[_i], 0u), fzm_hole(&_b[_i], 1u), 0.0, FUDE_ZOOM_NONE, FUDE_ZOOM_NONE };
        _r.length = hypot(_r.pb.x - _r.pa.x, _r.pb.y - _r.pa.y);
        u32 _over[2] = { FUDE_ZOOM_NONE, FUDE_ZOOM_NONE };
        for(u32 _e = 0; _e < 2u; _e++) {
            const fude_zoom_v2 _q = _e == 0u ? _r.pa : _r.pb;
            u32 _held = FUDE_ZOOM_NONE;
            for(u32 _j = 0; _j < _nb && _held == FUDE_ZOOM_NONE; _j++) {
                const u8 _k = _b[_j].part->kind;
                if(_j != _i && !_b[_j].fixed && _k != FUDE_ZOOM_MECH_SPRING && _k != FUDE_ZOOM_MECH_ROPE && fzm_on(_p, &_b[_j], _q, _tol * 0.5)) {
                    _held = _j;
                }
            }
            for(u32 _j = 0; _j < _nb && _held == FUDE_ZOOM_NONE && _over[_e] == FUDE_ZOOM_NONE; _j++) {
                if(_b[_j].part->kind == FUDE_ZOOM_MECH_PULLEY &&
                   hypot(_q.x - _b[_j].at.x, _q.y - _b[_j].at.y) <= fmin(_b[_j].hw, _b[_j].hh) + _tol) {
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
    // Sliders on rails: along the rail's line, between its ends. Racks: along their own length (as far as half of it
    // each way).
    for(u32 _i = 0; _i < _nb; _i++) {
        const u8 _k = _b[_i].part->kind;
        if(_k == FUDE_ZOOM_MECH_RACK) {
            const fude_zoom_mech_slide _sl = { _i, _b[_i].at, { cos(_b[_i].angle), sin(_b[_i].angle) }, -_b[_i].hw * 0.5, _b[_i].hw * 0.5 };
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
            const fude_zoom_v2 _u = { cos(_b[_j].angle), sin(_b[_j].angle) };
            const f64 _dx = _b[_i].at.x - _b[_j].at.x, _dy = _b[_i].at.y - _b[_j].at.y;
            const f64 _along = _dx * _u.x + _dy * _u.y, _across = -_dx * _u.y + _dy * _u.x;
            if(fabs(_across) <= _b[_j].hh + _b[_i].hh * 0.5 && fabs(_along) <= _b[_j].hw) {
                const f64 _room = fmax(_b[_j].hw - _b[_i].hw, 0.0);
                const fude_zoom_mech_slide _sl = { _i, { _b[_j].at.x + _u.x * _along, _b[_j].at.y + _u.y * _along }, _u, -_room - _along, _room - _along };
                rde_arr_add(&_p->slides, (any)&_sl);
                break;
            }
        }
    }
    // A gear on a rack (its pitch circle on the rack's pitch line, over it): meshed — its turn and the rack's travel
    // kept together (a's turn + ratio · b's travel).
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind != FUDE_ZOOM_MECH_GEAR) {
            continue;
        }
        const f64 _r = fude_zoom_mech_pitch(_b[_i].part, fmin(_b[_i].hw, _b[_i].hh));
        for(u32 _j = 0; _j < _nb; _j++) {
            if(_b[_j].part->kind != FUDE_ZOOM_MECH_RACK) {
                continue;
            }
            const fude_zoom_v2 _u = { cos(_b[_j].angle), sin(_b[_j].angle) }, _n = { -_u.y, _u.x };
            const f64 _dx = _b[_i].at.x - _b[_j].at.x, _dy = _b[_i].at.y - _b[_j].at.y;
            const f64 _along = _dx * _u.x + _dy * _u.y, _up = _dx * _n.x + _dy * _n.y;
            const f64 _line = fude_zoom_mech_rack_pitch_line(_b[_j].hh), _mod = 2.0 * _r / (f64)_b[_i].part->teeth;
            const b8 _above = fabs(_up - (_line + _r)) <= 0.6 * _mod, _below = fabs(_up + (_line + _r)) <= 0.6 * _mod;
            if((_above || _below) && fabs(_along) <= _b[_j].hw) {
                // (the rack goes as the gear's rim where they touch: over it, turning anticlockwise, the rack goes on — its
                // travel r a radian: a's turn − b's travel / r kept; under it, the other way)
                const f64 _side = _above ? 1.0 : -1.0;
                const fude_zoom_mech_mesh _m = { _i, _j, -_side / _r, true };
                rde_arr_add(&_p->meshes, (any)&_m);
            }
        }
    }
    // Gears whose pitch circles touch: meshed.
    for(u32 _i = 0; _i < _nb; _i++) {
        if(_b[_i].part->kind != FUDE_ZOOM_MECH_GEAR) {
            continue;
        }
        const f64 _ri = fude_zoom_mech_pitch(_b[_i].part, fmin(_b[_i].hw, _b[_i].hh));
        for(u32 _j = _i + 1u; _j < _nb; _j++) {
            if(_b[_j].part->kind != FUDE_ZOOM_MECH_GEAR) {
                continue;
            }
            const f64 _rj = fude_zoom_mech_pitch(_b[_j].part, fmin(_b[_j].hw, _b[_j].hh));
            const f64 _d = hypot(_b[_j].at.x - _b[_i].at.x, _b[_j].at.y - _b[_i].at.y);
            const f64 _mod = 2.0 * _ri / (f64)_b[_i].part->teeth;
            if(fabs(_d - (_ri + _rj)) <= 0.6 * _mod) {
                const fude_zoom_mech_mesh _m = { _i, _j, (f64)_b[_i].part->teeth / (f64)_b[_j].part->teeth, false };
                rde_arr_add(&_p->meshes, (any)&_m);
            }
        }
    }
    return _nb;
}

void fude_zoom_mech_gears(const fude_zoom_mech_plan* _p, f64* _omega, const b8* _driven) {
    const u32 _nb = (u32)rde_arr_length(&_p->bodies), _nm = (u32)rde_arr_length(&_p->meshes);
    if(_nm == 0u || _nb == 0u) {
        return;
    }
    const fude_zoom_mech_mesh* _m = (const fude_zoom_mech_mesh*)_p->meshes.memory;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _set_arr = rde_arr_new(sizeof(u8), _heap), _queue = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_set_arr, _nb);
    u8* _set = (u8*)_set_arr.memory;
    u32 _head = 0;
    // Driven ones first, then each train's first gear.
    for(u32 _pass = 0; _pass < 2u; _pass++) {
        for(u32 _k = 0; _k < _nm; _k++) {
            const u32 _g = _m[_k].a;
            if(!_set[_g] && (_pass == 1u || (_driven != NULL && _driven[_g]))) {
                _set[_g] = 1u;
                rde_arr_add(&_queue, (any)&_g);
                while(_head < (u32)rde_arr_length(&_queue)) {
                    const u32 _v = ((const u32*)_queue.memory)[_head++];
                    for(u32 _e = 0; _e < _nm; _e++) {
                        const u32 _o = _m[_e].rack ? FUDE_ZOOM_NONE : (_m[_e].a == _v ? _m[_e].b : (_m[_e].b == _v ? _m[_e].a : FUDE_ZOOM_NONE));
                        if(_o == FUDE_ZOOM_NONE || _set[_o]) {
                            continue;
                        }
                        _omega[_o] = -_omega[_v] * (_m[_e].a == _v ? _m[_e].ratio : 1.0 / _m[_e].ratio);
                        _set[_o] = 1u;
                        rde_arr_add(&_queue, (any)&_o);
                    }
                }
            }
        }
    }
    rde_arr_free(&_set_arr);
    rde_arr_free(&_queue);
}
