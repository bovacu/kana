// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/screens/vocabview.h"
#include "study/widgets/header.h"
#include "drawing/base/draw.h"
#include "drawing/base/icons.h"
#include "study/models/review.h"
#include "study/services/speech.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "study/widgets/wordcard.h"
#include "drawing/base/utf8.h"
#include "lang/lang.h"

// ===========================================================================
// See vocabview.h.
//
// The screen is drawn by hand like the other screens. Only the rows in view are drawn, so a long vocabulary costs
// nothing more to show, and the ids shown are kept until the vocabulary, the list shown or the search changes. A search
// compares folded text (fude_vocabview_fold), and a search typed in Latin letters is also read as the language's
// script (romaji as kana in Japanese).
// ===========================================================================

// The layout's sizes, in screen points (a list's chip, the gap between chips, a word's row), and the text sizes.
#define FUDE_VOCABVIEW_CHIP_H     36.0f
#define FUDE_VOCABVIEW_CHIP_GAP   8.0f
#define FUDE_VOCABVIEW_CHIP_PX    13.0f
#define FUDE_VOCABVIEW_CHIP_ROWS  3u       // The most rows of chips shown: more lists scroll inside them.
#define FUDE_VOCABVIEW_ROW        56.0f
#define FUDE_VOCABVIEW_WORD_PX    20.0f
#define FUDE_VOCABVIEW_SMALL_PX   13.0f

// Sets the screen up closed, on every word, with the words to be listed and the Review count to be worked out.
void fude_vocabview_init(fude_vocabview* _view, const fude_kanji_db* _db) {
    rde_memset(_view, 0, sizeof(*_view));
    fude_kit_scroller_init(&_view->scroller);
    fude_kit_scroller_init(&_view->chips_scroller);
    _view->db         = _db;
    _view->ids        = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    _view->chips      = rde_arr_new(sizeof(fude_vocabview_chip), rde_memory_allocator_get_default_std());
    _view->_listed_at = UINT32_MAX;
    _view->pressed    = -1;
    for(u32 _i = 0; _i < 3u; _i++) {
        _view->_due_for[_i] = UINT32_MAX;
    }
    fude_glyph_init(&_view->glyph, _db);
}

// Frees the ids, the chips and the glyph cache, and zeroes the screen.
void fude_vocabview_destroy(fude_vocabview* _view) {
    if(rde_arr_is_inited(&_view->ids)) {
        rde_arr_free(&_view->ids);
    }
    if(rde_arr_is_inited(&_view->chips)) {
        rde_arr_free(&_view->chips);
    }
    fude_glyph_destroy(&_view->glyph);
    rde_memset(_view, 0, sizeof(*_view));
}

// Opens on the list last shown, with no search, scrolled to the top; the words are listed again.
void fude_vocabview_open(fude_vocabview* _view) {
    _view->open       = true;
    _view->_listed_at = UINT32_MAX;
    fude_vocabview_search(_view, "");   // Its field starts empty.
    _view->pressed    = -1;
    rde_ui_scroller_stop(&_view->scroller);
    _view->scroller.offset.y = 0.0f;
    rde_ui_scroller_stop(&_view->chips_scroller);
    _view->in_chips     = false;
    _view->chips_reveal = true;   // The list shown's chip is in view.
}

// Closes the screen.
void fude_vocabview_close(fude_vocabview* _view) {
    _view->open = false;
}

// Shows list _list from its top, and makes it the list the word card saves new words into.
void fude_vocabview_show_list(fude_vocabview* _view, u32 _list) {
    _view->list         = _list;
    _view->_listed_at   = UINT32_MAX;
    _view->chips_reveal = true;   // Its chip is scrolled into view (a list just made may be past the rows shown).
    rde_ui_scroller_stop(&_view->scroller);
    _view->scroller.offset.y = 0.0f;
    fude_wordcard_prefer_list(_list);   // + Word saves into it.
}

// --- The search ------------------------------------------------------------------------------------------------------

// Writes _text into _out (_size bytes) the way a search compares it: A-Z as a-z, and the characters of readings as the
// language compares them (lang.h; in Japanese, エキ finds えき). The result is never longer than _text.
RDE_INTERNAL void fude_vocabview_fold(const c8* _text, c8* _out, usize _size) {
    usize _n = 0;
    for(const c8* _p = _text; *_p != 0;) {
        u32 _cp = fude_utf8_next(&_p);
        if(_cp >= 'A' && _cp <= 'Z') {
            _cp += 'a' - 'A';
        } else {
            _cp = fude_lang_reading_fold(_cp);
            if(_cp == 0u) {
                continue;   // The language leaves this one out (lang.h).
            }
        }
        c8 _one[5];
        fude_utf8_put(_cp, _one);
        const usize _len = rde_strlen(_one);
        if(_n + _len >= _size) {
            break;
        }
        rde_memcpy(_out + _n, _one, _len);
        _n += _len;
    }
    _out[_n] = 0;
}

