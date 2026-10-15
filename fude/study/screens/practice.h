// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_PRACTICE
#define FUDE_PRACTICE

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/glyph.h"
#include "drawing/ink/ink.h"
#include "study/handwriting/score.h"
#include "study/models/history.h"
#include "study/handwriting/guide.h"

// ===========================================================================
// Practice: one character, written over and over in practice-sheet squares.
//
// At the top the character writes itself on a loop, small, next to its readings and how earlier sessions went; below
// are N squares for the pen. Score marks every square that was written in (score.h): the reference is shown faintly
// behind the ink, each square gets a score and one line of feedback, and the average is shown. Scoring also SAVES the
// session (history.h): the time, and each square's drawing, score and details. Scoring again without writing anything
// new saves nothing new.
//
// A SET is practice over a list, such as a chart section, a Browse list or the album's weakest. Next scores (and so
// saves) what was written but not scored, then moves on; after the last character the set's SUMMARY shows how each
// one went this run, and "Weakest again" makes a new set of the ones under good. One character on its own (the
// viewer's Practice) is a set of one, and has none of that.
//
// GUIDED (guide.h): one big square instead, and the character is learnt in three steps: traced, written from its
// start dots, and written from memory. Only the last step is scored and saved, as a session of one square. Guided
// stays on across a set's characters until it is switched off.
//
// Where it sits: a study screen (study/screens). The study's verbs (fude_study_practice, fude_study_practice_set)
// open it from the viewer, Browse, the chart, the album, Check, the exams, the Vocabulary and the translator. The
// study shell (study/app/shell.c) owns the one Practice and places FUDE_PRACTICE_SCREEN in the app's screen table.
// Its main types and their lifecycle: fude_practice, made with fude_practice_init over the character data and freed
// with fude_practice_destroy when the app ends. It holds every square's ink.
// Threads: main thread only.
// Files: it appends sessions to the practice history (history.h) and reads a character's summary from it.
// Design: each square keeps its drawing in its own space, FUDE_PRACTICE_UNITS wide, so turning the screen resizes the
// squares without distorting what is in them. Only the pen writes (and the desktop mouse, or a finger when the hand
// is switched on: a FUDE_SCREEN_INPUT_WRITE screen); Undo takes back the last stroke, whichever square it went in.
// ===========================================================================

#define FUDE_PRACTICE_MAX_SQUARES     12
#define FUDE_PRACTICE_DEFAULT_SQUARES 6
#define FUDE_PRACTICE_UNITS           1000.0f
// A set's most characters: a longer list is cut to its first this many, and the verb that opens it says so (verbs.c).
// A bound by design: a set is one sitting, and its summary shows every character of it in one grid, without
// scrolling, its cells shrinking to 48 points at the least (fude_practice_render_summary), which a hundred fill.
#define FUDE_PRACTICE_SET_MAX         100u

RDE_STRUCT {
    const fude_kanji_db* db;
    fude_glyph           glyph;
    b8                   open;

    u32                  record;
    fude_kanji_info      info;
    u32                  squares;                               // How many squares are shown.
    fude_ink             inks[FUDE_PRACTICE_MAX_SQUARES];       // Each square's drawing.
    fude_score           scores[FUDE_PRACTICE_MAX_SQUARES];     // From the last Score.
    b8                   scored;                                // The scores are showing.
    b8                   changed;                               // Written in since the last save.
    rde_arr TYPE(u8)     strokes_in;                            // The square each stroke went to, for Undo.

    f64                  demo_start;      // When the small character's writing loop began.
    f64                  demo_done;       // When it finished writing (0 while it is still writing).
    fude_history_summary summary;
    b8                   has_summary;
    c8                   status[512];     // After scoring: the average, and whether it was saved.

    i32                  writing;         // The square the pen is in, or -1.

    // The set (with one record, plain practice).
    rde_arr TYPE(u32)    set;             // Its records, in order.
    rde_arr TYPE(f32)    set_results;     // Per position: this run's average, or below 0 when not scored.
    u32                  set_position;
    b8                   summary_open;    // The set is done and its summary shows.

    b8                   guided;
    fude_guide           guide;
    c8                   feedback[384];   // Guided: the scored attempt's feedback line.

    // The layout of the last frame (screen space).
    rde_vec_2F           square_tl[FUDE_PRACTICE_MAX_SQUARES];
    f32                  square_size;
} fude_practice;

// Makes Practice over the character data (_db may be NULL: it then never opens); destroy frees it.
void fude_practice_init(fude_practice* _practice, const fude_kanji_db* _db);
void fude_practice_destroy(fude_practice* _practice);

// Opens on one character (a record index), with empty squares and its history's summary.
void fude_practice_open(fude_practice* _practice, u32 _record);
// Opens on a list of characters (records, copied; at most FUDE_PRACTICE_SET_MAX), on the first. Nothing happens with
// an empty list or no character data.
void fude_practice_open_set(fude_practice* _practice, const u32* _records, u32 _count);
void fude_practice_close(fude_practice* _practice);

// Whether this is a set of more than one character.
b8   fude_practice_in_set(const fude_practice* _practice);
// Whether it is on the set's last character, where Next finishes.
b8   fude_practice_at_last(const fude_practice* _practice);
// Scores what was written and not yet scored (which saves it), then goes to the next character, or, after the last,
// to the summary.
void fude_practice_next(fude_practice* _practice);
// From the summary: how many characters scored under good this run; and a new set of them, the weakest first.
u32  fude_practice_weak_count(const fude_practice* _practice);
void fude_practice_weakest_again(fude_practice* _practice);

// Shows _count squares (1 to FUDE_PRACTICE_MAX_SQUARES). Squares that go away take their drawings with them.
void fude_practice_set_squares(fude_practice* _practice, u32 _count);
// Turns guided mode on or off: the squares are cleared, and the character starts again from step 1.
void fude_practice_set_guided(fude_practice* _practice, b8 _guided);
// Whether it is guided and on a step that is not scored (steps 1 and 2), where the squares' count and Score do
// nothing.
b8   fude_practice_guiding(const fude_practice* _practice);
// Once a frame: when guided, a finished step moves on to the next.
void fude_practice_update(fude_practice* _practice);
// Takes back the last stroke, whichever square it went in (not while the pen is down).
void fude_practice_undo(fude_practice* _practice);
// Empties every square and the status line; when guided, the step starts again.
void fude_practice_clear(fude_practice* _practice);
// Scores every square written in, and saves the session if anything is new. The status line says the average and
// whether it was saved. On a guided step that is not scored, it does nothing.
void fude_practice_score(fude_practice* _practice);

// The pen (or the desktop mouse), in screen space: a stroke goes in the square it starts in.
void fude_practice_pen_down(fude_practice* _practice, rde_vec_2F _screen);
void fude_practice_pen_moved(fude_practice* _practice, rde_vec_2F _screen);
void fude_practice_pen_up(fude_practice* _practice);

// Draws the page between _top and _bottom (screen y). Call it inside a 2D drawing block.
void fude_practice_render(fude_practice* _practice, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// Practice as a screen (screen.h), a writing screen. Its rows: for one character, Back, Undo, Clear, Score, Fewer,
// More and Guided; for a set, Back, Undo, Clear, Score, Guided and Next (Finish on the last); for the set's summary,
// Weakest again and Done.
extern const struct fude_screen FUDE_PRACTICE_SCREEN;

#endif
