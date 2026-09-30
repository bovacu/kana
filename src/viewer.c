#include "viewer.h"
#include "draw.h"
#include "icons.h"
#include "select.h"
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
    _viewer->list     = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    _viewer->pressed  = -1;
    _viewer->rows_for = UINT32_MAX;
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

// --- the words: listed, scrolled, tapped ------------------------------------------------

RDE_INTERNAL b8 kana_viewer_is_kanji(u32 _cp) {
    return (_cp >= 0x4E00u && _cp <= 0x9FFFu) || (_cp >= 0x3400u && _cp <= 0x4DBFu);
}

u32 kana_viewer_codepoint(const kana_viewer* _viewer) {
    kana_kanji_info _info;
    if(!_viewer->open || rde_arr_length(&_viewer->list) == 0 ||
       !kana_kanji_at(_viewer->db, ((const u32*)_viewer->list.memory)[_viewer->position], &_info)) {
        return 0;
    }
    return _info.codepoint;
}

RDE_INTERNAL void kana_viewer_row_set(kana_viewer_row* _row, u8 _kind, b8 _ticked, const kana_kanji_word* _w) {
    _row->kind   = _kind;
    _row->ticked = _ticked;
    snprintf(_row->written, sizeof(_row->written), "%s", _w->written);
    snprintf(_row->reading, sizeof(_row->reading), "%s", _w->reading);
    snprintf(_row->meaning, sizeof(_row->meaning), "%s", _w->meaning);
}

// The words for the character on screen: listed, the learner's first then the
// examples; in Add, the ones typed in, then the character's further words.
RDE_INTERNAL void kana_viewer_list_rows(kana_viewer* _viewer, u32 _record, u32 _codepoint) {
    const b8 _moved = _viewer->rows_for != _record || _viewer->rows_adding != _viewer->adding;
    if(!_moved && _viewer->rows_words == kana_userwords_revision()) {
        return;
    }
    _viewer->rows_for    = _record;
    _viewer->rows_adding = _viewer->adding;
    _viewer->rows_words  = kana_userwords_revision();
    _viewer->row_count   = 0;
    if(_moved) {
        kana_scroller_stop(&_viewer->scroll);
        _viewer->scroll.offset = 0.0f;
    }
    if(!kana_viewer_is_kanji(_codepoint)) {
        return;
    }

    u32       _words[KANA_KANJI_MAX_WORDS];
    u32       _examples = 0;
    const u32 _n        = kana_kanji_words(_viewer->db, _record, _words, KANA_KANJI_MAX_WORDS, &_examples);
    const u32 _yours    = kana_userwords_count(_codepoint);
    kana_kanji_word _w;
    for(u32 _i = 0; _i < _yours && _viewer->row_count < KANA_VIEWER_ROWS; _i++) {
        if(!kana_userwords_at(_codepoint, _i, &_w)) {
            continue;
        }
        if(_viewer->adding) {
            // In Add, only the ones typed in (the others show where they are, ticked).
            b8 _listed = false;
            for(u32 _k = 0; _k < _n && !_listed; _k++) {
                kana_kanji_word _d;
                _listed = kana_kanji_word_at(_viewer->db, _words[_k], &_d) && strcmp(_d.written, _w.written) == 0 && strcmp(_d.reading, _w.reading) == 0;
            }
            if(_listed) {
                continue;
            }
        }
        kana_viewer_row_set(&_viewer->rows[_viewer->row_count++], _viewer->adding ? KANA_VIEWER_ROW_TYPED : KANA_VIEWER_ROW_YOURS, true, &_w);
    }
    const u32 _from = _viewer->adding ? _examples : 0u;
    const u32 _to   = _viewer->adding ? _n : _examples;
    for(u32 _k = _from; _k < _to && _viewer->row_count < KANA_VIEWER_ROWS; _k++) {
        if(kana_kanji_word_at(_viewer->db, _words[_k], &_w)) {
            kana_viewer_row_set(&_viewer->rows[_viewer->row_count++], _viewer->adding ? KANA_VIEWER_ROW_TO_ADD : KANA_VIEWER_ROW_EXAMPLE,
                                _viewer->adding && kana_userwords_has(_codepoint, _w.written, _w.reading), &_w);
        }
    }
}

