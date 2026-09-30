#include "check.h"
#include "draw.h"
#include "theme.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See check.h.
// ===========================================================================

#define KANA_CHECK_MARGIN   16.0f
#define KANA_CHECK_BOX_MAX  300.0f   // the writing's square
#define KANA_CHECK_CELL     96.0f    // a candidate
#define KANA_CHECK_FILL     0.8f     // the writing and the model fill this much of the square
#define KANA_CHECK_STRIP    150.0f   // the whole writing, at most this tall
#define KANA_CHECK_READ     56.0f    // a character of the reading
#define KANA_CHECK_READ_GAP 6.0f
#define KANA_CHECK_ROWS     3u       // rows of the reading shown, at most

void kana_check_init(kana_check* _check, const kana_kanji_db* _db, const kana_catalog* _catalog) {
    memset(_check, 0, sizeof(*_check));
    _check->db      = _db;
    _check->catalog = _catalog;
    kana_glyph_init(&_check->glyph, _db);
    kana_ink_init(&_check->drawing);
    kana_segment_init(&_check->segment);
    _check->chars = rde_arr_new(sizeof(kana_check_char), rde_memory_allocator_get_default());
    _check->hits  = rde_arr_new(sizeof(kana_check_hit), rde_memory_allocator_get_default());
}

void kana_check_destroy(kana_check* _check) {
    rde_arr_free(&_check->hits);
    rde_arr_free(&_check->chars);
    kana_segment_destroy(&_check->segment);
    kana_ink_destroy(&_check->drawing);
    kana_glyph_destroy(&_check->glyph);
    memset(_check, 0, sizeof(*_check));
}

RDE_INTERNAL int kana_check_by_index(const void* _a, const void* _b) {
    const u32 _x = *(const u32*)_a;
    const u32 _y = *(const u32*)_b;
    return _x < _y ? -1 : _x > _y ? 1 : 0;
}

RDE_INTERNAL kana_check_char* kana_check_at(const kana_check* _check, u32 _index) {
    return _index < (u32)rde_arr_length(&_check->chars) ? &((kana_check_char*)_check->chars.memory)[_index] : NULL;
}

