// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/widgets/header.h"
#include "drawing/base/redraw.h"
#include "lang/lang.h"
#include "study/screens/scan.h"
#include "study/widgets/wordcard.h"
#include "drawing/base/draw.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/ink/mlkit.h"
#include "drawing/base/icons.h"
#include "study/services/speech.h"
#include "study/models/vocab.h"

// ===========================================================================
// See scan.h.
//
// What is shown is one texture, shown: a photo's own texture (rows bottom first, as RDE's textures are, already
// upright) or the camera's latest frame (rows top first, turned by the camera's rotation). The lines' corners are in
// the upright picture's pixels, y down, and picture_tl and picture_scale (from the last frame) map them to the screen.
// The scroller taps tells taps from drags everywhere and also scrolls the panel of translations.
// ===========================================================================

// The layout's sizes, in screen points, and the text sizes, in points.
#define FUDE_SCAN_MARGIN   24.0f
#define FUDE_SCAN_HEAD     72.0f      // The title and its line, above the picture.
// RDE's rotation for a turn of one degree clockwise on screen. A positive angle turns clockwise, since RDE's texture
// drawing works in a y-down space; this was checked with --scan-demo-turn.
#define FUDE_SCAN_TURN     (1.0f)
// The frame size asked of the camera, in pixels; RDE gives the nearest it has. It is plenty for text close enough to
// read, and each frame is cheap to show and to read.
#define FUDE_SCAN_CAMERA_W 1280u      // The width asked for.
#define FUDE_SCAN_CAMERA_H 720u       // The height asked for.
// Translate with Google: how many lines are on their way at once (sent in reading order, so the first ones come back
// first), then the panel of translations.
#define FUDE_SCAN_TRANSLATE_AT_ONCE 4u
#define FUDE_SCAN_PANEL_SHARE   0.45f   // The panel's share of the space under the title (at least _MIN tall).
#define FUDE_SCAN_PANEL_MIN     220.0f
#define FUDE_SCAN_PANEL_GAP     12.0f   // The space between the picture and the panel.
#define FUDE_SCAN_PANEL_PAD     16.0f
#define FUDE_SCAN_PANEL_TAUGHT_PX   15.0f   // A line's text, in the language taught.
#define FUDE_SCAN_PANEL_TAUGHT_LINE 21.0f
#define FUDE_SCAN_PANEL_TR_PX   17.0f   // The line's translation.
#define FUDE_SCAN_PANEL_TR_LINE 24.0f
#define FUDE_SCAN_PANEL_ROW_GAP 14.0f
#define FUDE_SCAN_PANEL_SPEAK   40.0f   // A row's speaker, at its right (where there is a voice).
#define FUDE_SCAN_PANEL_WORD    26.0f   // A word's row, under the translation.
#define FUDE_SCAN_PANEL_WORD_PX 14.0f

// Sets the screen up closed and empty.
void fude_scan_init(fude_scan* _scan) {
    rde_memset(_scan, 0, sizeof(*_scan));
    fude_kit_scroller_init(&_scan->taps);
    _scan->lines = rde_arr_new(sizeof(fude_scan_line), rde_memory_allocator_get_default_std());
}

// Closes the camera and switches RDE's camera subsystem off, and drops the probe and a permission still to act on.
// The frame shown stays.
RDE_INTERNAL void fude_scan_stop_camera(fude_scan* _scan) {
    if(_scan->camera != NULL) {
        rde_device_camera_close(_scan->camera);
        _scan->camera = NULL;
        rde_engine_enable_subsystem(RDE_SUBSYSTEM_CAMERA, false);   // It is on only while the camera is live.
    }
    if(_scan->probe != NULL) {
        rde_memory_texture_destroy(_scan->probe);
        _scan->probe = NULL;
    }
    _scan->camera_granted = false;
}

