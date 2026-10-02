#ifndef KANA_PRACTICE
#define KANA_PRACTICE

#include "rde.h"
#include "chars/kanji.h"
#include "widgets/glyph.h"
#include "ink/ink.h"
#include "handwriting/score.h"
#include "study/history.h"
#include "handwriting/guide.h"

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
// Each square keeps its drawing in its own KANA_PRACTICE_UNITS-wide space, so a
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

#define KANA_PRACTICE_MAX_SQUARES     12
#define KANA_PRACTICE_DEFAULT_SQUARES 6
#define KANA_PRACTICE_UNITS           1000.0f
#define KANA_PRACTICE_SET_MAX         100u      // a longer list is cut to its first this many

RDE_STRUCT {
    const kana_kanji_db* db;
    kana_glyph           glyph;
    b8                   open;

    u32                  record;
    kana_kanji_info      info;
    u32                  squares;                               // how many are shown
    kana_ink             inks[KANA_PRACTICE_MAX_SQUARES];       // each square's drawing
    kana_score           scores[KANA_PRACTICE_MAX_SQUARES];     // from the last Score
    b8                   scored;                                // scores are showing
    b8                   changed;                               // written since the last save
    rde_arr TYPE(u8)     strokes_in;                            // the square each stroke went to (Undo)

    f64                  demo_start;      // the small character's writing loop
    f64                  demo_done;       // when it finished writing (0: still writing)
    kana_history_summary summary;
    b8                   has_summary;
    c8                   status[128];     // after scoring: the average, and whether it was saved

    i32                  writing;         // the square the pen is in, or -1

    // The set (one record: plain practice).
    rde_arr TYPE(u32)    set;             // records, in order
    rde_arr TYPE(f32)    set_results;     // per position: this run's average, or < 0 (not scored)
    u32                  set_position;
    b8                   summary_open;    // the set is done: its summary shows

    b8                   guided;
    kana_guide           guide;
    c8                   feedback[96];    // guided: the scored attempt's line

    // Layout of the last frame (screen space).
    rde_vec_2F           square_tl[KANA_PRACTICE_MAX_SQUARES];
    f32                  square_size;
} kana_practice;

void kana_practice_init(kana_practice* _practice, const kana_kanji_db* _db);
void kana_practice_destroy(kana_practice* _practice);

// Opens on a character (a record index): empty squares, its history summary.
void kana_practice_open(kana_practice* _practice, u32 _record);
// Opens on a list of characters (records, copied; at most KANA_PRACTICE_SET_MAX),
// the first first.
void kana_practice_open_set(kana_practice* _practice, const u32* _records, u32 _count);
void kana_practice_close(kana_practice* _practice);

// A set of more than one.
b8   kana_practice_in_set(const kana_practice* _practice);
// On the set's last character: Next finishes.
b8   kana_practice_at_last(const kana_practice* _practice);
// Scores what was written and not yet scored (saving it), then the next
// character — or, after the last, the summary.
void kana_practice_next(kana_practice* _practice);
// From the summary: how many scored under good; a new set of them, weakest first.
u32  kana_practice_weak_count(const kana_practice* _practice);
void kana_practice_weakest_again(kana_practice* _practice);

void kana_practice_set_squares(kana_practice* _practice, u32 _count);
// Guided mode on or off: the squares cleared, the character from step 1.
void kana_practice_set_guided(kana_practice* _practice, b8 _guided);
// Guided, and on a step that is not scored (the squares' count and Score do nothing).
b8   kana_practice_guiding(const kana_practice* _practice);
// Once a frame (guided: a finished step moves on).
void kana_practice_update(kana_practice* _practice);
void kana_practice_undo(kana_practice* _practice);
void kana_practice_clear(kana_practice* _practice);
// Scores every square written in, and saves the session if anything is new.
void kana_practice_score(kana_practice* _practice);

// The pen (or the desktop mouse), screen space.
void kana_practice_pen_down(kana_practice* _practice, rde_vec_2F _screen);
void kana_practice_pen_moved(kana_practice* _practice, rde_vec_2F _screen);
void kana_practice_pen_up(kana_practice* _practice);

// Draws the page between _top and _bottom (screen y). Inside a 2D drawing block.
void kana_practice_render(kana_practice* _practice, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The screen (screen.h): its rows of buttons, and what they do.
extern const struct kana_screen KANA_PRACTICE_SCREEN;

#endif
