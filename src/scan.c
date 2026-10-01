#include "scan.h"
#include "draw.h"
#include "text.h"
#include "theme.h"
#include "mlkit.h"
#include "icons.h"
#include "speech.h"
#include "userwords.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See scan.h.
// ===========================================================================

#define KANA_SCAN_MARGIN   24.0f
#define KANA_SCAN_HEAD     72.0f      // the title and its line, above the picture
// RDE's rotation for a turn of one degree clockwise on screen: a positive angle
// turns clockwise (its texture draw works in a y-down space; checked with
// --scan-demo-turn).
#define KANA_SCAN_TURN     (1.0f)
#define KANA_SCAN_CAMERA_W 1280u      // the camera asked for this (RDE gives the nearest it has): plenty for
#define KANA_SCAN_CAMERA_H 720u       // text close enough to read, and each frame is cheap to show and to read
// Translate with Google: lines on their way at once (in reading order, so the
// first ones come first), and the panel of translations.
#define KANA_SCAN_TRANSLATE_AT_ONCE 4u
#define KANA_SCAN_PANEL_SHARE   0.45f   // of the space under the title (at least _MIN tall)
#define KANA_SCAN_PANEL_MIN     220.0f
#define KANA_SCAN_PANEL_GAP     12.0f   // between the picture and the panel
#define KANA_SCAN_PANEL_PAD     16.0f
#define KANA_SCAN_PANEL_JP_PX   15.0f   // a line's Japanese
#define KANA_SCAN_PANEL_JP_LINE 21.0f
#define KANA_SCAN_PANEL_TR_PX   17.0f   // its translation
#define KANA_SCAN_PANEL_TR_LINE 24.0f
#define KANA_SCAN_PANEL_ROW_GAP 14.0f
#define KANA_SCAN_PANEL_SPEAK   40.0f   // a row's speaker, at its right (where there is a voice)
#define KANA_SCAN_PANEL_WORD    26.0f   // a word's row under the translation
#define KANA_SCAN_PANEL_WORD_PX 14.0f

void kana_scan_init(kana_scan* _scan) {
    memset(_scan, 0, sizeof(*_scan));
    _scan->lines = rde_arr_new(sizeof(kana_scan_line), rde_memory_allocator_get_default_std());
}

RDE_INTERNAL void kana_scan_stop_camera(kana_scan* _scan) {
    if(_scan->camera != NULL) {
        rde_device_camera_close(_scan->camera);
        _scan->camera = NULL;
        rde_engine_enable_subsystem(RDE_SUBSYSTEM_CAMERA, false);   // on only while live
    }
    if(_scan->probe != NULL) {
        rde_memory_texture_destroy(_scan->probe);
        _scan->probe = NULL;
    }
    _scan->camera_granted = false;
}

// Whatever is shown, gone (the camera with it).
RDE_INTERNAL void kana_scan_forget(kana_scan* _scan) {
    kana_scan_stop_camera(_scan);
    if(_scan->photo != NULL) {
        rde_texture_unload(_scan->photo);
        _scan->photo = NULL;
    }
    if(_scan->frame != NULL) {
        rde_memory_texture_destroy(_scan->frame);
        _scan->frame = NULL;
    }
    _scan->shown           = NULL;
    _scan->shown_rotation  = 0.0f;
    _scan->shown_top_first = false;
    _scan->picture_w       = _scan->picture_h = 0;
    rde_arr_clear(&_scan->lines);
}

void kana_scan_destroy(kana_scan* _scan) {
    kana_scan_forget(_scan);
    if(rde_arr_is_inited(&_scan->lines)) {
        rde_arr_free(&_scan->lines);
    }
    memset(_scan, 0, sizeof(*_scan));
}

void kana_scan_open(kana_scan* _scan, rde_window* _window, rde_vec_2F _canvas_at) {
    kana_scan_forget(_scan);
    _scan->open      = true;
    _scan->window    = _window;
    _scan->canvas_at = _canvas_at;
    _scan->stage     = KANA_SCAN_EMPTY;
    _scan->message   = 0;
    _scan->write     = false;
    // Translate stays as the reader left it; what was on its way is not waited for.
    _scan->translate_asked    = 0;
    _scan->translate_prepared = false;
    kana_scroller_stop(&_scan->taps);
    _scan->taps.offset = 0.0f;
}

void kana_scan_close(kana_scan* _scan) {
    _scan->open = false;
    kana_scan_forget(_scan);
    kana_scroller_stop(&_scan->taps);
}

void kana_scan_pause(kana_scan* _scan) {
    if(_scan->stage == KANA_SCAN_LIVE) {
        kana_scan_hold(_scan);   // what was in view stays, to keep or not
    }
}

