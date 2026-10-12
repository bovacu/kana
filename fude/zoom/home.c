// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/home.h"
#include "zoom/words.h"
#include "zoom/pages.h"
#include "drawing/app/app.h"
#include "drawing/base/kfile.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "drawing/widgets/draw.h"
#include "drawing/widgets/icons.h"
#include "drawing/widgets/row.h"
#include "drawing/widgets/side.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// See home.h.
// ===========================================================================

#define FUDE_ZOOM_HOME_LEAST   210.0f   // a card's width, at least (as many to a row as fit)
#define FUDE_ZOOM_HOME_GAP     16.0f
#define FUDE_ZOOM_HOME_PAD     10.0f    // inside a card
#define FUDE_ZOOM_HOME_TEXT_H  50.0f    // a card's name and what it is, under its cover
#define FUDE_ZOOM_HOME_SHAPE   0.72f    // a cover's height, of its width
#define FUDE_ZOOM_HOME_HEAD    74.0f    // the header, over the cards
#define FUDE_ZOOM_HOME_HOLD    0.5      // a card held this long, still: its buttons
#define FUDE_ZOOM_HOME_SLOP    10.0f    // ...moved less than this
#define FUDE_ZOOM_HOME_LOADS   2u       // covers loaded a frame, at most

// --- lifetime ----------------------------------------------------------------------------------

void fude_zoom_home_init(fude_zoom_home* _home, struct fude_app* _app) {
    memset(_home, 0, sizeof(*_home));
    _home->app           = _app;
    _home->cards         = rde_arr_new(sizeof(fude_zoom_home_card), rde_memory_allocator_get_default_std());
    _home->boxes         = rde_arr_new(sizeof(fude_zoom_home_box), rde_memory_allocator_get_default_std());
    _home->held          = FUDE_ZOOM_HOME_NONE;
    _home->title         = FUDE_TEXT_ZOOM_HOME_CANVASES;
    _home->new_text      = FUDE_TEXT_ZOOM_HOME_NEW_CANVAS;
    _home->welcome_title = FUDE_TEXT_COUNT;
    _home->welcome_first = FUDE_TEXT_COUNT;
    for(u32 _k = 0; _k < FUDE_ZOOM_HOME_LINES; _k++) {
        _home->welcome_lines[_k] = FUDE_TEXT_COUNT;
    }
}

RDE_INTERNAL void fude_zoom_home_drop_covers(fude_zoom_home* _home) {
    for(u32 _k = 0; _k < FUDE_ZOOM_HOME_COVERS; _k++) {
        if(_home->covers[_k].texture != NULL) {
            rde_texture_unload(_home->covers[_k].texture);
        }
        memset(&_home->covers[_k], 0, sizeof(_home->covers[_k]));
    }
}

void fude_zoom_home_destroy(fude_zoom_home* _home) {
    fude_zoom_home_drop_covers(_home);
    rde_arr_free(&_home->cards);
    rde_arr_free(&_home->boxes);
}

void fude_zoom_home_open(fude_zoom_home* _home) {
    _home->open     = true;
    _home->gathered = false;
    _home->held     = FUDE_ZOOM_HOME_NONE;
    _home->pressing = false;
    const fude_note* _f = _home->folder != 0u ? fude_notes_find(_home->app->notes, _home->folder) : NULL;
    if(_f == NULL || _f->kind != FUDE_NOTE_FOLDER) {
        _home->folder = 0u;   // (the folder it showed gone since)
    }
    fude_scroller_stop(&_home->scroller);
    _home->scroller.offset = 0.0f;
}

void fude_zoom_home_close(fude_zoom_home* _home) {
    _home->open    = false;
    _home->welcome = false;
    _home->held    = FUDE_ZOOM_HOME_NONE;
    fude_scroller_stop(&_home->scroller);
    fude_zoom_home_drop_covers(_home);   // (pictures not needed on the canvas)
}

void fude_zoom_home_show_welcome(fude_zoom_home* _home) {
    if(_home->welcome_title == FUDE_TEXT_COUNT) {
        return;
    }
    if(!_home->open) {
        fude_zoom_home_open(_home);
    }
    _home->welcome = true;
}

