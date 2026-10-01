#ifndef KANA_REVIEW
#define KANA_REVIEW

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
// KANA_REVIEW_NEW_PER_DAY a day, so marking fifty at once does not make fifty
// due tomorrow. Kept in reviews.kana; days are the device's local days.
// ===========================================================================

#define KANA_REVIEW_NEW_PER_DAY 10u
#define KANA_REVIEW_SESSION     50u   // at most this many in one sitting

void kana_reviews_open(const c8* _path);
void kana_reviews_close(void);

// The local day today, as a day number (days since 1970-01-01).
u32  kana_reviews_today(void);

// Of _candidates (code points: the Studying and Known ones), what is due
// today, into _out, at most _max: the due first, the longest waiting first,
// then the new ones today still has room for. How many.
u32  kana_reviews_due(const u32* _candidates, u32 _count, u32* _out, u32 _max);

// An answer written from memory: its character's schedule moves (above).
// _quality: how well it was written, 0..1 (score.h).
void kana_reviews_answer(u32 _codepoint, b8 _right, f32 _quality);

// The day _codepoint is due (0: never reviewed: new).
u32  kana_reviews_due_day(u32 _codepoint);

// Changes with every answer (for what is shown from it).
u32  kana_reviews_revision(void);

#endif
