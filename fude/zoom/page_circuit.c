// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"
#include "drawing/base/utf8.h"

// ===========================================================================
// Page, electronics (circuit.h): wires drawn pin to pin, the circuit tool's choices, the circuit drawn over the
// schematic while it plays (meters, tags, its scope), a part's Value card, and parts lined up as they move.
//
// The file is in four parts, each with a note at its head: wires and the circuit run (the Wire tool, the Circuit
// tool's choices, parts worked by hand while playing); the circuit drawn over the schematic; a part's value; and what
// the lasso holds lined up as it moves. Pins and wires are found on the screen, in every frame the tools reach
// (_page->editable), so a wire can join parts at different depths.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// Declared ahead: the align and the wire-routing helpers are defined below, near what else uses them.
RDE_INTERNAL void fude_zoom_page_align_targets(fude_zoom_page* _page, const u8* _mine);
RDE_INTERNAL u32 fude_zoom_page_wire_way(fude_zoom_page* _page, fude_zoom_v2 _a, u32 _ao, i32 _ap, fude_zoom_v2 _b, u32 _bo, i32 _bp, fude_zoom_v2* _out);

// --- electronics (circuit.h): wires drawn pin to pin, the circuit run ----------------------------------

// See page_parts.h. The part's pin nearest _screen within _reach (screen points) at any depth shown: its object, its
// pin, where it is (screen). False: none. It works out the editable frames again (_page->editable), which
// fude_zoom_page_wire_point_at then reads.
b8 fude_zoom_page_pin_at(fude_zoom_page* _page, rde_vec_2F _screen, f64 _reach, u32* _object, u32* _pin, fude_zoom_v2* _at) {
    rde_arr_clear(&_page->editable);
    fude_zoom_render_editable(&_page->renderer, &_page->scene, &_page->editable);
    const fude_zoom_scene* _s = &_page->scene;
    rde_arr _found = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    f64 _least = _reach;
    b8 _any = false;
    for(u32 _v = 0; _v < (u32)rde_arr_length(&_page->editable); _v++) {
        const fude_zoom_visible _vis = ((const fude_zoom_visible*)_page->editable.memory)[_v];
        const f64 _k = fude_zoom_sim_scale(_vis.to_screen);
        if(!(_k > 0.0)) {
            continue;
        }
        const fude_zoom_v2 _c = fude_zoom_sim_apply(fude_zoom_sim_inverse(_vis.to_screen), (fude_zoom_v2){ _screen.x, _screen.y });
        const f64 _r = _reach / _k;
        rde_arr_clear(&_found);
        fude_zoom_scene_query(_s, _vis.frame, (fude_zoom_box){ _c.x - _r, _c.y - _r, _c.x + _r, _c.y + _r }, &_found);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
            const u32 _o = ((const u32*)_found.memory)[_i];
            const fude_zoom_part* _part = fude_zoom_part_of(_s, _o);
            if(_part == NULL || !fude_zoom_scene_touchable(_s, fude_zoom_scene_object(_s, _o))) {
                continue;
            }
            for(u32 _k2 = 0; _k2 < _part->pin_count; _k2++) {
                fude_zoom_v2 _p;
                if(!fude_zoom_part_pin_at(_s, _o, _k2, &_p)) {
                    continue;
                }
                _p = fude_zoom_sim_apply(_vis.to_screen, _p);
                const f64 _d = rde_math_hypot_f64(_p.x - _screen.x, _p.y - _screen.y);
                if(_d <= _least) {
                    _least   = _d;
                    *_object = _o;
                    *_pin    = _k2;
                    *_at     = _p;
                    _any     = true;
                }
            }
        }
    }
    rde_arr_free(&_found);
    return _any;
}

// Writes the nearest point of a wire to _screen within _reach (screen). False: none. It reads the editable frames as
// fude_zoom_page_pin_at last worked them out.
RDE_INTERNAL b8 fude_zoom_page_wire_point_at(fude_zoom_page* _page, rde_vec_2F _screen, f64 _reach, fude_zoom_v2* _at) {
    const fude_zoom_scene* _s = &_page->scene;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _found = rde_arr_new(sizeof(u32), _heap), _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    f64 _least = _reach;
    b8 _any = false;
    for(u32 _v = 0; _v < (u32)rde_arr_length(&_page->editable); _v++) {
        const fude_zoom_visible _vis = ((const fude_zoom_visible*)_page->editable.memory)[_v];
        const f64 _k = fude_zoom_sim_scale(_vis.to_screen);
        if(!(_k > 0.0)) {
            continue;
        }
        const fude_zoom_v2 _c = fude_zoom_sim_apply(fude_zoom_sim_inverse(_vis.to_screen), (fude_zoom_v2){ _screen.x, _screen.y });
        const f64 _r = _reach / _k;
        rde_arr_clear(&_found);
        fude_zoom_scene_query(_s, _vis.frame, (fude_zoom_box){ _c.x - _r, _c.y - _r, _c.x + _r, _c.y + _r }, &_found);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
            const u32 _o = ((const u32*)_found.memory)[_i];
            const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _o);
            if(_ob->kind != FUDE_ZOOM_KIND_SHAPE || _ob->channels != FUDE_ZOOM_SHAPE_WIRE || !fude_zoom_scene_touchable(_s, _ob)) {
                continue;
            }
            b8 _closed;
            rde_arr_clear(&_pts);
            fude_zoom_scene_shape_outline(_s, _o, 1u, &_pts, &_closed);
            const fude_zoom_v2* _p = (const fude_zoom_v2*)_pts.memory;
            for(u32 _j = 0; _j + 1u < (u32)rde_arr_length(&_pts); _j++) {
                const fude_zoom_v2 _a = fude_zoom_sim_apply(_vis.to_screen, _p[_j]), _b = fude_zoom_sim_apply(_vis.to_screen, _p[_j + 1u]);
                const fude_zoom_v2 _pen = { _screen.x, _screen.y };
                const f64 _t  = rde_math_segment_closest_t_2d_f64(_pen, _a, _b);
                const fude_zoom_v2 _q = { _a.x + (_b.x - _a.x) * _t, _a.y + (_b.y - _a.y) * _t };   // (the nearest point)
                const f64 _dd = rde_math_point_segment_distance_2d_f64(_pen, _a, _b);
                if(_dd <= _least) {
                    _least = _dd;
                    *_at   = _q;
                    _any   = true;
                }
            }
        }
    }
    rde_arr_free(&_found);
    rde_arr_free(&_pts);
    return _any;
}

// Where a wire's end goes for the pen at _screen: a pin (its middle), else a wire. False: on neither (a wire joins two
// things or is not made: _at the pen's place then).
RDE_INTERNAL b8 fude_zoom_page_wire_end(fude_zoom_page* _page, rde_vec_2F _screen, u32* _object, i32* _pin, fude_zoom_v2* _at) {
    u32 _o, _p;
    if(fude_zoom_page_pin_at(_page, _screen, FUDE_ZOOM_PAGE_PIN_REACH, &_o, &_p, _at)) {
        *_object = _o;
        *_pin    = (i32)_p;
        return true;
    }
    *_object = FUDE_ZOOM_NONE;
    *_pin    = -1;
    if(fude_zoom_page_wire_point_at(_page, _screen, FUDE_ZOOM_PAGE_WIRE_REACH, _at)) {
        return true;
    }
    *_at = (fude_zoom_v2){ _screen.x, _screen.y };
    return false;
}

// See page_parts.h. A wire starts on a pin's middle or on a wire; pressed on nothing, none.
void fude_zoom_page_wire_down(fude_zoom_page* _page, rde_vec_2F _screen) {
    if(!fude_zoom_page_wire_end(_page, _screen, &_page->wire_from, &_page->wire_from_pin, &_page->wire_start)) {
        return;
    }
    _page->wiring  = true;
    _page->pen_now = _screen;
}

// Writes a wire's way on the screen from _a (part _ao's pin _ap, or none) to _b (part _bo's pin _bp, or none) into
// _out (6 at most), routed as fude_zoom_wire_route routes, its stubs the grid long of the part it starts at (or ends
// at). Returns how many points.
RDE_INTERNAL u32 fude_zoom_page_wire_way(fude_zoom_page* _page, fude_zoom_v2 _a, u32 _ao, i32 _ap, fude_zoom_v2 _b, u32 _bo, i32 _bp, fude_zoom_v2* _out) {
    const fude_zoom_scene* _s = &_page->scene;
    f64 _stub = 0.0;
    const u32 _part = _ao != FUDE_ZOOM_NONE ? _ao : _bo;
    if(_part != FUDE_ZOOM_NONE) {
        _stub = fude_zoom_part_unit(_s, _part) * fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, fude_zoom_scene_object(_s, _part)->frame));
    }
    // The screen is Y up as the canvas: the pins' sides as they face on it.
    const u8 _sa = _ao != FUDE_ZOOM_NONE ? fude_zoom_part_side_at(_s, _ao, (u32)_ap) : 255u;
    const u8 _sb = _bo != FUDE_ZOOM_NONE ? fude_zoom_part_side_at(_s, _bo, (u32)_bp) : 255u;
    return fude_zoom_wire_route(_a, _sa, _b, _sb, _stub, _out);
}

// See page_parts.h. A wire joins two things: let go on nothing, under 6 points from where it began, or back on the pin
// it began at, there is none. It is made in the frame of the part it starts at (or ends at), else where the pen draws,
// 0.6 of the brush wide, and is one undo step.
void fude_zoom_page_wire_up(fude_zoom_page* _page) {
    if(!_page->wiring) {
        return;
    }
    _page->wiring = false;
    fude_zoom_scene* _s = &_page->scene;
    u32 _to;
    i32 _to_pin;
    fude_zoom_v2 _end;
    const b8 _joined = fude_zoom_page_wire_end(_page, _page->pen_now, &_to, &_to_pin, &_end);
    if(!_joined || rde_math_hypot_f64(_end.x - _page->wire_start.x, _end.y - _page->wire_start.y) < 6.0 || (_to != FUDE_ZOOM_NONE && _to == _page->wire_from && _to_pin == _page->wire_from_pin)) {
        return;   // A tap, or let go on nothing: no wire, since a wire joins two things.
    }
    // Into the frame of the part it starts at (or ends at); else where the pen draws.
    u32 _frame = _page->wire_from != FUDE_ZOOM_NONE ? fude_zoom_scene_object(_s, _page->wire_from)->frame :
                 (_to != FUDE_ZOOM_NONE ? fude_zoom_scene_object(_s, _to)->frame : FUDE_ZOOM_NONE);
    if(_frame == FUDE_ZOOM_NONE) {
        const fude_zoom_v2 _at   = fude_zoom_camera_to_frame(&_s->camera, _page->wire_start);
        const f64          _near = 4.0 / _s->camera.z;
        _frame = fude_zoom_camera_drawing_frame(_s, (fude_zoom_box){ _at.x - _near, _at.y - _near, _at.x + _near, _at.y + _near });
    }
    const fude_zoom_sim _to_screen = fude_zoom_page_frame_sim(_page, _frame);
    const fude_zoom_sim _back = fude_zoom_sim_inverse(_to_screen);
    // Its ends exactly at their pins (the frame's units), leaving and coming in as they face there.
    fude_zoom_v2 _a = fude_zoom_sim_apply(_back, _page->wire_start), _b = fude_zoom_sim_apply(_back, _end);
    u8 _sa = 255u, _sb = 255u;
    f64 _stub_a = 0.0, _stub_b = 0.0;
    const b8 _ha = _page->wire_from != FUDE_ZOOM_NONE && fude_zoom_part_pin_in(_s, _page->wire_from, (u32)_page->wire_from_pin, _frame, &_a, &_sa, &_stub_a);
    const b8 _hb = _to != FUDE_ZOOM_NONE && fude_zoom_part_pin_in(_s, _to, (u32)_to_pin, _frame, &_b, &_sb, &_stub_b);
    fude_zoom_v2 _r[6];
    const u32 _m = fude_zoom_wire_route(_a, _ha ? _sa : 255u, _b, _hb ? _sb : 255u, _ha ? _stub_a : _stub_b, _r);
    const f64 _k = fude_zoom_sim_scale(_to_screen);
    const f32 _radius = (f32)rde_math_max_f64((f64)fude_zoom_page_brush_radius(_page, _frame, _k) * 0.6, 0.6 / _k);
    const u32 _w = fude_zoom_wire_add(_s, _frame, _r, _m, _page->wire_from, _page->wire_from_pin, _to, _to_pin, _page->app->ink->color, _radius);
    fude_zoom_history_push(_s, _frame, fude_zoom_scene_object(_s, _w)->box, NULL, 0, &_w, 1);
}

// See page_parts.h. The Circuit tool: Wire (the pen draws wires; chosen again, it draws again), Play (what the lasso
// holds), Parts (the library's circuits' tabs). Always returns false: the panel closes.
b8 fude_zoom_circuit_choose(fude_app* _app, u32 _index) {
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return false;
    }
    switch(_index) {
    case 0: {   // The pen draws wires (chosen again: it draws again).
        const b8 _on = !_page->wire_tool;
        fude_zoom_page_pen_modes_off(_page);
        _page->wire_tool = _on;
        if(_on) {
            fude_zoom_page_pen_under(_page);
            fude_notice_show(fude_text(FUDE_TEXT_ZOOM_CIRCUIT_WIRE_HOW));
        }
        break;
    }
    case 1:
        _page->wire_tool = false;
        fude_zoom_page_play_selection_now(_page);
        break;
    case 2:
        fude_zoom_page_library_open_tabs(_page, FUDE_ZOOM_PAGE_LIBRARY_CIRCUITS);
        break;
    default:
        break;
    }
    fude_toolbar_refresh(&_app->ui->bar);
    return false;
}

