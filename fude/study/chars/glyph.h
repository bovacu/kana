#ifndef FUDE_GLYPH
#define FUDE_GLYPH

#include "rde.h"
#include "study/chars/kanji.h"

// ===========================================================================
// Drawing characters from their strokes (kanji.h): a whole character, part of
// one stroke (the viewer's animation), or a line of kana — Kana has no Japanese
// font, and KanjiVG has every kana, so Japanese text is drawn as handwriting.
//
// Positions are screen space (centre origin, Y up); KanjiVG's units are Y down,
// in a FUDE_KANJI_BOX-unit square whose top-left goes at _origin.
// ===========================================================================

#define FUDE_GLYPH_TOLERANCE  0.25f     // KanjiVG units: flattening precision (and the finest drawing uses)
// Drawing flattens to this many SCREEN units instead, when that is coarser: a
// 14-unit caption kana flattened at FUDE_GLYPH_TOLERANCE has points a fifth of a
// pixel apart, and every point costs the stroke tessellator a section of vertices.
#define FUDE_GLYPH_SCREEN_TOLERANCE 0.2f
#define FUDE_GLYPH_MAX_POINTS 1024u     // per stroke (the most any has is under 100)
#define FUDE_GLYPH_WIDTH      3.2f      // stroke width, KanjiVG units (its own drawings use 3)

// Timing of a character writing itself, seconds.
#define FUDE_GLYPH_STROKE_BASE 0.25     // every stroke takes at least this
#define FUDE_GLYPH_STROKE_PER  0.008    // ...plus this per KanjiVG unit of length
#define FUDE_GLYPH_STROKE_GAP  0.15     // pause between strokes

RDE_STRUCT {
    const fude_kanji_db*     db;
    rde_arr TYPE(rde_vec_2F) _points;
    rde_arr TYPE(f32)        _radii;
} fude_glyph;

void fude_glyph_init(fude_glyph* _glyph, const fude_kanji_db* _db);
void fude_glyph_destroy(fude_glyph* _glyph);

// A stroke as points (KanjiVG units) into _out (FUDE_GLYPH_MAX_POINTS); returns
// how many. _length (may be NULL) gets its length along the curve.
u32  fude_glyph_stroke_points(const fude_kanji_stroke* _stroke, rde_vec_2F* _out, f32* _length);

// Draws the first _fraction of a stroke's length, _scale screen units per
// KanjiVG unit, _radius wide. Returns where the drawn part ends, on screen.
rde_vec_2F fude_glyph_stroke(fude_glyph* _glyph, const fude_kanji_stroke* _stroke, rde_vec_2F _origin,
                             f32 _scale, f32 _radius, f32 _fraction, rde_color _color);

// A whole character, _size screen units square. False when the data has none.
b8   fude_glyph_character(fude_glyph* _glyph, u32 _codepoint, rde_vec_2F _origin, f32 _size, rde_color _color);

// A practice-sheet square: white, outlined, with dashed centre guides. _tl is
// its top-left corner.
void fude_glyph_box(rde_vec_2F _tl, f32 _size);

// A character WRITING ITSELF, _elapsed seconds in: faint ghosts of every stroke,
// then ink stroke by stroke with a red pen tip, and each stroke's number at its
// start (text _number_px tall; 0 for none). True once it has finished writing.
b8   fude_glyph_writing(fude_glyph* _glyph, const fude_kanji_info* _info, rde_vec_2F _tl, f32 _size, f64 _elapsed,
                        rde_font* _font, f32 _font_px, f32 _number_px);

// A reading as KANJIDIC2 writes it — kana, "、" between readings, '.' before
// okurigana (drawn in _soft), '-' for an affix. Stops before passing _max_x.
// Returns the x where it ended.
f32  fude_glyph_reading(fude_glyph* _glyph, const c8* _text, rde_vec_2F _origin, f32 _size, f32 _max_x, rde_color _ink, rde_color _soft);

#endif
