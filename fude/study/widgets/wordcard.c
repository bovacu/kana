// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"
#include "study/widgets/wordcard.h"
#include "drawing/ink/mlkit.h"
#include "drawing/app/ui.h"
#include "drawing/widgets/notice.h"
#include "drawing/widgets/kit.h"
#include "study/chars/catalog.h"
#include "drawing/base/draw.h"
#include "drawing/base/icons.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "study/chars/wordsplit.h"
#include "study/models/charnote.h"
#include "study/app/study.h"
#include "study/services/translate.h"
#include "drawing/base/utf8.h"

// ===========================================================================
// See wordcard.h. The card is built once with the rest of the UI (and again when the language changes). A chip for
// every found word (FUDE_WORDCARD_FOUND) is made up front and shown as needed. The lists have no limit, so their
// chips are one a list, made when the card opens or a list is made and the number of lists has changed
// (fude_wordcard_list_chips). Each open lays the card out from the top in two passes: the first measures how tall it
// is, the second places everything in a card that tall. Sizes are in the UI's screen units.
// ===========================================================================

// The card is the study's (study.h); the UI it is on is the core's.
#define FUDE_WORDCARD_OF(_ui) (&FUDE_STUDY((_ui)->app)->word)

#define FUDE_WORDCARD_WIDTH    560.0f   // The card's width at most; on a narrower screen, what fits.
#define FUDE_WORDCARD_MARGIN   22.0f
#define FUDE_WORDCARD_FIELD_H  44.0f
#define FUDE_WORDCARD_CHIP_H   34.0f
#define FUDE_WORDCARD_GAP      8.0f
#define FUDE_WORDCARD_CAPTION  20.0f    // A caption's height.
#define FUDE_WORDCARD_FIELD_PX 14u      // The fields' text size.
#define FUDE_WORDCARD_SENTENCE_LINES 3u   // Lines a sentence (or its translation) takes at most; past that it shrinks.

// How far Translate with Google has got in turning the meaning into the language taught.
typedef enum {
    FUDE_WORDCARD_TRANSLATE_NONE = 0,   // Not asked: the status shows the hint.
    FUDE_WORDCARD_TRANSLATE_WAITING,    // The models are asked for, or on their way.
    FUDE_WORDCARD_TRANSLATE_ASKED,      // The translator has the meaning, under ticket.
    FUDE_WORDCARD_TRANSLATE_DONE,       // In: the written field took it, and Google's badge shows.
    FUDE_WORDCARD_TRANSLATE_FAILED      // The status says why.
} FUDE_WORDCARD_TRANSLATE_;

// The three fields of a word, by index into fields.
enum { FUDE_WORDCARD_WRITTEN = 0, FUDE_WORDCARD_READING, FUDE_WORDCARD_MEANING };

// What was asked for: one for each fude_wordcard_ask call.
typedef enum {
    FUDE_WORDCARD_ASK_NONE = 0,
    FUDE_WORDCARD_ASK_WORD,
    FUDE_WORDCARD_ASK_SAVED,
    FUDE_WORDCARD_ASK_TYPED,
    FUDE_WORDCARD_ASK_READ,
    FUDE_WORDCARD_ASK_LIST,
    FUDE_WORDCARD_ASK_NOTE
} FUDE_WORDCARD_ASK_;

// What was asked for, until the UI's next update opens it. A static, so it survives the UI being built again.
static struct {
    u8                 kind;
    fude_wordcard_word word;
    u32                kanji;
    u32                id;
    fude_wordcard_word found[FUDE_WORDCARD_FOUND];
    u32                found_count;
    c8                 sentence[FUDE_VOCAB_SENTENCE];
    c8                 sentence_to[FUDE_VOCAB_SENTENCE];
} fude_wordcard_asked;

// The ids of the lists the last word saved went in: a new word comes with them ticked. Made on first use
// (fude_wordcard_last), on the std allocator, it lasts as long as the app, like the other statics here.
static rde_arr TYPE(u32) fude_wordcard_last_lists;
static u32 fude_wordcard_made_list  = 0;   // A list the list form made, for fude_wordcard_take_new_list.

// Returns the lists the last word saved went in (fude_wordcard_last_lists), making the array the first time.
RDE_INTERNAL rde_arr* fude_wordcard_last(void) {
    if(!rde_arr_is_inited(&fude_wordcard_last_lists)) {
        fude_wordcard_last_lists = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    }
    return &fude_wordcard_last_lists;
}

// See the header.
void fude_wordcard_ask_list(u32 _list) {
    rde_memset(&fude_wordcard_asked, 0, sizeof(fude_wordcard_asked));
    fude_wordcard_asked.kind = FUDE_WORDCARD_ASK_LIST;
    fude_wordcard_asked.id   = _list;
}

// See the header.
void fude_wordcard_ask_note(u32 _codepoint) {
    rde_memset(&fude_wordcard_asked, 0, sizeof(fude_wordcard_asked));
    fude_wordcard_asked.kind  = FUDE_WORDCARD_ASK_NOTE;
    fude_wordcard_asked.kanji = _codepoint;
}

// See the header.
u32 fude_wordcard_take_new_list(void) {
    const u32 _list = fude_wordcard_made_list;
    fude_wordcard_made_list = 0;
    return _list;
}

// See the header. It replaces the lists the last word went in.
void fude_wordcard_prefer_list(u32 _list) {
    rde_arr_clear(fude_wordcard_last());
    if(_list != 0u) {
        rde_arr_add(fude_wordcard_last(), (any)&_list);
    }
}

// Copies a word's three texts into _w, cut to fit; NULL counts as empty.
RDE_INTERNAL void fude_wordcard_copy(fude_wordcard_word* _w, const c8* _written, const c8* _reading, const c8* _meaning) {
    rde_snprintf(_w->written, sizeof(_w->written), "%s", _written != NULL ? _written : "");
    rde_snprintf(_w->reading, sizeof(_w->reading), "%s", _reading != NULL ? _reading : "");
    rde_snprintf(_w->meaning, sizeof(_w->meaning), "%s", _meaning != NULL ? _meaning : "");
}

// See the header. Whether it is saved already is looked up when the card opens.
void fude_wordcard_ask(const c8* _written, const c8* _reading, const c8* _meaning, u32 _kanji) {
    rde_memset(&fude_wordcard_asked, 0, sizeof(fude_wordcard_asked));
    fude_wordcard_asked.kind  = FUDE_WORDCARD_ASK_WORD;
    fude_wordcard_asked.kanji = _kanji;
    fude_wordcard_copy(&fude_wordcard_asked.word, _written, _reading, _meaning);
}

// See the header. The sentence and its translation are cut at a whole character to fit.
void fude_wordcard_ask_in(const c8* _written, const c8* _reading, const c8* _meaning, const c8* _sentence, const c8* _translation) {
    fude_wordcard_ask(_written, _reading, _meaning, 0u);
    fude_utf8_set(fude_wordcard_asked.sentence, sizeof(fude_wordcard_asked.sentence), _sentence);
    fude_utf8_set(fude_wordcard_asked.sentence_to, sizeof(fude_wordcard_asked.sentence_to), _translation);
}

// See the header.
void fude_wordcard_ask_saved(u32 _id) {
    rde_memset(&fude_wordcard_asked, 0, sizeof(fude_wordcard_asked));
    fude_wordcard_asked.kind = FUDE_WORDCARD_ASK_SAVED;
    fude_wordcard_asked.id   = _id;
}

// See the header.
void fude_wordcard_ask_typed(u32 _kanji) {
    rde_memset(&fude_wordcard_asked, 0, sizeof(fude_wordcard_asked));
    fude_wordcard_asked.kind  = FUDE_WORDCARD_ASK_TYPED;
    fude_wordcard_asked.kanji = _kanji;
}

