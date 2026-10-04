// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/chars/glyph.h"
#include "drawing/widgets/draw.h"
#include "drawing/base/theme.h"
#include "lang/lang.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See glyph.h.
// ===========================================================================

void fude_glyph_init(fude_glyph* _glyph, const fude_kanji_db* _db) {
    memset(_glyph, 0, sizeof(*_glyph));
    _glyph->db      = _db;
    _glyph->_points = rde_arr_new(sizeof(rde_vec_2F), rde_memory_allocator_get_default_std());
    _glyph->_radii  = rde_arr_new(sizeof(f32),        rde_memory_allocator_get_default_std());
}

void fude_glyph_destroy(fude_glyph* _glyph) {
    if(rde_arr_is_inited(&_glyph->_points)) { rde_arr_free(&_glyph->_points); }
    if(rde_arr_is_inited(&_glyph->_radii))  { rde_arr_free(&_glyph->_radii); }
    memset(_glyph, 0, sizeof(*_glyph));
}

RDE_INTERNAL u32 fude_glyph_flatten(const fude_kanji_stroke* _stroke, f32 _tolerance, rde_vec_2F* _out, f32* _length) {
    const u32 _n = fude_kanji_stroke_points(_stroke, _tolerance, _out, FUDE_GLYPH_MAX_POINTS);

    if(_length != NULL) {
        f32 _len = 0.0f;
        for(u32 _i = 1; _i < _n; _i++) {
            const f32 _dx = _out[_i].x - _out[_i - 1].x;
            const f32 _dy = _out[_i].y - _out[_i - 1].y;
            _len += sqrtf(_dx * _dx + _dy * _dy);
        }
        *_length = _len;
    }

    return _n;
}

u32 fude_glyph_stroke_points(const fude_kanji_stroke* _stroke, rde_vec_2F* _out, f32* _length) {
    return fude_glyph_flatten(_stroke, FUDE_GLYPH_TOLERANCE, _out, _length);
}

rde_vec_2F fude_glyph_stroke(fude_glyph* _glyph, const fude_kanji_stroke* _stroke, rde_vec_2F _origin,
                             f32 _scale, f32 _radius, f32 _fraction, rde_color _color) {
    rde_vec_2F _pts[FUDE_GLYPH_MAX_POINTS];
    f32        _length = 0.0f;
    const u32  _n      = fude_glyph_flatten(_stroke, fmaxf(FUDE_GLYPH_TOLERANCE, FUDE_GLYPH_SCREEN_TOLERANCE / _scale), _pts, &_length);
    const f32  _target = _length * rde_math_clamp_f32(_fraction, 0.0f, 1.0f);

    rde_arr_clear(&_glyph->_points);
    rde_arr_clear(&_glyph->_radii);

    f32        _walked = 0.0f;
    rde_vec_2F _end    = _origin;   // where the drawn part ends, laid out
    for(u32 _i = 0; _i < _n; _i++) {
        rde_vec_2F _p = _pts[_i];

        if(_i > 0) {
            const f32 _dx  = _pts[_i].x - _pts[_i - 1].x;
            const f32 _dy  = _pts[_i].y - _pts[_i - 1].y;
            const f32 _seg = sqrtf(_dx * _dx + _dy * _dy);
            if(_walked + _seg > _target) {
                // The pen is part-way along this piece.
                const f32 _t = _seg > 0.0f ? (_target - _walked) / _seg : 0.0f;
                _p = (rde_vec_2F){ _pts[_i - 1].x + _dx * _t, _pts[_i - 1].y + _dy * _t };
                _i = _n;   // last one
            }
            _walked += _seg;
        }

        _end               = (rde_vec_2F){ _origin.x + _p.x * _scale, _origin.y - _p.y * _scale };
        rde_vec_2F _screen = fude_draw_at(_end);   // as drawn (draw.h: a mirrored screen)
        rde_arr_add(&_glyph->_points, &_screen);
        rde_arr_add(&_glyph->_radii,  &_radius);
    }

    const u32 _count = (u32)rde_arr_length(&_glyph->_points);
    rde_rendering_2d_draw_stroke((const rde_vec_2F*)_glyph->_points.memory, (const f32*)_glyph->_radii.memory, _count, _color);
    return _end;
}

// A sign written on another letter, shown alone: a dotted circle where that letter
// goes (the middle of the box), as fonts show one.
RDE_INTERNAL void fude_glyph_base_hint(u32 _codepoint, rde_vec_2F _origin, f32 _scale) {
    if(!fude_lang_combining(_codepoint)) {
        return;
    }
    const rde_vec_2F _c    = { _origin.x + 54.5f * _scale, _origin.y - 56.0f * _scale };
    const f32        _r    = 19.0f * _scale;
    const f32        _line = fmaxf(0.6f, 0.45f * _scale);
    for(u32 _i = 0; _i < 20u; _i++) {
        const f32 _a = (f32)_i * 6.2831853f / 20.0f;
        const f32 _b = _a + 6.2831853f / 48.0f;
        fude_draw_line((rde_vec_2F){ _c.x + _r * cosf(_a), _c.y + _r * sinf(_a) }, (rde_vec_2F){ _c.x + _r * cosf(_b), _c.y + _r * sinf(_b) },
                       _line, fude_theme_active()->sheet_guide);
    }
}

