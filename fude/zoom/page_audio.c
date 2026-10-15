// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, audio synced to ink (0.1.93): recordings kept beside the canvas, the ink linked to them, and their panel.
//
// In order: the files (<id>.audio, the list, and <id>.rec<n>, each recording), opening and closing a canvas's
// recordings, recording and playing, the links made each frame, then the panel (its layout, drawing and presses) and
// the chips that open it. The pages' chip's press is here too. Recordings go through voice.h (RDE's audio threads,
// pumped here once a frame); transcripts through page_speech.c (the fude_zoom_page_heard_ and _listen_ functions).
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// --- audio synced to ink (0.1.93) ----------------------------------------------------------------------------
//
// The microphone's chip starts a recording (its permission asked the first time) and stops it; while it runs, each
// thing made on the canvas is linked to the moment it was begun (a stroke: when the pen went down). The recordings'
// chip opens their panel: each one, a tap playing it; under them where it is, −15 s, play or pause, +15 s, its bar to
// drag, and Replay ink (what was written after where it is, faint). With the panel up, a tap on linked ink plays from
// a moment before it was written. Kept beside the canvas (audio.h).

#define FUDE_ZOOM_PAGE_AUDIO_ROW    50.0   // A recording's row in its panel (points).
#define FUDE_ZOOM_PAGE_AUDIO_ROWS   3u     // Rows shown at once (the rest scrolled to).
#define FUDE_ZOOM_PAGE_AUDIO_HEAD   44.0   // The panel's head: its title and hint (points).
#define FUDE_ZOOM_PAGE_AUDIO_PLAYER 64.0   // Its player's row (points).
#define FUDE_ZOOM_PAGE_AUDIO_SAVE   10.0   // Seconds between saves of the list and its links while recording.
#define FUDE_ZOOM_PAGE_AUDIO_LEAD   1500u  // A tap on ink plays from this many milliseconds before it was written.

// Returns whether the app offers recordings (product.h: a notes app does, a workshop app does not).
b8 fude_zoom_page_audio_offered(void) {
    return fude_zoom_product_get()->audio;
}

// Writes the path of the canvas's list of recordings and links, <id>.audio.
RDE_INTERNAL void fude_zoom_page_audio_path(u32 _canvas, c8* _out, usize _size) {
    fude_zoom_page_side_path(_canvas, "audio", _out, _size);
}

// Writes the path of recording _number's sound, <id>.rec<n>.
void fude_zoom_page_audio_rec_path(u32 _canvas, u32 _number, c8* _out, usize _size) {
    c8 _ext[24];
    rde_snprintf(_ext, sizeof(_ext), "rec%u", _number);
    fude_zoom_page_side_path(_canvas, _ext, _out, _size);
}

// Saves the list of recordings and links beside the canvas, or deletes the file once there are none. Notes when it was
// saved, for the saves while recording.
void fude_zoom_page_audio_keep(fude_zoom_page* _page) {
    if(_page->canvas == 0u) {
        return;
    }
    c8 _path[RDE_MAX_PATH];
    fude_zoom_page_audio_path(_page->canvas, _path, sizeof(_path));
    if(rde_arr_length(&_page->audio.recordings) == 0u) {
        if(rde_file_exists(_path)) {
            rde_file_delete(_path);   // None left: nothing beside the canvas.
        }
        return;
    }
    fude_zoom_audio_save(&_page->audio, _path);
    _page->audio_saved_at = rde_engine_get_time_now();
}

// Writes _ms milliseconds as "m:ss", or "h:mm:ss" from an hour.
void fude_zoom_page_audio_time(u64 _ms, c8* _out, usize _size) {
    const u64 _s = _ms / 1000u;
    if(_s >= 3600u) {
        rde_snprintf(_out, _size, "%u:%02u:%02u", (u32)(_s / 3600u), (u32)((_s / 60u) % 60u), (u32)(_s % 60u));
    } else {
        rde_snprintf(_out, _size, "%u:%02u", (u32)(_s / 60u), (u32)(_s % 60u));
    }
}

// A canvas opened: its recordings read. One cut short by the app stopping (no length kept) is as long as what reached
// its file, and the list is saved again with it. The panel is closed, nothing plays, and Live text is as it was left.
void fude_zoom_page_audio_open(fude_zoom_page* _page) {
    c8 _path[RDE_MAX_PATH];
    fude_zoom_page_audio_path(_page->canvas, _path, sizeof(_path));
    fude_zoom_audio_load(&_page->audio, _path);
    b8 _mended = false;
    fude_zoom_recording* _r = (fude_zoom_recording*)_page->audio.recordings.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->audio.recordings); _i++) {
        if(_r[_i].frames == 0u) {
            c8 _rec[RDE_MAX_PATH];
            fude_zoom_page_audio_rec_path(_page->canvas, _r[_i].number, _rec, sizeof(_rec));
            _r[_i].frames = fude_zoom_audio_frames_in(fude_file_size(_rec));
            _mended = true;
        }
    }
    if(_mended) {
        fude_zoom_page_audio_keep(_page);
    }
    _page->audio_panel    = false;
    _page->audio_playing  = 0u;
    _page->audio_armed    = 0u;
    _page->audio_faint_at = FUDE_ZOOM_NONE;
    fude_zoom_page_listen_setting(_page, false);   // Live text: as it was left.
}

// Declared here for fude_zoom_page_audio_close; defined below.
RDE_INTERNAL void fude_zoom_page_audio_stop_recording(fude_zoom_page* _page);

// A canvas left: what was being recorded kept, what was playing stopped, and the recordings, links and transcripts in
// memory forgotten.
void fude_zoom_page_audio_close(fude_zoom_page* _page) {
    fude_zoom_page_audio_stop_recording(_page);
    fude_zoom_page_audio_stop_playing(_page);
    _page->audio_panel = false;
    fude_zoom_page_heard_forget(_page);
    fude_zoom_audio_clear(&_page->audio);
}

// The app going to the background: a recording is stopped and kept, and the player paused.
void fude_zoom_page_audio_pause(fude_zoom_page* _page) {
    fude_zoom_page_audio_stop_recording(_page);
    if(fude_zoom_voice_is_open(&_page->player)) {
        fude_zoom_voice_pause(&_page->player);
    }
}

