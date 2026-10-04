#include "study/screens/vocabview.h"
#include "study/widgets/header.h"
#include "drawing/widgets/draw.h"
#include "drawing/widgets/icons.h"
#include "study/models/review.h"
#include "study/services/speech.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "study/widgets/wordcard.h"
#include "drawing/base/utf8.h"
#include "lang/lang.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See vocabview.h. Drawn by hand like the other screens; the rows virtualised
// (only those in view are drawn), the list kept until the vocabulary changes.
// ===========================================================================

#define FUDE_VOCABVIEW_CHIP_H     36.0f
#define FUDE_VOCABVIEW_CHIP_GAP   8.0f
#define FUDE_VOCABVIEW_CHIP_PX    13.0f
#define FUDE_VOCABVIEW_ROW        56.0f
#define FUDE_VOCABVIEW_WORD_PX    20.0f
#define FUDE_VOCABVIEW_SMALL_PX   13.0f

void fude_vocabview_init(fude_vocabview* _view, const fude_kanji_db* _db) {
    memset(_view, 0, sizeof(*_view));
    _view->db         = _db;
    _view->ids        = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    _view->_listed_at = UINT32_MAX;
    _view->pressed    = -1;
    fude_glyph_init(&_view->glyph, _db);
}

void fude_vocabview_destroy(fude_vocabview* _view) {
    if(rde_arr_is_inited(&_view->ids)) {
        rde_arr_free(&_view->ids);
    }
    fude_glyph_destroy(&_view->glyph);
    memset(_view, 0, sizeof(*_view));
}

void fude_vocabview_open(fude_vocabview* _view) {
    _view->open       = true;
    _view->_listed_at = UINT32_MAX;
    fude_vocabview_search(_view, "");   // its field starts empty
    _view->pressed    = -1;
    fude_scroller_stop(&_view->scroller);
    _view->scroller.offset = 0.0f;
}

void fude_vocabview_close(fude_vocabview* _view) {
    _view->open = false;
}

void fude_vocabview_show_list(fude_vocabview* _view, u32 _list) {
    _view->list       = _list;
    _view->_listed_at = UINT32_MAX;
    fude_scroller_stop(&_view->scroller);
    _view->scroller.offset = 0.0f;
    fude_wordcard_prefer_list(_list);   // + Word saves in it
}

// --- the search ------------------------------------------------------------------------

// _text as a search compares it: A-Z as a-z, and readings' characters as the
// language compares them (lang.h; Japanese: エキ finds えき). Never longer than _text.
RDE_INTERNAL void fude_vocabview_fold(const c8* _text, c8* _out, usize _size) {
    usize _n = 0;
    for(const c8* _p = _text; *_p != 0;) {
        u32 _cp = fude_utf8_next(&_p);
        if(_cp >= 'A' && _cp <= 'Z') {
            _cp += 'a' - 'A';
        } else {
            _cp = fude_lang_reading_fold(_cp);
            if(_cp == 0u) {
                continue;   // left out (lang.h)
            }
        }
        c8 _one[5];
        fude_utf8_put(_cp, _one);
        const usize _len = strlen(_one);
        if(_n + _len >= _size) {
            break;
        }
        memcpy(_out + _n, _one, _len);
        _n += _len;
    }
    _out[_n] = 0;
}

void fude_vocabview_search(fude_vocabview* _view, const c8* _query) {
    // Without the spaces around it.
    while(*_query == ' ') {
        _query++;
    }
    usize _len = strlen(_query);
    while(_len > 0 && _query[_len - 1u] == ' ') {
        _len--;
    }
    c8 _was[sizeof(_view->query)];
    memcpy(_was, _view->query, sizeof(_was));
    snprintf(_view->query, sizeof(_view->query), "%.*s", (int)_len, _query);
    if(strcmp(_was, _view->query) == 0) {
        return;
    }
    fude_vocabview_fold(_view->query, _view->_query_fold, sizeof(_view->_query_fold));
    // Romaji (letters only): its kana too, so eki finds えき.
    b8 _letters = _view->query[0] != 0;
    for(const c8* _c = _view->query; *_c != 0 && _letters; _c++) {
        _letters = (*_c >= 'a' && *_c <= 'z') || (*_c >= 'A' && *_c <= 'Z') || *_c == '-' || *_c == '\'';
    }
    if(!_letters || !fude_lang_reading_from_latin(_view->_query_fold, _view->_query_kana, sizeof(_view->_query_kana))) {
        _view->_query_kana[0] = 0;
    }
    _view->_listed_at = UINT32_MAX;
    fude_scroller_stop(&_view->scroller);
    _view->scroller.offset = 0.0f;
}

