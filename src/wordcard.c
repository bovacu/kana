#include "wordcard.h"
#include "toolbar.h"
#include "toolbar_kit.h"
#include "catalog.h"
#include "draw.h"
#include "icons.h"
#include "text.h"
#include "theme.h"
#include "wordsplit.h"
#include "charnote.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See wordcard.h. Built once with the rest of the toolbar (again when the
// language changes); a list chip per possible list and a chip per found word made
// up front, shown as needed.
// ===========================================================================

#define KANA_WORDCARD_WIDTH    560.0f
#define KANA_WORDCARD_MARGIN   22.0f
#define KANA_WORDCARD_FIELD_H  44.0f
#define KANA_WORDCARD_CHIP_H   34.0f
#define KANA_WORDCARD_GAP      8.0f
#define KANA_WORDCARD_CAPTION  20.0f
#define KANA_WORDCARD_FIELD_PX 14u
#define KANA_WORDCARD_BACKDROP (rde_color){ 0, 0, 0, 110 }

enum { KANA_WORDCARD_WRITTEN = 0, KANA_WORDCARD_READING, KANA_WORDCARD_MEANING };

typedef enum {
    KANA_WORDCARD_ASK_NONE = 0,
    KANA_WORDCARD_ASK_WORD,
    KANA_WORDCARD_ASK_SAVED,
    KANA_WORDCARD_ASK_TYPED,
    KANA_WORDCARD_ASK_READ,
    KANA_WORDCARD_ASK_LIST,
    KANA_WORDCARD_ASK_NOTE
} KANA_WORDCARD_ASK_;

// What was asked for, until the toolbar's next update opens it.
static struct {
    u8                 kind;
    kana_wordcard_word word;
    u32                kanji;
    u32                id;
    kana_wordcard_word found[KANA_WORDCARD_FOUND];
    u32                found_count;
} kana_wordcard_asked;

// The lists the last word saved went in (ids): a new word comes with them ticked.
static u32 kana_wordcard_last_lists[KANA_VOCAB_LISTS];
static u32 kana_wordcard_last_count = 0;
static u32 kana_wordcard_made_list  = 0;   // a list the list form made, for kana_wordcard_take_new_list

void kana_wordcard_ask_list(u32 _list) {
    memset(&kana_wordcard_asked, 0, sizeof(kana_wordcard_asked));
    kana_wordcard_asked.kind = KANA_WORDCARD_ASK_LIST;
    kana_wordcard_asked.id   = _list;
}

void kana_wordcard_ask_note(u32 _codepoint) {
    memset(&kana_wordcard_asked, 0, sizeof(kana_wordcard_asked));
    kana_wordcard_asked.kind  = KANA_WORDCARD_ASK_NOTE;
    kana_wordcard_asked.kanji = _codepoint;
}

u32 kana_wordcard_take_new_list(void) {
    const u32 _list = kana_wordcard_made_list;
    kana_wordcard_made_list = 0;
    return _list;
}

void kana_wordcard_prefer_list(u32 _list) {
    kana_wordcard_last_count = 0;
    if(_list != 0u) {
        kana_wordcard_last_lists[kana_wordcard_last_count++] = _list;
    }
}

RDE_INTERNAL void kana_wordcard_copy(kana_wordcard_word* _w, const c8* _written, const c8* _reading, const c8* _meaning) {
    snprintf(_w->written, sizeof(_w->written), "%s", _written != NULL ? _written : "");
    snprintf(_w->reading, sizeof(_w->reading), "%s", _reading != NULL ? _reading : "");
    snprintf(_w->meaning, sizeof(_w->meaning), "%s", _meaning != NULL ? _meaning : "");
}

void kana_wordcard_ask(const c8* _written, const c8* _reading, const c8* _meaning, u32 _kanji) {
    memset(&kana_wordcard_asked, 0, sizeof(kana_wordcard_asked));
    kana_wordcard_asked.kind  = KANA_WORDCARD_ASK_WORD;
    kana_wordcard_asked.kanji = _kanji;
    kana_wordcard_copy(&kana_wordcard_asked.word, _written, _reading, _meaning);
}

void kana_wordcard_ask_saved(u32 _id) {
    memset(&kana_wordcard_asked, 0, sizeof(kana_wordcard_asked));
    kana_wordcard_asked.kind = KANA_WORDCARD_ASK_SAVED;
    kana_wordcard_asked.id   = _id;
}

void kana_wordcard_ask_typed(u32 _kanji) {
    memset(&kana_wordcard_asked, 0, sizeof(kana_wordcard_asked));
    kana_wordcard_asked.kind  = KANA_WORDCARD_ASK_TYPED;
    kana_wordcard_asked.kanji = _kanji;
}

void kana_wordcard_ask_read(const kana_kanji_db* _db, const c8* _text) {
    memset(&kana_wordcard_asked, 0, sizeof(kana_wordcard_asked));
    kana_wordcard_asked.kind = KANA_WORDCARD_ASK_READ;
    // The text on one line, whole characters while they fit, without the spaces around it.
    c8        _line[KANA_USERWORD_WRITTEN];
    usize     _n = 0;
    const c8* _p = _text != NULL ? _text : "";
    for(;;) {
        const c8* _from = _p;
        const u32 _cp   = kana_kanji_utf8_next(&_p);
        const usize _len = (usize)(_p - _from);
        if(_cp == 0u || _n + _len + 1u > sizeof(_line)) {
            break;
        }
        if(_cp == '\n' || _cp == '\r' || (_cp == ' ' && _n == 0u)) {
            continue;
        }
        memcpy(&_line[_n], _from, _len);
        _n += _len;
    }
    while(_n > 0 && _line[_n - 1u] == ' ') {
        _n--;
    }
    _line[_n] = 0;
    u32       _words[KANA_WORDCARD_FOUND];
    const u32 _found = _db != NULL ? kana_wordsplit(_db, _line, _words, KANA_WORDCARD_FOUND) : 0u;
    for(u32 _i = 0; _i < _found; _i++) {
        kana_kanji_word _w;
        if(kana_kanji_word_at(_db, _words[_i], &_w)) {
            kana_wordcard_copy(&kana_wordcard_asked.found[kana_wordcard_asked.found_count++], _w.written, _w.reading, _w.meaning);
        }
    }
    if(kana_wordcard_asked.found_count == 1u) {
        kana_wordcard_asked.word        = kana_wordcard_asked.found[0];   // one word: that one
        kana_wordcard_asked.found_count = 0;
    } else {
        kana_wordcard_copy(&kana_wordcard_asked.word, _line, "", "");
    }
}

