// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/app/readcard.h"
#include "drawing/base/redraw.h"
#include "drawing/app/screen.h"
#include "drawing/base/draw.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"

// ===========================================================================
// See readcard.h.
//
// The card is drawn in immediate mode each frame. The rectangles of its text and its buttons are stored as drawn, and
// the pointer is matched against last frame's. The wait is measured from the time the card opened, so it needs no
// update of its own.
// ===========================================================================

// Layout, in screen points: the card's widest size, its inner margin, the title's and the text's font sizes, the
// distance between the text's lines, and a button's size.
#define FUDE_READCARD_W       600.0f
#define FUDE_READCARD_PAD     28.0f
#define FUDE_READCARD_TITLE   24.0f
#define FUDE_READCARD_BODY    17.0f
#define FUDE_READCARD_LINE    26.0f
#define FUDE_READCARD_BUTTON  (rde_vec_2F){ 200.0f, 48.0f }
#define FUDE_READCARD_BAR     4.0f     // The scroll bar's width.
#define FUDE_READCARD_INSET   12.0f    // The text's room above its first line, and before the bar.

// Opens the card scrolled to the top, starts Close's wait, and clears any second button. See the header.
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

// Adds the second button, with words _text and _on to call when it is pressed.
void fude_readcard_set_action(fude_readcard* _card, u32 _text, void (*_on)(void)) {
    _card->action    = _text;
    _card->on_action = _on;
}

// Closes the card as read, whether by Close, Back (a plain card) or the second button: sets its bit, which the
// settings then save.
RDE_INTERNAL void fude_readcard_done(fude_readcard* _card) {
    _card->open = false;
    if(_card->read != NULL) {
        *_card->read |= _card->bit;   // Read; the settings save it.
    }
}

// Returns the whole seconds left of Close's wait, rounded up, or 0 when the wait is over.
u32 fude_readcard_wait(const fude_readcard* _card) {
    const f64 _left = (f64)_card->seconds - (rde_engine_get_time_now() - _card->opened_at);
    return _left > 0.0 ? (u32)rde_math_ceil_f64(_left) : 0u;
}

// Whether the wait is over, so Close and the second button work.
b8 fude_readcard_ready(const fude_readcard* _card) {
    return fude_readcard_wait(_card) == 0u;
}

// Whether point _p is inside the box _min to _max.
RDE_INTERNAL b8 fude_readcard_inside(rde_vec_2F _p, rde_vec_2F _min, rde_vec_2F _max) {
    return _p.x >= _min.x && _p.x <= _max.x && _p.y >= _min.y && _p.y <= _max.y;
}

// --- the pointer: Close, or the text's scroll; anywhere else nothing --------------------------

// A press on a button counts only once the wait is over; a press on the text starts scrolling it. Anywhere else, the
// press does nothing.
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

// Scrolls the text, when the press started on it.
void fude_readcard_pointer_moved(fude_readcard* _card, rde_vec_2F _screen, f64 _now) {
    if(_card->scroller.pressing) {
        fude_scroller_moved(&_card->scroller, _screen, _now);
    }
}

// Lets go of the text, or presses the button the pointer went down on, if it is still over it and the wait is over.
// The second button's function is called after the card has closed, so it may open another screen.
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

// Moves the text's scroll on.
void fude_readcard_update(fude_readcard* _card, f32 _dt) {
    if(!_card->open) {
        return;
    }
    fude_scroller_update(&_card->scroller, _dt, _card->content_h, _card->text_max.y - _card->text_min.y);
    rde_vec_2F _tap;
    fude_scroller_take_tap(&_card->scroller, &_tap);   // A tap on the text does nothing; it is taken and dropped.
}

// --- drawn ----------------------------------------------------------------------------------

