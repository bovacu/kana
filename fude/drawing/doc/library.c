// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/doc/library.h"
#include "drawing/doc/import.h"
#include "drawing/app/app.h"
#include "drawing/app/screen.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/ink/notes.h"
#include "drawing/widgets/draw.h"
#include "drawing/widgets/icons.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See library.h.
// ===========================================================================

#define FUDE_LIBRARY_PAD     16.0f   // inside a card
#define FUDE_LIBRARY_GAP     12.0f   // between cards
#define FUDE_LIBRARY_COVER_W 100.0f  // a book's cover, at most
#define FUDE_LIBRARY_COVER_H 136.0f
#define FUDE_LIBRARY_SECTION 40.0f   // a section's caption
#define FUDE_LIBRARY_OWN_H   58.0f   // a document's row

// --- lifetime --------------------------------------------------------------------------------

void fude_library_init(fude_library* _lib, fude_app* _app) {
    memset(_lib, 0, sizeof(*_lib));
    _lib->app = _app;
}

void fude_library_destroy(fude_library* _lib) {
    for(u32 _i = 0; _i < FUDE_LIBRARY_BOOKS; _i++) {
        if(_lib->covers[_i] != NULL) {
            rde_texture_unload(_lib->covers[_i]);
            _lib->covers[_i] = NULL;
        }
    }
}

void fude_library_open(fude_library* _lib) {
    _lib->open = true;
    fude_scroller_stop(&_lib->scroller);
    _lib->scroller.offset = 0.0f;
}

void fude_library_close(fude_library* _lib) {
    _lib->open = false;
    fude_scroller_stop(&_lib->scroller);
}

// The app's books (extension.h), at most FUDE_LIBRARY_BOOKS.
RDE_INTERNAL u32 fude_library_book_count(const fude_library* _lib) {
    const u32 _n = fude_app_ext(_lib->app)->library_count;
    return _n < FUDE_LIBRARY_BOOKS ? _n : FUDE_LIBRARY_BOOKS;
}

// The app's language as a book names it.
RDE_INTERNAL const c8* fude_library_language(void) {
    switch(fude_text_language()) {
        case RDE_LANGUAGE_ES_ES: return "es";
        case RDE_LANGUAGE_PT_BR: return "pt";
        case RDE_LANGUAGE_FR_FR: return "fr";
        case RDE_LANGUAGE_JA_JP: return "ja";
        default:                 return "en";
    }
}

// Is book _i shown? One without a language always; of those sharing its title,
// the one in the app's language — else the English one.
RDE_INTERNAL b8 fude_library_shown(const fude_library* _lib, u32 _i) {
    const fude_doc_book* _books = fude_app_ext(_lib->app)->library;
    const fude_doc_book* _b     = &_books[_i];
    const c8*            _ours  = fude_library_language();
    if(_b->language == NULL || strcmp(_b->language, _ours) == 0) {
        return true;
    }
    if(strcmp(_b->language, "en") != 0) {
        return false;
    }
    for(u32 _k = 0; _k < fude_library_book_count(_lib); _k++) {
        if(_books[_k].title == _b->title && _books[_k].language != NULL && strcmp(_books[_k].language, _ours) == 0) {
            return false;   // there is one in the app's language
        }
    }
    return true;
}

// The learner's documents: the canvases over their own PDFs, newest first.
RDE_INTERNAL void fude_library_gather(fude_library* _lib) {
    const fude_notes* _notes = _lib->app->notes;
    _lib->own_count          = 0;
    for(u32 _i = (u32)rde_arr_length(&_notes->notes); _i-- > 0 && _lib->own_count < FUDE_LIBRARY_OWN;) {
        const fude_note* _n = &((const fude_note*)_notes->notes.memory)[_i];
        if(_n->kind == FUDE_NOTE_CANVAS && _n->document == FUDE_NOTE_DOCUMENT_OWN) {
            _lib->own[_lib->own_count++] = _n->id;
        }
    }
}

