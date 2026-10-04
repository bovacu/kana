// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_EXAMLOG
#define FUDE_EXAMLOG

#include "rde.h"
#include "drawing/ink/ink.h"
#include "study/models/history.h"

// ===========================================================================
// Every exam taken (exam.h), for the statistics and for study marks: when, what
// it was of, and per character whether it was written right, its points and
// how well it was written — and the writing itself, kept for looking back.
//
// One file (<save dir>/exams.kana), only ever added to: each exam appends a
// chunk (the file rewritten atomically, see kfile.h). Opening loads every exam's
// results (not the drawings) into memory; exams are few, their results small.
// The drawings are read back from the file when asked for (the album).
//
// FILE ('KANA' header, kind 'EXAM'), one chunk per exam, oldest first:
//   'EXAM'  u32 time low, u32 time high (Unix seconds, when it finished),
//           u8 source (FUDE_EXAM_SOURCE_), 3 reserved,
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
    u8  source;        // FUDE_EXAM_SOURCE_
    u32 first_item;    // into fude_examlog_items()
    u32 item_count;
    u32 correct;
    f32 score;         // the average points, 0..100
    u32 chunk;         // which of the file's exam chunks it is (UINT32_MAX: not in the file)
} fude_examlog_exam;

RDE_STRUCT {
    u32 codepoint;
    b8  correct;       // recognition had it among its first candidates
    f32 score;         // points: quality when correct, 0 when not
    f32 quality;       // how well it was written (score.h), whatever it read as
    u32 read_as;       // the code point recognition put first (0: none)
} fude_examlog_item;

// Loads every exam from _path (missing: none yet), and appends there from now
// on. Without it, exams are kept in memory only (the tests).
void fude_examlog_open(const c8* _path);
void fude_examlog_close(void);

// One exam, its items and their drawings (_drawings[i] may be NULL: nothing
// written; _units: the square's side in the drawings' units). False when the
// file could not be written (it is kept in memory all the same).
b8   fude_examlog_add(u64 _time, u8 _source, const fude_examlog_item* _items, u32 _count, const fude_ink* const* _drawings, f32 _units);

u32                      fude_examlog_count(void);
const fude_examlog_exam* fude_examlog_exams(void);   // oldest first
const fude_examlog_item* fude_examlog_items(void);

// The exams in a row, latest first, that had _codepoint and got it right (0
// when the latest with it got it wrong, or none had it).
u32  fude_examlog_streak(u32 _codepoint);

// Goes up with every exam added.
u32  fude_examlog_revision(void);

// Answers' writing, read back from the file: item `item` of exam `exam`, its
// strokes [first_stroke, +stroke_count) of _strokes.
RDE_STRUCT {
    u32 exam;          // into fude_examlog_exams()
    u32 item;          // within the exam
    u32 first_stroke;
    u32 stroke_count;
} fude_examlog_writing;

// Seconds between two points of read-back writing: the log keeps no timing, so
// a replay draws at this steady pace.
#define FUDE_EXAMLOG_POINT_SECONDS (1.0f / 60.0f)

// The writing of exam _exam's answers (_exam UINT32_MAX: every exam), only those
// asking for _codepoint (0: all of them), appended to _writings (oldest exam
// first, in the order asked), their strokes to _strokes and points to _points
// (history.h's: a fraction of the square times 65535, Y up). How many writings;
// none when the exams live in memory only, or the file cannot be read.
u32  fude_examlog_read_writing(u32 _exam, u32 _codepoint, rde_arr* _writings, rde_arr* _strokes, rde_arr* _points);

#endif