// See the header. The text is split into the dictionary's words (wordsplit.h), at most FUDE_WORDCARD_FOUND of them.
void fude_wordcard_ask_read(const fude_kanji_db* _db, const c8* _text) {
    rde_memset(&fude_wordcard_asked, 0, sizeof(fude_wordcard_asked));
    fude_wordcard_asked.kind = FUDE_WORDCARD_ASK_READ;
    // The text on one line, whole characters while they fit in the written field, without the spaces around it.
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
        rde_memcpy(&_line[_n], _from, _len);
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
        fude_wordcard_asked.word        = fude_wordcard_asked.found[0];   // One word: the card shows that one.
        fude_wordcard_asked.found_count = 0;
    } else {
        fude_wordcard_copy(&fude_wordcard_asked.word, _line, "", "");
    }
}

// --- fields ------------------------------------------------------------------------------

// Replaces a field's text with _text (NULL or "" empties it).
RDE_INTERNAL void fude_wordcard_set_field(rde_ui_text_editor* _field, const c8* _text) {
    const usize _bytes = rde_ui_text_editor_get_byte_count(_field);
    if(_bytes > 0) {
        rde_ui_text_editor_delete_range(_field, 0, _bytes);
    }
    if(_text != NULL && _text[0] != 0) {
        rde_ui_text_editor_insert_at(_field, 0, _text, rde_strlen(_text));
    }
}

// Whether the text typed in _field fits in _size bytes, its NUL included. The field takes FUDE_USERWORD_CHAR_BYTES
// a character (vocab.h), but a character can be longer, and such a text is refused rather than cut.
RDE_INTERNAL b8 fude_wordcard_fits(rde_ui_text_editor* _field, usize _size) {
    return rde_ui_text_editor_get_byte_count(_field) < _size;
}

// The reading of a word in a language read in Latin letters (lang.h: fude_lang_latin): each letter's Latin name,
// joined, into _out of _size bytes. A character with none (a space, a digit) is kept as it is.
RDE_INTERNAL void fude_wordcard_latin_names(const c8* _written, c8* _out, usize _size) {
    usize     _n = 0;
    const c8* _p = _written;
    _out[0] = 0;
    for(u32 _cp = fude_utf8_next(&_p); _cp != 0u; _cp = fude_utf8_next(&_p)) {
        const c8* _latin = fude_lang_latin(_cp);
        c8        _one[5];
        if(_latin == NULL) {
            fude_utf8_put(_cp, _one);
            _latin = _one;
        }
        const usize _len = rde_strlen(_latin);
        if(_n + _len >= _size) {
            break;
        }
        rde_memcpy(_out + _n, _latin, _len);
        _n += _len;
        _out[_n] = 0;
    }
}

// The dictionary's word written exactly as _written, the commonest of those spelt so, into *_out. False when the
// dictionary has none (Japanese's, for one, has only words with a kanji in them).
RDE_INTERNAL b8 fude_wordcard_lookup(const fude_ui* _ui, const c8* _written, fude_kanji_word* _out) {
    const fude_study*    _study = FUDE_STUDY(_ui->app);
    const fude_kanji_db* _db    = _study->db;
    const u32            _word  = _db != NULL ? fude_wordsplit_word(_db, _written) : UINT32_MAX;
    return _word != UINT32_MAX && fude_kanji_word_at(_db, _word, _out) && rde_strcmp(_out->written, _written) == 0;
}

// Fills the reading and the meaning, when they are empty, from the dictionary's word for the written field.
RDE_INTERNAL void fude_wordcard_fill(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    c8 _written[FUDE_USERWORD_WRITTEN], _reading[FUDE_USERWORD_READING], _meaning[FUDE_USERWORD_MEANING];
    fude_kit_field_text_trimmed(_card->fields[FUDE_WORDCARD_WRITTEN], _written, sizeof(_written));
    fude_kit_field_text_trimmed(_card->fields[FUDE_WORDCARD_READING], _reading, sizeof(_reading));
    fude_kit_field_text_trimmed(_card->fields[FUDE_WORDCARD_MEANING], _meaning, sizeof(_meaning));
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

// --- the lists' chips --------------------------------------------------------------------

// Returns how many lists the card shows (a chip each).
RDE_INTERNAL u32 fude_wordcard_list_count(const fude_wordcard* _card) {
    return rde_arr_is_inited(&_card->lists) ? (u32)rde_arr_length(&_card->lists) : 0u;
}

// Returns the _index-th list the card shows; _index must be under fude_wordcard_list_count. The pointer is valid until
// the chips are next made (fude_wordcard_list_chips).
RDE_INTERNAL fude_wordcard_list* fude_wordcard_list_of(fude_wordcard* _card, u32 _index) {
    return &((fude_wordcard_list*)_card->lists.memory)[_index];
}

// Declared here so that fude_wordcard_list_chips can give it to the chips it makes: a list's chip, tapped.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_list(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data);

// Makes the card show _count lists, a chip each: when there were more, the chips past _count go; when there were
// fewer, new chips (empty, unticked) are made for the rest, on the card. The chips already there are kept, with their
// lists and ticks, so the caller sets only what is new.
RDE_INTERNAL void fude_wordcard_list_chips(fude_ui* _ui, u32 _count) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    const u32      _had  = fude_wordcard_list_count(_card);
    for(u32 _i = _count; _i < _had; _i++) {
        rde_ui_button_destroy(fude_wordcard_list_of(_card, _i)->chip);
    }
    if(_count == _had) {
        return;
    }
    rde_arr_resize(&_card->lists, _count);   // New lists come zeroed: no list, unticked, no chip yet.
    for(u32 _i = _had; _i < _count; _i++) {
        fude_wordcard_list_of(_card, _i)->chip = fude_kit_button(rde_ui_image_as_node(_card->modal.card), "", fude_wordcard_on_list, _ui);
    }
}

// --- the chips' looks --------------------------------------------------------------------

// Styles the chips: a list's chip shows whether it is ticked.
RDE_INTERNAL void fude_wordcard_style_chips(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    for(u32 _i = 0; _i < fude_wordcard_list_count(_card); _i++) {
        fude_kit_button_chip(fude_wordcard_list_of(_card, _i)->chip, fude_wordcard_list_of(_card, _i)->ticked);
    }
    for(u32 _i = 0; _i < _card->found_count; _i++) {
        fude_kit_button_chip(_card->found_chips[_i], false);
    }
    fude_kit_button_chip(_card->new_list, false);
    fude_kit_button_chip(_card->translate, false);
}

// Sets the found words' chips' texts: the word and its reading.
RDE_INTERNAL void fude_wordcard_label_found(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    for(u32 _i = 0; _i < _card->found_count && _i < FUDE_WORDCARD_FOUND; _i++) {
        c8 _label[FUDE_USERWORD_WRITTEN + FUDE_USERWORD_READING + 8];
        rde_snprintf(_label, sizeof(_label), "%s  %s", _card->found[_i].written, _card->found[_i].reading);
        rde_ui_button_set_text(_card->found_chips[_i], _label);
    }
}

// Gives Google's badge the image for the card's colour: the white one on a dark panel (luma under 128).
RDE_INTERNAL void fude_wordcard_style_badge(fude_ui* _ui) {
    fude_wordcard*  _card = FUDE_WORDCARD_OF(_ui);
    const rde_color _p    = fude_theme_active()->panel;
    rde_ui_style    _s    = fude_kit_style((rde_color){ 255, 255, 255, 255 }, 0.0f);
    _s.texture            = fude_translate_badge(0.299f * (f32)_p.r + 0.587f * (f32)_p.g + 0.114f * (f32)_p.b < 128.0f);
    if(_s.texture != NULL) {
        rde_ui_image_set_style(_card->badge, RDE_UI_STATE_NORMAL, _s);
    }
}

// --- layout ------------------------------------------------------------------------------

// A chip's width for _text at 13 units, with 32 units of room, and no wider than the card's inside.
RDE_INTERNAL f32 fude_wordcard_chip_w(const fude_ui* _ui, const c8* _text) {
    return rde_math_min_f32(FUDE_WORDCARD_WIDTH - 2.0f * FUDE_WORDCARD_MARGIN, fude_draw_text_width(_ui->font, (f32)FUDE_KIT_FONT_SIZE, _text, 13.0f) + 32.0f);
}

// Lays _n chips out in rows from _top (card units, Y up) between _x0 and _x1, placing them only when _place, and
// returns how tall the rows are.
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

