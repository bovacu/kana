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
// Word exams: the learner's words (vocab.h) written from memory, whole, in a box for each character, kana and all.
//
//   SETUP    choose how many (10, 20, 50 or all of them, shuffled) and how each is asked: by its MEANING and
//            READING, or by EAR (said aloud, with its meaning under it; only where there is a voice, speech.h).
//   WRITING  one word at a time: the prompt, then the word's boxes. A character the data cannot draw (a digit, a
//            Latin letter) is shown in its box, given. Undo, Clear, and Next (Finish on the last).
//   RESULTS  each word: what was written, small, and whether it was right; a tap opens the word card (wordcard.h).
//            The wrong ones can be tried again.
//
// Reviews (review.h) open it straight on writing what is due, by meaning and reading.
//
// MARKING, per box, the way exams mark a character: a box is right when recognition (ML Kit, with the app's matcher
// behind it) has its character among its first three candidates (as FUDE_EXAM_CANDIDATES); an empty box is wrong. A
// word is right when every box is. Every word answered moves its review schedule (review.h, under its
// FUDE_VOCAB_KEY), with how well it was written taken as its written boxes' points together.
//
// Where it sits: a study screen (study/screens), opened by the Vocabulary screen's Exam and Review (vocabview.h).
// The study shell (study/app/shell.c) owns the one word exam and places FUDE_WORDEXAM_SCREEN in the app's screen
// table.
// Its main types and their lifecycle: fude_wordexam, made with fude_wordexam_init over the character data and
// Browse's catalog (borrowed, not owned) and freed with fude_wordexam_destroy when the app ends. It holds every box's
// ink.
// Threads: main thread only. ML Kit's answers are asked for and then polled once a frame (recognize.h).
// Files: none of its own; it moves review schedules (review.h) once every answer is read.
// Design: a box holds a whole syllable where the script writes signs on a letter (Thai's and Hindi's vowel signs and
// tone marks, Arabic's harakat), and a letter in the form it takes there (Arabic's joined forms). ML Kit reads such a
// syllable whole; the matcher knows letters only, so without ML Kit only the letter is checked, for fewer points.
// ===========================================================================

// The most words in one exam.
#define FUDE_WORDEXAM_MAX    50u
#define FUDE_WORDEXAM_CHARS  8u        // A word longer than this is not asked.
#define FUDE_WORDEXAM_UNITS  1000.0f   // A box's side, in its ink's units.

typedef enum {
    FUDE_WORDEXAM_SETUP = 0,
    FUDE_WORDEXAM_WRITING,
    FUDE_WORDEXAM_RESULTS
} FUDE_WORDEXAM_STAGE_;

typedef enum {
    FUDE_WORDEXAM_BY_MEANING = 0,   // Its meaning and reading are shown.
    FUDE_WORDEXAM_BY_EAR            // It is said aloud, and its meaning shown.
} FUDE_WORDEXAM_BY_;

// One word of an exam, and its answer.
RDE_STRUCT {
    u32      word;                              // Its id (vocab.h).
    u32      count;                             // How many characters (boxes) it has.
    u32      chars[FUDE_WORDEXAM_CHARS];        // Each box's letter, as a code point, in the form written there.
    c8       written[FUDE_WORDEXAM_CHARS][24];  // Each box's text, UTF-8: its letter and any signs on it (Thai, Hindi).
    u32      records[FUDE_WORDEXAM_CHARS];      // The data's records (UINT32_MAX: given, not written).
    fude_ink ink[FUDE_WORDEXAM_CHARS];          // A box each, in a FUDE_WORDEXAM_UNITS square, Y up.
    b8       box_graded[FUDE_WORDEXAM_CHARS];
    b8       box_right[FUDE_WORDEXAM_CHARS];
    f32      box_points[FUDE_WORDEXAM_CHARS];   // Each box's points (score.h's), 0..100.
    b8       answered;
    b8       graded;
    b8       correct;
    f32      points;                            // The written boxes' points together (0..100).
} fude_wordexam_item;

// A chip on the setup, as laid out last frame (screen space).
RDE_STRUCT {
    rde_vec_2F min;
    rde_vec_2F max;
    u8         kind;    // 0 for how many, 1 for how it is asked.
    u8         value;
} fude_wordexam_chip;