RDE_INTERNAL b8 kana_viewer_inside(rde_vec_2F _p, rde_vec_2F _min, rde_vec_2F _max) {
    return _p.x >= _min.x && _p.x <= _max.x && _p.y >= _min.y && _p.y <= _max.y && _max.x > _min.x;
}

RDE_INTERNAL i32 kana_viewer_row_at(const kana_viewer* _viewer, rde_vec_2F _p) {
    if(!kana_viewer_inside(_p, _viewer->list_min, _viewer->list_max) || _viewer->row_h <= 0.0f) {
        return -1;
    }
    const i32 _i = (i32)floorf((_viewer->list_max.y - _p.y + _viewer->scroll.offset) / _viewer->row_h);
    return _i >= 0 && _i < (i32)_viewer->row_count ? _i : -1;
}

void kana_viewer_pointer_down(kana_viewer* _viewer, rde_vec_2F _screen, f64 _time) {
    _viewer->add_pressed = kana_viewer_inside(_screen, _viewer->add_min, _viewer->add_max);
    _viewer->in_list     = !_viewer->add_pressed && kana_viewer_inside(_screen, _viewer->list_min, _viewer->list_max);
    if(_viewer->in_list) {
        kana_scroller_down(&_viewer->scroll, _screen, _time);
        _viewer->pressed = kana_viewer_row_at(_viewer, _screen);
    }
}

void kana_viewer_pointer_moved(kana_viewer* _viewer, rde_vec_2F _screen, f64 _time) {
    if(_viewer->in_list) {
        kana_scroller_moved(&_viewer->scroll, _screen, _time);
        if(_viewer->scroll.dragging) {
            _viewer->pressed = -1;
        }
    } else if(_viewer->add_pressed && !kana_viewer_inside(_screen, _viewer->add_min, _viewer->add_max)) {
        _viewer->add_pressed = false;   // slid off it
    }
}

void kana_viewer_pointer_up(kana_viewer* _viewer, f64 _time) {
    if(_viewer->in_list) {
        kana_scroller_up(&_viewer->scroll, _time);
    }
    if(_viewer->add_pressed) {
        kana_viewer_set_adding(_viewer, true);
    }
    _viewer->in_list     = false;
    _viewer->add_pressed = false;
    _viewer->pressed     = -1;
}

void kana_viewer_set_adding(kana_viewer* _viewer, b8 _adding) {
    _viewer->adding      = _adding;
    _viewer->_tapped[0]  = 0;
}

void kana_viewer_update(kana_viewer* _viewer, f32 _dt) {
    if(!_viewer->open) {
        return;
    }
    kana_scroller_update(&_viewer->scroll, _dt, (f32)_viewer->row_count * _viewer->row_h, _viewer->list_max.y - _viewer->list_min.y);
    rde_vec_2F _at;
    if(!kana_scroller_take_tap(&_viewer->scroll, &_at)) {
        return;
    }
    const i32 _row = kana_viewer_row_at(_viewer, _at);
    if(_row < 0) {
        return;
    }
    const kana_viewer_row* _r = &_viewer->rows[_row];
    if(!_viewer->adding) {
        snprintf(_viewer->_tapped, sizeof(_viewer->_tapped), "%s", _r->written);   // to practise
        return;
    }
    // In Add: on or off the learner's words.
    const u32 _cp = kana_viewer_codepoint(_viewer);
    if(_r->ticked) {
        kana_userwords_remove(_cp, _r->written, _r->reading);
    } else {
        kana_userwords_add(_cp, _r->written, _r->reading, _r->meaning);
    }
}

