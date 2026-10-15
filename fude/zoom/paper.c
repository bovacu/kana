// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/paper.h"
#include "drawing/ink/canvas.h"

// ===========================================================================
// See paper.h. Lines are made whole and cut to the view (zoom.h's fude_zoom_clip_line, Liang–Barsky); rings as chords
// about FZPA_CHORD points long on screen, only over the angles the view takes in.
//
// The file is in three parts: the helpers that add a line (cut to the view or not) and make each pattern, then the
// endless canvas's papers (fude_zoom_paper_lines), then a notebook page's layouts and a printed sheet. Every helper
// stops adding once the output holds FUDE_ZOOM_PAPER_MOST lines, and each pattern gives up when the view would take
// thousands of its lines, so the work stays bounded: a view far too big for its step draws nothing of that pattern.
// Its own functions and constants are prefixed fzpa_ and FZPA_.
// ===========================================================================

#define FZPA_CHORD       12.0    // a ring's chord on screen (points): its sag under a third of a point past 60 points round
#define FZPA_FINEST      8.0     // engineering's finest lines this far apart on screen (points) at least
#define FZPA_HEX         1.0     // a hexagon's radius, of the step
#define FZPA_STAFF_GAP   6.0     // music: steps between a staff and the next
#define FZPA_CUE         0.3     // Cornell: its cue column, of the page's width
#define FZPA_SUMMARY     0.2     // ...its summary, of its height
#define FZPA_LINES_MOST  4000u   // a page's or a sheet's ruled lines one way, at most (as their checks below allow)
#define FZPA_WHOLE       9007199254740992.0   // 2^53: past it an f64 cannot hold every whole number

// See the header. A view past FZPA_WHOLE steps from 0 has lines that would all fall on each other: none. The patterns
// here count their lines with it too.
u32 fude_zoom_paper_count(f64 _k0, f64 _k1, f64 _most) {
    if(!(_k1 >= _k0) || !(_k1 - _k0 <= _most) || !(rde_math_abs_f64(_k0) < FZPA_WHOLE) || !(rde_math_abs_f64(_k1) < FZPA_WHOLE)) {
        return 0u;
    }
    return (u32)(_k1 - _k0) + 1u;
}

// Adds the line from _a to _b, of kind _kind, its strength held to 1. Nothing is added when the strength is not above 0
// or the output is full.
RDE_INTERNAL void fzpa_push_as(rde_arr* _out, fude_zoom_v2 _a, fude_zoom_v2 _b, f32 _strength, b8 _heavy, u8 _kind) {
    if(!(_strength > 0.0f) || rde_arr_length(_out) >= FUDE_ZOOM_PAPER_MOST) {
        return;
    }
    const fude_zoom_paper_line _l = { _a, _b, _strength > 1.0f ? 1.0f : _strength, _heavy, _kind };
    rde_arr_add(_out, (any)&_l);
}

// Adds a plain line (fzpa_push_as, in the paper's ink).
RDE_INTERNAL void fzpa_push(rde_arr* _out, fude_zoom_v2 _a, fude_zoom_v2 _b, f32 _strength, b8 _heavy) {
    fzpa_push_as(_out, _a, _b, _strength, _heavy, FUDE_ZOOM_PAPER_PLAIN);
}

// Writes how far along _n (a unit vector) the view's corners go, the least into *_lo and the most into *_hi.
RDE_INTERNAL void fzpa_span(fude_zoom_box _v, fude_zoom_v2 _n, f64* _lo, f64* _hi) {
    const f64 _c[4] = { _n.x * _v.min_x + _n.y * _v.min_y, _n.x * _v.max_x + _n.y * _v.min_y, _n.x * _v.max_x + _n.y * _v.max_y, _n.x * _v.min_x + _n.y * _v.max_y };
    *_lo = rde_math_min_f64(rde_math_min_f64(_c[0], _c[1]), rde_math_min_f64(_c[2], _c[3]));
    *_hi = rde_math_max_f64(rde_math_max_f64(_c[0], _c[1]), rde_math_max_f64(_c[2], _c[3]));
}

