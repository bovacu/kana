#include "study/app/welcome.h"
#include "drawing/app/screen.h"
#include "drawing/widgets/draw.h"
#include "drawing/widgets/icons.h"
#include "drawing/widgets/kit.h"
#include "drawing/base/text.h"
#include "drawing/base/theme.h"
#include "study/services/speech.h"

#include <math.h>

// ===========================================================================
// See welcome.h.
// ===========================================================================

#define FUDE_WELCOME_CARD_W  560.0f
#define FUDE_WELCOME_PAD     32.0f
#define FUDE_WELCOME_ICON    64.0f
#define FUDE_WELCOME_TITLE   26.0f
#define FUDE_WELCOME_BODY    18.0f
#define FUDE_WELCOME_LINE    27.0f
#define FUDE_WELCOME_BUTTON  (rde_vec_2F){ 190.0f, 48.0f }

static const struct { const c8* icon; FUDE_TEXT_ title; FUDE_TEXT_ body; } FUDE_WELCOME_PAGE[FUDE_WELCOME_PAGES] = {
    { FUDE_ICON_BOOK,  FUDE_TEXT_WELCOME_1_TITLE, FUDE_TEXT_WELCOME_1 },
    { FUDE_ICON_PEN,   FUDE_TEXT_WELCOME_2_TITLE, FUDE_TEXT_WELCOME_2 },
    { FUDE_ICON_CAMERA,     FUDE_TEXT_WELCOME_3_TITLE, FUDE_TEXT_WELCOME_3 },   // text from the camera
    { FUDE_ICON_LASSO,      FUDE_TEXT_WELCOME_4_TITLE, FUDE_TEXT_WELCOME_4 },   // copy, paste as handwriting, check
    { FUDE_ICON_TRANSLATE,  FUDE_TEXT_WELCOME_5_TITLE, FUDE_TEXT_WELCOME_5 },   // translation, both ways
    { FUDE_ICON_MENU,       FUDE_TEXT_WELCOME_6_TITLE, FUDE_TEXT_WELCOME_6 },   // what to study
    { FUDE_ICON_CHART_LINE, FUDE_TEXT_WELCOME_7_TITLE, FUDE_TEXT_WELCOME_7 },   // statistics, the album, your data
};

void fude_welcome_open(fude_welcome* _welcome) {
    _welcome->open    = true;
    _welcome->page    = 0;
    _welcome->pressed = 0;
}

void fude_welcome_offline(fude_welcome* _welcome) {
    if(_welcome->card != NULL) {
        fude_readcard_open(_welcome->card, FUDE_TEXT_OFFLINE_TITLE, FUDE_TEXT_OFFLINE, FUDE_WELCOME_CARD_SECONDS, _welcome->cards_read,
                           FUDE_WELCOME_CARD_OFFLINE);
    }
}

// Open settings: the device's text-to-speech (Android).
RDE_INTERNAL void fude_welcome_on_voice_settings(void) {
    fude_speech_open_settings();
}

void fude_welcome_voice_as(fude_welcome* _welcome, b8 _android) {
    if(_welcome->card == NULL) {
        return;
    }
    if(_android) {
        fude_readcard_open(_welcome->card, FUDE_TEXT_VOICE_TITLE, FUDE_TEXT_VOICE_STEPS_ANDROID, 0.0f, _welcome->cards_read, FUDE_WELCOME_CARD_VOICE);
        fude_readcard_set_action(_welcome->card, FUDE_TEXT_VOICE_OPEN, fude_welcome_on_voice_settings);
    } else {
        fude_readcard_open(_welcome->card, FUDE_TEXT_VOICE_TITLE, FUDE_TEXT_VOICE_STEPS_IOS, 0.0f, _welcome->cards_read, FUDE_WELCOME_CARD_VOICE);
    }
}

void fude_welcome_voice(fude_welcome* _welcome) {
#if defined(RDE_PLATFORM_ANDROID)
    fude_welcome_voice_as(_welcome, true);
#else
    fude_welcome_voice_as(_welcome, false);
#endif
}