b8 kana_check_open(kana_check* _check, const kana_ink* _ink, const u32* _ids, u32 _count) {
    if(_count == 0 || _check->db == NULL || _check->catalog == NULL) {
        return false;
    }

    // In writing order: strokes are kept in the order they were written, so by index.
    u32* _order = (u32*)rde_malloc(sizeof(u32) * _count);
    memcpy(_order, _ids, sizeof(u32) * _count);
    qsort(_order, _count, sizeof(u32), kana_check_by_index);

    kana_ink_destroy(&_check->drawing);
    kana_ink_init(&_check->drawing);
    _check->bounds_min = (rde_vec_2F){ 1e30f, 1e30f };
    _check->bounds_max = (rde_vec_2F){ -1e30f, -1e30f };
    for(u32 _i = 0; _i < _count; _i++) {
        if(_order[_i] >= kana_ink_stroke_count(_ink)) {
            continue;
        }
        const kana_ink_stroke* _stroke = kana_ink_stroke_at(_ink, _order[_i]);
        if(!_stroke->alive || _stroke->point_count == 0) {
            continue;
        }
        kana_ink_add_loaded_stroke(&_check->drawing, kana_ink_stroke_points(_ink, _stroke), _stroke->point_count, _stroke->color, _stroke->from_pen);
        _check->bounds_min = (rde_vec_2F){ fminf(_check->bounds_min.x, _stroke->bounds_min.x), fminf(_check->bounds_min.y, _stroke->bounds_min.y) };
        _check->bounds_max = (rde_vec_2F){ fmaxf(_check->bounds_max.x, _stroke->bounds_max.x), fmaxf(_check->bounds_max.y, _stroke->bounds_max.y) };
    }
    rde_free(_order);
    if(kana_ink_alive_strokes(&_check->drawing) == 0) {
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
    kana_recognize_forget(&_check->recognition);   // an answer still to come was for something else
    return true;
}

void kana_check_close(kana_check* _check) {
    _check->open = false;
    kana_scroller_stop(&_check->taps);
}

// Scores a character's strokes against its chosen candidate.
RDE_INTERNAL void kana_check_score(kana_check* _check, kana_check_char* _c) {
    kana_kanji_info _info;
    memset(&_c->score, 0, sizeof(_c->score));
    _c->score.empty = true;
    if(_c->chosen < 0 || !kana_kanji_at(_check->db, _c->candidates[_c->chosen].record, &_info)) {
        return;
    }
    kana_ink _one;
    kana_ink_init(&_one);
    for(u32 _s = _c->first; _s < _c->first + _c->count && _s < kana_ink_stroke_count(&_check->drawing); _s++) {
        const kana_ink_stroke* _stroke = kana_ink_stroke_at(&_check->drawing, _s);
        kana_ink_add_loaded_stroke(&_one, kana_ink_stroke_points(&_check->drawing, _stroke), _stroke->point_count, _stroke->color, _stroke->from_pen);
    }
    _c->score = kana_score_drawing(_check->db, &_info, &_one);
    kana_ink_destroy(&_one);
}

// The reading (segment.h) — as the text meant, if there is one and it fits.
RDE_INTERNAL void kana_check_read(kana_check* _check) {
    rde_arr_clear(&_check->chars);
    _check->selected     = 0;
    _check->meant_failed = false;

    b8 _as = false;
    if(_check->meant_count > 0) {
        _as = kana_segment_read_as(&_check->segment, _check->db, &_check->drawing, KANA_SEGMENT_AUTO, _check->meant, _check->meant_count);
        _check->meant_failed = !_as;
    }
    if(!_as) {
        kana_segment_read(&_check->segment, _check->db, _check->catalog, &_check->drawing, KANA_SEGMENT_AUTO);
    }

    if(_check->meant_failed) {
        _check->by_mlkit = false;   // ML Kit's reading did not fit the strokes: Kana's own
    }

    const u32                _n    = (u32)rde_arr_length(&_check->segment.chars);
    const kana_segment_char* _read = (const kana_segment_char*)_check->segment.chars.memory;
    if(_n > 0) {
        kana_check_char* _out = (kana_check_char*)rde_arr_add_n(&_check->chars, _n);
        for(u32 _i = 0; _i < _n; _i++) {
            kana_check_char* _c = &_out[_i];
            memset(_c, 0, sizeof(*_c));
            _c->first           = _read[_i].first;
            _c->count           = _read[_i].count;
            _c->min             = _read[_i].min;
            _c->max             = _read[_i].max;
            _c->candidate_count = _read[_i].candidate_count < KANA_CHECK_CANDIDATES ? _read[_i].candidate_count : KANA_CHECK_CANDIDATES;
            memcpy(_c->candidates, _read[_i].candidates, sizeof(kana_match_result) * _c->candidate_count);

            // Read by ML Kit: what its readings have here, then the matcher's
            // ranking of these strokes — the same candidates Browse would give.
            if(_check->by_mlkit) {
                kana_ink _one;
                kana_ink_init(&_one);
                for(u32 _s = _c->first; _s < _c->first + _c->count && _s < kana_ink_stroke_count(&_check->drawing); _s++) {
                    const kana_ink_stroke* _stroke = kana_ink_stroke_at(&_check->drawing, _s);
                    kana_ink_add_loaded_stroke(&_one, kana_ink_stroke_points(&_check->drawing, _stroke), _stroke->point_count, _stroke->color, _stroke->from_pen);
                }
                kana_match_result _matched[KANA_CHECK_CANDIDATES];
                const u32         _found = kana_match_rank(_check->db, _check->catalog, KANA_FILTER_ALL, &_one, _matched, KANA_CHECK_CANDIDATES);
                kana_ink_destroy(&_one);
                _c->candidate_count = kana_recognize_candidates(_check->db, &_check->recognition, _i, _n, NULL, KANA_FILTER_ALL,
                                                                _matched, _found, _c->candidates, KANA_CHECK_CANDIDATES);
            }
            _c->chosen = _c->candidate_count > 0 ? 0 : -1;   // the best, until told otherwise
            kana_check_score(_check, _c);
        }
    }
    _check->pending = false;
}

void kana_check_update(kana_check* _check) {
    if(!_check->open || !_check->pending || !_check->_drawn) {
        return;
    }
    // Nothing typed, and ML Kit there: it is asked first, and the selection read
    // as its answer (Kana's own reading when it has none, or it does not fit).
    if(_check->meant_count == 0 && !_check->by_mlkit && kana_recognize_available()) {
        if(!_check->_asked_mlkit) {
            _check->_asked_mlkit = kana_recognize_start(&_check->recognition, &_check->drawing);
            if(_check->_asked_mlkit) {
                return;   // "Reading…" until it answers
            }
        } else {
            if(!kana_recognize_poll(&_check->recognition)) {
                return;
            }
            _check->_asked_mlkit = false;
            if(_check->recognition.line_count > 0) {
                // Its best reading is what the selection is read AS.
                snprintf(_check->mlkit_text, sizeof(_check->mlkit_text), "%s", _check->recognition.lines[0]);
                _check->meant_count = kana_recognize_records(_check->db, _check->recognition.lines[0], _check->meant, KANA_CHECK_MEANT);
                _check->by_mlkit    = _check->meant_count > 0;
            }
        }
    }
    kana_check_read(_check);
}

void kana_check_read_as(kana_check* _check, const c8* _text) {
    _check->meant_count = 0;
    _check->by_mlkit    = false;
    const c8* _p = _text != NULL ? _text : "";
    while(*_p != 0 && _check->meant_count < KANA_CHECK_MEANT) {
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
            kana_romaji_to_hiragana(_run, _kana, sizeof(_kana));
            const c8* _k = _kana;
            for(u32 _cp = kana_kanji_utf8_next(&_k); _cp != 0 && _check->meant_count < KANA_CHECK_MEANT; _cp = kana_kanji_utf8_next(&_k)) {
                if(_upper && !_lower && _cp >= 0x3041 && _cp <= 0x3096) {
                    _cp += 0x60u;   // katakana
                }
                u32 _record;
                if(kana_kanji_find_index(_check->db, _cp, &_record)) {
                    _check->meant[_check->meant_count++] = _record;
                }
            }
            continue;
        }
        const u32 _cp = kana_kanji_utf8_next(&_p);
        u32       _record;
        if(_cp != 0 && _cp != ' ' && _cp != 0x3000 && kana_kanji_find_index(_check->db, _cp, &_record)) {
            _check->meant[_check->meant_count++] = _record;
        }
    }
    _check->pending = true;
    _check->_drawn  = false;
}

