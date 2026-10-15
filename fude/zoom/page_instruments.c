// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, the instruments: snapping onto what is drawn (snap.h), lying on the drawing (a compass's needle stuck
// in it), and the toolbar's Instruments and Arrange choices.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// --- the instruments snapping onto what is drawn (snap.h) ---------------------------------------

// The instrument in hand, just moved: dragged, the first of its key points (instrument.h: corners, zero marks, a
// protractor's centre, a compass's needle) near an end, a corner or a crossing goes onto it, the whole moved so
// (lying along another's edge, it stays there instead); the compass's hinge in hand, its pencil does (its radius and
// its way so). The target shown as a ring. Each move from where the hand has it, so it lets go again as soon as it is
// moved off. Snapped, with the offset pad up, one hand slides it from the point it snapped to instead (below), and the
// point it snapped by is kept for the pad (anchor_*). _radius: how near counts, in screen points.
void fude_zoom_page_instrument_snap(fude_zoom_page* _page, f64 _radius) {
    fude_zoom_instruments* _ins = &_page->instruments;
    _page->snap_hit.kind = FUDE_ZOOM_SNAP_NONE;
    if(_ins->growing) {
        return;   // (made longer or bigger: where it lies is not moving)
    }
    // Snapped, its offset pad up: one hand on it slides it across or up from the point it
    // snapped to, as the pad's arrows do — the way the hand first goes, in their steps, the
    // snap kept (the pad's × lets it go: then it is moved as any).
    if(_ins->held >= 0 && _ins->hands == 1u && !_ins->turning && _page->anchor_tool == _ins->held && fude_zoom_page_anchor_alive(_page)) {
        const fude_zoom_v2 _from = _ins->grab_hand[0];
        if(!_page->anchor_sliding || _page->anchor_slide_grab.x != _from.x || _page->anchor_slide_grab.y != _from.y) {
            _page->anchor_sliding     = true;   // (just taken)
            _page->anchor_slide_grab  = _from;
            _page->anchor_slide_axis  = -1;
            _page->anchor_slide_mm[0] = _page->anchor_mm[0];
            _page->anchor_slide_mm[1] = _page->anchor_mm[1];
        }
        _ins->docked = -1;   // (not laid along another's edge on the way)
        const f64 _dx = _ins->hand_at[0].x - _from.x, _dy = _ins->hand_at[0].y - _from.y;
        if(_page->anchor_slide_axis < 0 && rde_math_hypot_f64(_dx, _dy) >= 8.0) {
            _page->anchor_slide_axis = rde_math_abs_f64(_dx) >= rde_math_abs_f64(_dy) ? 0 : 1;
        }
        _page->anchor_mm[0] = _page->anchor_slide_mm[0];
        _page->anchor_mm[1] = _page->anchor_slide_mm[1];
        if(_page->anchor_slide_axis >= 0) {
            const u32 _axis = (u32)_page->anchor_slide_axis;
            const f64 _mpp  = rde_math_max_f64(fude_zoom_page_mm_per_point(_page), 1e-300);
            const f64 _step = fude_zoom_page_anchor_step(_page);
            _page->anchor_mm[_axis] = rde_math_round_f64((_page->anchor_slide_mm[_axis] + (_axis == 0u ? _dx : _dy) * _mpp) / _step) * _step;
        }
        fude_zoom_page_anchor_apply(_page);   // (where the hand's free move put it, put back on its line)
        return;
    }
    _page->anchor_sliding = false;
    if(_ins->held < 0 || _ins->docked >= 0 || _ins->hands > 1u) {
        if(_ins->held >= 0 && _page->anchor_tool == _ins->held) {
            _page->anchor_tool = -1;
        }
        return;
    }
    fude_zoom_instrument* _t = &_ins->tools[_ins->held];
    if(_ins->turning) {
        if(_t->kind != FUDE_ZOOM_INSTRUMENT_COMPASS) {
            if(_page->anchor_tool == _ins->held) {
                _page->anchor_tool = -1;   // turned about its middle: the point it was snapped by has gone
            }
            return;   // (a turn rests at its 15° detents)
        }
        const rde_vec_2F _pencil = { (f32)(_t->at.x + rde_math_cos_f64(_t->angle) * _t->radius), (f32)(_t->at.y + rde_math_sin_f64(_t->angle) * _t->radius) };
        const rde_vec_2F _on     = fude_zoom_page_snapped_within(_page, _pencil, _radius);
        if(_page->snap_hit.kind != FUDE_ZOOM_SNAP_NONE) {
            _t->radius      = rde_math_max_f64(rde_math_hypot_f64((f64)_on.x - _t->at.x, (f64)_on.y - _t->at.y), 4.0);
            _t->angle       = rde_math_atan2_f64((f64)_on.y - _t->at.y, (f64)_on.x - _t->at.x);
        }
        return;
    }
    fude_zoom_v2 _keys[FUDE_ZOOM_INSTRUMENT_KEYS];
    const u32 _n = fude_zoom_instrument_keys(_ins, (u32)_ins->held, _keys);
    f64 _best = 1e300;
    fude_zoom_v2 _move = { 0.0, 0.0 };
    fude_zoom_snap _hit = { FUDE_ZOOM_SNAP_NONE, { 0.0, 0.0 }, { 0.0, 0.0 }, { 0.0, 0.0 }, FUDE_ZOOM_NONE, FUDE_ZOOM_SNAP_NO_KEY };
    u32 _key = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        const rde_vec_2F _on = fude_zoom_page_snapped_within(_page, (rde_vec_2F){ (f32)_keys[_i].x, (f32)_keys[_i].y }, _radius);
        if(_page->snap_hit.kind == FUDE_ZOOM_SNAP_NONE) {
            continue;
        }
        const f64 _d = rde_math_hypot_f64((f64)_on.x - _keys[_i].x, (f64)_on.y - _keys[_i].y);
        if(_d < _best) {
            _best = _d;
            _move = (fude_zoom_v2){ (f64)_on.x - _keys[_i].x, (f64)_on.y - _keys[_i].y };
            _hit  = _page->snap_hit;
            _key  = _i;
        }
    }
    _page->snap_hit = _hit;
    if(_hit.kind != FUDE_ZOOM_SNAP_NONE) {
        _t->at.x += _move.x;
        _t->at.y += _move.y;
        if(_t->kind != FUDE_ZOOM_INSTRUMENT_COMPASS) {
            _t->pin = (i32)_key;   // it lies on the drawing by that point
        }
        // Snapped by that point: the offset pad's to move it from there (as it is let go).
        const fude_zoom_v2 _on = { _keys[_key].x + _move.x, _keys[_key].y + _move.y };
        _page->anchor_tool   = _ins->held;
        _page->anchor_serial = _t->serial;
        _page->anchor_key    = _key;
        _page->anchor_frame = _page->scene.camera.frame;
        _page->anchor_point = fude_zoom_camera_to_frame(&_page->scene.camera, _on);
        _page->anchor_mm[0] = 0.0;
        _page->anchor_mm[1] = 0.0;
    } else {
        _t->pin = -1;   // lying on the drawing by its middle
        if(_page->anchor_tool == _ins->held) {
            _page->anchor_tool = -1;   // moved off it by hand
        }
    }
}