// Starts a new recording: added to the list and kept at once (so one cut short is still there), with the microphone
// writing to <id>.rec<n>. When the microphone cannot be opened the recording is dropped and a notice says so. What
// was playing stops, since the speaker would be heard by the microphone.
RDE_INTERNAL void fude_zoom_page_audio_begin(fude_zoom_page* _page) {
    const u32 _number = fude_zoom_audio_begin(&_page->audio, (u64)rde_date_now_unix_seconds(), FUDE_ZOOM_AUDIO_RATE);
    c8 _path[RDE_MAX_PATH];
    fude_zoom_page_audio_rec_path(_page->canvas, _number, _path, sizeof(_path));
    if(!fude_zoom_voice_record(&_page->recorder, _path, FUDE_ZOOM_AUDIO_RATE)) {
        fude_zoom_audio_remove(&_page->audio, _number);
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_AUDIO_NO_MIC));
        return;
    }
    fude_zoom_page_audio_stop_playing(_page);   // One at a time: the speaker would be heard by the microphone.
    _page->audio_recording   = _number;
    _page->audio_started_at  = rde_engine_get_time_now();
    _page->audio_pushes_seen = _page->scene.history_pushes;
    _page->audio_down_linked = _page->audio_started_at - 1.0;   // (no pen-down linked yet: before the recording)
    fude_zoom_page_audio_keep(_page);   // In the list from the start: cut short, it is still there.
    fude_notice_show(fude_text(FUDE_TEXT_ZOOM_AUDIO_STARTED));
}

// Stops the recording running, if any: its length kept, its last words asked for when Live text is on, the list
// saved, and a notice says how long it was.
RDE_INTERNAL void fude_zoom_page_audio_stop_recording(fude_zoom_page* _page) {
    if(_page->audio_recording == 0u) {
        return;
    }
    const u32 _frames = fude_zoom_voice_record_stop(&_page->recorder);
    fude_zoom_recording* _r = fude_zoom_audio_find(&_page->audio, _page->audio_recording);
    if(_r != NULL) {
        _r->frames = _frames;
    }
    if(_page->heard_live && fude_listen_built()) {
        fude_zoom_page_heard_of(_page, _page->audio_recording, true)->wanted = true;   // Live text: its last words too.
    }
    _page->audio_recording = 0u;
    fude_zoom_page_audio_keep(_page);
    c8 _time[24], _say[160];
    fude_zoom_page_audio_time((u64)_frames * 1000u / FUDE_ZOOM_AUDIO_RATE, _time, sizeof(_time));
    FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_AUDIO_SAVED, FUDE_TS(_time));
    fude_notice_show(_say);
}

// The microphone's answer, on a later frame: audio_asked becomes 2 (yes) or 3 (no), which the next update acts on.
RDE_INTERNAL void fude_zoom_page_audio_permitted(RDE_MOBILE_PERMISSION_ _permission, RDE_MOBILE_PERMISSION_STATUS_ _status, any _user_data) {
    RDE_UNUSED(_permission); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page != NULL) {
        _page->audio_asked = _status == RDE_MOBILE_PERMISSION_STATUS_GRANTED ? 2u : 3u;
    }
}

// The microphone's chip: a recording started (once the microphone may be used), or the one running stopped. Asking
// for the microphone sets audio_asked to 1, so it is asked once at a time; the recording begins when the answer comes,
// if it is yes.
void fude_zoom_page_audio_toggle(fude_zoom_page* _page) {
    if(_page->audio_recording != 0u) {
        fude_zoom_page_audio_stop_recording(_page);
        return;
    }
    const RDE_MOBILE_PERMISSION_STATUS_ _now = rde_mobile_get_permission_status(RDE_MOBILE_PERMISSION_MICROPHONE);
    if(_now == RDE_MOBILE_PERMISSION_STATUS_GRANTED) {
        fude_zoom_page_audio_begin(_page);
    } else if(_now == RDE_MOBILE_PERMISSION_STATUS_DENIED_PERMANENTLY) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_AUDIO_DENIED));
    } else if(_page->audio_asked != 1u) {
        _page->audio_asked = 1u;   // Asked: it begins when the answer comes, if yes.
        rde_mobile_request_permission(RDE_MOBILE_PERMISSION_MICROPHONE, fude_zoom_page_audio_permitted, NULL);
    }
}

// Closes the player and forgets what was playing and its faint ink.
void fude_zoom_page_audio_stop_playing(fude_zoom_page* _page) {
    if(fude_zoom_voice_is_open(&_page->player)) {
        fude_zoom_voice_close(&_page->player);
    }
    _page->audio_playing  = 0u;
    _page->audio_faint_at = FUDE_ZOOM_NONE;
    rde_arr_clear(&_page->audio_faint);
}

// Puts recording _number in the player, at its start, playing if _play; one already in the player is kept where it is.
// Returns false when there is no such recording, one is being recorded, or its file cannot be opened.
b8 fude_zoom_page_audio_load(fude_zoom_page* _page, u32 _number, b8 _play) {
    if(_page->audio_playing == _number && fude_zoom_voice_is_open(&_page->player)) {
        if(_play) {
            fude_zoom_voice_play(&_page->player);
        }
        return true;
    }
    fude_zoom_page_audio_stop_playing(_page);
    const fude_zoom_recording* _r = fude_zoom_audio_find(&_page->audio, _number);
    if(_r == NULL || _page->audio_recording != 0u) {
        return false;
    }
    c8 _path[RDE_MAX_PATH];
    fude_zoom_page_audio_rec_path(_page->canvas, _number, _path, sizeof(_path));
    if(!fude_zoom_voice_open(&_page->player, _path, _r->rate, _r->frames)) {
        return false;
    }
    _page->audio_playing = _number;
    if(_play) {
        fude_zoom_voice_play(&_page->player);
    }
    return true;
}

// Returns where the player is, in milliseconds (0 with nothing open).
RDE_INTERNAL u32 fude_zoom_page_audio_ms(const fude_zoom_page* _page) {
    return _page->player.rate > 0u ? (u32)((u64)fude_zoom_voice_position(&_page->player) * 1000u / _page->player.rate) : 0u;
}

// Returns whether, with the panel up, there is ink linked to a recording under _screen. Never while recording.
b8 fude_zoom_page_audio_linked_at(fude_zoom_page* _page, rde_vec_2F _screen) {
    if(!_page->audio_panel || rde_arr_length(&_page->audio.links) == 0u || _page->audio_recording != 0u) {
        return false;
    }
    const u32 _o = fude_zoom_page_under(_page, _screen);
    u32 _number = 0, _ms = 0;
    return _o != FUDE_ZOOM_NONE && fude_zoom_audio_link_of(&_page->audio, fude_zoom_scene_object(&_page->scene, _o)->id, &_number, &_ms);
}