// --- fields ------------------------------------------------------------------------------

RDE_INTERNAL void kana_wordcard_set_field(rde_ui_text_editor* _field, const c8* _text) {
    const usize _bytes = rde_ui_text_editor_get_byte_count(_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_field, 0, _bytes);
    }
    if(_text != NULL && _text[0] != 0) {
        rde_ui_text_editor_insert_at(_field, 0, _text, strlen(_text));
    }
}

// A field's text without the spaces around it, into _out.
RDE_INTERNAL void kana_wordcard_get_field(rde_ui_text_editor* _field, c8* _out, usize _size) {
    c8*       _text = rde_ui_text_editor_get_text(_field, 0, rde_ui_text_editor_get_byte_count(_field));
    const c8* _s    = _text != NULL ? _text : "";
    while(*_s == ' ') {
        _s++;
    }
    snprintf(_out, _size, "%s", _s);
    usize _n = strlen(_out);
    while(_n > 0 && _out[_n - 1u] == ' ') {
        _out[--_n] = 0;
    }
    if(_text != NULL) {
        rde_ui_text_editor_free_text(_field, _text);
    }
}

RDE_INTERNAL b8 kana_wordcard_all_kana(const c8* _text) {
    const c8* _p = _text;
    b8        _any = false;
    for(u32 _cp = kana_kanji_utf8_next(&_p); _cp != 0u; _cp = kana_kanji_utf8_next(&_p)) {
        if(!((_cp >= 0x3041u && _cp <= 0x309Fu) || (_cp >= 0x30A0u && _cp <= 0x30FFu))) {
            return false;
        }
        _any = true;
    }
    return _any;
}

// The dictionary's word written exactly _written (Kana's dictionary has words with
// a kanji). False when it has none.
RDE_INTERNAL b8 kana_wordcard_lookup(const kana_toolbar* _toolbar, const c8* _written, kana_kanji_word* _out) {
    const kana_kanji_db* _db = _toolbar->browse != NULL ? _toolbar->browse->db : NULL;
    u32                  _words[2];
    return _db != NULL && kana_wordsplit(_db, _written, _words, 2u) == 1u && kana_kanji_word_at(_db, _words[0], _out) && strcmp(_out->written, _written) == 0;
}

// The reading and meaning, when empty, from the dictionary.
RDE_INTERNAL void kana_wordcard_fill(kana_toolbar* _toolbar) {
    kana_wordcard* _card = &_toolbar->word;
    c8 _written[KANA_USERWORD_WRITTEN], _reading[KANA_USERWORD_READING], _meaning[KANA_USERWORD_MEANING];
    kana_wordcard_get_field(_card->fields[KANA_WORDCARD_WRITTEN], _written, sizeof(_written));
    kana_wordcard_get_field(_card->fields[KANA_WORDCARD_READING], _reading, sizeof(_reading));
    kana_wordcard_get_field(_card->fields[KANA_WORDCARD_MEANING], _meaning, sizeof(_meaning));
    kana_kanji_word _w;
    if((_reading[0] == 0 || _meaning[0] == 0) && kana_wordcard_lookup(_toolbar, _written, &_w)) {
        if(_reading[0] == 0) {
            kana_wordcard_set_field(_card->fields[KANA_WORDCARD_READING], _w.reading);
        }
        if(_meaning[0] == 0) {
            kana_wordcard_set_field(_card->fields[KANA_WORDCARD_MEANING], _w.meaning);
        }
    }
}

// --- the chips' looks --------------------------------------------------------------------

RDE_INTERNAL void kana_wordcard_style_chips(kana_toolbar* _toolbar) {
    kana_wordcard* _card = &_toolbar->word;
    for(u32 _i = 0; _i < _card->list_count; _i++) {
        kana_toolbar_button_chip(_card->list_chips[_i], _card->ticked[_i]);
    }
    for(u32 _i = 0; _i < _card->found_count; _i++) {
        kana_toolbar_button_chip(_card->found_chips[_i], false);
    }
    kana_toolbar_button_chip(_card->new_list, false);
}

// --- layout ------------------------------------------------------------------------------

RDE_INTERNAL f32 kana_wordcard_chip_w(const kana_toolbar* _toolbar, const c8* _text) {
    return fminf(KANA_WORDCARD_WIDTH - 2.0f * KANA_WORDCARD_MARGIN, kana_draw_text_width(_toolbar->font, (f32)KANA_TOOLBAR_FONT_SIZE, _text, 13.0f) + 32.0f);
}

// Chips in rows from _top (card units, Y up) between _x0 and _x1: placed when
// _place. How tall they came.
RDE_INTERNAL f32 kana_wordcard_chips(const kana_toolbar* _toolbar, rde_ui_button* const* _chips, u32 _n, f32 _x0, f32 _x1, f32 _top, b8 _place) {
    f32 _x    = _x0;
    f32 _rows = _n > 0 ? 1.0f : 0.0f;
    for(u32 _i = 0; _i < _n; _i++) {
        const c8* _text = _chips[_i]->internal_label != NULL && _chips[_i]->internal_label->text != NULL ? _chips[_i]->internal_label->text : "";
        const f32 _w    = kana_wordcard_chip_w(_toolbar, _text);
        if(_x > _x0 && _x + _w > _x1) {
            _x = _x0;
            _rows += 1.0f;
        }
        if(_place) {
            const f32 _y = _top - (_rows - 1.0f) * (KANA_WORDCARD_CHIP_H + KANA_WORDCARD_GAP) - KANA_WORDCARD_CHIP_H * 0.5f;
            kana_toolbar_place(rde_ui_button_as_node(_chips[_i]), (rde_vec_2F){ _x + _w * 0.5f, _y }, (rde_vec_2F){ _w, KANA_WORDCARD_CHIP_H });
        }
        _x += _w + KANA_WORDCARD_GAP;
    }
    return _rows > 0.0f ? _rows * KANA_WORDCARD_CHIP_H + (_rows - 1.0f) * KANA_WORDCARD_GAP : 0.0f;
}

