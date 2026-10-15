// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/chars/glyph.h"
#include "drawing/base/draw.h"
#include "drawing/base/theme.h"
#include "lang/lang.h"

// ===========================================================================
// See glyph.h.
//
// A stroke is flattened into points (kanji.h: fude_kanji_stroke_points) and drawn as one variable-width stroke by
// RDE, through the fude_glyph's arrays of points and radii, which are reused from stroke to stroke so that drawing
// allocates nothing once they have grown.
// ===========================================================================

// Sets up _glyph to draw the characters of _db (NULL draws nothing).
void fude_glyph_init(fude_glyph* _glyph, const fude_kanji_db* _db) {
    rde_memset(_glyph, 0, sizeof(*_glyph));
    _glyph->db      = _db;
    _glyph->_points = rde_arr_new(sizeof(rde_vec_2F), rde_memory_allocator_get_default_std());
    _glyph->_radii  = rde_arr_new(sizeof(f32),        rde_memory_allocator_get_default_std());
}

// Frees _glyph's arrays and clears it.
void fude_glyph_destroy(fude_glyph* _glyph) {
    if(rde_arr_is_inited(&_glyph->_points)) { rde_arr_free(&_glyph->_points); }
    if(rde_arr_is_inited(&_glyph->_radii))  { rde_arr_free(&_glyph->_radii); }
    rde_memset(_glyph, 0, sizeof(*_glyph));
}

// Flattens _stroke into _out (at most FUDE_GLYPH_MAX_POINTS points, KanjiVG units) at _tolerance, and returns how
// many there are. _length (may be NULL) gets the length of the flattened line.
RDE_INTERNAL u32 fude_glyph_flatten(const fude_kanji_stroke* _stroke, f32 _tolerance, rde_vec_2F* _out, f32* _length) {
    const u32 _n = fude_kanji_stroke_points(_stroke, _tolerance, _out, FUDE_GLYPH_MAX_POINTS);

    if(_length != NULL) {
        f32 _len = 0.0f;
        for(u32 _i = 1; _i < _n; _i++) {
            const f32 _dx = _out[_i].x - _out[_i - 1].x;
            const f32 _dy = _out[_i].y - _out[_i - 1].y;
            _len += rde_math_sqrt_f32(_dx * _dx + _dy * _dy);
        }
        *_length = _len;
    }

    return _n;
}

// Flattens a stroke at FUDE_GLYPH_TOLERANCE, the finest (see the header).
u32 fude_glyph_stroke_points(const fude_kanji_stroke* _stroke, rde_vec_2F* _out, f32* _length) {
    return fude_glyph_flatten(_stroke, FUDE_GLYPH_TOLERANCE, _out, _length);
}

// Draws the first _fraction of a stroke already flattened: its _n points _pts (KanjiVG units) and their _length.
// Returns where the drawn part ends, as fude_glyph_stroke does.
RDE_INTERNAL rde_vec_2F fude_glyph_draw_points(fude_glyph* _glyph, const rde_vec_2F* _pts, u32 _n, f32 _length, rde_vec_2F _origin,
                                               f32 _scale, f32 _radius, f32 _fraction, rde_color _color) {
    const f32 _target = _length * rde_math_clamp_f32(_fraction, 0.0f, 1.0f);

    rde_arr_clear(&_glyph->_points);
    rde_arr_clear(&_glyph->_radii);

    f32        _walked = 0.0f;
    rde_vec_2F _end    = _origin;   // where the drawn part ends, laid out (before any mirroring)
    for(u32 _i = 0; _i < _n; _i++) {
        rde_vec_2F _p = _pts[_i];

        if(_i > 0) {
            const f32 _dx  = _pts[_i].x - _pts[_i - 1].x;
            const f32 _dy  = _pts[_i].y - _pts[_i - 1].y;
            const f32 _seg = rde_math_sqrt_f32(_dx * _dx + _dy * _dy);
            if(_walked + _seg > _target) {
                // The pen is part-way along this piece: the point is cut there, and it is the last one.
                const f32 _t = _seg > 0.0f ? (_target - _walked) / _seg : 0.0f;
                _p = (rde_vec_2F){ _pts[_i - 1].x + _dx * _t, _pts[_i - 1].y + _dy * _t };
                _i = _n;   // the last point
            }
            _walked += _seg;
        }

        _end               = (rde_vec_2F){ _origin.x + _p.x * _scale, _origin.y - _p.y * _scale };
        rde_vec_2F _screen = fude_draw_at(_end);   // where it is drawn (draw.h: a mirrored screen)
        rde_arr_add(&_glyph->_points, &_screen);
        rde_arr_add(&_glyph->_radii,  &_radius);
    }

    const u32 _count = (u32)rde_arr_length(&_glyph->_points);
    rde_rendering_2d_draw_stroke((const rde_vec_2F*)_glyph->_points.memory, (const f32*)_glyph->_radii.memory, _count, _color);
    return _end;
}

