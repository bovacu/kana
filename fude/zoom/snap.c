// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/snap.h"
#include "zoom/shape.h"
#include <math.h>
#include <stdlib.h>

// A candidate point, in a frame's units: kept when it beats the best so far
// (a better kind, or the same kind nearer).
// ...as a point of _object's own (_key: connect.h's), or of nothing in particular (fude_zoom_snap_offer).
RDE_INTERNAL void fude_zoom_snap_offer_of(fude_zoom_snap* _best, f64* _best_d, fude_zoom_sim _to, fude_zoom_v2 _p, u8 _kind, fude_zoom_v2 _screen, f64 _radius,
                                          u32 _object, i32 _key) {
    const fude_zoom_v2 _q = fude_zoom_sim_apply(_to, _p);
    const f64          _d = hypot(_q.x - _screen.x, _q.y - _screen.y);
    if(_d > _radius) {
        return;
    }
    if(_best->kind == FUDE_ZOOM_SNAP_NONE || _kind < _best->kind || (_kind == _best->kind && _d < *_best_d)) {
        _best->kind   = _kind;
        _best->at     = _q;
        _best->object = _object;
        _best->key    = _key;
        *_best_d      = _d;
    }
}

RDE_INTERNAL void fude_zoom_snap_offer(fude_zoom_snap* _best, f64* _best_d, fude_zoom_sim _to, fude_zoom_v2 _p, u8 _kind, fude_zoom_v2 _screen, f64 _radius) {
    fude_zoom_snap_offer_of(_best, _best_d, _to, _p, _kind, _screen, _radius, FUDE_ZOOM_NONE, FUDE_ZOOM_SNAP_NO_KEY);
}

// A piece of something drawn near the pen, on the screen: where crossings are looked for.
typedef struct {
    fude_zoom_v2 a, b;
    u32          object;
} fude_zoom_snap_piece;

#define FUDE_ZOOM_SNAP_PIECES 600u   // pieces looked at for crossings, at most

// The pieces of a line (screen points) near the pen kept for crossings.
RDE_INTERNAL void fude_zoom_snap_pieces(rde_arr* _pieces, const fude_zoom_v2* _p, u32 _n, b8 _closed, u32 _object, fude_zoom_v2 _screen, f64 _radius) {
    const u32 _m = _closed ? _n : (_n > 0 ? _n - 1u : 0u);
    for(u32 _i = 0; _i < _m && (u32)rde_arr_length(_pieces) < FUDE_ZOOM_SNAP_PIECES; _i++) {
        const fude_zoom_v2 _a = _p[_i], _b = _p[(_i + 1u) % _n];
        if(fmax(_a.x, _b.x) < _screen.x - _radius || fmin(_a.x, _b.x) > _screen.x + _radius || fmax(_a.y, _b.y) < _screen.y - _radius || fmin(_a.y, _b.y) > _screen.y + _radius) {
            continue;
        }
        const fude_zoom_snap_piece _piece = { _a, _b, _object };
        rde_arr_add(_pieces, (any)&_piece);
    }
}

// What is near the pen (its pieces kept in _pieces for what is reached from a point).
RDE_INTERNAL fude_zoom_snap fude_zoom_snap_search(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _screen, f64 _radius,
                                                  rde_arr* _pieces, f64* _best_out);

fude_zoom_snap fude_zoom_snap_find(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _screen, f64 _radius) {
    rde_arr _pieces = rde_arr_new(sizeof(fude_zoom_snap_piece), rde_memory_allocator_get_default_std());
    f64 _d;
    const fude_zoom_snap _best = fude_zoom_snap_search(_s, _frames, _frame_count, _screen, _radius, &_pieces, &_d);
    rde_arr_free(&_pieces);
    return _best;
}

// A point of the screen offered as _kind, with the piece _a-_b it comes of.
RDE_INTERNAL void fude_zoom_snap_offer_by(fude_zoom_snap* _best, f64* _best_d, fude_zoom_v2 _p, u8 _kind, fude_zoom_v2 _screen, f64 _radius, fude_zoom_v2 _a, fude_zoom_v2 _b) {
    const u8 _was = _best->kind;
    const fude_zoom_v2 _at = _best->at;
    fude_zoom_snap_offer_of(_best, _best_d, (fude_zoom_sim){ 1.0, 0.0, 0.0, 0.0 }, _p, _kind, _screen, _radius, FUDE_ZOOM_NONE, FUDE_ZOOM_SNAP_NO_KEY);
    if(_best->kind != _was || _best->at.x != _at.x || _best->at.y != _at.y) {
        _best->a = _a;
        _best->b = _b;
    }
}

