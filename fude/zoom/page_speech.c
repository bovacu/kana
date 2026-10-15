// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, speech to text (0.1.95, listen.h): a recording's transcript read on the device, Live text, and the
// transcript written out as text or as handwriting.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// --- speech to text (0.1.95, listen.h) ------------------------------------------------------------------------
//
// A recording's Transcribe reads it on the device, a piece of up to 28 s at a time (cut where it is quietest), each
// piece's phrases kept as they come (<canvas>.rec<n>.text); then As text or As handwriting writes them out in view,
// each line linked to when its words were said (Replay ink shows them come as they were spoken). With Live text the
// recording being made is read as it goes, a few seconds behind, its last words shown over the bottom row. The model
// is downloaded the first time (asked first: its size).

#define FUDE_ZOOM_PAGE_LISTEN_LIVE_MIN  (FUDE_LISTEN_RATE * 10u)   // Live text: a piece once this much more was said
#define FUDE_ZOOM_PAGE_LISTEN_CUT_MS    3000u                       // a piece cut at its quietest in its last 3 s
#define FUDE_ZOOM_PAGE_LISTEN_PAUSE_MS  2000u                       // a pause this long: a new paragraph
#define FUDE_ZOOM_PAGE_LISTEN_CAPTION   8.0                         // seconds the live caption's last words stay

// See page_parts.h: <id>.rec<n>.text, beside the canvas.
void fude_zoom_page_heard_path(u32 _canvas, u32 _number, c8* _out, usize _size) {
    c8 _ext[32];
    rde_snprintf(_ext, sizeof(_ext), "rec%u.text", _number);
    fude_zoom_page_side_path(_canvas, _ext, _out, _size);
}

// See page_parts.h. A transcript begun and not finished is wanted, so it is read on to its end. The pointer is into
// the page's list and stays valid until another transcript is added to it.
fude_zoom_page_heard* fude_zoom_page_heard_of(fude_zoom_page* _page, u32 _number, b8 _make) {
    fude_zoom_page_heard* _h = (fude_zoom_page_heard*)_page->heard.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->heard); _i++) {
        if(_h[_i].number == _number) {
            return &_h[_i];
        }
    }
    c8 _path[RDE_MAX_PATH];
    fude_zoom_page_heard_path(_page->canvas, _number, _path, sizeof(_path));
    if(!_make && !rde_file_exists(_path)) {
        return NULL;
    }
    fude_zoom_page_heard _new;
    rde_memset(&_new, 0, sizeof(_new));
    _new.number = _number;
    fude_zoom_transcript_init(&_new.t);
    fude_zoom_transcript_load(&_new.t, _path);
    _new.wanted = !_new.t.complete && rde_arr_length(&_new.t.segments) > 0u;   // (one begun: read on to its end)
    rde_arr_add(&_page->heard, (any)&_new);
    return &((fude_zoom_page_heard*)_page->heard.memory)[rde_arr_length(&_page->heard) - 1u];
}

// Writes a transcript to its file.
RDE_INTERNAL void fude_zoom_page_heard_keep(fude_zoom_page* _page, const fude_zoom_page_heard* _h) {
    c8 _path[RDE_MAX_PATH];
    fude_zoom_page_heard_path(_page->canvas, _h->number, _path, sizeof(_path));
    fude_zoom_transcript_save(&_h->t, _path);
}

// See page_parts.h. A piece being read for the canvas left is let go when it comes (fude_zoom_page_listen_update).
void fude_zoom_page_heard_forget(fude_zoom_page* _page) {
    fude_zoom_page_heard* _h = (fude_zoom_page_heard*)_page->heard.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->heard); _i++) {
        fude_zoom_transcript_free(&_h[_i].t);
    }
    rde_arr_clear(&_page->heard);
    _page->heard_reading = 0u;
    _page->heard_ask     = 0u;
}

