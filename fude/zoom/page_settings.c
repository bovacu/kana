// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"
#include "drawing/app/handwriting.h"

// ===========================================================================
// Page, Settings' sections: Measurements (units, the true size's calibration, the printer's check, the kerf),
// the offset pad (an instrument moved exact lengths from where it snapped), the core's Handwriting (Google ML Kit's
// switch: drawing/app/handwriting.h), and Display (the frame rate on the screen, in every build).
//
// Each section is four functions (build, layout, refresh, restyle) handed to the core in FUDE_ZOOM_PAGE_SECTIONS at
// the end of the file; they find the page through fude_zoom_page_the. What Settings changes here is saved with the
// settings (page_export.c gathers them, and the session sees them change).
// for measuring the canvas, not a setting anyone using the app needs).
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// --- Settings' Measurements, and the true size -------------------------------------------

// A bank card's size in millimetres (ISO/IEC 7810 ID-1): what everyone has to hand, so its outline drawn on the screen
// is matched against a real one to calibrate the true size.
#define FUDE_ZOOM_PAGE_CARD_W_MM 85.60
#define FUDE_ZOOM_PAGE_CARD_H_MM 53.98

// The units' buttons' names, in FUDE_ZOOM_UNIT_ order.
static const c8* const FUDE_ZOOM_PAGE_UNIT_NAMES[4] = { "mm", "cm", "in", "ft \xC2\xB7 in" };

// A unit's button (its index in _user_data): the page's lengths are said in it from now on.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_page_on_units(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    if(fude_zoom_page_the != NULL) {
        fude_zoom_page_set_units(fude_zoom_page_the, (u8)(uintptr_t)_user_data);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Calibrate: Settings closes, and the card's outline comes up on the page.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_calibrate(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _page->app->ui->side.settings_open = false;
    _page->app->ui->side.open          = false;
    // A first guess: a tablet's 160 points an inch.
    _page->calibrate_mm = _page->true_mm_per_point > 0.0f ? (f64)_page->true_mm_per_point : 25.4 / 160.0;
    _page->calibrating  = true;
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The calibration's keys (_user_data): 0 to 3 make the card smaller or bigger by 5% or 0.5% (kept between 0.02 and 2
// millimetres a point); 5 keeps the size tried as the true size; any other ends the calibration without keeping it.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_calibrate_key(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    // The card drawn bigger means a point is fewer millimetres.
    static const f64 _by[4] = { 1.05, 1.005, 1.0 / 1.005, 1.0 / 1.05 };
    const u32 _k = (u32)(uintptr_t)_user_data;
    if(_k < 4u) {
        _page->calibrate_mm = rde_math_min_f64(rde_math_max_f64(_page->calibrate_mm * _by[_k], 0.02), 2.0);
    } else {
        if(_k == 5u) {
            // Saved with the settings (the session sees it change).
            _page->true_mm_per_point = (f32)_page->calibrate_mm;
        }
        _page->calibrating = false;
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// While calibrating: the page covered, the bank card's outline at the size being tried, and how to match it below.
void fude_zoom_page_render_calibration(fude_zoom_page* _page) {
    if(!_page->calibrating) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ (f32)_half.x * 2.0f, (f32)_half.y * 2.0f }, _t->page);
    const f32 _w = (f32)(FUDE_ZOOM_PAGE_CARD_W_MM / _page->calibrate_mm), _h = (f32)(FUDE_ZOOM_PAGE_CARD_H_MM / _page->calibrate_mm);
    const rde_vec_2F _c = { 0.0f, 40.0f };
    rde_rendering_2d_draw_rounded_rectangle_with_border(_c, (rde_vec_2F){ _w, _h }, 0.12f, 12u, _t->tint, 2.0f, _t->accent, NULL);
    if(_page->app->font != NULL) {
        const c8* _say = fude_text(FUDE_TEXT_ZOOM_CALIBRATE_HOW);
        const f32 _px  = 15.0f;
        fude_draw_text_wrap(_page->app->font, _page->app->font_px, _say, (f32)(-rde_math_min_f64(_half.x - 24.0, 260.0)), _c.y + _h * 0.5f + 60.0f, _px,
                            (f32)rde_math_min_f64(_half.x * 2.0 - 48.0, 520.0), _px * 1.35f, _t->text);
    }
}

