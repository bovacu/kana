#include "study/widgets/pagetext.h"
#include "drawing/app/page.h"
#include "study/app/study.h"
#include "drawing/base/utf8.h"
#include "drawing/app/app.h"
#include "drawing/app/ui.h"
#include "drawing/widgets/kit.h"
#include "drawing/widgets/draw.h"
#include "drawing/widgets/icons.h"
#include "drawing/widgets/pagemenu.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/widgets/notice.h"
#include "study/services/mlkit.h"
#include "study/services/speech.h"
#include "study/models/vocab.h"
#include "study/widgets/wordcard.h"
#include "study/services/textscan.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See pagetext.h.
// ===========================================================================

#define FUDE_PAGETEXT_GAP          6.0f    // between the selection's box and its menu (pagemenu.c's)
#define FUDE_PAGETEXT_TEXT_CELL    72.0f   // Paste text: a character's cell, screen points at the zoom it is pasted at
#define FUDE_PAGETEXT_TEXT_WIDTH   0.8f    // ...and its lines no wider than this much of the screen
#define FUDE_PAGETEXT_NOTICE_CHARS 16u     // Copy as text says what it copied up to this long; more, how many
// Translate with Google's card by the selection.
#define FUDE_PAGETEXT_CARD_W      440.0f   // at most (narrower screens: what fits)
#define FUDE_PAGETEXT_CARD_PAD    14.0f
#define FUDE_PAGETEXT_CARD_FROM   16.0f    // what was read: size and line
#define FUDE_PAGETEXT_CARD_FROM_L 22.0f
#define FUDE_PAGETEXT_CARD_TO     18.0f    // its translation
#define FUDE_PAGETEXT_CARD_TO_L   25.0f
#define FUDE_PAGETEXT_CARD_WORD   26.0f    // a word's row under the translation

// The study's (a button's press is handed the app).
#define FUDE_PAGETEXT(_app) (&FUDE_STUDY(_app)->text)

RDE_INTERNAL void fude_pagetext_on_copy_text(fude_app* _app, void* _self, u32 _arg);

// --- reading the selection ----------------------------------------------------------

// Which selection: its strokes, as one number (a new lasso, or strokes added or
// taken away, gives another). The card is for the one it was read from.
RDE_INTERNAL u32 fude_pagetext_selection_key(const fude_pagetext* _text) {
    const fude_lasso* _lasso = _text->app->lasso;
    const u32         _n     = fude_lasso_count(_lasso);
    const u32*        _ids   = (const u32*)_lasso->selected.memory;
    rde_vec_2F        _min, _max;
    if(_n == 0u && fude_lasso_area(_lasso, &_min, &_max)) {
        // An area of a PDF's text: its box, as one number.
        const f32 _box[4] = { _min.x, _min.y, _max.x, _max.y };
        u32       _bits[4];
        memcpy(_bits, _box, sizeof(_bits));
        return ((((2166136261u ^ _bits[0]) * 16777619u ^ _bits[1]) * 16777619u ^ _bits[2]) * 16777619u ^ _bits[3]) | 1u;
    }
    u32               _key   = 2166136261u ^ _n;
    for(u32 _i = 0; _i < _n; _i++) {
        _key = (_key ^ _ids[_i]) * 16777619u;
    }
    return _n == 0 ? 0u : (_key | 1u);
}

// The selection read (textink.h), for _what: false when a reading cannot start
// now (one under way, nothing selected).
RDE_INTERNAL b8 fude_pagetext_read(fude_pagetext* _text, FUDE_PAGETEXT_READ_ _what) {
    fude_app* _app = _text->app;
    if(fude_lasso_count(_app->lasso) == 0) {
        // An area of a PDF's text: read from the PDF, no handwriting to recognise.
        rde_vec_2F _min, _max;
        if(!fude_lasso_area(_app->lasso, &_min, &_max) || _text->reading != FUDE_PAGETEXT_READ_NONE) {
            return false;
        }
        fude_doc_text_in(&_app->page->doc, _min, _max, _text->area_text, sizeof(_text->area_text));
        _text->area_ready = true;
        _text->reading    = (u8)_what;
        return true;
    }
    if(!_text->_reader_ready) {
        fude_textink_reader_init(&_text->reader, FUDE_STUDY(_app)->db, FUDE_STUDY(_app)->catalog);
        _text->_reader_ready = true;
    }
    if(!fude_textink_read(&_text->reader, _app->ink, (const u32*)_app->lasso->selected.memory, fude_lasso_count(_app->lasso))) {
        return false;
    }
    _text->reading = (u8)_what;
    return true;
}

