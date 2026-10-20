// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/instrument_parts.h"
#include "drawing/base/redraw.h"

// ===========================================================================
// See instrument.h. The instruments, drawn (fude_zoom_instruments_render): each in the pile's order, the top
// one last.
//
// In order: a number for a graduation and a text centred on a point; the graduations (in millimetres or inches) and a
// protractor's degrees; then what is drawn its own way (the compass, a circle template, a curve's edge lit, an ellipse
// template, the French curve, the size tab, the ×, a stencil), and fude_zoom_instruments_render, which draws them all.
//
// Part of the instruments: instrument_parts.h holds what the modules share and says which module holds what. Names:
// fude_zoom_instrument_ for one instrument (a slot), fude_zoom_instruments_ for what works on them all or on what the
// hands and the pen do. Main thread only, as all drawing.
// ===========================================================================

// --- drawing them ---------------------------------------------------------------------------

// Returns _v in f32, for drawing.
RDE_INTERNAL rde_vec_2F fude_zoom_v2_f(fude_zoom_v2 _v) { return (rde_vec_2F){ (f32)_v.x, (f32)_v.y }; }

// A number for a graduation: no trailing zeros, at most four decimals.
RDE_INTERNAL void fude_zoom_instrument_number(f64 _v, c8* _out, usize _size) {
    rde_snprintf(_out, _size, "%.4f", _v);
    c8* _dot = rde_strchr(_out, '.');
    if(_dot != NULL) {
        c8* _end = _out + rde_strlen(_out) - 1;
        while(_end > _dot && *_end == '0') { *_end-- = 0; }
        if(_end == _dot) { *_end = 0; }
    }
    if(rde_strcmp(_out, "-0") == 0) { rde_snprintf(_out, _size, "0"); }
}

// Text centred on _at (upright on the screen, whatever the instrument's turn:
// the app's font is drawn from curves, which do not turn).
RDE_INTERNAL void fude_zoom_instrument_text(const fude_zoom_instruments_look* _look, const c8* _text, fude_zoom_v2 _at, f32 _px, rde_color _color) {
    if(_look->font == NULL) {
        return;
    }
    const f32 _w = fude_draw_text_width(_look->font, _look->font_px, _text, _px);
    fude_draw_text_placed(_look->font, _text, (rde_vec_3F){ (f32)_at.x - _w * 0.5f, (f32)_at.y - _px * 0.36f, 0.0f },
                          (rde_vec_2F){ _px / _look->font_px, _px / _look->font_px }, 0.0f, _color);
}

// The graduations along a straight edge: a millimetre's ticks (or a finer or
// coarser step, so they stay at least 7 points apart), every fifth longer,
// every tenth longer still and numbered — in centimetres while they are 10 mm
// apart, as on a real ruler.
RDE_INTERNAL void fude_zoom_instrument_ticks(const fude_zoom_instrument_edge* _e, const fude_zoom_instruments_look* _look, f64 _zero) {
    const fude_zoom_v2 _d   = { _e->b.x - _e->a.x, _e->b.y - _e->a.y };
    const f64          _len = rde_math_hypot_f64(_d.x, _d.y);
    const fude_zoom_v2 _dir = _e->zero_at_b ? fude_zoom_v2_norm((fude_zoom_v2){ -_d.x, -_d.y }) : fude_zoom_v2_norm(_d);
    const fude_zoom_v2 _o   = _e->zero_at_b ? _e->b : _e->a;
    const fude_zoom_v2 _in  = _e->zero_at_b ? (fude_zoom_v2){ _dir.y, -_dir.x } : (fude_zoom_v2){ -_dir.y, _dir.x };   // into the body
    // A thin plate (a ruler far out: as wide as it lies on the drawing): its marks
    // shorter, its numbers left off (they would meet the other edge's).
    const f64 _fit     = _e->room > 0.0 ? rde_math_min_f64(1.0, _e->room / FUDE_ZOOM_RULER_WIDTH) : 1.0;
    const b8  _numbers = !(_e->room > 0.0) || _e->room >= 44.0;
    if(!(_look->mm_per_point > 0.0)) {
        return;
    }
    // The step: 1, 2 or 5 times a power of ten millimetres.
    const f64 _min_mm = 7.0 * _look->mm_per_point;
    f64 _p10 = rde_math_pow_f64(10.0, rde_math_floor_f64(rde_math_log10_f64(_min_mm)));
    f64 _step = _p10;
    u32 _series = 1u;
    if(_step < _min_mm)       { _step = 2.0 * _p10; _series = 2u; }
    if(_step < _min_mm)       { _step = 5.0 * _p10; _series = 5u; }
    if(_step < _min_mm)       { _step = 10.0 * _p10; _series = 1u; _p10 *= 10.0; }
    const u32 _major = _series == 1u ? 10u : _series == 2u ? 5u : 2u;
    const f64 _major_mm = _step * (f64)_major;
    // The numbers' unit: metres for a major of a metre or more, centimetres for one of 10 mm or more, else millimetres.
    const c8* _unit = "mm";
    f64       _per  = 1.0;
    if(_major_mm >= 1000.0 - 1e-9)                         { _unit = "m";  _per = 1000.0; }
    else if(rde_math_abs_f64(_major_mm - 10.0) < 1e-9)                 { _unit = "cm"; _per = 10.0; }
    else if(_major_mm >= 10.0 && _major_mm < 1000.0)       { _unit = "cm"; _per = 10.0; }
    const f64 _step_pt = _step / _look->mm_per_point;
    const f64 _label_every = _major_mm / _look->mm_per_point < 30.0 ? 2.0 : 1.0;   // numbers at least 30 points apart
    const b8  _crowded     = _major_mm / _look->mm_per_point * _label_every < 48.0;
    const u32 _count = (u32)rde_math_min_f64(rde_math_floor_f64((_len - _zero - 4.0) / _step_pt), 4000.0);
    for(u32 _i = 0; _i <= _count; _i++) {
        const f64 _t = _zero + (f64)_i * _step_pt;
        const b8  _big = _i % _major == 0u;
        const b8  _mid = !_big && _series == 1u && _i % 5u == 0u;
        const f64 _h   = _big ? 14.0 : _mid ? 10.0 : 6.0;
        const fude_zoom_v2 _p = { _o.x + _dir.x * _t, _o.y + _dir.y * _t };
        const fude_zoom_v2 _q = { _p.x + _in.x * _h * _fit, _p.y + _in.y * _h * _fit };
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_p), fude_zoom_v2_f(_q), _look->tick, _big ? 1.3f : 1.0f);
        if(_numbers && _big && rde_math_fmod_f64((f64)(_i / _major), _label_every) < 0.5) {
            c8 _n[32];
            fude_zoom_instrument_number((f64)_i * _step / _per, _n, sizeof(_n));
            if(_i == 0u && _crowded) {
                rde_snprintf(_n, sizeof(_n), "%s", _unit);   // no room for it between the numbers: in the zero's place
            }
            const fude_zoom_v2 _at = { _p.x + _in.x * 21.0, _p.y + _in.y * 21.0 };
            fude_zoom_instrument_text(_look, _n, _at, 11.0f, _look->text);
        }
    }
    // Its unit, between the first two numbers.
    if(_numbers && !_crowded) {
        const f64 _mid_t = _zero + 0.5 * _step_pt * (f64)_major * _label_every;
        const fude_zoom_v2 _u = { _o.x + _dir.x * _mid_t + _in.x * 21.0, _o.y + _dir.y * _mid_t + _in.y * 21.0 };
        fude_zoom_instrument_text(_look, _unit, _u, 10.0f, _look->text);
    }
}

