#include "drawing/widgets/notice.h"
#include "drawing/base/theme.h"
#include "drawing/widgets/draw.h"

#include <stdio.h>

// ===========================================================================
// See notice.h.
// ===========================================================================

#define FUDE_NOTICE_TIME 2.6    // it stays...
#define FUDE_NOTICE_FADE 0.4    // ...then fades out over this

RDE_INTERNAL c8  fude_notice_text[192];
RDE_INTERNAL f64 fude_notice_at = 0.0;   // when it was shown (engine clock); 0: none

void fude_notice_show(const c8* _text) {
    snprintf(fude_notice_text, sizeof(fude_notice_text), "%s", _text);
    fude_notice_at = rde_engine_get_time_now();
}

void fude_notice_render(rde_window* _window, rde_font* _font, f32 _font_px, f32 _above) {
    if(fude_notice_at <= 0.0 || fude_notice_text[0] == 0 || _font == NULL) {
        return;
    }
    const f64 _age = rde_engine_get_time_now() - fude_notice_at;
    if(_age >= FUDE_NOTICE_TIME + FUDE_NOTICE_FADE) {
        fude_notice_at = 0.0;
        return;
    }
    const f32         _fade = _age <= FUDE_NOTICE_TIME ? 1.0f : 1.0f - (f32)((_age - FUDE_NOTICE_TIME) / FUDE_NOTICE_FADE);
    const fude_theme* _t    = fude_theme_active();
    const rde_vec_2I  _size = rde_window_get_size(_window);
    const rde_vec_4I  _safe = rde_window_get_safe_area_insets(_window);   // left, top, right, bottom
    const f32         _px   = fude_draw_text_px_to_fit(_font, _font_px, fude_notice_text, 17.0f, (f32)_size.x * 0.85f - 40.0f, 0.6f);
    const f32         _w    = fude_draw_text_width(_font, _font_px, fude_notice_text, _px);
    const rde_vec_2F  _pill = { _w + 40.0f, _px + 26.0f };
    const rde_vec_2F  _c    = { 0.0f, -(f32)_size.y * 0.5f + (f32)_safe.w + _above + _pill.y * 0.5f };

    rde_color _back = _t->panel;
    _back.a         = (u8)((f32)_back.a * _fade);
    rde_color _edge = _t->outline;
    _edge.a         = (u8)((f32)_edge.a * _fade);
    fude_draw_card((rde_vec_2F){ _c.x - _pill.x * 0.5f, _c.y - _pill.y * 0.5f }, (rde_vec_2F){ _c.x + _pill.x * 0.5f, _c.y + _pill.y * 0.5f }, _pill.y * 0.5f, _back, _edge);
    rde_color _ink = _t->text;
    _ink.a         = (u8)((f32)_ink.a * _fade);
    fude_draw_text(_font, _font_px, fude_notice_text, _c.x - _w * 0.5f, _c.y - _px * 0.36f, _px, _ink);
}
