#include "vocabview.h"
#include "draw.h"
#include "icons.h"
#include "review.h"
#include "speech.h"
#include "text.h"
#include "theme.h"
#include "wordcard.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See vocabview.h. Drawn by hand like the other screens; the rows virtualised
// (only those in view are drawn), the list kept until the vocabulary changes.
// ===========================================================================

#define KANA_VOCABVIEW_TITLE_PX   17.0f
#define KANA_VOCABVIEW_CAPTION_PX 10.0f
#define KANA_VOCABVIEW_CHIP_H     36.0f
#define KANA_VOCABVIEW_CHIP_GAP   8.0f
#define KANA_VOCABVIEW_CHIP_PX    13.0f
#define KANA_VOCABVIEW_ROW        56.0f
#define KANA_VOCABVIEW_WORD_PX    20.0f
#define KANA_VOCABVIEW_SMALL_PX   13.0f

void kana_vocabview_init(kana_vocabview* _view, const kana_kanji_db* _db) {
    memset(_view, 0, sizeof(*_view));
    _view->db         = _db;
    _view->ids        = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    _view->_listed_at = UINT32_MAX;
    _view->pressed    = -1;
    kana_glyph_init(&_view->glyph, _db);
}

void kana_vocabview_destroy(kana_vocabview* _view) {
    if(rde_arr_is_inited(&_view->ids)) {
        rde_arr_free(&_view->ids);
    }
    kana_glyph_destroy(&_view->glyph);
    memset(_view, 0, sizeof(*_view));
}

void kana_vocabview_open(kana_vocabview* _view) {
    _view->open       = true;
    _view->_listed_at = UINT32_MAX;
    _view->pressed    = -1;
    kana_scroller_stop(&_view->scroller);
    _view->scroller.offset = 0.0f;
}

void kana_vocabview_close(kana_vocabview* _view) {
    _view->open = false;
}

void kana_vocabview_show_list(kana_vocabview* _view, u32 _list) {
    _view->list       = _list;
    _view->_listed_at = UINT32_MAX;
    kana_scroller_stop(&_view->scroller);
    _view->scroller.offset = 0.0f;
    kana_wordcard_prefer_list(_list);   // + Word saves in it
}

// The words of the list shown, newest first (again when the vocabulary changed).
RDE_INTERNAL void kana_vocabview_list(kana_vocabview* _view) {
    if(_view->_listed_at == kana_vocab_revision() && _view->_listed_list == _view->list) {
        return;
    }
    if(_view->list != 0u && kana_vocab_list_name(_view->list)[0] == 0) {
        _view->list = 0u;   // the list went: every word
    }
    _view->_listed_at   = kana_vocab_revision();
    _view->_listed_list = _view->list;
    rde_arr_clear(&_view->ids);
    if(_view->list == 0u) {
        for(u32 _i = kana_vocab_count(); _i-- > 0;) {
            const u32 _id = kana_vocab_at(_i)->id;
            rde_arr_add(&_view->ids, (any)&_id);
        }
    } else {
        const u32 _n = kana_vocab_list_words(_view->list, NULL, 0u);
        u32*      _w = (u32*)rde_arr_add_n(&_view->ids, _n);
        kana_vocab_list_words(_view->list, _w, _n);
        for(u32 _a = 0, _b = _n > 0 ? _n - 1u : 0u; _a < _b; _a++, _b--) {   // newest first
            const u32 _t = _w[_a];
            _w[_a]       = _w[_b];
            _w[_b]       = _t;
        }
    }
}

u32 kana_vocabview_words(kana_vocabview* _view, const u32** _ids) {
    kana_vocabview_list(_view);
    *_ids = (const u32*)_view->ids.memory;
    return (u32)rde_arr_length(&_view->ids);
}

RDE_INTERNAL b8 kana_vocabview_is_kanji(u32 _cp) {
    return (_cp >= 0x4E00u && _cp <= 0x9FFFu) || (_cp >= 0x3400u && _cp <= 0x4DBFu) || _cp == 0x3005u;
}