// The same in inches (the page's units in inches or feet): sixteenths, eighths,
// quarters or halves (as fine as stay 7 points apart), each finer one shorter,
// the inches numbered; zoomed out, inches with each foot numbered, then feet.
RDE_INTERNAL void fude_zoom_instrument_ticks_inch(const fude_zoom_instrument_edge* _e, const fude_zoom_instruments_look* _look, f64 _zero) {
    const fude_zoom_v2 _d   = { _e->b.x - _e->a.x, _e->b.y - _e->a.y };
    const f64          _len = rde_math_hypot_f64(_d.x, _d.y);
    const fude_zoom_v2 _dir = _e->zero_at_b ? fude_zoom_v2_norm((fude_zoom_v2){ -_d.x, -_d.y }) : fude_zoom_v2_norm(_d);
    const fude_zoom_v2 _o   = _e->zero_at_b ? _e->b : _e->a;
    const fude_zoom_v2 _in  = _e->zero_at_b ? (fude_zoom_v2){ _dir.y, -_dir.x } : (fude_zoom_v2){ -_dir.y, _dir.x };
    // A thin plate (a ruler far out: as wide as it lies on the drawing): its marks
    // shorter, its numbers left off (they would meet the other edge's).
    const f64 _fit     = _e->room > 0.0 ? rde_math_min_f64(1.0, _e->room / FUDE_ZOOM_RULER_WIDTH) : 1.0;
    const b8  _numbers = !(_e->room > 0.0) || _e->room >= 44.0;
    const f64 _min_in = 7.0 * _look->mm_per_point / 25.4;
    f64 _step_in;     // between ticks
    u32 _major;       // ticks between numbers
    f64 _per;         // a number's worth (inches)
    const c8* _unit;
    if(_min_in <= 0.5) {
        _step_in = 1.0 / 16.0;
        while(_step_in < _min_in) { _step_in *= 2.0; }
        _major = (u32)rde_math_round_to_i64_f64(1.0 / _step_in);
        _per   = 1.0;
        _unit  = "in";
    } else if(_min_in <= 1.0) {
        _step_in = 1.0;
        _major   = 12u;
        _per     = 12.0;
        _unit    = "ft";
    } else {
        // Feet, 1, 2 or 5 of a power of ten of them.
        const f64 _min_ft = _min_in / 12.0;
        const f64 _p10 = rde_math_pow_f64(10.0, rde_math_floor_f64(rde_math_log10_f64(_min_ft)));
        f64 _ft = _p10;
        if(_ft < _min_ft) { _ft = 2.0 * _p10; }
        if(_ft < _min_ft) { _ft = 5.0 * _p10; }
        if(_ft < _min_ft) { _ft = 10.0 * _p10; }
        _step_in = _ft * 12.0;
        _major   = 10u;
        _per     = 12.0;
        _unit    = "ft";
    }
    const f64 _step_pt = _step_in * 25.4 / _look->mm_per_point;
    const f64 _label_every = (f64)_major * _step_pt < 30.0 ? 2.0 : 1.0;
    const b8  _crowded     = (f64)_major * _step_pt * _label_every < 48.0;
    const u32 _count = (u32)rde_math_min_f64(rde_math_floor_f64((_len - _zero - 4.0) / _step_pt), 4000.0);
    for(u32 _i = 0; _i <= _count; _i++) {
        const f64 _t = _zero + (f64)_i * _step_pt;
        const b8  _big = _i % _major == 0u;
        // Shorter for each halving below the number's: the half, the quarter, the eighth, the sixteenth.
        f64 _h = 14.0;
        if(!_big) {
            u32 _level = 1u;
            for(u32 _m = _major / 2u; _m > 0u && _i % _m != 0u; _m /= 2u) {
                _level++;
            }
            _h = _major == 12u ? (_i % 6u == 0u ? 10.0 : 6.0) : rde_math_max_f64(14.0 - 2.5 * (f64)_level, 4.5);
        }
        const fude_zoom_v2 _p = { _o.x + _dir.x * _t, _o.y + _dir.y * _t };
        const fude_zoom_v2 _q = { _p.x + _in.x * _h * _fit, _p.y + _in.y * _h * _fit };
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_p), fude_zoom_v2_f(_q), _look->tick, _big ? 1.3f : 1.0f);
        if(_numbers && _big && rde_math_fmod_f64((f64)(_i / _major), _label_every) < 0.5) {
            c8 _n[32];
            fude_zoom_instrument_number((f64)_i * _step_in / _per, _n, sizeof(_n));
            if(_i == 0u && _crowded) {
                rde_snprintf(_n, sizeof(_n), "%s", _unit);   // no room for it between the numbers: in the zero's place
            }
            const fude_zoom_v2 _at = { _p.x + _in.x * 21.0, _p.y + _in.y * 21.0 };
            fude_zoom_instrument_text(_look, _n, _at, 11.0f, _look->text);
        }
    }
    if(_numbers && !_crowded) {
        const f64 _mid_t = _zero + 0.5 * _step_pt * (f64)_major * _label_every;   // between the first two numbers
        const fude_zoom_v2 _u = { _o.x + _dir.x * _mid_t + _in.x * 21.0, _o.y + _dir.y * _mid_t + _in.y * 21.0 };
        fude_zoom_instrument_text(_look, _unit, _u, 10.0f, _look->text);
    }
}

