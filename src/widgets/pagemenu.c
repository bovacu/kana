#include "widgets/pagemenu.h"
#include "app/app.h"
#include "app/ui.h"
#include "widgets/kit.h"
#include "widgets/draw.h"
#include "widgets/icons.h"
#include "base/text.h"
#include "base/theme.h"
#include "widgets/notice.h"
#include "services/mlkit.h"
#include "services/speech.h"
#include "study/vocab.h"
#include "widgets/wordcard.h"
#include "services/textscan.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See pagemenu.h.
// ===========================================================================

#define KANA_PAGEMENU_GAP          6.0f    // between the selection's box and its menu
#define KANA_PAGEMENU_CONTEXT_LIFT 56.0f   // the context menu sits this far above the finger
#define KANA_PAGEMENU_COPIED_TIME  1.2     // seconds Copy reads "Copied"
#define KANA_PAGEMENU_TEXT_CELL    72.0f   // Paste text: a character's cell, screen points at the zoom it is pasted at
#define KANA_PAGEMENU_TEXT_WIDTH   0.8f    // ...and its lines no wider than this much of the screen
#define KANA_PAGEMENU_NOTICE_CHARS 16u     // Copy as text says what it copied up to this long; more, how many
// Translate with Google's card by the selection.
#define KANA_PAGEMENU_CARD_W      440.0f   // at most (narrower screens: what fits)
#define KANA_PAGEMENU_CARD_PAD    14.0f
#define KANA_PAGEMENU_CARD_FROM   16.0f    // what was read: size and line
#define KANA_PAGEMENU_CARD_FROM_L 22.0f
#define KANA_PAGEMENU_CARD_TO     18.0f    // its translation
#define KANA_PAGEMENU_CARD_TO_L   25.0f
#define KANA_PAGEMENU_CARD_WORD   26.0f    // a word's row under the translation

// The selection menu's buttons, in order.
enum { KANA_SEL_CUT = 0, KANA_SEL_COPY, KANA_SEL_COPY_TEXT, KANA_SEL_TRANSLATE, KANA_SEL_SAVE_WORD, KANA_SEL_DUPLICATE, KANA_SEL_CHECK, KANA_SEL_DELETE };

// --- reading the selection ----------------------------------------------------------

// Which selection: its strokes, as one number (a new lasso, or strokes added or
// taken away, gives another). The card is for the one it was read from.
RDE_INTERNAL u32 kana_pagemenu_selection_key(const kana_pagemenu* _menu) {
    const kana_lasso* _lasso = _menu->app->lasso;
    const u32         _n     = kana_lasso_count(_lasso);
    const u32*        _ids   = (const u32*)_lasso->selected.memory;
    u32               _key   = 2166136261u ^ _n;
    for(u32 _i = 0; _i < _n; _i++) {
        _key = (_key ^ _ids[_i]) * 16777619u;
    }
    return _n == 0 ? 0u : (_key | 1u);
}

// The selection read (textink.h), for _what: false when a reading cannot start
// now (one under way, nothing selected).
RDE_INTERNAL b8 kana_pagemenu_read(kana_pagemenu* _menu, KANA_PAGEMENU_READ_ _what) {
    kana_app* _app = _menu->app;
    if(kana_lasso_count(_app->lasso) == 0) {
        return false;
    }
    if(!_menu->_reader_ready) {
        kana_textink_reader_init(&_menu->reader, _app->db, _app->catalog);
        _menu->_reader_ready = true;
    }
    if(!kana_textink_read(&_menu->reader, _app->ink, (const u32*)_app->lasso->selected.memory, kana_lasso_count(_app->lasso))) {
        return false;
    }
    _menu->reading = (u8)_what;
    return true;
}

void kana_pagemenu_save_word(kana_pagemenu* _menu) {
    kana_pagemenu_read(_menu, KANA_PAGEMENU_READ_VOCAB);
}

void kana_pagemenu_translate(kana_pagemenu* _menu) {
    if(kana_lasso_count(_menu->app->lasso) == 0 || !kana_translate_available()) {
        return;
    }
    if(!kana_mlkit_enabled()) {
        kana_notice_show(kana_text(KANA_TEXT_SEL_TRANSLATE_MLKIT_OFF));
        return;
    }
    if(kana_pagemenu_read(_menu, KANA_PAGEMENU_READ_TRANSLATE)) {
        _menu->card          = KANA_PAGEMENU_CARD_READING;
        _menu->card_of       = kana_pagemenu_selection_key(_menu);
        _menu->card_prepared = false;
        _menu->card_from[0]  = 0;
        _menu->card_to[0]    = 0;
        _menu->word_count    = 0;
    }
}

