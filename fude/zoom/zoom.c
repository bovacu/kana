// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/zoom.h"

#include <math.h>

// ===========================================================================
// See zoom.h.
// ===========================================================================

fude_zoom_box fude_zoom_box_empty(void) {
    return (fude_zoom_box){ 1.0, 1.0, -1.0, -1.0 };
}

b8 fude_zoom_box_is_empty(fude_zoom_box _b) {
    return _b.min_x > _b.max_x || _b.min_y > _b.max_y;
}

fude_zoom_box fude_zoom_box_union(fude_zoom_box _a, fude_zoom_box _b) {
    if(fude_zoom_box_is_empty(_a)) {
        return _b;
    }
    if(fude_zoom_box_is_empty(_b)) {
        return _a;
    }
    return (fude_zoom_box){
        _a.min_x < _b.min_x ? _a.min_x : _b.min_x, _a.min_y < _b.min_y ? _a.min_y : _b.min_y,
        _a.max_x > _b.max_x ? _a.max_x : _b.max_x, _a.max_y > _b.max_y ? _a.max_y : _b.max_y,
    };
}

fude_zoom_box fude_zoom_box_grow(fude_zoom_box _b, f64 _by) {
    if(fude_zoom_box_is_empty(_b)) {
        return _b;
    }
    return (fude_zoom_box){ _b.min_x - _by, _b.min_y - _by, _b.max_x + _by, _b.max_y + _by };
}

b8 fude_zoom_box_overlaps(fude_zoom_box _a, fude_zoom_box _b) {
    if(fude_zoom_box_is_empty(_a) || fude_zoom_box_is_empty(_b)) {
        return false;
    }
    return _a.min_x <= _b.max_x && _b.min_x <= _a.max_x && _a.min_y <= _b.max_y && _b.min_y <= _a.max_y;
}

b8 fude_zoom_box_contains(fude_zoom_box _b, fude_zoom_v2 _p) {
    return _p.x >= _b.min_x && _p.x <= _b.max_x && _p.y >= _b.min_y && _p.y <= _b.max_y;
}

f64 fude_zoom_box_area(fude_zoom_box _b) {
    return fude_zoom_box_is_empty(_b) ? 0.0 : (_b.max_x - _b.min_x) * (_b.max_y - _b.min_y);
}

fude_zoom_sim fude_zoom_sim_identity(void) {
    return (fude_zoom_sim){ 1.0, 0.0, 0.0, 0.0 };
}

fude_zoom_sim fude_zoom_sim_from_xform(fude_zoom_xform _x) {
    return (fude_zoom_sim){ _x.scale * cos(_x.rotation), _x.scale * sin(_x.rotation), _x.ox, _x.oy };
}

fude_zoom_sim fude_zoom_sim_compose(fude_zoom_sim _o, fude_zoom_sim _i) {
    // o(i(p)): the rotation-scales multiply as complex numbers, i's translation
    // goes through o.
    return (fude_zoom_sim){
        _o.a * _i.a - _o.b * _i.b,
        _o.a * _i.b + _o.b * _i.a,
        _o.a * _i.tx - _o.b * _i.ty + _o.tx,
        _o.b * _i.tx + _o.a * _i.ty + _o.ty,
    };
}

fude_zoom_sim fude_zoom_sim_inverse(fude_zoom_sim _s) {
    const f64 _d = _s.a * _s.a + _s.b * _s.b;
    const f64 _a = _s.a / _d;
    const f64 _b = -_s.b / _d;
    return (fude_zoom_sim){ _a, _b, -(_a * _s.tx - _b * _s.ty), -(_b * _s.tx + _a * _s.ty) };
}

fude_zoom_v2 fude_zoom_sim_apply(fude_zoom_sim _s, fude_zoom_v2 _p) {
    return (fude_zoom_v2){ _s.a * _p.x - _s.b * _p.y + _s.tx, _s.b * _p.x + _s.a * _p.y + _s.ty };
}

f64 fude_zoom_sim_scale(fude_zoom_sim _s) {
    return sqrt(_s.a * _s.a + _s.b * _s.b);
}

fude_zoom_box fude_zoom_sim_box(fude_zoom_sim _s, fude_zoom_box _b) {
    if(fude_zoom_box_is_empty(_b)) {
        return _b;
    }
    const fude_zoom_v2 _c[4] = {
        fude_zoom_sim_apply(_s, (fude_zoom_v2){ _b.min_x, _b.min_y }), fude_zoom_sim_apply(_s, (fude_zoom_v2){ _b.max_x, _b.min_y }),
        fude_zoom_sim_apply(_s, (fude_zoom_v2){ _b.min_x, _b.max_y }), fude_zoom_sim_apply(_s, (fude_zoom_v2){ _b.max_x, _b.max_y }),
    };
    fude_zoom_box _out = { _c[0].x, _c[0].y, _c[0].x, _c[0].y };
    for(u32 _i = 1; _i < 4; _i++) {
        _out = fude_zoom_box_union(_out, (fude_zoom_box){ _c[_i].x, _c[_i].y, _c[_i].x, _c[_i].y });
    }
    return _out;
}