// A y in card units (Y up) for _dy down from the card's top.
//
// fude_wordcard_layout lays the card out in two passes, and the steps below take the pass's numbers: _w is the card's
// width, _x0 and _x1 the left and right edges of its inside, _h its height (0 on the first pass, which only measures;
// this macro reads it), and _y how far down from the card's top the pass has got. A step places things only when
// _place, and returns _y below what it laid out.
#define FUDE_WORDCARD_TOP(_dy) (_h - (_dy))

// Lays out Translate with Google, and beside it its status or Google's badge. Takes the pass's numbers (see
// FUDE_WORDCARD_TOP), and returns _y below them.
RDE_INTERNAL f32 fude_wordcard_layout_translate(const fude_ui* _ui, const fude_wordcard* _card, f32 _x0, f32 _x1, f32 _h, f32 _y, b8 _place) {
    if(_place) {
        const c8* _text = _card->translate->internal_label != NULL && _card->translate->internal_label->text != NULL ? _card->translate->internal_label->text : "";
        const f32 _bw   = fude_wordcard_chip_w(_ui, _text);
        const f32 _rest = rde_math_max_f32(0.0f, _x1 - _x0 - _bw - FUDE_WORDCARD_GAP - 4.0f);
        const f32 _ry   = FUDE_WORDCARD_TOP(_y + FUDE_WORDCARD_CHIP_H * 0.5f);
        fude_kit_place(rde_ui_button_as_node(_card->translate), (rde_vec_2F){ _x0 + _bw * 0.5f, _ry }, (rde_vec_2F){ _bw, FUDE_WORDCARD_CHIP_H });
        fude_kit_place(rde_ui_label_as_node(_card->translate_status), (rde_vec_2F){ _x1 - _rest * 0.5f, _ry }, (rde_vec_2F){ _rest, FUDE_WORDCARD_CHIP_H });
        const f32 _badge = rde_math_min_f32(FUDE_TRANSLATE_BADGE_W, _rest);
        fude_kit_place(rde_ui_image_as_node(_card->badge), (rde_vec_2F){ _x1 - _rest + _badge * 0.5f, _ry },
                       (rde_vec_2F){ _badge, FUDE_TRANSLATE_BADGE_H * _badge / FUDE_TRANSLATE_BADGE_W });
    }
    _y += FUDE_WORDCARD_CHIP_H + FUDE_WORDCARD_GAP;
    return _y;
}

// Lays out the sentence the word was met in, under its caption, and its translation, each as many lines as it takes,
// up to FUDE_WORDCARD_SENTENCE_LINES. Takes the pass's numbers (see FUDE_WORDCARD_TOP), and returns _y below them.
RDE_INTERNAL f32 fude_wordcard_layout_sentence(const fude_ui* _ui, const fude_wordcard* _card, f32 _w, f32 _x0, f32 _x1, f32 _h, f32 _y, b8 _place) {
    _y += 4.0f;
    if(_place) {
        fude_kit_place(rde_ui_label_as_node(_card->sentence_caption), (rde_vec_2F){ _w * 0.5f, FUDE_WORDCARD_TOP(_y + FUDE_WORDCARD_CAPTION * 0.5f) },
                       (rde_vec_2F){ _x1 - _x0, FUDE_WORDCARD_CAPTION });
    }
    _y += FUDE_WORDCARD_CAPTION;
    rde_ui_label* const _labels[2] = { _card->sentence_label, _card->sentence_to_label };
    const c8* const     _texts[2]  = { _card->sentence, _card->sentence_to };
    const f32           _px[2]     = { 15.0f, 13.0f };
    for(u32 _k = 0; _k < 2u; _k++) {
        if(_texts[_k][0] == 0) {
            continue;
        }
        u32 _lines = fude_draw_text_wrap_lines(_ui->font, (f32)FUDE_KIT_FONT_SIZE, _texts[_k], _px[_k], _x1 - _x0);
        _lines     = _lines < 1u ? 1u : _lines > FUDE_WORDCARD_SENTENCE_LINES ? FUDE_WORDCARD_SENTENCE_LINES : _lines;
        const f32 _lh = (f32)_lines * _px[_k] * 1.4f + 4.0f;
        if(_place) {
            fude_kit_place(rde_ui_label_as_node(_labels[_k]), (rde_vec_2F){ _w * 0.5f, FUDE_WORDCARD_TOP(_y + _lh * 0.5f) }, (rde_vec_2F){ _x1 - _x0, _lh });
        }
        _y += _lh;
    }
    _y += FUDE_WORDCARD_GAP;
    return _y;
}

// Lays out the chips under their captions: the found words' (when there are several) and, in the word form, the
// lists' with New list (_lists, in their order); then, while a new list is being named, its name field and OK. Takes
// the pass's numbers (see FUDE_WORDCARD_TOP), and returns _y below them.
RDE_INTERNAL f32 fude_wordcard_layout_chips(const fude_ui* _ui, const fude_wordcard* _card, const rde_arr* _lists, f32 _w, f32 _x0, f32 _x1, f32 _h, f32 _y,
                                            b8 _place) {
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
        _y += fude_wordcard_chips(_ui, (rde_ui_button* const*)_lists->memory, (u32)rde_arr_length(_lists), _x0, _x1, FUDE_WORDCARD_TOP(_y), _place) + FUDE_WORDCARD_GAP;
    }
    if(_card->naming && !_card->list_mode && !_card->note_mode) {   // the word form's, as fude_wordcard_show_parts shows it
        if(_place) {
            const f32 _ok = 96.0f;
            fude_kit_place(fude_kit_field_node(_card->list_field), (rde_vec_2F){ _x0 + (_x1 - _x0 - _ok - FUDE_WORDCARD_GAP) * 0.5f, FUDE_WORDCARD_TOP(_y + FUDE_WORDCARD_FIELD_H * 0.5f) },
                               (rde_vec_2F){ _x1 - _x0 - _ok - FUDE_WORDCARD_GAP, FUDE_WORDCARD_FIELD_H });
            fude_kit_place(rde_ui_button_as_node(_card->list_ok), (rde_vec_2F){ _x1 - _ok * 0.5f, FUDE_WORDCARD_TOP(_y + FUDE_WORDCARD_FIELD_H * 0.5f) },
                               (rde_vec_2F){ _ok, FUDE_WORDCARD_FIELD_H - 4.0f });
        }
        _y += FUDE_WORDCARD_FIELD_H + FUDE_WORDCARD_GAP;
    }
    return _y;
}

// Shows the parts of the card that the form it is in has, and hides the rest. _list_count is how many lists' chips
// there are; _word is whether it is the word form (not the list or the note form), and _translate and _sentence
// whether Translate with Google and the sentence show.
RDE_INTERNAL void fude_wordcard_show_parts(fude_wordcard* _card, u32 _list_count, b8 _word, b8 _translate, b8 _sentence) {
    rde_ui_node_set_active(rde_ui_button_as_node(_card->translate), _translate);
    rde_ui_node_set_active(rde_ui_label_as_node(_card->translate_status), _translate && _card->translating != FUDE_WORDCARD_TRANSLATE_DONE);
    rde_ui_node_set_active(rde_ui_image_as_node(_card->badge), _translate && _card->translating == FUDE_WORDCARD_TRANSLATE_DONE);
    rde_ui_node_set_active(rde_ui_label_as_node(_card->sentence_caption), _sentence);
    rde_ui_node_set_active(rde_ui_label_as_node(_card->sentence_label), _sentence && _card->sentence[0] != 0);
    rde_ui_node_set_active(rde_ui_label_as_node(_card->sentence_to_label), _sentence && _card->sentence_to[0] != 0);
    rde_ui_node_set_active(fude_kit_field_node(_card->fields[FUDE_WORDCARD_WRITTEN]), !_card->note_mode);
    rde_ui_node_set_active(fude_kit_field_node(_card->note_field), _card->note_mode);
    for(u32 _i = 0; _i < FUDE_WORDCARD_FOUND; _i++) {
        rde_ui_node_set_active(rde_ui_button_as_node(_card->found_chips[_i]), _word && _card->found_count > 1u && _i < _card->found_count);
    }
    rde_ui_node_set_active(rde_ui_label_as_node(_card->found_caption), _word && _card->found_count > 1u);
    for(u32 _i = 0; _i < _list_count; _i++) {
        rde_ui_node_set_active(rde_ui_button_as_node(fude_wordcard_list_of(_card, _i)->chip), _word);
    }
    rde_ui_node_set_active(rde_ui_label_as_node(_card->lists_caption), _word);
    rde_ui_node_set_active(fude_kit_field_node(_card->fields[FUDE_WORDCARD_READING]), _word);
    rde_ui_node_set_active(fude_kit_field_node(_card->fields[FUDE_WORDCARD_MEANING]), _word);
    rde_ui_node_set_active(rde_ui_button_as_node(_card->new_list), _word && !_card->naming);
    rde_ui_node_set_active(fude_kit_field_node(_card->list_field), _word && _card->naming);
    rde_ui_node_set_active(rde_ui_button_as_node(_card->list_ok), _word && _card->naming);
    rde_ui_node_set_active(rde_ui_button_as_node(_card->remove), _word ? _card->id != 0u : _card->list_mode ? _card->list_id != 0u : fude_charnote_get(_card->note_cp)[0] != 0);
}

