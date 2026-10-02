#include "widgets/header.h"
#include "widgets/draw.h"
#include "base/theme.h"

// ===========================================================================
// See header.h.
// ===========================================================================

#define KANA_HEADER_TITLE_LARGE 24.0f   // kana_header_title's

void kana_header_badge(kana_glyph* _glyph, u32 _codepoint, rde_vec_2F _tl, f32 _box) {
    const kana_theme* _theme = kana_theme_active();
    const f32         _in    = _box * 0.14f;
    kana_draw_card((rde_vec_2F){ _tl.x, _tl.y - _box }, (rde_vec_2F){ _tl.x + _box, _tl.y }, _box * 0.26f, _theme->tint, _theme->tint);
    kana_glyph_character(_glyph, _codepoint, (rde_vec_2F){ _tl.x + _in, _tl.y - _in }, _box - 2.0f * _in, _theme->accent);
}

void kana_header_label(kana_glyph* _glyph, u32 _codepoint, f32 _left, f32 _mid) {
    const kana_theme* _theme = kana_theme_active();
    const f32         _box   = KANA_HEADER_LABEL;
    kana_draw_card((rde_vec_2F){ _left, _mid - _box * 0.5f }, (rde_vec_2F){ _left + _box, _mid + _box * 0.5f }, 8.0f, _theme->tint, _theme->tint);
    kana_glyph_character(_glyph, _codepoint, (rde_vec_2F){ _left + 4.0f, _mid + _box * 0.5f - 4.0f }, _box - 8.0f, _theme->accent);
}

void kana_header_draw(kana_glyph* _glyph, u32 _codepoint, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top,
                      const c8* _title, const c8* _caption, const c8* _right_text) {
    const kana_theme* _theme = kana_theme_active();
    const f32         _x     = _left + KANA_HEADER_TEXT_LEFT;
    const f32         _room  = _right - _x - (_right_text != NULL ? kana_draw_text_width(_font, _font_px, _right_text, 14.0f) + 16.0f : 0.0f);
    kana_header_badge(_glyph, _codepoint, (rde_vec_2F){ _left, _top }, KANA_HEADER_BADGE);
    kana_draw_text(_font, _font_px, _title, _x, _top - 19.0f, kana_draw_text_px_to_fit(_font, _font_px, _title, KANA_HEADER_TITLE_PX, _room, 0.6f), _theme->text);
    if(_caption != NULL) {
        kana_draw_text(_font, _font_px, _caption, _x, _top - 37.0f, kana_draw_text_px_to_fit(_font, _font_px, _caption, KANA_HEADER_CAPTION_PX, _room, 0.6f), _theme->text_soft);
    }
    if(_right_text != NULL) {
        kana_draw_text(_font, _font_px, _right_text, _right - kana_draw_text_width(_font, _font_px, _right_text, 14.0f), _top - 25.0f, 14.0f, _theme->text);
    }
}

void kana_header_title(rde_font* _font, f32 _font_px, const c8* _title, f32 _left, f32 _top) {
    kana_draw_text(_font, _font_px, _title, _left, _top - 30.0f, KANA_HEADER_TITLE_LARGE, kana_theme_active()->text);
}