// Recording _number's samples _from on, _n of them, into _out (decoded from its file). How many there were.
RDE_INTERNAL u32 fude_zoom_page_listen_read(fude_zoom_page* _page, u32 _number, u32 _from, u32 _n, f32* _out) {
    c8 _path[RDE_MAX_PATH];
    fude_zoom_page_audio_rec_path(_page->canvas, _number, _path, sizeof(_path));
    rde_file* _f = NULL;
    RDE_TRY({ _f = rde_file_open(_path, RDE_FILE_MODE_READ_BYTES); });
    if(_f == NULL) {
        return 0u;
    }
    u8  _block[FUDE_ZOOM_ADPCM_BLOCK];
    i16 _pcm[FUDE_ZOOM_ADPCM_FRAMES];
    u32 _got = 0;
    while(_got < _n) {
        const u32 _at = _from + _got, _k = _at / FUDE_ZOOM_ADPCM_FRAMES, _in = _at % FUDE_ZOOM_ADPCM_FRAMES;
        if(rde_file_read_at(_f, (u64)_k * FUDE_ZOOM_ADPCM_BLOCK, _block, FUDE_ZOOM_ADPCM_BLOCK) != FUDE_ZOOM_ADPCM_BLOCK) {
            break;
        }
        fude_zoom_adpcm_decode(_block, _pcm);
        for(u32 _i = _in; _i < FUDE_ZOOM_ADPCM_FRAMES && _got < _n; _i++) {
            _out[_got++] = (f32)_pcm[_i] / 32768.0f;
        }
    }
    rde_file_close(_f);
    return _got;
}

// See page_parts.h: one byte, 1 for on, in <save folder>/listen.kana; a missing file reads as off.
void fude_zoom_page_listen_setting(fude_zoom_page* _page, b8 _save) {
    c8 _path[RDE_MAX_PATH];
    rde_snprintf(_path, sizeof(_path), "%slisten.kana", fude_save_dir());
    if(_save) {
        fude_bytes _b = fude_bytes_new(16u);
        fude_put_u8(&_b, _page->heard_live ? 1u : 0u);
        fude_bytes_write_and_free(&_b, _path, NULL);
        return;
    }
    u32 _size = 0;
    u8* _data = rde_file_exists(_path) ? fude_file_read(_path, &_size) : NULL;
    _page->heard_live = _data != NULL && _size >= 1u && _data[0] == 1u;
    if(_data != NULL) {
        fude_file_free(_data);
    }
}

// See page_parts.h. A download that cannot start is said in a notice.
void fude_zoom_page_listen_download(fude_zoom_page* _page) {
    c8 _path[RDE_MAX_PATH];
    fude_listen_model_path(_path, sizeof(_path));
    _page->heard_downloading = fude_fetch_start(FUDE_LISTEN_MODEL_URL, _path);
    if(!_page->heard_downloading) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_LISTEN_FAILED));
    }
}

