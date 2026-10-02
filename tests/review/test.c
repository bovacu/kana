// review.h: the schedule, the day's new ones, persistence.
#include "study/review.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
extern u32 kana_reviews_fake_today;

int main(void) {
    system("rm -rf saves && mkdir -p saves");
    kana_reviews_open("saves/reviews.kana");
    u32 cands[15], out[64];
    for(u32 i = 0; i < 15u; i++) cands[i] = 0x4E00u + i;

    // Day 100: nothing reviewed yet: today's ten new ones, in the order given.
    kana_reviews_fake_today = 100;
    u32 n = kana_reviews_due(cands, 15, out, 64);
    CHECK(n == KANA_REVIEW_NEW_PER_DAY && out[0] == 0x4E00u && out[9] == 0x4E09u);

    // A right, cleanly: tomorrow. Today's room for new ones is one less.
    kana_reviews_answer(0x4E00u, true, 0.9f);
    CHECK(kana_reviews_due_day(0x4E00u) == 101u);
    n = kana_reviews_due(cands, 15, out, 64);
    CHECK(n == KANA_REVIEW_NEW_PER_DAY - 1u && out[0] == 0x4E01u);

    // Words (keys above the code points) have an allowance of their own: the
    // characters' new ones today do not use theirs up.
    { u32 mixed[20], mo[64];
      for(u32 i = 0; i < 15u; i++) mixed[i] = 0x4E00u + i;
      for(u32 i = 0; i < 5u; i++) mixed[15 + i] = 0x40000000u | (i + 1u);
      const u32 m = kana_reviews_due(mixed, 20, mo, 64);
      u32 words = 0; for(u32 i = 0; i < m; i++) words += mo[i] > 0x10FFFFu;
      CHECK(m == (KANA_REVIEW_NEW_PER_DAY - 1u) + 5u && words == 5u); }

    // Day 101: due; right again: three days.
    kana_reviews_fake_today = 101;
    n = kana_reviews_due(cands, 15, out, 64);
    CHECK(n >= 1u && out[0] == 0x4E00u);   // the due one first, then new ones
    kana_reviews_answer(0x4E00u, true, 0.9f);
    CHECK(kana_reviews_due_day(0x4E00u) == 104u);

    // Day 102: right early: nothing changes.
    kana_reviews_fake_today = 102;
    kana_reviews_answer(0x4E00u, true, 0.9f);
    CHECK(kana_reviews_due_day(0x4E00u) == 104u);

    // Day 104: right: the step grows past three days (3 x the ease).
    kana_reviews_fake_today = 104;
    kana_reviews_answer(0x4E00u, true, 0.9f);
    const u32 later = kana_reviews_due_day(0x4E00u);
    CHECK(later >= 104u + 7u && later <= 104u + 9u);
    printf("1 day, 3 days, then %u days\n", later - 104u);

    // Wrong: tomorrow again, and the steps start over.
    kana_reviews_fake_today = later;
    kana_reviews_answer(0x4E00u, false, 0.0f);
    CHECK(kana_reviews_due_day(0x4E00u) == later + 1u);
    kana_reviews_fake_today = later + 1u;
    kana_reviews_answer(0x4E00u, true, 0.9f);
    CHECK(kana_reviews_due_day(0x4E00u) == later + 2u);   // one day: the first step again

    // The longest waiting first: B due day 105, C due day 103 (both wrong then).
    kana_reviews_fake_today = 102; kana_reviews_answer(0x4E02u, false, 0.0f);   // due 103
    kana_reviews_fake_today = 104; kana_reviews_answer(0x4E01u, false, 0.0f);   // due 105
    kana_reviews_fake_today = 200;
    n = kana_reviews_due(cands, 15, out, 64);
    CHECK(n >= 3u && out[0] == 0x4E02u && out[1] == 0x4E01u);

    // Kept: reopened, the same days.
    const u32 a = kana_reviews_due_day(0x4E00u), c = kana_reviews_due_day(0x4E02u);
    kana_reviews_close();
    kana_reviews_open("saves/reviews.kana");
    CHECK(kana_reviews_due_day(0x4E00u) == a && kana_reviews_due_day(0x4E02u) == c && kana_reviews_due_day(0x4E0Eu) == 0u);

    // The real day is today's (no fake): a plausible day number.
    kana_reviews_fake_today = 0;
    CHECK(kana_reviews_today() > 19000u && kana_reviews_today() < 40000u);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
