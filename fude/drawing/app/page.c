// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/app/page.h"
#include "drawing/base/redraw.h"
#include "drawing/app/app.h"
#include "drawing/app/documents.h"
#include "drawing/app/ui.h"
#include "drawing/app/import.h"
#include "drawing/base/theme.h"
#include "drawing/base/draw.h"

// ===========================================================================
// See page.h.
//
// The pen, the mouse and a writing finger all write through the same three functions (fude_page_write_down, _moved
// and _up), which pick the tool: erase, lasso, or ink and marker. Fingers otherwise go to the canvas (canvas.h), which
// pans, zooms and reports taps and the long press. With the toolbar's hand on, a finger first goes through the small
// state machine below (FUDE_PAGE_FINGER_), which decides whether it writes or moves the page.
// ===========================================================================

#define FUDE_PAGE_FINGER_MOVE   6.0f    // A finger that moves this far (screen units) from where it landed writes.
#define FUDE_PAGE_FINGER_WAIT   0.12    // A finger down this long (seconds) with some movement writes.
#define FUDE_PAGE_ZOOM_TIME     1.2     // The zoom's toast stays this many seconds...
#define FUDE_PAGE_ZOOM_FADE     0.4     // ...then fades out over this many.

// What a finger is doing while the hand is on. NONE: no finger. PENDING: one finger is down and its points are kept
// until it is clear whether it writes. WRITING: it writes. GESTURE: two or more fingers move the page. IGNORED: a
// finger that opened the context menu, until it lifts.
typedef enum { FUDE_PAGE_FINGER_NONE = 0, FUDE_PAGE_FINGER_PENDING, FUDE_PAGE_FINGER_WRITING, FUDE_PAGE_FINGER_GESTURE, FUDE_PAGE_FINGER_IGNORED } FUDE_PAGE_FINGER_;

// Selects the area inside a lasso loop that took no ink but went round a document's text (lasso.h), so the app's text
// row shows for it. Only an app with a text row gets this.
void fude_page_lasso_text(fude_page_input* _page) {
    fude_app*  _app = _page->app;
    rde_vec_2F _min, _max;
    c8         _some[8];   // Only whether there is any text matters, so a little is enough.
    b8         _pending = false;   // Or a page under it still to be read (a scan's, for example).
    if(fude_app_ext(_app)->text_row != NULL && fude_lasso_looped(_app->lasso, &_min, &_max) &&
       (fude_doc_text_in(&_page->doc, _min, _max, _some, sizeof(_some), &_pending) > 0u || _pending)) {
        fude_lasso_select_area(_app->lasso, _min, _max);
    }
}

// Returns where a point of the screen is on the PAGE. It also hands the ink the current zoom, which the ink needs to
// store widths in page units.
RDE_INTERNAL rde_vec_2F fude_page_at(fude_page_input* _page, rde_vec_2F _screen) {
    _page->app->ink->zoom = _page->app->canvas->view.zoom;
    return fude_canvas_from_screen(_page->app->canvas, _screen);
}

// Erases at a screen point, with the eraser's radius in screen units turned into page units.
RDE_INTERNAL void fude_page_erase_at(fude_page_input* _page, rde_vec_2F _screen) {
    fude_app* _app = _page->app;
    fude_ink_erase_at(_app->ink, fude_canvas_from_screen(_app->canvas, _screen), _app->ink->eraser_radius / _app->canvas->view.zoom);
}

// --- writing: the pen's, the mouse's, and a writing finger's ---------------------------

