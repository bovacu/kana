#include "lang/ja/romaji.h"
#include "study/widgets/wordcard.h"
#include "drawing/app/ui.h"
#include "drawing/widgets/notice.h"
#include "drawing/widgets/kit.h"
#include "study/chars/catalog.h"
#include "drawing/widgets/draw.h"
#include "drawing/widgets/icons.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "lang/ja/wordsplit.h"
#include "study/models/charnote.h"
#include "study/app/study.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See wordcard.h. Built once with the rest of the UI (again when the
// language changes); a list chip per possible list and a chip per found word made
// up front, shown as needed.
// ===========================================================================

// The card is the study's (study.h); the UI it is on, the core's.
#define FUDE_WORDCARD_OF(_ui) (&FUDE_STUDY((_ui)->app)->word)

#define FUDE_WORDCARD_WIDTH    560.0f
#define FUDE_WORDCARD_MARGIN   22.0f
#define FUDE_WORDCARD_FIELD_H  44.0f
#define FUDE_WORDCARD_CHIP_H   34.0f
#define FUDE_WORDCARD_GAP      8.0f
#define FUDE_WORDCARD_CAPTION  20.0f
#define FUDE_WORDCARD_FIELD_PX 14u

enum { FUDE_WORDCARD_WRITTEN = 0, FUDE_WORDCARD_READING, FUDE_WORDCARD_MEANING };

typedef enum {
    FUDE_WORDCARD_ASK_NONE = 0,
    FUDE_WORDCARD_ASK_WORD,
    FUDE_WORDCARD_ASK_SAVED,
    FUDE_WORDCARD_ASK_TYPED,
    FUDE_WORDCARD_ASK_READ,
    FUDE_WORDCARD_ASK_LIST,
    FUDE_WORDCARD_ASK_NOTE
} FUDE_WORDCARD_ASK_;

// What was asked for, until the UI's next update opens it.
static struct {
    u8                 kind;
    fude_wordcard_word word;
    u32                kanji;
    u32                id;
    fude_wordcard_word found[FUDE_WORDCARD_FOUND];
    u32                found_count;
} fude_wordcard_asked;

// The lists the last word saved went in (ids): a new word comes with them ticked.
static u32 fude_wordcard_last_lists[FUDE_VOCAB_LISTS];
static u32 fude_wordcard_last_count = 0;
static u32 fude_wordcard_made_list  = 0;   // a list the list form made, for fude_wordcard_take_new_list

void fude_wordcard_ask_list(u32 _list) {
    memset(&fude_wordcard_asked, 0, sizeof(fude_wordcard_asked));
    fude_wordcard_asked.kind = FUDE_WORDCARD_ASK_LIST;
    fude_wordcard_asked.id   = _list;
}

void fude_wordcard_ask_note(u32 _codepoint) {
    memset(&fude_wordcard_asked, 0, sizeof(fude_wordcard_asked));
    fude_wordcard_asked.kind  = FUDE_WORDCARD_ASK_NOTE;
    fude_wordcard_asked.kanji = _codepoint;
}

u32 fude_wordcard_take_new_list(void) {
    const u32 _list = fude_wordcard_made_list;
    fude_wordcard_made_list = 0;
    return _list;
}

void fude_wordcard_prefer_list(u32 _list) {
    fude_wordcard_last_count = 0;
    if(_list != 0u) {
        fude_wordcard_last_lists[fude_wordcard_last_count++] = _list;
    }
}

RDE_INTERNAL void fude_wordcard_copy(fude_wordcard_word* _w, const c8* _written, const c8* _reading, const c8* _meaning) {
    snprintf(_w->written, sizeof(_w->written), "%s", _written != NULL ? _written : "");
    snprintf(_w->reading, sizeof(_w->reading), "%s", _reading != NULL ? _reading : "");
    snprintf(_w->meaning, sizeof(_w->meaning), "%s", _meaning != NULL ? _meaning : "");
}

void fude_wordcard_ask(const c8* _written, const c8* _reading, const c8* _meaning, u32 _kanji) {
    memset(&fude_wordcard_asked, 0, sizeof(fude_wordcard_asked));
    fude_wordcard_asked.kind  = FUDE_WORDCARD_ASK_WORD;
    fude_wordcard_asked.kanji = _kanji;
    fude_wordcard_copy(&fude_wordcard_asked.word, _written, _reading, _meaning);
}

void fude_wordcard_ask_saved(u32 _id) {
    memset(&fude_wordcard_asked, 0, sizeof(fude_wordcard_asked));
    fude_wordcard_asked.kind = FUDE_WORDCARD_ASK_SAVED;
    fude_wordcard_asked.id   = _id;
}

void fude_wordcard_ask_typed(u32 _kanji) {
    memset(&fude_wordcard_asked, 0, sizeof(fude_wordcard_asked));
    fude_wordcard_asked.kind  = FUDE_WORDCARD_ASK_TYPED;
    fude_wordcard_asked.kanji = _kanji;
}

