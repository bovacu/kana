// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/instrument_parts.h"

// ===========================================================================
// See instrument.h. The instruments, hands on one: a finger, the pen or the mouse taking one by its body, its
// knob, its × or its size tab; moved, turned (resting a moment at each 15°), made longer or shorter, docked against
// another's straight edge and sliding along it until it rests flush against the next in its way.
//
// In order: angles (brought round, the 15° detents), the hands measured from again, a vector made one long and the dot
// product; sliding (how far on it goes to rest against the next, the others it lies against, the slide); following the
// hands (fude_zoom_instruments_follow); then the fingers (down, an instrument held, its narrowest width, how near an
// edge a point is, a hand on a ×, moved, up).
//
// Part of the instruments: instrument_parts.h holds what the modules share and says which module holds what. Names:
// fude_zoom_instrument_ for one instrument (a slot), fude_zoom_instruments_ for what works on them all or on what the
// hands and the pen do. Main thread only.
// ===========================================================================

// --- hands on one ------------------------------------------------------------------------------

// Returns an angle brought into (-π, π].
f64 fude_zoom_instrument_wrap(f64 _a) {
    while(_a > RDE_PI)   { _a -= 2.0 * RDE_PI; }
    while(_a <= -RDE_PI) { _a += 2.0 * RDE_PI; }
    return _a;
}

// A turn resting a moment at each 15°.
RDE_INTERNAL f64 fude_zoom_instrument_detent(f64 _a) {
    const f64 _step = RDE_PI / 12.0;
    const f64 _near = rde_math_round_f64(_a / _step) * _step;
    return rde_math_abs_f64(_a - _near) < FUDE_ZOOM_INSTRUMENT_DETENT ? _near : _a;
}

// The hands as they are now become where the next moves are measured from.
void fude_zoom_instruments_regrab(fude_zoom_instruments* _ins) {
    if(_ins->held < 0) {
        return;
    }
    const fude_zoom_instrument* _t = &_ins->tools[_ins->held];
    _ins->grab_at    = _t->at;
    _ins->grab_angle = _t->angle;
    _ins->grab_r     = _t->radius;
    _ins->grab_size  = fude_zoom_instrument_size(_t);
    _ins->grab_hand[0] = _ins->hand_at[0];
    _ins->grab_hand[1] = _ins->hand_at[1];
}

// Returns _v made one long ((1, 0) for no length).
fude_zoom_v2 fude_zoom_v2_norm(fude_zoom_v2 _v) {
    const f64 _l = rde_math_hypot_f64(_v.x, _v.y);
    return _l > 0.0 ? (fude_zoom_v2){ _v.x / _l, _v.y / _l } : (fude_zoom_v2){ 1.0, 0.0 };
}

// Returns the dot product of _a and _b.
f64 fude_zoom_v2_dot(fude_zoom_v2 _a, fude_zoom_v2 _b) { return _a.x * _b.x + _a.y * _b.y; }