// Lays the card out for what it shows now: everything placed from the top, the card as tall as that, and what is not
// shown hidden.
RDE_INTERNAL void fude_wordcard_layout(fude_ui* _ui) {
    fude_wordcard*   _card   = FUDE_WORDCARD_OF(_ui);
    const rde_vec_2F _screen = fude_kit_screen_size((_ui)->window);
    _card->_layout   = false;
    _card->_laid_out = _screen;
    const f32 _m     = FUDE_WORDCARD_MARGIN;
    const f32 _w     = rde_math_min_f32(FUDE_WORDCARD_WIDTH, _screen.x - 32.0f);
    const f32 _x0    = _m;
    const f32 _x1    = _w - _m;
    // The lists' chips, then New list, laid out in rows together. While naming, the field replaces New list.
    const u32 _list_count = fude_wordcard_list_count(_card);
    rde_arr TYPE(rde_ui_button*) _lists = rde_arr_new_with_capacity(sizeof(rde_ui_button*), _list_count + 1u, rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < _list_count; _i++) {
        rde_arr_add(&_lists, (any)&fude_wordcard_list_of(_card, _i)->chip);
    }
    if(!_card->naming) {
        rde_arr_add(&_lists, (any)&_card->new_list);
    }
    const b8  _word     = !_card->list_mode && !_card->note_mode;
    const b8  _translate = _word && fude_translate_available();
    const b8  _sentence  = _word && (_card->sentence[0] != 0 || _card->sentence_to[0] != 0);

    // Measured on the first pass, then placed from the top of a card that tall on the second.
    f32 _measured = 0.0f;
    for(u32 _pass = 0; _pass < 2u; _pass++) {
        const b8 _place = _pass == 1u;
        f32      _h     = 0.0f;
        f32      _y     = 0.0f;   // Down from the card's top.
        if(_place) {
            // High on the screen, since the keyboard comes up under it.
            const rde_vec_4I _insets = rde_window_get_safe_area_insets(_ui->window);   // Left, top, right, bottom.
            _h                       = _measured;
            const f32 _cy            = rde_math_max_f32(_h * 0.5f + 16.0f, _screen.y - (f32)_insets.y - 32.0f - _h * 0.5f);
            fude_kit_modal_place(&_card->modal, _ui->window, (rde_vec_2F){ _screen.x * 0.5f, _cy }, (rde_vec_2F){ _w, _h });
        }
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
        if(_translate) {
            // Translate with Google, with its status, or Google's badge, beside it.
            _y = fude_wordcard_layout_translate(_ui, _card, _x0, _x1, _h, _y, _place);
        }
        if(_sentence) {
            // The sentence it was met in, and its translation, as many lines as they take, up to a few.
            _y = fude_wordcard_layout_sentence(_ui, _card, _w, _x0, _x1, _h, _y, _place);
        }
        _y = fude_wordcard_layout_chips(_ui, _card, &_lists, _w, _x0, _x1, _h, _y, _place);
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
        _measured = _y;
    }
    rde_arr_free(&_lists);

    // What shows, for the form the card is in.
    fude_wordcard_show_parts(_card, _list_count, _word, _translate, _sentence);
}
#undef FUDE_WORDCARD_TOP

// --- opening and closing -----------------------------------------------------------------

// See the header.
void fude_wordcard_close(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    _card->open   = false;
    _card->naming = false;
    fude_kit_modal_show(&_card->modal, false);
}

// Opens the card on _word, titled _title. A saved word (_id not 0) has its lists ticked as they are; a new word has
// the lists ticked that the last word saved went in. _kanji is the character page it comes from (0: none).
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
    rde_ui_text_editor_set_max_chars(_card->fields[FUDE_WORDCARD_WRITTEN], FUDE_USERWORD_WRITTEN_CHARS);
    c8 _remove[96];
    fude_draw_icon_label(_remove, sizeof(_remove), FUDE_ICON_TRASH, fude_text(FUDE_TEXT_WORD_REMOVE));
    rde_ui_button_set_text(_card->remove, _remove);
    fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_WRITTEN], _word->written);
    fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_READING], _word->reading);
    fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_MEANING], _word->meaning);
    rde_ui_label_set_text(_card->error, "");
    rde_ui_label_set_text(_card->sentence_label, _card->sentence);
    rde_ui_label_set_text(_card->sentence_to_label, _card->sentence_to);
    _card->translating        = FUDE_WORDCARD_TRANSLATE_NONE;
    _card->translate_says     = FUDE_TEXT_WORD_TRANSLATE_HINT;
    _card->translate_prepared = false;
    _card->ticket             = 0u;
    rde_ui_label_set_text(_card->translate_status, fude_text(FUDE_TEXT_WORD_TRANSLATE_HINT));

    // A chip for every list there is now, each ticked as above.
    fude_wordcard_list_chips(_ui, fude_vocab_list_count());
    const rde_arr* _last_lists = fude_wordcard_last();
    for(u32 _i = 0; _i < fude_wordcard_list_count(_card); _i++) {
        fude_wordcard_list* _l = fude_wordcard_list_of(_card, _i);
        _l->list = fude_vocab_list_at(_i);
        b8 _last = false;
        for(u32 _k = 0; _k < (u32)rde_arr_length(_last_lists) && _id == 0u; _k++) {
            _last = _last || ((const u32*)_last_lists->memory)[_k] == _l->list;
        }
        _l->ticked = _id != 0u ? fude_vocab_in_list(_l->list, _id) : _last;
        rde_ui_button_set_text(_l->chip, fude_vocab_list_name(_l->list));
    }
    fude_wordcard_label_found(_ui);
    fude_wordcard_style_chips(_ui);
    _card->open = true;
    fude_wordcard_layout(_ui);
    fude_kit_modal_show(&_card->modal, true);
    if(_word->written[0] == 0) {
        rde_ui_node_focus(rde_ui_text_editor_as_node(_card->fields[FUDE_WORDCARD_WRITTEN]));   // The keyboard comes up.
    }
}

