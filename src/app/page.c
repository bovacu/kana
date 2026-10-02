#include "app/page.h"
#include "app/app.h"
#include "app/ui.h"
#include "base/theme.h"
#include "widgets/draw.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See page.h.
// ===========================================================================

#define KANA_PAGE_ERASER_RADIUS 10.0f   // the eraser's reach, in SCREEN units: a fingertip-sized circle at any zoom
#define KANA_PAGE_FINGER_MOVE   6.0f    // screen units from where a finger landed: it writes
#define KANA_PAGE_FINGER_WAIT   0.12    // seconds down with some movement: it writes
#define KANA_PAGE_ZOOM_TIME     1.2     // the zoom's toast stays...
#define KANA_PAGE_ZOOM_FADE     0.4     // ...then fades out over this

typedef enum { KANA_PAGE_FINGER_NONE = 0, KANA_PAGE_FINGER_PENDING, KANA_PAGE_FINGER_WRITING, KANA_PAGE_FINGER_GESTURE, KANA_PAGE_FINGER_IGNORED } KANA_PAGE_FINGER_;

// Where a point of the screen is on the PAGE. Also hands the ink the current
// zoom, which it needs to store widths in page units.
RDE_INTERNAL rde_vec_2F kana_page_at(kana_page_input* _page, rde_vec_2F _screen) {
    _page->app->ink->zoom = _page->app->canvas->view.zoom;
    return kana_canvas_from_screen(_page->app->canvas, _screen);
}

RDE_INTERNAL void kana_page_erase_at(kana_page_input* _page, rde_vec_2F _screen) {
    kana_app* _app = _page->app;
    kana_ink_erase_at(_app->ink, kana_canvas_from_screen(_app->canvas, _screen), KANA_PAGE_ERASER_RADIUS / _app->canvas->view.zoom);
}

// --- writing: the pen's, the mouse's, and a writing finger's ---------------------------

RDE_INTERNAL void kana_page_write_down(kana_page_input* _page, rde_vec_2F _screen, b8 _from_pen, b8 _eraser) {
    kana_app* _app = _page->app;
    kana_pagemenu_close_context(&_app->ui->page);   // a press anywhere else dismisses it, and still does its job
    kana_canvas_release_fingers(_app->canvas);     // writing takes over from any finger gesture in progress
    if(_app->ui->bar.tool == KANA_TOOL_ERASE || _eraser) {
        _page->erasing = true;   // the Erase tool, or a pen's own eraser end
        kana_page_erase_at(_page, _screen);
        return;
    }
    if(_app->ui->bar.tool == KANA_TOOL_LASSO) {
        kana_lasso_pen_down(_app->lasso, _app->ink, kana_page_at(_page, _screen), _app->canvas->view.zoom);
        return;
    }
    kana_ink_begin(_app->ink, kana_page_at(_page, _screen), _from_pen, false);
}

RDE_INTERNAL void kana_page_write_moved(kana_page_input* _page, rde_vec_2F _screen) {
    kana_app* _app = _page->app;
    if(_page->erasing) {
        kana_page_erase_at(_page, _screen);
    } else if(kana_lasso_busy(_app->lasso)) {
        kana_lasso_pen_moved(_app->lasso, _app->ink, kana_canvas_from_screen(_app->canvas, _screen), _app->canvas->view.zoom);
    } else if(_app->ink->drawing) {
        kana_ink_extend(_app->ink, kana_page_at(_page, _screen));
    }
}

RDE_INTERNAL void kana_page_write_up(kana_page_input* _page) {
    kana_app* _app = _page->app;
    _page->erasing = false;
    kana_ink_erase_end(_app->ink);   // the whole swipe is one undo
    kana_ink_end(_app->ink);
    kana_lasso_pen_up(_app->lasso, _app->ink, _app->canvas->view.zoom);
}