// See page_parts.h.
b8 fude_zoom_circuit_chosen(const fude_app* _app, u32 _index) {
    RDE_UNUSED(_app);
    const fude_zoom_page* _page = fude_zoom_page_the;
    return _page != NULL && ((_index == 0u && _page->wire_tool) || (_index == 1u && _page->play_on && _page->play_circuit));
}

// See page_parts.h.
b8 fude_zoom_circuit_selected(const fude_app* _app) {
    RDE_UNUSED(_app);
    return fude_zoom_page_the != NULL && (fude_zoom_page_the->wire_tool || (fude_zoom_page_the->play_on && fude_zoom_page_the->play_circuit));
}

// See page_parts.h: the Circuit tool's choices, in fude_zoom_circuit_choose's order.
const fude_extension_choice FUDE_ZOOM_CIRCUIT_CHOICES[] = {
    { FUDE_TEXT_ZOOM_CIRCUIT_WIRE,  FUDE_ICON_CONNECTOR,   fude_zoom_circuit_choose },
    { FUDE_TEXT_ZOOM_PLAY,          FUDE_ICON_PLAY,        fude_zoom_circuit_choose },
    { FUDE_TEXT_ZOOM_INSERT_PARTS,  FUDE_ICON_SIGNAL_HIGH, fude_zoom_circuit_choose },
};
// A compile-time check that the table has its FUDE_ZOOM_CIRCUIT_CHOICES_N rows (the size is -1, which does
// not compile, when not).
typedef char fude_zoom_circuit_choices_counted[(sizeof(FUDE_ZOOM_CIRCUIT_CHOICES) / sizeof(FUDE_ZOOM_CIRCUIT_CHOICES[0]) == FUDE_ZOOM_CIRCUIT_CHOICES_N) ? 1 : -1];

// See page_parts.h. The circuit's part an object is, its index in the circuit's parts (FUDE_ZOOM_NONE: none): a linear
// search.
u32 fude_zoom_page_circuit_part(const fude_zoom_page* _page, u32 _object) {
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_page->circuit.parts.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->circuit.parts); _i++) {
        if(_p[_i].object == _object) {
            return _i;
        }
    }
    return FUDE_ZOOM_NONE;
}

// Shows the notice for what a tap made circuit part _part say: _said, in no language (fude_zoom_circuit_tap), with the
// word that goes with it (fude_zoom_circuit_tap_word, at board pin _pin) in the person's language: "Switch on",
// "D13: high", "100 lux". Nothing when there is nothing to say.
RDE_INTERNAL void fude_zoom_page_tap_notice(const fude_zoom_page* _page, u32 _part, i32 _pin, const c8* _said) {
    const fude_zoom_circuit_part* _p = &((const fude_zoom_circuit_part*)_page->circuit.parts.memory)[_part];
    c8 _line[160];
    switch(fude_zoom_circuit_tap_word(_p, _pin)) {
    case FUDE_ZOOM_TAP_ON:    rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_ZOOM_SWITCH_ON)); break;
    case FUDE_ZOOM_TAP_OFF:   rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_ZOOM_SWITCH_OFF)); break;
    case FUDE_ZOOM_TAP_HIGH:  FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_PIN_HIGH, FUDE_TS(_said)); break;
    case FUDE_ZOOM_TAP_LOW:   FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_PIN_LOW, FUDE_TS(_said)); break;
    case FUDE_ZOOM_TAP_BLINK: FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_PIN_BLINK, FUDE_TS(_said)); break;
    case FUDE_ZOOM_TAP_INPUT: FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_PIN_INPUT, FUDE_TS(_said)); break;
    case FUDE_ZOOM_TAP_LUX:   FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_LUX, FUDE_TS(_said)); break;
    default:                  rde_snprintf(_line, sizeof(_line), "%s", _said); break;
    }
    if(_line[0] != 0) {
        fude_notice_show(_line);
    }
}

// See page_parts.h. Running, a press on a part that is worked by hand (a switch, a button, a logic input, a pot, a
// light or heat sensor, an encoder, a board's pin, an oscilloscope's knob): worked, and what it does now said (a
// button's press is not: it goes up as the hand does). A part a mechanism works (its shaft's or its beam's) is left to
// the mechanism. A part is taken within 1.1 of its half width and 1.3 of its half height. True: it was one.
b8 fude_zoom_page_circuit_press(fude_zoom_page* _page, rde_vec_2F _screen) {
    const fude_zoom_scene* _s = &_page->scene;
    // A board's pin first (they are small), then the part under the pen.
    u32 _o, _pin;
    fude_zoom_v2 _at;
    c8 _say[64];
    if(fude_zoom_page_pin_at(_page, _screen, FUDE_ZOOM_PAGE_PIN_REACH, &_o, &_pin, &_at)) {
        const fude_zoom_part* _part = fude_zoom_part_of(_s, _o);
        const u32 _i = fude_zoom_page_circuit_part(_page, _o);
        if(_part != NULL && _part->model == FUDE_ZOOM_MODEL_BOARD && _i != FUDE_ZOOM_NONE && fude_zoom_circuit_tap(&_page->circuit, _i, (i32)_pin, _say, sizeof(_say))) {
            fude_zoom_page_tap_notice(_page, _i, (i32)_pin, _say);
            return true;
        }
    }
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_page->circuit.parts.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->circuit.parts); _i++) {
        const u8 _m = _p[_i].part->model;
        if(_m != FUDE_ZOOM_MODEL_SWITCH && _m != FUDE_ZOOM_MODEL_BUTTON && _m != FUDE_ZOOM_MODEL_LOGIC_IN && _m != FUDE_ZOOM_MODEL_POT &&
           _m != FUDE_ZOOM_MODEL_SPDT && _m != FUDE_ZOOM_MODEL_DPDT && _m != FUDE_ZOOM_MODEL_LDR && _m != FUDE_ZOOM_MODEL_THERMISTOR &&
           _m != FUDE_ZOOM_MODEL_ENCODER && _m != FUDE_ZOOM_MODEL_SLOT && _m != FUDE_ZOOM_MODEL_PHOTODIODE && _m != FUDE_ZOOM_MODEL_PHOTOTRANSISTOR &&
           _m != FUDE_ZOOM_MODEL_SCOPE) {
            continue;
        }
        if(_page->play_coupled && fude_zoom_coupling_works(&_page->coupling, _i)) {
            continue;   // Its shaft's or its beam's: the hand goes to the crank on it.
        }
        const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _p[_i].object);
        f64 _n[3];
        fude_zoom_scene_shape_numbers(_s, _p[_i].object, _n, 3u);
        const fude_zoom_sim _all = fude_zoom_sim_compose(fude_zoom_page_frame_sim(_page, _ob->frame), fude_zoom_object_sim(_ob));
        const fude_zoom_v2 _l = fude_zoom_sim_apply(fude_zoom_sim_inverse(_all), (fude_zoom_v2){ _screen.x, _screen.y });
        if(_m == FUDE_ZOOM_MODEL_SCOPE) {
            // An oscilloscope: a knob taken (dragged, turned as the hand goes; let go where it was taken, a step: its
            // right half clockwise, its left back); elsewhere on it, nothing.
            i32 _way = 0;
            const u32 _knob = _n[1] > 0.0 && _n[2] > 0.0 ? fude_zoom_scope_knob_at(_l.x / _n[1], _l.y / _n[2], &_way) : FUDE_ZOOM_NONE;
            if(_knob != FUDE_ZOOM_NONE) {
                f64 _ku, _kv, _kr;
                fude_zoom_scope_knob(_knob, &_ku, &_kv, &_kr);
                _page->knob_part    = _i;
                _page->knob         = _knob;
                _page->knob_way     = _way;
                _page->knob_from    = _screen;
                _page->knob_step    = (f32)rde_math_max_f64(2.0 * _kr * _n[2] * fude_zoom_sim_scale(_all), FUDE_ZOOM_PAGE_KNOB_STEP);
                _page->knob_dragged = false;
                return true;
            }
            continue;
        }
        if(rde_math_abs_f64(_l.x) <= _n[1] * 1.1 && rde_math_abs_f64(_l.y) <= _n[2] * 1.3) {
            const b8 _changed = fude_zoom_circuit_tap(&_page->circuit, _i, -1, _say, sizeof(_say));
            if(_m == FUDE_ZOOM_MODEL_BUTTON) {
                _page->circuit_held    = _p[_i].object;   // Let go: up again.
                _page->circuit_held_at = _page->circuit.time;
                _page->circuit_letting = false;
            } else if(_changed) {
                fude_zoom_page_tap_notice(_page, _i, -1, _say);
            }
            return true;
        }
    }
    return false;
}

// --- electronics drawn over the schematic (circuit.h) -------------------------------------------------

// See page_parts.h. The prefix is the largest from M down to n that the value reaches; 3 figures, or the whole number
// from 100 up. 0 is said without a prefix.
void fude_zoom_page_si(f64 _v, const c8* _unit, c8* _out, usize _size) {
    static const struct { f64 k; const c8* p; } _pre[] = { { 1e6, "M" }, { 1e3, "k" }, { 1.0, "" }, { 1e-3, "m" }, { 1e-6, "\xC2\xB5" }, { 1e-9, "n" } };
    const f64 _a = rde_math_abs_f64(_v);
    u32 _i = 2u;
    if(_a > 0.0) {
        for(_i = 0; _i < 5u && _a < _pre[_i].k * 0.9995; _i++) {
        }
    }
    const f64 _x = _v / _pre[_i].k;
    rde_snprintf(_out, _size, rde_math_abs_f64(_x) >= 99.95 ? "%.0f %s%s" : (rde_math_abs_f64(_x) >= 9.995 ? "%.1f %s%s" : "%.2f %s%s"), _x, _pre[_i].p, _unit);
}

// Returns an LED's light by the colour its name (a part's kept colour, or Value's word) names: circuit.h's one table
// of LED colours, whole words in any of the apps' languages; red for any other.
RDE_INTERNAL rde_color fude_zoom_page_led_color(const c8* _name) {
    return fude_zoom_led_color_get(fude_zoom_led_color_of(_name))->light;
}

// Returns a part's own units → the screen (its symbol's), and writes its half sizes (1 when it has no numbers).
RDE_INTERNAL fude_zoom_sim fude_zoom_page_part_sim(const fude_zoom_page* _page, u32 _object, f64* _hw, f64* _hh) {
    const fude_zoom_object* _o = fude_zoom_scene_object(&_page->scene, _object);
    f64 _n[3] = { 0.0, 1.0, 1.0 };
    fude_zoom_scene_shape_numbers(&_page->scene, _object, _n, 3u);
    *_hw = _n[1];
    *_hh = _n[2];
    return fude_zoom_sim_compose(fude_zoom_page_frame_sim(_page, _o->frame), fude_zoom_object_sim(_o));
}

// Returns Play's tags' letters' height: as big as the circuit's parts look (fude_zoom_circuit_tag_px); 0: none.
RDE_INTERNAL f32 fude_zoom_page_play_tag_px(fude_zoom_page* _page) {
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_page->circuit.parts.memory;
    rde_arr* _sizes = &_page->play_sizes;
    rde_arr_clear(_sizes);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->circuit.parts); _i++) {
        f64 _hw, _hh;
        const fude_zoom_sim _all = fude_zoom_page_part_sim(_page, _p[_i].object, &_hw, &_hh);
        f64 _on = rde_math_min_f64(rde_math_abs_f64(_hw), rde_math_abs_f64(_hh)) * fude_zoom_sim_scale(_all);
        rde_arr_add(_sizes, (any)&_on);
    }
    return (f32)fude_zoom_circuit_tag_px((f64*)_sizes->memory, (u32)rde_arr_length(_sizes), FUDE_ZOOM_PAGE_TAG_PX, FUDE_ZOOM_PAGE_TAG_LEAST);
}

// Draws a Play tag _px high (none: 0) under a part's corner _corner (screen), below and beside it.
RDE_INTERNAL void fude_zoom_page_play_tag(fude_zoom_page* _page, rde_vec_2F _corner, const c8* _text, fude_zoom_v2 _half, f32 _px) {
    if(_px <= 0.0f) {
        return;
    }
    fude_zoom_page_draw_tag_px(_page, (rde_vec_2F){ _corner.x, _corner.y + 30.0f * _px / FUDE_ZOOM_PAGE_TAG_PX }, _text, NULL, _half, _px);
}

// Writes a limit's measure and its limit as a tag says them ("45 mA / 30 mA").
RDE_INTERNAL void fude_zoom_page_limit_say(u8 _limit, f64 _measure, f64 _most, c8* _out, usize _size) {
    static const c8* const _units[FUDE_ZOOM_LIMIT_COUNT] = { "W", "A", "V", "V" };
    const c8* _unit = _units[_limit < FUDE_ZOOM_LIMIT_COUNT ? _limit : 0u];
    c8 _a[32], _b[32];
    fude_zoom_page_si(_measure, _unit, _a, sizeof(_a));
    fude_zoom_page_si(_most, _unit, _b, sizeof(_b));
    rde_snprintf(_out, _size, "%s / %s", _a, _b);
}

