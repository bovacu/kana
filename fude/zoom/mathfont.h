// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_MATHFONT
#define FUDE_ZOOM_MATHFONT

#include "rde.h"

// ===========================================================================
// A MATHS FONT read for the typesetter (tex.h): an OpenType font with
// TrueType outlines and a MATH table (Noto Sans Math: assets/fonts). Read here
// rather than through the engine's text, so that its glyphs are outlines to
// draw at any size — a formula as sharp ×10⁶ in as at ×1 — and its maths
// tables (the gaps and shifts a formula is laid out by, each glyph's italic
// correction and accent's place, the bigger forms of a bracket or a root and
// the pieces a taller one is built of) are there to lay it out by.
//
// Font units throughout (units_per_em of them an em). The file's bytes are the
// caller's, kept while the font is used.
// ===========================================================================

typedef struct {
    const u8* data;
    u32       size;
    u16       units_per_em;
    u32       glyphs;
    i16       ascender, descender;          // hhea's
    u32       cmap4, cmap12;                // the character maps' subtables (0: none)
    u32       loca, glyf, hmtx;
    u16       hmetrics;
    b8        long_loca;
    u32       math;                         // the MATH table (0: none)
    u32       constants, glyph_info, variants;
} fude_zoom_mfont;

// MathConstants, in the table's order (OpenType): the first four plain numbers, the rest font units.
typedef enum {
    FUDE_ZOOM_MATH_SCRIPT_PERCENT = 0, FUDE_ZOOM_MATH_SCRIPTSCRIPT_PERCENT, FUDE_ZOOM_MATH_DELIMITED_SUB_FORMULA_MIN_HEIGHT,
    FUDE_ZOOM_MATH_DISPLAY_OPERATOR_MIN_HEIGHT,
    FUDE_ZOOM_MATH_LEADING, FUDE_ZOOM_MATH_AXIS_HEIGHT, FUDE_ZOOM_MATH_ACCENT_BASE_HEIGHT, FUDE_ZOOM_MATH_FLATTENED_ACCENT_BASE_HEIGHT,
    FUDE_ZOOM_MATH_SUBSCRIPT_SHIFT_DOWN, FUDE_ZOOM_MATH_SUBSCRIPT_TOP_MAX, FUDE_ZOOM_MATH_SUBSCRIPT_BASELINE_DROP_MIN,
    FUDE_ZOOM_MATH_SUPERSCRIPT_SHIFT_UP, FUDE_ZOOM_MATH_SUPERSCRIPT_SHIFT_UP_CRAMPED, FUDE_ZOOM_MATH_SUPERSCRIPT_BOTTOM_MIN,
    FUDE_ZOOM_MATH_SUPERSCRIPT_BASELINE_DROP_MAX, FUDE_ZOOM_MATH_SUB_SUPERSCRIPT_GAP_MIN, FUDE_ZOOM_MATH_SUPERSCRIPT_BOTTOM_MAX_WITH_SUBSCRIPT,
    FUDE_ZOOM_MATH_SPACE_AFTER_SCRIPT, FUDE_ZOOM_MATH_UPPER_LIMIT_GAP_MIN, FUDE_ZOOM_MATH_UPPER_LIMIT_BASELINE_RISE_MIN,
    FUDE_ZOOM_MATH_LOWER_LIMIT_GAP_MIN, FUDE_ZOOM_MATH_LOWER_LIMIT_BASELINE_DROP_MIN, FUDE_ZOOM_MATH_STACK_TOP_SHIFT_UP,
    FUDE_ZOOM_MATH_STACK_TOP_DISPLAY_STYLE_SHIFT_UP, FUDE_ZOOM_MATH_STACK_BOTTOM_SHIFT_DOWN, FUDE_ZOOM_MATH_STACK_BOTTOM_DISPLAY_STYLE_SHIFT_DOWN,
    FUDE_ZOOM_MATH_STACK_GAP_MIN, FUDE_ZOOM_MATH_STACK_DISPLAY_STYLE_GAP_MIN, FUDE_ZOOM_MATH_STRETCH_STACK_TOP_SHIFT_UP,
    FUDE_ZOOM_MATH_STRETCH_STACK_BOTTOM_SHIFT_DOWN, FUDE_ZOOM_MATH_STRETCH_STACK_GAP_ABOVE_MIN, FUDE_ZOOM_MATH_STRETCH_STACK_GAP_BELOW_MIN,
    FUDE_ZOOM_MATH_FRACTION_NUMERATOR_SHIFT_UP, FUDE_ZOOM_MATH_FRACTION_NUMERATOR_DISPLAY_STYLE_SHIFT_UP,
    FUDE_ZOOM_MATH_FRACTION_DENOMINATOR_SHIFT_DOWN, FUDE_ZOOM_MATH_FRACTION_DENOMINATOR_DISPLAY_STYLE_SHIFT_DOWN,
    FUDE_ZOOM_MATH_FRACTION_NUMERATOR_GAP_MIN, FUDE_ZOOM_MATH_FRACTION_NUM_DISPLAY_STYLE_GAP_MIN, FUDE_ZOOM_MATH_FRACTION_RULE_THICKNESS,
    FUDE_ZOOM_MATH_FRACTION_DENOMINATOR_GAP_MIN, FUDE_ZOOM_MATH_FRACTION_DENOM_DISPLAY_STYLE_GAP_MIN,
    FUDE_ZOOM_MATH_SKEWED_FRACTION_HORIZONTAL_GAP, FUDE_ZOOM_MATH_SKEWED_FRACTION_VERTICAL_GAP, FUDE_ZOOM_MATH_OVERBAR_VERTICAL_GAP,
    FUDE_ZOOM_MATH_OVERBAR_RULE_THICKNESS, FUDE_ZOOM_MATH_OVERBAR_EXTRA_ASCENDER, FUDE_ZOOM_MATH_UNDERBAR_VERTICAL_GAP,
    FUDE_ZOOM_MATH_UNDERBAR_RULE_THICKNESS, FUDE_ZOOM_MATH_UNDERBAR_EXTRA_DESCENDER, FUDE_ZOOM_MATH_RADICAL_VERTICAL_GAP,
    FUDE_ZOOM_MATH_RADICAL_DISPLAY_STYLE_VERTICAL_GAP, FUDE_ZOOM_MATH_RADICAL_RULE_THICKNESS, FUDE_ZOOM_MATH_RADICAL_EXTRA_ASCENDER,
    FUDE_ZOOM_MATH_RADICAL_KERN_BEFORE_DEGREE, FUDE_ZOOM_MATH_RADICAL_KERN_AFTER_DEGREE, FUDE_ZOOM_MATH_RADICAL_DEGREE_BOTTOM_RAISE_PERCENT,
    FUDE_ZOOM_MATH_CONSTANTS
} FUDE_ZOOM_MATH_;