// The lines of a reading, replacing what there was: all kept.
RDE_INTERNAL void kana_scan_take_lines(kana_scan* _scan, const kana_textscan_result* _result) {
    rde_arr_clear(&_scan->lines);
    for(u32 _i = 0; _i < _result->line_count; _i++) {
        kana_scan_line _line;
        memset(&_line, 0, sizeof(_line));
        memcpy(_line.text, _result->lines[_i].text, sizeof(_line.text));
        memcpy(_line.corners, _result->lines[_i].corners, sizeof(_line.corners));
        _line.block = _result->lines[_i].block;
        _line.kept  = true;
        rde_arr_add(&_scan->lines, &_line);
    }
    _scan->picture_w = _result->width;
    _scan->picture_h = _result->height;
}

// --- the camera -----------------------------------------------------------------------

// The back camera, or whichever there is: its index, or UINT32_MAX.
RDE_INTERNAL u32 kana_scan_back_camera(void) {
    const u32 _count = rde_device_camera_get_count();
    for(u32 _i = 0; _i < _count; _i++) {
        if(rde_device_camera_get_position_at(_i) == RDE_DEVICE_CAMERA_POSITION_BACK) {
            return _i;
        }
    }
    return _count > 0 ? 0u : UINT32_MAX;
}

RDE_INTERNAL void kana_scan_start_camera(kana_scan* _scan) {
    kana_scan_forget(_scan);
    // RDE's camera is off until asked for; on only while the camera is live.
    const u32 _index = rde_engine_enable_subsystem(RDE_SUBSYSTEM_CAMERA, true) ? kana_scan_back_camera() : UINT32_MAX;
    _scan->camera = _index != UINT32_MAX ? rde_device_camera_open(_index, KANA_SCAN_CAMERA_W, KANA_SCAN_CAMERA_H, rde_memory_allocator_get_default_std()) : NULL;
    if(_scan->camera == NULL) {
        rde_engine_enable_subsystem(RDE_SUBSYSTEM_CAMERA, false);
        _scan->stage   = KANA_SCAN_EMPTY;
        _scan->message = KANA_TEXT_SCAN_NO_CAMERA;
        return;
    }
    _scan->probe   = rde_memory_texture_create(1, 1, 4, rde_memory_allocator_get_default_std());
    _scan->stage   = KANA_SCAN_LIVE;
    _scan->message = 0;
}

// The camera's permission answered (RDE calls this on a later frame).
RDE_INTERNAL void kana_scan_on_camera(RDE_MOBILE_PERMISSION_ _permission, RDE_MOBILE_PERMISSION_STATUS_ _status, any _user_data) {
    RDE_UNUSED(_permission);
    kana_scan* _scan = (kana_scan*)_user_data;
    if(_status == RDE_MOBILE_PERMISSION_STATUS_GRANTED) {
        _scan->camera_granted = true;   // opened on the next update, from the frame loop
    } else {
        _scan->message = KANA_TEXT_SCAN_CAMERA_DENIED;
    }
}

void kana_scan_camera(kana_scan* _scan) {
    if(_scan->stage == KANA_SCAN_WAITING || _scan->stage == KANA_SCAN_LIVE) {
        return;
    }
    if(!kana_mlkit_enabled()) {
        _scan->message = KANA_TEXT_SCAN_MLKIT_OFF;
        return;
    }
    const RDE_MOBILE_PERMISSION_STATUS_ _status = rde_mobile_get_permission_status(RDE_MOBILE_PERMISSION_CAMERA);
    if(_status == RDE_MOBILE_PERMISSION_STATUS_DENIED_PERMANENTLY) {
        _scan->message = KANA_TEXT_SCAN_CAMERA_DENIED;
        return;
    }
    if(_status != RDE_MOBILE_PERMISSION_STATUS_GRANTED) {
        rde_mobile_request_permission(RDE_MOBILE_PERMISSION_CAMERA, kana_scan_on_camera, _scan);   // asked now, as it is used
        return;
    }
    kana_scan_start_camera(_scan);
}

void kana_scan_hold(kana_scan* _scan) {
    if(_scan->stage != KANA_SCAN_LIVE) {
        return;
    }
    kana_scan_stop_camera(_scan);   // the frame shown and its lines stay
    _scan->stage   = _scan->frame != NULL ? KANA_SCAN_RESULT : KANA_SCAN_EMPTY;
    _scan->message = 0;
}