// --- the instruments lying on the drawing (a compass's needle stuck in it) ---------------------

// See page_parts.h.
fude_zoom_v2 fude_zoom_page_instrument_pin(const fude_zoom_page* _page, u32 _slot) {
    const fude_zoom_instrument* _t = &_page->instruments.tools[_slot];
    fude_zoom_v2 _keys[FUDE_ZOOM_INSTRUMENT_KEYS];
    if(_t->pin >= 0 && (u32)_t->pin < fude_zoom_instrument_keys(&_page->instruments, _slot, _keys)) {
        return _keys[_t->pin];
    }
    return _t->at;
}

// Where instrument _slot is on the screen now taken as where it lies on the drawing: its
// pinned point (instrument.h) in the camera's frame; its size a real length.
void fude_zoom_page_instrument_stick(fude_zoom_page* _page, u32 _slot) {
    fude_zoom_instrument* _t   = &_page->instruments.tools[_slot];
    fude_zoom_scene*      _s   = &_page->scene;
    const f64             _mpp = fude_zoom_page_mm_per_point(_page);
    if(!(_t->look_mpp > 0.0)) {
        _t->look_mpp = _mpp;   // (as it came out: its knob, × and tabs as they are now)
    }
    if(fude_zoom_instrument_round(_t->kind)) {
        _t->pin       = -1;   // its needle; its hole's centre
        _t->stuck_mm  = _t->radius * _mpp;
        _t->stuck_mm2 = (_t->kind == FUDE_ZOOM_INSTRUMENT_CORNER || _t->kind == FUDE_ZOOM_INSTRUMENT_CIRCLE ? fude_zoom_instrument_band(_t) : _t->radius2) * _mpp;
    } else {
        _t->stuck_mm  = fude_zoom_instrument_extent(&_page->instruments, _slot) * _mpp;   // how long (big) it is on the drawing
        if(_t->kind == FUDE_ZOOM_INSTRUMENT_RULER) {
            _t->stuck_mm2 = FUDE_ZOOM_INSTRUMENT_RULER_WIDTH * (_t->thick > 0.0 ? _t->thick : 1.0) * _mpp;   // and how wide
        }
    }
    _t->stuck_frame = _s->camera.frame;
    _t->stuck_point = fude_zoom_camera_to_frame(&_s->camera, fude_zoom_page_instrument_pin(_page, _slot));
    _t->stuck       = true;
}
// The compass's stick, another name for fude_zoom_page_instrument_stick (every instrument sticks so), as
// page_parts.h names fude_zoom_page_compass_follow.
#define fude_zoom_page_compass_stick fude_zoom_page_instrument_stick

