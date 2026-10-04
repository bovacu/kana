// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/widgets/header.h"
#include "lang/lang.h"
#include "study/screens/check.h"
#include "drawing/base/text.h"
#include "drawing/widgets/draw.h"
#include "drawing/base/theme.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See check.h.
// ===========================================================================

#define FUDE_CHECK_MARGIN   16.0f
#define FUDE_CHECK_BOX_MAX  300.0f   // the writing's square
#define FUDE_CHECK_CELL     96.0f    // a candidate
#define FUDE_CHECK_FILL     0.8f     // the writing and the model fill this much of the square
#define FUDE_CHECK_STRIP    150.0f   // the whole writing, at most this tall
#define FUDE_CHECK_READ     56.0f    // a character of the reading
#define FUDE_CHECK_READ_GAP 6.0f
#define FUDE_CHECK_ROWS     3u       // rows of the reading shown, at most

void fude_check_init(fude_check* _check, const fude_kanji_db* _db, const fude_catalog* _catalog) {
    memset(_check, 0, sizeof(*_check));
    _check->db      = _db;
    _check->catalog = _catalog;
    fude_glyph_init(&_check->glyph, _db);
    fude_ink_init(&_check->drawing);
    fude_segment_init(&_check->segment);
    _check->chars = rde_arr_new(sizeof(fude_check_char), rde_memory_allocator_get_default());
    _check->hits  = rde_arr_new(sizeof(fude_check_hit), rde_memory_allocator_get_default());
}

void fude_check_destroy(fude_check* _check) {
    rde_arr_free(&_check->hits);
    rde_arr_free(&_check->chars);
    fude_segment_destroy(&_check->segment);
    fude_ink_destroy(&_check->drawing);
    fude_glyph_destroy(&_check->glyph);
    memset(_check, 0, sizeof(*_check));
}

RDE_INTERNAL int fude_check_by_index(const void* _a, const void* _b) {
    const u32 _x = *(const u32*)_a;
    const u32 _y = *(const u32*)_b;
    return _x < _y ? -1 : _x > _y ? 1 : 0;
}

RDE_INTERNAL fude_check_char* fude_check_at(const fude_check* _check, u32 _index) {
    return _index < (u32)rde_arr_length(&_check->chars) ? &((fude_check_char*)_check->chars.memory)[_index] : NULL;
}

b8 fude_check_open(fude_check* _check, const fude_ink* _ink, const u32* _ids, u32 _count) {
    if(_count == 0 || _check->db == NULL || _check->catalog == NULL) {
        return false;
    }

    // In writing order: strokes are kept in the order they were written, so by index.
    u32* _order = (u32*)rde_malloc(sizeof(u32) * _count);
    memcpy(_order, _ids, sizeof(u32) * _count);
    qsort(_order, _count, sizeof(u32), fude_check_by_index);

    fude_ink_destroy(&_check->drawing);
    fude_ink_init(&_check->drawing);
    _check->bounds_min = (rde_vec_2F){ 1e30f, 1e30f };
    _check->bounds_max = (rde_vec_2F){ -1e30f, -1e30f };
    for(u32 _i = 0; _i < _count; _i++) {
        if(_order[_i] >= fude_ink_stroke_count(_ink)) {
            continue;
        }
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _order[_i]);
        if(!_stroke->alive || _stroke->point_count == 0 || _stroke->marker) {   // the marker's: not writing
            continue;
        }
        fude_ink_add_loaded_stroke(&_check->drawing, fude_ink_stroke_points(_ink, _stroke), _stroke->point_count, _stroke->color, _stroke->from_pen);
        _check->bounds_min = (rde_vec_2F){ fminf(_check->bounds_min.x, _stroke->bounds_min.x), fminf(_check->bounds_min.y, _stroke->bounds_min.y) };
        _check->bounds_max = (rde_vec_2F){ fmaxf(_check->bounds_max.x, _stroke->bounds_max.x), fmaxf(_check->bounds_max.y, _stroke->bounds_max.y) };
    }
    rde_free(_order);
    if(fude_ink_alive_strokes(&_check->drawing) == 0) {
        return false;
    }

    rde_arr_clear(&_check->chars);
    _check->selected      = 0;
    _check->meant_count   = 0;
    _check->meant_failed  = false;
    _check->pending       = true;
    _check->_drawn        = false;
    _check->open          = true;
    _check->mlkit_text[0] = 0;
    _check->by_mlkit      = false;
    _check->_asked_mlkit  = false;
    fude_recognize_forget(&_check->recognition);   // an answer still to come was for something else
    return true;
}