// Adds the part of the segment from _a to _b inside _box (zoom.h's fude_zoom_clip_line); nothing when none of it is,
// or when only a point of it is (it touches the box at a corner).
RDE_INTERNAL void fzpa_segment(rde_arr* _out, fude_zoom_box _box, fude_zoom_v2 _a, fude_zoom_v2 _b, f32 _strength, b8 _heavy) {
    if(fude_zoom_clip_line(&_a, &_b, _box) && (_a.x != _b.x || _a.y != _b.y)) {
        fzpa_push(_out, _a, _b, _strength, _heavy);
    }
}

// Adds the part of p + t·d, t in [_t0, _t1], that is inside _box; _d is a unit vector, and the range may be unbounded
// (±1e300: a line right across). The range is first cut to how far along _d the box's corners go, so the segment cut
// to the box (fzpa_segment) is no longer than the box is wide: ends a vast distance out would leave nothing exact.
RDE_INTERNAL void fzpa_clip(rde_arr* _out, fude_zoom_box _box, fude_zoom_v2 _p, fude_zoom_v2 _d, f64 _t0, f64 _t1, f32 _strength, b8 _heavy) {
    f64 _lo = 0.0, _hi = 0.0;
    fzpa_span(_box, _d, &_lo, &_hi);
    const f64 _at = _p.x * _d.x + _p.y * _d.y;   // (how far along _d the line's p is, as the corners' are measured)
    _t0 = rde_math_max_f64(_t0, _lo - _at);
    _t1 = rde_math_min_f64(_t1, _hi - _at);
    if(!(_t0 < _t1)) {
        return;
    }
    fzpa_segment(_out, _box, (fude_zoom_v2){ _p.x + _d.x * _t0, _p.y + _d.y * _t0 }, (fude_zoom_v2){ _p.x + _d.x * _t1, _p.y + _d.y * _t1 }, _strength, _heavy);
}

// Adds lines along _d (a unit vector) _gap / 2 apart, one through the middle, cut to the view: the paper's own, _gap
// apart, at strength _own, and the finer lattice's between them at _fine. Draws none when the view would take more
// than 4000.
RDE_INTERNAL void fzpa_family(rde_arr* _out, fude_zoom_box _v, fude_zoom_v2 _d, f64 _gap, f32 _own, f32 _fine) {
    const fude_zoom_v2 _n = { -_d.y, _d.x };
    f64 _lo = 0.0, _hi = 0.0;
    fzpa_span(_v, _n, &_lo, &_hi);
    const f64 _half = _gap * 0.5;
    const f64 _k0 = rde_math_ceil_f64(_lo / _half), _k1 = rde_math_floor_f64(_hi / _half);
    const u32 _lines = fude_zoom_paper_count(_k0, _k1, 4000.0);
    for(u32 _i = 0; _i < _lines; _i++) {
        const f64 _k = _k0 + (f64)_i;
        const b8 _odd = rde_math_fmod_f64(rde_math_abs_f64(_k), 2.0) == 1.0;
        fzpa_clip(_out, _v, (fude_zoom_v2){ _n.x * _k * _half, _n.y * _k * _half }, _d, -1e300, 1e300, _odd ? _fine : _own, false);
    }
}

