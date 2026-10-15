// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_EXAM
#define FUDE_EXAM

#include "rde.h"
#include "lang/lang.h"
#include "study/chars/kanji.h"
#include "study/chars/catalog.h"
#include "study/chars/glyph.h"
#include "drawing/ink/ink.h"
#include "drawing/widgets/scroll.h"
#include "study/handwriting/recognize.h"

// ===========================================================================
// Exams: characters written from memory, with no help, each once, and then marked.
//
//   SETUP    choose what it is of (the characters being studied, the known ones, a JLPT level, hiragana, katakana,
//            or what Select mode ticked) and how many (10, 20, 50 or all), shuffled.
//   PREVIEW  every character chosen, ticked: untick the ones to leave out.
//   WRITING  one at a time, a prompt and a blank square. For a kanji the prompt is its meaning, its readings, and one
//            of its example words with it blanked out (□生 がくせい "student"); for a kana it is its romaji. Each is
//            written once, then Next.
//   RESULTS  each character, the writing, whether it was right and its points. The wrong ones can be tried again,
//            or practised.
//
// A KEPT exam (opened from the album) opens straight on its results, as they were: the writing is read back from the
// log (examlog.h), and nothing is read or kept again.
//
// MARKING, per character: it is RIGHT when recognition (ML Kit, with the app's matcher behind it: recognize.h) has
// it among its first FUDE_EXAM_CANDIDATES candidates, so a messy but right character still counts. Its POINTS are
// then how well it was written (score.h's score, as in Practice), and 0 when it is wrong. The exam is PASSED with
// FUDE_EXAM_PASS of them right. Answers are read while the next one is written; the results wait for the last.
//
// When it is finished it is kept (examlog.h), and the marks follow (marks.h): three exams in a row with a character
// right make it Known, and a Known one that is wrong is Studying again. Every answer also moves its character's
// review schedule (review.h).
//
// Where it sits: a study screen (study/screens). The side panel's Exams, Select mode, Reviews (study/app/verbs.c)
// and the album open it. The study shell (study/app/shell.c) owns the one exam and places FUDE_EXAM_SCREEN in the
// app's screen table.
// Its main types and their lifecycle: fude_exam, made with fude_exam_init over the character data and Browse's
// catalog (borrowed, not owned) and freed with fude_exam_destroy when the app ends. It holds every answer's ink.
// Threads: main thread only. ML Kit's answers are asked for and then polled once a frame (recognize.h).
// Files: through the models only: it adds to the exam log, sets marks and moves reviews when an exam is finished,
// and reads a kept exam's writing back from the log.
// Design: an answer is read one at a time, one a frame for the matcher, because reading takes a moment. ML Kit is
// given 4 seconds an answer before the matcher decides alone.
// ===========================================================================

// The most characters in one exam.
#define FUDE_EXAM_MAX          100u
#define FUDE_EXAM_CANDIDATES   3u        // An answer is right when among this many first candidates.
#define FUDE_EXAM_PASS         0.8f      // An exam is passed with this share right.
#define FUDE_EXAM_KNOWN_STREAK 3u        // Exams in a row right that make a character Known.
#define FUDE_EXAM_UNITS        1000.0f   // The answer square's side, in its ink's units.

// What an exam is of: the learner's marks, each of the language's levels (lang.h, easiest first), each of its sets
// (the groups learnt whole, by their core characters: for Japanese, hiragana and katakana), a selection given, or the
// reviews due, numbered in that order. For Japanese: 0 studying, 1 known, 2-6 N5..N1, 7 hiragana, 8 katakana,
// 9 selection, 10 review. The exam log keeps these numbers (examlog.h).
typedef u32 FUDE_EXAM_SOURCE_;
#define FUDE_EXAM_SOURCE_STUDYING 0u
#define FUDE_EXAM_SOURCE_KNOWN    1u
#define FUDE_EXAM_SOURCE_MAX      (4u + FUDE_LANG_LEVELS + FUDE_LANG_GROUPS)   // The most sources there can be.
// The source numbers of the language's _level-th level, of its _set-th set, of a selection given and of the reviews
// due; and how many sources there are.
FUDE_EXAM_SOURCE_ fude_exam_source_level(u32 _level);
FUDE_EXAM_SOURCE_ fude_exam_source_set(u32 _set);     // The _set-th of the groups that are sets.
FUDE_EXAM_SOURCE_ fude_exam_source_selection(void);
FUDE_EXAM_SOURCE_ fude_exam_source_review(void);      // What review.h has due (the side panel's Reviews; never a chip).
u32               fude_exam_source_count(void);

