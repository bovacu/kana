// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/app/app.h"
#include "drawing/base/crash.h"
#include "drawing/app/ui.h"
#include "drawing/app/page.h"
#include "drawing/base/text.h"
#include "drawing/widgets/notice.h"
#include "drawing/base/save.h"
#include "drawing/base/android.h"
#include "drawing/ink/lasso.h"
#include "drawing/widgets/kit.h"
#include "drawing/base/draw.h"

// ===========================================================================
// See app.h.
//
// The screens are a fixed table in the app (fude_app.screens) whose order is the stack's: the first open one is on
// top. Nothing is pushed or popped; a screen is on the stack while its is_open says so. Each frame Back and Escape are
// handled first (fude_app_back), then a screen that has come back to the top is resumed, then the top one updates.
// Pointer events go to the top screen one pointer at a time (fude_app_screen_event).
// ===========================================================================

// --- the stack ----------------------------------------------------------------------------

// Returns the app's extension, or a static empty one when it has none, so callers never check for NULL.
const fude_extension* fude_app_ext(const fude_app* _app) {
    static const fude_extension _none = { 0 };
    return _app->ext != NULL ? _app->ext : &_none;
}

// Readies what needs the app's info before anything else runs: Android's Java, the save folder, the crash reports and
// the window's title. Stops the app with a clear message when it has no info or no name.
void fude_app_start(fude_app* _app) {
    // Every app describes itself (info.h); its name is what its id, save folder and crash reports are made from, so
    // without it nothing else here can work. Checked once, here, so nothing after needs to.
    rde_assert_fatal(_app->info != NULL && _app->info->name != NULL, "app: no fude_app_info, or one without a name");
#if defined(RDE_PLATFORM_ANDROID)
    fude_android_init();   // The apps' Java (android.h), which must be found on this thread, the engine's.
#endif
    fude_save_set_folder(fude_app_id(_app));
    // Install the crash handlers (crash.h) and mark this session as running. What the last session left is asked
    // about later, by the crash card (crashcard.h).
    fude_crash_install(fude_save_dir(), _app->info->name, _app->info->version != NULL ? _app->info->version : "");
    rde_window_set_title(_app->window, _app->info->name);   // On a computer, the window shows the app's name.
}

// On Android, makes the window's units dp, like iOS's points, so a tablet gets an iPad's sizes. Elsewhere the units
// are already points, and nothing is done.
void fude_app_window(rde_window* _window) {
#if defined(RDE_PLATFORM_ANDROID)
    rde_window_set_density_scaling(_window, true);
#else
    RDE_UNUSED(_window);
#endif
}

// The size of a window unit for each FUDE_UI_SIZE_: Small 0.8, Medium 1, Large 1.15. AUTO's entry is never used,
// because fude_app_ui_size never returns AUTO.
RDE_INTERNAL const f32 FUDE_APP_UI_SCALES[FUDE_UI_SIZE_COUNT] = { 1.0f, 0.8f, 1.0f, 1.15f };

// Returns the size chosen in Settings, or, when none was chosen, the device's: Small on a phone, Medium otherwise.
FUDE_UI_SIZE_ fude_app_ui_size(const fude_app* _app) {
    if(_app->ui_size != FUDE_UI_SIZE_AUTO && _app->ui_size < FUDE_UI_SIZE_COUNT) {
        return (FUDE_UI_SIZE_)_app->ui_size;
    }
    // Use the device's size. A phone is a screen whose shorter side, in the device's own points (dp), is under
    // FUDE_KIT_COMPACT_BELOW, where the phone's layout starts. The window's size is in scaled units, so the current
    // scale is multiplied back in to undo it.
    const rde_vec_2I _size = rde_window_get_size(_app->window);
    const f32        _side = rde_math_min_f32((f32)_size.x, (f32)_size.y) * rde_window_get_ui_scale(_app->window);
    return _side < FUDE_KIT_COMPACT_BELOW ? FUDE_UI_SIZE_SMALL : FUDE_UI_SIZE_MEDIUM;
}