// --- taps ------------------------------------------------------------------------------------

RDE_INTERNAL b8 fude_library_in(rde_vec_2F _p, rde_vec_2F _min, rde_vec_2F _max) {
    return _p.x >= _min.x && _p.x <= _max.x && _p.y >= _min.y && _p.y <= _max.y;
}

// A book's card or a document's row: opened, on the page.
RDE_INTERNAL void fude_library_tap(fude_library* _lib, rde_vec_2F _at) {
    if(!fude_library_in(_at, _lib->list_min, _lib->list_max)) {
        return;
    }
    fude_app* _app = _lib->app;
    for(u32 _i = 0; _i < fude_library_book_count(_lib); _i++) {
        if(fude_library_shown(_lib, _i) && fude_library_in(_at, _lib->book_min[_i], _lib->book_max[_i])) {
            if(fude_doc_open_book(_app, &fude_app_ext(_app)->library[_i], fude_text(FUDE_TEXT_LIBRARY_FOLDER)) != 0u) {
                fude_app_close_all(_app);
            }
            return;
        }
    }
    for(u32 _i = 0; _i < _lib->own_count; _i++) {
        if(fude_library_in(_at, _lib->own_min[_i], _lib->own_max[_i])) {
            fude_notes_open(_app->notes, _lib->own[_i]);
            fude_app_close_all(_app);
            return;
        }
    }
}

void fude_library_pointer_down(fude_library* _lib, rde_vec_2F _screen, f64 _time) {
    fude_scroller_down(&_lib->scroller, _screen, _time);
}

void fude_library_pointer_moved(fude_library* _lib, rde_vec_2F _screen, f64 _time) {
    fude_scroller_moved(&_lib->scroller, _screen, _time);
}

void fude_library_pointer_up(fude_library* _lib, f64 _time) {
    fude_scroller_up(&_lib->scroller, _time);
}

void fude_library_update(fude_library* _lib, f32 _dt) {
    if(!_lib->open) {
        return;
    }
    if(fude_import_update(_lib->app)) {
        fude_app_close_all(_lib->app);   // what came in is on the page
        return;
    }
    fude_scroller_update(&_lib->scroller, _dt, _lib->content_h, _lib->list_max.y - _lib->list_min.y);
    rde_vec_2F _at;
    if(fude_scroller_take_tap(&_lib->scroller, &_at)) {
        fude_library_tap(_lib, _at);
    }
}

// --- drawing ----------------------------------------------------------------------------------

// A book's cover in the box at _min-_max, as large as fits (loaded the first time).
RDE_INTERNAL void fude_library_draw_cover(fude_library* _lib, u32 _i, const fude_doc_book* _book, rde_vec_2F _min, rde_vec_2F _max) {
    const fude_theme* _t = fude_theme_active();
    if(!_lib->cover_tried[_i] && _book->cover != NULL) {
        _lib->cover_tried[_i] = true;
        _lib->covers[_i]      = rde_texture_load(_book->cover, NULL);
    }
    rde_vec_2F _size = { _max.x - _min.x, _max.y - _min.y };
    if(_lib->covers[_i] != NULL) {
        const rde_vec_2UI _px = rde_texture_get_size(_lib->covers[_i]);
        const f32         _k  = fminf(_size.x / (f32)_px.x, _size.y / (f32)_px.y);
        _size                 = (rde_vec_2F){ (f32)_px.x * _k, (f32)_px.y * _k };
    }
    const rde_vec_2F _c = { _min.x + _size.x * 0.5f, _max.y - _size.y * 0.5f };
    rde_rendering_2d_draw_rectangle(fude_draw_at(_c), (rde_vec_2F){ _size.x + 2.0f, _size.y + 2.0f }, _t->outline);
    rde_rendering_2d_draw_rectangle(fude_draw_at(_c), _size, (rde_color){ 255, 255, 255, 255 });
    if(_lib->covers[_i] != NULL) {
        const rde_vec_2UI _px = rde_texture_get_size(_lib->covers[_i]);
        rde_rendering_2d_draw_texture_2(_lib->covers[_i], (rde_vec_3F){ fude_draw_x(_c.x), _c.y, 0.0f }, (rde_vec_2F){ _size.x / (f32)_px.x, _size.y / (f32)_px.y }, 0.0f,
                                        (rde_color){ 255, 255, 255, 255 });
    }
}