// With the panel up, a tap on linked ink: its recording played from FUDE_ZOOM_PAGE_AUDIO_LEAD before it was written.
// Returns true when it was such a tap and the recording opened. Never while recording.
b8 fude_zoom_page_audio_tap(fude_zoom_page* _page, rde_vec_2F _screen) {
    if(!_page->audio_panel || rde_arr_length(&_page->audio.links) == 0u || _page->audio_recording != 0u) {
        return false;
    }
    const u32 _o = fude_zoom_page_under(_page, _screen);
    u32 _number = 0, _ms = 0;
    if(_o == FUDE_ZOOM_NONE || !fude_zoom_audio_link_of(&_page->audio, fude_zoom_scene_object(&_page->scene, _o)->id, &_number, &_ms)) {
        return false;
    }
    if(!fude_zoom_page_audio_load(_page, _number, false)) {
        return false;
    }
    const u32 _from = _ms > FUDE_ZOOM_PAGE_AUDIO_LEAD ? _ms - FUDE_ZOOM_PAGE_AUDIO_LEAD : 0u;
    fude_zoom_voice_seek(&_page->player, (u32)((u64)_from * _page->player.rate / 1000u));
    fude_zoom_voice_play(&_page->player);
    _page->audio_faint_at = FUDE_ZOOM_NONE;
    return true;
}

// Once a frame: the microphone's answer acted on; while recording, the microphone read, what the last step made
// linked, and the list saved every FUDE_ZOOM_PAGE_AUDIO_SAVE seconds; the player kept fed; speech read; and Replay
// ink's faint objects worked out again when the player has moved 40 ms or more or the scene's objects changed.
void fude_zoom_page_audio_update(fude_zoom_page* _page) {
    if(!_page->open) {
        return;
    }
    if(_page->audio_asked == 2u) {
        _page->audio_asked = 0u;
        if(_page->audio_recording == 0u) {
            fude_zoom_page_audio_begin(_page);
        }
    } else if(_page->audio_asked == 3u) {
        _page->audio_asked = 0u;
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_AUDIO_DENIED));
    }
    fude_zoom_scene* _s   = &_page->scene;
    const f64        _now = rde_engine_get_time_now();
    if(_page->audio_recording != 0u) {
        if(fude_zoom_voice_record_pump(&_page->recorder) > 0u) {
            fude_notice_show(fude_text(FUDE_TEXT_ZOOM_AUDIO_LOST));   // (some of what was said is gone: they are told)
        }
        // What the last step made, linked: to when it was begun (fude_zoom_audio_begun: when the pen last went down, if
        // that began it — within the recording and the last FUDE_ZOOM_AUDIO_DOWN_MOST seconds, and no step linked to it
        // yet; else now, so a text or a shape made after a stroke is linked to its own time, not the stroke's).
        if(_s->history_pushes != _page->audio_pushes_seen) {
            _page->audio_pushes_seen = _s->history_pushes;
            const fude_zoom_action* _a = fude_zoom_history_next_undo(_s);
            if(_a != NULL && (_a->type == FUDE_ZOOM_ACTION_BORN || _a->type == FUDE_ZOOM_ACTION_REPLACED)) {
                const u32 _first = _a->born_first, _count = _a->born;   // What it made: a step's born, however it came.
                const f64 _began = fude_zoom_audio_begun(_page->stroke_began, _page->audio_down_linked, _page->audio_started_at, _now);
                if(_began == _page->stroke_began) {
                    _page->audio_down_linked = _page->stroke_began;   // (that pen-down's step linked: the next is not its)
                }
                const u32 _ms    = (u32)rde_math_max_f64((_began - _page->audio_started_at) * 1000.0, 0.0);
                const u32* _t    = (const u32*)_s->targets.memory;
                for(u32 _k = 0; _k < _count && _first + _k < (u32)rde_arr_length(&_s->targets); _k++) {
                    const fude_zoom_object* _o = fude_zoom_scene_object(_s, _t[_first + _k]);
                    fude_zoom_audio_add_link(&_page->audio, _o->id, _page->audio_recording, _ms);
                }
            }
        }
        if(_now - _page->audio_saved_at >= FUDE_ZOOM_PAGE_AUDIO_SAVE) {
            fude_zoom_page_audio_keep(_page);
        }
    }
    if(_page->audio_playing != 0u && fude_zoom_voice_is_open(&_page->player)) {
        fude_zoom_voice_pump(&_page->player);
    }
    fude_zoom_page_listen_update(_page);   // Speech read: the model's download, the pieces.
    // Replay ink: what this recording linked past where it is, faint (one flag for each object in audio_faint), worked
    // out again as the player moves on.
    const b8 _replay = _page->audio_panel && _page->audio_replay && _page->audio_playing != 0u;
    if(!_replay) {
        if(rde_arr_length(&_page->audio_faint) > 0u) {
            rde_arr_clear(&_page->audio_faint);
        }
        _page->audio_faint_at = FUDE_ZOOM_NONE;
        return;
    }
    const u32 _ms = fude_zoom_page_audio_ms(_page);
    if(_page->audio_faint_at != FUDE_ZOOM_NONE && (_ms >= _page->audio_faint_at ? _ms - _page->audio_faint_at : _page->audio_faint_at - _ms) < 40u &&
       rde_arr_length(&_page->audio_faint) == fude_zoom_scene_object_count(_s)) {
        return;
    }
    _page->audio_faint_at = _ms;
    rde_arr_resize(&_page->audio_faint, fude_zoom_scene_object_count(_s));
    u8* _f = (u8*)_page->audio_faint.memory;
    rde_memset(_f, 0, rde_arr_length(&_page->audio_faint));
    const fude_zoom_audio_link* _l = (const fude_zoom_audio_link*)_page->audio.links.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->audio.links); _i++) {
        if(_l[_i].recording != _page->audio_playing || _l[_i].at_ms <= _ms) {
            continue;
        }
        const u32 _o = fude_zoom_scene_find_object(_s, _l[_i].object);
        if(_o != FUDE_ZOOM_NONE && _o < (u32)rde_arr_length(&_page->audio_faint)) {
            _f[_o] = 1u;
        }
    }
}

// --- its panel ---------------------------------------------------------------------------------------------------

