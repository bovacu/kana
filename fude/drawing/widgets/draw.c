// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/widgets/draw.h"
#include "drawing/base/utf8.h"
#include "drawing/base/theme.h"
#include "drawing/widgets/icons.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See draw.h.
// ===========================================================================

#define FUDE_DRAW_CHUNK 1024u   // points a stroke is drawn in at a time (the radii for them)

// --text-check (a developer's, look.h): every text drawn past the screen's sides,
// on the log once — a label that does not fit, cut at the edge.
RDE_INTERNAL rde_window* fude_draw_checking = NULL;   // the window checked against (NULL: off)
RDE_INTERNAL u64         fude_draw_checked[256];

void fude_draw_text_check(rde_window* _window) {
    fude_draw_checking = _window;
}

RDE_INTERNAL u64 fude_draw_hash(const c8* _text, usize _len);

RDE_INTERNAL void fude_draw_check(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _px) {
    if(_text[0] == 0) {
        return;
    }
    const f32 _half = (f32)rde_window_get_size(fude_draw_checking).x * 0.5f;
    const f32 _w    = fude_draw_text_width(_font, _font_px, _text, _px);
    if(_x >= -_half - 0.5f && _x + _w <= _half + 0.5f) {
        return;
    }
    const u64 _hash = fude_draw_hash(_text, strlen(_text));
    for(u32 _i = 0; _i < 256u; _i++) {
        if(fude_draw_checked[_i] == _hash) {
            return;
        }
        if(fude_draw_checked[_i] == 0) {
            fude_draw_checked[_i] = _hash;
            break;
        }
    }
    rde_log_level(RDE_LOG_LEVEL_WARNING, "text-check: past the screen by %.0f: \"%s\" (x %.0f, width %.0f, %.0f px)",
                  fmaxf(-_half - _x, _x + _w - _half), _text, _x, _w, _px);
}

// --- right to left: a screen drawn mirrored (draw.h) ---------------------------------------

#define FUDE_DRAW_KEEPS 128u

// A kept box: laid out from min to max, drawn dx along.
typedef struct {
    rde_vec_2F min, max;
    f32        dx;
} fude_draw_kept;

RDE_INTERNAL struct {
    b8             on;      // a mirrored screen being drawn
    f32            sum;     // its frame's left + right: x is drawn at sum - x
    u32            depth;   // kept boxes open
    f32            dx;      // the outermost open one's shift
    fude_draw_kept kept[FUDE_DRAW_KEEPS];   // this screen's, for its pointer
    u32            kept_count;
    // The pointer's: the last screen drawn's, as it ended.
    b8             in_on;
    f32            in_sum;
    fude_draw_kept in_kept[FUDE_DRAW_KEEPS];
    u32            in_count;
    i32            gesture;   // what a press picked: -1 the mirror, else a kept box
} fude_draw_m = { .gesture = -1 };

void fude_draw_mirror_begin(b8 _on, f32 _left, f32 _right) {
    fude_draw_m.on         = _on;
    fude_draw_m.sum        = _left + _right;
    fude_draw_m.depth      = 0;
    fude_draw_m.kept_count = 0;
}

void fude_draw_mirror_end(void) {
    fude_draw_m.in_on    = fude_draw_m.on;
    fude_draw_m.in_sum   = fude_draw_m.sum;
    fude_draw_m.in_count = fude_draw_m.kept_count;
    memcpy(fude_draw_m.in_kept, fude_draw_m.kept, sizeof(fude_draw_kept) * fude_draw_m.kept_count);
    fude_draw_m.on    = false;
    fude_draw_m.depth = 0;
}

RDE_INTERNAL void fude_draw_keep_open(rde_vec_2F _min, rde_vec_2F _max, b8 _for_pointer) {
    if(fude_draw_m.on && fude_draw_m.depth == 0) {
        fude_draw_m.dx = fude_draw_m.sum - _min.x - _max.x;
        if(_for_pointer && fude_draw_m.kept_count < FUDE_DRAW_KEEPS) {
            fude_draw_m.kept[fude_draw_m.kept_count++] = (fude_draw_kept){ _min, _max, fude_draw_m.dx };
        }
    }
    fude_draw_m.depth++;
}

