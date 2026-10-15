// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SCORE
#define FUDE_SCORE

#include "rde.h"
#include "study/chars/kanji.h"
#include "drawing/ink/ink.h"

// ===========================================================================
// Score: scores a written character against its reference. It is milestone 3, "the product" in docs/design.md: it
// says not only HOW WELL the character was written, but WHAT was wrong, stroke by stroke.
//
// Both drawings are prepared as for the draw-search (match.h): fitted into the same box, so that size and position
// do not count, and resampled evenly along each stroke. Then the drawn strokes are PAIRED with the reference ones by
// the cheapest assignment (the Hungarian algorithm), whatever order they were written in; a stroke may pair with
// nothing, which makes a missing or an extra stroke. From the pairing come:
//
//   count      the strokes drawn against the strokes expected
//   order      the reference strokes the drawing took out of sequence
//   direction  the pairs that match better walked backwards
//   shape      the mean distance between paired strokes, once the drawing is fitted onto the reference, which gives
//              the base score
//
// The score is the shape score minus a penalty per mistake. EVERY WEIGHT HERE IS A GUESS until it is tuned against
// real handwriting (design.md: "thresholds must be tuned against real attempts"); they are the constants below, with
// the tuning done so far.
//
// Where it sits: the study layer's handwriting (study/handwriting). Practice, Check, the exams and the word exam
// score with it; the practice history (history.h) describes a stored score again with fude_score_describe; the
// demo (study.h: fude_study_demo) scores its made-up writing with it.
// Its main types and their lifecycle: fude_score is a plain value, returned by value.
// Threads: call it on the app's main thread only: the scoring uses static scratch, and the feedback is written in
// the app's language (text.h).
// Files: none.
// ===========================================================================

#define FUDE_SCORE_SHAPE_PERFECT  3.0f    // the mean distance (in a box of 100) that still scores 100
#define FUDE_SCORE_SHAPE_ZERO     18.0f   // ...and the one that scores 0
#define FUDE_SCORE_REVERSE_MARGIN 3.0f    // backwards must beat forwards by this to count as reversed
#define FUDE_SCORE_GAP            30.0f   // the pairing cost of having no partner
#define FUDE_SCORE_FIT_STRETCH    1.25f   // the best fit: how much one axis may stretch against the other
#define FUDE_SCORE_FIT_SIZE       1.35f   // ...and how much it may change the size, either way

// A missing or extra stroke usually makes ANOTHER character (日 plus a stroke is 目), so it has to fail clearly. Order
// and direction are what the app is for ("the pedagogically valuable ones", design.md): they must outweigh a small
// wobble of shape, and bar an "Excellent" on their own.
// Tuned on 2026-09-30 on real attempts (5 characters, 41 squares), once the shape was judged after a best fit.
// Stroke errors are counted here only (the fit no longer lets an extra stroke drag the shape down too), so a missing
// or extra stroke takes perfect writing down to the edge between fair and poor (another character written must not
// score above 60), and an order or direction mistake, the point of the app, keeps a perfect shape out of good (under
// FUDE_THEME_GRADE_GOOD, 80: 100 - 25 is 75) and says which stroke it was. "Good" is the word for a score from that
// same bound, and "Excellent" from FUDE_SCORE_EXCELLENT, so the feedback and the score's colour (theme.h) agree.
#define FUDE_SCORE_MISSING        40.0f   // points off per missing stroke
#define FUDE_SCORE_EXTRA          40.0f   // ...per extra stroke
#define FUDE_SCORE_MISPLACED      25.0f   // ...per stroke out of order
#define FUDE_SCORE_REVERSED       25.0f   // ...per stroke drawn backwards
#define FUDE_SCORE_EXCELLENT      90.0f   // the feedback says "Excellent" from this score ("Good" from 80, theme.h)

RDE_STRUCT {
    b8  empty;          // nothing was drawn, so it was not scored
    f32 score;          // 0..100
    f32 shape;          // 0..100: the shape part alone
    u8  drawn;          // the strokes drawn
    u8  expected;       // the strokes the character has
    u8  missing;        // the reference strokes that nothing was paired with
    u8  extra;          // the drawn strokes that were paired with nothing
    u8  misplaced;      // the strokes out of order
    u8  reversed;       // the strokes drawn backwards
    u8  first_reversed; // a backwards stroke, as its reference number (1-based); 0 = none
    // An order mistake, where the order first goes wrong: reference stroke swap_a should have been written before
    // reference stroke swap_b, and was written after it; 0 = none. It is a swap of two strokes when only those two
    // are out of place, but not always: written 1 3 4 2, it is "2 before 4" (stroke 2 came last), and not a swap.
    u8  swap_a;
    u8  swap_b;
    u8  worst;          // the reference stroke whose shape is furthest off (1-based); 0 = none
    c8  feedback[384];  // one line for the learner, the most important thing first
} fude_score;

// Scores the alive strokes of _drawing (any units, Y up) against a character. With nothing drawn the score is marked
// empty; when the character has no reference strokes it scores 0. The feedback line is filled in either way.
fude_score fude_score_drawing(const fude_kanji_db* _db, const fude_kanji_info* _info, const fude_ink* _drawing);

// Writes _s->feedback from its other fields, in the app's current language, so that a score read back from the
// practice history (which does not store the line) says the same thing again.
void       fude_score_describe(fude_score* _s);

#endif