// Lets go of whatever is shown, and of the camera with it, and of the lines.
RDE_INTERNAL void fude_scan_forget(fude_scan* _scan) {
    fude_scan_stop_camera(_scan);
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

// Lets go of what is shown and of the camera, frees the lines, and zeroes the screen.
void fude_scan_destroy(fude_scan* _scan) {
    fude_scan_forget(_scan);
    if(rde_arr_is_inited(&_scan->lines)) {
        rde_arr_free(&_scan->lines);
    }
    rde_memset(_scan, 0, sizeof(*_scan));
}

// Opens on the hint, with nothing shown; what it writes goes to _canvas_at.
void fude_scan_open(fude_scan* _scan, rde_window* _window, rde_vec_2F _canvas_at) {
    fude_scan_forget(_scan);
    _scan->open      = true;
    _scan->window    = _window;
    _scan->canvas_at = _canvas_at;
    _scan->stage     = FUDE_SCAN_EMPTY;
    _scan->message   = 0;
    _scan->write     = false;
    // Translate stays as the reader left it, but translations still on their way are not waited for.
    _scan->translate_asked    = 0;
    _scan->translate_prepared = false;
    rde_ui_scroller_stop(&_scan->taps);
    _scan->taps.offset.y = 0.0f;
}

// Closes the screen, letting go of the picture and the camera.
void fude_scan_close(fude_scan* _scan) {
    _scan->open = false;
    fude_scan_forget(_scan);
    rde_ui_scroller_stop(&_scan->taps);
}

// The app goes to the background: a live camera is held, so it stops.
void fude_scan_pause(fude_scan* _scan) {
    if(_scan->stage == FUDE_SCAN_LIVE) {
        fude_scan_hold(_scan);   // What was in view stays, to keep or leave out.
    }
}

// Takes the lines of a reading, replacing those there were, all of them kept, and the picture's upright size.
RDE_INTERNAL void fude_scan_take_lines(fude_scan* _scan, const fude_textscan_result* _result) {
    rde_arr_clear(&_scan->lines);
    for(u32 _i = 0; _i < _result->line_count; _i++) {
        fude_scan_line _line;
        rde_memset(&_line, 0, sizeof(_line));
        rde_memcpy(_line.text, _result->lines[_i].text, sizeof(_line.text));
        rde_memcpy(_line.corners, _result->lines[_i].corners, sizeof(_line.corners));
        _line.block = _result->lines[_i].block;
        _line.kept  = true;
        rde_arr_add(&_scan->lines, &_line);
    }
    _scan->picture_w = _result->width;
    _scan->picture_h = _result->height;
}

// --- The camera ------------------------------------------------------------------------------------------------------

// Returns the back camera's index, else the first camera's, or UINT32_MAX when there is none.
RDE_INTERNAL u32 fude_scan_back_camera(void) {
    const u32 _count = rde_device_camera_get_count();
    for(u32 _i = 0; _i < _count; _i++) {
        if(rde_device_camera_get_position_at(_i) == RDE_DEVICE_CAMERA_POSITION_BACK) {
            return _i;
        }
    }
    return _count > 0 ? 0u : UINT32_MAX;
}

// Starts the live camera, letting go of what was shown. When there is no camera, or it cannot be opened, the screen
// says so and stays empty.
RDE_INTERNAL void fude_scan_start_camera(fude_scan* _scan) {
    fude_scan_forget(_scan);
    // RDE's camera subsystem is off until asked for, and on only while the camera is live.
    const u32 _index = rde_engine_enable_subsystem(RDE_SUBSYSTEM_CAMERA, true) ? fude_scan_back_camera() : UINT32_MAX;
    _scan->camera = _index != UINT32_MAX ? rde_device_camera_open(_index, FUDE_SCAN_CAMERA_W, FUDE_SCAN_CAMERA_H, rde_memory_allocator_get_default_std()) : NULL;
    if(_scan->camera == NULL) {
        rde_engine_enable_subsystem(RDE_SUBSYSTEM_CAMERA, false);
        _scan->stage   = FUDE_SCAN_EMPTY;
        _scan->message = FUDE_TEXT_SCAN_NO_CAMERA;
        return;
    }
    _scan->probe   = rde_memory_texture_create(1, 1, 4, rde_memory_allocator_get_default_std());
    _scan->stage   = FUDE_SCAN_LIVE;
    _scan->message = 0;
}

// The answer to the camera's permission (RDE calls this on a later frame). When granted, the camera opens on the next
// update, from the frame loop; when refused, the screen says where to allow it.
RDE_INTERNAL void fude_scan_on_camera(RDE_MOBILE_PERMISSION_ _permission, RDE_MOBILE_PERMISSION_STATUS_ _status, any _user_data) {
    RDE_UNUSED(_permission);
    fude_scan* _scan = (fude_scan*)_user_data;
    if(_status == RDE_MOBILE_PERMISSION_STATUS_GRANTED) {
        _scan->camera_granted = true;   // It opens on the next update, from the frame loop.
    } else {
        _scan->message = FUDE_TEXT_SCAN_CAMERA_DENIED;
    }
    fude_redraw_request();   // (render on demand: the answer comes with no event)
}

// See the header: starts the live camera, asking for its permission first when needed.
void fude_scan_camera(fude_scan* _scan) {
    if(_scan->stage == FUDE_SCAN_WAITING || _scan->stage == FUDE_SCAN_LIVE) {
        return;
    }
    if(!fude_mlkit_enabled()) {
        _scan->message = FUDE_TEXT_SCAN_MLKIT_OFF;
        return;
    }
    const RDE_MOBILE_PERMISSION_STATUS_ _status = rde_mobile_get_permission_status(RDE_MOBILE_PERMISSION_CAMERA);
    if(_status == RDE_MOBILE_PERMISSION_STATUS_DENIED_PERMANENTLY) {
        _scan->message = FUDE_TEXT_SCAN_CAMERA_DENIED;
        return;
    }
    if(_status != RDE_MOBILE_PERMISSION_STATUS_GRANTED) {
        rde_mobile_request_permission(RDE_MOBILE_PERMISSION_CAMERA, fude_scan_on_camera, _scan);   // Asked when used.
        return;
    }
    fude_scan_start_camera(_scan);
}

// Stops the live camera, keeping the frame shown and its lines as the result (or the hint when no frame came yet).
void fude_scan_hold(fude_scan* _scan) {
    if(_scan->stage != FUDE_SCAN_LIVE) {
        return;
    }
    fude_scan_stop_camera(_scan);   // The frame shown and its lines stay.
    _scan->stage   = _scan->frame != NULL ? FUDE_SCAN_RESULT : FUDE_SCAN_EMPTY;
    _scan->message = 0;
}

// While live: shows the camera's newest frame, and offers it to ML Kit whenever ML Kit is free. A refused permission
// ends the camera.
RDE_INTERNAL void fude_scan_update_camera(fude_scan* _scan) {
    const RDE_DEVICE_CAMERA_PERMISSION_ _permission = rde_device_camera_get_permission(_scan->camera);
    if(_permission == RDE_DEVICE_CAMERA_PERMISSION_DENIED) {
        fude_scan_forget(_scan);
        _scan->stage   = FUDE_SCAN_EMPTY;
        _scan->message = FUDE_TEXT_SCAN_CAMERA_DENIED;
        return;
    }
    if(_permission != RDE_DEVICE_CAMERA_PERMISSION_APPROVED) {
        return;   // The permission is still being asked for.
    }
    // The first frame, into the 1x1 probe, only tells the size; a texture of that size takes the frames after it.
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
    _scan->shown_top_first = true;   // The camera's rows, as they came.
    const b8 _sideways    = rde_math_fmod_f32(rde_math_abs_f32(_scan->shown_rotation), 180.0f) > 45.0f;
    if(rde_arr_length(&_scan->lines) == 0 || _scan->picture_w == 0) {
        _scan->picture_w = _sideways ? _size.y : _size.x;   // Until a reading gives the size.
        _scan->picture_h = _sideways ? _size.x : _size.y;
    }
    if(fude_textscan_read_frame(rde_memory_texture_get_pixels(_scan->frame), _size.x, _size.y, _scan->shown_rotation)) {   // Busy: a later frame.
        _scan->_read_started = rde_engine_get_time_now();
    }
}

// --- A photo from the library ----------------------------------------------------------------------------------------

// Opens the system's photo picker, holding a live camera first. Cancelling the pick goes back to the stage before.
void fude_scan_photos(fude_scan* _scan) {
    if(_scan->stage == FUDE_SCAN_WAITING) {
        return;
    }
    if(!fude_mlkit_enabled()) {
        _scan->message = FUDE_TEXT_SCAN_MLKIT_OFF;
        return;
    }
    fude_scan_hold(_scan);   // A live camera is stopped first.
    const FUDE_SCAN_STAGE_ _was = _scan->stage;
    if(fude_textscan_pick(_scan->window)) {
        _scan->_before = _was;
        _scan->stage   = FUDE_SCAN_WAITING;
        _scan->message = 0;
    }
}

// Shows a photo read: its image as a texture of the screen's own, and its lines. An image that cannot be loaded shows
// that reading failed instead.
void fude_scan_show(fude_scan* _scan, const fude_textscan_result* _result) {
    fude_scan_forget(_scan);
    _scan->stage   = FUDE_SCAN_RESULT;
    _scan->message = 0;
    if(_result->image != NULL && _result->image_size > 0) {
        _scan->photo = rde_texture_load_from_memory(_result->image, _result->image_size, "scan photo", NULL, rde_memory_allocator_get_default_std());
    }
    if(_scan->photo == NULL) {
        _scan->stage   = FUDE_SCAN_EMPTY;
        _scan->message = FUDE_TEXT_SCAN_FAILED;
        return;
    }
    _scan->shown           = _scan->photo;
    _scan->shown_rotation  = 0.0f;
    _scan->shown_top_first = false;
    fude_scan_take_lines(_scan, _result);
}

// --- Translate with Google -------------------------------------------------------------------------------------------

// Turns Translate with Google on or off, with the panel scrolled to its top.
void fude_scan_translate(fude_scan* _scan) {
    _scan->translate          = !_scan->translate;
    // The next turn asks for the models again, so a failed download is tried again.
    _scan->translate_prepared = false;   // Ask for the models again.
    rde_ui_scroller_stop(&_scan->taps);
    _scan->taps.offset.y = 0.0f;
}

// Returns whether Translate with Google is on.
b8 fude_scan_translating(const fude_scan* _scan) {
    return _scan->translate;
}

// Returns whether the panel of translations is up: Translate is on, and a picture that holds still has lines.
RDE_INTERNAL b8 fude_scan_panel_shown(const fude_scan* _scan) {
    return _scan->translate && _scan->stage == FUDE_SCAN_RESULT && rde_arr_length(&_scan->lines) > 0;
}

// Takes the translations that came back, into their lines. Then, while the panel is up, it asks for the models the
// first time (and again after a failed download), and sends the next lines, at most FUDE_SCAN_TRANSLATE_AT_ONCE on
// their way at once.
RDE_INTERNAL void fude_scan_update_translations(fude_scan* _scan) {
    fude_scan_line* _lines = (fude_scan_line*)_scan->lines.memory;
    const u32       _count = (u32)rde_arr_length(&_scan->lines);
    for(u32 _i = 0; _i < _count; _i++) {
        if(_lines[_i].translated == FUDE_SCAN_TRANSLATION_ASKED && fude_translate_take(_lines[_i].ticket, _lines[_i].translation, sizeof(_lines[_i].translation))) {
            _lines[_i].translated  = FUDE_SCAN_TRANSLATION_DONE;
            _scan->translate_asked -= _scan->translate_asked > 0 ? 1u : 0u;
        }
    }
    if(!fude_scan_panel_shown(_scan)) {
        return;
    }
    // The reader's language changed since: everything is translated again, into it.
    const c8* _to = fude_translate_target();
    if(rde_strcmp(_to, _scan->translate_to) != 0) {
        rde_snprintf(_scan->translate_to, sizeof(_scan->translate_to), "%s", _to);
        for(u32 _i = 0; _i < _count; _i++) {
            _lines[_i].translated = FUDE_SCAN_TRANSLATION_NONE;
        }
        _scan->translate_asked    = 0;   // What was on its way is not waited for.
        _scan->translate_prepared = false;
    }
    const FUDE_TRANSLATE_STATE_ _state = fude_translate_state(fude_lang_code(), _to);
    if(_state == FUDE_TRANSLATE_MISSING || _state == FUDE_TRANSLATE_FAILED) {
        if(!_scan->translate_prepared) {
            _scan->translate_prepared = true;
            fude_translate_prepare(fude_lang_code(), _to);
        }
        return;
    }
    if(_state != FUDE_TRANSLATE_READY) {
        return;
    }
    for(u32 _i = 0; _i < _count && _scan->translate_asked < FUDE_SCAN_TRANSLATE_AT_ONCE; _i++) {
        if(_lines[_i].translated != FUDE_SCAN_TRANSLATION_NONE) {
            continue;
        }
        const u32 _t = fude_translate_text(_lines[_i].text, fude_lang_code(), _to);
        if(_t == 0u) {
            break;
        }
        _lines[_i].ticket     = _t;
        _lines[_i].translated = FUDE_SCAN_TRANSLATION_ASKED;
        _scan->translate_asked++;
    }
}

// Scrolls the panel, opens the camera once permitted, shows and reads the camera's frames, takes a photo's reading,
// and moves the translations on.
void fude_scan_update(fude_scan* _scan, f32 _dt) {
    if(!_scan->open) {
        return;
    }
    // A drag scrolls the panel of translations, when it is up and long.
    if(fude_scan_panel_shown(_scan)) {
        fude_kit_scroller_update(&_scan->taps, _dt, _scan->panel_content, _scan->panel_view);
    } else {
        fude_kit_scroller_update(&_scan->taps, _dt, 0.0f, 1.0f);
    }
    if(_scan->camera_granted) {
        _scan->camera_granted = false;
        fude_scan_start_camera(_scan);
    }
    if(_scan->stage == FUDE_SCAN_LIVE && _scan->camera != NULL) {
        fude_redraw_changed();   // (render on demand: the camera's frames come with no event, about 30 a second)
        fude_scan_update_camera(_scan);
    } else if(_scan->stage == FUDE_SCAN_WAITING) {
        fude_redraw_after(FUDE_REDRAW_POLL);   // (render on demand: a photo's reading is polled, with no event)
    }
    // A frame was read: its lines go over what is live (a held frame keeps its own). The time the reading took is
    // counted, for measuring.
    fude_textscan_result _frame;
    if(fude_textscan_poll_frame(&_frame)) {
        if(_scan->_read_started > 0.0) {
            _scan->frames_read++;
            _scan->read_seconds   += rde_engine_get_time_now() - _scan->_read_started;
            _scan->_read_started   = 0.0;
        }
        if(_scan->stage == FUDE_SCAN_LIVE) {
            fude_scan_take_lines(_scan, &_frame);
        }
    }
    // A photo was picked and read, the pick was cancelled, or the reading failed.
    fude_textscan_result       _result;
    const FUDE_TEXTSCAN_STATE_ _state = fude_textscan_poll(&_result);
    if(_state == FUDE_TEXTSCAN_DONE) {
        fude_scan_show(_scan, &_result);
    } else if(_state == FUDE_TEXTSCAN_CANCELLED) {
        _scan->stage = _scan->_before;
    } else if(_state == FUDE_TEXTSCAN_FAILED) {
        _scan->stage   = _scan->_before == FUDE_SCAN_RESULT ? FUDE_SCAN_RESULT : FUDE_SCAN_EMPTY;
        _scan->message = FUDE_TEXT_SCAN_FAILED;
    }
    fude_scan_update_translations(_scan);
}

// --- The lines -------------------------------------------------------------------------------------------------------

// Returns how many lines are kept, 0 unless a result is shown.
u32 fude_scan_kept(const fude_scan* _scan) {
    if(_scan->stage != FUDE_SCAN_RESULT) {
        return 0;
    }
    u32 _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_scan->lines); _i++) {
        _n += ((const fude_scan_line*)_scan->lines.memory)[_i].kept ? 1u : 0u;
    }
    return _n;
}

