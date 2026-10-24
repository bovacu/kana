// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/screens/translator.h"
#include "lang/lang.h"
#include "study/widgets/header.h"
#include "study/widgets/wordcard.h"
#include "drawing/ink/mlkit.h"
#include "study/services/speech.h"
#include "study/models/vocab.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/base/draw.h"
#include "drawing/base/icons.h"
#include "drawing/widgets/notice.h"
#include "study/screens/practice.h"   // FUDE_PRACTICE_SET_MAX: Practice takes a translation's characters as a set.

// ===========================================================================
// See translator.h.
//
// The cards are a fixed array, newest first: a new one moves the others down one place and the oldest drops off the
// end. A card goes WAITING (for the models) -> ASKED (a ticket from translate.h) -> DONE, or FAILED when the models
// could not be had; each frame, fude_translator_translate moves every card on as far as it can.
// ===========================================================================

// The layout's sizes, in screen points, and the text sizes (_PX) and their line heights (_L), in points.
#define FUDE_TRANSLATOR_PAD      18.0f   // The space inside a card.
#define FUDE_TRANSLATOR_GAP      14.0f   // The space between cards.
#define FUDE_TRANSLATOR_FROM_PX  15.0f   // What was typed.
#define FUDE_TRANSLATOR_FROM_L   21.0f
#define FUDE_TRANSLATOR_TAUGHT_PX 30.0f   // The translation, in the language taught.
#define FUDE_TRANSLATOR_TAUGHT_L 40.0f
#define FUDE_TRANSLATOR_WORD     30.0f   // A word's row.
#define FUDE_TRANSLATOR_SPEAK    44.0f   // The speaker's square.

// --- Asking ----------------------------------------------------------------------------------------------------------

// Returns the language typed in: the reader's (English when the app's UI is in the language taught: Japanese
// while Japanese is taught).
RDE_INTERNAL const c8* fude_translator_from(void) {
    return fude_translate_target();
}

// Puts a new card on top for _text, with leading spaces dropped; it waits there for the translator. A text longer than
// a card holds (FUDE_TRANSLATOR_INPUT) is cut.
void fude_translator_ask(fude_translator* _tr, const c8* _text) {
    if(_text == NULL) {
        return;
    }
    while(*_text == ' ') {
        _text++;
    }
    if(*_text == 0) {
        return;
    }
    if(!fude_mlkit_enabled() && fude_translate_state(_tr->from, fude_lang_code()) == FUDE_TRANSLATE_UNAVAILABLE) {
        fude_notice_show(fude_text(FUDE_TEXT_SEL_TRANSLATE_MLKIT_OFF));
        return;
    }
    // The new card goes on top, and the oldest is let go when there is no room.
    const u32 _keep = _tr->count < FUDE_TRANSLATOR_CARDS ? _tr->count : FUDE_TRANSLATOR_CARDS - 1u;
    rde_memmove(&_tr->cards[1], &_tr->cards[0], sizeof(fude_translator_card) * _keep);
    _tr->count = _keep + 1u;
    fude_translator_card* _card = &_tr->cards[0];
    rde_memset(_card, 0, sizeof(*_card));
    rde_snprintf(_card->from, sizeof(_card->from), "%s", _text);
    _card->state           = FUDE_TRANSLATOR_WAITING;
    _tr->chosen              = 0u;
    _tr->scroller.offset.y   = 0.0f;
    _tr->scroller.velocity.y = 0.0f;
}

// See the header: the card chosen, only once its translation is in and not empty.
const fude_translator_card* fude_translator_chosen(const fude_translator* _tr) {
    if(_tr->chosen >= _tr->count) {
        return NULL;
    }
    const fude_translator_card* _card = &_tr->cards[_tr->chosen];
    return _card->state == FUDE_TRANSLATOR_DONE && _card->taught[0] != 0 ? _card : NULL;
}

// --- Each frame ------------------------------------------------------------------------------------------------------