b8 fude_glyph_character(fude_glyph* _glyph, u32 _codepoint, rde_vec_2F _origin, f32 _size, rde_color _color) {
    fude_kanji_info _info;
    if(_glyph->db == NULL || !fude_kanji_find(_glyph->db, _codepoint, &_info)) {
        return false;
    }

    const f32 _scale  = _size / FUDE_KANJI_BOX;
    const f32 _radius = fmaxf(0.8f, _scale * FUDE_GLYPH_WIDTH * 0.5f);
    // A character is never mirrored: its box goes where a right-to-left screen puts it.
    fude_draw_keep_begin((rde_vec_2F){ _origin.x, _origin.y - _size }, (rde_vec_2F){ _origin.x + _size, _origin.y }, false);
    fude_glyph_base_hint(_codepoint, _origin, _scale);
    for(u32 _s = 0; _s < _info.strokes; _s++) {
        fude_kanji_stroke _stroke;
        if(fude_kanji_stroke_at(_glyph->db, &_info, _s, &_stroke)) {
            fude_glyph_stroke(_glyph, &_stroke, _origin, _scale, _radius, 1.0f, _color);
        }
    }
    fude_draw_keep_end();
    return true;
}

void fude_glyph_box(rde_vec_2F _tl, f32 _size) {
    // A card: rounded a little (less when small), a hairline edge.
    fude_draw_card((rde_vec_2F){ _tl.x, _tl.y - _size }, (rde_vec_2F){ _tl.x + _size, _tl.y }, fminf(14.0f, _size * 0.06f),
                   fude_theme_active()->sheet, fude_theme_active()->sheet_outline);

    const f32 _dash = fmaxf(4.0f, _size / 40.0f);
    const f32 _cx   = _tl.x + _size * 0.5f;
    const f32 _cy   = _tl.y - _size * 0.5f;
    for(f32 _t = _dash * 0.5f; _t < _size - _dash * 0.5f; _t += _dash * 2.0f) {
        const f32 _e = fminf(_t + _dash, _size - _dash * 0.5f);
        fude_draw_line((rde_vec_2F){ _cx, _tl.y - _t }, (rde_vec_2F){ _cx, _tl.y - _e }, 0.7f, fude_theme_active()->sheet_guide);
        fude_draw_line((rde_vec_2F){ _tl.x + _t, _cy }, (rde_vec_2F){ _tl.x + _e, _cy }, 0.7f, fude_theme_active()->sheet_guide);
    }
}

RDE_INTERNAL b8 fude_glyph_writing_strokes(fude_glyph* _glyph, const fude_kanji_info* _info, rde_vec_2F _tl, f32 _scale, f32 _radius, f64 _elapsed,
                                           rde_font* _font, f32 _font_px, f32 _number_px);

b8 fude_glyph_writing(fude_glyph* _glyph, const fude_kanji_info* _info, rde_vec_2F _tl, f32 _size, f64 _elapsed,
                      rde_font* _font, f32 _font_px, f32 _number_px) {
    const f32 _scale  = _size / FUDE_KANJI_BOX;
    const f32 _radius = fmaxf(0.8f, _scale * FUDE_GLYPH_WIDTH * 0.5f);

    // Never mirrored, as a character is (fude_glyph_character).
    fude_draw_keep_begin((rde_vec_2F){ _tl.x, _tl.y - _size }, (rde_vec_2F){ _tl.x + _size, _tl.y }, false);
    const b8 _done = fude_glyph_writing_strokes(_glyph, _info, _tl, _scale, _radius, _elapsed, _font, _font_px, _number_px);
    fude_draw_keep_end();
    return _done;
}