void fude_pagetext_save_word(fude_pagetext* _text) {
    fude_pagetext_read(_text, FUDE_PAGETEXT_READ_VOCAB);
}

void fude_pagetext_translate(fude_pagetext* _text) {
    rde_vec_2F _min, _max;
    if((fude_lasso_count(_text->app->lasso) == 0 && !fude_lasso_area(_text->app->lasso, &_min, &_max)) || !fude_translate_available()) {
        return;
    }
    if(!fude_mlkit_enabled()) {
        fude_notice_show(fude_text(FUDE_TEXT_SEL_TRANSLATE_MLKIT_OFF));
        return;
    }
    if(fude_pagetext_read(_text, FUDE_PAGETEXT_READ_TRANSLATE)) {
        _text->card          = FUDE_PAGETEXT_CARD_READING;
        _text->card_of       = fude_pagetext_selection_key(_text);
        _text->card_prepared = false;
        _text->card_from[0]  = 0;
        _text->card_to[0]    = 0;
        _text->word_count    = 0;
    }
}

// What was copied, in a notice: itself when it is short (on one line), otherwise how much.
RDE_INTERNAL void fude_pagetext_notice_copied(const c8* _copied) {
    u32       _chars = 0;
    c8        _one_line[sizeof(((fude_textink_reader*)NULL)->text)];
    usize     _n     = 0;
    const c8* _p     = _copied;
    for(;;) {
        const c8* _from = _p;
        const u32 _cp   = fude_utf8_next(&_p);
        if(_cp == 0) {
            break;
        }
        _chars++;
        if(_cp == '\n') {
            _one_line[_n++] = ' ';
        } else {
            memcpy(&_one_line[_n], _from, (usize)(_p - _from));
            _n += (usize)(_p - _from);
        }
    }
    _one_line[_n] = 0;
    c8 _line[256];
    if(_chars <= FUDE_PAGETEXT_NOTICE_CHARS) {
        FUDE_TEXTF(_line, FUDE_TEXT_NOTICE_COPIED_TEXT, FUDE_TS(_one_line));
    } else {
        FUDE_TEXTF(_line, FUDE_TEXT_NOTICE_COPIED_N, FUDE_TN(_chars));
    }
    fude_notice_show(_line);
}

// Once a frame: a reading done goes where it was for.
RDE_INTERNAL void fude_pagetext_update_reading(fude_pagetext* _text) {
    fude_study* _study = FUDE_STUDY(_text->app);
    const c8*   _read  = NULL;
    if(_text->area_ready) {
        _text->area_ready = false;
        _read             = _text->area_text;   // a PDF's text: read already
    } else {
        if(!_text->_reader_ready || !fude_textink_update(&_text->reader)) {
            return;
        }
        _read = _text->reader.text;
    }
    const u8  _what = _text->reading;
    _text->reading  = FUDE_PAGETEXT_READ_NONE;
    if(_what == FUDE_PAGETEXT_READ_TRANSLATE && _text->card != FUDE_PAGETEXT_CARD_READING) {
        return;   // the selection went since
    }
    if(_read[0] == 0) {
        if(_what == FUDE_PAGETEXT_READ_TRANSLATE) {
            _text->card = FUDE_PAGETEXT_CARD_NONE;
        }
        fude_notice_show(fude_text(FUDE_TEXT_NOTICE_NOTHING_READ));
        return;
    }
    switch(_what) {
        case FUDE_PAGETEXT_READ_VOCAB:
            fude_wordcard_ask_read(_study->db, _read);   // a word of the dictionary, or the words in it
            break;
        case FUDE_PAGETEXT_READ_TRANSLATE:
            snprintf(_text->card_from, sizeof(_text->card_from), "%s", _read);
            _text->card       = FUDE_PAGETEXT_CARD_GETTING;
            _text->word_count = _study->db != NULL ? fude_wordsplit(_study->db, _read, _text->words, FUDE_WORDSPLIT_MAX) : 0u;
            break;
        default:
            rde_engine_set_clipboard(_read);
            fude_pagemenu_copied(&_text->app->ui->page, fude_pagetext_on_copy_text);
            fude_pagetext_notice_copied(_read);
            break;
    }
}

