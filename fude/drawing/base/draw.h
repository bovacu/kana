// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_DRAW
#define FUDE_DRAW

#include "rde.h"

// ===========================================================================
// Draw: the drawing every screen shares, in immediate mode inside a 2D drawing block, in screen units with Y up.
// Text at a size, a line, a box's outline, a stroke of even width, cards, chips, icons, and text that wraps or
// shrinks to fit.
//
// Where it sits: drawing/base, so it draws but knows no UI. Everything above uses it: Check, Practice, the album,
// Browse, the chart, the viewer, the glyphs, the widgets and the canvas apps share this one copy instead of keeping
// one each.
// Threads: main thread only. It draws through the engine's renderer and keeps state between calls (the mirror, the
// Fill font, the caches below).
// Design:
//   Right to left: screens always lay out left to right, and the drawing mirrors what they draw
//   (fude_draw_mirror_begin), so no screen needs code of its own for right-to-left languages.
//   Text widths are measured for real by the engine and kept, by font and the whole text, up to 1536 of them, the
//   one used longest ago making room for a new one (what a screen draws each frame stays kept). An estimate from
//   average advances runs short on real sentences (a notice wider than its pill, a wrapped line past its card) and is
//   off by a third for capitals and digits ("N5").
//   Wrapped text is laid out once and kept (the last 64 layouts, by font, text, width and size, each with all its
//   lines), so drawing it every frame only copies its lines. A text drawn whole on several lines keeps the size it
//   was found to fit at (the last 32), since finding it takes many layouts.
// ===========================================================================

// The UI font's files among the app's assets: Roboto in its four faces. The UI draws in the regular one, a canvas's
// notes in all four, and a PDF's visible text (pdf.h) in all four too, so every place that loads them names the same
// files.
#define FUDE_DRAW_FONT_REGULAR     "assets/fonts/Roboto-Regular.ttf"
#define FUDE_DRAW_FONT_BOLD        "assets/fonts/Roboto-Bold.ttf"
#define FUDE_DRAW_FONT_ITALIC      "assets/fonts/Roboto-Italic.ttf"
#define FUDE_DRAW_FONT_BOLD_ITALIC "assets/fonts/Roboto-BoldItalic.ttf"

// Draws _text with its first line's baseline starting at (_x, _y), _px tall. _font was loaded at _font_px, and the
// text is scaled from that size. A text that is only a number (a score, "3 / 20", "×2") is drawn in the language's
// own digits, as the UI strings are. A NULL font or text draws nothing.
void fude_draw_text(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, rde_color _color);
// Turns on a developer's check (--text-check): every text drawn past _window's sides is logged, once each.
void fude_draw_text_check(rde_window* _window);   // NULL turns the check off

// Draws a line from _a to _b, _radius wide on each side.
void fude_draw_line(rde_vec_2F _a, rde_vec_2F _b, f32 _radius, rde_color _color);

// Draws the outline of the box from _min (bottom-left) to _max (top-right), its line _radius wide on each side.
void fude_draw_outline(rde_vec_2F _min, rde_vec_2F _max, f32 _radius, rde_color _color);

// Draws a stroke through _count points, _radius wide all along. Any number of points: a long stroke is drawn in
// chunks that share their end points, so it stays joined.
void fude_draw_stroke_even(const rde_vec_2F* _points, u32 _count, f32 _radius, rde_color _color);

// Draws a card: a box from _min to _max, rounded by _radius at the corners, filled with _fill, with a hairline
// _border. An empty box draws nothing.
void fude_draw_card(rde_vec_2F _min, rde_vec_2F _max, f32 _radius, rde_color _fill, rde_color _border);
// Draws a chip: _text in a pill whose left edge is at _x, centred up and down on _mid. Returns the chip's width.
f32  fude_draw_chip(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _mid, f32 _px, rde_color _fill, rde_color _color);
// Slug (the engine's text renderer) draws a character's em about this many times the size asked for.
#define FUDE_DRAW_EM 1.31f
// Draws an icon (icons.h) centred on _center, _em across, in _font. The UI font draws Phosphor Regular icons, since
// Regular is its fallback; Phosphor Fill icons need the Fill font (see fude_draw_icon_fill). Right to left, a
// directional icon is drawn as its mirror image (fude_draw_icon_dir); only its place is mirrored, never the glyph.
void fude_draw_icon(rde_font* _font, f32 _font_px, const c8* _icon, rde_vec_2F _center, f32 _em, rde_color _color);
// The UI's direction: right to left or not, set as the UI is built. Right to left, directional icons (back,
// previous, undo) are drawn as their mirror images: fude_draw_icon_dir returns the other glyph of the icon's pair
// (back and forward, previous and next, undo and redo), and any other icon as it is.
void      fude_draw_set_rtl(b8 _rtl);
b8        fude_draw_is_rtl(void);
const c8* fude_draw_icon_dir(const c8* _icon);