// Sets the search to _query without the spaces around it, cut at a whole character should it be longer than the field
// takes. When it changed, it is folded, read as the language's script when it is all Latin letters, and the words are
// listed again from the top.
void fude_vocabview_search(fude_vocabview* _view, const c8* _query) {
    // The spaces around it are dropped.
    while(*_query == ' ') {
        _query++;
    }
    usize _len = rde_strlen(_query);
    while(_len > 0 && _query[_len - 1u] == ' ') {
        _len--;
    }
    c8 _was[sizeof(_view->query)];
    rde_memcpy(_was, _view->query, sizeof(_was));
    fude_utf8_copy(_view->query, sizeof(_view->query), _query, _len);
    if(rde_strcmp(_was, _view->query) == 0) {
        return;
    }
    fude_vocabview_fold(_view->query, _view->_query_fold, sizeof(_view->_query_fold));
    // A search of Latin letters only (romaji, in Japanese) is also read in the language's script, so eki finds えき.
    b8 _letters = _view->query[0] != 0;
    for(const c8* _c = _view->query; *_c != 0 && _letters; _c++) {
        _letters = (*_c >= 'a' && *_c <= 'z') || (*_c >= 'A' && *_c <= 'Z') || *_c == '-' || *_c == '\'';
    }
    if(!_letters || !fude_lang_reading_from_latin(_view->_query_fold, _view->_query_kana, sizeof(_view->_query_kana))) {
        _view->_query_kana[0] = 0;
    }
    _view->_listed_at = UINT32_MAX;
    rde_ui_scroller_stop(&_view->scroller);
    _view->scroller.offset.y = 0.0f;
}

// Returns whether word _w has what is searched for: in its written form, its reading or its meaning (folded), or, for
// a Latin search, its script reading in the written form or the reading.
RDE_INTERNAL b8 fude_vocabview_finds(const fude_vocabview* _view, const fude_vocab_word* _w) {
    c8 _folded[FUDE_USERWORD_MEANING];
    const c8* const _parts[] = { _w->written, _w->reading, _w->meaning };
    for(u32 _i = 0; _i < 3u; _i++) {
        fude_vocabview_fold(_parts[_i], _folded, sizeof(_folded));
        if(rde_strstr(_folded, _view->_query_fold) != NULL || (_i < 2u && _view->_query_kana[0] != 0 && rde_strstr(_folded, _view->_query_kana) != NULL)) {
            return true;
        }
    }
    return false;
}

// --- The words -------------------------------------------------------------------------------------------------------

// Lists the words shown, newest first: those of the list shown that the search finds. It is done again only when the
// vocabulary or the list shown changed (a new search resets _listed_at). A list that was deleted falls back to every
// word.
RDE_INTERNAL void fude_vocabview_list(fude_vocabview* _view) {
    if(_view->_listed_at == fude_vocab_revision() && _view->_listed_list == _view->list) {
        return;
    }
    if(_view->list != 0u && fude_vocab_list_name(_view->list)[0] == 0) {
        _view->list = 0u;   // The list is gone, so every word is shown.
    }
    _view->_listed_at   = fude_vocab_revision();
    _view->_listed_list = _view->list;
    _view->_listings++;
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
        for(u32 _a = 0, _b = _n > 0 ? _n - 1u : 0u; _a < _b; _a++, _b--) {   // Reversed, so the newest come first.
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
        rde_arr_resize(&_view->ids, _kept);
    }
}

// See the header: lists the words again first, if they need it.
u32 fude_vocabview_words(fude_vocabview* _view, const u32** _ids) {
    fude_vocabview_list(_view);
    *_ids = (const u32*)_view->ids.memory;
    return (u32)rde_arr_length(&_view->ids);
}

// Returns whether _cp counts as a kanji in a word: a han character, or 々 (U+3005, the repeat mark).
RDE_INTERNAL b8 fude_vocabview_is_kanji(u32 _cp) {
    return fude_utf8_is_han(_cp) || _cp == 0x3005u;   // And 々, the repeat mark.
}

