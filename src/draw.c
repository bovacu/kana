#include "draw.h"
#include "kanji.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See draw.h.
// ===========================================================================

#define KANA_DRAW_CHUNK 1024u   // points a stroke is drawn in at a time (the radii for them)

void kana_draw_text(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, rde_color _color) {
    if(_font == NULL || _text == NULL) {
        return;
    }
    const f32 _scale = _px / _font_px;
    rde_rendering_2d_draw_text_2(_font, _text, (rde_vec_3F){ _x, _y, 0.0f }, (rde_vec_2F){ _scale, _scale }, 0.0f, _color);
}

void kana_draw_line(rde_vec_2F _a, rde_vec_2F _b, f32 _radius, rde_color _color) {
    const rde_vec_2F _p[2] = { _a, _b };
    const f32        _r[2] = { _radius, _radius };
    rde_rendering_2d_draw_stroke(_p, _r, 2, _color);
}

void kana_draw_outline(rde_vec_2F _min, rde_vec_2F _max, f32 _radius, rde_color _color) {
    const rde_vec_2F _p[5] = { { _min.x, _max.y }, { _max.x, _max.y }, { _max.x, _min.y }, { _min.x, _min.y }, { _min.x, _max.y } };
    const f32        _r[5] = { _radius, _radius, _radius, _radius, _radius };
    rde_rendering_2d_draw_stroke(_p, _r, 5, _color);
}

void kana_draw_stroke_even(const rde_vec_2F* _points, u32 _count, f32 _radius, rde_color _color) {
    static f32 _radii[KANA_DRAW_CHUNK];
    static f32 _filled_with = -1.0f;
    if(_count == 0) {
        return;
    }
    if(_filled_with != _radius) {
        for(u32 _i = 0; _i < KANA_DRAW_CHUNK; _i++) {
            _radii[_i] = _radius;
        }
        _filled_with = _radius;
    }
    // In chunks that share their end point, so a long stroke stays joined.
    for(u32 _start = 0;; _start += KANA_DRAW_CHUNK - 1u) {
        const u32 _n = _count - _start < KANA_DRAW_CHUNK ? _count - _start : KANA_DRAW_CHUNK;
        rde_rendering_2d_draw_stroke(_points + _start, _radii, _n, _color);
        if(_start + _n >= _count) {
            break;
        }
    }
}

// --- text widths -----------------------------------------------------------------

// How wide _probe's characters are on average, per unit of size.
RDE_INTERNAL f32 kana_draw_measure(rde_font* _font, f32 _font_px, const c8* _probe, f32 _otherwise) {
    u32       _n = 0;
    const c8* _p = _probe;
    while(kana_kanji_utf8_next(&_p) != 0) {
        _n++;
    }
    const rde_vec_2F _m = _font != NULL ? rde_rich_text_measure(_probe, _font, 1.0f, 100000.0f, false) : (rde_vec_2F){ 0.0f, 0.0f };
    return _m.x > 0.0f && _n > 0 ? _m.x / ((f32)_n * _font_px) : _otherwise;
}

// A character's advance, per unit of size (measured the first time for a font).
RDE_INTERNAL f32 kana_draw_advance(rde_font* _font, f32 _font_px, u32 _cp) {
    static rde_font* _measured = NULL;
    static f32       _latin    = 0.0f;
    static f32       _japanese = 0.0f;
    if(_measured != _font || _latin <= 0.0f) {
        _measured = _font;
        _latin    = kana_draw_measure(_font, _font_px, "The quick brown fox jumps over the lazy dog; (e.g. lunch, dinner) 0123", 0.6f);
        _japanese = kana_draw_measure(_font, _font_px, "日本語のかなとカタカナ", 1.3f);
    }
    return _cp >= 0x2E80u ? _japanese : _latin;
}

f32 kana_draw_text_width(rde_font* _font, f32 _font_px, const c8* _text, f32 _px) {
    f32 _w = 0.0f;
    for(u32 _cp = kana_kanji_utf8_next(&_text); _cp != 0; _cp = kana_kanji_utf8_next(&_text)) {
        _w += kana_draw_advance(_font, _font_px, _cp) * _px;
    }
    return _w;
}

void kana_draw_text_fit(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width, c8* _out, usize _size) {
    snprintf(_out, _size, "%s", _text);
    if(kana_draw_text_width(_font, _font_px, _text, _px) <= _width) {
        return;
    }
    const f32 _ellipsis = kana_draw_advance(_font, _font_px, 'x') * _px;
    f32       _w        = 0.0f;
    const c8* _p        = _text;
    for(;;) {
        const c8* _before = _p;
        const u32 _cp     = kana_kanji_utf8_next(&_p);
        _w += kana_draw_advance(_font, _font_px, _cp) * _px;
        if(_cp == 0 || _w + _ellipsis > _width) {
            // Back to the last whole word when there is one not far back.
            usize _cut = (usize)(_before - _text);
            for(usize _k = _cut; _k > 0 && _cut - _k < 12u; _k--) {
                if(_text[_k - 1] == ' ') {
                    _cut = _k - 1;
                    break;
                }
            }
            while(_cut > 0 && (_text[_cut - 1] == ' ' || _text[_cut - 1] == ',' || _text[_cut - 1] == ';')) {
                _cut--;
            }
            if(_cut + 4u <= _size) {
                memcpy(_out, _text, _cut);
                memcpy(_out + _cut, "\xE2\x80\xA6", 4u);   // … and its NUL
            }
            return;
        }
    }
}