// The protractor's degrees: every one ticked, every fifth longer, every
// tenth numbered both ways round (as on a real one: 0 at either end).
RDE_INTERNAL void fude_zoom_instrument_degrees(const fude_zoom_instrument_edge* _e, const fude_zoom_instruments_look* _look) {
    // As many as its size has room for (it is as big as it is on the drawing): every degree
    // ticked and every tenth numbered both ways round; smaller, fewer; small, ticks only.
    const f64 _r     = _e->radius;
    const u32 _tick  = _r >= 110.0 ? 1u : (_r >= 60.0 ? 5u : 10u);
    const u32 _every = _r >= 140.0 ? 10u : (_r >= 90.0 ? 30u : 0u);
    const b8  _both  = _r >= 140.0;
    for(u32 _d = 0; _d <= 180u; _d++) {
        if(_d % _tick != 0u) {
            continue;
        }
        const f64 _t = _e->from + RDE_PI * (f64)_d / 180.0;
        const f64 _h = (_d % 10u == 0u ? 14.0 : _d % 5u == 0u ? 9.0 : 5.0) * rde_math_min_f64(1.0, _r / 110.0);
        const fude_zoom_v2 _p = { _e->centre.x + rde_math_cos_f64(_t) * _e->radius, _e->centre.y + rde_math_sin_f64(_t) * _e->radius };
        const fude_zoom_v2 _q = { _e->centre.x + rde_math_cos_f64(_t) * (_e->radius - _h), _e->centre.y + rde_math_sin_f64(_t) * (_e->radius - _h) };
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_p), fude_zoom_v2_f(_q), _look->tick, _d % 10u == 0u ? 1.3f : 1.0f);
        if(_every > 0u && _d % _every == 0u && !_both) {
            c8 _n[8];
            rde_snprintf(_n, sizeof(_n), "%u", _d);
            fude_zoom_instrument_text(_look, _n, (fude_zoom_v2){ _e->centre.x + rde_math_cos_f64(_t) * (_e->radius - 24.0), _e->centre.y + rde_math_sin_f64(_t) * (_e->radius - 24.0) }, 10.0f, _look->text);
            continue;
        }
        if(_both && _d % 10u == 0u) {
            c8 _n[8];
            rde_snprintf(_n, sizeof(_n), "%u", _d);
            fude_zoom_instrument_text(_look, _n, (fude_zoom_v2){ _e->centre.x + rde_math_cos_f64(_t) * (_e->radius - 26.0), _e->centre.y + rde_math_sin_f64(_t) * (_e->radius - 26.0) }, 10.0f, _look->text);
            rde_snprintf(_n, sizeof(_n), "%u", 180u - _d);
            fude_zoom_instrument_text(_look, _n, (fude_zoom_v2){ _e->centre.x + rde_math_cos_f64(_t) * (_e->radius - 44.0), _e->centre.y + rde_math_sin_f64(_t) * (_e->radius - 44.0) }, 9.0f, _look->tick);
        }
    }
    // Its centre: a small cross on the straight edge.
    const fude_zoom_v2 _c = _e->centre;
    const fude_zoom_v2 _u = { rde_math_cos_f64(_e->from + RDE_PI * 0.5), rde_math_sin_f64(_e->from + RDE_PI * 0.5) };
    rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_c), fude_zoom_v2_f((fude_zoom_v2){ _c.x + _u.x * 18.0, _c.y + _u.y * 18.0 }), _look->tick, 1.3f);
}

