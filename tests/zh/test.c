// fude/lang/zh, Hanzi's language: pinyin typed into readings (with tone numbers or without, of any length), how
// readings compare, the levels and the groups. Built as Hanzi (run.sh's @APP=hanzi): its strings and its data.
#include "lang/lang.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// Turns _latin into a reading in a buffer of _size bytes (at most 512) and says whether it read, keeping what came out.
static c8 got[512];
static b8 pinyin(const c8* _latin, usize _size) {
    memset(got, 'x', sizeof got);
    return fude_lang_reading_from_latin(_latin, got, _size);
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    // Tone numbers: the mark on an a or an e, else the o of ou, else the last vowel; 5 and none leave it unmarked.
    CHECK(pinyin("ni3hao3", 64) && strcmp(got, "nǐhǎo") == 0);
    CHECK(pinyin("ni3 hao3", 64) && strcmp(got, "nǐhǎo") == 0);
    CHECK(pinyin("NI3HAO3", 64) && strcmp(got, "nǐhǎo") == 0);              // capitals read as small letters
    CHECK(pinyin("lv4", 64) && strcmp(got, "lǜ") == 0);                       // v is ü
    CHECK(pinyin("dou1", 64) && strcmp(got, "dōu") == 0);                     // the o of ou
    CHECK(pinyin("gui4", 64) && strcmp(got, "guì") == 0);                     // the last vowel
    CHECK(pinyin("ma5", 64) && strcmp(got, "ma") == 0);
    CHECK(pinyin("xi'an1", 64) && strcmp(got, "xiān") == 0);                  // an apostrophe parts syllables
    CHECK(pinyin("nǐ hǎo", 64) && strcmp(got, "nǐhǎo") == 0);                 // marks typed as they are

    // Toneless pinyin runs its syllables together into one, of any length: 23 letters, then 200, read whole (a
    // syllable's letters once had room for 16).
    CHECK(pinyin("zhonghuarenmingongheguo", 64) && strcmp(got, "zhonghuarenmingongheguo") == 0);
    CHECK(pinyin("zhonghuarenmingongheguo2", 64) && strcmp(got, "zhonghuárenmingongheguo") == 0);   // the run's first a
    {
        c8 run[202];
        memset(run, 'a', 200); run[200] = '1'; run[201] = 0;                   // 200 a's, then tone 1
        CHECK(pinyin(run, sizeof got) && strncmp(got, "ā", 2) == 0 && strlen(got) == 201u && got[200] == 'a');
        run[200] = 0;                                                          // and with no tone
        CHECK(pinyin(run, sizeof got) && strlen(got) == 200u);
        CHECK(!pinyin(run, 200) && strlen(got) == 199u);                       // one byte short: refused, what fitted kept
    }

    // What does not fit is refused, the buffer always ending in a NUL inside it; what is not pinyin is refused.
    CHECK(!pinyin("ni3hao3", 5) && strlen(got) < 5u);
    CHECK(!pinyin("ni3hao3", 1) && got[0] == 0);
    CHECK(!pinyin("ni3!", 64) && !pinyin("3ni", 64) && !pinyin("", 64) && !pinyin(NULL, 64) && !pinyin("12", 64));
    CHECK(!fude_lang_reading_from_latin("ni3", NULL, 8) && !fude_lang_reading_from_latin("ni3", got, 0));

    // Readings compare without tone marks, ü as u, capitals as small letters.
    CHECK(fude_lang_reading_fold(0x01DAu) == 'u' && fude_lang_reading_fold(0x00FCu) == 'u' && fude_lang_reading_fold('N') == 'n');
    CHECK(fude_lang_reading_fold(0x00E1u) == 'a' && fude_lang_reading_fold(0x011Bu) == 'e' && fude_lang_reading_fold('q') == 'q');
    CHECK(fude_lang_reading_kinds() == 1u && !fude_lang_word_reading_latin() && fude_lang_reading_capital('A') == 'A');

    // The levels: HSK 1 to 6, and 7-9 as one.
    c8 name[32];
    CHECK(fude_lang_level_count() == 7u && fude_lang_level_value(0) == 1u && fude_lang_level_value(6) == 7u && fude_lang_level_value(7) == 0u);
    fude_lang_level_name(3, false, name, sizeof name); CHECK(strcmp(name, "HSK 3") == 0);
    fude_lang_level_name(7, true, name, sizeof name);  CHECK(strcmp(name, "HSK 7–9") == 0);
    fude_lang_level_name(0, true, name, sizeof name);  CHECK(name[0] == 0);

    // The groups: 国 simplified, 國 traditional only, a letter in none.
    CHECK(fude_lang_group(0x56FDu) == 0u && fude_lang_group(0x570Bu) == 1u && fude_lang_group('a') == FUDE_LANG_NO_GROUP);
    CHECK(!fude_lang_rtl() && fude_lang_latin(0x56FDu) == NULL && fude_lang_chart() == NULL);

    // Free writing is read by shape, size and place alone: none of lang.h's rules for it (no script switching costs,
    // no look-alikes across scripts, no small letters, no marks), and the line's frame fitted to the characters from
    // U+3040 on (a NULL placed: radicals such as ⺀ left out).
    { const fude_lang_writing_def* w = fude_lang_writing();
      CHECK(w->switch_cost == NULL && w->twin == NULL && w->script == NULL && w->choose == NULL);
      CHECK(w->size_twin == NULL && w->small == NULL && w->small_fits == NULL && w->voiced == NULL && w->voice_size == 0.0f);
      CHECK(w->mark == NULL && w->rare == NULL && w->placed == NULL); }

    printf(fails ? "zh %d FAILED\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