void fude_check_close(fude_check* _check) {
    _check->open = false;
    fude_scroller_stop(&_check->taps);
}

// Scores a character's strokes against its chosen candidate.
RDE_INTERNAL void fude_check_score(fude_check* _check, fude_check_char* _c) {
    fude_kanji_info _info;
    memset(&_c->score, 0, sizeof(_c->score));
    _c->score.empty = true;
    if(_c->chosen < 0 || !fude_kanji_at(_check->db, _c->candidates[_c->chosen].record, &_info)) {
        return;
    }
    fude_ink _one;
    fude_ink_init(&_one);
    for(u32 _s = _c->first; _s < _c->first + _c->count && _s < fude_ink_stroke_count(&_check->drawing); _s++) {
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(&_check->drawing, _s);
        fude_ink_add_loaded_stroke(&_one, fude_ink_stroke_points(&_check->drawing, _stroke), _stroke->point_count, _stroke->color, _stroke->from_pen);
    }
    _c->score = fude_score_drawing(_check->db, &_info, &_one);
    fude_ink_destroy(&_one);
}

// The reading (segment.h) — as the text meant, if there is one and it fits.
RDE_INTERNAL void fude_check_read(fude_check* _check) {
    rde_arr_clear(&_check->chars);
    _check->selected     = 0;
    _check->meant_failed = false;

    b8 _as = false;
    if(_check->meant_count > 0) {
        _as = fude_segment_read_as(&_check->segment, _check->db, &_check->drawing, FUDE_SEGMENT_AUTO, _check->meant, _check->meant_count);
        _check->meant_failed = !_as;
    }
    if(!_as) {
        fude_segment_read(&_check->segment, _check->db, _check->catalog, &_check->drawing, FUDE_SEGMENT_AUTO);
    }

    if(_check->meant_failed) {
        _check->by_mlkit = false;   // ML Kit's reading did not fit the strokes: Kana's own
    }

    const u32                _n    = (u32)rde_arr_length(&_check->segment.chars);
    const fude_segment_char* _read = (const fude_segment_char*)_check->segment.chars.memory;
    if(_n > 0) {
        fude_check_char* _out = (fude_check_char*)rde_arr_add_n(&_check->chars, _n);
        for(u32 _i = 0; _i < _n; _i++) {
            fude_check_char* _c = &_out[_i];
            memset(_c, 0, sizeof(*_c));
            _c->first           = _read[_i].first;
            _c->count           = _read[_i].count;
            _c->min             = _read[_i].min;
            _c->max             = _read[_i].max;
            _c->candidate_count = _read[_i].candidate_count < FUDE_CHECK_CANDIDATES ? _read[_i].candidate_count : FUDE_CHECK_CANDIDATES;
            memcpy(_c->candidates, _read[_i].candidates, sizeof(fude_match_result) * _c->candidate_count);

            // Read by ML Kit: what its readings have here, then the matcher's
            // ranking of these strokes — the same candidates Browse would give.
            if(_check->by_mlkit) {
                fude_ink _one;
                fude_ink_init(&_one);
                for(u32 _s = _c->first; _s < _c->first + _c->count && _s < fude_ink_stroke_count(&_check->drawing); _s++) {
                    const fude_ink_stroke* _stroke = fude_ink_stroke_at(&_check->drawing, _s);
                    fude_ink_add_loaded_stroke(&_one, fude_ink_stroke_points(&_check->drawing, _stroke), _stroke->point_count, _stroke->color, _stroke->from_pen);
                }
                fude_match_result _matched[FUDE_CHECK_CANDIDATES];
                const u32         _found = fude_match_rank(_check->db, _check->catalog, FUDE_FILTER_ALL, &_one, _matched, FUDE_CHECK_CANDIDATES);
                fude_ink_destroy(&_one);
                _c->candidate_count = fude_recognize_candidates(_check->db, &_check->recognition, _i, _n, NULL, FUDE_FILTER_ALL,
                                                                _matched, _found, _c->candidates, FUDE_CHECK_CANDIDATES);
            }
            _c->chosen = _c->candidate_count > 0 ? 0 : -1;   // the best, until told otherwise
            fude_check_score(_check, _c);
        }
    }
    _check->pending = false;
}