// Draws a screen right to left. Screens lay out left to right, as always, and what they draw between begin and end
// lands mirrored about the middle of _left.._right. Boxes, cards, lines and dots are mirrored; a text or an icon is
// put at the mirrored place but never mirrored itself (a text that started at x ends at x's mirror, so start-aligned
// text ends up right-aligned). fude_app does this around every screen when the UI is right to left, and maps the
// pointer back the same way (fude_draw_pointer), so a screen's own hit-testing still holds. With _on false,
// everything is drawn as it is, and the pointer is left as it is.
void       fude_draw_mirror_begin(b8 _on, f32 _left, f32 _right);
void       fude_draw_mirror_end(void);
// Inside a mirrored screen, draws something that must keep its own left and right as it is: a character's strokes,
// handwriting, a photo, a page, a tick, a run of Latin text. Its box, laid out from _min to _max, is moved to the
// box's mirrored place. Calls nest; the outermost box decides the move. With _for_pointer, a press inside the box
// maps the same way (for a box the person writes in); leave it false for what is only shown. Outside a mirrored
// screen it changes nothing.
void       fude_draw_keep_begin(rde_vec_2F _min, rde_vec_2F _max, b8 _for_pointer);
void       fude_draw_keep_end(void);
// For drawing straight with the engine inside a screen: turns a laid-out x (or point) into where it is drawn,
// mirrored, moved with a kept box, or as it is.
f32        fude_draw_x(f32 _x);
rde_vec_2F fude_draw_at(rde_vec_2F _p);
// Maps a pointer position on the screen to where the screen laid out what is under it, using the last screen drawn's
// mirror and kept boxes. _press marks a new press, which picks the mapping (inside a kept box or not) that its whole
// gesture keeps.
rde_vec_2F fude_draw_pointer(rde_vec_2F _p, b8 _press);

// The right-to-left mark that fude_draw_icon_label puts around an icon in a right-to-left UI.
#define FUDE_DRAW_RLM "\xE2\x80\x8F"   // U+200F, right-to-left mark
// Writes an icon and a label as one text ("+ Add") into _out. Right to left, the icon (turned by fude_draw_icon_dir)
// is set between right-to-left marks: a Phosphor icon is a private-use character, which reads as left to right, and
// would otherwise go to the left of an English label, or sit after an Arabic one.
void fude_draw_icon_label(c8* _out, usize _size, const c8* _icon, const c8* _text);
// Sets the Phosphor Fill font, loaded at _font_px, for icons that show something is on (a chosen tick, a studied
// star). The app sets it once; the font stays the app's to free.
void fude_draw_set_icon_fill(rde_font* _font, f32 _font_px);
// Draws an icon from Phosphor Fill, as fude_draw_icon does. Draws nothing until fude_draw_set_icon_fill has run.
void fude_draw_icon_fill(const c8* _icon, rde_vec_2F _center, f32 _em, rde_color _color);

// Draws right or wrong as a disc in the score's good or poor colour with a tick or a cross on it, _radius in size,
// centred on _center. Never mirrored, as the platforms keep a tick.
void fude_draw_verdict(rde_vec_2F _center, f32 _radius, b8 _right);

// Returns how wide _text is at _px. It is measured for real the first time and kept (texts over about 1 KB are
// measured every time). A text holding '[', the engine's markup, or one that measures as nothing, is estimated
// instead from an advance per character, measured once per font: Japanese (all one width) and Latin (the average).
f32  fude_draw_text_width(rde_font* _font, f32 _font_px, const c8* _text, f32 _px);
// Draws _text in lines no wider than _width, the first line's baseline at _y and each next line _line below. Lines
// break at spaces, between Japanese or Chinese characters (never before closing punctuation or a small kana, never
// after an opening bracket), and at line breaks. Every line is drawn, however many and however long. Returns how many
// lines it drew.
u32  fude_draw_text_wrap(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, f32 _width, f32 _line, rde_color _color);
// Returns how many lines fude_draw_text_wrap would draw, drawing nothing.
u32  fude_draw_text_wrap_lines(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width);
// Fills _from and _to (room for _max each) with where each of those lines starts and ends in _text, in bytes.
// Returns how many lines it filled.
u32  fude_draw_text_wrap_spans(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width, u32* _from, u32* _to, u32 _max);
// Returns the narrowest width that wraps _text in as few lines as _width does, so a short text's lines come out even
// (no word alone on the last). For labels and captions. The answer is kept, since it takes a dozen layouts to find.
f32  fude_draw_text_balanced_width(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width);
// Returns the size to draw _text at so it fits _width: _px, or as much smaller as it takes. A text is never cut, so a
// longer language's labels come out smaller.
f32  fude_draw_text_px_to_fit(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width);
// Draws _text whole within _width, its middle at _mid, never cut short: at _px; else on one line down to three
// quarters of it; else on up to _lines lines, a little smaller each step down to half of _px (past that, on as many
// lines as it takes). With _lines under 2 it stays on one line, as small as it takes. Returns the size drawn at.
f32  fude_draw_text_whole(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _mid, f32 _px, f32 _width, u32 _lines, rde_color _color);

#endif
