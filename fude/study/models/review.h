#ifndef FUDE_REVIEW
#define FUDE_REVIEW

#include "rde.h"

// ===========================================================================
// Reviews: spaced repetition for the characters being learnt. Each Studying or
// Known character (marks.h) has a schedule: the day it is next due, how many
// days the last step was, how easily it comes (its EASE). Writing it from
// memory — in Reviews, or in any exam — moves the schedule:
//
//   right, in time   the next step grows: 1 day, then 3, then the last step
//                    times the ease; a clean, well-written answer (points from
//                    score.h) makes it easier, a shaky one a little harder.
//   right, early     nothing changes (the step was not waited for).
//   wrong            due again tomorrow, the steps start over, and harder.
//
// Reviews (the side panel) asks what is due today, oldest first, then a few
// new ones — characters marked but never reviewed — at most
// FUDE_REVIEW_NEW_PER_DAY a day, so marking fifty at once does not make fifty
// due tomorrow. Kept in reviews.kana; days are the device's local days.
//
// WORDS (vocab.h) have schedules too, keyed above every code point
// (FUDE_VOCAB_KEY): written in word exams, reviewed from the Vocabulary screen;
// their new ones have an allowance of their own.
// ===========================================================================

#define FUDE_REVIEW_NEW_PER_DAY 10u
#define FUDE_REVIEW_SESSION     50u   // at most this many in one sitting

void fude_reviews_open(const c8* _path);
void fude_reviews_close(void);

// The local day today, as a day number (days since 1970-01-01).
u32  fude_reviews_today(void);
#if defined(FUDE_TESTS) || defined(RDE_DEBUG)
// Tests' and a developer's (the demo's, study/app/demo.c): the day it is, as
// fude_reviews_today counts them (0: the real one).
extern u32 fude_reviews_fake_today;
#endif

// Of _candidates (code points: the Studying and Known ones), what is due
// today, into _out, at most _max: the due first, the longest waiting first,
// then the new ones today still has room for. How many.
u32  fude_reviews_due(const u32* _candidates, u32 _count, u32* _out, u32 _max);

// An answer written from memory: its character's schedule moves (above).
// _quality: how well it was written, 0..1 (score.h).
void fude_reviews_answer(u32 _codepoint, b8 _right, f32 _quality);

// The day _codepoint is due (0: never reviewed: new).
u32  fude_reviews_due_day(u32 _codepoint);

// Changes with every answer (for what is shown from it).
u32  fude_reviews_revision(void);

#endif