// Once a frame: the card goes with its selection; otherwise its next step — the
// models asked for (the first time), the text sent, the answer taken.
RDE_INTERNAL void fude_pagetext_update_card(fude_pagetext* _text) {
    if(_text->card == FUDE_PAGETEXT_CARD_NONE) {
        return;
    }
    if(fude_pagetext_selection_key(_text) != _text->card_of) {
        _text->card = FUDE_PAGETEXT_CARD_NONE;
        return;
    }
    const c8* _to = fude_translate_target();
    if(_text->card == FUDE_PAGETEXT_CARD_GETTING) {
        const FUDE_TRANSLATE_STATE_ _state = fude_translate_state("ja", _to);
        if(_state == FUDE_TRANSLATE_READY) {
            _text->card_ticket = fude_translate_text(_text->card_from, "ja", _to);
            _text->card        = _text->card_ticket != 0u ? FUDE_PAGETEXT_CARD_ASKED : FUDE_PAGETEXT_CARD_DONE;
        } else if(_state == FUDE_TRANSLATE_MISSING || (_state == FUDE_TRANSLATE_FAILED && !_text->card_prepared)) {
            if(!_text->card_prepared) {
                _text->card_prepared = true;
                fude_translate_prepare("ja", _to);
            }
        } else if(_state == FUDE_TRANSLATE_FAILED || _state == FUDE_TRANSLATE_UNAVAILABLE) {
            _text->card = FUDE_PAGETEXT_CARD_FAILED;
        }
    }
    if(_text->card == FUDE_PAGETEXT_CARD_ASKED && fude_translate_take(_text->card_ticket, _text->card_to, sizeof(_text->card_to))) {
        _text->card = FUDE_PAGETEXT_CARD_DONE;
    }
}

// --- its buttons ----------------------------------------------------------------------

// Copy as text: the selection read; the text goes to the system clipboard when
// the reading is done.
RDE_INTERNAL void fude_pagetext_on_copy_text(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_pagetext_read(FUDE_PAGETEXT(_app), FUDE_PAGETEXT_READ_COPY);
}

RDE_INTERNAL void fude_pagetext_on_translate(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_pagetext_translate(FUDE_PAGETEXT(_app));
}

// Save word: the selection read (written by hand, or pasted text), then the word
// card with what it says.
RDE_INTERNAL void fude_pagetext_on_save_word(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_pagetext_save_word(FUDE_PAGETEXT(_app));
}

// Check: the selection read and checked (a character, or several).
RDE_INTERNAL void fude_pagetext_on_check(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_lasso_sync(_app->lasso, _app->ink);
    fude_check_open(FUDE_STUDY(_app)->check, _app->ink, (const u32*)_app->lasso->selected.memory, (u32)rde_arr_length(&_app->lasso->selected));
}

// Paste text: the system clipboard's text, where the menu was opened.
RDE_INTERNAL void fude_pagetext_on_paste_text(fude_app* _app, void* _self, u32 _arg) {
    fude_pagemenu* _menu = (fude_pagemenu*)_self;
    RDE_UNUSED(_arg);
    fude_pagemenu_close_context(_menu);
    c8* _clipboard = rde_engine_get_clipboard();
    if(_clipboard == NULL) {
        fude_notice_show(fude_text(FUDE_TEXT_NOTICE_NO_TEXT));
        return;
    }
    fude_pagetext_write(FUDE_PAGETEXT(_app), _clipboard, _menu->context_canvas);
    rde_engine_free_clipboard_str_ptr(_clipboard);
}

// Text from a photo: its lines are written where the menu was opened.
RDE_INTERNAL void fude_pagetext_on_scan(fude_app* _app, void* _self, u32 _arg) {
    fude_pagemenu* _menu = (fude_pagemenu*)_self;
    RDE_UNUSED(_arg);
    fude_pagemenu_close_context(_menu);
    fude_scan_open(FUDE_STUDY(_app)->scan, _app->window, _menu->context_canvas);
}

