// wordsplit.h against the real character data (argv[1]).
#include "lang/wordsplit.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
static fude_kanji_db db;
static void show(const c8* text, const c8* const* want, u32 nwant) {
    u32 w[FUDE_WORDSPLIT_MAX];
    const u32 n = fude_wordsplit(&db, text, w, FUDE_WORDSPLIT_MAX);
    printf("%s ->", text);
    for(u32 i = 0; i < n; i++) { fude_kanji_word x; fude_kanji_word_at(&db, w[i], &x); printf(" %s(%s)", x.written, x.reading); }
    printf("\n");
    for(u32 k = 0; k < nwant; k++) {
        b8 found = false;
        for(u32 i = 0; i < n; i++) { fude_kanji_word x; fude_kanji_word_at(&db, w[i], &x); found |= strcmp(x.written, want[k]) == 0; }
        if(!found) { printf("  missing %s\n", want[k]); fails++; }
    }
}
int main(int argc, char** argv) {
    CHECK(argc > 1 && fude_kanji_load(&db, argv[1]));
    printf("%u words in the data\n", db.word_count);
    const c8* a[] = { "今日", "日本語", "勉強" };            show("今日は日本語を勉強します。", a, 3);
    const c8* b[] = { "駅" };                               show("駅はどこですか？", b, 1);
    const c8* c[] = { "本", "面白い" };                     show("この本はとても面白かったです。", c, 2);
    const c8* d[] = { "食べる" };                           show("昨日、寿司を食べました。", d, 1);
    const c8* e[] = { "東京", "大学", "行く" };              show("東京大学に行きたい", e, 3);
    const c8* f[] = { "本" };                               show("本を読んだ", f, 1);
    const c8* g[] = { "読む" };                             show("本を読みま\nした", g, 1);
    const c8* h[] = { "空ける", "書く" };                     show("行と行の間を空けなさい。早く書きなさい。", h, 2);
    show("ありがとう", NULL, 0);
    u32 w[4]; CHECK(fude_wordsplit(&db, "", w, 4) == 0 && fude_wordsplit(&db, "ひらがなだけ", w, 4) == 0);
    CHECK(fude_wordsplit_kanji(&db, "日本語") == 0x65E5u && fude_wordsplit_kanji(&db, "ありがとう") == 0u);
    // Example sentences ('SENT'): the words' own, in the language set, English where none.
    u32 with = 0;
    for(u32 i = 0; i < db.word_count; i++) { fude_kanji_sentence s; with += fude_kanji_word_sentence(&db, i, &s); }
    printf("%u words with a sentence (of %u sentences)\n", with, db.sentence_count);
    CHECK(with > 10000u);
    const c8* langs[] = { "en", "es", "fr", "pt" };
    u32 tw[4];
    const u32 tn = fude_wordsplit(&db, "東京大学に行きたい", tw, 4);
    for(u32 i = 0; i < tn; i++) {
        fude_kanji_word x; fude_kanji_word_at(&db, tw[i], &x);
        for(u32 l = 0; l < 4; l++) {
            fude_kanji_set_language(&db, langs[l]);
            fude_kanji_sentence s;
            if(fude_kanji_word_sentence(&db, tw[i], &s)) {
                if(l == 0) { printf("%s: %s\n", x.written, s.japanese); c8 stem[64]; snprintf(stem, sizeof(stem), "%s", x.written); if(strlen(stem) > 3) { stem[strlen(stem) - 3] = 0; } CHECK(strstr(s.japanese, stem) != NULL); }   // its stem: 行く in 行って
                printf("   %s: %s\n", langs[l], s.translation);
                CHECK(s.translation[0] != 0);
            }
        }
    }
    fude_kanji_sentence s;
    CHECK(!fude_kanji_word_sentence(&db, db.word_count, &s) && !fude_kanji_word_sentence(&db, UINT32_MAX, &s));
    // Look-alikes ('LOOK'): the classic pairs, closest first; kana with kana.
    { const u32 pairs[][2] = { { 0x672A, 0x672B }, { 0x571F, 0x58EB }, { 0x9593, 0x554F }, { 0x5F85, 0x4F8D }, { 0x30B7, 0x30F3 }, { 0x308F, 0x308C } };
      for(u32 p = 0; p < sizeof(pairs) / sizeof(pairs[0]); p++) {
          u32 r, la[4]; CHECK(fude_kanji_find_index(&db, pairs[p][0], &r));
          const u32 n = fude_kanji_lookalikes(&db, r, la, 4);
          if(!(n > 0 && la[0] == pairs[p][1])) { printf("FAIL look-alike of %X: %u, first %X\n", pairs[p][0], n, n ? la[0] : 0); fails++; }
      }
      u32 r, la[4]; CHECK(fude_kanji_find_index(&db, 0x4EBA, &r) && fude_kanji_lookalikes(&db, r, la, 4) == 0u);   // 人: none close enough
      CHECK(fude_kanji_lookalikes(&db, db.count, la, 4) == 0u); }
    fude_kanji_unload(&db);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
