#include "draw.h"
#include "kanji.h"
#include "theme.h"

#include <math.h>
#include <stdint.h>
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

// --- the UI's shapes ----------------------------------------------------------------

RDE_INTERNAL rde_font* kana_draw_fill_font    = NULL;   // Phosphor Fill (kana_draw_set_icon_fill)
RDE_INTERNAL f32       kana_draw_fill_font_px = 32.0f;

void kana_draw_set_icon_fill(rde_font* _font, f32 _font_px) {
    kana_draw_fill_font    = _font;
    kana_draw_fill_font_px = _font_px;
}

// Phosphor's glyphs fill their em, from 1/16 of it under the baseline to 15/16
// over: the middle is 7/16 up. Slug's em is KANA_DRAW_EM times the size.
void kana_draw_icon(rde_font* _font, f32 _font_px, const c8* _icon, rde_vec_2F _center, f32 _em, rde_color _color) {
    const f32 _px = _em / KANA_DRAW_EM;
    kana_draw_text(_font, _font_px, _icon, _center.x - _em * 0.5f, _center.y - _em * 0.4375f, _px, _color);
}

void kana_draw_icon_fill(const c8* _icon, rde_vec_2F _center, f32 _em, rde_color _color) {
    kana_draw_icon(kana_draw_fill_font, kana_draw_fill_font_px, _icon, _center, _em, _color);
}

void kana_draw_card(rde_vec_2F _min, rde_vec_2F _max, f32 _radius, rde_color _fill, rde_color _border) {
    const rde_vec_2F _size = { _max.x - _min.x, _max.y - _min.y };
    if(_size.x <= 0.0f || _size.y <= 0.0f) {
        return;
    }
    const f32 _roundness = fminf(1.0f, 2.0f * _radius / fminf(_size.x, _size.y));
    rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ (_min.x + _max.x) * 0.5f, (_min.y + _max.y) * 0.5f }, _size, _roundness, 6,
                                                        _fill, 1.0f, _border, NULL);
}

f32 kana_draw_chip(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _mid, f32 _px, rde_color _fill, rde_color _color) {
    const f32 _h = _px + 12.0f;
    const f32 _w = kana_draw_text_width(_font, _font_px, _text, _px) + 22.0f;
    rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ _x + _w * 0.5f, _mid }, (rde_vec_2F){ _w, _h }, 1.0f, 8, _fill, NULL);
    kana_draw_text(_font, _font_px, _text, _x + 11.0f, _mid - _px * 0.36f, _px, _color);
    return _w;
}