// The compass: the circle its pencil goes round (faint; the arc being drawn
// lit), its legs to the hinge, its needle and its pencil, its radius written.
RDE_INTERNAL void fude_zoom_instrument_render_compass(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instruments_look* _look) {
    const fude_zoom_instrument* _t = &_ins->tools[_slot];
    const f64 _r = _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R;
    rde_color _faint = _look->accent;
    _faint.a = 90u;
    const u32 _sides = 96u;
    for(u32 _i = 0; _i < _sides; _i++) {
        const f64 _a0 = 2.0 * RDE_PI * (f64)_i / (f64)_sides, _a1 = 2.0 * RDE_PI * (f64)(_i + 1u) / (f64)_sides;
        if(_i % 2u == 0u) {   // (dashed: the way round, not a line drawn)
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_t->at.x + rde_math_cos_f64(_a0) * _r), (f32)(_t->at.y + rde_math_sin_f64(_a0) * _r) },
                                         (rde_vec_2F){ (f32)(_t->at.x + rde_math_cos_f64(_a1) * _r), (f32)(_t->at.y + rde_math_sin_f64(_a1) * _r) }, _faint, 1.2f);
        }
    }
    if(_ins->ruled == (i32)_slot) {
        const f64 _from = _t->angle + rde_math_min_f64(_ins->ruled_from, _ins->ruled_now), _to = _t->angle + rde_math_max_f64(_ins->ruled_from, _ins->ruled_now);
        const u32 _k = (u32)rde_math_min_f64(rde_math_max_f64((_to - _from) / 0.05, 1.0), 400.0);
        for(u32 _i = 0; _i < _k; _i++) {
            const f64 _a0 = _from + (_to - _from) * (f64)_i / (f64)_k, _a1 = _from + (_to - _from) * (f64)(_i + 1u) / (f64)_k;
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_t->at.x + rde_math_cos_f64(_a0) * _r), (f32)(_t->at.y + rde_math_sin_f64(_a0) * _r) },
                                         (rde_vec_2F){ (f32)(_t->at.x + rde_math_cos_f64(_a1) * _r), (f32)(_t->at.y + rde_math_sin_f64(_a1) * _r) }, _look->accent, 2.4f);
        }
    }
    const fude_zoom_v2 _pencil = fude_zoom_instrument_place(_t, (fude_zoom_v2){ _r, 0.0 });
    const fude_zoom_v2 _hinge  = fude_zoom_instrument_compass_hinge(_ins, _slot);
    // The legs: a body's width, outlined.
    const fude_zoom_v2 _ends[2] = { _t->at, _pencil };
    for(u32 _l = 0; _l < 2u; _l++) {
        const fude_zoom_v2 _d = fude_zoom_v2_norm((fude_zoom_v2){ _hinge.x - _ends[_l].x, _hinge.y - _ends[_l].y });
        const fude_zoom_v2 _n = { -_d.y * 5.0, _d.x * 5.0 };
        const rde_vec_2F _q[4] = { { (f32)(_ends[_l].x + _n.x * 0.3), (f32)(_ends[_l].y + _n.y * 0.3) }, { (f32)(_hinge.x + _n.x), (f32)(_hinge.y + _n.y) },
                                   { (f32)(_hinge.x - _n.x), (f32)(_hinge.y - _n.y) }, { (f32)(_ends[_l].x - _n.x * 0.3), (f32)(_ends[_l].y - _n.y * 0.3) } };
        rde_rendering_2d_draw_polygon(_q, 4u, _look->body, NULL);
        for(u32 _i = 0; _i < 4u; _i++) {
            rde_rendering_2d_draw_line_1(_q[_i], _q[(_i + 1u) % 4u], _look->line, 1.0f);
        }
    }
    // The needle (a fine point) and the pencil (its lead).
    rde_rendering_2d_draw_circle_with_border(fude_zoom_v2_f(_t->at), 4.0f, 16u, _look->body, 1.5f, _look->line, NULL);
    rde_rendering_2d_draw_circle(fude_zoom_v2_f(_t->at), 1.5f, 12u, _look->line, NULL);
    rde_rendering_2d_draw_circle_with_border(fude_zoom_v2_f(_pencil), 4.5f, 16u, _look->accent, 1.2f, _look->line, NULL);
    // The hinge: what opens it.
    rde_rendering_2d_draw_circle_with_border(fude_zoom_v2_f(_hinge), (f32)(FUDE_ZOOM_INSTRUMENT_KNOB * fude_zoom_instrument_icons(_t)), 24u, _look->body, 1.2f,
                                             _ins->turning && _ins->held == (i32)_slot ? _look->accent : _look->line, NULL);
    rde_rendering_2d_draw_circle(fude_zoom_v2_f(_hinge), 3.0f, 12u, _look->line, NULL);
    // Its radius, written above the hinge (tapped: typed).
    if(_look->units != NULL && _look->font != NULL) {
        c8 _v[FUDE_ZOOM_UNITS_TEXT], _said[FUDE_ZOOM_UNITS_TEXT + 8u];
        fude_zoom_units_format(_t->stuck_mm > 0.0 ? _t->stuck_mm : _r * _look->mm_per_point, _look->units, _v, sizeof(_v));
        rde_snprintf(_said, sizeof(_said), "R %s", _v);
        const fude_zoom_v2 _at = fude_zoom_instrument_compass_label(_ins, _slot);
        const f32 _w = fude_draw_text_width(_look->font, _look->font_px, _said, 13.0f) + 16.0f;
        rde_rendering_2d_draw_rounded_rectangle_with_border(fude_zoom_v2_f(_at), (rde_vec_2F){ _w, 24.0f }, 1.0f, 8u, _look->body, 1.0f, _look->accent, NULL);
        fude_zoom_instrument_text(_look, _said, _at, 13.0f, _look->text);
    }
}

// A circle template: its plate round the hole (the drawing seen through the hole),
// a tick at each quarter of the hole's edge and its centre marked, the arc being
// drawn lit; its diameter written under it (tapped: typed).
RDE_INTERNAL void fude_zoom_instrument_render_circle(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instruments_look* _look) {
    const fude_zoom_instrument* _t = &_ins->tools[_slot];
    const f64 _r = _t->radius > 1.0 ? _t->radius : FUDE_ZOOM_COMPASS_R;
    const f64 _h = _r + fude_zoom_instrument_rim(_t);
    const u32 _sides = 96u;
    for(u32 _i = 0; _i < _sides; _i++) {
        // A ray out from the centre to the plate's square edge, both ways round each side.
        rde_vec_2F _q[4];
        for(u32 _k = 0; _k < 2u; _k++) {
            const f64 _a  = 2.0 * RDE_PI * (f64)(_i + _k) / (f64)_sides;
            const f64 _c  = rde_math_cos_f64(_a), _sn = rde_math_sin_f64(_a);
            const f64 _m  = rde_math_max_f64(rde_math_abs_f64(_c), rde_math_abs_f64(_sn));
            const fude_zoom_v2 _in  = fude_zoom_instrument_place(_t, (fude_zoom_v2){ _c * _r, _sn * _r });
            const fude_zoom_v2 _out = fude_zoom_instrument_place(_t, (fude_zoom_v2){ _c / _m * _h, _sn / _m * _h });
            _q[_k == 0u ? 0u : 1u] = fude_zoom_v2_f(_in);
            _q[_k == 0u ? 3u : 2u] = fude_zoom_v2_f(_out);
        }
        rde_rendering_2d_draw_polygon(_q, 4u, _look->body, NULL);
        rde_rendering_2d_draw_line_1(_q[0], _q[1], _look->line, 1.2f);
    }
    fude_zoom_v2 _sq[4];
    fude_zoom_instrument_outline(_ins, _slot, _sq, 4u);
    for(u32 _i = 0; _i < 4u; _i++) {
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_sq[_i]), fude_zoom_v2_f(_sq[(_i + 1u) % 4u]), _look->line, 1.2f);
    }
    for(u32 _q4 = 0; _q4 < 4u; _q4++) {
        const f64 _a = _t->angle + RDE_PI * 0.5 * (f64)_q4;
        const fude_zoom_v2 _d = { rde_math_cos_f64(_a), rde_math_sin_f64(_a) };
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f((fude_zoom_v2){ _t->at.x + _d.x * _r, _t->at.y + _d.y * _r }),
                                     fude_zoom_v2_f((fude_zoom_v2){ _t->at.x + _d.x * (_r + 14.0), _t->at.y + _d.y * (_r + 14.0) }), _look->tick, 1.3f);
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f((fude_zoom_v2){ _t->at.x + _d.x * 3.0, _t->at.y + _d.y * 3.0 }),
                                     fude_zoom_v2_f((fude_zoom_v2){ _t->at.x + _d.x * 9.0, _t->at.y + _d.y * 9.0 }), _look->tick, 1.0f);
    }
    if(_ins->ruled == (i32)_slot) {
        const f64 _from = _t->angle + rde_math_min_f64(_ins->ruled_from, _ins->ruled_now), _to = _t->angle + rde_math_max_f64(_ins->ruled_from, _ins->ruled_now);
        const u32 _k = (u32)rde_math_min_f64(rde_math_max_f64((_to - _from) / 0.05, 1.0), 400.0);
        for(u32 _i = 0; _i < _k; _i++) {
            const f64 _a0 = _from + (_to - _from) * (f64)_i / (f64)_k, _a1 = _from + (_to - _from) * (f64)(_i + 1u) / (f64)_k;
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_t->at.x + rde_math_cos_f64(_a0) * _r), (f32)(_t->at.y + rde_math_sin_f64(_a0) * _r) },
                                         (rde_vec_2F){ (f32)(_t->at.x + rde_math_cos_f64(_a1) * _r), (f32)(_t->at.y + rde_math_sin_f64(_a1) * _r) }, _look->accent, 2.4f);
        }
    }
    if(_look->units != NULL && _look->font != NULL) {
        c8 _v[FUDE_ZOOM_UNITS_TEXT], _said[FUDE_ZOOM_UNITS_TEXT + 8u];
        fude_zoom_units_format(2.0 * (_t->stuck_mm > 0.0 ? _t->stuck_mm : _r * _look->mm_per_point), _look->units, _v, sizeof(_v));
        rde_snprintf(_said, sizeof(_said), "\xC3\x98 %s", _v);   // (Ø: the font has no ⌀)
        const fude_zoom_v2 _at = fude_zoom_instrument_compass_label(_ins, _slot);
        const f32 _w = fude_draw_text_width(_look->font, _look->font_px, _said, 13.0f) + 16.0f;
        rde_rendering_2d_draw_rounded_rectangle_with_border(fude_zoom_v2_f(_at), (rde_vec_2F){ _w, 24.0f }, 1.0f, 8u, _look->body, 1.0f, _look->accent, NULL);
        fude_zoom_instrument_text(_look, _said, _at, 13.0f, _look->text);
    }
}

