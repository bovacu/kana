// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_MATCH
#define FUDE_MATCH

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/catalog.h"
#include "drawing/ink/ink.h"

// ===========================================================================
// Match: draw-to-search. Which characters does a hand-drawn one look like?
//
// It is not recognition by a model but template matching against the baked strokes, the same comparison that
// milestone 3's scoring (score.h) is built on:
//
//   1. Both drawings are fitted into the same box, so that position and size stop mattering, and every stroke is
//      resampled to FUDE_MATCH_POINTS points evenly along its length, so that speed and sampling rate stop mattering.
//   2. Two strokes differ by the mean distance between their corresponding points. A stroke drawn backwards also
//      counts, at a penalty: a lookup should find the character even when the direction was wrong.
//   3. Two characters differ by the cheapest IN-ORDER alignment of their strokes, where a missing or extra stroke
//      costs FUDE_MATCH_GAP; and, when the stroke counts are equal, by an order-free pairing at a penalty, so that
//      strokes written in the wrong order still find the character.
//
// Only characters within FUDE_MATCH_STROKE_SLACK strokes of the drawing, and passing the filter, are compared:
// first roughly (4 points per stroke), then the closest few dozen exactly.
//
// Where it sits: the study layer's handwriting (study/handwriting). Browse's draw-search, Check, the exams and the
// word exam rank with it; the scorer (score.h) and the reading of free writing (segment.h) share its preparation;
// the bake (bake.h) finds look-alikes with it.
// Its main types and their lifecycle: fude_match_result and fude_match_stroke are values the caller keeps. Each
// character's strokes are prepared the first time it is compared and kept in this module's statics (~80,000
// strokes, ~10 MB for all of them, ~80 ms to prepare in a debug build, which every ranking used to pay), with the
// records sorted by stroke count, for the database they were made from; fude_match_release lets them go, and the
// shell calls it at the end.
// Threads: call it on the app's main thread only: the prepared strokes and the scratch are statics.
// Files: none; it reads the loaded character data.
// ===========================================================================

#define FUDE_MATCH_POINTS        16
#define FUDE_MATCH_MAX_STROKES   64
#define FUDE_MATCH_STROKE_SLACK  2       // compared: the drawn strokes, plus or minus this
#define FUDE_MATCH_GAP           28.0f   // a stroke missing or extra (box units; the box is 100)
#define FUDE_MATCH_REVERSED      10.0f   // a stroke drawn backwards
#define FUDE_MATCH_UNORDERED     4.0f    // the right strokes, in the wrong order

// A filter for reading free writing (segment.h): the catalog's characters, plus the marks written in text that the
// catalog leaves out (、。々・ー！？, ASCII's ! and ?, and the digits), where the character data has them.
#define FUDE_MATCH_TEXT ((FUDE_FILTER_)0xFFFFu)   // beyond any filter (catalog.h)

RDE_STRUCT {
    u32 record;
    f32 cost;     // lower is closer; roughly the mean point distance, in a 100-unit box
} fude_match_result;

// A stroke prepared for comparing: resampled, and fitted into the 100-unit box.
RDE_STRUCT {
    rde_vec_2F p[FUDE_MATCH_POINTS];
} fude_match_stroke;

// The shared preparation (the scorer uses it too). Both return how many strokes they prepared into _out (at most
// _max). fude_match_drawing takes a drawing's alive strokes, with Y flipped to KanjiVG's Y down.
u32 fude_match_drawing(const fude_ink* _drawing, fude_match_stroke* _out, u32 _max);
// fude_match_reference takes a reference character's strokes; it returns 0 when one of them cannot be read.
u32 fude_match_reference(const fude_kanji_db* _db, const fude_kanji_info* _info, fude_match_stroke* _out, u32 _max);
// Returns the mean distance between two strokes' corresponding points; _reversed walks _b backwards.
f32 fude_match_points_distance(const fude_match_stroke* _a, const fude_match_stroke* _b, b8 _reversed);
// The two steps of the preparation, for strokes gathered some other way: one stroke's points (any units, Y down)
// resampled into _out; then the strokes fitted together into the box, in place, keeping their proportions (a flat
// 一 stays flat).
void fude_match_resample_points(const rde_vec_2F* _points, u32 _count, fude_match_stroke* _out);
void fude_match_fit(fude_match_stroke* _strokes, u32 _count);

// Ranks the characters passing _filter against the alive strokes of _drawing (any units, Y up). Writes the best
// _max into _out, best first, and returns how many: 0 when the drawing is empty or there is no data.
u32 fude_match_rank(const fude_kanji_db* _db, const fude_catalog* _catalog, FUDE_FILTER_ _filter,
                    const fude_ink* _drawing, fude_match_result* _out, u32 _max);
// The same, for strokes already prepared (resampled and fitted together).
u32 fude_match_rank_strokes(const fude_kanji_db* _db, const fude_catalog* _catalog, FUDE_FILTER_ _filter,
                            const fude_match_stroke* _strokes, u32 _count, fude_match_result* _out, u32 _max);
// Returns what prepared strokes cost against one character, as a ranking would cost them; a huge cost (1e30) when
// the character has no strokes, or there are none or too many prepared ones.
f32 fude_match_cost_record(const fude_kanji_db* _db, u32 _record, const fude_match_stroke* _strokes, u32 _count);
// Gets a character's extent in KanjiVG's box (Y down). Returns false when it has no strokes.
b8  fude_match_extent(const fude_kanji_db* _db, u32 _record, rde_vec_2F* _min, rde_vec_2F* _max);
// Lets the prepared characters go; they are prepared again when next needed.
void fude_match_release(void);


#endif