// fude_glyph_writing's strokes, at _scale screen units per KanjiVG unit.
RDE_INTERNAL b8 fude_glyph_writing_strokes(fude_glyph* _glyph, const fude_kanji_info* _info, rde_vec_2F _tl, f32 _scale, f32 _radius, f64 _elapsed,
                                           rde_font* _font, f32 _font_px, f32 _number_px) {
    fude_glyph_base_hint(_info->codepoint, _tl, _scale);

    // Ghosts first: the whole shape, faint, so the eye knows where it is going.
    for(u32 _s = 0; _s < _info->strokes; _s++) {
        fude_kanji_stroke _stroke;
        if(fude_kanji_stroke_at(_glyph->db, _info, _s, &_stroke)) {
            fude_glyph_stroke(_glyph, &_stroke, _tl, _scale, _radius, 1.0f, fude_theme_active()->ghost);
        }
    }

    // Then the writing, stroke by stroke, in time.
    f64 _clock = _elapsed;
    for(u32 _s = 0; _s < _info->strokes; _s++) {
        fude_kanji_stroke _stroke;
        if(!fude_kanji_stroke_at(_glyph->db, _info, _s, &_stroke)) {
            continue;
        }

        rde_vec_2F _pts[FUDE_GLYPH_MAX_POINTS];
        f32        _length = 0.0f;
        fude_glyph_stroke_points(&_stroke, _pts, &_length);
        const f64  _duration = FUDE_GLYPH_STROKE_BASE + FUDE_GLYPH_STROKE_PER * (f64)_length;

        if(_clock <= 0.0) {
            return false;   // not reached yet: only its ghost
        }

        const f32        _fraction = (f32)fmin(1.0, _clock / _duration);
        const rde_vec_2F _tip      = fude_glyph_stroke(_glyph, &_stroke, _tl, _scale, _radius, _fraction, fude_theme_active()->ink);

        // Its number by where it starts: the order is the lesson.
        if(_number_px > 0.0f && _font != NULL) {
            c8 _num[8];
            snprintf(_num, sizeof(_num), "%u", _s + 1u);
            fude_draw_text(_font, _font_px, _num, _tl.x + _stroke.start.x * _scale - _number_px, _tl.y - _stroke.start.y * _scale + _number_px * 0.9f,
                           _number_px, fude_theme_active()->stroke_number);
        }

        if(_fraction < 1.0f) {
            rde_rendering_2d_draw_circle(fude_draw_at(_tip), _radius * 1.35f, 16u, fude_theme_active()->pen_tip, NULL);
            return false;
        }

        _clock -= _duration + FUDE_GLYPH_STROKE_GAP;
    }

    return true;
}

static rde_font* fude_glyph_text_font    = NULL;
static f32       fude_glyph_text_font_px = 14.0f;

void fude_glyph_set_text_font(rde_font* _font, f32 _font_px) {
    fude_glyph_text_font    = _font;
    fude_glyph_text_font_px = _font_px;
}

// fude_glyph_reading's walk: drawn when _draw, else only measured. Where it ends.
RDE_INTERNAL f32 fude_glyph_reading_walk(fude_glyph* _glyph, const c8* _text, rde_vec_2F _origin, f32 _size, f32 _max_x, rde_color _ink, rde_color _soft, b8 _draw) {
    rde_color _color = _ink;
    f32       _x     = _origin.x;

    for(u32 _cp = fude_utf8_next(&_text); _cp != 0 && _x + _size <= _max_x; _cp = fude_utf8_next(&_text)) {
        if(_cp == '.') {
            _color = _soft;
            continue;
        }
        if(_cp == '-') {
            if(_draw) {
                fude_draw_line((rde_vec_2F){ _x + _size * 0.15f, _origin.y - _size * 0.5f }, (rde_vec_2F){ _x + _size * 0.45f, _origin.y - _size * 0.5f }, 1.2f, _color);
            }
            _x += _size * 0.6f;
            continue;
        }
        if(_cp == 0x3001u) {   // 、 — a new reading starts
            _color = _ink;
        }
        fude_kanji_info _info;
        const b8 _strokes = _glyph->db != NULL && fude_kanji_find(_glyph->db, _cp, &_info);
        if(_strokes && _draw) {
            fude_glyph_character(_glyph, _cp, (rde_vec_2F){ _x, _origin.y }, _size, _color);
        }
        if(!_strokes && fude_glyph_text_font != NULL && _cp != 0x3001u) {
            // No strokes for it (a letter of pinyin): the text font, centred on the line.
            c8 _one[5];
            fude_utf8_put(_cp, _one);
            const f32 _px = _size * 0.8f;
            if(_draw) {
                fude_draw_text(fude_glyph_text_font, fude_glyph_text_font_px, _one, _x, _origin.y - _size * 0.5f - _px * 0.36f, _px, _color);
            }
            _x += fude_draw_text_width(fude_glyph_text_font, fude_glyph_text_font_px, _one, _px);
            continue;
        }
        _x += _cp == 0x3001u ? _size * 0.7f : _size * 0.95f;
    }

    return _x;
}

f32 fude_glyph_reading(fude_glyph* _glyph, const c8* _text, rde_vec_2F _origin, f32 _size, f32 _max_x, rde_color _ink, rde_color _soft) {
    // One run, read left to right however the screen runs (a right-to-left one
    // puts it, whole, where it starts there).
    const f32 _end = fude_glyph_reading_walk(_glyph, _text, _origin, _size, _max_x, _ink, _soft, false);
    fude_draw_keep_begin((rde_vec_2F){ _origin.x, _origin.y - _size }, (rde_vec_2F){ _end, _origin.y }, false);
    fude_glyph_reading_walk(_glyph, _text, _origin, _size, _max_x, _ink, _soft, true);
    fude_draw_keep_end();
    return _end;
}