// Straight things' sides near the pen (lines, rectangles without round corners, polygons, plain boards, sheets,
// dimensions), on the screen, into _out (fude_zoom_snap_piece; at most FUDE_ZOOM_SNAP_WAYS * 8).
RDE_INTERNAL void fude_zoom_snap_straights(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _screen, f64 _reach, rde_arr* _out) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _found = rde_arr_new(sizeof(u32), _heap), _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    for(u32 _f = 0; _f < _frame_count && (u32)rde_arr_length(_out) < 64u; _f++) {
        const fude_zoom_sim _to = _frames[_f].to_screen;
        const f64 _scale = fude_zoom_sim_scale(_to);
        if(!(_scale > 0.0)) {
            continue;
        }
        const fude_zoom_v2 _c = fude_zoom_sim_apply(fude_zoom_sim_inverse(_to), _screen);
        const f64 _r = _reach / _scale;
        rde_arr_clear(&_found);
        fude_zoom_scene_query(_s, _frames[_f].frame, (fude_zoom_box){ _c.x - _r, _c.y - _r, _c.x + _r, _c.y + _r }, &_found);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_found) && (u32)rde_arr_length(_out) < 64u; _i++) {
            const u32 _object = ((const u32*)_found.memory)[_i];
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || fude_zoom_scene_hides(_s, _o) || _o->kind != FUDE_ZOOM_KIND_SHAPE) {
                continue;
            }
            const u8 _type = _o->channels;
            f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
            const u32 _count = fude_zoom_scene_shape_numbers(_s, _object, _n, FUDE_ZOOM_SHAPE_NUMBERS);
            const b8 _straight = _type == FUDE_ZOOM_SHAPE_LINE || _type == FUDE_ZOOM_SHAPE_POLYGON || _type == FUDE_ZOOM_SHAPE_SHEET || _type == FUDE_ZOOM_SHAPE_DIMENSION ||
                                 _type == FUDE_ZOOM_SHAPE_GUIDE ||
                                 (_type == FUDE_ZOOM_SHAPE_RECT && (_count < 3u || _n[2] <= 0.0)) ||
                                 (_type == FUDE_ZOOM_SHAPE_BOARD && !fude_zoom_board_is_cut(_n, _count));
            if(!_straight) {
                continue;
            }
            b8 _closed;
            rde_arr_clear(&_pts);
            fude_zoom_scene_shape_outline(_s, _object, 1u, &_pts, &_closed);
            const u32 _m = (u32)rde_arr_length(&_pts);
            const fude_zoom_v2* _p = (const fude_zoom_v2*)_pts.memory;
            // (a dimension: only its line, between its own two ends)
            const u32 _from = _type == FUDE_ZOOM_SHAPE_DIMENSION ? 1u : 0u, _to_i = _type == FUDE_ZOOM_SHAPE_DIMENSION ? 2u : (_closed ? _m : _m - 1u);
            for(u32 _k = _from; _k < _to_i && _m >= 2u && (u32)rde_arr_length(_out) < 64u; _k++) {
                const fude_zoom_snap_piece _piece = { fude_zoom_sim_apply(_to, _p[_k]), fude_zoom_sim_apply(_to, _p[(_k + 1u) % _m]), _object };
                if(hypot(_piece.b.x - _piece.a.x, _piece.b.y - _piece.a.y) >= 8.0) {
                    rde_arr_add(_out, (any)&_piece);
                }
            }
        }
    }
    rde_arr_free(&_found);
    rde_arr_free(&_pts);
}