// Where the panel's parts are on the screen.
typedef struct {
    fude_zoom_box panel, close, rows, player;
    fude_zoom_box back, play, ahead, bar, replay;
    fude_zoom_box live;   // Live text's chip (where speech can be read; else empty).
} fude_zoom_page_audio_layout;

// Lays the panel out over the pages' strip's place, at the bottom: its height for up to FUDE_ZOOM_PAGE_AUDIO_ROWS
// rows, its width 620 points (780 with Live text), then the close button, the rows, and the player's row: −15 s,
// play, +15 s, the bar, Replay ink.
RDE_INTERNAL void fude_zoom_page_audio_lay(const fude_zoom_page* _page, fude_zoom_page_audio_layout* _l) {
    fude_zoom_page_strip_layout _s;
    fude_zoom_page_strip_lay(_page, &_s);
    const u32 _n = (u32)rde_arr_length(&_page->audio.recordings);
    const u32 _shown = _n < FUDE_ZOOM_PAGE_AUDIO_ROWS ? (_n > 0u ? _n : 1u) : FUDE_ZOOM_PAGE_AUDIO_ROWS;
    const f64 _bottom = _s.panel.min_y;
    const f64 _top    = _bottom + 2.0 * FUDE_ZOOM_PAGE_STRIP_PAD + FUDE_ZOOM_PAGE_AUDIO_HEAD + (f64)_shown * FUDE_ZOOM_PAGE_AUDIO_ROW + FUDE_ZOOM_PAGE_AUDIO_PLAYER;
    const f64 _right  = rde_math_min_f64(_s.panel.max_x, _s.panel.min_x + (fude_listen_built() ? 780.0 : 620.0));
    const f64 _x0 = _s.panel.min_x + FUDE_ZOOM_PAGE_STRIP_PAD, _x1 = _right - FUDE_ZOOM_PAGE_STRIP_PAD;
    _l->panel  = (fude_zoom_box){ _s.panel.min_x, _bottom, _right, _top };
    _l->close  = (fude_zoom_box){ _right - FUDE_ZOOM_PAGE_STRIP_CLOSE, _top - 44.0, _right - 8.0, _top - 8.0 };
    _l->player = (fude_zoom_box){ _x0, _bottom + FUDE_ZOOM_PAGE_STRIP_PAD, _x1, _bottom + FUDE_ZOOM_PAGE_STRIP_PAD + FUDE_ZOOM_PAGE_AUDIO_PLAYER };
    _l->rows   = (fude_zoom_box){ _x0, _l->player.max_y, _x1, _top - FUDE_ZOOM_PAGE_STRIP_PAD - FUDE_ZOOM_PAGE_AUDIO_HEAD };
    // Its player: −15 s, play, +15 s, the bar, Replay ink.
    const f64 _y0 = _l->player.min_y + 10.0, _y1 = _l->player.max_y - 10.0;
    _l->back   = (fude_zoom_box){ _x0, _y0, _x0 + 64.0, _y1 };
    _l->play   = (fude_zoom_box){ _x0 + 72.0, _y0, _x0 + 72.0 + 52.0, _y1 };
    _l->ahead  = (fude_zoom_box){ _x0 + 132.0, _y0, _x0 + 196.0, _y1 };
    _l->replay = (fude_zoom_box){ _x1 - 150.0, _y0, _x1, _y1 };
    _l->bar    = (fude_zoom_box){ _x0 + 212.0, _y0, _x1 - 166.0, _y1 };
    _l->live   = fude_listen_built() ? (fude_zoom_box){ _l->close.min_x - 8.0 - 170.0, _l->close.min_y + 2.0, _l->close.min_x - 8.0, _l->close.max_y - 2.0 }
                                     : (fude_zoom_box){ 0.0, 0.0, 0.0, 0.0 };
}

// What a recording's row's chips are, from its right: Delete (pressed once: asked), then what its words are at:
// Transcribe (the model not here yet: pressed once, its download asked), Downloading or Transcribing with how far,
// As text and As handwriting, or No speech found.
typedef enum {
    FUDE_ZOOM_PAGE_ROW_DELETE = 0,
    FUDE_ZOOM_PAGE_ROW_TRANSCRIBE,
    FUDE_ZOOM_PAGE_ROW_BUSY,
    FUDE_ZOOM_PAGE_ROW_AS_TEXT,
    FUDE_ZOOM_PAGE_ROW_AS_HAND,
    FUDE_ZOOM_PAGE_ROW_NONE,
} FUDE_ZOOM_PAGE_ROW_;

// One chip of a row: its box, what it is, whether it is asking (drawn red), and its words.
typedef struct {
    fude_zoom_box box;
    u8            kind;     // FUDE_ZOOM_PAGE_ROW_
    b8            asking;   // Pressed once: red, asking.
    c8            words[96];
} fude_zoom_page_row_chip;

// Returns a chip's width for its words at 14 points, with its padding (8 points a byte with no font).
RDE_INTERNAL f64 fude_zoom_page_chip_width(const fude_zoom_page* _page, const c8* _words) {
    return (_page->renderer.font != NULL ? (f64)fude_draw_text_width(_page->renderer.font, _page->renderer.font_px, _words, 14.0f) : 8.0 * (f64)rde_strlen(_words)) + 26.0;
}

