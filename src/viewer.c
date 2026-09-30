#include "viewer.h"
#include "draw.h"
#include "theme.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See viewer.h.
// ===========================================================================

#define KANA_VIEWER_MARGIN     28.0f     // screen units around everything
#define KANA_VIEWER_LINE       44.0f     // text line height
#define KANA_VIEWER_KANA_SIZE  30.0f     // a stroke-drawn kana in a reading
#define KANA_VIEWER_GUTTER     40.0f     // wide: between the character and its details
#define KANA_VIEWER_MIN_CHAR   300.0f    // stacked: words go (down to 3) before the character gets smaller than this
#define KANA_VIEWER_TITLE      36.0f     // the "Words" line
#define KANA_VIEWER_ROW        44.0f     // an example word's row (a finger's height)
#define KANA_VIEWER_WORD_PX    24.0f     // its written form
#define KANA_VIEWER_SMALL_PX   17.0f     // its reading and meaning

void kana_viewer_init(kana_viewer* _viewer, const kana_kanji_db* _db) {
    memset(_viewer, 0, sizeof(*_viewer));
    _viewer->db   = _db;
    _viewer->list    = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    _viewer->pressed = -1;
    kana_glyph_init(&_viewer->glyph, _db);
}

void kana_viewer_destroy(kana_viewer* _viewer) {
    if(rde_arr_is_inited(&_viewer->list)) {
        rde_arr_free(&_viewer->list);
    }
    kana_glyph_destroy(&_viewer->glyph);
    memset(_viewer, 0, sizeof(*_viewer));
}

b8 kana_viewer_available(const kana_viewer* _viewer) {
    return _viewer->db != NULL && _viewer->db->count > 0;
}

void kana_viewer_replay(kana_viewer* _viewer) {
    _viewer->started = rde_engine_get_time_now();
}

// --- taps on the words -----------------------------------------------------------

RDE_INTERNAL i32 kana_viewer_row_at(const kana_viewer* _viewer, rde_vec_2F _p) {
    for(u32 _i = 0; _i < _viewer->row_count; _i++) {
        const kana_viewer_row* _r = &_viewer->rows[_i];
        if(_p.x >= _r->min.x && _p.x <= _r->max.x && _p.y >= _r->min.y && _p.y <= _r->max.y) {
            return (i32)_i;
        }
    }
    return -1;
}

void kana_viewer_pointer_down(kana_viewer* _viewer, rde_vec_2F _screen, f64 _time) {
    kana_scroller_down(&_viewer->taps, _screen, _time);
    _viewer->pressed = kana_viewer_row_at(_viewer, _screen);
}

void kana_viewer_pointer_moved(kana_viewer* _viewer, rde_vec_2F _screen, f64 _time) {
    kana_scroller_moved(&_viewer->taps, _screen, _time);
    if(_viewer->taps.dragging) {
        _viewer->pressed = -1;
    }
}

void kana_viewer_pointer_up(kana_viewer* _viewer, f64 _time) {
    kana_scroller_up(&_viewer->taps, _time);
    _viewer->pressed = -1;
}

RDE_INTERNAL b8 kana_viewer_is_kanji(u32 _cp) {
    return (_cp >= 0x4E00u && _cp <= 0x9FFFu) || (_cp >= 0x3400u && _cp <= 0x4DBFu);
}

u32 kana_viewer_take_word(kana_viewer* _viewer, u32* _out, u32 _max) {
    rde_vec_2F _at;
    if(!kana_scroller_take_tap(&_viewer->taps, &_at)) {
        return 0;
    }
    const i32       _row = kana_viewer_row_at(_viewer, _at);
    kana_kanji_word _word;
    if(_row < 0 || !kana_kanji_word_at(_viewer->db, _viewer->rows[_row].word, &_word)) {
        return 0;
    }

    u32       _n = 0;
    const c8* _p = _word.written;
    for(u32 _cp = kana_kanji_utf8_next(&_p); _cp != 0 && _n < _max; _cp = kana_kanji_utf8_next(&_p)) {
        u32 _record;
        if(!kana_viewer_is_kanji(_cp) || !kana_kanji_find_index(_viewer->db, _cp, &_record)) {
            continue;
        }
        b8 _seen = false;
        for(u32 _i = 0; _i < _n && !_seen; _i++) {
            _seen = _out[_i] == _record;
        }
        if(!_seen) {
            _out[_n++] = _record;
        }
    }
    return _n;
}

