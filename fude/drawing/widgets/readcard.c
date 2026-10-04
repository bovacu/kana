#include "drawing/widgets/readcard.h"
#include "drawing/app/screen.h"
#include "drawing/widgets/draw.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"

#include <math.h>
#include <stdio.h>

// ===========================================================================
// See readcard.h.
// ===========================================================================

#define FUDE_READCARD_W       600.0f
#define FUDE_READCARD_PAD     28.0f
#define FUDE_READCARD_TITLE   24.0f
#define FUDE_READCARD_BODY    17.0f
#define FUDE_READCARD_LINE    26.0f
#define FUDE_READCARD_BUTTON  (rde_vec_2F){ 200.0f, 48.0f }
#define FUDE_READCARD_BAR     4.0f     // the scroll bar's width
#define FUDE_READCARD_INSET   12.0f    // the text's room above its first line, and before the bar

void fude_readcard_open(fude_readcard* _card, u32 _title, u32 _body, f32 _seconds, u8* _read, u8 _bit) {
    _card->open      = true;
    _card->title     = _title;
    _card->body      = _body;
    _card->seconds   = _seconds;
    _card->read      = _read;
    _card->bit       = _bit;
    _card->opened_at = rde_engine_get_time_now();
    _card->pressing  = false;
    _card->content_h = 0.0f;
    fude_scroller_stop(&_card->scroller);
    _card->scroller.offset = 0.0f;
    _card->action          = FUDE_TEXT_COUNT;
    _card->on_action       = NULL;
    _card->pressing_action = false;
}

void fude_readcard_set_action(fude_readcard* _card, u32 _text, void (*_on)(void)) {
    _card->action    = _text;
    _card->on_action = _on;
}

// Closed by Close (or Back, or its action): read.
RDE_INTERNAL void fude_readcard_done(fude_readcard* _card) {
    _card->open = false;
    if(_card->read != NULL) {
        *_card->read |= _card->bit;   // read: the settings save it
    }
}

u32 fude_readcard_wait(const fude_readcard* _card) {
    const f64 _left = (f64)_card->seconds - (rde_engine_get_time_now() - _card->opened_at);
    return _left > 0.0 ? (u32)ceil(_left) : 0u;
}

b8 fude_readcard_ready(const fude_readcard* _card) {
    return fude_readcard_wait(_card) == 0u;
}

RDE_INTERNAL b8 fude_readcard_inside(rde_vec_2F _p, rde_vec_2F _min, rde_vec_2F _max) {
    return _p.x >= _min.x && _p.x <= _max.x && _p.y >= _min.y && _p.y <= _max.y;
}

// --- the pointer: Close, or the text's scroll; anywhere else nothing --------------------------

void fude_readcard_pointer_down(fude_readcard* _card, rde_vec_2F _screen, f64 _now) {
    _card->pressing        = false;
    _card->pressing_action = false;
    if(_card->action != FUDE_TEXT_COUNT && fude_readcard_inside(_screen, _card->action_min, _card->action_max)) {
        _card->pressing_action = fude_readcard_ready(_card);
    } else if(fude_readcard_inside(_screen, _card->close_min, _card->close_max)) {
        _card->pressing = fude_readcard_ready(_card);
    } else if(fude_readcard_inside(_screen, _card->text_min, _card->text_max)) {
        fude_scroller_down(&_card->scroller, _screen, _now);
    }
}

void fude_readcard_pointer_moved(fude_readcard* _card, rde_vec_2F _screen, f64 _now) {
    if(_card->scroller.pressing) {
        fude_scroller_moved(&_card->scroller, _screen, _now);
    }
}

