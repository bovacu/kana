#ifndef FUDE_EXAM
#define FUDE_EXAM

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/catalog.h"
#include "study/chars/glyph.h"
#include "drawing/ink/ink.h"
#include "drawing/widgets/scroll.h"
#include "study/handwriting/recognize.h"

// ===========================================================================
// Exams: characters written from memory, no help, each once — and marked.
//
//   SETUP    what it is of — the characters being studied, the known ones,
//            a JLPT level, hiragana, katakana, or what Select mode ticked —
//            and how many (10, 20, 50 or all), shuffled.
//   PREVIEW  every one of them, ticked: untick the ones to leave out.
//   WRITING  one at a time, a prompt and a blank square. A kanji: its meaning,
//            its readings, and one of its example words with it blanked out
//            (□生 がくせい "student"); a kana: its romaji. Written once, then Next.
//   RESULTS  each character, the writing, right or not and its points; the
//            wrong ones can be tried again, or practised.
//
// A KEPT exam (the album's) opens straight on its results, as they were: the
// writing read back from the log (examlog.h), nothing read or kept again.
//
// MARKING, per character: RIGHT when recognition (ML Kit, with Kana's matcher
// behind it — recognize.h) has it among its first FUDE_EXAM_CANDIDATES
// candidates — a messy but right character still counts; its POINTS are then
// how well it was written (score.h's score, as in Practice), and 0 when wrong.
// The exam is PASSED with FUDE_EXAM_PASS of them right. Answers are read while
// the next is written; the results wait for the last.
//
// Finished, it is kept (examlog.h), and marks follow (marks.h): three exams in
// a row with a character right make it Known; a Known one wrong is Studying
// again.
// ===========================================================================

#define FUDE_EXAM_MAX          100u
#define FUDE_EXAM_CANDIDATES   3u        // right when among this many first candidates
#define FUDE_EXAM_PASS         0.8f      // passed with this share right
#define FUDE_EXAM_KNOWN_STREAK 3u        // exams in a row right: Known
#define FUDE_EXAM_UNITS        1000.0f   // the answer square's side, in its ink's units

typedef enum {
    FUDE_EXAM_SOURCE_STUDYING = 0,
    FUDE_EXAM_SOURCE_KNOWN,
    FUDE_EXAM_SOURCE_N5,
    FUDE_EXAM_SOURCE_N4,
    FUDE_EXAM_SOURCE_N3,
    FUDE_EXAM_SOURCE_N2,
    FUDE_EXAM_SOURCE_N1,
    FUDE_EXAM_SOURCE_HIRAGANA,
    FUDE_EXAM_SOURCE_KATAKANA,
    FUDE_EXAM_SOURCE_SELECTION,
    FUDE_EXAM_SOURCE_REVIEW,       // what review.h has due (the side panel's Reviews; never a chip)
    FUDE_EXAM_SOURCE_COUNT
} FUDE_EXAM_SOURCE_;

typedef enum {
    FUDE_EXAM_SETUP = 0,
    FUDE_EXAM_PREVIEW,
    FUDE_EXAM_WRITING,
    FUDE_EXAM_RESULTS
} FUDE_EXAM_STAGE_;

#define FUDE_EXAM_LENGTHS 4u   // 10, 20, 50, all

RDE_STRUCT {
    u32      record;
    b8       included;   // ticked in the preview
    b8       answered;   // moved past (with nothing written: wrong)
    b8       graded;
    b8       correct;
    f32      score;      // points: quality when correct, 0 when not
    f32      quality;    // how well it was written (score.h), whatever it read as
    u32      read_as;    // the record recognition put first (UINT32_MAX: none)
    fude_ink ink;        // the answer: FUDE_EXAM_UNITS square, Y up
} fude_exam_item;

// A chip on the setup screen, as laid out for the last frame (screen space).
RDE_STRUCT {
    rde_vec_2F min;
    rde_vec_2F max;
    u8         kind;     // 0: a source, 1: a length
    u8         value;
} fude_exam_chip;

