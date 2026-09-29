#ifndef KANA_PRACTICE
#define KANA_PRACTICE

#include "rde.h"
#include "kanji.h"
#include "glyph.h"
#include "ink.h"
#include "score.h"
#include "history.h"

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
// ===========================================================================

#define KANA_PRACTICE_MAX_SQUARES     12
#define KANA_PRACTICE_DEFAULT_SQUARES 6
#define KANA_PRACTICE_UNITS           1000.0f

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

    // Layout of the last frame (screen space).
    rde_vec_2F           square_tl[KANA_PRACTICE_MAX_SQUARES];
    f32                  square_size;
} kana_practice;

void kana_practice_init(kana_practice* _practice, const kana_kanji_db* _db);
void kana_practice_destroy(kana_practice* _practice);

// Opens on a character (a record index): empty squares, its history summary.
void kana_practice_open(kana_practice* _practice, u32 _record);
void kana_practice_close(kana_practice* _practice);

void kana_practice_set_squares(kana_practice* _practice, u32 _count);
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

#endif