typedef enum {
    FUDE_EXAM_SETUP = 0,
    FUDE_EXAM_PREVIEW,
    FUDE_EXAM_WRITING,
    FUDE_EXAM_RESULTS
} FUDE_EXAM_STAGE_;

#define FUDE_EXAM_LENGTHS 4u   // The lengths to choose from: 10, 20, 50 and all.

// One character of an exam, and its answer.
RDE_STRUCT {
    u32      record;
    b8       included;   // Ticked in the preview.
    b8       answered;   // Moved past (with nothing written it is wrong).
    b8       graded;
    b8       correct;
    f32      score;      // The points: the quality when correct, 0 when not.
    f32      quality;    // How well it was written (score.h), whatever it was read as.
    u32      read_as;    // The record recognition put first (UINT32_MAX when none).
    fude_ink ink;        // The answer, in a FUDE_EXAM_UNITS square, Y up.
} fude_exam_item;

// A chip on the setup screen, as laid out for the last frame (screen space).
RDE_STRUCT {
    rde_vec_2F min;
    rde_vec_2F max;
    u8         kind;     // 0 for a source, 1 for a length.
    u8         value;
} fude_exam_chip;

RDE_STRUCT {
    const fude_kanji_db* db;
    const fude_catalog*  catalog;     // Borrowed (Browse's): for the filters and recognition.
    fude_glyph           glyph;
    b8                   open;
    FUDE_EXAM_STAGE_     stage;

    FUDE_EXAM_SOURCE_    source;
    u32                  length;      // Its index into the lengths.
    rde_arr TYPE(u32)    selection;   // Select mode's ticks (or the reviews due), when opened with them.

    fude_exam_item       items[FUDE_EXAM_MAX];
    u32                  count;
    u32                  order[FUDE_EXAM_MAX];   // While writing: the included items, in the order asked.
    u32                  asked;                  // How many are in order.
    u32                  current;                // The one being written, an index into order.

    fude_recognition     recognition; // ML Kit reading an answer.
    u32                  grading;     // The item it is reading (UINT32_MAX: none).
    f64                  grading_since; // When ML Kit was asked for it (the engine's clock).
    b8                   saved;       // The results are in the log.
    u32                  kept;        // The kept exam shown (examlog.h), or UINT32_MAX for one just taken.
    b8                   pen;         // The pen is writing in the square.
    rde_random           rng;         // The shuffle's generator (RDE's), seeded as the screen opens.

    // The layout of the last frame, and taps (screen space).
    fude_scroller        scroller;    // The preview's and the results' grid; the setup's taps.
    fude_exam_chip       chips[FUDE_EXAM_SOURCE_MAX + FUDE_EXAM_LENGTHS];
    u32                  chip_count;
    rde_vec_2F           grid_min;
    rde_vec_2F           grid_max;
    f32                  cell;
    u32                  columns;
    f32                  content_h;
    rde_vec_2F           square_tl;
    f32                  square_size;
    i32                  tapped;      // In the results, an item tapped, for the viewer (-1: none).
} fude_exam;

// Makes the exam over the character data and Browse's catalog (both borrowed); destroy frees it.
void fude_exam_init(fude_exam* _exam, const fude_kanji_db* _db, const fude_catalog* _catalog);
void fude_exam_destroy(fude_exam* _exam);