// Draws the first _fraction of a stroke (see the header). It is flattened at FUDE_GLYPH_SCREEN_TOLERANCE screen units
// when that is coarser than FUDE_GLYPH_TOLERANCE, because small text does not need the finer points.
rde_vec_2F fude_glyph_stroke(fude_glyph* _glyph, const fude_kanji_stroke* _stroke, rde_vec_2F _origin,
                             f32 _scale, f32 _radius, f32 _fraction, rde_color _color) {
    rde_vec_2F _pts[FUDE_GLYPH_MAX_POINTS];
    f32        _length = 0.0f;
    const u32  _n      = fude_glyph_flatten(_stroke, rde_math_max_f32(FUDE_GLYPH_TOLERANCE, FUDE_GLYPH_SCREEN_TOLERANCE / _scale), _pts, &_length);
    return fude_glyph_draw_points(_glyph, _pts, _n, _length, _origin, _scale, _radius, _fraction, _color);
}

// When _codepoint is a sign written on another letter (lang.h: fude_lang_combining) and is shown alone, draws a
// dotted circle where that letter goes (the middle of the box), as fonts show one. The circle's centre and radius
// are in KanjiVG units.
RDE_INTERNAL void fude_glyph_base_hint(u32 _codepoint, rde_vec_2F _origin, f32 _scale) {
    if(!fude_lang_combining(_codepoint)) {
        return;
    }
    const rde_vec_2F _c    = { _origin.x + 54.5f * _scale, _origin.y - 56.0f * _scale };
    const f32        _r    = 19.0f * _scale;
    const f32        _line = rde_math_max_f32(0.6f, 0.45f * _scale);
    for(u32 _i = 0; _i < 20u; _i++) {
        const f32 _a = (f32)_i * (f32)(2.0 * RDE_PI) / 20.0f;
        const f32 _b = _a + (f32)(2.0 * RDE_PI) / 48.0f;
        fude_draw_line((rde_vec_2F){ _c.x + _r * rde_math_cos_f32(_a), _c.y + _r * rde_math_sin_f32(_a) }, (rde_vec_2F){ _c.x + _r * rde_math_cos_f32(_b), _c.y + _r * rde_math_sin_f32(_b) },
                       _line, fude_theme_active()->sheet_guide);
    }
}