// Sets the window's unit size for the interface size in effect, only when it changed, because setting it lays out
// everything again.
void fude_app_apply_ui_size(fude_app* _app) {
    const f32 _scale = FUDE_APP_UI_SCALES[fude_app_ui_size(_app)];
    if(rde_math_abs_f32(rde_window_get_ui_scale(_app->window) - _scale) > 0.001f) {
        rde_window_set_ui_scale(_app->window, _scale);   // The window's size changes, so everything is laid out again.
    }
}

// Returns the app's id, or makes one from its name: lowercase letters and digits only, the rest dropped ("My App 2"
// becomes "myapp2"), at most FUDE_SAVE_FOLDER_MAX - 1 characters, as the save folder it names holds (save.h says why
// that is enough: an app's name is never the person's). That one is in a static buffer, rewritten by each call.
const c8* fude_app_id(const fude_app* _app) {
    if(_app->info->id != NULL) {
        return _app->info->id;
    }
    static c8 _id[FUDE_SAVE_FOLDER_MAX];
    usize     _n = 0;
    for(const c8* _c = _app->info->name; _c != NULL && *_c != 0 && _n + 1u < sizeof(_id); _c++) {
        if((*_c >= 'a' && *_c <= 'z') || (*_c >= '0' && *_c <= '9')) { _id[_n++] = *_c; }
        else if(*_c >= 'A' && *_c <= 'Z')                              { _id[_n++] = (c8)(*_c - 'A' + 'a'); }
    }
    _id[_n] = 0;
    return _id;
}

// Whether the screen in _slot is open. A slot with no screen in it (no vt) never is.
RDE_INTERNAL b8 fude_app_slot_open(const fude_screen_slot* _slot) {
    return _slot->vt != NULL && _slot->vt->is_open(_slot->self);
}

// Returns the first open screen in the table, which is the one on top, or NULL when only the page shows.
const fude_screen_slot* fude_app_top(const fude_app* _app) {
    for(u32 _s = 0; _s < _app->screen_count; _s++) {
        if(fude_app_slot_open(&_app->screens[_s])) {
            return &_app->screens[_s];
        }
    }
    return NULL;
}

// Whether screen _screen, its index in the table, is open. An index past the table is not.
b8 fude_app_is_open(const fude_app* _app, u32 _screen) {
    return _screen < _app->screen_count && fude_app_slot_open(&_app->screens[_screen]);
}

// Closes every screen, calling each one's close up to four times, until it says it is no longer open, and lets go
// of the pointer.
void fude_app_close_all(fude_app* _app) {
    for(u32 _s = 0; _s < _app->screen_count; _s++) {
        // A screen may take several steps to close: the album, for example, closes its page first, then itself.
        for(u32 _step = 0; _step < 4u && fude_app_slot_open(&_app->screens[_s]) && _app->screens[_s].vt->close != NULL; _step++) {
            _app->screens[_s].vt->close(_app->screens[_s].self);
        }
    }
    _app->pointer = FUDE_POINTER_NONE;
}