// The edge the pen draws along held on the drawing: the one last drawn along (its corner where
// it begins: the keys start with them). Every instrument is as big as it is on the drawing all
// over (a ruler's band too: a line along either edge stays on it), so any point of it holds —
// this one keeps the drawn edge exact through the rounding of each zoom.
void fude_zoom_page_pin_ruled(fude_zoom_page* _page) {
    const fude_zoom_instruments* _ins = &_page->instruments;
    if(_ins->ruled < 0) {
        return;
    }
    fude_zoom_instrument* _t = &_page->instruments.tools[_ins->ruled];
    if(_t->kind != FUDE_ZOOM_INSTRUMENT_RULER || _t->pin == (i32)_ins->ruled_edge) {
        return;
    }
    _t->pin = (i32)_ins->ruled_edge;
    fude_zoom_page_instrument_stick(_page, (u32)_ins->ruled);
}

// Once a frame, each one: held, it is where the hand puts it (and lies there now); else its
// pinned point goes where that point of the drawing is on the screen (its turn and size
// kept), a compass's radius on the screen its real length at this zoom.
void fude_zoom_page_instruments_follow(fude_zoom_page* _page) {
    fude_zoom_instruments* _ins = &_page->instruments;
    for(u32 _k = 0; _k < FUDE_ZOOM_INSTRUMENT_MOST; _k++) {
        fude_zoom_instrument* _t = &_ins->tools[_k];
        if(!_t->shown) {
            continue;
        }
        if(_ins->held == (i32)_k || !_t->stuck || _t->stuck_frame >= fude_zoom_scene_frame_count(&_page->scene) || !(_t->stuck_mm > 0.0)) {
            fude_zoom_page_instrument_stick(_page, _k);
            continue;
        }
        // As big on the screen as it is on the drawing at this zoom (its knob, × and tabs with it)...
        const f64 _mpp = rde_math_max_f64(fude_zoom_page_mm_per_point(_page), 1e-300);
        _t->look = _t->look_mpp > 0.0 ? _t->look_mpp / _mpp : 1.0;
        if(fude_zoom_instrument_round(_t->kind)) {
            if(!(_t->stuck_mm2 > 0.0) && (_t->kind == FUDE_ZOOM_INSTRUMENT_CORNER || _t->kind == FUDE_ZOOM_INSTRUMENT_CIRCLE)) {
                _t->stuck_mm2 = fude_zoom_instrument_band(_t) * _mpp;   // (one kept before its plate's band was a real length)
            }
            _t->radius  = _t->stuck_mm / _mpp;
            _t->radius2 = _t->stuck_mm2 / _mpp;
        } else {
            _t->size = _t->stuck_mm / _mpp / fude_zoom_instrument_extent_unit(_t->kind);
            if(_t->kind == FUDE_ZOOM_INSTRUMENT_RULER) {
                if(!(_t->stuck_mm2 > 0.0)) {
                    _t->stuck_mm2 = FUDE_ZOOM_INSTRUMENT_RULER_WIDTH * (_t->thick > 0.0 ? _t->thick : 1.0) * _mpp;   // (one kept before its width was)
                }
                _t->thick = _t->stuck_mm2 / _mpp / FUDE_ZOOM_INSTRUMENT_RULER_WIDTH;
            }
        }
        // ...and its pinned point on that point of the drawing.
        const fude_zoom_v2 _want = fude_zoom_sim_apply(fude_zoom_page_frame_sim(_page, _t->stuck_frame), _t->stuck_point);
        const fude_zoom_v2 _now  = fude_zoom_page_instrument_pin(_page, _k);
        _t->at = (fude_zoom_v2){ _t->at.x + _want.x - _now.x, _t->at.y + _want.y - _now.y };
    }
}