// Adds a honeycomb of hexagons of radius _r (pointed at the top) over the view, _strength strong, each side once. Draws
// none when the view would take more than FUDE_ZOOM_PAPER_MOST sides.
RDE_INTERNAL void fzpa_hexes(rde_arr* _out, fude_zoom_box _v, f64 _r, f32 _strength) {
    if(!(_strength > 0.0f) || !(_r > 0.0)) {
        return;
    }
    const f64 _dx = rde_math_sqrt_f64(3.0) * _r, _dy = 1.5 * _r;
    const f64 _r0 = rde_math_floor_f64((_v.min_y - _r) / _dy), _r1 = rde_math_ceil_f64((_v.max_y + _r) / _dy);
    const f64 _c0 = rde_math_floor_f64((_v.min_x - _r) / _dx) - 1.0, _c1 = rde_math_ceil_f64((_v.max_x + _r) / _dx) + 1.0;
    const u32 _rows = fude_zoom_paper_count(_r0, _r1, (f64)FUDE_ZOOM_PAPER_MOST), _cols = fude_zoom_paper_count(_c0, _c1, (f64)FUDE_ZOOM_PAPER_MOST);
    if((f64)_rows * (f64)_cols * 3.0 > (f64)FUDE_ZOOM_PAPER_MOST) {
        return;
    }
    for(u32 _i = 0; _i < _rows; _i++) {
        const f64 _row = _r0 + (f64)_i;
        const f64 _shift = rde_math_fmod_f64(rde_math_abs_f64(_row), 2.0) == 1.0 ? 0.5 : 0.0;
        for(u32 _j = 0; _j < _cols; _j++) {
            const f64 _col = _c0 + (f64)_j;
            const fude_zoom_v2 _c = { _dx * (_col + _shift), _dy * _row };
            // Three of its six sides (from its right shoulder over its top to its left side): the rest its neighbours'.
            fude_zoom_v2 _p[4];
            for(u32 _i = 0; _i < 4u; _i++) {
                const f64 _a = RDE_PI / 6.0 + (f64)_i * RDE_PI / 3.0;
                _p[_i] = (fude_zoom_v2){ _c.x + _r * rde_math_cos_f64(_a), _c.y + _r * rde_math_sin_f64(_a) };
            }
            for(u32 _i = 0; _i < 3u; _i++) {
                fzpa_segment(_out, _v, _p[_i], _p[_i + 1u], _strength, false);
            }
        }
    }
}

// Adds a ring of radius _r round the middle, over the angles the view takes in, in chords about FZPA_CHORD points long
// (_px: points a unit): at least 4 chords, at most 720, and at least 24 when the view holds the middle.
RDE_INTERNAL void fzpa_ring(rde_arr* _out, fude_zoom_box _v, f64 _r, f64 _px, f32 _strength) {
    f64 _a0 = 0.0, _a1 = 2.0 * RDE_PI;
    const b8 _inside = _v.min_x <= 0.0 && _v.max_x >= 0.0 && _v.min_y <= 0.0 && _v.max_y >= 0.0;
    if(!_inside) {
        // The view's corners' angles round the middle, taken from the angle of the view's own middle; with the middle
        // outside the view, the view takes in less than half a turn.
        const f64 _mid = rde_math_atan2_f64((_v.min_y + _v.max_y) * 0.5, (_v.min_x + _v.max_x) * 0.5);
        const f64 _cx[4] = { _v.min_x, _v.max_x, _v.max_x, _v.min_x }, _cy[4] = { _v.min_y, _v.min_y, _v.max_y, _v.max_y };
        f64 _lo = 0.0, _hi = 0.0;
        for(u32 _i = 0; _i < 4u; _i++) {
            f64 _rel = rde_math_atan2_f64(_cy[_i], _cx[_i]) - _mid;
            _rel -= 2.0 * RDE_PI * rde_math_floor_f64((_rel + RDE_PI) / (2.0 * RDE_PI));
            _lo = _i == 0u ? _rel : rde_math_min_f64(_lo, _rel);
            _hi = _i == 0u ? _rel : rde_math_max_f64(_hi, _rel);
        }
        _a0 = _mid + _lo;
        _a1 = _mid + _hi;
    }
    const f64 _len = (_a1 - _a0) * _r * _px;
    u32 _n = (u32)rde_math_min_f64(rde_math_max_f64(rde_math_ceil_f64(_len / FZPA_CHORD), 4.0), 720.0);
    _n = _inside && _n < 24u ? 24u : _n;
    fude_zoom_v2 _prev = { _r * rde_math_cos_f64(_a0), _r * rde_math_sin_f64(_a0) };
    for(u32 _i = 1; _i <= _n; _i++) {
        const f64 _a = _a0 + (_a1 - _a0) * (f64)_i / (f64)_n;
        const fude_zoom_v2 _p = { _r * rde_math_cos_f64(_a), _r * rde_math_sin_f64(_a) };
        fzpa_segment(_out, _v, _prev, _p, _strength, false);
        _prev = _p;
    }
}