void fude_wordcard_ask_read(const fude_kanji_db* _db, const c8* _text) {
    memset(&fude_wordcard_asked, 0, sizeof(fude_wordcard_asked));
    fude_wordcard_asked.kind = FUDE_WORDCARD_ASK_READ;
    // The text on one line, whole characters while they fit, without the spaces around it.
    c8        _line[FUDE_USERWORD_WRITTEN];
    usize     _n = 0;
    const c8* _p = _text != NULL ? _text : "";
    for(;;) {
        const c8* _from = _p;
        const u32 _cp   = fude_utf8_next(&_p);
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
    u32       _words[FUDE_WORDCARD_FOUND];
    const u32 _found = _db != NULL ? fude_wordsplit(_db, _line, _words, FUDE_WORDCARD_FOUND) : 0u;
    for(u32 _i = 0; _i < _found; _i++) {
        fude_kanji_word _w;
        if(fude_kanji_word_at(_db, _words[_i], &_w)) {
            fude_wordcard_copy(&fude_wordcard_asked.found[fude_wordcard_asked.found_count++], _w.written, _w.reading, _w.meaning);
        }
    }
    if(fude_wordcard_asked.found_count == 1u) {
        fude_wordcard_asked.word        = fude_wordcard_asked.found[0];   // one word: that one
        fude_wordcard_asked.found_count = 0;
    } else {
        fude_wordcard_copy(&fude_wordcard_asked.word, _line, "", "");
    }
}

// --- fields ------------------------------------------------------------------------------

RDE_INTERNAL void fude_wordcard_set_field(rde_ui_text_editor* _field, const c8* _text) {
    const usize _bytes = rde_ui_text_editor_get_byte_count(_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_field, 0, _bytes);
    }
    if(_text != NULL && _text[0] != 0) {
        rde_ui_text_editor_insert_at(_field, 0, _text, strlen(_text));
    }
}

// A field's text without the spaces around it, into _out.
RDE_INTERNAL void fude_wordcard_get_field(rde_ui_text_editor* _field, c8* _out, usize _size) {
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

RDE_INTERNAL b8 fude_wordcard_all_kana(const c8* _text) {
    const c8* _p = _text;
    b8        _any = false;
    for(u32 _cp = fude_utf8_next(&_p); _cp != 0u; _cp = fude_utf8_next(&_p)) {
        if(!((_cp >= 0x3041u && _cp <= 0x309Fu) || (_cp >= 0x30A0u && _cp <= 0x30FFu))) {
            return false;
        }
        _any = true;
    }
    return _any;
}

// The dictionary's word written exactly _written (Kana's dictionary has words with
// a kanji). False when it has none.
RDE_INTERNAL b8 fude_wordcard_lookup(const fude_ui* _ui, const c8* _written, fude_kanji_word* _out) {
    const fude_study*    _study = FUDE_STUDY(_ui->app);
    const fude_kanji_db* _db    = _study->browse != NULL ? _study->db : NULL;
    u32                  _words[2];
    return _db != NULL && fude_wordsplit(_db, _written, _words, 2u) == 1u && fude_kanji_word_at(_db, _words[0], _out) && strcmp(_out->written, _written) == 0;
}

// The reading and meaning, when empty, from the dictionary.
RDE_INTERNAL void fude_wordcard_fill(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    c8 _written[FUDE_USERWORD_WRITTEN], _reading[FUDE_USERWORD_READING], _meaning[FUDE_USERWORD_MEANING];
    fude_wordcard_get_field(_card->fields[FUDE_WORDCARD_WRITTEN], _written, sizeof(_written));
    fude_wordcard_get_field(_card->fields[FUDE_WORDCARD_READING], _reading, sizeof(_reading));
    fude_wordcard_get_field(_card->fields[FUDE_WORDCARD_MEANING], _meaning, sizeof(_meaning));
    fude_kanji_word _w;
    if((_reading[0] == 0 || _meaning[0] == 0) && fude_wordcard_lookup(_ui, _written, &_w)) {
        if(_reading[0] == 0) {
            fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_READING], _w.reading);
        }
        if(_meaning[0] == 0) {
            fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_MEANING], _w.meaning);
        }
    }
}

// --- the chips' looks --------------------------------------------------------------------

RDE_INTERNAL void fude_wordcard_style_chips(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    for(u32 _i = 0; _i < _card->list_count; _i++) {
        fude_kit_button_chip(_card->list_chips[_i], _card->ticked[_i]);
    }
    for(u32 _i = 0; _i < _card->found_count; _i++) {
        fude_kit_button_chip(_card->found_chips[_i], false);
    }
    fude_kit_button_chip(_card->new_list, false);
}

// --- layout ------------------------------------------------------------------------------

RDE_INTERNAL f32 fude_wordcard_chip_w(const fude_ui* _ui, const c8* _text) {
    return fminf(FUDE_WORDCARD_WIDTH - 2.0f * FUDE_WORDCARD_MARGIN, fude_draw_text_width(_ui->font, (f32)FUDE_KIT_FONT_SIZE, _text, 13.0f) + 32.0f);
}

// Chips in rows from _top (card units, Y up) between _x0 and _x1: placed when
// _place. How tall they came.
RDE_INTERNAL f32 fude_wordcard_chips(const fude_ui* _ui, rde_ui_button* const* _chips, u32 _n, f32 _x0, f32 _x1, f32 _top, b8 _place) {
    f32 _x    = _x0;
    f32 _rows = _n > 0 ? 1.0f : 0.0f;
    for(u32 _i = 0; _i < _n; _i++) {
        const c8* _text = _chips[_i]->internal_label != NULL && _chips[_i]->internal_label->text != NULL ? _chips[_i]->internal_label->text : "";
        const f32 _w    = fude_wordcard_chip_w(_ui, _text);
        if(_x > _x0 && _x + _w > _x1) {
            _x = _x0;
            _rows += 1.0f;
        }
        if(_place) {
            const f32 _y = _top - (_rows - 1.0f) * (FUDE_WORDCARD_CHIP_H + FUDE_WORDCARD_GAP) - FUDE_WORDCARD_CHIP_H * 0.5f;
            fude_kit_place(rde_ui_button_as_node(_chips[_i]), (rde_vec_2F){ _x + _w * 0.5f, _y }, (rde_vec_2F){ _w, FUDE_WORDCARD_CHIP_H });
        }
        _x += _w + FUDE_WORDCARD_GAP;
    }
    return _rows > 0.0f ? _rows * FUDE_WORDCARD_CHIP_H + (_rows - 1.0f) * FUDE_WORDCARD_GAP : 0.0f;
}