// Sliding along the edge it lies on (_dir): how far on (or back) it goes to rest flush against the
// next one in its way — an edge of its lying along another's (facing it, nearly parallel), a corner
// of its on another's edge, or another's corner on an edge of its — within a dock's reach of it now
// (0: none near). The held one, as it is now.
RDE_INTERNAL f64 fude_zoom_instruments_against(const fude_zoom_instruments* _ins, fude_zoom_v2 _dir) {
    fude_zoom_instrument_edge _ea[FUDE_ZOOM_INSTRUMENT_EDGES];
    const u32 _na = fude_zoom_instrument_edges(_ins, (u32)_ins->held, _ea);
    f64 _best = 0.0, _best_d = FUDE_ZOOM_INSTRUMENT_DOCK;
    for(u32 _k = 0; _k < FUDE_ZOOM_INSTRUMENT_MOST; _k++) {
        if((i32)_k == _ins->held || (i32)_k == _ins->docked || !_ins->tools[_k].shown || _ins->tools[_k].kind == FUDE_ZOOM_INSTRUMENT_COMPASS) {
            continue;
        }
        fude_zoom_instrument_edge _eb[FUDE_ZOOM_INSTRUMENT_EDGES];
        const u32 _nb = fude_zoom_instrument_edges(_ins, _k, _eb);
        for(u32 _j = 0; _j < _nb; _j++) {
            if(_eb[_j].arc || _eb[_j].curve) {
                continue;
            }
            const fude_zoom_v2 _db  = fude_zoom_v2_norm((fude_zoom_v2){ _eb[_j].b.x - _eb[_j].a.x, _eb[_j].b.y - _eb[_j].a.y });
            const fude_zoom_v2 _out = { _db.y, -_db.x };   // (away from its body)
            const f64 _rate = fude_zoom_v2_dot(_dir, _out);   // how the gap to its line changes, a step along
            const f64 _blen = rde_math_hypot_f64(_eb[_j].b.x - _eb[_j].a.x, _eb[_j].b.y - _eb[_j].a.y);
            for(u32 _i = 0; _i < _na; _i++) {
                if(_ea[_i].arc || _ea[_i].curve) {
                    continue;
                }
                // A corner of ours (each edge's start) on its edge, from outside it.
                const f64 _gap = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_i].a.x - _eb[_j].a.x, _ea[_i].a.y - _eb[_j].a.y }, _out);
                const f64 _at  = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_i].a.x - _eb[_j].a.x, _ea[_i].a.y - _eb[_j].a.y }, _db);
                if(rde_math_abs_f64(_rate) > 0.15 && _at >= -1.0 && _at <= _blen + 1.0 && _gap > -FUDE_ZOOM_INSTRUMENT_DOCK && _gap < FUDE_ZOOM_INSTRUMENT_DOCK) {
                    const f64 _d = -_gap / _rate;
                    if(rde_math_abs_f64(_d) < _best_d) { _best_d = rde_math_abs_f64(_d); _best = _d; }
                }
                // Its corner on an edge of ours, from outside ours.
                const fude_zoom_v2 _da   = fude_zoom_v2_norm((fude_zoom_v2){ _ea[_i].b.x - _ea[_i].a.x, _ea[_i].b.y - _ea[_i].a.y });
                const fude_zoom_v2 _oa   = { _da.y, -_da.x };
                const f64          _ra   = -fude_zoom_v2_dot(_dir, _oa);   // (we move: its corner's gap to our line changes the other way)
                const f64          _alen = rde_math_hypot_f64(_ea[_i].b.x - _ea[_i].a.x, _ea[_i].b.y - _ea[_i].a.y);
                const f64 _gap2 = fude_zoom_v2_dot((fude_zoom_v2){ _eb[_j].a.x - _ea[_i].a.x, _eb[_j].a.y - _ea[_i].a.y }, _oa);
                const f64 _at2  = fude_zoom_v2_dot((fude_zoom_v2){ _eb[_j].a.x - _ea[_i].a.x, _eb[_j].a.y - _ea[_i].a.y }, _da);
                if(rde_math_abs_f64(_ra) > 0.15 && _at2 >= -1.0 && _at2 <= _alen + 1.0 && _gap2 > -FUDE_ZOOM_INSTRUMENT_DOCK && _gap2 < FUDE_ZOOM_INSTRUMENT_DOCK) {
                    const f64 _d = -_gap2 / _ra;
                    if(rde_math_abs_f64(_d) < _best_d) { _best_d = rde_math_abs_f64(_d); _best = _d; }
                }
            }
        }
    }
    return _best;
}

