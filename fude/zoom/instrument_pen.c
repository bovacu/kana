// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/instrument_parts.h"

// ===========================================================================
// See instrument.h. The instruments, the pen: drawing along an edge (the line exactly on it, never past its
// ends), and chained from edge to edge where they cross (instrument.h's CHAINED).
//
// In order: the point of an edge nearest the pen; an edge's crossings with the others (an edge as a line of points,
// where two segments cross, the nearest point of an edge, how far round one goes, the crossings found); then the pen
// (the turn told once, down, moved, along its edge, chained at a crossing, up) and what is told of the line being drawn
// (how far along, round, traced, straight, which way is out). How deep a chain has called itself is a variable of the
// module (fude_zoom_instruments_chain_depth).
//
// Part of the instruments: instrument_parts.h holds what the modules share and says which module holds what. Names:
// fude_zoom_instrument_ for one instrument (a slot), fude_zoom_instruments_ for what works on them all or on what the
// hands and the pen do. Main thread only.
// ===========================================================================

// --- the pen --------------------------------------------------------------------------------

// Where on edge _e the point nearest _p is: its distance along the edge (an
// angle on an arc), and how far _p is from it.
fude_zoom_v2 fude_zoom_instrument_on_edge(const fude_zoom_instrument_edge* _e, fude_zoom_v2 _p, f64* _along, f64* _off) {
    if(_e->arc && _e->span >= 2.0 * RDE_PI - 1e-9) {
        // All the way round (the compass): anywhere on it, from where its pencil is.
        f64 _t = rde_math_atan2_f64(_p.y - _e->centre.y, _p.x - _e->centre.x) - _e->from;
        _t = rde_math_fmod_f64(_t + 4.0 * RDE_PI, 2.0 * RDE_PI);
        const fude_zoom_v2 _q = { _e->centre.x + rde_math_cos_f64(_e->from + _t) * _e->radius, _e->centre.y + rde_math_sin_f64(_e->from + _t) * _e->radius };
        *_along = _t;
        *_off   = rde_math_hypot_f64(_p.x - _q.x, _p.y - _q.y);
        return _q;
    }
    if(_e->arc) {
        // Part of the way round (the protractor's half, a corner gauge's quarter): past either end, the nearer.
        const f64 _span = _e->span > 0.0 ? _e->span : RDE_PI;
        f64 _t = fude_zoom_instrument_wrap(rde_math_atan2_f64(_p.y - _e->centre.y, _p.x - _e->centre.x) - _e->from);
        if(_t < 0.0 || _t > _span) {
            _t = rde_math_abs_f64(fude_zoom_instrument_wrap(_t)) <= rde_math_abs_f64(fude_zoom_instrument_wrap(_t - _span)) ? 0.0 : _span;
        }
        const fude_zoom_v2 _q = { _e->centre.x + rde_math_cos_f64(_e->from + _t) * _e->radius, _e->centre.y + rde_math_sin_f64(_e->from + _t) * _e->radius };
        *_along = _t;
        *_off   = rde_math_hypot_f64(_p.x - _q.x, _p.y - _q.y);
        return _q;
    }
    const fude_zoom_v2 _d   = { _e->b.x - _e->a.x, _e->b.y - _e->a.y };
    const f64          _len = rde_math_hypot_f64(_d.x, _d.y);
    const fude_zoom_v2 _dir = fude_zoom_v2_norm(_d);
    const f64          _t   = rde_math_min_f64(rde_math_max_f64(fude_zoom_v2_dot((fude_zoom_v2){ _p.x - _e->a.x, _p.y - _e->a.y }, _dir), 0.0), _len);
    const fude_zoom_v2 _q   = { _e->a.x + _dir.x * _t, _e->a.y + _dir.y * _t };
    *_along = _t;
    *_off   = rde_math_hypot_f64(_p.x - _q.x, _p.y - _q.y);
    return _q;
}

// --- chained: one edge's crossings with the others ------------------------------------------

// Edge _j of _slot as a line of points on the screen (a straight one its ends; an arc or a
// curve many, closed ones closed). How many.
RDE_INTERNAL u32 fude_zoom_instrument_edge_line(const fude_zoom_instruments* _ins, u32 _slot, u32 _j, fude_zoom_v2* _out, u32 _max) {
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    const u32 _n = fude_zoom_instrument_edges(_ins, _slot, _e);
    if(_j >= _n || _max < 2u) {
        return 0;
    }
    const fude_zoom_instrument_edge* _ed = &_e[_j];
    if(_ed->curve) {
        const u32 _m = fude_zoom_instrument_curve_of(_ins, _slot, _ed, _out, _max - 1u);
        if(_m > 0 && !_ed->open) {
            _out[_m] = _out[0];   // (a loop closed: its last piece back to its first point)
            return _m + 1u;
        }
        return _m;
    }
    if(_ed->arc) {
        const u32 _sides = _max - 1u < 128u ? _max - 1u : 128u;
        for(u32 _i = 0; _i <= _sides; _i++) {
            const f64 _t = _ed->from + _ed->span * (f64)_i / (f64)_sides;
            _out[_i] = (fude_zoom_v2){ _ed->centre.x + rde_math_cos_f64(_t) * _ed->radius, _ed->centre.y + rde_math_sin_f64(_t) * _ed->radius };
        }
        return _sides + 1u;
    }
    _out[0] = _ed->a;
    _out[1] = _ed->b;
    return 2u;
}