static const fude_row_button FUDE_PAGETEXT_SELECTION[] = {
    FUDE_PAGEMENU_BUTTON_CUT,
    FUDE_PAGEMENU_BUTTON_COPY,
    { FUDE_TEXT_SEL_COPY_TEXT,  FUDE_ICON_TEXT_COPY, fude_pagetext_on_copy_text, 0, FUDE_ROW_QUIET, false, NULL },
    { FUDE_TEXT_SCAN_TRANSLATE, FUDE_ICON_TRANSLATE, fude_pagetext_on_translate, 0, FUDE_ROW_QUIET, false, fude_translate_available },
    { FUDE_TEXT_SEL_SAVE_WORD,  FUDE_ICON_BOOKMARK,  fude_pagetext_on_save_word, 0, FUDE_ROW_QUIET, false, NULL },
    FUDE_PAGEMENU_BUTTON_DUPLICATE,
    { FUDE_TEXT_SEL_CHECK,      FUDE_ICON_SEARCH,    fude_pagetext_on_check,     0, FUDE_ROW_QUIET, false, NULL },
    FUDE_PAGEMENU_BUTTON_DELETE,
};
const fude_row_def FUDE_PAGETEXT_SELECTION_ROW = FUDE_ROW_DEF(FUDE_PAGETEXT_SELECTION);

static const fude_row_button FUDE_PAGETEXT_CONTEXT[] = {
    FUDE_PAGEMENU_BUTTON_PASTE,
    { FUDE_TEXT_CTX_PASTE_TEXT, FUDE_ICON_TEXT_PASTE, fude_pagetext_on_paste_text, 0, FUDE_ROW_QUIET, false, NULL },
    { FUDE_TEXT_CTX_SCAN,       FUDE_ICON_SCAN,       fude_pagetext_on_scan,       0, FUDE_ROW_QUIET, false, NULL },
    FUDE_PAGEMENU_BUTTON_SELECT_ALL,
};
const fude_row_def FUDE_PAGETEXT_CONTEXT_ROW = FUDE_ROW_DEF(FUDE_PAGETEXT_CONTEXT);

// Over a PDF's text (no ink to cut, move or check): its text's buttons.
static const fude_row_button FUDE_PAGETEXT_TEXT[] = {
    { FUDE_TEXT_SEL_COPY_TEXT,  FUDE_ICON_TEXT_COPY, fude_pagetext_on_copy_text, 0, FUDE_ROW_QUIET, false, NULL },
    { FUDE_TEXT_SCAN_TRANSLATE, FUDE_ICON_TRANSLATE, fude_pagetext_on_translate, 0, FUDE_ROW_QUIET, false, fude_translate_available },
    { FUDE_TEXT_SEL_SAVE_WORD,  FUDE_ICON_BOOKMARK,  fude_pagetext_on_save_word, 0, FUDE_ROW_QUIET, false, NULL },
};
const fude_row_def FUDE_PAGETEXT_TEXT_ROW = FUDE_ROW_DEF(FUDE_PAGETEXT_TEXT);

// Copy as text and Save word say "Reading…" while the selection is read for them.
void fude_pagetext_selection_faces(const fude_pagetext* _text, const fude_row_def* _row, fude_row_face* _faces) {
    const u32 _for = _text->reading == FUDE_PAGETEXT_READ_COPY  ? fude_row_def_find(_row, fude_pagetext_on_copy_text) :
                     _text->reading == FUDE_PAGETEXT_READ_VOCAB ? fude_row_def_find(_row, fude_pagetext_on_save_word) : FUDE_ROW_NONE;
    if(_for != FUDE_ROW_NONE) {
        snprintf(_faces[_for].label, FUDE_ROW_LABEL, "%s", fude_text(FUDE_TEXT_SEL_READING));
    }
}

// Asks only whether there is text (iOS says nothing): reading it is Paste text's.
void fude_pagetext_context_faces(const fude_pagetext* _text, const fude_row_def* _row, fude_row_face* _faces) {
    RDE_UNUSED(_text);
    const u32 _paste_text = fude_row_def_find(_row, fude_pagetext_on_paste_text);
    const u32 _scan       = fude_row_def_find(_row, fude_pagetext_on_scan);
    if(_paste_text != FUDE_ROW_NONE) { _faces[_paste_text].disabled = rde_engine_is_clipboard_empty(); }
    if(_scan != FUDE_ROW_NONE)       { _faces[_scan].disabled       = !fude_textscan_available(); }
}

// --- writing text on the page ----------------------------------------------------------