// Write: remembers the press for fude_scan_take_text, when there is a result with lines kept.
void fude_scan_write(fude_scan* _scan) {
    if(_scan->stage == FUDE_SCAN_RESULT && fude_scan_kept(_scan) > 0) {
        _scan->write = true;
    }
}

// See the header: the bytes every kept line needs written out.
usize fude_scan_text_size(const fude_scan* _scan) {
    usize _n = 1u;   // One to spare: fude_scan_take_text wants room for a break and the final 0 after each line.
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_scan->lines); _i++) {
        const fude_scan_line* _line = &((const fude_scan_line*)_scan->lines.memory)[_i];
        _n += _line->kept ? rde_strlen(_line->text) + 1u : 0u;
    }
    return _n;
}

// See the header: the kept lines, one a line, in the order read. It stops at the first line that does not fit (with
// room for a line break and the final 0), so the lines after it are left out too.
b8 fude_scan_take_text(fude_scan* _scan, c8* _out, usize _size) {
    if(!_scan->write || _size == 0) {
        return false;
    }
    _scan->write = false;
    usize _n = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_scan->lines); _i++) {
        const fude_scan_line* _line = &((const fude_scan_line*)_scan->lines.memory)[_i];
        if(!_line->kept) {
            continue;
        }
        const usize _len = rde_strlen(_line->text);
        if(_n + _len + 2u > _size) {
            break;
        }
        if(_n > 0) {
            _out[_n++] = '\n';
        }
        rde_memcpy(&_out[_n], _line->text, _len);
        _n += _len;
    }
    _out[_n] = 0;
    return _n > 0;
}