// Where segments a-b and c-d cross (true), into _out: up to half a point past either's ends (instruments
// lying flush: one's edge through another's corner, a hair either side of it); lying along each other, none.
RDE_INTERNAL b8 fude_zoom_instrument_segments_cross(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_v2 _c, fude_zoom_v2 _d, fude_zoom_v2* _out) {
    const fude_zoom_v2 _r = { _b.x - _a.x, _b.y - _a.y }, _q = { _d.x - _c.x, _d.y - _c.y };
    const f64 _den = _r.x * _q.y - _r.y * _q.x;
    const f64 _lr = rde_math_hypot_f64(_r.x, _r.y), _lq = rde_math_hypot_f64(_q.x, _q.y);
    if(rde_math_abs_f64(_den) <= 1e-9 * _lr * _lq || !(_lr > 1e-12) || !(_lq > 1e-12)) {
        return false;
    }
    const fude_zoom_v2 _ca = { _c.x - _a.x, _c.y - _a.y };
    const f64 _t = (_ca.x * _q.y - _ca.y * _q.x) / _den, _u = (_ca.x * _r.y - _ca.y * _r.x) / _den;
    const f64 _st = 0.5 / _lr, _su = 0.5 / _lq;
    if(_t < -_st || _t > 1.0 + _st || _u < -_su || _u > 1.0 + _su) {
        return false;
    }
    *_out = (fude_zoom_v2){ _a.x + _r.x * _t, _a.y + _r.y * _t };
    return true;
}

// The point of edge _j of _slot nearest _p, and how far along it (its own measure: a
// straight one's distance from a, an arc's angle, a curve's length round it).
RDE_INTERNAL fude_zoom_v2 fude_zoom_instrument_edge_nearest(const fude_zoom_instruments* _ins, u32 _slot, u32 _j, fude_zoom_v2 _p, f64* _along, f64* _off) {
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, _slot, _e);
    if(_e[_j].curve) {
        fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
        f64 _round;
        const u32 _m = fude_zoom_instrument_curve_of(_ins, _slot, &_e[_j], _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
        if(_m == 0u) {
            *_along = 0.0;
            *_off   = 1e300;
            return _p;
        }
        return fude_zoom_instrument_on_curve(_c, _m, _p, 0.0, 0.0, _along, _off, &_round, _e[_j].open);
    }
    return fude_zoom_instrument_on_edge(&_e[_j], _p, _along, _off);
}

// How far round the edge once (2π, a curve's length; 0: it does not go round).
RDE_INTERNAL f64 fude_zoom_instrument_edge_round(const fude_zoom_instruments* _ins, u32 _slot, u32 _j) {
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, _slot, _e);
    if(_e[_j].curve) {
        if(_e[_j].open) {
            return 0.0;   // (a line with ends: never round and round)
        }
        fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
        const u32 _m = fude_zoom_instrument_curve_of(_ins, _slot, &_e[_j], _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
        return fude_zoom_instrument_curve_length(_c, _m, false);
    }
    return _e[_j].arc && _e[_j].span >= 2.0 * RDE_PI - 1e-9 ? 2.0 * RDE_PI : 0.0;
}

