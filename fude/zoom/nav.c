// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/nav.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// --- frames ----------------------------------------------------------------------------------

RDE_INTERNAL u32 fude_zoom_nav_parent(const fude_zoom_scene* _s, u32 _f) {
    return fude_zoom_scene_frame(_s, _f)->parent;
}

// The nearest frame both are in.
RDE_INTERNAL u32 fude_zoom_nav_common(const fude_zoom_scene* _s, u32 _a, u32 _b) {
    u32 _da = fude_zoom_scene_depth(_s, _a), _db = fude_zoom_scene_depth(_s, _b);
    while(_da > _db) { _a = fude_zoom_nav_parent(_s, _a); _da--; }
    while(_db > _da) { _b = fude_zoom_nav_parent(_s, _b); _db--; }
    while(_a != _b && _a != FUDE_ZOOM_NONE && _b != FUDE_ZOOM_NONE) {
        _a = fude_zoom_nav_parent(_s, _a);
        _b = fude_zoom_nav_parent(_s, _b);
    }
    return _a;
}

// log2 of a unit of frame _f in the units of its ancestor _up.
RDE_INTERNAL f64 fude_zoom_nav_log_unit(const fude_zoom_scene* _s, u32 _f, u32 _up) {
    f64 _g = 0.0;
    while(_f != _up && _f != FUDE_ZOOM_NONE) {
        _g += log2(fabs(fude_zoom_scene_frame(_s, _f)->xf.scale));
        _f  = fude_zoom_nav_parent(_s, _f);
    }
    return _g;
}

// --- flying ----------------------------------------------------------------------------------

RDE_INTERNAL f64 fude_zoom_nav_ease(f64 _t) {
    _t = _t < 0.0 ? 0.0 : (_t > 1.0 ? 1.0 : _t);
    return _t * _t * (3.0 - 2.0 * _t);
}

// The zoom (log2, the common frame's units) at eased time _e: from one end's to
// the other's, and out by the dip in the middle.
RDE_INTERNAL f64 fude_zoom_fly_zoom(const fude_zoom_flight* _f, f64 _e) {
    return _f->l[0] + (_f->l[1] - _f->l[0]) * _e - _f->l_dip * 4.0 * _e * (1.0 - _e);
}

RDE_INTERNAL void fude_zoom_fly_land(fude_zoom_flight* _f, fude_zoom_scene* _s, fude_zoom_v2 _half) {
    fude_zoom_camera_look_at(_s, _f->ends[1].frame, _f->ends[1].at, _f->ends[1].z);
    fude_zoom_camera_settle(_s, _half);
    _f->active = false;
}

b8 fude_zoom_fly_begin(fude_zoom_flight* _f, fude_zoom_scene* _s, fude_zoom_camera _to, fude_zoom_v2 _half, f64 _now) {
    memset(_f, 0, sizeof(*_f));
    _f->ends[0] = _s->camera;
    _f->ends[1] = _to;
    const u32 _common = fude_zoom_nav_common(_s, _f->ends[0].frame, _to.frame);
    for(u32 _k = 0; _k < 2u; _k++) {
        _f->g[_k] = fude_zoom_nav_log_unit(_s, _f->ends[_k].frame, _common);
        _f->l[_k] = log2(_f->ends[_k].z) - _f->g[_k];
    }
    _f->cross[0] = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _to.frame, _f->ends[0].frame), _to.at);
    _f->cross[1] = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _f->ends[0].frame, _to.frame), _f->ends[0].at);
    // How far apart, in the common frame (all that matters here is the scale).
    const fude_zoom_v2 _pa = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _f->ends[0].frame, _common), _f->ends[0].at);
    const fude_zoom_v2 _pb = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _to.frame, _common), _to.at);
    const f64 _d = hypot(_pb.x - _pa.x, _pb.y - _pa.y);
    const b8 _sane = _common != FUDE_ZOOM_NONE && isfinite(_f->l[0]) && isfinite(_f->l[1]) && isfinite(_d) &&
                     isfinite(_f->cross[0].x) && isfinite(_f->cross[0].y) && isfinite(_f->cross[1].x) && isfinite(_f->cross[1].y);
    // Already there (the same zoom, under a point apart on screen), or past flying: put there.
    if(!_sane || (fabs(_f->l[0] - _f->l[1]) < 0.01 && _d * exp2(fmin(_f->l[0], _f->l[1])) < 0.5)) {
        fude_zoom_fly_land(_f, _s, _half);
        return false;
    }
    // Out far enough in the middle that both ends are on screen together.
    const f64 _fit = log2(1.6 * fmin(_half.x, _half.y) / fmax(_d, 1e-300));
    _f->l_dip = _fit < fmin(_f->l[0], _f->l[1]) ? (_f->l[0] + _f->l[1]) * 0.5 - _fit : 0.0;
    // The way along, by eased time: by the view's width (2^-zoom), so the pan
    // happens while far out and the camera settles onto each end. The widths
    // from the furthest out, so none overflows.
    f64 _ls[FUDE_ZOOM_FLY_STEPS + 1];
    f64 _out = 1e300;
    for(u32 _i = 0; _i <= FUDE_ZOOM_FLY_STEPS; _i++) {
        _ls[_i] = fude_zoom_fly_zoom(_f, (f64)_i / (f64)FUDE_ZOOM_FLY_STEPS);
        _out    = fmin(_out, _ls[_i]);
    }
    _f->way[0] = 0.0;
    for(u32 _i = 1; _i <= FUDE_ZOOM_FLY_STEPS; _i++) {
        _f->way[_i] = _f->way[_i - 1] + 0.5 * (exp2(_out - _ls[_i - 1]) + exp2(_out - _ls[_i]));
    }
    const f64 _total = _f->way[FUDE_ZOOM_FLY_STEPS];
    for(u32 _i = 0; _i <= FUDE_ZOOM_FLY_STEPS; _i++) {
        _f->way[_i] = _total > 0.0 ? _f->way[_i] / _total : (f64)_i / (f64)FUDE_ZOOM_FLY_STEPS;
    }
    // Longer the further it zooms: half a second for a step aside, two and a
    // half across a thousand levels.
    const f64 _zooms = (_f->l[0] - _out) + (_f->l[1] - _out);
    _f->length = fmin(0.45 + 0.03 * _zooms, 2.5);
    _f->began  = _now;
    _f->active = true;
    return true;
}