// Moves the cards on: the models are asked for (once a visit, and again on a new visit after a failed download), the
// cards waiting are sent once the models are there, and the answers are taken, with the dictionary's words found in
// each. A card the translator would not take (no ticket) is done, empty.
RDE_INTERNAL void fude_translator_translate(fude_translator* _tr) {
    const FUDE_TRANSLATE_STATE_ _state = fude_translate_state(_tr->from, fude_lang_code());
    for(u32 _i = 0; _i < _tr->count; _i++) {
        fude_translator_card* _card = &_tr->cards[_i];
        if(_card->state != FUDE_TRANSLATOR_WAITING) {
            continue;
        }
        if(_state == FUDE_TRANSLATE_READY) {
            _card->ticket = fude_translate_text(_card->from, _tr->from, fude_lang_code());
            _card->state  = _card->ticket != 0u ? FUDE_TRANSLATOR_ASKED : FUDE_TRANSLATOR_DONE;
        } else if(_state == FUDE_TRANSLATE_MISSING || (_state == FUDE_TRANSLATE_FAILED && !_tr->prepared)) {
            if(!_tr->prepared) {
                _tr->prepared = true;
                fude_translate_prepare(_tr->from, fude_lang_code());
            }
        } else if(_state == FUDE_TRANSLATE_FAILED || _state == FUDE_TRANSLATE_UNAVAILABLE) {
            _card->state = FUDE_TRANSLATOR_FAILED;
        }
    }
    for(u32 _i = 0; _i < _tr->count; _i++) {
        fude_translator_card* _card = &_tr->cards[_i];
        if(_card->state == FUDE_TRANSLATOR_ASKED && fude_translate_take(_card->ticket, _card->taught, sizeof(_card->taught))) {
            _card->state      = FUDE_TRANSLATOR_DONE;
            _card->word_count = _tr->db != NULL && _card->taught[0] != 0 ? fude_wordsplit(_tr->db, _card->taught, _card->words, FUDE_WORDSPLIT_MAX) : 0u;
        }
    }
}

// Returns whether point _p is inside the box from _min to _max, edges included.
RDE_INTERNAL b8 fude_translator_in(rde_vec_2F _p, rde_vec_2F _min, rde_vec_2F _max) {
    return _p.x >= _min.x && _p.x <= _max.x && _p.y >= _min.y && _p.y <= _max.y;
}

// Acts on a tap in the cards' area: on a card's speaker, its translation is said aloud; on one of its words, the word
// card opens; anywhere else on it, the card is chosen for the row.
RDE_INTERNAL void fude_translator_tap(fude_translator* _tr, rde_vec_2F _at) {
    if(!fude_translator_in(_at, _tr->list_min, _tr->list_max)) {
        return;
    }
    for(u32 _i = 0; _i < _tr->count; _i++) {
        const fude_translator_card* _card = &_tr->cards[_i];
        if(!fude_translator_in(_at, _card->min, _card->max)) {
            continue;
        }
        if(_card->speak_max.x > _card->speak_min.x && fude_translator_in(_at, _card->speak_min, _card->speak_max)) {
            fude_speak(_card->taught);
            return;
        }
        if(_card->word_count > 0u && _at.y <= _card->words_top && _tr->db != NULL) {
            const i32       _k = (i32)rde_math_floor_f32((_card->words_top - _at.y) / FUDE_TRANSLATOR_WORD);
            fude_kanji_word _w;
            if(_k >= 0 && (u32)_k < _card->word_count && fude_kanji_word_at(_tr->db, _card->words[_k], &_w)) {
                fude_wordcard_ask_in(_w.written, _w.reading, _w.meaning, _card->taught, _card->from);   // To save it with its sentence, or see it saved.
                return;
            }
        }
        _tr->chosen = _i;
        return;
    }
}

// Moves the translations on, scrolls, and takes a tap.
void fude_translator_update(fude_translator* _tr, f32 _dt) {
    if(!_tr->open) {
        return;
    }
    fude_translator_translate(_tr);
    fude_kit_scroller_update(&_tr->scroller, _dt, _tr->content_h, _tr->list_max.y - _tr->list_min.y);
    rde_vec_2F _at;
    if(rde_ui_scroller_take_tap(&_tr->scroller, &_at)) {
        fude_translator_tap(_tr, _at);
    }
}