// What was copied, in a notice: itself when it is short (on one line), otherwise how much.
RDE_INTERNAL void kana_pagemenu_notice_copied(const c8* _text) {
    u32       _chars = 0;
    c8        _one_line[sizeof(((kana_textink_reader*)NULL)->text)];
    usize     _n     = 0;
    const c8* _p     = _text;
    for(;;) {
        const c8* _from = _p;
        const u32 _cp   = kana_kanji_utf8_next(&_p);
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
    if(_chars <= KANA_PAGEMENU_NOTICE_CHARS) {
        KANA_TEXTF(_line, KANA_TEXT_NOTICE_COPIED_TEXT, KANA_TS(_one_line));
    } else {
        KANA_TEXTF(_line, KANA_TEXT_NOTICE_COPIED_N, KANA_TN(_chars));
    }
    kana_notice_show(_line);
}

// Once a frame: a reading done goes where it was for.
RDE_INTERNAL void kana_pagemenu_update_reading(kana_pagemenu* _menu) {
    if(!_menu->_reader_ready || !kana_textink_update(&_menu->reader)) {
        return;
    }
    const c8* _text = _menu->reader.text;
    const u8  _what = _menu->reading;
    _menu->reading  = KANA_PAGEMENU_READ_NONE;
    if(_what == KANA_PAGEMENU_READ_TRANSLATE && _menu->card != KANA_PAGEMENU_CARD_READING) {
        return;   // the selection went since
    }
    if(_text[0] == 0) {
        if(_what == KANA_PAGEMENU_READ_TRANSLATE) {
            _menu->card = KANA_PAGEMENU_CARD_NONE;
        }
        kana_notice_show(kana_text(KANA_TEXT_NOTICE_NOTHING_READ));
        return;
    }
    switch(_what) {
        case KANA_PAGEMENU_READ_VOCAB:
            kana_wordcard_ask_read(_menu->app->db, _text);   // a word of the dictionary, or the words in it
            break;
        case KANA_PAGEMENU_READ_TRANSLATE:
            snprintf(_menu->card_from, sizeof(_menu->card_from), "%s", _text);
            _menu->card       = KANA_PAGEMENU_CARD_GETTING;
            _menu->word_count = _menu->app->db != NULL ? kana_wordsplit(_menu->app->db, _text, _menu->words, KANA_WORDSPLIT_MAX) : 0u;
            break;
        default:
            rde_engine_set_clipboard(_text);
            _menu->copied       = KANA_SEL_COPY_TEXT;
            _menu->copied_until = rde_engine_get_time_now() + KANA_PAGEMENU_COPIED_TIME;
            kana_pagemenu_notice_copied(_text);
            break;
    }
}

// Once a frame: the card goes with its selection; otherwise its next step — the
// models asked for (the first time), the text sent, the answer taken.
RDE_INTERNAL void kana_pagemenu_update_card(kana_pagemenu* _menu) {
    if(_menu->card == KANA_PAGEMENU_CARD_NONE) {
        return;
    }
    if(kana_pagemenu_selection_key(_menu) != _menu->card_of) {
        _menu->card = KANA_PAGEMENU_CARD_NONE;
        return;
    }
    const c8* _to = kana_translate_target();
    if(_menu->card == KANA_PAGEMENU_CARD_GETTING) {
        const KANA_TRANSLATE_STATE_ _state = kana_translate_state(_to);
        if(_state == KANA_TRANSLATE_READY) {
            _menu->card_ticket = kana_translate_text(_menu->card_from, _to);
            _menu->card        = _menu->card_ticket != 0u ? KANA_PAGEMENU_CARD_ASKED : KANA_PAGEMENU_CARD_DONE;
        } else if(_state == KANA_TRANSLATE_MISSING || (_state == KANA_TRANSLATE_FAILED && !_menu->card_prepared)) {
            if(!_menu->card_prepared) {
                _menu->card_prepared = true;
                kana_translate_prepare(_to);
            }
        } else if(_state == KANA_TRANSLATE_FAILED || _state == KANA_TRANSLATE_UNAVAILABLE) {
            _menu->card = KANA_PAGEMENU_CARD_FAILED;
        }
    }
    if(_menu->card == KANA_PAGEMENU_CARD_ASKED) {
        u32 _ticket;
        c8  _answer[KANA_TRANSLATE_TEXT];
        while(kana_translate_poll(&_ticket, _answer, sizeof(_answer))) {
            if(_ticket == _menu->card_ticket) {
                memcpy(_menu->card_to, _answer, sizeof(_menu->card_to));
                _menu->card = KANA_PAGEMENU_CARD_DONE;
            }
        }
    }
}

// --- the selection's menu ----------------------------------------------------------

RDE_INTERNAL void kana_pagemenu_on_cut(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    kana_lasso_cut(_app->lasso, _app->ink);   // copy, then one undoable delete
}

RDE_INTERNAL void kana_pagemenu_on_copy(kana_app* _app, void* _self, u32 _arg) {
    kana_pagemenu* _menu = (kana_pagemenu*)_self;
    RDE_UNUSED(_arg);
    kana_lasso_copy(_app->lasso, _app->ink);
    // Nothing on the page changes, so say it worked.
    _menu->copied       = KANA_SEL_COPY;
    _menu->copied_until = rde_engine_get_time_now() + KANA_PAGEMENU_COPIED_TIME;
}

// Copy as text: the selection read; the text goes to the system clipboard when
// the reading is done.
RDE_INTERNAL void kana_pagemenu_on_copy_text(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    kana_pagemenu_read((kana_pagemenu*)_self, KANA_PAGEMENU_READ_COPY);
}

RDE_INTERNAL void kana_pagemenu_on_translate(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    kana_pagemenu_translate((kana_pagemenu*)_self);
}

// Save word: the selection read (written by hand, or pasted text), then the word
// card with what it says.
RDE_INTERNAL void kana_pagemenu_on_save_word(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    kana_pagemenu_save_word((kana_pagemenu*)_self);
}

RDE_INTERNAL void kana_pagemenu_on_duplicate(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    kana_lasso_duplicate(_app->lasso, _app->ink, _app->canvas->view.zoom);
}

// Check: the selection read and checked (a character, or several).
RDE_INTERNAL void kana_pagemenu_on_check(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    kana_lasso_sync(_app->lasso, _app->ink);
    kana_check_open(_app->check, _app->ink, (const u32*)_app->lasso->selected.memory, (u32)rde_arr_length(&_app->lasso->selected));
}

RDE_INTERNAL void kana_pagemenu_on_delete(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    kana_lasso_delete(_app->lasso, _app->ink);   // one undoable edit
}

static const kana_row_button KANA_PAGEMENU_SELECTION[] = {
    { KANA_TEXT_SEL_CUT,       KANA_ICON_CUT,       kana_pagemenu_on_cut,       0, KANA_ROW_QUIET,  false, NULL },
    { KANA_TEXT_SEL_COPY,      KANA_ICON_COPY,      kana_pagemenu_on_copy,      0, KANA_ROW_QUIET,  false, NULL },
    { KANA_TEXT_SEL_COPY_TEXT, KANA_ICON_TEXT_COPY, kana_pagemenu_on_copy_text, 0, KANA_ROW_QUIET,  false, NULL },
    { KANA_TEXT_SCAN_TRANSLATE, KANA_ICON_TRANSLATE, kana_pagemenu_on_translate, 0, KANA_ROW_QUIET, false, kana_translate_available },
    { KANA_TEXT_SEL_SAVE_WORD, KANA_ICON_BOOKMARK,  kana_pagemenu_on_save_word, 0, KANA_ROW_QUIET,  false, NULL },
    { KANA_TEXT_SEL_DUPLICATE, KANA_ICON_DUPLICATE, kana_pagemenu_on_duplicate, 0, KANA_ROW_QUIET,  false, NULL },
    { KANA_TEXT_SEL_CHECK,     KANA_ICON_SEARCH,    kana_pagemenu_on_check,     0, KANA_ROW_QUIET,  false, NULL },
    { KANA_TEXT_DELETE,        KANA_ICON_TRASH,     kana_pagemenu_on_delete,    0, KANA_ROW_DANGER, false, NULL },
};
static const kana_row_def KANA_PAGEMENU_SELECTION_ROW = KANA_ROW_DEF(KANA_PAGEMENU_SELECTION);

// Copy and Copy as text say "Copied" a moment; Copy as text and Save word say
// "Reading…" while the selection is read for them.
RDE_INTERNAL void kana_pagemenu_selection_faces(kana_pagemenu* _menu, kana_row_face* _faces) {
    memset(_faces, 0, sizeof(kana_row_face) * KANA_ROW_BUTTONS);
    if(_menu->copied != KANA_ROW_NONE && rde_engine_get_time_now() < _menu->copied_until) {
        snprintf(_faces[_menu->copied].label, KANA_ROW_LABEL, "%s", kana_text(KANA_TEXT_SEL_COPIED));
    }
    if(_menu->reading == KANA_PAGEMENU_READ_COPY) {
        snprintf(_faces[KANA_SEL_COPY_TEXT].label, KANA_ROW_LABEL, "%s", kana_text(KANA_TEXT_SEL_READING));
    }
    if(_menu->reading == KANA_PAGEMENU_READ_VOCAB) {
        snprintf(_faces[KANA_SEL_SAVE_WORD].label, KANA_ROW_LABEL, "%s", kana_text(KANA_TEXT_SEL_READING));
    }
}

// Floating just above the selection's box — below it when there is no room
// above — and hidden while the selection is dragged.
RDE_INTERNAL void kana_pagemenu_place_selection(kana_pagemenu* _menu, b8 _hidden) {
    kana_app*  _app = _menu->app;
    rde_vec_2F _min, _max;
    kana_lasso_sync(_app->lasso, _app->ink);
    const b8   _show   = !_hidden && !kana_lasso_busy(_app->lasso) && kana_lasso_bounds(_app->lasso, _app->ink, &_min, &_max);
    rde_vec_2F _center = _menu->selection.center;
    if(_show) {
        // Page → Kana screen (centre origin) → UI canvas (bottom-left origin).
        const kana_view* _view   = &_app->canvas->view;
        const rde_vec_2F _screen = kana_kit_screen_size(_app->window);
        const rde_vec_4I _insets = rde_window_get_safe_area_insets(_app->window);   // left, top, right, bottom
        const rde_vec_2F _size   = _menu->selection.size;
        const f32        _x      = (_min.x + _max.x) * 0.5f * _view->zoom + _view->offset.x + _screen.x * 0.5f;
        const f32        _top    = _max.y * _view->zoom + _view->offset.y + _screen.y * 0.5f + KANA_LASSO_BOX_PAD;
        const f32        _bottom = _min.y * _view->zoom + _view->offset.y + _screen.y * 0.5f - KANA_LASSO_BOX_PAD;
        _center = (rde_vec_2F){ _x, _top + KANA_PAGEMENU_GAP + _size.y * 0.5f };
        if(_center.y + _size.y * 0.5f > _screen.y - (f32)_insets.y - KANA_KIT_SCREEN_EDGE) {
            _center.y = _bottom - KANA_PAGEMENU_GAP - _size.y * 0.5f;
        }
    }
    kana_row_show(&_menu->selection, _app->window, _show, _center);
}

// --- the context menu ----------------------------------------------------------------

enum { KANA_CTX_PASTE = 0, KANA_CTX_PASTE_TEXT, KANA_CTX_SCAN, KANA_CTX_SELECT_ALL };

RDE_INTERNAL void kana_pagemenu_on_paste(kana_app* _app, void* _self, u32 _arg) {
    kana_pagemenu* _menu = (kana_pagemenu*)_self;
    RDE_UNUSED(_arg);
    kana_pagemenu_close_context(_menu);
    // What was pasted comes in selected, ready to drag: that is the Lasso's job.
    kana_toolbar_set_tool(&_app->ui->bar, KANA_TOOL_LASSO);
    kana_lasso_paste(_app->lasso, _app->ink, _menu->context_canvas);
}

// Paste text: the system clipboard's text, where the menu was opened.
RDE_INTERNAL void kana_pagemenu_on_paste_text(kana_app* _app, void* _self, u32 _arg) {
    kana_pagemenu* _menu = (kana_pagemenu*)_self;
    RDE_UNUSED(_app); RDE_UNUSED(_arg);
    kana_pagemenu_close_context(_menu);
    c8* _text = rde_engine_get_clipboard();
    if(_text == NULL) {
        kana_notice_show(kana_text(KANA_TEXT_NOTICE_NO_TEXT));
        return;
    }
    kana_pagemenu_write_text(_menu, _text, _menu->context_canvas);
    rde_engine_free_clipboard_str_ptr(_text);
}

// Text from a photo: its lines are written where the menu was opened.
RDE_INTERNAL void kana_pagemenu_on_scan(kana_app* _app, void* _self, u32 _arg) {
    kana_pagemenu* _menu = (kana_pagemenu*)_self;
    RDE_UNUSED(_arg);
    kana_pagemenu_close_context(_menu);
    kana_scan_open(_app->scan, _app->window, _menu->context_canvas);
}

RDE_INTERNAL void kana_pagemenu_on_select_all(kana_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_arg);
    kana_pagemenu_close_context((kana_pagemenu*)_self);
    kana_toolbar_set_tool(&_app->ui->bar, KANA_TOOL_LASSO);
    kana_lasso_select_all(_app->lasso, _app->ink);
}

