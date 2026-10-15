// wordsplit.h against the real character data (argv[1]).
#include "study/chars/wordsplit.h"
#include "lang/lang.h"
#include <stdio.h>
#include <stdlib.h>
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
// Each word's written form and rank (wordsplit.c: WFRQ in the high 16 bits, its first place among a character's
// examples in the low 16), for alike_order.
static const c8** forms;
static u32*       ranks;
// Orders word numbers by form, then the commonest first, then by number: as the index keeps the words spelt alike.
static int by_form(const void* _a, const void* _b) {
    const u32 a = *(const u32*)_a, b = *(const u32*)_b;
    const int c = strcmp(forms[a], forms[b]);
    if(c != 0) { return c; }
    if(ranks[a] != ranks[b]) { return ranks[a] < ranks[b] ? -1 : 1; }
    return a < b ? -1 : (a > b ? 1 : 0);
}
// Every form with more than one word: splitting the form alone offers its commonest word first, then the others as
// common (by WFRQ), the commonest first, as many as the language offers. Returns how many forms were checked.
static u32 alike_order(void) {
    const u32 n = db.word_count;
    forms = malloc(sizeof(c8*) * n); ranks = malloc(sizeof(u32) * n);
    u32* order = malloc(sizeof(u32) * n);
    for(u32 i = 0; i < n; i++) {
        fude_kanji_word x; forms[i] = fude_kanji_word_at(&db, i, &x) ? x.written : "";
        ranks[i] = ((u32)fude_kanji_word_freq(&db, i) << 16) | 0xFFFFu; order[i] = i;
    }
    static u32 list[1024];
    for(u32 r = 0; r < db.count; r++) {
        const u32 k = fude_kanji_words(&db, r, list, 1024u, NULL);
        for(u32 p = 0; p < k; p++) { if(list[p] < n && p < (ranks[list[p]] & 0xFFFFu)) { ranks[list[p]] = (ranks[list[p]] & 0xFFFF0000u) | p; } }
    }
    qsort(order, n, sizeof(u32), by_form);
    const u32 most = fude_lang_words()->alike;
    u32 checked = 0;
    for(u32 i = 0, j; i < n; i = j) {
        for(j = i + 1u; j < n && strcmp(forms[order[j]], forms[order[i]]) == 0; j++) {}
        if(j - i < 2u || forms[order[i]][0] == 0) { continue; }
        u32 want[8], wants = 0;
        for(u32 k = i; k < j && wants < most && wants < 8u; k++) {
            if((ranks[order[k]] >> 16) == (ranks[order[i]] >> 16)) { want[wants++] = order[k]; }
        }
        u32 w[FUDE_WORDSPLIT_MAX];
        const u32 got = fude_wordsplit(&db, forms[order[i]], w, FUDE_WORDSPLIT_MAX);
        if(got == 0u || w[0] != order[i]) { continue; }   // the form alone read as other words (a particle at its end)
        checked++;
        b8 same = got >= wants;
        for(u32 k = 0; k < wants && same; k++) { same = w[k] == want[k]; }
        if(!same) { printf("FAIL words written %s: not offered the commonest first\n", forms[order[i]]); fails++; }
    }
    free(order); free(ranks); free(forms);
    return checked;
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
    // A whole word: the commonest written so; none for a word with more after it, or with no kanji.
    { fude_kanji_word x; const u32 nihongo = fude_wordsplit_word(&db, "日本語");
      CHECK(nihongo != UINT32_MAX && fude_kanji_word_at(&db, nihongo, &x) && strcmp(x.written, "日本語") == 0);
      CHECK(fude_wordsplit_word(&db, "日本語を") == UINT32_MAX && fude_wordsplit_word(&db, "ありがとう") == UINT32_MAX && fude_wordsplit_word(&db, "") == UINT32_MAX);
      CHECK(fude_wordsplit_word(&db, "日本\n語") == nihongo); }   // lines joined
    // A word written the same as others as common comes with them (本: ほん, もと...), each once a line.
    { u32 w[FUDE_WORDSPLIT_MAX]; const u32 n = fude_wordsplit(&db, "本と本", w, FUDE_WORDSPLIT_MAX);
      CHECK(n >= 1u && n <= 3u);
      for(u32 i = 0; i < n; i++) { for(u32 k = i + 1u; k < n; k++) { CHECK(w[i] != w[k]); } } }
    // ...and of those, the commonest first, every time: every form spelt by more than one word.
    CHECK(alike_order() > 50u);
    // Never more than asked for.
    { u32 w[2]; CHECK(fude_wordsplit(&db, "今日は日本語を勉強します。東京大学に行きたい", w, 2u) == 2u); }
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
                if(l == 0) { printf("%s: %s\n", x.written, s.taught); c8 stem[64]; snprintf(stem, sizeof(stem), "%s", x.written); if(strlen(stem) > 3) { stem[strlen(stem) - 3] = 0; } CHECK(strstr(s.taught, stem) != NULL); }   // its stem: 行く in 行って
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