// Opens the list form: list _list's name to change (0: a new list's, given a name to start from), and Delete list.
RDE_INTERNAL void fude_wordcard_open_list(fude_ui* _ui, u32 _list) {
    fude_wordcard*     _card = FUDE_WORDCARD_OF(_ui);
    fude_wordcard_word _none = { 0 };
    _card->sentence[0]    = 0;
    _card->sentence_to[0] = 0;
    fude_wordcard_open(_ui, &_none, 0u, 0u, fude_text(_list != 0u ? FUDE_TEXT_VOCAB_RENAME_LIST : FUDE_TEXT_WORD_NEW_LIST));
    _card->list_mode = true;
    _card->list_id   = _list;
    c8 _name[FUDE_VOCAB_LIST_NAME];
    if(_list != 0u) {
        rde_snprintf(_name, sizeof(_name), "%s", fude_vocab_list_name(_list));
    } else {
        fude_vocab_list_new_name(_name, sizeof(_name));
    }
    rde_ui_text_editor_set_placeholder(_card->fields[FUDE_WORDCARD_WRITTEN], fude_text(FUDE_TEXT_WORD_LIST_NAME));
    rde_ui_text_editor_set_max_chars(_card->fields[FUDE_WORDCARD_WRITTEN], FUDE_VOCAB_LIST_NAME_CHARS);   // the written field takes the name
    fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_WRITTEN], _name);
    rde_ui_text_editor_select_all(_card->fields[FUDE_WORDCARD_WRITTEN]);
    c8 _delete[96];
    fude_draw_icon_label(_delete, sizeof(_delete), FUDE_ICON_TRASH, fude_text(FUDE_TEXT_VOCAB_DELETE_LIST));
    rde_ui_button_set_text(_card->remove, _delete);
    fude_wordcard_layout(_ui);
    rde_ui_node_focus(rde_ui_text_editor_as_node(_card->fields[FUDE_WORDCARD_WRITTEN]));   // The keyboard comes up.
}

// Opens the note form: character _cp's note to write, and Delete note.
RDE_INTERNAL void fude_wordcard_open_note(fude_ui* _ui, u32 _cp) {
    fude_wordcard*     _card = FUDE_WORDCARD_OF(_ui);
    fude_wordcard_word _none = { 0 };
    c8 _ch[8], _title[96];
    fude_utf8_put(_cp, _ch);
    FUDE_TEXTF(_title, FUDE_TEXT_NOTE_TITLE, FUDE_TS(_ch));
    _card->found_count    = 0;
    _card->sentence[0]    = 0;
    _card->sentence_to[0] = 0;
    fude_wordcard_open(_ui, &_none, 0u, 0u, _title);
    _card->note_mode = true;
    _card->note_cp   = _cp;
    fude_wordcard_set_field(_card->note_field, fude_charnote_get(_cp));
    c8 _delete[96];
    fude_draw_icon_label(_delete, sizeof(_delete), FUDE_ICON_TRASH, fude_text(FUDE_TEXT_NOTE_DELETE));
    rde_ui_button_set_text(_card->remove, _delete);
    fude_wordcard_layout(_ui);
    rde_ui_node_focus(rde_ui_text_editor_as_node(_card->note_field));   // The keyboard comes up.
}

// Opens what was asked for, in the form it needs. A word asked for by its texts that is saved already opens as saved.
// A saved word or a list that has gone since opens nothing.
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
        return;   // It was removed since.
    }
    _card->found_count = fude_wordcard_asked.found_count;
    rde_memcpy(_card->found, fude_wordcard_asked.found, sizeof(_card->found));
    // Its sentence: a saved word's own, else the one it was met in now.
    const c8* _to   = "";
    const c8* _sent = _saved != NULL ? fude_vocab_sentence(_id, &_to) : "";
    if(_sent[0] == 0 && _to[0] == 0) {
        _sent = fude_wordcard_asked.sentence;
        _to   = fude_wordcard_asked.sentence_to;
    }
    fude_utf8_set(_card->sentence, sizeof(_card->sentence), _sent);
    fude_utf8_set(_card->sentence_to, sizeof(_card->sentence_to), _to);
    c8 _title[512];
    if(_saved != NULL) {
        rde_snprintf(_title, sizeof(_title), "%s", fude_text(FUDE_TEXT_WORD_SAVED_TITLE));
    } else if(fude_wordcard_asked.kind == FUDE_WORDCARD_ASK_TYPED && fude_wordcard_asked.kanji != 0u) {
        c8 _kanji[8];
        fude_utf8_put(fude_wordcard_asked.kanji, _kanji);
        FUDE_TEXTF(_title, FUDE_TEXT_WORD_TITLE, FUDE_TS(_kanji));
    } else {
        rde_snprintf(_title, sizeof(_title), "%s", fude_text(FUDE_TEXT_WORD_SAVE_TITLE));
    }
    fude_wordcard_open(_ui, &_word, _saved != NULL ? _id : 0u, _saved != NULL ? _saved->kanji : fude_wordcard_asked.kanji, _title);
    fude_wordcard_asked.kind = FUDE_WORDCARD_ASK_NONE;
}

// --- the meaning into the language taught -----------------------------------------------

// Sets how far the translation has got (_state, FUDE_WORDCARD_TRANSLATE_) and the status's text (_says, a FUDE_TEXT_
// id; FUDE_TEXT_COUNT for none). The card is laid out again when either changes.
RDE_INTERNAL void fude_wordcard_translating(fude_ui* _ui, u8 _state, u32 _says) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    if(_card->translating == _state && _card->translate_says == _says) {
        return;
    }
    _card->translating    = _state;
    _card->translate_says = _says;
    rde_ui_label_set_text(_card->translate_status, _says < FUDE_TEXT_COUNT ? fude_text(_says) : "");
    _card->_layout = true;   // The status, or the badge, now shows.
}

// When _s ends in _end (UTF-8), cuts it off and returns true.
RDE_INTERNAL b8 fude_wordcard_cut_end(c8* _s, const c8* _end) {
    const usize _n = rde_strlen(_s), _k = rde_strlen(_end);
    if(_n >= _k && rde_memcmp(&_s[_n - _k], _end, _k) == 0) {
        _s[_n - _k] = 0;
        return true;
    }
    return false;
}

// The translation of the meaning is in: the written field takes it. When it is one word of the dictionary, the field
// takes that word as the dictionary writes it, with its reading (Japanese: 食べました is 食べる). Otherwise it takes the
// text as it came, and the words found in it are offered.
RDE_INTERNAL void fude_wordcard_translated(fude_ui* _ui, c8* _taught) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    // Without the full stops and spaces at its end: Japanese's 。 and ．, its wide space, and the ASCII ones.
    while(fude_wordcard_cut_end(_taught, "\xE3\x80\x82") || fude_wordcard_cut_end(_taught, "\xEF\xBC\x8E") || fude_wordcard_cut_end(_taught, "\xE3\x80\x80") ||
          fude_wordcard_cut_end(_taught, ".") || fude_wordcard_cut_end(_taught, " ")) {
    }
    if(_taught[0] == 0) {
        fude_wordcard_translating(_ui, FUDE_WORDCARD_TRANSLATE_FAILED, FUDE_TEXT_SEL_TRANSLATION_NONE);
        return;
    }
    const fude_study*    _study = FUDE_STUDY(_ui->app);
    const fude_kanji_db* _db    = _study->db;
    u32                  _words[FUDE_WORDCARD_FOUND];
    const u32            _n     = _db != NULL ? fude_wordsplit(_db, _taught, _words, FUDE_WORDCARD_FOUND) : 0u;
    fude_kanji_word      _w;
    _card->found_count = 0;
    if(_n == 1u && fude_kanji_word_at(_db, _words[0], &_w)) {
        fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_WRITTEN], _w.written);
        fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_READING], _w.reading);
    } else {
        c8 _written[FUDE_USERWORD_WRITTEN];
        fude_utf8_set(_written, sizeof(_written), _taught);
        fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_WRITTEN], _written);
        fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_READING], "");
        fude_wordcard_fill(_ui);
        for(u32 _i = 0; _i < _n; _i++) {
            if(fude_kanji_word_at(_db, _words[_i], &_w)) {
                fude_wordcard_copy(&_card->found[_card->found_count++], _w.written, _w.reading, _w.meaning);
            }
        }
        fude_wordcard_label_found(_ui);
    }
    fude_wordcard_style_chips(_ui);
    fude_wordcard_style_badge(_ui);
    rde_ui_label_set_text(_card->error, "");
    fude_wordcard_translating(_ui, FUDE_WORDCARD_TRANSLATE_DONE, FUDE_TEXT_COUNT);
    _card->_layout = true;
}