u32 kana_vocabview_records(kana_vocabview* _view, u32* _out, u32 _max) {
    const u32* _ids;
    const u32  _n = kana_vocabview_words(_view, &_ids);
    u32        _r = 0;
    for(u32 _i = _n; _i-- > 0 && _r < _max;) {   // oldest first
        const kana_vocab_word* _w = kana_vocab_get(_ids[_i]);
        if(_w == NULL || _view->db == NULL) {
            continue;
        }
        // Its kanji; a word with none (すし, テレビ), its kana.
        b8 _kanji = false;
        for(const c8* _p = _w->written; *_p != 0;) {
            _kanji = _kanji || kana_vocabview_is_kanji(kana_kanji_utf8_next(&_p));
        }
        for(const c8* _p = _w->written; *_p != 0 && _r < _max;) {
            const u32 _cp = kana_kanji_utf8_next(&_p);
            u32       _record;
            if((_kanji && !kana_vocabview_is_kanji(_cp)) || _cp == 0x3005u || !kana_kanji_find_index(_view->db, _cp, &_record)) {
                continue;
            }
            b8 _seen = false;
            for(u32 _k = 0; _k < _r && !_seen; _k++) {
                _seen = _out[_k] == _record;
            }
            if(!_seen) {
                _out[_r++] = _record;
            }
        }
    }
    return _r;
}

u32 kana_vocabview_due(kana_vocabview* _view, u32* _out, u32 _max) {
    const u32* _ids;
    const u32  _n = kana_vocabview_words(_view, &_ids);
    if(_n == 0u) {
        return 0u;
    }
    // Oldest first: the new ones the day has room for are the longest saved.
    u32* _keys = (u32*)rde_malloc(sizeof(u32) * _n);
    for(u32 _i = 0; _i < _n; _i++) {
        _keys[_i] = KANA_VOCAB_KEY(_ids[_n - 1u - _i]);
    }
    u32* _due = (u32*)rde_malloc(sizeof(u32) * (_max > 0 ? _max : 1u));
    const u32 _d = kana_reviews_due(_keys, _n, _due, _max);
    for(u32 _i = 0; _i < _d; _i++) {
        _out[_i] = _due[_i] & ~KANA_VOCAB_KEY_BIT;
    }
    rde_free(_due);
    rde_free(_keys);
    return _d;
}

// --- the pointer -----------------------------------------------------------------------

RDE_INTERNAL b8 kana_vocabview_inside(rde_vec_2F _p, rde_vec_2F _min, rde_vec_2F _max) {
    return _p.x >= _min.x && _p.x <= _max.x && _p.y >= _min.y && _p.y <= _max.y && _max.x > _min.x;
}

RDE_INTERNAL i32 kana_vocabview_row_at(const kana_vocabview* _view, rde_vec_2F _p) {
    if(!kana_vocabview_inside(_p, _view->rows_min, _view->rows_max)) {
        return -1;
    }
    const i32 _i = (i32)floorf((_view->rows_max.y - _p.y + _view->scroller.offset) / KANA_VOCABVIEW_ROW);
    return _i >= 0 && _i < (i32)rde_arr_length(&_view->ids) ? _i : -1;
}

void kana_vocabview_pointer_down(kana_vocabview* _view, rde_vec_2F _screen, f64 _time) {
    _view->in_rows = kana_vocabview_inside(_screen, _view->rows_min, _view->rows_max);
    _view->pressed = _view->in_rows ? kana_vocabview_row_at(_view, _screen) : -1;
    kana_scroller_down(&_view->scroller, _screen, _time);
}

void kana_vocabview_pointer_moved(kana_vocabview* _view, rde_vec_2F _screen, f64 _time) {
    kana_scroller_moved(&_view->scroller, _screen, _time);
    if(_view->scroller.dragging) {
        _view->pressed = -1;
    }
}

void kana_vocabview_pointer_up(kana_vocabview* _view, f64 _time) {
    kana_scroller_up(&_view->scroller, _time);
    _view->pressed = -1;
}

