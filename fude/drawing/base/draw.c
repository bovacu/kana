// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/draw.h"
#include "drawing/base/utf8.h"
#include "drawing/base/theme.h"
#include "drawing/base/icons.h"

#include <stdint.h>

// ===========================================================================
// See draw.h.
//
// The file keeps its state between calls, all on the main thread: the mirror (fude_draw_m: the screen being drawn
// right to left, its kept boxes, and the last screen's copy that the pointer is mapped through), the measured text
// widths (1,536 kept: fude_draw_exact_lru, by the font, its size and the text), the wrapped layouts (64 kept), the
// balanced widths (32 kept) and the sizes of texts drawn whole (32 kept). The layouts, the balanced widths and the
// whole sizes are each kept in an rde_lru too, by the whole of what they are made from (the font and its size, the
// size drawn at, the width and every byte of the text: fude_draw_layout_key), so two texts never share one; in each,
// the one used longest ago makes room.
// ===========================================================================

#define FUDE_DRAW_CHUNK 1024u   // How many points a stroke is drawn in at a time; also the size of its radii array.

// The developer's --text-check (look.h): each text drawn past the screen's sides, such as a label that does not fit
// and is cut at the edge, is logged once.
RDE_INTERNAL rde_window* fude_draw_checking = NULL;   // The window checked against; NULL when the check is off.
// The hashes of the texts already logged, however many, so none is logged again each frame. Made on the std allocator
// when the check is turned on, freed when it is turned off.
RDE_INTERNAL rde_hash_set TYPE(u64) fude_draw_checked;

// Turns the text check on against _window, or off with NULL.
void fude_draw_text_check(rde_window* _window) {
    fude_draw_checking = _window;
    if(_window != NULL && !rde_hash_set_is_inited(&fude_draw_checked)) {
        fude_draw_checked = rde_hash_set_new(sizeof(u64), rde_hash_map_fn_u64_hash, rde_hash_map_fn_u64_cmp, NULL, rde_memory_allocator_get_default_std());
    } else if(_window == NULL && rde_hash_set_is_inited(&fude_draw_checked)) {
        rde_hash_set_free(&fude_draw_checked);
    }
}

// Logs _text, drawn from _x at _px, when it reaches past either side of the checked window, unless it was logged
// before. Screen x runs from minus half the window's width to plus half.
RDE_INTERNAL void fude_draw_check(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _px) {
    if(_text[0] == 0) {
        return;
    }
    const f32 _half = (f32)rde_window_get_size(fude_draw_checking).x * 0.5f;
    const f32 _w    = fude_draw_text_width(_font, _font_px, _text, _px);
    if(_x >= -_half - 0.5f && _x + _w <= _half + 0.5f) {
        return;
    }
    const u64 _hash = rde_hash_64(_text, rde_strlen(_text));
    if(!rde_hash_set_add(&fude_draw_checked, &_hash)) {
        return;   // Logged before.
    }
    rde_log_level(RDE_LOG_LEVEL_WARNING, "text-check: past the screen by %.0f: \"%s\" (x %.0f, width %.0f, %.0f px)",
                  rde_math_max_f32(-_half - _x, _x + _w - _half), _text, _x, _w, _px);
}

// --- right to left: a screen drawn mirrored (draw.h) ---------------------------------------

// The most kept boxes one screen records for the pointer; more are still drawn kept, but a press inside them is mapped
// through the mirror. A fixed array, because the state is copied whole at the end of every screen.
#define FUDE_DRAW_KEEPS 128u

// A kept box: laid out from min to max, and drawn moved dx along x.
typedef struct {
    rde_vec_2F min, max;
    f32        dx;
} fude_draw_kept;

// The mirror's state: the screen being drawn, and a copy of the last screen drawn, which the pointer is mapped through.
RDE_INTERNAL struct {
    b8             on;      // True while a mirrored screen is being drawn.
    f32            sum;     // The screen's left + right: a laid-out x is drawn at sum - x.
    u32            depth;   // How many kept boxes are open (they nest).
    f32            dx;      // How far the outermost open kept box is moved.
    fude_draw_kept kept[FUDE_DRAW_KEEPS];   // This screen's kept boxes that a press maps through.
    u32            kept_count;
    // The pointer's copy: the last screen drawn, as it was when it ended.
    b8             in_on;
    f32            in_sum;
    fude_draw_kept in_kept[FUDE_DRAW_KEEPS];
    u32            in_count;
    i32            gesture;   // What the last press picked: -1 for the mirror, else the index of a kept box.
} fude_draw_m = { .gesture = -1 };

// Starts a screen: mirrored about the middle of _left.._right when _on, as it is otherwise. No kept box is open.
void fude_draw_mirror_begin(b8 _on, f32 _left, f32 _right) {
    fude_draw_m.on         = _on;
    fude_draw_m.sum        = _left + _right;
    fude_draw_m.depth      = 0;
    fude_draw_m.kept_count = 0;
}