// Puts a line's four corners into _out, in screen space.
RDE_INTERNAL void fude_scan_quad(const fude_scan* _scan, const fude_scan_line* _line, rde_vec_2F _out[4]) {
    for(u32 _c = 0; _c < 4u; _c++) {
        _out[_c] = (rde_vec_2F){ _scan->picture_tl.x + _line->corners[_c].x * _scan->picture_scale, _scan->picture_tl.y - _line->corners[_c].y * _scan->picture_scale };
    }
}

// Returns whether point _p is inside the convex quad _q, whichever way its corners wind: it is inside when it is on
// the same side of every edge.
RDE_INTERNAL b8 fude_scan_inside(const rde_vec_2F _q[4], rde_vec_2F _p) {
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

// The pointer goes down and moves: the scroller tells a tap from a drag, and a drag scrolls the panel.
void fude_scan_pointer_down(fude_scan* _scan, rde_vec_2F _screen, f64 _time)  { rde_ui_scroller_pointer_down(&_scan->taps, _screen, _time); }
void fude_scan_pointer_moved(fude_scan* _scan, rde_vec_2F _screen, f64 _time) { rde_ui_scroller_pointer_moved(&_scan->taps, _screen, _time); }

// On a tap on a result: a word in the panel opens the word card, a row's speaker says its line, a row leaves its line
// out or takes it back, and so does a line's box on the picture.
void fude_scan_pointer_up(fude_scan* _scan, f64 _time) {
    rde_ui_scroller_pointer_up(&_scan->taps, _time);
    rde_vec_2F _at;
    if(!rde_ui_scroller_take_tap(&_scan->taps, &_at) || _scan->stage != FUDE_SCAN_RESULT) {
        return;
    }
    // The line under the tap: its row in the panel of translations, or its box on the picture (the smallest box when
    // boxes overlap).
    fude_scan_line* _lines = (fude_scan_line*)_scan->lines.memory;
    i32             _hit   = -1;
    f32             _area  = 1e30f;
    if(fude_scan_panel_shown(_scan) && _at.y <= _scan->panel_rows_top && _at.y >= _scan->panel_rows_bottom &&
       _at.x >= _scan->panel_left && _at.x <= _scan->panel_right) {
        // A word opens the word card (wordcard.h), to save it or see it saved.
        for(u32 _h = 0; _h < _scan->word_hit_count; _h++) {
            const fude_scan_word_hit* _word = &_scan->word_hits[_h];
            fude_kanji_word           _w;
            if(_at.y <= _word->top && _at.y > _word->bottom && _scan->db != NULL && fude_kanji_word_at(_scan->db, _word->word, &_w)) {
                const fude_scan_line* _in = _word->line < (u32)rde_arr_length(&_scan->lines) ? &_lines[_word->line] : NULL;
                fude_wordcard_ask_in(_w.written, _w.reading, _w.meaning, _in != NULL ? _in->text : "",
                                     _in != NULL && _in->translated == FUDE_SCAN_TRANSLATION_DONE ? _in->translation : "");
                return;
            }
        }
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_scan->lines); _i++) {
            if(_at.y <= _lines[_i].row_top && _at.y > _lines[_i].row_bottom) {
                if(fude_speech_available() && _at.x >= _scan->panel_right - FUDE_SCAN_PANEL_PAD - FUDE_SCAN_PANEL_SPEAK) {
                    fude_speak(_lines[_i].text);   // Its speaker says the line aloud.
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
        fude_scan_quad(_scan, &_lines[_i], _q);
        if(!fude_scan_inside(_q, _at)) {
            continue;
        }
        const f32 _a = rde_math_abs_f32((_q[1].x - _q[0].x) * (_q[3].y - _q[0].y) - (_q[1].y - _q[0].y) * (_q[3].x - _q[0].x));
        if(_a < _area) {
            _area = _a;
            _hit  = (i32)_i;
        }
    }
    if(_hit >= 0) {
        _lines[_hit].kept = !_lines[_hit].kept;
    }
}

// --- Drawing ---------------------------------------------------------------------------------------------------------

// Draws the panel of translations between _top and _bottom: a card with Google's badge at its top (next to the
// translations, as Google's terms ask), and under it a row for each line, scrolled by the screen's drag. A row has the
// line's text in the language taught, its translation (or that it is on its way, or could not be had), and the line's
// words. A left-out line's row is faded, as its box is. It records the rows and the words for taps, finds each line's
// words the first time its row is laid out, and works out the panel's content height.
RDE_INTERNAL void fude_scan_draw_panel(fude_scan* _scan, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme* _theme = fude_theme_active();
    fude_draw_card((rde_vec_2F){ _left, _bottom }, (rde_vec_2F){ _right, _top }, 16.0f, _theme->surface, _theme->outline);

    const rde_color _s       = _theme->surface;
    const f32       _badge_y = _top - FUDE_SCAN_PANEL_PAD - FUDE_TRANSLATE_BADGE_H * 0.5f;
    // The badge's dark version is used when the card is dark: its luma (Rec. 601 weights) under half.
    fude_translate_draw_badge(_left + FUDE_SCAN_PANEL_PAD, _badge_y, 0.299f * (f32)_s.r + 0.587f * (f32)_s.g + 0.114f * (f32)_s.b < 128.0f);

    // The rows, under the badge, clipped to the card.
    const b8  _speak  = fude_speech_available();
    const f32 _x      = _left + FUDE_SCAN_PANEL_PAD;
    const f32 _w      = _right - _left - 2.0f * FUDE_SCAN_PANEL_PAD - (_speak ? FUDE_SCAN_PANEL_SPEAK : 0.0f);
    const f32 _view_t = _badge_y - FUDE_TRANSLATE_BADGE_H * 0.5f - 12.0f;
    const f32 _view_b = _bottom + 8.0f;
    _scan->panel_view        = rde_math_max_f32(_view_t - _view_b, 1.0f);
    _scan->panel_left        = _left;
    _scan->panel_right       = _right;
    _scan->panel_rows_top    = _view_t;
    _scan->panel_rows_bottom = _view_b;
    if(_view_t <= _view_b) {
        return;
    }
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)fude_draw_x((_left + _right) * 0.5f), (i32)((_view_t + _view_b) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left), (u32)(_view_t - _view_b) });
    fude_scan_line* _lines = (fude_scan_line*)_scan->lines.memory;
    const u32       _count = (u32)rde_arr_length(&_scan->lines);
    f32             _y     = _view_t + _scan->taps.offset.y;   // The next row's top.
    _scan->word_hit_count  = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        fude_scan_line* _line = &_lines[_i];
        const c8*       _tr   = _line->translated != FUDE_SCAN_TRANSLATION_DONE ? fude_text(FUDE_TEXT_SCAN_TRANSLATING)
                              : _line->translation[0] != 0                       ? _line->translation
                                                                                 : fude_text(FUDE_TEXT_SCAN_TRANSLATION_NONE);
        const b8        _soft = _line->translated != FUDE_SCAN_TRANSLATION_DONE || _line->translation[0] == 0;
        const u32       _taught_n = fude_draw_text_wrap_lines(_font, _font_px, _line->text, FUDE_SCAN_PANEL_TAUGHT_PX, _w);
        const u32       _tr_n     = fude_draw_text_wrap_lines(_font, _font_px, _tr, FUDE_SCAN_PANEL_TR_PX, _w);
        if(!_line->words_found && _scan->db != NULL) {
            _line->words_found = true;
            _line->word_count  = (u8)fude_wordsplit(_scan->db, _line->text, _line->words, FUDE_WORDSPLIT_MAX);
        }
        const f32       _words_h = _line->word_count > 0 ? 6.0f + (f32)_line->word_count * FUDE_SCAN_PANEL_WORD : 0.0f;
        const f32       _h    = (f32)_taught_n * FUDE_SCAN_PANEL_TAUGHT_LINE + 4.0f + (f32)_tr_n * FUDE_SCAN_PANEL_TR_LINE + _words_h;
        _line->row_top    = _y;
        _line->row_bottom = _y - _h - FUDE_SCAN_PANEL_ROW_GAP;
        if(_y - _h < _view_t && _y > _view_b) {
            const u8  _alpha = _line->kept ? 255u : 110u;
            rde_color _taught_c = _theme->text_soft;
            rde_color _tr_c     = _soft ? _theme->text_soft : _theme->text;
            _taught_c.a = (u8)((u32)_taught_c.a * _alpha / 255u);
            _tr_c.a     = (u8)((u32)_tr_c.a * _alpha / 255u);
            fude_draw_text_wrap(_font, _font_px, _line->text, _x, _y - 16.0f, FUDE_SCAN_PANEL_TAUGHT_PX, _w, FUDE_SCAN_PANEL_TAUGHT_LINE, _taught_c);
            fude_draw_text_wrap(_font, _font_px, _tr, _x, _y - (f32)_taught_n * FUDE_SCAN_PANEL_TAUGHT_LINE - 4.0f - 18.0f, FUDE_SCAN_PANEL_TR_PX, _w,
                                FUDE_SCAN_PANEL_TR_LINE, _tr_c);
            if(_speak) {
                fude_draw_icon(_font, _font_px, FUDE_ICON_SPEAK, (rde_vec_2F){ _right - FUDE_SCAN_PANEL_PAD - FUDE_SCAN_PANEL_SPEAK * 0.5f, _y - 14.0f },
                               20.0f, _theme->accent);
            }
            // Its words: a bookmark (filled when the word is saved), the word, its reading and its meaning. Only the
            // first FUDE_SCAN_WORD_HITS words in view can be tapped.
            f32 _wy = _y - (f32)_taught_n * FUDE_SCAN_PANEL_TAUGHT_LINE - 4.0f - (f32)_tr_n * FUDE_SCAN_PANEL_TR_LINE - 6.0f;
            for(u32 _k = 0; _k < _line->word_count; _k++) {
                fude_kanji_word _word;
                if(_scan->db == NULL || !fude_kanji_word_at(_scan->db, _line->words[_k], &_word)) {
                    continue;
                }
                const f32 _mid  = _wy - FUDE_SCAN_PANEL_WORD * 0.5f;
                const b8  _theirs = fude_vocab_find(_word.written, _word.reading) != 0u;
                if(_theirs) {
                    fude_draw_icon_fill(FUDE_ICON_BOOKMARK, (rde_vec_2F){ _x + 9.0f, _mid }, 16.0f, _theme->accent);
                } else {
                    fude_draw_icon(_font, _font_px, FUDE_ICON_BOOKMARK, (rde_vec_2F){ _x + 9.0f, _mid }, 16.0f, _theme->accent);
                }
                c8 _say[400];
                rde_snprintf(_say, sizeof(_say), "%s  %s  ·  %s", _word.written, _word.reading, _word.meaning);
                const f32 _px = fude_draw_text_px_to_fit(_font, _font_px, _say, FUDE_SCAN_PANEL_WORD_PX, _w - 28.0f);
                rde_color _c  = _theirs ? _theme->text : _theme->text_soft;
                _c.a          = (u8)((u32)_c.a * _alpha / 255u);
                fude_draw_text(_font, _font_px, _say, _x + 28.0f, _mid - _px * 0.36f, _px, _c);
                if(_scan->word_hit_count < FUDE_SCAN_WORD_HITS && _wy - FUDE_SCAN_PANEL_WORD < _view_t && _wy > _view_b) {
                    _scan->word_hits[_scan->word_hit_count++] = (fude_scan_word_hit){ _wy, _wy - FUDE_SCAN_PANEL_WORD, _line->words[_k], _i };
                }
                _wy -= FUDE_SCAN_PANEL_WORD;
            }
        }
        _y -= _h + FUDE_SCAN_PANEL_ROW_GAP;
        if(_i + 1u < _count && _y + FUDE_SCAN_PANEL_ROW_GAP * 0.5f < _view_t) {
            const f32 _sep = _y + FUDE_SCAN_PANEL_ROW_GAP * 0.5f;
            rde_rendering_2d_draw_line((rde_vec_2F){ fude_draw_x(_x), _sep }, (rde_vec_2F){ fude_draw_x(_x + _w), _sep }, _theme->line);
        }
    }
    rde_rendering_end_clipping_rect();
    _scan->panel_content = (_view_t + _scan->taps.offset.y) - _y - FUDE_SCAN_PANEL_ROW_GAP;
}