// Draws how a part stands to its limits, over it (limits.h): past one, a warm glow growing as it heats, how much and
// its limit beside it (a source past its current: red, pulsing); burnt, charred (dark over it, a cross, "burnt");
// where the circuit would not settle, a red ring.
RDE_INTERNAL void fude_zoom_page_render_hurt(fude_zoom_page* _page, const fude_zoom_circuit_part* _q, fude_zoom_sim _all, f64 _hw, f64 _hh,
                                             fude_zoom_v2 _half, f32 _tag) {
    if(!_q->burnt && !_q->suspect && !(_q->stress > 1.0)) {
        return;
    }
    const f64 _k = fude_zoom_sim_scale(_all);
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 });
    const f32 _r = (f32)rde_math_max_f64(rde_math_max_f64(_hw, _hh) * _k * 1.15, 10.0);
    const rde_vec_2F _c = { (f32)_mid.x, (f32)_mid.y };
    // The pulse runs on Play's time, so a paused circuit stands still (a still screen: drawing/base/redraw.h).
    const f32 _pulse = 0.5f + 0.5f * rde_math_sin_f32((f32)_page->play_time * 6.0f);
    if(_q->burnt) {
        rde_rendering_2d_draw_circle(_c, _r, 32u, (rde_color){ 40, 30, 28, 150 }, NULL);
        const f32 _x = _r * 0.55f;
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - _x, _c.y - _x }, (rde_vec_2F){ _c.x + _x, _c.y + _x }, (rde_color){ 200, 40, 30, 230 }, rde_math_max_f32(_r * 0.08f, 2.0f));
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - _x, _c.y + _x }, (rde_vec_2F){ _c.x + _x, _c.y - _x }, (rde_color){ 200, 40, 30, 230 }, rde_math_max_f32(_r * 0.08f, 2.0f));
        if(_tag > 0.0f) {
            fude_zoom_page_draw_tag_px(_page, (rde_vec_2F){ _c.x - _r, _c.y - _r - 4.0f }, fude_text(FUDE_TEXT_ZOOM_PART_BURNT_TAG), NULL, _half, _tag);
        }
    } else if(_q->stress > 1.0) {
        const b8  _source = !(fude_zoom_limits_tau(_q->part) > 0.0);
        const f32 _heat   = _source ? 1.0f : rde_math_min_f32((f32)_q->heat, 1.0f);
        for(u32 _ring = 0; _ring < 3u; _ring++) {
            const rde_color _g = { 255, (u8)(_source ? 40.0f : 170.0f - 130.0f * _heat), 30,
                                   (u8)((20.0f + 25.0f * (f32)_ring) * (0.45f + 0.55f * (_source ? _pulse : _heat))) };
            rde_rendering_2d_draw_circle(_c, _r * (1.25f - 0.2f * (f32)_ring), 32u, _g, NULL);
        }
        if(_tag > 0.0f) {
            c8 _say[64];
            fude_zoom_page_limit_say(_q->worst, _q->measure[_q->worst], _q->limits.most[_q->worst], _say, sizeof(_say));
            fude_zoom_page_draw_tag_px(_page, (rde_vec_2F){ _c.x - _r, _c.y - _r - 4.0f }, _say, NULL, _half, _tag);
        }
    }
    if(_q->suspect) {
        rde_rendering_2d_draw_circle_with_border(_c, _r * 1.1f, 40u, (rde_color){ 0, 0, 0, 0 }, 2.5f, (rde_color){ 230, 40, 40, (u8)(150.0f + 100.0f * _pulse) }, NULL);
    }
}

// Draws an oscilloscope's settings along its screen's top, as a bench one shows them: CH1's volts a division in its
// yellow, CH2's in its cyan (when it is joined), its time a division in white: what its knobs are turned to. Nothing
// when it is too small for the words (13 points at most, 6 at least).
RDE_INTERNAL void fude_zoom_page_render_scope_readout(fude_zoom_page* _page, const fude_zoom_circuit_part* _q, fude_zoom_sim _all, f64 _hw, f64 _hh) {
    rde_font* _font = _page->renderer.font;
    const f32 _px = (f32)rde_math_min_f64(0.1 * _hh * fude_zoom_sim_scale(_all), 13.0);
    if(_font == NULL || _px < 6.0f) {
        return;
    }
    const fude_zoom_scope _sc = fude_zoom_display_scope_of(_q);
    f64 _u0, _v0, _u1, _v1;
    fude_zoom_scope_screen(&_u0, &_v0, &_u1, &_v1);
    static const u32 _knob[3] = { FUDE_ZOOM_SCOPE_KNOB_VOLTS1, FUDE_ZOOM_SCOPE_KNOB_VOLTS2, FUDE_ZOOM_SCOPE_KNOB_TIME };
    static const c8* const _head[3] = { "CH1 ", "CH2 ", "" };
    const rde_color _colour[3] = { { 250, 220, 60, 255 }, { 70, 220, 240, 255 }, { 230, 230, 230, 255 } };
    const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _u0 * _hw, _v1 * _hh }), _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _u1 * _hw, _v1 * _hh });
    const f32 _room = (f32)rde_math_hypot_f64(_b.x - _a.x, _b.y - _a.y);
    c8 _say[3][40];
    f32 _w[3], _all_w = 0.0f;
    for(u32 _i = 0; _i < 3u; _i++) {
        c8 _v[32];
        fude_zoom_scope_knob_say(&_sc, _knob[_i], _v, sizeof(_v));
        rde_snprintf(_say[_i], sizeof(_say[_i]), "%s%s", _head[_i], _v);
        _w[_i] = _i == 1u && _q->node[1] == FUDE_ZOOM_NONE ? 0.0f : fude_draw_text_width(_font, _page->renderer.font_px, _say[_i], _px);
        _all_w += _w[_i];
    }
    const f32 _gap = (_room - _all_w) / 4.0f;
    if(_gap < _px * 0.5f) {
        return;   // Too small to say it all: none of it, rather than part.
    }
    // Along the top edge, inside it, whichever way the part is turned: from its top-left corner toward its top-right.
    const f32 _dx = (f32)((_b.x - _a.x) / _room), _dy = (f32)((_b.y - _a.y) / _room);
    const fude_zoom_v2 _in = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _u0 * _hw, (_v1 - 0.09) * _hh });
    f32 _along = _gap;
    for(u32 _i = 0; _i < 3u; _i++) {
        if(_w[_i] <= 0.0f) {
            continue;
        }
        const f32 _x = (f32)_in.x + _dx * _along, _y = (f32)_in.y + _dy * _along;
        // On the screen's own dark, over what a wave past its top draws there.
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ _x + 0.5f * _w[_i], _y }, (rde_vec_2F){ _w[_i] + _px * 0.6f, _px * 1.3f }, (rde_color){ 12, 24, 18, 235 });
        fude_draw_text_placed(_font, _say[_i], (rde_vec_3F){ _x, _y - _px * 0.36f, 0.0f }, (rde_vec_2F){ _px / _page->renderer.font_px, _px / _page->renderer.font_px }, 0.0f,
                              _colour[_i]);
        _along += _w[_i] + _gap;
    }
}

// Returns whether the junction dots kept (page.h's junctions) were found for what is shown now: the scene as it is (its
// revision and its hidden layers), the same frames drawn (the editable list, as fude_zoom_render_editable gave it this
// frame) at the same places, and the same screen.
RDE_INTERNAL b8 fude_zoom_page_junctions_kept(const fude_zoom_page* _page, fude_zoom_v2 _half) {
    if(!_page->junctions_valid || _page->junctions_rev != _page->scene.revision || _page->junctions_hidden != _page->scene.layers_hidden ||
       _page->junctions_half.x != _half.x || _page->junctions_half.y != _half.y || rde_arr_length(&_page->junctions_view) != rde_arr_length(&_page->editable)) {
        return false;
    }
    const fude_zoom_visible* _was = (const fude_zoom_visible*)_page->junctions_view.memory;
    const fude_zoom_visible* _now = (const fude_zoom_visible*)_page->editable.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->editable); _i++) {
        if(_was[_i].frame != _now[_i].frame || _was[_i].depth != _now[_i].depth || _was[_i].to_screen.a != _now[_i].to_screen.a ||
           _was[_i].to_screen.b != _now[_i].to_screen.b || _was[_i].to_screen.tx != _now[_i].to_screen.tx || _was[_i].to_screen.ty != _now[_i].to_screen.ty) {
            return false;
        }
    }
    return true;
}

// Finds where wires join on the screen into page.h's junctions (a dot each: a wire's free end lying along another
// wire's segment, past its ends' first and last 2%, within 1.5 points), and the dots' radius (2.5 points, more for
// thick wires, 6 at most), from the wires in the frames drawn (the editable list); then keeps what they were found
// from. Every free end against every segment on the screen: done only when something changed. Its arrays are the
// page's scratch (page.h's circuit_found and junctions_points, _ends and _lines), emptied as it starts.
RDE_INTERNAL void fude_zoom_page_junctions_find(fude_zoom_page* _page, fude_zoom_v2 _half) {
    const fude_zoom_scene* _s = &_page->scene;
    rde_arr* _found = &_page->circuit_found;
    rde_arr* _pts   = &_page->junctions_points;
    rde_arr* _ends  = &_page->junctions_ends;
    rde_arr* _lines = &_page->junctions_lines;
    rde_arr_clear(_ends);
    rde_arr_clear(_lines);
    f32 _dot = 2.5f;
    for(u32 _v = 0; _v < (u32)rde_arr_length(&_page->editable); _v++) {
        const fude_zoom_visible _vis = ((const fude_zoom_visible*)_page->editable.memory)[_v];
        rde_arr_clear(_found);
        fude_zoom_scene_query(_s, _vis.frame, fude_zoom_sim_box(fude_zoom_sim_inverse(_vis.to_screen), (fude_zoom_box){ -_half.x, -_half.y, _half.x, _half.y }), _found);
        for(u32 _i = 0; _i < (u32)rde_arr_length(_found); _i++) {
            const u32 _o = ((const u32*)_found->memory)[_i];
            const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _o);
            if(_ob->kind != FUDE_ZOOM_KIND_SHAPE || _ob->channels != FUDE_ZOOM_SHAPE_WIRE || fude_zoom_scene_hides(_s, _ob)) {
                continue;
            }
            f64 _n[FUDE_ZOOM_WIRE_NUMBERS];
            const u32 _c = fude_zoom_scene_shape_numbers(_s, _o, _n, FUDE_ZOOM_WIRE_NUMBERS);
            fude_zoom_id _fa, _tb;
            i32 _fp, _tp;
            const u32 _k = fude_zoom_wire_of(_n, _c, NULL, &_fa, &_fp, &_tb, &_tp);
            b8 _closed;
            rde_arr_clear(_pts);
            fude_zoom_scene_shape_outline(_s, _o, 1u, _pts, &_closed);
            if(_k < 2u || rde_arr_length(_pts) < 2u) {
                continue;
            }
            const fude_zoom_v2* _p = (const fude_zoom_v2*)_pts->memory;
            for(u32 _j = 0; _j + 1u < (u32)rde_arr_length(_pts); _j++) {
                const fude_zoom_v2 _a = fude_zoom_sim_apply(_vis.to_screen, _p[_j]), _b = fude_zoom_sim_apply(_vis.to_screen, _p[_j + 1u]);
                rde_arr_add(_lines, (any)&_a);
                rde_arr_add(_lines, (any)&_b);
            }
            if(_fa == 0u || _fp < 0) {
                const fude_zoom_v2 _e = fude_zoom_sim_apply(_vis.to_screen, _p[0]);
                rde_arr_add(_ends, (any)&_e);
            }
            if(_tb == 0u || _tp < 0) {
                const fude_zoom_v2 _e = fude_zoom_sim_apply(_vis.to_screen, _p[rde_arr_length(_pts) - 1u]);
                rde_arr_add(_ends, (any)&_e);
            }
            _dot = rde_math_max_f32(_dot, rde_math_min_f32((f32)(_ob->radius * fude_zoom_sim_scale(_vis.to_screen) * 2.6), 6.0f));
        }
    }
    rde_arr_clear(&_page->junctions);
    const fude_zoom_v2* _e = (const fude_zoom_v2*)_ends->memory;
    const fude_zoom_v2* _l = (const fude_zoom_v2*)_lines->memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(_ends); _i++) {
        for(u32 _j = 0; _j + 1u < (u32)rde_arr_length(_lines); _j += 2u) {
            // (along the line, not near either end: there the nearest point is the end's foot, unclamped)
            const f64 _u = rde_math_segment_closest_t_2d_f64(_e[_i], _l[_j], _l[_j + 1u]);
            if(_u > 0.02 && _u < 0.98 && rde_math_point_segment_distance_2d_f64(_e[_i], _l[_j], _l[_j + 1u]) < 1.5) {
                rde_arr_add(&_page->junctions, (any)&_e[_i]);
                break;
            }
        }
    }
    // What they were found from, to know when to find them again.
    _page->junctions_dot    = _dot;
    _page->junctions_rev    = _s->revision;
    _page->junctions_hidden = _s->layers_hidden;
    _page->junctions_half   = _half;
    rde_arr_clear(&_page->junctions_view);
    if(rde_arr_length(&_page->editable) > 0u) {
        rde_memcpy(rde_arr_add_n(&_page->junctions_view, rde_arr_length(&_page->editable)), _page->editable.memory, rde_arr_length(&_page->editable) * sizeof(fude_zoom_visible));
    }
    _page->junctions_valid = true;
}