void fude_zoom_home_forget_cover(fude_zoom_home* _home, u32 _note) {
    for(u32 _k = 0; _k < FUDE_ZOOM_HOME_COVERS; _k++) {
        if(_home->covers[_k].note == _note && (_home->covers[_k].texture != NULL || _home->covers[_k].tried)) {
            if(_home->covers[_k].texture != NULL) {
                rde_texture_unload(_home->covers[_k].texture);
            }
            memset(&_home->covers[_k], 0, sizeof(_home->covers[_k]));
        }
    }
}

// A canvas's cover: kept, or loaded now (at most a few a frame; NULL: none, or not yet).
RDE_INTERNAL rde_texture* fude_zoom_home_cover_of(fude_zoom_home* _home, u32 _note, u32* _loads) {
    u32 _oldest = 0;
    for(u32 _k = 0; _k < FUDE_ZOOM_HOME_COVERS; _k++) {
        fude_zoom_home_cover* _c = &_home->covers[_k];
        if((_c->texture != NULL || _c->tried) && _c->note == _note) {
            _c->seen = _home->frame;
            return _c->texture;
        }
        const fude_zoom_home_cover* _o = &_home->covers[_oldest];
        const b8 _free = _c->texture == NULL && !_c->tried, _ofree = _o->texture == NULL && !_o->tried;
        if(!_ofree && (_free || _c->seen < _o->seen)) {
            _oldest = _k;
        }
    }
    if(*_loads >= FUDE_ZOOM_HOME_LOADS) {
        return NULL;
    }
    (*_loads)++;
    fude_zoom_home_cover* _c = &_home->covers[_oldest];
    if(_c->texture != NULL) {
        rde_texture_unload(_c->texture);
    }
    memset(_c, 0, sizeof(*_c));
    _c->note  = _note;
    _c->tried = true;
    _c->seen  = _home->frame;
    c8 _path[RDE_MAX_PATH];
    fude_zoom_home_side_path(_note, "cover.png", _path, sizeof(_path));
    if(rde_file_exists(_path)) {
        u32 _size = 0;
        u8* _bytes = fude_file_read(_path, &_size);
        if(_bytes != NULL) {
            _c->texture = rde_texture_load_from_memory(_bytes, _size, "cover", NULL, NULL);
            fude_file_free(_bytes);
        }
    }
    return _c->texture;
}

// --- taps ----------------------------------------------------------------------------------------

RDE_INTERNAL b8 fude_zoom_home_in(rde_vec_2F _p, fude_zoom_home_box _b) {
    return _p.x >= _b.min.x && _p.x <= _b.max.x && _p.y >= _b.min.y && _p.y <= _b.max.y;
}

RDE_INTERNAL u32 fude_zoom_home_card_at(const fude_zoom_home* _home, rde_vec_2F _at) {
    if(_at.y > _home->list_max.y || _at.y < _home->list_min.y) {
        return FUDE_ZOOM_HOME_NONE;
    }
    const fude_zoom_home_box* _b = (const fude_zoom_home_box*)_home->boxes.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_home->boxes); _i++) {
        if(fude_zoom_home_in(_at, _b[_i])) {
            return _i;
        }
    }
    return FUDE_ZOOM_HOME_NONE;
}

RDE_INTERNAL void fude_zoom_home_tap(fude_zoom_home* _home, rde_vec_2F _at) {
    fude_app* _app = _home->app;
    const fude_zoom_home_card* _cards = (const fude_zoom_home_card*)_home->cards.memory;
    if(_home->chips_shown && _home->held < (u32)rde_arr_length(&_home->cards)) {
        const u32 _note = _cards[_home->held].note;
        for(u32 _k = 0; _k < 2u; _k++) {
            if(fude_zoom_home_in(_at, _home->chips[_k])) {
                // (the side panel's card for it: rename it, or delete it once asked)
                fude_zoom_home_close(_home);
                fude_side_look_card(_app->ui, _note, _k == 0u ? FUDE_SIDE_CARD_RENAME : FUDE_SIDE_CARD_DELETE);
                return;
            }
        }
    }
    _home->held = FUDE_ZOOM_HOME_NONE;
    const u32 _i = fude_zoom_home_card_at(_home, _at);
    if(_i >= (u32)rde_arr_length(&_home->cards)) {
        return;
    }
    if(_cards[_i].kind == FUDE_NOTE_FOLDER) {
        _home->folder   = _cards[_i].note;
        _home->gathered = false;
        fude_scroller_stop(&_home->scroller);
        _home->scroller.offset = 0.0f;
        return;
    }
    fude_notes_open(_app->notes, _cards[_i].note);   // (the session opens it)
    fude_zoom_home_close(_home);
}