void fude_draw_keep_begin(rde_vec_2F _min, rde_vec_2F _max, b8 _for_pointer) {
    fude_draw_keep_open(_min, _max, _for_pointer);
}

void fude_draw_keep_end(void) {
    if(fude_draw_m.depth > 0) {
        fude_draw_m.depth--;
    }
}

f32 fude_draw_x(f32 _x) {
    if(!fude_draw_m.on) {
        return _x;
    }
    return fude_draw_m.depth > 0 ? _x + fude_draw_m.dx : fude_draw_m.sum - _x;
}

rde_vec_2F fude_draw_at(rde_vec_2F _p) {
    return (rde_vec_2F){ fude_draw_x(_p.x), _p.y };
}

// A span (a box's left and right) as drawn: mirrored, it swaps ends.
RDE_INTERNAL void fude_draw_span(f32* _a, f32* _b) {
    const f32 _x0 = fude_draw_x(*_a), _x1 = fude_draw_x(*_b);
    *_a = _x0 < _x1 ? _x0 : _x1;
    *_b = _x0 < _x1 ? _x1 : _x0;
}

rde_vec_2F fude_draw_pointer(rde_vec_2F _p, b8 _press) {
    if(!fude_draw_m.in_on) {
        return _p;
    }
    if(_press) {
        // The last box drawn under it (drawn last: on top).
        fude_draw_m.gesture = -1;
        for(u32 _i = fude_draw_m.in_count; _i-- > 0;) {
            const fude_draw_kept* _k = &fude_draw_m.in_kept[_i];
            if(_p.x >= _k->min.x + _k->dx && _p.x <= _k->max.x + _k->dx && _p.y >= _k->min.y && _p.y <= _k->max.y) {
                fude_draw_m.gesture = (i32)_i;
                break;
            }
        }
    }
    if(fude_draw_m.gesture >= 0 && (u32)fude_draw_m.gesture < fude_draw_m.in_count) {
        return (rde_vec_2F){ _p.x - fude_draw_m.in_kept[fude_draw_m.gesture].dx, _p.y };
    }
    return (rde_vec_2F){ fude_draw_m.in_sum - _p.x, _p.y };
}

// _text with its baseline starting at (_x, _y), as it is: where every placed text ends up.
RDE_INTERNAL void fude_draw_text_raw(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, rde_color _color) {
    const f32 _scale = _px / _font_px;
    rde_rendering_2d_draw_text_2(_font, _text, (rde_vec_3F){ _x, _y, 0.0f }, (rde_vec_2F){ _scale, _scale }, 0.0f, _color);
}

// A number alone (a score, "3 / 20", "×2"), which a screen formats itself: in
// the language's digits (its @digits), as the UI strings are (text.c). Texts
// with words are left alone: a meaning or a sentence keeps its own digits.
RDE_INTERNAL const c8* fude_draw_number_digits(const c8* _text, c8* _buf, usize _size) {
    b8 _digit = false;
    for(const c8* _p = _text; *_p != 0; _p++) {
        const u8 _c = (u8)*_p;
        if(_c >= '0' && _c <= '9') {
            _digit = true;
        } else if((_c >= 'A' && _c <= 'Z') || (_c >= 'a' && _c <= 'z')) {
            return _text;
        } else if(_c >= 0x80u && !(_c == 0xC3u && (u8)_p[1] == 0x97u) && !(_c == 0xC2u && (u8)_p[1] == 0xB7u) && !((_c & 0xC0u) == 0x80u)) {
            return _text;   // a letter of any script (only × and · pass)
        }
    }
    if(!_digit || strlen(_text) * 4u + 1u > _size) {
        return _text;
    }
    snprintf(_buf, _size, "%s", _text);
    rde_localization_localize_digits(_buf, _size);
    return _buf;
}