// Everything placed from the top, for what the card shows; the card as tall as that.
RDE_INTERNAL void kana_wordcard_layout(kana_toolbar* _toolbar) {
    kana_wordcard*   _card   = &_toolbar->word;
    const rde_vec_2F _screen = kana_toolbar_virtual_size(_toolbar);
    _card->_layout   = false;
    _card->_laid_out = _screen;
    const f32 _m     = KANA_WORDCARD_MARGIN;
    const f32 _w     = fminf(KANA_WORDCARD_WIDTH, _screen.x - 32.0f);
    const f32 _x0    = _m;
    const f32 _x1    = _w - _m;
    rde_ui_button* _lists[KANA_VOCAB_LISTS + 1u];
    for(u32 _i = 0; _i < _card->list_count; _i++) {
        _lists[_i] = _card->list_chips[_i];
    }
    _lists[_card->list_count] = _card->new_list;
    const u32 _list_n = _card->list_count + (_card->naming ? 0u : 1u);   // naming: the field instead of New list

    // Measured first, then placed from the top of a card that tall.
    f32 _measured = 0.0f;
    for(u32 _pass = 0; _pass < 2u; _pass++) {
        const b8 _place = _pass == 1u;
        f32      _h     = 0.0f;
        f32      _y     = 0.0f;   // down from the card's top
        if(_place) {
            // High on the screen: the keyboard comes up under it.
            const rde_vec_4I _insets = rde_window_get_safe_area_insets(_toolbar->window);   // left, top, right, bottom
            _h                       = _measured;
            const f32 _cy            = fmaxf(_h * 0.5f + 16.0f, _screen.y - (f32)_insets.y - 32.0f - _h * 0.5f);
            kana_toolbar_place(rde_ui_button_as_node(_card->backdrop), (rde_vec_2F){ _screen.x * 0.5f, _screen.y * 0.5f }, _screen);
            kana_toolbar_place(rde_ui_image_as_node(_card->card), (rde_vec_2F){ _screen.x * 0.5f, _cy }, (rde_vec_2F){ _w, _h });
        }
        #define KANA_WORDCARD_TOP(_dy) (_h - (_dy))
        _y += _m;
        if(_place) {
            kana_toolbar_place(rde_ui_label_as_node(_card->title), (rde_vec_2F){ _w * 0.5f, KANA_WORDCARD_TOP(_y + 16.0f) }, (rde_vec_2F){ _x1 - _x0, 32.0f });
        }
        _y += 32.0f + 10.0f;
        if(_card->note_mode) {
            if(_place) {
                kana_toolbar_place(kana_toolbar_field_node(_card->note_field), (rde_vec_2F){ _w * 0.5f, KANA_WORDCARD_TOP(_y + 70.0f) }, (rde_vec_2F){ _x1 - _x0, 140.0f });
            }
            _y += 140.0f + KANA_WORDCARD_GAP;
        }
        for(u32 _f = 0; _f < (_card->note_mode ? 0u : _card->list_mode ? 1u : 3u); _f++) {
            if(_place) {
                kana_toolbar_place(kana_toolbar_field_node(_card->fields[_f]), (rde_vec_2F){ _w * 0.5f, KANA_WORDCARD_TOP(_y + KANA_WORDCARD_FIELD_H * 0.5f) },
                                   (rde_vec_2F){ _x1 - _x0, KANA_WORDCARD_FIELD_H });
            }
            _y += KANA_WORDCARD_FIELD_H + KANA_WORDCARD_GAP;
        }
        if(_card->found_count > 1u && !_card->list_mode && !_card->note_mode) {
            _y += 4.0f;
            if(_place) {
                kana_toolbar_place(rde_ui_label_as_node(_card->found_caption), (rde_vec_2F){ _w * 0.5f, KANA_WORDCARD_TOP(_y + KANA_WORDCARD_CAPTION * 0.5f) },
                                   (rde_vec_2F){ _x1 - _x0, KANA_WORDCARD_CAPTION });
            }
            _y += KANA_WORDCARD_CAPTION + 4.0f;
            _y += kana_wordcard_chips(_toolbar, _card->found_chips, _card->found_count, _x0, _x1, KANA_WORDCARD_TOP(_y), _place) + KANA_WORDCARD_GAP;
        }
        if(!_card->list_mode && !_card->note_mode) {
            _y += 4.0f;
            if(_place) {
                kana_toolbar_place(rde_ui_label_as_node(_card->lists_caption), (rde_vec_2F){ _w * 0.5f, KANA_WORDCARD_TOP(_y + KANA_WORDCARD_CAPTION * 0.5f) },
                                   (rde_vec_2F){ _x1 - _x0, KANA_WORDCARD_CAPTION });
            }
            _y += KANA_WORDCARD_CAPTION + 4.0f;
            _y += kana_wordcard_chips(_toolbar, _lists, _list_n, _x0, _x1, KANA_WORDCARD_TOP(_y), _place) + KANA_WORDCARD_GAP;
        }
        if(_card->naming && !_card->list_mode) {
            if(_place) {
                const f32 _ok = 96.0f;
                kana_toolbar_place(kana_toolbar_field_node(_card->list_field), (rde_vec_2F){ _x0 + (_x1 - _x0 - _ok - KANA_WORDCARD_GAP) * 0.5f, KANA_WORDCARD_TOP(_y + KANA_WORDCARD_FIELD_H * 0.5f) },
                                   (rde_vec_2F){ _x1 - _x0 - _ok - KANA_WORDCARD_GAP, KANA_WORDCARD_FIELD_H });
                kana_toolbar_place(rde_ui_button_as_node(_card->list_ok), (rde_vec_2F){ _x1 - _ok * 0.5f, KANA_WORDCARD_TOP(_y + KANA_WORDCARD_FIELD_H * 0.5f) },
                                   (rde_vec_2F){ _ok, KANA_WORDCARD_FIELD_H - 4.0f });
            }
            _y += KANA_WORDCARD_FIELD_H + KANA_WORDCARD_GAP;
        }
        if(_place) {
            kana_toolbar_place(rde_ui_label_as_node(_card->error), (rde_vec_2F){ _w * 0.5f, KANA_WORDCARD_TOP(_y + 11.0f) }, (rde_vec_2F){ _x1 - _x0, 22.0f });
        }
        _y += 22.0f + 6.0f;
        if(_place) {
            const f32 _bw = 112.0f;
            const f32 _by = KANA_WORDCARD_TOP(_y + 20.0f);
            kana_toolbar_place(rde_ui_button_as_node(_card->save), (rde_vec_2F){ _x1 - _bw * 0.5f, _by }, (rde_vec_2F){ _bw, 40.0f });
            kana_toolbar_place(rde_ui_button_as_node(_card->cancel), (rde_vec_2F){ _x1 - _bw - KANA_WORDCARD_GAP - _bw * 0.5f, _by }, (rde_vec_2F){ _bw, 40.0f });
            kana_toolbar_place(rde_ui_button_as_node(_card->remove), (rde_vec_2F){ _x0 + _bw * 0.5f, _by }, (rde_vec_2F){ _bw, 40.0f });
        }
        _y += 40.0f + _m;
        #undef KANA_WORDCARD_TOP
        _measured = _y;
    }

    // What shows.
    const b8 _word = !_card->list_mode && !_card->note_mode;
    rde_ui_node_set_active(kana_toolbar_field_node(_card->fields[KANA_WORDCARD_WRITTEN]), !_card->note_mode);
    rde_ui_node_set_active(kana_toolbar_field_node(_card->note_field), _card->note_mode);
    for(u32 _i = 0; _i < KANA_WORDCARD_FOUND; _i++) {
        rde_ui_node_set_active(rde_ui_button_as_node(_card->found_chips[_i]), _word && _card->found_count > 1u && _i < _card->found_count);
    }
    rde_ui_node_set_active(rde_ui_label_as_node(_card->found_caption), _word && _card->found_count > 1u);
    for(u32 _i = 0; _i < KANA_VOCAB_LISTS; _i++) {
        rde_ui_node_set_active(rde_ui_button_as_node(_card->list_chips[_i]), _word && _i < _card->list_count);
    }
    rde_ui_node_set_active(rde_ui_label_as_node(_card->lists_caption), _word);
    rde_ui_node_set_active(kana_toolbar_field_node(_card->fields[KANA_WORDCARD_READING]), _word);
    rde_ui_node_set_active(kana_toolbar_field_node(_card->fields[KANA_WORDCARD_MEANING]), _word);
    rde_ui_node_set_active(rde_ui_button_as_node(_card->new_list), _word && !_card->naming);
    rde_ui_node_set_active(kana_toolbar_field_node(_card->list_field), _word && _card->naming);
    rde_ui_node_set_active(rde_ui_button_as_node(_card->list_ok), _word && _card->naming);
    rde_ui_node_set_active(rde_ui_button_as_node(_card->remove), _word ? _card->id != 0u : _card->list_mode ? _card->list_id != 0u : kana_charnote_get(_card->note_cp)[0] != 0);
}