// See the header. 々 itself is never taken, since it is not a character to write on its own.
u32 fude_vocabview_records(fude_vocabview* _view, u32* _out, u32 _max) {
    const u32* _ids;
    const u32  _n = fude_vocabview_words(_view, &_ids);
    u32        _r = 0;
    for(u32 _i = _n; _i-- > 0 && _r < _max;) {   // The oldest words first.
        const fude_vocab_word* _w = fude_vocab_get(_ids[_i]);
        if(_w == NULL || _view->db == NULL) {
            continue;
        }
        // A word's kanji; a word with none (すし, テレビ) gives its kana.
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

// See the header. The words go to review.h by their review keys (FUDE_VOCAB_KEY), and come back as ids.
u32 fude_vocabview_due(fude_vocabview* _view, u32* _out, u32 _max) {
    const u32* _ids;
    const u32  _n = fude_vocabview_words(_view, &_ids);
    if(_n == 0u) {
        return 0u;
    }
    // Oldest first, so the new ones the day has room for are the longest saved.
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

// --- The pointer -----------------------------------------------------------------------------------------------------

// Returns whether point _p is inside the box from _min to _max; a box with no width (none) has nothing in it.
RDE_INTERNAL b8 fude_vocabview_inside(rde_vec_2F _p, rde_vec_2F _min, rde_vec_2F _max) {
    return _p.x >= _min.x && _p.x <= _max.x && _p.y >= _min.y && _p.y <= _max.y && _max.x > _min.x;
}

// Returns the word's row under point _p, as laid out last frame and scrolled, or -1.
RDE_INTERNAL i32 fude_vocabview_row_at(const fude_vocabview* _view, rde_vec_2F _p) {
    if(!fude_vocabview_inside(_p, _view->rows_min, _view->rows_max)) {
        return -1;
    }
    const i32 _i = (i32)rde_math_floor_f32((_view->rows_max.y - _p.y + _view->scroller.offset.y) / FUDE_VOCABVIEW_ROW);
    return _i >= 0 && _i < (i32)rde_arr_length(&_view->ids) ? _i : -1;
}

// Starts a press: the row under it shows pressed while it is still a tap. A press in the chips' area, while it
// scrolls, is the area's: it scrolls the chips, not the words.
void fude_vocabview_pointer_down(fude_vocabview* _view, rde_vec_2F _screen, f64 _time) {
    _view->in_chips = _view->chips_scroll && fude_vocabview_inside(_screen, _view->chips_min, _view->chips_max);
    if(_view->in_chips) {
        _view->in_rows = false;
        _view->pressed = -1;
        rde_ui_scroller_pointer_down(&_view->chips_scroller, _screen, _time);
        return;
    }
    _view->in_rows = fude_vocabview_inside(_screen, _view->rows_min, _view->rows_max);
    _view->pressed = _view->in_rows ? fude_vocabview_row_at(_view, _screen) : -1;
    rde_ui_scroller_pointer_down(&_view->scroller, _screen, _time);
}

// Drags; once it is a drag, no row shows pressed.
void fude_vocabview_pointer_moved(fude_vocabview* _view, rde_vec_2F _screen, f64 _time) {
    if(_view->in_chips) {
        rde_ui_scroller_pointer_moved(&_view->chips_scroller, _screen, _time);
        return;
    }
    rde_ui_scroller_pointer_moved(&_view->scroller, _screen, _time);
    if(_view->scroller.dragging) {
        _view->pressed = -1;
    }
}

// Ends the press; a tap is taken in fude_vocabview_update.
void fude_vocabview_pointer_up(fude_vocabview* _view, f64 _time) {
    if(_view->in_chips) {
        rde_ui_scroller_pointer_up(&_view->chips_scroller, _time);
        _view->in_chips = false;
    } else {
        rde_ui_scroller_pointer_up(&_view->scroller, _time);
    }
    _view->pressed = -1;
}

// Acts on a tap: a list's chip shows that list (or, on the list shown, opens the form to rename or delete it), + New
// list opens the form for a new one, a row's reading is said aloud, and the rest of a row opens the word card.
RDE_INTERNAL void fude_vocabview_tap(fude_vocabview* _view, rde_vec_2F _at) {
    // A chip scrolled out of the chips' area is not there to tap.
    const b8 _in_chips = fude_vocabview_inside(_at, _view->chips_min, _view->chips_max);
    for(u32 _c = 0; _c < (u32)rde_arr_length(&_view->chips) && _in_chips; _c++) {
        const fude_vocabview_chip* _chip = &((const fude_vocabview_chip*)_view->chips.memory)[_c];
        if(!fude_vocabview_inside(_at, _chip->min, _chip->max)) {
            continue;
        }
        if(_chip->list == UINT32_MAX) {
            fude_wordcard_ask_list(0u);   // A new list, to name.
        } else if(_chip->list != 0u && _chip->list == _view->list) {
            fude_wordcard_ask_list(_chip->list);   // The list shown, to rename or delete.
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
        fude_speak(_w->reading[0] != 0 ? _w->reading : _w->written);   // Its reading, aloud.
        return;
    }
    fude_wordcard_ask_saved(_id);
}

// Keeps the words shown up to date, shows a list the word card just made, scrolls, and acts on a tap.
void fude_vocabview_update(fude_vocabview* _view, f32 _dt) {
    if(!_view->open) {
        return;
    }
    fude_vocabview_list(_view);
    const u32 _made = fude_wordcard_take_new_list();
    if(_made != 0u) {
        fude_vocabview_show_list(_view, _made);   // A list just made is shown.
    }
    fude_kit_scroller_update(&_view->scroller, _dt, _view->content_h, _view->rows_max.y - _view->rows_min.y);
    fude_kit_scroller_update(&_view->chips_scroller, _dt, _view->chips_h, _view->chips_max.y - _view->chips_min.y);
    rde_vec_2F _at;
    if(rde_ui_scroller_take_tap(&_view->scroller, &_at)) {
        fude_vocabview_tap(_view, _at);
    }
    if(rde_ui_scroller_take_tap(&_view->chips_scroller, &_at)) {
        fude_vocabview_tap(_view, _at);
    }
}

// --- Drawing ---------------------------------------------------------------------------------------------------------

// Fits _text on one line no wider than _width, into _out (_size bytes): as it is, or with its last items dropped when
// it is a list ("a; b; c"), or smaller. Returns the size to draw it at.
RDE_INTERNAL f32 fude_vocabview_fit(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width, c8* _out, usize _size) {
    rde_snprintf(_out, _size, "%s", _text);
    for(;;) {
        if(fude_draw_text_width(_font, _font_px, _out, _px) <= _width) {
            return _px;
        }
        c8* _cut = NULL;
        for(c8* _c = rde_strstr(_out, "; "); _c != NULL; _c = rde_strstr(_c + 1, "; ")) {
            _cut = _c;
        }
        if(_cut == NULL) {
            return fude_draw_text_px_to_fit(_font, _font_px, _out, _px, _width);
        }
        *_cut = 0;
    }
}

// Writes chip _c's label into _out (_size bytes) and returns the list it shows: chip 0 is All, with how many words
// there are; then each list, with how many words it has; then + New list (UINT32_MAX).
RDE_INTERNAL u32 fude_vocabview_chip_label(u32 _c, c8* _out, usize _size) {
    if(_c == 0u) {
        c8 _n[32];
        rde_snprintf(_n, sizeof(_n), "%u", fude_vocab_count());
        rde_snprintf(_out, _size, "%s  %s", fude_text(FUDE_TEXT_ALL), _n);
        return 0u;
    }
    if(_c <= fude_vocab_list_count()) {
        const u32 _list = fude_vocab_list_at(_c - 1u);
        rde_snprintf(_out, _size, "%s  %u", fude_vocab_list_name(_list), fude_vocab_list_words(_list, NULL, 0u));
        return _list;
    }
    fude_draw_icon_label(_out, _size, FUDE_ICON_PLUS, fude_text(FUDE_TEXT_WORD_NEW_LIST));
    return UINT32_MAX;
}

// Lays the lists' chips out into _view->chips, unscrolled: All, each list, and + New list, however many lists there
// are, from _y (the first row's top) between _left and _right, wrapping onto more rows. Returns how many rows they
// take, and sets *_last_top to the top of the last row that shows (FUDE_VOCABVIEW_CHIP_ROWS at most).
RDE_INTERNAL u32 fude_vocabview_lay_out_chips(fude_vocabview* _view, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _y, f32* _last_top) {
    c8  _line[512];
    u32 _rows = 1u;
    f32 _x    = _left;
    *_last_top = _y;
    rde_arr_clear(&_view->chips);
    for(u32 _c = 0; _c < fude_vocab_list_count() + 2u; _c++) {
        const u32 _list  = fude_vocabview_chip_label(_c, _line, sizeof(_line));
        const b8  _shown = _list == _view->list;
        const f32 _w     = rde_math_min_f32(_right - _left, fude_draw_text_width(_font, _font_px, _line, FUDE_VOCABVIEW_CHIP_PX) + (_shown && _list != 0u ? 52.0f : 32.0f));
        if(_x + _w > _right && _x > _left) {
            _x  = _left;
            _y -= FUDE_VOCABVIEW_CHIP_H + FUDE_VOCABVIEW_CHIP_GAP;
            _rows++;
            if(_rows <= FUDE_VOCABVIEW_CHIP_ROWS) {
                *_last_top = _y;
            }
        }
        const fude_vocabview_chip _chip = { { _x, _y - FUDE_VOCABVIEW_CHIP_H }, { _x + _w, _y }, _list, _w };
        rde_arr_add(&_view->chips, (any)&_chip);
        _x += _w + FUDE_VOCABVIEW_CHIP_GAP;
    }
    return _rows;
}

// While the chips scroll, brings the list shown's chip into the area when the list shown changed (a list just made can
// be past the rows that show). _area_top and _area_h are the area's top and height.
RDE_INTERNAL void fude_vocabview_reveal_chip(fude_vocabview* _view, f32 _area_top, f32 _area_h) {
    for(u32 _c = 0; _c < (u32)rde_arr_length(&_view->chips) && _view->chips_reveal; _c++) {
        const fude_vocabview_chip* _chip = &((const fude_vocabview_chip*)_view->chips.memory)[_c];
        if(_chip->list != _view->list) {
            continue;
        }
        const f32 _from = _area_top - _chip->max.y;   // How far under the area's top it is, unscrolled.
        f32       _at   = _view->chips_scroller.offset.y;
        if(_from < _at) {
            _at = _from;
        } else if(_from + FUDE_VOCABVIEW_CHIP_H > _at + _area_h) {
            _at = _from + FUDE_VOCABVIEW_CHIP_H - _area_h;
        }
        _view->chips_scroller.offset.y   = rde_math_max_f32(0.0f, rde_math_min_f32(_at, _view->chips_h - _area_h));
        _view->chips_scroller.velocity.y = 0.0f;
    }
    _view->chips_reveal = false;
}

// Draws the lists' chips under the header, from _area_top down, and returns where the words start under them. The one
// shown is in the accent, with a pencil to rename it. Past FUDE_VOCABVIEW_CHIP_ROWS rows, that many show, clipped, and
// the area scrolls on its own (fude_vocabview.chips_scroller), so dozens of lists never push the words off the screen.
// Each chip is kept as drawn, and the area, for taps.
RDE_INTERNAL f32 fude_vocabview_draw_chips(fude_vocabview* _view, rde_window* _window, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _area_top) {
    const fude_theme* _theme = fude_theme_active();
    c8                _line[512];
    f32               _last_top;
    const u32         _rows   = fude_vocabview_lay_out_chips(_view, _font, _font_px, _left, _right, _area_top, &_last_top);
    const f32         _bottom = _last_top - FUDE_VOCABVIEW_CHIP_H;   // The area's.
    _view->chips_min    = (rde_vec_2F){ _left, _bottom };
    _view->chips_max    = (rde_vec_2F){ _right, _area_top };
    _view->chips_h      = (f32)_rows * FUDE_VOCABVIEW_CHIP_H + (f32)(_rows - 1u) * FUDE_VOCABVIEW_CHIP_GAP;
    _view->chips_scroll = _rows > FUDE_VOCABVIEW_CHIP_ROWS;
    f32 _offset = 0.0f;
    if(_view->chips_scroll) {
        fude_vocabview_reveal_chip(_view, _area_top, _area_top - _bottom);
        _offset = _view->chips_scroller.offset.y;
        rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)fude_draw_x((_left + _right) * 0.5f), (i32)((_area_top + _bottom) * 0.5f) },
                                          (rde_vec_2UI){ (u32)(_right - _left), (u32)(_area_top - _bottom) });
    } else {
        rde_ui_scroller_stop(&_view->chips_scroller);
        _view->chips_scroller.offset.y = 0.0f;
        _view->chips_reveal          = false;
    }
    for(u32 _c = 0; _c < (u32)rde_arr_length(&_view->chips); _c++) {
        fude_vocabview_chip* _chip = &((fude_vocabview_chip*)_view->chips.memory)[_c];
        const f32            _x    = _chip->min.x;
        const f32            _w    = _chip->w;
        const f32            _y    = _chip->max.y + _offset;   // Its top, scrolled.
        _chip->min.y = _y - FUDE_VOCABVIEW_CHIP_H;
        _chip->max.y = _y;
        if(_y - FUDE_VOCABVIEW_CHIP_H > _area_top || _y < _bottom) {
            continue;   // Scrolled out of the area.
        }
        const u32 _list  = fude_vocabview_chip_label(_c, _line, sizeof(_line));
        const b8  _shown = _list == _view->list;
        rde_rendering_2d_draw_rounded_rectangle((rde_vec_2F){ fude_draw_x(_x + _w * 0.5f), _y - FUDE_VOCABVIEW_CHIP_H * 0.5f }, (rde_vec_2F){ _w, FUDE_VOCABVIEW_CHIP_H }, 1.0f, 10,
                                                _shown ? _theme->accent : _theme->surface_2, NULL);
        const rde_color _ink = _shown ? _theme->on_accent : _list == UINT32_MAX ? _theme->accent : _theme->text;
        const f32 _px = fude_draw_text_px_to_fit(_font, _font_px, _line, FUDE_VOCABVIEW_CHIP_PX, _w - 28.0f);
        fude_draw_text(_font, _font_px, _line, _x + 16.0f, _y - FUDE_VOCABVIEW_CHIP_H * 0.5f - _px * 0.42f, _px, _ink);
        if(_shown && _list != 0u) {
            fude_draw_icon(_font, _font_px, FUDE_ICON_NOTE_EDIT, (rde_vec_2F){ _x + _w - 20.0f, _y - FUDE_VOCABVIEW_CHIP_H * 0.5f }, 14.0f, _ink);   // Tap again to rename.
        }
    }
    if(_view->chips_scroll) {
        rde_rendering_end_clipping_rect();
    }
    return _last_top - (FUDE_VOCABVIEW_CHIP_H + 14.0f);
}

// Draws the header, the lists' chips (wrapping onto more lines) and the rows of the words in view, clipped to their
// area; or a card saying why there are none. It keeps the chips, the rows' area and the readings' column for taps.
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
    c8                _line[512];

    // The header: the language's badge (Japanese's 語), "Vocabulary", and how many words and lists.
    {
        c8 _words[512], _lists[64];
        FUDE_TEXTF(_words, FUDE_TEXT_VOCAB_WORDS_N, FUDE_TN(fude_vocab_count()));
        FUDE_TEXTF(_lists, FUDE_TEXT_VOCAB_LISTS_N, FUDE_TN(fude_vocab_list_count()));
        rde_snprintf(_line, sizeof(_line), "%s \xC2\xB7 %s", _words, _lists);   // A middle dot, ·, in UTF-8.
        fude_header_draw(&_view->glyph, fude_lang_badge(FUDE_LANG_BADGE_WORDS), _font, _font_px, _left, _right, _top, fude_text(FUDE_TEXT_VOCAB), _line, NULL);   // Such as 語.
    }

    // The lists' chips: All, each list, and + New list, however many lists there are (a few rows, then they scroll).
    const f32 _y = fude_vocabview_draw_chips(_view, _window, _font, _font_px, _left, _right, _top - 60.0f);

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
            rde_snprintf(_line, sizeof(_line), "%s", fude_text(_view->list != 0u ? FUDE_TEXT_VOCAB_LIST_EMPTY : FUDE_TEXT_VOCAB_EMPTY));
        }
        // The card is as tall as the text's lines, which are more on a phone.
        const u32 _lines = fude_draw_text_wrap_lines(_font, _font_px, _line, 14.0f, _right - _left - 36.0f);   // Lines.
        fude_draw_card((rde_vec_2F){ _left, _y - rde_math_max_f32(84.0f, (f32)_lines * 20.0f + 34.0f) }, (rde_vec_2F){ _right, _y }, 14.0f, _theme->surface, _theme->outline);
        fude_draw_text_wrap(_font, _font_px, _line, _left + 18.0f, _y - 30.0f, 14.0f, _right - _left - 36.0f, 20.0f, _theme->text_soft);
        return;
    }
    // The columns: the written form, the reading (after its speaker), the meaning, then the reviews' state and › at
    // the right.
    const b8  _speak     = fude_speech_available();
    const f32 _inner     = _right - _left;
    const f32 _written_w = rde_math_min_f32(220.0f, _inner * 0.26f);
    const f32 _reading_x = _left + 14.0f + _written_w + (_speak ? 30.0f : 12.0f);
    const f32 _reading_w = rde_math_min_f32(200.0f, _inner * 0.22f);
    const f32 _meaning_x = _reading_x + _reading_w + 14.0f;
    const f32 _state_w   = 86.0f;
    const f32 _meaning_w = _right - 30.0f - _state_w - _meaning_x;
    _view->reading_x0 = _speak ? _reading_x - 28.0f : 0.0f;
    _view->reading_x1 = _speak ? _meaning_x - 6.0f : 0.0f;
    const u32 _today  = fude_reviews_today();

    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)fude_draw_x((_left + _right) * 0.5f), (i32)((_y + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)(_right - _left), (u32)rde_math_max_f32(1.0f, _y - _bottom) });
    const u32 _first = (u32)rde_math_max_f32(0.0f, rde_math_floor_f32(_view->scroller.offset.y / FUDE_VOCABVIEW_ROW));
    // A row's line is its bottom edge, below its words and above the next row's.
    for(u32 _i = _first; _i < _n; _i++) {
        const f32 _row_top = _y - (f32)_i * FUDE_VOCABVIEW_ROW + _view->scroller.offset.y;
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
        f32 _px = fude_draw_text_px_to_fit(_font, _font_px, _w->written, FUDE_VOCABVIEW_WORD_PX, _written_w);
        fude_draw_text(_font, _font_px, _w->written, _left + 14.0f, _mid - _px * 0.38f, _px, _theme->ink);
        if(_speak) {
            fude_draw_icon(_font, _font_px, FUDE_ICON_SPEAK, (rde_vec_2F){ _reading_x - 14.0f, _mid }, 14.0f, _theme->accent);
        }
        _px = fude_draw_text_px_to_fit(_font, _font_px, _w->reading, FUDE_VOCABVIEW_SMALL_PX, _reading_w);
        fude_draw_text(_font, _font_px, _w->reading, _reading_x, _mid - _px * 0.36f, _px, _theme->text_soft);
        _px = fude_vocabview_fit(_font, _font_px, _w->meaning, FUDE_VOCABVIEW_SMALL_PX, _meaning_w, _fit, sizeof(_fit));
        fude_draw_text(_font, _font_px, _fit, _meaning_x, _mid - _px * 0.36f, _px, _theme->text);
        // Its reviews: new, due, or in how many days.
        const u32 _due = fude_reviews_due_day(FUDE_VOCAB_KEY(_w->id));
        rde_color _c   = _theme->text_soft;
        if(_due == 0u) {
            rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_REVIEW_NEW));
        } else if(_due <= _today) {
            rde_snprintf(_line, sizeof(_line), "%s", fude_text(FUDE_TEXT_REVIEW_DUE));
            _c = _theme->accent;
        } else {
            FUDE_TEXTF(_line, FUDE_TEXT_REVIEW_IN_N, FUDE_TN(_due - _today));
        }
        _px = fude_draw_text_px_to_fit(_font, _font_px, _line, 11.0f, _state_w);
        fude_draw_text(_font, _font_px, _line, _right - 28.0f - fude_draw_text_width(_font, _font_px, _line, _px), _mid - _px * 0.36f, _px, _c);
        fude_draw_icon(_font, _font_px, FUDE_ICON_NEXT, (rde_vec_2F){ _right - 12.0f, _mid }, 16.0f, _theme->text_soft);
        if(_i + 1u < _n) {
            fude_draw_line((rde_vec_2F){ _left, _row_top - FUDE_VOCABVIEW_ROW }, (rde_vec_2F){ _right, _row_top - FUDE_VOCABVIEW_ROW }, 0.5f, _theme->outline);
        }
    }
    rde_rendering_end_clipping_rect();
}