// The others it lies against now (besides the one it slides along): every edge of theirs an edge or a
// corner of its is on (within a hair), into its contacts.
RDE_INTERNAL void fude_zoom_instruments_find_contacts(fude_zoom_instruments* _ins) {
    _ins->contacts = 0;
    if(_ins->held < 0) {
        return;
    }
    fude_zoom_instrument_edge _ea[FUDE_ZOOM_INSTRUMENT_EDGES];
    const u32 _na = fude_zoom_instrument_edges(_ins, (u32)_ins->held, _ea);
    for(u32 _k = 0; _k < FUDE_ZOOM_INSTRUMENT_MOST && _ins->contacts < FUDE_ZOOM_INSTRUMENT_CONTACTS; _k++) {
        if((i32)_k == _ins->held || !_ins->tools[_k].shown || _ins->tools[_k].kind == FUDE_ZOOM_INSTRUMENT_COMPASS) {
            continue;
        }
        fude_zoom_instrument_edge _eb[FUDE_ZOOM_INSTRUMENT_EDGES];
        const u32 _nb = fude_zoom_instrument_edges(_ins, _k, _eb);
        for(u32 _j = 0; _j < _nb && _ins->contacts < FUDE_ZOOM_INSTRUMENT_CONTACTS; _j++) {
            if(_eb[_j].arc || _eb[_j].curve || ((i32)_k == _ins->docked && _j == _ins->dock_edge)) {
                continue;
            }
            const fude_zoom_v2 _db  = fude_zoom_v2_norm((fude_zoom_v2){ _eb[_j].b.x - _eb[_j].a.x, _eb[_j].b.y - _eb[_j].a.y });
            const fude_zoom_v2 _out = { _db.y, -_db.x };
            const f64 _blen = rde_math_hypot_f64(_eb[_j].b.x - _eb[_j].a.x, _eb[_j].b.y - _eb[_j].a.y);
            for(u32 _i = 0; _i < _na; _i++) {
                if(_ea[_i].arc || _ea[_i].curve) {
                    continue;
                }
                const f64 _g0 = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_i].a.x - _eb[_j].a.x, _ea[_i].a.y - _eb[_j].a.y }, _out);
                const f64 _p0 = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_i].a.x - _eb[_j].a.x, _ea[_i].a.y - _eb[_j].a.y }, _db);
                if(rde_math_abs_f64(_g0) <= 0.5 && _p0 >= -0.5 && _p0 <= _blen + 0.5) {
                    _ins->contact_slot[_ins->contacts] = (i32)_k;
                    _ins->contact_edge[_ins->contacts] = _j;
                    _ins->contact_own[_ins->contacts]  = _i;
                    _ins->contacts++;
                    break;
                }
            }
        }
    }
}