// The true size: the view in the home frame where a millimetre drawn is a millimetre on the glass, round the same
// middle. A true size not calibrated yet is taken as 1e-6 mm a point, so nothing divides by 0.
fude_zoom_camera fude_zoom_page_true_view(const fude_zoom_page* _page) {
    const fude_zoom_scene* _s    = &_page->scene;
    const u32              _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const fude_zoom_v2     _mid  = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _s->camera.frame, _home), _s->camera.at);
    return (fude_zoom_camera){ _home, _mid, FUDE_ZOOM_PAGE_MM_PER_UNIT / (f64)rde_math_max_f32(_page->true_mm_per_point, 1e-6f) };
}

// The printer's check (export.h): its page printed, then each line's length as measured. The second line typed: both
// scales are kept when each is within 0.8 to 1.25 of the line's true length, else a notice says it is too far off.
RDE_INTERNAL void fude_zoom_page_print_y_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _value) {
    RDE_UNUSED(_text); RDE_UNUSED(_unit);
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    const f64 _x = _page->print_measured_x / FUDE_ZOOM_CHECK_X_MM, _y = _value / FUDE_ZOOM_CHECK_Y_MM;
    if(!(_x > 0.8 && _x < 1.25 && _y > 0.8 && _y < 1.25)) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PRINTER_FAR));
        return;
    }
    _page->print_x = (f32)_x;   // Saved with the settings (the session sees them change).
    _page->print_y = (f32)_y;
    fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PRINTER_DONE));
}

// The first line's length typed: kept, and the numpad asks for the second.
RDE_INTERNAL void fude_zoom_page_print_x_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _value) {
    RDE_UNUSED(_text); RDE_UNUSED(_unit);
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    _page->print_measured_x = _value;
    c8 _start[32];
    rde_snprintf(_start, sizeof(_start), "%.0f", FUDE_ZOOM_CHECK_Y_MM);
    fude_zoom_numpad_open(&_page->numpad, _start, FUDE_ZOOM_UNIT_MM, false, NULL, fude_zoom_page_print_y_typed, _page);
    fude_zoom_numpad_ask(&_page->numpad, fude_text(FUDE_TEXT_ZOOM_PRINTER_ASK_Y));
}

// Asks for the printer's check's first line on the numpad, starting at its true length.
void fude_zoom_page_print_check_ask(fude_zoom_page* _page) {
    c8 _start[32];
    rde_snprintf(_start, sizeof(_start), "%.0f", FUDE_ZOOM_CHECK_X_MM);
    fude_zoom_numpad_open(&_page->numpad, _start, FUDE_ZOOM_UNIT_MM, false, NULL, fude_zoom_page_print_x_typed, _page);
    // The printed page says how: print at 100%, then measure.
    fude_zoom_numpad_ask(&_page->numpad, fude_text(FUDE_TEXT_ZOOM_PRINTER_ASK_X));
}

// Check: Settings closes, the check's page is shared (or saved where chosen) and the numpad waits for its lines.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_page_on_printer(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _page->app->ui->side.settings_open = false;
    _page->app->ui->side.open          = false;
    fude_zoom_export_choose(_page->app, 99u);   // The printer's check: an index no Export choice has.
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The saw's kerf typed on the numpad: kept when it is 0 to 50 millimetres, otherwise ignored.
RDE_INTERNAL void fude_zoom_page_kerf_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _mm) {
    RDE_UNUSED(_text); RDE_UNUSED(_unit);
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    if(_mm >= 0.0 && _mm <= 50.0) {
        // 0 typed is a cut with no width: kept as next to nothing, since 0 means "never set".
        _page->kerf_mm = _mm > 0.0 ? (f32)_mm : 0.001f;
    }
}

// Change (the kerf's row): Settings closes and the numpad asks for the kerf, in the page's units.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_kerf(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _page->app->ui->side.settings_open = false;
    _page->app->ui->side.open          = false;
    c8 _text[48];
    fude_zoom_page_length_text(_page, fude_zoom_page_kerf(_page), _text, sizeof(_text));
    fude_zoom_numpad_open(&_page->numpad, _text, _page->units.unit, false, NULL, fude_zoom_page_kerf_typed, _page);
    fude_zoom_numpad_ask(&_page->numpad, fude_text(FUDE_TEXT_ZOOM_KERF));
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// --- the offset pad: an instrument moved exact lengths across and up from the point it snapped to ---