// The pending finger writes: from where it landed, through where it has been.
RDE_INTERNAL void kana_page_finger_start_writing(kana_page_input* _page) {
    _page->app->ink->eraser      = false;
    _page->app->ink->sample_time = rde_engine_get_time_now();
    kana_page_write_down(_page, _page->finger_points[0], false, false);
    for(u32 _i = 1; _i < _page->finger_count; _i++) {
        kana_page_write_moved(_page, _page->finger_points[_i]);
    }
    _page->finger_state = KANA_PAGE_FINGER_WRITING;
}

RDE_INTERNAL f32 kana_page_finger_moved_from_start(const kana_page_input* _page, rde_vec_2F _p) {
    const rde_vec_2F _d = { _p.x - _page->finger_points[0].x, _p.y - _page->finger_points[0].y };
    return sqrtf(_d.x * _d.x + _d.y * _d.y);
}

RDE_INTERNAL void kana_page_finger_down(kana_page_input* _page, u64 _id, rde_vec_2F _screen) {
    kana_canvas* _canvas = _page->app->canvas;
    if(_page->finger_state == KANA_PAGE_FINGER_WRITING || _page->finger_state == KANA_PAGE_FINGER_IGNORED) {
        return;   // a resting hand
    }
    if(_page->finger_state == KANA_PAGE_FINGER_PENDING) {
        // A second finger before the first wrote: both are the page's gesture.
        kana_canvas_finger_down(_canvas, _page->finger_id, _page->finger_points[0]);
        if(_page->finger_count > 1) {
            kana_canvas_finger_moved(_canvas, _page->finger_id, _page->finger_points[_page->finger_count - 1]);
        }
        kana_canvas_finger_down(_canvas, _id, _screen);
        _page->finger_state   = KANA_PAGE_FINGER_GESTURE;
        _page->finger_gesture = 2;
        return;
    }
    if(_page->finger_state == KANA_PAGE_FINGER_GESTURE) {
        kana_canvas_finger_down(_canvas, _id, _screen);
        _page->finger_gesture++;
        return;
    }
    kana_pagemenu_close_context(&_page->app->ui->page);
    _page->finger_state     = KANA_PAGE_FINGER_PENDING;
    _page->finger_id        = _id;
    _page->finger_points[0] = _screen;
    _page->finger_count     = 1;
    _page->finger_since     = rde_engine_get_time_now();
}

RDE_INTERNAL void kana_page_finger_moved(kana_page_input* _page, u64 _id, rde_vec_2F _screen) {
    if(_page->finger_state == KANA_PAGE_FINGER_GESTURE) {
        kana_canvas_finger_moved(_page->app->canvas, _id, _screen);
        return;
    }
    if(_id != _page->finger_id) {
        return;
    }
    if(_page->finger_state == KANA_PAGE_FINGER_PENDING) {
        if(_page->finger_count < KANA_PAGE_FINGER_POINTS) {
            _page->finger_points[_page->finger_count++] = _screen;
        } else {
            _page->finger_points[KANA_PAGE_FINGER_POINTS - 1u] = _screen;
        }
        if(kana_page_finger_moved_from_start(_page, _screen) > KANA_PAGE_FINGER_MOVE) {
            kana_page_finger_start_writing(_page);
        }
    } else if(_page->finger_state == KANA_PAGE_FINGER_WRITING) {
        _page->app->ink->sample_time = rde_engine_get_time_now();
        kana_page_write_moved(_page, _screen);
    }
}

// Two fingers tapped: undo. Three: redo.
RDE_INTERNAL void kana_page_canvas_finger_up(kana_page_input* _page, u64 _id) {
    const KANA_CANVAS_TAP_ _tap = kana_canvas_finger_up(_page->app->canvas, _id);
    if(_tap == KANA_CANVAS_TAP_TWO) {
        kana_ink_undo(_page->app->ink);
    } else if(_tap == KANA_CANVAS_TAP_THREE) {
        kana_ink_redo(_page->app->ink);
    }
}