RDE_INTERNAL void fude_zoom_home_welcome_done(fude_zoom_home* _home) {
    _home->welcome = false;
    _home->app->cards_read |= FUDE_ZOOM_HOME_WELCOMED;   // (seen: saved with the settings)
}

RDE_INTERNAL void fude_zoom_home_new(fude_zoom_home* _home) {
    if(_home->on_new != NULL) {
        _home->on_new(_home->app, _home->folder);
    }
    fude_zoom_home_close(_home);
}

void fude_zoom_home_pointer_down(fude_zoom_home* _home, rde_vec_2F _at, f64 _now) {
    _home->press       = _at;
    _home->press_at    = _now;
    _home->pressing    = true;
    _home->press_moved = false;
    if(!_home->welcome) {
        fude_scroller_down(&_home->scroller, _at, _now);
    }
}

void fude_zoom_home_pointer_moved(fude_zoom_home* _home, rde_vec_2F _at, f64 _now) {
    if(fabsf(_at.x - _home->press.x) + fabsf(_at.y - _home->press.y) > FUDE_ZOOM_HOME_SLOP) {
        _home->press_moved = true;
    }
    if(!_home->welcome) {
        fude_scroller_moved(&_home->scroller, _at, _now);
    }
}

void fude_zoom_home_pointer_up(fude_zoom_home* _home, f64 _now) {
    const b8 _was = _home->pressing;
    _home->pressing = false;
    if(_home->welcome) {
        // (its two ways in; a tap anywhere else leaves it up)
        if(_was && !_home->press_moved) {
            if(fude_zoom_home_in(_home->press, _home->welcome_buttons[0])) {
                fude_zoom_home_welcome_done(_home);
                fude_zoom_home_close(_home);
                if(_home->on_first != NULL) {
                    _home->on_first(_home->app);
                }
            } else if(fude_zoom_home_in(_home->press, _home->welcome_buttons[1])) {
                fude_zoom_home_welcome_done(_home);
                fude_zoom_home_new(_home);
            }
        }
        return;
    }
    fude_scroller_up(&_home->scroller, _now);
}

void fude_zoom_home_update(fude_zoom_home* _home, f32 _dt) {
    if(!_home->open) {
        return;
    }
    const fude_notes* _notes = _home->app->notes;
    if(!_home->gathered || _home->gathered_rev != _notes->revision || _home->gathered_folder != _home->folder) {
        fude_zoom_home_gather(_notes, _home->folder, &_home->cards);
        _home->gathered        = true;
        _home->gathered_rev    = _notes->revision;
        _home->gathered_folder = _home->folder;
        _home->held            = FUDE_ZOOM_HOME_NONE;
    }
    fude_scroller_update(&_home->scroller, _dt, _home->content_h, _home->list_max.y - _home->list_min.y);
    // A card held still: its buttons (the tap its letting go makes, not one).
    b8 _swallow = false;
    if(_home->pressing && !_home->press_moved && !_home->welcome && rde_engine_get_time_now() - _home->press_at >= FUDE_ZOOM_HOME_HOLD &&
       _home->held == FUDE_ZOOM_HOME_NONE) {
        const u32 _i = fude_zoom_home_card_at(_home, _home->press);
        if(_i != FUDE_ZOOM_HOME_NONE) {
            _home->held        = _i;
            _home->press_moved = true;   // (let go: no tap)
            _swallow           = true;
        }
    }
    rde_vec_2F _at;
    if(fude_scroller_take_tap(&_home->scroller, &_at) && !_swallow && !_home->welcome) {
        if(rde_engine_get_time_now() - _home->press_at < FUDE_ZOOM_HOME_HOLD || _home->held != FUDE_ZOOM_HOME_NONE) {
            fude_zoom_home_tap(_home, _at);
        }
    }
}