// --- The screen (screen.h): its row, and what it does ----------------------------------------------------------------

#include "study/app/study.h"
#include "drawing/widgets/notice.h"
#include "study/models/vocab.h"
#include "study/models/sheet.h"

// The screen's is_open, close, pointer and render functions, which call the Vocabulary's own (screen.h).
FUDE_SCREEN_ADAPTERS(fude_vocabview, fude_vocabview)
FUDE_SCREEN_RENDER(fude_vocabview, fude_vocabview)

// The screen's frame.
RDE_INTERNAL void fude_vocabview_screen_update(fude_app* _app, any _self, f32 _dt) {
    RDE_UNUSED(_app);
    fude_vocabview_update((fude_vocabview*)_self, _dt);
}

// The field: every keystroke searches again.
RDE_INTERNAL void fude_vocabview_screen_search(any _self, const c8* _text) {
    fude_vocabview_search((fude_vocabview*)_self, _text);
}

// Returns what the words shown are called: the list's name, or Vocabulary.
RDE_INTERNAL const c8* fude_vocabview_title(const fude_vocabview* _view) {
    return _view->list != 0u ? fude_vocab_list_name(_view->list) : fude_text(FUDE_TEXT_VOCAB);
}

// Back closes the screen.
FUDE_ROW_CALL(fude_vocabview_row_back, fude_vocabview, fude_vocabview_close)