// The word has what is searched for: in its written form, its reading, or its meaning.
RDE_INTERNAL b8 fude_vocabview_finds(const fude_vocabview* _view, const fude_vocab_word* _w) {
    c8 _folded[FUDE_USERWORD_MEANING];
    const c8* const _parts[] = { _w->written, _w->reading, _w->meaning };
    for(u32 _i = 0; _i < 3u; _i++) {
        fude_vocabview_fold(_parts[_i], _folded, sizeof(_folded));
        if(strstr(_folded, _view->_query_fold) != NULL || (_i < 2u && _view->_query_kana[0] != 0 && strstr(_folded, _view->_query_kana) != NULL)) {
            return true;
        }
    }
    return false;
}

// --- the words -------------------------------------------------------------------------

// The words of the list shown, newest first, the search's only (again when the
// vocabulary changed).
RDE_INTERNAL void fude_vocabview_list(fude_vocabview* _view) {
    if(_view->_listed_at == fude_vocab_revision() && _view->_listed_list == _view->list) {
        return;
    }
    if(_view->list != 0u && fude_vocab_list_name(_view->list)[0] == 0) {
        _view->list = 0u;   // the list went: every word
    }
    _view->_listed_at   = fude_vocab_revision();
    _view->_listed_list = _view->list;
    rde_arr_clear(&_view->ids);
    if(_view->list == 0u) {
        for(u32 _i = fude_vocab_count(); _i-- > 0;) {
            const u32 _id = fude_vocab_at(_i)->id;
            rde_arr_add(&_view->ids, (any)&_id);
        }
    } else {
        const u32 _n = fude_vocab_list_words(_view->list, NULL, 0u);
        u32*      _w = (u32*)rde_arr_add_n(&_view->ids, _n);
        fude_vocab_list_words(_view->list, _w, _n);
        for(u32 _a = 0, _b = _n > 0 ? _n - 1u : 0u; _a < _b; _a++, _b--) {   // newest first
            const u32 _t = _w[_a];
            _w[_a]       = _w[_b];
            _w[_b]       = _t;
        }
    }
    if(_view->query[0] != 0) {
        u32*      _ids  = (u32*)_view->ids.memory;
        const u32 _n    = (u32)rde_arr_length(&_view->ids);
        u32       _kept = 0;
        for(u32 _i = 0; _i < _n; _i++) {
            const fude_vocab_word* _w = fude_vocab_get(_ids[_i]);
            if(_w != NULL && fude_vocabview_finds(_view, _w)) {
                _ids[_kept++] = _ids[_i];
            }
        }
        while(rde_arr_length(&_view->ids) > _kept) {
            rde_arr_remove(&_view->ids, rde_arr_length(&_view->ids) - 1u);   // the last: nothing moves
        }
    }
}

u32 fude_vocabview_words(fude_vocabview* _view, const u32** _ids) {
    fude_vocabview_list(_view);
    *_ids = (const u32*)_view->ids.memory;
    return (u32)rde_arr_length(&_view->ids);
}

RDE_INTERNAL b8 fude_vocabview_is_kanji(u32 _cp) {
    return (_cp >= 0x4E00u && _cp <= 0x9FFFu) || (_cp >= 0x3400u && _cp <= 0x4DBFu) || _cp == 0x3005u;
}