// --- drawing ------------------------------------------------------------------------------------------

// A picture fitted in the box _min-_max, kept to its shape, its top at the box's.
RDE_INTERNAL void fude_zoom_home_draw_cover(rde_texture* _tex, rde_vec_2F _min, rde_vec_2F _max) {
    const rde_vec_2UI _px = rde_texture_get_size(_tex);
    if(_px.x == 0u || _px.y == 0u) {
        return;
    }
    const f32 _bw = _max.x - _min.x, _bh = _max.y - _min.y;
    const f32 _k  = fminf(_bw / (f32)_px.x, _bh / (f32)_px.y);
    const rde_vec_2F _size = { (f32)_px.x * _k, (f32)_px.y * _k };
    const rde_vec_2F _c    = { (_min.x + _max.x) * 0.5f, _max.y - _size.y * 0.5f };
    rde_rendering_2d_draw_texture_2(_tex, (rde_vec_3F){ fude_draw_x(_c.x), _c.y, 0.0f }, (rde_vec_2F){ _k, _k }, 0.0f, (rde_color){ 255, 255, 255, 255 });
    rde_rendering_2d_draw_rectangle_border(fude_draw_at(_c), _size, 1.0f, fude_theme_active()->outline, NULL);   // (its edge: a page on the card)
}

RDE_INTERNAL void fude_zoom_home_draw_chip(rde_font* _font, f32 _font_px, fude_zoom_home_box _b, const c8* _text, b8 _danger, b8 _primary) {
    const fude_theme* _t = fude_theme_active();
    const rde_color _fill = _primary ? _t->accent : _t->surface_2;
    const rde_color _line = _danger ? _t->danger : _primary ? _t->accent : _t->outline;
    fude_draw_card(_b.min, _b.max, (_b.max.y - _b.min.y) * 0.5f, _fill, _line);
    const f32 _w  = _b.max.x - _b.min.x - 20.0f;
    const f32 _px = fude_draw_text_px_to_fit(_font, _font_px, _text, 15.0f, _w);
    const f32 _tw = fude_draw_text_width(_font, _font_px, _text, _px);
    fude_draw_text(_font, _font_px, _text, (_b.min.x + _b.max.x) * 0.5f - _tw * 0.5f, (_b.min.y + _b.max.y) * 0.5f - _px * 0.36f, _px,
                   _danger ? _t->danger : _primary ? _t->page : _t->text);
}