static const kana_row_button KANA_PAGEMENU_CONTEXT[] = {
    { KANA_TEXT_CTX_PASTE,      KANA_ICON_PASTE,      kana_pagemenu_on_paste,      0, KANA_ROW_QUIET, false, NULL },
    { KANA_TEXT_CTX_PASTE_TEXT, KANA_ICON_TEXT_PASTE, kana_pagemenu_on_paste_text, 0, KANA_ROW_QUIET, false, NULL },
    { KANA_TEXT_CTX_SCAN,       KANA_ICON_SCAN,       kana_pagemenu_on_scan,       0, KANA_ROW_QUIET, false, NULL },
    { KANA_TEXT_CTX_SELECT_ALL, KANA_ICON_SELECT_ALL, kana_pagemenu_on_select_all, 0, KANA_ROW_QUIET, false, NULL },
};
static const kana_row_def KANA_PAGEMENU_CONTEXT_ROW = KANA_ROW_DEF(KANA_PAGEMENU_CONTEXT);

void kana_pagemenu_open_context(kana_pagemenu* _menu, rde_vec_2F _screen, rde_vec_2F _canvas) {
    kana_app* _app = _menu->app;
    if(_menu->context.panel == NULL) {
        return;
    }
    _menu->context_canvas = _canvas;
    // What can be pasted now. Asks only whether there is text (iOS says nothing):
    // reading it is Paste text's.
    memset(_menu->context_faces, 0, sizeof(_menu->context_faces));
    _menu->context_faces[KANA_CTX_PASTE].disabled      = !kana_lasso_can_paste(_app->lasso);
    _menu->context_faces[KANA_CTX_PASTE_TEXT].disabled = rde_engine_is_clipboard_empty();
    _menu->context_faces[KANA_CTX_SCAN].disabled       = !kana_textscan_available();
    kana_row_apply(&_menu->context, _menu->context_faces);

    // Kana screen (centre origin) → UI canvas (bottom-left origin); above the
    // finger, or below it with no room above.
    const rde_vec_2F _ui     = kana_kit_screen_size(_app->window);
    const rde_vec_4I _insets = rde_window_get_safe_area_insets(_app->window);   // left, top, right, bottom
    const rde_vec_2F _size   = _menu->context.size;
    const rde_vec_2F _p      = { _screen.x + _ui.x * 0.5f, _screen.y + _ui.y * 0.5f };
    rde_vec_2F       _center = { _p.x, _p.y + KANA_PAGEMENU_CONTEXT_LIFT + _size.y * 0.5f };
    if(_center.y + _size.y * 0.5f > _ui.y - (f32)_insets.y - KANA_KIT_SCREEN_EDGE) {
        _center.y = _p.y - KANA_PAGEMENU_CONTEXT_LIFT - _size.y * 0.5f;
    }
    kana_row_show(&_menu->context, _app->window, true, _center);
}