// The stretch of a curve the pen has drawn along, lit.
RDE_INTERNAL void fude_zoom_instrument_render_curve_lit(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_v2* _c, u32 _m, const fude_zoom_instruments_look* _look, b8 _open) {
    if(_ins->ruled != (i32)_slot || _m < 2u) {
        return;
    }
    const f64 _from = rde_math_min_f64(_ins->ruled_from, _ins->ruled_now), _to = rde_math_max_f64(_ins->ruled_from, _ins->ruled_now);
    const u32 _k = (u32)rde_math_min_f64(rde_math_max_f64((_to - _from) / 3.0, 1.0), 600.0);
    fude_zoom_v2 _dir, _prev = fude_zoom_instrument_curve_at(_c, _m, _from, &_dir, _open);
    for(u32 _i = 1; _i <= _k; _i++) {
        const fude_zoom_v2 _next = fude_zoom_instrument_curve_at(_c, _m, _from + (_to - _from) * (f64)_i / (f64)_k, &_dir, _open);
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_prev), fude_zoom_v2_f(_next), _look->accent, 2.4f);
        _prev = _next;
    }
}

// An ellipse template: its plate round the hole, ticks at its axes' ends, its centre
// marked; its width and height written under it (tapped: typed).
RDE_INTERNAL void fude_zoom_instrument_render_ellipse(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instruments_look* _look) {
    const fude_zoom_instrument* _t = &_ins->tools[_slot];
    f64 _rx, _ry, _hx, _hy;
    fude_zoom_instrument_ellipse(_t, &_rx, &_ry, &_hx, &_hy);
    const u32 _sides = 120u;
    for(u32 _i = 0; _i < _sides; _i++) {
        rde_vec_2F _q[4];
        for(u32 _k = 0; _k < 2u; _k++) {
            const f64 _a = 2.0 * RDE_PI * (f64)(_i + _k) / (f64)_sides;
            const f64 _c = rde_math_cos_f64(_a), _sn = rde_math_sin_f64(_a);
            const f64 _m = rde_math_max_f64(rde_math_abs_f64(_c) / _hx, rde_math_abs_f64(_sn) / _hy);
            _q[_k == 0u ? 0u : 1u] = fude_zoom_v2_f(fude_zoom_instrument_place(_t, (fude_zoom_v2){ _c * _rx, _sn * _ry }));
            _q[_k == 0u ? 3u : 2u] = fude_zoom_v2_f(fude_zoom_instrument_place(_t, (fude_zoom_v2){ _c / _m, _sn / _m }));
        }
        rde_rendering_2d_draw_polygon(_q, 4u, _look->body, NULL);
        rde_rendering_2d_draw_line_1(_q[0], _q[1], _look->line, 1.2f);
    }
    fude_zoom_v2 _sq[4];
    fude_zoom_instrument_outline(_ins, _slot, _sq, 4u);
    for(u32 _i = 0; _i < 4u; _i++) {
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_sq[_i]), fude_zoom_v2_f(_sq[(_i + 1u) % 4u]), _look->line, 1.2f);
    }
    const fude_zoom_v2 _ends[4][2] = { { { _rx, 0.0 }, { _rx + 14.0, 0.0 } }, { { -_rx, 0.0 }, { -_rx - 14.0, 0.0 } },
                                       { { 0.0, _ry }, { 0.0, _ry + 14.0 } }, { { 0.0, -_ry }, { 0.0, -_ry - 14.0 } } };
    for(u32 _i = 0; _i < 4u; _i++) {
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(fude_zoom_instrument_place(_t, _ends[_i][0])), fude_zoom_v2_f(fude_zoom_instrument_place(_t, _ends[_i][1])), _look->tick, 1.3f);
        const fude_zoom_v2 _d = { _ends[_i][1].x - _ends[_i][0].x, _ends[_i][1].y - _ends[_i][0].y };
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(fude_zoom_instrument_place(_t, (fude_zoom_v2){ _d.x * 0.2, _d.y * 0.2 })),
                                     fude_zoom_v2_f(fude_zoom_instrument_place(_t, (fude_zoom_v2){ _d.x * 0.6, _d.y * 0.6 })), _look->tick, 1.0f);
    }
    fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
    const u32 _m = fude_zoom_instrument_curve(_ins, _slot, _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
    fude_zoom_instrument_render_curve_lit(_ins, _slot, _c, _m, _look, false);
    if(_look->units != NULL && _look->font != NULL) {
        c8 _w[FUDE_ZOOM_UNITS_TEXT], _h[FUDE_ZOOM_UNITS_TEXT], _said[2u * FUDE_ZOOM_UNITS_TEXT + 8u];
        fude_zoom_units_format(2.0 * (_t->stuck_mm > 0.0 ? _t->stuck_mm : _rx * _look->mm_per_point), _look->units, _w, sizeof(_w));
        fude_zoom_units_format(2.0 * (_t->stuck_mm2 > 0.0 ? _t->stuck_mm2 : _ry * _look->mm_per_point), _look->units, _h, sizeof(_h));
        rde_snprintf(_said, sizeof(_said), "%s \xC3\x97 %s", _w, _h);
        const fude_zoom_v2 _at = fude_zoom_instrument_compass_label(_ins, _slot);
        const f32 _tw = fude_draw_text_width(_look->font, _look->font_px, _said, 13.0f) + 16.0f;
        rde_rendering_2d_draw_rounded_rectangle_with_border(fude_zoom_v2_f(_at), (rde_vec_2F){ _tw, 24.0f }, 1.0f, 8u, _look->body, 1.0f, _look->accent, NULL);
        fude_zoom_instrument_text(_look, _said, _at, 13.0f, _look->text);
    }
}