// Adds engineering's lines one way (_vertical: lines of constant x; otherwise of constant y), their steps powers of ten
// of each other. A line's strength comes from how far apart on screen it and the lines as strong as it are (every 1,
// 5, 10, 50 or 100 of the finest step): from 0.2 at FZPA_FINEST points up to 1 at 72 more, and heavy from 120 points.
// Draws none when the view would take more than 4000.
RDE_INTERNAL void fzpa_graph(rde_arr* _out, fude_zoom_box _v, f64 _px, b8 _vertical) {
    // The finest step: the smallest power of ten at least FZPA_FINEST points on screen.
    const f64 _m = rde_math_pow_f64(10.0, rde_math_ceil_f64(rde_math_log10_f64(FZPA_FINEST / _px)));
    const f64 _from = _vertical ? _v.min_x : _v.min_y, _to = _vertical ? _v.max_x : _v.max_y;
    const f64 _k0 = rde_math_ceil_f64(_from / _m), _k1 = rde_math_floor_f64(_to / _m);
    const u32 _lines = rde_math_is_finite_f64(_m) ? fude_zoom_paper_count(_k0, _k1, 4000.0) : 0u;
    for(u32 _i = 0; _i < _lines; _i++) {
        const f64 _k  = _k0 + (f64)_i;
        const f64 _ak = rde_math_abs_f64(_k);
        const f64 _period = rde_math_fmod_f64(_ak, 100.0) == 0.0 ? 100.0 : rde_math_fmod_f64(_ak, 50.0) == 0.0 ? 50.0 : rde_math_fmod_f64(_ak, 10.0) == 0.0 ? 10.0 : rde_math_fmod_f64(_ak, 5.0) == 0.0 ? 5.0 : 1.0;
        const f64 _apart = _period * _m * _px;   // On screen, in points.
        const f32 _s = (f32)(0.2 + 0.8 * rde_math_min_f64(rde_math_max_f64((_apart - FZPA_FINEST) / 72.0, 0.0), 1.0));
        const b8  _heavy = _apart >= 120.0;
        const f64 _at = _k * _m;
        if(_vertical) {
            fzpa_push(_out, (fude_zoom_v2){ _at, _v.min_y }, (fude_zoom_v2){ _at, _v.max_y }, _s, _heavy);
        } else {
            fzpa_push(_out, (fude_zoom_v2){ _v.min_x, _at }, (fude_zoom_v2){ _v.max_x, _at }, _s, _heavy);
        }
    }
}

