#include "chars/catalog.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

static void romaji(const char* in, const char* want, int ok_want) {
    char out[64]; int ok = kana_romaji_to_hiragana(in, out, sizeof out);
    if(strcmp(out, want) != 0 || ok != ok_want) { printf("FAIL romaji '%s' -> '%s' (%d), want '%s' (%d)\n", in, out, ok, want, ok_want); fails++; }
}
static u32 cp_at(kana_catalog* c, u32 i) { kana_kanji_info k; kana_kanji_at(c->db, kana_catalog_results(c)[i], &k); return k.codepoint; }
static int rank_of(kana_catalog* c, u32 cp) { for(u32 i = 0; i < kana_catalog_result_count(c); i++) if(cp_at(c, i) == cp) return (int)i; return -1; }
static void show(kana_catalog* c, const char* label, u32 n) {
    printf("%-28s %4u results:", label, kana_catalog_result_count(c));
    for(u32 i = 0; i < n && i < kana_catalog_result_count(c); i++) { char u[5]; kana_kanji_utf8(cp_at(c, i), u); printf(" %s", u); }
    printf("\n");
}

int main(int argc, char** argv) {
    (void)argc;
    romaji("moku", "もく", 1); romaji("ki", "き", 1); romaji("shita", "した", 1); romaji("kitte", "きって", 1);
    romaji("matcha", "まっちゃ", 1); romaji("konnichiha", "こんにちは", 1); romaji("hon", "ほん", 1); romaji("kan'i", "かんい", 1);
    romaji("ryokou", "りょこう", 1); { char o[64]; CHECK(!kana_romaji_to_hiragana("tree", o, sizeof o)); } romaji("jinja", "じんじゃ", 1); romaji("fuji", "ふじ", 1);
    romaji("shinbun", "しんぶん", 1); romaji("oo-", "おおー", 1);

    kana_kanji_db db; CHECK(kana_kanji_load(&db, argv[1]));
    kana_catalog c; kana_catalog_init(&c, &db);
    printf("catalog: %u entries\n", (u32)rde_arr_length(&c.entries));

    kana_catalog_query(&c, KANA_FILTER_ALL, KANA_SORT_DEFAULT, "");  show(&c, "all/default", 12);
    CHECK(cp_at(&c, 0) == 0x3041);   // ぁ first: hiragana first
    kana_catalog_query(&c, KANA_FILTER_HIRAGANA, KANA_SORT_DEFAULT, ""); show(&c, "hiragana", 10); CHECK(kana_catalog_result_count(&c) == 86);
    kana_catalog_query(&c, KANA_FILTER_KATAKANA, KANA_SORT_DEFAULT, ""); show(&c, "katakana", 10); CHECK(kana_catalog_result_count(&c) == 90);
    kana_catalog_query(&c, KANA_FILTER_KANJI, KANA_SORT_DEFAULT, "");    show(&c, "kanji/default", 12);
    kana_catalog_query(&c, KANA_FILTER_KANJI, KANA_SORT_STROKES, "");    show(&c, "kanji/strokes", 12);
    kana_catalog_query(&c, KANA_FILTER_KANJI, KANA_SORT_ON, "");         show(&c, "kanji/on", 12);
    kana_catalog_query(&c, KANA_FILTER_KANJI, KANA_SORT_KUN, "");        show(&c, "kanji/kun", 12);
    kana_catalog_query(&c, KANA_FILTER_KANJI, KANA_SORT_MEANING, "");    show(&c, "kanji/meaning", 12);

    kana_catalog_query(&c, KANA_FILTER_ALL, KANA_SORT_DEFAULT, "tree");  show(&c, "search 'tree'", 10); CHECK(rank_of(&c, 0x6728) >= 0 && rank_of(&c, 0x6728) < 3);
    kana_catalog_query(&c, KANA_FILTER_ALL, KANA_SORT_DEFAULT, "Tree "); CHECK(rank_of(&c, 0x6728) >= 0 && rank_of(&c, 0x6728) < 3);
    kana_catalog_query(&c, KANA_FILTER_ALL, KANA_SORT_DEFAULT, "moku");  show(&c, "search 'moku'", 10); CHECK(rank_of(&c, 0x6728) >= 0 && rank_of(&c, 0x6728) < 12);
    kana_catalog_query(&c, KANA_FILTER_ALL, KANA_SORT_DEFAULT, "もく");  CHECK(rank_of(&c, 0x6728) >= 0);
    kana_catalog_query(&c, KANA_FILTER_ALL, KANA_SORT_DEFAULT, "モク");  CHECK(rank_of(&c, 0x6728) >= 0);
    kana_catalog_query(&c, KANA_FILTER_ALL, KANA_SORT_DEFAULT, "ki");    show(&c, "search 'ki'", 10); CHECK(rank_of(&c, 0x304D) == 0); CHECK(rank_of(&c, 0x6728) >= 0);
    kana_catalog_query(&c, KANA_FILTER_ALL, KANA_SORT_DEFAULT, "ka");    CHECK(rank_of(&c, 0x304B) >= 0 && rank_of(&c, 0x30AB) >= 0);   // か and カ
    kana_catalog_query(&c, KANA_FILTER_KANJI, KANA_SORT_DEFAULT, "a");   show(&c, "kanji 'a' (stem)", 10); CHECK(rank_of(&c, 0x4E0A) >= 0);   // 上 (あ.げる)
    kana_catalog_query(&c, KANA_FILTER_ALL, KANA_SORT_DEFAULT, "sun");   show(&c, "search 'sun'", 10); CHECK(rank_of(&c, 0x65E5) >= 0 && rank_of(&c, 0x65E5) < 3);
    kana_catalog_query(&c, KANA_FILTER_ALL, KANA_SORT_DEFAULT, "zzzq");  CHECK(kana_catalog_result_count(&c) == 0);
    kana_catalog_query(&c, KANA_FILTER_HIRAGANA, KANA_SORT_DEFAULT, "tree"); CHECK(kana_catalog_result_count(&c) == 0);

    kana_catalog_query(&c, KANA_FILTER_N5, KANA_SORT_DEFAULT, ""); show(&c, "N5", 12); CHECK(kana_catalog_result_count(&c) == 79);
    kana_catalog_query(&c, KANA_FILTER_N1, KANA_SORT_STROKES, ""); show(&c, "N1 by strokes", 8); CHECK(kana_catalog_result_count(&c) == 1232);
    kana_catalog_query(&c, KANA_FILTER_N5, KANA_SORT_DEFAULT, "water"); show(&c, "N5 'water'", 5); CHECK(rank_of(&c, 0x6C34) == 0);
    kana_catalog_destroy(&c);
    // Spanish: meanings in it, searched without accents; English still finds them.
    kana_kanji_set_language(&db, "es"); kana_catalog_init(&c, &db);
    kana_catalog_query(&c, KANA_FILTER_KANJI, KANA_SORT_DEFAULT, "arbol"); show(&c, "es 'arbol'", 5); CHECK(rank_of(&c, 0x6728) >= 0 && rank_of(&c, 0x6728) < 3);
    kana_catalog_query(&c, KANA_FILTER_KANJI, KANA_SORT_DEFAULT, "Árbol"); CHECK(rank_of(&c, 0x6728) >= 0 && rank_of(&c, 0x6728) < 3);
    kana_catalog_query(&c, KANA_FILTER_N5, KANA_SORT_DEFAULT, "water"); CHECK(rank_of(&c, 0x6C34) == 0);
    { char f[64]; kana_catalog_fold("Été, Ñandú, straße", f, sizeof f); CHECK(strcmp(f, "ete, nandu, strasse") == 0); }
    kana_catalog_destroy(&c); kana_kanji_unload(&db);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