void fude_welcome_update(fude_welcome* _welcome) {
    if(_welcome == NULL || _welcome->cards_read == NULL) {
        return;
    }
    const FUDE_SPEECH_ _voice = fude_speech_state();   // each frame: Android's engine starts, and is asked again coming back
    const u8           _read  = *_welcome->cards_read;
    if(_welcome->open || (_welcome->card != NULL && _welcome->card->open) || (_read & FUDE_WELCOME_CARD_OFFLINE) == 0u ||
       (_read & FUDE_WELCOME_CARD_VOICE) != 0u) {
        return;
    }
    if(_voice == FUDE_SPEECH_MISSING) {
        fude_welcome_voice(_welcome);   // after the tutorial and the offline card, once
    }
}

// Skipped or finished: the card, the first time.
RDE_INTERNAL void fude_welcome_close(fude_welcome* _welcome) {
    _welcome->open = false;
    if(_welcome->cards_read != NULL && (*_welcome->cards_read & FUDE_WELCOME_CARD_OFFLINE) == 0u) {
        fude_welcome_offline(_welcome);
    }
}

RDE_INTERNAL b8 fude_welcome_inside(rde_vec_2F _p, rde_vec_2F _min, rde_vec_2F _max) {
    return _p.x >= _min.x && _p.x <= _max.x && _p.y >= _min.y && _p.y <= _max.y;
}

void fude_welcome_pointer_down(fude_welcome* _welcome, rde_vec_2F _screen) {
    _welcome->pressed = fude_welcome_inside(_screen, _welcome->next_min, _welcome->next_max) ? 1
                      : fude_welcome_inside(_screen, _welcome->skip_min, _welcome->skip_max) ? 2 : 0;
}

void fude_welcome_pointer_up(fude_welcome* _welcome, rde_vec_2F _screen) {
    const i32 _was = _welcome->pressed;
    _welcome->pressed = 0;
    if(_was == 1 && fude_welcome_inside(_screen, _welcome->next_min, _welcome->next_max)) {
        if(_welcome->page + 1u < FUDE_WELCOME_PAGES) {
            _welcome->page++;
        } else {
            fude_welcome_close(_welcome);   // Start writing
        }
    } else if(_was == 2 && fude_welcome_inside(_screen, _welcome->skip_min, _welcome->skip_max)) {
        fude_welcome_close(_welcome);
    }
}

void fude_welcome_render(fude_welcome* _welcome, rde_window* _window, rde_font* _font, f32 _font_px) {
    if(!_welcome->open || _font == NULL) {
        return;
    }
    const fude_theme* _t      = fude_theme_active();
    const rde_vec_2I  _size   = rde_window_get_size(_window);
    const rde_vec_4I  _insets = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32         _sw     = (f32)_size.x;
    const f32         _sh     = (f32)_size.y;

    // The page's colour over everything, then the card in the middle.
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ _sw, _sh }, _t->page);
    const f32 _w     = fminf(FUDE_WELCOME_CARD_W, _sw - (f32)(_insets.x + _insets.z) - 32.0f);
    const f32 _inner = _w - 2.0f * FUDE_WELCOME_PAD;
    const u32 _page  = _welcome->page < FUDE_WELCOME_PAGES ? _welcome->page : FUDE_WELCOME_PAGES - 1u;
    FUDE_TEXT_ _title_id = FUDE_WELCOME_PAGE[_page].title, _body_id = FUDE_WELCOME_PAGE[_page].body;
#if defined(RDE_PLATFORM_ANDROID)
    // A stylus, not the Apple Pencil.
    if(_body_id == FUDE_TEXT_WELCOME_2) {
        _title_id = FUDE_TEXT_WELCOME_2_TITLE_ANDROID;
        _body_id  = FUDE_TEXT_WELCOME_2_ANDROID;
    }