// See page_parts.h. Once a frame: the download followed; a piece read taken in (kept); the next piece given to it —
// the recording being made first (Live text), then any whose Transcribe was pressed. One piece is read at a time:
// heard_reading is the recording it is from, and heard_piece_end where it ends.
void fude_zoom_page_listen_update(fude_zoom_page* _page) {
    if(!fude_listen_built() || _page->canvas == 0u) {
        return;
    }
    if(_page->heard_downloading) {
        u64 _got = 0, _total = 0;
        const FUDE_FETCH_ _state = fude_fetch_state(&_got, &_total);
        if(_state == FUDE_FETCH_DONE || _state == FUDE_FETCH_FAILED || _state == FUDE_FETCH_IDLE) {
            _page->heard_downloading = false;
            if(_state != FUDE_FETCH_DONE || !fude_listen_model_ready()) {
                fude_notice_show(fude_text(FUDE_TEXT_ZOOM_LISTEN_FAILED));
            }
        }
        return;
    }
    // A piece read: its phrases kept.
    u32 _tag = 0;
    u8  _failed = FUDE_LISTEN_OK;
    c8  _lang[8];
    if(_page->heard_reading == 0u && fude_listen_busy()) {
        // (one asked for by a canvas left since: taken, and let go)
        fude_zoom_transcript _lost;
        fude_zoom_transcript_init(&_lost);
        fude_listen_take(&_tag, &_lost, NULL, 0u, &_failed);
        fude_zoom_transcript_free(&_lost);
        return;
    }
    fude_zoom_page_heard* _h = _page->heard_reading != 0u ? fude_zoom_page_heard_of(_page, _page->heard_reading, true) : NULL;
    if(_h != NULL) {
        const u32 _before = (u32)rde_arr_length(&_h->t.segments);
        if(fude_listen_take(&_tag, &_h->t, _lang, sizeof(_lang), &_failed)) {
            _page->heard_reading = 0u;
            if(_failed == FUDE_LISTEN_DAMAGED) {
                // (the model there but not read: let go, to be downloaded again)
                c8 _path[RDE_MAX_PATH];
                fude_listen_model_path(_path, sizeof(_path));
                rde_file_delete(_path);
                fude_listen_shutdown();
                _h->wanted = false;
                fude_notice_show(fude_text(FUDE_TEXT_ZOOM_LISTEN_DAMAGED));
                return;
            }
            if(_failed == FUDE_LISTEN_CANNOT) {
                _h->wanted = false;   // (not here: the model kept)
                fude_notice_show(fude_text(FUDE_TEXT_ZOOM_LISTEN_CANNOT));
                return;
            }
            _h->t.done = _page->heard_piece_end;
            if(_h->t.language[0] == 0 && _lang[0] != 0) {
                rde_snprintf(_h->t.language, sizeof(_h->t.language), "%s", _lang);   // (the rest read in it)
            }
            const fude_zoom_recording* _r = fude_zoom_audio_find(&_page->audio, _h->number);
            if(_r != NULL && _h->number != _page->audio_recording && _h->t.done >= _r->frames) {
                _h->t.complete = true;
                _h->wanted     = false;
            }
            if((u32)rde_arr_length(&_h->t.segments) > _before) {
                _page->heard_said_at = rde_engine_get_time_now();
            }
            fude_zoom_page_heard_keep(_page, _h);
        }
        return;
    }
    if(!fude_listen_model_ready() || fude_listen_busy()) {
        return;
    }
    // The next piece: the recording being made (Live text), else one wanted.
    u32 _number = 0, _available = 0;
    b8  _live = false;
    if(_page->audio_recording != 0u && _page->heard_live) {
        const f64 _now = rde_engine_get_time_now();
        if(_now - _page->heard_checked_at >= 0.5) {
            _page->heard_checked_at = _now;
            c8 _path[RDE_MAX_PATH];
            fude_zoom_page_audio_rec_path(_page->canvas, _page->audio_recording, _path, sizeof(_path));
            _page->heard_available = fude_zoom_audio_frames_in(fude_file_size(_path));
        }
        fude_zoom_page_heard* _live_h = fude_zoom_page_heard_of(_page, _page->audio_recording, true);
        if(_page->heard_available >= _live_h->t.done + FUDE_ZOOM_PAGE_LISTEN_LIVE_MIN) {
            _number    = _page->audio_recording;
            _available = _page->heard_available;
            _live      = true;
        }
    }
    if(_number == 0u) {
        fude_zoom_page_heard* _all = (fude_zoom_page_heard*)_page->heard.memory;
        // (its recording found only for one still wanted: a frame goes through the transcripts once, not through the
        // recordings for each of them)
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_page->heard) && _number == 0u; _i++) {
            if(!_all[_i].wanted || _all[_i].t.complete || _all[_i].number == _page->audio_recording) {
                continue;
            }
            const fude_zoom_recording* _r = fude_zoom_audio_find(&_page->audio, _all[_i].number);
            if(_r == NULL) {
                _all[_i].wanted = false;   // (its recording gone: nothing left to read)
                continue;
            }
            if(_all[_i].t.done >= _r->frames) {
                _all[_i].t.complete = true;   // (read to its end: done)
                _all[_i].wanted     = false;
                fude_zoom_page_heard_keep(_page, &_all[_i]);
                continue;
            }
            _number    = _all[_i].number;
            _available = _r->frames;
        }
    }
    if(_number == 0u) {
        return;
    }
    _h = fude_zoom_page_heard_of(_page, _number, true);
    const u32 _most = FUDE_LISTEN_PIECE_MS * (FUDE_LISTEN_RATE / 1000u);
    const u32 _n    = _available - _h->t.done < _most ? _available - _h->t.done : _most;
    rde_arr _pcm = rde_arr_new(sizeof(f32), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_pcm, _n);
    const u32 _got = fude_zoom_page_listen_read(_page, _number, _h->t.done, _n, (f32*)_pcm.memory);
    // (not its end: cut where it is quietest near the piece's, so no word is cut in two)
    const b8  _last = !_live && _h->t.done + _got >= _available;
    const u32 _cut  = _last ? _got : fude_zoom_transcript_cut((const f32*)_pcm.memory, _got, FUDE_LISTEN_RATE, FUDE_ZOOM_PAGE_LISTEN_CUT_MS);
    c8 _prompt[256];
    fude_zoom_transcript_tail(&_h->t, 200u, _prompt, sizeof(_prompt));
    if(_cut > 0u && fude_listen_submit(_number, (const f32*)_pcm.memory, _cut, (u32)((u64)_h->t.done * 1000u / FUDE_LISTEN_RATE), _h->t.language, _prompt,
                                       _live ? 2u : 3u)) {
        _page->heard_reading   = _number;
        _page->heard_piece_end = _h->t.done + _cut;
    } else if(_got == 0u) {
        _h->t.complete = true;   // (its file shorter than said: done where it ends)
        _h->wanted     = false;
        fude_zoom_page_heard_keep(_page, _h);
    }
    rde_arr_free(&_pcm);
}

