#ifndef FUDE_DRAW
#define FUDE_DRAW

#include "rde.h"

// ===========================================================================
// The drawing every screen shares — immediate mode, inside a 2D drawing block,
// in screen units (Y up): text at a size, a line, a box's outline, a stroke of
// even width. One copy, used by Check, Practice, the album, Browse, the chart,
// the viewer and the glyphs, instead of one each.
// ===========================================================================

// _text with its first line's baseline starting at (_x, _y), _px tall; _font was
// loaded at _font_px (text scales from there).
void fude_draw_text(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, rde_color _color);

// From _a to _b, _radius wide on each side.
void fude_draw_line(rde_vec_2F _a, rde_vec_2F _b, f32 _radius, rde_color _color);

// The outline of the box from _min (bottom-left) to _max (top-right).
void fude_draw_outline(rde_vec_2F _min, rde_vec_2F _max, f32 _radius, rde_color _color);

// A stroke through _count points, _radius wide all along (any number of points).
void fude_draw_stroke_even(const rde_vec_2F* _points, u32 _count, f32 _radius, rde_color _color);

// A card: a rounded box from _min to _max, _radius at the corners, filled, with
// a hairline border.
void fude_draw_card(rde_vec_2F _min, rde_vec_2F _max, f32 _radius, rde_color _fill, rde_color _border);
// A chip: _text in a pill starting at _x, centred on _mid. Returns its width.
f32  fude_draw_chip(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _mid, f32 _px, rde_color _fill, rde_color _color);
// Slug draws a character's em about this many times the size asked for.
#define FUDE_DRAW_EM 1.31f
// An icon (icons.h) centred on _center, _em across: in _font (Regular is the UI
// font's fallback, so the UI font draws it), or Phosphor Fill — its font set
// once by the app.
void fude_draw_icon(rde_font* _font, f32 _font_px, const c8* _icon, rde_vec_2F _center, f32 _em, rde_color _color);
void fude_draw_set_icon_fill(rde_font* _font, f32 _font_px);
void fude_draw_icon_fill(const c8* _icon, rde_vec_2F _center, f32 _em, rde_color _color);

// Right or wrong, as a disc (the score's good or poor colour) with a tick or a
// cross, _radius, centred on _center.
void fude_draw_verdict(rde_vec_2F _center, f32 _radius, b8 _right);

// How wide _text is at _px: a short one (a label, a chip) measured for real
// the first time and kept; a long one estimated — an advance per character,
// measured once per font: Japanese (all one width) and Latin (the average).
f32  fude_draw_text_width(rde_font* _font, f32 _font_px, const c8* _text, f32 _px);
// _text in lines no wider than _width — broken at spaces, or between Japanese
// characters, and at line breaks — the first line's baseline at _y, the next
// _line below each. How many lines.
u32  fude_draw_text_wrap(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, f32 _width, f32 _line, rde_color _color);
// How many lines fude_draw_text_wrap would draw, drawing nothing.
u32  fude_draw_text_wrap_lines(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width);
// The size to draw _text at so it fits _width: _px, or smaller (down to
// _min_scale of it) when it would not fit.
f32  fude_draw_text_px_to_fit(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width, f32 _min_scale);
// _text into _out, cut with "…" (at a word when one is near) where it would pass
// _width at _px.
void fude_draw_text_fit(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width, c8* _out, usize _size);

#endif