u32 kana_check_count(const kana_check* _check) {
    return _check->pending ? 0u : (u32)rde_arr_length(&_check->chars);
}

void kana_check_select(kana_check* _check, u32 _index) {
    if(_index < kana_check_count(_check)) {
        _check->selected = _index;
    }
}

void kana_check_choose(kana_check* _check, u32 _index) {
    kana_check_char* _c = kana_check_at(_check, _check->selected);
    if(_check->pending || _c == NULL || _index >= _c->candidate_count) {
        return;
    }
    _c->chosen = (i32)_index;
    kana_check_score(_check, _c);
}

b8 kana_check_chosen_record(const kana_check* _check, u32* _record) {
    const kana_check_char* _c = _check->pending ? NULL : kana_check_at(_check, _check->selected);
    if(_c == NULL || _c->chosen < 0 || (u32)_c->chosen >= _c->candidate_count) {
        return false;
    }
    *_record = _c->candidates[_c->chosen].record;
    return true;
}

u32 kana_check_records(const kana_check* _check, u32* _out, u32 _max, b8 _unique) {
    u32 _n = 0;
    for(u32 _i = 0; _i < kana_check_count(_check) && _n < _max; _i++) {
        const kana_check_char* _c = kana_check_at(_check, _i);
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

void kana_check_pointer_down(kana_check* _check, rde_vec_2F _screen, f64 _time) {
    kana_scroller_down(&_check->taps, _screen, _time);
}

void kana_check_pointer_moved(kana_check* _check, rde_vec_2F _screen, f64 _time) {
    kana_scroller_moved(&_check->taps, _screen, _time);
}

void kana_check_pointer_up(kana_check* _check, f64 _time) {
    kana_scroller_up(&_check->taps, _time);
    rde_vec_2F _screen;
    if(!kana_scroller_take_tap(&_check->taps, &_screen) || _check->pending) {
        return;   // not a tap
    }
    const kana_check_char* _c = kana_check_at(_check, _check->selected);
    for(u32 _i = 0; _c != NULL && _i < _c->candidate_count; _i++) {
        const rde_vec_2F _tl = _check->cell_tl[_i];
        if(_screen.x >= _tl.x && _screen.x < _tl.x + _check->cell_size && _screen.y <= _tl.y && _screen.y > _tl.y - _check->cell_size) {
            kana_check_choose(_check, _i);
            return;
        }
    }
    const kana_check_hit* _hits = (const kana_check_hit*)_check->hits.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_check->hits); _i++) {
        if(_screen.x >= _hits[_i].min.x && _screen.x <= _hits[_i].max.x && _screen.y >= _hits[_i].min.y && _screen.y <= _hits[_i].max.y) {
            kana_check_select(_check, _hits[_i].index);
            return;
        }
    }
}