// --- Back ---------------------------------------------------------------------------------
// Android's Back and the desktop's Escape close the topmost thing that is open, the way its own button would. In
// order: a row's More card, the side panel's cards and then the panel, the toolbar's palettes, the page's menu, what
// the app's own page has open, the screen on top (its row's Back, else its Close, else its close function; a screen
// with none, such as the welcome, stays), the document bar's search, and finally a selection on the page. When
// nothing is open, Android's Back goes to the system, which leaves the app, and Escape does nothing. _system is true
// for Android's Back.
RDE_INTERNAL void fude_app_back(fude_app* _app, b8 _system) {
    fude_ui* _ui = _app->ui;
    // A row's More card (on a phone) is over everything, so it closes first.
    b8 _closed = fude_row_close_menu(&_ui->page.selection) || fude_row_close_menu(&_ui->page.text) || fude_row_close_menu(&_ui->page.context);
    for(u32 _r = 0; !_closed && _r < FUDE_UI_ROWS; _r++) {
        _closed = fude_row_close_menu(&_ui->rows[_r]);
    }
    if(_closed) {
        return;
    }
    if(fude_side_back(_ui)) {
        return;
    }
    if(_ui->bar.palette_open || _ui->bar.paper_open) {
        fude_toolbar_set_palette_open(&_ui->bar, false);
        fude_toolbar_set_paper_open(&_ui->bar, false);
        fude_ui_update(_ui);
        return;
    }
    if(_ui->page.context.open) {
        fude_pagemenu_close_context(&_ui->page);
        fude_ui_update(_ui);
        return;
    }
    const fude_screen_slot* _top = fude_app_top(_app);
    const fude_page_kind*   _pk  = fude_app_ext(_app)->page_kind;
    if(_top == NULL && _pk != NULL && _pk->back != NULL && _pk->back(_app)) {
        fude_ui_update(_ui);
        return;
    }
    if(_top != NULL) {
        if(!fude_ui_press_back(_ui) && _top->vt->close != NULL) {
            _top->vt->close(_top->self);
        }
        _app->pointer = FUDE_POINTER_NONE;
        return;
    }
    if(fude_docbar_back(&_ui->docbar)) {
        fude_ui_update(_ui);
        return;
    }
    // Last, clear the selection: the app's own page's, or else the lasso's.
    rde_vec_2F _min, _max;
    const fude_page_kind* _kind = fude_app_ext(_app)->page_kind;
    if(_kind != NULL && _kind->selection != NULL) {
        b8 _busy = false;
        if(_kind->selection(_app, &_min, &_max, &_busy)) {
            _kind->command(_app, FUDE_PAGE_CMD_DESELECT, (rde_vec_2F){ 0.0f, 0.0f });
            return;
        }
    } else if(fude_lasso_busy(_app->lasso) || fude_lasso_box(_app->lasso, _app->ink, &_min, &_max)) {
        fude_lasso_clear(_app->lasso, _app->ink);
        return;
    }
    if(_system) {
        rde_mobile_system_back();
    }
}

// --- each frame -----------------------------------------------------------------------------

// Runs the screens' part of the frame. See the header for the order. Returns false when no screen is open.
b8 fude_app_update(fude_app* _app, f32 _dt) {
    fude_app_apply_ui_size(_app);   // First, before anything reads the window's size.
#if defined(RDE_PLATFORM_MOBILE)
    // Lock the screen to portrait, either way up, on every phone and tablet; an app that turns (fude_app_info.turns,
    // such as Sketching) may be any way round. It is asked here, once the window has its size, because SDL's own
    // request when the window is created can come too early and do nothing, and Android then rotates freely. It is
    // asked again each frame until it succeeds.
    static b8 _portrait = false;
    if(!_portrait) {
        _portrait = rde_window_set_orientation_lock(_app->info->turns ? RDE_ORIENTATION_LOCK_NONE : RDE_ORIENTATION_LOCK_PORTRAIT_ANY);
    }
#endif

    // Back is handled first, so what it closes is gone before anything else looks. Android's Back acts on its
    // RELEASE, as Android does: the gesture (and adb) send the press and the release together, so both land in one
    // frame and "just pressed" never shows.
    const b8 _back = rde_input_key_is_just_released(_app->window, RDE_KEYBOARD_KEY_AC_BACK);
    if(_back || rde_input_key_is_just_pressed(_app->window, RDE_KEYBOARD_KEY_ESCAPE)) {
        fude_app_back(_app, _back);
    }

    // A screen that is back on top, because the one over it closed, is resumed so it picks up where it was.
    const fude_screen_slot* _top = fude_app_top(_app);
    if(_top != _app->top_seen) {
        if(_top != NULL && _app->top_seen != NULL && !fude_app_slot_open(_app->top_seen) && _top->vt->resume != NULL) {
            _top->vt->resume(_top->self);
        }
        _app->top_seen = _top;
        _app->pointer  = FUDE_POINTER_NONE;   // A pointer held on the screen that went is let go.
        // A crash report's notes say which screen was on top (its name in the code, never anything shown in it).
        c8 _note[48];
        rde_snprintf(_note, sizeof(_note), "screen %s", _top != NULL ? _top->vt->name : "the page");
        rde_crash_note(_note);
    }
    if(_top == NULL) {
        return false;
    }

#if !defined(RDE_PLATFORM_MOBILE)
    // The desktop mouse is polled for its moves, as on the page, while its button is held.
    if(_app->pointer == FUDE_POINTER_MOUSE && _top->vt->pointer_moved != NULL) {
        const rde_vec_2I _m = rde_input_mouse_get_position(_app->window);
        _app->pointer_last  = (rde_vec_2F){ (f32)_m.x, (f32)_m.y };
        _top->vt->pointer_moved(_top->self, fude_draw_pointer(_app->pointer_last, false), rde_engine_get_time_now());
    }
#endif
    if(_top->vt->update != NULL) {
        _top->vt->update(_app, _top->self, _dt);
    }
    return true;
}