// Everything placed from the top, for what the card shows; the card as tall as that.
RDE_INTERNAL void fude_wordcard_layout(fude_ui* _ui) {
    fude_wordcard*   _card   = FUDE_WORDCARD_OF(_ui);
    const rde_vec_2F _screen = fude_kit_screen_size((_ui)->window);
    _card->_layout   = false;
    _card->_laid_out = _screen;
    const f32 _m     = FUDE_WORDCARD_MARGIN;
    const f32 _w     = fminf(FUDE_WORDCARD_WIDTH, _screen.x - 32.0f);
    const f32 _x0    = _m;
    const f32 _x1    = _w - _m;
    rde_ui_button* _lists[FUDE_VOCAB_LISTS + 1u];
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
            const rde_vec_4I _insets = rde_window_get_safe_area_insets(_ui->window);   // left, top, right, bottom
            _h                       = _measured;
            const f32 _cy            = fmaxf(_h * 0.5f + 16.0f, _screen.y - (f32)_insets.y - 32.0f - _h * 0.5f);
            fude_kit_modal_place(&_card->modal, _ui->window, (rde_vec_2F){ _screen.x * 0.5f, _cy }, (rde_vec_2F){ _w, _h });
        }
        #define FUDE_WORDCARD_TOP(_dy) (_h - (_dy))
        _y += _m;
        if(_place) {
            fude_kit_place(rde_ui_label_as_node(_card->title), (rde_vec_2F){ _w * 0.5f, FUDE_WORDCARD_TOP(_y + 16.0f) }, (rde_vec_2F){ _x1 - _x0, 32.0f });
        }
        _y += 32.0f + 10.0f;
        if(_card->note_mode) {
            if(_place) {
                fude_kit_place(fude_kit_field_node(_card->note_field), (rde_vec_2F){ _w * 0.5f, FUDE_WORDCARD_TOP(_y + 70.0f) }, (rde_vec_2F){ _x1 - _x0, 140.0f });
            }
            _y += 140.0f + FUDE_WORDCARD_GAP;
        }
        for(u32 _f = 0; _f < (_card->note_mode ? 0u : _card->list_mode ? 1u : 3u); _f++) {
            if(_place) {
                fude_kit_place(fude_kit_field_node(_card->fields[_f]), (rde_vec_2F){ _w * 0.5f, FUDE_WORDCARD_TOP(_y + FUDE_WORDCARD_FIELD_H * 0.5f) },
                                   (rde_vec_2F){ _x1 - _x0, FUDE_WORDCARD_FIELD_H });
            }
            _y += FUDE_WORDCARD_FIELD_H + FUDE_WORDCARD_GAP;
        }
        if(_card->found_count > 1u && !_card->list_mode && !_card->note_mode) {
            _y += 4.0f;
            if(_place) {
                fude_kit_place(rde_ui_label_as_node(_card->found_caption), (rde_vec_2F){ _w * 0.5f, FUDE_WORDCARD_TOP(_y + FUDE_WORDCARD_CAPTION * 0.5f) },
                                   (rde_vec_2F){ _x1 - _x0, FUDE_WORDCARD_CAPTION });
            }
            _y += FUDE_WORDCARD_CAPTION + 4.0f;
            _y += fude_wordcard_chips(_ui, _card->found_chips, _card->found_count, _x0, _x1, FUDE_WORDCARD_TOP(_y), _place) + FUDE_WORDCARD_GAP;
        }
        if(!_card->list_mode && !_card->note_mode) {
            _y += 4.0f;
            if(_place) {
                fude_kit_place(rde_ui_label_as_node(_card->lists_caption), (rde_vec_2F){ _w * 0.5f, FUDE_WORDCARD_TOP(_y + FUDE_WORDCARD_CAPTION * 0.5f) },
                                   (rde_vec_2F){ _x1 - _x0, FUDE_WORDCARD_CAPTION });
            }
            _y += FUDE_WORDCARD_CAPTION + 4.0f;
            _y += fude_wordcard_chips(_ui, _lists, _list_n, _x0, _x1, FUDE_WORDCARD_TOP(_y), _place) + FUDE_WORDCARD_GAP;
        }
        if(_card->naming && !_card->list_mode) {
            if(_place) {
                const f32 _ok = 96.0f;
                fude_kit_place(fude_kit_field_node(_card->list_field), (rde_vec_2F){ _x0 + (_x1 - _x0 - _ok - FUDE_WORDCARD_GAP) * 0.5f, FUDE_WORDCARD_TOP(_y + FUDE_WORDCARD_FIELD_H * 0.5f) },
                                   (rde_vec_2F){ _x1 - _x0 - _ok - FUDE_WORDCARD_GAP, FUDE_WORDCARD_FIELD_H });
                fude_kit_place(rde_ui_button_as_node(_card->list_ok), (rde_vec_2F){ _x1 - _ok * 0.5f, FUDE_WORDCARD_TOP(_y + FUDE_WORDCARD_FIELD_H * 0.5f) },
                                   (rde_vec_2F){ _ok, FUDE_WORDCARD_FIELD_H - 4.0f });
            }
            _y += FUDE_WORDCARD_FIELD_H + FUDE_WORDCARD_GAP;
        }
        if(_place) {
            fude_kit_place(rde_ui_label_as_node(_card->error), (rde_vec_2F){ _w * 0.5f, FUDE_WORDCARD_TOP(_y + 11.0f) }, (rde_vec_2F){ _x1 - _x0, 22.0f });
        }
        _y += 22.0f + 6.0f;
        if(_place) {
            const f32 _bw = 112.0f;
            const f32 _by = FUDE_WORDCARD_TOP(_y + 20.0f);
            fude_kit_place(rde_ui_button_as_node(_card->save), (rde_vec_2F){ _x1 - _bw * 0.5f, _by }, (rde_vec_2F){ _bw, 40.0f });
            fude_kit_place(rde_ui_button_as_node(_card->cancel), (rde_vec_2F){ _x1 - _bw - FUDE_WORDCARD_GAP - _bw * 0.5f, _by }, (rde_vec_2F){ _bw, 40.0f });
            fude_kit_place(rde_ui_button_as_node(_card->remove), (rde_vec_2F){ _x0 + _bw * 0.5f, _by }, (rde_vec_2F){ _bw, 40.0f });
        }
        _y += 40.0f + _m;
        #undef FUDE_WORDCARD_TOP
        _measured = _y;
    }

    // What shows.
    const b8 _word = !_card->list_mode && !_card->note_mode;
    rde_ui_node_set_active(fude_kit_field_node(_card->fields[FUDE_WORDCARD_WRITTEN]), !_card->note_mode);
    rde_ui_node_set_active(fude_kit_field_node(_card->note_field), _card->note_mode);
    for(u32 _i = 0; _i < FUDE_WORDCARD_FOUND; _i++) {
        rde_ui_node_set_active(rde_ui_button_as_node(_card->found_chips[_i]), _word && _card->found_count > 1u && _i < _card->found_count);
    }
    rde_ui_node_set_active(rde_ui_label_as_node(_card->found_caption), _word && _card->found_count > 1u);
    for(u32 _i = 0; _i < FUDE_VOCAB_LISTS; _i++) {
        rde_ui_node_set_active(rde_ui_button_as_node(_card->list_chips[_i]), _word && _i < _card->list_count);
    }
    rde_ui_node_set_active(rde_ui_label_as_node(_card->lists_caption), _word);
    rde_ui_node_set_active(fude_kit_field_node(_card->fields[FUDE_WORDCARD_READING]), _word);
    rde_ui_node_set_active(fude_kit_field_node(_card->fields[FUDE_WORDCARD_MEANING]), _word);
    rde_ui_node_set_active(rde_ui_button_as_node(_card->new_list), _word && !_card->naming);
    rde_ui_node_set_active(fude_kit_field_node(_card->list_field), _word && _card->naming);
    rde_ui_node_set_active(rde_ui_button_as_node(_card->list_ok), _word && _card->naming);
    rde_ui_node_set_active(rde_ui_button_as_node(_card->remove), _word ? _card->id != 0u : _card->list_mode ? _card->list_id != 0u : fude_charnote_get(_card->note_cp)[0] != 0);
}