// --- drawing -----------------------------------------------------------------------

// The model, faint, fitted to the square the way the writing is: its own extent
// to KANA_CHECK_FILL of the square, centred — so the two can be compared.
RDE_INTERNAL void kana_check_draw_model(kana_check* _check, u32 _record, rde_vec_2F _tl, f32 _size) {
    kana_kanji_info _info;
    if(!kana_kanji_at(_check->db, _record, &_info)) {
        return;
    }
    rde_vec_2F _min = { 1e30f, 1e30f };
    rde_vec_2F _max = { -1e30f, -1e30f };
    for(u32 _s = 0; _s < _info.strokes; _s++) {
        kana_kanji_stroke _stroke;
        if(!kana_kanji_stroke_at(_check->db, &_info, _s, &_stroke)) {
            continue;
        }
        rde_vec_2F _pts[KANA_GLYPH_MAX_POINTS];
        const u32  _n = kana_glyph_stroke_points(&_stroke, _pts, NULL);
        for(u32 _k = 0; _k < _n; _k++) {
            _min = (rde_vec_2F){ fminf(_min.x, _pts[_k].x), fminf(_min.y, _pts[_k].y) };
            _max = (rde_vec_2F){ fmaxf(_max.x, _pts[_k].x), fmaxf(_max.y, _pts[_k].y) };
        }
    }
    const f32 _extent = fmaxf(fmaxf(_max.x - _min.x, _max.y - _min.y), 1.0f);
    const f32 _scale  = _size * KANA_CHECK_FILL / _extent;          // screen per KanjiVG unit
    const rde_vec_2F _center = { _tl.x + _size * 0.5f, _tl.y - _size * 0.5f };
    // KanjiVG is Y down: screen y = origin.y - y * scale.
    const rde_vec_2F _origin = { _center.x - (_min.x + _max.x) * 0.5f * _scale, _center.y + (_min.y + _max.y) * 0.5f * _scale };
    kana_glyph_character(&_check->glyph, _info.codepoint, _origin, _scale * KANA_KANJI_BOX, kana_theme_active()->reference);
}

// Strokes _first .. _first + _count - 1 of the drawing, the canvas point _mid at
// screen _center, _zoom screen units per canvas unit, _radius wide.
RDE_INTERNAL void kana_check_draw_strokes(kana_check* _check, u32 _first, u32 _count, rde_vec_2F _mid, rde_vec_2F _center, f32 _zoom, f32 _radius) {
    static rde_vec_2F _pos[4096];
    for(u32 _s = _first; _s < _first + _count && _s < kana_ink_stroke_count(&_check->drawing); _s++) {
        const kana_ink_stroke* _stroke = kana_ink_stroke_at(&_check->drawing, _s);
        if(!_stroke->alive) {
            continue;
        }
        const kana_ink_point* _p = kana_ink_stroke_points(&_check->drawing, _stroke);
        const u32             _n = _stroke->point_count < 4096u ? _stroke->point_count : 4096u;
        for(u32 _k = 0; _k < _n; _k++) {
            _pos[_k] = (rde_vec_2F){ _center.x + (_p[_k].position.x - _mid.x) * _zoom, _center.y + (_p[_k].position.y - _mid.y) * _zoom };
        }
        kana_draw_stroke_even(_pos, _n, _radius, kana_theme_active()->ink);
    }
}

// One character's writing, fitted to the square, drawn with the model's stroke width.
RDE_INTERNAL void kana_check_draw_writing(kana_check* _check, const kana_check_char* _c, rde_vec_2F _tl, f32 _size) {
    const f32        _extent = fmaxf(fmaxf(_c->max.x - _c->min.x, _c->max.y - _c->min.y), 1.0f);
    const rde_vec_2F _center = { _tl.x + _size * 0.5f, _tl.y - _size * 0.5f };
    const rde_vec_2F _mid    = { (_c->min.x + _c->max.x) * 0.5f, (_c->min.y + _c->max.y) * 0.5f };
    kana_check_draw_strokes(_check, _c->first, _c->count, _mid, _center, _size * KANA_CHECK_FILL / _extent,
                            fmaxf(1.0f, _size * KANA_CHECK_FILL * KANA_GLYPH_WIDTH / KANA_KANJI_BOX * 0.5f));
}

