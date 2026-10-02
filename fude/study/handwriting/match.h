#ifndef FUDE_MATCH
#define FUDE_MATCH

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/catalog.h"
#include "drawing/ink/ink.h"

// ===========================================================================
// Draw-to-search: which characters does a hand-drawn one look like?
//
// Not recognition by a model — template matching against the baked strokes,
// the same comparison milestone 3's scoring is built on:
//
//   1. Both drawings are fitted into the same box (position and size stop
//      mattering) and every stroke is resampled to FUDE_MATCH_POINTS points
//      evenly along its length (speed and sampling rate stop mattering).
//   2. Two strokes differ by the mean distance between their corresponding
//      points. Drawn backwards also counts, at a penalty: a lookup should find
//      the character even when the direction was wrong.
//   3. Two characters differ by the cheapest IN-ORDER alignment of their
//      strokes, where a stroke missing or extra costs FUDE_MATCH_GAP — and, for
//      equal stroke counts, by an order-free pairing at a penalty, so strokes
//      written in the wrong order still find the character.
//
// Only characters within FUDE_MATCH_STROKE_SLACK strokes of the drawing, and
// passing the filter, are compared: first roughly (4 points per stroke), then
// the closest few dozen exactly. Each character's strokes are prepared the
// first time it is compared and kept (~10 MB for all of them).
// ===========================================================================

#define FUDE_MATCH_POINTS        16
#define FUDE_MATCH_MAX_STROKES   64
#define FUDE_MATCH_STROKE_SLACK  2       // compared: drawn strokes ± this
#define FUDE_MATCH_GAP           28.0f   // a stroke missing or extra (box units, the box is 100)
#define FUDE_MATCH_REVERSED      10.0f   // a stroke drawn backwards
#define FUDE_MATCH_UNORDERED     4.0f    // right strokes, wrong order

// A filter for reading free writing (segment.h): the catalog's characters and
// the marks written in text that it leaves out — 、。ー々・！？ and the digits.
#define FUDE_MATCH_TEXT ((FUDE_FILTER_)FUDE_FILTER_COUNT)

RDE_STRUCT {
    u32 record;
    f32 cost;     // lower is closer; roughly the mean point distance, in a 100-unit box
} fude_match_result;

// A stroke prepared for comparing: resampled, fitted into the 100-unit box.
RDE_STRUCT {
    rde_vec_2F p[FUDE_MATCH_POINTS];
} fude_match_stroke;

// The shared preparation (scoring uses it too). Both return how many strokes.
// A drawing: its alive strokes, Y flipped to KanjiVG's Y-down.
u32 fude_match_drawing(const fude_ink* _drawing, fude_match_stroke* _out, u32 _max);
// A reference character.
u32 fude_match_reference(const fude_kanji_db* _db, const fude_kanji_info* _info, fude_match_stroke* _out, u32 _max);
// Mean distance between corresponding points; _reversed walks _b backwards.
f32 fude_match_points_distance(const fude_match_stroke* _a, const fude_match_stroke* _b, b8 _reversed);
// The two steps of the preparation, for strokes gathered some other way: one
// stroke's points (any units, Y down) resampled; then strokes fitted, together,
// into the box (in place).
void fude_match_resample_points(const rde_vec_2F* _points, u32 _count, fude_match_stroke* _out);
void fude_match_fit(fude_match_stroke* _strokes, u32 _count);

// Ranks the characters passing _filter against the alive strokes of _drawing
// (any units, Y up). Writes the best _max into _out, best first; returns how
// many. Nothing when the drawing is empty.
u32 fude_match_rank(const fude_kanji_db* _db, const fude_catalog* _catalog, FUDE_FILTER_ _filter,
                    const fude_ink* _drawing, fude_match_result* _out, u32 _max);
// The same, for strokes already prepared (resampled and fitted together).
u32 fude_match_rank_strokes(const fude_kanji_db* _db, const fude_catalog* _catalog, FUDE_FILTER_ _filter,
                            const fude_match_stroke* _strokes, u32 _count, fude_match_result* _out, u32 _max);
// Prepared strokes against one character, as a ranking would cost them; a huge
// cost when the character has no strokes.
f32 fude_match_cost_record(const fude_kanji_db* _db, u32 _record, const fude_match_stroke* _strokes, u32 _count);
// A character's extent in KanjiVG's box (Y down); false when it has no strokes.
b8  fude_match_extent(const fude_kanji_db* _db, u32 _record, rde_vec_2F* _min, rde_vec_2F* _max);
// Lets the prepared characters go (they are prepared again when next needed).
void fude_match_release(void);


#endif
