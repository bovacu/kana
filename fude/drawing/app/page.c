// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/app/page.h"
#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/doc/import.h"
#include "drawing/base/theme.h"
#include "drawing/widgets/draw.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See page.h.
// ===========================================================================

#define FUDE_PAGE_FINGER_MOVE   6.0f    // screen units from where a finger landed: it writes
#define FUDE_PAGE_FINGER_WAIT   0.12    // seconds down with some movement: it writes
#define FUDE_PAGE_ZOOM_TIME     1.2     // the zoom's toast stays...
#define FUDE_PAGE_ZOOM_FADE     0.4     // ...then fades out over this

typedef enum { FUDE_PAGE_FINGER_NONE = 0, FUDE_PAGE_FINGER_PENDING, FUDE_PAGE_FINGER_WRITING, FUDE_PAGE_FINGER_GESTURE, FUDE_PAGE_FINGER_IGNORED } FUDE_PAGE_FINGER_;

// Where a point of the screen is on the PAGE. Also hands the ink the current
// zoom, which it needs to store widths in page units.
// A loop that took no ink, round a document's text: that area selected
// (lasso.h), for the app's text row. Only where the app has one.
void fude_page_lasso_text(fude_page_input* _page) {
    fude_app*  _app = _page->app;
    rde_vec_2F _min, _max;
    c8         _some[8];   // whether there is any: a little is enough
    b8         _pending = false;   // ...or a page under it still to be read (a scan's)
    if(fude_app_ext(_app)->text_row != NULL && fude_lasso_looped(_app->lasso, &_min, &_max) &&
       (fude_doc_text_in(&_page->doc, _min, _max, _some, sizeof(_some), &_pending) > 0u || _pending)) {
        fude_lasso_select_area(_app->lasso, _min, _max);
    }
}

RDE_INTERNAL rde_vec_2F fude_page_at(fude_page_input* _page, rde_vec_2F _screen) {
    _page->app->ink->zoom = _page->app->canvas->view.zoom;
    return fude_canvas_from_screen(_page->app->canvas, _screen);
}

RDE_INTERNAL void fude_page_erase_at(fude_page_input* _page, rde_vec_2F _screen) {
    fude_app* _app = _page->app;
    fude_ink_erase_at(_app->ink, fude_canvas_from_screen(_app->canvas, _screen), _app->ink->eraser_radius / _app->canvas->view.zoom);
}

// --- writing: the pen's, the mouse's, and a writing finger's ---------------------------

RDE_INTERNAL void fude_page_write_down(fude_page_input* _page, rde_vec_2F _screen, b8 _from_pen, b8 _eraser) {
    fude_app* _app = _page->app;
    fude_pagemenu_close_context(&_app->ui->page);   // a press anywhere else dismisses it, and still does its job
    fude_canvas_release_fingers(_app->canvas);     // writing takes over from any finger gesture in progress
    if(_app->ui->bar.tool == FUDE_TOOL_ERASE || _eraser) {
        _page->erasing = true;   // the Erase tool, or a pen's own eraser end
        fude_page_erase_at(_page, _screen);
        return;
    }
    if(_app->ui->bar.tool == FUDE_TOOL_LASSO) {
        fude_lasso_pen_down(_app->lasso, _app->ink, fude_page_at(_page, _screen), _app->canvas->view.zoom);
        return;
    }
    _app->ink->marking = _app->ui->bar.tool == FUDE_TOOL_MARK;   // the marker's stroke, or the pen's
    fude_ink_begin(_app->ink, fude_page_at(_page, _screen), _from_pen, false);
}

RDE_INTERNAL void fude_page_write_moved(fude_page_input* _page, rde_vec_2F _screen) {
    fude_app* _app = _page->app;
    if(_page->erasing) {
        fude_page_erase_at(_page, _screen);
    } else if(fude_lasso_busy(_app->lasso)) {
        fude_lasso_pen_moved(_app->lasso, _app->ink, fude_canvas_from_screen(_app->canvas, _screen), _app->canvas->view.zoom);
    } else if(_app->ink->drawing) {
        fude_ink_extend(_app->ink, fude_page_at(_page, _screen));
    }
}