void fude_draw_text(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, rde_color _color) {
    if(_font == NULL || _text == NULL) {
        return;
    }
    c8 _digits[256];
    _text = fude_draw_number_digits(_text, _digits, sizeof(_digits));
    if(fude_draw_checking != NULL) {
        fude_draw_check(_font, _font_px, _text, _x, _px);
    }
    if(!fude_draw_m.on || fude_draw_m.depth > 0) {
        fude_draw_text_raw(_font, _font_px, _text, fude_draw_x(_x), _y, _px, _color);
        return;
    }
    // Mirrored: it ends where it would have started (its width measured on the
    // very shape that is drawn).
    const f32                   _scale = _px / _font_px;
    rde_text_engine_shaped_text _shape = rde_text_engine_shape_text(_font, NULL, _text);
    const f32                   _w     = (f32)_shape.bounding_box.width * _scale;
    rde_rendering_2d_draw_shaped_text_3(_font, &_shape, (rde_vec_3F){ fude_draw_m.sum - _x - _w, _y, 0.0f }, (rde_vec_2F){ _scale, _scale }, 0.0f, _color, NULL, NULL);
    rde_text_engine_free_shaped_text(&_shape);
}

void fude_draw_line(rde_vec_2F _a, rde_vec_2F _b, f32 _radius, rde_color _color) {
    const rde_vec_2F _p[2] = { fude_draw_at(_a), fude_draw_at(_b) };
    const f32        _r[2] = { _radius, _radius };
    rde_rendering_2d_draw_stroke(_p, _r, 2, _color);
}

void fude_draw_outline(rde_vec_2F _min, rde_vec_2F _max, f32 _radius, rde_color _color) {
    fude_draw_span(&_min.x, &_max.x);
    const rde_vec_2F _p[5] = { { _min.x, _max.y }, { _max.x, _max.y }, { _max.x, _min.y }, { _min.x, _min.y }, { _min.x, _max.y } };
    const f32        _r[5] = { _radius, _radius, _radius, _radius, _radius };
    rde_rendering_2d_draw_stroke(_p, _r, 5, _color);
}

void fude_draw_stroke_even(const rde_vec_2F* _points, u32 _count, f32 _radius, rde_color _color) {
    static f32        _radii[FUDE_DRAW_CHUNK];
    static f32        _filled_with = -1.0f;
    static rde_vec_2F _moved[FUDE_DRAW_CHUNK];   // the chunk as drawn, in a mirrored screen
    if(_count == 0) {
        return;
    }
    if(_filled_with != _radius) {
        for(u32 _i = 0; _i < FUDE_DRAW_CHUNK; _i++) {
            _radii[_i] = _radius;
        }
        _filled_with = _radius;
    }
    // In chunks that share their end point, so a long stroke stays joined.
    for(u32 _start = 0;; _start += FUDE_DRAW_CHUNK - 1u) {
        const u32 _n = _count - _start < FUDE_DRAW_CHUNK ? _count - _start : FUDE_DRAW_CHUNK;
        if(fude_draw_m.on) {
            for(u32 _i = 0; _i < _n; _i++) {
                _moved[_i] = fude_draw_at(_points[_start + _i]);
            }
            rde_rendering_2d_draw_stroke(_moved, _radii, _n, _color);
        } else {
            rde_rendering_2d_draw_stroke(_points + _start, _radii, _n, _color);
        }
        if(_start + _n >= _count) {
            break;
        }
    }
}

// --- the UI's shapes ----------------------------------------------------------------

RDE_INTERNAL rde_font* fude_draw_fill_font    = NULL;   // Phosphor Fill (fude_draw_set_icon_fill)
RDE_INTERNAL f32       fude_draw_fill_font_px = 32.0f;

void fude_draw_set_icon_fill(rde_font* _font, f32 _font_px) {
    fude_draw_fill_font    = _font;
    fude_draw_fill_font_px = _font_px;
}

// Right to left (fude_draw_set_rtl): the directional icons as their mirror
// images — Phosphor has each pair (back and forward, previous and next, undo
// and redo), so the other glyph of the pair is drawn.
static b8 fude_draw_rtl = false;

void fude_draw_set_rtl(b8 _rtl) {
    fude_draw_rtl = _rtl;
}

b8 fude_draw_is_rtl(void) {
    return fude_draw_rtl;
}

void fude_draw_icon_label(c8* _out, usize _size, const c8* _icon, const c8* _text) {
    if(fude_draw_rtl) {
        snprintf(_out, _size, FUDE_DRAW_RLM "%s" FUDE_DRAW_RLM " %s", fude_draw_icon_dir(_icon), _text);
    } else {
        snprintf(_out, _size, "%s %s", _icon, _text);
    }
}