void fude_library_render(fude_library* _lib, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_lib->open) {
        return;
    }
    const fude_theme*     _t      = fude_theme_active();
    const fude_extension* _ext    = fude_app_ext(_lib->app);
    const rde_vec_2I      _size   = rde_window_get_size(_window);
    const rde_vec_4I      _insets = rde_window_get_safe_area_insets(_window);
    const f32             _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + 24.0f;
    const f32             _right  = (f32)_size.x * 0.5f - (f32)_insets.z - 24.0f;
    const f32             _width  = _right - _left;

    // The header: the open book, its name, what it is for.
    fude_draw_icon(_font, _font_px, FUDE_ICON_BOOK, (rde_vec_2F){ _left + 18.0f, _top - 30.0f }, 30.0f, _t->accent);
    fude_draw_text(_font, _font_px, fude_text(FUDE_TEXT_LIBRARY_TITLE), _left + 48.0f, _top - 40.0f, 26.0f, _t->text);
    const u32 _caption = fude_draw_text_wrap(_font, _font_px, fude_text(FUDE_TEXT_LIBRARY_CAPTION), _left, _top - 78.0f, 15.0f, _width, 21.0f, _t->text_soft);
    const f32 _y       = _top - 78.0f - (f32)(_caption > 0u ? _caption - 1u : 0u) * 21.0f - 26.0f;

    _lib->list_min = (rde_vec_2F){ _left, _bottom };
    _lib->list_max = (rde_vec_2F){ _right, _y };
    fude_library_gather(_lib);
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)fude_draw_x((_left + _right) * 0.5f), (i32)((_y + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)_width + 8u, (u32)fmaxf(1.0f, _y - _bottom) });
    f32 _at = _y + _lib->scroller.offset;   // the next thing's top

    // The app's books.
    const u32 _books = fude_library_book_count(_lib);
    if(_books > 0u) {
        fude_draw_text(_font, _font_px, fude_text(FUDE_TEXT_LIBRARY_BOOKS), _left, _at - 26.0f, 14.0f, _t->text_soft);
        _at -= FUDE_LIBRARY_SECTION;
    }
    const f32 _text_x = _left + FUDE_LIBRARY_PAD + FUDE_LIBRARY_COVER_W + FUDE_LIBRARY_PAD;
    const f32 _text_w = _right - FUDE_LIBRARY_PAD - _text_x - 8.0f;   // room: measuring and drawing can differ by a little
    for(u32 _i = 0; _i < _books; _i++) {
        const fude_doc_book* _book   = &_ext->library[_i];
        if(!fude_library_shown(_lib, _i)) {
            _lib->book_min[_i] = _lib->book_max[_i] = (rde_vec_2F){ 1e9f, 1e9f };   // not tappable
            continue;
        }
        const c8*            _title  = fude_text(_book->title);
        const c8*            _about  = fude_text(_book->about);
        const c8*            _credit = _book->credit != NULL ? _book->credit : "";
        const u32 _title_n  = fude_draw_text_wrap_lines(_font, _font_px, _title, 18.0f, _text_w);
        const u32 _about_n  = fude_draw_text_wrap_lines(_font, _font_px, _about, 14.0f, _text_w);
        const u32 _credit_n = fude_draw_text_wrap_lines(_font, _font_px, _credit, 12.0f, _text_w);
        const f32 _text_h   = (f32)_title_n * 24.0f + 6.0f + (f32)_about_n * 20.0f + 8.0f + (f32)_credit_n * 17.0f;
        const f32 _h        = fmaxf(FUDE_LIBRARY_COVER_H, _text_h) + 2.0f * FUDE_LIBRARY_PAD;
        _lib->book_min[_i]  = (rde_vec_2F){ _left, _at - _h };
        _lib->book_max[_i]  = (rde_vec_2F){ _right, _at };
        if(_at - _h < _y && _at > _bottom) {
            fude_draw_card(_lib->book_min[_i], _lib->book_max[_i], 16.0f, _t->surface, _t->outline);
            fude_library_draw_cover(_lib, _i, _book, (rde_vec_2F){ _left + FUDE_LIBRARY_PAD, _at - FUDE_LIBRARY_PAD - FUDE_LIBRARY_COVER_H },
                                    (rde_vec_2F){ _left + FUDE_LIBRARY_PAD + FUDE_LIBRARY_COVER_W, _at - FUDE_LIBRARY_PAD });
            f32 _ty = _at - FUDE_LIBRARY_PAD - 18.0f;
            fude_draw_text_wrap(_font, _font_px, _title, _text_x, _ty, 18.0f, _text_w, 24.0f, _t->text);
            _ty -= (f32)_title_n * 24.0f + 6.0f;
            fude_draw_text_wrap(_font, _font_px, _about, _text_x, _ty, 14.0f, _text_w, 20.0f, _t->text_soft);
            _ty -= (f32)_about_n * 20.0f + 8.0f;
            fude_draw_text_wrap(_font, _font_px, _credit, _text_x, _ty, 12.0f, _text_w, 17.0f, _t->text_soft);
        }
        _at -= _h + FUDE_LIBRARY_GAP;
    }

    // The learner's own.
    _at -= 8.0f;
    fude_draw_text(_font, _font_px, fude_text(FUDE_TEXT_LIBRARY_YOURS), _left, _at - 26.0f, 14.0f, _t->text_soft);
    _at -= FUDE_LIBRARY_SECTION;
    if(_lib->own_count == 0u) {
        const u32 _lines = fude_draw_text_wrap_lines(_font, _font_px, fude_text(FUDE_TEXT_LIBRARY_YOURS_EMPTY), 15.0f, _width - 2.0f * FUDE_LIBRARY_PAD - 12.0f);   // room: measuring and drawing can differ by a little
        const f32 _h     = (f32)_lines * 21.0f + 2.0f * FUDE_LIBRARY_PAD;
        if(_at - _h < _y && _at > _bottom) {
            fude_draw_card((rde_vec_2F){ _left, _at - _h }, (rde_vec_2F){ _right, _at }, 16.0f, _t->surface, _t->outline);
            fude_draw_text_wrap(_font, _font_px, fude_text(FUDE_TEXT_LIBRARY_YOURS_EMPTY), _left + FUDE_LIBRARY_PAD, _at - FUDE_LIBRARY_PAD - 15.0f, 15.0f,
                                _width - 2.0f * FUDE_LIBRARY_PAD - 12.0f, 21.0f, _t->text_soft);
        }
        _at -= _h + FUDE_LIBRARY_GAP;
    }
    for(u32 _i = 0; _i < _lib->own_count; _i++) {
        const fude_note* _n = fude_notes_find(_lib->app->notes, _lib->own[_i]);
        _lib->own_min[_i]   = (rde_vec_2F){ _left, _at - FUDE_LIBRARY_OWN_H };
        _lib->own_max[_i]   = (rde_vec_2F){ _right, _at };
        if(_n != NULL && _at - FUDE_LIBRARY_OWN_H < _y && _at > _bottom) {
            fude_draw_card(_lib->own_min[_i], _lib->own_max[_i], 14.0f, _t->surface, _n->id == _lib->app->notes->open ? _t->accent : _t->outline);
            const f32 _mid = _at - FUDE_LIBRARY_OWN_H * 0.5f;
            fude_draw_icon(_font, _font_px, FUDE_ICON_BOOK, (rde_vec_2F){ _left + FUDE_LIBRARY_PAD + 10.0f, _mid }, 20.0f, _t->accent);
            c8 _date[48];
            fude_text_date(_date, sizeof(_date), _n->created);
            const f32 _date_w = fude_draw_text_width(_font, _font_px, _date, 13.0f);
            const f32 _name_w = _width - 2.0f * FUDE_LIBRARY_PAD - 36.0f - _date_w - 16.0f;
            const f32 _px     = fude_draw_text_px_to_fit(_font, _font_px, _n->name, 16.0f, _name_w);
            fude_draw_text(_font, _font_px, _n->name, _left + FUDE_LIBRARY_PAD + 36.0f, _mid - _px * 0.36f, _px, _t->text);
            fude_draw_text(_font, _font_px, _date, _right - FUDE_LIBRARY_PAD - _date_w, _mid - 13.0f * 0.36f, 13.0f, _t->text_soft);
        }
        _at -= FUDE_LIBRARY_OWN_H + 8.0f;
    }
    rde_rendering_end_clipping_rect();
    _lib->content_h = _y + _lib->scroller.offset - _at;
}

