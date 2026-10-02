// The string table: every language has every id; templates render per language.
#include "base/text.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while(0)
static const char* NAMES[] = {
#define KANA_TEXT_ID(_id) #_id,
#include "base/text_ids.h"
#undef KANA_TEXT_ID
};
int main(void) {
    const RDE_LANGUAGE_ langs[] = { RDE_LANGUAGE_EN_US, RDE_LANGUAGE_ES_ES, RDE_LANGUAGE_PT_BR, RDE_LANGUAGE_JA_JP, RDE_LANGUAGE_FR_FR };
    for(unsigned l = 0; l < 5; l++) {
        CHECK(rde_localization_load(KANA_TEXT_FILE, rde_localization_language_code(langs[l]), NULL));
        for(unsigned i = 0; i < KANA_TEXT_COUNT; i++) {
            if(!rde_localization_string_id_exists(NAMES[i])) { printf("missing %s in %s\n", NAMES[i], rde_localization_language_code(langs[l])); fails++; }
        }
    }
    char b[256];
    CHECK(kana_text_set_language(RDE_LANGUAGE_EN_US));
    KANA_TEXTF(b, KANA_TEXT_STROKES_N, KANA_TN(1)); CHECK(strcmp(b, "1 stroke") == 0);
    KANA_TEXTF(b, KANA_TEXT_STROKES_N, KANA_TN(5)); CHECK(strcmp(b, "5 strokes") == 0);
    KANA_TEXTF(b, KANA_TEXT_OF_N, KANA_TN(3), KANA_TN(9)); CHECK(strcmp(b, "3 of 9") == 0);
    KANA_TEXTF(b, KANA_TEXT_OF_N, KANA_TN(3), KANA_TN(9)); CHECK(strcmp(b, "3 of 9") == 0);   // kept, the second time
    CHECK(strcmp(kana_text(KANA_TEXT_BACK), "Back") == 0);
    CHECK(kana_text_set_language(RDE_LANGUAGE_PT_BR));
    KANA_TEXTF(b, KANA_TEXT_STROKES_N, KANA_TN(0)); CHECK(strcmp(b, "0 traço") == 0);    // Brazilian: 0 is singular
    KANA_TEXTF(b, KANA_TEXT_STROKES_N, KANA_TN(2)); CHECK(strcmp(b, "2 traços") == 0);
    CHECK(strcmp(kana_text(KANA_TEXT_BACK), "Voltar") == 0);
    CHECK(kana_text_set_language(RDE_LANGUAGE_JA_JP));
    KANA_TEXTF(b, KANA_TEXT_STROKES_N, KANA_TN(7)); CHECK(strcmp(b, "7画") == 0);
    KANA_TEXTF(b, KANA_TEXT_EXAM_RESULT, KANA_TN(8), KANA_TN(10), KANA_TN(77)); CHECK(strcmp(b, "10問中8問正解・77点") == 0);   // reordered
    CHECK(kana_text_set_language(RDE_LANGUAGE_FR_FR));
    CHECK(strcmp(kana_text(KANA_TEXT_PRACTICE), "S’entraîner") == 0);   // the typographic apostrophe survives
    KANA_TEXTF(b, KANA_TEXT_NOTE_DELETE_TITLE, KANA_TS("Tema 1")); CHECK(strcmp(b, "Supprimer « Tema 1 » ?") == 0);
    CHECK(kana_text_set_language(RDE_LANGUAGE_ES_ES));
    char d[64]; kana_text_date(d, sizeof d, 1790812800ull); printf("es date: %s\n", d);
    printf(fails ? "texttest %d FAILED\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