u32 fude_zoom_snap_rays(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _vertex, f64 _tol, fude_zoom_v2* _out, u32 _max) {
    rde_arr _straights = rde_arr_new(sizeof(fude_zoom_snap_piece), rde_memory_allocator_get_default_std());
    fude_zoom_snap_straights(_s, _frames, _frame_count, _vertex, _tol + 1.0, &_straights);
    const fude_zoom_snap_piece* _sp = (const fude_zoom_snap_piece*)_straights.memory;
    u32 _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_straights); _i++) {
        const fude_zoom_v2 _a = _sp[_i].a, _b = _sp[_i].b;
        const fude_zoom_v2 _d = { _b.x - _a.x, _b.y - _a.y };
        const f64 _l = hypot(_d.x, _d.y);
        if(!(_l > 0.0)) {
            continue;
        }
        fude_zoom_v2 _ways[2];
        u32 _w = 0;
        if(hypot(_a.x - _vertex.x, _a.y - _vertex.y) <= _tol) {
            _ways[_w++] = (fude_zoom_v2){ _d.x / _l, _d.y / _l };
        } else if(hypot(_b.x - _vertex.x, _b.y - _vertex.y) <= _tol) {
            _ways[_w++] = (fude_zoom_v2){ -_d.x / _l, -_d.y / _l };
        } else {
            const f64 _t = ((_vertex.x - _a.x) * _d.x + (_vertex.y - _a.y) * _d.y) / (_l * _l);
            const fude_zoom_v2 _f = { _a.x + _d.x * _t, _a.y + _d.y * _t };
            if(_t > 0.0 && _t < 1.0 && hypot(_f.x - _vertex.x, _f.y - _vertex.y) <= _tol) {
                _ways[_w++] = (fude_zoom_v2){ _d.x / _l, _d.y / _l };
                _ways[_w++] = (fude_zoom_v2){ -_d.x / _l, -_d.y / _l };
            }
        }
        for(u32 _k = 0; _k < _w && _n < _max; _k++) {
            b8 _seen = false;
            for(u32 _j = 0; _j < _n && !_seen; _j++) {
                _seen = _out[_j].x * _ways[_k].x + _out[_j].y * _ways[_k].y > cos(0.5 * 3.14159265358979323846 / 180.0);
            }
            if(!_seen) {
                _out[_n++] = _ways[_k];
            }
        }
    }
    rde_arr_free(&_straights);
    return _n;
}

// Circles and arcs whose round passes near the pen (screen): each one's centre and radius, into _out (pairs of
// fude_zoom_v2: its centre, then its radius in x).
RDE_INTERNAL void fude_zoom_snap_rounds(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _screen, f64 _radius, rde_arr* _out) {
    rde_arr _found = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    for(u32 _f = 0; _f < _frame_count && (u32)rde_arr_length(_out) < 64u; _f++) {
        const fude_zoom_sim _to = _frames[_f].to_screen;
        const f64 _scale = fude_zoom_sim_scale(_to);
        if(!(_scale > 0.0)) {
            continue;
        }
        const fude_zoom_v2 _c = fude_zoom_sim_apply(fude_zoom_sim_inverse(_to), _screen);
        const f64 _r = _radius / _scale;
        rde_arr_clear(&_found);
        fude_zoom_scene_query(_s, _frames[_f].frame, (fude_zoom_box){ _c.x - _r, _c.y - _r, _c.x + _r, _c.y + _r }, &_found);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
            const u32 _object = ((const u32*)_found.memory)[_i];
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _object);
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || fude_zoom_scene_hides(_s, _o) || _o->kind != FUDE_ZOOM_KIND_SHAPE ||
               (_o->channels != FUDE_ZOOM_SHAPE_ELLIPSE && _o->channels != FUDE_ZOOM_SHAPE_ARC)) {
                continue;
            }
            f64 _n[3];
            const u32 _count = fude_zoom_scene_shape_numbers(_s, _object, _n, 3u);
            if(_count < 2u || (_o->channels == FUDE_ZOOM_SHAPE_ELLIPSE && fabs(fabs(_n[0]) - fabs(_n[1])) > 1e-6 * fabs(_n[0]))) {
                continue;   // (an ellipse: no one radius)
            }
            const fude_zoom_sim _all = fude_zoom_sim_compose(_to, fude_zoom_object_sim(_o));
            const fude_zoom_v2 _pair[2] = { fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 }), { fabs(_n[0]) * fude_zoom_sim_scale(_all), 0.0 } };
            if(fabs(hypot(_screen.x - _pair[0].x, _screen.y - _pair[0].y) - _pair[1].x) <= 2.0 * _radius) {
                rde_arr_add(_out, (any)&_pair[0]);
                rde_arr_add(_out, (any)&_pair[1]);
            }
        }
    }
    rde_arr_free(&_found);
}

