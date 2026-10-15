// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_MATHFONT
#define FUDE_ZOOM_MATHFONT

#include "rde.h"

// ===========================================================================
// Mathfont: an OpenType maths font read for the typesetter (tex.h): an OpenType font with TrueType outlines and a MATH
// table, Noto Sans Math (assets/fonts/NotoSansMath-Regular.ttf). It is also the one reader of OpenType tables for the
// rest of the canvas: the PDF writer (pdf.c) reads the fonts it embeds through it, TrueType ones and CFF ones alike
// (their tables, a glyph's bytes and parts, a CFF font's CIDs).
//
// Where it sits: zoom, the canvas apps' layer. tex.c lays formulas out with it, formula.c loads it and draws its
// outlines, handmath.c reads its character map back, and pdf.c embeds the app's fonts with it.
// Its main types and their lifecycle: fude_zoom_mfont is a set of offsets into the font file's bytes. The caller owns
// the bytes and must keep them alive, unchanged, for as long as the font is used; there is nothing to free. In the
// apps, the page's fude_zoom_formulas (formula.h) owns both.
// Threads: called on the main thread. It never writes to the font, so reading one font from several threads at once
//   would be safe.
// Files: reads a .ttf or .otf in memory. The tables are read as the OpenType specification lays them out (and a CFF
//   table as Adobe's Compact Font Format note #5176 does), big-endian, each read checked against the file's end, so a
//   damaged font reads as zeros rather than past its bytes.
// Design:
//   The font is read here rather than through the engine's text, so that its glyphs are outlines that can be drawn at
//   any size: a formula is as sharp at ×10⁶ as at ×1. Its maths tables are what a formula is laid out by: the gaps and
//   shifts, each glyph's italic correction and where an accent over it sits, the bigger forms of a bracket or a root,
//   and the pieces a taller one is built of.
//   Every value is in font units; units_per_em of them make an em.
// ===========================================================================

// A loaded font: the caller's bytes, and where its tables are in them (byte offsets from the file's start).
typedef struct {
    const u8* data;
    // The file's length in bytes; 0 when loading failed, so that every other call then returns 0 or false.
    u32       size;
    u16       units_per_em;
    // How many glyphs the font has.
    u32       glyphs;
    i16       ascender, descender;          // from the hhea table
    u32       cmap4, cmap12;                // the Unicode character map subtables of format 4 and 12 (0: none)
    u32       loca, glyf, hmtx;             // (loca and glyf 0 in a font with CFF outlines)
    u32       cff;                          // the CFF table: a font with PostScript outlines (0: none, TrueType's)
    // How many glyphs have their own advance in hmtx; the glyphs after them share the last one's.
    u16       hmetrics;
    // Whether loca holds 32-bit offsets rather than 16-bit halved ones.
    b8        long_loca;
    u32       math;                         // the MATH table (0: none)
    // The MATH table's MathConstants, MathGlyphInfo and MathVariants subtables.
    u32       constants, glyph_info, variants;
} fude_zoom_mfont;

// The MathConstants values, in the table's order (OpenType). SCRIPT_PERCENT, SCRIPTSCRIPT_PERCENT and
// RADICAL_DEGREE_BOTTOM_RAISE_PERCENT are percentages; all the others are in font units. FUDE_ZOOM_MATH_CONSTANTS is
// how many there are.
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

// One piece of a glyph built taller or wider than its biggest form, such as a long bracket's top, middle or extender.
typedef struct {
    u32 glyph;
    i32 start, end;      // how far it may overlap the piece before and after it (font units)
    i32 advance;         // its length along the direction the glyph grows
    b8  extender;        // repeated as many times as it takes to reach the length
} fude_zoom_mfont_part;

// Reads the font in _data (_size bytes, kept by the caller) into _f. Returns false when it is not an OpenType font this
// can read (one missing a table it needs: head, hhea, maxp, cmap, hmtx, and glyf with loca or else CFF); _f must not
// be used then. A font with no MATH table loads, with math left 0. A font with CFF outlines loads with cff set and no
// glyf: its character map and advances are read, but fude_zoom_mfont_box, _outline and _glyph_data find no glyph in it.
b8  fude_zoom_mfont_load(fude_zoom_mfont* _f, const u8* _data, u32 _size);