const c8* fude_draw_icon_dir(const c8* _icon) {
    static const c8* const _pairs[][2] = {
        { FUDE_ICON_BACK, FUDE_ICON_ARROW_RIGHT }, { FUDE_ICON_PREV, FUDE_ICON_NEXT }, { FUDE_ICON_UNDO, FUDE_ICON_REDO },
    };
    if(!fude_draw_rtl || _icon == NULL) {
        return _icon;
    }
    for(u32 _i = 0; _i < (u32)(sizeof(_pairs) / sizeof(_pairs[0])); _i++) {
        if(strcmp(_icon, _pairs[_i][0]) == 0) {
            return _pairs[_i][1];
        }
        if(strcmp(_icon, _pairs[_i][1]) == 0) {
            return _pairs[_i][0];
        }
    }
    return _icon;
}

// Phosphor's glyphs fill their em, from 1/16 of it under the baseline to 15/16
// over: the middle is 7/16 up. Slug's em is FUDE_DRAW_EM times the size.
void fude_draw_icon(rde_font* _font, f32 _font_px, const c8* _icon, rde_vec_2F _center, f32 _em, rde_color _color) {
    if(_font == NULL || _icon == NULL) {
        return;
    }
    const f32 _px = _em / FUDE_DRAW_EM;
    fude_draw_text_raw(_font, _font_px, fude_draw_icon_dir(_icon), fude_draw_x(_center.x) - _em * 0.5f, _center.y - _em * 0.4375f, _px, _color);   // its middle mirrored, never the icon
}

void fude_draw_icon_fill(const c8* _icon, rde_vec_2F _center, f32 _em, rde_color _color) {
    fude_draw_icon(fude_draw_fill_font, fude_draw_fill_font_px, _icon, _center, _em, _color);
}

void fude_draw_card(rde_vec_2F _min, rde_vec_2F _max, f32 _radius, rde_color _fill, rde_color _border) {
    const rde_vec_2F _size = { _max.x - _min.x, _max.y - _min.y };
    if(_size.x <= 0.0f || _size.y <= 0.0f) {
        return;
    }
    const f32 _roundness = fminf(1.0f, 2.0f * _radius / fminf(_size.x, _size.y));
    rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ fude_draw_x((_min.x + _max.x) * 0.5f), (_min.y + _max.y) * 0.5f }, _size, _roundness, 6,
                                                        _fill, 1.0f, _border, NULL);
}

f32 fude_draw_chip(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _mid, f32 _px, rde_color _fill, rde_color _color) {
    const f32 _h = _px + 12.0f;
    const f32 _w = fude_draw_text_width(_font, _font_px, _text, _px) + 22.0f;
    rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ fude_draw_x(_x + _w * 0.5f), _mid }, (rde_vec_2F){ _w, _h }, 1.0f, 8, _fill, NULL);
    fude_draw_text(_font, _font_px, _text, _x + 11.0f, _mid - _px * 0.36f, _px, _color);
    return _w;
}

void fude_draw_verdict(rde_vec_2F _center, f32 _radius, b8 _right) {
    const fude_theme* _theme = fude_theme_active();
    // A tick is never mirrored (as the platforms keep it).
    fude_draw_keep_begin((rde_vec_2F){ _center.x - _radius, _center.y - _radius }, (rde_vec_2F){ _center.x + _radius, _center.y + _radius }, false);
    rde_rendering_2d_draw_circle(fude_draw_at(_center), _radius, 24, _right ? _theme->score_good : _theme->score_poor, NULL);
    const f32        _u = _radius * 0.5f;
    const f32        _w = fmaxf(1.2f, _radius * 0.13f);
    const rde_vec_2F _c = _center;
    if(_right) {
        fude_draw_line((rde_vec_2F){ _c.x - _u, _c.y }, (rde_vec_2F){ _c.x - _u * 0.25f, _c.y - _u * 0.7f }, _w, _theme->on_accent);
        fude_draw_line((rde_vec_2F){ _c.x - _u * 0.25f, _c.y - _u * 0.7f }, (rde_vec_2F){ _c.x + _u, _c.y + _u * 0.75f }, _w, _theme->on_accent);
    } else {
        fude_draw_line((rde_vec_2F){ _c.x - _u * 0.7f, _c.y - _u * 0.7f }, (rde_vec_2F){ _c.x + _u * 0.7f, _c.y + _u * 0.7f }, _w, _theme->on_accent);
        fude_draw_line((rde_vec_2F){ _c.x - _u * 0.7f, _c.y + _u * 0.7f }, (rde_vec_2F){ _c.x + _u * 0.7f, _c.y - _u * 0.7f }, _w, _theme->on_accent);
    }
    fude_draw_keep_end();
}