// See the header. One case for each of the canvas apps' papers; the core's four and the page layouts add nothing here.
u32 fude_zoom_paper_lines(u8 _paper, fude_zoom_box _view, f64 _step, f64 _fade, f64 _px, rde_arr* _out) {
    const u32 _was = (u32)rde_arr_length(_out);
    if(!(_step > 0.0) || !(_px > 0.0) || !rde_math_is_finite_f64(_step * _px) || _view.max_x <= _view.min_x || _view.max_y <= _view.min_y) {
        return 0u;
    }
    const f32 _fine = (f32)rde_math_min_f64(rde_math_max_f64(_fade, 0.0), 1.0);
    switch(_paper) {
        case FUDE_PAPER_ISOMETRIC: {
            const f64 _c = rde_math_cos_f64(RDE_PI / 6.0), _s = rde_math_sin_f64(RDE_PI / 6.0);
            fzpa_family(_out, _view, (fude_zoom_v2){ 0.0, 1.0 }, _step, 1.0f, _fine);
            fzpa_family(_out, _view, (fude_zoom_v2){ _c, _s }, _step, 1.0f, _fine);
            fzpa_family(_out, _view, (fude_zoom_v2){ -_c, _s }, _step, 1.0f, _fine);
            break;
        }
        case FUDE_PAPER_HEX:
            // The finer honeycomb's lines are not the coarser's: one fades out as the other comes in. The finer one is
            // left out until it is 0.05 strong.
            fzpa_hexes(_out, _view, _step * FZPA_HEX, 1.0f - _fine);
            fzpa_hexes(_out, _view, _step * FZPA_HEX * 0.5, _fine >= 0.05f ? _fine : 0.0f);
            break;
        case FUDE_PAPER_POLAR: {
            f64 _near = 0.0, _far = 0.0;
            {
                // The nearest and farthest the view comes to the middle.
                const f64 _dx = _view.min_x > 0.0 ? _view.min_x : _view.max_x < 0.0 ? -_view.max_x : 0.0;
                const f64 _dy = _view.min_y > 0.0 ? _view.min_y : _view.max_y < 0.0 ? -_view.max_y : 0.0;
                _near = rde_math_hypot_f64(_dx, _dy);
                _far  = rde_math_hypot_f64(rde_math_max_f64(rde_math_abs_f64(_view.min_x), rde_math_abs_f64(_view.max_x)), rde_math_max_f64(rde_math_abs_f64(_view.min_y), rde_math_abs_f64(_view.max_y)));
            }
            const f64 _half = _step * 0.5;
            // Rings _step / 2 apart, the paper's own at full strength and the finer between them at _fine; none past
            // 2000.
            const f64 _k0 = rde_math_max_f64(rde_math_ceil_f64(_near / _half), 1.0), _k1 = rde_math_floor_f64(_far / _half);
            if(_k1 - _k0 > 2000.0) {
                break;
            }
            const u32 _rings = fude_zoom_paper_count(_k0, _k1, 2000.0);
            for(u32 _i = 0; _i < _rings; _i++) {
                const f64 _k = _k0 + (f64)_i;
                fzpa_ring(_out, _view, _k * _half, _px, rde_math_fmod_f64(_k, 2.0) == 1.0 ? _fine : 1.0f);
            }
            // Rays every 15°, those of 45° stronger.
            for(u32 _i = 0; _i < 24u; _i++) {
                const f64 _a = (f64)_i * RDE_PI / 12.0;
                fzpa_clip(_out, _view, (fude_zoom_v2){ 0.0, 0.0 }, (fude_zoom_v2){ rde_math_cos_f64(_a), rde_math_sin_f64(_a) }, 0.0, 1e300, _i % 3u == 0u ? 1.0f : 0.6f, false);
            }
            break;
        }
        case FUDE_PAPER_ENGINEERING:
            fzpa_graph(_out, _view, _px, true);
            fzpa_graph(_out, _view, _px, false);
            break;
        case FUDE_PAPER_MUSIC: {
            // Gone once its lines run together: none at 3 points apart, full at 8.
            const f32 _s = (f32)rde_math_min_f64(rde_math_max_f64((_step * _px - 3.0) / 5.0, 0.0), 1.0);
            // A staff and the gap under it; staff _j's top line is at y = -_j * _period.
            const f64 _period = (4.0 + FZPA_STAFF_GAP) * _step;
            const f64 _j0 = rde_math_floor_f64(-_view.max_y / _period) - 1.0, _j1 = rde_math_ceil_f64(-_view.min_y / _period) + 1.0;
            if(_s <= 0.0f || _j1 - _j0 > 2000.0) {
                break;
            }
            const u32 _staves = fude_zoom_paper_count(_j0, _j1, 2000.0);
            for(u32 _n = 0; _n < _staves; _n++) {
                const f64 _j = _j0 + (f64)_n;
                for(u32 _i = 0; _i < 5u; _i++) {
                    const f64 _y = -_j * _period - (f64)_i * _step;
                    if(_y >= _view.min_y && _y <= _view.max_y) {
                        fzpa_push(_out, (fude_zoom_v2){ _view.min_x, _y }, (fude_zoom_v2){ _view.max_x, _y }, _s, false);
                    }
                }
            }
            break;
        }
        default:
            break;
    }
    return (u32)rde_arr_length(_out) - _was;
}

// See the header.
b8 fude_zoom_paper_is_page(u8 _paper) {
    return _paper == FUDE_PAPER_CORNELL || _paper == FUDE_PAPER_PLANNER;
}