// Where what is written out goes (screen points, centre origin: its top left): in view, under what is already there
// in view (the view's own frame's), else at the view's top when there is no room under it.
RDE_INTERNAL rde_vec_2F fude_zoom_page_listen_spot(fude_zoom_page* _page, f32 _w) {
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    const f32 _x   = rde_math_max_f32(-(f32)_half.x + 40.0f, -_w * 0.5f);
    f32       _top = (f32)_half.y - 110.0f;
    fude_zoom_scene* _s = &_page->scene;
    const fude_zoom_sim _to = fude_zoom_camera_sim(&_s->camera);
    const fude_zoom_box _view = fude_zoom_sim_box(fude_zoom_sim_inverse(_to), (fude_zoom_box){ -_half.x, -_half.y, _half.x, _half.y });
    rde_arr _found = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_scene_query(_s, _s->camera.frame, _view, &_found);
    f64 _lowest = 1e300;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_found); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, ((const u32*)_found.memory)[_i]);
        if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind != FUDE_ZOOM_KIND_MARK && _o->kind != FUDE_ZOOM_KIND_LAYER && _o->kind != FUDE_ZOOM_KIND_FRAME &&
           !fude_zoom_scene_hides(_s, _o)) {
            _lowest = rde_math_min_f64(_lowest, fude_zoom_sim_apply(_to, (fude_zoom_v2){ 0.0, _o->box.min_y }).y);
        }
    }
    rde_arr_free(&_found);
    if(_lowest < 1e299 && _lowest - 40.0 > -_half.y * 0.4) {
        _top = (f32)rde_math_min_f64((f64)_top, _lowest - 40.0);   // (under what is there, with room to write)
    }
    return (rde_vec_2F){ _x, _top };
}