RDE_INTERNAL void fude_page_write_up(fude_page_input* _page) {
    fude_app* _app = _page->app;
    _page->erasing = false;
    fude_ink_erase_end(_app->ink);   // the whole swipe is one undo
    fude_ink_end(_app->ink);
    fude_lasso_pen_up(_app->lasso, _app->ink, _app->canvas->view.zoom);
    fude_page_lasso_text(_page);
}

// The pending finger writes: from where it landed, through where it has been.
RDE_INTERNAL void fude_page_finger_start_writing(fude_page_input* _page) {
    _page->app->ink->eraser      = false;
    _page->app->ink->sample_time = rde_engine_get_time_now();
    fude_page_write_down(_page, _page->finger_points[0], false, false);
    for(u32 _i = 1; _i < _page->finger_count; _i++) {
        fude_page_write_moved(_page, _page->finger_points[_i]);
    }
    _page->finger_state = FUDE_PAGE_FINGER_WRITING;
}

RDE_INTERNAL f32 fude_page_finger_moved_from_start(const fude_page_input* _page, rde_vec_2F _p) {
    const rde_vec_2F _d = { _p.x - _page->finger_points[0].x, _p.y - _page->finger_points[0].y };
    return sqrtf(_d.x * _d.x + _d.y * _d.y);
}

RDE_INTERNAL void fude_page_finger_down(fude_page_input* _page, u64 _id, rde_vec_2F _screen) {
    fude_canvas* _canvas = _page->app->canvas;
    if(_page->finger_state == FUDE_PAGE_FINGER_WRITING || _page->finger_state == FUDE_PAGE_FINGER_IGNORED) {
        return;   // a resting hand
    }
    if(_page->finger_state == FUDE_PAGE_FINGER_PENDING) {
        // A second finger before the first wrote: both are the page's gesture.
        fude_canvas_finger_down(_canvas, _page->finger_id, _page->finger_points[0]);
        if(_page->finger_count > 1) {
            fude_canvas_finger_moved(_canvas, _page->finger_id, _page->finger_points[_page->finger_count - 1]);
        }
        fude_canvas_finger_down(_canvas, _id, _screen);
        _page->finger_state   = FUDE_PAGE_FINGER_GESTURE;
        _page->finger_gesture = 2;
        return;
    }
    if(_page->finger_state == FUDE_PAGE_FINGER_GESTURE) {
        fude_canvas_finger_down(_canvas, _id, _screen);
        _page->finger_gesture++;
        return;
    }
    fude_pagemenu_close_context(&_page->app->ui->page);
    _page->finger_state     = FUDE_PAGE_FINGER_PENDING;
    _page->finger_id        = _id;
    _page->finger_points[0] = _screen;
    _page->finger_count     = 1;
    _page->finger_since     = rde_engine_get_time_now();
}

RDE_INTERNAL void fude_page_finger_moved(fude_page_input* _page, u64 _id, rde_vec_2F _screen) {
    if(_page->finger_state == FUDE_PAGE_FINGER_GESTURE) {
        fude_canvas_finger_moved(_page->app->canvas, _id, _screen);
        return;
    }
    if(_id != _page->finger_id) {
        return;
    }
    if(_page->finger_state == FUDE_PAGE_FINGER_PENDING) {
        if(_page->finger_count < FUDE_PAGE_FINGER_POINTS) {
            _page->finger_points[_page->finger_count++] = _screen;
        } else {
            _page->finger_points[FUDE_PAGE_FINGER_POINTS - 1u] = _screen;
        }
        if(fude_page_finger_moved_from_start(_page, _screen) > FUDE_PAGE_FINGER_MOVE) {
            fude_page_finger_start_writing(_page);
        }
    } else if(_page->finger_state == FUDE_PAGE_FINGER_WRITING) {
        _page->app->ink->sample_time = rde_engine_get_time_now();
        fude_page_write_moved(_page, _screen);
    }
}

// Two fingers tapped: undo. Three: redo.
RDE_INTERNAL void fude_page_canvas_finger_up(fude_page_input* _page, u64 _id) {
    const FUDE_CANVAS_TAP_ _tap = fude_canvas_finger_up(_page->app->canvas, _id);
    if(_tap == FUDE_CANVAS_TAP_TWO) {
        fude_ink_undo(_page->app->ink);
    } else if(_tap == FUDE_CANVAS_TAP_THREE) {
        fude_ink_redo(_page->app->ink);
    }
}