// --- text widths -----------------------------------------------------------------

// How wide _probe's characters are on average, per unit of size.
RDE_INTERNAL f32 fude_draw_measure(rde_font* _font, f32 _font_px, const c8* _probe, f32 _otherwise) {
    u32       _n = 0;
    const c8* _p = _probe;
    while(fude_utf8_next(&_p) != 0) {
        _n++;
    }
    const rde_vec_2F _m = _font != NULL ? rde_rich_text_measure(_probe, _font, 1.0f, 100000.0f, false) : (rde_vec_2F){ 0.0f, 0.0f };
    return _m.x > 0.0f && _n > 0 ? _m.x / ((f32)_n * _font_px) : _otherwise;
}

// A character's advance, per unit of size (measured the first time for a font).
RDE_INTERNAL f32 fude_draw_advance(rde_font* _font, f32 _font_px, u32 _cp) {
    static rde_font* _measured = NULL;
    static f32       _latin    = 0.0f;
    static f32       _japanese = 0.0f;
    if(_measured != _font || _latin <= 0.0f) {
        _measured = _font;
        _latin    = fude_draw_measure(_font, _font_px, "The quick brown fox jumps over the lazy dog; (e.g. lunch, dinner) 0123", 0.6f);
        _japanese = fude_draw_measure(_font, _font_px, "日本語のかなとカタカナ", 1.3f);
    }
    return _cp >= 0x2E80u ? _japanese : _latin;
}

// Every text is measured for real — laid out once by the engine, then kept (by
// a 64-bit hash of it and its length): an estimate from average advances runs
// short on real sentences (a notice wider than its pill, a wrapped line past its
// card) and is off by a third for capitals and digits ("N5").
#define FUDE_DRAW_EXACT_SLOTS 2048u

typedef struct {
    const rde_font* font;
    u64             hash;
    u32             length;
    f32             width;     // per unit of size
} fude_draw_exact;

RDE_INTERNAL fude_draw_exact fude_draw_exact_cache[FUDE_DRAW_EXACT_SLOTS];
RDE_INTERNAL u32             fude_draw_exact_used = 0;

RDE_INTERNAL u64 fude_draw_hash(const c8* _text, usize _len) {
    u64 _hash = 14695981039346656037ull;
    for(usize _i = 0; _i < _len; _i++) {
        _hash = (_hash ^ (u8)_text[_i]) * 1099511628211ull;
    }
    return _hash;
}

RDE_INTERNAL f32 fude_draw_exact_width(rde_font* _font, f32 _font_px, const c8* _text, usize _len) {
    const u64 _hash = fude_draw_hash(_text, _len) ^ (u64)(uintptr_t)_font;
    u32 _slot = (u32)(_hash % FUDE_DRAW_EXACT_SLOTS);
    for(u32 _probe = 0; _probe < FUDE_DRAW_EXACT_SLOTS; _probe++, _slot = (_slot + 1u) % FUDE_DRAW_EXACT_SLOTS) {
        fude_draw_exact* _e = &fude_draw_exact_cache[_slot];
        if(_e->font == NULL) {
            break;
        }
        if(_e->font == _font && _e->hash == _hash && _e->length == (u32)_len) {
            return _e->width;
        }
    }
    const rde_vec_2F _m = rde_rich_text_measure(_text, _font, 1.0f, 100000.0f, false);
    const f32        _w = _m.x > 0.0f ? _m.x / _font_px : -1.0f;
    if(fude_draw_exact_used >= FUDE_DRAW_EXACT_SLOTS * 3u / 4u) {
        memset(fude_draw_exact_cache, 0, sizeof(fude_draw_exact_cache));   // full: start again
        fude_draw_exact_used = 0;
        _slot = (u32)(_hash % FUDE_DRAW_EXACT_SLOTS);
    }
    fude_draw_exact* _e = &fude_draw_exact_cache[_slot];
    _e->font   = _font;
    _e->hash   = _hash;
    _e->length = (u32)_len;
    _e->width  = _w;
    fude_draw_exact_used++;
    return _w;
}