// Opens on the setup, to choose. It does not open without character data.
void fude_exam_open(fude_exam* _exam);
// Opens kept exam _index (examlog.h) on its results. Returns false when there is no such exam or no character data.
b8   fude_exam_open_kept(fude_exam* _exam, u32 _index);
// Opens on Select mode's ticks (copied), straight to the preview of all of them, shuffled.
void fude_exam_open_with(fude_exam* _exam, const u32* _records, u32 _count);
// Opens on the reviews due (review.h; copied), straight to writing them: what is due is what is asked.
void fude_exam_open_review(fude_exam* _exam, const u32* _records, u32 _count);
// Closes it. An exam not finished yet is not kept.
void fude_exam_close(fude_exam* _exam);

// SETUP: how many characters source _source has.
u32  fude_exam_source_size(const fude_exam* _exam, FUDE_EXAM_SOURCE_ _source);
// How many characters the choice would ask: the source's, cut to the length chosen and to FUDE_EXAM_MAX.
u32  fude_exam_planned(const fude_exam* _exam);
// On to the preview: the source's characters, shuffled, cut to the length.
void fude_exam_preview(fude_exam* _exam);

// PREVIEW: tick every character (_on) or none; how many are ticked; and start writing them (with none ticked,
// nothing happens).
void fude_exam_tick_all(fude_exam* _exam, b8 _on);
u32  fude_exam_included(const fude_exam* _exam);
void fude_exam_start(fude_exam* _exam);
// Back to the setup.
void fude_exam_back(fude_exam* _exam);

// WRITING: Undo takes back the answer's last stroke, Clear empties it (neither while the pen is down), and Next marks
// it answered and moves on (after the last, to the results). fude_exam_at_last says whether the one being written is
// the last.
void fude_exam_undo(fude_exam* _exam);
void fude_exam_clear(fude_exam* _exam);
void fude_exam_next(fude_exam* _exam);
b8   fude_exam_at_last(const fude_exam* _exam);

// RESULTS: whether every answer has been read; the wrong ones' records, at most _max into _out (returns how many);
// and the wrong ones again as an exam of their own, shuffled, straight to writing (once every answer is read).
b8   fude_exam_graded(const fude_exam* _exam);
u32  fude_exam_wrong(const fude_exam* _exam, u32* _out, u32 _max);
void fude_exam_retry_wrong(fude_exam* _exam);
// Whether a result was tapped; if so its record goes into *_record. Each tap is handed out once.
b8   fude_exam_take_tap(fude_exam* _exam, u32* _record);
// The exam's characters in the order asked (for the viewer to walk), at most _max into _out. Returns how many.
u32  fude_exam_asked(const fude_exam* _exam, u32* _out, u32 _max);

// The pointer, in screen space. While writing, only the pen writes, and only in the answer square (a resting hand
// does nothing); at the other stages any pointer taps and scrolls.
void fude_exam_pointer_down(fude_exam* _exam, rde_vec_2F _screen, b8 _pen, f64 _time);
void fude_exam_pointer_moved(fude_exam* _exam, rde_vec_2F _screen, f64 _time);
void fude_exam_pointer_up(fude_exam* _exam, f64 _time);

// Once a frame while open: scrolls and takes taps, reads the answers, and keeps the finished exam (the log, the
// marks, the reviews) once every answer is read. fude_exam_render draws the stage between _top and _bottom (screen
// y), inside a 2D drawing block.
void fude_exam_update(fude_exam* _exam, f32 _dt);
void fude_exam_render(fude_exam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// A source's name ("N5", "Studying"), in the app's language; "" for a source that does not exist. The string stays
// valid until the language changes.
const c8* fude_exam_source_name(FUDE_EXAM_SOURCE_ _source);

// The exam as a screen (screen.h), with one row per stage: the setup's Close and "Next n"; the preview's Back, All,
// None and "Start n"; the writing's Quit, Undo, Clear and Next (Finish on the last); the results' Done, Retry wrong
// and Practice wrong (the wrong ones as a practice set). A character tapped in the results opens the viewer on it,
// walking the exam's characters.
extern const struct fude_screen FUDE_EXAM_SCREEN;

#endif
