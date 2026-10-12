// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/paper.h"
#include "drawing/ink/canvas.h"

#include <math.h>

// ===========================================================================
// See paper.h. Lines are made whole and cut to the view (Liang–Barsky); rings
// as chords about FZP_CHORD points long on screen, only over the angles the
// view takes in.
// ===========================================================================

#define FZP_PI          3.14159265358979323846
#define FZP_CHORD       12.0    // a ring's chord on screen (points): its sag under a third of a point past 60 points round
#define FZP_FINEST      8.0     // engineering's finest lines this far apart on screen (points) at least
#define FZP_HEX         1.0     // a hexagon's radius, of the step
#define FZP_STAFF_GAP   6.0     // music: steps between a staff and the next
#define FZP_CUE         0.3     // Cornell: its cue column, of the page's width
#define FZP_SUMMARY     0.2     // ...its summary, of its height

RDE_INTERNAL void fzp_push_as(rde_arr* _out, fude_zoom_v2 _a, fude_zoom_v2 _b, f32 _strength, b8 _heavy, u8 _kind) {
    if(!(_strength > 0.0f) || rde_arr_length(_out) >= FUDE_ZOOM_PAPER_MOST) {
        return;
    }
    const fude_zoom_paper_line _l = { _a, _b, _strength > 1.0f ? 1.0f : _strength, _heavy, _kind };
    rde_arr_add(_out, (any)&_l);
}

RDE_INTERNAL void fzp_push(rde_arr* _out, fude_zoom_v2 _a, fude_zoom_v2 _b, f32 _strength, b8 _heavy) {
    fzp_push_as(_out, _a, _b, _strength, _heavy, FUDE_ZOOM_PAPER_PLAIN);
}

// The part of p + t·d, t in [_t0, _t1], inside _box: pushed.
RDE_INTERNAL void fzp_clip(rde_arr* _out, fude_zoom_box _box, fude_zoom_v2 _p, fude_zoom_v2 _d, f64 _t0, f64 _t1, f32 _strength, b8 _heavy) {
    const f64 _p_[2] = { _p.x, _p.y }, _d_[2] = { _d.x, _d.y }, _lo[2] = { _box.min_x, _box.min_y }, _hi[2] = { _box.max_x, _box.max_y };
    for(u32 _k = 0; _k < 2u; _k++) {
        if(fabs(_d_[_k]) < 1e-300) {
            if(_p_[_k] < _lo[_k] || _p_[_k] > _hi[_k]) {
                return;
            }
            continue;
        }
        f64 _a = (_lo[_k] - _p_[_k]) / _d_[_k], _b = (_hi[_k] - _p_[_k]) / _d_[_k];
        if(_a > _b) {
            const f64 _x = _a;
            _a = _b;
            _b = _x;
        }
        _t0 = fmax(_t0, _a);
        _t1 = fmin(_t1, _b);
    }
    if(_t0 < _t1) {
        fzp_push(_out, (fude_zoom_v2){ _p.x + _d.x * _t0, _p.y + _d.y * _t0 }, (fude_zoom_v2){ _p.x + _d.x * _t1, _p.y + _d.y * _t1 }, _strength, _heavy);
    }
}

RDE_INTERNAL void fzp_segment(rde_arr* _out, fude_zoom_box _box, fude_zoom_v2 _a, fude_zoom_v2 _b, f32 _strength, b8 _heavy) {
    fzp_clip(_out, _box, _a, (fude_zoom_v2){ _b.x - _a.x, _b.y - _a.y }, 0.0, 1.0, _strength, _heavy);
}

// How far along _n (unit) the view's corners go: into *_lo, *_hi.
RDE_INTERNAL void fzp_span(fude_zoom_box _v, fude_zoom_v2 _n, f64* _lo, f64* _hi) {
    const f64 _c[4] = { _n.x * _v.min_x + _n.y * _v.min_y, _n.x * _v.max_x + _n.y * _v.min_y, _n.x * _v.max_x + _n.y * _v.max_y, _n.x * _v.min_x + _n.y * _v.max_y };
    *_lo = fmin(fmin(_c[0], _c[1]), fmin(_c[2], _c[3]));
    *_hi = fmax(fmax(_c[0], _c[1]), fmax(_c[2], _c[3]));
}

