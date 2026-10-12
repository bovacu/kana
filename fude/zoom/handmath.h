// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_HANDMATH
#define FUDE_ZOOM_HANDMATH

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/tex.h"

// ===========================================================================
// FORMULAS BY HAND (docs/product_split.md §4.3, LaTeX → handwriting): a set
// formula (tex.h) written out as pen strokes. Each of its glyphs is the
// Hershey maths and Greek sets' shape of its character (hand_maths.h: digits,
// letters, Greek, + − × ÷ ± = ≠ ≤ ≥ < > ∑ ∏ ∫ √ ∞ ∂ ∇ → ∈ ⊂ … — 177 of them),
// fitted into the glyph's own ink box — so it is where the set formula has it
// and as big —, its strokes smoothed and a little uneven, as a hand's are. Its
// rules (a fraction's bar, a root's) are lines. A tall bracket or root the font
// builds of pieces is one shape over all of them.
//
// The shapes are also what the handwritten maths reader (mathread.h) knows
// symbols by.
// ===========================================================================

// --- the shapes ------------------------------------------------------------------------------------

// The characters it has shapes for (in order): how many; *_cps at them.
u32 fude_zoom_handmath_symbols(const u32** _cps);
// _cp's shape: its strokes' points (fude_zoom_v2: Hershey units — a capital 21 tall —, y up, its baseline at y = 0)
// appended to _points, each stroke's end (past its last point, counted in _points) to _ends. How many strokes (0: no
// shape for it).
u32 fude_zoom_handmath_shape(u32 _cp, rde_arr* _points, rde_arr* _ends);
// A character as its shapes know it: a maths alphabet's (𝑥 𝐀 𝔄 𝟐 …) its plain letter or digit, ℎ h, the rest as they are.
u32 fude_zoom_handmath_base(u32 _cp);

// --- a font's glyphs' characters -------------------------------------------------------------------------

// Which character each of a font's glyphs is: its own (the font's cmap, read back), or the one it is a bigger form or a
// piece of (a tall bracket's).
typedef struct {
    rde_hash_map chars;   // glyph (u64) → character (u32)
    const void*  font;    // the font it was read from (NULL: none yet)
} fude_zoom_handmath_chars;

void fude_zoom_handmath_chars_init(fude_zoom_handmath_chars* _c);
void fude_zoom_handmath_chars_free(fude_zoom_handmath_chars* _c);
// Read from _font (again only when it is another font).
void fude_zoom_handmath_chars_read(fude_zoom_handmath_chars* _c, const fude_zoom_mfont* _font);
// Glyph _g's character (as fude_zoom_handmath_base gives it); 0: none known.
u32  fude_zoom_handmath_char(const fude_zoom_handmath_chars* _c, u32 _g);

// --- written ----------------------------------------------------------------------------------------

// _box (set with _font) written out by hand, its em _em long (the units the points come in), its baseline's left end at
// (0, 0), y up: its strokes' points (fude_zoom_v2) appended to _points, their ends to _ends. Seeded: the same formula and
// seed write alike. How many strokes; *_missing (NULL: not wanted) how many of its glyphs had no shape (left out).
u32  fude_zoom_handmath_write(const fude_zoom_handmath_chars* _c, const fude_zoom_mfont* _font, const fude_zoom_tex_box* _box, f64 _em, u32 _seed,
                              rde_arr* _points, rde_arr* _ends, u32* _missing);

#endif