// --- the screen (screen.h): its row ------------------------------------------------------

FUDE_SCREEN_ADAPTERS(fude_library, fude_library)
FUDE_SCREEN_RENDER(fude_library, fude_library)

RDE_INTERNAL void fude_library_screen_update(fude_app* _app, void* _self, f32 _dt) {
    RDE_UNUSED(_app);
    fude_library_update((fude_library*)_self, _dt);
}

FUDE_ROW_CALL(fude_library_row_back, fude_library, fude_library_close)

RDE_INTERNAL void fude_library_row_files(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_import_files(_app);
}

RDE_INTERNAL void fude_library_row_photos(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_import_photos(_app);
}

RDE_INTERNAL void fude_library_row_scan(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_import_camera(_app);
}

static const fude_row_button FUDE_LIBRARY_BUTTONS[] = {
    { FUDE_TEXT_BACK,           FUDE_ICON_BACK,   fude_library_row_back,   0, FUDE_ROW_QUIET, false, NULL },
    { FUDE_TEXT_LIBRARY_FILES,  FUDE_ICON_IMPORT, fude_library_row_files,  0, FUDE_ROW_QUIET, false, NULL },
    { FUDE_TEXT_LIBRARY_PHOTOS, FUDE_ICON_IMAGE,  fude_library_row_photos, 0, FUDE_ROW_QUIET, false, fude_import_photos_available },
    { FUDE_TEXT_LIBRARY_SCAN,   FUDE_ICON_SCAN,   fude_library_row_scan,   0, FUDE_ROW_PRIMARY, false, fude_import_camera_available },
};
static const fude_row_def FUDE_LIBRARY_BUTTON_ROWS[] = { FUDE_ROW_DEF(FUDE_LIBRARY_BUTTONS) };

RDE_INTERNAL u32 fude_library_screen_row(const void* _self) {
    RDE_UNUSED(_self);
    return 0u;
}

const fude_screen FUDE_LIBRARY_SCREEN = {
    .name = "library", .input = FUDE_SCREEN_INPUT_POINT,
    .is_open = fude_library_screen_is_open, .close = fude_library_screen_close,
    .update = fude_library_screen_update, .render = fude_library_screen_render,
    .pointer_down = fude_library_screen_down, .pointer_moved = fude_library_screen_moved, .pointer_up = fude_library_screen_up,
    .rows = FUDE_LIBRARY_BUTTON_ROWS, .row_count = 1u, .row = fude_library_screen_row,
    .field_hint = FUDE_TEXT_COUNT,
};