// + Word: the word card for a word typed in, with the list shown ticked.
RDE_INTERNAL void fude_vocabview_row_word(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    fude_wordcard_prefer_list(((const fude_vocabview*)_self)->list);
    fude_wordcard_ask_typed(0u);
}

// Translate with Google: opens the translator (translator.h), from the reader's language into the language taught.
RDE_INTERNAL void fude_vocabview_row_translate(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_translator_open(FUDE_STUDY(_app)->translator);
}

// Review: the words shown that are due today, as a word exam; a notice when there are none.
RDE_INTERNAL void fude_vocabview_row_review(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_arg);
    u32       _due[FUDE_REVIEW_SESSION];
    const u32 _n = fude_vocabview_due((fude_vocabview*)_self, _due, FUDE_REVIEW_SESSION);
    if(_n == 0u) {
        fude_notice_show(fude_text(FUDE_TEXT_REVIEWS_NONE));
    } else {
        fude_wordexam_open_review(FUDE_STUDY(_app)->wordexam, _due, _n);
    }
}

// Exam: a word exam of the words shown, named after them.
RDE_INTERNAL void fude_vocabview_row_exam(fude_app* _app, any _self, u32 _arg) {
    fude_vocabview* _view = (fude_vocabview*)_self;
    RDE_UNUSED(_arg);
    const u32* _ids;
    const u32  _n = fude_vocabview_words(_view, &_ids);
    fude_wordexam_open(FUDE_STUDY(_app)->wordexam, _ids, _n, fude_vocabview_title(_view));
}

