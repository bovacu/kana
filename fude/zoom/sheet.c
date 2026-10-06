// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/sheet.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See sheet.h.
// ===========================================================================

#define FUDE_ZOOM_SHEET_LABEL_PT 34.0   // its rulers' numbers this far apart on the screen at least
#define FUDE_ZOOM_SHEET_MARKS    4000u  // ticks or lines one way, at most (the screen's are far fewer)

u32 fude_zoom_sheet_numbers(f64* _n, f64 _hw, f64 _hh, f64 _scale, u32 _flags) {
    _n[0] = fabs(_hw);
    _n[1] = fabs(_hh);
    _n[2] = _scale >= 1.0 && isfinite(_scale) ? _scale : 1.0;
    _n[3] = (f64)_flags;
    return 4u;
}

b8 fude_zoom_sheet_of(const f64* _n, u32 _count, fude_zoom_sheet* _out) {
    if(_count < 2u) {
        return false;
    }
    _out->hw    = fabs(_n[0]);
    _out->hh    = fabs(_n[1]);
    _out->scale = _count >= 3u && _n[2] >= 1.0 && isfinite(_n[2]) ? _n[2] : 1.0;
    _out->flags = _count >= 4u && _n[3] >= 0.0 && _n[3] < 4294967296.0 ? (u32)_n[3] : 0u;
    return true;
}

b8 fude_zoom_sheet_step_for(f64 _mm_per_point, f64 _min_points, b8 _inch, fude_zoom_sheet_step* _out) {
    memset(_out, 0, sizeof(*_out));
    if(!(_mm_per_point > 0.0) || !isfinite(_mm_per_point)) {
        return false;
    }
    if(!_inch) {
        // 1, 2 or 5 times a power of ten millimetres; numbered every centimetre, metre... as a ruler is.
        const f64 _min = _min_points * _mm_per_point;
        f64 _p10  = pow(10.0, floor(log10(_min)));
        f64 _step = _p10;
        u32 _series = 1u;
        if(_step < _min) { _step = 2.0 * _p10; _series = 2u; }
        if(_step < _min) { _step = 5.0 * _p10; _series = 5u; }
        if(_step < _min) { _step = 10.0 * _p10; _series = 1u; }
        _out->step_mm = _step;
        _out->major   = _series == 1u ? 10u : _series == 2u ? 5u : 2u;
        _out->mid     = _series == 1u ? 5u : 0u;
        const f64 _major_mm = _step * (f64)_out->major;
        _out->unit   = _major_mm >= 1000.0 - 1e-9 ? "m" : _major_mm >= 10.0 - 1e-9 ? "cm" : "mm";
        _out->per_mm = _major_mm >= 1000.0 - 1e-9 ? 1000.0 : _major_mm >= 10.0 - 1e-9 ? 10.0 : 1.0;
        return true;
    }
    // Inches: sixteenths, eighths, quarters or halves, the inches numbered; zoomed out, inches with each foot
    // numbered, then three and six inches, then feet (1, 2 or 5 of a power of ten of them).
    const f64 _min_in = _min_points * _mm_per_point / 25.4;
    if(_min_in <= 0.5) {
        f64 _s = 1.0 / 16.0;
        while(_s < _min_in) {
            _s *= 2.0;
        }
        _out->step_mm = _s * 25.4;
        _out->major   = (u32)llround(1.0 / _s);
        _out->halves  = true;
        _out->per_mm  = 25.4;
        _out->unit    = "in";
        return true;
    }
    _out->per_mm = 304.8;
    _out->unit   = "ft";
    if(_min_in <= 1.0)      { _out->step_mm = 25.4;  _out->major = 12u; _out->mid = 6u; return true; }
    if(_min_in <= 3.0)      { _out->step_mm = 76.2;  _out->major = 4u;  _out->mid = 2u; return true; }
    if(_min_in <= 6.0)      { _out->step_mm = 152.4; _out->major = 2u;  _out->mid = 0u; return true; }
    const f64 _min_ft = _min_in / 12.0;
    f64 _p10 = pow(10.0, floor(log10(_min_ft)));
    f64 _ft  = _p10;
    u32 _series = 1u;
    if(_ft < _min_ft) { _ft = 2.0 * _p10; _series = 2u; }
    if(_ft < _min_ft) { _ft = 5.0 * _p10; _series = 5u; }
    if(_ft < _min_ft) { _ft = 10.0 * _p10; _series = 1u; }
    _out->step_mm = _ft * 304.8;
    _out->major   = _series == 1u ? 10u : _series == 2u ? 5u : 2u;
    _out->mid     = _series == 1u ? 5u : 0u;
    return true;
}