RDE_STRUCT {
    const fude_kanji_db* db;
    const fude_catalog*  catalog;     // borrowed (Browse's): filters and recognition
    fude_glyph           glyph;
    b8                   open;
    FUDE_EXAM_STAGE_     stage;

    FUDE_EXAM_SOURCE_    source;
    u32                  length;      // index into the lengths
    rde_arr TYPE(u32)    selection;   // Select mode's ticks, when opened with them

    fude_exam_item       items[FUDE_EXAM_MAX];
    u32                  count;
    u32                  order[FUDE_EXAM_MAX];   // writing: the included items, in the order asked
    u32                  asked;                  // how many in order
    u32                  current;                // into order

    fude_recognition     recognition; // ML Kit reading an answer
    u32                  grading;     // the item it reads (UINT32_MAX: none)
    b8                   saved;       // the results are in the log
    u32                  kept;        // the kept exam shown (examlog.h), or UINT32_MAX: one just taken
    b8                   pen;         // the pen is writing in the square
    u32                  rng;         // the shuffle's state

    // Layout of the last frame, and taps (screen space).
    fude_scroller        scroller;    // the preview's and results' grid; the setup's taps
    fude_exam_chip       chips[FUDE_EXAM_SOURCE_COUNT + FUDE_EXAM_LENGTHS];
    u32                  chip_count;
    rde_vec_2F           grid_min;
    rde_vec_2F           grid_max;
    f32                  cell;
    u32                  columns;
    f32                  content_h;
    rde_vec_2F           square_tl;
    f32                  square_size;
    i32                  tapped;      // results: a tapped item, for the viewer (-1: none)
} fude_exam;

void fude_exam_init(fude_exam* _exam, const fude_kanji_db* _db, const fude_catalog* _catalog);
void fude_exam_destroy(fude_exam* _exam);

// The setup, to choose.
void fude_exam_open(fude_exam* _exam);
// Kept exam _index (examlog.h), on its results. False when there is no such exam.
b8   fude_exam_open_kept(fude_exam* _exam, u32 _index);
// Select mode's ticks: straight to the preview of them.
void fude_exam_open_with(fude_exam* _exam, const u32* _records, u32 _count);
// Reviews (review.h): what is due, straight to writing them.
void fude_exam_open_review(fude_exam* _exam, const u32* _records, u32 _count);
void fude_exam_close(fude_exam* _exam);

// SETUP: how many characters a source has; the choice.
u32  fude_exam_source_size(const fude_exam* _exam, FUDE_EXAM_SOURCE_ _source);
// The characters it would ask (the source, cut to the length).
u32  fude_exam_planned(const fude_exam* _exam);
// On to the preview: the source's characters, shuffled, cut to the length.
void fude_exam_preview(fude_exam* _exam);

// PREVIEW: every one ticked or none; how many are; start with them.
void fude_exam_tick_all(fude_exam* _exam, b8 _on);
u32  fude_exam_included(const fude_exam* _exam);
void fude_exam_start(fude_exam* _exam);
// Back to the setup.
void fude_exam_back(fude_exam* _exam);

// WRITING: the answer in the square; Next marks it answered and moves on (after
// the last, the results).
void fude_exam_undo(fude_exam* _exam);
void fude_exam_clear(fude_exam* _exam);
void fude_exam_next(fude_exam* _exam);
b8   fude_exam_at_last(const fude_exam* _exam);

// RESULTS: all read yet; the wrong ones (records, at most _max), again as an
// exam of their own (straight to writing).
b8   fude_exam_graded(const fude_exam* _exam);
u32  fude_exam_wrong(const fude_exam* _exam, u32* _out, u32 _max);
void fude_exam_retry_wrong(fude_exam* _exam);
// A result tapped: its record, once per tap.
b8   fude_exam_take_tap(fude_exam* _exam, u32* _record);
// The exam's characters in the order asked (for the viewer), at most _max.
u32  fude_exam_asked(const fude_exam* _exam, u32* _out, u32 _max);

// The pointer (screen space). The pen writes in the answer square; any pointer
// taps and scrolls the rest.
void fude_exam_pointer_down(fude_exam* _exam, rde_vec_2F _screen, b8 _pen, f64 _time);
void fude_exam_pointer_moved(fude_exam* _exam, rde_vec_2F _screen, f64 _time);
void fude_exam_pointer_up(fude_exam* _exam, f64 _time);

// Once a frame: answers read, the finished exam kept and marks updated.
void fude_exam_update(fude_exam* _exam, f32 _dt);
void fude_exam_render(fude_exam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// A source's name ("N5", "Studying").
const c8* fude_exam_source_name(FUDE_EXAM_SOURCE_ _source);

// The screen (screen.h): its rows of buttons, and what they do.
extern const struct fude_screen FUDE_EXAM_SCREEN;

#endif
