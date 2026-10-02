// vocab.h: an old words.kana (the kanji's words) read into the vocabulary; words
// saved, found, changed, removed; lists; all of it saved and read back.
#include "study/vocab.h"
#include "base/kfile.h"
#include "base/text.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

static void put_s(kana_bytes* b, const char* s) { kana_put_u16(b, (u16)strlen(s)); kana_put_data(b, s, (u32)strlen(s)); }

int main(int argc, char** argv) {
    CHECK(kana_text_set_language(RDE_LANGUAGE_EN_US));
    mkdir("saves", 0755);
    // An old file: 日本 under 日 and under 本 (once in the vocabulary), 木 under 木.
    kana_bytes b = kana_bytes_new(256);
    kana_put_header(&b, 1u, KANA_TAG('U', 'W', 'R', 'D'));
    u32 c = kana_chunk_begin(&b, KANA_TAG('W', 'R', 'D', 'S'));
    kana_put_u32(&b, 3);
    const struct { u32 k; const char *w, *r, *m; } old[3] = { { 0x65E5, "日本", "にほん", "Japan" }, { 0x672C, "日本", "にほん", "Japan" }, { 0x6728, "木", "き", "tree" } };
    for(int i = 0; i < 3; i++) { kana_put_u32(&b, old[i].k); kana_put_u32(&b, 1700000000u + i); kana_put_u32(&b, 0); put_s(&b, old[i].w); put_s(&b, old[i].r); put_s(&b, old[i].m); }
    kana_chunk_end(&b, c);
    CHECK(kana_bytes_write_and_free(&b, "saves/words.kana", NULL));

    kana_vocab_open("saves/words.kana");
    CHECK(kana_vocab_count() == 2);
    const u32 nihon = kana_vocab_find("日本", "にほん");
    CHECK(nihon != 0 && kana_vocab_get(nihon)->kanji == 0x65E5 && kana_vocab_get(nihon)->added == 1700000000ull);
    CHECK(kana_vocab_kanji_count(0x65E5) == 1 && kana_vocab_kanji_count(0x672C) == 1 && kana_vocab_kanji_count(0x6728) == 1);
    kana_kanji_word kw; CHECK(kana_vocab_kanji_at(0x672C, 0, &kw) && strcmp(kw.meaning, "Japan") == 0 && !kana_vocab_kanji_at(0x672C, 1, &kw));

    // Words: saved once; found; changed; a clash refused.
    const u32 r0 = kana_vocab_revision();
    const u32 taberu = kana_vocab_add("食べる", "たべる", "to eat", 0x98DF);
    CHECK(taberu != 0 && taberu != nihon && kana_vocab_revision() != r0);
    CHECK(kana_vocab_add("食べる", "たべる", "eat (again)", 0) == taberu && kana_vocab_count() == 3);
    CHECK(kana_vocab_add("", "x", "y", 0) == 0);
    CHECK(kana_vocab_update(taberu, "食べる", "たべる", "to eat; to live on"));
    CHECK(!kana_vocab_update(taberu, "日本", "にほん", "clash"));
    CHECK(strcmp(kana_vocab_get(taberu)->meaning, "to eat; to live on") == 0);
    // A kanji's words: written with it, or saved from its page.
    const u32 sushi = kana_vocab_add("すし", "すし", "sushi", 0x98DF);
    u32 ids[8]; CHECK(kana_vocab_of_kanji(0x98DF, ids, 8) == 2 && ids[0] == taberu && ids[1] == sushi);
    // A long text is cut at a whole character.
    char longw[200]; longw[0] = 0; for(int i = 0; i < 40; i++) strcat(longw, "語");
    const u32 lw = kana_vocab_add(longw, "", "", 0);
    CHECK(lw != 0 && strlen(kana_vocab_get(lw)->written) == 63 && strlen(kana_vocab_get(lw)->written) % 3 == 0);

    // Lists.
    char name[64]; kana_vocab_list_new_name(name, sizeof name); CHECK(strcmp(name, "List 1") == 0);
    const u32 food = kana_vocab_list_add("Food");
    const u32 l1   = kana_vocab_list_add(name);
    kana_vocab_list_new_name(name, sizeof name); CHECK(strcmp(name, "List 3") == 0);
    CHECK(food != 0 && l1 != 0 && food != l1 && kana_vocab_list_count() == 2 && kana_vocab_list_at(0) == food);
    CHECK(kana_vocab_list_add("") == 0);
    kana_vocab_set_in_list(food, taberu, true); kana_vocab_set_in_list(food, sushi, true); kana_vocab_set_in_list(food, taberu, true);
    kana_vocab_set_in_list(l1, nihon, true); kana_vocab_set_in_list(l1, taberu, true);
    kana_vocab_set_in_list(food, 9999, true);
    CHECK(kana_vocab_list_words(food, NULL, 0) == 2 && kana_vocab_in_list(food, sushi) && !kana_vocab_in_list(food, nihon));
    CHECK(kana_vocab_list_rename(food, "Food & drink") && strcmp(kana_vocab_list_name(food), "Food & drink") == 0);
    kana_vocab_set_in_list(food, sushi, false); CHECK(kana_vocab_list_words(food, ids, 8) == 1 && ids[0] == taberu);
    // A word removed leaves every list.
    kana_vocab_remove(taberu);
    CHECK(kana_vocab_get(taberu) == NULL && kana_vocab_list_words(food, NULL, 0) == 0 && kana_vocab_list_words(l1, ids, 8) == 1 && ids[0] == nihon);

    // Read back: the same, and ids are not reused.
    kana_vocab_close(); kana_vocab_open("saves/words.kana");
    CHECK(kana_vocab_count() == 4 && kana_vocab_find("すし", "すし") == sushi && kana_vocab_get(taberu) == NULL);
    CHECK(kana_vocab_list_count() == 2 && strcmp(kana_vocab_list_name(food), "Food & drink") == 0 && kana_vocab_in_list(l1, nihon));
    const u32 again = kana_vocab_add("食べる", "たべる", "to eat", 0);
    CHECK(again != taberu && again > lw);
    const u32 l3 = kana_vocab_list_add("Third"); CHECK(l3 > l1);
    kana_vocab_list_remove(food);
    CHECK(kana_vocab_list_count() == 2 && kana_vocab_list_at(0) == l1 && kana_vocab_find("すし", "すし") == sushi);
    kana_vocab_close(); kana_vocab_open("saves/words.kana");
    CHECK(kana_vocab_list_count() == 2 && kana_vocab_list_at(1) == l3 && kana_vocab_count() == 5);
    // Keys for the reviews stay above code points.
    CHECK(KANA_VOCAB_KEY(sushi) > 0x10FFFFu);
    // Characters as a new list (Select mode's Save list): a kanji read by its first
    // kun reading, a kana by itself with its romaji.
    if(argc > 1) {
        static kana_kanji_db db;
        CHECK(kana_kanji_load(&db, argv[1]));
        u32 recs[2];
        CHECK(kana_kanji_find_index(&db, 0x6728u, &recs[0]) && kana_kanji_find_index(&db, 0x3042u, &recs[1]));   // 木 あ
        c8 name[KANA_VOCAB_LIST_NAME]; u32 saved = 0;
        const u32 before = kana_vocab_count();
        const u32 list = kana_vocab_add_characters(&db, recs, 2, name, sizeof(name), &saved);
        CHECK(list != 0 && saved == 2 && kana_vocab_count() >= before + 1 && kana_vocab_list_words(list, NULL, 0) == 2);   // a word already there is the same word
        const u32 ki = kana_vocab_find("木", "き"), a = kana_vocab_find("あ", "あ");
        CHECK(ki != 0 && a != 0 && kana_vocab_in_list(list, ki) && strcmp(kana_vocab_get(a)->meaning, "a") == 0);
        kana_kanji_unload(&db);
    }
    kana_vocab_close();
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
