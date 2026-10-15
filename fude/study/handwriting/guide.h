// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_GUIDE
#define FUDE_GUIDE

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/glyph.h"
#include "drawing/ink/ink.h"

// ===========================================================================
// Guide: guided training, Practice's guided mode (practice.h). One character is learnt in one big square, in three
// steps, each harder than the one before:
//
//   1 TRACE   the character shows faint in the square; the stroke to write writes itself, then stays stronger, with
//             a dot where it starts and an arrow for its direction.
//   2 HINT    the character shows fainter, with only the dot where the stroke starts.
//   3 RECALL  a blank square: the whole character from memory. Once it has all its strokes it is scored and saved
//             like a Practice square (score.h, history.h), and writing again starts a new attempt.
//
// In steps 1 and 2 each stroke is checked as the pen lifts, against the stroke it should be, IN PLACE (not fitted,
// because the model is in the square to follow, so where the stroke goes counts): it may be the wrong stroke (one
// that comes later), backwards, or not close enough. A wrong stroke flashes red and goes, the reason shows under
// the square, and the stroke writes itself again to show how; a right one stays. When a step is done, the next one
// begins after a moment (or at once, when the pen comes down).
//
// Where it sits: the study layer's handwriting (study/handwriting), used by Practice (practice.h).
// Its main types and their lifecycle: the caller owns a fude_guide, a plain value set up by fude_guide_start; it
// holds nothing to free. The ink is the caller's too (one square, _units wide, Y up): the guide reads its last
// stroke, and takes a wrong one back out of it.
// Threads: call it on the app's main thread only (it uses static scratch, and draws).
// Files: none.
// ===========================================================================

#define FUDE_GUIDE_MAX_POINTS 1024u   // the points of a wrong stroke kept to fade out (longer ones are cut)

typedef enum {
    FUDE_GUIDE_TRACE = 0,
    FUDE_GUIDE_HINT,
    FUDE_GUIDE_RECALL,
    FUDE_GUIDE_STAGE_COUNT
} FUDE_GUIDE_STAGE_;

typedef enum {
    FUDE_GUIDE_KEPT = 0,     // right (or, when recalling, simply written): it stays
    FUDE_GUIDE_TAKEN_BACK,   // wrong: taken back out of the ink, and the message says why
    FUDE_GUIDE_COMPLETE      // recalling, and that was the last stroke: score it
} FUDE_GUIDE_RESULT_;

RDE_STRUCT {
    const fude_kanji_db* db;
    fude_kanji_info      info;
    FUDE_GUIDE_STAGE_    stage;
    u32                  next;            // the stroke to write (0-based; info.strokes once the step is done)
    u32                  misses;          // the wrong tries at it
    f64                  shown_at;        // when it became the one to write, or was missed (its demonstration starts)
    f64                  stage_done_at;   // when the step's last stroke went in (0: still writing)
    c8                   message[384];    // what the last stroke got wrong ("" when it was right)
    rde_vec_2F           rejected[FUDE_GUIDE_MAX_POINTS];   // the last wrong stroke, in square units, fading out
    u32                  rejected_count;
    f64                  rejected_at;
} fude_guide;

// Starts step 1 of a character, with nothing written. _now is the app's clock, as in every call below.
void               fude_guide_start(fude_guide* _guide, const fude_kanji_db* _db, const fude_kanji_info* _info, f64 _now);
// Starts the current step again from its first stroke (the caller clears the ink).
void               fude_guide_restart(fude_guide* _guide, f64 _now);
// Once a frame: a finished step moves on to the next after a moment. Returns true when it did, and then the caller
// clears the ink for the next step.
b8                 fude_guide_update(fude_guide* _guide, f64 _now);
// When a step is done and waiting, goes on now (the pen came down). Returns true when it did.
b8                 fude_guide_skip_pause(fude_guide* _guide, f64 _now);

// Checks the ink's last stroke, which has just ended (_units is the square's width in ink units). A wrong stroke is
// undone in _ink. Returns what became of it.
FUDE_GUIDE_RESULT_ fude_guide_stroke(fude_guide* _guide, fude_ink* _ink, f32 _units, f64 _now);
// Tells the guide that Undo took the last stroke back out of the ink: in steps 1 and 2 that stroke is to be written
// again.
void               fude_guide_took_back(fude_guide* _guide, f64 _now);

// Draws, inside a 2D block, with the square's top-left at _tl and _size wide: fude_guide_render the help, under the
// ink; fude_guide_render_over the wrong stroke fading out and a finished step's tick, over it.
void               fude_guide_render(fude_guide* _guide, fude_glyph* _glyph, rde_vec_2F _tl, f32 _size, f64 _now);
void               fude_guide_render_over(fude_guide* _guide, rde_vec_2F _tl, f32 _size, f32 _units, f64 _now);

// Writes one line for under the square into _out: the step, and what to do.
void               fude_guide_prompt(const fude_guide* _guide, c8* _out, usize _size);

#endif