RDE_INTERNAL void kana_vocabview_tap(kana_vocabview* _view, rde_vec_2F _at) {
    for(u32 _c = 0; _c < _view->chip_count; _c++) {
        const kana_vocabview_chip* _chip = &_view->chips[_c];
        if(!kana_vocabview_inside(_at, _chip->min, _chip->max)) {
            continue;
        }
        if(_chip->list == UINT32_MAX) {
            kana_wordcard_ask_list(0u);   // a new one, named
        } else if(_chip->list != 0u && _chip->list == _view->list) {
            kana_wordcard_ask_list(_chip->list);   // the one shown: renamed or deleted
        } else {
            kana_vocabview_show_list(_view, _chip->list);
        }
        return;
    }
    const i32 _row = kana_vocabview_row_at(_view, _at);
    if(_row < 0) {
        return;
    }
    const u32              _id = ((const u32*)_view->ids.memory)[_row];
    const kana_vocab_word* _w  = kana_vocab_get(_id);
    if(_w == NULL) {
        return;
    }
    if(kana_speech_available() && _at.x >= _view->reading_x0 && _at.x < _view->reading_x1) {
        kana_speak(_w->reading[0] != 0 ? _w->reading : _w->written);   // its reading: aloud
        return;
    }
    kana_wordcard_ask_saved(_id);
}

void kana_vocabview_update(kana_vocabview* _view, f32 _dt) {
    if(!_view->open) {
        return;
    }
    kana_vocabview_list(_view);
    const u32 _made = kana_wordcard_take_new_list();
    if(_made != 0u) {
        kana_vocabview_show_list(_view, _made);   // a list just made: shown
    }
    kana_scroller_update(&_view->scroller, _dt, _view->content_h, _view->rows_max.y - _view->rows_min.y);
    rde_vec_2F _at;
    if(kana_scroller_take_tap(&_view->scroller, &_at)) {
        kana_vocabview_tap(_view, _at);
    }
}

// --- drawing ---------------------------------------------------------------------------

// _text in one line no wider than _width: as it is, or a little smaller, or (a
// list "a; b; c") its first items. Into _out; the size to draw it at.
RDE_INTERNAL f32 kana_vocabview_fit(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width, c8* _out, usize _size) {
    snprintf(_out, _size, "%s", _text);
    for(;;) {
        if(kana_draw_text_width(_font, _font_px, _out, _px) <= _width) {
            return _px;
        }
        c8* _cut = NULL;
        for(c8* _c = strstr(_out, "; "); _c != NULL; _c = strstr(_c + 1, "; ")) {
            _cut = _c;
        }
        if(_cut == NULL) {
            return kana_draw_text_px_to_fit(_font, _font_px, _out, _px, _width, 0.6f);
        }
        *_cut = 0;
    }
}