// Where wires join, a dot each (fude_zoom_page_render_circuit's): the frames the tools reach listed again, the
// junctions found again when what they were found from changed, and drawn in the ink's colour.
RDE_INTERNAL void fude_zoom_page_render_circuit_junctions(fude_zoom_page* _page, const fude_zoom_scene* _s, fude_zoom_v2 _half) {
    rde_arr_clear(&_page->editable);
    fude_zoom_render_editable(&_page->renderer, _s, &_page->editable);   // (the pins' marks use it too)
    // A canvas with no wire has no joins to look for, however its view moves: its objects are looked through for one
    // once each, as they are made (objects are only ever added; a canvas left starts the look again).
    const u32 _objects = fude_zoom_scene_object_count(_s);
    for(; !_page->junctions_wired && _page->junctions_looked < _objects; _page->junctions_looked++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _page->junctions_looked);
        _page->junctions_wired = _o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_WIRE;
    }
    if(!_page->junctions_wired) {
        return;
    }
    if(!fude_zoom_page_junctions_kept(_page, _half)) {
        fude_zoom_page_junctions_find(_page, _half);
    }
    const rde_color _ink = fude_theme_resolve(_page->app->ink->color);
    const fude_zoom_v2* _e = (const fude_zoom_v2*)_page->junctions.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->junctions); _i++) {
        rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)_e[_i].x, (f32)_e[_i].y }, _page->junctions_dot, 16u, _ink, NULL);
    }
}

// The wire being drawn, while one is (fude_zoom_page_render_circuit's): its way from where it began to the pen,
// solid when it would join something there, dashed when it would not; a ring on the pin it began at and on the one
// under the pen.
RDE_INTERNAL void fude_zoom_page_render_circuit_wiring(fude_zoom_page* _page, const fude_theme* _t) {
    if(_page->wiring) {
        u32 _to;
        i32 _to_pin;
        fude_zoom_v2 _end, _r[6];
        const b8 _joined = fude_zoom_page_wire_end(_page, _page->pen_now, &_to, &_to_pin, &_end);
        const u32 _m = fude_zoom_page_wire_way(_page, _page->wire_start, _page->wire_from, _page->wire_from_pin, _end, _to, _to_pin, _r);
        rde_vec_2F _sp[6];
        f32 _sr[6];
        for(u32 _i = 0; _i < _m; _i++) {
            _sp[_i] = (rde_vec_2F){ (f32)_r[_i].x, (f32)_r[_i].y };
            _sr[_i] = 1.5f;
        }
        if(_joined) {
            rde_rendering_2d_draw_stroke(_sp, _sr, _m, _t->accent);
        } else {
            for(u32 _i = 0; _i + 1u < _m; _i++) {   // On nothing yet, dashed: let go here, no wire.
                rde_rendering_2d_draw_line_segmented(_sp[_i], _sp[_i + 1u], _t->accent, 1.5f, 6.0f, 5.0f);
            }
        }
        if(_page->wire_from != FUDE_ZOOM_NONE) {   // The pin it starts at: its middle.
            rde_rendering_2d_draw_circle_with_border((rde_vec_2F){ (f32)_page->wire_start.x, (f32)_page->wire_start.y }, 6.0f, 16u, (rde_color){ 0, 0, 0, 0 }, 2.0f, _t->accent, NULL);
        }
        if(_to != FUDE_ZOOM_NONE) {
            rde_rendering_2d_draw_circle_with_border((rde_vec_2F){ (f32)_end.x, (f32)_end.y }, 6.0f, 16u, (rde_color){ 0, 0, 0, 0 }, 2.0f, _t->accent, NULL);
        }
    }
}

// Every part's pins marked while the Wire tool is in hand (fude_zoom_page_render_circuit's), in each frame the
// tools reach, but a part too small on the screen to wire. The objects on the screen are found into the page's
// scratch (page.h's circuit_found).
RDE_INTERNAL void fude_zoom_page_render_circuit_pins(fude_zoom_page* _page, const fude_zoom_scene* _s, fude_zoom_v2 _half, const fude_theme* _t) {
    if(_page->wire_tool) {
        rde_arr* _found = &_page->circuit_found;
        for(u32 _v = 0; _v < (u32)rde_arr_length(&_page->editable); _v++) {
            const fude_zoom_visible _vis = ((const fude_zoom_visible*)_page->editable.memory)[_v];
            rde_arr_clear(_found);
            fude_zoom_scene_query(_s, _vis.frame, fude_zoom_sim_box(fude_zoom_sim_inverse(_vis.to_screen), (fude_zoom_box){ -_half.x, -_half.y, _half.x, _half.y }), _found);
            for(u32 _i = 0; _i < (u32)rde_arr_length(_found); _i++) {
                const u32 _o = ((const u32*)_found->memory)[_i];
                const fude_zoom_part* _part = fude_zoom_part_of(_s, _o);
                if(_part == NULL || _part->pins == NULL || fude_zoom_scene_hides(_s, fude_zoom_scene_object(_s, _o))) {
                    continue;
                }
                const f64 _unit = fude_zoom_part_unit(_s, _o) * fude_zoom_sim_scale(_vis.to_screen);
                if(_unit < 5.0) {
                    continue;   // Too small to wire.
                }
                for(u32 _k = 0; _k < _part->pin_count; _k++) {
                    fude_zoom_v2 _p;
                    if(fude_zoom_part_pin_at(_s, _o, _k, &_p)) {
                        _p = fude_zoom_sim_apply(_vis.to_screen, _p);
                        rde_rendering_2d_draw_circle_with_border((rde_vec_2F){ (f32)_p.x, (f32)_p.y }, (f32)rde_math_min_f64(_unit * 0.3, 5.0), 12u, _t->surface, 1.5f, _t->accent, NULL);
                    }
                }
            }
        }
    }
}

// Each logic probe's chosen light, faintly in it, while nothing plays (fude_zoom_page_render_circuit's). A light is an
// attribute, so only the canvas's attributes are looked at (page.h's attributes, gathered once each as objects are
// made), in the order of the objects as before, not every object each frame: a canvas with none draws nothing at once.
RDE_INTERNAL void fude_zoom_page_render_circuit_probes(fude_zoom_page* _page, const fude_zoom_scene* _s, fude_zoom_v2 _half) {
    if(!_page->play_on) {
        fude_zoom_props_gather(_s, &_page->attributes_looked, &_page->attributes);
        const u32* _attributes = (const u32*)_page->attributes.memory;
        for(u32 _k = 0; _k < (u32)rde_arr_length(&_page->attributes); _k++) {
            const u32 _a = _attributes[_k];
            const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _a);
            rde_color _lit;
            if(!(_ob->flags & FUDE_ZOOM_FLAG_ALIVE) || _ob->kind != FUDE_ZOOM_KIND_SHAPE || _ob->channels != FUDE_ZOOM_SHAPE_PROPS || !fude_zoom_props_light(_s, _a, &_lit)) {
                continue;
            }
            const u32 _t = fude_zoom_props_target(_s, _a);
            const fude_zoom_part* _part = _t != FUDE_ZOOM_NONE ? fude_zoom_part_of(_s, _t) : NULL;
            if(_part == NULL || _part->model != FUDE_ZOOM_MODEL_LOGIC_OUT || fude_zoom_scene_hides(_s, fude_zoom_scene_object(_s, _t))) {
                continue;
            }
            f64 _hw, _hh;
            const fude_zoom_sim _all = fude_zoom_page_part_sim(_page, _t, &_hw, &_hh);
            const fude_zoom_v2  _c2  = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.2 * _hw, 0.0 });
            const f64           _r   = 0.6 * rde_math_min_f64(_hw, _hh) * fude_zoom_sim_scale(_all);
            if(_r < 1.0 || rde_math_abs_f64(_c2.x) > _half.x + _r || rde_math_abs_f64(_c2.y) > _half.y + _r) {
                continue;
            }
            _lit.a = 80u;
            rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)_c2.x, (f32)_c2.y }, (f32)_r, 24u, _lit, NULL);
        }
    }
}

// The most of the current's dots one piece of wire draws: a bound on a frame's drawing that a screen never reaches,
// with a dot every 18 points of it (fude_zoom_page_dots_along hands it only those on the screen).
#define FUDE_ZOOM_PAGE_DOTS_MOST 8192u

// One of the current's dots, amber, at _at on the screen (fude_zoom_page_dots_along's _dot).
RDE_INTERNAL void fude_zoom_page_current_dot(any _user, rde_vec_2F _at) {
    RDE_UNUSED(_user);
    rde_rendering_2d_draw_circle(_at, 2.6f, 10u, (rde_color){ 255, 196, 40, 255 }, NULL);
}

// The current, while a circuit plays (fude_zoom_page_render_circuit's): dots moving along each wire's pieces as
// said where it is called (_hs, _hk: the home frame's way onto the screen and its scale; _fdt: Play's time since the
// dots last moved), and each node's volts once, by its longest wire's middle (_tag, _tu: the tags' size and as
// much smaller their offsets).
RDE_INTERNAL void fude_zoom_page_render_circuit_current(fude_zoom_page* _page, fude_zoom_circuit* _c, fude_zoom_v2 _half, fude_zoom_sim _hs, f64 _hk,
                                                        f64 _fdt, f32 _tag, f32 _tu) {
    fude_zoom_circuit_wire* _w = (fude_zoom_circuit_wire*)_c->wires.memory;   // (their dots' phases moved on: the page's circuit)
    rde_arr_resize(&_page->circuit_labelled, 0u);
    rde_arr_resize(&_page->circuit_labelled, _c->nodes);   // Each node's voltage written once: all not yet.
    u8* _labelled = (u8*)_page->circuit_labelled.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->wires); _i++) {
        for(u32 _k = 0; _k < _w[_i].pieces; _k++) {
            const f64 _cur = _w[_i].current[_k];
            if(rde_math_abs_f64(_cur) < 1e-6) {
                continue;
            }
            const f64 _speed = (_cur > 0.0 ? 1.0 : -1.0) * rde_math_min_f64(rde_math_max_f64(25.0 * rde_math_log10_f64(rde_math_abs_f64(_cur) / 1e-6), 8.0), 160.0);
            _w[_i].phase[_k] = rde_math_fmod_f64(_w[_i].phase[_k] + _speed * _fdt, 18.0);
            if(_w[_i].phase[_k] < 0.0) {
                _w[_i].phase[_k] += 18.0;
            }
            // Along the piece (lengths along the wire, home units) in screen steps of 18 points, those on the screen.
            const f64 _from = _w[_i].cut[_k], _to = _w[_i].cut[_k + 1u];
            const f64 _step = 18.0 / rde_math_max_f64(_hk, 1e-300);
            fude_zoom_page_dots_along(_w[_i].points, _w[_i].count, false, _from, _to, _from + _w[_i].phase[_k] / rde_math_max_f64(_hk, 1e-300), _step, _hs, _half,
                                      FUDE_ZOOM_PAGE_DOTS_MOST, fude_zoom_page_current_dot, NULL);
        }
        // Its node's voltage, once a node (not ground), by its longest piece's middle, when the wire is long enough
        // on the screen for it.
        const u32 _node = _w[_i].node;
        if(_tag > 0.0f && _node != FUDE_ZOOM_NONE && _node != 0u && _node < _c->nodes && !_labelled[_node] && _w[_i].length * _hk > 40.0 * _tu) {
            _labelled[_node] = true;
            u32 _best = 0;
            f64 _most = -1.0;
            for(u32 _j = 0; _j + 1u < _w[_i].count; _j++) {
                const f64 _len = rde_math_hypot_f64(_w[_i].points[_j + 1u].x - _w[_i].points[_j].x, _w[_i].points[_j + 1u].y - _w[_i].points[_j].y);
                if(_len > _most) {
                    _most = _len;
                    _best = _j;
                }
            }
            const fude_zoom_v2 _m = fude_zoom_sim_apply(_hs, (fude_zoom_v2){ (_w[_i].points[_best].x + _w[_i].points[_best + 1u].x) * 0.5,
                                                                              (_w[_i].points[_best].y + _w[_i].points[_best + 1u].y) * 0.5 });
            if(rde_math_abs_f64(_m.x) <= _half.x && rde_math_abs_f64(_m.y) <= _half.y) {
                c8 _say[32];
                fude_zoom_page_si(fude_zoom_circuit_volts(_c, _node), "V", _say, sizeof(_say));
                fude_zoom_page_draw_tag_px(_page, (rde_vec_2F){ (f32)_m.x - 30.0f * _tu, (f32)_m.y + 34.0f * _tu }, _say, NULL, _half, _tag);
            }
        }
    }
}

// One part as the steps of fude_zoom_page_render_circuit that draw what it shows see it: the page, its scene, the
// circuit and the part; its way onto the screen (all), half its size in its own units (hw, hh), screen points a unit
// of it (k) and its middle on the screen; half the screen's size, the tags' size, and the theme.
typedef struct {
    fude_zoom_page*               page;
    const fude_zoom_scene*        s;
    const fude_zoom_circuit*      c;
    const fude_zoom_circuit_part* q;
    fude_zoom_sim                 all;
    f64                           hw, hh, k;
    fude_zoom_v2                  mid, half;
    f32                           tag;
    const fude_theme*             t;
} fude_zoom_page_shown;