// Once a frame while a translation is on its way: asks for the models the first time, sends the meaning once they
// are in, and takes the answer.
RDE_INTERNAL void fude_wordcard_translate_step(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    if(_card->translating == FUDE_WORDCARD_TRANSLATE_WAITING) {
        const FUDE_TRANSLATE_STATE_ _state = fude_translate_state(_card->translate_lang, fude_lang_code());
        if(_state == FUDE_TRANSLATE_READY) {
            _card->ticket = fude_translate_text(_card->translate_from, _card->translate_lang, fude_lang_code());
            fude_wordcard_translating(_ui, _card->ticket != 0u ? FUDE_WORDCARD_TRANSLATE_ASKED : FUDE_WORDCARD_TRANSLATE_FAILED,
                                      _card->ticket != 0u ? FUDE_TEXT_SCAN_TRANSLATING : FUDE_TEXT_SEL_TRANSLATION_NONE);
        } else if(_state == FUDE_TRANSLATE_MISSING || _state == FUDE_TRANSLATE_DOWNLOADING || (_state == FUDE_TRANSLATE_FAILED && !_card->translate_prepared)) {
            if(!_card->translate_prepared && _state != FUDE_TRANSLATE_DOWNLOADING) {
                _card->translate_prepared = true;
                fude_translate_prepare(_card->translate_lang, fude_lang_code());
            }
            fude_wordcard_translating(_ui, FUDE_WORDCARD_TRANSLATE_WAITING, FUDE_TEXT_SCAN_TRANSLATE_GETTING);
        } else {
            // Unavailable: "turn ML Kit on" only when it is off; with it on, the pair is one this
            // device cannot translate.
            const FUDE_TEXT_ _why = _state == FUDE_TRANSLATE_FAILED ? FUDE_TEXT_SCAN_TRANSLATE_FAILED
                                  : fude_mlkit_enabled()          ? FUDE_TEXT_SEL_TRANSLATION_NONE
                                                                  : FUDE_TEXT_SEL_TRANSLATE_MLKIT_OFF;
            fude_wordcard_translating(_ui, FUDE_WORDCARD_TRANSLATE_FAILED, _why);
        }
    }
    if(_card->translating == FUDE_WORDCARD_TRANSLATE_ASKED) {
        c8 _answer[FUDE_TRANSLATE_TEXT];
        if(fude_translate_take(_card->ticket, _answer, sizeof(_answer))) {
            _card->ticket = 0u;
            fude_wordcard_translated(_ui, _answer);
        }
    }
}

// See the header.
void fude_wordcard_update(fude_ui* _ui) {
    fude_wordcard*   _card   = FUDE_WORDCARD_OF(_ui);
    if(fude_wordcard_asked.kind != FUDE_WORDCARD_ASK_NONE) {
        fude_wordcard_open_asked(_ui);
    }
    if(_card->open) {
        fude_wordcard_translate_step(_ui);
    }
    const rde_vec_2F _screen = fude_kit_screen_size((_ui)->window);
    if(_card->open && (_card->_layout || _screen.x != _card->_laid_out.x || _screen.y != _card->_laid_out.y)) {
        fude_wordcard_layout(_ui);
    }
}

// --- saving ------------------------------------------------------------------------------

// The list form's Save: renames the list, or makes it. An empty name, or a list that could not be made, says so in
// the card instead.
RDE_INTERNAL void fude_wordcard_save_list(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    c8             _name[FUDE_VOCAB_LIST_NAME];
    if(!fude_wordcard_fits(_card->fields[FUDE_WORDCARD_WRITTEN], sizeof(_name))) {
        rde_ui_label_set_text(_card->error, fude_text(FUDE_TEXT_WORD_ERR_LONG));
        return;
    }
    fude_kit_field_text_trimmed(_card->fields[FUDE_WORDCARD_WRITTEN], _name, sizeof(_name));
    if(_name[0] == 0) {
        rde_ui_label_set_text(_card->error, fude_text(FUDE_TEXT_VOCAB_LIST_NAME_EMPTY));
        return;
    }
    if(_card->list_id != 0u) {
        fude_vocab_list_rename(_card->list_id, _name);
    } else {
        const u32 _list = fude_vocab_list_add(_name);
        if(_list == 0u) {
            rde_ui_label_set_text(_card->error, fude_text(FUDE_TEXT_VOCAB_LIST_NOT_MADE));
            return;
        }
        fude_wordcard_made_list = _list;
    }
    fude_wordcard_close(_ui);
}

// Save, in whichever form the card is in. For a word: the reading is filled in where it can be (from the dictionary,
// from a kana word itself, or from Latin letters' names), typed Latin letters are made the language's own reading,
// then the word is added or updated, its sentence kept, and its lists set. A problem is shown in the card instead.
RDE_INTERNAL void fude_wordcard_save(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    if(_card->list_mode) {
        fude_wordcard_save_list(_ui);
        return;
    }
    if(_card->note_mode) {
        if(!fude_wordcard_fits(_card->note_field, FUDE_CHARNOTE_TEXT)) {
            rde_ui_label_set_text(_card->error, fude_text(FUDE_TEXT_WORD_ERR_LONG));
            return;
        }
        c8* _text = rde_ui_text_editor_get_text(_card->note_field, 0, rde_ui_text_editor_get_byte_count(_card->note_field));
        fude_charnote_set(_card->note_cp, _text != NULL ? _text : "");
        if(_text != NULL) {
            rde_ui_text_editor_free_text(_card->note_field, _text);
        }
        fude_wordcard_close(_ui);
        return;
    }
    c8 _written[FUDE_USERWORD_WRITTEN], _reading[FUDE_USERWORD_READING], _meaning[FUDE_USERWORD_MEANING];
    if(!fude_wordcard_fits(_card->fields[FUDE_WORDCARD_WRITTEN], sizeof(_written)) || !fude_wordcard_fits(_card->fields[FUDE_WORDCARD_READING], sizeof(_reading)) ||
       !fude_wordcard_fits(_card->fields[FUDE_WORDCARD_MEANING], sizeof(_meaning))) {
        rde_ui_label_set_text(_card->error, fude_text(FUDE_TEXT_WORD_ERR_LONG));
        return;
    }
    fude_wordcard_fill(_ui);
    fude_kit_field_text_trimmed(_card->fields[FUDE_WORDCARD_WRITTEN], _written, sizeof(_written));
    fude_kit_field_text_trimmed(_card->fields[FUDE_WORDCARD_READING], _reading, sizeof(_reading));
    fude_kit_field_text_trimmed(_card->fields[FUDE_WORDCARD_MEANING], _meaning, sizeof(_meaning));
    if(_reading[0] == 0 && fude_lang_reads_as_written(_written)) {
        rde_snprintf(_reading, sizeof(_reading), "%s", _written);   // Japanese: a kana word reads as it is written.
    }
    if(_reading[0] == 0 && fude_lang_word_reading_latin()) {
        fude_wordcard_latin_names(_written, _reading, sizeof(_reading));   // Korean: 학교 reads hakgyo.
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
    } else if(_ascii && fude_lang_word_reading_latin()) {
        for(c8* _c = _reading; *_c != 0; _c++) {
            *_c = (*_c >= 'A' && *_c <= 'Z') ? (c8)(*_c + ('a' - 'A')) : *_c;   // Lower case, as the data writes it.
        }
    } else if(_ascii) {
        c8 _kana[FUDE_USERWORD_READING];
        if(fude_lang_reading_from_latin(_reading, _kana, sizeof(_kana))) {
            rde_snprintf(_reading, sizeof(_reading), "%s", _kana);
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
                _error = fude_text(FUDE_TEXT_WORD_ERR_NOT_SAVED);   // The written form is checked above, so not for being empty.
            }
        }
    }
    if(_error != NULL) {
        rde_ui_label_set_text(_card->error, _error);
        return;
    }

    // The sentence it was met in is kept, unless the word has one already.
    const c8* _had_to = "";
    if((_card->sentence[0] != 0 || _card->sentence_to[0] != 0) && fude_vocab_sentence(_id, &_had_to)[0] == 0 && _had_to[0] == 0) {
        fude_vocab_set_sentence(_id, _card->sentence, _card->sentence_to);
    }

    // Its lists, and the ones it went in remembered for the next new word.
    rde_arr_clear(fude_wordcard_last());
    const c8* _first = NULL;
    for(u32 _i = 0; _i < fude_wordcard_list_count(_card); _i++) {
        const fude_wordcard_list* _l = fude_wordcard_list_of(_card, _i);
        fude_vocab_set_in_list(_l->list, _id, _l->ticked);
        if(_l->ticked) {
            rde_arr_add(fude_wordcard_last(), (any)&_l->list);
            _first = _first != NULL ? _first : fude_vocab_list_name(_l->list);
        }
    }
    c8 _line[512];
    if(_first != NULL) {
        FUDE_TEXTF(_line, FUDE_TEXT_VOCAB_SAVED_IN, FUDE_TS(_written), FUDE_TS(_first));
    } else {
        FUDE_TEXTF(_line, FUDE_TEXT_VOCAB_SAVED, FUDE_TS(_written));
    }
    fude_notice_show(_line);
    fude_wordcard_close(_ui);
}

