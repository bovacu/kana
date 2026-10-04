// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_GUIDE
#define FUDE_GUIDE

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/glyph.h"
#include "drawing/ink/ink.h"

// ===========================================================================
// Guided training: Practice's guided mode (practice.h), one character in one
// big square, learnt in three steps, each harder than the one before:
//
//   1 TRACE   the character faint in the square; the stroke to write writes
//             itself, then stays stronger, with a dot where it starts and an
//             arrow for its direction.
//   2 HINT    the character fainter; only the dot where the stroke starts.
//   3 RECALL  a blank square: the whole character from memory. When it has
//             all its strokes it is scored and saved like a Practice square
//             (score.h, history.h), and writing again starts a new attempt.
//
// In steps 1 and 2 each stroke is checked as the pen lifts, against the stroke
// it should be, IN PLACE (the model is in the square to follow): the wrong
// stroke (one that comes later), backwards, or not close enough. A wrong
// stroke flashes red and goes, the reason shows under the square, and the
// stroke writes itself again to show how; a right one stays. A step done, the
// next begins after a moment (or at once, with the pen).
//
// The ink is the caller's (one square, _units wide, Y up); the guide reads its
// last stroke and takes a wrong one back out of it.
// ===========================================================================

#define FUDE_GUIDE_MAX_POINTS 1024u   // a wrong stroke kept to fade out (longer ones are cut)

typedef enum {
    FUDE_GUIDE_TRACE = 0,
    FUDE_GUIDE_HINT,
    FUDE_GUIDE_RECALL,
    FUDE_GUIDE_STAGE_COUNT
} FUDE_GUIDE_STAGE_;

typedef enum {
    FUDE_GUIDE_KEPT = 0,     // right (or, recalling, simply written): it stays
    FUDE_GUIDE_TAKEN_BACK,   // wrong: taken back out of the ink, the message says why
    FUDE_GUIDE_COMPLETE      // recalling, the last stroke: score it
} FUDE_GUIDE_RESULT_;

RDE_STRUCT {
    const fude_kanji_db* db;
    fude_kanji_info      info;
    FUDE_GUIDE_STAGE_    stage;
    u32                  next;            // the stroke to write (0-based; = info.strokes when the step is done)
    u32                  misses;          // wrong tries at it
    f64                  shown_at;        // when it became the one to write, or was missed (its demonstration starts)
    f64                  stage_done_at;   // when the step's last stroke went in (0: still writing)
    c8                   message[384];    // what the last stroke got ("" when it was right)
    rde_vec_2F           rejected[FUDE_GUIDE_MAX_POINTS];   // the last wrong stroke, square units, fading out
    u32                  rejected_count;
    f64                  rejected_at;
} fude_guide;

// Step 1 of a character, nothing written.
void               fude_guide_start(fude_guide* _guide, const fude_kanji_db* _db, const fude_kanji_info* _info, f64 _now);
// The current step again from its first stroke (the caller clears the ink).
void               fude_guide_restart(fude_guide* _guide, f64 _now);
// Once a frame: a finished step moves on after a moment. True when it did —
// the caller clears the ink for the next.
b8                 fude_guide_update(fude_guide* _guide, f64 _now);
// A step is done and waiting: go on now (the pen came down). True when it did.
b8                 fude_guide_skip_pause(fude_guide* _guide, f64 _now);

// The ink's last stroke has just ended.
FUDE_GUIDE_RESULT_ fude_guide_stroke(fude_guide* _guide, fude_ink* _ink, f32 _units, f64 _now);
// Undo took the last stroke back out of the ink.
void               fude_guide_took_back(fude_guide* _guide, f64 _now);

// Drawing, inside a 2D block, the square's top-left at _tl, _size wide:
// the help, under the ink; the wrong stroke fading and a step's tick, over it.
void               fude_guide_render(fude_guide* _guide, fude_glyph* _glyph, rde_vec_2F _tl, f32 _size, f64 _now);
void               fude_guide_render_over(fude_guide* _guide, rde_vec_2F _tl, f32 _size, f32 _units, f64 _now);

// One line for under the square: the step, and what to do.
void               fude_guide_prompt(const fude_guide* _guide, c8* _out, usize _size);

#endif