// What a part that shows a state draws (fude_zoom_page_render_circuit_part's): an LED's or a lamp's glow, a servo's
// horn, a display's lights (display.h) and a scope's readout, a seven-segment's segments, a meter's reading, a
// logic output's light, a switch's or a button's lever (a logic input's 1 or 0), a changeover switch's levers.
// False: the part is none of those.
RDE_INTERNAL b8 fude_zoom_page_render_circuit_indicators(const fude_zoom_page_shown* _v) {
    fude_zoom_page* _page = _v->page;
    const fude_zoom_circuit* _c = _v->c;
    const fude_zoom_circuit_part* _q = _v->q;
    const fude_zoom_sim _all = _v->all;
    const f64 _hw = _v->hw, _hh = _v->hh, _k = _v->k;
    const fude_zoom_v2 _mid = _v->mid, _half = _v->half;
    const f32 _tag = _v->tag;
    const fude_theme* _t = _v->t;
    c8 _say[48];
    switch(_q->part->model) {
    case FUDE_ZOOM_MODEL_LED:
        if(_q->shown > 0.02) {
            // A glow round it, as bright as its current (its symbol still seen through it).
            const f64 _b = rde_math_min_f64(_q->shown, 1.0);
            for(u32 _ring = 0; _ring < 3u; _ring++) {
                rde_color _g = fude_zoom_page_led_color(_q->color);
                _g.a = (u8)((18.0 + 22.0 * (f64)_ring) * (0.4 + 0.6 * _b));
                rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)_mid.x, (f32)_mid.y }, (f32)(_hh * _k * (1.5 - 0.35 * (f64)_ring) * (0.7 + 0.5 * _b)), 32u, _g, NULL);
            }
        }
        break;
    case FUDE_ZOOM_MODEL_SERVO: {
        // Its horn, from its middle, at its angle (0° right, counter-clockwise): a bar with a hole at its end. On a
        // mechanism's shaft (state[7] not below 0), what is on it is its horn: only its hub is drawn.
        const f64 _r  = 0.75 * rde_math_min_f64(_hw, _hh) * _k;
        const f64 _an = _q->shown * RDE_PI / 180.0 + rde_math_atan2_f64(_all.b, _all.a);
        const rde_vec_2F _c0 = { (f32)_mid.x, (f32)_mid.y }, _c1 = { (f32)(_mid.x + rde_math_cos_f64(_an) * _r), (f32)(_mid.y + rde_math_sin_f64(_an) * _r) };
        const rde_color _ink = fude_theme_active()->accent;
        if(_q->state[7] < 0.0) {
            rde_rendering_2d_draw_line_1(_c0, _c1, _ink, (f32)rde_math_max_f64(0.22 * _r, 3.0));
            rde_rendering_2d_draw_circle(_c1, (f32)rde_math_max_f64(0.07 * _r, 1.5), 12u, fude_theme_active()->surface, NULL);
        }
        rde_rendering_2d_draw_circle(_c0, (f32)rde_math_max_f64(0.2 * _r, 3.0), 20u, _ink, NULL);
        break;
    }
    case FUDE_ZOOM_MODEL_LAMP:
        if(_q->shown > 0.02) {
            const f64 _b = rde_math_min_f64(_q->shown, 1.0);
            for(u32 _ring = 0; _ring < 3u; _ring++) {
                const rde_color _g = { 255, 225, 110, (u8)((18.0 + 22.0 * (f64)_ring) * (0.4 + 0.6 * _b)) };
                rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)_mid.x, (f32)_mid.y }, (f32)(rde_math_min_f64(_hw, _hh) * _k * (1.6 - 0.35 * (f64)_ring)), 32u, _g, NULL);
            }
        }
        break;
    case FUDE_ZOOM_MODEL_SEG_PANEL:
    case FUDE_ZOOM_MODEL_LED_MATRIX:
    case FUDE_ZOOM_MODEL_BAR_GRAPH:
    case FUDE_ZOOM_MODEL_PANEL_METER:
    case FUDE_ZOOM_MODEL_CHAR_LCD:
    case FUDE_ZOOM_MODEL_SCOPE:
        fude_zoom_display_render(_c, _q, _all, _hw, _hh, fude_zoom_page_led_color(_q->color), _c->time);
        fude_zoom_page_render_scope_readout(_page, _q, _all, _hw, _hh);
        break;
    case FUDE_ZOOM_MODEL_SEVEN_SEG: {
        static const f64 _seg[7][4] = { { -0.3, 0.8, 0.3, 0.8 }, { 0.3, 0.8, 0.3, 0.15 }, { 0.3, 0.15, 0.3, -0.5 }, { -0.3, -0.5, 0.3, -0.5 },
                                        { -0.3, 0.15, -0.3, -0.5 }, { -0.3, 0.8, -0.3, 0.15 }, { -0.3, 0.15, 0.3, 0.15 } };
        for(u32 _g = 0; _g < 7u; _g++) {
            if(_q->state[_g] > 0.05) {
                const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _seg[_g][0] * _hw, _seg[_g][1] * _hh });
                const fude_zoom_v2 _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ _seg[_g][2] * _hw, _seg[_g][3] * _hh });
                rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, (rde_color){ 255, 40, 40, 230 },
                                             (f32)rde_math_max_f64(0.12 * _hw * _k, 2.0));
            }
        }
        break;
    }
    case FUDE_ZOOM_MODEL_VOLTMETER:
    case FUDE_ZOOM_MODEL_AMMETER:
        fude_zoom_page_si(_q->shown, _q->part->model == FUDE_ZOOM_MODEL_VOLTMETER ? "V" : "A", _say, sizeof(_say));
        fude_zoom_page_play_tag(_page, (rde_vec_2F){ (f32)(_mid.x + _hw * _k), (f32)(_mid.y + _hh * _k) }, _say, _half, _tag);
        break;
    case FUDE_ZOOM_MODEL_LOGIC_OUT:
        if(_q->shown > 0.5) {
            const fude_zoom_v2 _c2 = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.2 * _hw, 0.0 });
            rde_color _lit = _q->lit;
            _lit.a = (u8)((u32)_lit.a * 220u / 255u);
            rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)_c2.x, (f32)_c2.y }, (f32)(0.6 * rde_math_min_f64(_hw, _hh) * _k), 24u, _lit, NULL);
        }
        break;
    case FUDE_ZOOM_MODEL_LOGIC_IN:
    case FUDE_ZOOM_MODEL_SWITCH:
    case FUDE_ZOOM_MODEL_BUTTON:
        if(_q->switch_on & 1u) {
            const b8 _logic = _q->part->model == FUDE_ZOOM_MODEL_LOGIC_IN;
            const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ (_logic ? -0.55 : -0.5) * _hw, 0.0 });
            const fude_zoom_v2 _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ (_logic ? 0.3 : 0.5) * _hw, 0.0 });
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, _t->accent, (f32)rde_math_max_f64(0.08 * _hh * _k, 2.0));
        }
        if(_q->part->model == FUDE_ZOOM_MODEL_LOGIC_IN) {
            fude_zoom_page_play_tag(_page, (rde_vec_2F){ (f32)(_mid.x - _hw * _k), (f32)(_mid.y + _hh * _k) }, (_q->switch_on & 1u) ? "1" : "0", _half, _tag);
        }
        break;
    case FUDE_ZOOM_MODEL_SPDT:
    case FUDE_ZOOM_MODEL_DPDT: {
        // Its lever where it is: on A, or thrown to B.
        const u32 _poles = _q->part->model == FUDE_ZOOM_MODEL_DPDT ? 2u : 1u;
        for(u32 _pole = 0; _pole < _poles; _pole++) {
            const f64 _vc = _poles == 2u ? (_pole == 0u ? 2.0 / 3.0 : -1.0 / 3.0) : 0.0;
            const f64 _vt = (_q->switch_on & 1u) ? (_poles == 2u ? _vc - 1.0 / 3.0 : -2.0 / 3.0) : (_poles == 2u ? _vc : 2.0 / 3.0);
            const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ -0.5 * _hw, _vc * _hh });
            const fude_zoom_v2 _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.5 * _hw, _vt * _hh });
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, _t->accent, (f32)rde_math_max_f64(0.06 * _hh * _k, 2.0));
        }
        break;
    }
    default:
        return false;
    }
    return true;
}

// What any other part draws (fude_zoom_page_render_circuit_part's): a thyristor's bar of light while it conducts,
// a speaker's waves, a sensor's reading, a pot's turn, a stepper's or an encoder's pointer and reading, a slot's
// beam, a motor's rotor, a fuse blown, a buzzer's note, a board's driven pins; nothing for the rest.
RDE_INTERNAL void fude_zoom_page_render_circuit_devices(const fude_zoom_page_shown* _v) {
    const fude_zoom_scene* _s = _v->s;
    fude_zoom_page* _page = _v->page;
    const fude_zoom_circuit* _c = _v->c;
    const fude_zoom_circuit_part* _q = _v->q;
    const fude_zoom_sim _all = _v->all;
    const f64 _hw = _v->hw, _hh = _v->hh, _k = _v->k;
    const fude_zoom_v2 _mid = _v->mid, _half = _v->half;
    const f32 _tag = _v->tag;
    const fude_theme* _t = _v->t;
    c8 _say[48];
    switch(_q->part->model) {
    case FUDE_ZOOM_MODEL_SCR:
    case FUDE_ZOOM_MODEL_TRIAC:
        // On: a bar of light along it.
        if(_q->state[0] > 0.5) {
            const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ -0.85 * _hw, 0.0 }), _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.85 * _hw, 0.0 });
            rde_color _lit = _t->accent;
            _lit.a = 150u;
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, _lit, (f32)rde_math_max_f64(0.2 * _hh * _k, 3.0));
        }
        break;
    case FUDE_ZOOM_MODEL_SPEAKER:
        // Sounding: waves off its cone, as strong as it is.
        if(_q->shown > 2e-3) {
            rde_color _ink = _t->accent;
            _ink.a = (u8)(80.0 + 175.0 * rde_math_min_f64(_q->shown / 0.2, 1.0));
            for(u32 _w = 0; _w < 3u; _w++) {
                const f64 _r = (0.75 + 0.25 * (f64)_w) * _hw * _k;
                const fude_zoom_v2 _o = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.1 * _hw, 0.0 });
                const f64 _base = rde_math_atan2_f64(_all.b, _all.a);
                rde_vec_2F _arc[9];
                f32 _rr[9];
                for(u32 _z = 0; _z < 9u; _z++) {
                    const f64 _an = _base - 0.6 + 1.2 * (f64)_z / 8.0;
                    _arc[_z] = (rde_vec_2F){ (f32)(_o.x + rde_math_cos_f64(_an) * _r), (f32)(_o.y + rde_math_sin_f64(_an) * _r) };
                    _rr[_z] = 1.2f;
                }
                rde_rendering_2d_draw_stroke(_arc, _rr, 9u, _ink);
            }
        }
        break;
    case FUDE_ZOOM_MODEL_LDR:
    case FUDE_ZOOM_MODEL_PHOTODIODE:
    case FUDE_ZOOM_MODEL_PHOTOTRANSISTOR:
    case FUDE_ZOOM_MODEL_THERMISTOR:
        if(fude_zoom_circuit_lit(_q->part->model)) {
            c8 _lux[24];
            rde_snprintf(_lux, sizeof(_lux), "%.0f", _q->state[0]);
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_LUX, FUDE_TS(_lux));   // (its light, "lux" in the person's language)
        } else {
            rde_snprintf(_say, sizeof(_say), "%.0f \xC2\xB0" "C", _q->state[0]);
        }
        fude_zoom_page_play_tag(_page, (rde_vec_2F){ (f32)(_mid.x + _hw * _k), (f32)(_mid.y + _hh * _k) }, _say, _half, _tag);
        break;
    case FUDE_ZOOM_MODEL_POT:
        rde_snprintf(_say, sizeof(_say), "%.0f%%", _q->state[0] * 100.0);
        fude_zoom_page_play_tag(_page, (rde_vec_2F){ (f32)(_mid.x + _hw * _k), (f32)(_mid.y + _hh * _k) }, _say, _half, _tag);
        break;
    case FUDE_ZOOM_MODEL_STEPPER:
    case FUDE_ZOOM_MODEL_ENCODER: {
        // Its shaft's pointer at its angle (an encoder's: its detents, clockwise; on a mechanism's shaft, what is on it
        // shows it), and where it is: a stepper's shaft's degrees, an encoder's count.
        const b8  _step = _q->part->model == FUDE_ZOOM_MODEL_STEPPER;
        const f64 _an = (_step ? _q->state[1] * RDE_PI / 180.0 : -_q->state[0] * 2.0 * RDE_PI / rde_math_max_f64(_q->value[0], 1.0)) +
                        0.5 * RDE_PI + rde_math_atan2_f64(_all.b, _all.a);
        const f64 _r  = (_step ? 0.45 * _hh : 0.3 * rde_math_min_f64(_hw, _hh)) * _k;
        if(_q->state[7] < 0.0) {
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_mid.x, (f32)_mid.y }, (rde_vec_2F){ (f32)(_mid.x + rde_math_cos_f64(_an) * _r), (f32)(_mid.y + rde_math_sin_f64(_an) * _r) },
                                         _t->accent, (f32)rde_math_max_f64(0.06 * _r, 2.5));
        }
        if(_step) {
            rde_snprintf(_say, sizeof(_say), "%.0f\xC2\xB0", _q->state[1]);
        } else {
            rde_snprintf(_say, sizeof(_say), "%.0f", rde_math_round_f64(_q->state[0]));
        }
        fude_zoom_page_play_tag(_page, (rde_vec_2F){ (f32)(_mid.x + _hw * _k), (f32)(_mid.y + _hh * _k) }, _say, _half, _tag);
        break;
    }
    case FUDE_ZOOM_MODEL_SLOT: {
        // Its beam across its slot while its LED is lit and the slot clear; something in it, a block across it.
        const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ -0.2 * _hw, 0.3 * _hh }), _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.2 * _hw, 0.3 * _hh });
        if(_q->state[0] >= 0.5) {
            const fude_zoom_v2 _lo = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, -0.3 * _hh }), _hi = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 1.0 * _hh });
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_lo.x, (f32)_lo.y }, (rde_vec_2F){ (f32)_hi.x, (f32)_hi.y }, _t->accent, (f32)rde_math_max_f64(0.25 * _hw * _k, 3.0));
        } else if(_q->pin_i[0] > 1e-3) {
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, (rde_color){ 255, 60, 60, 200 },
                                         (f32)rde_math_max_f64(0.05 * _hh * _k, 2.5));
        }
        break;
    }
    case FUDE_ZOOM_MODEL_MOTOR: {
        const f64 _r = 0.85 * rde_math_min_f64(_hw * 0.6, _hh) * 0.8;
        const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ rde_math_cos_f64(_q->shown) * _r, rde_math_sin_f64(_q->shown) * _r });
        const fude_zoom_v2 _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ -rde_math_cos_f64(_q->shown) * _r, -rde_math_sin_f64(_q->shown) * _r });
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, _t->accent, 2.5f);
        break;
    }
    case FUDE_ZOOM_MODEL_FUSE:
        if(_q->state[0] > 0.5) {
            const fude_zoom_v2 _a = fude_zoom_sim_apply(_all, (fude_zoom_v2){ -0.4 * _hw, -_hh }), _b = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.4 * _hw, _hh });
            const fude_zoom_v2 _c2 = fude_zoom_sim_apply(_all, (fude_zoom_v2){ -0.4 * _hw, _hh }), _d = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.4 * _hw, -_hh });
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_a.x, (f32)_a.y }, (rde_vec_2F){ (f32)_b.x, (f32)_b.y }, _t->danger, 2.5f);
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_c2.x, (f32)_c2.y }, (rde_vec_2F){ (f32)_d.x, (f32)_d.y }, _t->danger, 2.5f);
        }
        break;
    case FUDE_ZOOM_MODEL_BUZZER:
        if(_q->shown > 0.5) {
            fude_zoom_page_play_tag(_page, (rde_vec_2F){ (f32)(_mid.x + _hw * _k), (f32)(_mid.y + _hh * _k) }, "\xE2\x99\xAA", _half, _tag);
        }
        break;
    case FUDE_ZOOM_MODEL_BOARD: {
        // Its driven pins: lit while high.
        for(u32 _k2 = 0; _k2 < _q->part->pin_count && _k2 < 64u; _k2++) {
            const u64 _bit = 1ull << _k2;
            if(!(_q->drive & _bit)) {
                continue;
            }
            const b8 _high = (_q->drive_blink & _bit) ? rde_math_fmod_f64(_c->time, 1.0) < 0.5 : (_q->drive_high & _bit) != 0u;
            fude_zoom_v2 _pp;
            if(fude_zoom_part_pin_at(_s, _q->object, _k2, &_pp)) {
                _pp = fude_zoom_sim_apply(fude_zoom_page_frame_sim(_page, fude_zoom_scene_object(_s, _q->object)->frame), _pp);
                rde_rendering_2d_draw_circle_with_border((rde_vec_2F){ (f32)_pp.x, (f32)_pp.y }, 5.0f, 16u, _high ? (rde_color){ 60, 220, 90, 255 } : _t->surface_2,
                                                         1.5f, _t->outline, NULL);
            }
        }
        break;
    }
    default:
        break;
    }
}