// A piece of a glyph built taller or wider than its biggest form (a long bracket: its top, an extender, its middle...).
typedef struct {
    u32 glyph;
    i32 start, end;      // how far it may overlap the piece before and after (font units)
    i32 advance;         // its length along the way it grows
    b8  extender;        // repeated as often as it takes
} fude_zoom_mfont_part;

// The font in _data (_size bytes). False: not a TrueType font this reads (then nothing of it is used).
b8  fude_zoom_mfont_load(fude_zoom_mfont* _f, const u8* _data, u32 _size);

// The glyph for _codepoint (0: none — the font's missing glyph).
u32 fude_zoom_mfont_glyph(const fude_zoom_mfont* _f, u32 _codepoint);
// Glyph _g's advance; its box (x min, y min, x max, y max; false: none — a space).
i32 fude_zoom_mfont_advance(const fude_zoom_mfont* _f, u32 _g);
b8  fude_zoom_mfont_box(const fude_zoom_mfont* _f, u32 _g, i32 _box[4]);
// Glyph _g's outline as closed polygons, its curves cut into straight pieces no further than _tolerance from them:
// points appended to _points (f64 x, y pairs: rde_arr of f64), each contour's end (one past its last point) to _ends
// (rde_arr of u32, counted from _points' start as it was). How many contours.
u32 fude_zoom_mfont_outline(const fude_zoom_mfont* _f, u32 _g, f64 _tolerance, rde_arr* _points, rde_arr* _ends);

// A MathConstants value (FUDE_ZOOM_MATH_); its italic correction; where an accent over it sits (its middle without one).
i32 fude_zoom_mfont_constant(const fude_zoom_mfont* _f, u32 _which);
i32 fude_zoom_mfont_italic(const fude_zoom_mfont* _f, u32 _g);
i32 fude_zoom_mfont_accent_at(const fude_zoom_mfont* _f, u32 _g);
// Glyph _g's bigger forms up (_vertical) or across, smallest first, each with its size: how many (into at most _most).
u32 fude_zoom_mfont_variants(const fude_zoom_mfont* _f, u32 _g, b8 _vertical, u32* _glyphs, i32* _sizes, u32 _most);
// ...and the pieces one bigger than all of them is built of (bottom or left first): how many (0: none).
u32 fude_zoom_mfont_assembly(const fude_zoom_mfont* _f, u32 _g, b8 _vertical, fude_zoom_mfont_part* _parts, u32 _most);
// The least two pieces of an assembly overlap (font units).
i32 fude_zoom_mfont_min_overlap(const fude_zoom_mfont* _f);

#endif