RDE_INTERNAL void fude_page_finger_up(fude_page_input* _page, u64 _id) {
    if(_page->finger_state == FUDE_PAGE_FINGER_GESTURE) {
        fude_page_canvas_finger_up(_page, _id);
        _page->finger_gesture = _page->finger_gesture > 0 ? _page->finger_gesture - 1u : 0u;
        if(_page->finger_gesture == 0) {
            _page->finger_state = FUDE_PAGE_FINGER_NONE;
        }
        return;
    }
    if(_id != _page->finger_id) {
        return;
    }
    if(_page->finger_state == FUDE_PAGE_FINGER_PENDING) {
        fude_page_finger_start_writing(_page);   // a short touch: a dot
    }
    if(_page->finger_state == FUDE_PAGE_FINGER_WRITING) {
        fude_page_write_up(_page);
    }
    _page->finger_state = FUDE_PAGE_FINGER_NONE;
}

// Once a frame (a finger at rest sends nothing): a pending finger that rested a
// moment with a little movement writes; one held quite still is the long press.
RDE_INTERNAL void fude_page_finger_poll(fude_page_input* _page) {
    if(_page->finger_state != FUDE_PAGE_FINGER_PENDING) {
        return;
    }
    const f64 _held  = rde_engine_get_time_now() - _page->finger_since;
    const f32 _moved = fude_page_finger_moved_from_start(_page, _page->finger_points[_page->finger_count - 1]);
    if(_held >= FUDE_CANVAS_LONG_PRESS_TIME && _moved < FUDE_PAGE_FINGER_MOVE) {
        fude_pagemenu_open_context(&_page->app->ui->page, _page->finger_points[0], fude_canvas_from_screen(_page->app->canvas, _page->finger_points[0]));
        _page->finger_state = FUDE_PAGE_FINGER_IGNORED;   // until it lifts
    } else if(_held >= FUDE_PAGE_FINGER_WAIT && _moved > 1.5f) {
        fude_page_finger_start_writing(_page);
    }
}

// A finger's writing let go (a pen came down, or a screen opened over the page).
RDE_INTERNAL void fude_page_finger_forget(fude_page_input* _page) {
    if(_page->finger_state == FUDE_PAGE_FINGER_WRITING) {
        fude_page_write_up(_page);
    }
    if(_page->finger_state == FUDE_PAGE_FINGER_GESTURE) {
        fude_canvas_release_fingers(_page->app->canvas);
    }
    _page->finger_state   = FUDE_PAGE_FINGER_NONE;
    _page->finger_count   = 0;
    _page->finger_gesture = 0;
}

void fude_page_pen_came(fude_page_input* _page) {
    _page->app->pen_ever = true;
    if(_page->app->finger_writes) {
        fude_page_finger_forget(_page);
        fude_app_set_finger_writes(_page->app, false);
    }
}

void fude_page_let_go(fude_page_input* _page) {
    fude_app* _app = _page->app;
    fude_page_finger_forget(_page);
    fude_lasso_pen_up(_app->lasso, _app->ink, _app->canvas->view.zoom);
    fude_page_lasso_text(_page);
    fude_ink_erase_end(_app->ink);
    fude_ink_end(_app->ink);
    _page->pen_on_ui = false;
    _page->erasing   = false;
}

// --- events -------------------------------------------------------------------------------

// A press at _screen on what the app draws over the page (extension.h: Kana's
// translation card); the page still gets it.
RDE_INTERNAL void fude_page_press_app(fude_page_input* _page, rde_vec_2F _screen) {
    const fude_extension* _ext = fude_app_ext(_page->app);
    if(_ext->page_press != NULL) {
        _ext->page_press(_page->app, _screen);
    }
}

