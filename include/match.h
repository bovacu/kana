#ifndef KANA_MATCH
#define KANA_MATCH

#include "rde.h"
#include "kanji.h"
#include "catalog.h"
#include "ink.h"

// ===========================================================================
// Draw-to-search: which characters does a hand-drawn one look like?
//
// Not recognition by a model — template matching against the baked strokes,
// the same comparison milestone 3's scoring is built on:
//
//   1. Both drawings are fitted into the same box (position and size stop
//      mattering) and every stroke is resampled to KANA_MATCH_POINTS points
//      evenly along its length (speed and sampling rate stop mattering).
//   2. Two strokes differ by the mean distance between their corresponding
//      points. Drawn backwards also counts, at a penalty: a lookup should find
//      the character even when the direction was wrong.
//   3. Two characters differ by the cheapest IN-ORDER alignment of their
//      strokes, where a stroke missing or extra costs KANA_MATCH_GAP — and, for
//      equal stroke counts, by an order-free pairing at a penalty, so strokes
//      written in the wrong order still find the character.
//
// Only characters within KANA_MATCH_STROKE_SLACK strokes of the drawing, and
// passing the filter, are compared.
// ===========================================================================

#define KANA_MATCH_POINTS        16
#define KANA_MATCH_MAX_STROKES   64
#define KANA_MATCH_STROKE_SLACK  2       // compared: drawn strokes ± this
#define KANA_MATCH_GAP           28.0f   // a stroke missing or extra (box units, the box is 100)
#define KANA_MATCH_REVERSED      10.0f   // a stroke drawn backwards
#define KANA_MATCH_UNORDERED     4.0f    // right strokes, wrong order

RDE_STRUCT {
    u32 record;
    f32 cost;     // lower is closer; roughly the mean point distance, in a 100-unit box
} kana_match_result;

// A stroke prepared for comparing: resampled, fitted into the 100-unit box.
RDE_STRUCT {
    rde_vec_2F p[KANA_MATCH_POINTS];
} kana_match_stroke;

// The shared preparation (scoring uses it too). Both return how many strokes.
// A drawing: its alive strokes, Y flipped to KanjiVG's Y-down.
u32 kana_match_drawing(const kana_ink* _drawing, kana_match_stroke* _out, u32 _max);
// A reference character.
u32 kana_match_reference(const kana_kanji_db* _db, const kana_kanji_info* _info, kana_match_stroke* _out, u32 _max);
// Mean distance between corresponding points; _reversed walks _b backwards.
f32 kana_match_points_distance(const kana_match_stroke* _a, const kana_match_stroke* _b, b8 _reversed);

// Ranks the characters passing _filter against the alive strokes of _drawing
// (any units, Y up). Writes the best _max into _out, best first; returns how
// many. Nothing when the drawing is empty.
u32 kana_match_rank(const kana_kanji_db* _db, const kana_catalog* _catalog, KANA_FILTER_ _filter,
                    const kana_ink* _drawing, kana_match_result* _out, u32 _max);

#endif