fude_zoom_snap fude_zoom_snap_find_from(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _screen, f64 _radius,
                                        fude_zoom_v2 _from, const fude_zoom_v2* _ways, u32 _way_count, f64 _step_deg) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _pieces = rde_arr_new(sizeof(fude_zoom_snap_piece), _heap);
    f64 _best_d = 1e300;
    fude_zoom_snap _best = fude_zoom_snap_search(_s, _frames, _frame_count, _screen, _radius, &_pieces, &_best_d);
    const f64 _away = hypot(_screen.x - _from.x, _screen.y - _from.y);
    if((_best.kind != FUDE_ZOOM_SNAP_NONE && _best.kind < FUDE_ZOOM_SNAP_PERP) || _away < 2.0 * _radius) {
        rde_arr_free(&_pieces);
        return _best;   // (on something drawn: that; too near the start for a way to show)
    }
    // Square to a piece near the pen: its foot.
    const fude_zoom_snap_piece* _pc = (const fude_zoom_snap_piece*)_pieces.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_pieces); _i++) {
        const fude_zoom_v2 _d = { _pc[_i].b.x - _pc[_i].a.x, _pc[_i].b.y - _pc[_i].a.y };
        const f64 _ll = _d.x * _d.x + _d.y * _d.y;
        if(_ll < 1e-12) {
            continue;
        }
        const f64 _t = ((_from.x - _pc[_i].a.x) * _d.x + (_from.y - _pc[_i].a.y) * _d.y) / _ll;
        if(_t < 0.0 || _t > 1.0) {
            continue;
        }
        const fude_zoom_v2 _foot = { _pc[_i].a.x + _d.x * _t, _pc[_i].a.y + _d.y * _t };
        if(hypot(_foot.x - _from.x, _foot.y - _from.y) >= 2.0 * _radius) {
            fude_zoom_snap_offer_by(&_best, &_best_d, _foot, FUDE_ZOOM_SNAP_PERP, _screen, _radius, _pc[_i].a, _pc[_i].b);
        }
    }
    // Touching a circle or an arc near the pen: where the line from _from is its tangent (the one of the two nearer).
    {
        rde_arr _rounds = rde_arr_new(sizeof(fude_zoom_v2), _heap);
        fude_zoom_snap_rounds(_s, _frames, _frame_count, _screen, _radius, &_rounds);
        const fude_zoom_v2* _rp = (const fude_zoom_v2*)_rounds.memory;
        for(u32 _i = 0; _i + 1u < (u32)rde_arr_length(&_rounds); _i += 2u) {
            const fude_zoom_v2 _c = _rp[_i];
            const f64 _r = _rp[_i + 1u].x, _dc = hypot(_from.x - _c.x, _from.y - _c.y);
            if(!(_dc > _r * (1.0 + 1e-9))) {
                continue;   // (from inside it: none)
            }
            const f64 _th = atan2(_from.y - _c.y, _from.x - _c.x), _al = acos(_r / _dc);
            for(u32 _k = 0; _k < 2u; _k++) {
                const f64 _a = _th + (_k == 0u ? _al : -_al);
                const fude_zoom_v2 _t = { _c.x + cos(_a) * _r, _c.y + sin(_a) * _r };
                fude_zoom_snap_offer_by(&_best, &_best_d, _t, FUDE_ZOOM_SNAP_TANGENT, _screen, _radius, _c, (fude_zoom_v2){ _r, 0.0 });
            }
        }
        rde_arr_free(&_rounds);
    }
    // Along a straight thing near, or a way asked for (or one of its turns every _step_deg): the pen's point brought
    // onto the line from _from that way. Carried on past a straight thing's end: onto that.
    {
        rde_arr _straights = rde_arr_new(sizeof(fude_zoom_snap_piece), _heap);
        fude_zoom_snap_straights(_s, _frames, _frame_count, _screen, FUDE_ZOOM_SNAP_ALONG, &_straights);
        const u32 _ns = (u32)rde_arr_length(&_straights);
        const fude_zoom_snap_piece* _sp = (const fude_zoom_snap_piece*)_straights.memory;
        const u32 _steps = _step_deg > 0.0 && _way_count > 0u ? (u32)floor(180.0 / _step_deg + 1e-9) : 0u;
        for(u32 _i = 0; _i < _ns + _way_count + _steps; _i++) {
            fude_zoom_v2 _d, _a, _b;
            f64 _tol = _radius * 0.7;
            if(_i < _ns) {
                _d = (fude_zoom_v2){ _sp[_i].b.x - _sp[_i].a.x, _sp[_i].b.y - _sp[_i].a.y };
                _a = _sp[_i].a;
                _b = _sp[_i].b;
            } else if(_i < _ns + _way_count) {
                _d = _ways[_i - _ns];
                _a = _b = _from;
            } else {
                const f64 _turn = (f64)(_i - _ns - _way_count) * _step_deg * 3.14159265358979323846 / 180.0, _c = cos(_turn), _sn = sin(_turn);
                _d = (fude_zoom_v2){ _ways[0].x * _c - _ways[0].y * _sn, _ways[0].x * _sn + _ways[0].y * _c };
                _a = _b = _from;
                _tol = _radius * 0.35;   // (an angle's step: only near it)
            }
            const f64 _l = hypot(_d.x, _d.y);
            if(!(_l > 0.0)) {
                continue;
            }
            _d.x /= _l; _d.y /= _l;
            const f64 _along = (_screen.x - _from.x) * _d.x + (_screen.y - _from.y) * _d.y;
            const fude_zoom_v2 _p = { _from.x + _d.x * _along, _from.y + _d.y * _along };
            fude_zoom_snap_offer_by(&_best, &_best_d, _p, FUDE_ZOOM_SNAP_PARALLEL, _screen, _tol, _a, _b);
            if(_i < _ns) {
                // Its own line carried on past an end, the pen near it (not far out): onto it.
                const f64 _t = ((_screen.x - _a.x) * _d.x + (_screen.y - _a.y) * _d.y) / _l;
                const f64 _past = _t < 0.0 ? -_t * _l : (_t > 1.0 ? (_t - 1.0) * _l : 0.0);
                if(_past > 0.0 && _past <= FUDE_ZOOM_SNAP_FAR_ON) {
                    const fude_zoom_v2 _e = { _a.x + _d.x * _t * _l, _a.y + _d.y * _t * _l };
                    fude_zoom_snap_offer_by(&_best, &_best_d, _e, FUDE_ZOOM_SNAP_EXTENSION, _screen, _radius * 0.7, _a, _b);
                }
            }
        }
        rde_arr_free(&_straights);
    }
    rde_arr_free(&_pieces);
    return _best;
}