// Live: the newest frame shown, and offered to ML Kit whenever it is free.
RDE_INTERNAL void kana_scan_update_camera(kana_scan* _scan) {
    const RDE_DEVICE_CAMERA_PERMISSION_ _permission = rde_device_camera_get_permission(_scan->camera);
    if(_permission == RDE_DEVICE_CAMERA_PERMISSION_DENIED) {
        kana_scan_forget(_scan);
        _scan->stage   = KANA_SCAN_EMPTY;
        _scan->message = KANA_TEXT_SCAN_CAMERA_DENIED;
        return;
    }
    if(_permission != RDE_DEVICE_CAMERA_PERMISSION_APPROVED) {
        return;   // still being asked
    }
    // The first frame only tells the size: a texture that size takes the rest.
    const rde_vec_2UI _size = rde_device_camera_get_size(_scan->camera);
    if(_size.x > 0 && _size.y > 0) {
        const rde_texture* _have = _scan->frame != NULL ? rde_memory_texture_get_texture(_scan->frame) : NULL;
        const rde_vec_2UI  _was  = _have != NULL ? rde_texture_get_size(_have) : (rde_vec_2UI){ 0, 0 };
        if(_was.x != _size.x || _was.y != _size.y) {
            if(_scan->frame != NULL) {
                rde_memory_texture_destroy(_scan->frame);
            }
            _scan->frame = rde_memory_texture_create(_size.x, _size.y, 4, rde_memory_allocator_get_default_std());
            _scan->shown = NULL;
        }
    }
    rde_memory_texture* _into = _scan->frame != NULL ? _scan->frame : _scan->probe;
    if(!rde_device_camera_update_texture(_scan->camera, _into) || _into != _scan->frame) {
        return;
    }
    rde_memory_texture_gpu_upload(_scan->frame, NULL);
    _scan->frames_shown++;
    _scan->shown           = rde_memory_texture_get_texture(_scan->frame);
    _scan->shown_rotation  = rde_device_camera_get_rotation(_scan->camera);
    _scan->shown_top_first = true;   // the camera's rows, as they came
    const b8 _sideways    = fmodf(fabsf(_scan->shown_rotation), 180.0f) > 45.0f;
    if(rde_arr_length(&_scan->lines) == 0 || _scan->picture_w == 0) {
        _scan->picture_w = _sideways ? _size.y : _size.x;   // until a reading says
        _scan->picture_h = _sideways ? _size.x : _size.y;
    }
    if(kana_textscan_read_frame(rde_memory_texture_get_pixels(_scan->frame), _size.x, _size.y, _scan->shown_rotation)) {   // busy: a later frame
        _scan->_read_started = rde_engine_get_time_now();
    }
}

// --- a photo from the library ---------------------------------------------------------------

void kana_scan_photos(kana_scan* _scan) {
    if(_scan->stage == KANA_SCAN_WAITING) {
        return;
    }
    if(!kana_mlkit_enabled()) {
        _scan->message = KANA_TEXT_SCAN_MLKIT_OFF;
        return;
    }
    kana_scan_hold(_scan);   // live: stopped first
    const KANA_SCAN_STAGE_ _was = _scan->stage;
    if(kana_textscan_pick(_scan->window)) {
        _scan->_before = _was;
        _scan->stage   = KANA_SCAN_WAITING;
        _scan->message = 0;
    }
}

void kana_scan_show(kana_scan* _scan, const kana_textscan_result* _result) {
    kana_scan_forget(_scan);
    _scan->stage   = KANA_SCAN_RESULT;
    _scan->message = 0;
    if(_result->image != NULL && _result->image_size > 0) {
        _scan->photo = rde_texture_load_from_memory(_result->image, _result->image_size, "scan photo", NULL, rde_memory_allocator_get_default_std());
    }
    if(_scan->photo == NULL) {
        _scan->stage   = KANA_SCAN_EMPTY;
        _scan->message = KANA_TEXT_SCAN_FAILED;
        return;
    }
    _scan->shown           = _scan->photo;
    _scan->shown_rotation  = 0.0f;
    _scan->shown_top_first = false;
    kana_scan_take_lines(_scan, _result);
}

// --- Translate with Google ------------------------------------------------------------

void kana_scan_translate(kana_scan* _scan) {
    _scan->translate          = !_scan->translate;
    _scan->translate_prepared = false;   // the next turn asks for the models again (a failed download is tried again)
    kana_scroller_stop(&_scan->taps);
    _scan->taps.offset = 0.0f;
}

b8 kana_scan_translating(const kana_scan* _scan) {
    return _scan->translate;
}

// Is the panel of translations up? On, and a held picture with lines.
RDE_INTERNAL b8 kana_scan_panel_shown(const kana_scan* _scan) {
    return _scan->translate && _scan->stage == KANA_SCAN_RESULT && rde_arr_length(&_scan->lines) > 0;
}