// The welcome: over the cards, a few lines and two ways in.
RDE_INTERNAL void fude_zoom_home_render_welcome(fude_zoom_home* _home, rde_window* _window, rde_font* _font, f32 _font_px) {
    const fude_theme* _t    = fude_theme_active();
    const rde_vec_2I  _size = rde_window_get_size(_window);
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ (f32)_size.x + 4.0f, (f32)_size.y + 4.0f }, (rde_color){ 0, 0, 0, 120 });
    const f32 _w = fminf(600.0f, (f32)_size.x - 48.0f), _text_w = _w - 56.0f;
    c8 _title[256];
    FUDE_TEXTF(_title, _home->welcome_title, FUDE_TS(_home->app->info->name));
    u32 _lines = fude_draw_text_wrap_lines(_font, _font_px, _title, 24.0f, _text_w);
    f32 _h = 28.0f + (f32)_lines * 32.0f + 12.0f;
    for(u32 _k = 0; _k < FUDE_ZOOM_HOME_LINES; _k++) {
        if(_home->welcome_lines[_k] != FUDE_TEXT_COUNT) {
            _h += (f32)fude_draw_text_wrap_lines(_font, _font_px, fude_text(_home->welcome_lines[_k]), 16.0f, _text_w - 30.0f) * 23.0f + 14.0f;
        }
    }
    _h += 16.0f + 46.0f + 28.0f;
    const rde_vec_2F _min = { -_w * 0.5f, -_h * 0.5f }, _max = { _w * 0.5f, _h * 0.5f };
    fude_draw_card(_min, _max, 20.0f, _t->surface, _t->outline);
    f32 _y = _max.y - 28.0f - 24.0f;
    fude_draw_text_wrap(_font, _font_px, _title, _min.x + 28.0f, _y, 24.0f, _text_w, 32.0f, _t->text);
    _y -= (f32)(_lines - 1u) * 32.0f + 42.0f;
    u32 _step = 1u;
    for(u32 _k = 0; _k < FUDE_ZOOM_HOME_LINES; _k++) {
        if(_home->welcome_lines[_k] == FUDE_TEXT_COUNT) {
            continue;
        }
        // (its number in a ring, the line beside it)
        rde_rendering_2d_draw_circle_with_border(fude_draw_at((rde_vec_2F){ _min.x + 40.0f, _y + 6.0f }), 12.0f, 24u, _t->surface_2, 1.0f, _t->accent, NULL);
        c8 _n[8];
        snprintf(_n, sizeof(_n), "%u", _step++);
        const f32 _nw = fude_draw_text_width(_font, _font_px, _n, 14.0f);
        fude_draw_text(_font, _font_px, _n, _min.x + 40.0f - _nw * 0.5f, _y + 1.0f, 14.0f, _t->accent);
        const u32 _l = fude_draw_text_wrap(_font, _font_px, fude_text(_home->welcome_lines[_k]), _min.x + 62.0f, _y, 16.0f, _text_w - 30.0f, 23.0f, _t->text_soft);
        _y -= (f32)_l * 23.0f + 14.0f;
    }
    // Its two ways in, from the right: the first step (the accent), a new canvas.
    const c8* _first = fude_text(_home->welcome_first != FUDE_TEXT_COUNT ? _home->welcome_first : _home->new_text);
    const c8* _new   = fude_text(_home->new_text);
    const f32 _fw = fminf(fude_draw_text_width(_font, _font_px, _first, 15.0f) + 44.0f, (_w - 72.0f) * 0.5f);
    const f32 _nw = fminf(fude_draw_text_width(_font, _font_px, _new, 15.0f) + 44.0f, (_w - 72.0f) * 0.5f);
    const f32 _by = _min.y + 28.0f;
    _home->welcome_buttons[0] = (fude_zoom_home_box){ { _max.x - 28.0f - _fw, _by }, { _max.x - 28.0f, _by + 46.0f } };
    _home->welcome_buttons[1] = (fude_zoom_home_box){ { _home->welcome_buttons[0].min.x - 12.0f - _nw, _by }, { _home->welcome_buttons[0].min.x - 12.0f, _by + 46.0f } };
    fude_zoom_home_draw_chip(_font, _font_px, _home->welcome_buttons[0], _first, false, true);
    fude_zoom_home_draw_chip(_font, _font_px, _home->welcome_buttons[1], _new, false, false);
}