// Starts what the tool does at _screen: an erase (the Erase tool, or the pen's eraser end when _eraser), a lasso loop,
// or a stroke of ink or of the marker. _from_pen says the pen, rather than a finger or the mouse, is writing.
RDE_INTERNAL void fude_page_write_down(fude_page_input* _page, rde_vec_2F _screen, b8 _from_pen, b8 _eraser) {
    fude_app* _app = _page->app;
    fude_pagemenu_close_context(&_app->ui->page);   // A press elsewhere dismisses it, and still does its own job.
    fude_canvas_release_fingers(_app->canvas);     // Writing takes over from any finger gesture in progress.
    if(_app->ui->bar.tool == FUDE_TOOL_ERASE || _eraser) {
        _page->erasing = true;   // The Erase tool, or a pen's own eraser end.
        fude_page_erase_at(_page, _screen);
        return;
    }
    if(_app->ui->bar.tool == FUDE_TOOL_LASSO) {
        fude_lasso_pen_down(_app->lasso, _app->ink, fude_page_at(_page, _screen), _app->canvas->view.zoom);
        return;
    }
    _app->ink->marking = _app->ui->bar.tool == FUDE_TOOL_MARK;   // The marker's stroke, or the pen's.
    fude_ink_begin(_app->ink, fude_page_at(_page, _screen), _from_pen);
}

// Moves on what was started: the erase, the lasso, or the stroke. Nothing happens when nothing was started, so a pen
// hovering over the screen draws nothing.
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

// Ends whatever was started (each end does nothing when its part was not started), then checks whether a lasso loop
// went round a document's text.
RDE_INTERNAL void fude_page_write_up(fude_page_input* _page) {
    fude_app* _app = _page->app;
    _page->erasing = false;
    fude_ink_erase_end(_app->ink);   // The whole swipe is one undo step.
    fude_ink_end(_app->ink);
    fude_lasso_pen_up(_app->lasso, _app->ink, _app->canvas->view.zoom);
    fude_page_lasso_text(_page);
}

// The pending finger starts writing, from where it landed and through every point it has been since.
RDE_INTERNAL void fude_page_finger_start_writing(fude_page_input* _page) {
    _page->app->ink->eraser      = false;
    _page->app->ink->sample_time = rde_engine_get_time_now();
    fude_page_write_down(_page, _page->finger_points[0], false, false);
    for(u32 _i = 1; _i < _page->finger_count; _i++) {
        fude_page_write_moved(_page, _page->finger_points[_i]);
    }
    _page->finger_state = FUDE_PAGE_FINGER_WRITING;
}

// Returns how far _p is from where the pending finger landed, in screen units.
RDE_INTERNAL f32 fude_page_finger_moved_from_start(const fude_page_input* _page, rde_vec_2F _p) {
    const rde_vec_2F _d = { _p.x - _page->finger_points[0].x, _p.y - _page->finger_points[0].y };
    return rde_math_sqrt_f32(_d.x * _d.x + _d.y * _d.y);
}

// A finger lands while the hand is on. The first one becomes pending. A second landing before the first writes makes
// both the page's gesture, and later ones join it. One landing while a finger writes is a resting hand and is ignored.
RDE_INTERNAL void fude_page_finger_down(fude_page_input* _page, u64 _id, rde_vec_2F _screen) {
    fude_canvas* _canvas = _page->app->canvas;
    if(_page->finger_state == FUDE_PAGE_FINGER_WRITING || _page->finger_state == FUDE_PAGE_FINGER_IGNORED) {
        return;   // A resting hand.
    }
    if(_page->finger_state == FUDE_PAGE_FINGER_PENDING) {
        // A second finger landed before the first wrote, so both are the page's gesture. The first finger is handed
        // to the canvas where it landed and where it is now.
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

// A finger moves while the hand is on. A gesture's fingers move the page. The pending finger keeps its points (the
// last one is overwritten once FUDE_PAGE_FINGER_POINTS are kept) and writes once it has moved far enough; a writing
// finger writes on.
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

// Hands a finger's lift to the canvas. If it ended a tap of two fingers, it undoes; of three, it redoes.
RDE_INTERNAL void fude_page_canvas_finger_up(fude_page_input* _page, u64 _id) {
    const FUDE_CANVAS_TAP_ _tap = fude_canvas_finger_up(_page->app->canvas, _id);
    if(_tap == FUDE_CANVAS_TAP_TWO) {
        fude_ink_undo(_page->app->ink);
    } else if(_tap == FUDE_CANVAS_TAP_THREE) {
        fude_ink_redo(_page->app->ink);
    }
}

// A finger lifts while the hand is on. The gesture ends when its last finger lifts. A pending finger that lifts was a
// short touch, so it writes a dot.
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
        fude_page_finger_start_writing(_page);   // A short touch is a dot.
    }
    if(_page->finger_state == FUDE_PAGE_FINGER_WRITING) {
        fude_page_write_up(_page);
    }
    _page->finger_state = FUDE_PAGE_FINGER_NONE;
}