f64 fude_zoom_sheet_tick(const fude_zoom_sheet_step* _s, u64 _i) {
    if(_s->major == 0u || _i % _s->major == 0u) {
        return 14.0;
    }
    if(_s->halves) {
        u32 _level = 1u;
        for(u32 _m = _s->major / 2u; _m > 0u && _i % _m != 0u; _m /= 2u) {
            _level++;
        }
        return fmax(14.0 - 2.5 * (f64)_level, 4.5);
    }
    return _s->mid != 0u && _i % _s->mid == 0u ? 10.0 : 6.0;
}

void fude_zoom_sheet_number(f64 _v, c8* _out, usize _size) {
    snprintf(_out, _size, "%.4f", _v);
    c8* _dot = strchr(_out, '.');
    if(_dot != NULL) {
        c8* _end = _out + strlen(_out) - 1;
        while(_end > _dot && *_end == '0') { *_end-- = 0; }
        if(_end == _dot) { *_end = 0; }
    }
    if(strcmp(_out, "-0") == 0) {
        snprintf(_out, _size, "0");
    }
}

// The way _d (its own units) goes on the screen, a unit long.
RDE_INTERNAL fude_zoom_v2 fude_zoom_sheet_way(fude_zoom_sim _all, fude_zoom_v2 _d) {
    const fude_zoom_v2 _w = { _all.a * _d.x - _all.b * _d.y, _all.b * _d.x + _all.a * _d.y };
    const f64 _l = hypot(_w.x, _w.y);
    return _l > 0.0 ? (fude_zoom_v2){ _w.x / _l, _w.y / _l } : (fude_zoom_v2){ 0.0, 0.0 };
}

// Indices _i of marks _step apart from 0 that fall in [_lo, _hi] (clamped to 0.._len): into *_first, *_last.
// False: none.
RDE_INTERNAL b8 fude_zoom_sheet_range(f64 _lo, f64 _hi, f64 _len, f64 _step, u64* _first, u64* _last) {
    _lo = fmax(_lo, 0.0);
    _hi = fmin(_hi, _len);
    if(!(_step > 0.0) || _hi < _lo) {
        return false;
    }
    const f64 _f = ceil(_lo / _step - 1e-9), _l = floor(_hi / _step + 1e-9);
    if(_l < _f || _f > 9e15) {
        return false;
    }
    *_first = (u64)_f;
    *_last  = (u64)fmin(_l, _f + (f64)FUDE_ZOOM_SHEET_MARKS);
    return true;
}

void fude_zoom_sheet_marks(const fude_zoom_sheet* _sheet, fude_zoom_sim _all, f64 _mm_per_unit, b8 _inch, fude_zoom_box _view,
                           rde_arr* _lines, rde_arr* _labels, fude_zoom_sheet_label* _unit) {
    fude_zoom_sheet_marks_stuck(_sheet, _all, _mm_per_unit, _inch, _view, _lines, _labels, _unit, NULL, NULL);
}