u32 kana_viewer_take_word(kana_viewer* _viewer, u32* _out, u32 _max) {
    if(_viewer->_tapped[0] == 0) {
        return 0;
    }
    u32       _n = 0;
    const c8* _p = _viewer->_tapped;
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
    _viewer->_tapped[0] = 0;
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
    _viewer->adding   = false;
    _viewer->rows_for = UINT32_MAX;   // listed again
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
    _viewer->open   = false;
    _viewer->adding = false;
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

// --- the page ----------------------------------------------------------------------

RDE_INTERNAL b8 kana_viewer_is_kana(u32 _cp) {
    return (_cp >= 0x3041u && _cp <= 0x3096u) || (_cp >= 0x30A1u && _cp <= 0x30FAu);
}

// The words' rows, _visible of them from _top down between _x0 and _right,
// scrolled; each written, reading and meaning — then › (tap: practise) or, in
// Add, a tick. The area is kept for the pointer.
RDE_INTERNAL void kana_viewer_draw_rows(kana_viewer* _viewer, rde_window* _window, rde_font* _font, f32 _font_px, f32 _x0, f32 _right, f32 _top, u32 _visible,
                                        const c8* _empty) {
    const kana_theme* _theme = kana_theme_active();
    const f32         _h     = KANA_VIEWER_ROW;
    _viewer->row_h    = _h;
    _viewer->list_min = (rde_vec_2F){ _x0, _top - (f32)_visible * _h };
    _viewer->list_max = (rde_vec_2F){ _right, _top };
    if(_viewer->row_count == 0) {
        kana_draw_text(_font, _font_px, _empty, _x0 + 8.0f, _top - _h * 0.5f - 6.0f, 16.0f, _theme->text_soft);
        return;
    }

    // The columns: the widest written form and reading.
    c8        _line[256];
    const f32 _em        = kana_draw_text_width(_font, _font_px, "\xE3\x81\x82", 1.0f);   // a Japanese character (あ), per unit of size
    f32       _written_w = 2.0f * _em * KANA_VIEWER_WORD_PX;
    f32       _reading_w = 3.0f * _em * KANA_VIEWER_SMALL_PX;
    for(u32 _i = 0; _i < _viewer->row_count; _i++) {
        _written_w = fmaxf(_written_w, kana_draw_text_width(_font, _font_px, _viewer->rows[_i].written, KANA_VIEWER_WORD_PX));
        _reading_w = fmaxf(_reading_w, kana_draw_text_width(_font, _font_px, _viewer->rows[_i].reading, KANA_VIEWER_SMALL_PX));
    }
    _written_w = fminf(_written_w, 5.0f * _em * KANA_VIEWER_WORD_PX);
    _reading_w = fminf(_reading_w, 8.0f * _em * KANA_VIEWER_SMALL_PX);
    const f32 _reading_x = _x0 + 12.0f + _written_w + 16.0f;
    const f32 _meaning_x = _reading_x + _reading_w + 16.0f;

    const f32 _bottom = _top - (f32)_visible * _h;
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)((_x0 + _right) * 0.5f), (i32)((_top + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _x0), (u32)(_top - _bottom) });
    for(u32 _i = 0; _i < _viewer->row_count; _i++) {
        const kana_viewer_row* _r        = &_viewer->rows[_i];
        const f32              _row_top  = _top - (f32)_i * _h + _viewer->scroll.offset;
        if(_row_top - _h > _top || _row_top < _bottom) {
            continue;
        }
        const f32 _mid      = _row_top - _h * 0.5f;
        const f32 _baseline = _mid - KANA_VIEWER_SMALL_PX * 0.35f;
        if(_viewer->pressed == (i32)_i) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ (_x0 + _right) * 0.5f, _mid }, (rde_vec_2F){ _right - _x0, _h }, _theme->select_fill);
        }
        if(_r->kind == KANA_VIEWER_ROW_YOURS || _r->kind == KANA_VIEWER_ROW_TYPED) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ _x0 + 2.0f, _mid }, (rde_vec_2F){ 4.0f, _h - 12.0f }, _theme->button_selected);   // the learner's
        }
        kana_draw_text_fit(_font, _font_px, _r->written, KANA_VIEWER_WORD_PX, _written_w, _line, sizeof(_line));
        kana_draw_text(_font, _font_px, _line, _x0 + 12.0f, _mid - KANA_VIEWER_WORD_PX * 0.38f, KANA_VIEWER_WORD_PX, _theme->ink);
        kana_draw_text_fit(_font, _font_px, _r->reading, KANA_VIEWER_SMALL_PX, _reading_w, _line, sizeof(_line));
        kana_draw_text(_font, _font_px, _line, _reading_x, _baseline, KANA_VIEWER_SMALL_PX, _theme->text_soft);
        kana_draw_text_fit(_font, _font_px, _r->meaning, KANA_VIEWER_SMALL_PX, _right - 40.0f - _meaning_x, _line, sizeof(_line));
        kana_draw_text(_font, _font_px, _line, _meaning_x, _baseline, KANA_VIEWER_SMALL_PX, _theme->text);
        if(_r->kind == KANA_VIEWER_ROW_TO_ADD || _r->kind == KANA_VIEWER_ROW_TYPED) {
            // A tick: on, the learner's; off, one to add.
            const rde_vec_2F _c = { _right - 20.0f, _mid };
            if(_r->ticked) {
                rde_rendering_2d_draw_circle(_c, 11.0f, 24, _theme->button_selected, NULL);
                kana_draw_line((rde_vec_2F){ _c.x - 5.0f, _c.y }, (rde_vec_2F){ _c.x - 1.5f, _c.y - 4.0f }, 1.4f, _theme->button_text);
                kana_draw_line((rde_vec_2F){ _c.x - 1.5f, _c.y - 4.0f }, (rde_vec_2F){ _c.x + 5.5f, _c.y + 4.5f }, 1.4f, _theme->button_text);
            } else {
                rde_rendering_2d_draw_circle_border(_c, 11.0f, 1.5f, 24, _theme->text_soft, NULL);
            }
        } else {
            kana_draw_text(_font, _font_px, "\xE2\x80\xBA", _right - 14.0f, _baseline, KANA_VIEWER_SMALL_PX + 4.0f, _theme->text_soft);   // ›: it opens
        }
        kana_draw_line((rde_vec_2F){ _x0, _row_top - _h }, (rde_vec_2F){ _right, _row_top - _h }, 0.5f, _theme->line);
    }
    rde_rendering_end_clipping_rect();
}