// Works out a recording's row's chips into _out, from the row's right, and returns how many (at most 3). The one being
// recorded has none. Delete is always there; the transcript's chips only where speech can be read (fude_listen_built).
RDE_INTERNAL u32 fude_zoom_page_audio_row_chips(fude_zoom_page* _page, const fude_zoom_recording* _rec, fude_zoom_box _row, fude_zoom_page_row_chip _out[3]) {
    if(_rec->number == _page->audio_recording) {
        return 0u;   // Being made: nothing to do with it yet.
    }
    u32 _n = 0;
    f64 _x = _row.max_x - 6.0;
    // Adds a chip of kind _kind with its words, to the left of the last.
    #define FZPG_CHIP(_kind, _asking, ...) do { \
        fude_zoom_page_row_chip* _c = &_out[_n++]; _c->kind = (_kind); _c->asking = (_asking); rde_snprintf(_c->words, sizeof(_c->words), __VA_ARGS__); \
        const f64 _w = fude_zoom_page_chip_width(_page, _c->words); \
        _c->box = (fude_zoom_box){ _x - _w, _row.min_y + 6.0, _x, _row.max_y - 6.0 }; _x -= _w + 6.0; } while(0)
    const b8 _armed = _page->audio_armed == _rec->number;
    FZPG_CHIP(FUDE_ZOOM_PAGE_ROW_DELETE, _armed, "%s", fude_text(_armed ? FUDE_TEXT_ZOOM_AUDIO_DELETE_ASK : FUDE_TEXT_DELETE));
    if(fude_listen_built()) {
        const fude_zoom_page_heard* _h = fude_zoom_page_heard_of(_page, _rec->number, false);
        if(_h != NULL && _h->t.complete) {
            if(rde_arr_length(&_h->t.segments) > 0u) {
                FZPG_CHIP(FUDE_ZOOM_PAGE_ROW_AS_HAND, false, "%s", fude_text(FUDE_TEXT_ZOOM_LISTEN_AS_HAND));
                FZPG_CHIP(FUDE_ZOOM_PAGE_ROW_AS_TEXT, false, "%s", fude_text(FUDE_TEXT_ZOOM_LISTEN_AS_TEXT));
            } else {
                FZPG_CHIP(FUDE_ZOOM_PAGE_ROW_NONE, false, "%s", fude_text(FUDE_TEXT_ZOOM_LISTEN_NONE));
            }
        } else if(_h != NULL && _h->wanted && _page->heard_downloading) {
            u64 _got = 0, _total = 0;
            fude_fetch_state(&_got, &_total);
            c8 _say[96];
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_LISTEN_DOWNLOADING, FUDE_TN(_total > 0u ? _got * 100u / _total : 0u));
            FZPG_CHIP(FUDE_ZOOM_PAGE_ROW_BUSY, false, "%s", _say);
        } else if(_h != NULL && _h->wanted) {
            // How far: what is read, and the piece being read as far as it has gone.
            f64 _done = (f64)_h->t.done;
            if(_page->heard_reading == _rec->number) {
                _done += (f64)fude_listen_progress() * (f64)(_page->heard_piece_end - _h->t.done);
            }
            c8 _say[96];
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_LISTEN_READING, FUDE_TN((u32)rde_math_min_f64(_done * 100.0 / rde_math_max_f64((f64)_rec->frames, 1.0), 99.0)));
            FZPG_CHIP(FUDE_ZOOM_PAGE_ROW_BUSY, false, "%s", _page->heard_reading == _rec->number || fude_listen_model_ready() ? _say : fude_text(FUDE_TEXT_ZOOM_LISTEN_WAITING));
        } else if(_page->heard_ask == _rec->number && !fude_listen_model_ready()) {
            c8 _say[96];
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_LISTEN_DOWNLOAD_ASK, FUDE_TN(FUDE_ZOOM_PAGE_LISTEN_MB));
            FZPG_CHIP(FUDE_ZOOM_PAGE_ROW_TRANSCRIBE, true, "%s", _say);
        } else {
            FZPG_CHIP(FUDE_ZOOM_PAGE_ROW_TRANSCRIBE, false, "%s", fude_text(FUDE_TEXT_ZOOM_LISTEN_TRANSCRIBE));
        }
    }
    #undef FZPG_CHIP
    return _n;
}

// Returns how far the rows can be scrolled: their height past what the panel shows.
RDE_INTERNAL f64 fude_zoom_page_audio_most(const fude_zoom_page* _page, const fude_zoom_page_audio_layout* _l) {
    return rde_math_max_f64((f64)rde_arr_length(&_page->audio.recordings) * FUDE_ZOOM_PAGE_AUDIO_ROW - (_l->rows.max_y - _l->rows.min_y), 0.0);
}

// Draws a chip: its box, its words in its middle; an accent fill when on, red words and outline when _danger.
RDE_INTERNAL void fude_zoom_page_audio_chip(fude_zoom_page* _page, fude_zoom_box _b, const c8* _words, b8 _on, b8 _danger) {
    const fude_theme* _t = fude_theme_active();
    const rde_vec_2F _c = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) }, _size = { (f32)(_b.max_x - _b.min_x), (f32)(_b.max_y - _b.min_y) };
    rde_rendering_2d_draw_rounded_rectangle_with_border(_c, _size, 0.5f, 8u, _on ? _t->accent : _t->surface_2, 1.0f, _danger ? _t->danger : _on ? _t->accent : _t->outline, NULL);
    fude_zoom_render_text_in(&_page->renderer, _words, (u32)rde_strlen(_words), _b, 14.0, false, _danger ? _t->danger : _on ? _t->page : _t->text);
}