void fude_zoom_sheet_marks_stuck(const fude_zoom_sheet* _sheet, fude_zoom_sim _all, f64 _mm_per_unit, b8 _inch, fude_zoom_box _view,
                                 rde_arr* _lines, rde_arr* _labels, fude_zoom_sheet_label* _unit, fude_zoom_box* _bands, u32* _band_count) {
    if(_band_count != NULL) {
        *_band_count = 0u;
    }
    rde_arr_clear(_lines);
    rde_arr_clear(_labels);
    if(_unit != NULL) {
        memset(_unit, 0, sizeof(*_unit));
    }
    const f64 _k = fude_zoom_sim_scale(_all);   // points one of its own units
    if(!(_k > 0.0) || !(_mm_per_unit > 0.0) || !(_sheet->hw > 0.0) || !(_sheet->hh > 0.0)) {
        return;
    }
    const f64 _hw = _sheet->hw, _hh = _sheet->hh;
    const f64 _mm_pt = _mm_per_unit / _k;
    const fude_zoom_box _lv = fude_zoom_sim_box(fude_zoom_sim_inverse(_all), _view);   // the screen, in its own units
    const f64 _margin = 24.0 / _k;
    // The grid: lines across it from its bottom left corner, the numbered ones stronger (its sides the outline's).
    fude_zoom_sheet_step _g;
    if(_bands == NULL && (_sheet->flags & FUDE_ZOOM_SHEET_GRID) && fude_zoom_sheet_step_for(_mm_pt, FUDE_ZOOM_SHEET_GRID_PT, _inch, &_g)) {
        const f64 _su = _g.step_mm / _mm_per_unit;
        const f64 _y0 = fmax(_lv.min_y, -_hh), _y1 = fmin(_lv.max_y, _hh);
        const f64 _x0 = fmax(_lv.min_x, -_hw), _x1 = fmin(_lv.max_x, _hw);
        u64 _f, _l;
        if(_y1 > _y0 && fude_zoom_sheet_range(_lv.min_x + _hw, _lv.max_x + _hw, 2.0 * _hw, _su, &_f, &_l)) {
            for(u64 _i = _f; _i <= _l; _i++) {
                const f64 _x = -_hw + (f64)_i * _su;
                if(_i == 0u || _x >= _hw - 1e-9 * _hw) {
                    continue;
                }
                const fude_zoom_sheet_line _line = { fude_zoom_sim_apply(_all, (fude_zoom_v2){ _x, _y0 }), fude_zoom_sim_apply(_all, (fude_zoom_v2){ _x, _y1 }),
                                                     (u8)(_i % _g.major == 0u ? 2u : 1u) };
                rde_arr_add(_lines, (any)&_line);
            }
        }
        if(_x1 > _x0 && fude_zoom_sheet_range(_lv.min_y + _hh, _lv.max_y + _hh, 2.0 * _hh, _su, &_f, &_l)) {
            for(u64 _i = _f; _i <= _l; _i++) {
                const f64 _y = -_hh + (f64)_i * _su;
                if(_i == 0u || _y >= _hh - 1e-9 * _hh) {
                    continue;
                }
                const fude_zoom_sheet_line _line = { fude_zoom_sim_apply(_all, (fude_zoom_v2){ _x0, _y }), fude_zoom_sim_apply(_all, (fude_zoom_v2){ _x1, _y }),
                                                     (u8)(_i % _g.major == 0u ? 2u : 1u) };
                rde_arr_add(_lines, (any)&_line);
            }
        }
    }
    // Its rulers: too small on the screen to read, none.
    fude_zoom_sheet_step _t;
    if(2.0 * fmin(_hw, _hh) * _k < FUDE_ZOOM_SHEET_RULERS || !fude_zoom_sheet_step_for(_mm_pt, FUDE_ZOOM_SHEET_TICK_PT, _inch, &_t)) {
        return;
    }
    const f64 _su = _t.step_mm / _mm_per_unit;
    const b8  _numbered = 2.0 * fmin(_hw, _hh) * _k >= FUDE_ZOOM_SHEET_NUMBERS;
    u32 _every = 1u;   // numbers every so many numbered ticks (far enough apart to read)
    while(_t.step_mm * (f64)_t.major * (f64)_every / _mm_pt < FUDE_ZOOM_SHEET_LABEL_PT && _every < 1024u) {
        _every *= 2u;
    }
    // Bottom, top (along x), left, right (along y): where each starts, which way it goes, and which way is in.
    static const fude_zoom_v2 _from[4] = { { -1.0, -1.0 }, { -1.0, 1.0 }, { -1.0, -1.0 }, { 1.0, -1.0 } };
    static const fude_zoom_v2 _in[4]   = { { 0.0, 1.0 }, { 0.0, -1.0 }, { 1.0, 0.0 }, { -1.0, 0.0 } };
    // Stuck to the screen (zoomed in, its top or its left edge off it, the screen's edge on it): the top ruler along the
    // screen's top, the left one down its left, each on a band of its own (a sheet square to the screen only).
    const b8  _square = _bands != NULL && _all.a > 0.0 && fabs(_all.b) <= 1e-9 * _all.a;
    const f64 _band   = FUDE_ZOOM_SHEET_BAND_PT / _k;
    const b8  _stick_top  = _square && _hh > _lv.max_y && _lv.max_y > -_hh + 2.0 * _band && _lv.max_x > -_hw && _lv.min_x < _hw;
    const b8  _stick_left = _square && -_hw < _lv.min_x && _lv.min_x < _hw - 2.0 * _band && _lv.max_y > -_hh && _lv.min_y < _hh;
    if(_stick_top) {
        const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ fmax(-_hw, _lv.min_x), _lv.max_y - _band });
        const fude_zoom_v2 _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ fmin(_hw, _lv.max_x), _lv.max_y });
        _bands[(*_band_count)++] = (fude_zoom_box){ _a.x, _a.y, _b.x, _b.y };
    }
    if(_stick_left) {
        const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _lv.min_x, fmax(-_hh, _lv.min_y) });
        const fude_zoom_v2 _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _lv.min_x + _band, fmin(_hh, _lv.max_y) });
        _bands[(*_band_count)++] = (fude_zoom_box){ _a.x, _a.y, _b.x, _b.y };
    }
    for(u32 _side = 0; _side < 4u; _side++) {
        const b8  _along_x = _side < 2u;
        const b8  _stuck   = (_side == 1u && _stick_top) || (_side == 2u && _stick_left);
        if(_bands != NULL && !_stuck) {
            continue;   // (over the drawing: only what is stuck — the rest drawn with the sheet)
        }
        const u32 _side_first = (u32)rde_arr_length(_labels);   // (a stuck ruler's number nearest the screen's middle says its unit: its corner is off it)
        fude_zoom_v2 _o = { _from[_side].x * _hw, _from[_side].y * _hh };
        if(_side == 1u && _stick_top) {
            _o.y = _lv.max_y;
        } else if(_side == 2u && _stick_left) {
            _o.x = _lv.min_x;
        }
        const f64 _len = _along_x ? 2.0 * _hw : 2.0 * _hh;
        // On the screen at all: its line within the screen's box (a margin round it for the numbers).
        const f64 _across = _along_x ? _o.y : _o.x;
        if(_along_x ? (_across < _lv.min_y - _margin || _across > _lv.max_y + _margin) : (_across < _lv.min_x - _margin || _across > _lv.max_x + _margin)) {
            continue;
        }
        const f64 _lo = (_along_x ? _lv.min_x - _o.x : _lv.min_y - _o.y) - _margin;
        const f64 _hi = (_along_x ? _lv.max_x - _o.x : _lv.max_y - _o.y) + _margin;
        u64 _f, _l;
        if(!fude_zoom_sheet_range(_lo, _hi, _len, _su, &_f, &_l)) {
            continue;
        }
        const fude_zoom_v2 _inward = fude_zoom_sheet_way(_all, _in[_side]);
        for(u64 _i = _f; _i <= _l; _i++) {
            const f64 _u = (f64)_i * _su;
            const fude_zoom_v2 _p = fude_zoom_sim_apply(_all, _along_x ? (fude_zoom_v2){ _o.x + _u, _o.y } : (fude_zoom_v2){ _o.x, _o.y + _u });
            const f64 _h = fude_zoom_sheet_tick(&_t, _i);
            const fude_zoom_sheet_line _tick = { _p, { _p.x + _inward.x * _h, _p.y + _inward.y * _h }, 0u };
            rde_arr_add(_lines, (any)&_tick);
            // Numbered: not at its corners (where the rulers meet; its unit is by the first) — a stuck one's not where the
            // other stuck one crosses it.
            if(!_numbered || _i % _t.major != 0u || (_i / _t.major) % _every != 0u || _i == 0u || (_len - _u) * _k < 22.0) {
                continue;
            }
            if((_side == 1u && _stick_top && _stick_left && _p.x < _view.min_x + FUDE_ZOOM_SHEET_BAND_PT + 20.0) ||
               (_side == 2u && _stick_left && _stick_top && _p.y > _view.max_y - FUDE_ZOOM_SHEET_BAND_PT - 12.0)) {
                continue;
            }
            fude_zoom_sheet_label _label;
            _label.at = (fude_zoom_v2){ _tick.b.x + _inward.x * 3.0, _tick.b.y + _inward.y * 3.0 };
            _label.in = _inward;
            fude_zoom_sheet_number((f64)_i * _t.step_mm / _t.per_mm, _label.text, sizeof(_label.text));
            rde_arr_add(_labels, (any)&_label);
        }
        if(_stuck && (u32)rde_arr_length(_labels) > _side_first) {
            fude_zoom_sheet_label* _ls = (fude_zoom_sheet_label*)_labels->memory;
            const f64 _mid = _along_x ? (_view.min_x + _view.max_x) * 0.5 : (_view.min_y + _view.max_y) * 0.5;
            u32 _near = _side_first;
            for(u32 _k = _side_first; _k < (u32)rde_arr_length(_labels); _k++) {
                const f64 _d = fabs((_along_x ? _ls[_k].at.x : _ls[_k].at.y) - _mid), _dn = fabs((_along_x ? _ls[_near].at.x : _ls[_near].at.y) - _mid);
                _near = _d < _dn ? _k : _near;
            }
            const usize _len_t = strlen(_ls[_near].text);
            snprintf(_ls[_near].text + _len_t, sizeof(_ls[_near].text) - _len_t, " %s", _t.unit);
        }
    }
    if(_unit != NULL && _numbered && _bands == NULL) {
        const fude_zoom_v2 _c  = fude_zoom_sim_apply(_all, (fude_zoom_v2){ -_hw, -_hh });
        const fude_zoom_v2 _iu = fude_zoom_sheet_way(_all, (fude_zoom_v2){ 0.0, 1.0 }), _ir = fude_zoom_sheet_way(_all, (fude_zoom_v2){ 1.0, 0.0 });
        _unit->at = (fude_zoom_v2){ _c.x + (_iu.x + _ir.x) * 22.0, _c.y + (_iu.y + _ir.y) * 22.0 };
        _unit->in = (fude_zoom_v2){ 0.0, 0.0 };
        snprintf(_unit->text, sizeof(_unit->text), "%s", _t.unit);
    }
}

