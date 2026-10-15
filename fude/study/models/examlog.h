// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_EXAMLOG
#define FUDE_EXAMLOG

#include "rde.h"
#include "drawing/ink/ink.h"
#include "study/models/history.h"

// ===========================================================================
// Exam log: every exam taken (exam.h), kept for the statistics and for the study marks. For each exam it keeps when
// it finished and what it was of, and for each character asked whether it was written right, its points and how well
// it was written, and the writing itself, so it can be looked at again later.
//
// Where it sits: a study model (study/models). The exam screen adds to it (exam.h), the album shows the exams and
// their writing (album.h), the statistics count them (stats.h), the viewer finds the learner's mix-ups in it
// (viewer.h), and the study's session opens the file (study/app/files.c).
// Its main types and their lifecycle: there is no object; the log is one set held by this module. fude_examlog_open
// loads it when the session opens and fude_examlog_close frees it when the app ends. Without fude_examlog_open, exams
// are kept in memory only, which is what the tests use. fude_examlog_exam and fude_examlog_item are read-only views
// into the module's arrays.
// Threads: main thread only.
// Files: one file, <save dir>/exams.kana, in the KANA format (kfile.h), read with fude_save_load (save.h: the main
// file, else its .bak). It is only ever added to: each exam appends one chunk, and the file is rewritten atomically
// with the old bytes kept as they were. Its layout:
//   header kind 'EXAM', then one chunk per exam, oldest first:
//   'EXAM'  u32 time low, u32 time high (when it finished, in Unix seconds), u8 source (FUDE_EXAM_SOURCE_),
//           3 reserved bytes, u32 count, u32 item record size (24), then per item:
//             u32 code point, u8 correct, 3 reserved bytes, f32 score (the points: the quality when correct, 0 when
//             not), f32 quality, u32 read as (the code point recognition put first; 0 when it read none),
//             u32 reserved
//           then, per item, its drawing: u16 strokes, and per stroke u16 points and the points as u16 x, u16 y (a
//           fraction of the square times 65535, Y up).
// Design: opening loads every exam's results into memory but not the drawings, since exams are few and their results
// small. The drawings are read back from the file only when asked for (the album, a kept exam's results).
// ===========================================================================

// One exam, as kept.
RDE_STRUCT {
    u64 time;          // When it finished, in Unix seconds.
    u8  source;        // What it was of: a FUDE_EXAM_SOURCE_ (exam.h).
    u32 first_item;    // Its first item in fude_examlog_items().
    u32 item_count;
    u32 correct;
    f32 score;         // The average points, 0..100.
    u32 chunk;         // Which of the file's exam chunks it is (UINT32_MAX when it is not in the file).
} fude_examlog_exam;

// One character of an exam, as kept.
RDE_STRUCT {
    u32 codepoint;
    b8  correct;       // Recognition had it among its first candidates.
    f32 score;         // The points: the quality when correct, 0 when not.
    f32 quality;       // How well it was written (score.h), whatever it was read as.
    u32 read_as;       // The code point recognition put first (0 when it read none).
} fude_examlog_item;

// Loads every exam from _path, and appends there from now on. When nothing has been saved yet the log starts empty.
// A NULL _path keeps the exams in memory only (the tests), as not opening the log at all does.
void fude_examlog_open(const c8* _path);
// Forgets every exam and the path, and frees their memory.
void fude_examlog_close(void);

// Adds one exam: its _count items (at most 256 are kept) and their drawings. _drawings may be NULL, and so may any
// _drawings[i], when nothing was written; _units is the answer square's side in the drawings' units. Returns false
// when the file could not be written; the exam is kept in memory all the same. In memory only, it returns true.
b8   fude_examlog_add(u64 _time, u8 _source, const fude_examlog_item* _items, u32 _count, const fude_ink* const* _drawings, f32 _units);

// The exams, oldest first, and all their items in one array (each exam's from its first_item). Both pointers are
// the module's: NULL before the log is opened, and valid only until the next exam is added or the log is opened or
// closed again.
u32                      fude_examlog_count(void);
const fude_examlog_exam* fude_examlog_exams(void);   // Oldest first.
const fude_examlog_item* fude_examlog_items(void);

// How many exams in a row, latest first, had _codepoint and got it right. It is 0 when the latest exam with it got
// it wrong, or when no exam had it.
u32  fude_examlog_streak(u32 _codepoint);

// A number that goes up with every exam added, so what shows the exams knows when to look again.
u32  fude_examlog_revision(void);

// One answer's writing, as read back from the file: item `item` of exam `exam`, whose strokes are
// [first_stroke, first_stroke + stroke_count) of the strokes array it was read into.
RDE_STRUCT {
    u32 exam;          // Its exam in fude_examlog_exams().
    u32 item;          // Its item within the exam.
    u32 first_stroke;
    u32 stroke_count;
} fude_examlog_writing;

// The time between two points of read-back writing, in seconds. The log keeps no timing, so a replay draws at this
// steady pace.
#define FUDE_EXAMLOG_POINT_SECONDS (1.0f / 60.0f)

// Reads back the writing of exam _exam's answers (UINT32_MAX for every exam), only those that asked for _codepoint
// (0 for all of them). Each writing is appended to _writings (fude_examlog_writing, oldest exam first, items in the
// order asked), its strokes to _strokes (fude_history_stroke) and its points to _points (fude_history_point: a
// fraction of the square times 65535, Y up). The caller owns the three arrays. A drawing the file has damaged adds
// nothing to any of them. Returns how many writings were found: 0 when the exams live in memory only or the file
// cannot be read.
u32  fude_examlog_read_writing(u32 _exam, u32 _codepoint, rde_arr* _writings, rde_arr* _strokes, rde_arr* _points);

#endif