u32 fude_vocabview_records(fude_vocabview* _view, u32* _out, u32 _max) {
    const u32* _ids;
    const u32  _n = fude_vocabview_words(_view, &_ids);
    u32        _r = 0;
    for(u32 _i = _n; _i-- > 0 && _r < _max;) {   // oldest first
        const fude_vocab_word* _w = fude_vocab_get(_ids[_i]);
        if(_w == NULL || _view->db == NULL) {
            continue;
        }
        // Its kanji; a word with none (すし, テレビ), its kana.
        b8 _kanji = false;
        for(const c8* _p = _w->written; *_p != 0;) {
            _kanji = _kanji || fude_vocabview_is_kanji(fude_utf8_next(&_p));
        }
        for(const c8* _p = _w->written; *_p != 0 && _r < _max;) {
            const u32 _cp = fude_utf8_next(&_p);
            u32       _record;
            if((_kanji && !fude_vocabview_is_kanji(_cp)) || _cp == 0x3005u || !fude_kanji_find_index(_view->db, _cp, &_record)) {
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

u32 fude_vocabview_due(fude_vocabview* _view, u32* _out, u32 _max) {
    const u32* _ids;
    const u32  _n = fude_vocabview_words(_view, &_ids);
    if(_n == 0u) {
        return 0u;
    }
    // Oldest first: the new ones the day has room for are the longest saved.
    u32* _keys = (u32*)rde_malloc(sizeof(u32) * _n);
    for(u32 _i = 0; _i < _n; _i++) {
        _keys[_i] = FUDE_VOCAB_KEY(_ids[_n - 1u - _i]);
    }
    u32* _due = (u32*)rde_malloc(sizeof(u32) * (_max > 0 ? _max : 1u));
    const u32 _d = fude_reviews_due(_keys, _n, _due, _max);
    for(u32 _i = 0; _i < _d; _i++) {
        _out[_i] = _due[_i] & ~FUDE_VOCAB_KEY_BIT;
    }
    rde_free(_due);
    rde_free(_keys);
    return _d;
}

// --- the pointer -----------------------------------------------------------------------

RDE_INTERNAL b8 fude_vocabview_inside(rde_vec_2F _p, rde_vec_2F _min, rde_vec_2F _max) {
    return _p.x >= _min.x && _p.x <= _max.x && _p.y >= _min.y && _p.y <= _max.y && _max.x > _min.x;
}

RDE_INTERNAL i32 fude_vocabview_row_at(const fude_vocabview* _view, rde_vec_2F _p) {
    if(!fude_vocabview_inside(_p, _view->rows_min, _view->rows_max)) {
        return -1;
    }
    const i32 _i = (i32)floorf((_view->rows_max.y - _p.y + _view->scroller.offset) / FUDE_VOCABVIEW_ROW);
    return _i >= 0 && _i < (i32)rde_arr_length(&_view->ids) ? _i : -1;
}

void fude_vocabview_pointer_down(fude_vocabview* _view, rde_vec_2F _screen, f64 _time) {
    _view->in_rows = fude_vocabview_inside(_screen, _view->rows_min, _view->rows_max);
    _view->pressed = _view->in_rows ? fude_vocabview_row_at(_view, _screen) : -1;
    fude_scroller_down(&_view->scroller, _screen, _time);
}

void fude_vocabview_pointer_moved(fude_vocabview* _view, rde_vec_2F _screen, f64 _time) {
    fude_scroller_moved(&_view->scroller, _screen, _time);
    if(_view->scroller.dragging) {
        _view->pressed = -1;
    }
}

void fude_vocabview_pointer_up(fude_vocabview* _view, f64 _time) {
    fude_scroller_up(&_view->scroller, _time);
    _view->pressed = -1;
}

RDE_INTERNAL void fude_vocabview_tap(fude_vocabview* _view, rde_vec_2F _at) {
    for(u32 _c = 0; _c < _view->chip_count; _c++) {
        const fude_vocabview_chip* _chip = &_view->chips[_c];
        if(!fude_vocabview_inside(_at, _chip->min, _chip->max)) {
            continue;
        }
        if(_chip->list == UINT32_MAX) {
            fude_wordcard_ask_list(0u);   // a new one, named
        } else if(_chip->list != 0u && _chip->list == _view->list) {
            fude_wordcard_ask_list(_chip->list);   // the one shown: renamed or deleted
        } else {
            fude_vocabview_show_list(_view, _chip->list);
        }
        return;
    }
    const i32 _row = fude_vocabview_row_at(_view, _at);
    if(_row < 0) {
        return;
    }
    const u32              _id = ((const u32*)_view->ids.memory)[_row];
    const fude_vocab_word* _w  = fude_vocab_get(_id);
    if(_w == NULL) {
        return;
    }
    if(fude_speech_available() && _at.x >= _view->reading_x0 && _at.x < _view->reading_x1) {
        fude_speak(_w->reading[0] != 0 ? _w->reading : _w->written);   // its reading: aloud
        return;
    }
    fude_wordcard_ask_saved(_id);
}

void fude_vocabview_update(fude_vocabview* _view, f32 _dt) {
    if(!_view->open) {
        return;
    }
    fude_vocabview_list(_view);
    const u32 _made = fude_wordcard_take_new_list();
    if(_made != 0u) {
        fude_vocabview_show_list(_view, _made);   // a list just made: shown
    }
    fude_scroller_update(&_view->scroller, _dt, _view->content_h, _view->rows_max.y - _view->rows_min.y);
    rde_vec_2F _at;
    if(fude_scroller_take_tap(&_view->scroller, &_at)) {
        fude_vocabview_tap(_view, _at);
    }
}

// --- drawing ---------------------------------------------------------------------------

// _text in one line no wider than _width: as it is, or a little smaller, or (a
// list "a; b; c") its first items. Into _out; the size to draw it at.
RDE_INTERNAL f32 fude_vocabview_fit(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width, c8* _out, usize _size) {
    snprintf(_out, _size, "%s", _text);
    for(;;) {
        if(fude_draw_text_width(_font, _font_px, _out, _px) <= _width) {
            return _px;
        }
        c8* _cut = NULL;
        for(c8* _c = strstr(_out, "; "); _c != NULL; _c = strstr(_c + 1, "; ")) {
            _cut = _c;
        }
        if(_cut == NULL) {
            return fude_draw_text_px_to_fit(_font, _font_px, _out, _px, _width, 0.6f);
        }
        *_cut = 0;
    }
}

void fude_vocabview_render(fude_vocabview* _view, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_view->open) {
        return;
    }
    fude_vocabview_list(_view);
    const fude_theme* _theme  = fude_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _insets = rde_window_get_safe_area_insets(_window);
    const f32         _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + 24.0f;
    const f32         _right  = (f32)_size.x * 0.5f - (f32)_insets.z - 24.0f;
    c8                _line[256];

    // The header: 語, "Vocabulary", how many words and lists.
    {
        c8 _words[64], _lists[64];
        FUDE_TEXTF(_words, FUDE_TEXT_VOCAB_WORDS_N, FUDE_TN(fude_vocab_count()));
        FUDE_TEXTF(_lists, FUDE_TEXT_VOCAB_LISTS_N, FUDE_TN(fude_vocab_list_count()));
        snprintf(_line, sizeof(_line), "%s \xC2\xB7 %s", _words, _lists);   // ·
        fude_header_draw(&_view->glyph, fude_lang_badge(FUDE_LANG_BADGE_WORDS), _font, _font_px, _left, _right, _top, fude_text(FUDE_TEXT_VOCAB), _line, NULL);   // 語
    }

    // The lists: All, each list, + New list; the one shown in the accent.
    f32 _y = _top - 60.0f;
    {
        _view->chip_count = 0;
        f32 _x = _left;
        for(u32 _c = 0; _c < fude_vocab_list_count() + 2u; _c++) {
            u32 _list;
            if(_c == 0u) {
                _list = 0u;
                c8 _n[32];
                snprintf(_n, sizeof(_n), "%u", fude_vocab_count());
                snprintf(_line, sizeof(_line), "%s  %s", fude_text(FUDE_TEXT_ALL), _n);
            } else if(_c <= fude_vocab_list_count()) {
                _list = fude_vocab_list_at(_c - 1u);
                snprintf(_line, sizeof(_line), "%s  %u", fude_vocab_list_name(_list), fude_vocab_list_words(_list, NULL, 0u));
            } else {
                _list = UINT32_MAX;
                fude_draw_icon_label(_line, sizeof(_line), FUDE_ICON_PLUS, fude_text(FUDE_TEXT_WORD_NEW_LIST));
            }
            const b8  _shown = _list == _view->list;
            const f32 _w     = fminf(_right - _left, fude_draw_text_width(_font, _font_px, _line, FUDE_VOCABVIEW_CHIP_PX) + (_shown && _list != 0u ? 52.0f : 32.0f));
            if(_x + _w > _right && _x > _left) {
                _x  = _left;
                _y -= FUDE_VOCABVIEW_CHIP_H + FUDE_VOCABVIEW_CHIP_GAP;
            }
            const rde_vec_2F _min = { _x, _y - FUDE_VOCABVIEW_CHIP_H };
            const rde_vec_2F _max = { _x + _w, _y };
            rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ fude_draw_x(_x + _w * 0.5f), _y - FUDE_VOCABVIEW_CHIP_H * 0.5f }, (rde_vec_2F){ _w, FUDE_VOCABVIEW_CHIP_H }, 1.0f, 10,
                                                    _shown ? _theme->accent : _theme->surface_2, NULL);
            const rde_color _ink = _shown ? _theme->on_accent : _list == UINT32_MAX ? _theme->accent : _theme->text;
            const f32 _px = fude_draw_text_px_to_fit(_font, _font_px, _line, FUDE_VOCABVIEW_CHIP_PX, _w - 28.0f, 0.6f);
            fude_draw_text(_font, _font_px, _line, _x + 16.0f, _y - FUDE_VOCABVIEW_CHIP_H * 0.5f - _px * 0.42f, _px, _ink);
            if(_shown && _list != 0u) {
                fude_draw_icon(_font, _font_px, FUDE_ICON_NOTE_EDIT, (rde_vec_2F){ _x + _w - 20.0f, _y - FUDE_VOCABVIEW_CHIP_H * 0.5f }, 14.0f, _ink);   // tap again: rename
            }
            _view->chips[_view->chip_count++] = (fude_vocabview_chip){ _min, _max, _list };
            _x += _w + FUDE_VOCABVIEW_CHIP_GAP;
        }
        _y -= FUDE_VOCABVIEW_CHIP_H + 14.0f;
    }

    // The words.
    const u32  _n   = (u32)rde_arr_length(&_view->ids);
    const u32* _ids = (const u32*)_view->ids.memory;
    _view->rows_min  = (rde_vec_2F){ _left, _bottom };
    _view->rows_max  = (rde_vec_2F){ _right, _y };
    _view->content_h = (f32)_n * FUDE_VOCABVIEW_ROW;
    if(_n == 0u) {
        if(_view->query[0] != 0) {
            FUDE_TEXTF(_line, FUDE_TEXT_VOCAB_SEARCH_NONE, FUDE_TS(_view->query));
        } else {
            snprintf(_line, sizeof(_line), "%s", fude_text(_view->list != 0u ? FUDE_TEXT_VOCAB_LIST_EMPTY : FUDE_TEXT_VOCAB_EMPTY));
        }
        const u32 _lines = fude_draw_text_wrap_lines(_font, _font_px, _line, 14.0f, _right - _left - 36.0f);   // as tall as they are (a phone's are more)
        fude_draw_card((rde_vec_2F){ _left, _y - fmaxf(84.0f, (f32)_lines * 20.0f + 34.0f) }, (rde_vec_2F){ _right, _y }, 14.0f, _theme->surface, _theme->outline);
        fude_draw_text_wrap(_font, _font_px, _line, _left + 18.0f, _y - 30.0f, 14.0f, _right - _left - 36.0f, 20.0f, _theme->text_soft);
        return;
    }
    // Columns: the written form, the reading (after its speaker), the meaning; the
    // review's state and › at the right.
    const b8  _speak     = fude_speech_available();
    const f32 _inner     = _right - _left;
    const f32 _written_w = fminf(220.0f, _inner * 0.26f);
    const f32 _reading_x = _left + 14.0f + _written_w + (_speak ? 30.0f : 12.0f);
    const f32 _reading_w = fminf(200.0f, _inner * 0.22f);
    const f32 _meaning_x = _reading_x + _reading_w + 14.0f;
    const f32 _state_w   = 86.0f;
    const f32 _meaning_w = _right - 30.0f - _state_w - _meaning_x;
    _view->reading_x0 = _speak ? _reading_x - 28.0f : 0.0f;
    _view->reading_x1 = _speak ? _meaning_x - 6.0f : 0.0f;
    const u32 _today  = fude_reviews_today();

    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)fude_draw_x((_left + _right) * 0.5f), (i32)((_y + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left), (u32)fmaxf(1.0f, _y - _bottom) });
    const u32 _first = (u32)fmaxf(0.0f, floorf(_view->scroller.offset / FUDE_VOCABVIEW_ROW));
    for(u32 _i = _first; _i < _n; _i++) {
        const f32 _row_top = _y - (f32)_i * FUDE_VOCABVIEW_ROW + _view->scroller.offset;
        if(_row_top < _bottom) {
            break;
        }
        const fude_vocab_word* _w = fude_vocab_get(_ids[_i]);
        if(_w == NULL) {
            continue;
        }
        const f32 _mid = _row_top - FUDE_VOCABVIEW_ROW * 0.5f;
        if(_view->pressed == (i32)_i) {
            rde_rendering_2d_draw_rectangle((rde_vec_2F){ fude_draw_x((_left + _right) * 0.5f), _mid }, (rde_vec_2F){ _inner, FUDE_VOCABVIEW_ROW }, _theme->select_fill);
        }
        c8  _fit[FUDE_USERWORD_MEANING];
        f32 _px = fude_draw_text_px_to_fit(_font, _font_px, _w->written, FUDE_VOCABVIEW_WORD_PX, _written_w, 0.5f);
        fude_draw_text(_font, _font_px, _w->written, _left + 14.0f, _mid - _px * 0.38f, _px, _theme->ink);
        if(_speak) {
            fude_draw_icon(_font, _font_px, FUDE_ICON_SPEAK, (rde_vec_2F){ _reading_x - 14.0f, _mid }, 14.0f, _theme->accent);
        }
        _px = fude_draw_text_px_to_fit(_font, _font_px, _w->reading, FUDE_VOCABVIEW_SMALL_PX, _reading_w, 0.6f);
        fude_draw_text(_font, _font_px, _w->reading, _reading_x, _mid - _px * 0.36f, _px, _theme->text_soft);
        _px = fude_vocabview_fit(_font, _font_px, _w->meaning, FUDE_VOCABVIEW_SMALL_PX, _meaning_w, _fit, sizeof(_fit));
        fude_draw_text(_font, _font_px, _fit, _meaning_x, _mid - _px * 0.36f, _px, _theme->text);
        // Its reviews: new, due, or when.
        const u32 _due = fude_reviews_due_day(FUDE_VOCAB_KEY(_w->id));
        rde_color _c   = _theme->text_soft;
        if(_due == 0u) {
            snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_REVIEW_NEW));
        } else if(_due <= _today) {
            snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_REVIEW_DUE));
            _c = _theme->accent;
        } else {
            FUDE_TEXTF(_line, FUDE_TEXT_REVIEW_IN_N, FUDE_TN(_due - _today));
        }
        _px = fude_draw_text_px_to_fit(_font, _font_px, _line, 11.0f, _state_w, 0.7f);
        fude_draw_text(_font, _font_px, _line, _right - 28.0f - fude_draw_text_width(_font, _font_px, _line, _px), _mid - _px * 0.36f, _px, _c);
        fude_draw_icon(_font, _font_px, FUDE_ICON_NEXT, (rde_vec_2F){ _right - 12.0f, _mid }, 16.0f, _theme->text_soft);
        if(_i + 1u < _n) {
            fude_draw_line((rde_vec_2F){ _left, _row_top - FUDE_VOCABVIEW_ROW }, (rde_vec_2F){ _right, _row_top - FUDE_VOCABVIEW_ROW }, 0.5f, _theme->outline);
        }
    }
    rde_rendering_end_clipping_rect();
}