// --- opening and closing -----------------------------------------------------------------

void kana_wordcard_close(kana_toolbar* _toolbar) {
    kana_wordcard* _card = &_toolbar->word;
    _card->open   = false;
    _card->naming = false;
    rde_ui_node_set_active(rde_ui_button_as_node(_card->backdrop), false);
    rde_ui_node_set_active(rde_ui_image_as_node(_card->card), false);
}

// Opens on _word (saved: id), its lists ticked as they are — or, a new word, as
// the last one's were.
RDE_INTERNAL void kana_wordcard_open(kana_toolbar* _toolbar, const kana_wordcard_word* _word, u32 _id, u32 _kanji, const c8* _title) {
    kana_wordcard* _card = &_toolbar->word;
    _card->id        = _id;
    _card->kanji     = _kanji;
    _card->naming    = false;
    _card->list_mode = false;
    _card->list_id   = 0u;
    _card->note_mode = false;
    _card->note_cp   = 0u;
    rde_ui_label_set_text(_card->title, _title);
    rde_ui_text_editor_set_placeholder(_card->fields[KANA_WORDCARD_WRITTEN], kana_text(KANA_TEXT_WORD_FIELD_WRITTEN));
    c8 _remove[96];
    snprintf(_remove, sizeof(_remove), KANA_ICON_TRASH " %s", kana_text(KANA_TEXT_WORD_REMOVE));
    rde_ui_button_set_text(_card->remove, _remove);
    kana_wordcard_set_field(_card->fields[KANA_WORDCARD_WRITTEN], _word->written);
    kana_wordcard_set_field(_card->fields[KANA_WORDCARD_READING], _word->reading);
    kana_wordcard_set_field(_card->fields[KANA_WORDCARD_MEANING], _word->meaning);
    rde_ui_label_set_text(_card->error, "");

    _card->list_count = kana_vocab_list_count();
    for(u32 _i = 0; _i < _card->list_count; _i++) {
        _card->lists[_i] = kana_vocab_list_at(_i);
        b8 _last = false;
        for(u32 _k = 0; _k < kana_wordcard_last_count && _id == 0u; _k++) {
            _last = _last || kana_wordcard_last_lists[_k] == _card->lists[_i];
        }
        _card->ticked[_i] = _id != 0u ? kana_vocab_in_list(_card->lists[_i], _id) : _last;
        rde_ui_button_set_text(_card->list_chips[_i], kana_vocab_list_name(_card->lists[_i]));
    }
    for(u32 _i = 0; _i < _card->found_count && _i < KANA_WORDCARD_FOUND; _i++) {
        c8 _label[KANA_USERWORD_WRITTEN + KANA_USERWORD_READING + 8];
        snprintf(_label, sizeof(_label), "%s  %s", _card->found[_i].written, _card->found[_i].reading);
        rde_ui_button_set_text(_card->found_chips[_i], _label);
    }
    kana_wordcard_style_chips(_toolbar);
    _card->open = true;
    kana_wordcard_layout(_toolbar);
    rde_ui_node_set_active(rde_ui_button_as_node(_card->backdrop), true);
    rde_ui_node_set_active(rde_ui_image_as_node(_card->card), true);
    if(_word->written[0] == 0) {
        rde_ui_node_focus(rde_ui_text_editor_as_node(_card->fields[KANA_WORDCARD_WRITTEN]));   // the keyboard comes up to type it
    }
}