b8 fude_zoom_sheet_grid_snap(const fude_zoom_sheet* _sheet, f64 _mm_per_unit, f64 _mm_per_point, b8 _inch, fude_zoom_v2 _p, fude_zoom_v2* _out) {
    fude_zoom_sheet_step _g;
    if(!(_sheet->flags & FUDE_ZOOM_SHEET_GRID) || !(_mm_per_unit > 0.0) || !fude_zoom_sheet_step_for(_mm_per_point, FUDE_ZOOM_SHEET_GRID_PT, _inch, &_g)) {
        return false;
    }
    if(fabs(_p.x) > _sheet->hw || fabs(_p.y) > _sheet->hh) {
        return false;
    }
    const f64 _su = _g.step_mm / _mm_per_unit;
    _out->x = fmin(-_sheet->hw + round((_p.x + _sheet->hw) / _su) * _su, _sheet->hw);
    _out->y = fmin(-_sheet->hh + round((_p.y + _sheet->hh) / _su) * _su, _sheet->hh);
    return true;
}

f64 fude_zoom_sheet_round(f64 _mm, f64 _mm_per_point, b8 _inch) {
    fude_zoom_sheet_step _t;
    if(!fude_zoom_sheet_step_for(_mm_per_point, FUDE_ZOOM_SHEET_TICK_PT, _inch, &_t)) {
        return _mm;
    }
    return fmax(round(_mm / _t.step_mm), 1.0) * _t.step_mm;
}