RDE_INTERNAL void kana_page_finger_up(kana_page_input* _page, u64 _id) {
    if(_page->finger_state == KANA_PAGE_FINGER_GESTURE) {
        kana_page_canvas_finger_up(_page, _id);
        _page->finger_gesture = _page->finger_gesture > 0 ? _page->finger_gesture - 1u : 0u;
        if(_page->finger_gesture == 0) {
            _page->finger_state = KANA_PAGE_FINGER_NONE;
        }
        return;
    }
    if(_id != _page->finger_id) {
        return;
    }
    if(_page->finger_state == KANA_PAGE_FINGER_PENDING) {
        kana_page_finger_start_writing(_page);   // a short touch: a dot
    }
    if(_page->finger_state == KANA_PAGE_FINGER_WRITING) {
        kana_page_write_up(_page);
    }
    _page->finger_state = KANA_PAGE_FINGER_NONE;
}

// Once a frame (a finger at rest sends nothing): a pending finger that rested a
// moment with a little movement writes; one held quite still is the long press.
RDE_INTERNAL void kana_page_finger_poll(kana_page_input* _page) {
    if(_page->finger_state != KANA_PAGE_FINGER_PENDING) {
        return;
    }
    const f64 _held  = rde_engine_get_time_now() - _page->finger_since;
    const f32 _moved = kana_page_finger_moved_from_start(_page, _page->finger_points[_page->finger_count - 1]);
    if(_held >= KANA_CANVAS_LONG_PRESS_TIME && _moved < KANA_PAGE_FINGER_MOVE) {
        kana_pagemenu_open_context(&_page->app->ui->page, _page->finger_points[0], kana_canvas_from_screen(_page->app->canvas, _page->finger_points[0]));
        _page->finger_state = KANA_PAGE_FINGER_IGNORED;   // until it lifts
    } else if(_held >= KANA_PAGE_FINGER_WAIT && _moved > 1.5f) {
        kana_page_finger_start_writing(_page);
    }
}

// A finger's writing let go (a pen came down, or a screen opened over the page).
RDE_INTERNAL void kana_page_finger_forget(kana_page_input* _page) {
    if(_page->finger_state == KANA_PAGE_FINGER_WRITING) {
        kana_page_write_up(_page);
    }
    if(_page->finger_state == KANA_PAGE_FINGER_GESTURE) {
        kana_canvas_release_fingers(_page->app->canvas);
    }
    _page->finger_state   = KANA_PAGE_FINGER_NONE;
    _page->finger_count   = 0;
    _page->finger_gesture = 0;
}

void kana_page_pen_came(kana_page_input* _page) {
    _page->app->pen_ever = true;
    if(_page->app->finger_writes) {
        kana_page_finger_forget(_page);
        kana_app_set_finger_writes(_page->app, false);
    }
}

void kana_page_let_go(kana_page_input* _page) {
    kana_app* _app = _page->app;
    kana_page_finger_forget(_page);
    kana_lasso_pen_up(_app->lasso, _app->ink, _app->canvas->view.zoom);
    kana_ink_erase_end(_app->ink);
    kana_ink_end(_app->ink);
    _page->pen_on_ui = false;
    _page->erasing   = false;
}

// --- events -------------------------------------------------------------------------------