// Answers in, to their lines; then, when on, the models asked for (the first
// time) and the next lines sent, a few at a time.
RDE_INTERNAL void kana_scan_update_translations(kana_scan* _scan) {
    kana_scan_line* _lines = (kana_scan_line*)_scan->lines.memory;
    const u32       _count = (u32)rde_arr_length(&_scan->lines);
    u32             _ticket;
    c8              _answer[KANA_TRANSLATE_TEXT];
    while(kana_translate_poll(&_ticket, _answer, sizeof(_answer))) {
        _scan->translate_asked -= _scan->translate_asked > 0 ? 1u : 0u;
        for(u32 _i = 0; _i < _count; _i++) {
            if(_lines[_i].translated == KANA_SCAN_TRANSLATION_ASKED && _lines[_i].ticket == _ticket) {
                memcpy(_lines[_i].translation, _answer, sizeof(_lines[_i].translation));
                _lines[_i].translated = KANA_SCAN_TRANSLATION_DONE;
                break;
            }
        }   // none: the line is gone since (another picture)
    }
    if(!kana_scan_panel_shown(_scan)) {
        return;
    }
    // Another language since: everything again, into it.
    const c8* _to = kana_translate_target();
    if(strcmp(_to, _scan->translate_to) != 0) {
        snprintf(_scan->translate_to, sizeof(_scan->translate_to), "%s", _to);
        for(u32 _i = 0; _i < _count; _i++) {
            _lines[_i].translated = KANA_SCAN_TRANSLATION_NONE;
        }
        _scan->translate_prepared = false;
    }
    const KANA_TRANSLATE_STATE_ _state = kana_translate_state(_to);
    if(_state == KANA_TRANSLATE_MISSING || _state == KANA_TRANSLATE_FAILED) {
        if(!_scan->translate_prepared) {
            _scan->translate_prepared = true;
            kana_translate_prepare(_to);
        }
        return;
    }
    if(_state != KANA_TRANSLATE_READY) {
        return;
    }
    for(u32 _i = 0; _i < _count && _scan->translate_asked < KANA_SCAN_TRANSLATE_AT_ONCE; _i++) {
        if(_lines[_i].translated != KANA_SCAN_TRANSLATION_NONE) {
            continue;
        }
        const u32 _t = kana_translate_text(_lines[_i].text, _to);
        if(_t == 0u) {
            break;
        }
        _lines[_i].ticket     = _t;
        _lines[_i].translated = KANA_SCAN_TRANSLATION_ASKED;
        _scan->translate_asked++;
    }
}

void kana_scan_update(kana_scan* _scan, f32 _dt) {
    if(!_scan->open) {
        return;
    }
    // A drag scrolls the panel of translations, when it is up and long.
    if(kana_scan_panel_shown(_scan)) {
        kana_scroller_update(&_scan->taps, _dt, _scan->panel_content, _scan->panel_view);
    } else {
        kana_scroller_update(&_scan->taps, _dt, 0.0f, 1.0f);
    }
    if(_scan->camera_granted) {
        _scan->camera_granted = false;
        kana_scan_start_camera(_scan);
    }
    if(_scan->stage == KANA_SCAN_LIVE && _scan->camera != NULL) {
        kana_scan_update_camera(_scan);
    }
    // A frame read: its lines over what is live (a held frame keeps its own).
    kana_textscan_result _frame;
    if(kana_textscan_poll_frame(&_frame)) {
        if(_scan->_read_started > 0.0) {
            _scan->frames_read++;
            _scan->read_seconds   += rde_engine_get_time_now() - _scan->_read_started;
            _scan->_read_started   = 0.0;
        }
        if(_scan->stage == KANA_SCAN_LIVE) {
            kana_scan_take_lines(_scan, &_frame);
        }
    }
    // A photo picked and read.
    kana_textscan_result       _result;
    const KANA_TEXTSCAN_STATE_ _state = kana_textscan_poll(&_result);
    if(_state == KANA_TEXTSCAN_DONE) {
        kana_scan_show(_scan, &_result);
    } else if(_state == KANA_TEXTSCAN_CANCELLED) {
        _scan->stage = _scan->_before;
    } else if(_state == KANA_TEXTSCAN_FAILED) {
        _scan->stage   = _scan->_before == KANA_SCAN_RESULT ? KANA_SCAN_RESULT : KANA_SCAN_EMPTY;
        _scan->message = KANA_TEXT_SCAN_FAILED;
    }
    kana_scan_update_translations(_scan);
}

// --- the lines ----------------------------------------------------------------------

u32 kana_scan_kept(const kana_scan* _scan) {
    if(_scan->stage != KANA_SCAN_RESULT) {
        return 0;
    }
    u32 _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_scan->lines); _i++) {
        _n += ((const kana_scan_line*)_scan->lines.memory)[_i].kept ? 1u : 0u;
    }
    return _n;
}

void kana_scan_write(kana_scan* _scan) {
    if(_scan->stage == KANA_SCAN_RESULT && kana_scan_kept(_scan) > 0) {
        _scan->write = true;
    }
}