// Draws the title and the status line, the panel of translations when it is up, and the picture with every line's box
// over it (filled when kept, faint when left out), or the hint when there is no picture yet. It records where the
// picture is for taps.
void fude_scan_render(fude_scan* _scan, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_scan->open) {
        return;
    }
    const fude_theme* _theme  = fude_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _insets = rde_window_get_safe_area_insets(_window);   // The order is left, top, right, bottom.
    const f32         _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + FUDE_SCAN_MARGIN;
    const f32         _right  = (f32)_size.x * 0.5f - (f32)_insets.z - FUDE_SCAN_MARGIN;
    const f32         _width  = _right - _left;
    const u32         _found  = (u32)rde_arr_length(&_scan->lines);

    // The title, and what is happening (or what went wrong) under it.
    fude_header_title(_font, _font_px, fude_text(FUDE_TEXT_SCAN_TITLE), _left, _top);
    c8 _line[512] = "";
    if(_scan->message != 0) {
        rde_snprintf(_line, sizeof(_line), "%s", fude_text((FUDE_TEXT_)_scan->message));
    } else if(_scan->stage == FUDE_SCAN_WAITING) {
        rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_SEL_READING));
    } else if(_scan->stage == FUDE_SCAN_LIVE) {
        if(_found == 0) { rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_SCAN_LIVE_NONE)); }
        else            { FUDE_TEXTF(_line, FUDE_TEXT_SCAN_LIVE, FUDE_TN(_found)); }
    } else if(_scan->stage == FUDE_SCAN_RESULT) {
        if(_found == 0) { rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_SCAN_NONE)); }
        else            { FUDE_TEXTF(_line, FUDE_TEXT_SCAN_FOUND, FUDE_TN(_found)); }
    }
    // While translating, when the models are on their way, or cannot be had, the status line says so instead.
    const b8                    _panel = fude_scan_panel_shown(_scan);
    const FUDE_TRANSLATE_STATE_ _tstate = _panel ? fude_translate_state(fude_lang_code(), fude_translate_target()) : FUDE_TRANSLATE_READY;
    b8                          _bad    = _scan->message != 0;
    if(_scan->message == 0 && _tstate == FUDE_TRANSLATE_DOWNLOADING) {
        rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_SCAN_TRANSLATE_GETTING));
    } else if(_scan->message == 0 && _tstate == FUDE_TRANSLATE_FAILED) {
        rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_SCAN_TRANSLATE_FAILED));
        _bad = true;
    }
    if(_line[0] != 0) {
        const rde_color _c = _bad ? _theme->score_poor : _theme->text_soft;
        fude_draw_text(_font, _font_px, _line, _left, _top - 56.0f, fude_draw_text_px_to_fit(_font, _font_px, _line, 15.0f, _width), _c);
    }

    // With the panel of translations up (at most 70% of the space), the picture keeps the space above it.
    const f32 _area_top = _top - FUDE_SCAN_HEAD;
    f32       _area_bot = _bottom;
    if(_panel) {
        const f32 _space   = _area_top - _bottom;
        const f32 _panel_h = rde_math_min_f32(rde_math_max_f32(_space * FUDE_SCAN_PANEL_SHARE, FUDE_SCAN_PANEL_MIN), _space * 0.7f);
        fude_scan_draw_panel(_scan, _window, _font, _font_px, _left, _right, _bottom + _panel_h, _bottom);
        _area_bot = _bottom + _panel_h + FUDE_SCAN_PANEL_GAP;
    }
    const f32 _area_h   = _area_top - _area_bot;
    if(_scan->shown == NULL || _scan->picture_w == 0 || _scan->picture_h == 0 || _area_h <= 0.0f) {
        // Nothing to show yet: what the screen is for, in the middle.
        if(_scan->stage != FUDE_SCAN_WAITING && _scan->stage != FUDE_SCAN_LIVE) {
            const f32 _mid = (_area_top + _area_bot) * 0.5f;
            fude_draw_icon(_font, _font_px, FUDE_ICON_SCAN, (rde_vec_2F){ 0.0f, _mid + 70.0f }, 64.0f, _theme->text_soft);
            const f32 _w = rde_math_min_f32(_width, 520.0f);
            fude_draw_text_wrap(_font, _font_px, fude_text(FUDE_TEXT_SCAN_HINT), -_w * 0.5f, _mid - 10.0f, 17.0f, _w, 26.0f, _theme->text_soft);
        }
        return;
    }

    // The picture, upright and as large as fits, in the middle; the lines over it.
    const f32 _scale = rde_math_min_f32(_width / (f32)_scan->picture_w, _area_h / (f32)_scan->picture_h);
    const f32 _pw    = (f32)_scan->picture_w * _scale;
    const f32 _ph    = (f32)_scan->picture_h * _scale;
    _scan->picture_scale = _scale;
    _scan->picture_tl    = (rde_vec_2F){ (_left + _right) * 0.5f - _pw * 0.5f, _area_top - (_area_h - _ph) * 0.5f };
    // The photo and the lines over it are never mirrored (a right-to-left screen only moves them to their place:
    // draw.h), and a tap on a line maps the same way.
    fude_draw_keep_begin((rde_vec_2F){ _scan->picture_tl.x, _scan->picture_tl.y - _ph }, (rde_vec_2F){ _scan->picture_tl.x + _pw, _scan->picture_tl.y }, true);
    // A picture whose rows are top first (a camera frame) is flipped first, since RDE draws a texture's first row at
    // the bottom; then it is turned upright.
    rde_rendering_2d_draw_texture_2(_scan->shown, (rde_vec_3F){ fude_draw_x(_scan->picture_tl.x + _pw * 0.5f), _scan->picture_tl.y - _ph * 0.5f, 0.0f },
                                    (rde_vec_2F){ _scale, _scan->shown_top_first ? -_scale : _scale }, FUDE_SCAN_TURN * _scan->shown_rotation,
                                    (rde_color){ 255, 255, 255, 255 });

    const fude_scan_line* _lines = (const fude_scan_line*)_scan->lines.memory;
    for(u32 _i = 0; _i < _found; _i++) {
        rde_vec_2F _q[4];
        fude_scan_quad(_scan, &_lines[_i], _q);
        for(u32 _k = 0; _k < 4u; _k++) {
            _q[_k] = fude_draw_at(_q[_k]);
        }
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
    fude_draw_keep_end();
}