// Lines along _d (unit) _gap apart (one through the middle), in the view; every other one _fine (the finer lattice's).
RDE_INTERNAL void fzp_family(rde_arr* _out, fude_zoom_box _v, fude_zoom_v2 _d, f64 _gap, f32 _own, f32 _fine) {
    const fude_zoom_v2 _n = { -_d.y, _d.x };
    f64 _lo = 0.0, _hi = 0.0;
    fzp_span(_v, _n, &_lo, &_hi);
    const f64 _half = _gap * 0.5;
    const f64 _k0 = ceil(_lo / _half), _k1 = floor(_hi / _half);
    if(_k1 - _k0 > 4000.0) {
        return;
    }
    for(f64 _k = _k0; _k <= _k1; _k += 1.0) {
        const b8 _odd = fmod(fabs(_k), 2.0) == 1.0;
        fzp_clip(_out, _v, (fude_zoom_v2){ _n.x * _k * _half, _n.y * _k * _half }, _d, -1e300, 1e300, _odd ? _fine : _own, false);
    }
}

// Hexagons _r round (pointed at the top) over the view, _strength strong.
RDE_INTERNAL void fzp_hexes(rde_arr* _out, fude_zoom_box _v, f64 _r, f32 _strength) {
    if(!(_strength > 0.0f) || !(_r > 0.0)) {
        return;
    }
    const f64 _dx = sqrt(3.0) * _r, _dy = 1.5 * _r;
    const f64 _r0 = floor((_v.min_y - _r) / _dy), _r1 = ceil((_v.max_y + _r) / _dy);
    const f64 _c0 = floor((_v.min_x - _r) / _dx) - 1.0, _c1 = ceil((_v.max_x + _r) / _dx) + 1.0;
    if((_r1 - _r0 + 1.0) * (_c1 - _c0 + 1.0) * 3.0 > (f64)FUDE_ZOOM_PAPER_MOST) {
        return;
    }
    for(f64 _row = _r0; _row <= _r1; _row += 1.0) {
        const f64 _shift = fmod(fabs(_row), 2.0) == 1.0 ? 0.5 : 0.0;
        for(f64 _col = _c0; _col <= _c1; _col += 1.0) {
            const fude_zoom_v2 _c = { _dx * (_col + _shift), _dy * _row };
            // Three of its six sides (from its right shoulder over its top to its left side): the rest its neighbours'.
            fude_zoom_v2 _p[4];
            for(u32 _i = 0; _i < 4u; _i++) {
                const f64 _a = FZP_PI / 6.0 + (f64)_i * FZP_PI / 3.0;
                _p[_i] = (fude_zoom_v2){ _c.x + _r * cos(_a), _c.y + _r * sin(_a) };
            }
            for(u32 _i = 0; _i < 3u; _i++) {
                fzp_segment(_out, _v, _p[_i], _p[_i + 1u], _strength, false);
            }
        }
    }
}

// A ring _r round the middle, over the angles the view takes in, in chords about FZP_CHORD points long (_px: points a
// unit).
RDE_INTERNAL void fzp_ring(rde_arr* _out, fude_zoom_box _v, f64 _r, f64 _px, f32 _strength) {
    f64 _a0 = 0.0, _a1 = 2.0 * FZP_PI;
    const b8 _inside = _v.min_x <= 0.0 && _v.max_x >= 0.0 && _v.min_y <= 0.0 && _v.max_y >= 0.0;
    if(!_inside) {
        // (the view's corners' angles round the middle, from its own middle's: the view less than half a turn of it)
        const f64 _mid = atan2((_v.min_y + _v.max_y) * 0.5, (_v.min_x + _v.max_x) * 0.5);
        const f64 _cx[4] = { _v.min_x, _v.max_x, _v.max_x, _v.min_x }, _cy[4] = { _v.min_y, _v.min_y, _v.max_y, _v.max_y };
        f64 _lo = 0.0, _hi = 0.0;
        for(u32 _i = 0; _i < 4u; _i++) {
            f64 _rel = atan2(_cy[_i], _cx[_i]) - _mid;
            _rel -= 2.0 * FZP_PI * floor((_rel + FZP_PI) / (2.0 * FZP_PI));
            _lo = _i == 0u ? _rel : fmin(_lo, _rel);
            _hi = _i == 0u ? _rel : fmax(_hi, _rel);
        }
        _a0 = _mid + _lo;
        _a1 = _mid + _hi;
    }
    const f64 _len = (_a1 - _a0) * _r * _px;
    u32 _n = (u32)fmin(fmax(ceil(_len / FZP_CHORD), 4.0), 720.0);
    _n = _inside && _n < 24u ? 24u : _n;
    fude_zoom_v2 _prev = { _r * cos(_a0), _r * sin(_a0) };
    for(u32 _i = 1; _i <= _n; _i++) {
        const f64 _a = _a0 + (_a1 - _a0) * (f64)_i / (f64)_n;
        const fude_zoom_v2 _p = { _r * cos(_a), _r * sin(_a) };
        fzp_segment(_out, _v, _prev, _p, _strength, false);
        _prev = _p;
    }
}