void kana_viewer_show(kana_viewer* _viewer, const u32* _records, u32 _count, u32 _position) {
    if(!kana_viewer_available(_viewer) || _count == 0) {
        return;
    }

    rde_arr_clear(&_viewer->list);
    memcpy(rde_arr_add_n(&_viewer->list, _count), _records, (usize)_count * sizeof(u32));
    _viewer->position = _position < _count ? _position : 0u;
    _viewer->open     = true;
    _viewer->pressed  = -1;
    kana_scroller_stop(&_viewer->taps);
    kana_viewer_replay(_viewer);
}

b8 kana_viewer_show_codepoint(kana_viewer* _viewer, u32 _codepoint) {
    for(u32 _i = 0; kana_viewer_available(_viewer) && _i < _viewer->db->count; _i++) {
        kana_kanji_info _info;
        if(kana_kanji_at(_viewer->db, _i, &_info) && _info.codepoint == _codepoint) {
            kana_viewer_show(_viewer, &_i, 1u, 0u);
            return true;
        }
    }
    return false;
}

void kana_viewer_close(kana_viewer* _viewer) {
    _viewer->open = false;
}

void kana_viewer_next(kana_viewer* _viewer) {
    const u32 _n = (u32)rde_arr_length(&_viewer->list);
    if(_n > 0) {
        _viewer->position = (_viewer->position + 1u) % _n;
        kana_viewer_replay(_viewer);
    }
}

void kana_viewer_prev(kana_viewer* _viewer) {
    const u32 _n = (u32)rde_arr_length(&_viewer->list);
    if(_n > 0) {
        _viewer->position = (_viewer->position + _n - 1u) % _n;
        kana_viewer_replay(_viewer);
    }
}

// --- text that has to fit ------------------------------------------------------------
//
// Widths are estimated, not laid out: an advance per character, measured once
// through the font — Japanese (Noto Sans JP, all one width) and Latin (the UI
// font's average). Good enough for the columns and to stop a meaning at the
// screen's edge.

// How wide _probe's characters are on average, per unit of size.
RDE_INTERNAL f32 kana_viewer_measure(rde_font* _font, f32 _font_px, const c8* _probe, f32 _otherwise) {
    u32       _n = 0;
    const c8* _p = _probe;
    while(kana_kanji_utf8_next(&_p) != 0) {
        _n++;
    }
    const rde_vec_2F _m = _font != NULL ? rde_rich_text_measure(_probe, _font, 1.0f, 100000.0f, false) : (rde_vec_2F){ 0.0f, 0.0f };
    return _m.x > 0.0f && _n > 0 ? _m.x / ((f32)_n * _font_px) : _otherwise;
}

RDE_INTERNAL f32 kana_viewer_advance(kana_viewer* _viewer, rde_font* _font, f32 _font_px, u32 _cp) {
    if(_cp >= 0x2E80u) {
        if(_viewer->_japanese_em <= 0.0f) {
            _viewer->_japanese_em = kana_viewer_measure(_font, _font_px, "日本語のかなとカタカナ", 1.3f);
        }
        return _viewer->_japanese_em;
    }
    if(_viewer->_latin_em <= 0.0f) {
        _viewer->_latin_em = kana_viewer_measure(_font, _font_px, "The quick brown fox jumps over the lazy dog; (e.g. lunch, dinner) 0123", 0.6f);
    }
    return _viewer->_latin_em;
}

RDE_INTERNAL f32 kana_viewer_width(kana_viewer* _viewer, rde_font* _font, f32 _font_px, const c8* _text, f32 _px) {
    f32 _w = 0.0f;
    for(u32 _cp = kana_kanji_utf8_next(&_text); _cp != 0; _cp = kana_kanji_utf8_next(&_text)) {
        _w += kana_viewer_advance(_viewer, _font, _font_px, _cp) * _px;
    }
    return _w;
}