void fude_page_event(fude_page_input* _page, rde_event* _event) {
    fude_app* _app = _page->app;
    fude_ui*  _ui  = _app->ui;
    fude_ink* _ink = _app->ink;

    switch(_event->type) {
        // --- pen ---------------------------------------------------------
        //
        // PEN_AXIS carries pressure and tilt and NOTHING else carries them, so
        // it has to be folded into the live state before any move is sampled.
        case RDE_EVENT_TYPE_PEN_AXIS: {
            fude_ink_pen_axis(_ink, &_event->data.pen_event_data);
        } break;

        case RDE_EVENT_TYPE_PEN_DOWN: {
            const rde_event_pen* _pen    = &_event->data.pen_event_data;
            const rde_vec_2F     _screen = fude_app_window_to_screen(_app, _pen->position);
            fude_page_pen_came(_page);
            _ink->pen_seen    = true;
            _ink->eraser      = _pen->eraser;
            _ink->sample_time = (f64)_event->time_stamp * 1e-9;   // SDL event time, ns

            // On the UI the pen is pressing a button — the UI gets it as a
            // synthetic mouse click right after this event — so it must not also
            // write underneath. (What the app draws over the page — Kana's
            // translation card — answers here.)
            fude_page_press_app(_page, _screen);
            if(fude_ui_hit(_ui, _screen)) {
                _page->pen_on_ui = true;
                break;
            }
            fude_toolbar_close_panels(&_ui->bar);   // a press on the page closes the bar's panels
            fude_page_write_down(_page, _screen, true, _pen->eraser);
        } break;

        case RDE_EVENT_TYPE_PEN_MOVED: {
            _ink->pen_seen = true;
            if(_page->pen_on_ui) {
                break;
            }
            // Only while the tip is down (write_moved draws only then): a pen
            // hovering in proximity still reports motion, and inking on hover
            // would be a mess.
            _ink->sample_time = (f64)_event->time_stamp * 1e-9;   // SDL event time, ns
            fude_page_write_moved(_page, fude_app_window_to_screen(_app, _event->data.pen_event_data.position));
        } break;

        case RDE_EVENT_TYPE_PEN_UP: {
            _ink->pen_seen   = true;
            _page->pen_on_ui = false;
            fude_page_write_up(_page);
        } break;

        // Apple Pencil's double tap: what the learner set it to do (the system's
        // Apple Pencil settings) — the eraser and back, most often.
        case RDE_EVENT_TYPE_PEN_DOUBLE_TAP: {
            _ink->pen_seen = true;
            fude_toolbar_pen_double_tap(&_ui->bar, _event->data.pen_event_data.tap_action);
        } break;

        case RDE_EVENT_TYPE_PEN_PROXIMITY_IN:
        case RDE_EVENT_TYPE_PEN_PROXIMITY_OUT: {
            _ink->pen_seen = true;
        } break;

        // --- touch: moves the page; writes only with the hand on -----------
        //
        // SDL echoes every pen contact as a synthetic touch too (on by default),
        // sent right after PEN_DOWN; the engine flags those `from_pen` and they
        // are skipped — otherwise writing would drag the page along. A finger
        // landing while the pen is writing is a resting palm, not a gesture, so
        // it is never picked up.
        case RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(_touch->from_pen) {
                break;
            }
            _page->touch_seen = true;
            const rde_vec_2F _pos = fude_app_touch_to_screen(_touch->init_touch_position);
            // A finger on the UI is using it, not moving the page.
            if(!_ink->drawing && !_page->erasing) {
                fude_page_press_app(_page, _pos);   // what the app drew over the page (the translation card's speaker)
            }
            if(fude_ui_hit(_ui, _pos)) {
                break;
            }
            fude_toolbar_close_panels(&_ui->bar);
            if(_app->finger_writes) {
                fude_page_finger_down(_page, _touch->finger_id, _pos);   // one finger writes; two move the page
                break;
            }
            fude_pagemenu_close_context(&_ui->page);
            if(!_ink->drawing && !_page->erasing && !fude_lasso_busy(_app->lasso)) {
                fude_canvas_finger_down(_app->canvas, _touch->finger_id, _pos);
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(_touch->from_pen) {
                break;
            }
            if(_page->finger_state != FUDE_PAGE_FINGER_NONE) {
                fude_page_finger_moved(_page, _touch->finger_id, fude_app_touch_to_screen(_touch->moved_touch_position));
                break;
            }
            if(!_ink->drawing) {
                fude_canvas_finger_moved(_app->canvas, _touch->finger_id, fude_app_touch_to_screen(_touch->moved_touch_position));
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_UP: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(_touch->from_pen) {
                break;
            }
            if(_page->finger_state != FUDE_PAGE_FINGER_NONE) {
                fude_page_finger_up(_page, _touch->finger_id);
                break;
            }
            fude_page_canvas_finger_up(_page, _touch->finger_id);
        } break;

        // --- the mouse, on a computer ---------------------------------------
        //
        // Desktop only. On mobile SDL synthesizes mouse events from the pen AND from
        // touch (SDL_HINT_PEN_MOUSE_EVENTS / TOUCH_MOUSE_EVENTS, on by default), so
        // this would open yet another stroke on every pen-down.
#if !defined(RDE_PLATFORM_MOBILE)
        case RDE_EVENT_TYPE_MOUSE_BUTTON_PRESSED: {
            // The mouse reports CENTRE-ORIGIN already, unlike pen and touch.
            const rde_vec_2I _m      = rde_input_mouse_get_position(_app->window);
            const rde_vec_2F _screen = { (f32)_m.x, (f32)_m.y };
            // The UI already took this click (a button), or it landed on it.
            if(!_event->handled) {
                fude_page_press_app(_page, _screen);   // what the app drew over the page (the translation card's speaker)
            }
            if(_event->handled || fude_ui_hit(_ui, _screen)) {
                break;
            }
            fude_toolbar_close_panels(&_ui->bar);
            // Right-click stands in for the long press.
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_RIGHT) {
                fude_pagemenu_open_context(&_ui->page, _screen, fude_canvas_from_screen(_app->canvas, _screen));
                break;
            }
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT) {
                fude_page_write_down(_page, _screen, false, false);
            }
        } break;

        case RDE_EVENT_TYPE_MOUSE_BUTTON_RELEASED: {
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT) {
                fude_page_write_up(_page);
            }
        } break;
