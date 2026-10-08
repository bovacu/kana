// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/placer.h"
#include "zoom/circuit.h"
#include "zoom/symbol.h"
#include "zoom/shape.h"
#include "zoom/display.h"
#include "drawing/base/theme.h"
#include <math.h>
#include <string.h>

#define FZPL_DEG 0.017453292519943295

fude_zoom_placer fude_zoom_placer_make(fude_zoom_scene* _s, u32 _frame, fude_zoom_sim _to_frame, rde_color _ink, rde_arr* _born) {
    return (fude_zoom_placer){ _s, _frame, _to_frame, fude_zoom_sim_scale(_to_frame), _ink, _born, { 0.0, 0.0 } };
}

fude_zoom_v2 fude_zoom_placer_at(const fude_zoom_placer* _p, f64 _x, f64 _y) {
    return fude_zoom_sim_apply(_p->to_frame, (fude_zoom_v2){ _p->o.x + _x, _p->o.y + _y });
}

u32 fude_zoom_placer_part(fude_zoom_placer* _p, const c8* _id, f64 _x, f64 _y, f64 _turn, f64 _w, f64 _h, const c8* _text) {
    const u32 _kind = fude_zoom_symbol_find(_id);
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
    if(_info == NULL) {
        return FUDE_ZOOM_NONE;
    }
    // (its size as it comes: a sized display's as big as its text says)
    f64 _cw = (f64)_info->w, _ch = (f64)_info->h;
    fude_zoom_display_room(fude_zoom_part_of_text(_kind, _text), &_cw, &_ch);
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS + 200];
    const u32 _count = fude_zoom_symbol_numbers(_n, _kind, (_w > 0.0 ? _w : _cw) * 0.5 * _p->u, (_h > 0.0 ? _h : _ch) * 0.5 * _p->u, 12.0 * _p->u, _text);
    const f64 _turned = _turn * FZPL_DEG + atan2(_p->to_frame.b, _p->to_frame.a);
    const u32 _made = fude_zoom_scene_add_shape_fill(_p->s, _p->frame, (fude_zoom_place){ fude_zoom_placer_at(_p, _x, _y), _turned, 1.0 }, FUDE_ZOOM_SHAPE_SYMBOL, _n,
                                                     _count, _p->ink, (f32)_p->u, FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN, FUDE_THEME_PAGE_FILL, 0);
    rde_arr_add(_p->born, (any)&_made);
    return _made;
}

fude_zoom_v2 fude_zoom_placer_pin(const fude_zoom_placer* _p, u32 _o, u32 _pin) {
    fude_zoom_v2 _at = { 0.0, 0.0 };
    fude_zoom_part_pin_at(_p->s, _o, _pin, &_at);
    _at = fude_zoom_sim_apply(fude_zoom_sim_inverse(_p->to_frame), _at);
    return (fude_zoom_v2){ _at.x - _p->o.x, _at.y - _p->o.y };
}

void fude_zoom_placer_wire(fude_zoom_placer* _p, u32 _a, u32 _pa, u32 _b, u32 _pb, b8 _straight) {
    fude_zoom_v2 _p0, _p1, _r[6];
    if(_a == FUDE_ZOOM_NONE || _b == FUDE_ZOOM_NONE || !fude_zoom_part_pin_at(_p->s, _a, _pa, &_p0) || !fude_zoom_part_pin_at(_p->s, _b, _pb, &_p1)) {
        return;
    }
    u32 _m = 2u;
    _r[0] = _p0;
    _r[1] = _p1;
    if(!_straight) {
        _m = fude_zoom_wire_route(_p0, fude_zoom_part_side_at(_p->s, _a, _pa), _p1, fude_zoom_part_side_at(_p->s, _b, _pb), fude_zoom_part_unit(_p->s, _a), _r);
    }
    const u32 _w = fude_zoom_wire_add(_p->s, _p->frame, _r, _m, _a, (i32)_pa, _b, (i32)_pb, _p->ink, (f32)((_straight ? 1.5 : 0.9) * _p->u));
    rde_arr_add(_p->born, (any)&_w);
}

void fude_zoom_placer_wire_via(fude_zoom_placer* _p, u32 _a, u32 _pa, u32 _b, u32 _pb, const fude_zoom_v2* _via, u32 _n) {
    fude_zoom_v2 _r[FUDE_ZOOM_WIRE_POINTS];
    if(_a == FUDE_ZOOM_NONE || _b == FUDE_ZOOM_NONE || _n + 2u > FUDE_ZOOM_WIRE_POINTS || !fude_zoom_part_pin_at(_p->s, _a, _pa, &_r[0]) ||
       !fude_zoom_part_pin_at(_p->s, _b, _pb, &_r[_n + 1u])) {
        return;
    }
    for(u32 _i = 0; _i < _n; _i++) {
        _r[_i + 1u] = fude_zoom_placer_at(_p, _via[_i].x, _via[_i].y);
    }
    const u32 _w = fude_zoom_wire_add(_p->s, _p->frame, _r, _n + 2u, _a, (i32)_pa, _b, (i32)_pb, _p->ink, (f32)(0.9 * _p->u));
    rde_arr_add(_p->born, (any)&_w);
}

void fude_zoom_placer_text(fude_zoom_placer* _p, f64 _x, f64 _y, f64 _size, f64 _w, const c8* _text) {
    const f64 _turned = atan2(_p->to_frame.b, _p->to_frame.a);
    const u32 _t = fude_zoom_scene_add_text(_p->s, _p->frame, (fude_zoom_place){ fude_zoom_placer_at(_p, _x, _y), _turned, 1.0 }, _size * _p->u, _w * _p->u,
                                            _size * 1.25 * _p->u, FUDE_ZOOM_TEXT_PLAIN, _p->ink, (rde_color){ 0, 0, 0, 0 }, _text, (u32)strlen(_text), 0);
    rde_arr_add(_p->born, (any)&_t);
}

u32 fude_zoom_placer_path(fude_zoom_placer* _p, const fude_zoom_v2* _pts, u32 _n, b8 _closed, f64 _width) {
    const u32 _m = _closed ? _n + 1u : _n;
    if(_n == 0u || _m < 2u || _m > 64u) {
        return FUDE_ZOOM_NONE;
    }
    const i8  _q    = fude_zoom_quantum_for(4.0 / _p->u);
    const f64 _grid = ldexp(1.0, _q);
    const fude_zoom_v2 _t = fude_zoom_placer_at(_p, _pts[0].x, _pts[0].y);
    fude_zoom_qpoint _qp[64];
    for(u32 _i = 0; _i < _m; _i++) {
        const fude_zoom_v2 _at = fude_zoom_placer_at(_p, _pts[_i % _n].x, _pts[_i % _n].y);
        _qp[_i] = (fude_zoom_qpoint){ (i32)llround((_at.x - _t.x) / _grid), (i32)llround((_at.y - _t.y) / _grid), 1023u, 0u };
    }
    const u32 _o = fude_zoom_scene_add_stroke(_p->s, _p->frame, _t, _q, _qp, _m, 0u, _p->ink, (f32)(_width * 0.5 * _p->u), 0u, 0, 0);
    rde_arr_add(_p->born, (any)&_o);
    return _o;
}
