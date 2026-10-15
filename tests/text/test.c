#include "drawing/base/utf8.h"
// The string table: every language has every id; templates render per language.
#include "drawing/base/text.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while(0)
static const char* NAMES[] = {
#define FUDE_TEXT_ID(_id) #_id,
#include "text_ids.h"
#undef FUDE_TEXT_ID
};
int main(void) {
    const RDE_LANGUAGE_ langs[] = { RDE_LANGUAGE_EN_US, RDE_LANGUAGE_ES_ES, RDE_LANGUAGE_PT_BR, RDE_LANGUAGE_JA_JP, RDE_LANGUAGE_FR_FR };
    for(unsigned l = 0; l < 5; l++) {
        CHECK(rde_localization_load(FUDE_TEXT_FILE, rde_localization_language_code(langs[l]), NULL));
        for(unsigned i = 0; i < FUDE_TEXT_COUNT; i++) {
            if(!rde_localization_string_id_exists(NAMES[i])) { printf("missing %s in %s\n", NAMES[i], rde_localization_language_code(langs[l])); fails++; }
        }
    }
    char b[256];
    CHECK(fude_text_set_language(RDE_LANGUAGE_EN_US));
    FUDE_TEXTF(b, FUDE_TEXT_STROKES_N, FUDE_TN(1)); CHECK(strcmp(b, "1 stroke") == 0);
    FUDE_TEXTF(b, FUDE_TEXT_STROKES_N, FUDE_TN(5)); CHECK(strcmp(b, "5 strokes") == 0);
    FUDE_TEXTF(b, FUDE_TEXT_OF_N, FUDE_TN(3), FUDE_TN(9)); CHECK(strcmp(b, "3 of 9") == 0);
    FUDE_TEXTF(b, FUDE_TEXT_OF_N, FUDE_TN(3), FUDE_TN(9)); CHECK(strcmp(b, "3 of 9") == 0);   // kept, the second time
    CHECK(strcmp(fude_text(FUDE_TEXT_BACK), "Back") == 0);
    CHECK(fude_text_set_language(RDE_LANGUAGE_PT_BR));
    FUDE_TEXTF(b, FUDE_TEXT_STROKES_N, FUDE_TN(0)); CHECK(strcmp(b, "0 traço") == 0);    // Brazilian: 0 is singular
    FUDE_TEXTF(b, FUDE_TEXT_STROKES_N, FUDE_TN(2)); CHECK(strcmp(b, "2 traços") == 0);
    CHECK(strcmp(fude_text(FUDE_TEXT_BACK), "Voltar") == 0);
    CHECK(fude_text_set_language(RDE_LANGUAGE_JA_JP));
    FUDE_TEXTF(b, FUDE_TEXT_STROKES_N, FUDE_TN(7)); CHECK(strcmp(b, "7画") == 0);
    FUDE_TEXTF(b, FUDE_TEXT_EXAM_RESULT, FUDE_TN(8), FUDE_TN(10), FUDE_TN(77)); CHECK(strcmp(b, "10問中8問正解・77点") == 0);   // reordered
    CHECK(fude_text_set_language(RDE_LANGUAGE_FR_FR));
    CHECK(strcmp(fude_text(FUDE_TEXT_PRACTICE), "S’entraîner") == 0);   // the typographic apostrophe survives
    FUDE_TEXTF(b, FUDE_TEXT_NOTE_DELETE_TITLE, FUDE_TS("Tema 1")); CHECK(strcmp(b, "Supprimer « Tema 1 » ?") == 0);
    CHECK(fude_text_set_language(RDE_LANGUAGE_EN_US));
    FUDE_TEXTF(b, FUDE_TEXT_DATA_EXPORT_SAVED, FUDE_TN(3), FUDE_TS("/a/b 2.kanabackup"));   // a number, then a string
    CHECK(strcmp(b, "Saved (3 files): /a/b 2.kanabackup") == 0);
    // A placeholder inside a plural's branch is filled in: the branch is a message of its own, as RDE renders it.
    FUDE_TEXTF(b, FUDE_TEXT_VOCAB_LIST_SAVED, FUDE_TN(3), FUDE_TS("Lesson 2"));
    CHECK(strcmp(b, "Saved as Lesson 2: 3 characters, in your vocabulary") == 0);
    FUDE_TEXTF(b, FUDE_TEXT_VOCAB_LIST_SAVED, FUDE_TN(1), FUDE_TS("Lesson {2}"));   // braces in an argument stay text
    CHECK(strcmp(b, "Saved as Lesson {2}: 1 character, in your vocabulary") == 0);
    // A practice sheet saved on a computer, cut to what a sheet takes: the file's name and the count, in one notice's
    // line (a notice keeps 191 bytes) in every language.
    FUDE_TEXTF(b, FUDE_TEXT_SHEET_SAVED_FIRST_N, FUDE_TN(200), FUDE_TS("Kanji N5"));
    CHECK(strcmp(b, "Practice sheet saved: Kanji N5 (its first 200 characters)") == 0);
    for(unsigned l = 0; l < 5; l++) {
        CHECK(fude_text_set_language(langs[l]));
        FUDE_TEXTF(b, FUDE_TEXT_SHEET_SAVED_FIRST_N, FUDE_TN(200), FUDE_TS("Kanji N5"));
        CHECK(strstr(b, "Kanji N5") != NULL && strstr(b, "200") != NULL && strlen(b) < 192u);
    }
    // Filled-in templates are kept by the whole of what they are made from (text.c's fude_text_key, in an rde_lru of
    // 256): a string holding what once separated the arguments in the key is not the arguments it seems to hold, and
    // more templates than are kept, asked again in a random order, are each as first made.
    CHECK(fude_text_set_language(RDE_LANGUAGE_EN_US));
    { char one[128], two[128];
      FUDE_TEXTF(one, FUDE_TEXT_NOTE_DELETE_TITLE, FUDE_TS("a\x1F$b"));
      FUDE_TEXTF(two, FUDE_TEXT_NOTE_DELETE_TITLE, FUDE_TS("a"), FUDE_TS("b"));
      CHECK(strcmp(one, "Delete “a\x1F$b”?") == 0 && strcmp(two, "Delete “a”?") == 0);
      static char first[600][48];
      for(unsigned i = 0; i < 600u; i++) FUDE_TEXTF(first[i], FUDE_TEXT_OF_N, FUDE_TN(i), FUDE_TN(600));
      CHECK(strcmp(first[5], "5 of 600") == 0 && strcmp(first[599], "599 of 600") == 0);
      unsigned differ = 0, s = 12345u;
      for(unsigned r = 0; r < 4000u; r++) {
          s = s * 1103515245u + 12345u;
          const unsigned i = (s >> 8) % 600u;
          char again[48];
          FUDE_TEXTF(again, FUDE_TEXT_OF_N, FUDE_TN(i), FUDE_TN(600));
          differ += strcmp(again, first[i]) != 0;
      }
      CHECK(differ == 0u); }
    CHECK(fude_text_set_language(RDE_LANGUAGE_FR_FR));
    CHECK(fude_text_set_language(RDE_LANGUAGE_ES_ES));
    char d[64]; fude_text_date(d, sizeof d, 1790812800ull); printf("es date: %s\n", d);
    // UTF-8: a character cut short (a lead byte, then letters) is one malformed byte, and the
    // letters after it are read.
    { const c8* p = "\xE3" "AB\xE3\x81\x82"; u32 got[4]; u32 n = 0;
      for(u32 cp = fude_utf8_next(&p); cp != 0 && n < 4u; cp = fude_utf8_next(&p)) got[n++] = cp;
      CHECK(n == 4u && got[0] == 0xFFFDu && got[1] == 'A' && got[2] == 'B' && got[3] == 0x3042u);
      // Not well formed: an overlong NUL (C0 80) and an overlong '/' (E0 80 AF), a surrogate (ED A0 80), past U+10FFFF
      // (F4 90 80 80): each lead byte alone is malformed, each byte after it too, and nothing is read as a character.
      const c8* bad[4] = { "\xC0\x80", "\xE0\x80\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80" };
      for(u32 b = 0; b < 4u; b++) {
          const c8* q = bad[b]; u32 n2 = 0, odd = 0;
          for(u32 cp = fude_utf8_next(&q); cp != 0; cp = fude_utf8_next(&q)) { n2++; odd += cp != 0xFFFDu; }
          CHECK(n2 == (u32)strlen(bad[b]) && odd == 0u);
      }
      // The shortest forms at each length's edges are read: U+0080, U+0800, U+FFFD itself, U+10000, U+10FFFF.
      { u32 len2 = 0; CHECK(fude_utf8_decode("\xC2\x80", &len2) == 0x80u && len2 == 2u);
        CHECK(fude_utf8_decode("\xE0\xA0\x80", &len2) == 0x800u && len2 == 3u);
        CHECK(fude_utf8_decode("\xEF\xBF\xBD", &len2) == 0xFFFDu && len2 == 3u);
        CHECK(fude_utf8_decode("\xF0\x90\x80\x80", &len2) == 0x10000u && len2 == 4u);
        CHECK(fude_utf8_decode("\xF4\x8F\xBF\xBF", &len2) == 0x10FFFFu && len2 == 4u);
        c8 enc[5]; u32 bad_round = 0;   // every code point but the surrogates written and read back
        for(u32 cp = 1; cp <= 0x10FFFFu; cp += cp < 0x3000u ? 1u : 97u) {
            if(cp >= 0xD800u && cp <= 0xDFFFu) continue;
            const u32 w = fude_utf8_put(cp, enc);
            bad_round += fude_utf8_decode(enc, &len2) != cp || len2 != w;
        }
        CHECK(bad_round == 0u); }
      c8 none[1] = { 'x' }; fude_utf8_copy(none, 0u, "abc", 3u); CHECK(none[0] == 'x');   // no room: nothing written
      c8 two[3]; fude_utf8_set(two, sizeof two, "\xE3\x81\x82"); CHECK(two[0] == 0); }   // a character that does not fit: left out
    // A document's read lines are searched as the PDF's own text is: case, accents and full-width forms alike, an
    // accent in the text left out (and kept in the match), each match found after the one before.
    { usize end = 0;
      CHECK(fude_utf8_find("Café au lait", "CAFE", 0u, &end) == 0 && end == strlen("Café"));
      CHECK(fude_utf8_find("ｔｏｋｙｏ駅", "Tokyo", 0u, &end) == 0 && end == strlen("ｔｏｋｙｏ"));             // full-width letters
      CHECK(fude_utf8_find("cafe\xCC\x81 noir", "café", 0u, &end) == 0 && end == strlen("cafe\xCC\x81"));   // a combining accent
      CHECK(fude_utf8_find("x \xCC\x81" "a", "a", 0u, &end) == 4 && end == 5);                              // never starting on an accent
      CHECK(fude_utf8_find("日本語の本", "本", 0u, &end) == 3 && end == 6);
      CHECK(fude_utf8_find("日本語の本", "本", end, &end) == 12 && end == 15);
      CHECK(fude_utf8_find("日本語の本", "本", end, &end) == -1);
      CHECK(fude_utf8_find("abc", "", 0u, &end) == -1 && fude_utf8_find("abc", "\xCC\x81", 0u, &end) == -1);   // nothing to compare
      CHECK(fude_utf8_find("ab", "abc", 0u, &end) == -1 && fude_utf8_find("Ölbaum", "olb", 0u, &end) == 0);
      CHECK(fude_utf8_search_fold(0xFF21u) == 'a' && fude_utf8_search_fold(0x3000u) == ' ' && fude_utf8_search_fold(0x0301u) == 0u);
      CHECK(fude_utf8_search_fold(0x3042u) == 0x3042u && fude_utf8_search_fold(0x0416u) == 0x0416u); }       // other scripts as they are
    printf(fails ? "texttest %d FAILED\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