#endif

        default: break;
    }
}

// --- each frame ---------------------------------------------------------------------------

void fude_page_update(fude_page_input* _page) {
    fude_app*    _app    = _page->app;
    rde_window*  _window = _app->window;
    fude_import_update(_app);   // a document brought in: a canvas of its own, opened
    fude_doc_update(&_page->doc, _app);
    fude_canvas_update(_app->canvas, rde_window_get_size(_window), rde_window_get_safe_area_insets(_window));   // a flick's coast; a document's bounds
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_C)) {
        fude_ink_clear(_app->ink);
    }
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_Z)) {
        fude_ink_undo(_app->ink);
    }
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_Y)) {
        fude_ink_redo(_app->ink);
    }
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_BACKSPACE) || rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_DELETE)) {
        fude_lasso_delete(_app->lasso, _app->ink);
    }
#if defined(RDE_DEBUG)
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_M)) {
        _page->show_samples = !_page->show_samples;
    }
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_H)) {
        _page->show_hud = !_page->show_hud;
    }
#endif
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_R)) {
        fude_canvas_reset_view(_app->canvas);
    }
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_B)) {
        _app->ink->brush_scale = _app->ink->brush_scale == FUDE_INK_BRUSH_SCALE_PAGE ? FUDE_INK_BRUSH_SCALE_SCREEN : FUDE_INK_BRUSH_SCALE_PAGE;
        fude_toolbar_refresh(&_app->ui->bar);
    }

    // Desktop dragging, so the thing can be exercised without a tablet in hand.
    // Not on mobile, where the "mouse" is SDL's echo of the pen.
#if !defined(RDE_PLATFORM_MOBILE)
    if(rde_input_mouse_is_button_pressed(_window, RDE_MOUSE_BUTTON_LEFT)) {
        const rde_vec_2I _m = rde_input_mouse_get_position(_window);
        fude_page_write_moved(_page, (rde_vec_2F){ (f32)_m.x, (f32)_m.y });
    }
#endif

    // A writing finger's moment to decide, and its long press (polled: a finger
    // at rest sends nothing).
    fude_page_finger_poll(_page);
    // A finger held still: the page's context menu.
    rde_vec_2F _press;
    if(fude_canvas_long_press(_app->canvas, &_press)) {
        fude_pagemenu_open_context(&_app->ui->page, _press, fude_canvas_from_screen(_app->canvas, _press));
    }
}

// --- drawing ------------------------------------------------------------------------------

void fude_page_zoom_seen(fude_page_input* _page) {
    _page->zoom_seen = -1.0f;
}