// The list form: list _list's name (0: a new list's), to save; Delete list.
RDE_INTERNAL void kana_wordcard_open_list(kana_toolbar* _toolbar, u32 _list) {
    kana_wordcard*     _card = &_toolbar->word;
    kana_wordcard_word _none = { 0 };
    kana_wordcard_open(_toolbar, &_none, 0u, 0u, kana_text(_list != 0u ? KANA_TEXT_VOCAB_RENAME_LIST : KANA_TEXT_WORD_NEW_LIST));
    _card->list_mode = true;
    _card->list_id   = _list;
    c8 _name[KANA_VOCAB_LIST_NAME];
    if(_list != 0u) {
        snprintf(_name, sizeof(_name), "%s", kana_vocab_list_name(_list));
    } else {
        kana_vocab_list_new_name(_name, sizeof(_name));
    }
    rde_ui_text_editor_set_placeholder(_card->fields[KANA_WORDCARD_WRITTEN], kana_text(KANA_TEXT_WORD_LIST_NAME));
    kana_wordcard_set_field(_card->fields[KANA_WORDCARD_WRITTEN], _name);
    rde_ui_text_editor_select_all(_card->fields[KANA_WORDCARD_WRITTEN]);
    c8 _delete[96];
    snprintf(_delete, sizeof(_delete), KANA_ICON_TRASH " %s", kana_text(KANA_TEXT_VOCAB_DELETE_LIST));
    rde_ui_button_set_text(_card->remove, _delete);
    kana_wordcard_layout(_toolbar);
    rde_ui_node_focus(rde_ui_text_editor_as_node(_card->fields[KANA_WORDCARD_WRITTEN]));   // the keyboard comes up
}

// The note form: character _cp's note, to write; Delete note.
RDE_INTERNAL void kana_wordcard_open_note(kana_toolbar* _toolbar, u32 _cp) {
    kana_wordcard*     _card = &_toolbar->word;
    kana_wordcard_word _none = { 0 };
    c8 _ch[8], _title[96];
    kana_kanji_utf8(_cp, _ch);
    KANA_TEXTF(_title, KANA_TEXT_NOTE_TITLE, KANA_TS(_ch));
    _card->found_count = 0;
    kana_wordcard_open(_toolbar, &_none, 0u, 0u, _title);
    _card->note_mode = true;
    _card->note_cp   = _cp;
    kana_wordcard_set_field(_card->note_field, kana_charnote_get(_cp));
    c8 _delete[96];
    snprintf(_delete, sizeof(_delete), KANA_ICON_TRASH " %s", kana_text(KANA_TEXT_NOTE_DELETE));
    rde_ui_button_set_text(_card->remove, _delete);
    kana_wordcard_layout(_toolbar);
    rde_ui_node_focus(rde_ui_text_editor_as_node(_card->note_field));   // the keyboard comes up
}

RDE_INTERNAL void kana_wordcard_open_asked(kana_toolbar* _toolbar) {
    kana_wordcard*     _card = &_toolbar->word;
    if(kana_wordcard_asked.kind == KANA_WORDCARD_ASK_NOTE) {
        kana_wordcard_asked.kind = KANA_WORDCARD_ASK_NONE;
        kana_wordcard_open_note(_toolbar, kana_wordcard_asked.kanji);
        return;
    }
    if(kana_wordcard_asked.kind == KANA_WORDCARD_ASK_LIST) {
        kana_wordcard_asked.kind = KANA_WORDCARD_ASK_NONE;
        if(kana_wordcard_asked.id == 0u || kana_vocab_list_name(kana_wordcard_asked.id)[0] != 0) {
            kana_wordcard_open_list(_toolbar, kana_wordcard_asked.id);
        }
        return;
    }
    kana_wordcard_word _word = kana_wordcard_asked.word;
    u32                _id   = 0u;
    if(kana_wordcard_asked.kind == KANA_WORDCARD_ASK_SAVED) {
        _id = kana_wordcard_asked.id;
    } else if(kana_wordcard_asked.kind == KANA_WORDCARD_ASK_WORD) {
        _id = kana_vocab_find(_word.written, _word.reading);
    }
    const kana_vocab_word* _saved = _id != 0u ? kana_vocab_get(_id) : NULL;
    if(_saved != NULL) {
        kana_wordcard_copy(&_word, _saved->written, _saved->reading, _saved->meaning);
    } else if(kana_wordcard_asked.kind == KANA_WORDCARD_ASK_SAVED) {
        kana_wordcard_asked.kind = KANA_WORDCARD_ASK_NONE;
        return;   // it went since
    }
    _card->found_count = kana_wordcard_asked.found_count;
    memcpy(_card->found, kana_wordcard_asked.found, sizeof(_card->found));
    c8 _title[128];
    if(_saved != NULL) {
        snprintf(_title, sizeof(_title), "%s", kana_text(KANA_TEXT_WORD_SAVED_TITLE));
    } else if(kana_wordcard_asked.kind == KANA_WORDCARD_ASK_TYPED && kana_wordcard_asked.kanji != 0u) {
        c8 _kanji[8];
        kana_kanji_utf8(kana_wordcard_asked.kanji, _kanji);
        KANA_TEXTF(_title, KANA_TEXT_WORD_TITLE, KANA_TS(_kanji));
    } else {
        snprintf(_title, sizeof(_title), "%s", kana_text(KANA_TEXT_WORD_SAVE_TITLE));
    }
    kana_wordcard_open(_toolbar, &_word, _saved != NULL ? _id : 0u, _saved != NULL ? _saved->kanji : kana_wordcard_asked.kanji, _title);
    kana_wordcard_asked.kind = KANA_WORDCARD_ASK_NONE;
}

