#ifndef KANA_WORDEXAM
#define KANA_WORDEXAM

#include "rde.h"
#include "kanji.h"
#include "catalog.h"
#include "glyph.h"
#include "ink.h"
#include "scroll.h"
#include "recognize.h"
#include "vocab.h"

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
// Kana's matcher behind it) has it among its first KANA_EXAM_CANDIDATES; an empty
// box is wrong. The word is right when every box is. Every word answered moves its
// review (review.h, its KANA_VOCAB_KEY): how well it was written is the boxes'
// points, together.
// ===========================================================================

#define KANA_WORDEXAM_MAX    50u
#define KANA_WORDEXAM_CHARS  8u        // a word longer than this is not asked
#define KANA_WORDEXAM_UNITS  1000.0f   // a box's side, in its ink's units

typedef enum {
    KANA_WORDEXAM_SETUP = 0,
    KANA_WORDEXAM_WRITING,
    KANA_WORDEXAM_RESULTS
} KANA_WORDEXAM_STAGE_;

typedef enum {
    KANA_WORDEXAM_BY_MEANING = 0,   // its meaning and reading shown
    KANA_WORDEXAM_BY_EAR            // said aloud; its meaning shown
} KANA_WORDEXAM_BY_;

RDE_STRUCT {
    u32      word;                              // its id (vocab.h)
    u32      count;                             // characters
    u32      chars[KANA_WORDEXAM_CHARS];        // code points
    u32      records[KANA_WORDEXAM_CHARS];      // the data's (UINT32_MAX: given, not written)
    kana_ink ink[KANA_WORDEXAM_CHARS];          // a box each: KANA_WORDEXAM_UNITS square, Y up
    b8       box_graded[KANA_WORDEXAM_CHARS];
    b8       box_right[KANA_WORDEXAM_CHARS];
    f32      box_points[KANA_WORDEXAM_CHARS];   // score.h's, 0..100
    b8       answered;
    b8       graded;
    b8       correct;
    f32      points;                            // the written boxes', together (0..100)
} kana_wordexam_item;

// A chip on the setup, as laid out last frame (screen space).
RDE_STRUCT {
    rde_vec_2F min;
    rde_vec_2F max;
    u8         kind;    // 0: how many, 1: asked by
    u8         value;
} kana_wordexam_chip;

RDE_STRUCT {
    const kana_kanji_db* db;
    const kana_catalog*  catalog;
    kana_glyph           glyph;
    b8                   open;
    KANA_WORDEXAM_STAGE_ stage;
    KANA_WORDEXAM_BY_    by;
    u32                  length;                    // index into the lengths (10, 20, 50, all)
    b8                   review;                    // opened by Reviews
    c8                   title[64];                 // what it is of (a list's name, Vocabulary, Reviews)

    rde_arr TYPE(u32)    words;                     // what it can ask (ids)
    kana_wordexam_item   items[KANA_WORDEXAM_MAX];
    u32                  count;                     // asked
    u32                  current;

    kana_recognition     recognition;
    u32                  grading_item;              // the box ML Kit reads (UINT32_MAX: none)
    u32                  grading_box;
    f64                  grading_since;
    b8                   kept;                      // the reviews moved
    b8                   pen;                       // writing: in a box
    u32                  pen_box;
    u32                  strokes[KANA_WORDEXAM_CHARS * 64u];   // each stroke's box, in the order written (for Undo)
    u32                  stroke_count;
    u32                  rng;
    b8                   spoken;                    // by ear: the current word said once

    // Layout of the last frame (screen space).
    kana_scroller        scroller;
    kana_wordexam_chip   chips[8];
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
} kana_wordexam;

void kana_wordexam_init(kana_wordexam* _exam, const kana_kanji_db* _db, const kana_catalog* _catalog);
void kana_wordexam_destroy(kana_wordexam* _exam);
// The setup, for words _ids (what _title names).
void kana_wordexam_open(kana_wordexam* _exam, const u32* _ids, u32 _count, const c8* _title);
// Reviews: _ids, straight to writing them, by meaning and reading.
void kana_wordexam_open_review(kana_wordexam* _exam, const u32* _ids, u32 _count);
void kana_wordexam_close(kana_wordexam* _exam);

// How many the setup makes: its length of the words it can ask.
u32  kana_wordexam_planned(const kana_wordexam* _exam);
void kana_wordexam_start(kana_wordexam* _exam);
void kana_wordexam_undo(kana_wordexam* _exam);
void kana_wordexam_clear(kana_wordexam* _exam);
// On to the next word (the last: the results).
void kana_wordexam_next(kana_wordexam* _exam);
b8   kana_wordexam_at_last(const kana_wordexam* _exam);
b8   kana_wordexam_graded(const kana_wordexam* _exam);
u32  kana_wordexam_wrong(const kana_wordexam* _exam);
// The wrong ones again, straight to writing.
void kana_wordexam_retry_wrong(kana_wordexam* _exam);
// A result tapped: its word (id); false when none was. Once per tap.
b8   kana_wordexam_take_tap(kana_wordexam* _exam, u32* _word);

void kana_wordexam_pointer_down(kana_wordexam* _exam, rde_vec_2F _screen, b8 _pen, f64 _time);
void kana_wordexam_pointer_moved(kana_wordexam* _exam, rde_vec_2F _screen, f64 _time);
void kana_wordexam_pointer_up(kana_wordexam* _exam, f64 _time);
// Once a frame: answers read (one box at a time), the reviews moved when all are.
void kana_wordexam_update(kana_wordexam* _exam, f32 _dt);
void kana_wordexam_render(kana_wordexam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

#endif