// A French curve: its plate (the curve its edge), the stretch drawn along lit.
RDE_INTERNAL void fude_zoom_instrument_render_french(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instruments_look* _look) {
    fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
    const u32 _m = fude_zoom_instrument_curve(_ins, _slot, _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
    if(_m < 3u) {
        return;
    }
    rde_arr _tri = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    const u32 _n = fude_zoom_fill_triangulate(_c, _m, &_tri);
    const fude_zoom_v2* _tp = (const fude_zoom_v2*)_tri.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        const rde_vec_2F _q[3] = { fude_zoom_v2_f(_tp[3u * _i]), fude_zoom_v2_f(_tp[3u * _i + 1u]), fude_zoom_v2_f(_tp[3u * _i + 2u]) };
        rde_rendering_2d_draw_polygon(_q, 3u, _look->body, NULL);
    }
    rde_arr_free(&_tri);
    for(u32 _i = 0; _i < _m; _i++) {
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_c[_i]), fude_zoom_v2_f(_c[(_i + 1u) % _m]), _look->line, 1.2f);
    }
    fude_zoom_instrument_render_curve_lit(_ins, _slot, _c, _m, _look, false);
}

// Its size tab: a small round grip past its end, two arrows along its way (lit while held).
RDE_INTERNAL void fude_zoom_instrument_render_grow(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instruments_look* _look) {
    fude_zoom_v2 _at;
    if(!fude_zoom_instrument_grow_at(_ins, _slot, &_at)) {
        return;
    }
    const fude_zoom_instrument* _t  = &_ins->tools[_slot];
    const b8                    _on = _ins->growing && _ins->held == (i32)_slot;
    const f64 _g = fude_zoom_instrument_icons(_t);
    rde_rendering_2d_draw_circle_with_border(fude_zoom_v2_f(_at), (f32)(FUDE_ZOOM_INSTRUMENT_GROW * _g), 20u, _on ? _look->accent : _look->body, 1.2f, _look->line, NULL);
    const rde_color    _c = _on ? _look->body : _look->tick;
    const fude_zoom_v2 _d = { rde_math_cos_f64(_t->angle), rde_math_sin_f64(_t->angle) }, _n = { -_d.y, _d.x };
    for(i32 _s = -1; _s <= 1; _s += 2) {
        const fude_zoom_v2 _tip  = { _at.x + _d.x * 6.0 * _g * _s, _at.y + _d.y * 6.0 * _g * _s };
        const fude_zoom_v2 _back = { _at.x + _d.x * 2.0 * _g * _s, _at.y + _d.y * 2.0 * _g * _s };
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_tip), fude_zoom_v2_f((fude_zoom_v2){ _back.x + _n.x * 3.5 * _g, _back.y + _n.y * 3.5 * _g }), _c, 1.5f);
        rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_tip), fude_zoom_v2_f((fude_zoom_v2){ _back.x - _n.x * 3.5 * _g, _back.y - _n.y * 3.5 * _g }), _c, 1.5f);
    }
    rde_rendering_2d_draw_line_1(fude_zoom_v2_f((fude_zoom_v2){ _at.x - _d.x * 6.0 * _g, _at.y - _d.y * 6.0 * _g }), fude_zoom_v2_f((fude_zoom_v2){ _at.x + _d.x * 6.0 * _g, _at.y + _d.y * 6.0 * _g }), _c, 1.5f);
}

// Its ×: a small round button on it (lit while pressed).
RDE_INTERNAL void fude_zoom_instrument_render_close(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instruments_look* _look) {
    const fude_zoom_v2 _x  = fude_zoom_instrument_close_at(_ins, _slot);
    const b8           _on = _ins->closing == (i32)_slot;
    const f64          _g  = fude_zoom_instrument_icons(&_ins->tools[_slot]);
    rde_rendering_2d_draw_circle_with_border(fude_zoom_v2_f(_x), (f32)(FUDE_ZOOM_INSTRUMENT_CLOSE * _g), 20u, _on ? _look->accent : _look->body, 1.2f, _look->line, NULL);
    const f32 _d = (f32)(4.0 * _g);
    const rde_color _c = _on ? _look->body : _look->tick;
    rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_x.x - _d, (f32)_x.y - _d }, (rde_vec_2F){ (f32)_x.x + _d, (f32)_x.y + _d }, _c, 1.5f);
    rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_x.x - _d, (f32)_x.y + _d }, (rde_vec_2F){ (f32)_x.x + _d, (f32)_x.y - _d }, _c, 1.5f);
}