// The edge drawn along now: its crossings with every other edge shown (another
// instrument's, this one's others), kept (the instruments do not move while it draws).
RDE_INTERNAL void fude_zoom_instruments_find_crosses(fude_zoom_instruments* _ins) {
    _ins->cross_n = 0;
    if(_ins->ruled < 0) {
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr TYPE(fude_zoom_v2) _mine_arr  = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr TYPE(fude_zoom_v2) _other_arr = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    fude_zoom_v2* _mine  = (fude_zoom_v2*)rde_arr_add_n(&_mine_arr, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS + 2u);   // (sized once: they stay put)
    fude_zoom_v2* _other = (fude_zoom_v2*)rde_arr_add_n(&_other_arr, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS + 2u);
    const u32 _nm = fude_zoom_instrument_edge_line(_ins, (u32)_ins->ruled, _ins->ruled_edge, _mine, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS + 2u);
    for(u32 _k = 0; _k < FUDE_ZOOM_INSTRUMENT_MOST; _k++) {
        if(!_ins->tools[_k].shown) {
            continue;
        }
        fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
        const u32 _ne = fude_zoom_instrument_edges(_ins, _k, _e);
        for(u32 _j = 0; _j < _ne; _j++) {
            if((i32)_k == _ins->ruled && _j == _ins->ruled_edge) {
                continue;
            }
            const u32 _no = fude_zoom_instrument_edge_line(_ins, _k, _j, _other, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS + 2u);
            for(u32 _a = 0; _a + 1u < _nm; _a++) {
                for(u32 _b = 0; _b + 1u < _no; _b++) {
                    fude_zoom_v2 _x;
                    if(!fude_zoom_instrument_segments_cross(_mine[_a], _mine[_a + 1u], _other[_b], _other[_b + 1u], &_x)) {
                        continue;
                    }
                    // (the same edge's crossing found twice, at a shared end of its pieces: once — another
                    // edge through the same point is kept: one lying flush runs through this one's corner)
                    b8 _again = false;
                    for(u32 _i = 0; _i < _ins->cross_n && !_again; _i++) {
                        _again = _ins->cross_slot[_i] == _k && _ins->cross_edge[_i] == _j &&
                                 rde_math_hypot_f64(_ins->cross_at[_i].x - _x.x, _ins->cross_at[_i].y - _x.y) < 0.5;
                    }
                    if(_again || _ins->cross_n >= FUDE_ZOOM_INSTRUMENT_CROSSES) {
                        continue;
                    }
                    f64 _along, _off;
                    _x = fude_zoom_instrument_edge_nearest(_ins, (u32)_ins->ruled, _ins->ruled_edge, _x, &_along, &_off);
                    _ins->cross_at[_ins->cross_n]    = _x;
                    _ins->cross_along[_ins->cross_n] = _along;
                    _ins->cross_slot[_ins->cross_n]  = _k;
                    _ins->cross_edge[_ins->cross_n]  = _j;
                    _ins->cross_n++;
                }
            }
        }
    }
    rde_arr_free(&_mine_arr);
    rde_arr_free(&_other_arr);
}

// See the header. The turn is said once: taking it clears it.
b8 fude_zoom_instruments_ruled_turn(fude_zoom_instruments* _ins, fude_zoom_v2* _corner, fude_zoom_v2* _out_before) {
    if(!_ins->ruled_turned) {
        return false;
    }
    _ins->ruled_turned = false;
    *_corner     = _ins->ruled_corner;
    *_out_before = _ins->ruled_out_before;
    return true;
}

// Declared ahead: the pen's moves use them, and each calls the other.
RDE_INTERNAL fude_zoom_v2 fude_zoom_instruments_follow_edge(fude_zoom_instruments* _ins, fude_zoom_v2 _at);
RDE_INTERNAL fude_zoom_v2 fude_zoom_instruments_chain(fude_zoom_instruments* _ins, fude_zoom_v2 _at, f64 _before, fude_zoom_v2 _q);

// See the header. In order: a × or a size tab (with none held); a knob, the top one first; a body, the top one (more
// than a hair in from its edges; a thin one from a little round it too); an edge within FUDE_ZOOM_INSTRUMENT_SNAP,
// the nearest of the top one that has one, its crossings found; else the pen taken as a finger.
FUDE_ZOOM_INSTRUMENT_PEN_ fude_zoom_instruments_pen_down(fude_zoom_instruments* _ins, fude_zoom_v2 _at, fude_zoom_v2* _snapped) {
    _ins->ruled = -1;
    // A × under the pen first: lifted there, that one is put away.
    if(_ins->held < 0 && _ins->closing_hand == 0u) {
        const i32 _x = fude_zoom_instruments_close_under(_ins, _at);
        if(_x >= 0) {
            _ins->closing      = _x;
            _ins->closing_hand = FUDE_ZOOM_INSTRUMENT_PEN_HAND;
            _ins->closing_at   = _at;
            return FUDE_ZOOM_INSTRUMENT_PEN_HELD;
        }
        const i32 _g = fude_zoom_instruments_grow_under(_ins, _at);
        if(_g >= 0) {
            fude_zoom_instruments_take_grow(_ins, (u32)_g, FUDE_ZOOM_INSTRUMENT_PEN_HAND, _at);
            return FUDE_ZOOM_INSTRUMENT_PEN_HELD;
        }
    }
    // The knob of the one on top under the pen, then its edges, then its body.
    for(u32 _i = FUDE_ZOOM_INSTRUMENT_MOST; _i-- > 0;) {
        const u32 _k = _ins->order[_i];
        if(!_ins->tools[_k].shown) {
            continue;
        }
        const fude_zoom_v2 _knob = fude_zoom_instrument_knob(_ins, _k);
        if(rde_math_hypot_f64(_at.x - _knob.x, _at.y - _knob.y) <= FUDE_ZOOM_INSTRUMENT_KNOB * fude_zoom_instrument_icons(&_ins->tools[_k]) + 6.0 && _ins->held < 0) {
            _ins->held       = (i32)_k;
            _ins->hands      = 1u;
            _ins->hand_id[0] = FUDE_ZOOM_INSTRUMENT_PEN_HAND;
            _ins->hand_at[0] = _at;
            _ins->turning    = true;
            fude_zoom_instrument_raise(_ins, _k);
            fude_zoom_instruments_regrab(_ins);
            return FUDE_ZOOM_INSTRUMENT_PEN_HELD;
        }
    }
    // On a body (the one on top): it is held — the pen draws along an edge from outside it, as a
    // pencil against a ruler does (from a hair inside the edge too). A thin one (a true-size ruler
    // zoomed out is a narrow band) is held from a little round it as well, so there is always
    // FUDE_ZOOM_INSTRUMENT_GRIP of it to take; drawing along it begins past that. Not the compass:
    // its circle runs through its own pencil.
    for(u32 _i = FUDE_ZOOM_INSTRUMENT_MOST; _i-- > 0 && _ins->held < 0;) {
        const u32 _k = _ins->order[_i];
        if(!_ins->tools[_k].shown || _ins->tools[_k].kind == FUDE_ZOOM_INSTRUMENT_COMPASS) {
            continue;
        }
        const b8  _in   = fude_zoom_instrument_inside(_ins, _k, _at);
        const f64 _thin = fude_zoom_instrument_thin(_ins, _k);
        const f64 _off  = fude_zoom_instrument_edge_off(_ins, _k, _at);
        // (On it, but a hair from another's edge — a ruler lying across another, the pen at the one
        // under it: that edge.)
        f64 _other = 1e300;
        for(u32 _o = 0; _in && _o < FUDE_ZOOM_INSTRUMENT_MOST; _o++) {
            if(_o != _k && _ins->tools[_o].shown) {
                _other = rde_math_min_f64(_other, fude_zoom_instrument_edge_off(_ins, _o, _at));
            }
        }
        if((_in && _off > rde_math_min_f64(3.0, _thin * 0.25) && _other > 6.0) || (!_in && _off <= rde_math_max_f64(0.0, FUDE_ZOOM_INSTRUMENT_GRIP - _thin))) {
            fude_zoom_instruments_hold(_ins, FUDE_ZOOM_INSTRUMENT_PEN_HAND, _at, _k);
            return FUDE_ZOOM_INSTRUMENT_PEN_HELD;
        }
        if(_in) {
            break;   // (on the one on top, by its edge: it draws, not one under it)
        }
    }
    f64 _best = FUDE_ZOOM_INSTRUMENT_SNAP;
    for(u32 _i = FUDE_ZOOM_INSTRUMENT_MOST; _i-- > 0;) {
        const u32 _k = _ins->order[_i];
        if(!_ins->tools[_k].shown) {
            continue;
        }
        fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
        const u32 _n = fude_zoom_instrument_edges(_ins, _k, _e);
        for(u32 _j = 0; _j < _n; _j++) {
            f64 _along, _off;
            fude_zoom_v2 _q;
            if(_e[_j].curve) {
                fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
                f64 _round;
                const u32 _m = fude_zoom_instrument_curve_of(_ins, _k, &_e[_j], _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
                if(_m == 0u) {
                    continue;
                }
                _q = fude_zoom_instrument_on_curve(_c, _m, _at, 0.0, 0.0, &_along, &_off, &_round, _e[_j].open);
            } else {
                _q = fude_zoom_instrument_on_edge(&_e[_j], _at, &_along, &_off);
            }
            if(_off < _best) {
                _best             = _off;
                _ins->ruled       = (i32)_k;
                _ins->ruled_edge  = _j;
                _ins->ruled_from  = _along;
                _ins->ruled_now   = _along;
                *_snapped         = _q;
            }
        }
        if(_ins->ruled >= 0) {
            _ins->ruled_start  = *_snapped;
            _ins->ruled_cornered = false;
            _ins->ruled_held   = -1;
            _ins->ruled_turned = false;
            fude_zoom_instruments_find_crosses(_ins);
            return FUDE_ZOOM_INSTRUMENT_PEN_RULED;   // the one on top wins
        }
    }
    if(_ins->held < 0 && fude_zoom_instruments_finger_down(_ins, FUDE_ZOOM_INSTRUMENT_PEN_HAND, _at)) {
        return FUDE_ZOOM_INSTRUMENT_PEN_HELD;
    }
    return FUDE_ZOOM_INSTRUMENT_PEN_NONE;
}

// See the header. Drawing along an edge: the point on it, chained at its crossings. Holding one: it follows.
fude_zoom_v2 fude_zoom_instruments_pen_moved(fude_zoom_instruments* _ins, fude_zoom_v2 _at) {
    if(fude_zoom_instruments_closing_moved(_ins, FUDE_ZOOM_INSTRUMENT_PEN_HAND, _at)) {
        return _at;
    }
    if(_ins->ruled >= 0) {
        const f64          _before = _ins->ruled_now;
        const fude_zoom_v2 _q      = fude_zoom_instruments_follow_edge(_ins, _at);
        return fude_zoom_instruments_chain(_ins, _at, _before, _q);
    }
    if(_ins->held >= 0 && _ins->hand_id[0] == FUDE_ZOOM_INSTRUMENT_PEN_HAND) {
        _ins->hand_at[0] = _at;
        fude_zoom_instruments_follow(_ins);
    }
    return _at;
}

// The pen along the edge it draws along: the point of it nearest the pen, going on from where it
// was (round and round counted on), ruled_now moved there.
RDE_INTERNAL fude_zoom_v2 fude_zoom_instruments_follow_edge(fude_zoom_instruments* _ins, fude_zoom_v2 _at) {
    {
        fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
        fude_zoom_instrument_edges(_ins, (u32)_ins->ruled, _e);
        f64 _along, _off;
        if(_e[_ins->ruled_edge].curve) {
            // Along the curve from where the pen is on it (a quarter of the way round at most: never across it);
            // round and round, counted on.
            fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
            const b8  _open  = _e[_ins->ruled_edge].open;
            const u32 _m     = fude_zoom_instrument_curve_of(_ins, (u32)_ins->ruled, &_e[_ins->ruled_edge], _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
            if(_m == 0u) {
                return _at;
            }
            f64       _round = fude_zoom_instrument_curve_length(_c, _m, _open);
            const fude_zoom_v2 _r = fude_zoom_instrument_on_curve(_c, _m, _at, _ins->ruled_now, _open ? rde_math_max_f64(_round * 0.25, 80.0) : _round * 0.25, &_along, &_off, &_round, _open);
            while(!_open && _round > 0.0 && _along - _ins->ruled_now > _round * 0.5) { _along -= _round; }
            while(!_open && _round > 0.0 && _ins->ruled_now - _along > _round * 0.5) { _along += _round; }
            _ins->ruled_now = _along;
            return _r;
        }
        const fude_zoom_instrument_edge* _ed = &_e[_ins->ruled_edge];
        if(_ed->arc && _ed->span < 2.0 * RDE_PI - 1e-9 && rde_math_hypot_f64(_at.x - _ed->centre.x, _at.y - _ed->centre.y) < _ed->radius * 0.6) {
            // Part of the way round (a corner gauge's, the protractor's), the pen well in over the plate: where it
            // points from there swings, and would take the line back to an end at once — it stays where it was.
            const f64 _t = rde_math_min_f64(rde_math_max_f64(_ins->ruled_now, 0.0), _ed->span > 0.0 ? _ed->span : RDE_PI);
            return (fude_zoom_v2){ _ed->centre.x + rde_math_cos_f64(_ed->from + _t) * _ed->radius, _ed->centre.y + rde_math_sin_f64(_ed->from + _t) * _ed->radius };
        }
        const fude_zoom_v2 _q = fude_zoom_instrument_on_edge(_ed, _at, &_along, &_off);
        if(_e[_ins->ruled_edge].span >= 2.0 * RDE_PI - 1e-9) {
            // Round and round: the turn counted on from where it was (past a whole turn, more than 360°).
            while(_along - _ins->ruled_now > RDE_PI)  { _along -= 2.0 * RDE_PI; }
            while(_ins->ruled_now - _along > RDE_PI)  { _along += 2.0 * RDE_PI; }
        }
        _ins->ruled_now = _along;
        return _q;
    }
}

// How deep fude_zoom_instruments_chain has called itself (a corner turned onto an edge with another corner on it):
// held to four, so a pen move turns a few corners at most.
RDE_INTERNAL u32 fude_zoom_instruments_chain_depth = 0;

// Chained (instrument.h): the pen gone past a crossing of this edge with another — nearer
// that one, the line turns onto it at the crossing; nearer this one, it stays at the crossing
// when going on would be into the other instrument. _before: how far along it was. Returns
// where the line goes now.
RDE_INTERNAL fude_zoom_v2 fude_zoom_instruments_chain(fude_zoom_instruments* _ins, fude_zoom_v2 _at, f64 _before, fude_zoom_v2 _q) {
    if(_ins->cross_n == 0) {
        return _q;
    }
    const f64 _round = fude_zoom_instrument_edge_round(_ins, (u32)_ins->ruled, _ins->ruled_edge);
    const f64 _now   = _ins->ruled_now;
    // Turned onto this edge at a corner: not on from it into the one it left (along this edge
    // that way, beside it, is inside that one's body) — held at the corner till the pen goes
    // the other way.
    for(u32 _i = 0; _ins->ruled_cornered && _i < _ins->cross_n; _i++) {
        if(rde_math_hypot_f64(_ins->cross_at[_i].x - _ins->ruled_start.x, _ins->cross_at[_i].y - _ins->ruled_start.y) >= 3.0 ||
           _ins->tools[_ins->cross_slot[_i]].kind == FUDE_ZOOM_INSTRUMENT_COMPASS || _ins->cross_slot[_i] == (u32)_ins->ruled) {
            continue;   // (its own next edge — a corner gauge's side into its round — is never into itself: its round's
                        // chord runs over its own plate)
        }
        const fude_zoom_v2 _c   = _ins->cross_at[_i];
        const f64          _len = rde_math_hypot_f64(_q.x - _c.x, _q.y - _c.y);
        if(!(_len > 1e-9)) {
            continue;
        }
        const fude_zoom_v2 _way   = { (_q.x - _c.x) / _len, (_q.y - _c.y) / _len };
        const fude_zoom_v2 _out   = fude_zoom_instruments_ruled_outward(_ins);
        const fude_zoom_v2 _probe = { _c.x + _way.x * 3.0 + _out.x * 1.5, _c.y + _way.y * 3.0 + _out.y * 1.5 };
        if(fude_zoom_instrument_inside(_ins, _ins->cross_slot[_i], _probe)) {
            f64 _x = _ins->cross_along[_i];
            if(_round > 0.0) {
                while(_x - _now > _round * 0.5) { _x -= _round; }
                while(_now - _x > _round * 0.5) { _x += _round; }
            }
            _ins->ruled_now = _x;
            return _c;
        }
    }
    i32 _pick = -1;
    f64 _pick_d = 1e300, _pick_along = 0.0;
    for(u32 _i = 0; _i < _ins->cross_n; _i++) {
        if(rde_math_hypot_f64(_ins->cross_at[_i].x - _ins->ruled_start.x, _ins->cross_at[_i].y - _ins->ruled_start.y) < 3.0) {
            continue;   // (where the line began on this edge: not turned at)
        }
        f64 _x = _ins->cross_along[_i];
        if(_round > 0.0) {
            while(_x - _before > _round * 0.5) { _x -= _round; }
            while(_before - _x > _round * 0.5) { _x += _round; }
        }
        // Gone past it since the last move (or held at it, the pen still past it the way it came; or the line at it
        // still — at the end of this edge, the pen past it: weighed again each move, else it stayed there for good).
        const b8 _past = (_i == (u32)_ins->ruled_held && (_now - _x) * _ins->ruled_hold_way > 0.0) ||
                         (_now > _before ? (_x > _before + 1e-9 && _x <= _now) : (_x < _before - 1e-9 && _x >= _now)) ||
                         rde_math_hypot_f64(_ins->cross_at[_i].x - _q.x, _ins->cross_at[_i].y - _q.y) < 0.75;
        if(_past && rde_math_abs_f64(_x - _before) < _pick_d) {
            _pick_d     = rde_math_abs_f64(_x - _before);
            _pick       = (i32)_i;
            _pick_along = _x;
        }
    }
    if(_pick < 0) {
        _ins->ruled_held = -1;
        return _q;
    }
    // Several edges crossing it there at once (three set squares meeting at a point): of those, the one the
    // pen goes along — the nearest it — not merely the first met (found on the tablet: going round the
    // outside of three, the line turned in along one lying under the others).
    f64 _a2, _off2;
    fude_zoom_instrument_edge_nearest(_ins, _ins->cross_slot[_pick], _ins->cross_edge[_pick], _at, &_a2, &_off2);
    for(u32 _i = 0; _i < _ins->cross_n; _i++) {
        if((i32)_i == _pick || rde_math_hypot_f64(_ins->cross_at[_i].x - _ins->cross_at[_pick].x, _ins->cross_at[_i].y - _ins->cross_at[_pick].y) > 4.0 ||
           rde_math_hypot_f64(_ins->cross_at[_i].x - _ins->ruled_start.x, _ins->cross_at[_i].y - _ins->ruled_start.y) < 3.0) {
            continue;
        }
        f64 _ai, _offi;
        fude_zoom_instrument_edge_nearest(_ins, _ins->cross_slot[_i], _ins->cross_edge[_i], _at, &_ai, &_offi);
        if(_offi < _off2) {
            _off2 = _offi;
            _pick = (i32)_i;   // (its along: the nearest's, a hair from it)
        }
    }
    const fude_zoom_v2 _x = _ins->cross_at[_pick];
    // Which edge the pen is nearer now: this one (past the crossing), or the one it crosses.
    const f64 _off1 = rde_math_hypot_f64(_at.x - _q.x, _at.y - _q.y);
    if(_off2 < _off1) {
        // Turned: the corner at the crossing; the other edge drawn along from it.
        _ins->ruled_now        = _pick_along;
        _ins->ruled_out_before = fude_zoom_instruments_ruled_outward(_ins);
        _ins->ruled_corner     = _x;
        _ins->ruled_turned     = true;
        _ins->ruled            = (i32)_ins->cross_slot[_pick];
        _ins->ruled_edge       = _ins->cross_edge[_pick];
        f64 _ax, _offx;
        fude_zoom_instrument_edge_nearest(_ins, (u32)_ins->ruled, _ins->ruled_edge, _x, &_ax, &_offx);
        _ins->ruled_from  = _ax;
        _ins->ruled_now   = _ax;
        _ins->ruled_start    = _x;
        _ins->ruled_cornered = true;
        _ins->ruled_held     = -1;
        fude_zoom_instruments_find_crosses(_ins);
        const fude_zoom_v2 _q2 = fude_zoom_instruments_follow_edge(_ins, _at);
        if(fude_zoom_instruments_chain_depth >= 4u) {
            return _q2;
        }
        fude_zoom_instruments_chain_depth++;   // (from the corner on, as any move: a few corners at most at once)
        const fude_zoom_v2 _r2 = fude_zoom_instruments_chain(_ins, _at, _ax, _q2);
        fude_zoom_instruments_chain_depth--;
        return _r2;
    }
    // Nearer this one: as with real ones, the line cannot go on into the instrument it meets
    // (past the crossing, beside this edge, is inside the other's body: held at the crossing
    // till the pen turns, or goes back); where nothing is in the way (a compass's circle,
    // leaving one the line began under) it goes on.
    const u32 _other = _ins->cross_slot[_pick];
    b8 _blocked = false;
    if(_ins->tools[_other].kind != FUDE_ZOOM_INSTRUMENT_COMPASS) {
        const f64 _len = rde_math_hypot_f64(_q.x - _x.x, _q.y - _x.y);
        const fude_zoom_v2 _way = _len > 1e-9 ? (fude_zoom_v2){ (_q.x - _x.x) / _len, (_q.y - _x.y) / _len } : (fude_zoom_v2){ 0.0, 0.0 };
        const fude_zoom_v2 _out = fude_zoom_instruments_ruled_outward(_ins);
        const fude_zoom_v2 _probe = { _x.x + _way.x * 3.0 + _out.x * 1.5, _x.y + _way.y * 3.0 + _out.y * 1.5 };
        _blocked = _len > 1e-9 && fude_zoom_instrument_inside(_ins, _other, _probe);
    }
    if(_blocked) {
        if(_ins->ruled_held != _pick) {
            _ins->ruled_hold_way = _now > _pick_along ? 1.0 : -1.0;   // (the way it came: back the other way, let go)
        }
        _ins->ruled_held = _pick;
        _ins->ruled_now  = _pick_along;
        return _x;
    }
    _ins->ruled_held = -1;
    return _q;
}

// See the header. A × pressed by the pen is put away if the pen is still on it.
void fude_zoom_instruments_pen_up(fude_zoom_instruments* _ins) {
    _ins->ruled = -1;
    fude_zoom_instruments_closing_up(_ins, FUDE_ZOOM_INSTRUMENT_PEN_HAND);
    if(_ins->held >= 0 && _ins->hand_id[0] == FUDE_ZOOM_INSTRUMENT_PEN_HAND) {
        _ins->held    = -1;
        _ins->hands   = 0u;
        _ins->turning = false;
        _ins->growing = false;
        _ins->docked  = -1;
    }
}

// See the header. The label goes 30 points out from the edge beside the pen (a hole's: inside it).
b8 fude_zoom_instruments_ruled(const fude_zoom_instruments* _ins, f64* _along, b8* _degrees, fude_zoom_v2* _label_at) {
    if(_ins->ruled < 0) {
        return false;
    }
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, (u32)_ins->ruled, _e);
    const fude_zoom_instrument_edge* _ed = &_e[_ins->ruled_edge];
    if(_ed->curve) {
        // How far along the curve; said beside the pen, outside the instrument.
        fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS], _dir;
        const u32 _m = fude_zoom_instrument_curve_of(_ins, (u32)_ins->ruled, _ed, _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
        if(_m == 0u) {
            return false;
        }
        const fude_zoom_v2 _q = fude_zoom_instrument_curve_at(_c, _m, _ins->ruled_now, &_dir, _ed->open);
        const f64 _side = _ed->hole ? -1.0 : 1.0;
        *_degrees  = false;
        *_along    = rde_math_abs_f64(_ins->ruled_now - _ins->ruled_from);
        *_label_at = (fude_zoom_v2){ _q.x + _dir.y * 30.0 * _side, _q.y - _dir.x * 30.0 * _side };
        return true;
    }
    *_degrees = _ed->arc;
    *_along   = _ed->arc ? rde_math_abs_f64(_ins->ruled_now - _ins->ruled_from) * 180.0 / RDE_PI : rde_math_abs_f64(_ins->ruled_now - _ins->ruled_from);
    // Beside the pen, outside the edge.
    if(_ed->arc) {
        const f64 _t = _ed->from + _ins->ruled_now;
        *_label_at = (fude_zoom_v2){ _ed->centre.x + rde_math_cos_f64(_t) * (_ed->radius + 30.0), _ed->centre.y + rde_math_sin_f64(_t) * (_ed->radius + 30.0) };
    } else {
        const fude_zoom_v2 _dir = fude_zoom_v2_norm((fude_zoom_v2){ _ed->b.x - _ed->a.x, _ed->b.y - _ed->a.y });
        *_label_at = (fude_zoom_v2){ _ed->a.x + _dir.x * _ins->ruled_now + _dir.y * 30.0, _ed->a.y + _dir.y * _ins->ruled_now - _dir.x * 30.0 };
    }
    return true;
}

// See the header.
b8 fude_zoom_instruments_ruled_round(const fude_zoom_instruments* _ins) {
    if(_ins->ruled < 0) {
        return false;
    }
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, (u32)_ins->ruled, _e);
    return _e[_ins->ruled_edge].arc && !_e[_ins->ruled_edge].curve;
}

// See the header.
b8 fude_zoom_instruments_ruled_traced(const fude_zoom_instruments* _ins) {
    if(_ins->ruled < 0) {
        return false;
    }
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, (u32)_ins->ruled, _e);
    return _e[_ins->ruled_edge].trace;
}