b8 fude_zoom_fly_step(fude_zoom_flight* _f, fude_zoom_scene* _s, fude_zoom_v2 _half, f64 _now) {
    if(!_f->active) {
        return false;
    }
    const f64 _t = (_now - _f->began) / _f->length;
    if(_t >= 1.0) {
        fude_zoom_fly_land(_f, _s, _half);
        return false;
    }
    const f64 _e = fude_zoom_nav_ease(_t);
    const f64 _l = fude_zoom_fly_zoom(_f, _e);
    const f64 _k = _e * (f64)FUDE_ZOOM_FLY_STEPS;
    const u32 _i = (u32)_k < FUDE_ZOOM_FLY_STEPS ? (u32)_k : FUDE_ZOOM_FLY_STEPS - 1u;
    const f64 _u = _f->way[_i] + (_f->way[_i + 1u] - _f->way[_i]) * (_k - (f64)_i);
    // Worked out from the coarser end while the camera is out past it, else
    // from the deeper: walking up from it then reaches the camera's level.
    const u32 _coarse = _f->l[0] <= _f->l[1] ? 0u : 1u;
    const u32 _x      = _l <= _f->l[_coarse] ? _coarse : 1u - _coarse;
    const f64 _from_x = _x == 0u ? _u : 1.0 - _u;   // of the way from end x to the other
    const fude_zoom_camera* _end = &_f->ends[_x];
    fude_zoom_v2 _p = { _end->at.x + _from_x * (_f->cross[_x].x - _end->at.x), _end->at.y + _from_x * (_f->cross[_x].y - _end->at.y) };
    // Its zoom in that end's frame's units, and up to where that zoom belongs.
    f64 _lz = _l + _f->g[_x];
    u32 _frame = _end->frame;
    const f64 _floor = log2(FUDE_ZOOM_Z_MIN);
    while(_lz < _floor && fude_zoom_nav_parent(_s, _frame) != FUDE_ZOOM_NONE) {
        const fude_zoom_frame* _fr = fude_zoom_scene_frame(_s, _frame);
        _p      = fude_zoom_sim_apply(fude_zoom_sim_from_xform(_fr->xf), _p);
        _lz    -= log2(fabs(_fr->xf.scale));   // a parent's unit is 1/scale of its child's
        _frame  = _fr->parent;
    }
    fude_zoom_camera_look_at(_s, _frame, _p, exp2(_lz));
    fude_zoom_camera_settle(_s, _half);
    return true;
}