// Makes a new list with the name typed (when empty, "List n"), gives it a chip, and ticks it for the word. When the
// list could not be made, the card says so.
RDE_INTERNAL void fude_wordcard_make_list(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    c8             _name[FUDE_VOCAB_LIST_NAME];
    if(!fude_wordcard_fits(_card->list_field, sizeof(_name))) {
        rde_ui_label_set_text(_card->error, fude_text(FUDE_TEXT_WORD_ERR_LONG));
        return;
    }
    fude_kit_field_text_trimmed(_card->list_field, _name, sizeof(_name));
    if(_name[0] == 0) {
        fude_vocab_list_new_name(_name, sizeof(_name));
    }
    const u32 _list = fude_vocab_list_add(_name);
    _card->naming = false;
    if(_list != 0u) {
        const u32 _at = fude_wordcard_list_count(_card);
        fude_wordcard_list_chips(_ui, _at + 1u);
        fude_wordcard_list* _l = fude_wordcard_list_of(_card, _at);
        _l->list   = _list;
        _l->ticked = true;
        rde_ui_button_set_text(_l->chip, fude_vocab_list_name(_list));
        rde_ui_label_set_text(_card->error, "");
    } else {
        rde_ui_label_set_text(_card->error, fude_text(FUDE_TEXT_VOCAB_LIST_NOT_MADE));
    }
    fude_wordcard_style_chips(_ui);
    _card->_layout = true;
}

// --- callbacks ---------------------------------------------------------------------------

// Cancel, or a tap on the backdrop: closes the card without saving.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_cancel(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_wordcard_close((fude_ui*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Save.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_save(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_wordcard_save((fude_ui*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Remove, Delete list or Delete note, by the form the card is in; each says in a notice what went.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_remove(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui*          _ui = (fude_ui*)_user_data;
    if(FUDE_WORDCARD_OF(_ui)->note_mode) {
        fude_charnote_set(FUDE_WORDCARD_OF(_ui)->note_cp, NULL);   // Delete note.
        fude_wordcard_close(_ui);
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    if(FUDE_WORDCARD_OF(_ui)->list_mode) {
        // Delete list: the list goes, but its words stay in the vocabulary.
        if(FUDE_WORDCARD_OF(_ui)->list_id != 0u) {
            c8 _line[512];
            FUDE_TEXTF(_line, FUDE_TEXT_VOCAB_LIST_DELETED, FUDE_TS(fude_vocab_list_name(FUDE_WORDCARD_OF(_ui)->list_id)));
            fude_vocab_list_remove(FUDE_WORDCARD_OF(_ui)->list_id);
            fude_notice_show(_line);
        }
        fude_wordcard_close(_ui);
        return RDE_UI_EVENT_RESULT_CONSUME;
    }
    const fude_vocab_word* _w       = fude_vocab_get(FUDE_WORDCARD_OF(_ui)->id);
    if(_w != NULL) {
        c8 _line[512];
        FUDE_TEXTF(_line, FUDE_TEXT_VOCAB_REMOVED, FUDE_TS(_w->written));
        fude_vocab_remove(FUDE_WORDCARD_OF(_ui)->id);
        fude_notice_show(_line);
    }
    fude_wordcard_close(_ui);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A list's chip: ticks or unticks the list. _user_data is the UI, and the chip is the one whose node was tapped (the
// chips are made again as lists come and go, so none keeps an index of its own).
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_list(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_info);
    fude_wordcard* _card = FUDE_WORDCARD_OF((fude_ui*)_user_data);
    for(u32 _i = 0; _i < fude_wordcard_list_count(_card); _i++) {
        fude_wordcard_list* _l = fude_wordcard_list_of(_card, _i);
        if(rde_ui_button_as_node(_l->chip) == _node) {
            _l->ticked = !_l->ticked;
            fude_kit_button_chip(_l->chip, _l->ticked);
            break;
        }
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// A word found in what was read: the fields take it. _user_data is its chip's fude_wordcard_ref.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_found(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    const fude_wordcard_ref* _ref  = (const fude_wordcard_ref*)_user_data;
    fude_wordcard*           _card = FUDE_WORDCARD_OF(_ref->ui);
    const u32                _i    = _ref->index;
    if(_i < _card->found_count) {
        fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_WRITTEN], _card->found[_i].written);
        fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_READING], _card->found[_i].reading);
        fude_wordcard_set_field(_card->fields[FUDE_WORDCARD_MEANING], _card->found[_i].meaning);
        _card->id = fude_vocab_find(_card->found[_i].written, _card->found[_i].reading);   // When saved, that one.
        _card->_layout = true;
    }
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// See the header: the meaning, in the reader's language, into the language taught, for the written field.
void fude_wordcard_translate(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    if(!_card->open || _card->translating == FUDE_WORDCARD_TRANSLATE_WAITING || _card->translating == FUDE_WORDCARD_TRANSLATE_ASKED) {
        return;   // One is on its way already.
    }
    fude_kit_field_text_trimmed(_card->fields[FUDE_WORDCARD_MEANING], _card->translate_from, sizeof(_card->translate_from));
    if(_card->translate_from[0] == 0) {
        fude_wordcard_translating(_ui, FUDE_WORDCARD_TRANSLATE_FAILED, FUDE_TEXT_WORD_TRANSLATE_EMPTY);
        rde_ui_node_focus(rde_ui_text_editor_as_node(_card->fields[FUDE_WORDCARD_MEANING]));
        return;
    }
    _card->translate_lang = fude_translate_target();
    _card->ticket         = 0u;
    fude_wordcard_translating(_ui, FUDE_WORDCARD_TRANSLATE_WAITING, FUDE_TEXT_SCAN_TRANSLATING);
    fude_wordcard_translate_step(_ui);   // Sent at once when the models are in.
}

// Translate with Google, pressed.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_translate(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_wordcard_translate((fude_ui*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// New list: shows the field to name it.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_new_list(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_ui*  _ui = (fude_ui*)_user_data;
    fude_wordcard* _card    = FUDE_WORDCARD_OF(_ui);
    _card->naming = true;
    fude_wordcard_set_field(_card->list_field, "");
    fude_wordcard_layout(_ui);
    rde_ui_node_focus(rde_ui_text_editor_as_node(_card->list_field));
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Add, beside the new list's name: makes the list.
RDE_INTERNAL RDE_UI_EVENT_RESULT_ fude_wordcard_on_list_ok(rde_ui_node* _node, const rde_ui_event_info* _info, any _user_data) {
    RDE_UNUSED(_node); RDE_UNUSED(_info);
    fude_wordcard_make_list((fude_ui*)_user_data);
    return RDE_UI_EVENT_RESULT_CONSUME;
}

// Return in a field: the written field looks the word up and goes on to the reading, the reading goes on to the
// meaning, and the meaning saves. In the new list's name it makes the list; in the list form it saves.
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