// --- The screen (screen.h): its row of buttons, and what it does -----------------------------------------------------

#include "study/app/study.h"
#include "study/services/translate.h"

// The screen's is_open, close, pointer and render functions, which call the scan's own (screen.h).
FUDE_SCREEN_ADAPTERS(fude_scan, fude_scan)
FUDE_SCREEN_RENDER(fude_scan, fude_scan)

// Updates the screen, then hands on Write: the kept lines are written on the page where its menu was opened (the way
// Paste text does), and the screen closes. The text goes through a buffer made as big as every kept line needs
// (fude_scan_text_size, on the std allocator: a reading has no bound on its lines), so none is ever left out; it is
// freed once written.
RDE_INTERNAL void fude_scan_screen_update(fude_app* _app, any _self, f32 _dt) {
    fude_scan* _scan = (fude_scan*)_self;
    fude_scan_update(_scan, _dt);
    if(!_scan->write) {
        return;
    }
    rde_arr TYPE(c8) _lines = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_lines, fude_scan_text_size(_scan));
    if(fude_scan_take_text(_scan, (c8*)_lines.memory, rde_arr_length(&_lines))) {
        const rde_vec_2F _at = _scan->canvas_at;
        fude_scan_close(_scan);
        fude_study_write_text(_app, (const c8*)_lines.memory, _at);
    }
    rde_arr_free(&_lines);
}

