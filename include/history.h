#ifndef KANA_HISTORY
#define KANA_HISTORY

#include "rde.h"
#include "ink.h"
#include "score.h"

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

#define KANA_HISTORY_VERSION 1u

RDE_STRUCT {
    u32 sessions;
    f32 first;        // the first session's average
    f32 last;         // the latest session's average
    f32 best;         // the best session average
    u64 last_time;    // when the latest was (Unix seconds)
} kana_history_summary;

// The folder, created if missing, ending in '/'.
const c8* kana_history_dir(void);

// Appends one session. _drawings[i] and _scores[i] per square; a square whose
// score is .empty is stored as unscored. _square_size: the squares' side in the
// drawings' units.
b8 kana_history_save(u32 _codepoint, u64 _time, u32 _squares, const kana_score* _scores,
                     const kana_ink* const* _drawings, f32 _square_size);

// Reads a character's sessions down to a summary. False when it has none.
b8 kana_history_summarize(u32 _codepoint, kana_history_summary* _out);

#endif