void kana_page_event(kana_page_input* _page, rde_event* _event) {
    kana_app* _app = _page->app;
    kana_ui*  _ui  = _app->ui;
    kana_ink* _ink = _app->ink;

    switch(_event->type) {
        // --- pen ---------------------------------------------------------
        //
        // PEN_AXIS carries pressure and tilt and NOTHING else carries them, so
        // it has to be folded into the live state before any move is sampled.
        case RDE_EVENT_TYPE_PEN_AXIS: {
            kana_ink_pen_axis(_ink, &_event->data.pen_event_data);
        } break;

        case RDE_EVENT_TYPE_PEN_DOWN: {
            const rde_event_pen* _pen    = &_event->data.pen_event_data;
            const rde_vec_2F     _screen = kana_app_window_to_screen(_app, _pen->position);
            kana_page_pen_came(_page);
            _ink->pen_seen    = true;
            _ink->eraser      = _pen->eraser;
            _ink->sample_time = (f64)_event->time_stamp * 1e-9;   // SDL event time, ns

            // On the UI the pen is pressing a button — the UI gets it as a
            // synthetic mouse click right after this event — so it must not also
            // write underneath. (Translate with Google's card is drawn by Kana:
            // its speaker answers here.)
            kana_pagemenu_card_press(&_ui->page, _screen);
            if(kana_ui_hit(_ui, _screen)) {
                _page->pen_on_ui = true;
                break;
            }
            kana_page_write_down(_page, _screen, true, _pen->eraser);
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
            kana_page_write_moved(_page, kana_app_window_to_screen(_app, _event->data.pen_event_data.position));
        } break;

        case RDE_EVENT_TYPE_PEN_UP: {
            _ink->pen_seen   = true;
            _page->pen_on_ui = false;
            kana_page_write_up(_page);
        } break;

        // Apple Pencil's double tap: what the learner set it to do (the system's
        // Apple Pencil settings) — the eraser and back, most often.
        case RDE_EVENT_TYPE_PEN_DOUBLE_TAP: {
            _ink->pen_seen = true;
            kana_toolbar_pen_double_tap(&_ui->bar, _event->data.pen_event_data.tap_action);
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
            const rde_vec_2F _pos = kana_app_touch_to_screen(_touch->init_touch_position);
            // A finger on the UI is using it, not moving the page.
            if(!_ink->drawing && !_page->erasing) {
                kana_pagemenu_card_press(&_ui->page, _pos);   // the translation card's speaker
            }
            if(kana_ui_hit(_ui, _pos)) {
                break;
            }
            if(_app->finger_writes) {
                kana_page_finger_down(_page, _touch->finger_id, _pos);   // one finger writes; two move the page
                break;
            }
            kana_pagemenu_close_context(&_ui->page);
            if(!_ink->drawing && !_page->erasing && !kana_lasso_busy(_app->lasso)) {
                kana_canvas_finger_down(_app->canvas, _touch->finger_id, _pos);
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(_touch->from_pen) {
                break;
            }
            if(_page->finger_state != KANA_PAGE_FINGER_NONE) {
                kana_page_finger_moved(_page, _touch->finger_id, kana_app_touch_to_screen(_touch->moved_touch_position));
                break;
            }
            if(!_ink->drawing) {
                kana_canvas_finger_moved(_app->canvas, _touch->finger_id, kana_app_touch_to_screen(_touch->moved_touch_position));
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_UP: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(_touch->from_pen) {
                break;
            }
            if(_page->finger_state != KANA_PAGE_FINGER_NONE) {
                kana_page_finger_up(_page, _touch->finger_id);
                break;
            }
            kana_page_canvas_finger_up(_page, _touch->finger_id);
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
                kana_pagemenu_card_press(&_ui->page, _screen);   // the translation card's speaker
            }
            if(_event->handled || kana_ui_hit(_ui, _screen)) {
                break;
            }
            // Right-click stands in for the long press.
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_RIGHT) {
                kana_pagemenu_open_context(&_ui->page, _screen, kana_canvas_from_screen(_app->canvas, _screen));
                break;
            }
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT) {
                kana_page_write_down(_page, _screen, false, false);
            }
        } break;

        case RDE_EVENT_TYPE_MOUSE_BUTTON_RELEASED: {
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT) {
                kana_page_write_up(_page);
            }
        } break;
#endif

        default: break;
    }
}

// --- each frame ---------------------------------------------------------------------------