b8 kana_scan_take_text(kana_scan* _scan, c8* _out, usize _size) {
    if(!_scan->write || _size == 0) {
        return false;
    }
    _scan->write = false;
    usize _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_scan->lines); _i++) {
        const kana_scan_line* _line = &((const kana_scan_line*)_scan->lines.memory)[_i];
        if(!_line->kept) {
            continue;
        }
        const usize _len = strlen(_line->text);
        if(_n + _len + 2u > _size) {
            break;
        }
        if(_n > 0) {
            _out[_n++] = '\n';
        }
        memcpy(&_out[_n], _line->text, _len);
        _n += _len;
    }
    _out[_n] = 0;
    return _n > 0;
}

// A line's corners on screen.
RDE_INTERNAL void kana_scan_quad(const kana_scan* _scan, const kana_scan_line* _line, rde_vec_2F _out[4]) {
    for(u32 _c = 0; _c < 4u; _c++) {
        _out[_c] = (rde_vec_2F){ _scan->picture_tl.x + _line->corners[_c].x * _scan->picture_scale, _scan->picture_tl.y - _line->corners[_c].y * _scan->picture_scale };
    }
}

// Inside a convex quad, either winding.
RDE_INTERNAL b8 kana_scan_inside(const rde_vec_2F _q[4], rde_vec_2F _p) {
    i32 _sign = 0;
    for(u32 _i = 0; _i < 4u; _i++) {
        const rde_vec_2F _a = _q[_i], _b = _q[(_i + 1u) % 4u];
        const f32        _x = (_b.x - _a.x) * (_p.y - _a.y) - (_b.y - _a.y) * (_p.x - _a.x);
        const i32        _s = _x > 0.0f ? 1 : (_x < 0.0f ? -1 : 0);
        if(_s != 0) {
            if(_sign != 0 && _s != _sign) {
                return false;
            }
            _sign = _s;
        }
    }
    return true;
}

void kana_scan_pointer_down(kana_scan* _scan, rde_vec_2F _screen, f64 _time)  { kana_scroller_down(&_scan->taps, _screen, _time); }
void kana_scan_pointer_moved(kana_scan* _scan, rde_vec_2F _screen, f64 _time) { kana_scroller_moved(&_scan->taps, _screen, _time); }

void kana_scan_pointer_up(kana_scan* _scan, f64 _time) {
    kana_scroller_up(&_scan->taps, _time);
    rde_vec_2F _at;
    if(!kana_scroller_take_tap(&_scan->taps, &_at) || _scan->stage != KANA_SCAN_RESULT) {
        return;
    }
    // The line under the tap: its row in the panel of translations, or its box
    // on the picture (the smallest when they overlap).
    kana_scan_line* _lines = (kana_scan_line*)_scan->lines.memory;
    i32             _hit   = -1;
    f32             _area  = 1e30f;
    if(kana_scan_panel_shown(_scan) && _at.y <= _scan->panel_rows_top && _at.y >= _scan->panel_rows_bottom &&
       _at.x >= _scan->panel_left && _at.x <= _scan->panel_right) {
        // A word: to the learner's words, or back out of them.
        for(u32 _h = 0; _h < _scan->word_hit_count; _h++) {
            const kana_scan_word_hit* _hit = &_scan->word_hits[_h];
            kana_kanji_word           _w;
            if(_at.y <= _hit->top && _at.y > _hit->bottom && _scan->db != NULL && kana_kanji_word_at(_scan->db, _hit->word, &_w)) {
                const u32 _kanji = kana_wordsplit_kanji(_scan->db, _w.written);
                if(_kanji != 0 && kana_userwords_has(_kanji, _w.written, _w.reading)) {
                    kana_userwords_remove(_kanji, _w.written, _w.reading);
                } else if(_kanji != 0) {
                    kana_userwords_add(_kanji, _w.written, _w.reading, _w.meaning);
                }
                return;
            }
        }
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_scan->lines); _i++) {
            if(_at.y <= _lines[_i].row_top && _at.y > _lines[_i].row_bottom) {
                if(kana_speech_available() && _at.x >= _scan->panel_right - KANA_SCAN_PANEL_PAD - KANA_SCAN_PANEL_SPEAK) {
                    kana_speak(_lines[_i].text);   // its speaker: the line aloud
                } else {
                    _lines[_i].kept = !_lines[_i].kept;
                }
                break;
            }
        }
        return;
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_scan->lines); _i++) {
        rde_vec_2F _q[4];
        kana_scan_quad(_scan, &_lines[_i], _q);
        if(!kana_scan_inside(_q, _at)) {
            continue;
        }
        const f32 _a = fabsf((_q[1].x - _q[0].x) * (_q[3].y - _q[0].y) - (_q[1].y - _q[0].y) * (_q[3].x - _q[0].x));
        if(_a < _area) {
            _area = _a;
            _hit  = (i32)_i;
        }
    }
    if(_hit >= 0) {
        _lines[_hit].kept = !_lines[_hit].kept;
    }
}