// The held instrument, dragged to a place of its own: against another's
// straight edge it docks (turned to lie exactly along it, touching it from
// outside), then slides along that edge until pulled away from it — stopping
// flush against the next one in its way, so it rests against two or more at
// once. Arcs and curves (a template's hole, a French curve, a stencil's lines)
// are never docked to.
RDE_INTERNAL void fude_zoom_instruments_slide(fude_zoom_instruments* _ins, fude_zoom_v2 _want) {
    fude_zoom_instrument* _t = &_ins->tools[_ins->held];
    _ins->contacts = 0;
    if(_ins->docked >= 0) {
        fude_zoom_instrument_edge _eb[FUDE_ZOOM_INSTRUMENT_EDGES];
        fude_zoom_instrument_edges(_ins, (u32)_ins->docked, _eb);
        const fude_zoom_instrument_edge* _e   = &_eb[_ins->dock_edge];
        const fude_zoom_v2               _dir = fude_zoom_v2_norm((fude_zoom_v2){ _e->b.x - _e->a.x, _e->b.y - _e->a.y });
        const fude_zoom_v2               _out = { _dir.y, -_dir.x };
        const fude_zoom_v2               _d   = { _want.x - _ins->dock_from.x, _want.y - _ins->dock_from.y };
        if(rde_math_abs_f64(fude_zoom_v2_dot(_d, _out)) <= FUDE_ZOOM_INSTRUMENT_UNDOCK) {
            f64 _along = fude_zoom_v2_dot(_d, _dir);
            _t->at = (fude_zoom_v2){ _ins->dock_from.x + _dir.x * _along, _ins->dock_from.y + _dir.y * _along };
            // Up against the next one in its way (within a dock's reach): flush with it, resting on both.
            _along += fude_zoom_instruments_against(_ins, _dir);
            _t->at = (fude_zoom_v2){ _ins->dock_from.x + _dir.x * _along, _ins->dock_from.y + _dir.y * _along };
            fude_zoom_instruments_find_contacts(_ins);
            // Slid off the end of it: free again.
            fude_zoom_instrument_edge _ea[FUDE_ZOOM_INSTRUMENT_EDGES];
            fude_zoom_instrument_edges(_ins, (u32)_ins->held, _ea);
            const f64 _len = rde_math_hypot_f64(_e->b.x - _e->a.x, _e->b.y - _e->a.y);
            const f64 _p0  = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_ins->own_edge].a.x - _e->a.x, _ea[_ins->own_edge].a.y - _e->a.y }, _dir);
            const f64 _p1  = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_ins->own_edge].b.x - _e->a.x, _ea[_ins->own_edge].b.y - _e->a.y }, _dir);
            if(rde_math_max_f64(_p0, _p1) < 0.0 || rde_math_min_f64(_p0, _p1) > _len) {
                _ins->docked = -1;
            }
            return;
        }
        _ins->docked = -1;
    }
    _t->at = _want;
    // Near enough another's edge, and nearly parallel: docked to it.
    f64 _best = FUDE_ZOOM_INSTRUMENT_DOCK;
    i32 _to = -1;
    u32 _own = 0, _theirs = 0;
    f64 _turn = 0.0;
    fude_zoom_instrument_edge _ea[FUDE_ZOOM_INSTRUMENT_EDGES];
    const u32 _na = fude_zoom_instrument_edges(_ins, (u32)_ins->held, _ea);
    for(u32 _k = 0; _k < FUDE_ZOOM_INSTRUMENT_MOST; _k++) {
        if((i32)_k == _ins->held || !_ins->tools[_k].shown) {
            continue;
        }
        fude_zoom_instrument_edge _eb[FUDE_ZOOM_INSTRUMENT_EDGES];
        const u32 _nb = fude_zoom_instrument_edges(_ins, _k, _eb);
        for(u32 _i = 0; _i < _na; _i++) {
            if(_ea[_i].arc || _ea[_i].curve) {
                continue;   // (only straight edges dock: a curve's a and b are its ends, not a line along it)
            }
            const fude_zoom_v2 _da = fude_zoom_v2_norm((fude_zoom_v2){ _ea[_i].b.x - _ea[_i].a.x, _ea[_i].b.y - _ea[_i].a.y });
            for(u32 _j = 0; _j < _nb; _j++) {
                if(_eb[_j].arc || _eb[_j].curve) {
                    continue;
                }
                const fude_zoom_v2 _db = fude_zoom_v2_norm((fude_zoom_v2){ _eb[_j].b.x - _eb[_j].a.x, _eb[_j].b.y - _eb[_j].a.y });
                // Facing each other: going opposite ways along the same line.
                const f64 _off = fude_zoom_instrument_wrap(rde_math_atan2_f64(-_db.y, -_db.x) - rde_math_atan2_f64(_da.y, _da.x));
                if(rde_math_abs_f64(_off) > FUDE_ZOOM_INSTRUMENT_DOCK_TURN) {
                    continue;
                }
                const fude_zoom_v2 _out = { _db.y, -_db.x };
                const fude_zoom_v2 _mid = { (_ea[_i].a.x + _ea[_i].b.x) * 0.5, (_ea[_i].a.y + _ea[_i].b.y) * 0.5 };
                const f64 _gap = fude_zoom_v2_dot((fude_zoom_v2){ _mid.x - _eb[_j].a.x, _mid.y - _eb[_j].a.y }, _out);
                // Overlapping along it.
                const f64 _len = rde_math_hypot_f64(_eb[_j].b.x - _eb[_j].a.x, _eb[_j].b.y - _eb[_j].a.y);
                const f64 _p0  = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_i].a.x - _eb[_j].a.x, _ea[_i].a.y - _eb[_j].a.y }, _db);
                const f64 _p1  = fude_zoom_v2_dot((fude_zoom_v2){ _ea[_i].b.x - _eb[_j].a.x, _ea[_i].b.y - _eb[_j].a.y }, _db);
                if(rde_math_max_f64(_p0, _p1) < 8.0 || rde_math_min_f64(_p0, _p1) > _len - 8.0) {
                    continue;
                }
                if(rde_math_abs_f64(_gap) < _best) {
                    _best   = rde_math_abs_f64(_gap);
                    _to     = (i32)_k;
                    _own    = _i;
                    _theirs = _j;
                    _turn   = _off;
                }
            }
        }
    }
    if(_to < 0) {
        return;
    }
    // Turned to lie exactly along it (round the middle of its own edge), then
    // moved onto its line.
    const fude_zoom_v2 _pivot = { (_ea[_own].a.x + _ea[_own].b.x) * 0.5, (_ea[_own].a.y + _ea[_own].b.y) * 0.5 };
    const f64 _c = rde_math_cos_f64(_turn), _s = rde_math_sin_f64(_turn);
    const fude_zoom_v2 _rel = { _t->at.x - _pivot.x, _t->at.y - _pivot.y };
    _t->at    = (fude_zoom_v2){ _pivot.x + _rel.x * _c - _rel.y * _s, _pivot.y + _rel.x * _s + _rel.y * _c };
    _t->angle = fude_zoom_instrument_wrap(_t->angle + _turn);
    fude_zoom_instrument_edge _eb[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(_ins, (u32)_to, _eb);
    fude_zoom_instrument_edges(_ins, (u32)_ins->held, _ea);
    const fude_zoom_v2 _db  = fude_zoom_v2_norm((fude_zoom_v2){ _eb[_theirs].b.x - _eb[_theirs].a.x, _eb[_theirs].b.y - _eb[_theirs].a.y });
    const fude_zoom_v2 _out = { _db.y, -_db.x };
    const fude_zoom_v2 _mid = { (_ea[_own].a.x + _ea[_own].b.x) * 0.5, (_ea[_own].a.y + _ea[_own].b.y) * 0.5 };
    const f64 _gap = fude_zoom_v2_dot((fude_zoom_v2){ _mid.x - _eb[_theirs].a.x, _mid.y - _eb[_theirs].a.y }, _out);
    _t->at = (fude_zoom_v2){ _t->at.x - _out.x * _gap, _t->at.y - _out.y * _gap };
    _ins->docked    = _to;
    _ins->dock_edge = _theirs;
    _ins->own_edge  = _own;
    // Up against the next one in its way along it too, at once (resting on both).
    const f64 _more = fude_zoom_instruments_against(_ins, _db);
    _t->at = (fude_zoom_v2){ _t->at.x + _db.x * _more, _t->at.y + _db.y * _more };
    fude_zoom_instruments_find_contacts(_ins);
    _ins->dock_from = _t->at;
    // What the hands do next is measured from here (the turn just made is the instrument's, not theirs).
    _ins->grab_at    = (fude_zoom_v2){ _ins->grab_at.x + (_t->at.x - _want.x), _ins->grab_at.y + (_t->at.y - _want.y) };
    _ins->grab_angle = _t->angle;
}