// Add: the character small, what to do, and every word to pick from.
RDE_INTERNAL void kana_viewer_render_adding(kana_viewer* _viewer, rde_window* _window, rde_font* _font, f32 _font_px, const kana_kanji_info* _info,
                                            f32 _left, f32 _right, f32 _top, f32 _bottom) {
    const kana_theme* _theme = kana_theme_active();
    const f32         _size  = fminf(170.0f, (_right - _left) * 0.3f);
    const rde_vec_2F  _tl    = { _left, _top - 48.0f };
    kana_glyph_box(_tl, _size);
    kana_glyph_writing(&_viewer->glyph, _info, _tl, _size, rde_engine_get_time_now() - _viewer->started, _font, _font_px, 13.0f);

    const f32 _tx = _left + _size + 24.0f;
    kana_draw_text(_font, _font_px, "Add words", _tx, _tl.y - 30.0f, 26.0f, _theme->text);
    c8 _line[160];
    snprintf(_line, sizeof(_line), "%u of yours", kana_userwords_count(_info->codepoint));
    kana_draw_text(_font, _font_px, _line, _tx, _tl.y - 62.0f, 17.0f, _theme->text_soft);
    kana_draw_text(_font, _font_px, "Tap a word to add it to yours, or to take it off.", _tx, _tl.y - 96.0f, 16.0f, _theme->text_soft);
    kana_draw_text(_font, _font_px, "Something else? Type your own.", _tx, _tl.y - 122.0f, 16.0f, _theme->text_soft);

    const f32 _list_top = _tl.y - _size - 20.0f;
    kana_draw_line((rde_vec_2F){ _left, _list_top }, (rde_vec_2F){ _right, _list_top }, 0.5f, _theme->line);
    const u32 _visible = (u32)fmaxf(1.0f, floorf((_list_top - _bottom) / KANA_VIEWER_ROW));
    kana_viewer_draw_rows(_viewer, _window, _font, _font_px, _left, _right, _list_top, _visible, "No more dictionary words for this one: Type your own.");
}