// Back closes the screen; Camera, Photos and Translate with Google call the functions of the same names.
FUDE_ROW_CALL(fude_scan_row_back,      fude_scan, fude_scan_close)
FUDE_ROW_CALL(fude_scan_row_camera,    fude_scan, fude_scan_camera)
FUDE_ROW_CALL(fude_scan_row_photos,    fude_scan, fude_scan_photos)
FUDE_ROW_CALL(fude_scan_row_translate, fude_scan, fude_scan_translate)   // The panel of translations, on or off.

// The last button: Hold while the camera is live, else Write, which is handed on in the update (fude_scan_take_text).
RDE_INTERNAL void fude_scan_row_write(fude_app* _app, any _self, u32 _arg) {
    fude_scan* _scan = (fude_scan*)_self;
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    if(_scan->stage == FUDE_SCAN_LIVE) {
        fude_scan_hold(_scan);
    } else {
        fude_scan_write(_scan);
    }
}

// The one row; the enum is its button indices. Translate with Google shows only where the translator is.
enum { FUDE_SCAN_ROW_BACK = 0, FUDE_SCAN_ROW_CAMERA, FUDE_SCAN_ROW_PHOTOS, FUDE_SCAN_ROW_TRANSLATE, FUDE_SCAN_ROW_WRITE };
static const fude_row_button FUDE_SCAN_BUTTONS[] = {
    { FUDE_TEXT_BACK,           FUDE_ICON_BACK,      fude_scan_row_back,      0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_SCAN_CAMERA,    FUDE_ICON_CAMERA,    fude_scan_row_camera,    0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_SCAN_PHOTOS,    FUDE_ICON_IMAGE,     fude_scan_row_photos,    0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_SCAN_TRANSLATE, FUDE_ICON_TRANSLATE, fude_scan_row_translate, 0, FUDE_ROW_PLAIN,   false, fude_translate_available },
    { FUDE_TEXT_SCAN_WRITE_N,   FUDE_ICON_PEN,       fude_scan_row_write,     0, FUDE_ROW_PRIMARY, false, NULL },
};
static const fude_row_def FUDE_SCAN_BUTTON_ROWS[] = { FUDE_ROW_DEF(FUDE_SCAN_BUTTONS) };

