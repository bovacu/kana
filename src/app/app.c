#include "app/app.h"
#include "app/ui.h"
#include "app/page.h"
#include "base/text.h"
#include "widgets/notice.h"
#include "study/marks.h"
#include "study/review.h"

#include <string.h>

// ===========================================================================
// See app.h.
// ===========================================================================

// --- the stack ----------------------------------------------------------------------------

RDE_INTERNAL b8 kana_app_slot_open(const kana_screen_slot* _slot) {
    return _slot->vt != NULL && _slot->vt->is_open(_slot->self);
}

const kana_screen_slot* kana_app_top(const kana_app* _app) {
    for(u32 _s = 0; _s < KANA_SCREEN_COUNT; _s++) {
        if(kana_app_slot_open(&_app->screens[_s])) {
            return &_app->screens[_s];
        }
    }
    return NULL;
}

KANA_SCREEN_ kana_app_top_id(const kana_app* _app) {
    const kana_screen_slot* _top = kana_app_top(_app);
    return _top != NULL ? (KANA_SCREEN_)(_top - _app->screens) : KANA_SCREEN_COUNT;
}

b8 kana_app_is_open(const kana_app* _app, KANA_SCREEN_ _screen) {
    return kana_app_slot_open(&_app->screens[_screen]);
}

void kana_app_close_all(kana_app* _app) {
    for(u32 _s = 0; _s < KANA_SCREEN_COUNT; _s++) {
        // Each closes as many steps as it has (the album: its page, then itself).
        for(u32 _step = 0; _step < 4u && kana_app_slot_open(&_app->screens[_s]) && _app->screens[_s].vt->close != NULL; _step++) {
            _app->screens[_s].vt->close(_app->screens[_s].self);
        }
    }
    _app->pointer = KANA_POINTER_NONE;
}

// --- each frame -----------------------------------------------------------------------------

b8 kana_app_update(kana_app* _app, f32 _dt) {
    // Select mode (select.h) ends when Browse and the chart are gone.
    if(_app->selection->active && !_app->browse->open && !_app->chart->open) {
        _app->selection->active = false;
    }

    // A screen back on top — the one over it closed — picks up where it was.
    const kana_screen_slot* _top = kana_app_top(_app);
    if(_top != _app->top_seen) {
        if(_top != NULL && _app->top_seen != NULL && !kana_app_slot_open(_app->top_seen) && _top->vt->resume != NULL) {
            _top->vt->resume(_top->self);
        }
        _app->top_seen = _top;
        _app->pointer  = KANA_POINTER_NONE;   // a pointer held over the one that went is let go
    }
    if(_top == NULL) {
        return false;
    }

#if !defined(RDE_PLATFORM_MOBILE)
    // The desktop mouse is polled, as on the page.
    if(_app->pointer == KANA_POINTER_MOUSE && _top->vt->pointer_moved != NULL) {
        const rde_vec_2I _m = rde_input_mouse_get_position(_app->window);
        _app->pointer_last  = (rde_vec_2F){ (f32)_m.x, (f32)_m.y };
        _top->vt->pointer_moved(_top->self, _app->pointer_last, rde_engine_get_time_now());
    }
#endif
    if(_top->vt->update != NULL) {
        _top->vt->update(_app, _top->self, _dt);
    }
    if(rde_input_key_is_just_pressed(_app->window, RDE_KEYBOARD_KEY_ESCAPE) && _top->vt->close != NULL && kana_app_slot_open(_top)) {
        _top->vt->close(_top->self);
    }
    return true;
}

b8 kana_app_render(kana_app* _app) {
    // The first open screen that is not an overlay: it fills the screen.
    for(u32 _s = 0; _s < KANA_SCREEN_COUNT; _s++) {
        const kana_screen_slot* _slot = &_app->screens[_s];
        if(kana_app_slot_open(_slot) && !_slot->vt->overlay) {
            kana_screen_frame _frame;
            kana_ui_frame(_app->ui, (KANA_SCREEN_)_s, &_frame);
            _slot->vt->render(_slot->self, &_frame);
            return true;
        }
    }
    return false;
}