// Called once a frame, because a finger at rest sends no events. A pending finger held quite still for the long
// press's time opens the context menu; one that rested a moment (FUDE_PAGE_FINGER_WAIT) with a little movement (over
// 1.5 screen units) writes.
RDE_INTERNAL void fude_page_finger_poll(fude_page_input* _page) {
    if(_page->finger_state != FUDE_PAGE_FINGER_PENDING) {
        return;
    }
    const f64 _held  = rde_engine_get_time_now() - _page->finger_since;
    const f32 _moved = fude_page_finger_moved_from_start(_page, _page->finger_points[_page->finger_count - 1]);
    // Render on demand: the next moment to decide at, the wait's end and then the long press's.
    fude_redraw_after((_held < FUDE_PAGE_FINGER_WAIT ? FUDE_PAGE_FINGER_WAIT : FUDE_CANVAS_LONG_PRESS_TIME) - _held);
    if(_held >= FUDE_CANVAS_LONG_PRESS_TIME && _moved < FUDE_PAGE_FINGER_MOVE) {
        fude_pagemenu_open_context(&_page->app->ui->page, _page->finger_points[0], fude_canvas_from_screen(_page->app->canvas, _page->finger_points[0]));
        _page->finger_state = FUDE_PAGE_FINGER_IGNORED;   // Until it lifts.
    } else if(_held >= FUDE_PAGE_FINGER_WAIT && _moved > 1.5f) {
        fude_page_finger_start_writing(_page);
    }
}

// Lets go of what the fingers were doing, ending a finger's stroke or the page's gesture, when a pen comes down or a
// screen opens over the page.
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

// A pen came down: remembers that a pen was used (saved), and turns the hand off if it was on.
void fude_page_pen_came(fude_page_input* _page) {
    _page->app->pen_ever = true;
    if(_page->app->finger_writes) {
        fude_page_finger_forget(_page);
        fude_app_set_finger_writes(_page->app, false);
    }
}

// Ends whatever the pen, the mouse or a finger was doing on the page. See the header.
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

// Hands a press at _screen to what the app draws over the page (extension.h: page_press; a study app's translation
// card, for example). The page still gets the press as well.
RDE_INTERNAL void fude_page_press_app(fude_page_input* _page, rde_vec_2F _screen) {
    const fude_extension* _ext = fude_app_ext(_page->app);
    if(_ext->page_press != NULL) {
        _ext->page_press(_page->app, _screen);
    }
}