void fude_readcard_pointer_up(fude_readcard* _card, rde_vec_2F _screen, f64 _now) {
    if(_card->scroller.pressing) {
        fude_scroller_up(&_card->scroller, _now);
    }
    const b8 _was        = _card->pressing;
    const b8 _was_action = _card->pressing_action;
    _card->pressing        = false;
    _card->pressing_action = false;
    if(_was && fude_readcard_ready(_card) && fude_readcard_inside(_screen, _card->close_min, _card->close_max)) {
        fude_readcard_done(_card);
    } else if(_was_action && fude_readcard_ready(_card) && fude_readcard_inside(_screen, _card->action_min, _card->action_max)) {
        void (*_on)(void) = _card->on_action;
        fude_readcard_done(_card);
        if(_on != NULL) {
            _on();
        }
    }
}

void fude_readcard_update(fude_readcard* _card, f32 _dt) {
    if(!_card->open) {
        return;
    }
    fude_scroller_update(&_card->scroller, _dt, _card->content_h, _card->text_max.y - _card->text_min.y);
    rde_vec_2F _tap;
    fude_scroller_take_tap(&_card->scroller, &_tap);   // a tap on the text does nothing
}

// --- drawn ----------------------------------------------------------------------------------

void fude_readcard_render(fude_readcard* _card, rde_window* _window, rde_font* _font, f32 _font_px) {
    if(!_card->open || _font == NULL) {
        return;
    }
    const fude_theme* _t      = fude_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32         _sw     = (f32)_size.x;
    const f32         _sh     = (f32)_size.y;

    // The page, nearly opaque, over everything: what is under it shows but can't be reached.
    rde_color _veil = _t->page;
    _veil.a         = 235u;
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ fude_draw_x(0.0f), 0.0f }, (rde_vec_2F){ _sw, _sh }, _veil);

    // The card: as wide as it may be, as tall as its text — up to the safe area,
    // past which the text scrolls.
    const f32 _w      = fminf(FUDE_READCARD_W, _sw - (f32)(_insets.x + _insets.z) - 32.0f);
    const f32 _inner  = _w - 2.0f * FUDE_READCARD_PAD;
    const f32 _text_w = _inner - FUDE_READCARD_BAR - FUDE_READCARD_INSET;   // room for the scroll bar
    const c8* _title  = fude_text((FUDE_TEXT_)_card->title);
    static c8 _body[4096];   // its text, its \n (two characters: a strings file holds no newline) made line breaks
    {
        const c8* _from = fude_text((FUDE_TEXT_)_card->body);
        usize     _n    = 0;
        for(; *_from != 0 && _n + 1u < sizeof(_body); _from++) {
            if(_from[0] == '\\' && _from[1] == 'n') {
                _body[_n++] = '\n';
                _from++;
            } else {
                _body[_n++] = *_from;
            }
        }
        _body[_n] = 0;
    }
    const f32 _tpx    = fude_draw_text_px_to_fit(_font, _font_px, _title, FUDE_READCARD_TITLE, _inner, 0.6f);
    const u32 _lines  = fude_draw_text_wrap_lines(_font, _font_px, _body, FUDE_READCARD_BODY, _text_w);
    _card->content_h  = (f32)_lines * FUDE_READCARD_LINE + 2.0f * FUDE_READCARD_INSET;
    // Close alone at the right; with a second button, both as wide as halves of the card allow.
    const rde_vec_2F _b      = { _card->action != FUDE_TEXT_COUNT ? fminf(FUDE_READCARD_BUTTON.x, (_inner - 12.0f) * 0.5f) : FUDE_READCARD_BUTTON.x,
                                 FUDE_READCARD_BUTTON.y };
    const f32        _fixed  = FUDE_READCARD_PAD + _tpx * 1.3f + 18.0f + 22.0f + _b.y + FUDE_READCARD_PAD;
    const f32        _room   = _sh - (f32)(_insets.y + _insets.w) - 48.0f;
    const f32        _view_h = fmaxf(FUDE_READCARD_LINE * 3.0f, fminf(_card->content_h, _room - _fixed));
    const f32        _h      = _fixed + _view_h;
    const f32        _cy     = ((f32)_insets.w - (f32)_insets.y) * 0.5f;   // the safe area's middle
    const f32        _top    = _cy + _h * 0.5f;
    const f32        _l      = -_w * 0.5f;
    fude_draw_card((rde_vec_2F){ _l, _top - _h }, (rde_vec_2F){ _l + _w, _top }, 24.0f, _t->surface, _t->outline);

    // The title, centred.
    f32       _y  = _top - FUDE_READCARD_PAD - _tpx;
    const f32 _tw = fude_draw_text_width(_font, _font_px, _title, _tpx);
    fude_draw_text(_font, _font_px, _title, -_tw * 0.5f, _y, _tpx, _t->text);
    _y -= _tpx * 0.3f + 18.0f;

    // The text, clipped to its view and scrolled.
    _card->text_min = (rde_vec_2F){ _l + FUDE_READCARD_PAD, _y - _view_h };
    _card->text_max = (rde_vec_2F){ _l + _w - FUDE_READCARD_PAD, _y };
    const b8 _scrolls = _card->content_h > _view_h + 0.5f;
    if(_scrolls) {   // hairlines where the text runs on, above and below
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ fude_draw_x(0.0f), _y + 1.0f }, (rde_vec_2F){ _inner, 1.0f }, _t->outline);
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ fude_draw_x(0.0f), _y - _view_h - 1.0f }, (rde_vec_2F){ _inner, 1.0f }, _t->outline);
    }
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)fude_draw_x(0.0f), (i32)(_y - _view_h * 0.5f) }, (rde_vec_2UI){ (u32)_inner + 4u, (u32)fmaxf(1.0f, _view_h) });
    fude_draw_text_wrap(_font, _font_px, _body, _card->text_min.x, _y - FUDE_READCARD_INSET - FUDE_READCARD_BODY * 0.8f + _card->scroller.offset, FUDE_READCARD_BODY, _text_w,
                        FUDE_READCARD_LINE, _t->text_soft);
    rde_rendering_end_clipping_rect();

    // Where in the text: a thin bar at its right, when it scrolls.
    if(_scrolls) {
        const f32 _track = _view_h - 8.0f;
        const f32 _thumb = fmaxf(28.0f, _track * _view_h / _card->content_h);
        const f32 _span  = fmaxf(1.0f, _card->content_h - _view_h);
        const f32 _at    = fminf(1.0f, fmaxf(0.0f, _card->scroller.offset / _span));
        const f32 _x     = _l + _w - FUDE_READCARD_PAD - FUDE_READCARD_BAR * 0.5f;
        const f32 _ty    = _y - 4.0f - _thumb * 0.5f - (_track - _thumb) * _at;
        rde_color _bar   = _t->text_soft;
        _bar.a           = 110u;
        fude_draw_card((rde_vec_2F){ _x - FUDE_READCARD_BAR * 0.5f, _ty - _thumb * 0.5f }, (rde_vec_2F){ _x + FUDE_READCARD_BAR * 0.5f, _ty + _thumb * 0.5f },
                       FUDE_READCARD_BAR * 0.5f, _bar, _bar);
    }

    // Close, at the bottom right: counting down, then the accent's.
    const u32 _wait = fude_readcard_wait(_card);
    const f32 _bx   = _l + _w - FUDE_READCARD_PAD - _b.x * 0.5f;
    const f32 _by   = _top - _h + FUDE_READCARD_PAD + _b.y * 0.5f;
    _card->close_min = (rde_vec_2F){ _bx - _b.x * 0.5f, _by - _b.y * 0.5f };
    _card->close_max = (rde_vec_2F){ _bx + _b.x * 0.5f, _by + _b.y * 0.5f };
    c8 _label[64];
    if(_wait > 0u) {
        FUDE_TEXTF(_label, FUDE_TEXT_READ_CLOSE_IN, FUDE_TN(_wait));
        fude_draw_card(_card->close_min, _card->close_max, 14.0f, _t->surface_2, _t->surface_2);
    } else {
        snprintf(_label, sizeof(_label), "%s", fude_text(FUDE_TEXT_CLOSE));
        rde_color _c = _t->accent;
        if(_card->pressing) {
            _c.r = (u8)((u32)_c.r * 85u / 100u); _c.g = (u8)((u32)_c.g * 85u / 100u); _c.b = (u8)((u32)_c.b * 85u / 100u);
        }
        fude_draw_card(_card->close_min, _card->close_max, 14.0f, _c, _c);
    }
    const f32 _lp = fude_draw_text_px_to_fit(_font, _font_px, _label, 17.0f, _b.x - 24.0f, 0.6f);
    const f32 _lw = fude_draw_text_width(_font, _font_px, _label, _lp);
    fude_draw_text(_font, _font_px, _label, _bx - _lw * 0.5f, _by - _lp * 0.36f, _lp, _wait > 0u ? _t->text_soft : _t->on_accent);

    // The second button, at Close's left: a plain one.
    if(_card->action != FUDE_TEXT_COUNT) {
        const f32 _ax = _bx - _b.x - 12.0f;
        _card->action_min = (rde_vec_2F){ _ax - _b.x * 0.5f, _by - _b.y * 0.5f };
        _card->action_max = (rde_vec_2F){ _ax + _b.x * 0.5f, _by + _b.y * 0.5f };
        fude_draw_card(_card->action_min, _card->action_max, 14.0f, _card->pressing_action ? _t->select_fill : _t->surface_2, _t->surface_2);
        const c8* _action = fude_text((FUDE_TEXT_)_card->action);
        const f32 _ap     = fude_draw_text_px_to_fit(_font, _font_px, _action, 17.0f, _b.x - 24.0f, 0.6f);
        const f32 _aw     = fude_draw_text_width(_font, _font_px, _action, _ap);
        fude_draw_text(_font, _font_px, _action, _ax - _aw * 0.5f, _by - _ap * 0.36f, _ap, _t->accent);
    }
}