// Draws the panel (closed instead when there are no recordings): its head, Live text, its rows newest first, and its
// player. Not while presenting.
void fude_zoom_page_render_audio(fude_zoom_page* _page) {
    if(!_page->audio_panel || _page->presenting || !_page->open) {
        return;
    }
    if(rde_arr_length(&_page->audio.recordings) == 0u) {
        _page->audio_panel = false;
        return;
    }
    fude_zoom_page_audio_layout _l;
    fude_zoom_page_audio_lay(_page, &_l);
    _page->audio_scroll = rde_math_min_f64(rde_math_max_f64(_page->audio_scroll, 0.0), fude_zoom_page_audio_most(_page, &_l));
    const fude_theme* _t = fude_theme_active();
    const fude_zoom_box _p = _l.panel;
    rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (f32)((_p.min_x + _p.max_x) * 0.5), (f32)((_p.min_y + _p.max_y) * 0.5) },
                                                        (rde_vec_2F){ (f32)(_p.max_x - _p.min_x), (f32)(_p.max_y - _p.min_y) }, 0.05f, 8u, _t->surface, 1.0f, _t->outline, NULL);
    // Its head: what it is, and the tap that plays from the ink.
    const c8* _title = fude_text(FUDE_TEXT_ZOOM_AUDIO_RECORDINGS);
    const f64 _head_right = fude_listen_built() ? _l.live.min_x - 8.0 : _l.close.min_x - 8.0;
    const fude_zoom_box _hb = { _l.rows.min_x, _l.rows.max_y + 20.0, _head_right, _p.max_y - FUDE_ZOOM_PAGE_STRIP_PAD };
    fude_zoom_render_text_at(&_page->renderer, _title, (u32)rde_strlen(_title), _hb, 16.0, false, true, _t->text);
    const c8* _hint = fude_text(FUDE_TEXT_ZOOM_AUDIO_TAP_HINT);
    fude_zoom_render_text_at(&_page->renderer, _hint, (u32)rde_strlen(_hint), (fude_zoom_box){ _l.rows.min_x, _l.rows.max_y, _head_right, _l.rows.max_y + 20.0 },
                             12.0, false, true, _t->text_soft);
    {
        const fude_zoom_box _b = _l.close;
        const rde_vec_2F _c = { (f32)((_b.min_x + _b.max_x) * 0.5), (f32)((_b.min_y + _b.max_y) * 0.5) };
        rde_rendering_2d_draw_circle_with_border(_c, 16.0f, 24u, _t->surface_2, 1.0f, _t->outline, NULL);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y - 5.0f }, (rde_vec_2F){ _c.x + 5.0f, _c.y + 5.0f }, _t->text, 1.6f);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y + 5.0f }, (rde_vec_2F){ _c.x + 5.0f, _c.y - 5.0f }, _t->text, 1.6f);
    }
    // Live text (where speech is read): on, or its download asked.
    if(fude_listen_built()) {
        c8 _say[96];
        if(_page->heard_ask == FUDE_ZOOM_NONE && !fude_listen_model_ready()) {
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_LISTEN_DOWNLOAD_ASK, FUDE_TN(FUDE_ZOOM_PAGE_LISTEN_MB));
        } else {
            rde_snprintf(_say, sizeof(_say), "%s", fude_text(FUDE_TEXT_ZOOM_LISTEN_LIVE));
        }
        fude_zoom_page_audio_chip(_page, _l.live, _say, _page->heard_live, _page->heard_ask == FUDE_ZOOM_NONE && !fude_listen_model_ready());
    }
    // Its recordings, newest first: each one's name, when, how long; its Delete (pressed once: asked).
    const fude_zoom_recording* _r = (const fude_zoom_recording*)_page->audio.recordings.memory;
    const u32 _n = (u32)rde_arr_length(&_page->audio.recordings);
    for(u32 _k = 0; _k < _n; _k++) {
        const fude_zoom_recording* _rec = &_r[_n - 1u - _k];
        const f64 _top = _l.rows.max_y - (f64)_k * FUDE_ZOOM_PAGE_AUDIO_ROW + _page->audio_scroll;
        const fude_zoom_box _row = { _l.rows.min_x, _top - FUDE_ZOOM_PAGE_AUDIO_ROW + 4.0, _l.rows.max_x, _top - 2.0 };
        if(_row.max_y > _l.rows.max_y + 0.5 || _row.min_y < _l.rows.min_y - 0.5) {
            continue;
        }
        const b8 _on = _rec->number == _page->audio_playing;
        rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (f32)((_row.min_x + _row.max_x) * 0.5), (f32)((_row.min_y + _row.max_y) * 0.5) },
                                                            (rde_vec_2F){ (f32)(_row.max_x - _row.min_x), (f32)(_row.max_y - _row.min_y) }, 0.15f, 8u, _t->surface_2, _on ? 2.0f : 1.0f,
                                                            _on ? _t->accent : _t->outline, NULL);
        c8 _name[96], _date[48], _len[24], _line[200];
        FUDE_TEXTF(_name, FUDE_TEXT_ZOOM_AUDIO_RECORDING_N, FUDE_TN(_rec->number));
        fude_text_date(_date, sizeof(_date), _rec->started);
        fude_zoom_page_audio_time((u64)_rec->frames * 1000u / (_rec->rate > 0u ? _rec->rate : 1u), _len, sizeof(_len));
        rde_snprintf(_line, sizeof(_line), "%s \xC2\xB7 %s \xC2\xB7 %s", _name, _date, _len);
        fude_zoom_page_row_chip _chips[3];
        const u32 _nc = fude_zoom_page_audio_row_chips(_page, _rec, _row, _chips);
        const f64 _text_right = _nc > 0u ? _chips[_nc - 1u].box.min_x - 8.0 : _row.max_x - 8.0;
        fude_zoom_render_text_at(&_page->renderer, _line, (u32)rde_strlen(_line), (fude_zoom_box){ _row.min_x + 12.0, _row.min_y, _text_right, _row.max_y }, 15.0, false, true,
                                 _rec->number == _page->audio_recording ? _t->danger : _t->text);
        for(u32 _c = 0; _c < _nc; _c++) {
            const b8 _lit = _chips[_c].kind == FUDE_ZOOM_PAGE_ROW_AS_TEXT || _chips[_c].kind == FUDE_ZOOM_PAGE_ROW_AS_HAND;
            fude_zoom_page_audio_chip(_page, _chips[_c].box, _chips[_c].words, _lit, _chips[_c].asking);
        }
    }
    // Its player.
    const b8 _open = _page->audio_playing != 0u && fude_zoom_voice_is_open(&_page->player);
    fude_zoom_page_audio_chip(_page, _l.back, fude_text(FUDE_TEXT_ZOOM_AUDIO_BACK), false, false);
    fude_zoom_page_audio_chip(_page, _l.ahead, fude_text(FUDE_TEXT_ZOOM_AUDIO_AHEAD), false, false);
    fude_zoom_page_audio_chip(_page, _l.play, "", _open && _page->player.playing, false);
    {
        // Play: a triangle; playing: two bars.
        const rde_vec_2F _c = { (f32)((_l.play.min_x + _l.play.max_x) * 0.5), (f32)((_l.play.min_y + _l.play.max_y) * 0.5) };
        const rde_color _ink = _open && _page->player.playing ? _t->page : _t->text;
        if(_open && _page->player.playing) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ _c.x - 5.0f, _c.y }, (rde_vec_2F){ 4.0f, 16.0f }, _ink);
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ _c.x + 5.0f, _c.y }, (rde_vec_2F){ 4.0f, 16.0f }, _ink);
        } else {
            rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y - 8.0f }, (rde_vec_2F){ _c.x - 5.0f, _c.y + 8.0f }, _ink, 2.0f);
            rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x - 5.0f, _c.y + 8.0f }, (rde_vec_2F){ _c.x + 8.0f, _c.y }, _ink, 2.0f);
            rde_rendering_2d_draw_line_1((rde_vec_2F){ _c.x + 8.0f, _c.y }, (rde_vec_2F){ _c.x - 5.0f, _c.y - 8.0f }, _ink, 2.0f);
        }
    }
    fude_zoom_page_audio_chip(_page, _l.replay, fude_text(FUDE_TEXT_ZOOM_AUDIO_REPLAY), _page->audio_replay, false);
    {
        // Its bar: where it is, of how long (dragged: where it would go).
        const u32 _frames = _open ? _page->player.frames : 0u;
        const f64 _k = _page->audio_scrubbing ? (f64)_page->audio_scrub : _frames > 0u ? (f64)fude_zoom_voice_position(&_page->player) / (f64)_frames : 0.0;
        const f64 _mid = (_l.bar.min_y + _l.bar.max_y) * 0.5 + 6.0, _x0 = _l.bar.min_x + 6.0, _x1 = _l.bar.max_x - 6.0;
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_x0, (f32)_mid }, (rde_vec_2F){ (f32)_x1, (f32)_mid }, _t->outline, 3.0f);
        rde_rendering_2d_draw_line_1((rde_vec_2F){ (f32)_x0, (f32)_mid }, (rde_vec_2F){ (f32)(_x0 + (_x1 - _x0) * _k), (f32)_mid }, _t->accent, 3.0f);
        rde_rendering_2d_draw_circle((rde_vec_2F){ (f32)(_x0 + (_x1 - _x0) * _k), (f32)_mid }, 7.0f, 20u, _t->accent, NULL);
        c8 _at[24], _all[24], _times[64];
        const u32 _rate = _open ? _page->player.rate : FUDE_ZOOM_AUDIO_RATE;
        fude_zoom_page_audio_time((u64)(_k * (f64)_frames) * 1000u / _rate, _at, sizeof(_at));
        fude_zoom_page_audio_time((u64)_frames * 1000u / _rate, _all, sizeof(_all));
        rde_snprintf(_times, sizeof(_times), "%s / %s", _at, _all);
        fude_zoom_render_text_in(&_page->renderer, _times, (u32)rde_strlen(_times), (fude_zoom_box){ _l.bar.min_x, _l.bar.min_y - 4.0, _l.bar.max_x, _mid - 6.0 }, 12.0, false, _t->text_soft);
    }
}

