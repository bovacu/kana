#include "study/handwriting/textink.h"
#include "study/chars/glyph.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See textink.h.
// ===========================================================================

#define FUDE_TEXTINK_LINE    1.25f              // a line's height, in cells
#define FUDE_TEXTINK_INSET   0.06f              // a character's margin in its cell, in cells
#define FUDE_TEXTINK_POINT_S (1.0f / 120.0f)    // seconds between written points (the timing a stroke keeps)

// --- text → ink --------------------------------------------------------------------

RDE_INTERNAL b8 fude_textink_space(u32 _cp) {
    return _cp == ' ' || _cp == '\t' || _cp == 0x3000;
}

fude_textink_result fude_textink_write(const fude_kanji_db* _db, const c8* _text, f32 _cell, f32 _max_width, f32 _radius, rde_color _color,
                                       fude_clip* _clip) {
    fude_textink_result _result = { 0, 0 };
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    if(!rde_arr_is_inited(&_clip->strokes)) {
        _clip->strokes = rde_arr_new(sizeof(fude_ink_stroke), _heap);
    }
    if(!rde_arr_is_inited(&_clip->points)) {
        _clip->points = rde_arr_new(sizeof(fude_ink_point), _heap);
    }
    rde_arr_clear(&_clip->strokes);
    rde_arr_clear(&_clip->points);
    if(_db == NULL || _text == NULL || _cell <= 0.0f) {
        return _result;
    }

    // Laid out from (0, 0) at the first cell's top-left, Y up; centred at the end.
    const u32  _per_line = (u32)fmaxf(1.0f, floorf(_max_width / _cell));
    const f32  _k        = _cell * (1.0f - 2.0f * FUDE_TEXTINK_INSET) / FUDE_KANJI_BOX;   // KanjiVG units → canvas units
    static rde_vec_2F _pts[FUDE_GLYPH_MAX_POINTS];
    u32        _column = 0;
    u32        _line   = 0;
    rde_vec_2F _min    = { 1e30f, 1e30f };
    rde_vec_2F _max    = { -1e30f, -1e30f };

    const c8* _p = _text;
    for(u32 _cp = fude_utf8_next(&_p); _cp != 0; _cp = fude_utf8_next(&_p)) {
        if(_cp == '\r') {
            continue;
        }
        if(_cp == '\n') {
            _column = 0;
            _line++;
            continue;
        }
        if(_column >= _per_line) {
            _column = 0;
            _line++;
        }
        if(fude_textink_space(_cp)) {
            _column++;
            continue;
        }
        u32             _record;
        fude_kanji_info _info;
        if(!fude_kanji_find_index(_db, _cp, &_record) || !fude_kanji_at(_db, _record, &_info) || _info.strokes == 0) {
            _result.skipped++;
            continue;
        }

        // The cell's top-left; the character inset in it, KanjiVG's Y down.
        const rde_vec_2F _tl = { (f32)_column * _cell + _cell * FUDE_TEXTINK_INSET, -(f32)_line * _cell * FUDE_TEXTINK_LINE - _cell * FUDE_TEXTINK_INSET };
        for(u32 _s = 0; _s < _info.strokes; _s++) {
            fude_kanji_stroke _stroke;
            if(!fude_kanji_stroke_at(_db, &_info, _s, &_stroke)) {
                continue;
            }
            const u32 _n = fude_glyph_stroke_points(&_stroke, _pts, NULL);
            if(_n == 0) {
                continue;
            }
            fude_ink_stroke _out;
            memset(&_out, 0, sizeof(_out));
            _out.first_point = (u32)rde_arr_length(&_clip->points);
            _out.point_count = _n;
            _out.color       = _color;
            _out.from_pen    = true;
            _out.alive       = true;
            fude_ink_point* _q = (fude_ink_point*)rde_arr_add_n(&_clip->points, _n);
            for(u32 _i = 0; _i < _n; _i++) {
                const rde_vec_2F _at = { _tl.x + _pts[_i].x * _k, _tl.y - _pts[_i].y * _k };
                _q[_i] = (fude_ink_point){ .position = _at, .pressure = 0.5f, .radius = _radius, .time = (f32)_i * FUDE_TEXTINK_POINT_S };
                _min = (rde_vec_2F){ fminf(_min.x, _at.x - _radius), fminf(_min.y, _at.y - _radius) };
                _max = (rde_vec_2F){ fmaxf(_max.x, _at.x + _radius), fmaxf(_max.y, _at.y + _radius) };
            }
            _out.bounds_min = _q[0].position;
            _out.bounds_max = _q[0].position;
            for(u32 _i = 1; _i < _n; _i++) {
                _out.bounds_min = (rde_vec_2F){ fminf(_out.bounds_min.x, _q[_i].position.x), fminf(_out.bounds_min.y, _q[_i].position.y) };
                _out.bounds_max = (rde_vec_2F){ fmaxf(_out.bounds_max.x, _q[_i].position.x), fmaxf(_out.bounds_max.y, _q[_i].position.y) };
            }
            _out.bounds_min = (rde_vec_2F){ _out.bounds_min.x - _radius, _out.bounds_min.y - _radius };
            _out.bounds_max = (rde_vec_2F){ _out.bounds_max.x + _radius, _out.bounds_max.y + _radius };
            rde_arr_add(&_clip->strokes, &_out);
        }
        _result.drawn++;
        _column++;
    }

    // Centred: a paste drops the clip with its centre where it is asked to.
    if(_result.drawn > 0) {
        const rde_vec_2F _c = { (_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f };
        fude_ink_point*  _q = (fude_ink_point*)_clip->points.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_clip->points); _i++) {
            _q[_i].position = (rde_vec_2F){ _q[_i].position.x - _c.x, _q[_i].position.y - _c.y };
        }
        fude_ink_stroke* _s = (fude_ink_stroke*)_clip->strokes.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_clip->strokes); _i++) {
            _s[_i].bounds_min = (rde_vec_2F){ _s[_i].bounds_min.x - _c.x, _s[_i].bounds_min.y - _c.y };
            _s[_i].bounds_max = (rde_vec_2F){ _s[_i].bounds_max.x - _c.x, _s[_i].bounds_max.y - _c.y };
        }
    }
    return _result;
}