// Engineering's lines one way (_vertical: x = …), their steps tens of each other: each one as strong as the gap to the
// next as strong is wide on screen.
RDE_INTERNAL void fzp_graph(rde_arr* _out, fude_zoom_box _v, f64 _px, b8 _vertical) {
    const f64 _m = pow(10.0, ceil(log10(FZP_FINEST / _px)));   // (the finest: at least FZP_FINEST points apart)
    const f64 _from = _vertical ? _v.min_x : _v.min_y, _to = _vertical ? _v.max_x : _v.max_y;
    const f64 _k0 = ceil(_from / _m), _k1 = floor(_to / _m);
    if(!isfinite(_m) || _k1 - _k0 > 4000.0) {
        return;
    }
    for(f64 _k = _k0; _k <= _k1; _k += 1.0) {
        const f64 _ak = fabs(_k);
        const f64 _period = fmod(_ak, 100.0) == 0.0 ? 100.0 : fmod(_ak, 50.0) == 0.0 ? 50.0 : fmod(_ak, 10.0) == 0.0 ? 10.0 : fmod(_ak, 5.0) == 0.0 ? 5.0 : 1.0;
        const f64 _apart = _period * _m * _px;   // (on screen)
        const f32 _s = (f32)(0.2 + 0.8 * fmin(fmax((_apart - FZP_FINEST) / 72.0, 0.0), 1.0));
        const b8  _heavy = _apart >= 120.0;
        const f64 _at = _k * _m;
        if(_vertical) {
            fzp_push(_out, (fude_zoom_v2){ _at, _v.min_y }, (fude_zoom_v2){ _at, _v.max_y }, _s, _heavy);
        } else {
            fzp_push(_out, (fude_zoom_v2){ _v.min_x, _at }, (fude_zoom_v2){ _v.max_x, _at }, _s, _heavy);
        }
    }
}