// Handles a pen event on the page (fude_page_event); any other event is left alone.
RDE_INTERNAL void fude_page_pen_event(fude_page_input* _page, rde_event* _event) {
    fude_app* _app = _page->app;
    fude_ui*  _ui  = _app->ui;
    fude_ink* _ink = _app->ink;

    switch(_event->type) {
        // --- pen ---------------------------------------------------------
        //
        // PEN_AXIS carries pressure and tilt and NOTHING else carries them, so it has to be folded into the live
        // state before any move is sampled.
        case RDE_EVENT_TYPE_PEN_AXIS: {
            fude_ink_pen_axis(_ink, &_event->data.pen_event_data);
        } break;

        case RDE_EVENT_TYPE_PEN_DOWN: {
            const rde_event_pen* _pen    = &_event->data.pen_event_data;
            const rde_vec_2F     _screen = fude_app_window_to_screen(_app, _pen->position);
            fude_page_pen_came(_page);
            _ink->pen_seen    = true;
            _ink->eraser      = _pen->eraser;
            _ink->sample_time = (f64)_event->time_stamp * 1e-9;   // SDL's event time, in nanoseconds.

            // On the UI the pen is pressing a button, which the UI gets as a synthetic mouse click right after this
            // event, so it must not also write underneath. What the app draws over the page (a study app's translation
            // card, for example) answers here.
            fude_page_press_app(_page, _screen);
            if(fude_ui_hit(_ui, _screen)) {
                _page->pen_on_ui = true;
                break;
            }
            fude_toolbar_close_panels(&_ui->bar);   // A press on the page closes the toolbar's panels.
            fude_page_write_down(_page, _screen, true, _pen->eraser);
        } break;

        case RDE_EVENT_TYPE_PEN_MOVED: {
            _ink->pen_seen = true;
            if(_page->pen_on_ui) {
                break;
            }
            // This writes only while the tip is down (write_moved draws only then): a pen hovering in proximity still
            // reports motion, and inking on hover would be a mess.
            _ink->sample_time = (f64)_event->time_stamp * 1e-9;   // SDL's event time, in nanoseconds.
            fude_page_write_moved(_page, fude_app_window_to_screen(_app, _event->data.pen_event_data.position));
        } break;

        case RDE_EVENT_TYPE_PEN_UP: {
            _ink->pen_seen   = true;
            _page->pen_on_ui = false;
            fude_page_write_up(_page);
        } break;

        // Apple Pencil's double tap does what the user set it to do in the system's Apple Pencil settings: most
        // often, switching to the eraser and back.
        case RDE_EVENT_TYPE_PEN_DOUBLE_TAP: {
            _ink->pen_seen = true;
            fude_toolbar_pen_double_tap(&_ui->bar, _event->data.pen_event_data.tap_action);
        } break;

        case RDE_EVENT_TYPE_PEN_PROXIMITY_IN:
        case RDE_EVENT_TYPE_PEN_PROXIMITY_OUT: {
            _ink->pen_seen = true;
        } break;

        default: break;
    }
}