// Draws a whole character (see the header).
b8 fude_glyph_character(fude_glyph* _glyph, u32 _codepoint, rde_vec_2F _origin, f32 _size, rde_color _color) {
    fude_kanji_info _info;
    if(_glyph->db == NULL || !fude_kanji_find(_glyph->db, _codepoint, &_info)) {
        return false;
    }

    const f32 _scale  = _size / FUDE_KANJI_BOX;
    const f32 _radius = rde_math_max_f32(0.8f, _scale * FUDE_GLYPH_WIDTH * 0.5f);
    // A character is never mirrored: on a right-to-left screen its box goes where that screen puts it, and the strokes
    // keep their own direction.
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

// Draws a practice-sheet square (see the header).
void fude_glyph_box(rde_vec_2F _tl, f32 _size) {
    // A card, rounded a little (less when it is small), with a hairline edge; then the dashed guides through its
    // centre.
    fude_draw_card((rde_vec_2F){ _tl.x, _tl.y - _size }, (rde_vec_2F){ _tl.x + _size, _tl.y }, rde_math_min_f32(14.0f, _size * 0.06f),
                   fude_theme_active()->sheet, fude_theme_active()->sheet_outline);

    const f32 _dash = rde_math_max_f32(4.0f, _size / 40.0f);
    const f32 _cx   = _tl.x + _size * 0.5f;
    const f32 _cy   = _tl.y - _size * 0.5f;
    for(f32 _t = _dash * 0.5f; _t < _size - _dash * 0.5f; _t += _dash * 2.0f) {
        const f32 _e = rde_math_min_f32(_t + _dash, _size - _dash * 0.5f);
        fude_draw_line((rde_vec_2F){ _cx, _tl.y - _t }, (rde_vec_2F){ _cx, _tl.y - _e }, 0.7f, fude_theme_active()->sheet_guide);
        fude_draw_line((rde_vec_2F){ _tl.x + _t, _cy }, (rde_vec_2F){ _tl.x + _e, _cy }, 0.7f, fude_theme_active()->sheet_guide);
    }
}

// Declared here so that fude_glyph_writing can come first.
RDE_INTERNAL b8 fude_glyph_writing_strokes(fude_glyph* _glyph, const fude_kanji_info* _info, rde_vec_2F _tl, f32 _scale, f32 _radius, f64 _elapsed,
                                           rde_font* _font, f32 _font_px, f32 _number_px);

// Draws a character writing itself (see the header).
b8 fude_glyph_writing(fude_glyph* _glyph, const fude_kanji_info* _info, rde_vec_2F _tl, f32 _size, f64 _elapsed,
                      rde_font* _font, f32 _font_px, f32 _number_px) {
    const f32 _scale  = _size / FUDE_KANJI_BOX;
    const f32 _radius = rde_math_max_f32(0.8f, _scale * FUDE_GLYPH_WIDTH * 0.5f);

    // It is never mirrored, as a character is not (fude_glyph_character).
    fude_draw_keep_begin((rde_vec_2F){ _tl.x, _tl.y - _size }, (rde_vec_2F){ _tl.x + _size, _tl.y }, false);
    const b8 _done = fude_glyph_writing_strokes(_glyph, _info, _tl, _scale, _radius, _elapsed, _font, _font_px, _number_px);
    fude_draw_keep_end();
    return _done;
}

// Draws fude_glyph_writing's strokes, at _scale screen units per KanjiVG unit. Each stroke takes
// FUDE_GLYPH_STROKE_BASE seconds plus FUDE_GLYPH_STROKE_PER for each unit of its length, with FUDE_GLYPH_STROKE_GAP
// between strokes. Returns true once the last stroke is finished.
RDE_INTERNAL b8 fude_glyph_writing_strokes(fude_glyph* _glyph, const fude_kanji_info* _info, rde_vec_2F _tl, f32 _scale, f32 _radius, f64 _elapsed,
                                           rde_font* _font, f32 _font_px, f32 _number_px) {
    fude_glyph_base_hint(_info->codepoint, _tl, _scale);

    // The ghosts first: the whole shape, faint, so that the eye knows where the writing is going.
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

        // Flattened once, at the finest tolerance: its length times the stroke (the same at any size), and the same
        // points draw it.
        rde_vec_2F _pts[FUDE_GLYPH_MAX_POINTS];
        f32        _length   = 0.0f;
        const u32  _n        = fude_glyph_stroke_points(&_stroke, _pts, &_length);
        const f64  _duration = FUDE_GLYPH_STROKE_BASE + FUDE_GLYPH_STROKE_PER * (f64)_length;

        if(_clock <= 0.0) {
            return false;   // this stroke is not reached yet, so only its ghost shows
        }

        const f32        _fraction = (f32)rde_math_min_f64(1.0, _clock / _duration);
        const rde_vec_2F _tip      = fude_glyph_draw_points(_glyph, _pts, _n, _length, _tl, _scale, _radius, _fraction, fude_theme_active()->ink);

        // The stroke's number goes beside where it starts, because the order is what is being taught.
        if(_number_px > 0.0f && _font != NULL) {
            c8 _num[8];
            rde_snprintf(_num, sizeof(_num), "%u", _s + 1u);
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

// The font for what the data has no strokes for (fude_glyph_set_text_font), and its size in pixels as loaded.
static rde_font* fude_glyph_text_font    = NULL;
static f32       fude_glyph_text_font_px = 14.0f;

// Sets the font for what the data has no strokes for (see the header).
void fude_glyph_set_text_font(rde_font* _font, f32 _font_px) {
    fude_glyph_text_font    = _font;
    fude_glyph_text_font_px = _font_px;
}

// fude_glyph_reading's walk along _text: it draws when _draw is true, and otherwise only measures. Returns the x
// where it ended. A '.' starts the okurigana (drawn in _soft) and '-' is an affix, drawn as a short dash.
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
        if(_cp == 0x3001u) {   // "、": a new reading starts, in _ink again
            _color = _ink;
        }
        fude_kanji_info _info;
        const b8 _strokes = _glyph->db != NULL && fude_kanji_find(_glyph->db, _cp, &_info);
        if(_strokes && _draw) {
            fude_glyph_character(_glyph, _cp, (rde_vec_2F){ _x, _origin.y }, _size, _color);
        }
        if(!_strokes && fude_glyph_text_font != NULL && _cp != 0x3001u) {
            // The data has no strokes for it (a letter of pinyin), so it is drawn in the text font, centred on the
            // line.
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

// Draws a reading as the data writes it (see the header).
f32 fude_glyph_reading(fude_glyph* _glyph, const c8* _text, rde_vec_2F _origin, f32 _size, f32 _max_x, rde_color _ink, rde_color _soft) {
    // It is measured first, so that it can be kept as one run, read left to right however the screen runs (a
    // right-to-left screen puts it, whole, where it starts there). Then it is drawn.
    const f32 _end = fude_glyph_reading_walk(_glyph, _text, _origin, _size, _max_x, _ink, _soft, false);
    fude_draw_keep_begin((rde_vec_2F){ _origin.x, _origin.y - _size }, (rde_vec_2F){ _end, _origin.y }, false);
    fude_glyph_reading_walk(_glyph, _text, _origin, _size, _max_x, _ink, _soft, true);
    fude_draw_keep_end();
    return _end;
}