// Ends a screen: copies its mirror and kept boxes for the pointer to use until the next screen ends, then stops
// mirroring.
void fude_draw_mirror_end(void) {
    fude_draw_m.in_on    = fude_draw_m.on;
    fude_draw_m.in_sum   = fude_draw_m.sum;
    fude_draw_m.in_count = fude_draw_m.kept_count;
    rde_memcpy(fude_draw_m.in_kept, fude_draw_m.kept, sizeof(fude_draw_kept) * fude_draw_m.kept_count);
    fude_draw_m.on    = false;
    fude_draw_m.depth = 0;
}

// Opens a kept box. Only the outermost box sets the move: its laid-out box goes to its mirrored place, so it moves by
// sum - min.x - max.x. With _for_pointer it is recorded for the pointer, while there is room for it.
void fude_draw_keep_begin(rde_vec_2F _min, rde_vec_2F _max, b8 _for_pointer) {
    if(fude_draw_m.on && fude_draw_m.depth == 0) {
        fude_draw_m.dx = fude_draw_m.sum - _min.x - _max.x;
        if(_for_pointer && fude_draw_m.kept_count < FUDE_DRAW_KEEPS) {
            fude_draw_m.kept[fude_draw_m.kept_count++] = (fude_draw_kept){ _min, _max, fude_draw_m.dx };
        }
    }
    fude_draw_m.depth++;
}

// Closes the innermost kept box; an extra call does nothing.
void fude_draw_keep_end(void) {
    if(fude_draw_m.depth > 0) {
        fude_draw_m.depth--;
    }
}

// Returns where a laid-out x is drawn: as it is, moved with the open kept box, or mirrored (sum - x).
f32 fude_draw_x(f32 _x) {
    if(!fude_draw_m.on) {
        return _x;
    }
    return fude_draw_m.depth > 0 ? _x + fude_draw_m.dx : fude_draw_m.sum - _x;
}

// Returns where a laid-out point is drawn; only its x changes (see fude_draw_x).
rde_vec_2F fude_draw_at(rde_vec_2F _p) {
    return (rde_vec_2F){ fude_draw_x(_p.x), _p.y };
}

// Turns a span (a box's left and right) into where it is drawn, keeping *_a the smaller, since mirroring swaps the
// ends.
RDE_INTERNAL void fude_draw_span(f32* _a, f32* _b) {
    const f32 _x0 = fude_draw_x(*_a), _x1 = fude_draw_x(*_b);
    *_a = _x0 < _x1 ? _x0 : _x1;
    *_b = _x0 < _x1 ? _x1 : _x0;
}