// The zoom as a percentage, in a pill at the bottom-right, while it is changing
// and for a moment after.
RDE_INTERNAL void fude_page_render_zoom(fude_page_input* _page, rde_window* _window) {
    fude_app* _app = _page->app;
    const f32 _zoom = fude_canvas_zoom_shown(_app->canvas);   // reading a document: against 100%, its width across the screen
    const f64 _now  = rde_engine_get_time_now();
    if(_page->zoom_seen < 0.0f) {
        _page->zoom_seen = _zoom;
    } else if(fabsf(_zoom - _page->zoom_seen) > 1e-5f) {
        _page->zoom_seen     = _zoom;
        _page->zoom_shown_at = _now;
    }
    const f64 _age = _now - _page->zoom_shown_at;
    if(_age >= FUDE_PAGE_ZOOM_TIME + FUDE_PAGE_ZOOM_FADE || _app->font == NULL) {
        return;
    }
    const f32 _fade = _age <= FUDE_PAGE_ZOOM_TIME ? 1.0f : 1.0f - (f32)((_age - FUDE_PAGE_ZOOM_TIME) / FUDE_PAGE_ZOOM_FADE);

    // The text, measured (once per new value): the pill fits it, and it sits in
    // the middle — across by its width, up and down by its digits' height.
    static c8         _shown[16] = "";
    static rde_vec_2F _measured  = { 0.0f, 0.0f };
    c8 _text[16];
    snprintf(_text, sizeof(_text), "%.0f%%", (f64)(_zoom * 100.0f));
    const f32 _px    = 20.0f;
    const f32 _scale = _px / _app->font_px;
    if(strcmp(_text, _shown) != 0) {
        snprintf(_shown, sizeof(_shown), "%s", _text);
        _measured = rde_rich_text_measure(_text, _app->font, _scale, 10000.0f, false);
    }
    const f32 _em     = _measured.y / 1.17f;          // the line is ~1.17 em in Roboto
    const f32 _digits = _em * 0.711f;                 // Roboto's digit (cap) height, in em

    const fude_theme* _t      = fude_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _safe   = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const rde_vec_2F  _pill   = { fmaxf(72.0f, _measured.x + 32.0f), fmaxf(40.0f, _digits + 26.0f) };
    const rde_vec_2F  _center = { (f32)_size.x * 0.5f - (f32)_safe.z - 16.0f - _pill.x * 0.5f, -(f32)_size.y * 0.5f + (f32)_safe.w + 16.0f + _pill.y * 0.5f };
    rde_color _back = _t->panel;
    _back.a         = (u8)((f32)_back.a * _fade);
    rde_rendering_2d_draw_rounded_rectangle(_center, _pill, 1.0f, 8u, _back, NULL);
    rde_color _ink = _t->button_text;
    _ink.a         = (u8)((f32)_ink.a * _fade);
    // The position is where the text starts, on its baseline.
    fude_draw_text(_app->font, _app->font_px, _text, _center.x - _measured.x * 0.5f, _center.y - _digits * 0.5f, _px, _ink);
}

void fude_page_render(fude_page_input* _page, rde_window* _window) {
    fude_app*        _app  = _page->app;
    const fude_view  _view = _app->canvas->view;
    const rde_vec_2I _size = rde_window_get_size(_window);
    fude_canvas_draw_grid(_app->canvas, _size);
    fude_doc_render(&_page->doc, _app->canvas, _window);
    fude_lasso_render_under(_app->lasso, _app->ink, _view.offset, _view.zoom);
    fude_ink_render(_app->ink, _view.offset, _view.zoom, (rde_vec_2F){ (f32)_size.x * 0.5f, (f32)_size.y * 0.5f }, rde_engine_get_time_now(), _page->show_samples);
    fude_lasso_render_over(_app->lasso, _app->ink, _view.offset, _view.zoom);
    fude_page_render_zoom(_page, _window);
    fude_doc_render_pill(&_page->doc, _app, _window);
    if(fude_app_ext(_app)->page_render != NULL) {
        fude_app_ext(_app)->page_render(_app, _window);   // what the app draws over the page (Kana: Translate with Google's card)
    }
}

void fude_page_init(fude_page_input* _page, fude_app* _app) {
    memset(_page, 0, sizeof(*_page));
    _page->app           = _app;
    _page->zoom_seen     = -1.0f;
    _page->zoom_shown_at = -100.0;
    fude_doc_init(&_page->doc);
}
