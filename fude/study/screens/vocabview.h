#ifndef FUDE_VOCABVIEW
#define FUDE_VOCABVIEW

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/glyph.h"
#include "drawing/widgets/scroll.h"
#include "study/models/vocab.h"

// ===========================================================================
// The Vocabulary screen (the side panel's Vocabulary): the learner's words
// (vocab.h), newest first — all of them, or one list's.
//
//   the lists as chips (All, each list with its count, + New list): a tap shows
//   that list; a tap on the one shown renames or deletes it (the word card's
//   list form, wordcard.h);
//   the words, a row each: written, its reading (a tap there says it), its
//   meaning, and where its reviews are (new, due, in n days); a tap opens the
//   word card, to change it, file it in lists, or take it out.
//
// Its row (row.h): Back, + Word (typed in; in the list shown), Review n
// (the words due, review.h), Exam (word exams, wordexam.h), Sheet (their
// characters as a practice sheet), Practice (their characters as a set).
// ===========================================================================

RDE_STRUCT {
    rde_vec_2F min;
    rde_vec_2F max;
    u32        list;   // the list it shows (0: All); UINT32_MAX: + New list
} fude_vocabview_chip;

RDE_STRUCT {
    const fude_kanji_db* db;
    fude_glyph           glyph;
    b8                   open;
    u32                  list;           // the list shown (0: every word)
    rde_arr TYPE(u32)    ids;            // the words shown, newest first
    u32                  _listed_at;     // fude_vocab_revision they were listed at (UINT32_MAX: to list)
    u32                  _listed_list;

    // Layout of the last frame, and taps (screen space).
    fude_scroller        scroller;
    fude_vocabview_chip  chips[FUDE_VOCAB_LISTS + 2u];
    u32                  chip_count;
    rde_vec_2F           rows_min;
    rde_vec_2F           rows_max;
    f32                  content_h;
    f32                  reading_x0;     // the readings' column (its speaker included)
    f32                  reading_x1;
    i32                  pressed;        // the row under a press still a tap (-1: none)
    b8                   in_rows;        // the press began in the rows
} fude_vocabview;

void fude_vocabview_init(fude_vocabview* _view, const fude_kanji_db* _db);
void fude_vocabview_destroy(fude_vocabview* _view);
void fude_vocabview_open(fude_vocabview* _view);
void fude_vocabview_close(fude_vocabview* _view);
// Shows list _list (0: every word).
void fude_vocabview_show_list(fude_vocabview* _view, u32 _list);

// The words shown: their ids, newest first. How many.
u32  fude_vocabview_words(fude_vocabview* _view, const u32** _ids);
// Their characters as records, each once, in the order they come (the
// words' order: oldest first), at most _max into _out. How many.
u32  fude_vocabview_records(fude_vocabview* _view, u32* _out, u32 _max);
// Of the words shown, the ones review.h has due today (and new ones it has room
// for): ids into _out, at most _max. How many.
u32  fude_vocabview_due(fude_vocabview* _view, u32* _out, u32 _max);

void fude_vocabview_pointer_down(fude_vocabview* _view, rde_vec_2F _screen, f64 _time);
void fude_vocabview_pointer_moved(fude_vocabview* _view, rde_vec_2F _screen, f64 _time);
void fude_vocabview_pointer_up(fude_vocabview* _view, f64 _time);
// Once a frame: the list kept up with the vocabulary; scrolling; taps.
void fude_vocabview_update(fude_vocabview* _view, f32 _dt);
// Inside a 2D drawing block, screen space centre origin, Y up, between _top and
// _bottom (above the row of buttons).
void fude_vocabview_render(fude_vocabview* _view, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The screen (screen.h): its rows of buttons, and what they do.
extern const struct fude_screen FUDE_VOCAB_SCREEN;

#endif
