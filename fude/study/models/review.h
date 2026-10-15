// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_REVIEW
#define FUDE_REVIEW

#include "rde.h"

// ===========================================================================
// Reviews: spaced repetition for the characters being learnt. Each Studying or Known character (marks.h) has a
// schedule: the day it is next due, how many days the last step was, and how easily it comes (its EASE). Writing it
// from memory, in Reviews or in any exam, moves the schedule:
//
//   right, in time   the next step grows: 1 day, then 3, then the last step times the ease. A clean, well-written
//                    answer (its points from score.h) makes it easier; a shaky one makes it a little harder.
//   right, early     nothing changes, because the step was not waited for.
//   wrong            it is due again tomorrow, the steps start over, and it becomes harder.
//
// Reviews (the side panel) asks for what is due today, the longest waiting first, then a few new ones (characters
// marked but never reviewed), at most FUDE_REVIEW_NEW_PER_DAY a day, so that marking fifty at once does not make fifty
// due tomorrow. Days are the device's local days.
//
// WORDS (vocab.h) have schedules too, under keys above every code point (FUDE_VOCAB_KEY). They are written in word
// exams and reviewed from the Vocabulary screen, and their new ones have a daily allowance of their own.
//
// Where it sits: a study model (study/models). The exams (exam.h) and word exams (wordexam.h) answer it; the side
// panel's Reviews (study/app/verbs.c) and the Vocabulary screen (vocabview.h) ask what is due; the study's session
// opens the file (study/app/files.c).
// Its main types and their lifecycle: there is no object; the schedules are one set held by this module.
// fude_reviews_open loads them when the session opens and fude_reviews_close frees them. Without fude_reviews_open
// (or with a NULL path) the schedules live in memory only, which is what the tests use.
// Threads: main thread only.
// Files: one small file, <save dir>/reviews.kana, in the KANA format (kfile.h), written whole and atomically after
// each sitting's answers (an exam's: once, not once an answer) and read back with fude_save_load (save.h: the main
// file, else its .bak). Its layout:
//   header kind 'REVW', then one chunk:
//   'RVWS'  u32 count, u32 record size (20), then per schedule, sorted by key: u32 key (a code point, or a word's
//           FUDE_VOCAB_KEY), u16 the last step in days, u16 ease in hundredths, u32 the day it is due, u16 right
//           answers in a row (in time), u16 wrong answers ever, u32 the day of its first review.
// Design: the ease starts at 2.5; a good answer (quality 0.8 or more) adds 0.1, up to 3.0, a shaky one (under 0.5)
// takes 0.1 away, and a wrong one 0.2, never under 1.3. A step is always at least a day longer than the last, and
// at most ten years. A day is a day number, counted from 1970-01-01 in local time.
// ===========================================================================

#define FUDE_REVIEW_NEW_PER_DAY 10u
// The most asked in one sitting. A bound by design, not a cap on what is kept: every schedule is kept, and what is
// due past the first fifty (the longest waiting first) is asked in the next sitting. A sitting is meant to be
// finished at once, and fifty characters written from memory is already a long one. The side panel's "Reviews · n"
// counts the next sitting, so it shows at most this many too (study.c: fude_study_reviews_count).
#define FUDE_REVIEW_SESSION     50u

// Loads the schedules from _path, and saves there from now on. When nothing has been saved yet there are none. A NULL
// _path keeps them in memory only.
void fude_reviews_open(const c8* _path);
// Forgets every schedule and the path, and frees their memory. Nothing is saved.
void fude_reviews_close(void);

// Today, the device's local day, as a day number (days since 1970-01-01).
u32  fude_reviews_today(void);
#if defined(FUDE_TESTS) || defined(RDE_DEBUG)
// For the tests and for a developer (the demo, study/app/demo.c): the day it is, counted as fude_reviews_today
// counts them. 0 means the real day.
extern u32 fude_reviews_fake_today;
#endif

// Of _candidates (keys: the code points of the Studying and Known characters, or words' FUDE_VOCAB_KEY), writes into
// _out what is to be reviewed today, at most _max and at most FUDE_REVIEW_SESSION: first those due, the longest
// waiting first, then, in the order given, the new ones today still has room for. Returns how many it wrote.
u32  fude_reviews_due(const u32* _candidates, u32 _count, u32* _out, u32 _max);

// One answer written from memory: its character's code point (or a word's FUDE_VOCAB_KEY), whether it was right, and
// how well it was written, 0..1 (score.h's points divided by 100).
typedef struct {
    u32 key;
    b8  right;
    f32 quality;
} fude_review_answer;

// A sitting's answers (an exam's, a word exam's): each one's schedule moves as described above, then the file is
// saved once (a failure is logged). Saving once a sitting, not once an answer, keeps a 100-character exam to one write.
void fude_reviews_answer(const fude_review_answer* _answers, u32 _count);

// The day _codepoint is due, as a day number; 0 when it has never been reviewed (it is new).
u32  fude_reviews_due_day(u32 _codepoint);

// A number that changes with every answer, so what shows the schedules knows when to look again.
u32  fude_reviews_revision(void);

#endif