// The numpad's answer for a round instrument's size (the label above its hinge tapped): a compass's or a corner
// gauge's radius, a circle template's diameter, an ellipse template's width and then (asked again) its height. The
// units typed in become the canvas's. Ignored when the instrument was put away or replaced meanwhile (its serial).
RDE_INTERNAL void fude_zoom_page_compass_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _mm) {
    RDE_UNUSED(_text);
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    _page->units.unit = _unit;
    fude_zoom_instrument* _t = &_page->instruments.tools[_page->compass_typing % FUDE_ZOOM_INSTRUMENT_MOST];
    if(_mm > 0.0 && _t->shown && _t->kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE && _t->serial == _page->compass_serial) {
        // An ellipse template's width, then its height.
        if(_page->compass_step == 0u) {
            _t->stuck_mm = _mm * 0.5;
            _page->compass_step = 1u;
            c8 _again[48];
            fude_zoom_page_length_text(_page, 2.0 * _t->stuck_mm2, _again, sizeof(_again));
            fude_zoom_numpad_open(&_page->numpad, _again, _page->units.unit, false, NULL, fude_zoom_page_compass_typed, _page);
            fude_zoom_numpad_ask(&_page->numpad, fude_text(FUDE_TEXT_ZOOM_ELLIPSE_HEIGHT));
        } else {
            _t->stuck_mm2 = _mm * 0.5;
        }
        fude_zoom_page_compass_follow(_page);
        return;
    }
    if(_mm > 0.0 && _t->shown && fude_zoom_instrument_round(_t->kind) && _t->serial == _page->compass_serial) {
        _t->stuck_mm = _t->kind == FUDE_ZOOM_INSTRUMENT_CIRCLE ? _mm * 0.5 : _mm;   // (a circle template's: its diameter typed)
        fude_zoom_page_compass_follow(_page);
    }
}