void kana_viewer_render(kana_viewer* _viewer, rde_window* _window, rde_font* _font, f32 _font_px, f32 _bottom_bar) {
    _viewer->list_min = _viewer->list_max = (rde_vec_2F){ 0.0f, 0.0f };
    _viewer->add_min  = _viewer->add_max  = (rde_vec_2F){ 0.0f, 0.0f };
    if(!_viewer->open || rde_arr_length(&_viewer->list) == 0) {
        return;
    }

    kana_kanji_info _info;
    const u32 _record = ((const u32*)_viewer->list.memory)[_viewer->position];
    if(!kana_kanji_at(_viewer->db, _record, &_info)) {
        return;
    }
    kana_viewer_list_rows(_viewer, _record, _info.codepoint);

    const rde_vec_2I _size_px = rde_window_get_size(_window);
    const rde_vec_4I _insets  = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32 _hw     = (f32)_size_px.x * 0.5f;
    const f32 _hh     = (f32)_size_px.y * 0.5f;
    const f32 _top    = _hh - (f32)_insets.y - KANA_VIEWER_MARGIN;
    const f32 _bottom = -_hh + (f32)_insets.w + _bottom_bar + KANA_VIEWER_MARGIN;
    const f32 _left   = -_hw + (f32)_insets.x + KANA_VIEWER_MARGIN;
    const f32 _right  = _hw - (f32)_insets.z - KANA_VIEWER_MARGIN;

    // --- header: what it is -------------------------------------------------------
    const b8 _kana = !kana_viewer_is_kanji(_info.codepoint);
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
    const b8 _is_kana = (_info.codepoint >= 0x3041u && _info.codepoint <= 0x3096u) || (_info.codepoint >= 0x30A1u && _info.codepoint <= 0x30FAu);
    snprintf(_line, sizeof(_line), "%s   %u stroke%s%s      %u / %u",
             _is_kana ? (_info.codepoint < 0x30A0u ? "Hiragana" : "Katakana") : "Kanji",
             _info.strokes, _info.strokes == 1 ? "" : "s", _extra,
             _viewer->position + 1u, (u32)rde_arr_length(&_viewer->list));
    kana_draw_text(_font, _font_px, _line, _left, _top - 16.0f, 22.0f, kana_theme_active()->text);

    if(_viewer->adding && !_kana) {
        kana_viewer_render_adding(_viewer, _window, _font, _font_px, &_info, _left, _right, _top, _bottom);
        return;
    }

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

    // --- the character, and where its details go --------------------------------------
    // The words show as many rows as fit (they scroll); at least one, for "none yet".
    const u32 _rows    = _viewer->row_count > 0u ? _viewer->row_count : 1u;
    u32       _visible = _kana ? 0u : (_rows < 6u ? _rows : 6u);
    const f32 _details = _kana ? 0.0f : (_part_count > 0 ? 4.0f : 3.0f) * KANA_VIEWER_LINE;   // On, Kun, meaning, parts
    const b8  _wide    = !_kana && (_right - _left) > (_top - _bottom) * 1.25f;
    f32        _size;
    rde_vec_2F _tl;
    f32        _x0;   // the details' left
    f32        _y0;   // and top
    if(_wide) {
        // Beside it: the character as tall as the screen allows, up to under half
        // its width; the words as many rows as the column holds.
        _size = fmaxf(120.0f, fminf(_top - 48.0f - _bottom, (_right - _left) * 0.45f));
        _tl   = (rde_vec_2F){ _left, _top - 48.0f };
        _x0   = _left + _size + KANA_VIEWER_GUTTER;
        _y0   = _tl.y - 8.0f;
        const f32 _fit = floorf((_y0 - _details - KANA_VIEWER_TITLE - _bottom) / KANA_VIEWER_ROW);
        _visible = _kana || _fit < 1.0f ? 0u : (u32)fminf((f32)_rows, _fit);
    } else {
        // Under it: rows go, down to three, before the character gets small.
        const f32 _room = (_top - 48.0f) - (_bottom + _details);
        #define KANA_VIEWER_WORDS_H(n) ((n) > 0u ? KANA_VIEWER_TITLE + (f32)(n) * KANA_VIEWER_ROW : 0.0f)
        while(_visible > 3u && _room - KANA_VIEWER_WORDS_H(_visible) < KANA_VIEWER_MIN_CHAR) {
            _visible--;
        }
        if(_room - KANA_VIEWER_WORDS_H(_visible) < 160.0f) {
            _visible = 0;   // a small screen: the character first
        }
        _size = fmaxf(120.0f, fminf(_right - _left, _room - KANA_VIEWER_WORDS_H(_visible)));
        #undef KANA_VIEWER_WORDS_H
        _tl   = (rde_vec_2F){ -_size * 0.5f, _top - 48.0f };
        _x0   = _left;
        _y0   = _tl.y - _size - 20.0f;
    }

    kana_glyph_box(_tl, _size);
    kana_glyph_writing(&_viewer->glyph, &_info, _tl, _size, rde_engine_get_time_now() - _viewer->started, _font, _font_px, 18.0f);
    kana_selection_draw_mark(kana_marks_get(_info.codepoint), _tl, _size, false);
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
    kana_draw_text_fit(_font, _font_px, _meanings[0] != 0 ? _meanings : "(no meaning listed)", 22.0f, _right - _x0, _line, sizeof(_line));
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

    // --- words: the learner's, then the examples; Add by the title ------------------
    if(_visible == 0) {
        return;
    }
    const f32 _wy = _y0 - _details;
    kana_draw_text(_font, _font_px, "Words", _x0, _wy - KANA_VIEWER_TITLE + 12.0f, 20.0f, _theme->text_soft);
    {
        const c8* _label = KANA_ICON_PLUS " Add";
        const f32 _w     = kana_draw_text_width(_font, _font_px, _label, 17.0f) + 28.0f;
        _viewer->add_min = (rde_vec_2F){ _right - _w, _wy - KANA_VIEWER_TITLE + 4.0f };
        _viewer->add_max = (rde_vec_2F){ _right, _wy - 2.0f };
        rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ (_viewer->add_min.x + _viewer->add_max.x) * 0.5f, (_viewer->add_min.y + _viewer->add_max.y) * 0.5f },
                                                (rde_vec_2F){ _w, _viewer->add_max.y - _viewer->add_min.y }, 0.6f, 8,
                                                _viewer->add_pressed ? kana_theme_active()->button_selected : kana_theme_active()->button, NULL);
        kana_draw_text(_font, _font_px, _label, _viewer->add_min.x + 14.0f, (_viewer->add_min.y + _viewer->add_max.y) * 0.5f - 6.0f, 17.0f,
                       _theme->button_text);
    }
    kana_draw_line((rde_vec_2F){ _x0, _wy - KANA_VIEWER_TITLE }, (rde_vec_2F){ _right, _wy - KANA_VIEWER_TITLE }, 0.5f, _theme->line);
    kana_viewer_draw_rows(_viewer, _window, _font, _font_px, _x0, _right, _wy - KANA_VIEWER_TITLE, _visible, "No words yet: Add some.");
}