// --- ink → text ----------------------------------------------------------------------

void fude_textink_reader_init(fude_textink_reader* _reader, const fude_kanji_db* _db, const fude_catalog* _catalog) {
    memset(_reader, 0, sizeof(*_reader));
    _reader->db      = _db;
    _reader->catalog = _catalog;
    fude_ink_init(&_reader->drawing);
    fude_segment_init(&_reader->segment);
}

void fude_textink_reader_destroy(fude_textink_reader* _reader) {
    fude_ink_destroy(&_reader->drawing);
    fude_segment_destroy(&_reader->segment);
    fude_recognize_forget(&_reader->recognition);
    memset(_reader, 0, sizeof(*_reader));
}

RDE_INTERNAL int fude_textink_by_index(const void* _a, const void* _b) {
    const u32 _x = *(const u32*)_a, _y = *(const u32*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

b8 fude_textink_read(fude_textink_reader* _reader, const fude_ink* _ink, const u32* _ids, u32 _count) {
    if(_count == 0 || _reader->db == NULL || _reader->catalog == NULL) {
        return false;
    }
    // In writing order: strokes are kept in the order they were written.
    u32* _order = (u32*)rde_malloc(sizeof(u32) * _count);
    memcpy(_order, _ids, sizeof(u32) * _count);
    qsort(_order, _count, sizeof(u32), fude_textink_by_index);
    fude_ink_destroy(&_reader->drawing);
    fude_ink_init(&_reader->drawing);
    for(u32 _i = 0; _i < _count; _i++) {
        if(_order[_i] >= fude_ink_stroke_count(_ink)) {
            continue;
        }
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _order[_i]);
        if(_stroke->alive && _stroke->point_count > 0 && !_stroke->marker) {   // the marker's: not writing
            fude_ink_add_loaded_stroke(&_reader->drawing, fude_ink_stroke_points(_ink, _stroke), _stroke->point_count, _stroke->color, _stroke->from_pen);
        }
    }
    rde_free(_order);
    if(fude_ink_alive_strokes(&_reader->drawing) == 0) {
        return false;
    }
    fude_recognize_forget(&_reader->recognition);
    _reader->text[0] = 0;
    _reader->asked   = false;
    _reader->reading = true;
    return true;
}

// Kana's own reading: each character's best match, a line break between lines.
RDE_INTERNAL void fude_textink_read_own(fude_textink_reader* _reader) {
    fude_segment_read(&_reader->segment, _reader->db, _reader->catalog, &_reader->drawing, FUDE_SEGMENT_AUTO);
    const fude_segment_char* _chars = (const fude_segment_char*)_reader->segment.chars.memory;
    usize                    _n     = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_reader->segment.chars); _i++) {
        fude_kanji_info _info;
        if(_chars[_i].candidate_count == 0 || !fude_kanji_at(_reader->db, _chars[_i].candidates[0].record, &_info)) {
            continue;
        }
        c8 _one[8];
        fude_utf8_put(_info.codepoint, _one);
        const b8    _break = _i > 0 && _chars[_i].line != _chars[_i - 1].line;
        const usize _len   = strlen(_one) + (_break ? 1u : 0u);
        if(_n + _len + 1u > sizeof(_reader->text)) {
            break;
        }
        if(_break) {
            _reader->text[_n++] = '\n';
        }
        memcpy(&_reader->text[_n], _one, strlen(_one));
        _n += strlen(_one);
    }
    _reader->text[_n] = 0;
}

b8 fude_textink_update(fude_textink_reader* _reader) {
    if(!_reader->reading) {
        return false;
    }
    // ML Kit first, where it is: its best reading, as Check takes it.
    if(fude_recognize_available()) {
        if(!_reader->asked) {
            _reader->asked = fude_recognize_start(&_reader->recognition, &_reader->drawing);
            return false;   // asked, or busy with another request: again next frame
        }
        if(!fude_recognize_poll(&_reader->recognition)) {
            return false;
        }
        _reader->asked = false;
        if(_reader->recognition.line_count > 0 && _reader->recognition.lines[0][0] != 0) {
            snprintf(_reader->text, sizeof(_reader->text), "%s", _reader->recognition.lines[0]);
            _reader->reading = false;
            return true;
        }
    }
    fude_textink_read_own(_reader);
    _reader->reading = false;
    return true;
}