fude_zoom_camera fude_zoom_nav_framing(fude_zoom_v2 _half, u32 _frame, fude_zoom_box _box, f64 _fill) {
    const f64 _w = _box.max_x - _box.min_x, _h = _box.max_y - _box.min_y;
    // (A box with nothing in it, a point: framed as one 1/1000 of the frame's unit across — never a zoom without end.)
    const f64 _big = fmax(fmax(_w, _h), 1e-3);
    const f64 _z = fmin(2.0 * _half.x * _fill / fmax(_w, _big * 1e-3), 2.0 * _half.y * _fill / fmax(_h, _big * 1e-3));
    return (fude_zoom_camera){ _frame, { (_box.min_x + _box.max_x) * 0.5, (_box.min_y + _box.max_y) * 0.5 }, _z };
}

// --- marks -----------------------------------------------------------------------------------

typedef struct {
    const fude_zoom_scene* s;
    u32                    skip;       // the FRAME object of the frame the camera is in (in its parent)
    fude_zoom_sim          to_screen;  // the searched frame's units → the screen
    fude_zoom_box          view;       // the screen, in those units
    f64                    z;          // those units → screen points
    u32                    frame;
    u32                    seen;
    fude_zoom_nav_mark     best[8];
    f64                    best_d[8];  // screen points (1e300: none)
    u32                    filled;
    fude_zoom_nav_mark     rings[FUDE_ZOOM_NAV_RINGS];
    u32                    ring_count;
} fude_zoom_nav_search;

// Something drawn in frame _frame, or deeper (alive, not a bookmark or a layer's record, not
// hidden)? A frame whose drawing was all rubbed out is still "used" (its undo keeps it), and
// nothing would be seen at the end of an arrow to it.
RDE_INTERNAL b8 fude_zoom_nav_frame_shows(const fude_zoom_scene* _s, u32 _frame, u32 _depth) {
    if(_frame == FUDE_ZOOM_NONE || _depth > 16u) {
        return false;
    }
    const fude_zoom_frame* _f = fude_zoom_scene_frame(_s, _frame);
    if(_f->removed) {
        return false;
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_f->order); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, ((const u32*)_f->order.memory)[_i]);
        if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind == FUDE_ZOOM_KIND_MARK || _o->kind == FUDE_ZOOM_KIND_LAYER) {
            continue;
        }
        if(_o->kind == FUDE_ZOOM_KIND_FRAME) {
            if(fude_zoom_nav_frame_shows(_s, _o->child, _depth + 1u)) {
                return true;
            }
            continue;
        }
        if(!fude_zoom_scene_hides(_s, _o)) {
            return true;
        }
    }
    return false;
}

RDE_INTERNAL b8 fude_zoom_nav_visit(any _user, u32 _value, fude_zoom_box _box, fude_zoom_box _leaf, f64 _d2) {
    fude_zoom_nav_search*   _q = (fude_zoom_nav_search*)_user;
    const fude_zoom_object* _o = fude_zoom_scene_object(_q->s, _value);
    if(++_q->seen > 4096u) {
        return false;
    }
    // Only what is drawn: not a bookmark, not a layer's record (nothing to see, nowhere: its box is a
    // point at the origin — an arrow to it flew there zoomed in without end), not what a hidden layer
    // hides, not a frame let go of.
    if(_value == _q->skip || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || _o->kind == FUDE_ZOOM_KIND_MARK || _o->kind == FUDE_ZOOM_KIND_LAYER ||
       fude_zoom_scene_hides(_q->s, _o) || (_o->kind == FUDE_ZOOM_KIND_FRAME && !fude_zoom_nav_frame_shows(_q->s, _o->child, 0u))) {
        return true;
    }
    // What a tap shows: it and its neighbours (its leaf), unless they spread far wider.
    const f64     _size = fmax(_box.max_x - _box.min_x, _box.max_y - _box.min_y);
    if(!(_size > 0.0)) {
        return true;   // (nothing there to show)
    }
    const f64     _wide = fmax(_leaf.max_x - _leaf.min_x, _leaf.max_y - _leaf.min_y);
    fude_zoom_box _show = _wide <= _size * 16.0 ? _leaf : _box;
    const fude_zoom_v2 _c = fude_zoom_sim_apply(_q->to_screen, (fude_zoom_v2){ (_box.min_x + _box.max_x) * 0.5, (_box.min_y + _box.max_y) * 0.5 });
    if(fude_zoom_box_overlaps(_box, _q->view)) {
        // On screen: a ring when it is too small to see (else it is drawn and seen).
        if(_size * _q->z < FUDE_ZOOM_NAV_RING && _q->ring_count < FUDE_ZOOM_NAV_RINGS) {
            _q->rings[_q->ring_count++] = (fude_zoom_nav_mark){ { (f32)_c.x, (f32)_c.y }, 0.0f, true, _q->frame, _show };
        }
        return true;
    }
    const f64 _angle  = atan2(_c.y, _c.x);
    const u32 _sector = (u32)floor((_angle + 3.14159265358979) / 6.28318530717959 * 8.0 + 0.5) % 8u;
    const f64 _d      = sqrt(_d2) * _q->z;
    if(_d < _q->best_d[_sector]) {
        _q->filled        += _q->best_d[_sector] >= 1e300 ? 1u : 0u;
        _q->best_d[_sector] = _d;
        _q->best[_sector]   = (fude_zoom_nav_mark){ { 0.0f, 0.0f }, (f32)_angle, false, _q->frame, _show };
    }
    return _q->filled < 8u || _q->ring_count < FUDE_ZOOM_NAV_RINGS;
}