void fude_check_update(fude_check* _check) {
    if(!_check->open || !_check->pending || !_check->_drawn) {
        return;
    }
    // Nothing typed, and ML Kit there: it is asked first, and the selection read
    // as its answer (Kana's own reading when it has none, or it does not fit).
    if(_check->meant_count == 0 && !_check->by_mlkit && fude_recognize_available()) {
        if(!_check->_asked_mlkit) {
            _check->_asked_mlkit = fude_recognize_start(&_check->recognition, &_check->drawing);
            if(_check->_asked_mlkit) {
                return;   // "Reading…" until it answers
            }
        } else {
            if(!fude_recognize_poll(&_check->recognition)) {
                return;
            }
            _check->_asked_mlkit = false;
            if(_check->recognition.line_count > 0) {
                // Its best reading is what the selection is read AS.
                snprintf(_check->mlkit_text, sizeof(_check->mlkit_text), "%s", _check->recognition.lines[0]);
                _check->meant_count = fude_recognize_records(_check->db, _check->recognition.lines[0], _check->meant, FUDE_CHECK_MEANT);
                _check->by_mlkit    = _check->meant_count > 0;
            }
        }
    }
    fude_check_read(_check);
}

void fude_check_read_as(fude_check* _check, const c8* _text) {
    _check->meant_count = 0;
    _check->by_mlkit    = false;
    const c8* _p = _text != NULL ? _text : "";
    while(*_p != 0 && _check->meant_count < FUDE_CHECK_MEANT) {
        // Romaji: a run of letters (and ' -), to hiragana — to katakana when upper case.
        if((*_p >= 'a' && *_p <= 'z') || (*_p >= 'A' && *_p <= 'Z') || *_p == '-' || *_p == '\'') {
            c8  _run[128];
            u32 _len   = 0;
            b8  _upper = false;
            b8  _lower = false;
            while(((*_p >= 'a' && *_p <= 'z') || (*_p >= 'A' && *_p <= 'Z') || *_p == '-' || *_p == '\'') && _len + 1 < sizeof(_run)) {
                _upper = _upper || (*_p >= 'A' && *_p <= 'Z');
                _lower = _lower || (*_p >= 'a' && *_p <= 'z');
                _run[_len++] = *_p++;
            }
            _run[_len] = 0;
            c8 _kana[512];
            fude_lang_reading_from_latin(_run, _kana, sizeof(_kana));
            const c8* _k = _kana;
            for(u32 _cp = fude_utf8_next(&_k); _cp != 0 && _check->meant_count < FUDE_CHECK_MEANT; _cp = fude_utf8_next(&_k)) {
                if(_upper && !_lower && _cp >= 0x3041 && _cp <= 0x3096) {
                    _cp += 0x60u;   // katakana
                }
                u32 _record;
                if(fude_kanji_find_index(_check->db, _cp, &_record)) {
                    _check->meant[_check->meant_count++] = _record;
                }
            }
            continue;
        }
        const u32 _cp = fude_utf8_next(&_p);
        u32       _record;
        if(_cp != 0 && _cp != ' ' && _cp != 0x3000 && fude_kanji_find_index(_check->db, _cp, &_record)) {
            _check->meant[_check->meant_count++] = _record;
        }
    }
    _check->pending = true;
    _check->_drawn  = false;
}

u32 fude_check_count(const fude_check* _check) {
    return _check->pending ? 0u : (u32)rde_arr_length(&_check->chars);
}

void fude_check_select(fude_check* _check, u32 _index) {
    if(_index < fude_check_count(_check)) {
        _check->selected = _index;
    }
}

void fude_check_choose(fude_check* _check, u32 _index) {
    fude_check_char* _c = fude_check_at(_check, _check->selected);
    if(_check->pending || _c == NULL || _index >= _c->candidate_count) {
        return;
    }
    _c->chosen = (i32)_index;
    fude_check_score(_check, _c);
}

b8 fude_check_chosen_record(const fude_check* _check, u32* _record) {
    const fude_check_char* _c = _check->pending ? NULL : fude_check_at(_check, _check->selected);
    if(_c == NULL || _c->chosen < 0 || (u32)_c->chosen >= _c->candidate_count) {
        return false;
    }
    *_record = _c->candidates[_c->chosen].record;
    return true;
}

u32 fude_check_records(const fude_check* _check, u32* _out, u32 _max, b8 _unique) {
    u32 _n = 0;
    for(u32 _i = 0; _i < fude_check_count(_check) && _n < _max; _i++) {
        const fude_check_char* _c = fude_check_at(_check, _i);
        if(_c->chosen < 0) {
            continue;
        }
        const u32 _record = _c->candidates[_c->chosen].record;
        b8        _seen   = false;
        for(u32 _k = 0; _unique && _k < _n && !_seen; _k++) {
            _seen = _out[_k] == _record;
        }
        if(!_seen) {
            _out[_n++] = _record;
        }
    }
    return _n;
}