// --- the screen (screen.h): its row, and what it does --------------------------------------

#include "study/app/study.h"
#include "drawing/widgets/notice.h"
#include "study/models/vocab.h"
#include "study/models/sheet.h"

FUDE_SCREEN_ADAPTERS(fude_vocabview, fude_vocabview)
FUDE_SCREEN_RENDER(fude_vocabview, fude_vocabview)

RDE_INTERNAL void fude_vocabview_screen_update(fude_app* _app, void* _self, f32 _dt) {
    RDE_UNUSED(_app);
    fude_vocabview_update((fude_vocabview*)_self, _dt);
}

// The field: every keystroke searches.
RDE_INTERNAL void fude_vocabview_screen_search(void* _self, const c8* _text) {
    fude_vocabview_search((fude_vocabview*)_self, _text);
}

// What the screen shows is called: the list's name, or Vocabulary.
RDE_INTERNAL const c8* fude_vocabview_title(const fude_vocabview* _view) {
    return _view->list != 0u ? fude_vocab_list_name(_view->list) : fude_text(FUDE_TEXT_VOCAB);
}

FUDE_ROW_CALL(fude_vocabview_row_back, fude_vocabview, fude_vocabview_close)

// + Word: typed in (the list shown, ticked).
RDE_INTERNAL void fude_vocabview_row_word(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_wordcard_prefer_list(((const fude_vocabview*)_self)->list);
    fude_wordcard_ask_typed(0u);
}