// Which row is up: there is only the one.
RDE_INTERNAL u32 fude_scan_screen_row(const any _self) {
    RDE_UNUSED(_self);
    return 0u;
}

// How the buttons show: the last is Hold while the camera is live, then "Write n" with how many lines are kept (greyed
// out at none); Translate with Google is selected while it is on.
RDE_INTERNAL void fude_scan_screen_faces(const any _self, u32 _row, fude_row_face* _faces) {
    const fude_scan* _scan = (const fude_scan*)_self;
    const b8         _live = _scan->stage == FUDE_SCAN_LIVE;
    const u32        _kept = fude_scan_kept(_scan);
    RDE_UNUSED(_row);
    fude_row_face* _write = &_faces[FUDE_SCAN_ROW_WRITE];
    if(_live) {
        rde_snprintf(_write->label, sizeof(_write->label), "%s", fude_text(FUDE_TEXT_SCAN_HOLD));
        _write->icon = FUDE_ICON_PAUSE;
    } else if(_kept > 0u) {
        fude_row_face_count(_write, FUDE_TEXT_SCAN_WRITE_N, _kept);
    } else {
        rde_snprintf(_write->label, sizeof(_write->label), "%s", fude_text(FUDE_TEXT_SCAN_WRITE));
    }
    _write->disabled = !_live && _kept == 0u;
    _faces[FUDE_SCAN_ROW_TRANSLATE].selected = fude_scan_translating(_scan);
}

const fude_screen FUDE_SCAN_SCREEN = {
    .name = "scan", .input = FUDE_SCREEN_INPUT_POINT,
    .is_open = fude_scan_screen_is_open, .close = fude_scan_screen_close,
    .update = fude_scan_screen_update, .render = fude_scan_screen_render,
    .pointer_down = fude_scan_screen_down, .pointer_moved = fude_scan_screen_moved, .pointer_up = fude_scan_screen_up,
    .rows = FUDE_SCAN_BUTTON_ROWS, .row_count = 1u, .row = fude_scan_screen_row, .faces = fude_scan_screen_faces,
    .field_hint = FUDE_TEXT_COUNT,
};