// Returns whether the instrument the pad moves is still out (not put away, its slot not another's since).
b8 fude_zoom_page_anchor_alive(const fude_zoom_page* _page) {
    if(_page->anchor_tool < 0 || _page->anchor_tool >= (i32)FUDE_ZOOM_INSTRUMENT_MOST) {
        return false;
    }
    const fude_zoom_instrument* _t = &_page->instruments.tools[_page->anchor_tool];
    return _t->shown && _t->serial == _page->anchor_serial;
}

// Puts the instrument back where its snapped point plus the offset says (that point followed in the drawing: panned
// or zoomed since, it goes back onto it), and lays it there.
void fude_zoom_page_anchor_apply(fude_zoom_page* _page) {
    fude_zoom_instruments* _ins = &_page->instruments;
    if(!fude_zoom_page_anchor_alive(_page) || _page->anchor_frame >= fude_zoom_scene_frame_count(&_page->scene)) {
        return;
    }
    fude_zoom_v2 _keys[FUDE_ZOOM_INSTRUMENT_KEYS];
    if(_page->anchor_key >= fude_zoom_instrument_keys(_ins, (u32)_page->anchor_tool, _keys)) {
        return;
    }
    const fude_zoom_v2 _ref = fude_zoom_sim_apply(fude_zoom_page_frame_sim(_page, _page->anchor_frame), _page->anchor_point);
    const f64 _mpp = rde_math_max_f64(fude_zoom_page_mm_per_point(_page), 1e-300);
    fude_zoom_instrument* _t = &_ins->tools[_page->anchor_tool];
    _t->at.x += _ref.x + _page->anchor_mm[0] / _mpp - _keys[_page->anchor_key].x;
    _t->at.y += _ref.y + _page->anchor_mm[1] / _mpp - _keys[_page->anchor_key].y;
    fude_zoom_page_instrument_stick(_page, (u32)_page->anchor_tool);   // Lying there now.
}

// Returns an arrow's step in millimetres: the first that is at least 4 points on the screen at this zoom. In metric,
// 1, 2 or 5 of a power of ten, from a hundredth of a millimetre; in inches or feet, from a 64th of an inch up to
// 100 feet.
f64 fude_zoom_page_anchor_step(const fude_zoom_page* _page) {
    const f64 _want = 4.0 * fude_zoom_page_mm_per_point(_page);
    if(_page->units.unit == FUDE_ZOOM_UNIT_IN || _page->units.unit == FUDE_ZOOM_UNIT_FT) {
        static const f64 _inches[] = { 1.0 / 64.0, 1.0 / 32.0, 1.0 / 16.0, 0.125, 0.25, 0.5, 1.0, 3.0, 6.0, 12.0, 24.0, 60.0, 120.0, 240.0, 600.0, 1200.0 };
        for(u32 _i = 0; _i < sizeof(_inches) / sizeof(_inches[0]); _i++) {
            if(_inches[_i] * 25.4 >= _want) {
                return _inches[_i] * 25.4;
            }
        }
        return 1200.0 * 25.4;
    }
    f64 _step = 0.01;
    for(u32 _i = 0; _i < 40u && _step < _want; _i++) {
        const f64 _ten = rde_math_pow_f64(10.0, rde_math_floor_f64(rde_math_log10_f64(_step)) );
        _step = _step < 2.0 * _ten - 1e-12 ? 2.0 * _ten : (_step < 5.0 * _ten - 1e-12 ? 5.0 * _ten : 10.0 * _ten);
    }
    return _step;
}

// Writes an axis's offset as the pad shows it: "X +12 mm" (_axis 0) or "Y −5 mm" (1), in the page's units.
void fude_zoom_page_anchor_said(const fude_zoom_page* _page, u32 _axis, c8* _out, usize _size) {
    const f64 _mm = _page->anchor_mm[_axis];
    c8 _v[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format(rde_math_abs_f64(_mm), &_page->units, _v, sizeof(_v));
    rde_snprintf(_out, _size, "%s %s%s", _axis == 0u ? "X" : "Y", rde_math_abs_f64(_mm) < 1e-9 ? "" : (_mm > 0.0 ? "+" : "\xE2\x88\x92"), _v);
}

// An arrow pressed: the offset on _axis moved one step (_sign) onto the step's marks, and the instrument moved with it.
RDE_INTERNAL void fude_zoom_page_anchor_nudge(u32 _axis, f64 _sign) {
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || _page->anchor_tool < 0) {
        return;
    }
    const f64 _step = fude_zoom_page_anchor_step(_page);
    // Onto the step's marks.
    _page->anchor_mm[_axis] = rde_math_round_f64(_page->anchor_mm[_axis] / _step + _sign) * _step;
    fude_zoom_page_anchor_apply(_page);
}