// --- input -------------------------------------------------------------------------

void fude_check_pointer_down(fude_check* _check, rde_vec_2F _screen, f64 _time) {
    fude_scroller_down(&_check->taps, _screen, _time);
}

void fude_check_pointer_moved(fude_check* _check, rde_vec_2F _screen, f64 _time) {
    fude_scroller_moved(&_check->taps, _screen, _time);
}

void fude_check_pointer_up(fude_check* _check, f64 _time) {
    fude_scroller_up(&_check->taps, _time);
    rde_vec_2F _screen;
    if(!fude_scroller_take_tap(&_check->taps, &_screen) || _check->pending) {
        return;   // not a tap
    }
    const fude_check_char* _c = fude_check_at(_check, _check->selected);
    for(u32 _i = 0; _c != NULL && _i < _c->candidate_count; _i++) {
        const rde_vec_2F _tl = _check->cell_tl[_i];
        if(_screen.x >= _tl.x && _screen.x < _tl.x + _check->cell_size && _screen.y <= _tl.y && _screen.y > _tl.y - _check->cell_size) {
            fude_check_choose(_check, _i);
            return;
        }
    }
    const fude_check_hit* _hits = (const fude_check_hit*)_check->hits.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_check->hits); _i++) {
        if(_screen.x >= _hits[_i].min.x && _screen.x <= _hits[_i].max.x && _screen.y >= _hits[_i].min.y && _screen.y <= _hits[_i].max.y) {
            fude_check_select(_check, _hits[_i].index);
            return;
        }
    }
}

// --- drawing -----------------------------------------------------------------------

// The model, faint, fitted to the square the way the writing is: its own extent
// to FUDE_CHECK_FILL of the square, centred — so the two can be compared.
RDE_INTERNAL void fude_check_draw_model(fude_check* _check, u32 _record, rde_vec_2F _tl, f32 _size) {
    fude_kanji_info _info;
    if(!fude_kanji_at(_check->db, _record, &_info)) {
        return;
    }
    rde_vec_2F _min = { 1e30f, 1e30f };
    rde_vec_2F _max = { -1e30f, -1e30f };
    for(u32 _s = 0; _s < _info.strokes; _s++) {
        fude_kanji_stroke _stroke;
        if(!fude_kanji_stroke_at(_check->db, &_info, _s, &_stroke)) {
            continue;
        }
        rde_vec_2F _pts[FUDE_GLYPH_MAX_POINTS];
        const u32  _n = fude_glyph_stroke_points(&_stroke, _pts, NULL);
        for(u32 _k = 0; _k < _n; _k++) {
            _min = (rde_vec_2F){ fminf(_min.x, _pts[_k].x), fminf(_min.y, _pts[_k].y) };
            _max = (rde_vec_2F){ fmaxf(_max.x, _pts[_k].x), fmaxf(_max.y, _pts[_k].y) };
        }
    }
    const f32 _extent = fmaxf(fmaxf(_max.x - _min.x, _max.y - _min.y), 1.0f);
    const f32 _scale  = _size * FUDE_CHECK_FILL / _extent;          // screen per KanjiVG unit
    const rde_vec_2F _center = { _tl.x + _size * 0.5f, _tl.y - _size * 0.5f };
    // KanjiVG is Y down: screen y = origin.y - y * scale.
    const rde_vec_2F _origin = { _center.x - (_min.x + _max.x) * 0.5f * _scale, _center.y + (_min.y + _max.y) * 0.5f * _scale };
    fude_glyph_character(&_check->glyph, _info.codepoint, _origin, _scale * FUDE_KANJI_BOX, fude_theme_active()->reference);
}

// Strokes _first .. _first + _count - 1 of the drawing, the canvas point _mid at
// screen _center, _zoom screen units per canvas unit, _radius wide.
RDE_INTERNAL void fude_check_draw_strokes(fude_check* _check, u32 _first, u32 _count, rde_vec_2F _mid, rde_vec_2F _center, f32 _zoom, f32 _radius) {
    static rde_vec_2F _pos[4096];
    for(u32 _s = _first; _s < _first + _count && _s < fude_ink_stroke_count(&_check->drawing); _s++) {
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(&_check->drawing, _s);
        if(!_stroke->alive) {
            continue;
        }
        const fude_ink_point* _p = fude_ink_stroke_points(&_check->drawing, _stroke);
        const u32             _n = _stroke->point_count < 4096u ? _stroke->point_count : 4096u;
        for(u32 _k = 0; _k < _n; _k++) {
            _pos[_k] = (rde_vec_2F){ _center.x + (_p[_k].position.x - _mid.x) * _zoom, _center.y + (_p[_k].position.y - _mid.y) * _zoom };
        }
        fude_draw_stroke_even(_pos, _n, _radius, fude_theme_active()->ink);
    }
}

