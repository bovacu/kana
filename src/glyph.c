#include "glyph.h"
#include "draw.h"
#include "theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See glyph.h.
// ===========================================================================

void kana_glyph_init(kana_glyph* _glyph, const kana_kanji_db* _db) {
    memset(_glyph, 0, sizeof(*_glyph));
    _glyph->db      = _db;
    _glyph->_points = rde_arr_new(sizeof(rde_vec_2F), rde_memory_allocator_get_default_std());
    _glyph->_radii  = rde_arr_new(sizeof(f32),        rde_memory_allocator_get_default_std());
}

void kana_glyph_destroy(kana_glyph* _glyph) {
    if(rde_arr_is_inited(&_glyph->_points)) { rde_arr_free(&_glyph->_points); }
    if(rde_arr_is_inited(&_glyph->_radii))  { rde_arr_free(&_glyph->_radii); }
    memset(_glyph, 0, sizeof(*_glyph));
}

RDE_INTERNAL u32 kana_glyph_flatten(const kana_kanji_stroke* _stroke, f32 _tolerance, rde_vec_2F* _out, f32* _length) {
    const u32 _n = kana_kanji_stroke_points(_stroke, _tolerance, _out, KANA_GLYPH_MAX_POINTS);

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

u32 kana_glyph_stroke_points(const kana_kanji_stroke* _stroke, rde_vec_2F* _out, f32* _length) {
    return kana_glyph_flatten(_stroke, KANA_GLYPH_TOLERANCE, _out, _length);
}

rde_vec_2F kana_glyph_stroke(kana_glyph* _glyph, const kana_kanji_stroke* _stroke, rde_vec_2F _origin,
                             f32 _scale, f32 _radius, f32 _fraction, rde_color _color) {
    rde_vec_2F _pts[KANA_GLYPH_MAX_POINTS];
    f32        _length = 0.0f;
    const u32  _n      = kana_glyph_flatten(_stroke, fmaxf(KANA_GLYPH_TOLERANCE, KANA_GLYPH_SCREEN_TOLERANCE / _scale), _pts, &_length);
    const f32  _target = _length * rde_math_clamp_f32(_fraction, 0.0f, 1.0f);

    rde_arr_clear(&_glyph->_points);
    rde_arr_clear(&_glyph->_radii);

    f32 _walked = 0.0f;
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

        rde_vec_2F _screen = { _origin.x + _p.x * _scale, _origin.y - _p.y * _scale };
        rde_arr_add(&_glyph->_points, &_screen);
        rde_arr_add(&_glyph->_radii,  &_radius);
    }

    const u32 _count = (u32)rde_arr_length(&_glyph->_points);
    rde_rendering_2d_draw_stroke((const rde_vec_2F*)_glyph->_points.memory, (const f32*)_glyph->_radii.memory, _count, _color);
    return ((const rde_vec_2F*)_glyph->_points.memory)[_count - 1];
}

b8 kana_glyph_character(kana_glyph* _glyph, u32 _codepoint, rde_vec_2F _origin, f32 _size, rde_color _color) {
    kana_kanji_info _info;
    if(_glyph->db == NULL || !kana_kanji_find(_glyph->db, _codepoint, &_info)) {
        return false;
    }

    const f32 _scale  = _size / KANA_KANJI_BOX;
    const f32 _radius = fmaxf(0.8f, _scale * KANA_GLYPH_WIDTH * 0.5f);
    for(u32 _s = 0; _s < _info.strokes; _s++) {
        kana_kanji_stroke _stroke;
        if(kana_kanji_stroke_at(_glyph->db, &_info, _s, &_stroke)) {
            kana_glyph_stroke(_glyph, &_stroke, _origin, _scale, _radius, 1.0f, _color);
        }
    }
    return true;
}

void kana_glyph_box(rde_vec_2F _tl, f32 _size) {
    // A card: rounded a little (less when small), a hairline edge.
    kana_draw_card((rde_vec_2F){ _tl.x, _tl.y - _size }, (rde_vec_2F){ _tl.x + _size, _tl.y }, fminf(14.0f, _size * 0.06f),
                   kana_theme_active()->sheet, kana_theme_active()->sheet_outline);

    const f32 _dash = fmaxf(4.0f, _size / 40.0f);
    const f32 _cx   = _tl.x + _size * 0.5f;
    const f32 _cy   = _tl.y - _size * 0.5f;
    for(f32 _t = _dash * 0.5f; _t < _size - _dash * 0.5f; _t += _dash * 2.0f) {
        const f32 _e = fminf(_t + _dash, _size - _dash * 0.5f);
        kana_draw_line((rde_vec_2F){ _cx, _tl.y - _t }, (rde_vec_2F){ _cx, _tl.y - _e }, 0.7f, kana_theme_active()->sheet_guide);
        kana_draw_line((rde_vec_2F){ _tl.x + _t, _cy }, (rde_vec_2F){ _tl.x + _e, _cy }, 0.7f, kana_theme_active()->sheet_guide);
    }
}