// An offset typed on the numpad: the page's units become the unit it was typed in, and a finite length is kept and
// applied.
RDE_INTERNAL void fude_zoom_page_anchor_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _mm) {
    RDE_UNUSED(_text);
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    _page->units.unit = _unit;
    if(_page->anchor_tool >= 0 && rde_math_is_finite_f64(_mm)) {
        _page->anchor_mm[_page->anchor_typing] = _mm;   // Less than nothing: left, or down.
        fude_zoom_page_anchor_apply(_page);
    }
}

// The pad's buttons (_user_data): 0 ◀, 2 ▶, 3 ▼, 5 ▲ move it a step; 6 × lets the instrument go; 1 (X) and 4 (Y)
// ask for the offset on the numpad.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_anchor(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || _page->anchor_tool < 0) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    switch((u32)(uintptr_t)_user_data) {
        case 0: fude_zoom_page_anchor_nudge(0u, -1.0); break;   // ◀
        case 2: fude_zoom_page_anchor_nudge(0u,  1.0); break;   // ▶
        case 3: fude_zoom_page_anchor_nudge(1u, -1.0); break;   // ▼
        case 5: fude_zoom_page_anchor_nudge(1u,  1.0); break;   // ▲
        case 6: _page->anchor_tool = -1; break;                 // ×: done with it.
        default: {
            // X or Y tapped: typed.
            _page->anchor_typing = (u8)(_user_data == (any)(uintptr_t)1u ? 0u : 1u);
            c8 _text[48];
            fude_zoom_page_length_text(_page, _page->anchor_mm[_page->anchor_typing], _text, sizeof(_text));
            fude_zoom_numpad_open(&_page->numpad, _text, _page->units.unit, false, NULL, fude_zoom_page_anchor_typed, _page);
            fude_zoom_numpad_ask(&_page->numpad, fude_text(_page->anchor_typing == 0u ? FUDE_TEXT_ZOOM_OFFSET_X : FUDE_TEXT_ZOOM_OFFSET_Y));
        } break;
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The pad's grip: a hand pressing on it (with a margin of 4 points) starts dragging the pad; where it is left, it
// stays. Returns true when the press was the grip's.
b8 fude_zoom_page_grip_down(fude_zoom_page* _page, rde_vec_2F _screen, u64 _hand) {
    if(!_page->shown_anchor || _page->anchor_grip == NULL || _page->anchor_dragging) {
        return false;
    }
    const rde_vec_2F _size = fude_kit_screen_size(_page->app->window);
    const rde_vec_2F _p    = { _screen.x + _size.x * 0.5f, _screen.y + _size.y * 0.5f };
    if(rde_math_abs_f32(_p.x - _page->anchor_grip_at.x) > FUDE_ZOOM_PAGE_ANCHOR_GRIP_W * 0.5f + 4.0f || rde_math_abs_f32(_p.y - _page->anchor_grip_at.y) > FUDE_ZOOM_PAGE_UI_CHIP_H * 0.5f + 4.0f) {
        return false;
    }
    _page->anchor_dragging  = true;
    _page->anchor_drag_hand = _hand;
    _page->anchor_drag_from = _p;
    _page->anchor_from      = _page->anchor_center;
    return true;
}

// The grip dragged by the hand that took it: the pad follows. Returns true when the move was the grip's.
b8 fude_zoom_page_grip_moved(fude_zoom_page* _page, rde_vec_2F _screen, u64 _hand) {
    if(!_page->anchor_dragging || _page->anchor_drag_hand != _hand) {
        return false;
    }
    const rde_vec_2F _size = fude_kit_screen_size(_page->app->window);
    const rde_vec_2F _p    = { _screen.x + _size.x * 0.5f, _screen.y + _size.y * 0.5f };
    _page->anchor_custom    = true;
    _page->anchor_custom_at = (rde_vec_2F){ _page->anchor_from.x + _p.x - _page->anchor_drag_from.x, _page->anchor_from.y + _p.y - _page->anchor_drag_from.y };
    return true;
}