// Translate with Google: into Japanese (translator.h), from the reader's language.
RDE_INTERNAL void fude_vocabview_row_translate(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_translator_open(FUDE_STUDY(_app)->translator);
}

// Review: the words due today, as a word exam.
RDE_INTERNAL void fude_vocabview_row_review(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_arg);
    u32       _due[FUDE_REVIEW_SESSION];
    const u32 _n = fude_vocabview_due((fude_vocabview*)_self, _due, FUDE_REVIEW_SESSION);
    if(_n == 0u) {
        fude_notice_show(fude_text(FUDE_TEXT_REVIEWS_NONE));
    } else {
        fude_wordexam_open_review(FUDE_STUDY(_app)->wordexam, _due, _n);
    }
}

RDE_INTERNAL void fude_vocabview_row_exam(fude_app* _app, void* _self, u32 _arg) {
    fude_vocabview* _view = (fude_vocabview*)_self;
    RDE_UNUSED(_arg);
    const u32* _ids;
    const u32  _n = fude_vocabview_words(_view, &_ids);
    fude_wordexam_open(FUDE_STUDY(_app)->wordexam, _ids, _n, fude_vocabview_title(_view));
}

// Sheet and Practice: the words' characters (one more than a sheet takes: too many is told).
RDE_INTERNAL void fude_vocabview_row_sheet(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_arg);
    u32       _records[FUDE_SHEET_MAX + 1u];
    const u32 _n = fude_vocabview_records((fude_vocabview*)_self, _records, FUDE_SHEET_MAX + 1u);
    fude_study_sheet(_app, _records, _n);
}

