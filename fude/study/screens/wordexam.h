// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_WORDEXAM
#define FUDE_WORDEXAM

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/catalog.h"
#include "study/chars/glyph.h"
#include "drawing/ink/ink.h"
#include "drawing/widgets/scroll.h"
#include "study/handwriting/recognize.h"
#include "study/models/vocab.h"

// ===========================================================================
// Word exams: the learner's words (vocab.h) written from memory, whole — a box
// for each character, kana and all.
//
//   SETUP    how many (10, 20, 50 or all of them, shuffled) and how each is
//            asked: by its MEANING and READING, or by EAR (said aloud, its
//            meaning under it — where there is a voice, speech.h).
//   WRITING  one at a time: the prompt, then the word's boxes; a character the
//            data cannot draw (a digit, a letter) shows in its box, given.
//            Undo, Clear; Next, Finish on the last.
//   RESULTS  each word: what was written, small, right or not; a tap opens the
//            word card (wordcard.h). The wrong ones can be tried again.
//
// Reviews (review.h) open straight on writing what is due, by meaning and reading.
//
// MARKING, per box, as exams mark a character: right when recognition (ML Kit,
// Kana's matcher behind it) has it among its first FUDE_EXAM_CANDIDATES; an empty
// box is wrong. The word is right when every box is. Every word answered moves its
// review (review.h, its FUDE_VOCAB_KEY): how well it was written is the boxes'
// points, together.
// ===========================================================================

#define FUDE_WORDEXAM_MAX    50u
#define FUDE_WORDEXAM_CHARS  8u        // a word longer than this is not asked
#define FUDE_WORDEXAM_UNITS  1000.0f   // a box's side, in its ink's units

typedef enum {
    FUDE_WORDEXAM_SETUP = 0,
    FUDE_WORDEXAM_WRITING,
    FUDE_WORDEXAM_RESULTS
} FUDE_WORDEXAM_STAGE_;

typedef enum {
    FUDE_WORDEXAM_BY_MEANING = 0,   // its meaning and reading shown
    FUDE_WORDEXAM_BY_EAR            // said aloud; its meaning shown
} FUDE_WORDEXAM_BY_;

RDE_STRUCT {
    u32      word;                              // its id (vocab.h)
    u32      count;                             // characters
    u32      chars[FUDE_WORDEXAM_CHARS];        // code points (a box's first: its letter)
    c8       written[FUDE_WORDEXAM_CHARS][24];  // a box's whole syllable: its letter and the signs written on it (Thai, Hindi), UTF-8
    u32      records[FUDE_WORDEXAM_CHARS];      // the data's (UINT32_MAX: given, not written)
    fude_ink ink[FUDE_WORDEXAM_CHARS];          // a box each: FUDE_WORDEXAM_UNITS square, Y up
    b8       box_graded[FUDE_WORDEXAM_CHARS];
    b8       box_right[FUDE_WORDEXAM_CHARS];
    f32      box_points[FUDE_WORDEXAM_CHARS];   // score.h's, 0..100
    b8       answered;
    b8       graded;
    b8       correct;
    f32      points;                            // the written boxes', together (0..100)
} fude_wordexam_item;

// A chip on the setup, as laid out last frame (screen space).
RDE_STRUCT {
    rde_vec_2F min;
    rde_vec_2F max;
    u8         kind;    // 0: how many, 1: asked by
    u8         value;
} fude_wordexam_chip;

RDE_STRUCT {
    const fude_kanji_db* db;
    const fude_catalog*  catalog;
    fude_glyph           glyph;
    b8                   open;
    FUDE_WORDEXAM_STAGE_ stage;
    FUDE_WORDEXAM_BY_    by;
    u32                  length;                    // index into the lengths (10, 20, 50, all)
    b8                   review;                    // opened by Reviews
    c8                   title[256];                // what it is of (a list's name, Vocabulary, Reviews)

    rde_arr TYPE(u32)    words;                     // what it can ask (ids)
    fude_wordexam_item   items[FUDE_WORDEXAM_MAX];
    u32                  count;                     // asked
    u32                  current;

    fude_recognition     recognition;
    u32                  grading_item;              // the box ML Kit reads (UINT32_MAX: none)
    u32                  grading_box;
    f64                  grading_since;
    b8                   kept;                      // the reviews moved
    b8                   pen;                       // writing: in a box
    u32                  pen_box;
    u32                  strokes[FUDE_WORDEXAM_CHARS * 64u];   // each stroke's box, in the order written (for Undo)
    u32                  stroke_count;
    u32                  rng;
    b8                   spoken;                    // by ear: the current word said once

    // Layout of the last frame (screen space).
    fude_scroller        scroller;
    fude_wordexam_chip   chips[8];
    u32                  chip_count;
    rde_vec_2F           boxes_tl;
    f32                  box;
    f32                  box_gap;
    rde_vec_2F           speaker_min;               // by ear: say it again
    rde_vec_2F           speaker_max;
    rde_vec_2F           rows_min;                  // results
    rde_vec_2F           rows_max;
    f32                  content_h;
    i32                  tapped;                    // results: a word tapped (an item), for the owner (-1: none)
} fude_wordexam;

void fude_wordexam_init(fude_wordexam* _exam, const fude_kanji_db* _db, const fude_catalog* _catalog);
void fude_wordexam_destroy(fude_wordexam* _exam);
// The setup, for words _ids (what _title names).
void fude_wordexam_open(fude_wordexam* _exam, const u32* _ids, u32 _count, const c8* _title);
// Reviews: _ids, straight to writing them, by meaning and reading.
void fude_wordexam_open_review(fude_wordexam* _exam, const u32* _ids, u32 _count);
void fude_wordexam_close(fude_wordexam* _exam);

// How many the setup makes: its length of the words it can ask.
u32  fude_wordexam_planned(const fude_wordexam* _exam);
void fude_wordexam_start(fude_wordexam* _exam);
void fude_wordexam_undo(fude_wordexam* _exam);
void fude_wordexam_clear(fude_wordexam* _exam);
// On to the next word (the last: the results).
void fude_wordexam_next(fude_wordexam* _exam);
b8   fude_wordexam_at_last(const fude_wordexam* _exam);
b8   fude_wordexam_graded(const fude_wordexam* _exam);
u32  fude_wordexam_wrong(const fude_wordexam* _exam);
// The wrong ones again, straight to writing.
void fude_wordexam_retry_wrong(fude_wordexam* _exam);
// A result tapped: its word (id); false when none was. Once per tap.
b8   fude_wordexam_take_tap(fude_wordexam* _exam, u32* _word);

void fude_wordexam_pointer_down(fude_wordexam* _exam, rde_vec_2F _screen, b8 _pen, f64 _time);
void fude_wordexam_pointer_moved(fude_wordexam* _exam, rde_vec_2F _screen, f64 _time);
void fude_wordexam_pointer_up(fude_wordexam* _exam, f64 _time);
// Once a frame: answers read (one box at a time), the reviews moved when all are.
void fude_wordexam_update(fude_wordexam* _exam, f32 _dt);
void fude_wordexam_render(fude_wordexam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The screen (screen.h): its rows of buttons, and what they do.
extern const struct fude_screen FUDE_WORDEXAM_SCREEN;

#endif