// The pointer drives the scroller, which scrolls the cards and tells a tap from a drag; fude_translator_update takes
// the tap.
void fude_translator_pointer_down(fude_translator* _tr, rde_vec_2F _screen, f64 _time) {
    rde_ui_scroller_pointer_down(&_tr->scroller, _screen, _time);
}

// Drags the cards.
void fude_translator_pointer_moved(fude_translator* _tr, rde_vec_2F _screen, f64 _time) {
    rde_ui_scroller_pointer_moved(&_tr->scroller, _screen, _time);
}

// Ends the drag, or the tap.
void fude_translator_pointer_up(fude_translator* _tr, f64 _time) {
    rde_ui_scroller_pointer_up(&_tr->scroller, _time);
}

// --- Drawing ---------------------------------------------------------------------------------------------------------

// Returns a card's height at _width: what was typed, _taught (its translation in the language taught, or how far it
// got), and its words.
RDE_INTERNAL f32 fude_translator_card_h(const fude_translator_card* _card, const c8* _taught, rde_font* _font, f32 _font_px, f32 _width) {
    const f32 _inner = _width - 2.0f * FUDE_TRANSLATOR_PAD - FUDE_TRANSLATOR_SPEAK;
    const u32 _from  = fude_draw_text_wrap_lines(_font, _font_px, _card->from, FUDE_TRANSLATOR_FROM_PX, _inner);
    const u32 _said  = fude_draw_text_wrap_lines(_font, _font_px, _taught, FUDE_TRANSLATOR_TAUGHT_PX, _inner);
    return 2.0f * FUDE_TRANSLATOR_PAD + (f32)_from * FUDE_TRANSLATOR_FROM_L + 8.0f + (f32)_said * FUDE_TRANSLATOR_TAUGHT_L +
           (_card->word_count > 0u ? 12.0f + (f32)_card->word_count * FUDE_TRANSLATOR_WORD : 0.0f);
}