f32 fude_draw_text_width(rde_font* _font, f32 _font_px, const c8* _text, f32 _px) {
    const usize _len = strlen(_text);
    if(_font != NULL && _len > 0 && strchr(_text, '[') == NULL) {   // [ is the engine's markup: estimated
        const f32 _w = fude_draw_exact_width(_font, _font_px, _text, _len);
        if(_w > 0.0f) {
            return _w * _px;
        }
    }
    f32 _w = 0.0f;
    for(u32 _cp = fude_utf8_next(&_text); _cp != 0; _cp = fude_utf8_next(&_text)) {
        _w += fude_draw_advance(_font, _font_px, _cp) * _px;
    }
    return _w;
}

// --- wrapping -------------------------------------------------------------------

// A text's lines for a width and size, kept: laid out once (each candidate line
// measured where a line may break), then drawn from where each line starts and
// ends, every frame.
#define FUDE_DRAW_WRAP_SLOTS 64u
#define FUDE_DRAW_WRAP_LINES 32u

typedef struct {
    const rde_font* font;
    u64             hash;
    u32             length;
    f32             width, px;
    u32             lines;
    u16             from[FUDE_DRAW_WRAP_LINES], to[FUDE_DRAW_WRAP_LINES];   // each line's bytes
} fude_draw_wrapped;

RDE_INTERNAL fude_draw_wrapped fude_draw_wrap_cache[FUDE_DRAW_WRAP_SLOTS];
RDE_INTERNAL u32               fude_draw_wrap_next = 0;

// _text[_from.._to), as wide as it draws.
RDE_INTERNAL f32 fude_draw_span_width(rde_font* _font, f32 _font_px, const c8* _text, usize _from, usize _to, f32 _px) {
    c8          _row[512];
    const usize _n = _to - _from < sizeof(_row) - 1u ? _to - _from : sizeof(_row) - 1u;
    memcpy(_row, _text + _from, _n);
    _row[_n] = 0;
    return fude_draw_text_width(_font, _font_px, _row, _px);
}

// Japanese and Chinese line breaking (kinsoku): what never starts a line —
// closing punctuation and brackets, the small kana, the long-vowel mark, the
// iteration marks — and what never ends one: an opening bracket.
RDE_INTERNAL b8 fude_draw_no_line_start(u32 _cp) {
    switch(_cp) {
        case 0x3001: case 0x3002: case 0xFF0C: case 0xFF0E: case 0x30FB: case 0xFF1A: case 0xFF1B: case 0xFF01: case 0xFF1F:   // 、。，．・：；！？
        case 0x300D: case 0x300F: case 0xFF09: case 0x3015: case 0xFF3D: case 0xFF5D: case 0x3009: case 0x300B: case 0x3011:   // 」』）〕］｝〉》】
        case 0x30FC: case 0x3005: case 0x309D: case 0x309E: case 0x30FD: case 0x30FE: case 0x2026: case 0x2025:                // ー々ゝゞヽヾ…‥
        case 0x3041: case 0x3043: case 0x3045: case 0x3047: case 0x3049: case 0x3063: case 0x3083: case 0x3085: case 0x3087:   // ぁぃぅぇぉっゃゅょ
        case 0x308E: case 0x30A1: case 0x30A3: case 0x30A5: case 0x30A7: case 0x30A9: case 0x30C3: case 0x30E3: case 0x30E5:   // ゎァィゥェォッャュ
        case 0x30E7: case 0x30EE: case 0x30F5: case 0x30F6:                                                                    // ョヮヵヶ
        case ',': case '.': case ')': case '!': case '?': case ':': case ';':
            return true;
        default:
            return false;
    }
}

RDE_INTERNAL b8 fude_draw_no_line_end(u32 _cp) {
    switch(_cp) {
        case 0x300C: case 0x300E: case 0xFF08: case 0x3014: case 0xFF3B: case 0xFF5B: case 0x3008: case 0x300A: case 0x3010:   // 「『（〔［｛〈《【
            return true;
        default:
            return false;
    }
}