void kana_vocabview_render(kana_vocabview* _view, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_view->open) {
        return;
    }
    kana_vocabview_list(_view);
    const kana_theme* _theme  = kana_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _insets = rde_window_get_safe_area_insets(_window);
    const f32         _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + 24.0f;
    const f32         _right  = (f32)_size.x * 0.5f - (f32)_insets.z - 24.0f;
    c8                _line[256];

    // The header: 語, "Vocabulary", how many words and lists.
    {
        const f32 _box = 40.0f;
        kana_draw_card((rde_vec_2F){ _left, _top - _box }, (rde_vec_2F){ _left + _box, _top }, _box * 0.26f, _theme->tint, _theme->tint);
        kana_glyph_character(&_view->glyph, 0x8A9Eu, (rde_vec_2F){ _left + _box * 0.14f, _top - _box * 0.14f }, _box * 0.72f, _theme->accent);   // 語
        kana_draw_text(_font, _font_px, kana_text(KANA_TEXT_VOCAB), _left + 54.0f, _top - 19.0f, KANA_VOCABVIEW_TITLE_PX, _theme->text);
        c8 _words[64], _lists[64];
        KANA_TEXTF(_words, KANA_TEXT_VOCAB_WORDS_N, KANA_TN(kana_vocab_count()));
        KANA_TEXTF(_lists, KANA_TEXT_VOCAB_LISTS_N, KANA_TN(kana_vocab_list_count()));
        snprintf(_line, sizeof(_line), "%s \xC2\xB7 %s", _words, _lists);   // ·
        kana_draw_text(_font, _font_px, _line, _left + 54.0f, _top - 37.0f, KANA_VOCABVIEW_CAPTION_PX, _theme->text_soft);
    }

    // The lists: All, each list, + New list; the one shown in the accent.
    f32 _y = _top - 60.0f;
    {
        _view->chip_count = 0;
        f32 _x = _left;
        for(u32 _c = 0; _c < kana_vocab_list_count() + 2u; _c++) {
            u32 _list;
            if(_c == 0u) {
                _list = 0u;
                c8 _n[32];
                snprintf(_n, sizeof(_n), "%u", kana_vocab_count());
                snprintf(_line, sizeof(_line), "%s  %s", kana_text(KANA_TEXT_ALL), _n);
            } else if(_c <= kana_vocab_list_count()) {
                _list = kana_vocab_list_at(_c - 1u);
                snprintf(_line, sizeof(_line), "%s  %u", kana_vocab_list_name(_list), kana_vocab_list_words(_list, NULL, 0u));
            } else {
                _list = UINT32_MAX;
                snprintf(_line, sizeof(_line), KANA_ICON_PLUS " %s", kana_text(KANA_TEXT_WORD_NEW_LIST));
            }
            const b8  _shown = _list == _view->list;
            const f32 _w     = fminf(_right - _left, kana_draw_text_width(_font, _font_px, _line, KANA_VOCABVIEW_CHIP_PX) + (_shown && _list != 0u ? 52.0f : 32.0f));
            if(_x + _w > _right && _x > _left) {
                _x  = _left;
                _y -= KANA_VOCABVIEW_CHIP_H + KANA_VOCABVIEW_CHIP_GAP;
            }
            const rde_vec_2F _min = { _x, _y - KANA_VOCABVIEW_CHIP_H };
            const rde_vec_2F _max = { _x + _w, _y };
            rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ _x + _w * 0.5f, _y - KANA_VOCABVIEW_CHIP_H * 0.5f }, (rde_vec_2F){ _w, KANA_VOCABVIEW_CHIP_H }, 1.0f, 10,
                                                    _shown ? _theme->accent : _theme->surface_2, NULL);
            const rde_color _ink = _shown ? _theme->on_accent : _list == UINT32_MAX ? _theme->accent : _theme->text;
            const f32 _px = kana_draw_text_px_to_fit(_font, _font_px, _line, KANA_VOCABVIEW_CHIP_PX, _w - 28.0f, 0.6f);
            kana_draw_text(_font, _font_px, _line, _x + 16.0f, _y - KANA_VOCABVIEW_CHIP_H * 0.5f - _px * 0.42f, _px, _ink);
            if(_shown && _list != 0u) {
                kana_draw_icon(_font, _font_px, KANA_ICON_NOTE_EDIT, (rde_vec_2F){ _x + _w - 20.0f, _y - KANA_VOCABVIEW_CHIP_H * 0.5f }, 14.0f, _ink);   // tap again: rename
            }
            _view->chips[_view->chip_count++] = (kana_vocabview_chip){ _min, _max, _list };
            _x += _w + KANA_VOCABVIEW_CHIP_GAP;
        }
        _y -= KANA_VOCABVIEW_CHIP_H + 14.0f;
    }

    // The words.
    const u32  _n   = (u32)rde_arr_length(&_view->ids);
    const u32* _ids = (const u32*)_view->ids.memory;
    _view->rows_min  = (rde_vec_2F){ _left, _bottom };
    _view->rows_max  = (rde_vec_2F){ _right, _y };
    _view->content_h = (f32)_n * KANA_VOCABVIEW_ROW;
    if(_n == 0u) {
        kana_draw_card((rde_vec_2F){ _left, _y - 84.0f }, (rde_vec_2F){ _right, _y }, 14.0f, _theme->surface, _theme->outline);
        const KANA_TEXT_ _empty = _view->list != 0u ? KANA_TEXT_VOCAB_LIST_EMPTY : KANA_TEXT_VOCAB_EMPTY;
        kana_draw_text_wrap(_font, _font_px, kana_text(_empty), _left + 18.0f, _y - 30.0f, 14.0f, _right - _left - 36.0f, 20.0f, _theme->text_soft);
        return;
    }
    // Columns: the written form, the reading (after its speaker), the meaning; the
    // review's state and › at the right.
    const b8  _speak     = kana_speech_available();
    const f32 _inner     = _right - _left;
    const f32 _written_w = fminf(220.0f, _inner * 0.26f);
    const f32 _reading_x = _left + 14.0f + _written_w + (_speak ? 30.0f : 12.0f);
    const f32 _reading_w = fminf(200.0f, _inner * 0.22f);
    const f32 _meaning_x = _reading_x + _reading_w + 14.0f;
    const f32 _state_w   = 86.0f;
    const f32 _meaning_w = _right - 30.0f - _state_w - _meaning_x;
    _view->reading_x0 = _speak ? _reading_x - 28.0f : 0.0f;
    _view->reading_x1 = _speak ? _meaning_x - 6.0f : 0.0f;
    const u32 _today  = kana_reviews_today();

    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)((_left + _right) * 0.5f), (i32)((_y + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left), (u32)fmaxf(1.0f, _y - _bottom) });
    const u32 _first = (u32)fmaxf(0.0f, floorf(_view->scroller.offset / KANA_VOCABVIEW_ROW));
    for(u32 _i = _first; _i < _n; _i++) {
        const f32 _row_top = _y - (f32)_i * KANA_VOCABVIEW_ROW + _view->scroller.offset;
        if(_row_top < _bottom) {
            break;
        }
        const kana_vocab_word* _w = kana_vocab_get(_ids[_i]);
        if(_w == NULL) {
            continue;
        }
        const f32 _mid = _row_top - KANA_VOCABVIEW_ROW * 0.5f;
        if(_view->pressed == (i32)_i) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ (_left + _right) * 0.5f, _mid }, (rde_vec_2F){ _inner, KANA_VOCABVIEW_ROW }, _theme->select_fill);
        }
        c8  _fit[KANA_USERWORD_MEANING];
        f32 _px = kana_draw_text_px_to_fit(_font, _font_px, _w->written, KANA_VOCABVIEW_WORD_PX, _written_w, 0.5f);
        kana_draw_text(_font, _font_px, _w->written, _left + 14.0f, _mid - _px * 0.38f, _px, _theme->ink);
        if(_speak) {
            kana_draw_icon(_font, _font_px, KANA_ICON_SPEAK, (rde_vec_2F){ _reading_x - 14.0f, _mid }, 14.0f, _theme->accent);
        }
        _px = kana_draw_text_px_to_fit(_font, _font_px, _w->reading, KANA_VOCABVIEW_SMALL_PX, _reading_w, 0.6f);
        kana_draw_text(_font, _font_px, _w->reading, _reading_x, _mid - _px * 0.36f, _px, _theme->text_soft);
        _px = kana_vocabview_fit(_font, _font_px, _w->meaning, KANA_VOCABVIEW_SMALL_PX, _meaning_w, _fit, sizeof(_fit));
        kana_draw_text(_font, _font_px, _fit, _meaning_x, _mid - _px * 0.36f, _px, _theme->text);
        // Its reviews: new, due, or when.
        const u32 _due = kana_reviews_due_day(KANA_VOCAB_KEY(_w->id));
        rde_color _c   = _theme->text_soft;
        if(_due == 0u) {
            snprintf(_line, sizeof(_line), "%s", kana_text(KANA_TEXT_REVIEW_NEW));
        } else if(_due <= _today) {
            snprintf(_line, sizeof(_line), "%s", kana_text(KANA_TEXT_REVIEW_DUE));
            _c = _theme->accent;
        } else {
            KANA_TEXTF(_line, KANA_TEXT_REVIEW_IN_N, KANA_TN(_due - _today));
        }
        _px = kana_draw_text_px_to_fit(_font, _font_px, _line, 11.0f, _state_w, 0.7f);
        kana_draw_text(_font, _font_px, _line, _right - 28.0f - kana_draw_text_width(_font, _font_px, _line, _px), _mid - _px * 0.36f, _px, _c);
        kana_draw_icon(_font, _font_px, KANA_ICON_NEXT, (rde_vec_2F){ _right - 12.0f, _mid }, 16.0f, _theme->text_soft);
        if(_i + 1u < _n) {
            kana_draw_line((rde_vec_2F){ _left, _row_top - KANA_VOCABVIEW_ROW }, (rde_vec_2F){ _right, _row_top - KANA_VOCABVIEW_ROW }, 0.5f, _theme->outline);
        }
    }
    rde_rendering_end_clipping_rect();
}