// One character's writing, fitted to the square, drawn with the model's stroke width.
RDE_INTERNAL void fude_check_draw_writing(fude_check* _check, const fude_check_char* _c, rde_vec_2F _tl, f32 _size) {
    const f32        _extent = fmaxf(fmaxf(_c->max.x - _c->min.x, _c->max.y - _c->min.y), 1.0f);
    const rde_vec_2F _center = { _tl.x + _size * 0.5f, _tl.y - _size * 0.5f };
    const rde_vec_2F _mid    = { (_c->min.x + _c->max.x) * 0.5f, (_c->min.y + _c->max.y) * 0.5f };
    fude_draw_keep_begin((rde_vec_2F){ _tl.x, _tl.y - _size }, (rde_vec_2F){ _tl.x + _size, _tl.y }, false);   // as written (draw.h)
    fude_check_draw_strokes(_check, _c->first, _c->count, _mid, _center, _size * FUDE_CHECK_FILL / _extent,
                            fmaxf(1.0f, _size * FUDE_CHECK_FILL * FUDE_GLYPH_WIDTH / FUDE_KANJI_BOX * 0.5f));
    fude_draw_keep_end();
}

RDE_INTERNAL void fude_check_add_hit(fude_check* _check, rde_vec_2F _min, rde_vec_2F _max, u32 _index) {
    fude_check_hit* _hit = (fude_check_hit*)rde_arr_add_n(&_check->hits, 1);
    *_hit = (fude_check_hit){ .min = _min, .max = _max, .index = _index };
}

// The whole writing, fitted to the width and at most FUDE_CHECK_STRIP tall, a
// box around each character read. Returns how tall it came out.
RDE_INTERNAL f32 fude_check_draw_all(fude_check* _check, f32 _left, f32 _width, f32 _top, f32 _max_height) {
    const fude_theme* _theme = fude_theme_active();
    const f32         _pad   = 6.0f;
    const f32         _bw    = fmaxf(_check->bounds_max.x - _check->bounds_min.x, 1.0f);
    const f32         _bh    = fmaxf(_check->bounds_max.y - _check->bounds_min.y, 1.0f);
    const f32         _zoom  = fminf((_width - 2.0f * _pad) / _bw, (_max_height - 2.0f * _pad) / _bh);
    const f32         _h     = _bh * _zoom + 2.0f * _pad;
    const rde_vec_2F  _mid   = { (_check->bounds_min.x + _check->bounds_max.x) * 0.5f, (_check->bounds_min.y + _check->bounds_max.y) * 0.5f };
    const rde_vec_2F  _at    = { _left + _pad + _bw * _zoom * 0.5f, _top - _h * 0.5f };

    // The writing as written, and a tap on a character in it (draw.h).
    fude_draw_keep_begin((rde_vec_2F){ _left, _top - _h }, (rde_vec_2F){ _left + _bw * _zoom + 2.0f * _pad, _top }, true);
    const u32 _n = fude_check_count(_check);
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_check_char* _c   = fude_check_at(_check, _i);
        const rde_vec_2F       _min = { _at.x + (_c->min.x - _mid.x) * _zoom - 3.0f, _at.y + (_c->min.y - _mid.y) * _zoom - 3.0f };
        const rde_vec_2F       _max = { _at.x + (_c->max.x - _mid.x) * _zoom + 3.0f, _at.y + (_c->max.y - _mid.y) * _zoom + 3.0f };
        if(_i == _check->selected) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ fude_draw_x((_min.x + _max.x) * 0.5f), (_min.y + _max.y) * 0.5f }, (rde_vec_2F){ _max.x - _min.x, _max.y - _min.y }, _theme->select_fill);
        }
        fude_draw_outline(_min, _max, _i == _check->selected ? 1.2f : 0.8f, _i == _check->selected ? _theme->select : _theme->line);
        fude_check_add_hit(_check, _min, _max, _i);
    }
    fude_check_draw_strokes(_check, 0, fude_ink_stroke_count(&_check->drawing), _mid, _at, _zoom, rde_math_clamp_f32(_zoom * 2.5f, 0.8f, 2.5f));
    fude_draw_keep_end();
    return _h;
}