// Draws the header, Google's badge, and the cards (or a card saying there are none yet), clipped to their area. It
// records each card's box, its speaker and its words' top for taps, and the cards' content height.
void fude_translator_render(fude_translator* _tr, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_tr->open) {
        return;
    }
    const fude_theme* _t      = fude_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _insets = rde_window_get_safe_area_insets(_window);
    const f32         _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + 24.0f;
    const f32         _right  = (f32)_size.x * 0.5f - (f32)_insets.z - 24.0f;
    const f32         _width  = _right - _left;

    // The header: the language's badge (Japanese's is 訳), the title ("Into Japanese", say), and the language typed in,
    // an arrow, and the language taught, named in itself (日本語). When the UI is in the language taught, English is
    // what is typed. Both names are the UI's list's (text.h). The header leaves room at the top right for the field,
    // which the UI draws.
    c8                  _caption[96];
    const c8*           _target;
    const c8*           _target_flag;
    const RDE_LANGUAGE_ _taught = fude_lang_ui_language(&_target, &_target_flag);
    const RDE_LANGUAGE_ _typed  = fude_text_language() != _taught ? fude_text_language() : RDE_LANGUAGE_EN_US;
    const c8*           _name   = "";
    for(u32 _l = 0; _l < FUDE_TEXT_LANGUAGES; _l++) {
        _name = FUDE_TEXT_LANGUAGE_LIST[_l].language == _typed ? FUDE_TEXT_LANGUAGE_LIST[_l].name : _name;
    }
    // The arrow is Phosphor's icon, since every app's font has it and not every one has →; it points the way the UI
    // reads.
    if(fude_draw_is_rtl()) {   // Each piece is set apart by a right-to-left mark (draw.h: fude_draw_icon_label).
        rde_snprintf(_caption, sizeof(_caption), FUDE_DRAW_RLM "%s" FUDE_DRAW_RLM " %s " FUDE_DRAW_RLM "%s", _name, fude_draw_icon_dir(FUDE_ICON_ARROW_RIGHT), _target);
    } else {
        rde_snprintf(_caption, sizeof(_caption), "%s %s %s", _name, FUDE_ICON_ARROW_RIGHT, _target);
    }
    fude_header_draw(&_tr->glyph, fude_lang_badge(FUDE_LANG_BADGE_TRANSLATE), _font, _font_px, _left, _right - 380.0f, _top, fude_text(FUDE_TEXT_TRANSLATOR_TITLE), _caption, NULL);

    // Google's badge, next to the translations; its dark version on a dark page (luma, Rec. 601 weights, under half).
    const rde_color _p    = _t->page;
    const b8        _dark = 0.299f * (f32)_p.r + 0.587f * (f32)_p.g + 0.114f * (f32)_p.b < 128.0f;
    f32             _y    = _top - 62.0f;
    fude_translate_draw_badge(_left, _y - FUDE_TRANSLATE_BADGE_H * 0.5f, _dark);
    _y -= FUDE_TRANSLATE_BADGE_H + 14.0f;

    _tr->list_min = (rde_vec_2F){ _left, _bottom };
    _tr->list_max = (rde_vec_2F){ _right, _y };
    if(_tr->count == 0u) {
        _tr->content_h = 0.0f;
        const u32 _lines = fude_draw_text_wrap_lines(_font, _font_px, fude_text(FUDE_TEXT_TRANSLATOR_EMPTY), 15.0f, _width - 40.0f);
        fude_draw_card((rde_vec_2F){ _left, _y - 40.0f - (f32)_lines * 22.0f }, (rde_vec_2F){ _right, _y }, 14.0f, _t->surface, _t->outline);
        fude_draw_text_wrap(_font, _font_px, fude_text(FUDE_TEXT_TRANSLATOR_EMPTY), _left + 20.0f, _y - 32.0f, 15.0f, _width - 40.0f, 22.0f, _t->text_soft);
        return;
    }

    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)fude_draw_x((_left + _right) * 0.5f), (i32)((_y + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)_width + 8u, (u32)rde_math_max_f32(1.0f, _y - _bottom) });
    const b8 _speak = fude_speech_available();
    f32      _at    = _y + _tr->scroller.offset.y;   // The next card's top.
    f32      _total = 0.0f;
    for(u32 _i = 0; _i < _tr->count; _i++) {
        fude_translator_card* _card = &_tr->cards[_i];
        // Its translation, in the language taught, or how far it got.
        const c8* _said       = _card->taught;
        rde_color _said_color = _t->ink;
        f32       _taught_px  = FUDE_TRANSLATOR_TAUGHT_PX;
        if(_card->state == FUDE_TRANSLATOR_WAITING) {
            _said = fude_text(fude_translate_state(_tr->from, fude_lang_code()) == FUDE_TRANSLATE_DOWNLOADING ? FUDE_TEXT_SCAN_TRANSLATE_GETTING : FUDE_TEXT_SCAN_TRANSLATING);
            _said_color = _t->text_soft;
        } else if(_card->state == FUDE_TRANSLATOR_ASKED) {
            _said = fude_text(FUDE_TEXT_SCAN_TRANSLATING);
            _said_color = _t->text_soft;
        } else if(_card->state == FUDE_TRANSLATOR_FAILED) {
            _said = fude_text(FUDE_TEXT_SCAN_TRANSLATE_FAILED);
            _said_color = _t->score_poor;
        } else if(_said[0] == 0) {
            _said = fude_text(FUDE_TEXT_TRANSLATOR_NONE);
            _said_color = _t->text_soft;
        }
        if(_said_color.r != _t->ink.r || _said_color.g != _t->ink.g || _said_color.b != _t->ink.b) {
            _taught_px = 16.0f;   // A status, not the translation.
        }
        const f32 _h     = fude_translator_card_h(_card, _said, _font, _font_px, _width);
        const f32 _inner = _width - 2.0f * FUDE_TRANSLATOR_PAD - FUDE_TRANSLATOR_SPEAK;
        _card->min       = (rde_vec_2F){ _left, _at - _h };
        _card->max       = (rde_vec_2F){ _right, _at };
        _card->speak_min = _card->speak_max = (rde_vec_2F){ 0.0f, 0.0f };
        _card->words_top = -1e30f;
        if(_at - _h < _y && _at > _bottom) {
            // The card chosen for the row is ringed in the accent, by a slightly larger card behind it.
            const b8 _chosen = _i == _tr->chosen && _card->state == FUDE_TRANSLATOR_DONE && _card->taught[0] != 0;
            if(_chosen) {
                fude_draw_card((rde_vec_2F){ _left - 2.0f, _at - _h - 2.0f }, (rde_vec_2F){ _right + 2.0f, _at + 2.0f }, 18.0f, _t->accent, _t->accent);
            }
            fude_draw_card(_card->min, _card->max, 16.0f, _t->surface, _chosen ? _t->accent : _t->outline);
            f32 _cy = _at - FUDE_TRANSLATOR_PAD;
            fude_draw_text_wrap(_font, _font_px, _card->from, _left + FUDE_TRANSLATOR_PAD, _cy - FUDE_TRANSLATOR_FROM_PX, FUDE_TRANSLATOR_FROM_PX, _inner,
                                FUDE_TRANSLATOR_FROM_L, _t->text_soft);
            _cy -= (f32)fude_draw_text_wrap_lines(_font, _font_px, _card->from, FUDE_TRANSLATOR_FROM_PX, _inner) * FUDE_TRANSLATOR_FROM_L + 8.0f;
            fude_draw_text_wrap(_font, _font_px, _said, _left + FUDE_TRANSLATOR_PAD, _cy - _taught_px * 1.05f, _taught_px, _inner, FUDE_TRANSLATOR_TAUGHT_L, _said_color);
            // With a voice, and a translation to say, a speaker goes at its right.
            if(_speak && _card->state == FUDE_TRANSLATOR_DONE && _card->taught[0] != 0) {
                const rde_vec_2F _c = { _right - FUDE_TRANSLATOR_PAD - FUDE_TRANSLATOR_SPEAK * 0.5f, _cy - FUDE_TRANSLATOR_TAUGHT_L * 0.5f };
                fude_draw_icon(_font, _font_px, FUDE_ICON_SPEAK, _c, 22.0f, _t->accent);
                _card->speak_min = (rde_vec_2F){ _c.x - FUDE_TRANSLATOR_SPEAK * 0.5f, _c.y - FUDE_TRANSLATOR_SPEAK * 0.5f };
                _card->speak_max = (rde_vec_2F){ _c.x + FUDE_TRANSLATOR_SPEAK * 0.5f, _c.y + FUDE_TRANSLATOR_SPEAK * 0.5f };
            }
            _cy -= (f32)fude_draw_text_wrap_lines(_font, _font_px, _said, _taught_px, _inner) * FUDE_TRANSLATOR_TAUGHT_L;
            // Its words: the bookmark (filled when the word is the learner's), the word, its reading and its meaning.
            if(_card->word_count > 0u && _tr->db != NULL) {
                _cy -= 12.0f;
                fude_draw_line((rde_vec_2F){ _left + FUDE_TRANSLATOR_PAD, _cy + 6.0f }, (rde_vec_2F){ _right - FUDE_TRANSLATOR_PAD, _cy + 6.0f }, 0.5f, _t->outline);
                _card->words_top = _cy;
                for(u32 _k = 0; _k < _card->word_count; _k++) {
                    fude_kanji_word _w;
                    if(!fude_kanji_word_at(_tr->db, _card->words[_k], &_w)) {
                        continue;
                    }
                    const f32 _mid    = _cy - ((f32)_k + 0.5f) * FUDE_TRANSLATOR_WORD;
                    const b8  _theirs = fude_vocab_find(_w.written, _w.reading) != 0u;
                    const rde_vec_2F _b = { _left + FUDE_TRANSLATOR_PAD + 9.0f, _mid };
                    if(_theirs) { fude_draw_icon_fill(FUDE_ICON_BOOKMARK, _b, 16.0f, _t->accent); }
                    else        { fude_draw_icon(_font, _font_px, FUDE_ICON_BOOKMARK, _b, 16.0f, _t->accent); }
                    c8 _say[400];
                    rde_snprintf(_say, sizeof(_say), "%s  %s  \xC2\xB7  %s", _w.written, _w.reading, _w.meaning);   // A middle dot, ·, in UTF-8.
                    const f32 _px = fude_draw_text_px_to_fit(_font, _font_px, _say, 15.0f, _width - 2.0f * FUDE_TRANSLATOR_PAD - 30.0f);
                    fude_draw_text(_font, _font_px, _say, _left + FUDE_TRANSLATOR_PAD + 30.0f, _mid - _px * 0.36f, _px, _theirs ? _t->text : _t->text_soft);
                }
            }
        }
        _at    -= _h + FUDE_TRANSLATOR_GAP;
        _total += _h + FUDE_TRANSLATOR_GAP;
    }
    rde_rendering_end_clipping_rect();
    _tr->content_h = _total;
}

