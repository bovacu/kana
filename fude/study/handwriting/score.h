#ifndef FUDE_SCORE
#define FUDE_SCORE

#include "rde.h"
#include "study/chars/kanji.h"
#include "drawing/ink/ink.h"

// ===========================================================================
// Scoring a written character against its reference — milestone 3, "the product"
// in docs/design.md: not only HOW WELL, but WHAT was wrong, stroke by stroke.
//
// Both are prepared as for the draw-search (match.h): fitted into the same box,
// so size and position don't count, and resampled evenly along each stroke.
// Then the drawn strokes are PAIRED with the reference ones by the cheapest
// assignment (Hungarian), whatever order they were written in — a stroke may
// pair with nothing, which is a missing or an extra stroke. From the pairing:
//
//   count      drawn vs expected strokes
//   order      reference strokes the drawing took out of sequence
//   direction  pairs that match better walked backwards
//   shape      mean distance between paired strokes → the base score
//
// The score is the shape score minus a penalty per mistake. EVERY WEIGHT HERE IS
// A GUESS until it is tuned against real handwriting (design.md: "thresholds must
// be tuned against real attempts"); they are the constants below.
// ===========================================================================

#define FUDE_SCORE_SHAPE_PERFECT  3.0f    // mean distance (box of 100) still scoring 100
#define FUDE_SCORE_SHAPE_ZERO     18.0f   // ...and scoring 0
#define FUDE_SCORE_REVERSE_MARGIN 3.0f    // backwards must beat forwards by this to count
#define FUDE_SCORE_GAP            30.0f   // pairing cost of "no partner"
#define FUDE_SCORE_FIT_STRETCH    1.25f   // the best fit: how much one axis may stretch against the other
#define FUDE_SCORE_FIT_SIZE       1.35f   // ...and the size change it may make, either way

// A missing or extra stroke usually makes ANOTHER character (日 + a stroke is
// 目): it has to fail clearly. Order and direction are what the app is for
// ("the pedagogically valuable ones", design.md): they must outweigh a small
// wobble of shape, and bar an "Excellent" on their own.
// Tuned 2026-09-30 on real attempts (5 characters, 41 squares) once shape was
// judged after a best fit: stroke errors are counted here only — the fit no
// longer lets an extra stroke drag the shape down too — so a missing or extra
// stroke takes perfect writing to the fair/poor edge (another character written
// must not score above 60), and order or direction, the point of the app, keeps
// a perfect shape out of "good" (80) and says which stroke.
#define FUDE_SCORE_MISSING        40.0f   // points off per missing stroke
#define FUDE_SCORE_EXTRA          40.0f   // ...per extra stroke
#define FUDE_SCORE_MISPLACED      25.0f   // ...per stroke out of order
#define FUDE_SCORE_REVERSED       25.0f   // ...per stroke drawn backwards

RDE_STRUCT {
    b8  empty;          // nothing was drawn: not scored
    f32 score;          // 0..100
    f32 shape;          // 0..100: the shape part alone
    u8  drawn;          // strokes drawn
    u8  expected;       // strokes the character has
    u8  missing;        // reference strokes nothing was paired with
    u8  extra;          // drawn strokes paired with nothing
    u8  misplaced;      // strokes out of order
    u8  reversed;       // strokes drawn backwards
    u8  first_reversed; // a backwards stroke, as its reference number (1-based); 0 = none
    u8  swap_a;         // an order mistake: reference strokes swap_a and swap_b; 0 = none
    u8  swap_b;
    u8  worst;          // the reference stroke whose shape is furthest off (1-based); 0 = none
    c8  feedback[96];   // one line for the learner
} fude_score;

// Scores the alive strokes of _drawing (any units, Y up) against a character.
fude_score fude_score_drawing(const fude_kanji_db* _db, const fude_kanji_info* _info, const fude_ink* _drawing);

// Writes _s->feedback from its other fields — so a score read back from the
// practice history (which does not store the line) says the same thing again.
void       fude_score_describe(fude_score* _s);

#endif