// The reading: a row (or a few) of the characters read, each with its score.
// Returns how tall it came out.
RDE_INTERNAL f32 fude_check_draw_reading(fude_check* _check, rde_font* _font, f32 _font_px, f32 _left, f32 _width, f32 _top) {
    const fude_theme* _theme = fude_theme_active();
    const f32         _step  = FUDE_CHECK_READ + FUDE_CHECK_READ_GAP;
    const f32         _row   = FUDE_CHECK_READ + 22.0f;
    const u32         _cols  = (u32)fmaxf(1.0f, floorf((_width + FUDE_CHECK_READ_GAP) / _step));
    const u32         _n     = fude_check_count(_check);
    const u32         _shown = _n < _cols * FUDE_CHECK_ROWS ? _n : _cols * FUDE_CHECK_ROWS;
    c8                _line[512];
    for(u32 _i = 0; _i < _shown; _i++) {
        const fude_check_char* _c  = fude_check_at(_check, _i);
        const rde_vec_2F       _tl = { _left + (f32)(_i % _cols) * _step, _top - (f32)(_i / _cols) * _row };
        const rde_vec_2F       _br = { _tl.x + FUDE_CHECK_READ, _tl.y - FUDE_CHECK_READ };
        if(_i == _check->selected) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ fude_draw_x(_tl.x + FUDE_CHECK_READ * 0.5f), _tl.y - FUDE_CHECK_READ * 0.5f }, (rde_vec_2F){ FUDE_CHECK_READ, FUDE_CHECK_READ }, _theme->select_fill);
            fude_draw_outline((rde_vec_2F){ _tl.x, _br.y }, (rde_vec_2F){ _br.x, _tl.y }, 1.2f, _theme->select);
        }
        fude_kanji_info _info;
        if(_c->chosen >= 0 && fude_kanji_at(_check->db, _c->candidates[_c->chosen].record, &_info)) {
            const f32 _glyph = FUDE_CHECK_READ * 0.76f;
            fude_glyph_character(&_check->glyph, _info.codepoint, (rde_vec_2F){ _tl.x + (FUDE_CHECK_READ - _glyph) * 0.5f, _tl.y - (FUDE_CHECK_READ - _glyph) * 0.5f }, _glyph, _theme->ink);
        }
        if(!_c->score.empty) {
            snprintf(_line, sizeof(_line), "%.0f", (f64)_c->score.score);
            fude_draw_text(_font, _font_px, _line, _tl.x + 4.0f, _br.y - 16.0f, 13.0f, fude_theme_grade(_c->score.score));
        }
        fude_check_add_hit(_check, (rde_vec_2F){ _tl.x, _br.y - 18.0f }, (rde_vec_2F){ _br.x, _tl.y }, _i);
    }
    return (f32)((_shown + _cols - 1u) / _cols) * _row;
}