// --- Lifetime --------------------------------------------------------------------------------------------------------

// Sets the translator up closed, with no cards.
void fude_translator_init(fude_translator* _tr, const fude_kanji_db* _db) {
    rde_memset(_tr, 0, sizeof(*_tr));
    fude_kit_scroller_init(&_tr->scroller);
    _tr->db = _db;
    fude_glyph_init(&_tr->glyph, _db);
}

// Frees the glyph cache (the cards hold nothing to free), and zeroes the translator.
void fude_translator_destroy(fude_translator* _tr) {
    fude_glyph_destroy(&_tr->glyph);
    rde_memset(_tr, 0, sizeof(*_tr));
}

// Opens the translator, noting the language typed in for this visit. Cards whose models failed last visit wait again.
void fude_translator_open(fude_translator* _tr) {
    _tr->open     = true;
    _tr->prepared = false;
    _tr->from     = fude_translator_from();
    rde_ui_scroller_stop(&_tr->scroller);
    // The cards whose models failed on the last visit wait again, so the models are asked for again.
    for(u32 _i = 0; _i < _tr->count; _i++) {
        if(_tr->cards[_i].state == FUDE_TRANSLATOR_FAILED) {
            _tr->cards[_i].state = FUDE_TRANSLATOR_WAITING;
        }
    }
}