// Returns the glyph for _codepoint, or 0 (the font's missing glyph) when it has none. The format 12 map is used when
// there is one, else the format 4 map, which only covers U+0000 to U+FFFF.
u32 fude_zoom_mfont_glyph(const fude_zoom_mfont* _f, u32 _codepoint);
// fude_zoom_mfont_advance returns glyph _g's advance width. fude_zoom_mfont_box fills _box with its ink box (x min,
// y min, x max, y max, y up) and returns false, with a zero box, when it has none, such as a space.
i32 fude_zoom_mfont_advance(const fude_zoom_mfont* _f, u32 _g);
b8  fude_zoom_mfont_box(const fude_zoom_mfont* _f, u32 _g, i32 _box[4]);
// Appends glyph _g's outline as closed polygons and returns how many contours it added. Curves are cut into straight
// pieces no further than _tolerance (font units) from them. The points go to _points as f64 x, y pairs (an rde_arr of
// f64), y up. For each contour, _ends (an rde_arr of u32) gets the index one past its last point, counting points (not
// f64s) from where _points ended before the call. A contour's closing point is not repeated.
u32 fude_zoom_mfont_outline(const fude_zoom_mfont* _f, u32 _g, f64 _tolerance, rde_arr* _points, rde_arr* _ends);

// fude_zoom_mfont_constant returns a MathConstants value (a FUDE_ZOOM_MATH_ value), or 0 when the font has no MATH
// table. fude_zoom_mfont_italic returns glyph _g's italic correction, 0 when it has none. fude_zoom_mfont_accent_at
// returns the x an accent over glyph _g is centred on; for a glyph the table does not list, that is the middle of its
// ink box, or half its advance when it has no ink.
i32 fude_zoom_mfont_constant(const fude_zoom_mfont* _f, u32 _which);
i32 fude_zoom_mfont_italic(const fude_zoom_mfont* _f, u32 _g);
i32 fude_zoom_mfont_accent_at(const fude_zoom_mfont* _f, u32 _g);
// Writes glyph _g's bigger forms, growing up (_vertical) or across, smallest first, into _glyphs, and each one's size
// along that direction into _sizes. Returns how many, at most _most; 0 when the font has none for it.
u32 fude_zoom_mfont_variants(const fude_zoom_mfont* _f, u32 _g, b8 _vertical, u32* _glyphs, i32* _sizes, u32 _most);
// Writes the pieces a form bigger than all of those is built of, bottom or left first, into _parts. Returns how many,
// at most _most; 0 when the glyph cannot be built bigger.
u32 fude_zoom_mfont_assembly(const fude_zoom_mfont* _f, u32 _g, b8 _vertical, fude_zoom_mfont_part* _parts, u32 _most);
// Returns how much two pieces of an assembly must overlap at least (font units); 0 when the font has no MATH table.
i32 fude_zoom_mfont_min_overlap(const fude_zoom_mfont* _f);

// --- any table, for embedding the font (pdf.c) ---------------------------------------------------------------------

// The big-endian u16 or u32 at byte _at of the font file; 0 when it runs past the file's end (or
// the font did not load).
u32 fude_zoom_mfont_u16(const fude_zoom_mfont* _f, u32 _at);
u32 fude_zoom_mfont_u32(const fude_zoom_mfont* _f, u32 _at);
// Returns where table _tag starts in the file (a byte offset; the tag's four characters big-endian, 'cvt ' is
// 0x63767420), its length in bytes into *_size (may be NULL). 0 when the font has no such table, or it runs past the
// file's end.
u32 fude_zoom_mfont_table(const fude_zoom_mfont* _f, u32 _tag, u32* _size);
// Finds glyph _g's bytes in glyf: *_at where they start in the file, *_len how many. Returns false for a glyph with
// none (a space), one out of range, one past the file's end, and every glyph of a CFF font.
b8  fude_zoom_mfont_glyph_data(const fude_zoom_mfont* _f, u32 _g, u32* _at, u32* _len);
// Writes the glyphs composite glyph _g is made of (its own parts, not theirs) into _glyphs, and returns how many, at
// most _most; 0 for a simple glyph or one with no bytes. A font subset must keep them for _g to draw.
u32 fude_zoom_mfont_parts(const fude_zoom_mfont* _f, u32 _g, u32* _glyphs, u32 _most);
// A CID-keyed CFF font (its Top DICT has ROS) names each glyph by a CID through its charset, and a PDF names its glyphs
// by those CIDs. Resizes _cids (an rde_arr of u16, made by the caller) to the font's glyph count and writes each
// glyph's CID into it (glyph 0's is 0; a glyph the charset leaves out gets 0). Returns false, _cids left alone, when
// the font has no CFF or its CFF is not CID-keyed: its glyphs are then named by their glyph ids.
b8  fude_zoom_mfont_cids(const fude_zoom_mfont* _f, rde_arr* _cids);

#endif