// A stencil: its see-through plate (its lines' box), its lines — the one the pen goes along lit as it goes.
RDE_INTERNAL void fude_zoom_instrument_render_stencil(const fude_zoom_instruments* _ins, u32 _slot, const fude_zoom_instruments_look* _look) {
    fude_zoom_v2 _ol[4];
    const u32 _n = fude_zoom_instrument_outline(_ins, _slot, _ol, 4u);
    rde_vec_2F _pf[4];
    for(u32 _i = 0; _i < _n; _i++) {
        _pf[_i] = fude_zoom_v2_f(_ol[_i]);
    }
    if(_n == 4u) {
        rde_rendering_2d_draw_polygon(_pf, 4u, _look->body, NULL);
        for(u32 _i = 0; _i < 4u; _i++) {
            rde_rendering_2d_draw_line_1(_pf[_i], _pf[(_i + 1u) % 4u], _look->line, 1.0f);
        }
    }
    fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
    const u32 _ne = fude_zoom_instrument_edges(_ins, _slot, _e);
    for(u32 _j = 0; _j < _ne; _j++) {
        fude_zoom_v2 _c[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
        const u32 _m = fude_zoom_instrument_curve_of(_ins, _slot, &_e[_j], _c, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
        for(u32 _i = 0; _i + 1u < _m + (_e[_j].open ? 0u : 1u) && _m >= 2u; _i++) {
            rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_c[_i]), fude_zoom_v2_f(_c[(_i + 1u) % _m]), _look->tick, 2.0f);
        }
        if(_ins->ruled == (i32)_slot && _ins->ruled_edge == _j) {
            fude_zoom_instrument_render_curve_lit(_ins, _slot, _c, _m, _look, _e[_j].open);
        }
    }
}