// The held instrument after its hands moved: its size tab makes it longer or shorter (its other end staying); a
// compass's hinge sets its radius and its way; its knob turns it (resting at each 15°); one hand drags it (docking
// and sliding); two turn and move it as the line between them does.
void fude_zoom_instruments_follow(fude_zoom_instruments* _ins) {
    fude_zoom_instrument* _t = &_ins->tools[_ins->held];
    if(_ins->growing) {
        // Its size tab: as long (or as big) as reaches the hand, along its own way; the end that does not
        // grow kept where it is.
        fude_zoom_v2 _tab, _fixed;
        if(!fude_zoom_instrument_grow_local(_t, &_tab, &_fixed)) {
            return;
        }
        const fude_zoom_v2 _fs  = fude_zoom_instrument_place(_t, _fixed);
        const fude_zoom_v2 _dir = { rde_math_cos_f64(_t->angle), rde_math_sin_f64(_t->angle) };
        const f64 _reach = fude_zoom_v2_dot((fude_zoom_v2){ _ins->hand_at[0].x - _fs.x, _ins->hand_at[0].y - _fs.y }, _dir) - FUDE_ZOOM_INSTRUMENT_GROW_OUT * fude_zoom_instrument_icons(_t);
        const f64 _whole = fude_zoom_v2_dot((fude_zoom_v2){ _tab.x - _fixed.x, _tab.y - _fixed.y }, (fude_zoom_v2){ 1.0, 0.0 }) - FUDE_ZOOM_INSTRUMENT_GROW_OUT * fude_zoom_instrument_icons(_t);
        const f64 _k     = fude_zoom_instrument_size(_t);
        const f64 _min   = _t->kind == FUDE_ZOOM_INSTRUMENT_RULER ? 0.3 : 0.5;
        _t->size = rde_math_max_f64(_whole > 1.0 ? _k * _reach / _whole : _k, _min);   // (no top: zoomed in, it is long on the screen already)
        // Its middle moved so the end that stays, stays.
        fude_zoom_v2 _tab2, _fixed2;
        fude_zoom_instrument_grow_local(_t, &_tab2, &_fixed2);
        const fude_zoom_v2 _now = fude_zoom_instrument_place(_t, _fixed2);
        _t->at = (fude_zoom_v2){ _t->at.x + _fs.x - _now.x, _t->at.y + _fs.y - _now.y };
        return;
    }
    if(_ins->turning && _t->kind == FUDE_ZOOM_INSTRUMENT_COMPASS) {
        // The compass's hinge: the pencil goes where the hand takes it (its radius and its way), the needle stays.
        const fude_zoom_v2 _pencil = { _t->at.x + rde_math_cos_f64(_ins->grab_angle) * _ins->grab_r + _ins->hand_at[0].x - _ins->grab_hand[0].x,
                                       _t->at.y + rde_math_sin_f64(_ins->grab_angle) * _ins->grab_r + _ins->hand_at[0].y - _ins->grab_hand[0].y };
        _t->radius      = rde_math_max_f64(rde_math_hypot_f64(_pencil.x - _t->at.x, _pencil.y - _t->at.y), 4.0);
        _t->angle       = rde_math_atan2_f64(_pencil.y - _t->at.y, _pencil.x - _t->at.x);
        _ins->turned_at = rde_engine_get_time_now();
        return;
    }
    if(_ins->turning) {
        // The knob: turned round its middle as the pen goes round it.
        const f64 _from = rde_math_atan2_f64(_ins->grab_hand[0].y - _t->at.y, _ins->grab_hand[0].x - _t->at.x);
        const f64 _now  = rde_math_atan2_f64(_ins->hand_at[0].y - _t->at.y, _ins->hand_at[0].x - _t->at.x);
        _t->angle      = fude_zoom_instrument_detent(fude_zoom_instrument_wrap(_ins->grab_angle + (_now - _from)));
        _ins->docked   = -1;
        _ins->turned_at = rde_engine_get_time_now();
        return;
    }
    if(_ins->hands == 1u) {
        const fude_zoom_v2 _want = { _ins->grab_at.x + _ins->hand_at[0].x - _ins->grab_hand[0].x, _ins->grab_at.y + _ins->hand_at[0].y - _ins->grab_hand[0].y };
        fude_zoom_instruments_slide(_ins, _want);
        return;
    }
    // Two hands: turned and moved as the line between them is (never scaled).
    const fude_zoom_v2 _g0 = _ins->grab_hand[0], _g1 = _ins->grab_hand[1];
    const fude_zoom_v2 _h0 = _ins->hand_at[0],   _h1 = _ins->hand_at[1];
    const f64 _turn  = fude_zoom_instrument_wrap(rde_math_atan2_f64(_h1.y - _h0.y, _h1.x - _h0.x) - rde_math_atan2_f64(_g1.y - _g0.y, _g1.x - _g0.x));
    const f64 _angle = fude_zoom_instrument_detent(fude_zoom_instrument_wrap(_ins->grab_angle + _turn));
    const f64 _used  = _angle - _ins->grab_angle;
    const fude_zoom_v2 _gm = { (_g0.x + _g1.x) * 0.5, (_g0.y + _g1.y) * 0.5 };
    const fude_zoom_v2 _hm = { (_h0.x + _h1.x) * 0.5, (_h0.y + _h1.y) * 0.5 };
    const f64 _c = rde_math_cos_f64(_used), _s = rde_math_sin_f64(_used);
    const fude_zoom_v2 _rel = { _ins->grab_at.x - _gm.x, _ins->grab_at.y - _gm.y };
    _t->at    = (fude_zoom_v2){ _hm.x + _rel.x * _c - _rel.y * _s, _hm.y + _rel.x * _s + _rel.y * _c };
    _t->angle = _angle;
    _ins->docked    = -1;
    _ins->turned_at = rde_engine_get_time_now();
}