void fude_zoom_home_render(fude_zoom_home* _home, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom) {
    if(!_home->open) {
        return;
    }
    _home->frame++;
    const fude_theme* _t      = fude_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _insets = rde_window_get_safe_area_insets(_window);
    const f32         _left   = -(f32)_size.x * 0.5f + (f32)_insets.x + 24.0f;
    const f32         _right  = (f32)_size.x * 0.5f - (f32)_insets.z - 24.0f;
    const f32         _width  = _right - _left;
    const fude_notes* _notes  = _home->app->notes;

    // The header: home, or the folder it is in.
    const fude_note* _folder = _home->folder != 0u ? fude_notes_find(_notes, _home->folder) : NULL;
    fude_draw_icon(_font, _font_px, _folder != NULL ? FUDE_ICON_FOLDER_OPEN : FUDE_ICON_HOME, (rde_vec_2F){ _left + 18.0f, _top - 32.0f }, 30.0f, _t->accent);
    const c8* _title = _folder != NULL ? _folder->name : fude_text(_home->title);
    const f32 _tpx   = fude_draw_text_px_to_fit(_font, _font_px, _title, 26.0f, _width - 52.0f);
    fude_draw_text(_font, _font_px, _title, _left + 48.0f, _top - 42.0f, _tpx, _t->text);

    const f32 _y = _top - FUDE_ZOOM_HOME_HEAD;
    _home->list_min = (rde_vec_2F){ _left, _bottom };
    _home->list_max = (rde_vec_2F){ _right, _y };
    const u32 _cols  = fude_zoom_home_columns(_width, FUDE_ZOOM_HOME_LEAST, FUDE_ZOOM_HOME_GAP);
    const f32 _cw    = (_width - (f32)(_cols - 1u) * FUDE_ZOOM_HOME_GAP) / (f32)_cols;
    const f32 _cover = (_cw - 2.0f * FUDE_ZOOM_HOME_PAD) * FUDE_ZOOM_HOME_SHAPE;
    const f32 _ch    = FUDE_ZOOM_HOME_PAD + _cover + FUDE_ZOOM_HOME_TEXT_H;
    const u32 _n     = (u32)rde_arr_length(&_home->cards);
    rde_arr_resize(&_home->boxes, _n);
    const fude_zoom_home_card* _cards = (const fude_zoom_home_card*)_home->cards.memory;
    fude_zoom_home_box*        _boxes = (fude_zoom_home_box*)_home->boxes.memory;
    _home->chips_shown = false;
    u32 _loads = 0;
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)fude_draw_x((_left + _right) * 0.5f), (i32)((_y + _bottom) * 0.5f) },
                                      (rde_vec_2UI){ (u32)_width + 8u, (u32)fmaxf(1.0f, _y - _bottom) });
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_home_card* _c = &_cards[_i];
        const u32 _row = _i / _cols, _col = _i % _cols;
        const f32 _x0  = _left + (f32)_col * (_cw + FUDE_ZOOM_HOME_GAP);
        const f32 _top_y = _y - 6.0f + _home->scroller.offset - (f32)_row * (_ch + FUDE_ZOOM_HOME_GAP);
        _boxes[_i] = (fude_zoom_home_box){ { _x0, _top_y - _ch }, { _x0 + _cw, _top_y } };
        if(_top_y - _ch > _y || _top_y < _bottom) {
            continue;
        }
        const b8 _open = _c->kind == FUDE_NOTE_CANVAS && _c->note == _notes->open;
        fude_draw_card(_boxes[_i].min, _boxes[_i].max, 14.0f, _t->surface, _open ? _t->accent : _t->outline);
        if(_open) {   // (the one open: ringed)
            rde_rendering_2d_draw_rounded_rectangle_with_border(fude_draw_at((rde_vec_2F){ _x0 + _cw * 0.5f, _top_y - _ch * 0.5f }), (rde_vec_2F){ _cw - 2.0f, _ch - 2.0f },
                                                                0.08f, 8u, (rde_color){ 0, 0, 0, 0 }, 2.0f, _t->accent, NULL);
        }
        const rde_vec_2F _cmin = { _x0 + FUDE_ZOOM_HOME_PAD, _top_y - FUDE_ZOOM_HOME_PAD - _cover }, _cmax = { _x0 + _cw - FUDE_ZOOM_HOME_PAD, _top_y - FUDE_ZOOM_HOME_PAD };
        const rde_vec_2F _cmid = { (_cmin.x + _cmax.x) * 0.5f, (_cmin.y + _cmax.y) * 0.5f };
        const rde_vec_2F _csize = { _cmax.x - _cmin.x, _cmax.y - _cmin.y };
        c8 _about[160];
        _about[0] = 0;
        if(_c->kind == FUDE_NOTE_FOLDER) {
            rde_rendering_2d_draw_rectangle(fude_draw_at(_cmid), _csize, _t->surface_2);
            fude_draw_icon(_font, _font_px, FUDE_ICON_FOLDER, _cmid, fminf(_csize.y * 0.45f, 64.0f), _t->accent);
            FUDE_TEXTF(_about, FUDE_TEXT_ZOOM_HOME_ITEMS, FUDE_TN(_c->inside));
        } else {
            rde_texture* _tex = fude_zoom_home_cover_of(_home, _c->note, &_loads);
            rde_rendering_2d_draw_rectangle(fude_draw_at(_cmid), _csize, _tex != NULL ? _t->surface_2 : _t->page);
            if(_tex != NULL) {
                fude_zoom_home_draw_cover(_tex, _cmin, _cmax);
            } else {
                rde_color _soft = _t->text_soft;
                _soft.a = (u8)((u32)_soft.a * 40u / 100u);
                fude_draw_icon(_font, _font_px, _c->notebook ? FUDE_ICON_BOOK : FUDE_ICON_DRAW, _cmid, fminf(_csize.y * 0.35f, 52.0f), _soft);
            }
            rde_rendering_2d_draw_rectangle_border(fude_draw_at(_cmid), _csize, 1.0f, _t->outline, NULL);
            c8 _date[48], _edited[96];
            fude_text_date(_date, sizeof(_date), _c->edited);
            FUDE_TEXTF(_edited, FUDE_TEXT_ZOOM_HOME_EDITED, FUDE_TS(_date));
            const u32 _kind = _c->notebook ? FUDE_TEXT_ZOOM_HOME_NOTEBOOK : _c->pdf ? FUDE_TEXT_ZOOM_HOME_PDF : FUDE_TEXT_COUNT;
            snprintf(_about, sizeof(_about), "%s%s%s", _kind != FUDE_TEXT_COUNT ? fude_text(_kind) : "", _kind != FUDE_TEXT_COUNT ? " \xC2\xB7 " : "", _edited);
        }
        // Its name and what it is, under its cover (never cut: smaller when long).
        const fude_note* _note = fude_notes_find(_notes, _c->note);
        const c8* _name = _note != NULL ? _note->name : "";
        const f32 _tw   = _cw - 2.0f * FUDE_ZOOM_HOME_PAD - 4.0f;
        const f32 _npx  = fude_draw_text_px_to_fit(_font, _font_px, _name, 16.0f, _tw);
        fude_draw_text(_font, _font_px, _name, _x0 + FUDE_ZOOM_HOME_PAD, _cmin.y - 22.0f, _npx, _t->text);
        const f32 _apx  = fude_draw_text_px_to_fit(_font, _font_px, _about, 13.0f, _tw);
        fude_draw_text(_font, _font_px, _about, _x0 + FUDE_ZOOM_HOME_PAD, _cmin.y - 41.0f, _apx, _t->text_soft);
        // Held: its Rename and Delete, over its cover.
        if(_i == _home->held) {
            const c8* _words[2] = { fude_text(FUDE_TEXT_RENAME), fude_text(FUDE_TEXT_DELETE) };
            const f32 _bw = fminf((_csize.x - 24.0f) * 0.5f, 140.0f);
            for(u32 _k = 0; _k < 2u; _k++) {
                const f32 _bx = _cmid.x + (_k == 0u ? -6.0f - _bw : 6.0f);
                _home->chips[_k] = (fude_zoom_home_box){ { _bx, _cmid.y - 20.0f }, { _bx + _bw, _cmid.y + 20.0f } };
                fude_zoom_home_draw_chip(_font, _font_px, _home->chips[_k], _words[_k], _k == 1u, false);
            }
            _home->chips_shown = true;
        }
    }
    rde_rendering_end_clipping_rect();
    const u32 _rows = (_n + _cols - 1u) / _cols;
    _home->content_h = (f32)_rows * (_ch + FUDE_ZOOM_HOME_GAP) + 12.0f;
    if(_home->welcome) {
        fude_zoom_home_render_welcome(_home, _window, _font, _font_px);
    }
}