// The pen down on a round instrument's size label (the topmost first): its numpad, asking the radius, the diameter or
// the width. True: taken.
b8 fude_zoom_page_compass_label_down(fude_zoom_page* _page, rde_vec_2F _screen) {
    const fude_zoom_instruments* _ins = &_page->instruments;
    for(u32 _i = FUDE_ZOOM_INSTRUMENT_MOST; _i-- > 0;) {
        const u32 _k = _ins->order[_i];
        const fude_zoom_instrument* _t = &_ins->tools[_k];
        if(!_t->shown || !fude_zoom_instrument_round(_t->kind)) {
            continue;
        }
        const fude_zoom_v2 _at = fude_zoom_instrument_compass_label(_ins, _k);
        if(rde_math_abs_f64(_screen.x - _at.x) > 52.0 || rde_math_abs_f64(_screen.y - _at.y) > 14.0) {
            continue;
        }
        _page->compass_typing = _k;
        _page->compass_serial = _t->serial;
        const b8 _circle  = _t->kind == FUDE_ZOOM_INSTRUMENT_CIRCLE;
        const b8 _ellipse = _t->kind == FUDE_ZOOM_INSTRUMENT_ELLIPSE;
        _page->compass_step = 0u;
        c8 _text[48];
        fude_zoom_page_length_text(_page, _circle || _ellipse ? 2.0 * _t->stuck_mm : _t->stuck_mm, _text, sizeof(_text));
        fude_zoom_numpad_open(&_page->numpad, _text, _page->units.unit, false, NULL, fude_zoom_page_compass_typed, _page);
        fude_zoom_numpad_ask(&_page->numpad, fude_text(_ellipse ? FUDE_TEXT_ZOOM_ELLIPSE_WIDTH : _circle ? FUDE_TEXT_ZOOM_CIRCLE_DIAMETER : FUDE_TEXT_ZOOM_COMPASS_RADIUS));
        return true;
    }
    return false;
}