#endif
    const c8* _title = fude_text(_title_id);
    const c8* _body  = fude_text(_body_id);
    const f32 _title_px = fude_draw_text_px_to_fit(_font, _font_px, _title, FUDE_WELCOME_TITLE, _inner, 0.6f);
    // On a phone the first page says the app is made for tablets, under its words.
    const c8* _phone = _page == 0u && fude_kit_compact(_window) ? fude_text(FUDE_TEXT_WELCOME_PHONE) : NULL;
    // A screen too short for it (a phone): smaller words and icon.
    f32 _body_px = FUDE_WELCOME_BODY, _line = FUDE_WELCOME_LINE, _icon = FUDE_WELCOME_ICON;
    u32 _lines   = fude_draw_text_wrap_lines(_font, _font_px, _body, _body_px, _inner);
    u32 _notes   = _phone != NULL ? fude_draw_text_wrap_lines(_font, _font_px, _phone, _body_px, _inner) : 0u;
    #define FUDE_WELCOME_H() (FUDE_WELCOME_PAD + _icon + 24.0f + _title_px * 1.4f + 16.0f + (f32)_lines * _line + (_notes > 0u ? 14.0f + (f32)_notes * _line : 0.0f) + \
                              28.0f + 12.0f + 28.0f + FUDE_WELCOME_BUTTON.y + FUDE_WELCOME_PAD)
    if(FUDE_WELCOME_H() > _sh - (f32)(_insets.y + _insets.w) - 32.0f) {
        _body_px = 15.0f;
        _line    = 22.0f;
        _icon    = 44.0f;
        _lines   = fude_draw_text_wrap_lines(_font, _font_px, _body, _body_px, _inner);
        _notes   = _phone != NULL ? fude_draw_text_wrap_lines(_font, _font_px, _phone, _body_px, _inner) : 0u;
    }
    const f32 _h = FUDE_WELCOME_H();
    #undef FUDE_WELCOME_H
    const f32 _cy  = ((f32)_insets.w - (f32)_insets.y) * 0.5f;   // the safe area's middle
    const f32 _top = _cy + _h * 0.5f;
    const f32 _l   = -_w * 0.5f;
    fude_draw_card((rde_vec_2F){ _l, _top - _h }, (rde_vec_2F){ _l + _w, _top }, 24.0f, _t->surface, _t->outline);

    f32 _y = _top - FUDE_WELCOME_PAD - _icon * 0.5f;
    fude_draw_icon(_font, _font_px, FUDE_WELCOME_PAGE[_page].icon, (rde_vec_2F){ 0.0f, _y }, _icon, _t->accent);
    _y -= _icon * 0.5f + 24.0f + _title_px;
    const f32 _tw = fude_draw_text_width(_font, _font_px, _title, _title_px);
    fude_draw_text(_font, _font_px, _title, -_tw * 0.5f, _y, _title_px, _t->text);
    _y -= _title_px * 0.4f + 16.0f + _body_px * 0.8f;
    fude_draw_text_wrap(_font, _font_px, _body, _l + FUDE_WELCOME_PAD, _y, _body_px, _inner, _line, _t->text_soft);
    _y -= (f32)_lines * _line + 12.0f;
    if(_notes > 0u) {
        fude_draw_text_wrap(_font, _font_px, _phone, _l + FUDE_WELCOME_PAD, _y - _body_px * 0.8f + 2.0f, _body_px, _inner, _line, _t->accent);
        _y -= 14.0f + (f32)_notes * _line;
    }

    // Where it is: a dot a page.
    for(u32 _i = 0; _i < FUDE_WELCOME_PAGES; _i++) {
        const f32 _x = ((f32)_i - (f32)(FUDE_WELCOME_PAGES - 1u) * 0.5f) * 18.0f;
        rde_rendering_2d_draw_circle((rde_vec_2F){ _x, _y }, _i == _page ? 5.0f : 3.5f, 20, _i == _page ? _t->accent : _t->outline, NULL);
    }

    // Skip (not on the last page) at the left, Next / Start writing at the right.
    const rde_vec_2F _b    = { fminf(FUDE_WELCOME_BUTTON.x, (_inner - 12.0f) * 0.5f), FUDE_WELCOME_BUTTON.y };   // halves of a narrow card
    const f32        _by   = _top - _h + FUDE_WELCOME_PAD + _b.y * 0.5f;
    const b8         _last = _page + 1u == FUDE_WELCOME_PAGES;
    const f32        _nx   = _l + _w - FUDE_WELCOME_PAD - _b.x * 0.5f;
    _welcome->next_min = (rde_vec_2F){ _nx - _b.x * 0.5f, _by - _b.y * 0.5f };
    _welcome->next_max = (rde_vec_2F){ _nx + _b.x * 0.5f, _by + _b.y * 0.5f };
    rde_color _next_c = _t->accent;
    if(_welcome->pressed == 1) {
        _next_c.r = (u8)((u32)_next_c.r * 85u / 100u); _next_c.g = (u8)((u32)_next_c.g * 85u / 100u); _next_c.b = (u8)((u32)_next_c.b * 85u / 100u);
    }
    fude_draw_card(_welcome->next_min, _welcome->next_max, 14.0f, _next_c, _next_c);
    const c8* _next  = fude_text(_last ? FUDE_TEXT_WELCOME_START : FUDE_TEXT_NEXT);
    const f32 _np    = fude_draw_text_px_to_fit(_font, _font_px, _next, 17.0f, _b.x - 24.0f, 0.6f);
    const f32 _nw    = fude_draw_text_width(_font, _font_px, _next, _np);
    fude_draw_text(_font, _font_px, _next, _nx - _nw * 0.5f, _by - _np * 0.36f, _np, _t->on_accent);
    if(_last) {
        _welcome->skip_min = _welcome->skip_max = (rde_vec_2F){ 0.0f, 0.0f };
    } else {
        const f32 _sx = _l + FUDE_WELCOME_PAD + _b.x * 0.5f;
        _welcome->skip_min = (rde_vec_2F){ _sx - _b.x * 0.5f, _by - _b.y * 0.5f };
        _welcome->skip_max = (rde_vec_2F){ _sx + _b.x * 0.5f, _by + _b.y * 0.5f };
        if(_welcome->pressed == 2) {
            fude_draw_card(_welcome->skip_min, _welcome->skip_max, 14.0f, _t->surface_2, _t->surface_2);
        }
        const c8* _skip = fude_text(FUDE_TEXT_WELCOME_SKIP);
        const f32 _sp   = fude_draw_text_px_to_fit(_font, _font_px, _skip, 17.0f, _b.x - 24.0f, 0.6f);
        const f32 _skw  = fude_draw_text_width(_font, _font_px, _skip, _sp);
        fude_draw_text(_font, _font_px, _skip, _sx - _skw * 0.5f, _by - _sp * 0.36f, _sp, _t->text_soft);
    }
}

