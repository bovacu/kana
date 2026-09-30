#include "draw.h"

// ===========================================================================
// See draw.h.
// ===========================================================================

#define KANA_DRAW_CHUNK 1024u   // points a stroke is drawn in at a time (the radii for them)

void kana_draw_text(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, rde_color _color) {
    if(_font == NULL || _text == NULL) {
        return;
    }
    const f32 _scale = _px / _font_px;
    rde_rendering_2d_draw_text_2(_font, _text, (rde_vec_3F){ _x, _y, 0.0f }, (rde_vec_2F){ _scale, _scale }, 0.0f, _color);
}

void kana_draw_line(rde_vec_2F _a, rde_vec_2F _b, f32 _radius, rde_color _color) {
    const rde_vec_2F _p[2] = { _a, _b };
    const f32        _r[2] = { _radius, _radius };
    rde_rendering_2d_draw_stroke(_p, _r, 2, _color);
}

void kana_draw_outline(rde_vec_2F _min, rde_vec_2F _max, f32 _radius, rde_color _color) {
    const rde_vec_2F _p[5] = { { _min.x, _max.y }, { _max.x, _max.y }, { _max.x, _min.y }, { _min.x, _min.y }, { _min.x, _max.y } };
    const f32        _r[5] = { _radius, _radius, _radius, _radius, _radius };
    rde_rendering_2d_draw_stroke(_p, _r, 5, _color);
}

void kana_draw_stroke_even(const rde_vec_2F* _points, u32 _count, f32 _radius, rde_color _color) {
    static f32 _radii[KANA_DRAW_CHUNK];
    static f32 _filled_with = -1.0f;
    if(_count == 0) {
        return;
    }
    if(_filled_with != _radius) {
        for(u32 _i = 0; _i < KANA_DRAW_CHUNK; _i++) {
            _radii[_i] = _radius;
        }
        _filled_with = _radius;
    }
    // In chunks that share their end point, so a long stroke stays joined.
    for(u32 _start = 0;; _start += KANA_DRAW_CHUNK - 1u) {
        const u32 _n = _count - _start < KANA_DRAW_CHUNK ? _count - _start : KANA_DRAW_CHUNK;
        rde_rendering_2d_draw_stroke(_points + _start, _radii, _n, _color);
        if(_start + _n >= _count) {
            break;
        }
    }
}