void kana_draw_verdict(rde_vec_2F _center, f32 _radius, b8 _right) {
    const kana_theme* _theme = kana_theme_active();
    rde_rendering_2d_draw_circle(_center, _radius, 24, _right ? _theme->score_good : _theme->score_poor, NULL);
    const f32        _u = _radius * 0.5f;
    const f32        _w = fmaxf(1.2f, _radius * 0.13f);
    const rde_vec_2F _c = _center;
    if(_right) {
        kana_draw_line((rde_vec_2F){ _c.x - _u, _c.y }, (rde_vec_2F){ _c.x - _u * 0.25f, _c.y - _u * 0.7f }, _w, _theme->on_accent);
        kana_draw_line((rde_vec_2F){ _c.x - _u * 0.25f, _c.y - _u * 0.7f }, (rde_vec_2F){ _c.x + _u, _c.y + _u * 0.75f }, _w, _theme->on_accent);
    } else {
        kana_draw_line((rde_vec_2F){ _c.x - _u * 0.7f, _c.y - _u * 0.7f }, (rde_vec_2F){ _c.x + _u * 0.7f, _c.y + _u * 0.7f }, _w, _theme->on_accent);
        kana_draw_line((rde_vec_2F){ _c.x - _u * 0.7f, _c.y + _u * 0.7f }, (rde_vec_2F){ _c.x + _u * 0.7f, _c.y - _u * 0.7f }, _w, _theme->on_accent);
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

// Short texts (labels, chips, titles) are measured for real — laid out once by
// the engine, then kept: an estimate from average advances is off by a third
// for capitals and digits ("N5"), which puts a chip's text off centre. Longer
// ones (meanings, sentences) are estimated.
#define KANA_DRAW_EXACT_BYTES 48u
#define KANA_DRAW_EXACT_SLOTS 512u

typedef struct {
    const rde_font* font;
    u32             hash;
    f32             width;     // per unit of size
    c8              text[KANA_DRAW_EXACT_BYTES];
} kana_draw_exact;

RDE_INTERNAL kana_draw_exact kana_draw_exact_cache[KANA_DRAW_EXACT_SLOTS];
RDE_INTERNAL u32             kana_draw_exact_used = 0;

RDE_INTERNAL f32 kana_draw_exact_width(rde_font* _font, f32 _font_px, const c8* _text, usize _len) {
    u32 _hash = 2166136261u;
    for(usize _i = 0; _i < _len; _i++) {
        _hash = (_hash ^ (u8)_text[_i]) * 16777619u;
    }
    _hash ^= (u32)(uintptr_t)_font;
    u32 _slot = _hash % KANA_DRAW_EXACT_SLOTS;
    for(u32 _probe = 0; _probe < KANA_DRAW_EXACT_SLOTS; _probe++, _slot = (_slot + 1u) % KANA_DRAW_EXACT_SLOTS) {
        kana_draw_exact* _e = &kana_draw_exact_cache[_slot];
        if(_e->font == NULL) {
            break;
        }
        if(_e->font == _font && _e->hash == _hash && strcmp(_e->text, _text) == 0) {
            return _e->width;
        }
    }
    const rde_vec_2F _m = rde_rich_text_measure(_text, _font, 1.0f, 100000.0f, false);
    const f32        _w = _m.x > 0.0f ? _m.x / _font_px : -1.0f;
    if(kana_draw_exact_used >= KANA_DRAW_EXACT_SLOTS * 3u / 4u) {
        memset(kana_draw_exact_cache, 0, sizeof(kana_draw_exact_cache));   // full: start again
        kana_draw_exact_used = 0;
        _slot = _hash % KANA_DRAW_EXACT_SLOTS;
    }
    kana_draw_exact* _e = &kana_draw_exact_cache[_slot];
    _e->font  = _font;
    _e->hash  = _hash;
    _e->width = _w;
    memcpy(_e->text, _text, _len + 1u);
    kana_draw_exact_used++;
    return _w;
}

f32 kana_draw_text_width(rde_font* _font, f32 _font_px, const c8* _text, f32 _px) {
    const usize _len = strlen(_text);
    if(_font != NULL && _len > 0 && _len < KANA_DRAW_EXACT_BYTES && strchr(_text, '[') == NULL) {
        const f32 _w = kana_draw_exact_width(_font, _font_px, _text, _len);
        if(_w > 0.0f) {
            return _w * _px;
        }
    }
    f32 _w = 0.0f;
    for(u32 _cp = kana_kanji_utf8_next(&_text); _cp != 0; _cp = kana_kanji_utf8_next(&_text)) {
        _w += kana_draw_advance(_font, _font_px, _cp) * _px;
    }
    return _w;
}

u32 kana_draw_text_wrap(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, f32 _width, f32 _line, rde_color _color) {
    u32       _lines = 0;
    const c8* _s     = _text;
    c8        _row[512];
    while(*_s != 0) {
        while(*_s == ' ') {
            _s++;
        }
        if(*_s == 0) {
            break;
        }
        // As far as fits: back to the last place a line may break when it does not.
        const c8* _p     = _s;
        const c8* _fit   = _s;
        const c8* _next  = _s;
        const c8* _break = NULL;
        for(;;) {
            const c8* _at = _p;
            const u32 _cp = kana_kanji_utf8_next(&_p);
            if(_cp == 0 || (usize)(_p - _s) >= sizeof(_row)) {
                _fit = _next = _at;
                break;
            }
            if(_cp == '\n') {
                _fit  = _at;
                _next = _p;
                break;
            }
            const usize _n = (usize)(_p - _s);
            memcpy(_row, _s, _n);
            _row[_n] = 0;
            if(_at > _s && kana_draw_text_width(_font, _font_px, _row, _px) > _width) {
                _fit = _next = _break != NULL ? _break : _at;
                break;
            }
            if(_cp == ' ') {
                _break = _at;    // before a space
            } else if(_cp >= 0x2E80u) {
                _break = _p;     // after a Japanese character
            }
        }
        usize _n = (usize)(_fit - _s);
        while(_n > 0 && _s[_n - 1] == ' ') {
            _n--;
        }
        memcpy(_row, _s, _n);
        _row[_n] = 0;
        kana_draw_text(_font, _font_px, _row, _x, _y - (f32)_lines * _line, _px, _color);
        _lines++;
        if(_next == _s) {
            break;   // nothing would fit: stop rather than loop
        }
        _s = _next;
    }
    return _lines;
}

f32 kana_draw_text_px_to_fit(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width, f32 _min_scale) {
    const f32 _w = kana_draw_text_width(_font, _font_px, _text, _px);
    if(_w <= _width || _w <= 0.0f) {
        return _px;
    }
    return _px * fmaxf(_min_scale, _width / _w);
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