// Closes the translator; its cards stay.
void fude_translator_close(fude_translator* _tr) {
    _tr->open = false;
    rde_ui_scroller_stop(&_tr->scroller);
}

// --- The screen (screen.h): its row, and what it does ----------------------------------------------------------------

#include "study/app/study.h"

// The screen's is_open, close, pointer and render functions, which call the translator's own (screen.h).
FUDE_SCREEN_ADAPTERS(fude_translator, fude_translator)
FUDE_SCREEN_RENDER(fude_translator, fude_translator)

// The translator's frame.
RDE_INTERNAL void fude_translator_screen_update(fude_app* _app, any _self, f32 _dt) {
    RDE_UNUSED(_app);
    fude_translator_update((fude_translator*)_self, _dt);
}

// Return in the field: translates what was typed.
RDE_INTERNAL void fude_translator_screen_field(any _self, const c8* _text) {
    fude_translator_ask((fude_translator*)_self, _text);
}

// Back closes the translator.
FUDE_ROW_CALL(fude_translator_row_back, fude_translator, fude_translator_close)

// Save: saves the whole translation as one word of the vocabulary, through the word card, filled in with the
// translation (in the language taught) and what was typed as its meaning. It goes in the list the Vocabulary shows.
RDE_INTERNAL void fude_translator_row_save(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_arg);
    const fude_translator_card* _card = fude_translator_chosen((const fude_translator*)_self);
    if(_card != NULL) {
        fude_wordcard_prefer_list(FUDE_STUDY(_app)->vocab->list);
        fude_wordcard_ask(_card->taught, "", _card->from, 0u);
    }
}

// Write: writes the translation on the page, in the characters' own strokes, in the middle of what is on screen. Every
// screen is closed first, so it can be seen.
RDE_INTERNAL void fude_translator_row_write(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_arg);
    const fude_translator_card* _card = fude_translator_chosen((const fude_translator*)_self);
    if(_card == NULL) {
        return;
    }
    // Closing the screens frees nothing, but the card belongs to this screen, so its text is copied first.
    c8 _taught[FUDE_TRANSLATE_TEXT];
    rde_snprintf(_taught, sizeof(_taught), "%s", _card->taught);   // A copy, as the card is the screen's.
    fude_app_close_all(_app);
    fude_study_write_text(_app, _taught, fude_canvas_from_screen(_app->canvas, (rde_vec_2F){ 0.0f, 0.0f }));
}