// What part _q shows while its circuit plays (fude_zoom_page_render_circuit's), unless it is off the screen: a
// spark where a contact just broke a coil's current, what its kind shows (the two families above), then how it
// stands to its limits (fude_zoom_page_render_hurt).
RDE_INTERNAL void fude_zoom_page_render_circuit_part(fude_zoom_page* _page, const fude_zoom_scene* _s, const fude_zoom_circuit* _c,
                                                     const fude_zoom_circuit_part* _q, fude_zoom_v2 _half, f32 _tag, const fude_theme* _t) {
    f64 _hw, _hh;
    const fude_zoom_sim _all = fude_zoom_page_part_sim(_page, _q->object, &_hw, &_hh);
    const f64 _k = fude_zoom_sim_scale(_all);
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(_all, (fude_zoom_v2){ 0.0, 0.0 });
    if(rde_math_abs_f64(_mid.x) > _half.x + (_hw + _hh) * _k || rde_math_abs_f64(_mid.y) > _half.y + (_hw + _hh) * _k) {
        return;
    }
    // A contact that just sparked (it broke a coil's current): a spark at it, a moment.
    const f64 _spark = fude_zoom_circuit_sparked(_q);
    if(_spark > 0.0 && _c->time - _spark < 0.3) {
        const f64 _r = 0.6 * rde_math_min_f64(_hw, _hh) * _k + 4.0;
        const rde_color _hot = { 255, 210, 60, 255 };
        for(u32 _ray = 0; _ray < 8u; _ray++) {
            const f64 _an = RDE_PI * 0.25 * (f64)_ray + 0.3;
            const f64 _in = (_ray % 2u == 0u ? 0.25 : 0.45) * _r, _out = (_ray % 2u == 0u ? 1.0 : 0.65) * _r;
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)(_mid.x + rde_math_cos_f64(_an) * _in), (f32)(_mid.y + rde_math_sin_f64(_an) * _in) },
                                         (rde_vec_2F){ (f32)(_mid.x + rde_math_cos_f64(_an) * _out), (f32)(_mid.y + rde_math_sin_f64(_an) * _out) }, _hot, 2.5f);
        }
    }
    const fude_zoom_page_shown _v = { .page = _page, .s = _s, .c = _c, .q = _q, .all = _all, .hw = _hw, .hh = _hh, .k = _k, .mid = _mid,
                                      .half = _half, .tag = _tag, .t = _t };
    if(!fude_zoom_page_render_circuit_indicators(&_v)) {
        fude_zoom_page_render_circuit_devices(&_v);
    }
    fude_zoom_page_render_hurt(_page, _q, _all, _hw, _hh, _half, _tag);
}

// See page_parts.h. Always: a dot where a wire's free end lands on another wire, the wire being drawn, every pin while
// the Wire tool is in hand, and a logic probe's chosen light. While a circuit plays: current as dots along the wires,
// each node's volts once, then what each part shows (a glow, a lever, a needle, a tag) and how it stands to its
// limits. Each of those is a step above (fude_zoom_page_render_circuit_*), taken in that order; what a part shows is
// a case for each kind of part, in two families of cases.
void fude_zoom_page_render_circuit(fude_zoom_page* _page, fude_zoom_v2 _half) {
    const fude_zoom_scene* _s = &_page->scene;
    const fude_theme* _t = fude_theme_active();
    // Where wires join (a free end on another wire): a dot, as schematics have them. Not looked for on a canvas with
    // no wire, and found again only when what they are found from changed (fude_zoom_page_junctions_kept), not every
    // frame.
    fude_zoom_page_render_circuit_junctions(_page, _s, _half);
    // The wire being drawn: its way on to the pen (on to a pin there).
    fude_zoom_page_render_circuit_wiring(_page, _t);
    // The pen drawing wires: every part's pins marked (a breadboard's holes are its own marks).
    fude_zoom_page_render_circuit_pins(_page, _s, _half, _t);
    // A logic probe's light chosen (props.h), while not playing: its colour faintly in it (playing: lit, or not).
    fude_zoom_page_render_circuit_probes(_page, _s, _half);
    if(!_page->play_on || !_page->play_circuit || !_page->circuit.ok) {
        return;
    }
    const fude_zoom_circuit* _c = &_page->circuit;
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const fude_zoom_sim _hs = fude_zoom_page_frame_sim(_page, _home);
    const f64 _hk = fude_zoom_sim_scale(_hs);
    const f32 _tag = fude_zoom_page_play_tag_px(_page);
    const f32 _tu  = _tag / FUDE_ZOOM_PAGE_TAG_PX;   // How far from what it tells of, as much smaller.
    // The current's dots move by Play's time since they last moved (the page's circuit_drawn_at), a tenth of a second
    // at most: paused, they stand still (a still screen: drawing/base/redraw.h).
    const f64 _fdt = rde_math_min_f64(rde_math_max_f64(_page->play_time - _page->circuit_drawn_at, 0.0), 0.1);
    _page->circuit_drawn_at = _page->play_time;
    // Current: dots along each wire's pieces, as fast as it is strong (25 points a second for each tenfold over a
    // microamp, from 8 to 160), its way.
    fude_zoom_page_render_circuit_current(_page, &_page->circuit, _half, _hs, _hk, _fdt, _tag, _tu);
    // What the parts show.
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_c->parts.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        const fude_zoom_circuit_part* _q = &_p[_i];
        fude_zoom_page_render_circuit_part(_page, _s, _c, _q, _half, _tag, _t);
    }
}

// --- a part's value: Value on the lasso's row (valueform.h) ------------------------------------------
//
// A circuit's part alone (a resistor, a capacitor, an inductor, a source's volts, a current, a clock's frequency, a
// fuse's), a motor's speed and way round, a weight's or a crate's mass: its number typed, its unit chosen; an LED's
// colour chosen. Its text made again so (the part made again: fude_zoom_page_symbol_text_commit), as it would be typed:
// "4.7kΩ", "100µF", "9V", "30 rpm cw", "2 kg".

// One of the units a value is asked in.
typedef struct {
    const c8* name;    // as it is written after the number ("kΩ")
    f64       times;   // what one of it is in the SI unit (a kΩ: 1000 Ω)
} fude_zoom_page_unit;

// Each kind of value's units, the smallest first (Value offers them in this order). A mass's SI unit is the kilogram.
RDE_INTERNAL const fude_zoom_page_unit FUDE_ZOOM_PAGE_OHMS[]   = { { "\xCE\xA9", 1.0 }, { "k\xCE\xA9", 1e3 }, { "M\xCE\xA9", 1e6 } };
RDE_INTERNAL const fude_zoom_page_unit FUDE_ZOOM_PAGE_FARADS[] = { { "pF", 1e-12 }, { "nF", 1e-9 }, { "\xC2\xB5" "F", 1e-6 }, { "mF", 1e-3 } };
RDE_INTERNAL const fude_zoom_page_unit FUDE_ZOOM_PAGE_HENRYS[] = { { "\xC2\xB5" "H", 1e-6 }, { "mH", 1e-3 }, { "H", 1.0 } };
RDE_INTERNAL const fude_zoom_page_unit FUDE_ZOOM_PAGE_VOLTS[]  = { { "mV", 1e-3 }, { "V", 1.0 } };
RDE_INTERNAL const fude_zoom_page_unit FUDE_ZOOM_PAGE_AMPS[]   = { { "\xC2\xB5" "A", 1e-6 }, { "mA", 1e-3 }, { "A", 1.0 } };
RDE_INTERNAL const fude_zoom_page_unit FUDE_ZOOM_PAGE_HERTZ[]  = { { "Hz", 1.0 }, { "kHz", 1e3 }, { "MHz", 1e6 } };
RDE_INTERNAL const fude_zoom_page_unit FUDE_ZOOM_PAGE_RPM[]    = { { "rpm", 1.0 } };
RDE_INTERNAL const fude_zoom_page_unit FUDE_ZOOM_PAGE_GRAMS[]  = { { "g", 1e-3 }, { "kg", 1.0 } };
// The LED colours Value offers, as they are written into the part's text (fude_zoom_page_led_color reads them).
RDE_INTERNAL const c8* const FUDE_ZOOM_PAGE_LED_WORDS[5] = { "red", "green", "blue", "yellow", "white" };

// What Value asks of object _o: its units (and how many), whether its number and unit are spaced ("30 rpm"; when not,
// what its text says after them is kept: "5V 50Hz" keeps " 50Hz"), and a motor's way round; an LED or a logic probe:
// its colour (*_led, with no units). False: nothing.
RDE_INTERNAL b8 fude_zoom_page_value_kind(const fude_zoom_page* _page, u32 _o, const fude_zoom_page_unit** _units, u32* _count, b8* _spaced, b8* _motor, b8* _led) {
    *_units = NULL; *_count = 0u; *_spaced = false; *_motor = false; *_led = false;
    const fude_zoom_part* _part = _o < fude_zoom_scene_object_count(&_page->scene) ? fude_zoom_part_of(&_page->scene, _o) : NULL;
    // Its units are table _t, all of it.
    #define FZPG_UNITS(_t) do { *_units = (_t); *_count = (u32)(sizeof(_t) / sizeof((_t)[0])); } while(0)
    if(_part != NULL) {
        switch(_part->model) {
        case FUDE_ZOOM_MODEL_RESISTOR: case FUDE_ZOOM_MODEL_POT:                      FZPG_UNITS(FUDE_ZOOM_PAGE_OHMS);   return true;
        case FUDE_ZOOM_MODEL_CAPACITOR:                                               FZPG_UNITS(FUDE_ZOOM_PAGE_FARADS); return true;
        case FUDE_ZOOM_MODEL_INDUCTOR:                                                FZPG_UNITS(FUDE_ZOOM_PAGE_HENRYS); return true;
        case FUDE_ZOOM_MODEL_VSOURCE: case FUDE_ZOOM_MODEL_BATTERY: case FUDE_ZOOM_MODEL_ACSOURCE:
        case FUDE_ZOOM_MODEL_ZENER: case FUDE_ZOOM_MODEL_RAIL:                        FZPG_UNITS(FUDE_ZOOM_PAGE_VOLTS);  return true;
        case FUDE_ZOOM_MODEL_ISOURCE: case FUDE_ZOOM_MODEL_FUSE:                      FZPG_UNITS(FUDE_ZOOM_PAGE_AMPS);   return true;
        case FUDE_ZOOM_MODEL_CLOCK:                                                   FZPG_UNITS(FUDE_ZOOM_PAGE_HERTZ);  return true;
        case FUDE_ZOOM_MODEL_LED: case FUDE_ZOOM_MODEL_LOGIC_OUT:                     *_led = true;                     return true;   // A colour.
        default: return false;
        }
    }
    const fude_zoom_mech_part* _m = _o < fude_zoom_scene_object_count(&_page->scene) ? fude_zoom_mech_of(&_page->scene, _o) : NULL;
    if(_m != NULL && _m->kind == FUDE_ZOOM_MECH_MOTOR) {
        FZPG_UNITS(FUDE_ZOOM_PAGE_RPM);
        *_spaced = *_motor = true;
        return true;
    }
    if(_m != NULL && (_m->kind == FUDE_ZOOM_MECH_WEIGHT || _m->kind == FUDE_ZOOM_MECH_CRATE)) {
        FZPG_UNITS(FUDE_ZOOM_PAGE_GRAMS);
        *_spaced = true;
        return true;
    }
    #undef FZPG_UNITS
    return false;
}