// --- opening and closing -----------------------------------------------------------------

void fude_wordcard_close(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    _card->open   = false;
    _card->naming = false;
    fude_kit_modal_show(&_card->modal, false);
}

// Opens on _word (saved: id), its lists ticked as they are — or, a new word, as
// the last one's were.
RDE_INTERNAL void fude_wordcard_open(fude_ui* _ui, const fude_wordcard_word* _word, u32 _id, u32 _kanji, const c8* _title) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    _card->id        = _id;
    _card->kanji     = _kanji;
    _card->naming    = false;
    _card->list_mode = false;
    _card->list_id   = 0u;
    _card->note_mode = false;
    _card->note_cp   = 0u;
    rde_ui_label_set_text(_card->title, _title);
    rde_ui_text_editor_set_placeholder(_card->fields[FUDE_WORDCARD_WRITTEN], fude_text(FUDE_TEXT_WORD_FIELD_WRITTEN));
    c8 _remove[96];
    snprintf(_remove, sizeof(_remove), FUDE_ICON_TRASH " %s", fude_text(FUDE_TEXT_WORD_REMOVE));
    rde_ui_button_set_text(_card->remove, _remove);
    fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_WRITTEN], _word->written);
    fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_READING], _word->reading);
    fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_MEANING], _word->meaning);
    rde_ui_label_set_text(_card->error, "");

    _card->list_count = fude_vocab_list_count();
    for(u32 _i = 0; _i < _card->list_count; _i++) {
        _card->lists[_i] = fude_vocab_list_at(_i);
        b8 _last = false;
        for(u32 _k = 0; _k < fude_wordcard_last_count && _id == 0u; _k++) {
            _last = _last || fude_wordcard_last_lists[_k] == _card->lists[_i];
        }
        _card->ticked[_i] = _id != 0u ? fude_vocab_in_list(_card->lists[_i], _id) : _last;
        rde_ui_button_set_text(_card->list_chips[_i], fude_vocab_list_name(_card->lists[_i]));
    }
    for(u32 _i = 0; _i < _card->found_count && _i < FUDE_WORDCARD_FOUND; _i++) {
        c8 _label[FUDE_USERWORD_WRITTEN + FUDE_USERWORD_READING + 8];
        snprintf(_label, sizeof(_label), "%s  %s", _card->found[_i].written, _card->found[_i].reading);
        rde_ui_button_set_text(_card->found_chips[_i], _label);
    }
    fude_wordcard_style_chips(_ui);
    _card->open = true;
    fude_wordcard_layout(_ui);
    fude_kit_modal_show(&_card->modal, true);
    if(_word->written[0] == 0) {
        rde_ui_node_focus(rde_ui_text_editor_as_node(_card->fields[FUDE_WORDCARD_WRITTEN]));   // the keyboard comes up to type it
    }
}

