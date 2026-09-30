#ifndef KANA_EXAMLOG
#define KANA_EXAMLOG

#include "rde.h"
#include "ink.h"

// ===========================================================================
// Every exam taken (exam.h), for the statistics and for study marks: when, what
// it was of, and per character whether it was written right, its points and
// how well it was written — and the writing itself, kept for looking back.
//
// One file (<save dir>/exams.kana), only ever added to: each exam appends a
// chunk (the file rewritten atomically, see kfile.h). Opening loads every exam's
// results (not the drawings) into memory; exams are few, their results small.
//
// FILE ('KANA' header, kind 'EXAM'), one chunk per exam, oldest first:
//   'EXAM'  u32 time low, u32 time high (Unix seconds, when it finished),
//           u8 source (KANA_EXAM_SOURCE_), 3 reserved,
//           u32 count, u32 item record size (24), then per item:
//             u32 code point, u8 correct, 3 reserved, f32 score (points: the
//             quality when correct, 0 when not), f32 quality, u32 read as (the
//             code point recognition put first; 0: none), u32 reserved
//           then per item its drawing: u16 strokes, per stroke u16 points and
//           the points as u16 x, u16 y (a fraction of the square times 65535,
//           Y up).
// ===========================================================================

RDE_STRUCT {
    u64 time;          // when it finished (Unix seconds)
    u8  source;        // KANA_EXAM_SOURCE_
    u32 first_item;    // into kana_examlog_items()
    u32 item_count;
    u32 correct;
    f32 score;         // the average points, 0..100
} kana_examlog_exam;

RDE_STRUCT {
    u32 codepoint;
    b8  correct;       // recognition had it among its first candidates
    f32 score;         // points: quality when correct, 0 when not
    f32 quality;       // how well it was written (score.h), whatever it read as
    u32 read_as;       // the code point recognition put first (0: none)
} kana_examlog_item;

// Loads every exam from _path (missing: none yet), and appends there from now
// on. Without it, exams are kept in memory only (the tests).
void kana_examlog_open(const c8* _path);
void kana_examlog_close(void);

// One exam, its items and their drawings (_drawings[i] may be NULL: nothing
// written; _units: the square's side in the drawings' units). False when the
// file could not be written (it is kept in memory all the same).
b8   kana_examlog_add(u64 _time, u8 _source, const kana_examlog_item* _items, u32 _count, const kana_ink* const* _drawings, f32 _units);

u32                      kana_examlog_count(void);
const kana_examlog_exam* kana_examlog_exams(void);   // oldest first
const kana_examlog_item* kana_examlog_items(void);

// The exams in a row, latest first, that had _codepoint and got it right (0
// when the latest with it got it wrong, or none had it).
u32  kana_examlog_streak(u32 _codepoint);

// Goes up with every exam added.
u32  kana_examlog_revision(void);

#endif
