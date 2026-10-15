// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/widgets/notice.h"
#include "drawing/base/theme.h"
#include "drawing/base/draw.h"
#include "drawing/base/clock.h"

// ===========================================================================
// See notice.h.
// ===========================================================================

#define FUDE_NOTICE_TIME 2.6    // How long it stays fully shown, in seconds,
#define FUDE_NOTICE_FADE 0.4    // and then how long it takes to fade out.

// The notice's text, whole (made by the first notice, on the std allocator, and kept).
RDE_INTERNAL rde_str fude_notice_text;
RDE_INTERNAL f64     fude_notice_at = 0.0;   // When it was shown, by the app's clock (clock.h); 0 when none is showing.

// Copies _text as the notice and starts its time.
void fude_notice_show(const c8* _text) {
    if(!rde_str_is_inited(&fude_notice_text)) {
        fude_notice_text = rde_str_new("", rde_memory_allocator_get_default_std());
    }
    rde_str_clear(&fude_notice_text);
    rde_str_append_str(&fude_notice_text, _text != NULL ? _text : "");
    fude_notice_at = fude_clock_now();   // the app's clock, so a screenshot taken mid-fade is the same every time
}

// Draws the notice in its pill, centred across, _above the bottom of the safe area, fading over its last
// FUDE_NOTICE_FADE seconds; once it has faded it is forgotten.
void fude_notice_render(rde_window* _window, rde_font* _font, f32 _font_px, f32 _above) {
    const c8* _text = rde_str_is_inited(&fude_notice_text) ? rde_str_to_char_ptr(&fude_notice_text) : "";
    if(fude_notice_at <= 0.0 || _text[0] == 0 || _font == NULL) {
        return;
    }
    const f64 _age = fude_clock_now() - fude_notice_at;
    if(_age >= FUDE_NOTICE_TIME + FUDE_NOTICE_FADE) {
        fude_notice_at = 0.0;
        return;
    }
    const f32         _fade = _age <= FUDE_NOTICE_TIME ? 1.0f : 1.0f - (f32)((_age - FUDE_NOTICE_TIME) / FUDE_NOTICE_FADE);
    const fude_theme* _t    = fude_theme_active();
    const rde_vec_2I  _size = rde_window_get_size(_window);
    const rde_vec_4I  _safe = rde_window_get_safe_area_insets(_window);   // Left, top, right, bottom.
    // Its words at a reading size (17, lines 23 apart), wrapped in a pill clear of the screen's sides (the toolbar is
    // often there): as wide as they are, at most 520 and at most 70% of the screen less 40, with balanced lines.
    const f32         _px    = 17.0f;
    const f32         _lh    = 23.0f;
    const f32         _most  = rde_math_min_f32(520.0f, (f32)_size.x * 0.7f - 40.0f);
    const f32         _w     = fude_draw_text_balanced_width(_font, _font_px, _text, _px, rde_math_min_f32(fude_draw_text_width(_font, _font_px, _text, _px), _most));
    const u32         _lines = fude_draw_text_wrap_lines(_font, _font_px, _text, _px, _w + 0.5f);
    const rde_vec_2F  _pill  = { _w + 40.0f, (f32)(_lines > 0u ? _lines - 1u : 0u) * _lh + _px + 26.0f };
    const rde_vec_2F  _c     = { 0.0f, -(f32)_size.y * 0.5f + (f32)_safe.w + _above + _pill.y * 0.5f };

    rde_color _back = _t->panel;
    _back.a         = (u8)((f32)_back.a * _fade);
    rde_color _edge = _t->outline;
    _edge.a         = (u8)((f32)_edge.a * _fade);
    fude_draw_card((rde_vec_2F){ _c.x - _pill.x * 0.5f, _c.y - _pill.y * 0.5f }, (rde_vec_2F){ _c.x + _pill.x * 0.5f, _c.y + _pill.y * 0.5f },
                   rde_math_min_f32(_pill.y * 0.5f, 22.0f), _back, _edge);
    rde_color _ink = _t->text;
    _ink.a         = (u8)((f32)_ink.a * _fade);
    const f32 _top = _c.y + (f32)(_lines > 0u ? _lines - 1u : 0u) * _lh * 0.5f;   // The first line's middle.
    fude_draw_text_wrap(_font, _font_px, _text, _c.x - _w * 0.5f, _top - _px * 0.36f, _px, _w + 0.5f, _lh, _ink);
}
