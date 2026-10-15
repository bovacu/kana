// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_HANDMATH
#define FUDE_ZOOM_HANDMATH

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/tex.h"

// ===========================================================================
// Handmath: a typeset formula (tex.h) written out as pen strokes, so a formula can be turned into handwriting. Its
// symbol shapes are also what the handwritten maths reader (mathread.h) recognises symbols by.
//
// Where it sits: zoom, the canvas apps' layer. The page uses it for the lasso's Handwriting button on formulas
// (page_text.c), and keeps one fude_zoom_handmath_chars for the formulas' font (made in page.c). mathread.c builds its
// templates from fude_zoom_handmath_shape.
// Its main types and their lifecycle: fude_zoom_handmath_chars is the caller's struct. fude_zoom_handmath_chars_init
// creates its map on the standard heap and fude_zoom_handmath_chars_free frees it. The arrays the strokes go into are
// the caller's.
// Threads: called on the main thread. It keeps no global state: each call touches only the caller's struct and arrays.
// Files: none. The Hershey shapes are compiled in from hand_maths.h, which tools/hershey/hand_maths.py generates.
// Design:
//   Each glyph is written with the shape of its character from the Hershey maths and Greek sets (hand_maths.h, 180
//   symbols: digits, letters, Greek, + − × ÷ ± = ≠ ≤ ≥ < > ∑ ∏ ∫ √ ∞ ∂ ∇ → ∈ ⊂ …). About 140 more are drawn in
//   handmath.c as short text specs (accents, arrows, set and logic signs, ⌊⌋ ⟨⟩, ∅ ≈ ∬ ∮), and these are looked for
//   first. They also replace the Hershey forms of ∑ ∏ ∫ ∇ E M W [ ], which Hershey draws thick, with several strokes
//   side by side, where a hand writes a single stroke.
//   The shape is fitted into the glyph's own ink box, so it sits where the typeset formula has it and is as big. Its
//   strokes are smoothed (corners sharper than 60° stay corners) and made a little uneven, as a hand's are, seeded so
//   the same formula always comes out the same.
//   Rules (a fraction's bar, a root's bar) are written as lines. A tall bracket or root that the font builds out of
//   several pieces is written as one shape over all of them.
//   Glyphs and rules are written in the order a hand writes them: left to right, so a fraction's bar comes before
//   what is over it.
//   The typesetter keeps only glyph numbers, so the character each glyph stands for is read back from the font
//   (fude_zoom_handmath_chars).
//   See docs/infinite_canvas_design.md §65.
// ===========================================================================

// --- the shapes ------------------------------------------------------------------------------------

// Returns how many characters have a Hershey shape (the hand_maths.h table), and points *_cps at their code points in
// ascending order. The table is static, so the pointer stays valid for the program's life. The shapes drawn in
// handmath.c itself are not in this list.
u32 fude_zoom_handmath_symbols(const u32** _cps);
// Appends the shape of character _cp and returns how many strokes it added; 0 means there is no shape for it.
// The points (fude_zoom_v2) go to _points in Hershey units (a capital is 21 tall), y up, the baseline at y = 0. For
// each stroke, _ends (u32) gets the index one past its last point, counted from the start of _points. _cp is first
// mapped through fude_zoom_handmath_base, and a shape drawn in handmath.c is used before a Hershey one.
u32 fude_zoom_handmath_shape(u32 _cp, rde_arr* _points, rde_arr* _ends);
// Returns the character whose shape stands for _cp: a letter or digit of a maths alphabet (𝑥 𝐀 𝔄 𝟐 …) gives its plain
// letter or digit, and ℎ gives h. Any other character is returned as it is, except U+2061 (the invisible function
// application), which gives 0.
u32 fude_zoom_handmath_base(u32 _cp);

// --- a font's glyphs' characters -------------------------------------------------------------------------

// The character each glyph of a font stands for: its own character, read back from the font's cmap, or, for a bigger
// form of a glyph or a piece of a tall bracket, the character it is a form or a piece of.
typedef struct {
    rde_hash_map chars;   // glyph number (u64 key) to character (u32)
    const any    font;    // the font it was last read from, or NULL before the first read; only compared, never read
} fude_zoom_handmath_chars;

// fude_zoom_handmath_chars_init sets up an empty map on the standard heap; fude_zoom_handmath_chars_free frees it and
// clears the struct.
void fude_zoom_handmath_chars_init(fude_zoom_handmath_chars* _c);
void fude_zoom_handmath_chars_free(fude_zoom_handmath_chars* _c);
// Fills the map from _font, replacing what was there. It does nothing when _font is NULL or is the font it was last
// read from (compared by address), so it is cheap to call before every use.
void fude_zoom_handmath_chars_read(fude_zoom_handmath_chars* _c, const fude_zoom_mfont* _font);
// Returns the character glyph _g stands for, already mapped through fude_zoom_handmath_base, or 0 when it is not known.
u32  fude_zoom_handmath_char(const fude_zoom_handmath_chars* _c, u32 _g);

// --- written ----------------------------------------------------------------------------------------

// Writes _box (set by tex.h with _font, and _c read from the same font) out by hand, and returns how many strokes it
// added. _em is the length of an em in the units the points come out in; the baseline's left end is at (0, 0), y up.
// The points (fude_zoom_v2) are appended to _points, and each stroke's end (one past its last point, counted from the
// start of _points) to _ends. _seed chooses the unevenness: the same formula and seed are written the same way.
// *_missing, unless _missing is NULL, gets how many glyphs had no shape and were left out. Returns 0, adding nothing,
// when _c, _font or _box is NULL or _em is not above 0.
u32  fude_zoom_handmath_write(const fude_zoom_handmath_chars* _c, const fude_zoom_mfont* _font, const fude_zoom_tex_box* _box, f64 _em, u32 _seed,
                              rde_arr* _points, rde_arr* _ends, u32* _missing);

#endif