// The toolbar's Instruments: choice _index of FUDE_ZOOM_INSTRUMENT_CHOICES taken. Below FUDE_ZOOM_INSTRUMENT_COUNT
// another instrument comes out, stuck where it came, a template's or a corner gauge's sizes rounded to 5 mm steps
// (in inches, quarters); the Stencil opens My pieces instead. After them come the Tape and Dimension (the pen
// measures), the Saw, Guides and Trim, each chosen again to go back to drawing. Returns true to keep the panel open.
b8 fude_zoom_instruments_choose(fude_app* _app, u32 _index) {
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return false;
    }
    if(_index == FUDE_ZOOM_INSTRUMENT_STENCIL) {
        // My pieces' panel, a tap on one laying it as a stencil (opened, never toggled: a tablet's tap can come twice).
        if(!_page->pieces_open) {
            _page->pieces_scroll = 0.0;
            _page->piece_armed   = -1;
        }
        fude_zoom_page_pieces(_page);
        _page->pieces_open    = true;
        _page->pieces_stencil = true;
        return false;
    }
    if(_index < FUDE_ZOOM_INSTRUMENT_COUNT) {
        // Another one out (each put away by its own ×).
        const i32 _slot = fude_zoom_instrument_add(&_page->instruments, (FUDE_ZOOM_INSTRUMENT_)_index, fude_zoom_page_half(_page));
        if(_slot < 0) {
            fude_notice_show(fude_text(FUDE_TEXT_ZOOM_INSTRUMENTS_FULL));
        } else if(_index == FUDE_ZOOM_INSTRUMENT_COMPASS) {
            fude_zoom_page_compass_stick(_page, (u32)_slot);   // its needle in the drawing where it came, its radius a real length
        } else if(_index == FUDE_ZOOM_INSTRUMENT_ELLIPSE) {
            // Its hole round sizes to begin with.
            fude_zoom_page_compass_stick(_page, (u32)_slot);
            fude_zoom_instrument* _t = &_page->instruments.tools[_slot];
            const b8  _inch = _page->units.unit == FUDE_ZOOM_UNIT_IN || _page->units.unit == FUDE_ZOOM_UNIT_FT;
            const f64 _step = _inch ? 25.4 * 0.25 : 5.0;
            _t->stuck_mm  = 0.5 * rde_math_max_f64(_step, rde_math_round_f64(2.0 * _t->stuck_mm / _step) * _step);
            _t->stuck_mm2 = 0.5 * rde_math_max_f64(_step, rde_math_round_f64(2.0 * _t->stuck_mm2 / _step) * _step);
            fude_zoom_page_instruments_follow(_page);
        } else if(_index == FUDE_ZOOM_INSTRUMENT_CORNER) {
            // Its round a round size to begin with (5 mm steps; in inches, quarters).
            fude_zoom_page_compass_stick(_page, (u32)_slot);
            fude_zoom_instrument* _t = &_page->instruments.tools[_slot];
            const b8  _inch = _page->units.unit == FUDE_ZOOM_UNIT_IN || _page->units.unit == FUDE_ZOOM_UNIT_FT;
            const f64 _step = _inch ? 25.4 * 0.25 : 5.0;
            _t->stuck_mm = rde_math_max_f64(_step, rde_math_round_f64(_t->stuck_mm / _step) * _step);
            fude_zoom_page_instruments_follow(_page);
        } else if(_index == FUDE_ZOOM_INSTRUMENT_CIRCLE) {
            // Its hole a round size to begin with (5 mm steps; in inches, quarters).
            fude_zoom_page_compass_stick(_page, (u32)_slot);
            fude_zoom_instrument* _t = &_page->instruments.tools[_slot];
            const b8  _inch = _page->units.unit == FUDE_ZOOM_UNIT_IN || _page->units.unit == FUDE_ZOOM_UNIT_FT;
            const f64 _step = _inch ? 25.4 * 0.25 : 5.0;
            _t->stuck_mm = 0.5 * rde_math_max_f64(_step, rde_math_round_f64(2.0 * _t->stuck_mm / _step) * _step);
            fude_zoom_page_instruments_follow(_page);
        }
        return true;
    }
    if(_index == FUDE_ZOOM_INSTRUMENT_COUNT + 4u) {
        // Trim: the pen cuts lines' pieces away (chosen again: it draws again).
        const b8 _on = !_page->trim_tool;
        fude_zoom_page_pen_modes_off(_page);
        _page->trim_tool = _on;
        if(_on) {
            fude_zoom_page_pen_under(_page);
            fude_notice_show(fude_text(FUDE_TEXT_ZOOM_TRIM_HOW));
        }
        fude_toolbar_refresh(&_app->ui->bar);
        return false;
    }
    if(_index == FUDE_ZOOM_INSTRUMENT_COUNT + 3u) {
        // Guides: the pen draws them (chosen again: it draws again).
        const b8 _on = _page->shape_tool != FUDE_ZOOM_SHAPE_GUIDE;
        fude_zoom_page_pen_modes_off(_page);
        _page->shape_tool = _on ? (u8)FUDE_ZOOM_SHAPE_GUIDE : 0u;
        if(_on) {
            fude_zoom_page_pen_under(_page);
            fude_notice_show(fude_text(FUDE_TEXT_ZOOM_GUIDE_HOW));
        }
        fude_toolbar_refresh(&_app->ui->bar);
        return false;
    }
    if(_index == FUDE_ZOOM_INSTRUMENT_COUNT + 2u) {
        // The Saw: on (the pen in hand: it cuts at once) or off.
        _page->saw = !_page->saw;
        if(_page->saw) {
            fude_zoom_page_pen_under(_page);
        }
        fude_notice_show(fude_text(_page->saw ? FUDE_TEXT_ZOOM_SAW_ON : FUDE_TEXT_ZOOM_SAW_OFF));
        fude_toolbar_refresh(&_app->ui->bar);
        return true;
    }
    // The tape or a dimension: the pen measures (chosen again: it draws again).
    const u8 _mode = (u8)(_index - FUDE_ZOOM_INSTRUMENT_COUNT + 1u);
    const u8 _next = _page->measure_tool == _mode ? 0u : _mode;
    fude_zoom_page_pen_modes_off(_page);   // (a tape left showing too)
    _page->measure_tool = _next;
    if(_next != 0) {
        fude_zoom_page_pen_under(_page);
    }
    fude_toolbar_refresh(&_app->ui->bar);
    return false;
}