// See the header. Cornell's cue column is FZPA_CUE of the page's width and its summary FZPA_SUMMARY of its height; a
// planner's boxes are 0.55 of a step square, 0.6 of a step in from the left. No line is ruled within a quarter of a
// step of the page's foot. Adds nothing when the page would take more than 4000 lines.
u32 fude_zoom_paper_page(u8 _paper, fude_zoom_box _page, f64 _top, f64 _step, rde_arr* _out) {
    const u32 _was = (u32)rde_arr_length(_out);
    if(!fude_zoom_paper_is_page(_paper) || !(_step > 0.0) || _page.max_x <= _page.min_x || _top <= _page.min_y) {
        return 0u;
    }
    const f64 _w = _page.max_x - _page.min_x, _h = _page.max_y - _page.min_y;
    if((_top - _page.min_y) / _step > 4000.0) {
        return 0u;
    }
    if(_paper == FUDE_PAPER_CORNELL) {
        // Its notes ruled across, its summary's line across, its cue column's down to it, a line under its title.
        const f64 _cue = _page.min_x + FZPA_CUE * _w, _summary = _page.min_y + FZPA_SUMMARY * _h;
        fzpa_push(_out, (fude_zoom_v2){ _page.min_x, _top }, (fude_zoom_v2){ _page.max_x, _top }, 1.0f, true);
        for(u32 _k = 1; _k <= FZPA_LINES_MOST; _k++) {   // (by index: a step taken off a number far out may not move it)
            const f64 _y = _top - (f64)_k * _step;
            if(!(_y > _page.min_y + _step * 0.25)) {
                break;
            }
            if(rde_math_abs_f64(_y - _summary) > _step * 0.3) {
                fzpa_push(_out, (fude_zoom_v2){ _page.min_x, _y }, (fude_zoom_v2){ _page.max_x, _y }, 1.0f, false);
            }
        }
        fzpa_push(_out, (fude_zoom_v2){ _page.min_x, _summary }, (fude_zoom_v2){ _page.max_x, _summary }, 1.0f, true);
        fzpa_push(_out, (fude_zoom_v2){ _cue, _top }, (fude_zoom_v2){ _cue, _summary }, 1.0f, true);
    } else {
        // A title over its line, then lines each with a box to tick at their start.
        fzpa_push(_out, (fude_zoom_v2){ _page.min_x, _top }, (fude_zoom_v2){ _page.max_x, _top }, 1.0f, true);
        const f64 _x0 = _page.min_x + 0.6 * _step, _side = 0.55 * _step;
        for(u32 _k = 1; _k <= FZPA_LINES_MOST; _k++) {
            const f64 _y = _top - (f64)_k * _step;
            if(!(_y > _page.min_y + _step * 0.25)) {
                break;
            }
            fzpa_push(_out, (fude_zoom_v2){ _page.min_x, _y }, (fude_zoom_v2){ _page.max_x, _y }, 1.0f, false);
            const f64 _b0 = _y + 0.18 * _step, _b1 = _b0 + _side;
            if(_b1 < _y + _step) {
                const fude_zoom_v2 _c[4] = { { _x0, _b0 }, { _x0 + _side, _b0 }, { _x0 + _side, _b1 }, { _x0, _b1 } };
                for(u32 _e = 0; _e < 4u; _e++) {
                    fzpa_push(_out, _c[_e], _c[(_e + 1u) % 4u], 1.0f, false);
                }
            }
        }
    }
    return (u32)rde_arr_length(_out) - _was;
}

// The _px fude_zoom_paper_sheet hands fude_zoom_paper_lines: a sheet's units are millimetres.
#define FZPA_PRINT_PX     24.0   // a sheet's points to a millimetre, as its patterns are made (a ring's chords half a millimetre)
#define FZPA_PRINT_GRAPH  8.0    // ...engineering's: its finest lines a millimetre apart