void kana_pagemenu_close_context(kana_pagemenu* _menu) {
    kana_row_show(&_menu->context, _menu->app->window, false, _menu->context.center);
}

// --- writing text on the page ----------------------------------------------------------

void kana_pagemenu_write_text(kana_pagemenu* _menu, const c8* _text, rde_vec_2F _canvas) {
    kana_app* _app   = _menu->app;
    const f32 _zoom  = _app->canvas->view.zoom > 0.0f ? _app->canvas->view.zoom : 1.0f;
    const f32 _width = (f32)rde_window_get_size(_app->window).x * KANA_PAGEMENU_TEXT_WIDTH;
    const kana_textink_result _r = kana_textink_write(_app->db, _text, KANA_PAGEMENU_TEXT_CELL / _zoom, _width / _zoom,
                                                      kana_ink_pen_radius(_app->ink, 0.5f), _app->ink->color, &_menu->_text_clip);
    if(_r.drawn == 0) {
        kana_notice_show(kana_text(KANA_TEXT_NOTICE_NOTHING_TO_WRITE));
        return;
    }
    // What was written comes in selected, ready to drag: the Lasso's job, as Paste.
    kana_toolbar_set_tool(&_app->ui->bar, KANA_TOOL_LASSO);
    kana_lasso_paste_clip(_app->lasso, _app->ink, &_menu->_text_clip, _canvas);
    if(_r.skipped > 0) {
        c8 _line[192];
        KANA_TEXTF(_line, KANA_TEXT_NOTICE_LEFT_OUT, KANA_TN(_r.skipped));
        kana_notice_show(_line);
    }
}