RDE_STRUCT {
    const fude_kanji_db* db;
    const fude_catalog*  catalog;
    fude_glyph           glyph;
    b8                   open;
    FUDE_WORDEXAM_STAGE_ stage;
    FUDE_WORDEXAM_BY_    by;
    u32                  length;                    // Its index into the lengths (10, 20, 50, all).
    b8                   review;                    // It was opened by Reviews.
    c8                   title[256];                // What it is of (a list's name, Vocabulary, Reviews).

    rde_arr TYPE(u32)    words;                     // What it can ask (word ids).
    fude_wordexam_item   items[FUDE_WORDEXAM_MAX];
    u32                  count;                     // How many are asked.
    u32                  current;

    fude_recognition     recognition;
    u32                  grading_item;              // The word whose box ML Kit is reading (UINT32_MAX: none).
    u32                  grading_box;
    f64                  grading_since;
    b8                   kept;                      // The reviews have moved.
    b8                   pen;                       // While writing: the pen is in a box.
    u32                  pen_box;
    rde_arr TYPE(u32)    strokes;                   // The word's strokes' boxes, in the order written (for Undo).
    rde_random           rng;                       // The shuffle's generator (RDE's), seeded as the screen opens.
    b8                   spoken;                    // By ear: the current word has been said once.

    // The layout of the last frame (screen space).
    fude_scroller        scroller;
    fude_wordexam_chip   chips[8];
    u32                  chip_count;
    rde_vec_2F           boxes_tl;
    f32                  box;
    f32                  box_gap;
    rde_vec_2F           speaker_min;               // By ear: a tap here says it again.
    rde_vec_2F           speaker_max;
    rde_vec_2F           rows_min;                  // The results' rows.
    rde_vec_2F           rows_max;
    f32                  content_h;
    i32                  tapped;                    // Results: a word tapped (an item), for the owner (-1: none).
} fude_wordexam;

// Makes the word exam over the character data and Browse's catalog (both borrowed); destroy frees it.
void fude_wordexam_init(fude_wordexam* _exam, const fude_kanji_db* _db, const fude_catalog* _catalog);
void fude_wordexam_destroy(fude_wordexam* _exam);
// Opens the setup for the words _ids (copied); _title names what they are. It does not open without character data.
void fude_wordexam_open(fude_wordexam* _exam, const u32* _ids, u32 _count, const c8* _title);
// Opens on the reviews due: _ids (copied), in their order, straight to writing them, by meaning and reading.
void fude_wordexam_open_review(fude_wordexam* _exam, const u32* _ids, u32 _count);
// Closes it. If the answers have not all been read yet, the reviews do not move.
void fude_wordexam_close(fude_wordexam* _exam);

// How many words the setup's choice makes: its length, out of the words that can be asked (a word that is gone, too
// long, or has nothing to write cannot be). Then start them, shuffled, straight to writing.
u32  fude_wordexam_planned(const fude_wordexam* _exam);
void fude_wordexam_start(fude_wordexam* _exam);
// While writing: Undo takes back the last stroke, whichever box it went in; Clear empties every box of the word.
void fude_wordexam_undo(fude_wordexam* _exam);
void fude_wordexam_clear(fude_wordexam* _exam);
// On to the next word (after the last, to the results); and whether the word being written is the last.
void fude_wordexam_next(fude_wordexam* _exam);
b8   fude_wordexam_at_last(const fude_wordexam* _exam);
// Whether every answer has been read, and how many words are wrong.
b8   fude_wordexam_graded(const fude_wordexam* _exam);
u32  fude_wordexam_wrong(const fude_wordexam* _exam);
// The wrong ones again, shuffled, straight to writing (once every answer is read).
void fude_wordexam_retry_wrong(fude_wordexam* _exam);
// Whether a result was tapped; if so its word's id goes into *_word. Each tap is handed out once.
b8   fude_wordexam_take_tap(fude_wordexam* _exam, u32* _word);

// The pointer, in screen space. While writing, only the pen writes, and only in a box that is not given (a resting
// hand does nothing); a tap on the speaker says the word again. At the other stages any pointer taps and scrolls.
void fude_wordexam_pointer_down(fude_wordexam* _exam, rde_vec_2F _screen, b8 _pen, f64 _time);
void fude_wordexam_pointer_moved(fude_wordexam* _exam, rde_vec_2F _screen, f64 _time);
void fude_wordexam_pointer_up(fude_wordexam* _exam, f64 _time);
// Once a frame while open: takes taps, says each word as it comes up (by ear), reads the answers one box at a time,
// and moves the reviews once all are read. fude_wordexam_render draws the stage between _top and _bottom (screen
// y), inside a 2D drawing block.
void fude_wordexam_update(fude_wordexam* _exam, f32 _dt);
void fude_wordexam_render(fude_wordexam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The word exam as a screen (screen.h), with one row per stage: the setup's Close and "Start n"; the writing's Quit,
// Undo, Clear and Next (Finish on the last); the results' Done and Retry wrong. A word tapped in the results opens
// the word card.
extern const struct fude_screen FUDE_WORDEXAM_SCREEN;

#endif