// The character looked at closely: its writing in the square (the chosen one
// faint behind it), its score and line under it, its candidates beside it.
RDE_INTERNAL void fude_check_draw_detail(fude_check* _check, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const fude_theme*      _theme = fude_theme_active();
    const fude_check_char* _c     = fude_check_at(_check, _check->selected);
    const f32              _width = _right - _left;
    c8                     _line[512];
    _check->cell_size = FUDE_CHECK_CELL;
    if(_c == NULL) {
        return;
    }

    const f32        _room = _top - _bottom - 90.0f;
    const f32        _box  = fmaxf(120.0f, fminf(FUDE_CHECK_BOX_MAX, fminf(_width * 0.45f, _room)));
    const rde_vec_2F _tl   = { _left, _top };
    fude_glyph_box(_tl, _box);
    if(_c->chosen >= 0) {
        fude_check_draw_model(_check, _c->candidates[_c->chosen].record, _tl, _box);
    }
    fude_check_draw_writing(_check, _c, _tl, _box);

    if(_c->chosen >= 0 && !_c->score.empty) {
        snprintf(_line, sizeof(_line), "%.0f", (f64)_c->score.score);
        fude_draw_text(_font, _font_px, _line, _left, _tl.y - _box - 40.0f, 30.0f, fude_theme_grade(_c->score.score));
        fude_draw_text(_font, _font_px, _c->score.feedback, _left + 64.0f, _tl.y - _box - 38.0f, 18.0f, _theme->text);
    }

    // The candidates: beside the square when there is room, under it otherwise.
    const b8  _beside = _width - _box - 32.0f >= 2.0f * FUDE_CHECK_CELL;
    const f32 _cx0    = _beside ? _left + _box + 32.0f : _left;
    const f32 _cy0    = _beside ? _tl.y : _tl.y - _box - 70.0f;
    const u32 _cols   = (u32)fmaxf(1.0f, floorf((_right - _cx0) / FUDE_CHECK_CELL));
    for(u32 _i = 0; _i < _c->candidate_count; _i++) {
        const rde_vec_2F _cell = { _cx0 + (f32)(_i % _cols) * FUDE_CHECK_CELL, _cy0 - (f32)(_i / _cols) * FUDE_CHECK_CELL };
        _check->cell_tl[_i] = _cell;
        if(_cell.y - FUDE_CHECK_CELL < _bottom) {
            _check->cell_tl[_i] = (rde_vec_2F){ 1e30f, 1e30f };   // not shown: not tappable
            continue;
        }
        fude_kanji_info _info;
        if(!fude_kanji_at(_check->db, _c->candidates[_i].record, &_info)) {
            continue;
        }
        if((i32)_i == _c->chosen) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ fude_draw_x(_cell.x + FUDE_CHECK_CELL * 0.5f), _cell.y - FUDE_CHECK_CELL * 0.5f }, (rde_vec_2F){ FUDE_CHECK_CELL - 6.0f, FUDE_CHECK_CELL - 6.0f }, _theme->select_fill);
            fude_draw_outline((rde_vec_2F){ _cell.x + 3.0f, _cell.y - FUDE_CHECK_CELL + 3.0f }, (rde_vec_2F){ _cell.x + FUDE_CHECK_CELL - 3.0f, _cell.y - 3.0f }, 1.2f, _theme->select);
        }
        const f32 _glyph = FUDE_CHECK_CELL * 0.62f;
        fude_glyph_character(&_check->glyph, _info.codepoint, (rde_vec_2F){ _cell.x + (FUDE_CHECK_CELL - _glyph) * 0.5f, _cell.y - FUDE_CHECK_CELL * 0.1f }, _glyph, _theme->ink);
        snprintf(_line, sizeof(_line), "%u", _i + 1u);
        fude_draw_text(_font, _font_px, _line, _cell.x + 8.0f, _cell.y - FUDE_CHECK_CELL + 10.0f, 14.0f, _theme->text_soft);
    }
}

void fude_check_render(fude_check* _check, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_check->open) {
        return;
    }
    _check->_drawn = true;
    rde_arr_clear(&_check->hits);
    for(u32 _i = 0; _i < FUDE_CHECK_CANDIDATES; _i++) {
        _check->cell_tl[_i] = (rde_vec_2F){ 1e30f, 1e30f };
    }

    const fude_theme* _theme  = fude_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32         _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + FUDE_CHECK_MARGIN;
    const f32         _right  = (f32)_size.x * 0.5f - (f32)_insets.z - FUDE_CHECK_MARGIN;
    const f32         _width  = _right - _left;
    const u32         _n      = fude_check_count(_check);
    c8                _line[512];

    fude_header_title(_font, _font_px, fude_text(FUDE_TEXT_CHECK_TITLE), _left, _top);

    if(_check->pending) {
        fude_draw_text(_font, _font_px, fude_text(FUDE_TEXT_CHECK_READING), _left, _top - 60.0f, 15.0f, _theme->text_soft);
        return;
    }

    const fude_check_char* _only = _n == 1 ? fude_check_at(_check, 0) : NULL;
    if(_n == 0) {
        snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_CHECK_NOTHING));
    } else if(_only != NULL) {
        snprintf(_line, sizeof(_line), "%s", fude_text(_check->meant_count > 0 && !_check->meant_failed ? FUDE_TEXT_CHECK_MEANT_OK
                                                      : _only->candidate_count > 0 ? FUDE_TEXT_CHECK_CANDIDATES : FUDE_TEXT_CHECK_NO_MATCH));
    } else {
        f32 _sum    = 0.0f;
        u32 _scored = 0;
        for(u32 _i = 0; _i < _n; _i++) {
            const fude_check_char* _c = fude_check_at(_check, _i);
            if(!_c->score.empty) {
                _sum += _c->score.score;
                _scored++;
            }
        }
        const f32 _mean = _scored > 0 ? _sum / (f32)_scored : 0.0f;
        const FUDE_TEXT_ _which = _check->by_mlkit ? FUDE_TEXT_CHECK_MLKIT_N : _check->meant_count > 0 && !_check->meant_failed ? FUDE_TEXT_CHECK_MEANT_N
                                                                                                                                  : FUDE_TEXT_CHECK_READ_N;
        FUDE_TEXTF(_line, _which, FUDE_TN(_n), FUDE_TN(lroundf(_mean)));
    }
    if(_check->meant_failed) {
        snprintf(_line, sizeof(_line), "%s", fude_text(_check->mlkit_text[0] != 0 ? FUDE_TEXT_CHECK_MLKIT_TOO_MANY : FUDE_TEXT_CHECK_TOO_FEW));
    }
    fude_draw_text(_font, _font_px, _line, _left, _top - 60.0f, 15.0f, _theme->text_soft);

    f32 _y = _top - 86.0f;
    if(_n > 1) {
        _y -= fude_check_draw_all(_check, _left, _width, _y, fminf(FUDE_CHECK_STRIP, (_y - _bottom) * 0.25f)) + 14.0f;
        _y -= fude_check_draw_reading(_check, _font, _font_px, _left, _width, _y) + 10.0f;
    }
    fude_check_draw_detail(_check, _font, _font_px, _left, _right, _y, _bottom);
}

