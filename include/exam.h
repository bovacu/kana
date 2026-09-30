#ifndef KANA_EXAM
#define KANA_EXAM

#include "rde.h"
#include "kanji.h"
#include "catalog.h"
#include "glyph.h"
#include "ink.h"
#include "scroll.h"
#include "recognize.h"

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
// MARKING, per character: RIGHT when recognition (ML Kit, with Kana's matcher
// behind it — recognize.h) has it among its first KANA_EXAM_CANDIDATES
// candidates — a messy but right character still counts; its POINTS are then
// how well it was written (score.h's score, as in Practice), and 0 when wrong.
// The exam is PASSED with KANA_EXAM_PASS of them right. Answers are read while
// the next is written; the results wait for the last.
//
// Finished, it is kept (examlog.h), and marks follow (marks.h): three exams in
// a row with a character right make it Known; a Known one wrong is Studying
// again.
// ===========================================================================

#define KANA_EXAM_MAX          100u
#define KANA_EXAM_CANDIDATES   3u        // right when among this many first candidates
#define KANA_EXAM_PASS         0.8f      // passed with this share right
#define KANA_EXAM_KNOWN_STREAK 3u        // exams in a row right: Known
#define KANA_EXAM_UNITS        1000.0f   // the answer square's side, in its ink's units

typedef enum {
    KANA_EXAM_SOURCE_STUDYING = 0,
    KANA_EXAM_SOURCE_KNOWN,
    KANA_EXAM_SOURCE_N5,
    KANA_EXAM_SOURCE_N4,
    KANA_EXAM_SOURCE_N3,
    KANA_EXAM_SOURCE_N2,
    KANA_EXAM_SOURCE_N1,
    KANA_EXAM_SOURCE_HIRAGANA,
    KANA_EXAM_SOURCE_KATAKANA,
    KANA_EXAM_SOURCE_SELECTION,
    KANA_EXAM_SOURCE_COUNT
} KANA_EXAM_SOURCE_;

typedef enum {
    KANA_EXAM_SETUP = 0,
    KANA_EXAM_PREVIEW,
    KANA_EXAM_WRITING,
    KANA_EXAM_RESULTS
} KANA_EXAM_STAGE_;

#define KANA_EXAM_LENGTHS 4u   // 10, 20, 50, all

RDE_STRUCT {
    u32      record;
    b8       included;   // ticked in the preview
    b8       answered;   // moved past (with nothing written: wrong)
    b8       graded;
    b8       correct;
    f32      score;      // points: quality when correct, 0 when not
    f32      quality;    // how well it was written (score.h), whatever it read as
    u32      read_as;    // the record recognition put first (UINT32_MAX: none)
    kana_ink ink;        // the answer: KANA_EXAM_UNITS square, Y up
} kana_exam_item;

// A chip on the setup screen, as laid out for the last frame (screen space).
RDE_STRUCT {
    rde_vec_2F min;
    rde_vec_2F max;
    u8         kind;     // 0: a source, 1: a length
    u8         value;
} kana_exam_chip;

RDE_STRUCT {
    const kana_kanji_db* db;
    const kana_catalog*  catalog;     // borrowed (Browse's): filters and recognition
    kana_glyph           glyph;
    b8                   open;
    KANA_EXAM_STAGE_     stage;

    KANA_EXAM_SOURCE_    source;
    u32                  length;      // index into the lengths
    rde_arr TYPE(u32)    selection;   // Select mode's ticks, when opened with them

    kana_exam_item       items[KANA_EXAM_MAX];
    u32                  count;
    u32                  order[KANA_EXAM_MAX];   // writing: the included items, in the order asked
    u32                  asked;                  // how many in order
    u32                  current;                // into order

    kana_recognition     recognition; // ML Kit reading an answer
    u32                  grading;     // the item it reads (UINT32_MAX: none)
    b8                   saved;       // the results are in the log
    b8                   pen;         // the pen is writing in the square
    u32                  rng;         // the shuffle's state

    // Layout of the last frame, and taps (screen space).
    kana_scroller        scroller;    // the preview's and results' grid; the setup's taps
    kana_exam_chip       chips[KANA_EXAM_SOURCE_COUNT + KANA_EXAM_LENGTHS];
    u32                  chip_count;
    rde_vec_2F           grid_min;
    rde_vec_2F           grid_max;
    f32                  cell;
    u32                  columns;
    f32                  content_h;
    rde_vec_2F           square_tl;
    f32                  square_size;
    i32                  tapped;      // results: a tapped item, for the viewer (-1: none)
} kana_exam;

void kana_exam_init(kana_exam* _exam, const kana_kanji_db* _db, const kana_catalog* _catalog);
void kana_exam_destroy(kana_exam* _exam);

// The setup, to choose.
void kana_exam_open(kana_exam* _exam);
// Select mode's ticks: straight to the preview of them.
void kana_exam_open_with(kana_exam* _exam, const u32* _records, u32 _count);
void kana_exam_close(kana_exam* _exam);

// SETUP: how many characters a source has; the choice.
u32  kana_exam_source_size(const kana_exam* _exam, KANA_EXAM_SOURCE_ _source);
// The characters it would ask (the source, cut to the length).
u32  kana_exam_planned(const kana_exam* _exam);
// On to the preview: the source's characters, shuffled, cut to the length.
void kana_exam_preview(kana_exam* _exam);

// PREVIEW: every one ticked or none; how many are; start with them.
void kana_exam_tick_all(kana_exam* _exam, b8 _on);
u32  kana_exam_included(const kana_exam* _exam);
void kana_exam_start(kana_exam* _exam);
// Back to the setup.
void kana_exam_back(kana_exam* _exam);

// WRITING: the answer in the square; Next marks it answered and moves on (after
// the last, the results).
void kana_exam_undo(kana_exam* _exam);
void kana_exam_clear(kana_exam* _exam);
void kana_exam_next(kana_exam* _exam);
b8   kana_exam_at_last(const kana_exam* _exam);

// RESULTS: all read yet; the wrong ones (records, at most _max), again as an
// exam of their own (straight to writing).
b8   kana_exam_graded(const kana_exam* _exam);
u32  kana_exam_wrong(const kana_exam* _exam, u32* _out, u32 _max);
void kana_exam_retry_wrong(kana_exam* _exam);
// A result tapped: its record, once per tap.
b8   kana_exam_take_tap(kana_exam* _exam, u32* _record);
// The exam's characters in the order asked (for the viewer), at most _max.
u32  kana_exam_asked(const kana_exam* _exam, u32* _out, u32 _max);

// The pointer (screen space). The pen writes in the answer square; any pointer
// taps and scrolls the rest.
void kana_exam_pointer_down(kana_exam* _exam, rde_vec_2F _screen, b8 _pen, f64 _time);
void kana_exam_pointer_moved(kana_exam* _exam, rde_vec_2F _screen, f64 _time);
void kana_exam_pointer_up(kana_exam* _exam, f64 _time);

// Once a frame: answers read, the finished exam kept and marks updated.
void kana_exam_update(kana_exam* _exam, f32 _dt);
void kana_exam_render(kana_exam* _exam, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// A source's name ("N5", "Studying").
const c8* kana_exam_source_name(KANA_EXAM_SOURCE_ _source);

#endif