// _text into _out, cut with "…" where it would pass _width at _px.
RDE_INTERNAL void kana_viewer_fit(kana_viewer* _viewer, rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width, c8* _out, usize _size) {
    snprintf(_out, _size, "%s", _text);
    if(kana_viewer_width(_viewer, _font, _font_px, _text, _px) <= _width) {
        return;
    }
    const f32 _ellipsis = kana_viewer_advance(_viewer, _font, _font_px, 'x') * _px;
    f32       _w        = 0.0f;
    const c8* _p        = _text;
    for(;;) {
        const c8* _before = _p;
        const u32 _cp     = kana_kanji_utf8_next(&_p);
        _w += kana_viewer_advance(_viewer, _font, _font_px, _cp) * _px;
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

// --- the page ----------------------------------------------------------------------

RDE_INTERNAL b8 kana_viewer_is_kana(u32 _cp) {
    return (_cp >= 0x3041u && _cp <= 0x3096u) || (_cp >= 0x30A1u && _cp <= 0x30FAu);
}

void kana_viewer_render(kana_viewer* _viewer, rde_font* _font, f32 _font_px, rde_vec_2I _window, rde_vec_4I _insets, f32 _bottom_bar) {
    if(!_viewer->open || rde_arr_length(&_viewer->list) == 0) {
        return;
    }

    kana_kanji_info _info;
    const u32 _record = ((const u32*)_viewer->list.memory)[_viewer->position];
    if(!kana_kanji_at(_viewer->db, _record, &_info)) {
        return;
    }

    const f32 _hw     = (f32)_window.x * 0.5f;
    const f32 _hh     = (f32)_window.y * 0.5f;
    const f32 _top    = _hh - (f32)_insets.y - KANA_VIEWER_MARGIN;
    const f32 _bottom = -_hh + (f32)_insets.w + _bottom_bar + KANA_VIEWER_MARGIN;
    const f32 _left   = -_hw + (f32)_insets.x + KANA_VIEWER_MARGIN;
    const f32 _right  = _hw - (f32)_insets.z - KANA_VIEWER_MARGIN;

    // --- header: what it is -------------------------------------------------------
    const b8 _kana = kana_viewer_is_kana(_info.codepoint);
    c8 _line[256];
    c8 _extra[96] = "";
    if(_info.jlpt_n != 0) {
        snprintf(_extra, sizeof(_extra), "   JLPT N%u", _info.jlpt_n);
    }
    {
        const usize _len = strlen(_extra);
        if(_info.grade >= 1 && _info.grade <= 6)       { snprintf(_extra + _len, sizeof(_extra) - _len, "   grade %u", _info.grade); }
        else if(_info.grade == 8)                      { snprintf(_extra + _len, sizeof(_extra) - _len, "   secondary school"); }
        else if(_info.grade == 9 || _info.grade == 10) { snprintf(_extra + _len, sizeof(_extra) - _len, "   jinmeiyou (names)"); }
    }
    snprintf(_line, sizeof(_line), "%s   %u stroke%s%s      %u / %u",
             _kana ? (_info.codepoint < 0x30A0u ? "Hiragana" : "Katakana") : "Kanji",
             _info.strokes, _info.strokes == 1 ? "" : "s", _extra,
             _viewer->position + 1u, (u32)rde_arr_length(&_viewer->list));
    kana_draw_text(_font, _font_px, _line, _left, _top - 16.0f, 22.0f, kana_theme_active()->text);

    // The parts it is built from, those that can be drawn.
    u32 _parts[KANA_KANJI_MAX_PARTS];
    u32 _part_count = 0;
    if(!_kana) {
        u32 _all[KANA_KANJI_MAX_PARTS];
        const u32 _n = kana_kanji_parts(_viewer->db, _record, _all, KANA_KANJI_MAX_PARTS);
        for(u32 _i = 0; _i < _n; _i++) {
            u32 _index;
            if(kana_kanji_find_index(_viewer->db, _all[_i], &_index)) {
                _parts[_part_count++] = _all[_i];
            }
        }
    }

    // The example words (kanji only): as many as fit, see below.
    u32 _words[KANA_VIEWER_WORDS];
    u32 _word_count = _kana ? 0u : kana_kanji_words(_viewer->db, _record, _words, KANA_VIEWER_WORDS);

    // --- the character, and where its details go --------------------------------------
    const f32 _details = _kana ? 0.0f : (_part_count > 0 ? 4.0f : 3.0f) * KANA_VIEWER_LINE;   // On, Kun, meaning, parts
    const b8  _wide    = !_kana && (_right - _left) > (_top - _bottom) * 1.25f;
    f32        _size;
    rde_vec_2F _tl;
    f32        _x0;   // the details' left
    f32        _y0;   // and top
    if(_wide) {
        // Beside it: the character as tall as the screen allows, up to under half
        // its width; the words as many as the column holds.
        _size = fmaxf(120.0f, fminf(_top - 48.0f - _bottom, (_right - _left) * 0.45f));
        _tl   = (rde_vec_2F){ _left, _top - 48.0f };
        _x0   = _left + _size + KANA_VIEWER_GUTTER;
        _y0   = _tl.y - 8.0f;
        const f32 _rows = floorf((_y0 - _details - KANA_VIEWER_TITLE - _bottom) / KANA_VIEWER_ROW);
        _word_count = _rows < 1.0f ? 0u : (u32)fminf((f32)_word_count, _rows);
    } else {
        // Under it: words go, down to three, before the character gets small.
        const f32 _room = (_top - 48.0f) - (_bottom + _details);
        #define KANA_VIEWER_WORDS_H(n) ((n) > 0u ? KANA_VIEWER_TITLE + (f32)(n) * KANA_VIEWER_ROW : 0.0f)
        while(_word_count > 3u && _room - KANA_VIEWER_WORDS_H(_word_count) < KANA_VIEWER_MIN_CHAR) {
            _word_count--;
        }
        if(_room - KANA_VIEWER_WORDS_H(_word_count) < 160.0f) {
            _word_count = 0;   // a small screen: the character first
        }
        _size = fmaxf(120.0f, fminf(_right - _left, _room - KANA_VIEWER_WORDS_H(_word_count)));
        #undef KANA_VIEWER_WORDS_H
        _tl   = (rde_vec_2F){ -_size * 0.5f, _top - 48.0f };
        _x0   = _left;
        _y0   = _tl.y - _size - 20.0f;
    }

    kana_glyph_box(_tl, _size);
    kana_glyph_writing(&_viewer->glyph, &_info, _tl, _size, rde_engine_get_time_now() - _viewer->started, _font, _font_px, 18.0f);

    _viewer->row_count = 0;
    if(_kana) {
        return;
    }

    // --- readings and meaning --------------------------------------------------------
    const kana_theme* _theme = kana_theme_active();
    kana_draw_text(_font, _font_px, "On", _x0, _y0 - KANA_VIEWER_KANA_SIZE * 0.7f, 20.0f, _theme->text_soft);
    kana_glyph_reading(&_viewer->glyph, kana_kanji_on(_viewer->db, &_info), (rde_vec_2F){ _x0 + 60.0f, _y0 },
                       KANA_VIEWER_KANA_SIZE, _right, _theme->ink, _theme->text_soft);
    kana_draw_text(_font, _font_px, "Kun", _x0, _y0 - KANA_VIEWER_LINE - KANA_VIEWER_KANA_SIZE * 0.7f, 20.0f, _theme->text_soft);
    kana_glyph_reading(&_viewer->glyph, kana_kanji_kun(_viewer->db, &_info), (rde_vec_2F){ _x0 + 60.0f, _y0 - KANA_VIEWER_LINE },
                       KANA_VIEWER_KANA_SIZE, _right, _theme->ink, _theme->text_soft);

    const c8* _meanings = kana_kanji_meanings(_viewer->db, &_info);
    kana_viewer_fit(_viewer, _font, _font_px, _meanings[0] != 0 ? _meanings : "(no meaning listed)", 22.0f, _right - _x0, _line, sizeof(_line));
    kana_draw_text(_font, _font_px, _line, _x0, _y0 - 2.0f * KANA_VIEWER_LINE - KANA_VIEWER_KANA_SIZE * 0.7f, 22.0f, _theme->text);

    if(_part_count > 0) {
        const f32 _py = _y0 - 3.0f * KANA_VIEWER_LINE;
        kana_draw_text(_font, _font_px, "Parts", _x0, _py - KANA_VIEWER_KANA_SIZE * 0.7f, 20.0f, _theme->text_soft);
        f32 _x = _x0 + 60.0f;
        for(u32 _i = 0; _i < _part_count && _x + KANA_VIEWER_KANA_SIZE <= _right; _i++) {
            kana_glyph_character(&_viewer->glyph, _parts[_i], (rde_vec_2F){ _x, _py }, KANA_VIEWER_KANA_SIZE, _theme->ink);
            _x += KANA_VIEWER_KANA_SIZE * 1.3f;
        }
    }

    // --- example words: written, reading, meaning; a row each, tapped to practise ---
    if(_word_count == 0) {
        return;
    }
    kana_kanji_word _shown[KANA_VIEWER_WORDS];
    const f32       _em        = kana_viewer_advance(_viewer, _font, _font_px, 0x3042u);   // a Japanese character, per unit of size
    f32             _written_w = 2.0f * _em * KANA_VIEWER_WORD_PX;    // the columns: the widest shown
    f32             _reading_w = 3.0f * _em * KANA_VIEWER_SMALL_PX;
    for(u32 _i = 0; _i < _word_count; _i++) {
        kana_kanji_word_at(_viewer->db, _words[_i], &_shown[_i]);
        _written_w = fmaxf(_written_w, kana_viewer_width(_viewer, _font, _font_px, _shown[_i].written, KANA_VIEWER_WORD_PX));
        _reading_w = fmaxf(_reading_w, kana_viewer_width(_viewer, _font, _font_px, _shown[_i].reading, KANA_VIEWER_SMALL_PX));
    }
    _reading_w = fminf(_reading_w, 8.0f * kana_viewer_advance(_viewer, _font, _font_px, 0x3042u) * KANA_VIEWER_SMALL_PX);

    const f32 _wy = _y0 - _details;
    kana_draw_text(_font, _font_px, "Words", _x0, _wy - KANA_VIEWER_TITLE + 12.0f, 20.0f, _theme->text_soft);
    kana_draw_line((rde_vec_2F){ _x0, _wy - KANA_VIEWER_TITLE }, (rde_vec_2F){ _right, _wy - KANA_VIEWER_TITLE }, 0.5f, _theme->line);

    const f32 _reading_x = _x0 + 8.0f + _written_w + 16.0f;
    const f32 _meaning_x = _reading_x + _reading_w + 16.0f;
    for(u32 _i = 0; _i < _word_count; _i++) {
        const f32 _row_top  = _wy - KANA_VIEWER_TITLE - (f32)_i * KANA_VIEWER_ROW;
        const f32 _baseline = _row_top - KANA_VIEWER_ROW * 0.5f - KANA_VIEWER_SMALL_PX * 0.35f;
        _viewer->rows[_i] = (kana_viewer_row){ .min = { _x0, _row_top - KANA_VIEWER_ROW }, .max = { _right, _row_top }, .word = _words[_i] };

        if(_viewer->pressed == (i32)_i) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ (_x0 + _right) * 0.5f, _row_top - KANA_VIEWER_ROW * 0.5f },
                                            (rde_vec_2F){ _right - _x0, KANA_VIEWER_ROW }, _theme->select_fill);
        }
        kana_draw_text(_font, _font_px, _shown[_i].written, _x0 + 8.0f, _row_top - KANA_VIEWER_ROW * 0.5f - KANA_VIEWER_WORD_PX * 0.38f,
                       KANA_VIEWER_WORD_PX, _theme->ink);
        kana_viewer_fit(_viewer, _font, _font_px, _shown[_i].reading, KANA_VIEWER_SMALL_PX, _reading_w, _line, sizeof(_line));
        kana_draw_text(_font, _font_px, _line, _reading_x, _baseline, KANA_VIEWER_SMALL_PX, _theme->text_soft);
        kana_viewer_fit(_viewer, _font, _font_px, _shown[_i].meaning, KANA_VIEWER_SMALL_PX, _right - 28.0f - _meaning_x, _line, sizeof(_line));
        kana_draw_text(_font, _font_px, _line, _meaning_x, _baseline, KANA_VIEWER_SMALL_PX, _theme->text);
        kana_draw_text(_font, _font_px, "\xE2\x80\xBA", _right - 14.0f, _baseline, KANA_VIEWER_SMALL_PX + 4.0f, _theme->text_soft);   // ›: it opens
        kana_draw_line((rde_vec_2F){ _x0, _row_top - KANA_VIEWER_ROW }, (rde_vec_2F){ _right, _row_top - KANA_VIEWER_ROW }, 0.5f, _theme->line);
    }
    _viewer->row_count = _word_count;
}
