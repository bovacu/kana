// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/handwriting/textink.h"
#include "drawing/base/redraw.h"
#include "study/chars/glyph.h"
#include "lang/lang.h"

// ===========================================================================
// See textink.h.
//
// Text is laid out in cells from (0, 0) at the first cell's top-left, Y up, and the whole is centred at the end. Left
// to right, a character takes a cell and the line wraps after as many cells as fit. Right to left, a cursor runs from
// x 0 leftwards over the ink written so far, because joined letters are placed by where they join rather than by
// cells.
// ===========================================================================

#define FUDE_TEXTINK_LINE    1.25f              // a line's height, in cells
#define FUDE_TEXTINK_INSET   0.06f              // a character's margin in its cell, in cells
#define FUDE_TEXTINK_POINT_S (1.0f / 120.0f)    // seconds between written points (the timing a stroke keeps)
// Where joined forms join, on the baseline, in KanjiVG units: Arabic's strokes (its app's tools/strokes) end a
// joining tail at the box's left, x 11, and take the join from the letter before at its right, x 98.
#define FUDE_TEXTINK_JOIN_LEFT  11.0f
#define FUDE_TEXTINK_JOIN_RIGHT 98.0f
#define FUDE_TEXTINK_RTL_GAP    0.08f   // right to left: the gap between a word's letters that do not join, in cells
#define FUDE_TEXTINK_RTL_SPACE  0.55f   // ...and the space between words

// --- text → ink --------------------------------------------------------------------

// True for the spaces that leave a cell empty: space, tab and the ideographic space (U+3000).
RDE_INTERNAL b8 fude_textink_space(u32 _cp) {
    return _cp == ' ' || _cp == '\t' || _cp == 0x3000;
}

// Finds how far a character's strokes reach, left and right, in KanjiVG units (the whole box when it has none).
RDE_INTERNAL void fude_textink_extent(const fude_kanji_db* _db, const fude_kanji_info* _info, f32* _x0, f32* _x1) {
    static rde_vec_2F _pts[FUDE_GLYPH_MAX_POINTS];
    *_x0 = 1e30f;
    *_x1 = -1e30f;
    for(u32 _s = 0; _s < _info->strokes; _s++) {
        fude_kanji_stroke _stroke;
        if(!fude_kanji_stroke_at(_db, _info, _s, &_stroke)) {
            continue;
        }
        const u32 _n = fude_glyph_stroke_points(&_stroke, _pts, NULL);
        for(u32 _i = 0; _i < _n; _i++) {
            *_x0 = rde_math_min_f32(*_x0, _pts[_i].x);
            *_x1 = rde_math_max_f32(*_x1, _pts[_i].x);
        }
    }
    if(*_x0 > *_x1) {
        *_x0 = 0.0f;
        *_x1 = FUDE_KANJI_BOX;
    }
}

// Returns the letter beside _i in its word, before it (_step -1) or after it (+1), passing over marks, and its place
// in *_at. Returns 0 at a space, at a line's end or at the text's.
RDE_INTERNAL u32 fude_textink_neighbour(const u32* _cps, u32 _count, u32 _i, i32 _step, u32* _at) {
    for(i64 _j = (i64)_i + _step; _j >= 0 && _j < (i64)_count; _j += _step) {
        const u32 _c = _cps[_j];
        if(fude_lang_combining(_c)) {
            continue;
        }
        if(fude_textink_space(_c) || _c == '\n' || _c == '\r') {
            return 0u;
        }
        if(_at != NULL) {
            *_at = (u32)_j;
        }
        return _c;
    }
    return 0u;
}

// Empties _clip's strokes and points, after setting them up on the standard heap when they are not yet.
RDE_INTERNAL void fude_textink_clip_reset(fude_clip* _clip) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    if(!rde_arr_is_inited(&_clip->strokes)) {
        _clip->strokes = rde_arr_new(sizeof(fude_ink_stroke), _heap);
    }
    if(!rde_arr_is_inited(&_clip->points)) {
        _clip->points = rde_arr_new(sizeof(fude_ink_point), _heap);
    }
    rde_arr_clear(&_clip->strokes);
    rde_arr_clear(&_clip->points);
}

// Decodes _text (UTF-8) into _cps, which has room for a code point for each byte of it. Returns how many there are.
RDE_INTERNAL u32 fude_textink_codepoints(const c8* _text, u32* _cps) {
    u32 _count = 0;
    for(const c8* _p = _text;;) {
        const u32 _c = fude_utf8_next(&_p);
        if(_c == 0) {
            break;
        }
        _cps[_count++] = _c;
    }
    return _count;
}