static const f64 FUDE_ZOOM_PAPERS[FUDE_ZOOM_PAPER_COUNT][2] = {
    { 210.0, 297.0 }, { 297.0, 420.0 }, { 420.0, 594.0 }, { 594.0, 841.0 }, { 841.0, 1189.0 }, { 215.9, 279.4 }, { 279.4, 431.8 },
};

void fude_zoom_paper_size(u32 _paper, f64* _w_mm, f64* _h_mm) {
    const u32 _p = _paper < FUDE_ZOOM_PAPER_COUNT ? _paper : 0u;
    *_w_mm = FUDE_ZOOM_PAPERS[_p][0];
    *_h_mm = FUDE_ZOOM_PAPERS[_p][1];
}

u32 fude_zoom_paper_find(f64 _w_mm, f64 _h_mm) {
    for(u32 _i = 0; _i < FUDE_ZOOM_PAPER_COUNT; _i++) {
        const f64 _w = FUDE_ZOOM_PAPERS[_i][0], _h = FUDE_ZOOM_PAPERS[_i][1];
        if((fabs(_w_mm - _w) <= 0.5 && fabs(_h_mm - _h) <= 0.5) || (fabs(_w_mm - _h) <= 0.5 && fabs(_h_mm - _w) <= 0.5)) {
            return _i;
        }
    }
    return FUDE_ZOOM_NONE;
}