// Handles a touch event on the page (fude_page_event); any other event is left alone.
RDE_INTERNAL void fude_page_touch_event(fude_page_input* _page, rde_event* _event) {
    fude_app* _app = _page->app;
    fude_ui*  _ui  = _app->ui;
    fude_ink* _ink = _app->ink;

    switch(_event->type) {
        // --- touch: moves the page; writes only with the hand on -----------
        //
        // SDL echoes every pen contact as a synthetic touch too (on by default), sent right after PEN_DOWN. The engine
        // flags those `from_pen` and they are skipped; otherwise writing would drag the page along. A finger landing
        // while the pen is writing is a resting palm, not a gesture, so it is never picked up.
        case RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(_touch->from_pen) {
                break;
            }
            _page->touch_seen = true;
            const rde_vec_2F _pos = fude_app_touch_to_screen(_touch->init_touch_position);
            // A finger on the UI is using it, not moving the page.
            if(!_ink->drawing && !_page->erasing) {
                fude_page_press_app(_page, _pos);   // What the app drew over the page (the translation card's speaker).
            }
            if(fude_ui_hit(_ui, _pos)) {
                break;
            }
            fude_toolbar_close_panels(&_ui->bar);
            if(_app->finger_writes) {
                fude_page_finger_down(_page, _touch->finger_id, _pos);   // One finger writes; two move the page.
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

        default: break;
    }
}

// --- the mouse, on a computer ---------------------------------------
//
// Desktop only. On mobile, SDL synthesizes mouse events from the pen AND from touch
// (SDL_HINT_PEN_MOUSE_EVENTS / TOUCH_MOUSE_EVENTS, on by default), so this would open yet another stroke on
// every pen-down.
#if !defined(RDE_PLATFORM_MOBILE)
// Handles a mouse button on the page (fude_page_event); any other event is left alone.
RDE_INTERNAL void fude_page_mouse_event(fude_page_input* _page, rde_event* _event) {
    fude_app* _app = _page->app;
    fude_ui*  _ui  = _app->ui;

    switch(_event->type) {
        case RDE_EVENT_TYPE_MOUSE_BUTTON_PRESSED: {
            // The mouse reports CENTRE-ORIGIN already, unlike pen and touch.
            const rde_vec_2I _m      = rde_input_mouse_get_position(_app->window);
            const rde_vec_2F _screen = { (f32)_m.x, (f32)_m.y };
            // Skip a click the UI already took (a button), or one that landed on it.
            if(!_event->handled) {
                fude_page_press_app(_page, _screen);   // Such as the translation card's speaker.
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

        default: break;
    }
}
#endif

// Handles a pen, touch or mouse event on the page. See the header for what each does. Each kind has its own step, and
// an event is handled by the one of its kind.
void fude_page_event(fude_page_input* _page, rde_event* _event) {
    fude_page_pen_event(_page, _event);
    fude_page_touch_event(_page, _event);
#if !defined(RDE_PLATFORM_MOBILE)
    fude_page_mouse_event(_page, _event);
#endif
}

// --- each frame ---------------------------------------------------------------------------

// Runs the page's part of the frame. See the header. The keys work on every platform with a keyboard.
void fude_page_update(fude_page_input* _page) {
    fude_app*    _app    = _page->app;
    rde_window*  _window = _app->window;
    fude_import_update(_app);   // A document brought in becomes a canvas of its own, opened.
    // The open canvas's document, shown where the page is (doc.h). An empty path means the canvas has none.
    const fude_note*      _note = fude_notes_find(_app->notes, _app->notes->open);
    const fude_extension* _ext  = fude_app_ext(_app);
    c8                    _doc_path[RDE_MAX_PATH];
    if(_note == NULL || _note->kind != FUDE_NOTE_CANVAS || !fude_documents_path(_app, _note->id, _note->document, _doc_path, sizeof(_doc_path))) {
        _doc_path[0] = 0;
    }
    const fude_doc_place _at = { _doc_path, _app->canvas, _app->window, _ext->picture_read, _ext->picture_lines };
    fude_doc_update(&_page->doc, &_at);
    fude_canvas_update(_app->canvas, rde_window_get_size(_window), rde_window_get_safe_area_insets(_window));   // A flick's coast; a document's bounds.
    // A keyboard's shortcuts, never while a text field has the keyboard (a rename, a search): what is typed there is
    // text. Undo and redo are the platform's own, with Command or Control held: Z undoes, Shift+Z or Y redoes. No
    // single letter changes the page: on a tablet with a keyboard, a slip of a finger must never clear a page
    // (Clear is on the toolbar).
    const b8 _typing  = _app->ui != NULL && rde_ui_canvas_get_focused(_app->ui->canvas) != NULL;
    const b8 _command = rde_input_key_is_pressed(_window, RDE_KEYBOARD_KEY_LEFT_GUI) || rde_input_key_is_pressed(_window, RDE_KEYBOARD_KEY_RIGHT_GUI) ||
                        rde_input_key_is_pressed(_window, RDE_KEYBOARD_KEY_LEFT_CTRL) || rde_input_key_is_pressed(_window, RDE_KEYBOARD_KEY_RIGHT_CTRL);
    const b8 _shift   = rde_input_key_is_pressed(_window, RDE_KEYBOARD_KEY_LEFT_SHIFT) || rde_input_key_is_pressed(_window, RDE_KEYBOARD_KEY_RIGHT_SHIFT);
    if(!_typing && _command && rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_Z)) {
        if(_shift) {
            fude_ink_redo(_app->ink);
        } else {
            fude_ink_undo(_app->ink);
        }
    }
    if(!_typing && _command && rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_Y)) {
        fude_ink_redo(_app->ink);
    }
    if(!_typing && (rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_BACKSPACE) || rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_DELETE))) {
        fude_lasso_delete(_app->lasso, _app->ink);
    }