// Maps a pointer position through the last screen drawn: back through a kept box when the gesture's press landed in
// one, else back through the mirror. See the header.
rde_vec_2F fude_draw_pointer(rde_vec_2F _p, b8 _press) {
    if(!fude_draw_m.in_on) {
        return _p;
    }
    if(_press) {
        // Pick the last kept box drawn under the press, since what is drawn last is on top.
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

// Draws _text with its baseline starting at (_x, _y), exactly there: no mirroring and no change of digits. Placed
// texts and icons end up here; only a text mirrored whole is drawn from its shape instead (fude_draw_text).
RDE_INTERNAL void fude_draw_text_raw(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, rde_color _color) {
    const f32 _scale = _px / _font_px;
    rde_rendering_2d_draw_text_2(_font, _text, (rde_vec_3F){ _x, _y, 0.0f }, (rde_vec_2F){ _scale, _scale }, 0.0f, _color);
}

// When _text is only a number (a score, "3 / 20", "×2"), which a screen formats itself, returns it in _buf in the
// language's own digits (its @digits), as the UI strings are (text.c). Returns _text itself when it holds any letter
// of any script, since a meaning or a sentence keeps its own digits, when it has no digit, or when _buf (_size bytes)
// might be too small (a localized digit can take up to 4 bytes).
RDE_INTERNAL const c8* fude_draw_number_digits(const c8* _text, c8* _buf, usize _size) {
    b8 _digit = false;
    for(const c8* _p = _text; *_p != 0; _p++) {
        const u8 _c = (u8)*_p;
        if(_c >= '0' && _c <= '9') {
            _digit = true;
        } else if((_c >= 'A' && _c <= 'Z') || (_c >= 'a' && _c <= 'z')) {
            return _text;
        } else if(_c >= 0x80u && !(_c == 0xC3u && (u8)_p[1] == 0x97u) && !(_c == 0xC2u && (u8)_p[1] == 0xB7u) && !((_c & 0xC0u) == 0x80u)) {
            return _text;   // A letter of any script; only × and · pass.
        }
    }
    if(!_digit || rde_strlen(_text) * 4u + 1u > _size) {
        return _text;
    }
    rde_snprintf(_buf, _size, "%s", _text);
    rde_localization_localize_digits(_buf, _size);
    return _buf;
}

// Draws a text at its place, in the language's digits when it is only a number. In a mirrored screen it is placed so
// that it ends where it would have started. See the header.
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
    // Mirrored: the text ends where it would have started. Its width is taken from the very shape that is drawn, so
    // the two always agree. It is shaped once a call, as the engine shapes a text drawn from the left
    // (rde_rendering_2d_draw_text_2 shapes, draws and frees it): a right-to-left screen costs no more than a
    // left-to-right one.
    const f32                   _scale = _px / _font_px;
    rde_text_engine_shaped_text _shape = rde_text_engine_shape_text(_font, NULL, _text);
    const f32                   _w     = (f32)_shape.bounding_box.width * _scale;
    rde_rendering_2d_draw_shaped_text_3(_font, &_shape, (rde_vec_3F){ fude_draw_m.sum - _x - _w, _y, 0.0f }, (rde_vec_2F){ _scale, _scale }, 0.0f, _color, NULL, NULL);
    rde_text_engine_free_shaped_text(&_shape);
}

// Draws a line as a two-point stroke of even radius, at its drawn place.
void fude_draw_line(rde_vec_2F _a, rde_vec_2F _b, f32 _radius, rde_color _color) {
    const rde_vec_2F _p[2] = { fude_draw_at(_a), fude_draw_at(_b) };
    const f32        _r[2] = { _radius, _radius };
    rde_rendering_2d_draw_stroke(_p, _r, 2, _color);
}

// Draws a box's outline as one closed five-point stroke, at its drawn place.
void fude_draw_outline(rde_vec_2F _min, rde_vec_2F _max, f32 _radius, rde_color _color) {
    fude_draw_span(&_min.x, &_max.x);
    const rde_vec_2F _p[5] = { { _min.x, _max.y }, { _max.x, _max.y }, { _max.x, _min.y }, { _min.x, _min.y }, { _min.x, _max.y } };
    const f32        _r[5] = { _radius, _radius, _radius, _radius, _radius };
    rde_rendering_2d_draw_stroke(_p, _r, 5, _color);
}

// Draws a stroke of even radius in chunks of FUDE_DRAW_CHUNK points, because the engine takes a radius per point. The
// radii array is static and refilled only when the radius changes; in a mirrored screen each chunk's points are moved
// into a second static array first.
void fude_draw_stroke_even(const rde_vec_2F* _points, u32 _count, f32 _radius, rde_color _color) {
    static f32        _radii[FUDE_DRAW_CHUNK];
    static f32        _filled_with = -1.0f;
    static rde_vec_2F _moved[FUDE_DRAW_CHUNK];   // The chunk as drawn, in a mirrored screen.
    if(_count == 0) {
        return;
    }
    if(_filled_with != _radius) {
        for(u32 _i = 0; _i < FUDE_DRAW_CHUNK; _i++) {
            _radii[_i] = _radius;
        }
        _filled_with = _radius;
    }
    // Each chunk starts at the last point of the one before, so a long stroke stays joined.
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

RDE_INTERNAL rde_font* fude_draw_fill_font    = NULL;   // The Phosphor Fill font (fude_draw_set_icon_fill).
// The size the Fill font was loaded at, in pixels.
RDE_INTERNAL f32       fude_draw_fill_font_px = 32.0f;

// Keeps the Phosphor Fill font for fude_draw_icon_fill; the app still owns it.
void fude_draw_set_icon_fill(rde_font* _font, f32 _font_px) {
    fude_draw_fill_font    = _font;
    fude_draw_fill_font_px = _font_px;
}

// True when the UI is right to left (fude_draw_set_rtl). Directional icons are then drawn as their mirror images:
// Phosphor has each of them in a pair (back and forward, previous and next, undo and redo), so the other glyph of the
// pair is drawn.
static b8 fude_draw_rtl = false;

// Sets whether the UI is right to left.
void fude_draw_set_rtl(b8 _rtl) {
    fude_draw_rtl = _rtl;
}

// Returns whether the UI is right to left.
b8 fude_draw_is_rtl(void) {
    return fude_draw_rtl;
}

// Writes "<icon> <text>" into _out; right to left, the turned icon is set between right-to-left marks. See the header.
void fude_draw_icon_label(c8* _out, usize _size, const c8* _icon, const c8* _text) {
    if(fude_draw_rtl) {
        rde_snprintf(_out, _size, FUDE_DRAW_RLM "%s" FUDE_DRAW_RLM " %s", fude_draw_icon_dir(_icon), _text);
    } else {
        rde_snprintf(_out, _size, "%s %s", _icon, _text);
    }
}

// Returns the other glyph of a directional icon's pair when the UI is right to left, and any other icon as it is.
const c8* fude_draw_icon_dir(const c8* _icon) {
    static const c8* const _pairs[][2] = {
        { FUDE_ICON_BACK, FUDE_ICON_ARROW_RIGHT }, { FUDE_ICON_PREV, FUDE_ICON_NEXT }, { FUDE_ICON_UNDO, FUDE_ICON_REDO },
    };
    if(!fude_draw_rtl || _icon == NULL) {
        return _icon;
    }
    for(u32 _i = 0; _i < (u32)(sizeof(_pairs) / sizeof(_pairs[0])); _i++) {
        if(rde_strcmp(_icon, _pairs[_i][0]) == 0) {
            return _pairs[_i][1];
        }
        if(rde_strcmp(_icon, _pairs[_i][1]) == 0) {
            return _pairs[_i][0];
        }
    }
    return _icon;
}

// Draws an icon centred on _center, _em across. Phosphor's glyphs fill their em, from 1/16 of it below the baseline to
// 15/16 above, so the middle is 7/16 (0.4375) of the em above the baseline. Slug draws an em FUDE_DRAW_EM times the
// size asked for, so the size asked for is the em divided by that.
void fude_draw_icon(rde_font* _font, f32 _font_px, const c8* _icon, rde_vec_2F _center, f32 _em, rde_color _color) {
    if(_font == NULL || _icon == NULL) {
        return;
    }
    const f32 _px = _em / FUDE_DRAW_EM;
    fude_draw_text_raw(_font, _font_px, fude_draw_icon_dir(_icon), fude_draw_x(_center.x) - _em * 0.5f, _center.y - _em * 0.4375f, _px, _color);   // Its middle is mirrored, never the glyph.
}

// Draws an icon from the Phosphor Fill font; nothing while that font is not set.
void fude_draw_icon_fill(const c8* _icon, rde_vec_2F _center, f32 _em, rde_color _color) {
    fude_draw_icon(fude_draw_fill_font, fude_draw_fill_font_px, _icon, _center, _em, _color);
}

// Draws a rounded box with a border 1 unit wide. The engine takes the rounding as a fraction of half the shorter
// side, so _radius is turned into that fraction, at most 1 (a pill).
void fude_draw_card(rde_vec_2F _min, rde_vec_2F _max, f32 _radius, rde_color _fill, rde_color _border) {
    const rde_vec_2F _size = { _max.x - _min.x, _max.y - _min.y };
    if(_size.x <= 0.0f || _size.y <= 0.0f) {
        return;
    }
    const f32 _roundness = rde_math_min_f32(1.0f, 2.0f * _radius / rde_math_min_f32(_size.x, _size.y));
    rde_rendering_2d_draw_rounded_rectangle_with_border((rde_vec_2F){ fude_draw_x((_min.x + _max.x) * 0.5f), (_min.y + _max.y) * 0.5f }, _size, _roundness, 6,
                                                        _fill, 1.0f, _border, NULL);
}

// Draws a chip: a pill 12 units taller than the text, with 11 units of padding on each side of it. Returns its width.
f32 fude_draw_chip(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _mid, f32 _px, rde_color _fill, rde_color _color) {
    const f32 _h = _px + 12.0f;
    const f32 _w = fude_draw_text_width(_font, _font_px, _text, _px) + 22.0f;
    rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ fude_draw_x(_x + _w * 0.5f), _mid }, (rde_vec_2F){ _w, _h }, 1.0f, 8, _fill, NULL);
    fude_draw_text(_font, _font_px, _text, _x + 11.0f, _mid - _px * 0.36f, _px, _color);
    return _w;
}