// --- the card ----------------------------------------------------------------------------

void kana_pagemenu_render(kana_pagemenu* _menu, rde_window* _window) {
    kana_app*  _app = _menu->app;
    rde_vec_2F _min, _max;
    if(_menu->card == KANA_PAGEMENU_CARD_NONE || _app->font == NULL || !kana_lasso_bounds(_app->lasso, _app->ink, &_min, &_max)) {
        _menu->card_min = _menu->card_max = (rde_vec_2F){ 0.0f, 0.0f };
        return;
    }
    const kana_theme* _t       = kana_theme_active();
    rde_font* const   _font    = _app->font;
    const f32         _font_px = _app->font_px;
    const rde_vec_2I  _size    = rde_window_get_size(_window);
    const rde_vec_4I  _insets  = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32         _sl      = -(f32)_size.x * 0.5f + (f32)_insets.x + KANA_KIT_SCREEN_EDGE;
    const f32         _sr      = (f32)_size.x * 0.5f - (f32)_insets.z - KANA_KIT_SCREEN_EDGE;
    const f32         _st      = (f32)_size.y * 0.5f - (f32)_insets.y - KANA_KIT_SCREEN_EDGE;
    const f32         _sb      = -(f32)_size.y * 0.5f + (f32)_insets.w + KANA_KIT_SCREEN_EDGE;

    // What it says: what was read, and its translation — or how far it got.
    const c8* _from = _menu->card == KANA_PAGEMENU_CARD_READING ? kana_text(KANA_TEXT_SEL_READING) : _menu->card_from;
    const c8* _to   = NULL;
    rde_color _to_c = _t->text_soft;
    switch(_menu->card) {
        case KANA_PAGEMENU_CARD_GETTING:
            _to = kana_text(kana_translate_state(kana_translate_target()) == KANA_TRANSLATE_DOWNLOADING ? KANA_TEXT_SCAN_TRANSLATE_GETTING : KANA_TEXT_SCAN_TRANSLATING);
            break;
        case KANA_PAGEMENU_CARD_ASKED:  _to = kana_text(KANA_TEXT_SCAN_TRANSLATING); break;
        case KANA_PAGEMENU_CARD_FAILED: _to = kana_text(KANA_TEXT_SCAN_TRANSLATE_FAILED); _to_c = _t->score_poor; break;
        case KANA_PAGEMENU_CARD_DONE:
            _to   = _menu->card_to[0] != 0 ? _menu->card_to : kana_text(KANA_TEXT_SEL_TRANSLATION_NONE);
            _to_c = _menu->card_to[0] != 0 ? _t->text : _t->text_soft;
            break;
        default: break;
    }

    // Its size: as wide as it may be, as tall as its lines.
    const f32 _w      = fminf(KANA_PAGEMENU_CARD_W, _sr - _sl);
    const f32 _inner  = _w - 2.0f * KANA_PAGEMENU_CARD_PAD;
    const u32 _from_n = kana_draw_text_wrap_lines(_font, _font_px, _from, KANA_PAGEMENU_CARD_FROM, _inner);
    const u32 _to_n   = _to != NULL ? kana_draw_text_wrap_lines(_font, _font_px, _to, KANA_PAGEMENU_CARD_TO, _inner) : 0u;
    const u32 _words  = _menu->card != KANA_PAGEMENU_CARD_READING ? _menu->word_count : 0u;
    const f32 _h      = 2.0f * KANA_PAGEMENU_CARD_PAD + KANA_TRANSLATE_BADGE_H + 10.0f + (f32)_from_n * KANA_PAGEMENU_CARD_FROM_L +
                        (_to_n > 0 ? 6.0f + (f32)_to_n * KANA_PAGEMENU_CARD_TO_L : 0.0f) + (_words > 0 ? 8.0f + (f32)_words * KANA_PAGEMENU_CARD_WORD : 0.0f);

    // Where: on the selection's other side from its menu (the menu goes above
    // when there is room), centred on it, kept on screen.
    const kana_view* _view    = &_app->canvas->view;
    const f32        _box_t   = _max.y * _view->zoom + _view->offset.y + KANA_LASSO_BOX_PAD;
    const f32        _box_b   = _min.y * _view->zoom + _view->offset.y - KANA_LASSO_BOX_PAD;
    const f32        _cx      = (_min.x + _max.x) * 0.5f * _view->zoom + _view->offset.x;
    const b8         _menu_up = _box_t + KANA_PAGEMENU_GAP + _menu->selection.size.y <= _st;
    f32              _top     = _menu_up ? _box_b - KANA_PAGEMENU_GAP : _box_t + KANA_PAGEMENU_GAP + _h;
    _top = fmaxf(fminf(_top, _st), _sb + _h);
    const f32 _left = fmaxf(fminf(_cx - _w * 0.5f, _sr - _w), _sl);
    _menu->card_min = (rde_vec_2F){ _left, _top - _h };
    _menu->card_max = (rde_vec_2F){ _left + _w, _top };

    kana_draw_card(_menu->card_min, _menu->card_max, 16.0f, _t->surface, _t->outline);
    const rde_color _s = _t->surface;
    f32             _y = _top - KANA_PAGEMENU_CARD_PAD - KANA_TRANSLATE_BADGE_H * 0.5f;
    // With a voice and something read: a speaker at the top right says it.
    if(kana_speech_available() && _menu->card != KANA_PAGEMENU_CARD_READING) {
        const rde_vec_2F _c = { _left + _w - KANA_PAGEMENU_CARD_PAD - 10.0f, _y - 6.0f };
        kana_draw_icon(_font, _font_px, KANA_ICON_SPEAK, _c, 20.0f, _t->accent);
        _menu->speak_min = (rde_vec_2F){ _c.x - 26.0f, _c.y - 22.0f };
        _menu->speak_max = (rde_vec_2F){ _c.x + 22.0f, _c.y + 22.0f };
    } else {
        _menu->speak_min = _menu->speak_max = (rde_vec_2F){ 0.0f, 0.0f };
    }
    kana_translate_draw_badge(_left + KANA_PAGEMENU_CARD_PAD, _y, 0.299f * (f32)_s.r + 0.587f * (f32)_s.g + 0.114f * (f32)_s.b < 128.0f);
    _y -= KANA_TRANSLATE_BADGE_H * 0.5f + 10.0f;
    kana_draw_text_wrap(_font, _font_px, _from, _left + KANA_PAGEMENU_CARD_PAD, _y - 16.0f, KANA_PAGEMENU_CARD_FROM, _inner, KANA_PAGEMENU_CARD_FROM_L, _t->text_soft);
    _y -= (f32)_from_n * KANA_PAGEMENU_CARD_FROM_L + 6.0f;
    if(_to != NULL) {
        kana_draw_text_wrap(_font, _font_px, _to, _left + KANA_PAGEMENU_CARD_PAD, _y - 18.0f, KANA_PAGEMENU_CARD_TO, _inner, KANA_PAGEMENU_CARD_TO_L, _to_c);
        _y -= (f32)_to_n * KANA_PAGEMENU_CARD_TO_L;
    }
    // Its words: the bookmark (filled: the learner's), the word, its reading, its meaning.
    _y -= 8.0f;
    _menu->words_top   = _y;
    _menu->words_left  = _left;
    _menu->words_right = _left + _w;
    for(u32 _k = 0; _k < _words && _app->db != NULL; _k++) {
        kana_kanji_word _word;
        if(!kana_kanji_word_at(_app->db, _menu->words[_k], &_word)) {
            continue;
        }
        const f32 _mid    = _y - ((f32)_k + 0.5f) * KANA_PAGEMENU_CARD_WORD;
        const b8  _theirs = kana_vocab_find(_word.written, _word.reading) != 0u;
        if(_theirs) {
            kana_draw_icon_fill(KANA_ICON_BOOKMARK, (rde_vec_2F){ _left + KANA_PAGEMENU_CARD_PAD + 9.0f, _mid }, 16.0f, _t->accent);
        } else {
            kana_draw_icon(_font, _font_px, KANA_ICON_BOOKMARK, (rde_vec_2F){ _left + KANA_PAGEMENU_CARD_PAD + 9.0f, _mid }, 16.0f, _t->accent);
        }
        c8 _say[400];
        snprintf(_say, sizeof(_say), "%s  %s  ·  %s", _word.written, _word.reading, _word.meaning);
        const f32 _px = kana_draw_text_px_to_fit(_font, _font_px, _say, 14.0f, _inner - 28.0f, 0.55f);
        kana_draw_text(_font, _font_px, _say, _left + KANA_PAGEMENU_CARD_PAD + 28.0f, _mid - _px * 0.36f, _px, _theirs ? _t->text : _t->text_soft);
    }
}