RDE_INTERNAL void kana_check_add_hit(kana_check* _check, rde_vec_2F _min, rde_vec_2F _max, u32 _index) {
    kana_check_hit* _hit = (kana_check_hit*)rde_arr_add_n(&_check->hits, 1);
    *_hit = (kana_check_hit){ .min = _min, .max = _max, .index = _index };
}

// The whole writing, fitted to the width and at most KANA_CHECK_STRIP tall, a
// box around each character read. Returns how tall it came out.
RDE_INTERNAL f32 kana_check_draw_all(kana_check* _check, f32 _left, f32 _width, f32 _top, f32 _max_height) {
    const kana_theme* _theme = kana_theme_active();
    const f32         _pad   = 6.0f;
    const f32         _bw    = fmaxf(_check->bounds_max.x - _check->bounds_min.x, 1.0f);
    const f32         _bh    = fmaxf(_check->bounds_max.y - _check->bounds_min.y, 1.0f);
    const f32         _zoom  = fminf((_width - 2.0f * _pad) / _bw, (_max_height - 2.0f * _pad) / _bh);
    const f32         _h     = _bh * _zoom + 2.0f * _pad;
    const rde_vec_2F  _mid   = { (_check->bounds_min.x + _check->bounds_max.x) * 0.5f, (_check->bounds_min.y + _check->bounds_max.y) * 0.5f };
    const rde_vec_2F  _at    = { _left + _pad + _bw * _zoom * 0.5f, _top - _h * 0.5f };

    const u32 _n = kana_check_count(_check);
    for(u32 _i = 0; _i < _n; _i++) {
        const kana_check_char* _c   = kana_check_at(_check, _i);
        const rde_vec_2F       _min = { _at.x + (_c->min.x - _mid.x) * _zoom - 3.0f, _at.y + (_c->min.y - _mid.y) * _zoom - 3.0f };
        const rde_vec_2F       _max = { _at.x + (_c->max.x - _mid.x) * _zoom + 3.0f, _at.y + (_c->max.y - _mid.y) * _zoom + 3.0f };
        if(_i == _check->selected) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ (_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f }, (rde_vec_2F){ _max.x - _min.x, _max.y - _min.y }, _theme->select_fill);
        }
        kana_draw_outline(_min, _max, _i == _check->selected ? 1.2f : 0.8f, _i == _check->selected ? _theme->select : _theme->line);
        kana_check_add_hit(_check, _min, _max, _i);
    }
    kana_check_draw_strokes(_check, 0, kana_ink_stroke_count(&_check->drawing), _mid, _at, _zoom, rde_math_clamp_f32(_zoom * 2.5f, 0.8f, 2.5f));
    return _h;
}