// --- the screen (screen.h) ----------------------------------------------------------------

RDE_INTERNAL b8   fude_readcard_screen_is_open(const void* _self) { return ((const fude_readcard*)_self)->open; }
// Back: a plain card (no wait) closes; a must-read one does not.
RDE_INTERNAL void fude_readcard_screen_close(void* _self) {
    fude_readcard* _card = (fude_readcard*)_self;
    if(_card->seconds <= 0.0f) {
        fude_readcard_done(_card);
    }
}
RDE_INTERNAL void fude_readcard_screen_update(struct fude_app* _app, void* _self, f32 _dt) { RDE_UNUSED(_app); fude_readcard_update((fude_readcard*)_self, _dt); }
RDE_INTERNAL void fude_readcard_screen_down(void* _self, rde_vec_2F _at, b8 _pen, f64 _now) { RDE_UNUSED(_pen); fude_readcard_pointer_down((fude_readcard*)_self, _at, _now); }
RDE_INTERNAL void fude_readcard_screen_moved(void* _self, rde_vec_2F _at, f64 _now) { fude_readcard_pointer_moved((fude_readcard*)_self, _at, _now); }
RDE_INTERNAL void fude_readcard_screen_up(void* _self, rde_vec_2F _at, f64 _now) { fude_readcard_pointer_up((fude_readcard*)_self, _at, _now); }
RDE_INTERNAL void fude_readcard_screen_render(void* _self, const fude_screen_frame* _f) {
    fude_readcard_render((fude_readcard*)_self, _f->window, _f->font, _f->font_px);
}

// Over everything, no row of its own; Escape and Back close only a plain card.
const fude_screen FUDE_READCARD_SCREEN = {
    .name = "readcard", .input = FUDE_SCREEN_INPUT_POINT, .overlay = true,
    .is_open = fude_readcard_screen_is_open,
    .close = fude_readcard_screen_close,
    .update = fude_readcard_screen_update,
    .render = fude_readcard_screen_render,
    .pointer_down = fude_readcard_screen_down, .pointer_moved = fude_readcard_screen_moved, .pointer_up = fude_readcard_screen_up,
    .field_hint = FUDE_TEXT_COUNT,
};