// A press on the panel: taken (true) when it is inside it. A press on the bar while something plays starts dragging
// it; any other press is acted on when it lifts.
b8 fude_zoom_page_audio_down(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand) {
    if(!_page->audio_panel || _page->presenting || _page->audio_hand != 0u) {
        return false;
    }
    fude_zoom_page_audio_layout _l;
    fude_zoom_page_audio_lay(_page, &_l);
    if(!fude_zoom_page_box_holds(_l.panel, _at)) {
        return false;
    }
    _page->audio_hand      = _hand;
    _page->audio_down      = _at;
    _page->audio_scrubbing = fude_zoom_page_box_holds(_l.bar, _at) && _page->audio_playing != 0u;
    if(_page->audio_scrubbing) {
        _page->audio_scrub = (f32)rde_math_min_f64(rde_math_max_f64(((f64)_at.x - _l.bar.min_x - 6.0) / rde_math_max_f64(_l.bar.max_x - _l.bar.min_x - 12.0, 1.0), 0.0), 1.0);
    }
    return true;
}

// A move of the hand that pressed the panel: the bar dragged, or the rows scrolled. Returns true when it was that hand.
b8 fude_zoom_page_audio_moved(fude_zoom_page* _page, rde_vec_2F _at, u64 _hand) {
    if(_page->audio_hand != _hand || _hand == 0u) {
        return false;
    }
    fude_zoom_page_audio_layout _l;
    fude_zoom_page_audio_lay(_page, &_l);
    if(_page->audio_scrubbing) {
        _page->audio_scrub = (f32)rde_math_min_f64(rde_math_max_f64(((f64)_at.x - _l.bar.min_x - 6.0) / rde_math_max_f64(_l.bar.max_x - _l.bar.min_x - 12.0, 1.0), 0.0), 1.0);
    } else if(fude_zoom_page_box_holds(_l.rows, _page->audio_down)) {
        _page->audio_scroll = rde_math_min_f64(rde_math_max_f64(_page->audio_scroll + (f64)(_at.y - _page->audio_down.y), 0.0), fude_zoom_page_audio_most(_page, &_l));
        _page->audio_down.y = _at.y;
    }
    return true;
}