// Draws the veil over the screen, then the card centred in the safe area: its title, its text (clipped, scrolled, with
// a scroll bar when it runs on), Close and the second button. Stores where each part is for the pointer.
void fude_readcard_render(fude_readcard* _card, rde_window* _window, rde_font* _font, f32 _font_px) {
    if(!_card->open || _font == NULL) {
        return;
    }
    const fude_theme* _t      = fude_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _insets = rde_window_get_safe_area_insets(_window);   // Left, top, right, bottom.
    const f32         _sw     = (f32)_size.x;
    const f32         _sh     = (f32)_size.y;

    // The page's colour, nearly opaque, over everything: what is under it shows through but can't be reached.
    rde_color _veil = _t->page;
    _veil.a         = 235u;
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ fude_draw_x(0.0f), 0.0f }, (rde_vec_2F){ _sw, _sh }, _veil);

    // The card is as wide as it may be and as tall as its text, up to the safe area; past that, the text scrolls.
    const f32 _w      = rde_math_min_f32(FUDE_READCARD_W, _sw - (f32)(_insets.x + _insets.z) - 32.0f);
    const f32 _inner  = _w - 2.0f * FUDE_READCARD_PAD;
    const f32 _text_w = _inner - FUDE_READCARD_BAR - FUDE_READCARD_INSET;   // Room for the scroll bar.
    const c8* _title  = fude_text((FUDE_TEXT_)_card->title);
    // The text, with each \n (two characters: strings hold no newline) made a line break. However long a translation
    // makes it, it is shown whole: the buffer grows to the text (it never needs more than the text's own length), and
    // is kept between frames, so drawing the card each frame allocates nothing once it has grown.
    static rde_arr TYPE(c8) _body_buf;
    if(!rde_arr_is_inited(&_body_buf)) {
        _body_buf = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    }
    const c8* _from = fude_text((FUDE_TEXT_)_card->body);
    rde_arr_resize(&_body_buf, rde_strlen(_from) + 1u);
    c8*   _body = (c8*)_body_buf.memory;
    usize _n    = 0;
    for(; *_from != 0; _from++) {
        if(_from[0] == '\\' && _from[1] == 'n') {
            _body[_n++] = '\n';
            _from++;
        } else {
            _body[_n++] = *_from;
        }
    }
    _body[_n] = 0;
    const f32 _tpx    = fude_draw_text_px_to_fit(_font, _font_px, _title, FUDE_READCARD_TITLE, _inner);
    const u32 _lines  = fude_draw_text_wrap_lines(_font, _font_px, _body, FUDE_READCARD_BODY, _text_w);
    _card->content_h  = (f32)_lines * FUDE_READCARD_LINE + 2.0f * FUDE_READCARD_INSET;
    // Close is alone at the right. With a second button, both are as wide as half the card allows. The card's height
    // is the fixed part (margins, title, gaps and buttons) plus the text's view, which is at least three lines.
    const rde_vec_2F _b      = { _card->action != FUDE_TEXT_COUNT ? rde_math_min_f32(FUDE_READCARD_BUTTON.x, (_inner - 12.0f) * 0.5f) : FUDE_READCARD_BUTTON.x,
                                 FUDE_READCARD_BUTTON.y };
    const f32        _fixed  = FUDE_READCARD_PAD + _tpx * 1.3f + 18.0f + 22.0f + _b.y + FUDE_READCARD_PAD;
    const f32        _room   = _sh - (f32)(_insets.y + _insets.w) - 48.0f;
    const f32        _view_h = rde_math_max_f32(FUDE_READCARD_LINE * 3.0f, rde_math_min_f32(_card->content_h, _room - _fixed));
    const f32        _h      = _fixed + _view_h;
    const f32        _cy     = ((f32)_insets.w - (f32)_insets.y) * 0.5f;   // The safe area's middle.
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
    if(_scrolls) {   // Hairlines above and below, where the text runs on.
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ fude_draw_x(0.0f), _y + 1.0f }, (rde_vec_2F){ _inner, 1.0f }, _t->outline);
        rde_rendering_2d_draw_rectangle((rde_vec_2F){ fude_draw_x(0.0f), _y - _view_h - 1.0f }, (rde_vec_2F){ _inner, 1.0f }, _t->outline);
    }
    rde_rendering_begin_clipping_rect(_window, (rde_vec_2I){ (i32)fude_draw_x(0.0f), (i32)(_y - _view_h * 0.5f) }, (rde_vec_2UI){ (u32)_inner + 4u, (u32)rde_math_max_f32(1.0f, _view_h) });
    fude_draw_text_wrap(_font, _font_px, _body, _card->text_min.x, _y - FUDE_READCARD_INSET - FUDE_READCARD_BODY * 0.8f + _card->scroller.offset, FUDE_READCARD_BODY, _text_w,
                        FUDE_READCARD_LINE, _t->text_soft);
    rde_rendering_end_clipping_rect();

    // When the text scrolls, a thin bar at its right shows where in the text the view is.
    if(_scrolls) {
        const f32 _track = _view_h - 8.0f;
        const f32 _thumb = rde_math_max_f32(28.0f, _track * _view_h / _card->content_h);
        const f32 _span  = rde_math_max_f32(1.0f, _card->content_h - _view_h);
        const f32 _at    = rde_math_min_f32(1.0f, rde_math_max_f32(0.0f, _card->scroller.offset / _span));
        const f32 _x     = _l + _w - FUDE_READCARD_PAD - FUDE_READCARD_BAR * 0.5f;
        const f32 _ty    = _y - 4.0f - _thumb * 0.5f - (_track - _thumb) * _at;
        rde_color _bar   = _t->text_soft;
        _bar.a           = 110u;
        fude_draw_card((rde_vec_2F){ _x - FUDE_READCARD_BAR * 0.5f, _ty - _thumb * 0.5f }, (rde_vec_2F){ _x + FUDE_READCARD_BAR * 0.5f, _ty + _thumb * 0.5f },
                       FUDE_READCARD_BAR * 0.5f, _bar, _bar);
    }

    // Close, at the bottom right: grey and counting down the seconds, then in the accent colour (darker while
    // pressed).
    const u32 _wait = fude_readcard_wait(_card);
    const f32 _bx   = _l + _w - FUDE_READCARD_PAD - _b.x * 0.5f;
    const f32 _by   = _top - _h + FUDE_READCARD_PAD + _b.y * 0.5f;
    _card->close_min = (rde_vec_2F){ _bx - _b.x * 0.5f, _by - _b.y * 0.5f };
    _card->close_max = (rde_vec_2F){ _bx + _b.x * 0.5f, _by + _b.y * 0.5f };
    c8 _label[512];
    if(_wait > 0u) {
        FUDE_TEXTF(_label, FUDE_TEXT_READ_CLOSE_IN, FUDE_TN(_wait));
        // Render on demand: the count goes down when the seconds left fall to the next whole number.
        fude_redraw_after((f64)_card->seconds - (rde_engine_get_time_now() - _card->opened_at) - (f64)(_wait - 1u));
        fude_draw_card(_card->close_min, _card->close_max, 14.0f, _t->surface_2, _t->surface_2);
    } else {
        rde_snprintf(_label, sizeof(_label), "%s", fude_text(FUDE_TEXT_CLOSE));
        rde_color _c = _t->accent;
        if(_card->pressing) {
            _c.r = (u8)((u32)_c.r * 85u / 100u); _c.g = (u8)((u32)_c.g * 85u / 100u); _c.b = (u8)((u32)_c.b * 85u / 100u);
        }
        fude_draw_card(_card->close_min, _card->close_max, 14.0f, _c, _c);
    }
    const f32 _lp = fude_draw_text_px_to_fit(_font, _font_px, _label, 17.0f, _b.x - 24.0f);
    const f32 _lw = fude_draw_text_width(_font, _font_px, _label, _lp);
    fude_draw_text(_font, _font_px, _label, _bx - _lw * 0.5f, _by - _lp * 0.36f, _lp, _wait > 0u ? _t->text_soft : _t->on_accent);

    // The second button, at Close's left, plain.
    if(_card->action != FUDE_TEXT_COUNT) {
        const f32 _ax = _bx - _b.x - 12.0f;
        _card->action_min = (rde_vec_2F){ _ax - _b.x * 0.5f, _by - _b.y * 0.5f };
        _card->action_max = (rde_vec_2F){ _ax + _b.x * 0.5f, _by + _b.y * 0.5f };
        fude_draw_card(_card->action_min, _card->action_max, 14.0f, _card->pressing_action ? _t->select_fill : _t->surface_2, _t->surface_2);
        const c8* _action = fude_text((FUDE_TEXT_)_card->action);
        const f32 _ap     = fude_draw_text_px_to_fit(_font, _font_px, _action, 17.0f, _b.x - 24.0f);
        const f32 _aw     = fude_draw_text_width(_font, _font_px, _action, _ap);
        fude_draw_text(_font, _font_px, _action, _ax - _aw * 0.5f, _by - _ap * 0.36f, _ap, _t->accent);
    }
}