void kana_app_render_overlays(kana_app* _app) {
    for(u32 _s = KANA_SCREEN_COUNT; _s-- > 0;) {
        const kana_screen_slot* _slot = &_app->screens[_s];
        if(kana_app_slot_open(_slot) && _slot->vt->overlay) {
            kana_screen_frame _frame;
            kana_ui_frame(_app->ui, (KANA_SCREEN_)_s, &_frame);
            _slot->vt->render(_slot->self, &_frame);
        }
    }
}

// --- the pointer -----------------------------------------------------------------------------

// The screen on top's input: one pointer — pen, finger or (desktop) mouse — at a
// time, whichever pressed first. Presses on the UI (its row, its bar) are the
// UI's. A writing screen (Practice) takes a finger only with the hand on, and a
// pen takes over from a writing finger.
void kana_app_screen_event(kana_app* _app, rde_event* _event) {
    const kana_screen_slot* _top = kana_app_top(_app);
    if(_top == NULL || _top->vt->pointer_down == NULL) {
        return;
    }
    const kana_screen* _vt    = _top->vt;
    void*              _self  = _top->self;
    const b8           _write = _vt->input == KANA_SCREEN_INPUT_WRITE;
    const f64          _now   = rde_engine_get_time_now();

    switch(_event->type) {
        case RDE_EVENT_TYPE_PEN_DOWN: {
            const rde_vec_2F _at = kana_app_window_to_screen(_app, _event->data.pen_event_data.position);
            kana_page_pen_came(_app->page);
            if(_write && _app->pointer == KANA_POINTER_FINGER) {
                _vt->pointer_up(_self, _app->pointer_last, _now);   // the pen takes over from a writing finger
                _app->pointer = KANA_POINTER_NONE;
            }
            if(_app->pointer == KANA_POINTER_NONE && !kana_ui_hit(_app->ui, _at)) {
                _app->pointer      = KANA_POINTER_PEN;
                _app->pointer_last = _at;
                _vt->pointer_down(_self, _at, true, _now);
            }
        } break;

        case RDE_EVENT_TYPE_PEN_MOVED: {
            if(_app->pointer == KANA_POINTER_PEN) {
                _app->pointer_last = kana_app_window_to_screen(_app, _event->data.pen_event_data.position);
                _vt->pointer_moved(_self, _app->pointer_last, _now);
            }
        } break;

        case RDE_EVENT_TYPE_PEN_UP: {
            if(_app->pointer == KANA_POINTER_PEN) {
                _vt->pointer_up(_self, _app->pointer_last, _now);
                _app->pointer = KANA_POINTER_NONE;
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_DOWN: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            const rde_vec_2F        _at    = kana_app_touch_to_screen(_touch->init_touch_position);
            if(!_touch->from_pen && _app->pointer == KANA_POINTER_NONE && (!_write || _app->finger_writes) && !kana_ui_hit(_app->ui, _at)) {
                _app->pointer        = KANA_POINTER_FINGER;
                _app->pointer_finger = _touch->finger_id;
                _app->pointer_last   = _at;
                _vt->pointer_down(_self, _at, _app->finger_writes, _now);   // the hand on: it writes where a pen would
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_MOVED: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(!_touch->from_pen && _app->pointer == KANA_POINTER_FINGER && _touch->finger_id == _app->pointer_finger) {
                _app->pointer_last = kana_app_touch_to_screen(_touch->moved_touch_position);
                _vt->pointer_moved(_self, _app->pointer_last, _now);
            }
        } break;

        case RDE_EVENT_TYPE_MOBILE_TOUCH_UP: {
            const rde_event_mobile* _touch = &_event->data.mobile_event_data;
            if(!_touch->from_pen && _app->pointer == KANA_POINTER_FINGER && _touch->finger_id == _app->pointer_finger) {
                _vt->pointer_up(_self, _app->pointer_last, _now);
                _app->pointer = KANA_POINTER_NONE;
            }
        } break;

#if !defined(RDE_PLATFORM_MOBILE)
        // The mouse reports centre-origin already, unlike pen and touch; it writes
        // on a pad, like the pen.
        case RDE_EVENT_TYPE_MOUSE_BUTTON_PRESSED: {
            const rde_vec_2I _m  = rde_input_mouse_get_position(_app->window);
            const rde_vec_2F _at = { (f32)_m.x, (f32)_m.y };
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT && _app->pointer == KANA_POINTER_NONE && !_event->handled &&
               !kana_ui_hit(_app->ui, _at)) {
                _app->pointer      = KANA_POINTER_MOUSE;
                _app->pointer_last = _at;
                _vt->pointer_down(_self, _at, true, _now);
            }
        } break;

        case RDE_EVENT_TYPE_MOUSE_BUTTON_RELEASED: {
            if(_event->data.mouse_event_data.button == RDE_MOUSE_BUTTON_LEFT && _app->pointer == KANA_POINTER_MOUSE) {
                _vt->pointer_up(_self, _app->pointer_last, _now);
                _app->pointer = KANA_POINTER_NONE;
            }
        } break;
#endif

        default: break;
    }
}

// Pen positions arrive WINDOW-RELATIVE: top-left origin, Y down. Kana's screen
// space is centre-origin with Y up. Every pen position has to make this trip,
// and getting it wrong shows as ink mirrored about the middle of the screen
// rather than as anything subtle.
rde_vec_2F kana_app_window_to_screen(const kana_app* _app, rde_vec_2F _pixel) {
    const rde_vec_2I _size = rde_window_get_size(_app->window);
    return (rde_vec_2F){ _pixel.x - (f32)_size.x * 0.5f, (f32)_size.y * 0.5f - _pixel.y };
}

// Touch events report centre-origin with Y DOWN.
rde_vec_2F kana_app_touch_to_screen(rde_vec_2I _touch) {
    return (rde_vec_2F){ (f32)_touch.x, -(f32)_touch.y };
}

// --- the verbs ----------------------------------------------------------------------------

void kana_app_practice(kana_app* _app, const u32* _records, u32 _count) {
    if(_count == 1u) {
        kana_practice_open(_app->practice, _records[0]);
    } else if(_count > 1u) {
        kana_practice_open_set(_app->practice, _records, _count);
    }
}

void kana_app_practice_set(kana_app* _app, const u32* _records, u32 _count) {
    if(_count > 0u) {
        kana_practice_open_set(_app->practice, _records, _count);
    }
}

void kana_app_view(kana_app* _app, const u32* _records, u32 _count, u32 _at) {
    if(_count > 0u) {
        kana_viewer_show(_app->viewer, _records, _count, _at < _count ? _at : 0u);
    }
}

void kana_app_exam(kana_app* _app, const u32* _records, u32 _count) {
    if(_count > 0u) {
        kana_exam_open_with(_app->exam, _records, _count);
    }
}

void kana_app_sheet(kana_app* _app, const u32* _records, u32 _count) {
    const u32 _keep = _count < KANA_SHEET_MAX + 1u ? _count : KANA_SHEET_MAX + 1u;   // one more than fits: too many is told
    memcpy(_app->sheet, _records, sizeof(u32) * _keep);
    _app->sheet_count = _keep;
}

void kana_app_write_text(kana_app* _app, const c8* _text, rde_vec_2F _canvas) {
    kana_pagemenu_write_text(&_app->ui->page, _text, _canvas);
}

u32 kana_app_reviews_due(kana_app* _app, u32* _out, u32 _max) {
    static u32 _marked[4096];
    u32        _n = kana_marks_list(KANA_MARK_STUDYING, _marked, 4096u);
    _n += kana_marks_list(KANA_MARK_KNOWN, _marked + _n, 4096u - _n);
    u32       _due[KANA_REVIEW_SESSION];
    const u32 _d = kana_reviews_due(_marked, _n, _due, KANA_REVIEW_SESSION);
    u32       _r = 0;
    for(u32 _i = 0; _i < _d && _r < _max && _app->db != NULL; _i++) {
        if(kana_kanji_find_index(_app->db, _due[_i], &_out[_r])) {
            _r++;
        }
    }
    return _r;
}

b8 kana_app_review(kana_app* _app) {
    u32       _records[KANA_REVIEW_SESSION];
    const u32 _n = kana_app_reviews_due(_app, _records, KANA_REVIEW_SESSION);
    if(_n == 0u) {
        kana_notice_show(kana_text(KANA_TEXT_REVIEWS_NONE));
        return false;
    }
    kana_lasso_clear(_app->lasso, _app->ink);
    kana_exam_open_review(_app->exam, _records, _n);
    return true;
}

void kana_app_open(kana_app* _app, KANA_SCREEN_ _screen) {
    kana_lasso_clear(_app->lasso, _app->ink);
    switch(_screen) {
        case KANA_SCREEN_BROWSE: kana_chart_close(_app->chart);  kana_browse_open(_app->browse); break;
        case KANA_SCREEN_CHART:  kana_browse_close(_app->browse); kana_chart_open(_app->chart);  break;
        case KANA_SCREEN_EXAM:   kana_exam_open(_app->exam);      break;
        case KANA_SCREEN_VOCAB:  kana_vocabview_open(_app->vocab); break;
        case KANA_SCREEN_STATS:  kana_stats_open(_app->stats);    break;
        case KANA_SCREEN_ALBUM:  kana_album_open(_app->album);    break;
        default: break;
    }
}

void kana_app_set_finger_writes(kana_app* _app, b8 _on) {
    if(_app->finger_writes == _on) {
        return;
    }
    _app->finger_writes = _on;
    kana_toolbar_refresh(&_app->ui->bar);
    kana_notice_show(kana_text(_on ? KANA_TEXT_FINGER_ON : KANA_TEXT_FINGER_OFF));   // whoever switched it
}

// --- Select mode's row (Browse's and the chart's) ---------------------------------------
//
// What to do with the ticked characters (select.h). Its owner is whichever of the
// two is on top; All takes what that one has in view.

#include "screens/chart.h"
#include "study/vocab.h"
#include "widgets/icons.h"

RDE_INTERNAL void kana_app_select_all(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    const kana_screen_slot* _top = kana_app_top(_app);
    const u32*              _records;
    const u32               _n = _top != NULL && _top->vt->in_view != NULL ? _top->vt->in_view(_top->self, &_records) : 0u;
    if(_n > 0u) {
        kana_selection_add(_app->selection, _records, _n);
    }
}

RDE_INTERNAL void kana_app_select_none(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    kana_selection_clear(_app->selection);
}

// Study: the ticked marked Studying — or, when every one of them already is, no
// longer marked.
RDE_INTERNAL void kana_app_select_study(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    const u32 _count = kana_selection_count(_app->selection);
    if(_count == 0u || _app->db == NULL) {
        return;
    }
    static u32 _cps[16384];
    const u32  _n   = _count < 16384u ? _count : 16384u;
    b8         _all = true;
    for(u32 _i = 0; _i < _n; _i++) {
        kana_kanji_info _info;
        kana_kanji_at(_app->db, kana_selection_records(_app->selection)[_i], &_info);
        _cps[_i] = _info.codepoint;
        _all     = _all && kana_marks_get(_info.codepoint) == KANA_MARK_STUDYING;
    }
    kana_marks_set_many(_cps, _n, _all ? KANA_MARK_NONE : KANA_MARK_STUDYING);
}

// Exam (straight to its preview), Sheet, Practice: the ticked, in the order ticked.
RDE_INTERNAL void kana_app_select_exam(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    kana_app_exam(_app, kana_selection_records(_app->selection), kana_selection_count(_app->selection));
}

RDE_INTERNAL void kana_app_select_sheet(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    kana_app_sheet(_app, kana_selection_records(_app->selection), kana_selection_count(_app->selection));
}

RDE_INTERNAL void kana_app_select_practice(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    kana_app_practice_set(_app, kana_selection_records(_app->selection), kana_selection_count(_app->selection));
}

// Save list: the ticked, in the order ticked, as a new list of the vocabulary
// (vocab.h): each character a word (kana_vocab_add_characters).
RDE_INTERNAL void kana_app_select_list(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    if(kana_selection_count(_app->selection) == 0u || _app->db == NULL) {
        return;
    }
    c8        _name[KANA_VOCAB_LIST_NAME];
    u32       _saved = 0;
    const u32 _list  = kana_vocab_add_characters(_app->db, kana_selection_records(_app->selection), kana_selection_count(_app->selection), _name, sizeof(_name), &_saved);
    if(_list == 0u) {
        kana_notice_show(kana_text(KANA_TEXT_VOCAB_LISTS_FULL));
        return;
    }
    c8 _line[192];
    KANA_TEXTF(_line, KANA_TEXT_VOCAB_LIST_SAVED, KANA_TN(_saved), KANA_TS(_name));
    kana_notice_show(_line);
}

// Done: taps open characters again; the ticks stay for next time.
RDE_INTERNAL void kana_app_select_done(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    _app->selection->active = false;
}

enum { KANA_APP_SELECT_ALL = 0, KANA_APP_SELECT_NONE, KANA_APP_SELECT_STUDY, KANA_APP_SELECT_EXAM, KANA_APP_SELECT_SHEET, KANA_APP_SELECT_LIST,
       KANA_APP_SELECT_PRACTICE, KANA_APP_SELECT_DONE };
const kana_row_button KANA_APP_SELECT_BUTTONS[KANA_APP_SELECT_COUNT] = {
    { KANA_TEXT_ALL,        KANA_ICON_SELECT_ALL,  kana_app_select_all,      0, KANA_ROW_QUIET,   false, NULL },
    { KANA_TEXT_NONE,       KANA_ICON_SELECT_NONE, kana_app_select_none,     0, KANA_ROW_QUIET,   false, NULL },
    { KANA_TEXT_STUDY,      KANA_ICON_STAR,        kana_app_select_study,    0, KANA_ROW_QUIET,   false, NULL },
    { KANA_TEXT_EXAM,       KANA_ICON_EXAM,        kana_app_select_exam,     0, KANA_ROW_QUIET,   false, NULL },
    { KANA_TEXT_SHEET,      KANA_ICON_PRINT,       kana_app_select_sheet,    0, KANA_ROW_QUIET,   false, NULL },
    { KANA_TEXT_SAVE_LIST,  KANA_ICON_LISTS,       kana_app_select_list,     0, KANA_ROW_QUIET,   false, NULL },
    { KANA_TEXT_PRACTICE_N, KANA_ICON_PEN,         kana_app_select_practice, 0, KANA_ROW_PRIMARY, true,  NULL },
    { KANA_TEXT_DONE,       KANA_ICON_CHECK,       kana_app_select_done,     0, KANA_ROW_QUIET,   false, NULL },
};

// "Practice n", the ticked; everything that acts on them greyed out without any.
void kana_app_select_faces(const kana_selection* _selection, kana_row_face* _faces) {
    const u32 _n = kana_selection_count(_selection);
    kana_row_face_count(&_faces[KANA_APP_SELECT_PRACTICE], KANA_TEXT_PRACTICE_N, _n);
    _faces[KANA_APP_SELECT_PRACTICE].disabled = _n == 0u;
    _faces[KANA_APP_SELECT_NONE].disabled     = _n == 0u;
    _faces[KANA_APP_SELECT_STUDY].disabled    = _n == 0u;
    _faces[KANA_APP_SELECT_EXAM].disabled     = _n == 0u;
    _faces[KANA_APP_SELECT_SHEET].disabled    = _n == 0u;
    _faces[KANA_APP_SELECT_LIST].disabled     = _n == 0u;
}