b8 kana_glyph_writing(kana_glyph* _glyph, const kana_kanji_info* _info, rde_vec_2F _tl, f32 _size, f64 _elapsed,
                      rde_font* _font, f32 _font_px, f32 _number_px) {
    const f32 _scale  = _size / KANA_KANJI_BOX;
    const f32 _radius = fmaxf(0.8f, _scale * KANA_GLYPH_WIDTH * 0.5f);

    // Ghosts first: the whole shape, faint, so the eye knows where it is going.
    for(u32 _s = 0; _s < _info->strokes; _s++) {
        kana_kanji_stroke _stroke;
        if(kana_kanji_stroke_at(_glyph->db, _info, _s, &_stroke)) {
            kana_glyph_stroke(_glyph, &_stroke, _tl, _scale, _radius, 1.0f, kana_theme_active()->ghost);
        }
    }

    // Then the writing, stroke by stroke, in time.
    f64 _clock = _elapsed;
    for(u32 _s = 0; _s < _info->strokes; _s++) {
        kana_kanji_stroke _stroke;
        if(!kana_kanji_stroke_at(_glyph->db, _info, _s, &_stroke)) {
            continue;
        }

        rde_vec_2F _pts[KANA_GLYPH_MAX_POINTS];
        f32        _length = 0.0f;
        kana_glyph_stroke_points(&_stroke, _pts, &_length);
        const f64  _duration = KANA_GLYPH_STROKE_BASE + KANA_GLYPH_STROKE_PER * (f64)_length;

        if(_clock <= 0.0) {
            return false;   // not reached yet: only its ghost
        }

        const f32        _fraction = (f32)fmin(1.0, _clock / _duration);
        const rde_vec_2F _tip      = kana_glyph_stroke(_glyph, &_stroke, _tl, _scale, _radius, _fraction, kana_theme_active()->ink);

        // Its number by where it starts: the order is the lesson.
        if(_number_px > 0.0f && _font != NULL) {
            c8 _num[8];
            snprintf(_num, sizeof(_num), "%u", _s + 1u);
            kana_draw_text(_font, _font_px, _num, _tl.x + _stroke.start.x * _scale - _number_px, _tl.y - _stroke.start.y * _scale + _number_px * 0.9f,
                           _number_px, kana_theme_active()->stroke_number);
        }

        if(_fraction < 1.0f) {
            rde_rendering_2d_draw_circle(_tip, _radius * 1.35f, 16u, kana_theme_active()->pen_tip, NULL);
            return false;
        }

        _clock -= _duration + KANA_GLYPH_STROKE_GAP;
    }

    return true;
}

f32 kana_glyph_reading(kana_glyph* _glyph, const c8* _text, rde_vec_2F _origin, f32 _size, f32 _max_x, rde_color _ink, rde_color _soft) {
    rde_color _color = _ink;
    f32       _x     = _origin.x;

    for(u32 _cp = kana_kanji_utf8_next(&_text); _cp != 0 && _x + _size <= _max_x; _cp = kana_kanji_utf8_next(&_text)) {
        if(_cp == '.') {
            _color = _soft;
            continue;
        }
        if(_cp == '-') {
            kana_draw_line((rde_vec_2F){ _x + _size * 0.15f, _origin.y - _size * 0.5f }, (rde_vec_2F){ _x + _size * 0.45f, _origin.y - _size * 0.5f }, 1.2f, _color);
            _x += _size * 0.6f;
            continue;
        }
        if(_cp == 0x3001u) {   // 、 — a new reading starts
            _color = _ink;
        }
        kana_glyph_character(_glyph, _cp, (rde_vec_2F){ _x, _origin.y }, _size, _color);
        _x += _cp == 0x3001u ? _size * 0.7f : _size * 0.95f;
    }

    return _x;
}