RDE_INTERNAL const fude_draw_wrapped* fude_draw_wrap_layout(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width) {
    const usize _len  = strlen(_text);
    const u64   _hash = fude_draw_hash(_text, _len);
    for(u32 _i = 0; _i < FUDE_DRAW_WRAP_SLOTS; _i++) {
        const fude_draw_wrapped* _w = &fude_draw_wrap_cache[_i];
        if(_w->font == _font && _w->hash == _hash && _w->length == (u32)_len && _w->width == _width && _w->px == _px) {
            return _w;
        }
    }
    fude_draw_wrapped* _w = &fude_draw_wrap_cache[fude_draw_wrap_next];
    fude_draw_wrap_next = (fude_draw_wrap_next + 1u) % FUDE_DRAW_WRAP_SLOTS;
    *_w = (fude_draw_wrapped){ .font = _font, .hash = _hash, .length = (u32)_len, .width = _width, .px = _px };

    usize _s = 0;   // the line's start
    while(_s < _len && _len < 0xFFFFu && _w->lines < FUDE_DRAW_WRAP_LINES) {
        while(_s < _len && _text[_s] == ' ') {
            _s++;
        }
        if(_s >= _len) {
            break;
        }
        // Each place the line may end (before a space, after a CJK character, at a
        // line break or the end), measured; the last that fits ends it.
        usize     _end  = _s;   // the line so far: _text[_s.._end)
        usize     _next = _s;   // where the next line starts
        const c8* _p    = _text + _s;
        for(;;) {
            const usize _here = (usize)(_p - _text);
            const u32   _cp   = fude_utf8_next(&_p);
            const usize _past = (usize)(_p - _text);
            const b8    _gap  = _cp == ' ' || _cp == '\n' || _cp == 0;
            if(!_gap && _cp < 0x2E80u) {
                continue;   // inside a word
            }
            if(!_gap) {   // after a CJK character: not before what may not start a line, nor after what may not end one
                const c8* _peek = _p;
                if(fude_draw_no_line_end(_cp) || fude_draw_no_line_start(fude_utf8_next(&_peek))) {
                    continue;
                }
            }
            const usize _try = _gap ? _here : _past;
            if(_try > _s && fude_draw_span_width(_font, _font_px, _text, _s, _try, _px) > _width) {
                if(_end == _s) {
                    // Not one place fits (a word wider than the line): as many characters as do, one at least.
                    const c8* _q = _text + _s;
                    while((usize)(_q - _text) < _try) {
                        const c8* _was = _q;
                        fude_utf8_next(&_q);
                        if(_end > _s && fude_draw_span_width(_font, _font_px, _text, _s, (usize)(_q - _text), _px) > _width) {
                            _q = _was;
                            break;
                        }
                        _end = (usize)(_q - _text);
                    }
                    _next = _end;
                }
                break;
            }
            _end  = _try;
            _next = _cp == 0 ? _here : _past;
            if(_cp == 0 || _cp == '\n') {
                break;
            }
        }
        while(_end > _s && _text[_end - 1u] == ' ') {
            _end--;
        }
        _w->from[_w->lines] = (u16)_s;
        _w->to[_w->lines]   = (u16)_end;
        _w->lines++;
        if(_next <= _s) {
            break;   // nothing would fit: stop rather than loop
        }
        _s = _next;
    }
    return _w;
}

// fude_draw_text_wrap, drawing or (_draw false) only counting the lines.
RDE_INTERNAL u32 fude_draw_text_wrap_do(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, f32 _width, f32 _line, rde_color _color, b8 _draw) {
    if(_text == NULL || _text[0] == 0) {
        return 0;
    }
    const fude_draw_wrapped* _w = fude_draw_wrap_layout(_font, _font_px, _text, _px, _width);
    if(_draw) {
        c8 _row[512];
        for(u32 _i = 0; _i < _w->lines; _i++) {
            const usize _n = (usize)(_w->to[_i] - _w->from[_i]) < sizeof(_row) - 1u ? (usize)(_w->to[_i] - _w->from[_i]) : sizeof(_row) - 1u;
            memcpy(_row, _text + _w->from[_i], _n);
            _row[_n] = 0;
            fude_draw_text(_font, _font_px, _row, _x, _y - (f32)_i * _line, _px, _color);
        }
    }
    return _w->lines;
}