// See the header. With none held: a × first, then a size tab, then a body. With one held: a second finger on it or
// within 360 points of its middle turns it with the first. One hand on a × at a time.
b8 fude_zoom_instruments_finger_down(fude_zoom_instruments* _ins, u64 _id, fude_zoom_v2 _at) {
    if(_ins->closing_hand != 0u) {
        return false;   // (one hand on a × at a time)
    }
    if(_ins->held < 0) {
        const i32 _x = fude_zoom_instruments_close_under(_ins, _at);
        if(_x >= 0) {
            _ins->closing      = _x;
            _ins->closing_hand = _id;
            _ins->closing_at   = _at;
            return true;
        }
        const i32 _g = fude_zoom_instruments_grow_under(_ins, _at);
        if(_g >= 0) {
            fude_zoom_instruments_take_grow(_ins, (u32)_g, _id, _at);
            return true;
        }
    }
    if(_ins->held >= 0) {
        // A second hand near the one held turns it.
        if(_ins->hands == 1u && !_ins->turning && _id != _ins->hand_id[0]) {
            const fude_zoom_instrument* _t = &_ins->tools[_ins->held];
            if(fude_zoom_instrument_inside(_ins, (u32)_ins->held, _at) || rde_math_hypot_f64(_at.x - _t->at.x, _at.y - _t->at.y) < 360.0) {
                _ins->hand_id[1] = _id;
                _ins->hand_at[1] = _at;
                _ins->hands      = 2u;
                fude_zoom_instruments_regrab(_ins);
                return true;
            }
        }
        return false;
    }
    const i32 _k = fude_zoom_instruments_at(_ins, _at);
    if(_k < 0) {
        return false;
    }
    fude_zoom_instruments_hold(_ins, _id, _at, (u32)_k);
    return true;
}