// --- the screen (screen.h): its row --------------------------------------------------------------------

RDE_INTERNAL b8 fude_zoom_home_screen_is_open(const void* _self) {
    return ((const fude_zoom_home*)_self)->open;
}

RDE_INTERNAL void fude_zoom_home_screen_down(void* _self, rde_vec_2F _at, b8 _pen, f64 _now) {
    RDE_UNUSED(_pen);
    fude_zoom_home_pointer_down((fude_zoom_home*)_self, _at, _now);
}

RDE_INTERNAL void fude_zoom_home_screen_moved(void* _self, rde_vec_2F _at, f64 _now) {
    fude_zoom_home_pointer_moved((fude_zoom_home*)_self, _at, _now);
}

RDE_INTERNAL void fude_zoom_home_screen_up(void* _self, rde_vec_2F _at, f64 _now) {
    RDE_UNUSED(_at);
    fude_zoom_home_pointer_up((fude_zoom_home*)_self, _now);
}

FUDE_SCREEN_RENDER(fude_zoom_home, fude_zoom_home)

RDE_INTERNAL void fude_zoom_home_screen_update(struct fude_app* _app, void* _self, f32 _dt) {
    RDE_UNUSED(_app);
    fude_zoom_home_update((fude_zoom_home*)_self, _dt);
}

