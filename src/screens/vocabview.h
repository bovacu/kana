#ifndef KANA_VOCABVIEW
#define KANA_VOCABVIEW

#include "rde.h"
#include "chars/kanji.h"
#include "widgets/glyph.h"
#include "widgets/scroll.h"
#include "study/vocab.h"

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
} kana_vocabview_chip;

RDE_STRUCT {
    const kana_kanji_db* db;
    kana_glyph           glyph;
    b8                   open;
    u32                  list;           // the list shown (0: every word)
    rde_arr TYPE(u32)    ids;            // the words shown, newest first
    u32                  _listed_at;     // kana_vocab_revision they were listed at (UINT32_MAX: to list)
    u32                  _listed_list;

    // Layout of the last frame, and taps (screen space).
    kana_scroller        scroller;
    kana_vocabview_chip  chips[KANA_VOCAB_LISTS + 2u];
    u32                  chip_count;
    rde_vec_2F           rows_min;
    rde_vec_2F           rows_max;
    f32                  content_h;
    f32                  reading_x0;     // the readings' column (its speaker included)
    f32                  reading_x1;
    i32                  pressed;        // the row under a press still a tap (-1: none)
    b8                   in_rows;        // the press began in the rows
} kana_vocabview;

void kana_vocabview_init(kana_vocabview* _view, const kana_kanji_db* _db);
void kana_vocabview_destroy(kana_vocabview* _view);
void kana_vocabview_open(kana_vocabview* _view);
void kana_vocabview_close(kana_vocabview* _view);
// Shows list _list (0: every word).
void kana_vocabview_show_list(kana_vocabview* _view, u32 _list);

// The words shown: their ids, newest first. How many.
u32  kana_vocabview_words(kana_vocabview* _view, const u32** _ids);
// Their characters as records, each once, in the order they come (the
// words' order: oldest first), at most _max into _out. How many.
u32  kana_vocabview_records(kana_vocabview* _view, u32* _out, u32 _max);
// Of the words shown, the ones review.h has due today (and new ones it has room
// for): ids into _out, at most _max. How many.
u32  kana_vocabview_due(kana_vocabview* _view, u32* _out, u32 _max);

void kana_vocabview_pointer_down(kana_vocabview* _view, rde_vec_2F _screen, f64 _time);
void kana_vocabview_pointer_moved(kana_vocabview* _view, rde_vec_2F _screen, f64 _time);
void kana_vocabview_pointer_up(kana_vocabview* _view, f64 _time);
// Once a frame: the list kept up with the vocabulary; scrolling; taps.
void kana_vocabview_update(kana_vocabview* _view, f32 _dt);
// Inside a 2D drawing block, screen space centre origin, Y up, between _top and
// _bottom (above the row of buttons).
void kana_vocabview_render(kana_vocabview* _view, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The screen (screen.h): its rows of buttons, and what they do.
extern const struct kana_screen KANA_VOCAB_SCREEN;

#endif