// A label under _parent, _px tall, left-aligned, that lets taps through.
RDE_INTERNAL rde_ui_label* fude_wordcard_label(fude_ui* _ui, rde_ui_node* _parent, f32 _px) {
    rde_ui_label* _label = rde_ui_label_create(NULL);
    rde_ui_label_set_font(_label, _ui->font);
    rde_ui_label_set_font_scale(_label, _px / (f32)FUDE_KIT_FONT_SIZE);
    rde_ui_label_set_alignment(_label, RDE_UI_LABEL_H_ALIGN_LEFT, RDE_UI_LABEL_V_ALIGN_MIDDLE);
    rde_ui_node_set_raycast_target(rde_ui_label_as_node(_label), false);
    rde_ui_node_add_child(_parent, rde_ui_label_as_node(_label));
    return _label;
}

// A one-line field under _parent, in its box, holding at most _max characters, with Return handled by
// fude_wordcard_on_submit.
RDE_INTERNAL rde_ui_text_editor* fude_wordcard_field(fude_ui* _ui, rde_ui_node* _parent, const c8* _placeholder, u32 _max) {
    rde_ui_text_editor* _f = rde_ui_text_editor_create(_ui->font, NULL);
    rde_ui_text_editor_set_font_size(_f, FUDE_WORDCARD_FIELD_PX);
    rde_ui_text_editor_set_max_chars(_f, _max);
    rde_ui_text_editor_set_placeholder(_f, _placeholder);
    rde_ui_text_editor_set_content_insets(_f, 12.0f, 8.0f, 12.0f, 8.0f);
    rde_ui_node_set_user_data(rde_ui_text_editor_as_node(_f), _ui);
    rde_ui_text_editor_set_on_submit(_f, fude_wordcard_on_submit);
    fude_kit_field_box(_parent, _f);
    return _f;
}

// See the header. The fields hold at most as many characters as the texts they make (vocab.h: a word's, a list's
// name; charnote.h: a note). The lists' chips are made when the card opens (fude_wordcard_list_chips).
void fude_wordcard_create(fude_ui* _ui, rde_ui_node* _root) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    fude_wordcard_forget(_ui);   // Nothing to let go on a first build; the UI lets go before it builds again.
    rde_memset(_card, 0, sizeof(*_card));
    _card->lists = rde_arr_new(sizeof(fude_wordcard_list), rde_memory_allocator_get_default_std());
    // Over everything, with a backdrop that cancels it when tapped.
    fude_kit_modal_create(&_card->modal, _root, fude_wordcard_on_cancel, _ui);
    rde_ui_node* _c = rde_ui_image_as_node(_card->modal.card);

    _card->title = fude_wordcard_label(_ui, _c, 17.0f);
    const c8* const _placeholders[3] = { fude_text(FUDE_TEXT_WORD_FIELD_WRITTEN), fude_text(FUDE_TEXT_WORD_FIELD_READING), fude_text(FUDE_TEXT_WORD_FIELD_MEANING) };
    const u32       _max[3]          = { FUDE_USERWORD_WRITTEN_CHARS, FUDE_USERWORD_READING_CHARS, FUDE_USERWORD_MEANING_CHARS };
    for(u32 _i = 0; _i < 3u; _i++) {
        _card->fields[_i] = fude_wordcard_field(_ui, _c, _placeholders[_i], _max[_i]);
    }
    c8 _translate[96];
    fude_draw_icon_label(_translate, sizeof(_translate), FUDE_ICON_TRANSLATE, fude_text(FUDE_TEXT_SCAN_TRANSLATE));
    _card->translate        = fude_kit_button(_c, _translate, fude_wordcard_on_translate, _ui);
    _card->translate_status = fude_wordcard_label(_ui, _c, 12.0f);
    rde_ui_label_set_wrap(_card->translate_status, true);
    rde_ui_label_set_auto_fit(_card->translate_status, true);
    rde_ui_label_set_auto_fit_min_scale(_card->translate_status, 0.7f);
    _card->badge = rde_ui_image_create(NULL);
    rde_ui_node_set_raycast_target(rde_ui_image_as_node(_card->badge), false);
    rde_ui_node_add_child(_c, rde_ui_image_as_node(_card->badge));
    _card->sentence_caption = fude_wordcard_label(_ui, _c, 12.0f);
    rde_ui_label_set_text(_card->sentence_caption, fude_text(FUDE_TEXT_WORD_SENTENCE));
    _card->sentence_label    = fude_wordcard_label(_ui, _c, 15.0f);
    _card->sentence_to_label = fude_wordcard_label(_ui, _c, 13.0f);
    for(u32 _i = 0; _i < 2u; _i++) {
        rde_ui_label* _l = _i == 0u ? _card->sentence_label : _card->sentence_to_label;
        rde_ui_label_set_wrap(_l, true);
        rde_ui_label_set_auto_fit(_l, true);
        rde_ui_label_set_auto_fit_min_scale(_l, 0.6f);
    }
    _card->found_caption = fude_wordcard_label(_ui, _c, 12.0f);
    rde_ui_label_set_text(_card->found_caption, fude_text(FUDE_TEXT_WORD_FOUND));
    for(u32 _i = 0; _i < FUDE_WORDCARD_FOUND; _i++) {
        _card->refs[_i]        = (fude_wordcard_ref){ _ui, _i };
        _card->found_chips[_i] = fude_kit_button(_c, "", fude_wordcard_on_found, _ui);
        rde_ui_button_set_on_click(_card->found_chips[_i], fude_wordcard_on_found, &_card->refs[_i]);
    }
    _card->lists_caption = fude_wordcard_label(_ui, _c, 12.0f);
    rde_ui_label_set_text(_card->lists_caption, fude_text(FUDE_TEXT_WORD_LISTS));
    c8 _label[96];
    fude_draw_icon_label(_label, sizeof(_label), FUDE_ICON_PLUS, fude_text(FUDE_TEXT_WORD_NEW_LIST));
    _card->new_list   = fude_kit_button(_c, _label, fude_wordcard_on_new_list, _ui);
    _card->list_field = fude_wordcard_field(_ui, _c, fude_text(FUDE_TEXT_WORD_LIST_NAME), FUDE_VOCAB_LIST_NAME_CHARS);
    _card->note_field = fude_wordcard_field(_ui, _c, fude_text(FUDE_TEXT_NOTE_PLACEHOLDER), FUDE_CHARNOTE_CHARS);
    rde_ui_text_editor_set_multiline(_card->note_field, true);
    _card->list_ok    = fude_kit_button(_c, fude_text(FUDE_TEXT_ADD), fude_wordcard_on_list_ok, _ui);
    _card->error      = fude_wordcard_label(_ui, _c, 12.0f);
    fude_draw_icon_label(_label, sizeof(_label), FUDE_ICON_TRASH, fude_text(FUDE_TEXT_WORD_REMOVE));
    _card->remove     = fude_kit_button(_c, _label, fude_wordcard_on_remove, _ui);
    _card->cancel     = fude_kit_button(_c, fude_text(FUDE_TEXT_CANCEL), fude_wordcard_on_cancel, _ui);
    fude_draw_icon_label(_label, sizeof(_label), FUDE_ICON_CHECK, fude_text(FUDE_TEXT_SAVE));
    _card->save       = fude_kit_button(_c, _label, fude_wordcard_on_save, _ui);
    fude_wordcard_close(_ui);
}

// See the header: frees the lists' array; their chips go with the canvas.
void fude_wordcard_forget(fude_ui* _ui) {
    fude_wordcard* _card = FUDE_WORDCARD_OF(_ui);
    if(rde_arr_is_inited(&_card->lists)) {
        rde_arr_free(&_card->lists);
    }
}

// See the header. Does nothing before the card is built.
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
    rde_ui_label_set_color(_card->translate_status, _t->text_soft);
    rde_ui_label_set_color(_card->sentence_caption, _t->text_soft);
    rde_ui_label_set_color(_card->sentence_label, _t->button_text);
    rde_ui_label_set_color(_card->sentence_to_label, _t->text_soft);
    if(_card->translating == FUDE_WORDCARD_TRANSLATE_DONE) {
        fude_wordcard_style_badge(_ui);
    }
    fude_kit_button_plain(_card->cancel);
    fude_kit_button_primary(_card->save);
    fude_kit_button_danger_quiet(_card->remove);
    fude_kit_button_plain(_card->list_ok);
    fude_wordcard_style_chips(_ui);
}