void fude_pagetext_write(fude_pagetext* _text, const c8* _utf8, rde_vec_2F _canvas) {
    fude_app* _app   = _text->app;
    const f32 _zoom  = _app->canvas->view.zoom > 0.0f ? _app->canvas->view.zoom : 1.0f;
    const f32 _width = (f32)rde_window_get_size(_app->window).x * FUDE_PAGETEXT_TEXT_WIDTH;
    const fude_textink_result _r = fude_textink_write(FUDE_STUDY(_app)->db, _utf8, FUDE_PAGETEXT_TEXT_CELL / _zoom, _width / _zoom,
                                                      fude_ink_pen_radius(_app->ink, 0.5f), _app->ink->color, &_text->_text_clip);
    if(_r.drawn == 0) {
        fude_notice_show(fude_text(FUDE_TEXT_NOTICE_NOTHING_TO_WRITE));
        return;
    }
    // What was written comes in selected, ready to drag: the Lasso's job, as Paste.
    fude_toolbar_set_tool(&_app->ui->bar, FUDE_TOOL_LASSO);
    fude_lasso_paste_clip(_app->lasso, _app->ink, &_text->_text_clip, _canvas);
    if(_r.skipped > 0) {
        c8 _line[192];
        FUDE_TEXTF(_line, FUDE_TEXT_NOTICE_LEFT_OUT, FUDE_TN(_r.skipped));
        fude_notice_show(_line);
    }
}

// --- the card ----------------------------------------------------------------------------