// Draws screen _slot, number _s in the table, in its frame from the UI. When the UI is right to left it is mirrored
// about the middle of the window's safe width (draw.h: fude_draw_mirror_begin), and its pointer is mapped back.
RDE_INTERNAL void fude_app_render_screen(fude_app* _app, const fude_screen_slot* _slot, u32 _s) {
    fude_screen_frame _frame;
    fude_ui_frame(_app->ui, _s, &_frame);
    const f32        _hw     = (f32)rde_window_get_size(_app->window).x * 0.5f;
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_app->window);   // Left, top, right, bottom.
    fude_draw_mirror_begin(fude_draw_is_rtl(), -_hw + (f32)_insets.x, _hw - (f32)_insets.z);
    _slot->vt->render(_slot->self, &_frame);
    fude_draw_mirror_end();
}

// Draws the first open screen that is not an overlay, which fills the screen. Returns false when there is none.
b8 fude_app_render(fude_app* _app) {
    for(u32 _s = 0; _s < _app->screen_count; _s++) {
        const fude_screen_slot* _slot = &_app->screens[_s];
        if(fude_app_slot_open(_slot) && !_slot->vt->overlay) {
            fude_app_render_screen(_app, _slot, _s);
            return true;
        }
    }
    return false;
}

// Draws the open overlays from the bottom of the stack up, so the one on top is drawn last.
void fude_app_render_overlays(fude_app* _app) {
    for(u32 _s = _app->screen_count; _s-- > 0;) {
        const fude_screen_slot* _slot = &_app->screens[_s];
        if(fude_app_slot_open(_slot) && _slot->vt->overlay) {
            fude_app_render_screen(_app, _slot, _s);
        }
    }
}

// --- the pointer -----------------------------------------------------------------------------

