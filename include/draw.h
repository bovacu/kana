#ifndef KANA_DRAW
#define KANA_DRAW

#include "rde.h"

// ===========================================================================
// The drawing every screen shares — immediate mode, inside a 2D drawing block,
// in screen units (Y up): text at a size, a line, a box's outline, a stroke of
// even width. One copy, used by Check, Practice, the album, Browse, the chart,
// the viewer and the glyphs, instead of one each.
// ===========================================================================

// _text with its first line's baseline starting at (_x, _y), _px tall; _font was
// loaded at _font_px (text scales from there).
void kana_draw_text(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, rde_color _color);

// From _a to _b, _radius wide on each side.
void kana_draw_line(rde_vec_2F _a, rde_vec_2F _b, f32 _radius, rde_color _color);

// The outline of the box from _min (bottom-left) to _max (top-right).
void kana_draw_outline(rde_vec_2F _min, rde_vec_2F _max, f32 _radius, rde_color _color);

// A stroke through _count points, _radius wide all along (any number of points).
void kana_draw_stroke_even(const rde_vec_2F* _points, u32 _count, f32 _radius, rde_color _color);

#endif