// See the header. Each in the pile's order, the top one last: the compass, the templates, the French curve and the
// stencils their own way; the others their plate (a set square a band round a hollow middle), their graduations (in
// inches when the page is), the edges at work lit, their knob, their angle while turning, a corner gauge's round
// written, and their × and size tab.
void fude_zoom_instruments_render(const fude_zoom_instruments* _ins, const fude_zoom_instruments_look* _look, f64 _now) {
    for(u32 _o = 0; _o < FUDE_ZOOM_INSTRUMENT_MOST; _o++) {
        const u32 _k = _ins->order[_o];
        const fude_zoom_instrument* _t = &_ins->tools[_k];
        if(!_t->shown) {
            continue;
        }
        const u32 _kind = _t->kind;
        if(_kind == FUDE_ZOOM_INSTRUMENT_COMPASS) {
            fude_zoom_instrument_render_compass(_ins, _k, _look);
            fude_zoom_instrument_render_close(_ins, _k, _look);
            continue;
        }
        if(_kind == FUDE_ZOOM_INSTRUMENT_CIRCLE) {
            fude_zoom_instrument_render_circle(_ins, _k, _look);
            fude_zoom_instrument_render_close(_ins, _k, _look);
            continue;
        }
        if(_kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE || _kind == FUDE_ZOOM_INSTRUMENT_CURVE || _kind == FUDE_ZOOM_INSTRUMENT_STENCIL) {
            if(_kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE) {
                fude_zoom_instrument_render_ellipse(_ins, _k, _look);
            } else if(_kind == FUDE_ZOOM_INSTRUMENT_CURVE) {
                fude_zoom_instrument_render_french(_ins, _k, _look);
            } else {
                fude_zoom_instrument_render_stencil(_ins, _k, _look);
            }
            // Its knob (it turns), its ×, and its size tab (a French curve's, a stencil's; an ellipse template has
            // none).
            const fude_zoom_v2 _kn = fude_zoom_instrument_knob(_ins, _k);
            rde_rendering_2d_draw_circle_with_border(fude_zoom_v2_f(_kn), (f32)(FUDE_ZOOM_INSTRUMENT_KNOB * fude_zoom_instrument_icons(&_ins->tools[_k])), 24u, _look->body, 1.2f,
                                                     _ins->turning && _ins->held == (i32)_k ? _look->accent : _look->line, NULL);
            fude_zoom_instrument_render_close(_ins, _k, _look);
            fude_zoom_instrument_render_grow(_ins, _k, _look);
            continue;
        }
        fude_zoom_v2 _ol[FUDE_ZOOM_ARC_SIDES + 8u];
        const u32 _n = fude_zoom_instrument_outline(_ins, _k, _ol, FUDE_ZOOM_ARC_SIDES + 8u);
        rde_vec_2F _pf[FUDE_ZOOM_ARC_SIDES + 8u];
        for(u32 _i = 0; _i < _n; _i++) {
            _pf[_i] = fude_zoom_v2_f(_ol[_i]);
        }
        if(_kind == FUDE_ZOOM_INSTRUMENT_SQUARE_45 || _kind == FUDE_ZOOM_INSTRUMENT_SQUARE_30) {
            // A band round a hollow middle, as a real set square: three quads.
            fude_zoom_v2 _c[3], _h[3];
            fude_zoom_square_corners(_kind, fude_zoom_instrument_size(_t), _c);
            // The hollow: the triangle drawn in by the band's width all round,
            // which is the triangle shrunk about its incentre.
            const f64 _la = _c[1].x - _c[0].x, _lb = _c[2].y - _c[0].y;
            const f64 _ri = (_la + _lb - rde_math_hypot_f64(_la, _lb)) * 0.5;
            const fude_zoom_v2 _ic = { _c[0].x + _ri, _c[0].y + _ri };
            const f64 _shrink = rde_math_max_f64(0.0, (_ri - FUDE_ZOOM_SQUARE_HOLLOW) / _ri);
            for(u32 _i = 0; _i < 3u; _i++) {
                _h[_i] = fude_zoom_instrument_place(_t, (fude_zoom_v2){ _ic.x + (_c[_i].x - _ic.x) * _shrink, _ic.y + (_c[_i].y - _ic.y) * _shrink });
            }
            for(u32 _i = 0; _i < 3u; _i++) {
                const u32 _j = (_i + 1u) % 3u;
                const rde_vec_2F _q[4] = { _pf[_i], _pf[_j], fude_zoom_v2_f(_h[_j]), fude_zoom_v2_f(_h[_i]) };
                rde_rendering_2d_draw_polygon(_q, 4u, _look->body, NULL);
            }
            for(u32 _i = 0; _i < 3u; _i++) {
                rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_h[_i]), fude_zoom_v2_f(_h[(_i + 1u) % 3u]), _look->line, 1.0f);
            }
        } else {
            rde_rendering_2d_draw_polygon(_pf, _n, _look->body, NULL);
        }
        for(u32 _i = 0; _i < _n; _i++) {
            rde_rendering_2d_draw_line_1(_pf[_i], _pf[(_i + 1u) % _n], _look->line, 1.2f);
        }
        // The graduations.
        fude_zoom_instrument_edge _e[FUDE_ZOOM_INSTRUMENT_EDGES];
        const u32 _ne = fude_zoom_instrument_edges(_ins, _k, _e);
        for(u32 _i = 0; _i < _ne; _i++) {
            if(!_e[_i].graduated) {
                continue;
            }
            if(_e[_i].arc) {
                fude_zoom_instrument_degrees(&_e[_i], _look);
            } else {
                const f64 _zero = _kind == FUDE_ZOOM_INSTRUMENT_RULER ? FUDE_ZOOM_RULER_ZERO : 0.0;
                if(_look->units != NULL && (_look->units->unit == FUDE_ZOOM_UNIT_IN || _look->units->unit == FUDE_ZOOM_UNIT_FT) && _look->mm_per_point > 0.0) {
                    fude_zoom_instrument_ticks_inch(&_e[_i], _look, _zero);   // the page in inches: so are they
                } else {
                    fude_zoom_instrument_ticks(&_e[_i], _look, _zero);
                }
            }
        }
        // The edges at work: the one the pen draws along, the two lying together.
        for(u32 _i = 0; _i < _ne; _i++) {
            b8 _lit = (_ins->ruled == (i32)_k && _ins->ruled_edge == _i) ||
                      (_ins->docked >= 0 && ((_ins->held == (i32)_k && _ins->own_edge == _i) || (_ins->docked == (i32)_k && _ins->dock_edge == _i)));
            for(u32 _c = 0; _ins->docked >= 0 && _ins->held >= 0 && _c < _ins->contacts && !_lit; _c++) {
                _lit = (_ins->contact_slot[_c] == (i32)_k && _ins->contact_edge[_c] == _i);   // (the others it rests against too)
            }
            if(!_lit) {
                continue;
            }
            if(_e[_i].arc) {
                // (an arc part of the way round: a corner gauge's round, lit as it is)
                if(_e[_i].span < 2.0 * RDE_PI - 1e-9 && !_e[_i].graduated) {
                    for(u32 _s = 0; _s < 24u; _s++) {
                        const f64 _a0 = _e[_i].from + _e[_i].span * (f64)_s / 24.0, _a1 = _e[_i].from + _e[_i].span * (f64)(_s + 1u) / 24.0;
                        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_e[_i].centre.x + rde_math_cos_f64(_a0) * _e[_i].radius), (f32)(_e[_i].centre.y + rde_math_sin_f64(_a0) * _e[_i].radius) },
                                                     (rde_vec_2F){ (f32)(_e[_i].centre.x + rde_math_cos_f64(_a1) * _e[_i].radius), (f32)(_e[_i].centre.y + rde_math_sin_f64(_a1) * _e[_i].radius) }, _look->accent, 2.4f);
                    }
                }
                continue;
            }
            rde_rendering_2d_draw_line_1(fude_zoom_v2_f(_e[_i].a), fude_zoom_v2_f(_e[_i].b), _look->accent, 2.4f);
        }
        // Its knob.
        const fude_zoom_v2 _kn = fude_zoom_instrument_knob(_ins, _k);
        const f64          _kg = fude_zoom_instrument_icons(_t);
        rde_rendering_2d_draw_circle_with_border(fude_zoom_v2_f(_kn), (f32)(FUDE_ZOOM_INSTRUMENT_KNOB * _kg), 24u, _look->body, 1.2f,
                                                 _ins->turning && _ins->held == (i32)_k ? _look->accent : _look->line, NULL);
        const f64 _a = _t->angle;
        for(u32 _i = 0; _i < 2u; _i++) {
            // Two small curved arrows' worth: arcs either side, a turn's sign.
            const f64 _s0 = _a + (_i == 0u ? 0.35 : 0.35 + RDE_PI);
            for(u32 _s = 0; _s < 6u; _s++) {
                const f64 _u0 = _s0 + 0.32 * (f64)_s, _u1 = _s0 + 0.32 * (f64)(_s + 1u);
                rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_kn.x + rde_math_cos_f64(_u0) * 7.0 * _kg), (f32)(_kn.y + rde_math_sin_f64(_u0) * 7.0 * _kg) },
                                             (rde_vec_2F){ (f32)(_kn.x + rde_math_cos_f64(_u1) * 7.0 * _kg), (f32)(_kn.y + rde_math_sin_f64(_u1) * 7.0 * _kg) }, _look->tick, 1.2f);
            }
        }
        // Its angle, while it turns and a moment after.
        if(_ins->held == (i32)_k && _now - _ins->turned_at < 1.2 && _ins->turned_at > 0.0) {
            fude_redraw_after(1.2 - (_now - _ins->turned_at));   // (render on demand: it goes then, the hand still down)
            f64 _deg = rde_math_fmod_f64(_t->angle * 180.0 / RDE_PI + 360.0, 180.0);
            c8 _s[24];
            fude_zoom_instrument_number(rde_math_round_f64(_deg * 10.0) / 10.0, _s, sizeof(_s));
            rde_strcat(_s, sizeof(_s), "\xC2\xB0");
            const rde_vec_2F _c = fude_zoom_v2_f(_t->at);
            rde_rendering_2d_draw_rounded_rectangle_with_border(_c, (rde_vec_2F){ 64.0f, 26.0f }, 1.0f, 8u, _look->body, 1.0f, _look->accent, NULL);
            fude_zoom_instrument_text(_look, _s, _t->at, 13.0f, _look->text);
        }
        // A corner gauge's round, said under its plate (tapped: typed).
        if(_kind == FUDE_ZOOM_INSTRUMENT_CORNER && _look->units != NULL && _look->font != NULL) {
            c8 _v[FUDE_ZOOM_UNITS_TEXT], _said[FUDE_ZOOM_UNITS_TEXT + 8u];
            fude_zoom_units_format(_t->stuck_mm > 0.0 ? _t->stuck_mm : fude_zoom_corner_r(_t) * _look->mm_per_point, _look->units, _v, sizeof(_v));
            rde_snprintf(_said, sizeof(_said), "R %s", _v);
            const fude_zoom_v2 _at = fude_zoom_instrument_compass_label(_ins, _k);
            const f32 _w = fude_draw_text_width(_look->font, _look->font_px, _said, 13.0f) + 16.0f;
            rde_rendering_2d_draw_rounded_rectangle_with_border(fude_zoom_v2_f(_at), (rde_vec_2F){ _w, 24.0f }, 1.0f, 8u, _look->body, 1.0f, _look->accent, NULL);
            fude_zoom_instrument_text(_look, _said, _at, 13.0f, _look->text);
        }
        fude_zoom_instrument_render_close(_ins, _k, _look);
        fude_zoom_instrument_render_grow(_ins, _k, _look);
    }
}