// One hand (_id at _at) takes instrument _k by its body: raised to the top, the hands measured from here.
void fude_zoom_instruments_hold(fude_zoom_instruments* _ins, u64 _id, fude_zoom_v2 _at, u32 _k) {
    _ins->held       = (i32)_k;
    _ins->hands      = 1u;
    _ins->hand_id[0] = _id;
    _ins->hand_at[0] = _at;
    _ins->turning    = false;
    fude_zoom_instrument_raise(_ins, _k);
    fude_zoom_instruments_regrab(_ins);
}

// Half an instrument's narrowest width on the screen (its outline's box, its own way round): a
// ruler's, half its band.
f64 fude_zoom_instrument_thin(const fude_zoom_instruments* _ins, u32 _slot) {
    fude_zoom_v2 _o[FUDE_ZOOM_ARC_SIDES + 8u];
    const u32 _n = fude_zoom_instrument_outline(_ins, _slot, _o, FUDE_ZOOM_ARC_SIDES + 8u);
    if(_n < 3u) {
        return 1e9;
    }
    fude_zoom_box _b = fude_zoom_box_empty();
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_v2 _l = fude_zoom_instrument_unplace(&_ins->tools[_slot], _o[_i]);
        _b = fude_zoom_box_union(_b, (fude_zoom_box){ _l.x, _l.y, _l.x, _l.y });
    }
    return 0.5 * rde_math_min_f64(_b.max_x - _b.min_x, _b.max_y - _b.min_y);
}