// As text: recording _number's transcript as one text in view, in the pen's colour, linked to when it began. One undo
// step.
void fude_zoom_page_listen_as_text(fude_zoom_page* _page, u32 _number) {
    fude_zoom_page_heard* _h = fude_zoom_page_heard_of(_page, _number, false);
    if(_h == NULL || rde_arr_length(&_h->t.segments) == 0u) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_LISTEN_NONE));
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _out = rde_arr_new(sizeof(c8), _heap), _lines = rde_arr_new(sizeof(fude_zoom_transcript_line), _heap);
    fude_zoom_transcript_lines(&_h->t, 48u, FUDE_ZOOM_PAGE_LISTEN_PAUSE_MS, &_out, &_lines);
    const c8* _text = (const c8*)_out.memory;
    fude_zoom_scene* _s = &_page->scene;
    const f32 _px = 18.0f;
    f32 _w = 0.0f, _hh = 0.0f;
    fude_zoom_page_measure_words(_page, _text, _px, &_w, &_hh);
    const f64 _k  = _s->camera.z;
    const fude_zoom_v2 _tl = fude_zoom_sim_apply(fude_zoom_sim_inverse(fude_zoom_camera_sim(&_s->camera)), (fude_zoom_v2){ fude_zoom_page_listen_spot(_page, _w).x,
                                                                                                                          fude_zoom_page_listen_spot(_page, _w).y });
    const u32 _o = fude_zoom_scene_add_text(_s, _s->camera.frame, (fude_zoom_place){ _tl, 0.0, 1.0 }, (f64)_px / _k, (f64)_w / _k, (f64)_hh / _k, FUDE_ZOOM_TEXT_PLAIN,
                                            _page->app->ink->color, (rde_color){ 0, 0, 0, 0 }, _text, (u32)rde_strlen(_text), 0);
    fude_zoom_history_push(_s, _s->camera.frame, fude_zoom_scene_object(_s, _o)->box, NULL, 0u, &_o, 1u);
    fude_zoom_audio_add_link(&_page->audio, fude_zoom_scene_object(_s, _o)->id, _number, ((const fude_zoom_transcript_segment*)_h->t.segments.memory)[0].t0_ms);
    fude_zoom_page_audio_keep(_page);
    rde_arr_free(&_out);
    rde_arr_free(&_lines);
}

// As handwriting: its lines written out in view in the pen's colour (hand.h), each line's strokes linked to when its
// words were said. One undo step.
void fude_zoom_page_listen_as_hand(fude_zoom_page* _page, u32 _number) {
    fude_zoom_page_heard* _h = fude_zoom_page_heard_of(_page, _number, false);
    if(_h == NULL || rde_arr_length(&_h->t.segments) == 0u) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_LISTEN_NONE));
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _out = rde_arr_new(sizeof(c8), _heap), _lines = rde_arr_new(sizeof(fude_zoom_transcript_line), _heap);
    rde_arr _pts = rde_arr_new(sizeof(fude_zoom_v2), _heap), _ends = rde_arr_new(sizeof(u32), _heap), _path = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr _born = rde_arr_new(sizeof(u32), _heap);
    const u32 _n = fude_zoom_transcript_lines(&_h->t, 36u, FUDE_ZOOM_PAGE_LISTEN_PAUSE_MS, &_out, &_lines);
    const c8* _all = (const c8*)_out.memory;
    const fude_zoom_transcript_line* _l = (const fude_zoom_transcript_line*)_lines.memory;
    fude_zoom_scene* _s = &_page->scene;
    const f64 _k   = _s->camera.z;
    const f64 _cap = 16.0 / _k;   // (a capital 16 points tall on the screen as it is)
    const fude_zoom_v2 _tl = fude_zoom_sim_apply(fude_zoom_sim_inverse(fude_zoom_camera_sim(&_s->camera)), (fude_zoom_v2){ fude_zoom_page_listen_spot(_page, 520.0f).x,
                                                                                                                          fude_zoom_page_listen_spot(_page, 520.0f).y });
    c8 _one[512];
    for(u32 _i = 0; _i < _n; _i++) {
        if(_l[_i].len == 0u) {
            continue;   // (a paragraph's empty line)
        }
        const u32 _len = _l[_i].len < sizeof(_one) - 1u ? _l[_i].len : (u32)sizeof(_one) - 1u;
        rde_memcpy(_one, _all + _l[_i].at, _len);
        _one[_len] = 0;
        rde_arr_clear(&_pts);
        rde_arr_clear(&_ends);
        fude_zoom_hand_write(_one, _cap, _number * 131u + _i, &_pts, &_ends);
        const fude_zoom_v2* _p = (const fude_zoom_v2*)_pts.memory;
        const u32*          _e = (const u32*)_ends.memory;
        const u32 _was = (u32)rde_arr_length(&_born);
        for(u32 _k2 = 0, _from = 0; _k2 < (u32)rde_arr_length(&_ends); _from = _e[_k2], _k2++) {
            rde_arr_clear(&_path);
            for(u32 _j = _from; _j < _e[_k2]; _j++) {
                const fude_zoom_v2 _f = { _tl.x + _p[_j].x, _tl.y - _cap * 1.4 - (f64)_i * _cap * 1.9 + _p[_j].y };
                rde_arr_add(&_path, (any)&_f);
            }
            fude_zoom_page_add_path(_page, _s->camera.frame, (const fude_zoom_v2*)_path.memory, (u32)rde_arr_length(&_path), false, _page->app->ink->color,
                                    (f32)(_cap * 0.066), &_born);
        }
        // (this line's strokes: when its words were said)
        for(u32 _b = _was; _b < (u32)rde_arr_length(&_born); _b++) {
            fude_zoom_audio_add_link(&_page->audio, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_b])->id, _number, _l[_i].t0_ms);
        }
    }
    if(rde_arr_length(&_born) > 0u) {
        fude_zoom_box _box = fude_zoom_box_empty();
        for(u32 _b = 0; _b < (u32)rde_arr_length(&_born); _b++) {
            _box = fude_zoom_box_union(_box, fude_zoom_scene_object(_s, ((const u32*)_born.memory)[_b])->box);
        }
        fude_zoom_history_push(_s, _s->camera.frame, _box, NULL, 0u, (const u32*)_born.memory, (u32)rde_arr_length(&_born));
        fude_zoom_page_audio_keep(_page);
    }
    rde_arr_free(&_out);
    rde_arr_free(&_lines);
    rde_arr_free(&_pts);
    rde_arr_free(&_ends);
    rde_arr_free(&_path);
    rde_arr_free(&_born);
}