u32 fude_zoom_paper_lines(u8 _paper, fude_zoom_box _view, f64 _step, f64 _fade, f64 _px, rde_arr* _out) {
    const u32 _was = (u32)rde_arr_length(_out);
    if(!(_step > 0.0) || !(_px > 0.0) || !isfinite(_step * _px) || _view.max_x <= _view.min_x || _view.max_y <= _view.min_y) {
        return 0u;
    }
    const f32 _fine = (f32)fmin(fmax(_fade, 0.0), 1.0);
    switch(_paper) {
        case FUDE_PAPER_ISOMETRIC: {
            const f64 _c = cos(FZP_PI / 6.0), _s = sin(FZP_PI / 6.0);
            fzp_family(_out, _view, (fude_zoom_v2){ 0.0, 1.0 }, _step, 1.0f, _fine);
            fzp_family(_out, _view, (fude_zoom_v2){ _c, _s }, _step, 1.0f, _fine);
            fzp_family(_out, _view, (fude_zoom_v2){ -_c, _s }, _step, 1.0f, _fine);
            break;
        }
        case FUDE_PAPER_HEX:
            // (the finer honeycomb is not in the coarser: one fades out as the other comes in)
            fzp_hexes(_out, _view, _step * FZP_HEX, 1.0f - _fine);
            fzp_hexes(_out, _view, _step * FZP_HEX * 0.5, _fine >= 0.05f ? _fine : 0.0f);   // (barely come in: not yet)
            break;
        case FUDE_PAPER_POLAR: {
            f64 _near = 0.0, _far = 0.0;
            {
                // (the nearest and farthest the view comes to the middle)
                const f64 _dx = _view.min_x > 0.0 ? _view.min_x : _view.max_x < 0.0 ? -_view.max_x : 0.0;
                const f64 _dy = _view.min_y > 0.0 ? _view.min_y : _view.max_y < 0.0 ? -_view.max_y : 0.0;
                _near = hypot(_dx, _dy);
                _far  = hypot(fmax(fabs(_view.min_x), fabs(_view.max_x)), fmax(fabs(_view.min_y), fabs(_view.max_y)));
            }
            const f64 _half = _step * 0.5;
            const f64 _k0 = fmax(ceil(_near / _half), 1.0), _k1 = floor(_far / _half);
            if(_k1 - _k0 > 2000.0) {
                break;
            }
            for(f64 _k = _k0; _k <= _k1; _k += 1.0) {
                fzp_ring(_out, _view, _k * _half, _px, fmod(_k, 2.0) == 1.0 ? _fine : 1.0f);
            }
            // Rays every 15°, those of 45° stronger.
            for(u32 _i = 0; _i < 24u; _i++) {
                const f64 _a = (f64)_i * FZP_PI / 12.0;
                fzp_clip(_out, _view, (fude_zoom_v2){ 0.0, 0.0 }, (fude_zoom_v2){ cos(_a), sin(_a) }, 0.0, 1e300, _i % 3u == 0u ? 1.0f : 0.6f, false);
            }
            break;
        }
        case FUDE_PAPER_ENGINEERING:
            fzp_graph(_out, _view, _px, true);
            fzp_graph(_out, _view, _px, false);
            break;
        case FUDE_PAPER_MUSIC: {
            const f32 _s = (f32)fmin(fmax((_step * _px - 3.0) / 5.0, 0.0), 1.0);   // (gone once its lines run together)
            const f64 _period = (4.0 + FZP_STAFF_GAP) * _step;
            const f64 _j0 = floor(-_view.max_y / _period) - 1.0, _j1 = ceil(-_view.min_y / _period) + 1.0;
            if(_s <= 0.0f || _j1 - _j0 > 2000.0) {
                break;
            }
            for(f64 _j = _j0; _j <= _j1; _j += 1.0) {
                for(u32 _i = 0; _i < 5u; _i++) {
                    const f64 _y = -_j * _period - (f64)_i * _step;
                    if(_y >= _view.min_y && _y <= _view.max_y) {
                        fzp_push(_out, (fude_zoom_v2){ _view.min_x, _y }, (fude_zoom_v2){ _view.max_x, _y }, _s, false);
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

b8 fude_zoom_paper_is_page(u8 _paper) {
    return _paper == FUDE_PAPER_CORNELL || _paper == FUDE_PAPER_PLANNER;
}

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
        const f64 _cue = _page.min_x + FZP_CUE * _w, _summary = _page.min_y + FZP_SUMMARY * _h;
        fzp_push(_out, (fude_zoom_v2){ _page.min_x, _top }, (fude_zoom_v2){ _page.max_x, _top }, 1.0f, true);
        for(f64 _y = _top - _step; _y > _page.min_y + _step * 0.25; _y -= _step) {
            if(fabs(_y - _summary) > _step * 0.3) {
                fzp_push(_out, (fude_zoom_v2){ _page.min_x, _y }, (fude_zoom_v2){ _page.max_x, _y }, 1.0f, false);
            }
        }
        fzp_push(_out, (fude_zoom_v2){ _page.min_x, _summary }, (fude_zoom_v2){ _page.max_x, _summary }, 1.0f, true);
        fzp_push(_out, (fude_zoom_v2){ _cue, _top }, (fude_zoom_v2){ _cue, _summary }, 1.0f, true);
    } else {
        // A title over its line, then lines each with a box to tick at their start.
        fzp_push(_out, (fude_zoom_v2){ _page.min_x, _top }, (fude_zoom_v2){ _page.max_x, _top }, 1.0f, true);
        const f64 _x0 = _page.min_x + 0.6 * _step, _side = 0.55 * _step;
        for(f64 _y = _top - _step; _y > _page.min_y + _step * 0.25; _y -= _step) {
            fzp_push(_out, (fude_zoom_v2){ _page.min_x, _y }, (fude_zoom_v2){ _page.max_x, _y }, 1.0f, false);
            const f64 _b0 = _y + 0.18 * _step, _b1 = _b0 + _side;
            if(_b1 < _y + _step) {
                const fude_zoom_v2 _c[4] = { { _x0, _b0 }, { _x0 + _side, _b0 }, { _x0 + _side, _b1 }, { _x0, _b1 } };
                for(u32 _e = 0; _e < 4u; _e++) {
                    fzp_push(_out, _c[_e], _c[(_e + 1u) % 4u], 1.0f, false);
                }
            }
        }
    }
    return (u32)rde_arr_length(_out) - _was;
}

#define FZP_PRINT_PX     24.0   // a sheet's points to a millimetre, as its patterns are made (a ring's chords half a millimetre)
#define FZP_PRINT_GRAPH  8.0    // ...engineering's: its finest lines a millimetre apart

u32 fude_zoom_paper_sheet(u8 _paper, fude_zoom_box _page, f64 _margin, f64 _ruled, f64 _grid, f64 _staff, rde_arr* _out) {
    const u32 _was = (u32)rde_arr_length(_out);
    const f64 _w = _page.max_x - _page.min_x, _h = _page.max_y - _page.min_y;
    if(!(_w > 0.0) || !(_h > 0.0) || !(_ruled > 0.0) || !(_grid > 0.0) || !(_staff > 0.0) || _h / fmin(_ruled, fmin(_grid, _staff)) > 4000.0 ||
       _w / _grid > 4000.0) {
        return 0u;
    }
    const f64 _edge = 1e-9 * (_w + _h);   // (a line on the page's own edge: left out)
    switch(_paper) {
        case FUDE_PAPER_LINES: {
            for(f64 _y = _page.max_y - _margin; _y > _page.min_y + _edge; _y -= _ruled) {
                fzp_push(_out, (fude_zoom_v2){ _page.min_x, _y }, (fude_zoom_v2){ _page.max_x, _y }, 1.0f, false);
            }
            fzp_push_as(_out, (fude_zoom_v2){ _page.min_x + _margin, _page.max_y }, (fude_zoom_v2){ _page.min_x + _margin, _page.min_y }, 1.0f, false,
                        FUDE_ZOOM_PAPER_MARGIN);
            break;
        }
        case FUDE_PAPER_SQUARES:
        case FUDE_PAPER_DOTS: {
            for(f64 _y = _page.max_y - _grid; _y > _page.min_y + _edge; _y -= _grid) {
                if(_paper == FUDE_PAPER_SQUARES) {
                    fzp_push(_out, (fude_zoom_v2){ _page.min_x, _y }, (fude_zoom_v2){ _page.max_x, _y }, 1.0f, false);
                    continue;
                }
                for(f64 _x = _page.min_x + _grid; _x < _page.max_x - _edge; _x += _grid) {
                    fzp_push_as(_out, (fude_zoom_v2){ _x, _y }, (fude_zoom_v2){ _x, _y }, 1.0f, false, FUDE_ZOOM_PAPER_DOT);
                }
            }
            for(f64 _x = _page.min_x + _grid; _paper == FUDE_PAPER_SQUARES && _x < _page.max_x - _edge; _x += _grid) {
                fzp_push(_out, (fude_zoom_v2){ _x, _page.max_y }, (fude_zoom_v2){ _x, _page.min_y }, 1.0f, false);
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
            // (as the page shows them: round its middle — music's staves from under its margin)
            const b8 _music = _paper == FUDE_PAPER_MUSIC;
            const fude_zoom_v2 _o = _music ? (fude_zoom_v2){ _page.min_x, _page.max_y - _margin }
                                           : (fude_zoom_v2){ (_page.min_x + _page.max_x) * 0.5, (_page.min_y + _page.max_y) * 0.5 };
            const fude_zoom_box _on = { _page.min_x - _o.x, _page.min_y - _o.y, _page.max_x - _o.x, (_music ? _o.y : _page.max_y) - _o.y };
            const u32 _from = (u32)rde_arr_length(_out);
            fude_zoom_paper_lines(_paper, _on, _music ? _staff : _grid, 0.0, _paper == FUDE_PAPER_ENGINEERING ? FZP_PRINT_GRAPH : FZP_PRINT_PX, _out);
            fude_zoom_paper_line* _l = (fude_zoom_paper_line*)_out->memory;
            for(u32 _i = _from; _i < (u32)rde_arr_length(_out); _i++) {
                _l[_i].a = (fude_zoom_v2){ _l[_i].a.x + _o.x, _l[_i].a.y + _o.y };
                _l[_i].b = (fude_zoom_v2){ _l[_i].b.x + _o.x, _l[_i].b.y + _o.y };
            }
            break;
        }
        default:
            break;   // (none: blank)
    }
    return (u32)rde_arr_length(_out) - _was;
}