// How near _at is to an instrument's nearest edge (screen points).
f64 fude_zoom_instrument_edge_off(const fude_zoom_instruments* _ins, u32 _slot, fude_zoom_v2 _at) {
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    const u32 _n = fude_zoom_instrument_edges(_ins, _slot, _e);
    f64 _best = 1e300;
    for(u32 _j = 0; _j < _n; _j++) {
        f64 _along, _off;
        if(_e[_j].curve) {
            fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
            f64 _round;
            const u32 _m = fude_zoom_instrument_curve_of(_ins, _slot, &_e[_j], _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
            if(_m == 0u) {
                continue;
            }
            fude_zoom_instrument_on_curve(_c, _m, _at, 0.0, 0.0, &_along, &_off, &_round, _e[_j].open);
        } else {
            fude_zoom_instrument_on_edge(&_e[_j], _at, &_along, &_off);
        }
        _best = rde_math_min_f64(_best, _off);
    }
    return _best;
}

// A hand on a × going off it: it is not put away (the hand stays the instruments', doing nothing).
b8 fude_zoom_instruments_closing_moved(fude_zoom_instruments* _ins, u64 _id, fude_zoom_v2 _at) {
    if(_ins->closing_hand == 0u || _ins->closing_hand != _id) {
        return false;
    }
    if(rde_math_hypot_f64(_at.x - _ins->closing_at.x, _at.y - _ins->closing_at.y) > FUDE_ZOOM_INSTRUMENT_CLOSE + 8.0) {
        _ins->closing = -1;
    }
    return true;
}

// That hand lifted: on the × still, put away.
b8 fude_zoom_instruments_closing_up(fude_zoom_instruments* _ins, u64 _id) {
    if(_ins->closing_hand == 0u || _ins->closing_hand != _id) {
        return false;
    }
    if(_ins->closing >= 0) {
        fude_zoom_instrument_remove(_ins, (u32)_ins->closing);
    }
    _ins->closing      = -1;
    _ins->closing_hand = 0u;
    return true;
}

// See the header.
b8 fude_zoom_instruments_finger_moved(fude_zoom_instruments* _ins, u64 _id, fude_zoom_v2 _at) {
    if(fude_zoom_instruments_closing_moved(_ins, _id, _at)) {
        return true;
    }
    if(_ins->held < 0) {
        return false;
    }
    for(u32 _i = 0; _i < _ins->hands; _i++) {
        if(_ins->hand_id[_i] == _id) {
            _ins->hand_at[_i] = _at;
            fude_zoom_instruments_follow(_ins);
            return true;
        }
    }
    return false;
}

// See the header. Of two hands, the other goes on alone; the last lets it go.
b8 fude_zoom_instruments_finger_up(fude_zoom_instruments* _ins, u64 _id) {
    if(fude_zoom_instruments_closing_up(_ins, _id)) {
        return true;
    }
    if(_ins->held < 0) {
        return false;
    }
    for(u32 _i = 0; _i < _ins->hands; _i++) {
        if(_ins->hand_id[_i] == _id) {
            if(_ins->hands == 2u) {
                // The other hand goes on dragging it alone.
                _ins->hand_id[0] = _ins->hand_id[1 - _i];
                _ins->hand_at[0] = _ins->hand_at[1 - _i];
                _ins->hands      = 1u;
                fude_zoom_instruments_regrab(_ins);
            } else {
                // Let go: it rests where it is (touching the other still, it
                // docks again as soon as it is dragged).
                _ins->held    = -1;
                _ins->hands   = 0u;
                _ins->turning = false;
                _ins->growing = false;
                _ins->docked  = -1;
            }
            return true;
        }
    }
    return false;
}