u32 fude_zoom_nav_marks(const fude_zoom_scene* _s, fude_zoom_v2 _half, f32 _inset, fude_zoom_nav_mark* _out) {
    // Each searched frame: the camera's, then two above it — the nearest in
    // each direction over all three.
    fude_zoom_nav_mark _best[8];
    f64                _best_d[8];
    fude_zoom_nav_mark _rings[FUDE_ZOOM_NAV_RINGS];
    u32                _ring_count = 0;
    for(u32 _i = 0; _i < 8u; _i++) {
        _best_d[_i] = 1e300;
    }
    const fude_zoom_sim _cam_to_screen = fude_zoom_camera_sim(&_s->camera);
    u32 _frame = _s->camera.frame, _skip = FUDE_ZOOM_NONE, _from = _s->camera.frame;
    for(u32 _level = 0; _level < 3u && _frame != FUDE_ZOOM_NONE; _level++) {
        fude_zoom_nav_search _q;
        memset(&_q, 0, sizeof _q);
        _q.s         = _s;
        _q.skip      = _skip;
        _q.frame     = _frame;
        _q.to_screen = fude_zoom_sim_compose(_cam_to_screen, fude_zoom_scene_sim(_s, _frame, _from));
        _q.view      = fude_zoom_sim_box(fude_zoom_sim_inverse(_q.to_screen), (fude_zoom_box){ -_half.x, -_half.y, _half.x, _half.y });
        _q.z         = fude_zoom_sim_scale(_q.to_screen);
        for(u32 _i = 0; _i < 8u; _i++) {
            _q.best_d[_i] = 1e300;
        }
        const fude_zoom_v2 _mid = fude_zoom_sim_apply(fude_zoom_sim_inverse(_q.to_screen), (fude_zoom_v2){ 0.0, 0.0 });
        fude_zoom_index_nearest(&fude_zoom_scene_frame(_s, _frame)->index, _mid, fude_zoom_nav_visit, &_q, 4096u);
        // The frames in it are not in its index: each looked at too (what is drawn deeper).
        const fude_zoom_frame* _fr = fude_zoom_scene_frame(_s, _frame);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_fr->kids); _i++) {
            const u32               _k = ((const u32*)_fr->kids.memory)[_i];
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _k);
            if(_k == _skip || !fude_zoom_scene_frame_used(_s, _o->child)) {
                continue;
            }
            const f64 _dx = _mid.x < _o->box.min_x ? _o->box.min_x - _mid.x : (_mid.x > _o->box.max_x ? _mid.x - _o->box.max_x : 0.0);
            const f64 _dy = _mid.y < _o->box.min_y ? _o->box.min_y - _mid.y : (_mid.y > _o->box.max_y ? _mid.y - _o->box.max_y : 0.0);
            const u32 _seen = _q.seen;
            fude_zoom_nav_visit(&_q, _k, _o->box, _o->box, _dx * _dx + _dy * _dy);
            _q.seen = _seen;
        }
        for(u32 _i = 0; _i < 8u; _i++) {
            if(_q.best_d[_i] < _best_d[_i]) {
                _best_d[_i] = _q.best_d[_i];
                _best[_i]   = _q.best[_i];
            }
        }
        for(u32 _i = 0; _i < _q.ring_count && _ring_count < FUDE_ZOOM_NAV_RINGS; _i++) {
            _rings[_ring_count++] = _q.rings[_i];
        }
        _skip  = fude_zoom_scene_frame(_s, _frame)->object;
        _frame = fude_zoom_scene_frame(_s, _frame)->parent;
    }
    // The arrows nearest first, none within a third of a radian of a nearer one,
    // each where its way out meets the inset edge.
    u32 _n = 0;
    for(;;) {
        i32 _pick = -1;
        for(u32 _i = 0; _i < 8u; _i++) {
            if(_best_d[_i] < 1e300 && (_pick < 0 || _best_d[_i] < _best_d[_pick])) {
                _pick = (i32)_i;
            }
        }
        if(_pick < 0) {
            break;
        }
        fude_zoom_nav_mark _m = _best[_pick];
        _best_d[_pick] = 1e300;
        b8 _close = false;
        for(u32 _j = 0; _j < _n && !_close; _j++) {
            f32 _da = fabsf(_out[_j].angle - _m.angle);
            _da     = _da > 3.14159265f ? 6.2831853f - _da : _da;
            _close  = _da < 0.33f;
        }
        if(_close) {
            continue;
        }
        const f32 _cx = cosf(_m.angle), _cy = sinf(_m.angle);
        const f32 _hx = (f32)_half.x - _inset, _hy = (f32)_half.y - _inset;
        const f32 _t  = fminf(fabsf(_cx) > 1e-6f ? _hx / fabsf(_cx) : 1e30f, fabsf(_cy) > 1e-6f ? _hy / fabsf(_cy) : 1e30f);
        _m.at = (rde_vec_2F){ _cx * _t, _cy * _t };
        _out[_n++] = _m;
    }
    for(u32 _i = 0; _i < _ring_count && _n < FUDE_ZOOM_NAV_MARKS; _i++) {
        _out[_n++] = _rings[_i];
    }
    return _n;
}

