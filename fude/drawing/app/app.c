#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/app/page.h"
#include "drawing/base/text.h"
#include "drawing/widgets/notice.h"
#include "drawing/base/save.h"

#include <string.h>

// ===========================================================================
// See app.h.
// ===========================================================================

// --- the stack ----------------------------------------------------------------------------

const fude_extension* fude_app_ext(const fude_app* _app) {
    static const fude_extension _none = { 0 };
    return _app->ext != NULL ? _app->ext : &_none;
}

void fude_app_start(fude_app* _app) {
    fude_save_set_folder(fude_app_id(_app));
    if(_app->info != NULL && _app->info->name != NULL) {
        rde_window_set_title(_app->window, _app->info->name);   // a computer's window: the app's name
    }
}

const c8* fude_app_id(const fude_app* _app) {
    if(_app->info->id != NULL) {
        return _app->info->id;
    }
    static c8 _id[64];
    usize     _n = 0;
    for(const c8* _c = _app->info->name; _c != NULL && *_c != 0 && _n + 1u < sizeof(_id); _c++) {
        if((*_c >= 'a' && *_c <= 'z') || (*_c >= '0' && *_c <= '9')) { _id[_n++] = *_c; }
        else if(*_c >= 'A' && *_c <= 'Z')                              { _id[_n++] = (c8)(*_c - 'A' + 'a'); }
    }
    _id[_n] = 0;
    return _id;
}

RDE_INTERNAL b8 fude_app_slot_open(const fude_screen_slot* _slot) {
    return _slot->vt != NULL && _slot->vt->is_open(_slot->self);
}

const fude_screen_slot* fude_app_top(const fude_app* _app) {
    for(u32 _s = 0; _s < _app->screen_count; _s++) {
        if(fude_app_slot_open(&_app->screens[_s])) {
            return &_app->screens[_s];
        }
    }
    return NULL;
}

b8 fude_app_is_open(const fude_app* _app, u32 _screen) {
    return _screen < _app->screen_count && fude_app_slot_open(&_app->screens[_screen]);
}

void fude_app_close_all(fude_app* _app) {
    for(u32 _s = 0; _s < _app->screen_count; _s++) {
        // Each closes as many steps as it has (the album: its page, then itself).
        for(u32 _step = 0; _step < 4u && fude_app_slot_open(&_app->screens[_s]) && _app->screens[_s].vt->close != NULL; _step++) {
            _app->screens[_s].vt->close(_app->screens[_s].self);
        }
    }
    _app->pointer = FUDE_POINTER_NONE;
}

// --- each frame -----------------------------------------------------------------------------

b8 fude_app_update(fude_app* _app, f32 _dt) {
    // A screen back on top — the one over it closed — picks up where it was.
    const fude_screen_slot* _top = fude_app_top(_app);
    if(_top != _app->top_seen) {
        if(_top != NULL && _app->top_seen != NULL && !fude_app_slot_open(_app->top_seen) && _top->vt->resume != NULL) {
            _top->vt->resume(_top->self);
        }
        _app->top_seen = _top;
        _app->pointer  = FUDE_POINTER_NONE;   // a pointer held over the one that went is let go
    }
    if(_top == NULL) {
        return false;
    }

#if !defined(RDE_PLATFORM_MOBILE)
    // The desktop mouse is polled, as on the page.
    if(_app->pointer == FUDE_POINTER_MOUSE && _top->vt->pointer_moved != NULL) {
        const rde_vec_2I _m = rde_input_mouse_get_position(_app->window);
        _app->pointer_last  = (rde_vec_2F){ (f32)_m.x, (f32)_m.y };
        _top->vt->pointer_moved(_top->self, _app->pointer_last, rde_engine_get_time_now());
    }
#endif
    if(_top->vt->update != NULL) {
        _top->vt->update(_app, _top->self, _dt);
    }
    if(rde_input_key_is_just_pressed(_app->window, RDE_KEYBOARD_KEY_ESCAPE) && _top->vt->close != NULL && fude_app_slot_open(_top)) {
        _top->vt->close(_top->self);
    }
    return true;
}

b8 fude_app_render(fude_app* _app) {
    // The first open screen that is not an overlay: it fills the screen.
    for(u32 _s = 0; _s < _app->screen_count; _s++) {
        const fude_screen_slot* _slot = &_app->screens[_s];
        if(fude_app_slot_open(_slot) && !_slot->vt->overlay) {
            fude_screen_frame _frame;
            fude_ui_frame(_app->ui, _s, &_frame);
            _slot->vt->render(_slot->self, &_frame);
            return true;
        }
    }
    return false;
}

void fude_app_render_overlays(fude_app* _app) {
    for(u32 _s = _app->screen_count; _s-- > 0;) {
        const fude_screen_slot* _slot = &_app->screens[_s];
        if(fude_app_slot_open(_slot) && _slot->vt->overlay) {
            fude_screen_frame _frame;
            fude_ui_frame(_app->ui, _s, &_frame);
            _slot->vt->render(_slot->self, &_frame);
        }
    }
}

// --- the pointer -----------------------------------------------------------------------------