// The grip let go by the hand that took it. Returns true when it was the grip's.
b8 fude_zoom_page_grip_up(fude_zoom_page* _page, u64 _hand) {
    if(!_page->anchor_dragging || _page->anchor_drag_hand != _hand) {
        return false;
    }
    _page->anchor_dragging = false;
    return true;
}

// Writes the kerf's row: its width in the page's units, or "thin" under a hundredth of a millimetre.
void fude_zoom_page_kerf_said(const fude_zoom_page* _page, c8* _out, usize _size) {
    if(fude_zoom_page_kerf(_page) < 0.01) {
        rde_snprintf(_out, _size, "%s", fude_text(FUDE_TEXT_ZOOM_KERF_THIN));
        return;
    }
    c8 _v[FUDE_ZOOM_UNITS_TEXT];
    fude_zoom_units_format(fude_zoom_page_kerf(_page), &_page->units, _v, sizeof(_v));
    fude_text_format(_out, _size, FUDE_TEXT_ZOOM_KERF_IS, (const fude_text_arg[]){ FUDE_TS(_v) }, 1u);
}

// The printer's row: what it was found to do, or only its name.
RDE_INTERNAL void fude_zoom_page_printer_said(const fude_zoom_page* _page, c8* _out, usize _size) {
    if(_page->print_x > 0.0f && _page->print_y > 0.0f) {
        c8 _x[16], _y[16];
        rde_snprintf(_x, sizeof(_x), "%.1f%%", (f64)_page->print_x * 100.0);
        rde_snprintf(_y, sizeof(_y), "%.1f%%", (f64)_page->print_y * 100.0);
        fude_text_format(_out, _size, FUDE_TEXT_ZOOM_PRINTER_IS, (const fude_text_arg[]){ FUDE_TS(_x), FUDE_TS(_y) }, 2u);
    } else {
        rde_snprintf(_out, _size, "%s", fude_text(FUDE_TEXT_ZOOM_PRINTER));
    }
}

// Brings Measurements' buttons up to date; declared here for the fitting row's buttons.
RDE_INTERNAL void fude_zoom_page_measure_refresh(fude_ui* _ui, b8 _force);

// Settings' fitting row: parts turned to fit (0) or their grain kept along the board's (1).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_page_on_grain(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page != NULL) {
        _page->keep_grain = (uintptr_t)_user_data == 1u;   // Saved with the settings, gathered as they change.
        fude_zoom_page_measure_refresh(NULL, true);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Measurements' build: its header; the units' buttons; the true size's and the printer's rows with Calibrate and
// Check; the kerf's row with Change; and the fitting row with its two buttons.
RDE_INTERNAL void fude_zoom_page_measure_build(fude_ui* _ui, rde_ui_node* _card) {
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return;
    }
    _page->measure_header = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_ZOOM_MEASUREMENTS), FUDE_SIDE_HEADER_PX);
    _page->units_label    = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_ZOOM_UNITS), FUDE_SIDE_ROW_PX);
    for(u32 _i = 0; _i < 4u; _i++) {
        _page->units_buttons[_i] = fude_kit_button(_card, FUDE_ZOOM_PAGE_UNIT_NAMES[_i], fude_zoom_page_on_units, (any)(uintptr_t)_i);
    }
    _page->true_label       = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_ZOOM_TRUE_SIZE), FUDE_SIDE_ROW_PX);
    _page->calibrate_button = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_CALIBRATE), fude_zoom_page_on_calibrate, NULL);
    c8 _said[96];
    fude_zoom_page_printer_said(_page, _said, sizeof(_said));
    _page->printer_label    = fude_side_label(_ui, _card, _said, FUDE_SIDE_ROW_PX);
    _page->printer_button   = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_PRINTER_CHECK), fude_zoom_page_on_printer, NULL);
    fude_zoom_page_kerf_said(_page, _said, sizeof(_said));
    _page->kerf_label       = fude_side_label(_ui, _card, _said, FUDE_SIDE_ROW_PX);
    _page->kerf_button      = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_CHANGE), fude_zoom_page_on_kerf, NULL);
    _page->grain_label      = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_ZOOM_FIT_GRAIN), FUDE_SIDE_ROW_PX);
    _page->grain_buttons[0] = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_FIT_GRAIN_TURN), fude_zoom_page_on_grain, (any)(uintptr_t)0u);
    _page->grain_buttons[1] = fude_kit_button(_card, fude_text(FUDE_TEXT_ZOOM_FIT_GRAIN_KEEP), fude_zoom_page_on_grain, (any)(uintptr_t)1u);
}

