#include "welcome.h"
#include "draw.h"
#include "icons.h"
#include "text.h"
#include "theme.h"

#include <math.h>

// ===========================================================================
// See welcome.h.
// ===========================================================================

#define KANA_WELCOME_CARD_W  560.0f
#define KANA_WELCOME_PAD     32.0f
#define KANA_WELCOME_ICON    64.0f
#define KANA_WELCOME_TITLE   26.0f
#define KANA_WELCOME_BODY    18.0f
#define KANA_WELCOME_LINE    27.0f
#define KANA_WELCOME_BUTTON  (rde_vec_2F){ 190.0f, 48.0f }

static const struct { const c8* icon; KANA_TEXT_ title; KANA_TEXT_ body; } KANA_WELCOME_PAGE[KANA_WELCOME_PAGES] = {
    { KANA_ICON_BOOK,  KANA_TEXT_WELCOME_1_TITLE, KANA_TEXT_WELCOME_1 },
    { KANA_ICON_PEN,   KANA_TEXT_WELCOME_2_TITLE, KANA_TEXT_WELCOME_2 },
    { KANA_ICON_LASSO, KANA_TEXT_WELCOME_3_TITLE, KANA_TEXT_WELCOME_3 },
    { KANA_ICON_MENU,  KANA_TEXT_WELCOME_4_TITLE, KANA_TEXT_WELCOME_4 },
};

void kana_welcome_open(kana_welcome* _welcome) {
    _welcome->open    = true;
    _welcome->page    = 0;
    _welcome->pressed = 0;
}

RDE_INTERNAL b8 kana_welcome_inside(rde_vec_2F _p, rde_vec_2F _min, rde_vec_2F _max) {
    return _p.x >= _min.x && _p.x <= _max.x && _p.y >= _min.y && _p.y <= _max.y;
}

void kana_welcome_pointer_down(kana_welcome* _welcome, rde_vec_2F _screen) {
    _welcome->pressed = kana_welcome_inside(_screen, _welcome->next_min, _welcome->next_max) ? 1
                      : kana_welcome_inside(_screen, _welcome->skip_min, _welcome->skip_max) ? 2 : 0;
}

void kana_welcome_pointer_up(kana_welcome* _welcome, rde_vec_2F _screen) {
    const i32 _was = _welcome->pressed;
    _welcome->pressed = 0;
    if(_was == 1 && kana_welcome_inside(_screen, _welcome->next_min, _welcome->next_max)) {
        if(_welcome->page + 1u < KANA_WELCOME_PAGES) {
            _welcome->page++;
        } else {
            _welcome->open = false;   // Start writing
        }
    } else if(_was == 2 && kana_welcome_inside(_screen, _welcome->skip_min, _welcome->skip_max)) {
        _welcome->open = false;
    }
}