// --- depth -----------------------------------------------------------------------------------

// log10 of a unit of frame _f in the root's units.
RDE_INTERNAL f64 fude_zoom_nav_log10_unit(const fude_zoom_scene* _s, u32 _f) {
    f64 _g = 0.0;
    for(; _f != FUDE_ZOOM_NONE && fude_zoom_nav_parent(_s, _f) != FUDE_ZOOM_NONE; _f = fude_zoom_nav_parent(_s, _f)) {
        _g += log10(fabs(fude_zoom_scene_frame(_s, _f)->xf.scale));
    }
    return _g;
}

f64 fude_zoom_nav_depth_of(const fude_zoom_scene* _s, fude_zoom_camera _c) {
    // The zoom in home's units: a unit of the camera's frame is 10^(g_cam − g_home) of home's.
    return log10(_c.z) - (fude_zoom_nav_log10_unit(_s, _c.frame) - fude_zoom_nav_log10_unit(_s, _s->home));
}

f64 fude_zoom_nav_depth(const fude_zoom_scene* _s) {
    return fude_zoom_nav_depth_of(_s, _s->camera);
}

void fude_zoom_nav_say(f64 _log10, c8* _out, usize _size) {
    if(_size == 0) {
        return;
    }
    if(_log10 >= -4.0 && _log10 < 3.0) {
        const f64 _m = pow(10.0, _log10);
        if(_m >= 9.95) {
            snprintf(_out, _size, "\xC3\x97%.0f", _m);
        } else if(_m >= 0.995) {
            snprintf(_out, _size, "\xC3\x97%.1f", _m);
        } else {
            // One figure that counts: ×0.5, ×0.05, ×0.004.
            snprintf(_out, _size, "\xC3\x97%.*f", (int)ceil(-_log10 - 1e-9), _m);
        }
        // "×2.0" says no more than "×2".
        const usize _n = strlen(_out);
        if(_n > 2u && _out[_n - 1u] == '0' && _out[_n - 2u] == '.') {
            _out[_n - 2u] = 0;
        }
        return;
    }
    // Its first figure (left out when it is 1) times 10 to the power, the power
    // in superscript digits: ×10⁶, ×3·10³, ×4·10⁻⁶.
    static const c8* const SUPER[10] = { "\xE2\x81\xB0", "\xC2\xB9", "\xC2\xB2", "\xC2\xB3", "\xE2\x81\xB4", "\xE2\x81\xB5", "\xE2\x81\xB6", "\xE2\x81\xB7", "\xE2\x81\xB8", "\xE2\x81\xB9" };
    i32 _e    = (i32)floor(_log10);
    i32 _lead = (i32)floor(pow(10.0, _log10 - (f64)_e) + 0.5);
    if(_lead >= 10) {
        _lead = 1;
        _e++;
    }
    c8 _digits[16];
    snprintf(_digits, sizeof _digits, "%d", _e < 0 ? -_e : _e);
    if(_lead > 1) {
        snprintf(_out, _size, "\xC3\x97%d\xC2\xB7" "10%s", _lead, _e < 0 ? "\xE2\x81\xBB" : "");
    } else {
        snprintf(_out, _size, "\xC3\x97" "10%s", _e < 0 ? "\xE2\x81\xBB" : "");
    }
    for(const c8* _d = _digits; *_d != 0; _d++) {
        const usize _n = strlen(_out);
        snprintf(_out + _n, _size - _n, "%s", SUPER[*_d - '0']);
    }
}