RDE_INTERNAL fude_zoom_snap fude_zoom_snap_search(const fude_zoom_scene* _s, const fude_zoom_visible* _frames, u32 _frame_count, fude_zoom_v2 _screen, f64 _radius,
                                                  rde_arr* _pieces_out, f64* _best_out) {
    fude_zoom_snap _best = { FUDE_ZOOM_SNAP_NONE, _screen, { 0.0, 0.0 }, { 0.0, 0.0 }, FUDE_ZOOM_NONE, FUDE_ZOOM_SNAP_NO_KEY };
    f64            _best_d = 1e300;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _found = rde_arr_new(sizeof(u32), _heap);
    rde_arr _q     = rde_arr_new(sizeof(fude_zoom_qpoint), _heap);
    rde_arr _pts   = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr _line   = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    for(u32 _f = 0; _f < _frame_count; _f++) {
        const fude_zoom_sim _to    = _frames[_f].to_screen;
        const fude_zoom_sim _back  = fude_zoom_sim_inverse(_to);
        const f64           _scale = fude_zoom_sim_scale(_to);
        if(!(_scale > 0.0)) {
            continue;
        }
        const fude_zoom_v2 _c = fude_zoom_sim_apply(_back, _screen);
        const f64          _r = _radius / _scale;
        rde_arr_clear(&_found);
        fude_zoom_scene_query(_s, _frames[_f].frame, (fude_zoom_box){ _c.x - _r, _c.y - _r, _c.x + _r, _c.y + _r }, &_found);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
            const u32               _object = ((const u32*)_found.memory)[_i];
            const fude_zoom_object* _o      = fude_zoom_scene_object(_s, _object);
            if(!(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || fude_zoom_scene_hides(_s, _o)) {
                continue;
            }
            if(_o->kind == FUDE_ZOOM_KIND_STROKE && _o->count > 0) {
                rde_arr_clear(&_q);
                rde_arr_add_n(&_q, _o->count);
                if(fude_zoom_scene_points(_s, _object, (fude_zoom_qpoint*)_q.memory)) {
                    const fude_zoom_qpoint* _p = (const fude_zoom_qpoint*)_q.memory;
                    fude_zoom_snap_offer_of(&_best, &_best_d, _to, fude_zoom_scene_point_at(_o, &_p[0]), FUDE_ZOOM_SNAP_END, _screen, _radius, _object, 0);
                    fude_zoom_snap_offer_of(&_best, &_best_d, _to, fude_zoom_scene_point_at(_o, &_p[_o->count - 1u]), FUDE_ZOOM_SNAP_END, _screen, _radius, _object, 1);
                    rde_arr_clear(&_line);
                    fude_zoom_v2* _l = rde_arr_add_n(&_line, _o->count);
                    for(u32 _k = 0; _k < _o->count; _k++) {
                        _l[_k] = fude_zoom_sim_apply(_to, fude_zoom_scene_point_at(_o, &_p[_k]));
                    }
                    fude_zoom_snap_pieces(_pieces_out, _l, _o->count, false, _object, _screen, _radius);
                }
            } else if(_o->kind == FUDE_ZOOM_KIND_SHAPE) {
                b8 _closed;
                rde_arr_clear(&_pts);
                const u8 _type = _o->channels;
                {
                    // Its line as drawn, for crossings.
                    b8 _shut = false;
                    rde_arr_clear(&_line);
                    fude_zoom_scene_shape_outline(_s, _object, 96u, &_line, &_shut);
                    fude_zoom_v2* _l = (fude_zoom_v2*)_line.memory;
                    for(u32 _k = 0; _k < (u32)rde_arr_length(&_line); _k++) {
                        _l[_k] = fude_zoom_sim_apply(_to, _l[_k]);
                    }
                    fude_zoom_snap_pieces(_pieces_out, _l, (u32)rde_arr_length(&_line), _shut, _object, _screen, _radius);
                }
                if(_type == FUDE_ZOOM_SHAPE_GUIDE || fude_zoom_shape_is_attribute(_type)) {
                    continue;   // a guide: where things cross it and along it only (its ends are no ends); a constraint: nothing
                }
                // Corners exactly: a rectangle's and a polygon's outline points are their corners (no round corners asked for).
                fude_zoom_scene_shape_outline(_s, _object, _type == FUDE_ZOOM_SHAPE_ELLIPSE ? 64u : 1u, &_pts, &_closed);
                const u32           _n = (u32)rde_arr_length(&_pts);
                const fude_zoom_v2* _p = (const fude_zoom_v2*)_pts.memory;
                if(_n == 0) {
                    continue;
                }
                if(_type == FUDE_ZOOM_SHAPE_ELLIPSE) {
                    // Its middle and its four ends (the outline starts at one, a quarter round each).
                    fude_zoom_v2 _mid = { 0.0, 0.0 };
                    for(u32 _k = 0; _k < _n; _k++) { _mid.x += _p[_k].x; _mid.y += _p[_k].y; }
                    _mid.x /= (f64)_n; _mid.y /= (f64)_n;
                    fude_zoom_snap_offer_of(&_best, &_best_d, _to, _mid, FUDE_ZOOM_SNAP_CENTRE, _screen, _radius, _object, FUDE_ZOOM_SNAP_CENTRE_KEY);
                    for(u32 _k = 0; _k < 4u; _k++) {
                        fude_zoom_snap_offer(&_best, &_best_d, _to, _p[(_k * _n) / 4u], FUDE_ZOOM_SNAP_CORNER, _screen, _radius);
                    }
                    continue;
                }
                if(_type == FUDE_ZOOM_SHAPE_SYMBOL) {
                    // A diagram's symbol: its box's corners, the middles of its sides (where connectors meet it) and its middle.
                    f64 _sn[3];
                    if(fude_zoom_scene_shape_numbers(_s, _object, _sn, 3u) == 3u) {
                        const fude_zoom_sim _os = fude_zoom_object_sim(_o);
                        const f64 _hw = _sn[1], _hh = _sn[2];
                        const fude_zoom_v2 _corner[4] = { { -_hw, -_hh }, { _hw, -_hh }, { _hw, _hh }, { -_hw, _hh } };
                        const fude_zoom_v2 _side[4]   = { { 0, -_hh }, { _hw, 0 }, { 0, _hh }, { -_hw, 0 } };
                        for(u32 _k = 0; _k < 4u; _k++) {
                            fude_zoom_snap_offer(&_best, &_best_d, _to, fude_zoom_sim_apply(_os, _corner[_k]), FUDE_ZOOM_SNAP_CORNER, _screen, _radius);
                            fude_zoom_snap_offer(&_best, &_best_d, _to, fude_zoom_sim_apply(_os, _side[_k]), FUDE_ZOOM_SNAP_MID, _screen, _radius);
                        }
                        fude_zoom_snap_offer_of(&_best, &_best_d, _to, fude_zoom_sim_apply(_os, (fude_zoom_v2){ 0.0, 0.0 }), FUDE_ZOOM_SNAP_CENTRE, _screen, _radius, _object, FUDE_ZOOM_SNAP_CENTRE_KEY);
                    }
                    continue;
                }
                if(_type == FUDE_ZOOM_SHAPE_ARC && _n == 3u) {
                    // Its ends, the middle of its round (not of a chord) and its centre.
                    fude_zoom_snap_offer_of(&_best, &_best_d, _to, _p[0], FUDE_ZOOM_SNAP_END, _screen, _radius, _object, 0);
                    fude_zoom_snap_offer_of(&_best, &_best_d, _to, _p[2], FUDE_ZOOM_SNAP_END, _screen, _radius, _object, 2);
                    fude_zoom_snap_offer(&_best, &_best_d, _to, _p[1], FUDE_ZOOM_SNAP_MID, _screen, _radius);
                    fude_zoom_snap_offer_of(&_best, &_best_d, _to, fude_zoom_sim_apply(fude_zoom_object_sim(_o), (fude_zoom_v2){ 0.0, 0.0 }), FUDE_ZOOM_SNAP_CENTRE, _screen, _radius,
                                            _object, FUDE_ZOOM_SNAP_CENTRE_KEY);   // (its translation: its centre)
                    continue;
                }
                const b8 _ends = !_closed;
                fude_zoom_v2 _mid = { 0.0, 0.0 };
                for(u32 _k = 0; _k < _n; _k++) {
                    fude_zoom_snap_offer_of(&_best, &_best_d, _to, _p[_k], _ends ? FUDE_ZOOM_SNAP_END : FUDE_ZOOM_SNAP_CORNER, _screen, _radius, _object, (i32)_k);
                    _mid.x += _p[_k].x; _mid.y += _p[_k].y;
                    if(_k + 1u < _n || _closed) {
                        const fude_zoom_v2 _b = _p[(_k + 1u) % _n];
                        fude_zoom_snap_offer(&_best, &_best_d, _to, (fude_zoom_v2){ (_p[_k].x + _b.x) * 0.5, (_p[_k].y + _b.y) * 0.5 }, FUDE_ZOOM_SNAP_MID, _screen, _radius);
                    }
                }
                if(_closed) {
                    fude_zoom_snap_offer_of(&_best, &_best_d, _to, (fude_zoom_v2){ _mid.x / (f64)_n, _mid.y / (f64)_n }, FUDE_ZOOM_SNAP_CENTRE, _screen, _radius, _object, FUDE_ZOOM_SNAP_CENTRE_KEY);
                }
            }
        }
    }
    // Where two different things cross, near the pen.
    const fude_zoom_snap_piece* _pc = (const fude_zoom_snap_piece*)_pieces_out->memory;
    const u32 _np = (u32)rde_arr_length(_pieces_out);
    const b8 _better = _best.kind == FUDE_ZOOM_SNAP_END || _best.kind == FUDE_ZOOM_SNAP_CORNER;   // (an end or a corner there already: it wins)
    for(u32 _i = 0; _i < _np && !_better; _i++) {
        for(u32 _j = _i + 1u; _j < _np; _j++) {
            if(_pc[_i].object == _pc[_j].object) {
                continue;
            }
            const fude_zoom_v2 _d = { _pc[_i].b.x - _pc[_i].a.x, _pc[_i].b.y - _pc[_i].a.y }, _e = { _pc[_j].b.x - _pc[_j].a.x, _pc[_j].b.y - _pc[_j].a.y };
            const f64 _den = _d.x * _e.y - _d.y * _e.x;
            if(fabs(_den) < 1e-12) {
                continue;
            }
            const fude_zoom_v2 _w = { _pc[_j].a.x - _pc[_i].a.x, _pc[_j].a.y - _pc[_i].a.y };
            const f64 _t = (_w.x * _e.y - _w.y * _e.x) / _den, _u = (_w.x * _d.y - _w.y * _d.x) / _den;
            if(_t < 0.0 || _t > 1.0 || _u < 0.0 || _u > 1.0) {
                continue;
            }
            const fude_zoom_v2 _x = { _pc[_i].a.x + _d.x * _t, _pc[_i].a.y + _d.y * _t };
            fude_zoom_snap_offer(&_best, &_best_d, (fude_zoom_sim){ 1.0, 0.0, 0.0, 0.0 }, _x, FUDE_ZOOM_SNAP_CROSS, _screen, _radius);
        }
    }
    // Nothing better: on a line or an outline right under the pen, its nearest point.
    if(_best.kind == FUDE_ZOOM_SNAP_NONE) {
        f64 _nd = FUDE_ZOOM_SNAP_REACH_ON;
        for(u32 _i = 0; _i < _np; _i++) {
            const fude_zoom_v2 _d = { _pc[_i].b.x - _pc[_i].a.x, _pc[_i].b.y - _pc[_i].a.y };
            const f64 _ll = _d.x * _d.x + _d.y * _d.y;
            const f64 _t  = _ll > 0.0 ? fmin(fmax(((_screen.x - _pc[_i].a.x) * _d.x + (_screen.y - _pc[_i].a.y) * _d.y) / _ll, 0.0), 1.0) : 0.0;
            const fude_zoom_v2 _q = { _pc[_i].a.x + _d.x * _t, _pc[_i].a.y + _d.y * _t };
            const f64 _dq = hypot(_q.x - _screen.x, _q.y - _screen.y);
            if(_dq <= _nd) {
                _nd   = _dq;
                _best = (fude_zoom_snap){ FUDE_ZOOM_SNAP_NEAREST, _q, _pc[_i].a, _pc[_i].b, FUDE_ZOOM_NONE, FUDE_ZOOM_SNAP_NO_KEY };
                _best_d = _dq;
            }
        }
    }
    rde_arr_free(&_found);
    rde_arr_free(&_q);
    rde_arr_free(&_pts);
    rde_arr_free(&_line);
    *_best_out = _best_d;
    return _best;
}