void kana_wordcard_update(kana_toolbar* _toolbar) {
    kana_wordcard*   _card   = &_toolbar->word;
    if(kana_wordcard_asked.kind != KANA_WORDCARD_ASK_NONE) {
        kana_wordcard_open_asked(_toolbar);
    }
    const rde_vec_2F _screen = kana_toolbar_virtual_size(_toolbar);
    if(_card->open && (_card->_layout || _screen.x != _card->_laid_out.x || _screen.y != _card->_laid_out.y)) {
        kana_wordcard_layout(_toolbar);
    }
}

// --- saving ------------------------------------------------------------------------------

// The list form's Save: the list renamed, or made.
RDE_INTERNAL void kana_wordcard_save_list(kana_toolbar* _toolbar) {
    kana_wordcard* _card = &_toolbar->word;
    c8             _name[KANA_VOCAB_LIST_NAME];
    kana_wordcard_get_field(_card->fields[KANA_WORDCARD_WRITTEN], _name, sizeof(_name));
    if(_name[0] == 0) {
        rde_ui_label_set_text(_card->error, kana_text(KANA_TEXT_VOCAB_LIST_NAME_EMPTY));
        return;
    }
    if(_card->list_id != 0u) {
        kana_vocab_list_rename(_card->list_id, _name);
    } else {
        const u32 _list = kana_vocab_list_add(_name);
        if(_list == 0u) {
            rde_ui_label_set_text(_card->error, kana_text(KANA_TEXT_VOCAB_LISTS_FULL));
            return;
        }
        kana_wordcard_made_list = _list;
    }
    kana_wordcard_close(_toolbar);
}

RDE_INTERNAL void kana_wordcard_save(kana_toolbar* _toolbar) {
    kana_wordcard* _card = &_toolbar->word;
    if(_card->list_mode) {
        kana_wordcard_save_list(_toolbar);
        return;
    }
    if(_card->note_mode) {
        c8* _text = rde_ui_text_editor_get_text(_card->note_field, 0, rde_ui_text_editor_get_byte_count(_card->note_field));
        kana_charnote_set(_card->note_cp, _text != NULL ? _text : "");
        if(_text != NULL) {
            rde_ui_text_editor_free_text(_card->note_field, _text);
        }
        kana_wordcard_close(_toolbar);
        return;
    }
    c8 _written[KANA_USERWORD_WRITTEN], _reading[KANA_USERWORD_READING], _meaning[KANA_USERWORD_MEANING];
    kana_wordcard_fill(_toolbar);
    kana_wordcard_get_field(_card->fields[KANA_WORDCARD_WRITTEN], _written, sizeof(_written));
    kana_wordcard_get_field(_card->fields[KANA_WORDCARD_READING], _reading, sizeof(_reading));
    kana_wordcard_get_field(_card->fields[KANA_WORDCARD_MEANING], _meaning, sizeof(_meaning));
    if(_reading[0] == 0 && kana_wordcard_all_kana(_written)) {
        snprintf(_reading, sizeof(_reading), "%s", _written);   // a kana word reads as it is written
    }
    b8 _ascii = _reading[0] != 0;
    for(const c8* _c = _reading; *_c != 0; _c++) {
        _ascii = _ascii && (u8)*_c < 0x80u;
    }
    const c8* _error = NULL;
    if(_written[0] == 0) {
        _error = kana_text(KANA_TEXT_WORD_ERR_EMPTY);
    } else if(_reading[0] == 0) {
        _error = kana_text(KANA_TEXT_WORD_ERR_READING);
    } else if(_ascii) {
        c8 _kana[KANA_USERWORD_READING];
        if(kana_romaji_to_hiragana(_reading, _kana, sizeof(_kana))) {
            snprintf(_reading, sizeof(_reading), "%s", _kana);
        } else {
            _error = kana_text(KANA_TEXT_WORD_ERR_ROMAJI);
        }
    }
    u32 _id = _card->id;
    if(_error == NULL) {
        if(_id != 0u) {
            if(!kana_vocab_update(_id, _written, _reading, _meaning)) {
                _error = kana_text(KANA_TEXT_WORD_ERR_SAVED);
            }
        } else {
            _id = kana_vocab_add(_written, _reading, _meaning, _card->kanji);
            if(_id == 0u) {
                _error = kana_text(KANA_TEXT_WORD_ERR_EMPTY);
            }
        }
    }
    if(_error != NULL) {
        rde_ui_label_set_text(_card->error, _error);
        return;
    }

    // Its lists; the ones it went in, for the next new word.
    kana_wordcard_last_count = 0;
    const c8* _first = NULL;
    for(u32 _i = 0; _i < _card->list_count; _i++) {
        kana_vocab_set_in_list(_card->lists[_i], _id, _card->ticked[_i]);
        if(_card->ticked[_i]) {
            kana_wordcard_last_lists[kana_wordcard_last_count++] = _card->lists[_i];
            _first = _first != NULL ? _first : kana_vocab_list_name(_card->lists[_i]);
        }
    }
    c8 _line[256];
    if(_first != NULL) {
        KANA_TEXTF(_line, KANA_TEXT_VOCAB_SAVED_IN, KANA_TS(_written), KANA_TS(_first));
    } else {
        KANA_TEXTF(_line, KANA_TEXT_VOCAB_SAVED, KANA_TS(_written));
    }
    kana_toolbar_notice(_toolbar, _line);
    kana_wordcard_close(_toolbar);
}