void fude_pagetext_render(fude_pagetext* _text, rde_window* _window) {
    fude_app*  _app = _text->app;
    rde_vec_2F _min, _max;
    if(_text->card == FUDE_PAGETEXT_CARD_NONE || _app->font == NULL || !fude_lasso_box(_app->lasso, _app->ink, &_min, &_max)) {
        _text->card_min = _text->card_max = (rde_vec_2F){ 0.0f, 0.0f };
        return;
    }
    const fude_theme* _t       = fude_theme_active();
    rde_font* const   _font    = _app->font;
    const f32         _font_px = _app->font_px;
    const rde_vec_2I  _size    = rde_window_get_size(_window);
    const rde_vec_4I  _insets  = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32         _sl      = -(f32)_size.x * 0.5f + (f32)_insets.x + FUDE_KIT_SCREEN_EDGE;
    const f32         _sr      = (f32)_size.x * 0.5f - (f32)_insets.z - FUDE_KIT_SCREEN_EDGE;
    const f32         _st      = (f32)_size.y * 0.5f - (f32)_insets.y - FUDE_KIT_SCREEN_EDGE;
    const f32         _sb      = -(f32)_size.y * 0.5f + (f32)_insets.w + FUDE_KIT_SCREEN_EDGE;

    // What it says: what was read, and its translation — or how far it got.
    const c8* _from = _text->card == FUDE_PAGETEXT_CARD_READING ? fude_text(FUDE_TEXT_SEL_READING) : _text->card_from;
    const c8* _to   = NULL;
    rde_color _to_c = _t->text_soft;
    switch(_text->card) {
        case FUDE_PAGETEXT_CARD_GETTING:
            _to = fude_text(fude_translate_state("ja", fude_translate_target()) == FUDE_TRANSLATE_DOWNLOADING ? FUDE_TEXT_SCAN_TRANSLATE_GETTING : FUDE_TEXT_SCAN_TRANSLATING);
            break;
        case FUDE_PAGETEXT_CARD_ASKED:  _to = fude_text(FUDE_TEXT_SCAN_TRANSLATING); break;
        case FUDE_PAGETEXT_CARD_FAILED: _to = fude_text(FUDE_TEXT_SCAN_TRANSLATE_FAILED); _to_c = _t->score_poor; break;
        case FUDE_PAGETEXT_CARD_DONE:
            _to   = _text->card_to[0] != 0 ? _text->card_to : fude_text(FUDE_TEXT_SEL_TRANSLATION_NONE);
            _to_c = _text->card_to[0] != 0 ? _t->text : _t->text_soft;
            break;
        default: break;
    }

    // Its size: as wide as it may be, as tall as its lines.
    const f32 _w      = fminf(FUDE_PAGETEXT_CARD_W, _sr - _sl);
    const f32 _inner  = _w - 2.0f * FUDE_PAGETEXT_CARD_PAD;
    const u32 _from_n = fude_draw_text_wrap_lines(_font, _font_px, _from, FUDE_PAGETEXT_CARD_FROM, _inner);
    const u32 _to_n   = _to != NULL ? fude_draw_text_wrap_lines(_font, _font_px, _to, FUDE_PAGETEXT_CARD_TO, _inner) : 0u;
    const u32 _words  = _text->card != FUDE_PAGETEXT_CARD_READING ? _text->word_count : 0u;
    const f32 _h      = 2.0f * FUDE_PAGETEXT_CARD_PAD + FUDE_TRANSLATE_BADGE_H + 10.0f + (f32)_from_n * FUDE_PAGETEXT_CARD_FROM_L +
                        (_to_n > 0 ? 6.0f + (f32)_to_n * FUDE_PAGETEXT_CARD_TO_L : 0.0f) + (_words > 0 ? 8.0f + (f32)_words * FUDE_PAGETEXT_CARD_WORD : 0.0f);

    // Where: on the selection's other side from its menu (the menu goes above
    // when there is room), centred on it, kept on screen.
    const fude_view* _view    = &_app->canvas->view;
    const f32        _box_t   = _max.y * _view->zoom + _view->offset.y + FUDE_LASSO_BOX_PAD;
    const f32        _box_b   = _min.y * _view->zoom + _view->offset.y - FUDE_LASSO_BOX_PAD;
    const f32        _cx      = (_min.x + _max.x) * 0.5f * _view->zoom + _view->offset.x;
    const b8         _menu_up = _box_t + FUDE_PAGETEXT_GAP + _app->ui->page.selection.size.y <= _st;
    f32              _top     = _menu_up ? _box_b - FUDE_PAGETEXT_GAP : _box_t + FUDE_PAGETEXT_GAP + _h;
    _top = fmaxf(fminf(_top, _st), _sb + _h);
    const f32 _left = fmaxf(fminf(_cx - _w * 0.5f, _sr - _w), _sl);
    _text->card_min = (rde_vec_2F){ _left, _top - _h };
    _text->card_max = (rde_vec_2F){ _left + _w, _top };

    fude_draw_card(_text->card_min, _text->card_max, 16.0f, _t->surface, _t->outline);
    const rde_color _s = _t->surface;
    f32             _y = _top - FUDE_PAGETEXT_CARD_PAD - FUDE_TRANSLATE_BADGE_H * 0.5f;
    // With a voice and something read: a speaker at the top right says it.
    if(fude_speech_available() && _text->card != FUDE_PAGETEXT_CARD_READING) {
        const rde_vec_2F _c = { _left + _w - FUDE_PAGETEXT_CARD_PAD - 10.0f, _y - 6.0f };
        fude_draw_icon(_font, _font_px, FUDE_ICON_SPEAK, _c, 20.0f, _t->accent);
        _text->speak_min = (rde_vec_2F){ _c.x - 26.0f, _c.y - 22.0f };
        _text->speak_max = (rde_vec_2F){ _c.x + 22.0f, _c.y + 22.0f };
    } else {
        _text->speak_min = _text->speak_max = (rde_vec_2F){ 0.0f, 0.0f };
    }
    fude_translate_draw_badge(_left + FUDE_PAGETEXT_CARD_PAD, _y, 0.299f * (f32)_s.r + 0.587f * (f32)_s.g + 0.114f * (f32)_s.b < 128.0f);
    _y -= FUDE_TRANSLATE_BADGE_H * 0.5f + 10.0f;
    fude_draw_text_wrap(_font, _font_px, _from, _left + FUDE_PAGETEXT_CARD_PAD, _y - 16.0f, FUDE_PAGETEXT_CARD_FROM, _inner, FUDE_PAGETEXT_CARD_FROM_L, _t->text_soft);
    _y -= (f32)_from_n * FUDE_PAGETEXT_CARD_FROM_L + 6.0f;
    if(_to != NULL) {
        fude_draw_text_wrap(_font, _font_px, _to, _left + FUDE_PAGETEXT_CARD_PAD, _y - 18.0f, FUDE_PAGETEXT_CARD_TO, _inner, FUDE_PAGETEXT_CARD_TO_L, _to_c);
        _y -= (f32)_to_n * FUDE_PAGETEXT_CARD_TO_L;
    }
    // Its words: the bookmark (filled: the learner's), the word, its reading, its meaning.
    _y -= 8.0f;
    _text->words_top   = _y;
    _text->words_left  = _left;
    _text->words_right = _left + _w;
    for(u32 _k = 0; _k < _words && FUDE_STUDY(_app)->db != NULL; _k++) {
        fude_kanji_word _word;
        if(!fude_kanji_word_at(FUDE_STUDY(_app)->db, _text->words[_k], &_word)) {
            continue;
        }
        const f32 _mid    = _y - ((f32)_k + 0.5f) * FUDE_PAGETEXT_CARD_WORD;
        const b8  _theirs = fude_vocab_find(_word.written, _word.reading) != 0u;
        if(_theirs) {
            fude_draw_icon_fill(FUDE_ICON_BOOKMARK, (rde_vec_2F){ _left + FUDE_PAGETEXT_CARD_PAD + 9.0f, _mid }, 16.0f, _t->accent);
        } else {
            fude_draw_icon(_font, _font_px, FUDE_ICON_BOOKMARK, (rde_vec_2F){ _left + FUDE_PAGETEXT_CARD_PAD + 9.0f, _mid }, 16.0f, _t->accent);
        }
        c8 _say[400];
        snprintf(_say, sizeof(_say), "%s  %s  ·  %s", _word.written, _word.reading, _word.meaning);
        const f32 _px = fude_draw_text_px_to_fit(_font, _font_px, _say, 14.0f, _inner - 28.0f, 0.55f);
        fude_draw_text(_font, _font_px, _say, _left + FUDE_PAGETEXT_CARD_PAD + 28.0f, _mid - _px * 0.36f, _px, _theirs ? _t->text : _t->text_soft);
    }
}

