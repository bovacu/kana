// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/zoom.h"

// ===========================================================================
// See zoom.h.
//
// The box helpers, then the similarity transforms, then line clipping. All of it is pure f64 maths on values passed
// by copy: no state, no allocation.
// ===========================================================================

// Returns the empty box (min 1, max -1 on both axes), the start of a union.
fude_zoom_box fude_zoom_box_empty(void) {
    return (fude_zoom_box){ 1.0, 1.0, -1.0, -1.0 };
}

// True when the box is empty on either axis (min > max).
b8 fude_zoom_box_is_empty(fude_zoom_box _b) {
    return _b.min_x > _b.max_x || _b.min_y > _b.max_y;
}

// Returns the box round both; an empty box adds nothing, so two empty boxes give an empty one.
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

// Returns _b widened by _by on every side; an empty box stays empty, so it never turns into a box round nothing.
fude_zoom_box fude_zoom_box_grow(fude_zoom_box _b, f64 _by) {
    if(fude_zoom_box_is_empty(_b)) {
        return _b;
    }
    return (fude_zoom_box){ _b.min_x - _by, _b.min_y - _by, _b.max_x + _by, _b.max_y + _by };
}

// True when the boxes share any point, touching edges included; false when either is empty.
b8 fude_zoom_box_overlaps(fude_zoom_box _a, fude_zoom_box _b) {
    if(fude_zoom_box_is_empty(_a) || fude_zoom_box_is_empty(_b)) {
        return false;
    }
    return _a.min_x <= _b.max_x && _b.min_x <= _a.max_x && _a.min_y <= _b.max_y && _b.min_y <= _a.max_y;
}

// True when _p is inside _b or on its edge; always false for an empty box.
b8 fude_zoom_box_contains(fude_zoom_box _b, fude_zoom_v2 _p) {
    return _p.x >= _b.min_x && _p.x <= _b.max_x && _p.y >= _b.min_y && _p.y <= _b.max_y;
}

// Returns the box's area, 0 for an empty one.
f64 fude_zoom_box_area(fude_zoom_box _b) {
    return fude_zoom_box_is_empty(_b) ? 0.0 : (_b.max_x - _b.min_x) * (_b.max_y - _b.min_y);
}

// Returns the transform that leaves every point where it is.
fude_zoom_sim fude_zoom_sim_identity(void) {
    return (fude_zoom_sim){ 1.0, 0.0, 0.0, 0.0 };
}

// Returns the transform from a frame to its parent: a = scale·cos(rotation), b = scale·sin(rotation), the origin as
// the translation.
fude_zoom_sim fude_zoom_sim_from_xform(fude_zoom_xform _x) {
    return (fude_zoom_sim){ _x.scale * rde_math_cos_f64(_x.rotation), _x.scale * rde_math_sin_f64(_x.rotation), _x.ox, _x.oy };
}

// Returns _o after _i: _i is applied first.
fude_zoom_sim fude_zoom_sim_compose(fude_zoom_sim _o, fude_zoom_sim _i) {
    // o(i(p)): the rotation-scales multiply as complex numbers, i's translation goes through o.
    return (fude_zoom_sim){
        _o.a * _i.a - _o.b * _i.b,
        _o.a * _i.b + _o.b * _i.a,
        _o.a * _i.tx - _o.b * _i.ty + _o.tx,
        _o.b * _i.tx + _o.a * _i.ty + _o.ty,
    };
}

// Returns the inverse transform: the conjugate rotation-scale over |s|², and the translation taken back through it.
// Divides by |s|², so a zero scale gives infinities; the header asks for a non-zero scale.
fude_zoom_sim fude_zoom_sim_inverse(fude_zoom_sim _s) {
    const f64 _d = _s.a * _s.a + _s.b * _s.b;
    const f64 _a = _s.a / _d;
    const f64 _b = -_s.b / _d;
    return (fude_zoom_sim){ _a, _b, -(_a * _s.tx - _b * _s.ty), -(_b * _s.tx + _a * _s.ty) };
}

// Returns _p mapped by _s.
fude_zoom_v2 fude_zoom_sim_apply(fude_zoom_sim _s, fude_zoom_v2 _p) {
    return (fude_zoom_v2){ _s.a * _p.x - _s.b * _p.y + _s.tx, _s.b * _p.x + _s.a * _p.y + _s.ty };
}

// Returns the scale factor |s| = sqrt(a² + b²).
f64 fude_zoom_sim_scale(fude_zoom_sim _s) {
    return rde_math_sqrt_f64(_s.a * _s.a + _s.b * _s.b);
}

// Returns the axis-aligned box round _b's four corners mapped by _s (an empty box stays empty). With a rotation the
// result is bigger than the box itself.
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

// Cuts the segment *_a to *_b to the part inside _box (Liang–Barsky): each of the four edges narrows the segment's
// parameter range [_t0, _t1]. Returns false when none of it is inside, with the points left as they were; they are
// written only once the whole range is known, and only an end that was cut is written: one inside the box stays
// exactly as it was (worked out again, _a + (_b - _a) could round), so a caller can tell a cut end by comparing.
b8 fude_zoom_clip_line(fude_zoom_v2* _a, fude_zoom_v2* _b, fude_zoom_box _box) {
    const f64 _dx = _b->x - _a->x, _dy = _b->y - _a->y;
    const f64 _p[4] = { -_dx, _dx, -_dy, _dy };
    const f64 _q[4] = { _a->x - _box.min_x, _box.max_x - _a->x, _a->y - _box.min_y, _box.max_y - _a->y };
    f64 _t0 = 0.0, _t1 = 1.0;
    for(u32 _i = 0; _i < 4u; _i++) {
        if(_p[_i] == 0.0) {
            if(_q[_i] < 0.0) {
                return false;
            }
            continue;
        }
        const f64 _t = _q[_i] / _p[_i];
        if(_p[_i] < 0.0) {
            _t0 = rde_math_max_f64(_t0, _t);
        } else {
            _t1 = rde_math_min_f64(_t1, _t);
        }
        if(_t0 > _t1) {
            return false;
        }
    }
    const fude_zoom_v2 _from = *_a;
    if(_t0 > 0.0) {
        *_a = (fude_zoom_v2){ _from.x + _dx * _t0, _from.y + _dy * _t0 };
    }
    if(_t1 < 1.0) {
        *_b = (fude_zoom_v2){ _from.x + _dx * _t1, _from.y + _dy * _t1 };
    }
    return true;
}

// See the header.
i32 fude_zoom_by_u32(const any _a, const any _b) {
    const u32 _x = *(const u32*)_a, _y = *(const u32*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}