// The list form: list _list's name (0: a new list's), to save; Delete list.
RDE_INTERNAL void fude_wordcard_open_list(fude_ui* _ui, u32 _list) {
    fude_wordcard*     _card = FUDE_WORDCARD_OF(_ui);
    fude_wordcard_word _none = { 0 };
    fude_wordcard_open(_ui, &_none, 0u, 0u, fude_text(_list != 0u ? FUDE_TEXT_VOCAB_RENAME_LIST : FUDE_TEXT_WORD_NEW_LIST));
    _card->list_mode = true;
    _card->list_id   = _list;
    c8 _name[FUDE_VOCAB_LIST_NAME];
    if(_list != 0u) {
        snprintf(_name, sizeof(_name), "%s", fude_vocab_list_name(_list));
    } else {
        fude_vocab_list_new_name(_name, sizeof(_name));
    }
    rde_ui_text_editor_set_placeholder(_card->fields[FUDE_WORDCARD_WRITTEN], fude_text(FUDE_TEXT_WORD_LIST_NAME));
    fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_WRITTEN], _name);
    rde_ui_text_editor_select_all(_card->fields[FUDE_WORDCARD_WRITTEN]);
    c8 _delete[96];
    snprintf(_delete, sizeof(_delete), FUDE_ICON_TRASH " %s", fude_text(FUDE_TEXT_VOCAB_DELETE_LIST));
    rde_ui_button_set_text(_card->remove, _delete);
    fude_wordcard_layout(_ui);
    rde_ui_node_focus(rde_ui_text_editor_as_node(_card->fields[FUDE_WORDCARD_WRITTEN]));   // the keyboard comes up
}

// The note form: character _cp's note, to write; Delete note.
RDE_INTERNAL void fude_wordcard_open_note(fude_ui* _ui, u32 _cp) {
    fude_wordcard*     _card = FUDE_WORDCARD_OF(_ui);
    fude_wordcard_word _none = { 0 };
    c8 _ch[8], _title[96];
    fude_utf8_put(_cp, _ch);
    FUDE_TEXTF(_title, FUDE_TEXT_NOTE_TITLE, FUDE_TS(_ch));
    _card->found_count = 0;
    fude_wordcard_open(_ui, &_none, 0u, 0u, _title);
    _card->note_mode = true;
    _card->note_cp   = _cp;
    fude_wordcard_set_field(_card->note_field, fude_charnote_get(_cp));
    c8 _delete[96];
    snprintf(_delete, sizeof(_delete), FUDE_ICON_TRASH " %s", fude_text(FUDE_TEXT_NOTE_DELETE));
    rde_ui_button_set_text(_card->remove, _delete);
    fude_wordcard_layout(_ui);
    rde_ui_node_focus(rde_ui_text_editor_as_node(_card->note_field));   // the keyboard comes up
}

RDE_INTERNAL void fude_wordcard_open_asked(fude_ui* _ui) {
    fude_wordcard*     _card = FUDE_WORDCARD_OF(_ui);
    if(fude_wordcard_asked.kind == FUDE_WORDCARD_ASK_NOTE) {
        fude_wordcard_asked.kind = FUDE_WORDCARD_ASK_NONE;
        fude_wordcard_open_note(_ui, fude_wordcard_asked.kanji);
        return;
    }
    if(fude_wordcard_asked.kind == FUDE_WORDCARD_ASK_LIST) {
        fude_wordcard_asked.kind = FUDE_WORDCARD_ASK_NONE;
        if(fude_wordcard_asked.id == 0u || fude_vocab_list_name(fude_wordcard_asked.id)[0] != 0) {
            fude_wordcard_open_list(_ui, fude_wordcard_asked.id);
        }
        return;
    }
    fude_wordcard_word _word = fude_wordcard_asked.word;
    u32                _id   = 0u;
    if(fude_wordcard_asked.kind == FUDE_WORDCARD_ASK_SAVED) {
        _id = fude_wordcard_asked.id;
    } else if(fude_wordcard_asked.kind == FUDE_WORDCARD_ASK_WORD) {
        _id = fude_vocab_find(_word.written, _word.reading);
    }
    const fude_vocab_word* _saved = _id != 0u ? fude_vocab_get(_id) : NULL;
    if(_saved != NULL) {
        fude_wordcard_copy(&_word, _saved->written, _saved->reading, _saved->meaning);
    } else if(fude_wordcard_asked.kind == FUDE_WORDCARD_ASK_SAVED) {
        fude_wordcard_asked.kind = FUDE_WORDCARD_ASK_NONE;
        return;   // it went since
    }
    _card->found_count = fude_wordcard_asked.found_count;
    memcpy(_card->found, fude_wordcard_asked.found, sizeof(_card->found));
    c8 _title[128];
    if(_saved != NULL) {
        snprintf(_title, sizeof(_title), "%s", fude_text(FUDE_TEXT_WORD_SAVED_TITLE));
    } else if(fude_wordcard_asked.kind == FUDE_WORDCARD_ASK_TYPED && fude_wordcard_asked.kanji != 0u) {
        c8 _kanji[8];
        fude_utf8_put(fude_wordcard_asked.kanji, _kanji);
        FUDE_TEXTF(_title, FUDE_TEXT_WORD_TITLE, FUDE_TS(_kanji));
    } else {
        snprintf(_title, sizeof(_title), "%s", fude_text(FUDE_TEXT_WORD_SAVE_TITLE));
    }
    fude_wordcard_open(_ui, &_word, _saved != NULL ? _id : 0u, _saved != NULL ? _saved->kanji : fude_wordcard_asked.kanji, _title);
    fude_wordcard_asked.kind = FUDE_WORDCARD_ASK_NONE;
}