// The reading: a row (or a few) of the characters read, each with its score.
// Returns how tall it came out.
RDE_INTERNAL f32 kana_check_draw_reading(kana_check* _check, rde_font* _font, f32 _font_px, f32 _left, f32 _width, f32 _top) {
    const kana_theme* _theme = kana_theme_active();
    const f32         _step  = KANA_CHECK_READ + KANA_CHECK_READ_GAP;
    const f32         _row   = KANA_CHECK_READ + 22.0f;
    const u32         _cols  = (u32)fmaxf(1.0f, floorf((_width + KANA_CHECK_READ_GAP) / _step));
    const u32         _n     = kana_check_count(_check);
    const u32         _shown = _n < _cols * KANA_CHECK_ROWS ? _n : _cols * KANA_CHECK_ROWS;
    c8                _line[16];
    for(u32 _i = 0; _i < _shown; _i++) {
        const kana_check_char* _c  = kana_check_at(_check, _i);
        const rde_vec_2F       _tl = { _left + (f32)(_i % _cols) * _step, _top - (f32)(_i / _cols) * _row };
        const rde_vec_2F       _br = { _tl.x + KANA_CHECK_READ, _tl.y - KANA_CHECK_READ };
        if(_i == _check->selected) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ _tl.x + KANA_CHECK_READ * 0.5f, _tl.y - KANA_CHECK_READ * 0.5f }, (rde_vec_2F){ KANA_CHECK_READ, KANA_CHECK_READ }, _theme->select_fill);
            kana_draw_outline((rde_vec_2F){ _tl.x, _br.y }, (rde_vec_2F){ _br.x, _tl.y }, 1.2f, _theme->select);
        }
        kana_kanji_info _info;
        if(_c->chosen >= 0 && kana_kanji_at(_check->db, _c->candidates[_c->chosen].record, &_info)) {
            const f32 _glyph = KANA_CHECK_READ * 0.76f;
            kana_glyph_character(&_check->glyph, _info.codepoint, (rde_vec_2F){ _tl.x + (KANA_CHECK_READ - _glyph) * 0.5f, _tl.y - (KANA_CHECK_READ - _glyph) * 0.5f }, _glyph, _theme->ink);
        }
        if(!_c->score.empty) {
            snprintf(_line, sizeof(_line), "%.0f", (f64)_c->score.score);
            kana_draw_text(_font, _font_px, _line, _tl.x + 4.0f, _br.y - 16.0f, 13.0f, kana_theme_grade(_c->score.score));
        }
        kana_check_add_hit(_check, (rde_vec_2F){ _tl.x, _br.y - 18.0f }, (rde_vec_2F){ _br.x, _tl.y }, _i);
    }
    return (f32)((_shown + _cols - 1u) / _cols) * _row;
}

// The character looked at closely: its writing in the square (the chosen one
// faint behind it), its score and line under it, its candidates beside it.
RDE_INTERNAL void kana_check_draw_detail(kana_check* _check, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const kana_theme*      _theme = kana_theme_active();
    const kana_check_char* _c     = kana_check_at(_check, _check->selected);
    const f32              _width = _right - _left;
    c8                     _line[160];
    _check->cell_size = KANA_CHECK_CELL;
    if(_c == NULL) {
        return;
    }

    const f32        _room = _top - _bottom - 90.0f;
    const f32        _box  = fmaxf(120.0f, fminf(KANA_CHECK_BOX_MAX, fminf(_width * 0.45f, _room)));
    const rde_vec_2F _tl   = { _left, _top };
    kana_glyph_box(_tl, _box);
    if(_c->chosen >= 0) {
        kana_check_draw_model(_check, _c->candidates[_c->chosen].record, _tl, _box);
    }
    kana_check_draw_writing(_check, _c, _tl, _box);

    if(_c->chosen >= 0 && !_c->score.empty) {
        snprintf(_line, sizeof(_line), "%.0f", (f64)_c->score.score);
        kana_draw_text(_font, _font_px, _line, _left, _tl.y - _box - 40.0f, 30.0f, kana_theme_grade(_c->score.score));
        kana_draw_text(_font, _font_px, _c->score.feedback, _left + 64.0f, _tl.y - _box - 38.0f, 18.0f, _theme->text);
    }

    // The candidates: beside the square when there is room, under it otherwise.
    const b8  _beside = _width - _box - 32.0f >= 2.0f * KANA_CHECK_CELL;
    const f32 _cx0    = _beside ? _left + _box + 32.0f : _left;
    const f32 _cy0    = _beside ? _tl.y : _tl.y - _box - 70.0f;
    const u32 _cols   = (u32)fmaxf(1.0f, floorf((_right - _cx0) / KANA_CHECK_CELL));
    for(u32 _i = 0; _i < _c->candidate_count; _i++) {
        const rde_vec_2F _cell = { _cx0 + (f32)(_i % _cols) * KANA_CHECK_CELL, _cy0 - (f32)(_i / _cols) * KANA_CHECK_CELL };
        _check->cell_tl[_i] = _cell;
        if(_cell.y - KANA_CHECK_CELL < _bottom) {
            _check->cell_tl[_i] = (rde_vec_2F){ 1e30f, 1e30f };   // not shown: not tappable
            continue;
        }
        kana_kanji_info _info;
        if(!kana_kanji_at(_check->db, _c->candidates[_i].record, &_info)) {
            continue;
        }
        if((i32)_i == _c->chosen) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ _cell.x + KANA_CHECK_CELL * 0.5f, _cell.y - KANA_CHECK_CELL * 0.5f }, (rde_vec_2F){ KANA_CHECK_CELL - 6.0f, KANA_CHECK_CELL - 6.0f }, _theme->select_fill);
            kana_draw_outline((rde_vec_2F){ _cell.x + 3.0f, _cell.y - KANA_CHECK_CELL + 3.0f }, (rde_vec_2F){ _cell.x + KANA_CHECK_CELL - 3.0f, _cell.y - 3.0f }, 1.2f, _theme->select);
        }
        const f32 _glyph = KANA_CHECK_CELL * 0.62f;
        kana_glyph_character(&_check->glyph, _info.codepoint, (rde_vec_2F){ _cell.x + (KANA_CHECK_CELL - _glyph) * 0.5f, _cell.y - KANA_CHECK_CELL * 0.1f }, _glyph, _theme->ink);
        snprintf(_line, sizeof(_line), "%u", _i + 1u);
        kana_draw_text(_font, _font_px, _line, _cell.x + 8.0f, _cell.y - KANA_CHECK_CELL + 10.0f, 14.0f, _theme->text_soft);
    }
}