// Measurements' layout from _y down, in a card _kw wide with margins _m: returns the _y below it. A row's words take
// 60% of the width (leaving at least 150 points) and its button sits at the right.
RDE_INTERNAL f32 fude_zoom_page_measure_layout(fude_ui* _ui, f32 _y, f32 _m, f32 _kw) {
    RDE_UNUSED(_ui);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || _page->measure_header == NULL) {
        return _y;
    }
    const f32 _lw = _kw - 2.0f * _m;
    fude_kit_place(rde_ui_label_as_node(_page->measure_header), (rde_vec_2F){ _m + _lw * 0.5f, _y }, (rde_vec_2F){ _lw, 30.0f });
    _y -= 40.0f;
    fude_kit_place(rde_ui_label_as_node(_page->units_label), (rde_vec_2F){ _m + _lw * 0.5f, _y }, (rde_vec_2F){ _lw, 30.0f });
    _y -= 38.0f;
    const f32 _bw = (_lw - 3.0f * 8.0f) / 4.0f;
    for(u32 _i = 0; _i < 4u; _i++) {
        fude_kit_place(rde_ui_button_as_node(_page->units_buttons[_i]), (rde_vec_2F){ _m + _bw * 0.5f + (f32)_i * (_bw + 8.0f), _y }, (rde_vec_2F){ _bw, 40.0f });
    }
    _y -= 52.0f;
    fude_kit_place(rde_ui_label_as_node(_page->true_label), (rde_vec_2F){ _m + rde_math_min_f32(_lw * 0.6f, _lw - 150.0f) * 0.5f, _y }, (rde_vec_2F){ rde_math_min_f32(_lw * 0.6f, _lw - 150.0f), 44.0f });
    fude_kit_place(rde_ui_button_as_node(_page->calibrate_button), (rde_vec_2F){ _kw - _m - 70.0f, _y }, (rde_vec_2F){ 140.0f, 40.0f });
    _y -= 52.0f;
    fude_kit_place(rde_ui_label_as_node(_page->printer_label), (rde_vec_2F){ _m + rde_math_min_f32(_lw * 0.6f, _lw - 150.0f) * 0.5f, _y }, (rde_vec_2F){ rde_math_min_f32(_lw * 0.6f, _lw - 150.0f), 44.0f });
    fude_kit_place(rde_ui_button_as_node(_page->printer_button), (rde_vec_2F){ _kw - _m - 70.0f, _y }, (rde_vec_2F){ 140.0f, 40.0f });
    _y -= 52.0f;
    fude_kit_place(rde_ui_label_as_node(_page->kerf_label), (rde_vec_2F){ _m + rde_math_min_f32(_lw * 0.6f, _lw - 150.0f) * 0.5f, _y }, (rde_vec_2F){ rde_math_min_f32(_lw * 0.6f, _lw - 150.0f), 44.0f });
    fude_kit_place(rde_ui_button_as_node(_page->kerf_button), (rde_vec_2F){ _kw - _m - 70.0f, _y }, (rde_vec_2F){ 140.0f, 40.0f });
    _y -= 52.0f;
    // Fitting parts: turned to fit, or their grain kept along the board's.
    fude_kit_place(rde_ui_label_as_node(_page->grain_label), (rde_vec_2F){ _m + _lw * 0.5f, _y }, (rde_vec_2F){ _lw, 30.0f });
    _y -= 38.0f;
    const f32 _gw = (_lw - 8.0f) * 0.5f;
    for(u32 _i = 0; _i < 2u; _i++) {
        fude_kit_place(rde_ui_button_as_node(_page->grain_buttons[_i]), (rde_vec_2F){ _m + _gw * 0.5f + (f32)_i * (_gw + 8.0f), _y }, (rde_vec_2F){ _gw, 40.0f });
    }
    return _y - 52.0f;
}