// See the header. The core's four are ruled here; the page layouts are fude_zoom_paper_page from under the margin; the
// patterns are fude_zoom_paper_lines with no fade, made round the page's middle (music's from under its margin) and
// moved onto the page. Adds nothing when a spacing would put more than 4000 lines down or across the page.
u32 fude_zoom_paper_sheet(u8 _paper, fude_zoom_box _page, f64 _margin, f64 _ruled, f64 _grid, f64 _staff, rde_arr* _out) {
    const u32 _was = (u32)rde_arr_length(_out);
    const f64 _w = _page.max_x - _page.min_x, _h = _page.max_y - _page.min_y;
    if(!(_w > 0.0) || !(_h > 0.0) || !(_ruled > 0.0) || !(_grid > 0.0) || !(_staff > 0.0) || _h / rde_math_min_f64(_ruled, rde_math_min_f64(_grid, _staff)) > 4000.0 ||
       _w / _grid > 4000.0) {
        return 0u;
    }
    // A line on the page's own edge is left out; this is how near counts as on it.
    const f64 _edge = 1e-9 * (_w + _h);
    switch(_paper) {
        case FUDE_PAPER_LINES: {
            // Each way by index, at most FZPA_LINES_MOST + 1 (a step taken off a number far out may not move it).
            for(u32 _k = 0; _k <= FZPA_LINES_MOST; _k++) {
                const f64 _y = _page.max_y - _margin - (f64)_k * _ruled;
                if(!(_y > _page.min_y + _edge)) {
                    break;
                }
                fzpa_push(_out, (fude_zoom_v2){ _page.min_x, _y }, (fude_zoom_v2){ _page.max_x, _y }, 1.0f, false);
            }
            fzpa_push_as(_out, (fude_zoom_v2){ _page.min_x + _margin, _page.max_y }, (fude_zoom_v2){ _page.min_x + _margin, _page.min_y }, 1.0f, false,
                        FUDE_ZOOM_PAPER_MARGIN);
            break;
        }
        case FUDE_PAPER_SQUARES:
        case FUDE_PAPER_DOTS: {
            for(u32 _k = 1; _k <= FZPA_LINES_MOST + 1u; _k++) {
                const f64 _y = _page.max_y - (f64)_k * _grid;
                if(!(_y > _page.min_y + _edge)) {
                    break;
                }
                if(_paper == FUDE_PAPER_SQUARES) {
                    fzpa_push(_out, (fude_zoom_v2){ _page.min_x, _y }, (fude_zoom_v2){ _page.max_x, _y }, 1.0f, false);
                    continue;
                }
                for(u32 _j = 1; _j <= FZPA_LINES_MOST + 1u; _j++) {
                    const f64 _x = _page.min_x + (f64)_j * _grid;
                    if(!(_x < _page.max_x - _edge)) {
                        break;
                    }
                    fzpa_push_as(_out, (fude_zoom_v2){ _x, _y }, (fude_zoom_v2){ _x, _y }, 1.0f, false, FUDE_ZOOM_PAPER_DOT);
                }
            }
            for(u32 _j = 1; _paper == FUDE_PAPER_SQUARES && _j <= FZPA_LINES_MOST + 1u; _j++) {
                const f64 _x = _page.min_x + (f64)_j * _grid;
                if(!(_x < _page.max_x - _edge)) {
                    break;
                }
                fzpa_push(_out, (fude_zoom_v2){ _x, _page.max_y }, (fude_zoom_v2){ _x, _page.min_y }, 1.0f, false);
            }
            break;
        }
        case FUDE_PAPER_CORNELL:
        case FUDE_PAPER_PLANNER:
            fude_zoom_paper_page(_paper, _page, _page.max_y - _margin, _ruled, _out);
            break;
        case FUDE_PAPER_ISOMETRIC:
        case FUDE_PAPER_HEX:
        case FUDE_PAPER_POLAR:
        case FUDE_PAPER_ENGINEERING:
        case FUDE_PAPER_MUSIC: {
            // As the page shows them: round its middle, and music's staves from under its margin.
            const b8 _music = _paper == FUDE_PAPER_MUSIC;
            const fude_zoom_v2 _o = _music ? (fude_zoom_v2){ _page.min_x, _page.max_y - _margin }
                                           : (fude_zoom_v2){ (_page.min_x + _page.max_x) * 0.5, (_page.min_y + _page.max_y) * 0.5 };
            const fude_zoom_box _on = { _page.min_x - _o.x, _page.min_y - _o.y, _page.max_x - _o.x, (_music ? _o.y : _page.max_y) - _o.y };
            const u32 _from = (u32)rde_arr_length(_out);
            fude_zoom_paper_lines(_paper, _on, _music ? _staff : _grid, 0.0, _paper == FUDE_PAPER_ENGINEERING ? FZPA_PRINT_GRAPH : FZPA_PRINT_PX, _out);
            fude_zoom_paper_line* _l = (fude_zoom_paper_line*)_out->memory;
            for(u32 _i = _from; _i < (u32)rde_arr_length(_out); _i++) {
                _l[_i].a = (fude_zoom_v2){ _l[_i].a.x + _o.x, _l[_i].a.y + _o.y };
                _l[_i].b = (fude_zoom_v2){ _l[_i].b.x + _o.x, _l[_i].b.y + _o.y };
            }
            break;
        }
        default:
            break;   // None: a blank page.
    }
    return (u32)rde_arr_length(_out) - _was;
}