// --- drawing ---------------------------------------------------------------------------

// The panel of translations between _top and _bottom: a card with Google's
// badge at its top — by the translations, as Google's terms ask — and a row a
// line under it, scrolled by the screen's drag: the line's Japanese, and its
// translation (or that it is on its way, or could not be had). A left-out
// line's row is faded, as its box is.
RDE_INTERNAL void kana_scan_draw_panel(kana_scan* _scan, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const kana_theme* _theme = kana_theme_active();
    kana_draw_card((rde_vec_2F){ _left, _bottom }, (rde_vec_2F){ _right, _top }, 16.0f, _theme->surface, _theme->outline);

    const rde_color _s       = _theme->surface;
    const f32       _badge_y = _top - KANA_SCAN_PANEL_PAD - KANA_TRANSLATE_BADGE_H * 0.5f;
    kana_translate_draw_badge(_left + KANA_SCAN_PANEL_PAD, _badge_y, 0.299f * (f32)_s.r + 0.587f * (f32)_s.g + 0.114f * (f32)_s.b < 128.0f);

    // The rows, under the badge, clipped to the card.
    const b8  _speak  = kana_speech_available();
    const f32 _x      = _left + KANA_SCAN_PANEL_PAD;
    const f32 _w      = _right - _left - 2.0f * KANA_SCAN_PANEL_PAD - (_speak ? KANA_SCAN_PANEL_SPEAK : 0.0f);
    const f32 _view_t = _badge_y - KANA_TRANSLATE_BADGE_H * 0.5f - 12.0f;
    const f32 _view_b = _bottom + 8.0f;
    _scan->panel_view        = fmaxf(_view_t - _view_b, 1.0f);
    _scan->panel_left        = _left;
    _scan->panel_right       = _right;
    _scan->panel_rows_top    = _view_t;
    _scan->panel_rows_bottom = _view_b;
    if(_view_t <= _view_b) {
        return;
    }
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)((_left + _right) * 0.5f), (i32)((_view_t + _view_b) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left), (u32)(_view_t - _view_b) });
    kana_scan_line* _lines = (kana_scan_line*)_scan->lines.memory;
    const u32       _count = (u32)rde_arr_length(&_scan->lines);
    f32             _y     = _view_t + _scan->taps.offset;   // the next row's top
    _scan->word_hit_count  = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        kana_scan_line* _line = &_lines[_i];
        const c8*       _tr   = _line->translated != KANA_SCAN_TRANSLATION_DONE ? kana_text(KANA_TEXT_SCAN_TRANSLATING)
                              : _line->translation[0] != 0                       ? _line->translation
                                                                                 : kana_text(KANA_TEXT_SCAN_TRANSLATION_NONE);
        const b8        _soft = _line->translated != KANA_SCAN_TRANSLATION_DONE || _line->translation[0] == 0;
        const u32       _jp_n = kana_draw_text_wrap_lines(_font, _font_px, _line->text, KANA_SCAN_PANEL_JP_PX, _w);
        const u32       _tr_n = kana_draw_text_wrap_lines(_font, _font_px, _tr, KANA_SCAN_PANEL_TR_PX, _w);
        if(!_line->words_found && _scan->db != NULL) {
            _line->words_found = true;
            _line->word_count  = (u8)kana_wordsplit(_scan->db, _line->text, _line->words, KANA_WORDSPLIT_MAX);
        }
        const f32       _words_h = _line->word_count > 0 ? 6.0f + (f32)_line->word_count * KANA_SCAN_PANEL_WORD : 0.0f;
        const f32       _h    = (f32)_jp_n * KANA_SCAN_PANEL_JP_LINE + 4.0f + (f32)_tr_n * KANA_SCAN_PANEL_TR_LINE + _words_h;
        _line->row_top    = _y;
        _line->row_bottom = _y - _h - KANA_SCAN_PANEL_ROW_GAP;
        if(_y - _h < _view_t && _y > _view_b) {
            const u8  _alpha = _line->kept ? 255u : 110u;
            rde_color _jp_c  = _theme->text_soft;
            rde_color _tr_c  = _soft ? _theme->text_soft : _theme->text;
            _jp_c.a = (u8)((u32)_jp_c.a * _alpha / 255u);
            _tr_c.a = (u8)((u32)_tr_c.a * _alpha / 255u);
            kana_draw_text_wrap(_font, _font_px, _line->text, _x, _y - 16.0f, KANA_SCAN_PANEL_JP_PX, _w, KANA_SCAN_PANEL_JP_LINE, _jp_c);
            kana_draw_text_wrap(_font, _font_px, _tr, _x, _y - (f32)_jp_n * KANA_SCAN_PANEL_JP_LINE - 4.0f - 18.0f, KANA_SCAN_PANEL_TR_PX, _w,
                                KANA_SCAN_PANEL_TR_LINE, _tr_c);
            if(_speak) {
                kana_draw_icon(_font, _font_px, KANA_ICON_SPEAK, (rde_vec_2F){ _right - KANA_SCAN_PANEL_PAD - KANA_SCAN_PANEL_SPEAK * 0.5f, _y - 14.0f },
                               20.0f, _theme->accent);
            }
            // Its words: + to add (✓ the learner's), the word, its reading, its meaning.
            f32 _wy = _y - (f32)_jp_n * KANA_SCAN_PANEL_JP_LINE - 4.0f - (f32)_tr_n * KANA_SCAN_PANEL_TR_LINE - 6.0f;
            for(u32 _k = 0; _k < _line->word_count; _k++) {
                kana_kanji_word _word;
                if(_scan->db == NULL || !kana_kanji_word_at(_scan->db, _line->words[_k], &_word)) {
                    continue;
                }
                const f32 _mid  = _wy - KANA_SCAN_PANEL_WORD * 0.5f;
                const u32 _kanji = kana_wordsplit_kanji(_scan->db, _word.written);
                const b8  _theirs = _kanji != 0 && kana_userwords_has(_kanji, _word.written, _word.reading);
                kana_draw_icon(_font, _font_px, _theirs ? KANA_ICON_CHECK_CIRCLE : KANA_ICON_PLUS, (rde_vec_2F){ _x + 9.0f, _mid }, 16.0f, _theme->accent);
                c8 _say[400];
                snprintf(_say, sizeof(_say), "%s  %s  ·  %s", _word.written, _word.reading, _word.meaning);
                const f32 _px = kana_draw_text_px_to_fit(_font, _font_px, _say, KANA_SCAN_PANEL_WORD_PX, _w - 28.0f, 0.55f);
                rde_color _c  = _theirs ? _theme->text : _theme->text_soft;
                _c.a          = (u8)((u32)_c.a * _alpha / 255u);
                kana_draw_text(_font, _font_px, _say, _x + 28.0f, _mid - _px * 0.36f, _px, _c);
                if(_scan->word_hit_count < KANA_SCAN_WORD_HITS && _wy - KANA_SCAN_PANEL_WORD < _view_t && _wy > _view_b) {
                    _scan->word_hits[_scan->word_hit_count++] = (kana_scan_word_hit){ _wy, _wy - KANA_SCAN_PANEL_WORD, _line->words[_k] };
                }
                _wy -= KANA_SCAN_PANEL_WORD;
            }
        }
        _y -= _h + KANA_SCAN_PANEL_ROW_GAP;
        if(_i + 1u < _count && _y + KANA_SCAN_PANEL_ROW_GAP * 0.5f < _view_t) {
            const f32 _sep = _y + KANA_SCAN_PANEL_ROW_GAP * 0.5f;
            rde_rendering_2d_draw_line((rde_vec_2F){ _x, _sep }, (rde_vec_2F){ _x + _w, _sep }, _theme->line);
        }
    }
    rde_rendering_end_clipping_rect();
    _scan->panel_content = (_view_t + _scan->taps.offset) - _y - KANA_SCAN_PANEL_ROW_GAP;
}