// Sheet: a practice sheet of the words' characters. One more than a sheet takes is gathered, so that too many can be
// told (fude_study_sheet).
RDE_INTERNAL void fude_vocabview_row_sheet(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_arg);
    u32       _records[FUDE_SHEET_MAX + 1u];
    const u32 _n = fude_vocabview_records((fude_vocabview*)_self, _records, FUDE_SHEET_MAX + 1u);
    fude_study_sheet(_app, _records, _n);
}

// Practice: the words' characters, as a set (gathered the same way as Sheet's).
RDE_INTERNAL void fude_vocabview_row_practice(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_arg);
    u32       _records[FUDE_SHEET_MAX + 1u];
    const u32 _n = fude_vocabview_records((fude_vocabview*)_self, _records, FUDE_SHEET_MAX + 1u);
    fude_study_practice_set(_app, _records, _n);
}

// The one row; the enum is its button indices. Translate with Google shows only where the translator is.
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

// Which row is up: there is only the one.
RDE_INTERNAL u32 fude_vocabview_screen_row(const any _self) {
    RDE_UNUSED(_self);
    return 0u;
}

// How the buttons show: "Review n" counts the words shown that are due today, and the rest are greyed out without
// words. The count is worked out again only when what it depends on changes: the words shown (listed again for the
// vocabulary, the list or the search), the reviews, or the day. All three are compared as they are, so no change can
// go unseen; they and the count are kept in the screen (_due_for, _due_n).
RDE_INTERNAL void fude_vocabview_screen_faces(const any _self, u32 _row, fude_row_face* _faces) {
    fude_vocabview* _view = (fude_vocabview*)_self;
    RDE_UNUSED(_row);
    const u32* _ids;
    const b8   _words = fude_vocabview_words(_view, &_ids) > 0u;   // Listed again first, if they changed.
    const u32  _now[3] = { _view->_listings, fude_reviews_revision(), fude_reviews_today() };
    if(rde_memcmp(_now, _view->_due_for, sizeof(_now)) != 0) {
        u32 _due[FUDE_REVIEW_SESSION];
        rde_memcpy(_view->_due_for, _now, sizeof(_view->_due_for));
        _view->_due_n = fude_vocabview_due(_view, _due, FUDE_REVIEW_SESSION);
    }
    fude_row_face_count(&_faces[FUDE_VOCAB_ROW_REVIEW], FUDE_TEXT_VOCAB_REVIEW_N, _view->_due_n);
    _faces[FUDE_VOCAB_ROW_REVIEW].disabled   = _view->_due_n == 0u;
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
    .field_hint = FUDE_TEXT_VOCAB_SEARCH_HINT, .field_max = FUDE_VOCABVIEW_FIELD_MAX, .field_change = fude_vocabview_screen_search,
};