// The screen on top's input: one pointer — pen, finger or (desktop) mouse — at a
// time, whichever pressed first. Presses on the UI (its row, its bar) are the
// UI's. A writing screen (Practice) takes a finger only with the hand on, and a
// pen takes over from a writing finger.
void fude_app_screen_event(fude_app* _app, rde_event* _event) {
    const fude_screen_slot* _top = fude_app_top(_app);
    if(_top == NULL || _top->vt->pointer_down == NULL) {
        return;
    }
    const fude_screen* _vt    = _top->vt;
    void*              _self  = _top->self;
    const b8           _write = _vt->input == FUDE_SCREEN_INPUT_WRITE;
    const f64          _now   = rde_engine_get_time_now();

    switch(_event->type) {
        case RDE_EVENT_TYPE_PEN_DOWN: {
            const rde_vec_2F _at = fude_app_window_to_screen(_app, _event->data.pen_event_data.position);
            fude_page_pen_came(_app->page);
            if(_write && _app->pointer == FUDE_POINTER_FINGER) {
                _vt->pointer_up(_self, _app->pointer_last, _now);   // the pen takes over from a writing finger
                _app->pointer = FUDE_POINTER_NONE;
            }
            if(_app->pointer == FUDE_POINTER_NONE && !fude_ui_hit(_app->ui, _at)) {
                _app->pointer      = FUDE_POINTER_PEN;
                _app->pointer_last = _at;
                _vt->pointer_down(_self, _at, true, _now);
            }
        } break;

        case RDE_EVENT_TYPE_PEN_MOVED: {
            if(_app->pointer == FUDE_POINTER_PEN) {
                _app->pointer_last = fude_app_window_to_screen(_app, _event->data.pen_event_data.position);
                _vt->pointer_moved(_self, _app->pointer_last, _now);
            }
        } break;

        case RDE_EVENT_TYPE_PEN_UP: {
            if(_app->pointer == FUDE_POINTER_PEN) {
                _vt->pointer_up(_self, _app->pointer_last, _now);
                _app->pointer = FUDE_POINTER_NONE;
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            const rde_vec_2F        _at    = fude_app_touch_to_screen(_touch->init_touch_position);
            if(!_touch->from_pen && _app->pointer == FUDE_POINTER_NONE && (!_write || _app->finger_writes) && !fude_ui_hit(_app->ui, _at)) {
                _app->pointer        = FUDE_POINTER_FINGER;
                _app->pointer_finger = _touch->finger_id;
                _app->pointer_last   = _at;
                _vt->pointer_down(_self, _at, _app->finger_writes, _now);   // the hand on: it writes where a pen would
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(!_touch->from_pen && _app->pointer == FUDE_POINTER_FINGER && _touch->finger_id == _app->pointer_finger) {
                _app->pointer_last = fude_app_touch_to_screen(_touch->moved_touch_position);
                _vt->pointer_moved(_self, _app->pointer_last, _now);
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_UP: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(!_touch->from_pen && _app->pointer == FUDE_POINTER_FINGER && _touch->finger_id == _app->pointer_finger) {
                _vt->pointer_up(_self, _app->pointer_last, _now);
                _app->pointer = FUDE_POINTER_NONE;
            }
        } break;

#if !defined(RDE_PLATFORM_MOBILE)
        // The mouse reports centre-origin already, unlike pen and touch; it writes
        // on a pad, like the pen.
        case RDE_EVENT_TYPE_MOUSE_BUTTON_PRESSED: {
            const rde_vec_2I _m  = rde_input_mouse_get_position(_app->window);
            const rde_vec_2F _at = { (f32)_m.x, (f32)_m.y };
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT && _app->pointer == FUDE_POINTER_NONE && !_event->handled &&
               !fude_ui_hit(_app->ui, _at)) {
                _app->pointer      = FUDE_POINTER_MOUSE;
                _app->pointer_last = _at;
                _vt->pointer_down(_self, _at, true, _now);
            }
        } break;

        case RDE_EVENT_TYPE_MOUSE_BUTTON_RELEASED: {
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT && _app->pointer == FUDE_POINTER_MOUSE) {
                _vt->pointer_up(_self, _app->pointer_last, _now);
                _app->pointer = FUDE_POINTER_NONE;
            }
        } break;
#endif

        default: break;
    }
}

// Pen positions arrive WINDOW-RELATIVE: top-left origin, Y down. The app's
// screen space is centre-origin with Y up. Every pen position has to make this trip,
// and getting it wrong shows as ink mirrored about the middle of the screen
// rather than as anything subtle.
rde_vec_2F fude_app_window_to_screen(const fude_app* _app, rde_vec_2F _pixel) {
    const rde_vec_2I _size = rde_window_get_size(_app->window);
    return (rde_vec_2F){ _pixel.x - (f32)_size.x * 0.5f, (f32)_size.y * 0.5f - _pixel.y };
}

// Touch events report centre-origin with Y DOWN.
rde_vec_2F fude_app_touch_to_screen(rde_vec_2I _touch) {
    return (rde_vec_2F){ (f32)_touch.x, -(f32)_touch.y };
}

void fude_app_set_finger_writes(fude_app* _app, b8 _on) {
    if(_app->finger_writes == _on) {
        return;
    }
    _app->finger_writes = _on;
    fude_toolbar_refresh(&_app->ui->bar);
    fude_notice_show(fude_text(_on ? FUDE_TEXT_FINGER_ON : FUDE_TEXT_FINGER_OFF));   // whoever switched it
}