// The hand that pressed the panel lifted: the bar let go seeks there; otherwise what is at where it went down acts:
// Live text, close, play or pause (the newest when none is open), −15 s or +15 s, Replay ink, or a row's chip or the
// row itself (which plays it). A Delete or a download asked for is lit until the next lift, which clears it unless it
// is the same press again. Returns true when it was that hand.
b8 fude_zoom_page_audio_up(fude_zoom_page* _page, u64 _hand) {
    if(_page->audio_hand != _hand || _hand == 0u) {
        return false;
    }
    _page->audio_hand = 0u;
    fude_zoom_page_audio_layout _l;
    fude_zoom_page_audio_lay(_page, &_l);
    if(_page->audio_scrubbing) {
        _page->audio_scrubbing = false;
        fude_zoom_voice_seek(&_page->player, (u32)((f64)_page->audio_scrub * (f64)_page->player.frames));
        _page->audio_faint_at = FUDE_ZOOM_NONE;
        return true;
    }
    const rde_vec_2F _at = _page->audio_down;
    const u32 _armed = _page->audio_armed;
    const u32 _ask   = _page->heard_ask;
    _page->audio_armed = 0u;
    _page->heard_ask   = 0u;
    if(fude_listen_built() && fude_zoom_page_box_holds(_l.live, _at)) {
        // Live text: on or off (its model asked for first, then downloaded).
        if(!fude_listen_model_ready() && !_page->heard_downloading) {
            if(_ask != FUDE_ZOOM_NONE) {
                _page->heard_ask = FUDE_ZOOM_NONE;
                c8 _say[300];
                FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_LISTEN_ABOUT, FUDE_TN(FUDE_ZOOM_PAGE_LISTEN_MB));
                fude_notice_show(_say);
                return true;
            }
            fude_zoom_page_listen_download(_page);
            _page->heard_live = true;
        } else {
            _page->heard_live = !_page->heard_live;
        }
        fude_zoom_page_listen_setting(_page, true);
        return true;
    }
    if(fude_zoom_page_box_holds(_l.close, _at)) {
        _page->audio_panel = false;
        fude_zoom_page_audio_stop_playing(_page);
        return true;
    }
    const b8 _open = _page->audio_playing != 0u && fude_zoom_voice_is_open(&_page->player);
    if(fude_zoom_page_box_holds(_l.play, _at)) {
        if(!_open) {
            // None chosen: the newest.
            const u32 _n = (u32)rde_arr_length(&_page->audio.recordings);
            if(_n > 0u) {
                fude_zoom_page_audio_load(_page, ((const fude_zoom_recording*)_page->audio.recordings.memory)[_n - 1u].number, true);
            }
        } else if(_page->player.playing) {
            fude_zoom_voice_pause(&_page->player);
        } else {
            fude_zoom_voice_play(&_page->player);
        }
        return true;
    }
    if(_open && (fude_zoom_page_box_holds(_l.back, _at) || fude_zoom_page_box_holds(_l.ahead, _at))) {
        const i64 _step = (i64)_page->player.rate * 15 * (fude_zoom_page_box_holds(_l.back, _at) ? -1 : 1);
        const i64 _to   = (i64)fude_zoom_voice_position(&_page->player) + _step;
        fude_zoom_voice_seek(&_page->player, _to < 0 ? 0u : (u32)_to);
        _page->audio_faint_at = FUDE_ZOOM_NONE;
        return true;
    }
    if(fude_zoom_page_box_holds(_l.replay, _at)) {
        _page->audio_replay   = !_page->audio_replay;
        _page->audio_faint_at = FUDE_ZOOM_NONE;
        return true;
    }
    if(!fude_zoom_page_box_holds(_l.rows, _at)) {
        return true;
    }
    const u32 _n = (u32)rde_arr_length(&_page->audio.recordings);
    const u32 _k = (u32)rde_math_floor_f64(((_l.rows.max_y - (f64)_at.y) + _page->audio_scroll) / FUDE_ZOOM_PAGE_AUDIO_ROW);
    if(_k >= _n) {
        return true;
    }
    const fude_zoom_recording _rec = ((const fude_zoom_recording*)_page->audio.recordings.memory)[_n - 1u - _k];
    const u32 _number = _rec.number;
    const f64 _top = _l.rows.max_y - (f64)_k * FUDE_ZOOM_PAGE_AUDIO_ROW + _page->audio_scroll;
    const fude_zoom_box _row = { _l.rows.min_x, _top - FUDE_ZOOM_PAGE_AUDIO_ROW + 4.0, _l.rows.max_x, _top - 2.0 };
    fude_zoom_page_row_chip _chips[3];
    _page->audio_armed = _armed;   // As they were drawn: the chips are worked out again.
    _page->heard_ask   = _ask;
    const u32 _nc = fude_zoom_page_audio_row_chips(_page, &_rec, _row, _chips);
    _page->audio_armed = 0u;
    _page->heard_ask   = 0u;
    for(u32 _c = 0; _c < _nc; _c++) {
        if(!fude_zoom_page_box_holds(_chips[_c].box, _at)) {
            continue;
        }
        switch(_chips[_c].kind) {
            case FUDE_ZOOM_PAGE_ROW_DELETE: {
                // Its Delete: asked once, then gone (its file and its words too).
                if(_armed != _number) {
                    _page->audio_armed = _number;
                    return true;
                }
                if(_page->audio_playing == _number) {
                    fude_zoom_page_audio_stop_playing(_page);
                }
                c8 _path[RDE_MAX_PATH];
                fude_zoom_page_audio_rec_path(_page->canvas, _number, _path, sizeof(_path));
                rde_file_delete(_path);
                fude_zoom_page_heard_path(_page->canvas, _number, _path, sizeof(_path));
                if(rde_file_exists(_path)) {
                    rde_file_delete(_path);
                }
                fude_zoom_page_heard* _h = fude_zoom_page_heard_of(_page, _number, false);
                if(_h != NULL) {
                    _h->wanted = false;
                    fude_zoom_transcript_clear(&_h->t);
                }
                fude_zoom_audio_remove(&_page->audio, _number);
                fude_zoom_page_audio_keep(_page);
                fude_notice_show(fude_text(FUDE_TEXT_ZOOM_AUDIO_DELETED));
                return true;
            }
            case FUDE_ZOOM_PAGE_ROW_TRANSCRIBE: {
                // Transcribe: read from where it was left (the model asked for first, then downloaded).
                if(!fude_listen_model_ready() && !_page->heard_downloading) {
                    if(_ask != _number) {
                        _page->heard_ask = _number;
                        c8 _say[300];
                        FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_LISTEN_ABOUT, FUDE_TN(FUDE_ZOOM_PAGE_LISTEN_MB));
                        fude_notice_show(_say);
                        return true;
                    }
                    fude_zoom_page_listen_download(_page);
                }
                fude_zoom_page_heard_of(_page, _number, true)->wanted = true;
                return true;
            }
            case FUDE_ZOOM_PAGE_ROW_AS_TEXT:
                fude_zoom_page_listen_as_text(_page, _number);
                return true;
            case FUDE_ZOOM_PAGE_ROW_AS_HAND:
                fude_zoom_page_listen_as_hand(_page, _number);
                return true;
            default:
                return true;   // How far it is: nothing to press.
        }
    }
    fude_zoom_page_audio_load(_page, _number, true);
    return true;
}

// The microphone's chip: fude_zoom_page_audio_toggle. A second press within 0.35 s is the same tap.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_mic(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    const f64       _now  = rde_engine_get_time_now();
    if(_page == NULL || !_page->open || _now - _page->mic_pressed_at < 0.35) {
        return RDE_UI_EVENT_RESULT_DEFAULT;   // A tablet's tap comes twice.
    }
    _page->mic_pressed_at = _now;
    fude_zoom_page_audio_toggle(_page);
    return RDE_UI_EVENT_RESULT_DEFAULT;
}

// The recordings' chip: the panel up (the pages' strip, Find's canvases and the map put away) or put away (what was
// playing stopped). A second press within 0.35 s is the same tap.
RDE_UI_EVENT_RESULT_ fude_zoom_page_on_listen(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info); RDE_UNUSED(_user_data);
    fude_zoom_page* _page = fude_zoom_page_the;
    const f64       _now  = rde_engine_get_time_now();
    if(_page == NULL || !_page->open || _now - _page->listen_pressed_at < 0.35) {
        return RDE_UI_EVENT_RESULT_DEFAULT;
    }
    _page->listen_pressed_at = _now;
    _page->audio_panel = !_page->audio_panel;
    if(_page->audio_panel) {
        _page->strip_open       = false;
        _page->everywhere_shown = false;
        _page->map_open         = false;
    } else {
        fude_zoom_page_audio_stop_playing(_page);
    }
    return RDE_UI_EVENT_RESULT_DEFAULT;
}