// --- the screen (screen.h) ----------------------------------------------------------------

RDE_INTERNAL b8   fude_welcome_screen_is_open(const void* _self) { return ((const fude_welcome*)_self)->open; }
RDE_INTERNAL void fude_welcome_screen_down(void* _self, rde_vec_2F _at, b8 _pen, f64 _now) { RDE_UNUSED(_pen); RDE_UNUSED(_now); fude_welcome_pointer_down((fude_welcome*)_self, _at); }
RDE_INTERNAL void fude_welcome_screen_moved(void* _self, rde_vec_2F _at, f64 _now) { RDE_UNUSED(_self); RDE_UNUSED(_at); RDE_UNUSED(_now); }
RDE_INTERNAL void fude_welcome_screen_up(void* _self, rde_vec_2F _at, f64 _now) { RDE_UNUSED(_now); fude_welcome_pointer_up((fude_welcome*)_self, _at); }
RDE_INTERNAL void fude_welcome_screen_render(void* _self, const fude_screen_frame* _f) {
    fude_welcome_render((fude_welcome*)_self, _f->window, _f->font, _f->font_px);
}

// Over everything, the page and the screens drawn under it; no row of its own.
const fude_screen FUDE_WELCOME_SCREEN = {
    .name = "welcome", .input = FUDE_SCREEN_INPUT_POINT, .overlay = true,
    .is_open = fude_welcome_screen_is_open,
    .render = fude_welcome_screen_render,
    .pointer_down = fude_welcome_screen_down, .pointer_moved = fude_welcome_screen_moved, .pointer_up = fude_welcome_screen_up,
    .field_hint = FUDE_TEXT_COUNT,
};