// --- the screen (screen.h): its row and field, and what they do ----------------------------

#include "study/app/study.h"
#include "drawing/widgets/icons.h"
#include "study/screens/practice.h"

FUDE_SCREEN_ADAPTERS(fude_check, fude_check)
FUDE_SCREEN_RENDER(fude_check, fude_check)

RDE_INTERNAL void fude_check_screen_update(fude_app* _app, void* _self, f32 _dt) {
    RDE_UNUSED(_app); RDE_UNUSED(_dt);
    fude_check_update((fude_check*)_self);   // reads the selection once the screen has shown
}

// Return in "I meant…": the selection read as that (empty: freely again).
RDE_INTERNAL void fude_check_screen_field(void* _self, const c8* _text) {
    fude_check_read_as((fude_check*)_self, _text);
}

FUDE_ROW_CALL(fude_check_row_back, fude_check, fude_check_close)

// Stroke order: the characters read writing themselves — the viewer over Check,
// on the one looked at (Prev / Next go through the rest, those with a reading).
RDE_INTERNAL void fude_check_row_order(fude_app* _app, void* _self, u32 _arg) {
    fude_check* _check = (fude_check*)_self;
    RDE_UNUSED(_arg);
    const u32 _n       = fude_check_count(_check);
    u32*      _records = _n > 0 ? (u32*)rde_malloc(sizeof(u32) * _n) : NULL;
    u32       _record  = 0;
    if(_records != NULL && fude_check_chosen_record(_check, &_record)) {
        u32 _count = 0, _at = 0;
        for(u32 _i = 0; _i < _n; _i++) {
            const fude_check_char* _c = &((const fude_check_char*)_check->chars.memory)[_i];
            if(_c->chosen >= 0) {
                if(_i == _check->selected) {
                    _at = _count;
                }
                _records[_count++] = _c->candidates[_c->chosen].record;
            }
        }
        fude_study_view(_app, _records, _count, _at);
    }
    rde_free(_records);
}

// Practice what was read: one character alone, several as a set (each once).
RDE_INTERNAL void fude_check_row_practice(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_arg);
    u32       _records[FUDE_PRACTICE_SET_MAX];
    const u32 _n = fude_check_records((const fude_check*)_self, _records, FUDE_PRACTICE_SET_MAX, true);
    fude_study_practice(_app, _records, _n);
}

static const fude_row_button FUDE_CHECK_BUTTONS[] = {
    { FUDE_TEXT_BACK,         FUDE_ICON_BACK,         fude_check_row_back,     0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_STROKE_ORDER, FUDE_ICON_STROKE_ORDER, fude_check_row_order,    0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_PRACTICE,     FUDE_ICON_PEN,          fude_check_row_practice, 0, FUDE_ROW_PRIMARY, false, NULL },
};
static const fude_row_def FUDE_CHECK_BUTTON_ROWS[] = { FUDE_ROW_DEF(FUDE_CHECK_BUTTONS) };

RDE_INTERNAL u32 fude_check_screen_row(const void* _self) {
    RDE_UNUSED(_self);
    return 0u;
}

const fude_screen FUDE_CHECK_SCREEN = {
    .name = "check", .input = FUDE_SCREEN_INPUT_POINT,
    .is_open = fude_check_screen_is_open, .close = fude_check_screen_close,
    .update = fude_check_screen_update, .render = fude_check_screen_render,
    .pointer_down = fude_check_screen_down, .pointer_moved = fude_check_screen_moved, .pointer_up = fude_check_screen_up,
    .rows = FUDE_CHECK_BUTTON_ROWS, .row_count = 1u, .row = fude_check_screen_row,
    .field_hint = FUDE_TEXT_CHECK_FIELD, .field_max = FUDE_CHECK_MEANT, .field_submit = fude_check_screen_field,
};