// The toolbar's Instruments: whether choice _index shows chosen (an instrument: any of its kind out; a pen mode: on).
b8 fude_zoom_instruments_chosen(const fude_app* _app, u32 _index) {
    RDE_UNUSED(_app);
    const fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return false;
    }
    if(_index == FUDE_ZOOM_INSTRUMENT_COUNT + 2u) {
        return _page->saw;
    }
    if(_index == FUDE_ZOOM_INSTRUMENT_COUNT + 3u) {
        return _page->shape_tool == FUDE_ZOOM_SHAPE_GUIDE;
    }
    if(_index == FUDE_ZOOM_INSTRUMENT_COUNT + 4u) {
        return _page->trim_tool;
    }
    return _index < FUDE_ZOOM_INSTRUMENT_COUNT ? fude_zoom_instruments_count(&_page->instruments, (FUDE_ZOOM_INSTRUMENT_)_index) > 0u :
                                                 _page->measure_tool == (u8)(_index - FUDE_ZOOM_INSTRUMENT_COUNT + 1u);
}

// The toolbar's Instruments, whether it shows chosen: while the pen measures, draws guides or trims (with the pen in
// hand), or saws. The Saw stays on under the Lasso and the Eraser (they select and erase as ever, and show chosen
// themselves); the pen back, it saws again.
b8 fude_zoom_instruments_selected(const fude_app* _app) {
    const fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return false;
    }
    const FUDE_TOOL_ _bar = _app->ui->bar.tool;
    return _page->measure_tool != 0 || ((_page->shape_tool == FUDE_ZOOM_SHAPE_GUIDE || _page->trim_tool) && _bar == FUDE_TOOL_DRAW) ||
           (_page->saw && _bar != FUDE_TOOL_ERASE && _bar != FUDE_TOOL_LASSO);
}

// The toolbar's Instruments' choices: the instruments in FUDE_ZOOM_INSTRUMENT_ order, then the Tape, Dimension, the
// Saw, Guides and Trim (product.h numbers them all: FUDE_ZOOM_TOOLKIT_).
const fude_extension_choice FUDE_ZOOM_INSTRUMENT_CHOICES[] = {
    { FUDE_TEXT_ZOOM_RULER,      FUDE_ICON_RULER,      fude_zoom_instruments_choose },
    { FUDE_TEXT_ZOOM_SQUARE_45,  FUDE_ICON_TRIANGLE,   fude_zoom_instruments_choose },
    { FUDE_TEXT_ZOOM_SQUARE_30,  FUDE_ICON_SQUARE_30,  fude_zoom_instruments_choose },
    { FUDE_TEXT_ZOOM_PROTRACTOR, FUDE_ICON_PROTRACTOR, fude_zoom_instruments_choose },
    { FUDE_TEXT_ZOOM_COMPASS,    FUDE_ICON_COMPASS,    fude_zoom_instruments_choose },
    { FUDE_TEXT_ZOOM_CIRCLE,     FUDE_ICON_CIRCLE,     fude_zoom_instruments_choose },
    { FUDE_TEXT_ZOOM_ELLIPSE,    FUDE_ICON_CIRCLE,     fude_zoom_instruments_choose },
    { FUDE_TEXT_ZOOM_FRENCH,     FUDE_ICON_GUIDED,     fude_zoom_instruments_choose },
    { FUDE_TEXT_ZOOM_CORNER,     FUDE_ICON_FULLSCREEN, fude_zoom_instruments_choose },
    { FUDE_TEXT_ZOOM_STENCIL,    FUDE_ICON_FOLDER,     fude_zoom_instruments_choose },   // (one of My pieces laid as one: piece.h)
    { FUDE_TEXT_ZOOM_TAPE,       FUDE_ICON_MEASURE,    fude_zoom_instruments_choose },
    { FUDE_TEXT_ZOOM_DIMENSION,  FUDE_ICON_LINE,       fude_zoom_instruments_choose },
    { FUDE_TEXT_ZOOM_SAW,        FUDE_ICON_CUT,        fude_zoom_instruments_choose },
    { FUDE_TEXT_ZOOM_GUIDE,      FUDE_ICON_GRIP_V,     fude_zoom_instruments_choose },   // (the pen draws guides: shape.h)
    { FUDE_TEXT_ZOOM_TRIM,       FUDE_ICON_CUT,        fude_zoom_instruments_choose },   // (the pen trims lines: trim.h)
};
// A compile-time check that the table has its FUDE_ZOOM_TOOLKIT_COUNT rows (the size is -1, which does
// not compile, when not).
typedef char fude_zoom_instrument_choices_counted[(sizeof(FUDE_ZOOM_INSTRUMENT_CHOICES) / sizeof(FUDE_ZOOM_INSTRUMENT_CHOICES[0]) == FUDE_ZOOM_TOOLKIT_COUNT) ? 1 : -1];