// Measurements' refresh: the chosen unit and fitting lit, and the printer's and kerf's rows said again.
RDE_INTERNAL void fude_zoom_page_measure_refresh(fude_ui* _ui, b8 _force) {
    RDE_UNUSED(_ui); RDE_UNUSED(_force);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || _page->measure_header == NULL) {
        return;
    }
    for(u32 _i = 0; _i < 4u; _i++) {
        if(_page->units_choice == _i) { fude_kit_button_selected(_page->units_buttons[_i]); }
        else                          { fude_kit_button_quiet(_page->units_buttons[_i]); }
    }
    c8 _said[96];
    fude_zoom_page_printer_said(_page, _said, sizeof(_said));
    rde_ui_label_set_text(_page->printer_label, _said);
    fude_zoom_page_kerf_said(_page, _said, sizeof(_said));
    rde_ui_label_set_text(_page->kerf_label, _said);
    for(u32 _i = 0; _i < 2u; _i++) {
        if((_i == 1u) == _page->keep_grain) { fude_kit_button_selected(_page->grain_buttons[_i]); }
        else                                { fude_kit_button_quiet(_page->grain_buttons[_i]); }
    }
}

// Measurements' restyle: the theme's colours on its words and buttons, then a refresh.
RDE_INTERNAL void fude_zoom_page_measure_restyle(fude_ui* _ui) {
    RDE_UNUSED(_ui);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || _page->measure_header == NULL) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    rde_ui_label_set_color(_page->measure_header, _t->text_soft);
    rde_ui_label_set_color(_page->units_label, _t->text);
    rde_ui_label_set_color(_page->true_label, _t->text);
    rde_ui_label_set_color(_page->printer_label, _t->text);
    rde_ui_label_set_color(_page->kerf_label, _t->text);
    rde_ui_label_set_color(_page->grain_label, _t->text);
    fude_kit_restyle_button(_page->calibrate_button);
    fude_kit_restyle_button(_page->printer_button);
    fude_kit_restyle_button(_page->kerf_button);
    fude_zoom_page_measure_refresh(_ui, true);
}

// --- Settings' Display: the frame rate on the screen -----------------------------------------------------------------

// Off or On pressed (below).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_page_on_fps(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);

// Display's build: its header, and the frame rate's row with Off and On.
RDE_INTERNAL void fude_zoom_page_display_build(fude_ui* _ui, rde_ui_node* _card) {
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return;
    }
    _page->display_header = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_ZOOM_DISPLAY), FUDE_SIDE_HEADER_PX);
    _page->fps_label      = fude_side_label(_ui, _card, fude_text(FUDE_TEXT_ZOOM_SHOW_FPS), FUDE_SIDE_ROW_PX);
    _page->fps_buttons[0] = fude_kit_button(_card, fude_text(FUDE_TEXT_OFF), fude_zoom_page_on_fps, (any)(uintptr_t)0u);
    _page->fps_buttons[1] = fude_kit_button(_card, fude_text(FUDE_TEXT_ON), fude_zoom_page_on_fps, (any)(uintptr_t)1u);
}

// Display's layout from _y down: the row's name, then Off and On at the right. Returns the _y below it.
RDE_INTERNAL f32 fude_zoom_page_display_layout(fude_ui* _ui, f32 _y, f32 _m, f32 _kw) {
    RDE_UNUSED(_ui);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || _page->display_header == NULL) {
        return _y;
    }
    const f32 _lw = _kw - 2.0f * _m;
    fude_kit_place(rde_ui_label_as_node(_page->display_header), (rde_vec_2F){ _m + _lw * 0.5f, _y }, (rde_vec_2F){ _lw, 30.0f });
    _y -= 44.0f;
    // Its name, then Off and On at the right.
    const f32 _bw = 70.0f, _names = rde_math_max_f32(_lw - 2.0f * _bw - 2.0f * 8.0f, 60.0f);
    fude_kit_place(rde_ui_label_as_node(_page->fps_label), (rde_vec_2F){ _m + _names * 0.5f, _y }, (rde_vec_2F){ _names, 44.0f });
    for(u32 _i = 0; _i < 2u; _i++) {
        fude_kit_place(rde_ui_button_as_node(_page->fps_buttons[_i]), (rde_vec_2F){ _kw - _m - _bw * 0.5f - (f32)(1u - _i) * (_bw + 8.0f), _y },
                       (rde_vec_2F){ _bw, 40.0f });
    }
    return _y - 52.0f;
}