RDE_INTERNAL void fude_vocabview_row_practice(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_arg);
    u32       _records[FUDE_SHEET_MAX + 1u];
    const u32 _n = fude_vocabview_records((fude_vocabview*)_self, _records, FUDE_SHEET_MAX + 1u);
    fude_study_practice_set(_app, _records, _n);
}

enum { FUDE_VOCAB_ROW_BACK = 0, FUDE_VOCAB_ROW_WORD, FUDE_VOCAB_ROW_TRANSLATE, FUDE_VOCAB_ROW_REVIEW, FUDE_VOCAB_ROW_EXAM, FUDE_VOCAB_ROW_SHEET, FUDE_VOCAB_ROW_PRACTICE };
static const fude_row_button FUDE_VOCAB_BUTTONS[] = {
    { FUDE_TEXT_BACK,           FUDE_ICON_BACK,  fude_vocabview_row_back,     0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_VOCAB_ADD_WORD, FUDE_ICON_PLUS,  fude_vocabview_row_word,     0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_SCAN_TRANSLATE, FUDE_ICON_TRANSLATE, fude_vocabview_row_translate, 0, FUDE_ROW_QUIET, false, fude_translate_available },
    { FUDE_TEXT_VOCAB_REVIEW_N, FUDE_ICON_RETRY, fude_vocabview_row_review,   0, FUDE_ROW_PRIMARY, true,  NULL },
    { FUDE_TEXT_EXAM,           FUDE_ICON_EXAM,  fude_vocabview_row_exam,     0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_SHEET,          FUDE_ICON_PRINT, fude_vocabview_row_sheet,    0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_PRACTICE,       FUDE_ICON_PEN,   fude_vocabview_row_practice, 0, FUDE_ROW_QUIET,   false, NULL },
};
static const fude_row_def FUDE_VOCAB_BUTTON_ROWS[] = { FUDE_ROW_DEF(FUDE_VOCAB_BUTTONS) };

