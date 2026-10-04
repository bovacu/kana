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
// A developer's check (--text-check): texts drawn past the screen's sides, on the log.
void fude_draw_text_check(rde_window* _window);   // NULL: off

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
// Right to left (the UI's direction, set as it is built): directional icons —
// back, previous, undo — are drawn as their mirror images (fude_draw_icon_dir:
// the other glyph of their pair; any other icon as it is).
void      fude_draw_set_rtl(b8 _rtl);
b8        fude_draw_is_rtl(void);
const c8* fude_draw_icon_dir(const c8* _icon);

// A screen drawn right to left. The screens lay out left to right, as ever, and
// what they draw between begin and end lands mirrored about the middle of _left.._right:
// boxes, cards, lines and dots mirrored; a text or an icon put at the mirrored
// place but never itself mirrored (a text that started at x ends at x's mirror,
// so start-aligned text ends up right-aligned). fude_app does it around every
// screen when the UI is right to left, and maps the pointer back the same way
// (fude_draw_pointer), so a screen's own hit-testing holds. _on false: drawn as
// it is (and the pointer as it is).
void       fude_draw_mirror_begin(b8 _on, f32 _left, f32 _right);
void       fude_draw_mirror_end(void);
// What must keep its own left and right inside a mirrored screen — a
// character's strokes, handwriting, a photo, a page, a tick, a run of Latin —
// drawn as it is, its box (laid out from _min to _max) put at the box's mirrored
// place. Nests. _for_pointer: a press inside maps the same way (a box written
// in); not for what is only shown. Nothing outside a mirrored screen.
void       fude_draw_keep_begin(rde_vec_2F _min, rde_vec_2F _max, b8 _for_pointer);
void       fude_draw_keep_end(void);
// For drawing straight with the engine inside a screen: a laid-out x (or point)
// to where it is drawn — mirrored, moved with a kept box, or as it is.
f32        fude_draw_x(f32 _x);
rde_vec_2F fude_draw_at(rde_vec_2F _p);
// A pointer on the screen to where the screen laid out what is under it (the
// last screen drawn's mirror and kept boxes). _press: a new press, which picks
// the mapping (in a kept box or not) its whole gesture keeps.
rde_vec_2F fude_draw_pointer(rde_vec_2F _p, b8 _press);

// An icon at the start of a label, as one text ("+ Add"): into _out. Right to
// left, the pieces parted by right-to-left marks — a Phosphor icon (private
// use) reads as left to right, and would otherwise go with an English label to
// the left, or sit after an Arabic one.
#define FUDE_DRAW_RLM "\xE2\x80\x8F"   // U+200F, right-to-left mark
void fude_draw_icon_label(c8* _out, usize _size, const c8* _icon, const c8* _text);
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
// The narrowest width wrapping _text in as many lines as _width does: a short
// text's lines even (no word alone on the last). For labels and captions.
f32  fude_draw_text_balanced_width(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width);
// The size to draw _text at so it fits _width: _px, or smaller (down to
// _min_scale of it) when it would not fit.
f32  fude_draw_text_px_to_fit(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width, f32 _min_scale);
// _text whole in _width, its line's middle at _mid — never cut short: at _px,
// else smaller (to three quarters), else on up to _lines lines (smaller still if
// need be), else as small as it takes. Returns the size drawn at.
f32  fude_draw_text_whole(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _mid, f32 _px, f32 _width, u32 _lines, rde_color _color);

#endif