// Finds the strokes of _ch, the form the letter _cp takes where it is written, into *_info. When that form has none,
// the letter itself is taken, and *_with_after is cleared, because the letter alone does not write the one after it.
// Returns false when neither has strokes.
RDE_INTERNAL b8 fude_textink_lookup(const fude_kanji_db* _db, u32 _ch, u32 _cp, fude_kanji_info* _info, b8* _with_after) {
    u32             _record;
    b8              _found = fude_kanji_find_index(_db, _ch, &_record) && fude_kanji_at(_db, _record, _info) && _info->strokes > 0;
    if(!_found && _ch != _cp) {
        _found = fude_kanji_find_index(_db, _cp, &_record) && fude_kanji_at(_db, _record, _info) && _info->strokes > 0;   // a form without strokes: the letter itself
        *_with_after = false;
    }
    return _found;
}

// Right to left: places the letter whose strokes are _info after the ink written so far, and returns its cell's
// top-left. A form that joins the letter before (_joined, with a letter before it on the line: _have_last) is moved
// left from that letter's box by where the two join; any other letter goes a gap to the left of the ink, on a new line
// when this one is full. *_cursor (where the ink ends, on the left), *_last_left (the last letter's box's left edge)
// and *_line move on. _cell, _max_width and _k (KanjiVG to canvas units) are fude_textink_write's.
RDE_INTERNAL rde_vec_2F fude_textink_place_rtl(const fude_kanji_db* _db, const fude_kanji_info* _info, b8 _joined, b8 _have_last, f32 _cell, f32 _max_width,
                                               f32 _k, f32* _cursor, f32* _last_left, u32* _line) {
    f32 _x0, _x1;
    fude_textink_extent(_db, _info, &_x0, &_x1);
    if(_joined && _have_last) {
        *_last_left -= (FUDE_TEXTINK_JOIN_RIGHT - FUDE_TEXTINK_JOIN_LEFT) * _k;
    } else {
        if(*_cursor < 0.0f && _cell - *_cursor > _max_width) {
            *_cursor = 0.0f;
            (*_line)++;
        }
        *_last_left = *_cursor - (*_cursor < 0.0f ? _cell * FUDE_TEXTINK_RTL_GAP : 0.0f) - _x1 * _k;
    }
    *_cursor = *_last_left + _x0 * _k;
    return (rde_vec_2F){ *_last_left, -(f32)(*_line) * _cell * FUDE_TEXTINK_LINE - _cell * FUDE_TEXTINK_INSET };
}

// Adds the strokes of a character (_info) to _clip, as ink written with the pen: its cell's top-left at _tl, KanjiVG
// units scaled by _k with KanjiVG's Y down, points of radius _radius, FUDE_TEXTINK_POINT_S seconds apart, and strokes
// of _color. *_min and *_max grow to hold every point with its radius.
RDE_INTERNAL void fude_textink_add_strokes(const fude_kanji_db* _db, const fude_kanji_info* _info, rde_vec_2F _tl, f32 _k, f32 _radius, rde_color _color,
                                           fude_clip* _clip, rde_vec_2F* _min, rde_vec_2F* _max) {
    static rde_vec_2F _pts[FUDE_GLYPH_MAX_POINTS];
    for(u32 _s = 0; _s < _info->strokes; _s++) {
        fude_kanji_stroke _stroke;
        if(!fude_kanji_stroke_at(_db, _info, _s, &_stroke)) {
            continue;
        }
        const u32 _n = fude_glyph_stroke_points(&_stroke, _pts, NULL);
        if(_n == 0) {
            continue;
        }
        fude_ink_stroke _out;
        rde_memset(&_out, 0, sizeof(_out));
        _out.first_point = (u32)rde_arr_length(&_clip->points);
        _out.point_count = _n;
        _out.color       = _color;
        _out.from_pen    = true;
        _out.alive       = true;
        fude_ink_point* _q = (fude_ink_point*)rde_arr_add_n(&_clip->points, _n);
        for(u32 _p = 0; _p < _n; _p++) {
            const rde_vec_2F _at = { _tl.x + _pts[_p].x * _k, _tl.y - _pts[_p].y * _k };
            _q[_p] = (fude_ink_point){ .position = _at, .pressure = 0.5f, .radius = _radius, .time = (f32)_p * FUDE_TEXTINK_POINT_S };
            *_min = (rde_vec_2F){ rde_math_min_f32(_min->x, _at.x - _radius), rde_math_min_f32(_min->y, _at.y - _radius) };
            *_max = (rde_vec_2F){ rde_math_max_f32(_max->x, _at.x + _radius), rde_math_max_f32(_max->y, _at.y + _radius) };
        }
        _out.bounds_min = _q[0].position;
        _out.bounds_max = _q[0].position;
        for(u32 _p = 1; _p < _n; _p++) {
            _out.bounds_min = (rde_vec_2F){ rde_math_min_f32(_out.bounds_min.x, _q[_p].position.x), rde_math_min_f32(_out.bounds_min.y, _q[_p].position.y) };
            _out.bounds_max = (rde_vec_2F){ rde_math_max_f32(_out.bounds_max.x, _q[_p].position.x), rde_math_max_f32(_out.bounds_max.y, _q[_p].position.y) };
        }
        _out.bounds_min = (rde_vec_2F){ _out.bounds_min.x - _radius, _out.bounds_min.y - _radius };
        _out.bounds_max = (rde_vec_2F){ _out.bounds_max.x + _radius, _out.bounds_max.y + _radius };
        rde_arr_add(&_clip->strokes, &_out);
    }
}