// See page_parts.h.
u32 fude_zoom_page_value_lone(const fude_zoom_page* _page) {
    const u32 _o = fude_zoom_page_lone_symbol(_page);
    const fude_zoom_page_unit* _u;
    u32 _n;
    b8 _sp, _mo, _led;
    return _o != FUDE_ZOOM_NONE && fude_zoom_page_value_kind(_page, _o, &_u, &_n, &_sp, &_mo, &_led) ? _o : FUDE_ZOOM_NONE;
}

// Gives the part Value is open on its text _text (the part made again: one undo step), and lets go of it.
RDE_INTERNAL void fude_zoom_page_value_set(fude_zoom_page* _page, const c8* _text) {
    if(_page->value_object == FUDE_ZOOM_NONE) {
        return;
    }
    _page->symbol_editing = _page->value_object;
    _page->value_object   = FUDE_ZOOM_NONE;
    fude_zoom_page_symbol_text_commit(_page, _text);
}

// OK on the card: the number in its unit (_unit, an index into its units), the motor's way (1 clockwise), written as
// the part's text. A negative number keeps its sign, but a motor's turns the other way only by its way.
RDE_INTERNAL void fude_zoom_page_value_done(any _self, f64 _number, u32 _unit, u32 _way) {
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    const u32 _o = _page->value_object;
    const fude_zoom_page_unit* _units;
    u32 _count;
    b8 _spaced, _motor, _led;
    if(_o == FUDE_ZOOM_NONE || !fude_zoom_page_value_kind(_page, _o, &_units, &_count, &_spaced, &_motor, &_led) || _count == 0u) {
        return;
    }
    // What its text says after its value, kept: an AC source's frequency.
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _c = fude_zoom_scene_shape_numbers(&_page->scene, _o, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    c8 _was[FUDE_ZOOM_SYMBOL_TEXT];
    fude_zoom_symbol_text(_n, _c, _was, sizeof(_was));
    const c8* _rest = "";
    if(!_motor && !_spaced) {
        const c8* _sp = rde_strchr(_was, ' ');
        _rest = _sp != NULL && _sp[1] != 0 ? _sp : "";
    }
    // The sign written with the rest (not put in front after, which would push the text's last byte past its end when
    // the text is as long as it can be), with room for all of it, then cut to a part's text before a whole character.
    c8 _built[FUDE_ZOOM_SYMBOL_TEXT + 64];
    rde_snprintf(_built, sizeof(_built), "%s%.6g%s%s%s%s", !_motor && _number < 0.0 ? "-" : "", rde_math_abs_f64(_number), _spaced ? " " : "",
                 _units[_unit < _count ? _unit : 0u].name, _motor && _way == 1u ? " cw" : "", _rest);
    c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
    fude_utf8_set(_text, sizeof(_text), _built);
    fude_zoom_page_value_set(_page, _text);
}

// See page_parts.h: the colour's word becomes the LED's text.
void fude_zoom_page_value_led(any _self, u32 _index) {
    fude_zoom_page_value_set((fude_zoom_page*)_self, FUDE_ZOOM_PAGE_LED_WORDS[_index % 5u]);
}

// See page_parts.h. A logic probe's colour lit chosen: its light (props.h) made again, its text and its name as they
// were. One undo step.
void fude_zoom_page_value_probe(any _self, u32 _index) {
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    fude_zoom_scene* _s = &_page->scene;
    const u32 _o = _page->value_object;
    if(_o >= fude_zoom_scene_object_count(_s) || !(fude_zoom_scene_object(_s, _o)->flags & FUDE_ZOOM_FLAG_ALIVE)) {
        return;
    }
    u32 _died[1], _born[1], _nd = 0;
    const u32 _old = fude_zoom_props_find(_s, _o, FUDE_ZOOM_PROPS_LIGHT);
    if(_old != FUDE_ZOOM_NONE) {
        fude_zoom_scene_set_alive(_s, _old, false);
        _died[_nd++] = _old;
    }
    _born[0] = fude_zoom_props_add_light(_s, _o, fude_zoom_page_led_color(FUDE_ZOOM_PAGE_LED_WORDS[_index % 5u]));
    const fude_zoom_object* _t = fude_zoom_scene_object(_s, _o);
    fude_zoom_history_push(_s, _t->frame, _t->box, _died, _nd, _born, 1u);
}

// See page_parts.h. An LED or a probe gets the colours' choice card; anything else Value's card, its number shown in
// the largest of its units it is at least one of (volts: V from 1 V, else mV), a motor's way read from its text as
// mech.c turns it (fude_zoom_mech_rpm: its number's sign and a whole word for the way; "ccw" is not clockwise).
void fude_zoom_page_value_selection(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_zoom_page* _page = fude_zoom_page_the;
    const u32 _o = _page != NULL && _page->open ? fude_zoom_page_value_lone(_page) : FUDE_ZOOM_NONE;
    if(_o == FUDE_ZOOM_NONE) {
        return;
    }
    const fude_zoom_page_unit* _units;
    u32 _count;
    b8 _spaced, _motor, _led;
    fude_zoom_page_value_kind(_page, _o, &_units, &_count, &_spaced, &_motor, &_led);
    f64 _n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 _c = fude_zoom_scene_shape_numbers(&_page->scene, _o, _n, FUDE_ZOOM_SHAPE_NUMBERS);
    const u32 _kind = fude_zoom_symbol_kind_of(_n, _c);
    const c8* _title = _kind < FUDE_ZOOM_SYMBOL_KINDS ? fude_text(FUDE_ZOOM_PAGE_SYMBOL_NAMES[_kind]) : "";
    c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
    fude_zoom_symbol_text(_n, _c, _text, sizeof(_text));
    _page->value_object = _o;
    if(_led) {
        const c8* _labels[5] = { fude_text(FUDE_TEXT_ZOOM_LED_RED), fude_text(FUDE_TEXT_ZOOM_LED_GREEN), fude_text(FUDE_TEXT_ZOOM_LED_BLUE),
                                 fude_text(FUDE_TEXT_ZOOM_LED_YELLOW), fude_text(FUDE_TEXT_ZOOM_LED_WHITE) };
        const fude_zoom_part* _part = fude_zoom_part_of(&_page->scene, _o);
        fude_zoom_choice_open(&_page->choice_form, _title, _labels, NULL, 5u,
                              _part != NULL && _part->model == FUDE_ZOOM_MODEL_LOGIC_OUT ? fude_zoom_page_value_probe : fude_zoom_page_value_led, _page);
        return;
    }
    // Its value now in the SI unit (a mass in kilograms: "500 g" is 0.5), in the largest of its units it is one of.
    f64 _v = 0.0;
    const b8 _has = fude_zoom_circuit_value(_text, 0u, &_v);
    if(_has && _units == FUDE_ZOOM_PAGE_GRAMS) {
        _v = rde_strchr(_text, 'g') != NULL ? _v / 1000.0 : _v;
    }
    u32 _way = 0u;
    if(_motor) {
        _way = fude_zoom_mech_rpm(_text, 0.0) < 0.0 ? 1u : 0u;   // (1: clockwise; the card's number has no sign)
        _v = rde_math_abs_f64(_v);
    }
    u32 _unit = 0u;
    for(u32 _i = 0; _i < _count; _i++) {
        if(rde_math_abs_f64(_v) >= _units[_i].times * 0.999999) {
            _unit = _i;
        }
    }
    if(_units == FUDE_ZOOM_PAGE_VOLTS && rde_math_abs_f64(_v) >= 1e-3) {
        _unit = rde_math_abs_f64(_v) >= 1.0 ? 1u : 0u;
    }
    c8 _number[24];
    rde_snprintf(_number, sizeof(_number), "%.6g", _has ? _v / _units[_unit].times : 0.0);
    const c8* _names[FUDE_ZOOM_VALUE_UNITS];
    for(u32 _i = 0; _i < _count && _i < FUDE_ZOOM_VALUE_UNITS; _i++) {
        _names[_i] = _units[_i].name;
    }
    const c8* _ways[2] = { fude_text(FUDE_TEXT_ZOOM_TURN_CCW), fude_text(FUDE_TEXT_ZOOM_TURN_CW) };
    fude_zoom_value_open(&_page->value_form, _title, _number, _names, _count, _unit, _motor ? _ways : NULL, _motor ? 2u : 0u, _way, fude_zoom_page_value_done, _page);
}

// --- aligned as it moves: parts' pins (Electronics), parts' holes and middles (Mechanisms) -----------------------
//
// What the lasso holds, moved: each of its points that join things (a part's pins and its middle, a mechanism part's
// holes) pulled level or plumb with one of the others' (theirs, a wire's corners and loose ends) within a few points,
// the nearest each way, held a little further once it is (no flicker at the edge); in Electronics, a way nothing lines
// up on its pins land on the parts' own grid. A dashed line across the screen through what it lines up with, a ring
// on it. The wires its parts keep are drawn routed to them as they go (hidden where they were), as they will be made
// when it is let go. Everything here is in screen points.

#define FUDE_ZOOM_PAGE_ALIGN_REACH   9.0     // points: how near to be pulled level or plumb
#define FUDE_ZOOM_PAGE_ALIGN_MOVERS  96u     // a selection's points, at most
#define FUDE_ZOOM_PAGE_ALIGN_TARGETS 6000u   // the others' on the screen, at most

// Adds a thing's points that line up, on screen, to _out (until it holds _max): a part's pins and its middle; a
// mechanism part's holes and its middle; with _wires, a wire's corners and its loose ends.
RDE_INTERNAL void fude_zoom_page_align_points(fude_zoom_page* _page, u32 _o, rde_arr* _out, u32 _max, b8 _wires) {
    const fude_zoom_scene* _s = &_page->scene;
    const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _o);
    if(!(_ob->flags & FUDE_ZOOM_FLAG_ALIVE) || _ob->kind != FUDE_ZOOM_KIND_SHAPE) {
        return;
    }
    const fude_zoom_sim _fs = fude_zoom_page_frame_sim(_page, _ob->frame);
    if(_ob->channels == FUDE_ZOOM_SHAPE_WIRE) {
        if(!_wires) {
            return;
        }
        f64 _n[FUDE_ZOOM_WIRE_NUMBERS];
        const u32 _c = fude_zoom_scene_shape_numbers(_s, _o, _n, FUDE_ZOOM_WIRE_NUMBERS);
        fude_zoom_v2 _p[FUDE_ZOOM_WIRE_POINTS];
        fude_zoom_id _from, _to;
        i32 _fp, _tp;
        const u32 _k = fude_zoom_wire_of(_n, _c, _p, &_from, &_fp, &_to, &_tp);
        const fude_zoom_sim _all = fude_zoom_sim_compose(_fs, fude_zoom_object_sim(_ob));
        for(u32 _i = 0; _i < _k && (u32)rde_arr_length(_out) < _max; _i++) {
            // Its corners; its ends where they are on no pin: a pin's own is there already.
            if((_i == 0u && _from != 0u && _fp >= 0) || (_i + 1u == _k && _to != 0u && _tp >= 0)) {
                continue;
            }
            const fude_zoom_v2 _q = fude_zoom_sim_apply(_all, _p[_i]);
            rde_arr_add(_out, (any)&_q);
        }
        return;
    }
    if(_ob->channels != FUDE_ZOOM_SHAPE_SYMBOL) {
        return;
    }
    const fude_zoom_part* _part = fude_zoom_part_of(_s, _o);
    const fude_zoom_mech_part* _m = _part == NULL ? fude_zoom_mech_of(_s, _o) : NULL;
    if(_part == NULL && _m == NULL) {
        return;
    }
    const fude_zoom_v2 _mid = fude_zoom_sim_apply(_fs, fude_zoom_scene_place_of(_s, _o).t);
    if((u32)rde_arr_length(_out) < _max) {
        rde_arr_add(_out, (any)&_mid);
    }
    if(_part != NULL) {
        for(u32 _k = 0; _k < _part->pin_count && (u32)rde_arr_length(_out) < _max; _k++) {
            fude_zoom_v2 _q;
            if(fude_zoom_part_pin_at(_s, _o, _k, &_q)) {
                const fude_zoom_v2 _p = fude_zoom_sim_apply(_fs, _q);
                rde_arr_add(_out, (any)&_p);
            }
        }
        return;
    }
    f64 _n[3];
    if(fude_zoom_scene_shape_numbers(_s, _o, _n, 3u) < 3u) {
        return;
    }
    const fude_zoom_sim _all = fude_zoom_sim_compose(_fs, fude_zoom_object_sim(_ob));
    for(u32 _h = 0; _h < _m->hole_count && (u32)rde_arr_length(_out) < _max; _h++) {
        const fude_zoom_v2 _p = fude_zoom_sim_apply(_all, (fude_zoom_v2){ (f64)_m->holes[_h][0] * _n[1], (f64)_m->holes[_h][1] * _n[2] });
        rde_arr_add(_out, (any)&_p);
    }
}