// The narrowest width that wraps _text in as few lines as _width does — its
// lines even, no word left alone on the last (kept: a search of a dozen layouts).
#define FUDE_DRAW_BALANCE_SLOTS 32u
RDE_INTERNAL struct { const rde_font* font; u64 hash; u32 length; f32 width, px, balanced; } fude_draw_balance_cache[FUDE_DRAW_BALANCE_SLOTS];
RDE_INTERNAL u32 fude_draw_balance_next = 0;

f32 fude_draw_text_balanced_width(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width) {
    if(_font == NULL || _text == NULL || _text[0] == 0) {
        return _width;
    }
    const usize _len  = strlen(_text);
    const u64   _hash = fude_draw_hash(_text, _len);
    for(u32 _i = 0; _i < FUDE_DRAW_BALANCE_SLOTS; _i++) {
        if(fude_draw_balance_cache[_i].font == _font && fude_draw_balance_cache[_i].hash == _hash && fude_draw_balance_cache[_i].length == (u32)_len &&
           fude_draw_balance_cache[_i].width == _width && fude_draw_balance_cache[_i].px == _px) {
            return fude_draw_balance_cache[_i].balanced;
        }
    }
    const u32 _lines = fude_draw_text_wrap_lines(_font, _font_px, _text, _px, _width);
    f32       _best  = _width;
    if(_lines > 1u) {
        f32 _lo = _width / (f32)_lines * 0.9f;
        f32 _hi = _width;
        for(u32 _step = 0; _step < 12u; _step++) {
            const f32 _mid = (_lo + _hi) * 0.5f;
            if(fude_draw_text_wrap_lines(_font, _font_px, _text, _px, _mid) <= _lines) { _hi = _mid; _best = _mid; }
            else                                                                     { _lo = _mid; }
        }
        _best = fminf(_width, _best + 1.0f);
    }
    fude_draw_balance_cache[fude_draw_balance_next].font     = _font;
    fude_draw_balance_cache[fude_draw_balance_next].hash     = _hash;
    fude_draw_balance_cache[fude_draw_balance_next].length   = (u32)_len;
    fude_draw_balance_cache[fude_draw_balance_next].width    = _width;
    fude_draw_balance_cache[fude_draw_balance_next].px       = _px;
    fude_draw_balance_cache[fude_draw_balance_next].balanced = _best;
    fude_draw_balance_next = (fude_draw_balance_next + 1u) % FUDE_DRAW_BALANCE_SLOTS;
    return _best;
}

u32 fude_draw_text_wrap(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, f32 _width, f32 _line, rde_color _color) {
    return fude_draw_text_wrap_do(_font, _font_px, _text, _x, _y, _px, _width, _line, _color, true);
}

u32 fude_draw_text_wrap_lines(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width) {
    return fude_draw_text_wrap_do(_font, _font_px, _text, 0.0f, 0.0f, _px, _width, 0.0f, (rde_color){ 0, 0, 0, 0 }, false);
}

f32 fude_draw_text_px_to_fit(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width) {
    const f32 _w = fude_draw_text_width(_font, _font_px, _text, _px);
    if(_w <= _width || _w <= 0.0f) {
        return _px;
    }
    return _px * _width / _w;
}

f32 fude_draw_text_whole(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _mid, f32 _px, f32 _width, u32 _lines, rde_color _color) {
    if(_text == NULL || _text[0] == 0) {
        return _px;
    }
    // One line: as it is, or smaller — to three quarters, or as small as it
    // takes when it may have only one.
    const f32 _p1 = fude_draw_text_px_to_fit(_font, _font_px, _text, _px, _width);
    if(_p1 >= _px * 0.75f || _lines < 2u) {
        fude_draw_text(_font, _font_px, _text, _x, _mid - _p1 * 0.38f, _p1, _color);
        return _p1;
    }
    // More lines, a little smaller each step until they are few enough.
    f32 _p = _px * 0.92f;
    u32 _n = fude_draw_text_wrap_lines(_font, _font_px, _text, _p, _width);
    while(_n > _lines && _p > _px * 0.5f) {
        _p -= 0.5f;
        _n  = fude_draw_text_wrap_lines(_font, _font_px, _text, _p, _width);
    }
    const f32 _line = _p * 1.18f;
    fude_draw_text_wrap(_font, _font_px, _text, _x, _mid + (f32)(_n - 1u) * _line * 0.5f - _p * 0.38f, _p, _width, _line, _color);
    return _p;
}