void kana_page_update(kana_page_input* _page) {
    kana_app*    _app    = _page->app;
    rde_window*  _window = _app->window;
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_C)) {
        kana_ink_clear(_app->ink);
    }
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_Z)) {
        kana_ink_undo(_app->ink);
    }
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_Y)) {
        kana_ink_redo(_app->ink);
    }
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_BACKSPACE) || rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_DELETE)) {
        kana_lasso_delete(_app->lasso, _app->ink);
    }
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_ESCAPE)) {
        kana_lasso_clear(_app->lasso, _app->ink);
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
        kana_canvas_reset_view(_app->canvas);
    }
    if(rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_B)) {
        _app->ink->brush_scale = _app->ink->brush_scale == KANA_INK_BRUSH_SCALE_PAGE ? KANA_INK_BRUSH_SCALE_SCREEN : KANA_INK_BRUSH_SCALE_PAGE;
        kana_toolbar_refresh(&_app->ui->bar);
    }

    // Desktop dragging, so the thing can be exercised without a tablet in hand.
    // Not on mobile, where the "mouse" is SDL's echo of the pen.
#if !defined(RDE_PLATFORM_MOBILE)
    if(rde_input_mouse_is_button_pressed(_window, RDE_MOUSE_BUTTON_LEFT)) {
        const rde_vec_2I _m = rde_input_mouse_get_position(_window);
        kana_page_write_moved(_page, (rde_vec_2F){ (f32)_m.x, (f32)_m.y });
    }
#endif

    // A writing finger's moment to decide, and its long press (polled: a finger
    // at rest sends nothing).
    kana_page_finger_poll(_page);
    // A finger held still: the page's context menu.
    rde_vec_2F _press;
    if(kana_canvas_long_press(_app->canvas, &_press)) {
        kana_pagemenu_open_context(&_app->ui->page, _press, kana_canvas_from_screen(_app->canvas, _press));
    }
}

// --- drawing ------------------------------------------------------------------------------

void kana_page_zoom_seen(kana_page_input* _page) {
    _page->zoom_seen = -1.0f;
}

// The zoom as a percentage, in a pill at the bottom-right, while it is changing
// and for a moment after.
RDE_INTERNAL void kana_page_render_zoom(kana_page_input* _page, rde_window* _window) {
    kana_app* _app = _page->app;
    const f32 _zoom = _app->canvas->view.zoom;
    const f64 _now  = rde_engine_get_time_now();
    if(_page->zoom_seen < 0.0f) {
        _page->zoom_seen = _zoom;
    } else if(fabsf(_zoom - _page->zoom_seen) > 1e-5f) {
        _page->zoom_seen     = _zoom;
        _page->zoom_shown_at = _now;
    }
    const f64 _age = _now - _page->zoom_shown_at;
    if(_age >= KANA_PAGE_ZOOM_TIME + KANA_PAGE_ZOOM_FADE || _app->font == NULL) {
        return;
    }
    const f32 _fade = _age <= KANA_PAGE_ZOOM_TIME ? 1.0f : 1.0f - (f32)((_age - KANA_PAGE_ZOOM_TIME) / KANA_PAGE_ZOOM_FADE);

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

    const kana_theme* _t      = kana_theme_active();
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
    kana_draw_text(_app->font, _app->font_px, _text, _center.x - _measured.x * 0.5f, _center.y - _digits * 0.5f, _px, _ink);
}

void kana_page_render(kana_page_input* _page, rde_window* _window) {
    kana_app*        _app  = _page->app;
    const kana_view  _view = _app->canvas->view;
    const rde_vec_2I _size = rde_window_get_size(_window);
    kana_canvas_draw_grid(_app->canvas, _size);
    kana_lasso_render_under(_app->lasso, _app->ink, _view.offset, _view.zoom);
    kana_ink_render(_app->ink, _view.offset, _view.zoom, (rde_vec_2F){ (f32)_size.x * 0.5f, (f32)_size.y * 0.5f }, rde_engine_get_time_now(), _page->show_samples);
    kana_lasso_render_over(_app->lasso, _app->ink, _view.offset, _view.zoom);
    kana_page_render_zoom(_page, _window);
    kana_pagemenu_render(&_app->ui->page, _window);   // Translate with Google's card, by the selection
}

void kana_page_init(kana_page_input* _page, kana_app* _app) {
    memset(_page, 0, sizeof(*_page));
    _page->app           = _app;
    _page->zoom_seen     = -1.0f;
    _page->zoom_shown_at = -100.0;
}
