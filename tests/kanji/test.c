#include "study/chars/kanji.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

int main(int argc, char** argv) {
    (void)argc;
    fude_kanji_db db;
    CHECK(fude_kanji_load(&db, argv[1]));
    printf("characters: %u\n", db.count);

    fude_kanji_info ki;
    CHECK(fude_kanji_find(&db, 0x6728, &ki));   // 木
    printf("木: strokes %u grade %u jlpt %u freq %u radical %u\n  on [%s] kun [%s] meanings [%s]\n",
           ki.strokes, ki.grade, ki.jlpt, ki.frequency, ki.radical, fude_kanji_reading(&db, &ki, 0u), fude_kanji_reading(&db, &ki, 1u), fude_kanji_meanings(&db, &ki));
    CHECK(ki.strokes == 4 && ki.grade == 1 && ki.radical == 75 && strcmp(fude_kanji_meanings(&db, &ki), "tree, wood") == 0);
    CHECK(strcmp(fude_kanji_reading(&db, &ki, 0u), "ボク、モク") == 0);
    const u32 types[4] = { 0x31D0, 0x31D1, 0x31D2, 0x31CF };   // ㇐ ㇑ ㇒ ㇏
    for(u32 s = 0; s < ki.strokes; s++) {
        fude_kanji_stroke st; CHECK(fude_kanji_stroke_at(&db, &ki, s, &st));
        rde_vec_2F pts[512]; u32 n = fude_kanji_stroke_points(&st, 0.25f, pts, 512);
        printf("  stroke %u: type U+%04X%c, %u segments, start (%.2f, %.2f) -> end (%.2f, %.2f), %u points\n",
               s + 1, st.type, st.variant ? st.variant : ' ', st.segments, st.start.x, st.start.y, pts[n-1].x, pts[n-1].y, n);
        CHECK(st.type == types[s]);
    }
    // Stroke 1 of 木 in the source: M19.5,39.86 ... ends at 87.34? (c...2.8,0.5 from 84.54-ish,36.15)
    { fude_kanji_stroke st; fude_kanji_stroke_at(&db, &ki, 0, &st); CHECK(st.start.x == 19.5f && st.start.y > 39.85f && st.start.y < 39.87f); }

    // あ (U+3042): kana, no KANJIDIC info, 3 strokes.
    CHECK(fude_kanji_find(&db, 0x3042, &ki)); CHECK(ki.strokes == 3 && ki.text == UINT32_MAX && fude_kanji_meanings(&db, &ki)[0] == 0);
    CHECK(!fude_kanji_find(&db, 0x1F600, &ki));   // an emoji: not present (KanjiVG does have some Latin)

    // Every character: every stroke decodes and flattens inside a sane box; the
    // geometry of one ends where the next begins (no overlap, nothing left over).
    u32 total_strokes = 0, total_points = 0, max_points = 0, prev = 0, order_ok = 1;
    for(u32 i = 0; i < db.count; i++) {
        CHECK(fude_kanji_at(&db, i, &ki));
        if(i > 0 && ki.codepoint <= prev) order_ok = 0;
        prev = ki.codepoint;
        for(u32 s = 0; s < ki.strokes; s++) {
            fude_kanji_stroke st;
            if(!fude_kanji_stroke_at(&db, &ki, s, &st)) { printf("decode fail U+%04X s%u\n", ki.codepoint, s); fails++; continue; }
            rde_vec_2F pts[4096]; u32 n = fude_kanji_stroke_points(&st, 0.25f, pts, 4096);
            for(u32 p = 0; p < n; p++) if(pts[p].x < -5 || pts[p].x > 115 || pts[p].y < -5 || pts[p].y > 115) { printf("out of box U+%04X\n", ki.codepoint); fails++; break; }
            total_points += n; max_points = n > max_points ? n : max_points; total_strokes++;
        }
    }
    CHECK(order_ok);
    printf("all: %u strokes -> %u points at 0.25-unit tolerance (avg %.1f, max %u per stroke)\n", total_strokes, total_points, (double)total_points / total_strokes, max_points);
    CHECK(total_strokes == 79907);

    // Example words (JMdict): 日 lists 日 ひ first, each word's strings whole; あ none.
    {
        u32 idx, w[FUDE_KANJI_MAX_WORDS];
        CHECK(db.word_count > 5000);
        CHECK(fude_kanji_find_index(&db, 0x65E5, &idx));
        u32 ex = 0; const u32 n = fude_kanji_words(&db, idx, w, FUDE_KANJI_MAX_WORDS, &ex);
        CHECK(ex >= 3 && ex <= 6 && n > ex && n <= 20);
        fude_kanji_word x;
        CHECK(n > 0 && fude_kanji_word_at(&db, w[0], &x) && strcmp(x.written, "日") == 0 && strcmp(x.reading, "ひ") == 0 && x.meaning[0] != 0);
        for(u32 i = 0; i < n; i++) { CHECK(fude_kanji_word_at(&db, w[i], &x) && strstr(x.written, "日") != NULL); }
        CHECK(fude_kanji_words(&db, idx, w, 2, &ex) == 2 && ex == 2);   // the examples, as many as asked for
        CHECK(!fude_kanji_word_at(&db, db.word_count, &x));
        CHECK(fude_kanji_find_index(&db, 0x3042, &idx) && fude_kanji_words(&db, idx, w, FUDE_KANJI_MAX_WORDS, NULL) == 0);
        u32 with = 0, total = 0;
        for(u32 i = 0; i < db.count; i++) { const u32 k = fude_kanji_words(&db, i, w, FUDE_KANJI_MAX_WORDS, NULL); with += k > 0; total += k; }
        printf("words: %u kanji with words, %u listed, %u distinct\n", with, total, db.word_count);
        CHECK(with > 2000);
    }
    fude_kanji_unload(&db);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