void fude_zoom_snap_align(const fude_zoom_v2* _movers, u32 _nm, const fude_zoom_v2* _targets, u32 _nt, f64 _dx, f64 _dy, f64 _reach,
                          f64 _keep, f64* _out_dx, f64* _out_dy, f64* _line_x, f64* _line_y) {
    const f64 _was_x = *_line_x, _was_y = *_line_y;
    f64 _bx = _reach, _by = _reach, _px = 0.0, _py = 0.0;
    *_line_x = NAN;
    *_line_y = NAN;
    for(u32 _i = 0; _i < _nm; _i++) {
        const f64 _mx = _movers[_i].x + _dx, _my = _movers[_i].y + _dy;
        // (what it lines up with already: held a little further)
        if(!isnan(_was_x) && fabs(_was_x - _mx) < fmax(_keep, _bx)) {
            _bx = fabs(_was_x - _mx);
            _px = _was_x - _mx;
            *_line_x = _was_x;
        }
        if(!isnan(_was_y) && fabs(_was_y - _my) < fmax(_keep, _by)) {
            _by = fabs(_was_y - _my);
            _py = _was_y - _my;
            *_line_y = _was_y;
        }
        for(u32 _j = 0; _j < _nt; _j++) {
            const f64 _ex = _targets[_j].x - _mx, _ey = _targets[_j].y - _my;
            if(fabs(_ex) < _bx) {
                _bx = fabs(_ex);
                _px = _ex;
                *_line_x = _targets[_j].x;
            }
            if(fabs(_ey) < _by) {
                _by = fabs(_ey);
                _py = _ey;
                *_line_y = _targets[_j].y;
            }
        }
    }
    *_out_dx = _dx + _px;
    *_out_dy = _dy + _py;
}

f64 fude_zoom_snap_grid(f64 _v, f64 _origin, f64 _step) {
    if(!(_step > 0.0)) {
        return _v;
    }
    return _origin + round((_v - _origin) / _step) * _step;
}