void kana_welcome_render(kana_welcome* _welcome, rde_window* _window, rde_font* _font, f32 _font_px) {
    if(!_welcome->open || _font == NULL) {
        return;
    }
    const kana_theme* _t      = kana_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32         _sw     = (f32)_size.x;
    const f32         _sh     = (f32)_size.y;

    // The page's colour over everything, then the card in the middle.
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ _sw, _sh }, _t->page);
    const f32 _w     = fminf(KANA_WELCOME_CARD_W, _sw - (f32)(_insets.x + _insets.z) - 32.0f);
    const f32 _inner = _w - 2.0f * KANA_WELCOME_PAD;
    const u32 _page  = _welcome->page < KANA_WELCOME_PAGES ? _welcome->page : KANA_WELCOME_PAGES - 1u;
    const c8* _title = kana_text(KANA_WELCOME_PAGE[_page].title);
    const c8* _body  = kana_text(KANA_WELCOME_PAGE[_page].body);
    const f32 _title_px = kana_draw_text_px_to_fit(_font, _font_px, _title, KANA_WELCOME_TITLE, _inner, 0.6f);
    const u32 _lines = kana_draw_text_wrap_lines(_font, _font_px, _body, KANA_WELCOME_BODY, _inner);
    const f32 _h = KANA_WELCOME_PAD + KANA_WELCOME_ICON + 24.0f + _title_px * 1.4f + 16.0f + (f32)_lines * KANA_WELCOME_LINE + 28.0f + 12.0f + 28.0f +
                   KANA_WELCOME_BUTTON.y + KANA_WELCOME_PAD;
    const f32 _cy  = ((f32)_insets.w - (f32)_insets.y) * 0.5f;   // the safe area's middle
    const f32 _top = _cy + _h * 0.5f;
    const f32 _l   = -_w * 0.5f;
    kana_draw_card((rde_vec_2F){ _l, _top - _h }, (rde_vec_2F){ _l + _w, _top }, 24.0f, _t->surface, _t->outline);

    f32 _y = _top - KANA_WELCOME_PAD - KANA_WELCOME_ICON * 0.5f;
    kana_draw_icon(_font, _font_px, KANA_WELCOME_PAGE[_page].icon, (rde_vec_2F){ 0.0f, _y }, KANA_WELCOME_ICON, _t->accent);
    _y -= KANA_WELCOME_ICON * 0.5f + 24.0f + _title_px;
    const f32 _tw = kana_draw_text_width(_font, _font_px, _title, _title_px);
    kana_draw_text(_font, _font_px, _title, -_tw * 0.5f, _y, _title_px, _t->text);
    _y -= _title_px * 0.4f + 16.0f + KANA_WELCOME_BODY * 0.8f;
    kana_draw_text_wrap(_font, _font_px, _body, _l + KANA_WELCOME_PAD, _y, KANA_WELCOME_BODY, _inner, KANA_WELCOME_LINE, _t->text_soft);
    _y -= (f32)_lines * KANA_WELCOME_LINE + 12.0f;

    // Where it is: a dot a page.
    for(u32 _i = 0; _i < KANA_WELCOME_PAGES; _i++) {
        const f32 _x = ((f32)_i - (f32)(KANA_WELCOME_PAGES - 1u) * 0.5f) * 18.0f;
        rde_rendering_2d_draw_circle((rde_vec_2F){ _x, _y }, _i == _page ? 5.0f : 3.5f, 20, _i == _page ? _t->accent : _t->outline, NULL);
    }

    // Skip (not on the last page) at the left, Next / Start writing at the right.
    const rde_vec_2F _b    = KANA_WELCOME_BUTTON;
    const f32        _by   = _top - _h + KANA_WELCOME_PAD + _b.y * 0.5f;
    const b8         _last = _page + 1u == KANA_WELCOME_PAGES;
    const f32        _nx   = _l + _w - KANA_WELCOME_PAD - _b.x * 0.5f;
    _welcome->next_min = (rde_vec_2F){ _nx - _b.x * 0.5f, _by - _b.y * 0.5f };
    _welcome->next_max = (rde_vec_2F){ _nx + _b.x * 0.5f, _by + _b.y * 0.5f };
    rde_color _next_c = _t->accent;
    if(_welcome->pressed == 1) {
        _next_c.r = (u8)((u32)_next_c.r * 85u / 100u); _next_c.g = (u8)((u32)_next_c.g * 85u / 100u); _next_c.b = (u8)((u32)_next_c.b * 85u / 100u);
    }
    kana_draw_card(_welcome->next_min, _welcome->next_max, 14.0f, _next_c, _next_c);
    const c8* _next  = kana_text(_last ? KANA_TEXT_WELCOME_START : KANA_TEXT_NEXT);
    const f32 _np    = kana_draw_text_px_to_fit(_font, _font_px, _next, 17.0f, _b.x - 24.0f, 0.6f);
    const f32 _nw    = kana_draw_text_width(_font, _font_px, _next, _np);
    kana_draw_text(_font, _font_px, _next, _nx - _nw * 0.5f, _by - _np * 0.36f, _np, _t->on_accent);
    if(_last) {
        _welcome->skip_min = _welcome->skip_max = (rde_vec_2F){ 0.0f, 0.0f };
    } else {
        const f32 _sx = _l + KANA_WELCOME_PAD + _b.x * 0.5f;
        _welcome->skip_min = (rde_vec_2F){ _sx - _b.x * 0.5f, _by - _b.y * 0.5f };
        _welcome->skip_max = (rde_vec_2F){ _sx + _b.x * 0.5f, _by + _b.y * 0.5f };
        if(_welcome->pressed == 2) {
            kana_draw_card(_welcome->skip_min, _welcome->skip_max, 14.0f, _t->surface_2, _t->surface_2);
        }
        const c8* _skip = kana_text(KANA_TEXT_WELCOME_SKIP);
        const f32 _sp   = kana_draw_text_px_to_fit(_font, _font_px, _skip, 17.0f, _b.x - 24.0f, 0.6f);
        const f32 _skw  = kana_draw_text_width(_font, _font_px, _skip, _sp);
        kana_draw_text(_font, _font_px, _skip, _sx - _skw * 0.5f, _by - _sp * 0.36f, _sp, _t->text_soft);
    }
}