b8 kana_pagemenu_card_press(kana_pagemenu* _menu, rde_vec_2F _screen) {
    const kana_kanji_db* _db = _menu->app->db;
    // A word's row: the word card (wordcard.h), to save it or see it saved.
    if(_menu->card != KANA_PAGEMENU_CARD_NONE && _menu->card != KANA_PAGEMENU_CARD_READING && _db != NULL &&
       _screen.x >= _menu->words_left && _screen.x <= _menu->words_right && _screen.y <= _menu->words_top) {
        const i32 _k = (i32)floorf((_menu->words_top - _screen.y) / KANA_PAGEMENU_CARD_WORD);
        kana_kanji_word _w;
        if(_k >= 0 && (u32)_k < _menu->word_count && kana_kanji_word_at(_db, _menu->words[_k], &_w)) {
            kana_wordcard_ask(_w.written, _w.reading, _w.meaning, 0u);
            return true;
        }
    }
    if(_menu->card == KANA_PAGEMENU_CARD_NONE || _menu->card_from[0] == 0 ||
       _screen.x < _menu->speak_min.x || _screen.x > _menu->speak_max.x || _screen.y < _menu->speak_min.y || _screen.y > _menu->speak_max.y) {
        return false;
    }
    kana_speak(_menu->card_from);
    return true;
}