// Hands a pointer event to the screen on top. One pointer is followed at a time, pen, finger or (on a desktop) mouse,
// whichever pressed first. Presses on the UI (its row, its bar) are left to the UI. A writing screen (Practice, for
// example) takes a finger only with the hand on, and a pen coming down takes over from a writing finger.
void fude_app_screen_event(fude_app* _app, rde_event* _event) {
    const fude_screen_slot* _top = fude_app_top(_app);
    if(_top == NULL || _top->vt->pointer_down == NULL) {
        return;
    }
    const fude_screen* _vt    = _top->vt;
    any                _self  = _top->self;
    const b8           _write = _vt->input == FUDE_SCREEN_INPUT_WRITE;
    const f64          _now   = rde_engine_get_time_now();

    switch(_event->type) {
        case RDE_EVENT_TYPE_PEN_DOWN: {
            const rde_vec_2F _at = fude_app_window_to_screen(_app, _event->data.pen_event_data.position);
            fude_page_pen_came(_app->page);
            if(_write && _app->pointer == FUDE_POINTER_FINGER) {
                _vt->pointer_up(_self, fude_draw_pointer(_app->pointer_last, false), _now);   // The pen takes over.
                _app->pointer = FUDE_POINTER_NONE;
            }
            if(_app->pointer == FUDE_POINTER_NONE && !fude_ui_hit(_app->ui, _at)) {
                _app->pointer      = FUDE_POINTER_PEN;
                _app->pointer_last = _at;
                _vt->pointer_down(_self, fude_draw_pointer(_at, true), true, _now);
            }
        } break;

        case RDE_EVENT_TYPE_PEN_MOVED: {
            if(_app->pointer == FUDE_POINTER_PEN) {
                _app->pointer_last = fude_app_window_to_screen(_app, _event->data.pen_event_data.position);
                _vt->pointer_moved(_self, fude_draw_pointer(_app->pointer_last, false), _now);
            }
        } break;

        case RDE_EVENT_TYPE_PEN_UP: {
            if(_app->pointer == FUDE_POINTER_PEN) {
                _vt->pointer_up(_self, fude_draw_pointer(_app->pointer_last, false), _now);
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
                _vt->pointer_down(_self, fude_draw_pointer(_at, true), _app->finger_writes, _now);   // Hand on: writes.
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(!_touch->from_pen && _app->pointer == FUDE_POINTER_FINGER && _touch->finger_id == _app->pointer_finger) {
                _app->pointer_last = fude_app_touch_to_screen(_touch->moved_touch_position);
                _vt->pointer_moved(_self, fude_draw_pointer(_app->pointer_last, false), _now);
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_UP: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(!_touch->from_pen && _app->pointer == FUDE_POINTER_FINGER && _touch->finger_id == _app->pointer_finger) {
                _vt->pointer_up(_self, fude_draw_pointer(_app->pointer_last, false), _now);
                _app->pointer = FUDE_POINTER_NONE;
            }
        } break;

#if !defined(RDE_PLATFORM_MOBILE)
        // The mouse is read in centre-origin coordinates already, unlike pen and touch. It writes like the pen does,
        // because on a computer the mouse may be a drawing pad.
        case RDE_EVENT_TYPE_MOUSE_BUTTON_PRESSED: {
            const rde_vec_2I _m  = rde_input_mouse_get_position(_app->window);
            const rde_vec_2F _at = { (f32)_m.x, (f32)_m.y };
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT && _app->pointer == FUDE_POINTER_NONE && !_event->handled &&
               !fude_ui_hit(_app->ui, _at)) {
                _app->pointer      = FUDE_POINTER_MOUSE;
                _app->pointer_last = _at;
                _vt->pointer_down(_self, fude_draw_pointer(_at, true), true, _now);
            }
        } break;

        case RDE_EVENT_TYPE_MOUSE_BUTTON_RELEASED: {
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT && _app->pointer == FUDE_POINTER_MOUSE) {
                _vt->pointer_up(_self, fude_draw_pointer(_app->pointer_last, false), _now);
                _app->pointer = FUDE_POINTER_NONE;
            }
        } break;
#endif

        default: break;
    }
}

// Converts a pen position to screen space. Pen positions arrive WINDOW-RELATIVE: top-left origin, Y down. The app's
// screen space is centre-origin with Y up. Every pen position has to make this trip, and getting it wrong shows as ink
// mirrored about the middle of the screen rather than as anything subtle.
rde_vec_2F fude_app_window_to_screen(const fude_app* _app, rde_vec_2F _pixel) {
    const rde_vec_2I _size = rde_window_get_size(_app->window);
    return (rde_vec_2F){ _pixel.x - (f32)_size.x * 0.5f, (f32)_size.y * 0.5f - _pixel.y };
}

// Converts a touch position to screen space. Touch events report centre-origin with Y DOWN, so only Y is flipped.
rde_vec_2F fude_app_touch_to_screen(rde_vec_2I _touch) {
    return (rde_vec_2F){ (f32)_touch.x, -(f32)_touch.y };
}

// Turns the hand on or off, refreshes the toolbar's hand button and says in a notice which it is now, whether the
// user or a pen coming down switched it. Does nothing when it is already so.
void fude_app_set_finger_writes(fude_app* _app, b8 _on) {
    if(_app->finger_writes == _on) {
        return;
    }
    _app->finger_writes = _on;
    fude_toolbar_refresh(&_app->ui->bar);
#if defined(RDE_PLATFORM_ANDROID)
    const FUDE_TEXT_ _off = FUDE_TEXT_FINGER_OFF_ANDROID;   // Says "a stylus", not "the Apple Pencil".
#else
    const FUDE_TEXT_ _off = FUDE_TEXT_FINGER_OFF;
#endif
    fude_notice_show(fude_text(_on ? FUDE_TEXT_FINGER_ON : _off));   // Shown whoever switched it.
}