// The live caption: while Live text reads the recording being made, its last words over the bottom row a while.
void fude_zoom_page_render_caption(fude_zoom_page* _page) {
    if(_page->audio_recording == 0u || !_page->heard_live || _page->presenting || !_page->open ||
       rde_engine_get_time_now() - _page->heard_said_at > FUDE_ZOOM_PAGE_LISTEN_CAPTION) {
        return;
    }
    const fude_zoom_page_heard* _h = fude_zoom_page_heard_of(_page, _page->audio_recording, false);
    const u32 _n = _h != NULL ? (u32)rde_arr_length(&_h->t.segments) : 0u;
    if(_n == 0u) {
        return;
    }
    const fude_zoom_transcript_segment _last = ((const fude_zoom_transcript_segment*)_h->t.segments.memory)[_n - 1u];
    c8 _words[400];
    const u32 _len = _last.len < sizeof(_words) - 1u ? _last.len : (u32)sizeof(_words) - 1u;
    rde_memcpy(_words, (const c8*)_h->t.text.memory + _last.at, _len);
    _words[_len] = 0;
    const fude_zoom_v2 _half = fude_zoom_page_half(_page);
    const f64 _w = rde_math_min_f64(_half.x * 1.4, 760.0);
    const f64 _y = -_half.y + (f64)fude_zoom_page_notice_above(_page) + 70.0;
    const fude_zoom_box _b = { -_w * 0.5, _y - 30.0, _w * 0.5, _y + 30.0 };
    const fude_theme* _t = fude_theme_active();
    rde_color _bg = _t->surface;
    _bg.a = 235;
    rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ 0.0f, (f32)_y }, (rde_vec_2F){ (f32)_w, 60.0f }, 0.3f, 8u, _bg, 1.0f, _t->outline, NULL);
    fude_zoom_render_text_in(&_page->renderer, _words, (u32)rde_strlen(_words), (fude_zoom_box){ _b.min_x + 16.0, _b.min_y + 4.0, _b.max_x - 16.0, _b.max_y - 4.0 }, 16.0,
                             false, _t->text);
}