// --- lifetime ------------------------------------------------------------------------------

void kana_pagemenu_init(kana_pagemenu* _menu, kana_app* _app) {
    memset(_menu, 0, sizeof(*_menu));
    _menu->app    = _app;
    _menu->copied = KANA_ROW_NONE;
}

void kana_pagemenu_destroy(kana_pagemenu* _menu) {
    if(_menu->_reader_ready) {
        kana_textink_reader_destroy(&_menu->reader);
        _menu->_reader_ready = false;
    }
    rde_arr* const _clip[] = { &_menu->_text_clip.strokes, &_menu->_text_clip.points };
    for(u32 _i = 0; _i < 2u; _i++) {
        if(rde_arr_is_inited(_clip[_i])) {
            rde_arr_free(_clip[_i]);
        }
    }
}

void kana_pagemenu_build(kana_pagemenu* _menu, rde_ui_node* _root) {
    kana_row_create(&_menu->selection, _root, &KANA_PAGEMENU_SELECTION_ROW, _menu->app, _menu, kana_ui_after_press);
    kana_row_create(&_menu->context, _root, &KANA_PAGEMENU_CONTEXT_ROW, _menu->app, _menu, kana_ui_after_press);
}

void kana_pagemenu_restyle(kana_pagemenu* _menu) {
    kana_row_restyle(&_menu->selection);
    kana_row_restyle(&_menu->context);
}

void kana_pagemenu_update(kana_pagemenu* _menu, b8 _hidden) {
    kana_pagemenu_update_reading(_menu);
    kana_pagemenu_update_card(_menu);
    if(_hidden) {
        kana_pagemenu_close_context(_menu);
    }
    kana_pagemenu_place_selection(_menu, _hidden);
    kana_row_face _faces[KANA_ROW_BUTTONS];
    kana_pagemenu_selection_faces(_menu, _faces);
    kana_row_apply(&_menu->selection, _faces);
}

b8 kana_pagemenu_hit(const kana_pagemenu* _menu, rde_vec_2F _screen, rde_vec_2F _ui) {
    // The card is kept in Kana's screen space.
    if(_menu->card != KANA_PAGEMENU_CARD_NONE && _screen.x >= _menu->card_min.x && _screen.x <= _menu->card_max.x &&
       _screen.y >= _menu->card_min.y && _screen.y <= _menu->card_max.y) {
        return true;
    }
    return kana_row_hit(&_menu->selection, _ui) || kana_row_hit(&_menu->context, _ui);
}
