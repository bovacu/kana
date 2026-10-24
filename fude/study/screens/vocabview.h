// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_VOCABVIEW
#define FUDE_VOCABVIEW

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/glyph.h"
#include "drawing/widgets/kit.h"
#include "study/models/vocab.h"

// ===========================================================================
// Vocabulary screen (the side panel's Vocabulary): the learner's words (vocab.h), newest first, either all of them or
// one list's. Of those it shows the ones the field at the top right finds as it is typed: in what is written (駅 finds
// every word with 駅 in it), in the reading (in kana or in romaji: えき, eki), or in the meaning. What is shown is what
// Review, Exam, Sheet and Practice take.
//
//   the lists, as chips (All, each list with its count, and + New list): a tap shows that list, and a tap on the list
//   already shown renames or deletes it (the word card's list form, wordcard.h). They wrap onto at most three rows,
//   which scroll when there are more;
//   the words, a row each: the written form, its reading (a tap there says it aloud), its meaning, and where its
//   reviews are (new, due, or in n days). A tap elsewhere opens the word card, to change the word, file it in lists,
//   or take it out.
//
// Where it sits: a study screen (study/screens), opened from the side panel. The study shell (study/app/shell.c) owns
// the one Vocabulary screen and places FUDE_VOCAB_SCREEN in the app's screen table.
// Its main types and their lifecycle: fude_vocabview, made with fude_vocabview_init over the character data and freed
// with fude_vocabview_destroy when the app ends.
// Threads: main thread only.
// Files: none of its own; the words are vocab.h's.
// Design: the words shown are worked out again only when the vocabulary's revision, the list shown or the search
// changes, not every frame.
// ===========================================================================

// The most characters the search field takes, and the most bytes they can be (a character is at most 4 in UTF-8, and
// one more for the final 0), so a search is never cut in the middle of a character.
#define FUDE_VOCABVIEW_FIELD_MAX 40u
#define FUDE_VOCABVIEW_QUERY     (FUDE_VOCABVIEW_FIELD_MAX * 4u + 1u)

// A list's chip, as laid out last frame (scrolled, when the chips scroll).
RDE_STRUCT {
    rde_vec_2F min;
    rde_vec_2F max;
    u32        list;   // The list it shows (0: All), or UINT32_MAX for + New list.
    f32        w;      // Its width, kept from its layout so that it is drawn exactly as wide.
} fude_vocabview_chip;

RDE_STRUCT {
    const fude_kanji_db* db;
    fude_glyph           glyph;
    b8                   open;
    u32                  list;           // The list shown (0: every word).
    c8                   query[FUDE_VOCABVIEW_QUERY];         // What the field searches for, as typed ("": every word).
    c8                   _query_fold[FUDE_VOCABVIEW_QUERY];   // Folded for searching: lower case, katakana as hiragana
                                                              // (lang.h); never longer than the query.
    // The same read as romaji ("" when it is not romaji): only a query of Latin letters is, at most
    // FUDE_VOCABVIEW_FIELD_MAX of them, and each makes at most 3 bytes of the language's script.
    c8                   _query_kana[FUDE_VOCABVIEW_FIELD_MAX * 3u + 1u];
    rde_arr TYPE(u32)    ids;            // The words shown, newest first.
    u32                  _listed_at;     // The fude_vocab_revision they were listed at (UINT32_MAX: to list).
    u32                  _listed_list;
    u32                  _listings;      // How many times the words shown were listed: what is worked out from them
                                         // (the Review count) is worked out again when it changes.
    // The Review count on the row ("Review n"), and what it was worked out for: the listing, the reviews' revision and
    // the day (fude_vocabview_screen_faces). UINT32_MAX in each until it is first worked out.
    u32                  _due_for[3];
    u32                  _due_n;

    // The layout of the last frame, and taps (screen space).
    rde_ui_scroller      scroller;
    rde_arr TYPE(fude_vocabview_chip) chips;   // All, every list, and + New list.
    rde_vec_2F           rows_min;
    rde_vec_2F           rows_max;
    f32                  content_h;
    f32                  reading_x0;     // The readings' column, its speaker included.
    f32                  reading_x1;
    i32                  pressed;        // The row under a press that is still a tap (-1: none).
    b8                   in_rows;        // The press began in the rows.
    // The chips' area. At most a few rows of chips show (vocabview.c: FUDE_VOCABVIEW_CHIP_ROWS); with more lists than
    // fit, the area scrolls on its own, so dozens of lists never push the words off the screen.
    rde_ui_scroller      chips_scroller;
    rde_vec_2F           chips_min;      // The area, as laid out last frame.
    rde_vec_2F           chips_max;
    f32                  chips_h;        // The height of all the chips' rows.
    b8                   chips_scroll;   // There were more rows than show, so the area scrolls.
    b8                   chips_reveal;   // The list shown changed: its chip is scrolled into view.
    b8                   in_chips;       // The press began in the area while it scrolls.
} fude_vocabview;

// Makes the screen over the character data (for the words' characters); destroy frees it.
void fude_vocabview_init(fude_vocabview* _view, const fude_kanji_db* _db);
void fude_vocabview_destroy(fude_vocabview* _view);
// Opens it with its field empty, on the list last shown; closes it.
void fude_vocabview_open(fude_vocabview* _view);
void fude_vocabview_close(fude_vocabview* _view);
// Shows list _list (0: every word). + Word then saves into that list.
void fude_vocabview_show_list(fude_vocabview* _view, u32 _list);
// Shows the words _query finds, the spaces around it dropped ("": every word of the list shown).
void fude_vocabview_search(fude_vocabview* _view, const c8* _query);

// The words shown: their ids, newest first, into *_ids. Returns how many. The ids belong to the screen and stay valid
// until the words shown next change (the vocabulary, the list shown or the search).
u32  fude_vocabview_words(fude_vocabview* _view, const u32** _ids);
// The characters of the words shown, as records, each once, in the order they come (the words oldest first), at most
// _max into _out: a word's kanji, or its kana when it has none (すし, テレビ). Returns how many.
u32  fude_vocabview_records(fude_vocabview* _view, u32* _out, u32 _max);
// Of the words shown, the ones review.h has due today (and the new ones it has room for, the longest saved first):
// their ids, at most _max into _out. Returns how many.
u32  fude_vocabview_due(fude_vocabview* _view, u32* _out, u32 _max);

// One pointer, in screen space: a drag scrolls, and a tap is taken in fude_vocabview_update.
void fude_vocabview_pointer_down(fude_vocabview* _view, rde_vec_2F _screen, f64 _time);
void fude_vocabview_pointer_moved(fude_vocabview* _view, rde_vec_2F _screen, f64 _time);
void fude_vocabview_pointer_up(fude_vocabview* _view, f64 _time);
// Once a frame while open: keeps the words shown up with the vocabulary, shows a list just made, scrolls, and acts on
// a tap.
void fude_vocabview_update(fude_vocabview* _view, f32 _dt);
// Draws the screen between _top and _bottom, above the row of buttons. Call it inside a 2D drawing block; screen
// space has its origin in the centre, Y up.
void fude_vocabview_render(fude_vocabview* _view, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The Vocabulary as a screen (screen.h). Its field searches as it is typed. Its row: Back; + Word (typed in, into the
// list shown); Translate with Google (the translator, translator.h, where there is one); "Review n" (the words due,
// review.h, as a word exam); Exam (a word exam of the words shown, wordexam.h); Sheet (their characters as a
// practice sheet); Practice (their characters as a set).
extern const struct fude_screen FUDE_VOCAB_SCREEN;

#endif