void fude_wordcard_update(fude_ui* _ui) {
    fude_wordcard*   _card   = FUDE_WORDCARD_OF(_ui);
    if(fude_wordcard_asked.kind != FUDE_WORDCARD_ASK_NONE) {
        fude_wordcard_open_asked(_ui);
    }
    const rde_vec_2F _screen = fude_kit_screen_size((_ui)->window);
    if(_card->open && (_card->_layout || _screen.x != _card->_laid_out.x || _screen.y != _card->_laid_out.y)) {
        fude_wordcard_layout(_ui);
    }
}

// --- saving ------------------------------------------------------------------------------

// The list form's Save: the list renamed, or made.
RDE_INTERNAL void fude_wordcard_save_list(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    c8             _name[FUDE_VOCAB_LIST_NAME];
    fude_wordcard_get_field(_card->fields[FUDE_WORDCARD_WRITTEN], _name, sizeof(_name));
    if(_name[0] == 0) {
        rde_ui_label_set_text(_card->error, fude_text(FUDE_TEXT_VOCAB_LIST_NAME_EMPTY));
        return;
    }
    if(_card->list_id != 0u) {
        fude_vocab_list_rename(_card->list_id, _name);
    } else {
        const u32 _list = fude_vocab_list_add(_name);
        if(_list == 0u) {
            rde_ui_label_set_text(_card->error, fude_text(FUDE_TEXT_VOCAB_LISTS_FULL));
            return;
        }
        fude_wordcard_made_list = _list;
    }
    fude_wordcard_close(_ui);
}

RDE_INTERNAL void fude_wordcard_save(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    if(_card->list_mode) {
        fude_wordcard_save_list(_ui);
        return;
    }
    if(_card->note_mode) {
        c8* _text = rde_ui_text_editor_get_text(_card->note_field, 0, rde_ui_text_editor_get_byte_count(_card->note_field));
        fude_charnote_set(_card->note_cp, _text != NULL ? _text : "");
        if(_text != NULL) {
            rde_ui_text_editor_free_text(_card->note_field, _text);
        }
        fude_wordcard_close(_ui);
        return;
    }
    c8 _written[FUDE_USERWORD_WRITTEN], _reading[FUDE_USERWORD_READING], _meaning[FUDE_USERWORD_MEANING];
    fude_wordcard_fill(_ui);
    fude_wordcard_get_field(_card->fields[FUDE_WORDCARD_WRITTEN], _written, sizeof(_written));
    fude_wordcard_get_field(_card->fields[FUDE_WORDCARD_READING], _reading, sizeof(_reading));
    fude_wordcard_get_field(_card->fields[FUDE_WORDCARD_MEANING], _meaning, sizeof(_meaning));
    if(_reading[0] == 0 && fude_wordcard_all_kana(_written)) {
        snprintf(_reading, sizeof(_reading), "%s", _written);   // a kana word reads as it is written
    }
    b8 _ascii = _reading[0] != 0;
    for(const c8* _c = _reading; *_c != 0; _c++) {
        _ascii = _ascii && (u8)*_c < 0x80u;
    }
    const c8* _error = NULL;
    if(_written[0] == 0) {
        _error = fude_text(FUDE_TEXT_WORD_ERR_EMPTY);
    } else if(_reading[0] == 0) {
        _error = fude_text(FUDE_TEXT_WORD_ERR_READING);
    } else if(_ascii) {
        c8 _kana[FUDE_USERWORD_READING];
        if(fude_romaji_to_hiragana(_reading, _kana, sizeof(_kana))) {
            snprintf(_reading, sizeof(_reading), "%s", _kana);
        } else {
            _error = fude_text(FUDE_TEXT_WORD_ERR_ROMAJI);
        }
    }
    u32 _id = _card->id;
    if(_error == NULL) {
        if(_id != 0u) {
            if(!fude_vocab_update(_id, _written, _reading, _meaning)) {
                _error = fude_text(FUDE_TEXT_WORD_ERR_SAVED);
            }
        } else {
            _id = fude_vocab_add(_written, _reading, _meaning, _card->kanji);
            if(_id == 0u) {
                _error = fude_text(FUDE_TEXT_WORD_ERR_EMPTY);
            }
        }
    }
    if(_error != NULL) {
        rde_ui_label_set_text(_card->error, _error);
        return;
    }

    // Its lists; the ones it went in, for the next new word.
    fude_wordcard_last_count = 0;
    const c8* _first = NULL;
    for(u32 _i = 0; _i < _card->list_count; _i++) {
        fude_vocab_set_in_list(_card->lists[_i], _id, _card->ticked[_i]);
        if(_card->ticked[_i]) {
            fude_wordcard_last_lists[fude_wordcard_last_count++] = _card->lists[_i];
            _first = _first != NULL ? _first : fude_vocab_list_name(_card->lists[_i]);
        }
    }
    c8 _line[256];
    if(_first != NULL) {
        FUDE_TEXTF(_line, FUDE_TEXT_VOCAB_SAVED_IN, FUDE_TS(_written), FUDE_TS(_first));
    } else {
        FUDE_TEXTF(_line, FUDE_TEXT_VOCAB_SAVED, FUDE_TS(_written));
    }
    fude_notice_show(_line);
    fude_wordcard_close(_ui);
}