// The toolbar's Arrange: what the lasso holds lined up or spread out (select.h); a notice when too few things are
// held (two, or three to spread). The panel stays open for the next.
RDE_INTERNAL b8 fude_zoom_arrange_choose(fude_app* _app, u32 _index) {
    RDE_UNUSED(_app);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open) {
        return false;
    }
    if(!fude_zoom_select_arrange(&_page->selection, &_page->scene, (FUDE_ZOOM_ARRANGE_)_index) && (u32)rde_arr_length(&_page->selection.picks) < (_index >= 6u ? 3u : 2u)) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_ARRANGE_NEED));
    }
    return true;
}

// The toolbar's Arrange's choices, in FUDE_ZOOM_ARRANGE_ order.
const fude_extension_choice FUDE_ZOOM_ARRANGE_CHOICES[] = {
    { FUDE_TEXT_ZOOM_ALIGN_LEFT,    FUDE_ICON_ALIGN,        fude_zoom_arrange_choose },
    { FUDE_TEXT_ZOOM_ALIGN_CENTRE,  FUDE_ICON_ALIGN_CENTER, fude_zoom_arrange_choose },
    { FUDE_TEXT_ZOOM_ALIGN_RIGHT,   FUDE_ICON_ALIGN_RIGHT,  fude_zoom_arrange_choose },
    { FUDE_TEXT_ZOOM_ALIGN_TOP,     FUDE_ICON_ALIGN_TOP,    fude_zoom_arrange_choose },
    { FUDE_TEXT_ZOOM_ALIGN_MIDDLE,  FUDE_ICON_ALIGN_MIDDLE, fude_zoom_arrange_choose },
    { FUDE_TEXT_ZOOM_ALIGN_BOTTOM,  FUDE_ICON_ALIGN_BOTTOM, fude_zoom_arrange_choose },
    { FUDE_TEXT_ZOOM_SPREAD_ACROSS, FUDE_ICON_COLUMNS,      fude_zoom_arrange_choose },
    { FUDE_TEXT_ZOOM_SPREAD_DOWN,   FUDE_ICON_PAPER_LINES,  fude_zoom_arrange_choose },
};
// A compile-time check that the table has its FUDE_ZOOM_ARRANGE_COUNT rows (the size is -1, which does
// not compile, when not).
typedef char fude_zoom_arrange_choices_counted[(sizeof(FUDE_ZOOM_ARRANGE_CHOICES) / sizeof(FUDE_ZOOM_ARRANGE_CHOICES[0]) == FUDE_ZOOM_ARRANGE_COUNT) ? 1 : -1];