// --- the screen (screen.h) ----------------------------------------------------------------

// Whether the card is open.
RDE_INTERNAL b8   fude_readcard_screen_is_open(const any _self) { return ((const fude_readcard*)_self)->open; }
// Back and Escape: a plain card (no wait) closes, as read; a must-read one does not.
RDE_INTERNAL void fude_readcard_screen_close(any _self) {
    fude_readcard* _card = (fude_readcard*)_self;
    if(_card->seconds <= 0.0f) {
        fude_readcard_done(_card);
    }
}
// The update, the pointer and the drawing hand on to the card's own functions above.
RDE_INTERNAL void fude_readcard_screen_update(struct fude_app* _app, any _self, f32 _dt) { RDE_UNUSED(_app); fude_readcard_update((fude_readcard*)_self, _dt); }
// A press.
RDE_INTERNAL void fude_readcard_screen_down(any _self, rde_vec_2F _at, b8 _pen, f64 _now) { RDE_UNUSED(_pen); fude_readcard_pointer_down((fude_readcard*)_self, _at, _now); }
// A move.
RDE_INTERNAL void fude_readcard_screen_moved(any _self, rde_vec_2F _at, f64 _now) { fude_readcard_pointer_moved((fude_readcard*)_self, _at, _now); }
// A lift.
RDE_INTERNAL void fude_readcard_screen_up(any _self, rde_vec_2F _at, f64 _now) { fude_readcard_pointer_up((fude_readcard*)_self, _at, _now); }
// Draws the card.
RDE_INTERNAL void fude_readcard_screen_render(any _self, const fude_screen_frame* _f) {
    fude_readcard_render((fude_readcard*)_self, _f->window, _f->font, _f->font_px);
}

// The card as a screen: an overlay over everything, with no row of its own and no text field. Escape and Back close
// only a plain card.
const fude_screen FUDE_READCARD_SCREEN = {
    .name = "readcard", .input = FUDE_SCREEN_INPUT_POINT, .overlay = true,
    .is_open = fude_readcard_screen_is_open,
    .close = fude_readcard_screen_close,
    .update = fude_readcard_screen_update,
    .render = fude_readcard_screen_render,
    .pointer_down = fude_readcard_screen_down, .pointer_moved = fude_readcard_screen_moved, .pointer_up = fude_readcard_screen_up,
    .field_hint = FUDE_TEXT_COUNT,
};
