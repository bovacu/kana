// fude/lang/ko, Hangul's language: romanization typed back into Hangul (and its limit: a text longer than any reading
// is refused whole), the letters' Latin names, the levels and the groups; and a syllable written by hand, read by the
// study's own reader (segment.h). Built as Hangul (run.sh's @APP=hangul): its strings and its data.
#include "lang/lang.h"
#include "drawing/base/text.h"
#include "study/handwriting/segment.h"
#include <stdio.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// Turns _latin into Hangul in a buffer of _size bytes (at most 1024) and says whether it read, keeping what came out.
static c8 got[1024];
static b8 hangul(const c8* _latin, usize _size) {
    memset(got, 'x', sizeof got);
    return fude_lang_reading_from_latin(_latin, got, _size);
}

// Writes a character's own strokes into an ink, its box's top-left at (ox, oy), k canvas units a data unit (Y up).
static void write_char(const fude_kanji_db* db, fude_ink* ink, u32 cp, f32 ox, f32 oy, f32 k) {
    fude_kanji_info info;
    if(!fude_kanji_find(db, cp, &info)) return;
    for(u32 s = 0; s < info.strokes; s++) {
        fude_kanji_stroke st; static rde_vec_2F pts[1024];
        const u32 n = fude_kanji_stroke_at(db, &info, s, &st) ? fude_kanji_stroke_points(&st, 0.25f, pts, 1024u) : 0u;
        if(n == 0) continue;
        ink->sample_time = 0.0; ink->zoom = 1.0f;
        fude_ink_begin(ink, (rde_vec_2F){ ox + pts[0].x * k, oy - pts[0].y * k }, true);
        for(u32 i = 1; i < n; i++) { ink->sample_time += 0.01; fude_ink_extend(ink, (rde_vec_2F){ ox + pts[i].x * k, oy - pts[i].y * k }); }
        fude_ink_end(ink);
    }
}

int main(int argc, char** argv) {

    // Consonants between two vowels: the longest run at their end that starts a syllable does, the rest ends the one
    // before (hak-gyo, an-nyeong). A hyphen parts syllables; spaces are kept; capitals read as small letters.
    CHECK(hangul("hakgyo", 64) && strcmp(got, "학교") == 0);
    CHECK(hangul("annyeong", 64) && strcmp(got, "안녕") == 0);
    CHECK(hangul("sarang-e", 64) && strcmp(got, "사랑에") == 0);
    CHECK(hangul("an nyeong", 64) && strcmp(got, "안 녕") == 0);
    CHECK(hangul("HAKGYO", 64) && strcmp(got, "학교") == 0);

    // The limit: 255 bytes of romanization are read whole ("ga" 127 times, then "a": 128 syllables), 256 are refused
    // whole, with nothing written: a text that long is not a reading, and is not read in part.
    {
        c8 text[300];
        for(u32 i = 0; i < 127u; i++) { text[2u * i] = 'g'; text[2u * i + 1u] = 'a'; }
        text[254] = 'a'; text[255] = 0;
        CHECK(strlen(text) == 255u && hangul(text, sizeof got) && strlen(got) == 128u * 3u);
        CHECK(strncmp(got, "가가", 6) == 0 && strcmp(got + 127u * 3u, "아") == 0);
        text[255] = 'a'; text[256] = 0;
        CHECK(!hangul(text, sizeof got) && got[0] == 0);
        memset(text, ' ', 299); text[299] = 0;                                 // spaces alone are no reading either
        CHECK(!hangul(text, sizeof got) && got[0] == 0);
    }

    // Hangul that does not fit is refused, with the syllables that fitted kept; what is not romanization is refused.
    CHECK(!hangul("hakgyo", 4) && strcmp(got, "학") == 0);
    CHECK(!hangul("hakgyo", 1) && got[0] == 0);
    CHECK(!hangul("hak1", 64) && !hangul("xyz", 64) && !hangul("", 64) && !hangul(NULL, 64) && !hangul("한", 64));
    CHECK(!fude_lang_reading_from_latin("hakgyo", NULL, 8) && !fude_lang_reading_from_latin("hakgyo", got, 0));

    // The letters' Latin names: a jamo's, a syllable's romanization.
    CHECK(strcmp(fude_lang_latin(0x3131u), "g/k") == 0 && strcmp(fude_lang_latin(0xD55Cu), "han") == 0);
    CHECK(strcmp(fude_lang_latin(0xAC00u), "ga") == 0 && fude_lang_latin('a') == NULL);
    CHECK(fude_lang_word_reading_latin() && fude_lang_reading_kinds() == 2u);

    // The levels, short and long (in English, the tests' language), and the groups.
    c8 name[64];
    CHECK(fude_lang_level_count() == 3u && fude_lang_level_value(2) == 3u && fude_lang_level_value(3) == 0u);
    fude_lang_level_name(1, false, name, sizeof name); CHECK(strcmp(name, "초급") == 0);
    fude_lang_level_name(1, true, name, sizeof name);  CHECK(strcmp(name, "Beginner 초급") == 0);
    fude_lang_level_name(0, true, name, sizeof name);  CHECK(name[0] == 0);
    CHECK(fude_lang_group(0x3131u) == 0u && fude_lang_group(0xD55Cu) == 1u && fude_lang_group(0x5B78u) == 2u && fude_lang_group('a') == FUDE_LANG_NO_GROUP);
    CHECK(fude_lang_core(0x3131u) && !fude_lang_core(0x3133u));                // ㄱ is core; the cluster ㄳ is not

    // Free writing is read by shape, size and place alone: none of lang.h's rules for it (no cost for switching
    // between Hangul and hanja, no look-alikes, no small letters, no marks).
    { const fude_lang_writing_def* w = fude_lang_writing();
      CHECK(w->switch_cost == NULL && w->twin == NULL && w->script == NULL && w->choose == NULL);
      CHECK(w->size_twin == NULL && w->small == NULL && w->small_fits == NULL && w->voiced == NULL && w->voice_size == 0.0f);
      CHECK(w->mark == NULL && w->rare == NULL && w->placed == NULL); }

    // A syllable written by hand pays only for its size and place, as a kana does in Japanese: no cost for being rare,
    // which a syllable (no school grade, as no hanja has) once paid as a kanji outside the Jouyou does, 20 and more,
    // and its jamo did not.
    if(argc > 1) {
        static fude_kanji_db db; CHECK(fude_kanji_load(&db, argv[1]));
        fude_catalog cat; fude_catalog_init(&cat, &db);
        fude_ink ink; fude_ink_init(&ink);
        write_char(&db, &ink, 0xD55Cu, 0.0f, 0.0f, 2.0f);   // 한
        fude_segment seg; fude_segment_init(&seg);
        u32 hanr = 0; CHECK(fude_kanji_find_index(&db, 0xD55Cu, &hanr));
        f32 place = -1.0f;
        if(fude_segment_read(&seg, &db, &cat, &ink, FUDE_SEGMENT_ACROSS) == 1u) {
            const fude_segment_char* c = (const fude_segment_char*)seg.chars.memory;
            for(u32 k = 0; k < c->candidate_count; k++) {
                if(c->candidates[k].record == hanr) { place = (c->candidates[k].cost - c->shape[k]) * (f32)c->count; }
            }
        }
        printf("한 read as one, its size and place costing %.2f\n", (double)place);
        CHECK(place >= 0.0f && place < 15.0f);
        fude_segment_destroy(&seg); fude_ink_destroy(&ink); fude_catalog_destroy(&cat); fude_kanji_unload(&db);
    }

    printf(fails ? "ko %d FAILED\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
