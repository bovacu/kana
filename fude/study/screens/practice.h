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
// At the top the character writes itself on a loop, small, next to its
// readings and how earlier sessions went; below, N squares for the pen. Score
// marks every square that was written in (score.h) — the reference shown faintly
// behind the ink, a score and one line of feedback per square, and the average —
// and SAVES the session (history.h): the time, each square's drawing, score and
// details. Scoring again without writing anything new saves nothing new.
//
// Each square keeps its drawing in its own FUDE_PRACTICE_UNITS-wide space, so a
// rotation of the screen resizes the squares without distorting what is in them.
// Only the pen writes; Undo takes back the last stroke, wherever it went.
//
// A SET is practice over a list — a chart section, a Browse list, the album's
// weakest: Next scores (and saves) what was written but not scored, then moves
// on; after the last, the set's SUMMARY shows how each character went this run,
// and "weakest again" makes a new set of the ones under good. One character (the
// viewer's Practice) is a set of one, and has none of that.
//
// GUIDED (guide.h): one big square instead, and the character learnt in three
// steps — traced, from its start dots, from memory; only the last is scored
// and saved, as a session of one square. It stays on across a set's
// characters until switched off.
// ===========================================================================

#define FUDE_PRACTICE_MAX_SQUARES     12
#define FUDE_PRACTICE_DEFAULT_SQUARES 6
#define FUDE_PRACTICE_UNITS           1000.0f
#define FUDE_PRACTICE_SET_MAX         100u      // a longer list is cut to its first this many

RDE_STRUCT {
    const fude_kanji_db* db;
    fude_glyph           glyph;
    b8                   open;

    u32                  record;
    fude_kanji_info      info;
    u32                  squares;                               // how many are shown
    fude_ink             inks[FUDE_PRACTICE_MAX_SQUARES];       // each square's drawing
    fude_score           scores[FUDE_PRACTICE_MAX_SQUARES];     // from the last Score
    b8                   scored;                                // scores are showing
    b8                   changed;                               // written since the last save
    rde_arr TYPE(u8)     strokes_in;                            // the square each stroke went to (Undo)

    f64                  demo_start;      // the small character's writing loop
    f64                  demo_done;       // when it finished writing (0: still writing)
    fude_history_summary summary;
    b8                   has_summary;
    c8                   status[128];     // after scoring: the average, and whether it was saved

    i32                  writing;         // the square the pen is in, or -1

    // The set (one record: plain practice).
    rde_arr TYPE(u32)    set;             // records, in order
    rde_arr TYPE(f32)    set_results;     // per position: this run's average, or < 0 (not scored)
    u32                  set_position;
    b8                   summary_open;    // the set is done: its summary shows

    b8                   guided;
    fude_guide           guide;
    c8                   feedback[96];    // guided: the scored attempt's line

    // Layout of the last frame (screen space).
    rde_vec_2F           square_tl[FUDE_PRACTICE_MAX_SQUARES];
    f32                  square_size;
} fude_practice;

void fude_practice_init(fude_practice* _practice, const fude_kanji_db* _db);
void fude_practice_destroy(fude_practice* _practice);

// Opens on a character (a record index): empty squares, its history summary.
void fude_practice_open(fude_practice* _practice, u32 _record);
// Opens on a list of characters (records, copied; at most FUDE_PRACTICE_SET_MAX),
// the first first.
void fude_practice_open_set(fude_practice* _practice, const u32* _records, u32 _count);
void fude_practice_close(fude_practice* _practice);

// A set of more than one.
b8   fude_practice_in_set(const fude_practice* _practice);
// On the set's last character: Next finishes.
b8   fude_practice_at_last(const fude_practice* _practice);
// Scores what was written and not yet scored (saving it), then the next
// character — or, after the last, the summary.
void fude_practice_next(fude_practice* _practice);
// From the summary: how many scored under good; a new set of them, weakest first.
u32  fude_practice_weak_count(const fude_practice* _practice);
void fude_practice_weakest_again(fude_practice* _practice);

void fude_practice_set_squares(fude_practice* _practice, u32 _count);
// Guided mode on or off: the squares cleared, the character from step 1.
void fude_practice_set_guided(fude_practice* _practice, b8 _guided);
// Guided, and on a step that is not scored (the squares' count and Score do nothing).
b8   fude_practice_guiding(const fude_practice* _practice);
// Once a frame (guided: a finished step moves on).
void fude_practice_update(fude_practice* _practice);
void fude_practice_undo(fude_practice* _practice);
void fude_practice_clear(fude_practice* _practice);
// Scores every square written in, and saves the session if anything is new.
void fude_practice_score(fude_practice* _practice);

// The pen (or the desktop mouse), screen space.
void fude_practice_pen_down(fude_practice* _practice, rde_vec_2F _screen);
void fude_practice_pen_moved(fude_practice* _practice, rde_vec_2F _screen);
void fude_practice_pen_up(fude_practice* _practice);

// Draws the page between _top and _bottom (screen y). Inside a 2D drawing block.
void fude_practice_render(fude_practice* _practice, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The screen (screen.h): its rows of buttons, and what they do.
extern const struct fude_screen FUDE_PRACTICE_SCREEN;

#endif
