#ifndef FUDE_HISTORY
#define FUDE_HISTORY

#include "rde.h"
#include "drawing/ink/ink.h"
#include "study/handwriting/score.h"

// ===========================================================================
// Practice history: every scored practice session, kept for progress — the
// "album" to come — and never overwritten, only added to.
//
// ONE FILE PER CHARACTER: <save dir>/practice/<code point in hex>.kana, the KANA
// format (kfile.h), kind 'PRAC', one 'SESS' chunk per session, oldest first. A
// session is appended by rewriting that one small file atomically.
//
//   'SESS'  u64 time (Unix seconds, UTC), u32 code point, f32 average score
//           (over the squares that were scored), u32 square count, u32 square
//           record size, then per square:
//             the record: u8 scored (0: left empty), f32 score, f32 shape, u8
//             drawn, expected, missing, extra, misplaced, reversed,
//             first_reversed, swap_a, swap_b, worst   (score.h)
//             then its drawing: u32 stroke count; per stroke u32 point count,
//             then per point u16 x, u16 y — a fraction of the square's side
//             times 65535, Y up — and f32 time (seconds since the stroke began)
//
// The drawing is stored RELATIVE TO ITS SQUARE, so it can be redrawn at any
// size, and with its timing, so it can be replayed stroke by stroke.
// ===========================================================================

#define FUDE_HISTORY_VERSION 1u

RDE_STRUCT {
    u32 sessions;
    f32 first;        // the first session's average
    f32 last;         // the latest session's average
    f32 best;         // the best session average
    u64 last_time;    // when the latest was (Unix seconds)
} fude_history_summary;

// The folder, created if missing, ending in '/'.
const c8* fude_history_dir(void);

// Appends one session. _drawings[i] and _scores[i] per square; a square whose
// score is .empty is stored as unscored. _square_size: the squares' side in the
// drawings' units.
b8 fude_history_save(u32 _codepoint, u64 _time, u32 _squares, const fude_score* _scores,
                     const fude_ink* const* _drawings, f32 _square_size);

// Reads a character's sessions down to a summary. False when it has none.
b8 fude_history_summarize(u32 _codepoint, fude_history_summary* _out);

// Goes up by one on every save: whoever shows history re-reads it when this moved.
u32 fude_history_revision(void);

// Every character with a history file, as code points (appended to _out, a u32
// rde_arr), in no particular order.
void fude_history_list(rde_arr* _out);

// --- a character's whole history, for the album ------------------------------------

RDE_STRUCT {
    u16 x, y;         // a fraction of the square's side, times 65535; Y up
    f32 time;         // seconds since the stroke began
} fude_history_point;

RDE_STRUCT {
    u32 first_point;  // into fude_history.points
    u32 point_count;
} fude_history_stroke;

RDE_STRUCT {
    fude_score score;        // as scored then; .empty when the square was left empty
    u32        first_stroke; // into fude_history.strokes
    u32        stroke_count;
} fude_history_square;

RDE_STRUCT {
    u64 time;                // Unix seconds, UTC
    f32 average;
    u32 first_square;        // into fude_history.squares
    u32 square_count;
} fude_history_session;

RDE_STRUCT {
    u32 codepoint;
    rde_arr TYPE(fude_history_session) sessions;   // oldest first
    rde_arr TYPE(fude_history_square)  squares;
    rde_arr TYPE(fude_history_stroke)  strokes;
    rde_arr TYPE(fude_history_point)   points;
} fude_history;

void fude_history_init(fude_history* _history);
void fude_history_destroy(fude_history* _history);
// Replaces _history's contents with _codepoint's sessions. False (and empty)
// when it has none or the file is unreadable.
b8   fude_history_load(fude_history* _history, u32 _codepoint);

#endif