// See the header.
b8 fude_zoom_instruments_ruled_straight(const fude_zoom_instruments* _ins) {
    if(_ins->ruled < 0) {
        return false;
    }
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, (u32)_ins->ruled, _e);
    return !_e[_ins->ruled_edge].arc && !_e[_ins->ruled_edge].curve;
}

// See the header. A compass's circle has no outside (0, 0): the line is drawn on it.
fude_zoom_v2 fude_zoom_instruments_ruled_outward(const fude_zoom_instruments* _ins) {
    if(_ins->ruled < 0) {
        return (fude_zoom_v2){ 0.0, 0.0 };
    }
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, (u32)_ins->ruled, _e);
    const fude_zoom_instrument_edge* _ed = &_e[_ins->ruled_edge];
    if(_ed->curve) {
        // Round it counter-clockwise, its outside on the right; a hole's inside, on the left.
        fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS], _dir;
        const u32 _m = fude_zoom_instrument_curve_of(_ins, (u32)_ins->ruled, _ed, _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
        if(_m == 0u) {
            return (fude_zoom_v2){ 0.0, 0.0 };
        }
        fude_zoom_instrument_curve_at(_c, _m, _ins->ruled_now, &_dir, _ed->open);
        return _ed->hole ? (fude_zoom_v2){ -_dir.y, _dir.x } : (fude_zoom_v2){ _dir.y, -_dir.x };
    }
    if(_ed->arc) {
        if(_ins->tools[_ins->ruled].kind == FUDE_ZOOM_INSTRUMENT_CIRCLE) {
            const f64 _t = _ed->from + _ins->ruled_now;
            return (fude_zoom_v2){ -rde_math_cos_f64(_t), -rde_math_sin_f64(_t) };   // the hole is its outside: the line inside it
        }
        if(_ed->span >= 2.0 * RDE_PI - 1e-9) {
            return (fude_zoom_v2){ 0.0, 0.0 };
        }
        const f64 _t = _ed->from + _ins->ruled_now;
        return (fude_zoom_v2){ rde_math_cos_f64(_t), rde_math_sin_f64(_t) };
    }
    const fude_zoom_v2 _d = fude_zoom_v2_norm((fude_zoom_v2){ _ed->b.x - _ed->a.x, _ed->b.y - _ed->a.y });
    return (fude_zoom_v2){ _d.y, -_d.x };   // (counter-clockwise round it: its outside on the right)
}