RDE_INTERNAL u32 fude_vocabview_screen_row(const void* _self) {
    RDE_UNUSED(_self);
    return 0u;
}

// Review n: the words due today (counted again only when the words, the reviews,
// the day or the list shown change); the rest need words.
RDE_INTERNAL void fude_vocabview_screen_faces(const void* _self, u32 _row, fude_row_face* _faces) {
    fude_vocabview* _view = (fude_vocabview*)_self;
    RDE_UNUSED(_row);
    static u32 _for = UINT32_MAX, _due_n = 0;
    const u32  _now = fude_vocab_revision() * 131u + fude_reviews_revision() * 17u + fude_reviews_today() * 7u + _view->list;
    if(_now != _for) {
        u32 _due[FUDE_REVIEW_SESSION];
        _for   = _now;
        _due_n = fude_vocabview_due(_view, _due, FUDE_REVIEW_SESSION);
    }
    const u32* _ids;
    const b8   _words = fude_vocabview_words(_view, &_ids) > 0u;
    fude_row_face_count(&_faces[FUDE_VOCAB_ROW_REVIEW], FUDE_TEXT_VOCAB_REVIEW_N, _due_n);
    _faces[FUDE_VOCAB_ROW_REVIEW].disabled   = _due_n == 0u;
    _faces[FUDE_VOCAB_ROW_EXAM].disabled     = !_words;
    _faces[FUDE_VOCAB_ROW_SHEET].disabled    = !_words;
    _faces[FUDE_VOCAB_ROW_PRACTICE].disabled = !_words;
}

const fude_screen FUDE_VOCAB_SCREEN = {
    .name = "vocabulary", .input = FUDE_SCREEN_INPUT_POINT,
    .is_open = fude_vocabview_screen_is_open, .close = fude_vocabview_screen_close,
    .update = fude_vocabview_screen_update, .render = fude_vocabview_screen_render,
    .pointer_down = fude_vocabview_screen_down, .pointer_moved = fude_vocabview_screen_moved, .pointer_up = fude_vocabview_screen_up,
    .rows = FUDE_VOCAB_BUTTON_ROWS, .row_count = 1u, .row = fude_vocabview_screen_row, .faces = fude_vocabview_screen_faces,
    .field_hint = FUDE_TEXT_VOCAB_SEARCH_HINT, .field_max = 40u, .field_change = fude_vocabview_screen_search,
};