// Back: out of a folder, else to the canvas (the welcome first, if it is up).
RDE_INTERNAL void fude_zoom_home_back(fude_zoom_home* _home) {
    if(_home->welcome) {
        fude_zoom_home_welcome_done(_home);
        return;
    }
    if(_home->held != FUDE_ZOOM_HOME_NONE) {
        _home->held = FUDE_ZOOM_HOME_NONE;
        return;
    }
    const fude_note* _f = _home->folder != 0u ? fude_notes_find(_home->app->notes, _home->folder) : NULL;
    if(_f != NULL) {
        _home->folder   = _f->parent;
        _home->gathered = false;
        fude_scroller_stop(&_home->scroller);
        _home->scroller.offset = 0.0f;
        return;
    }
    fude_zoom_home_close(_home);
}

// (Escape and the system's Back: the same)
RDE_INTERNAL void fude_zoom_home_screen_back(void* _self) {
    fude_zoom_home_back((fude_zoom_home*)_self);
}

FUDE_ROW_CALL(fude_zoom_home_row_back, fude_zoom_home, fude_zoom_home_back)
FUDE_ROW_CALL(fude_zoom_home_row_new, fude_zoom_home, fude_zoom_home_new)

static const fude_row_button FUDE_ZOOM_HOME_BUTTONS[] = {
    { FUDE_TEXT_BACK,                 FUDE_ICON_BACK, fude_zoom_home_row_back, 0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_ZOOM_HOME_NEW_CANVAS, FUDE_ICON_PLUS, fude_zoom_home_row_new,  0, FUDE_ROW_PRIMARY, false, NULL },
};
static const fude_row_def FUDE_ZOOM_HOME_ROWS[] = { FUDE_ROW_DEF(FUDE_ZOOM_HOME_BUTTONS) };

RDE_INTERNAL u32 fude_zoom_home_screen_row(const void* _self) {
    RDE_UNUSED(_self);
    return 0u;
}

// (New says what the app makes: a notebook, a project)
RDE_INTERNAL void fude_zoom_home_screen_faces(const void* _self, u32 _row, fude_row_face* _faces) {
    RDE_UNUSED(_row);
    const fude_zoom_home* _home = (const fude_zoom_home*)_self;
    snprintf(_faces[1].label, sizeof(_faces[1].label), "%s", fude_text(_home->new_text));
}

const fude_screen FUDE_ZOOM_HOME_SCREEN = {
    .name = "home", .input = FUDE_SCREEN_INPUT_POINT,
    .is_open = fude_zoom_home_screen_is_open, .close = fude_zoom_home_screen_back,
    .update = fude_zoom_home_screen_update, .render = fude_zoom_home_screen_render,
    .pointer_down = fude_zoom_home_screen_down, .pointer_moved = fude_zoom_home_screen_moved, .pointer_up = fude_zoom_home_screen_up,
    .rows = FUDE_ZOOM_HOME_ROWS, .row_count = 1u, .row = fude_zoom_home_screen_row, .faces = fude_zoom_home_screen_faces,
    .field_hint = FUDE_TEXT_COUNT,
};