// Moves the ink in _clip, its points and its strokes' bounds, so that the middle of _min.._max, the bounds of all of
// it, is at (0, 0).
RDE_INTERNAL void fude_textink_centre(fude_clip* _clip, rde_vec_2F _min, rde_vec_2F _max) {
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

// Writes text as strokes (see the header).
fude_textink_result fude_textink_write(const fude_kanji_db* _db, const c8* _text, f32 _cell, f32 _max_width, f32 _radius, rde_color _color,
                                       fude_clip* _clip) {
    fude_textink_result _result = { 0, 0 };
    fude_textink_clip_reset(_clip);
    if(_db == NULL || _text == NULL || _cell <= 0.0f) {
        return _result;
    }

    // Laid out from (0, 0) at the first cell's top-left, Y up, and centred at the end.
    const u32  _per_line = (u32)rde_math_max_f32(1.0f, rde_math_floor_f32(_max_width / _cell));
    const f32  _k        = _cell * (1.0f - 2.0f * FUDE_TEXTINK_INSET) / FUDE_KANJI_BOX;   // KanjiVG to canvas units
    u32        _column = 0;
    u32        _line   = 0;
    rde_vec_2F _min    = { 1e30f, 1e30f };
    rde_vec_2F _max    = { -1e30f, -1e30f };

    // The text as code points, so that each letter can be written as its neighbours make it (lang.h: fude_lang_form,
    // Arabic's joined forms).
    rde_arr TYPE(u32) _cps_list = rde_arr_new_with_capacity(sizeof(u32), rde_strlen(_text) + 1u, rde_memory_allocator_get_default_std());
    rde_arr_resize(&_cps_list, rde_strlen(_text) + 1u);
    u32* _cps = (u32*)_cps_list.memory;
    u32 _count = fude_textink_codepoints(_text, _cps);
    // Right to left (Arabic), it is written from x 0 leftwards: a joined form is placed so that its join meets the
    // letter before's, and one that does not join goes a little gap after the ink before it (a wider one after a
    // space). A sign written on a letter (a vowel sign, a tone mark, a haraka) goes in that letter's cell, over it, in
    // either direction.
    const b8   _rtl       = fude_lang_rtl();
    f32        _cursor    = 0.0f;   // right to left: where the ink so far ends, on the left
    f32        _last_left = 0.0f;   // ...and the last letter's box's left edge
    b8         _have_last = false;
    rde_vec_2F _last_tl   = { 0.0f, 0.0f };

    for(u32 _i = 0; _i < _count; _i++) {
        const u32 _cp = _cps[_i];
        if(_cp == '\r') {
            continue;
        }
        if(_cp == '\n') {
            _column    = 0;
            _cursor    = 0.0f;
            _have_last = false;
            _line++;
            continue;
        }
        const b8 _mark = _have_last && fude_lang_combining(_cp);
        if(!_mark && !_rtl && _column >= _per_line) {
            _column = 0;
            _line++;
        }
        if(fude_textink_space(_cp)) {
            _column++;
            _cursor   -= _cell * FUDE_TEXTINK_RTL_SPACE;
            _have_last = false;
            continue;
        }
        // The character as it is written there, and whether it joins the letter before.
        u32 _ch         = _cp;
        b8  _joined     = false;
        b8  _with_after = false;
        u32 _after_at   = _i;
        if(!_mark) {
            const u32 _before = fude_textink_neighbour(_cps, _count, _i, -1, NULL);
            const u32 _after  = fude_textink_neighbour(_cps, _count, _i, 1, &_after_at);
            _ch     = fude_lang_form(_before, _cp, _after, &_with_after);
            _joined = _before != 0u && _ch != fude_lang_form(0u, _cp, _after, NULL);
        }
        fude_kanji_info _info;
        b8              _found = fude_textink_lookup(_db, _ch, _cp, &_info, &_with_after);
        if(!_found) {
            _result.skipped++;
            continue;
        }

        // The cell's top-left; the character is inset in it, with KanjiVG's Y down.
        rde_vec_2F _tl;
        if(_mark) {
            _tl = _last_tl;
        } else if(_rtl) {
            _tl = fude_textink_place_rtl(_db, &_info, _joined, _have_last, _cell, _max_width, _k, &_cursor, &_last_left, &_line);
        } else {
            _tl = (rde_vec_2F){ (f32)_column * _cell + _cell * FUDE_TEXTINK_INSET, -(f32)_line * _cell * FUDE_TEXTINK_LINE - _cell * FUDE_TEXTINK_INSET };
        }
        fude_textink_add_strokes(_db, &_info, _tl, _k, _radius, _color, _clip, &_min, &_max);
        _result.drawn++;
        if(!_mark) {
            _last_tl   = _tl;
            _have_last = true;
            _column++;
        }
        if(_with_after) {
            _i = _after_at;   // the letter after was written in this one (لا)
        }
    }
    rde_arr_free(&_cps_list);

    // Centred, because a paste drops the clip with its centre where it is asked to.
    if(_result.drawn > 0) {
        fude_textink_centre(_clip, _min, _max);
    }
    return _result;
}

// --- ink → text ----------------------------------------------------------------------

// Sets up a reader (see the header).
void fude_textink_reader_init(fude_textink_reader* _reader, const fude_kanji_db* _db, const fude_catalog* _catalog) {
    rde_memset(_reader, 0, sizeof(*_reader));
    _reader->db      = _db;
    _reader->catalog = _catalog;
    fude_ink_init(&_reader->drawing);
    fude_segment_init(&_reader->segment);
}

// Frees a reader (see the header).
void fude_textink_reader_destroy(fude_textink_reader* _reader) {
    fude_ink_destroy(&_reader->drawing);
    fude_segment_destroy(&_reader->segment);
    fude_recognize_forget(&_reader->recognition);
    rde_memset(_reader, 0, sizeof(*_reader));
}

// Orders stroke indices (rde_sort's comparer).
RDE_INTERNAL int fude_textink_by_index(const any _a, const any _b) {
    const u32 _x = *(const u32*)_a, _y = *(const u32*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// Starts reading strokes (see the header). They are copied into the reader's own drawing, so the page may change
// meanwhile.
b8 fude_textink_read(fude_textink_reader* _reader, const fude_ink* _ink, const u32* _ids, u32 _count) {
    if(_count == 0 || _reader->db == NULL || _reader->catalog == NULL) {
        return false;
    }
    // In writing order: the ink keeps its strokes in the order they were written, so their indices give it.
    u32* _order = (u32*)rde_malloc(sizeof(u32) * _count);
    rde_memcpy(_order, _ids, sizeof(u32) * _count);
    rde_sort(_order, _count, sizeof(u32), fude_textink_by_index);
    fude_ink_destroy(&_reader->drawing);
    fude_ink_init(&_reader->drawing);
    for(u32 _i = 0; _i < _count; _i++) {
        if(_order[_i] >= fude_ink_stroke_count(_ink)) {
            continue;
        }
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _order[_i]);
        if(_stroke->alive && _stroke->point_count > 0 && !_stroke->marker) {   // the marker's strokes are not writing
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

// The app's own reading (segment.h): each character's best match, with a line break between lines. It is cut at
// FUDE_TEXTINK_TEXT bytes.
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
        const usize _len   = rde_strlen(_one) + (_break ? 1u : 0u);
        if(_n + _len + 1u > sizeof(_reader->text)) {
            break;
        }
        if(_break) {
            _reader->text[_n++] = '\n';
        }
        rde_memcpy(&_reader->text[_n], _one, rde_strlen(_one));
        _n += rde_strlen(_one);
    }
    _reader->text[_n] = 0;
}

// Moves the reading on, once a frame (see the header). ML Kit's best reading is taken when it has one; otherwise, or
// without ML Kit, the app's own.
b8 fude_textink_update(fude_textink_reader* _reader) {
    if(!_reader->reading) {
        return false;
    }
    // ML Kit first, where it is: its best reading, as Check takes it.
    if(fude_recognize_available()) {
        if(!_reader->asked) {
            _reader->asked = fude_recognize_start(&_reader->recognition, &_reader->drawing);
            if(!_reader->asked) {
                fude_redraw_after(FUDE_REDRAW_POLL);   // (render on demand: ML Kit busy, asked again on a later frame)
            }
            return false;   // asked, or ML Kit is busy with another request: try again next frame
        }
        if(!fude_recognize_poll(&_reader->recognition)) {
            return false;
        }
        _reader->asked = false;
        if(_reader->recognition.line_count > 0 && _reader->recognition.lines[0][0] != 0) {
            rde_snprintf(_reader->text, sizeof(_reader->text), "%s", _reader->recognition.lines[0]);
            _reader->reading = false;
            return true;
        }
    }
    fude_textink_read_own(_reader);
    _reader->reading = false;
    return true;
}