// Practice: opens Practice on the translation's characters that the data has, each once, as a set. A set takes at most
// FUDE_PRACTICE_SET_MAX characters (practice.h) and is cut there anyway, so they are gathered up to that many and no
// further; the bound is Practice's, not the translation's. With none, a notice says so.
RDE_INTERNAL void fude_translator_row_practice(fude_app* _app, any _self, u32 _arg) {
    RDE_UNUSED(_arg);
    const fude_translator*      _tr   = (const fude_translator*)_self;
    const fude_translator_card* _card = fude_translator_chosen(_tr);
    if(_card == NULL || _tr->db == NULL) {
        return;
    }
    u32       _records[FUDE_PRACTICE_SET_MAX];
    u32       _n = 0;
    const c8* _p = _card->taught;
    for(u32 _cp = fude_utf8_next(&_p); _cp != 0 && _n < FUDE_PRACTICE_SET_MAX; _cp = fude_utf8_next(&_p)) {
        u32 _record;
        b8  _again = false;
        if(!fude_kanji_find_index(_tr->db, _cp, &_record)) {
            continue;
        }
        for(u32 _j = 0; _j < _n; _j++) {
            _again = _again || _records[_j] == _record;
        }
        if(!_again) {
            _records[_n++] = _record;
        }
    }
    if(_n == 0u) {
        fude_notice_show(fude_text(FUDE_TEXT_TRANSLATOR_NOTHING));
        return;
    }
    fude_study_practice_set(_app, _records, _n);
}

// The one row; the enum is its button indices.
enum { FUDE_TRANSLATOR_ROW_BACK = 0, FUDE_TRANSLATOR_ROW_SAVE, FUDE_TRANSLATOR_ROW_WRITE, FUDE_TRANSLATOR_ROW_PRACTICE };
static const fude_row_button FUDE_TRANSLATOR_BUTTONS[] = {
    { FUDE_TEXT_BACK,       FUDE_ICON_BACK,     fude_translator_row_back,     0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_SAVE,       FUDE_ICON_BOOKMARK, fude_translator_row_save,     0, FUDE_ROW_PRIMARY, false, NULL },
    { FUDE_TEXT_SCAN_WRITE, FUDE_ICON_DRAW,     fude_translator_row_write,    0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_PRACTICE,   FUDE_ICON_PEN,      fude_translator_row_practice, 0, FUDE_ROW_QUIET,   false, NULL },
};
static const fude_row_def FUDE_TRANSLATOR_BUTTON_ROWS[] = { FUDE_ROW_DEF(FUDE_TRANSLATOR_BUTTONS) };

// Which row is up: there is only the one.
RDE_INTERNAL u32 fude_translator_screen_row(const any _self) {
    RDE_UNUSED(_self);
    return 0u;
}

// How the buttons show: Save, Write and Practice act on the card chosen, so they are greyed out until its translation
// is in.
RDE_INTERNAL void fude_translator_screen_faces(const any _self, u32 _row, fude_row_face* _faces) {
    RDE_UNUSED(_row);
    const b8 _none = fude_translator_chosen((const fude_translator*)_self) == NULL;
    _faces[FUDE_TRANSLATOR_ROW_SAVE].disabled     = _none;
    _faces[FUDE_TRANSLATOR_ROW_WRITE].disabled    = _none;
    _faces[FUDE_TRANSLATOR_ROW_PRACTICE].disabled = _none;
}

const fude_screen FUDE_TRANSLATOR_SCREEN = {
    .name = "translator", .input = FUDE_SCREEN_INPUT_POINT,
    .is_open = fude_translator_screen_is_open, .close = fude_translator_screen_close,
    .update = fude_translator_screen_update, .render = fude_translator_screen_render,
    .pointer_down = fude_translator_screen_down, .pointer_moved = fude_translator_screen_moved, .pointer_up = fude_translator_screen_up,
    .rows = FUDE_TRANSLATOR_BUTTON_ROWS, .row_count = 1u, .row = fude_translator_screen_row, .faces = fude_translator_screen_faces,
    .field_hint = FUDE_TEXT_TRANSLATOR_HINT, .field_max = FUDE_TRANSLATOR_INPUT - 1u, .field_submit = fude_translator_screen_field,
};