// Puts the others' points (every part's, mechanism part's and wire's not in _mine, a flag for each object) into
// align_targets: those within 1.2 of the screen's half size only, at most FUDE_ZOOM_PAGE_ALIGN_TARGETS.
RDE_INTERNAL void fude_zoom_page_align_targets(fude_zoom_page* _page, const u8* _mine) {
    fude_zoom_scene* _s = &_page->scene;
    rde_arr_clear(&_page->align_targets);
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    const u32 _no = fude_zoom_scene_object_count(_s);
    for(u32 _o = 0; _o < _no; _o++) {
        const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _o);
        if((_mine != NULL && _mine[_o]) || !(_ob->flags & FUDE_ZOOM_FLAG_ALIVE) || _ob->kind != FUDE_ZOOM_KIND_SHAPE ||
           (_ob->channels != FUDE_ZOOM_SHAPE_SYMBOL && _ob->channels != FUDE_ZOOM_SHAPE_WIRE) || fude_zoom_scene_hides(_s, _ob) ||
           !fude_zoom_scene_frame_shown(_s, _ob->frame)) {
            continue;
        }
        const u32 _was = (u32)rde_arr_length(&_page->align_targets);
        fude_zoom_page_align_points(_page, _o, &_page->align_targets, FUDE_ZOOM_PAGE_ALIGN_TARGETS, true);
        // Only what is on the screen: what lines up off it, nobody sees.
        fude_zoom_v2* _t = (fude_zoom_v2*)_page->align_targets.memory;
        u32 _kept = _was;
        for(u32 _k = _was; _k < (u32)rde_arr_length(&_page->align_targets); _k++) {
            if(rde_math_abs_f64(_t[_k].x) <= _half.x * 1.2 && rde_math_abs_f64(_t[_k].y) <= _half.y * 1.2) {
                _t[_kept++] = _t[_k];
            }
        }
        rde_arr_resize(&_page->align_targets, _kept);
    }
}

// A drag begun: what it holds that lines up, the others', the grid; the wires its parts keep, lifted off the canvas
// (the selection's lifted flags) and kept with their ends in align_wires. Lining up is done only in the Electronics
// and Mechanisms topics; the wires are lifted in any.
RDE_INTERNAL void fude_zoom_page_align_begin(fude_zoom_page* _page) {
    fude_zoom_selection* _sel = &_page->selection;
    fude_zoom_scene* _s = &_page->scene;
    _page->align_from = _sel->grab_at;
    _page->align_x = _page->align_y = RDE_NAN_F64;
    _page->align_grid = 0.0;
    rde_arr_clear(&_page->align_movers);
    rde_arr_clear(&_page->align_targets);
    rde_arr_clear(&_page->align_wires);
    const u32 _no = fude_zoom_scene_object_count(_s);
    rde_arr _mine_arr = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_mine_arr, _no);
    u8* _mine = (u8*)_mine_arr.memory;
    u32 _first_part = FUDE_ZOOM_NONE;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_sel->picks); _i++) {
        const u32 _o = ((const fude_zoom_pick*)_sel->picks.memory)[_i].object;
        if(_o < _no) {
            _mine[_o] = 1u;
            if(_first_part == FUDE_ZOOM_NONE && fude_zoom_part_of(_s, _o) != NULL) {
                _first_part = _o;
            }
        }
    }
    // The wires its parts keep (not themselves held): lifted, drawn routed as it goes. lifted holds a flag for every
    // object, grown to the scene's count.
    while((u32)rde_arr_length(&_sel->lifted) < _no) {
        const u8 _zero = 0u;
        rde_arr_add(&_sel->lifted, (any)&_zero);
    }
    for(u32 _w = 0; _w < _no; _w++) {
        const fude_zoom_object* _ob = fude_zoom_scene_object(_s, _w);
        if(_mine[_w] || !(_ob->flags & FUDE_ZOOM_FLAG_ALIVE) || _ob->kind != FUDE_ZOOM_KIND_SHAPE || _ob->channels != FUDE_ZOOM_SHAPE_WIRE) {
            continue;
        }
        f64 _n[FUDE_ZOOM_WIRE_NUMBERS];
        const u32 _c = fude_zoom_scene_shape_numbers(_s, _w, _n, FUDE_ZOOM_WIRE_NUMBERS);
        fude_zoom_v2 _p[FUDE_ZOOM_WIRE_POINTS];
        fude_zoom_id _ids[2];
        i32 _pins[2];
        const u32 _k = fude_zoom_wire_of(_n, _c, _p, &_ids[0], &_pins[0], &_ids[1], &_pins[1]);
        if(_k < 2u) {
            continue;
        }
        const fude_zoom_sim _all = fude_zoom_sim_compose(fude_zoom_page_frame_sim(_page, _ob->frame), fude_zoom_object_sim(_ob));
        fude_zoom_page_drag_wire _dw;
        rde_memset(&_dw, 0, sizeof(_dw));
        _dw.wire = _w;
        b8 _kept_by_held = false;
        for(u32 _e = 0; _e < 2u; _e++) {
            const fude_zoom_v2 _end = _p[_e == 0u ? 0u : _k - 1u];
            _dw.part[_e] = _ids[_e] != 0u && _pins[_e] >= 0 ? fude_zoom_wire_part(_s, _ids[_e], _pins[_e], _ob->frame, fude_zoom_sim_apply(fude_zoom_object_sim(_ob), _end)) : FUDE_ZOOM_NONE;
            _dw.pin[_e] = _pins[_e];
            _dw.moving[_e] = _dw.part[_e] != FUDE_ZOOM_NONE && _dw.part[_e] < _no && _mine[_dw.part[_e]];
            _dw.at[_e] = fude_zoom_sim_apply(_all, _end);
            fude_zoom_v2 _q;
            if(_dw.part[_e] != FUDE_ZOOM_NONE && fude_zoom_part_pin_at(_s, _dw.part[_e], (u32)_dw.pin[_e], &_q)) {
                _dw.at[_e] = fude_zoom_sim_apply(fude_zoom_page_frame_sim(_page, fude_zoom_scene_object(_s, _dw.part[_e])->frame), _q);
            }
            _kept_by_held = _kept_by_held || _dw.moving[_e];
        }
        if(_kept_by_held) {
            rde_arr_add(&_page->align_wires, (any)&_dw);
            ((u8*)_sel->lifted.memory)[_w] = 1u;
            _mine[_w] = 1u;   // Its corners not lined up with: it is going.
        }
    }
    if(_page->topic == FUDE_ZOOM_TOPIC_ELECTRONICS || _page->topic == FUDE_ZOOM_TOPIC_MECHANISMS) {
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_sel->picks); _i++) {
            const u32 _o = ((const fude_zoom_pick*)_sel->picks.memory)[_i].object;
            if(_o < _no) {
                fude_zoom_page_align_points(_page, _o, &_page->align_movers, FUDE_ZOOM_PAGE_ALIGN_MOVERS, false);
            }
        }
        if(rde_arr_length(&_page->align_movers) > 0u) {
            fude_zoom_page_align_targets(_page, _mine);
        }
        // Electronics: the first part's grid (its pins on it), halved until it is 40 points or less on screen; under 6
        // points, none.
        if(_page->topic == FUDE_ZOOM_TOPIC_ELECTRONICS && _first_part != FUDE_ZOOM_NONE) {
            const fude_zoom_object* _po = fude_zoom_scene_object(_s, _first_part);
            const fude_zoom_sim _fs = fude_zoom_page_frame_sim(_page, _po->frame);
            f64 _step = fude_zoom_part_unit(_s, _first_part) * fude_zoom_sim_scale(_fs);
            while(_step > 40.0) {
                _step *= 0.5;
            }
            _page->align_grid   = _step >= 6.0 ? _step : 0.0;
            _page->align_origin = fude_zoom_sim_apply(_fs, (fude_zoom_v2){ 0.0, 0.0 });
            if(rde_math_abs_f64(_fs.b) > 1e-9 * rde_math_abs_f64(_fs.a)) {
                _page->align_grid = 0.0;   // A turned frame: its grid is not the screen's way.
            }
        }
    }
    rde_arr_free(&_mine_arr);
}

// See page_parts.h. A new drag (its grab point changed) begins again; a move is then pulled into line
// (fude_zoom_snap_align, held out to 1.5 times the reach), or onto the grid the ways nothing lines up.
void fude_zoom_page_align(fude_zoom_page* _page) {
    fude_zoom_selection* _sel = &_page->selection;
    if(_sel->grab < FUDE_ZOOM_GRAB_MOVE) {
        return;
    }
    if(_page->align_from.x != _sel->grab_at.x || _page->align_from.y != _sel->grab_at.y) {
        fude_zoom_page_align_begin(_page);
    }
    if(_sel->grab != FUDE_ZOOM_GRAB_MOVE || rde_arr_length(&_page->align_movers) == 0u) {
        return;
    }
    fude_zoom_snap_align((const fude_zoom_v2*)_page->align_movers.memory, (u32)rde_arr_length(&_page->align_movers),
                         (const fude_zoom_v2*)_page->align_targets.memory, (u32)rde_arr_length(&_page->align_targets), _sel->drag.tx, _sel->drag.ty,
                         FUDE_ZOOM_PAGE_ALIGN_REACH, FUDE_ZOOM_PAGE_ALIGN_REACH * 1.5, &_sel->drag.tx, &_sel->drag.ty, &_page->align_x, &_page->align_y);
    // A way nothing lines up: its first point on the grid.
    if(_page->align_grid > 0.0) {
        const fude_zoom_v2 _m = ((const fude_zoom_v2*)_page->align_movers.memory)[0];
        if(rde_math_is_nan_f64(_page->align_x)) {
            _sel->drag.tx = fude_zoom_snap_grid(_m.x + _sel->drag.tx, _page->align_origin.x, _page->align_grid) - _m.x;
        }
        if(rde_math_is_nan_f64(_page->align_y)) {
            _sel->drag.ty = fude_zoom_snap_grid(_m.y + _sel->drag.ty, _page->align_origin.y, _page->align_grid) - _m.y;
        }
    }
}

// See page_parts.h. The lines are dashed (6 on, 6 off).
void fude_zoom_page_render_align(fude_zoom_page* _page, fude_zoom_v2 _half) {
    const fude_zoom_selection* _sel = &_page->selection;
    const fude_theme* _t = fude_theme_active();
    const fude_zoom_scene* _s = &_page->scene;
    if(_sel->grab >= FUDE_ZOOM_GRAB_MOVE) {
        const fude_zoom_page_drag_wire* _dw = (const fude_zoom_page_drag_wire*)_page->align_wires.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->align_wires); _i++) {
            fude_zoom_v2 _e[2];
            for(u32 _k = 0; _k < 2u; _k++) {
                _e[_k] = _dw[_i].moving[_k] ? fude_zoom_sim_apply(_sel->drag, _dw[_i].at[_k]) : _dw[_i].at[_k];
            }
            fude_zoom_v2 _r[6];
            const u32 _m = fude_zoom_page_wire_way(_page, _e[0], _dw[_i].part[0], _dw[_i].pin[0], _e[1], _dw[_i].part[1], _dw[_i].pin[1], _r);
            const fude_zoom_object* _wo = fude_zoom_scene_object(_s, _dw[_i].wire);
            const f32 _w = (f32)rde_math_max_f64((f64)_wo->radius * fude_zoom_sim_scale(fude_zoom_page_frame_sim(_page, _wo->frame)), 0.8);
            rde_vec_2F _sp[6];
            f32 _sr[6];
            for(u32 _k = 0; _k < _m; _k++) {
                _sp[_k] = (rde_vec_2F){ (f32)_r[_k].x, (f32)_r[_k].y };
                _sr[_k] = _w;
            }
            rde_rendering_2d_draw_stroke(_sp, _sr, _m, fude_theme_resolve(_wo->color));
        }
    }
    if(_sel->grab != FUDE_ZOOM_GRAB_MOVE) {
        return;
    }
    const rde_color _ink = _t->accent;
    if(!rde_math_is_nan_f64(_page->align_x)) {
        for(f64 _y = -_half.y; _y < _half.y; _y += 12.0) {
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_page->align_x, (f32)_y }, (rde_vec_2F){ (f32)_page->align_x, (f32)rde_math_min_f64(_y + 6.0, _half.y) }, _ink, 1.2f);
        }
    }
    if(!rde_math_is_nan_f64(_page->align_y)) {
        for(f64 _x = -_half.x; _x < _half.x; _x += 12.0) {
            rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_x, (f32)_page->align_y }, (rde_vec_2F){ (f32)rde_math_min_f64(_x + 6.0, _half.x), (f32)_page->align_y }, _ink, 1.2f);
        }
    }
    // A ring on what each line goes through: the target its points line up with.
    const fude_zoom_v2* _tg = (const fude_zoom_v2*)_page->align_targets.memory;
    for(u32 _j = 0; _j < (u32)rde_arr_length(&_page->align_targets); _j++) {
        if((!rde_math_is_nan_f64(_page->align_x) && rde_math_abs_f64(_tg[_j].x - _page->align_x) < 1e-6) || (!rde_math_is_nan_f64(_page->align_y) && rde_math_abs_f64(_tg[_j].y - _page->align_y) < 1e-6)) {
            rde_rendering_2d_draw_circle_with_border((rde_vec_2F){ (f32)_tg[_j].x, (f32)_tg[_j].y }, 5.0f, 14u, (rde_color){ 0, 0, 0, 0 }, 1.6f, _ink, NULL);
        }
    }
}