// A new list, named (empty: "List n"), the word in it.
RDE_INTERNAL void kana_wordcard_make_list(kana_toolbar* _toolbar) {
    kana_wordcard* _card = &_toolbar->word;
    c8             _name[KANA_VOCAB_LIST_NAME];
    kana_wordcard_get_field(_card->list_field, _name, sizeof(_name));
    if(_name[0] == 0) {
        kana_vocab_list_new_name(_name, sizeof(_name));
    }
    const u32 _list = kana_vocab_list_add(_name);
    _card->naming = false;
    if(_list != 0u && _card->list_count < KANA_VOCAB_LISTS) {
        _card->lists[_card->list_count]  = _list;
        _card->ticked[_card->list_count] = true;
        rde_ui_button_set_text(_card->list_chips[_card->list_count], kana_vocab_list_name(_list));
        _card->list_count++;
    }
    kana_wordcard_style_chips(_toolbar);
    _card->_layout = true;
}

// --- callbacks ---------------------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_wordcard_on_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_wordcard_close((kana_toolbar*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_wordcard_on_save(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_wordcard_save((kana_toolbar*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_wordcard_on_remove(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar*          _toolbar = (kana_toolbar*)_user_data;
    if(_toolbar->word.note_mode) {
        kana_charnote_set(_toolbar->word.note_cp, NULL);   // Delete note
        kana_wordcard_close(_toolbar);
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    if(_toolbar->word.list_mode) {
        // Delete list: the list goes, its words stay.
        if(_toolbar->word.list_id != 0u) {
            c8 _line[256];
            KANA_TEXTF(_line, KANA_TEXT_VOCAB_LIST_DELETED, KANA_TS(kana_vocab_list_name(_toolbar->word.list_id)));
            kana_vocab_list_remove(_toolbar->word.list_id);
            kana_toolbar_notice(_toolbar, _line);
        }
        kana_wordcard_close(_toolbar);
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    const kana_vocab_word* _w       = kana_vocab_get(_toolbar->word.id);
    if(_w != NULL) {
        c8 _line[256];
        KANA_TEXTF(_line, KANA_TEXT_VOCAB_REMOVED, KANA_TS(_w->written));
        kana_vocab_remove(_toolbar->word.id);
        kana_toolbar_notice(_toolbar, _line);
    }
    kana_wordcard_close(_toolbar);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_wordcard_on_list(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_wordcard_ref* _ref  = (const kana_wordcard_ref*)_user_data;
    kana_wordcard*           _card = &_ref->toolbar->word;
    if(_ref->index < _card->list_count) {
        _card->ticked[_ref->index] = !_card->ticked[_ref->index];
        kana_toolbar_button_chip(_card->list_chips[_ref->index], _card->ticked[_ref->index]);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A word found in what was read: the fields take it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_wordcard_on_found(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const kana_wordcard_ref* _ref  = (const kana_wordcard_ref*)_user_data;
    kana_wordcard*           _card = &_ref->toolbar->word;
    const u32                _i    = _ref->index - KANA_VOCAB_LISTS;
    if(_i < _card->found_count) {
        kana_wordcard_set_field(_card->fields[KANA_WORDCARD_WRITTEN], _card->found[_i].written);
        kana_wordcard_set_field(_card->fields[KANA_WORDCARD_READING], _card->found[_i].reading);
        kana_wordcard_set_field(_card->fields[KANA_WORDCARD_MEANING], _card->found[_i].meaning);
        _card->id = kana_vocab_find(_card->found[_i].written, _card->found[_i].reading);   // saved already: that one
        _card->_layout = true;
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_wordcard_on_new_list(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_toolbar*  _toolbar = (kana_toolbar*)_user_data;
    kana_wordcard* _card    = &_toolbar->word;
    if(_card->list_count >= KANA_VOCAB_LISTS) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _card->naming = true;
    kana_wordcard_set_field(_card->list_field, "");
    kana_wordcard_layout(_toolbar);
    rde_ui_node_focus(rde_ui_text_editor_as_node(_card->list_field));
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ kana_wordcard_on_list_ok(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    kana_wordcard_make_list((kana_toolbar*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Return in a field: the written one looks the word up and goes on to the
// reading, the reading to the meaning, the meaning saves; the list's name makes it.
RDE_INTERNAL void kana_wordcard_on_submit(rde_ui_node* _node, any _user_data) {
    kana_toolbar*  _toolbar = (kana_toolbar*)_user_data;
    kana_wordcard* _card    = &_toolbar->word;
    if(_node == rde_ui_text_editor_as_node(_card->list_field)) {
        kana_wordcard_make_list(_toolbar);
        return;
    }
    if(_card->list_mode) {
        kana_wordcard_save(_toolbar);
        return;
    }
    if(_node == rde_ui_text_editor_as_node(_card->fields[KANA_WORDCARD_WRITTEN])) {
        kana_wordcard_fill(_toolbar);
    }
    for(u32 _i = 0; _i < 2u; _i++) {
        if(_node == rde_ui_text_editor_as_node(_card->fields[_i])) {
            rde_ui_node_focus(rde_ui_text_editor_as_node(_card->fields[_i + 1u]));
            return;
        }
    }
    kana_wordcard_save(_toolbar);
}

// --- building ----------------------------------------------------------------------------

RDE_INTERNAL rde_ui_label* kana_wordcard_label(kana_toolbar* _toolbar, rde_ui_node* _parent, f32 _px) {
    rde_ui_label* _label = rde_ui_label_create(NULL);
    rde_ui_label_set_font(_label, _toolbar->font);
    rde_ui_label_set_font_scale(_label, _px / (f32)KANA_TOOLBAR_FONT_SIZE);
    rde_ui_label_set_alignment(_label, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
    rde_ui_node_set_raycast_target(rde_ui_label_as_node(_label), false);
    rde_ui_node_add_child(_parent, rde_ui_label_as_node(_label));
    return _label;
}

RDE_INTERNAL rde_ui_text_editor* kana_wordcard_field(kana_toolbar* _toolbar, rde_ui_node* _parent, const c8* _placeholder, u32 _max) {
    rde_ui_text_editor* _f = rde_ui_text_editor_create(_toolbar->font, NULL);
    rde_ui_text_editor_set_multiline(_f, false);
    rde_ui_text_editor_set_font_size(_f, KANA_WORDCARD_FIELD_PX);
    rde_ui_text_editor_set_max_chars(_f, _max);
    rde_ui_text_editor_set_placeholder(_f, _placeholder);
    rde_ui_text_editor_set_content_insets(_f, 12.0f, 8.0f, 12.0f, 8.0f);
    rde_ui_text_editor_add_plugin(_f, rde_ui_text_editor_plugin_ime_get());   // Japanese from the keyboard
    rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_f), _toolbar);
    rde_ui_text_editor_set_on_submit(_f, kana_wordcard_on_submit);
    kana_toolbar_field_box(_parent, _f);
    return _f;
}

void kana_wordcard_create(kana_toolbar* _toolbar, rde_ui_node* _root) {
    kana_wordcard* _card = &_toolbar->word;
    memset(_card, 0, sizeof(*_card));
    // Over everything, with a backdrop that cancels it.
    _card->backdrop = rde_ui_button_create(NULL, NULL);
    rde_ui_button_set_on_click(_card->backdrop, kana_wordcard_on_cancel, _toolbar);
    rde_ui_node_add_child(_root, rde_ui_button_as_node(_card->backdrop));
    _card->card     = rde_ui_image_create(NULL);
    rde_ui_node* _c = rde_ui_image_as_node(_card->card);
    rde_ui_node_set_blocks_input(_c, true);
    rde_ui_node_add_child(_root, _c);

    _card->title = kana_wordcard_label(_toolbar, _c, 17.0f);
    const c8* const _placeholders[3] = { kana_text(KANA_TEXT_WORD_FIELD_WRITTEN), kana_text(KANA_TEXT_WORD_FIELD_READING), kana_text(KANA_TEXT_WORD_FIELD_MEANING) };
    const u32       _max[3]          = { 24u, 32u, 60u };
    for(u32 _i = 0; _i < 3u; _i++) {
        _card->fields[_i] = kana_wordcard_field(_toolbar, _c, _placeholders[_i], _max[_i]);
    }
    _card->found_caption = kana_wordcard_label(_toolbar, _c, 12.0f);
    rde_ui_label_set_text(_card->found_caption, kana_text(KANA_TEXT_WORD_FOUND));
    for(u32 _i = 0; _i < KANA_WORDCARD_FOUND; _i++) {
        _card->refs[KANA_VOCAB_LISTS + _i] = (kana_wordcard_ref){ _toolbar, KANA_VOCAB_LISTS + _i };
        _card->found_chips[_i]             = kana_toolbar_button(_toolbar, _c, "", kana_wordcard_on_found);
        rde_ui_button_set_on_click(_card->found_chips[_i], kana_wordcard_on_found, &_card->refs[KANA_VOCAB_LISTS + _i]);
    }
    _card->lists_caption = kana_wordcard_label(_toolbar, _c, 12.0f);
    rde_ui_label_set_text(_card->lists_caption, kana_text(KANA_TEXT_WORD_LISTS));
    for(u32 _i = 0; _i < KANA_VOCAB_LISTS; _i++) {
        _card->refs[_i]       = (kana_wordcard_ref){ _toolbar, _i };
        _card->list_chips[_i] = kana_toolbar_button(_toolbar, _c, "", kana_wordcard_on_list);
        rde_ui_button_set_on_click(_card->list_chips[_i], kana_wordcard_on_list, &_card->refs[_i]);
    }
    c8 _label[96];
    snprintf(_label, sizeof(_label), KANA_ICON_PLUS " %s", kana_text(KANA_TEXT_WORD_NEW_LIST));
    _card->new_list   = kana_toolbar_button(_toolbar, _c, _label, kana_wordcard_on_new_list);
    _card->list_field = kana_wordcard_field(_toolbar, _c, kana_text(KANA_TEXT_WORD_LIST_NAME), 30u);
    _card->note_field = kana_wordcard_field(_toolbar, _c, kana_text(KANA_TEXT_NOTE_PLACEHOLDER), 180u);
    rde_ui_text_editor_set_multiline(_card->note_field, true);
    _card->list_ok    = kana_toolbar_button(_toolbar, _c, kana_text(KANA_TEXT_ADD), kana_wordcard_on_list_ok);
    _card->error      = kana_wordcard_label(_toolbar, _c, 12.0f);
    snprintf(_label, sizeof(_label), KANA_ICON_TRASH " %s", kana_text(KANA_TEXT_WORD_REMOVE));
    _card->remove     = kana_toolbar_button(_toolbar, _c, _label, kana_wordcard_on_remove);
    _card->cancel     = kana_toolbar_button(_toolbar, _c, kana_text(KANA_TEXT_CANCEL), kana_wordcard_on_cancel);
    snprintf(_label, sizeof(_label), KANA_ICON_CHECK " %s", kana_text(KANA_TEXT_SAVE));
    _card->save       = kana_toolbar_button(_toolbar, _c, _label, kana_wordcard_on_save);
    kana_wordcard_close(_toolbar);
}

void kana_wordcard_apply_theme(kana_toolbar* _toolbar) {
    kana_wordcard*    _card = &_toolbar->word;
    const kana_theme* _t    = kana_theme_active();
    if(_card->card == NULL) {
        return;
    }
    kana_toolbar_style_panel(_card->card, 18.0f, 1.0f);
    kana_toolbar_button_colors(_card->backdrop, KANA_WORDCARD_BACKDROP, 0.0f, (rde_color){ 0, 0, 0, 0 });
    for(u32 _i = 0; _i < 3u; _i++) {
        kana_toolbar_style_field(_card->fields[_i]);
    }
    kana_toolbar_style_field(_card->list_field);
    kana_toolbar_style_field(_card->note_field);
    rde_ui_label_set_color(_card->title, _t->button_text);
    rde_ui_label_set_color(_card->found_caption, _t->text_soft);
    rde_ui_label_set_color(_card->lists_caption, _t->text_soft);
    rde_ui_label_set_color(_card->error, _t->score_poor);
    kana_toolbar_button_plain(_card->cancel);
    kana_toolbar_button_primary(_card->save);
    kana_toolbar_button_danger_quiet(_card->remove);
    kana_toolbar_button_plain(_card->list_ok);
    kana_wordcard_style_chips(_toolbar);
}