#if defined(RDE_DEBUG)
    if(!_typing && rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_M)) {
        _page->show_samples = !_page->show_samples;
    }
    if(!_typing && rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_H)) {
        _page->show_hud = !_page->show_hud;
    }
#endif
    if(!_typing && rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_R)) {
        fude_canvas_reset_view(_app->canvas);   // R: the view back to the start (it changes nothing on the page)
    }
    if(!_typing && rde_input_key_is_just_pressed(_window, RDE_KEYBOARD_KEY_B)) {   // B: the brush by the page or the screen
        _app->ink->brush_scale = _app->ink->brush_scale == FUDE_INK_BRUSH_SCALE_PAGE ? FUDE_INK_BRUSH_SCALE_SCREEN : FUDE_INK_BRUSH_SCALE_PAGE;
        fude_toolbar_refresh(&_app->ui->bar);
    }

    // Desktop dragging, so the page can be exercised without a tablet in hand. Not on mobile, where the "mouse" is
    // SDL's echo of the pen.
#if !defined(RDE_PLATFORM_MOBILE)
    if(rde_input_mouse_is_button_pressed(_window, RDE_MOUSE_BUTTON_LEFT)) {
        const rde_vec_2I _m = rde_input_mouse_get_position(_window);
        fude_page_write_moved(_page, (rde_vec_2F){ (f32)_m.x, (f32)_m.y });
    }
#endif

    // A writing finger's moment to decide, and its long press. They are polled because a finger at rest sends nothing.
    fude_page_finger_poll(_page);
    // A finger held still opens the page's context menu (the hand off: the canvas tracks the fingers).
    rde_vec_2F _press;
    if(fude_canvas_long_press(_app->canvas, &_press)) {
        fude_pagemenu_open_context(&_app->ui->page, _press, fude_canvas_from_screen(_app->canvas, _press));
    }
}

// --- drawing ------------------------------------------------------------------------------

// Marks the zoom as not seen yet, so the next frame takes the zoom then as seen and shows no toast for it.
void fude_page_zoom_seen(fude_page_input* _page) {
    _page->zoom_seen = -1.0f;
}