// A new list, named (empty: "List n"), the word in it.
RDE_INTERNAL void fude_wordcard_make_list(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    c8             _name[FUDE_VOCAB_LIST_NAME];
    fude_wordcard_get_field(_card->list_field, _name, sizeof(_name));
    if(_name[0] == 0) {
        fude_vocab_list_new_name(_name, sizeof(_name));
    }
    const u32 _list = fude_vocab_list_add(_name);
    _card->naming = false;
    if(_list != 0u && _card->list_count < FUDE_VOCAB_LISTS) {
        _card->lists[_card->list_count]  = _list;
        _card->ticked[_card->list_count] = true;
        rde_ui_button_set_text(_card->list_chips[_card->list_count], fude_vocab_list_name(_list));
        _card->list_count++;
    }
    fude_wordcard_style_chips(_ui);
    _card->_layout = true;
}

// --- callbacks ---------------------------------------------------------------------------

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_wordcard_close((fude_ui*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_save(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_wordcard_save((fude_ui*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_remove(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui*          _ui = (fude_ui*)_user_data;
    if(FUDE_WORDCARD_OF(_ui)->note_mode) {
        fude_charnote_set(FUDE_WORDCARD_OF(_ui)->note_cp, NULL);   // Delete note
        fude_wordcard_close(_ui);
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    if(FUDE_WORDCARD_OF(_ui)->list_mode) {
        // Delete list: the list goes, its words stay.
        if(FUDE_WORDCARD_OF(_ui)->list_id != 0u) {
            c8 _line[256];
            FUDE_TEXTF(_line, FUDE_TEXT_VOCAB_LIST_DELETED, FUDE_TS(fude_vocab_list_name(FUDE_WORDCARD_OF(_ui)->list_id)));
            fude_vocab_list_remove(FUDE_WORDCARD_OF(_ui)->list_id);
            fude_notice_show(_line);
        }
        fude_wordcard_close(_ui);
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    const fude_vocab_word* _w       = fude_vocab_get(FUDE_WORDCARD_OF(_ui)->id);
    if(_w != NULL) {
        c8 _line[256];
        FUDE_TEXTF(_line, FUDE_TEXT_VOCAB_REMOVED, FUDE_TS(_w->written));
        fude_vocab_remove(FUDE_WORDCARD_OF(_ui)->id);
        fude_notice_show(_line);
    }
    fude_wordcard_close(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_list(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_wordcard_ref* _ref  = (const fude_wordcard_ref*)_user_data;
    fude_wordcard*           _card = FUDE_WORDCARD_OF(_ref->ui);
    if(_ref->index < _card->list_count) {
        _card->ticked[_ref->index] = !_card->ticked[_ref->index];
        fude_kit_button_chip(_card->list_chips[_ref->index], _card->ticked[_ref->index]);
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A word found in what was read: the fields take it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_found(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_wordcard_ref* _ref  = (const fude_wordcard_ref*)_user_data;
    fude_wordcard*           _card = FUDE_WORDCARD_OF(_ref->ui);
    const u32                _i    = _ref->index - FUDE_VOCAB_LISTS;
    if(_i < _card->found_count) {
        fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_WRITTEN], _card->found[_i].written);
        fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_READING], _card->found[_i].reading);
        fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_MEANING], _card->found[_i].meaning);
        _card->id = fude_vocab_find(_card->found[_i].written, _card->found[_i].reading);   // saved already: that one
        _card->_layout = true;
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_new_list(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui*  _ui = (fude_ui*)_user_data;
    fude_wordcard* _card    = FUDE_WORDCARD_OF(_ui);
    if(_card->list_count >= FUDE_VOCAB_LISTS) {
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    _card->naming = true;
    fude_wordcard_set_field(_card->list_field, "");
    fude_wordcard_layout(_ui);
    rde_ui_node_focus(rde_ui_text_editor_as_node(_card->list_field));
    return RDE_UI_EVENT_RESULT_CONSUME;
}

RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_list_ok(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_wordcard_make_list((fude_ui*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Return in a field: the written one looks the word up and goes on to the
// reading, the reading to the meaning, the meaning saves; the list's name makes it.
RDE_INTERNAL void fude_wordcard_on_submit(rde_ui_node* _node, any _user_data) {
    fude_ui*  _ui = (fude_ui*)_user_data;
    fude_wordcard* _card    = FUDE_WORDCARD_OF(_ui);
    if(_node == rde_ui_text_editor_as_node(_card->list_field)) {
        fude_wordcard_make_list(_ui);
        return;
    }
    if(_card->list_mode) {
        fude_wordcard_save(_ui);
        return;
    }
    if(_node == rde_ui_text_editor_as_node(_card->fields[FUDE_WORDCARD_WRITTEN])) {
        fude_wordcard_fill(_ui);
    }
    for(u32 _i = 0; _i < 2u; _i++) {
        if(_node == rde_ui_text_editor_as_node(_card->fields[_i])) {
            rde_ui_node_focus(rde_ui_text_editor_as_node(_card->fields[_i + 1u]));
            return;
        }
    }
    fude_wordcard_save(_ui);
}

// --- building ----------------------------------------------------------------------------

RDE_INTERNAL rde_ui_label* fude_wordcard_label(fude_ui* _ui, rde_ui_node* _parent, f32 _px) {
    rde_ui_label* _label = rde_ui_label_create(NULL);
    rde_ui_label_set_font(_label, _ui->font);
    rde_ui_label_set_font_scale(_label, _px / (f32)FUDE_KIT_FONT_SIZE);
    rde_ui_label_set_alignment(_label, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
    rde_ui_node_set_raycast_target(rde_ui_label_as_node(_label), false);
    rde_ui_node_add_child(_parent, rde_ui_label_as_node(_label));
    return _label;
}

RDE_INTERNAL rde_ui_text_editor* fude_wordcard_field(fude_ui* _ui, rde_ui_node* _parent, const c8* _placeholder, u32 _max) {
    rde_ui_text_editor* _f = rde_ui_text_editor_create(_ui->font, NULL);
    rde_ui_text_editor_set_multiline(_f, false);
    rde_ui_text_editor_set_font_size(_f, FUDE_WORDCARD_FIELD_PX);
    rde_ui_text_editor_set_max_chars(_f, _max);
    rde_ui_text_editor_set_placeholder(_f, _placeholder);
    rde_ui_text_editor_set_content_insets(_f, 12.0f, 8.0f, 12.0f, 8.0f);
    rde_ui_text_editor_add_plugin(_f, rde_ui_text_editor_plugin_ime_get());   // Japanese from the keyboard
    rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_f), _ui);
    rde_ui_text_editor_set_on_submit(_f, fude_wordcard_on_submit);
    fude_kit_field_box(_parent, _f);
    return _f;
}

void fude_wordcard_create(fude_ui* _ui, rde_ui_node* _root) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    memset(_card, 0, sizeof(*_card));
    // Over everything, with a backdrop that cancels it.
    fude_kit_modal_create(&_card->modal, _root, fude_wordcard_on_cancel, _ui);
    rde_ui_node* _c = rde_ui_image_as_node(_card->modal.card);

    _card->title = fude_wordcard_label(_ui, _c, 17.0f);
    const c8* const _placeholders[3] = { fude_text(FUDE_TEXT_WORD_FIELD_WRITTEN), fude_text(FUDE_TEXT_WORD_FIELD_READING), fude_text(FUDE_TEXT_WORD_FIELD_MEANING) };
    const u32       _max[3]          = { 24u, 32u, 60u };
    for(u32 _i = 0; _i < 3u; _i++) {
        _card->fields[_i] = fude_wordcard_field(_ui, _c, _placeholders[_i], _max[_i]);
    }
    _card->found_caption = fude_wordcard_label(_ui, _c, 12.0f);
    rde_ui_label_set_text(_card->found_caption, fude_text(FUDE_TEXT_WORD_FOUND));
    for(u32 _i = 0; _i < FUDE_WORDCARD_FOUND; _i++) {
        _card->refs[FUDE_VOCAB_LISTS + _i] = (fude_wordcard_ref){ _ui, FUDE_VOCAB_LISTS + _i };
        _card->found_chips[_i]             = fude_kit_button(_c, "", fude_wordcard_on_found, _ui);
        rde_ui_button_set_on_click(_card->found_chips[_i], fude_wordcard_on_found, &_card->refs[FUDE_VOCAB_LISTS + _i]);
    }
    _card->lists_caption = fude_wordcard_label(_ui, _c, 12.0f);
    rde_ui_label_set_text(_card->lists_caption, fude_text(FUDE_TEXT_WORD_LISTS));
    for(u32 _i = 0; _i < FUDE_VOCAB_LISTS; _i++) {
        _card->refs[_i]       = (fude_wordcard_ref){ _ui, _i };
        _card->list_chips[_i] = fude_kit_button(_c, "", fude_wordcard_on_list, _ui);
        rde_ui_button_set_on_click(_card->list_chips[_i], fude_wordcard_on_list, &_card->refs[_i]);
    }
    c8 _label[96];
    snprintf(_label, sizeof(_label), FUDE_ICON_PLUS " %s", fude_text(FUDE_TEXT_WORD_NEW_LIST));
    _card->new_list   = fude_kit_button(_c, _label, fude_wordcard_on_new_list, _ui);
    _card->list_field = fude_wordcard_field(_ui, _c, fude_text(FUDE_TEXT_WORD_LIST_NAME), 30u);
    _card->note_field = fude_wordcard_field(_ui, _c, fude_text(FUDE_TEXT_NOTE_PLACEHOLDER), 180u);
    rde_ui_text_editor_set_multiline(_card->note_field, true);
    _card->list_ok    = fude_kit_button(_c, fude_text(FUDE_TEXT_ADD), fude_wordcard_on_list_ok, _ui);
    _card->error      = fude_wordcard_label(_ui, _c, 12.0f);
    snprintf(_label, sizeof(_label), FUDE_ICON_TRASH " %s", fude_text(FUDE_TEXT_WORD_REMOVE));
    _card->remove     = fude_kit_button(_c, _label, fude_wordcard_on_remove, _ui);
    _card->cancel     = fude_kit_button(_c, fude_text(FUDE_TEXT_CANCEL), fude_wordcard_on_cancel, _ui);
    snprintf(_label, sizeof(_label), FUDE_ICON_CHECK " %s", fude_text(FUDE_TEXT_SAVE));
    _card->save       = fude_kit_button(_c, _label, fude_wordcard_on_save, _ui);
    fude_wordcard_close(_ui);
}

void fude_wordcard_apply_theme(fude_ui* _ui) {
    fude_wordcard*    _card = FUDE_WORDCARD_OF(_ui);
    const fude_theme* _t    = fude_theme_active();
    if(_card->modal.card == NULL) {
        return;
    }
    fude_kit_modal_restyle(&_card->modal, 18.0f);
    for(u32 _i = 0; _i < 3u; _i++) {
        fude_kit_style_field(_card->fields[_i]);
    }
    fude_kit_style_field(_card->list_field);
    fude_kit_style_field(_card->note_field);
    rde_ui_label_set_color(_card->title, _t->button_text);
    rde_ui_label_set_color(_card->found_caption, _t->text_soft);
    rde_ui_label_set_color(_card->lists_caption, _t->text_soft);
    rde_ui_label_set_color(_card->error, _t->score_poor);
    fude_kit_button_plain(_card->cancel);
    fude_kit_button_primary(_card->save);
    fude_kit_button_danger_quiet(_card->remove);
    fude_kit_button_plain(_card->list_ok);
    fude_wordcard_style_chips(_ui);
}