// Draws a disc in the score's good or poor colour with a tick or a cross on it, drawn as lines in the accent's text
// colour.
void fude_draw_verdict(rde_vec_2F _center, f32 _radius, b8 _right) {
    const fude_theme* _theme = fude_theme_active();
    // A tick is never mirrored, as the platforms keep it the same way round.
    fude_draw_keep_begin((rde_vec_2F){ _center.x - _radius, _center.y - _radius }, (rde_vec_2F){ _center.x + _radius, _center.y + _radius }, false);
    rde_rendering_2d_draw_circle(fude_draw_at(_center), _radius, 24, _right ? _theme->score_good : _theme->score_poor, NULL);
    const f32        _u = _radius * 0.5f;
    const f32        _w = rde_math_max_f32(1.2f, _radius * 0.13f);
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

// Returns how wide _probe's characters are on average, per unit of size, or _otherwise when it cannot be measured.
RDE_INTERNAL f32 fude_draw_measure(rde_font* _font, f32 _font_px, const c8* _probe, f32 _otherwise) {
    const u32        _n = fude_utf8_count(_probe);
    const rde_vec_2F _m = _font != NULL ? rde_rich_text_measure(_probe, _font, 1.0f, 100000.0f, false) : (rde_vec_2F){ 0.0f, 0.0f };
    return _m.x > 0.0f && _n > 0 ? _m.x / ((f32)_n * _font_px) : _otherwise;
}

// Returns an estimate of a character's advance, per unit of size: the Japanese one from U+2E80 up (the CJK blocks),
// the Latin one below. Both are measured from sample texts the first time a font is asked for; only the last font's
// are kept, so they are measured again when the font changes. 0.6 and 1.3 are used when a sample cannot be measured.
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

// Every text is measured for real: laid out once by the engine, then kept. An estimate from average advances runs
// short on real sentences (a notice wider than its pill, a wrapped line past its card) and is off by a third for
// capitals and digits ("N5"). Widths are kept by font and the whole text, so two texts never share a width, up to
// FUDE_DRAW_EXACT_KEPT of them; past that, the one used longest ago makes room, so the texts a screen draws every
// frame stay kept however many others come and go (forgetting them all at once, as this did, measured a whole screen
// again each time a new text filled the map).
#define FUDE_DRAW_EXACT_KEPT 1536u
#define FUDE_DRAW_EXACT_KEY  1024u   // A key's most bytes; a longer text is measured each time (no UI text is).

// The kept widths, per unit of size, one for each slot of fude_draw_exact_lru (made on first use, on the std
// allocator), which knows whose width each slot holds (its key: the font's pointer, the size it was loaded at, then
// the text's bytes) and which was used longest ago. A width depends on nothing else: a font's letters never change
// while it is loaded.
RDE_INTERNAL f32     fude_draw_exact[FUDE_DRAW_EXACT_KEPT];
RDE_INTERNAL rde_lru fude_draw_exact_lru;

// Returns the first _len bytes of _text's width per unit of size, as kept or measured now and kept in a free slot or
// the one used longest ago. Returns -1 when the engine measures it as nothing.
RDE_INTERNAL f32 fude_draw_exact_width(rde_font* _font, f32 _font_px, const c8* _text, usize _len) {
    u8          _key[FUDE_DRAW_EXACT_KEY];
    const usize _head    = sizeof(_font) + sizeof(_font_px);
    const usize _key_len = _head + _len;
    const b8    _keep    = _key_len <= sizeof(_key);
    if(_keep) {
        if(!rde_lru_is_inited(&fude_draw_exact_lru)) {
            fude_draw_exact_lru = rde_lru_new(FUDE_DRAW_EXACT_KEPT, rde_memory_allocator_get_default_std());
        }
        rde_memcpy(_key, &_font, sizeof(_font));
        rde_memcpy(_key + sizeof(_font), &_font_px, sizeof(_font_px));
        rde_memcpy(_key + _head, _text, _len);
        const u32 _kept = rde_lru_find(&fude_draw_exact_lru, _key, _key_len);
        if(_kept != RDE_LRU_NONE) {
            return fude_draw_exact[_kept];
        }
    }
    const rde_vec_2F _m = rde_rich_text_measure(_text, _font, 1.0f, 100000.0f, false);
    const f32        _w = _m.x > 0.0f ? _m.x / _font_px : -1.0f;
    if(_keep) {
        fude_draw_exact[rde_lru_add(&fude_draw_exact_lru, _key, _key_len, NULL)] = _w;
    }
    return _w;
}

// Returns _text's width at _px: measured and kept, or estimated from per-character advances for markup or a text that
// measures as nothing. See the header.
f32 fude_draw_text_width(rde_font* _font, f32 _font_px, const c8* _text, f32 _px) {
    const usize _len = rde_strlen(_text);
    if(_font != NULL && _len > 0 && rde_strchr(_text, '[') == NULL) {   // '[' starts the engine's markup: estimate.
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

// A text's lines for a width and size are laid out once, measuring each candidate line where a line may break, and
// kept; every frame then draws them from where each line starts and ends.
// How many layouts are kept; the one used longest ago makes room.
#define FUDE_DRAW_WRAP_SLOTS 64u

// One kept layout: its lines (what it is the layout of: fude_draw_wrap_lru's key for its slot). However many lines a
// text wraps to, all are kept: a card's body or a page's text may be long, and none of it is ever left out.
typedef struct {
    u32 lines;
    rde_arr TYPE(u32) spans;   // Each line's start and end, in bytes: two numbers a line. Made the first time the slot is.
} fude_draw_wrapped;

// The kept layouts, one for each slot of fude_draw_wrap_lru (made on first use, on the std allocator), which knows
// what each is the layout of and which was used longest ago. fude_draw_key is where a key is built (both caches').
RDE_INTERNAL fude_draw_wrapped fude_draw_wrap_cache[FUDE_DRAW_WRAP_SLOTS];
RDE_INTERNAL rde_lru           fude_draw_wrap_lru;
RDE_INTERNAL rde_arr TYPE(u8)  fude_draw_key;

// Builds in fude_draw_key the whole of what a layout or a balanced width is made from: the font and the size it was
// made at, the size drawn at and the width, then the first _len bytes of the text. Returns its length in bytes.
RDE_INTERNAL usize fude_draw_layout_key(const rde_font* _font, f32 _font_px, f32 _px, f32 _width, const c8* _text, usize _len) {
    if(!rde_arr_is_inited(&fude_draw_key)) {
        fude_draw_key = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    }
    const usize _head = sizeof(_font) + 3u * sizeof(f32);
    rde_arr_clear(&fude_draw_key);
    u8* _k = (u8*)rde_arr_add_n(&fude_draw_key, _head + _len);
    rde_memcpy(_k, &_font, sizeof(_font));
    rde_memcpy(_k + sizeof(_font), &_font_px, sizeof(f32));
    rde_memcpy(_k + sizeof(_font) + sizeof(f32), &_px, sizeof(f32));
    rde_memcpy(_k + sizeof(_font) + 2u * sizeof(f32), &_width, sizeof(f32));
    rde_memcpy(_k + _head, _text, _len);
    return _head + _len;
}

// The bytes _text[_from.._to) copied with a 0 after them into fude_draw_row (made on first use, on the std
// allocator, as long as the longest line yet), so a line of any length is measured or drawn whole. The pointer is
// valid until the next call.
RDE_INTERNAL rde_arr TYPE(c8) fude_draw_row;
RDE_INTERNAL const c8* fude_draw_row_of(const c8* _text, usize _from, usize _to) {
    if(!rde_arr_is_inited(&fude_draw_row)) {
        fude_draw_row = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    }
    rde_arr_resize(&fude_draw_row, _to - _from + 1u);
    c8* _row = (c8*)fude_draw_row.memory;
    rde_memcpy(_row, _text + _from, _to - _from);
    _row[_to - _from] = 0;
    return _row;
}

// Returns how wide the bytes _text[_from.._to) draw at _px.
RDE_INTERNAL f32 fude_draw_span_width(rde_font* _font, f32 _font_px, const c8* _text, usize _from, usize _to, f32 _px) {
    return fude_draw_text_width(_font, _font_px, fude_draw_row_of(_text, _from, _to), _px);
}

// Japanese and Chinese line breaking (kinsoku). Returns true for what never starts a line: closing punctuation and
// brackets, the small kana, the long-vowel mark, the iteration marks and ellipses, and Latin closing punctuation.
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

// Returns true for what never ends a line: an opening bracket.
RDE_INTERNAL b8 fude_draw_no_line_end(u32 _cp) {
    switch(_cp) {
        case 0x300C: case 0x300E: case 0xFF08: case 0x3014: case 0xFF3B: case 0xFF5B: case 0x3008: case 0x300A: case 0x3010:   // 「『（〔［｛〈《【
            return true;
        default:
            return false;
    }
}

// Returns _text's layout in lines no wider than _width at _px, from the kept ones or laid out now into a free slot or
// the one used longest ago. A line may end before a space, after a CJK character (as kinsoku allows), at a line break
// or at the end; the last of those that fits ends it. A word wider than the line is cut between characters. The
// pointer stays valid for at least FUDE_DRAW_WRAP_SLOTS - 1 more layouts asked for (a slot is reused only once every
// other was used after it).
RDE_INTERNAL const fude_draw_wrapped* fude_draw_wrap_layout(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width) {
    const usize _len = rde_strlen(_text);
    if(!rde_lru_is_inited(&fude_draw_wrap_lru)) {
        fude_draw_wrap_lru = rde_lru_new(FUDE_DRAW_WRAP_SLOTS, rde_memory_allocator_get_default_std());
    }
    const usize _key_len = fude_draw_layout_key(_font, _font_px, _px, _width, _text, _len);
    const u32   _kept    = rde_lru_find(&fude_draw_wrap_lru, (const any)fude_draw_key.memory, _key_len);
    if(_kept != RDE_LRU_NONE) {
        return &fude_draw_wrap_cache[_kept];
    }
    fude_draw_wrapped* _w = &fude_draw_wrap_cache[rde_lru_add(&fude_draw_wrap_lru, (const any)fude_draw_key.memory, _key_len, NULL)];
    if(!rde_arr_is_inited(&_w->spans)) {
        _w->spans = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    }
    rde_arr_clear(&_w->spans);
    _w->lines = 0;

    usize _s = 0;   // Where the line starts.
    while(_s < _len) {
        while(_s < _len && _text[_s] == ' ') {
            _s++;
        }
        if(_s >= _len) {
            break;
        }
        // Measure the line at each place it may end (before a space, after a CJK character, at a line break or at the
        // end); the last place that fits ends it.
        usize     _end  = _s;   // The line so far: _text[_s.._end).
        usize     _next = _s;   // Where the next line starts.
        const c8* _p    = _text + _s;
        for(;;) {
            const usize _here = (usize)(_p - _text);
            const u32   _cp   = fude_utf8_next(&_p);
            const usize _past = (usize)(_p - _text);
            const b8    _gap  = _cp == ' ' || _cp == '\n' || _cp == 0;
            if(!_gap && _cp < 0x2E80u) {
                continue;   // Inside a word: no break here.
            }
            if(!_gap) {   // After a CJK character: kinsoku forbids a break here before or after some characters.
                const c8* _peek = _p;
                if(fude_draw_no_line_end(_cp) || fude_draw_no_line_start(fude_utf8_next(&_peek))) {
                    continue;
                }
            }
            const usize _try = _gap ? _here : _past;
            if(_try > _s && fude_draw_span_width(_font, _font_px, _text, _s, _try, _px) > _width) {
                if(_end == _s) {
                    // Not one place fits (a word wider than the line): take as many characters as fit, at least one.
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
        u32* _span = (u32*)rde_arr_add_n(&_w->spans, 2u);
        _span[0]   = (u32)_s;
        _span[1]   = (u32)_end;
        _w->lines++;
        if(_next <= _s) {
            break;   // Nothing would fit: stop rather than loop forever.
        }
        _s = _next;
    }
    return _w;
}

// Lays _text out (see fude_draw_wrap_layout) and draws its lines, each _line below the one before, or with _draw false
// only counts them. Returns the number of lines. Each line is drawn whole, however long.
RDE_INTERNAL u32 fude_draw_text_wrap_do(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, f32 _width, f32 _line, rde_color _color, b8 _draw) {
    if(_text == NULL || _text[0] == 0) {
        return 0;
    }
    const fude_draw_wrapped* _w = fude_draw_wrap_layout(_font, _font_px, _text, _px, _width);
    if(_draw) {
        const u32* _span = (const u32*)_w->spans.memory;
        for(u32 _i = 0; _i < _w->lines; _i++) {
            fude_draw_text(_font, _font_px, fude_draw_row_of(_text, _span[2u * _i], _span[2u * _i + 1u]), _x, _y - (f32)_i * _line, _px, _color);
        }
    }
    return _w->lines;
}

// The narrowest width that wraps a text in as few lines as its width does, so its lines come out even and no word is
// left alone on the last. Finding it takes a dozen layouts, so the answer is kept: FUDE_DRAW_BALANCE_SLOTS answers,
// one for each slot of fude_draw_balance_lru (made on first use), by the same whole key as a layout's; the one used
// longest ago makes room.
#define FUDE_DRAW_BALANCE_SLOTS 32u
RDE_INTERNAL f32     fude_draw_balance_kept[FUDE_DRAW_BALANCE_SLOTS];
RDE_INTERNAL rde_lru fude_draw_balance_lru;

// Returns the balanced width: a binary search, 12 steps between 90% of _width shared out over the lines and _width,
// for the narrowest width that keeps the same number of lines, plus 1 pixel of slack. See the header.
f32 fude_draw_text_balanced_width(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width) {
    if(_font == NULL || _text == NULL || _text[0] == 0) {
        return _width;
    }
    const usize _len = rde_strlen(_text);
    if(!rde_lru_is_inited(&fude_draw_balance_lru)) {
        fude_draw_balance_lru = rde_lru_new(FUDE_DRAW_BALANCE_SLOTS, rde_memory_allocator_get_default_std());
    }
    const usize _key_len = fude_draw_layout_key(_font, _font_px, _px, _width, _text, _len);   // (built before it is read)
    const u32   _kept    = rde_lru_find(&fude_draw_balance_lru, (const any)fude_draw_key.memory, _key_len);
    if(_kept != RDE_LRU_NONE) {
        return fude_draw_balance_kept[_kept];
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
        _best = rde_math_min_f32(_width, _best + 1.0f);
    }
    // (the layouts above built their own keys in fude_draw_key: this one's again)
    const usize _again = fude_draw_layout_key(_font, _font_px, _px, _width, _text, _len);
    fude_draw_balance_kept[rde_lru_add(&fude_draw_balance_lru, (const any)fude_draw_key.memory, _again, NULL)] = _best;
    return _best;
}

// Draws _text wrapped to _width. Returns how many lines it drew.
u32 fude_draw_text_wrap(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, f32 _width, f32 _line, rde_color _color) {
    return fude_draw_text_wrap_do(_font, _font_px, _text, _x, _y, _px, _width, _line, _color, true);
}

// Copies the wrapped lines' byte ranges, at most _max of them. Returns how many it copied.
u32 fude_draw_text_wrap_spans(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width, u32* _from, u32* _to, u32 _max) {
    if(_text == NULL || _text[0] == 0) {
        return 0;
    }
    const fude_draw_wrapped* _w    = fude_draw_wrap_layout(_font, _font_px, _text, _px, _width);
    const u32*               _span = (const u32*)_w->spans.memory;
    const u32                _n    = _w->lines < _max ? _w->lines : _max;
    for(u32 _i = 0; _i < _n; _i++) {
        _from[_i] = _span[2u * _i];
        _to[_i]   = _span[2u * _i + 1u];
    }
    return _n;
}

// Returns how many lines _text wraps to, drawing nothing.
u32 fude_draw_text_wrap_lines(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width) {
    return fude_draw_text_wrap_do(_font, _font_px, _text, 0.0f, 0.0f, _px, _width, 0.0f, (rde_color){ 0, 0, 0, 0 }, false);
}

// Returns _px, or the size at which _text's width is exactly _width when it is wider at _px.
f32 fude_draw_text_px_to_fit(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width) {
    const f32 _w = fude_draw_text_width(_font, _font_px, _text, _px);
    if(_w <= _width || _w <= 0.0f) {
        return _px;
    }
    return _px * _width / _w;
}

// The size and number of lines a text drawn whole on several lines takes (fude_draw_text_whole). Finding them lays
// the text out at each size tried, up to some forty layouts, which would push every other layout out of the wrap
// cache each frame; so the answer is kept: FUDE_DRAW_WHOLE_SLOTS of them, one for each slot of fude_draw_whole_lru
// (made on first use), by the layout's whole key and the lines allowed; the one used longest ago makes room.
#define FUDE_DRAW_WHOLE_SLOTS 32u
typedef struct {
    f32 px;      // The size it is drawn at.
    u32 lines;   // How many lines it takes at that size.
} fude_draw_whole;
RDE_INTERNAL fude_draw_whole fude_draw_whole_kept[FUDE_DRAW_WHOLE_SLOTS];
RDE_INTERNAL rde_lru         fude_draw_whole_lru;

// Returns the size and lines of _text drawn whole on at most _lines lines: shrinking by half a pixel at a time from
// 92% of _px until it takes at most _lines lines or reaches half of _px. Kept, as above.
RDE_INTERNAL fude_draw_whole fude_draw_whole_size(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width, u32 _lines) {
    if(!rde_lru_is_inited(&fude_draw_whole_lru)) {
        fude_draw_whole_lru = rde_lru_new(FUDE_DRAW_WHOLE_SLOTS, rde_memory_allocator_get_default_std());
    }
    const usize _len = rde_strlen(_text);
    // The key: the layout's (its text last), then the lines allowed. Its length is the text's plus a fixed part, so
    // two keys of one length hold texts of one length, and the same bytes mean the same text and the same lines.
    usize _key_len = fude_draw_layout_key(_font, _font_px, _px, _width, _text, _len);
    rde_memcpy(rde_arr_add_n(&fude_draw_key, sizeof(u32)), &_lines, sizeof(u32));
    _key_len += sizeof(u32);
    const u32 _kept = rde_lru_find(&fude_draw_whole_lru, (const any)fude_draw_key.memory, _key_len);
    if(_kept != RDE_LRU_NONE) {
        return fude_draw_whole_kept[_kept];
    }
    fude_draw_whole _w = { _px * 0.92f, 0u };
    _w.lines = fude_draw_text_wrap_lines(_font, _font_px, _text, _w.px, _width);
    while(_w.lines > _lines && _w.px > _px * 0.5f) {
        _w.px   -= 0.5f;
        _w.lines = fude_draw_text_wrap_lines(_font, _font_px, _text, _w.px, _width);
    }
    // (the layouts above built their own keys in fude_draw_key: this one's again)
    _key_len = fude_draw_layout_key(_font, _font_px, _px, _width, _text, _len);
    rde_memcpy(rde_arr_add_n(&fude_draw_key, sizeof(u32)), &_lines, sizeof(u32));
    _key_len += sizeof(u32);
    fude_draw_whole_kept[rde_lru_add(&fude_draw_whole_lru, (const any)fude_draw_key.memory, _key_len, NULL)] = _w;
    return _w;
}

// Draws _text whole: on one line when it fits at three quarters of _px or more, else wrapped at the size
// fude_draw_whole_size finds. The lines are 1.18 times the size apart and centred up and down on _mid. Returns the
// size drawn at. See the header.
f32 fude_draw_text_whole(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _mid, f32 _px, f32 _width, u32 _lines, rde_color _color) {
    if(_text == NULL || _text[0] == 0) {
        return _px;
    }
    // One line: at its size or smaller, down to three quarters of it, or as small as it takes when only one line is
    // allowed.
    const f32 _p1 = fude_draw_text_px_to_fit(_font, _font_px, _text, _px, _width);
    if(_p1 >= _px * 0.75f || _lines < 2u) {
        fude_draw_text(_font, _font_px, _text, _x, _mid - _p1 * 0.38f, _p1, _color);
        return _p1;
    }
    // Several lines: a little smaller at each step, until they are few enough.
    const fude_draw_whole _size = fude_draw_whole_size(_font, _font_px, _text, _px, _width, _lines);
    const f32             _p    = _size.px;
    const u32             _n    = _size.lines;
    const f32             _line = _p * 1.18f;
    fude_draw_text_wrap(_font, _font_px, _text, _x, _mid + (f32)(_n - 1u) * _line * 0.5f - _p * 0.38f, _p, _width, _line, _color);
    return _p;
}