void kana_check_render(kana_check* _check, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_check->open) {
        return;
    }
    _check->_drawn = true;
    rde_arr_clear(&_check->hits);
    for(u32 _i = 0; _i < KANA_CHECK_CANDIDATES; _i++) {
        _check->cell_tl[_i] = (rde_vec_2F){ 1e30f, 1e30f };
    }

    const kana_theme* _theme  = kana_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32         _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + KANA_CHECK_MARGIN;
    const f32         _right  = (f32)_size.x * 0.5f - (f32)_insets.z - KANA_CHECK_MARGIN;
    const f32         _width  = _right - _left;
    const u32         _n      = kana_check_count(_check);
    c8                _line[200];

    kana_draw_text(_font, _font_px, "Check", _left, _top - 30.0f, 26.0f, _theme->text);

    if(_check->pending) {
        kana_draw_text(_font, _font_px, "Reading…", _left, _top - 60.0f, 17.0f, _theme->text_soft);
        return;
    }

    const kana_check_char* _only = _n == 1 ? kana_check_at(_check, 0) : NULL;
    if(_n == 0) {
        snprintf(_line, sizeof(_line), "Nothing to read here.");
    } else if(_only != NULL) {
        snprintf(_line, sizeof(_line), "%s", _check->meant_count > 0 && !_check->meant_failed ? "Checked as what you meant."
                                           : _only->candidate_count > 0 ? "What the selection looks like, best first. Tap the one you meant."
                                           : "Nothing looks like this (it is compared with characters of about as many strokes).");
    } else {
        f32 _sum    = 0.0f;
        u32 _scored = 0;
        for(u32 _i = 0; _i < _n; _i++) {
            const kana_check_char* _c = kana_check_at(_check, _i);
            if(!_c->score.empty) {
                _sum += _c->score.score;
                _scored++;
            }
        }
        const f32 _mean = _scored > 0 ? _sum / (f32)_scored : 0.0f;
        if(_check->by_mlkit) {
            snprintf(_line, sizeof(_line), "Read by ML Kit: %u characters, %.0f on average. Tap one to look closer.", _n, (f64)_mean);
        } else if(_check->meant_count > 0 && !_check->meant_failed) {
            snprintf(_line, sizeof(_line), "Checked as what you meant: %u characters, %.0f on average. Tap one to look closer.", _n, (f64)_mean);
        } else {
            snprintf(_line, sizeof(_line), "Read as %u characters, %.0f on average. Tap one to look closer.", _n, (f64)_mean);
        }
    }
    if(_check->meant_failed) {
        snprintf(_line, sizeof(_line), _check->mlkit_text[0] != 0 ? "ML Kit read more characters than there are strokes: read by Kana instead."
                                                                  : "Fewer strokes than the characters meant: read freely instead.");
    }
    kana_draw_text(_font, _font_px, _line, _left, _top - 60.0f, 17.0f, _theme->text_soft);

    f32 _y = _top - 86.0f;
    if(_n > 1) {
        _y -= kana_check_draw_all(_check, _left, _width, _y, fminf(KANA_CHECK_STRIP, (_y - _bottom) * 0.25f)) + 14.0f;
        _y -= kana_check_draw_reading(_check, _font, _font_px, _left, _width, _y) + 10.0f;
    }
    kana_check_draw_detail(_check, _font, _font_px, _left, _right, _y, _bottom);
}