b8 fude_pagetext_press(fude_pagetext* _text, rde_vec_2F _screen) {
    const fude_kanji_db* _db = FUDE_STUDY(_text->app)->db;
    // A word's row: the word card (wordcard.h), to save it or see it saved.
    if(_text->card != FUDE_PAGETEXT_CARD_NONE && _text->card != FUDE_PAGETEXT_CARD_READING && _db != NULL &&
       _screen.x >= _text->words_left && _screen.x <= _text->words_right && _screen.y <= _text->words_top) {
        const i32 _k = (i32)floorf((_text->words_top - _screen.y) / FUDE_PAGETEXT_CARD_WORD);
        fude_kanji_word _w;
        if(_k >= 0 && (u32)_k < _text->word_count && fude_kanji_word_at(_db, _text->words[_k], &_w)) {
            fude_wordcard_ask_in(_w.written, _w.reading, _w.meaning, _text->card_from, _text->card == FUDE_PAGETEXT_CARD_DONE ? _text->card_to : "");
            return true;
        }
    }
    if(_text->card == FUDE_PAGETEXT_CARD_NONE || _text->card_from[0] == 0 ||
       _screen.x < _text->speak_min.x || _screen.x > _text->speak_max.x || _screen.y < _text->speak_min.y || _screen.y > _text->speak_max.y) {
        return false;
    }
    fude_speak(_text->card_from);
    return true;
}

b8 fude_pagetext_hit(const fude_pagetext* _text, rde_vec_2F _screen) {
    // The card is kept in the app's screen space.
    return _text->card != FUDE_PAGETEXT_CARD_NONE && _screen.x >= _text->card_min.x && _screen.x <= _text->card_max.x &&
           _screen.y >= _text->card_min.y && _screen.y <= _text->card_max.y;
}

// --- lifetime ------------------------------------------------------------------------------

void fude_pagetext_init(fude_pagetext* _text, fude_app* _app) {
    memset(_text, 0, sizeof(*_text));
    _text->app = _app;
}

void fude_pagetext_destroy(fude_pagetext* _text) {
    if(_text->_reader_ready) {
        fude_textink_reader_destroy(&_text->reader);
        _text->_reader_ready = false;
    }
    rde_arr* const _clip[] = { &_text->_text_clip.strokes, &_text->_text_clip.points };
    for(u32 _i = 0; _i < 2u; _i++) {
        if(rde_arr_is_inited(_clip[_i])) {
            rde_arr_free(_clip[_i]);
        }
    }
}

void fude_pagetext_update(fude_pagetext* _text) {
    fude_pagetext_update_reading(_text);
    fude_pagetext_update_card(_text);
}