// Display's refresh: Off or On lit.
RDE_INTERNAL void fude_zoom_page_display_refresh(fude_ui* _ui, b8 _force) {
    RDE_UNUSED(_ui); RDE_UNUSED(_force);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || _page->display_header == NULL) {
        return;
    }
    for(u32 _i = 0; _i < 2u; _i++) {
        if((_i == 1u) == _page->show_fps) { fude_kit_button_selected(_page->fps_buttons[_i]); }
        else                              { fude_kit_button_quiet(_page->fps_buttons[_i]); }
    }
}

// Display's restyle: the theme's colours, then a refresh.
RDE_INTERNAL void fude_zoom_page_display_restyle(fude_ui* _ui) {
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || _page->display_header == NULL) {
        return;
    }
    const fude_theme* _t = fude_theme_active();
    rde_ui_label_set_color(_page->display_header, _t->text_soft);
    rde_ui_label_set_color(_page->fps_label, _t->text);
    fude_zoom_page_display_refresh(_ui, true);
}

// Off or On pressed: kept with the settings, the frame rate counted afresh.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_zoom_page_on_fps(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _page->show_fps   = (uintptr_t)_user_data == 1u;
    _page->fps        = 0.0;
    _page->fps_frames = 0u;
    _page->fps_since  = rde_engine_get_time_now();
    fude_zoom_page_display_refresh(NULL, true);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// The frame rate, at the top right in the theme's text (just its number), clear of the toolbar where it is there.
// The frames are counted every frame, shown or not, and the rate is worked out each half second.
void fude_zoom_page_render_fps(fude_zoom_page* _page) {
    const f64 _now = rde_engine_get_time_now();
    _page->fps_frames++;
    if(_page->fps_since <= 0.0 || _now < _page->fps_since) {
        _page->fps_since  = _now;
        _page->fps_frames = 0u;
    } else if(_now - _page->fps_since >= 0.5) {
        _page->fps        = (f64)_page->fps_frames / (_now - _page->fps_since);
        _page->fps_frames = 0u;
        _page->fps_since  = _now;
    }
    if(!_page->show_fps || !(_page->fps > 0.0)) {
        return;
    }
    c8 _said[16];
    rde_snprintf(_said, sizeof(_said), "%.0f", _page->fps);
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    const rde_vec_4I   _in   = rde_window_get_safe_area_insets(_page->app->window);   // left, top, right, bottom
    const f64 _px = 16.0, _w = 56.0, _h = 22.0;
    f64 _x1 = _half.x - (f64)_in.z - 10.0, _y1 = _half.y - (f64)_in.y - 8.0;
    // The toolbar there: just left of it.
    if(_page->app->ui != NULL && !_page->app->ui->bar.hidden) {
        const fude_toolbar* _bar = &_page->app->ui->bar;
        const f64 _b0 = _bar->center.x - _bar->panel_size.x * 0.5 - _half.x, _b1 = _bar->center.x + _bar->panel_size.x * 0.5 - _half.x;
        const f64 _bb = _bar->center.y - _bar->panel_size.y * 0.5 - _half.y, _bt = _bar->center.y + _bar->panel_size.y * 0.5 - _half.y;
        if(_b1 > _x1 - _w && _b0 < _x1 && _bt > _y1 - _h && _bb < _y1) {
            _x1 = _b0 - 8.0;
        }
    }
    fude_zoom_render_text_at(&_page->renderer, _said, (u32)rde_strlen(_said), (fude_zoom_box){ _x1 - _w, _y1 - _h, _x1, _y1 }, _px, false, false,
                             fude_theme_active()->text);
}

// The canvas's Settings sections, in their order: Measurements, the core's Handwriting, and Display.
const fude_extension_section FUDE_ZOOM_PAGE_SECTIONS[FUDE_ZOOM_PAGE_SECTION_COUNT] = {
    { fude_zoom_page_measure_build, fude_zoom_page_measure_layout, fude_zoom_page_measure_refresh, fude_zoom_page_measure_restyle },
    FUDE_HANDWRITING_SECTION,
    { fude_zoom_page_display_build, fude_zoom_page_display_layout, fude_zoom_page_display_refresh, fude_zoom_page_display_restyle },
};