void kana_scan_render(kana_scan* _scan, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_scan->open) {
        return;
    }
    const kana_theme* _theme  = kana_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32         _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + KANA_SCAN_MARGIN;
    const f32         _right  = (f32)_size.x * 0.5f - (f32)_insets.z - KANA_SCAN_MARGIN;
    const f32         _width  = _right - _left;
    const u32         _found  = (u32)rde_arr_length(&_scan->lines);

    // The title, and what is happening (or what went wrong) under it.
    kana_draw_text(_font, _font_px, kana_text(KANA_TEXT_SCAN_TITLE), _left, _top - 30.0f, 24.0f, _theme->text);
    c8 _line[256] = "";
    if(_scan->message != 0) {
        snprintf(_line, sizeof(_line), "%s", kana_text((KANA_TEXT_)_scan->message));
    } else if(_scan->stage == KANA_SCAN_WAITING) {
        snprintf(_line, sizeof(_line), "%s", kana_text(KANA_TEXT_SEL_READING));
    } else if(_scan->stage == KANA_SCAN_LIVE) {
        if(_found == 0) { snprintf(_line, sizeof(_line), "%s", kana_text(KANA_TEXT_SCAN_LIVE_NONE)); }
        else            { KANA_TEXTF(_line, KANA_TEXT_SCAN_LIVE, KANA_TN(_found)); }
    } else if(_scan->stage == KANA_SCAN_RESULT) {
        if(_found == 0) { snprintf(_line, sizeof(_line), "%s", kana_text(KANA_TEXT_SCAN_NONE)); }
        else            { KANA_TEXTF(_line, KANA_TEXT_SCAN_FOUND, KANA_TN(_found)); }
    }
    // Translating: its models on their way, or not to be had, say so instead.
    const b8                    _panel = kana_scan_panel_shown(_scan);
    const KANA_TRANSLATE_STATE_ _tstate = _panel ? kana_translate_state(kana_translate_target()) : KANA_TRANSLATE_READY;
    b8                          _bad    = _scan->message != 0;
    if(_scan->message == 0 && _tstate == KANA_TRANSLATE_DOWNLOADING) {
        snprintf(_line, sizeof(_line), "%s", kana_text(KANA_TEXT_SCAN_TRANSLATE_GETTING));
    } else if(_scan->message == 0 && _tstate == KANA_TRANSLATE_FAILED) {
        snprintf(_line, sizeof(_line), "%s", kana_text(KANA_TEXT_SCAN_TRANSLATE_FAILED));
        _bad = true;
    }
    if(_line[0] != 0) {
        const rde_color _c = _bad ? _theme->score_poor : _theme->text_soft;
        kana_draw_text(_font, _font_px, _line, _left, _top - 56.0f, kana_draw_text_px_to_fit(_font, _font_px, _line, 15.0f, _width, 0.6f), _c);
    }

    // With the panel of translations up, the picture keeps the space above it.
    const f32 _area_top = _top - KANA_SCAN_HEAD;
    f32       _area_bot = _bottom;
    if(_panel) {
        const f32 _space   = _area_top - _bottom;
        const f32 _panel_h = fminf(fmaxf(_space * KANA_SCAN_PANEL_SHARE, KANA_SCAN_PANEL_MIN), _space * 0.7f);
        kana_scan_draw_panel(_scan, _window, _font, _font_px, _left, _right, _bottom + _panel_h, _bottom);
        _area_bot = _bottom + _panel_h + KANA_SCAN_PANEL_GAP;
    }
    const f32 _area_h   = _area_top - _area_bot;
    if(_scan->shown == NULL || _scan->picture_w == 0 || _scan->picture_h == 0 || _area_h <= 0.0f) {
        // Nothing yet: what the screen is for, in the middle.
        if(_scan->stage != KANA_SCAN_WAITING && _scan->stage != KANA_SCAN_LIVE) {
            const f32 _mid = (_area_top + _area_bot) * 0.5f;
            kana_draw_icon(_font, _font_px, KANA_ICON_SCAN, (rde_vec_2F){ 0.0f, _mid + 70.0f }, 64.0f, _theme->text_soft);
            const f32 _w = fminf(_width, 520.0f);
            kana_draw_text_wrap(_font, _font_px, kana_text(KANA_TEXT_SCAN_HINT), -_w * 0.5f, _mid - 10.0f, 17.0f, _w, 26.0f, _theme->text_soft);
        }
        return;
    }

    // The picture, upright and as large as fits, in the middle; the lines over it.
    const f32 _scale = fminf(_width / (f32)_scan->picture_w, _area_h / (f32)_scan->picture_h);
    const f32 _pw    = (f32)_scan->picture_w * _scale;
    const f32 _ph    = (f32)_scan->picture_h * _scale;
    _scan->picture_scale = _scale;
    _scan->picture_tl    = (rde_vec_2F){ (_left + _right) * 0.5f - _pw * 0.5f, _area_top - (_area_h - _ph) * 0.5f };
    // Rows top first (a camera frame) are mirrored back first — RDE draws a
    // texture's first row at the bottom — then the frame turned upright.
    rde_rendering_2d_draw_texture_2(_scan->shown, (rde_vec_3F){ _scan->picture_tl.x + _pw * 0.5f, _scan->picture_tl.y - _ph * 0.5f, 0.0f },
                                    (rde_vec_2F){ _scale, _scan->shown_top_first ? -_scale : _scale }, KANA_SCAN_TURN * _scan->shown_rotation,
                                    (rde_color){ 255, 255, 255, 255 });

    const kana_scan_line* _lines = (const kana_scan_line*)_scan->lines.memory;
    for(u32 _i = 0; _i < _found; _i++) {
        rde_vec_2F _q[4];
        kana_scan_quad(_scan, &_lines[_i], _q);
        if(_lines[_i].kept) {
            rde_color _fill = _theme->accent;
            _fill.a         = 60;
            rde_rendering_2d_draw_polygon_with_border(_q, 4u, _fill, 2.0f, _theme->accent, NULL);
        } else {
            rde_color _edge = _theme->text_soft;
            _edge.a         = 170;
            rde_rendering_2d_draw_polygon_border(_q, 4u, 1.0f, _edge, NULL);
        }
    }
}