// Draws the zoom as a percentage, in a pill at the bottom right (left, right to left), while it is changing and for a
// moment after, then fades it out.
RDE_INTERNAL void fude_page_render_zoom(fude_page_input* _page, rde_window* _window) {
    fude_app* _app = _page->app;
    const f32 _zoom = fude_canvas_zoom_shown(_app->canvas);   // On a document, 100% is its width across the screen.
    const f64 _now  = rde_engine_get_time_now();
    if(_page->zoom_seen < 0.0f) {
        _page->zoom_seen = _zoom;
    } else if(rde_math_abs_f32(_zoom - _page->zoom_seen) > 1e-5f) {
        _page->zoom_seen     = _zoom;
        _page->zoom_shown_at = _now;
    }
    const f64 _age = _now - _page->zoom_shown_at;
    if(_age >= FUDE_PAGE_ZOOM_TIME + FUDE_PAGE_ZOOM_FADE || _app->font == NULL) {
        return;
    }
    const f32 _fade = _age <= FUDE_PAGE_ZOOM_TIME ? 1.0f : 1.0f - (f32)((_age - FUDE_PAGE_ZOOM_TIME) / FUDE_PAGE_ZOOM_FADE);
    // Render on demand: still while fully shown, then fading every frame until it is gone.
    if(_age < FUDE_PAGE_ZOOM_TIME) {
        fude_redraw_after(FUDE_PAGE_ZOOM_TIME - _age);
    } else {
        fude_redraw_changed();
    }

    // The text is measured once per new value. The pill fits it, and it sits in the middle: across by its width, and
    // up and down by its digits' height.
    static c8         _shown[16] = "";
    static rde_vec_2F _measured  = { 0.0f, 0.0f };
    c8 _text[16];
    rde_snprintf(_text, sizeof(_text), "%.0f%%", (f64)(_zoom * 100.0f));
    const f32 _px    = 20.0f;
    const f32 _scale = _px / _app->font_px;
    if(rde_strcmp(_text, _shown) != 0) {
        rde_snprintf(_shown, sizeof(_shown), "%s", _text);
        _measured = rde_rich_text_measure(_text, _app->font, _scale, 10000.0f, false);
    }
    const f32 _em     = _measured.y / 1.17f;          // A line is about 1.17 em in Roboto.
    const f32 _digits = _em * 0.711f;                 // Roboto's digit (cap) height, in em.

    const fude_theme* _t      = fude_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _safe   = rde_window_get_safe_area_insets(_window);   // Left, top, right, bottom.
    const rde_vec_2F  _pill   = { rde_math_max_f32(72.0f, _measured.x + 32.0f), rde_math_max_f32(40.0f, _digits + 26.0f) };
    // At the bottom right, or the bottom left in a right-to-left UI. The page is drawn outside the screens' mirror
    // (draw.h: fude_draw_mirror_begin), where fude_draw_x changes nothing, so the side is chosen here, each from its
    // own edge's safe area.
    const f32         _right  = (f32)_size.x * 0.5f - (f32)_safe.z - 16.0f - _pill.x * 0.5f;
    const f32         _left   = -(f32)_size.x * 0.5f + (f32)_safe.x + 16.0f + _pill.x * 0.5f;
    const rde_vec_2F  _center = { fude_draw_is_rtl() ? _left : _right, -(f32)_size.y * 0.5f + (f32)_safe.w + 16.0f + _pill.y * 0.5f };
    rde_color _back = _t->panel;
    _back.a         = (u8)((f32)_back.a * _fade);
    rde_rendering_2d_draw_rounded_rectangle(_center, _pill, 1.0f, 8u, _back, NULL);
    rde_color _ink = _t->button_text;
    _ink.a         = (u8)((f32)_ink.a * _fade);
    // The position is where the text starts, on its baseline.
    fude_draw_text(_app->font, _app->font_px, _text, _center.x - _measured.x * 0.5f, _center.y - _digits * 0.5f, _px, _ink);
}

// Draws the page from the bottom up: the paper, the document, the selection's glow under the ink, the ink, the
// lasso's loop and box over it, the zoom's toast, the document's page pill, and what the app draws over the page.
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
    fude_doc_render_pill(&_page->doc, _app->canvas->view, _app->font, _app->font_px, _window);
    if(fude_app_ext(_app)->page_render != NULL) {
        fude_app_ext(_app)->page_render(_app, _window);   // Such as a study app's Translate with Google card.
    }
}

// Sets the page up with an empty document. The zoom has not been seen yet, and its toast was shown long ago, so none
// shows at launch.
void fude_page_init(fude_page_input* _page, fude_app* _app) {
    rde_memset(_page, 0, sizeof(*_page));
    _page->app           = _app;
    _page->zoom_seen     = -1.0f;
    _page->zoom_shown_at = -100.0;
    fude_doc_init(&_page->doc);
}

// Closes the document and frees what it holds, with the list of its pages the canvas has turned (canvas.h).
void fude_page_destroy(fude_page_input* _page) {
    fude_doc_destroy(&_page->doc);
    if(_page->app != NULL && _page->app->canvas != NULL) {
        fude_page_reset(&_page->app->canvas->page);
    }
}
